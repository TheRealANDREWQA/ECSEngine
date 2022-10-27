#include "ecspch.h"
#include "ModuleUtilities.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../Utilities/Serialization/SerializationHelpers.h"
#include "../../Internal/Resources/AssetDatabase.h"
#include "../../Internal/LinkComponents.h"

namespace ECSEngine {

	// ---------------------------------------------------------------------------------------------------------------------

	// Returns true if the type has conformant field types else false
	bool ConvertTargetAssetsToHandles(
		const AssetDatabase* database,
		const Reflection::ReflectionType* target_type,
		const void* target_data,
		CapacityStream<unsigned int>& handles,
		CapacityStream<ECS_ASSET_TYPE>& types
	) {
		for (size_t index = 0; index < target_type->fields.size; index++) {
			AssetTargetFieldFromReflection asset_field = GetAssetTargetFieldFromReflection(target_type, index, target_data);

			if (!asset_field.success) {
				return false;
			}
			else if (asset_field.type != ECS_ASSET_TYPE_COUNT) {
				handles.AddSafe(database->FindAssetEx(asset_field.asset, asset_field.type));
				types.AddSafe(asset_field.type);
			}
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void ConvertHandlesToAssets(
		const AssetDatabase* database,
		const Reflection::ReflectionType* link_type,
		const void* link_data,
		CapacityStream<Stream<void>>& assets,
		CapacityStream<ECS_ASSET_TYPE>& types
	) {
		for (size_t index = 0; index < link_type->fields.size; index++) {
			ECS_ASSET_TYPE handle_type = FindAssetMetadataMacro(link_type->fields[index].tag);
			if (handle_type != ECS_ASSET_TYPE_COUNT) {
				// We have a handle
				unsigned int handle = *(unsigned int*)function::OffsetPointer(link_data, link_type->fields[index].info.pointer_offset);
				const void* metadata = database->GetAssetConst(handle, handle_type);
				Stream<void> asset_data = GetAssetFromMetadata(metadata, handle_type);
				assets.AddSafe(asset_data);
				types.AddSafe(handle_type);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------

	bool ModuleFromTargetToLinkComponent(
		ModuleLinkComponentTarget module_link, 
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* target_type, 
		const Reflection::ReflectionType* link_type, 
		const AssetDatabase* asset_database, 
		const void* target_data, 
		void* link_data,
		AllocatorPolymorphic allocator
	)
	{
		// Determine if the link component has a DLL function
		bool needs_dll = Reflection::GetReflectionTypeLinkComponentNeedsDLL(link_type);
		if (needs_dll && (module_link.build_function == nullptr || module_link.reverse_function == nullptr)) {
			// Fail
			return false;
		}

		// Extract the handles of the target
		ECS_STACK_CAPACITY_STREAM(unsigned int, asset_handles, 512);
		ECS_STACK_CAPACITY_STREAM(ECS_ASSET_TYPE, asset_types, 512);
		bool success = ConvertTargetAssetsToHandles(asset_database, target_type, target_data, asset_handles, asset_types);
		if (!success) {
			return false;
		}

		// Needs DLL but the functions are present
		if (needs_dll) {
			// Use the function
			ModuleLinkComponentReverseFunctionData reverse_data;
			reverse_data.asset_handles = asset_handles;
			reverse_data.link_component = link_data;
			reverse_data.component = target_data;
			module_link.reverse_function(&reverse_data);
		}
		else {
			const size_t MAX_LINK_SIZE = ECS_KB * 4;
			size_t link_type_size = Reflection::GetReflectionTypeByteSize(link_type);
			ECS_ASSERT(link_type_size < MAX_LINK_SIZE);
			size_t link_type_storage[MAX_LINK_SIZE / sizeof(size_t)];
			memcpy(link_type_storage, link_data, link_type_size);

			struct RestoreLinkData {
				void operator()() {
					if (link_type_size > 0) {
						memcpy(link_data, link_initial_storage, link_type_size);
					}
				}

				void* link_data;
				void* link_initial_storage;
				size_t link_type_size;
			};

			StackScope<RestoreLinkData> restore_link({ link_data, link_type_storage, link_type_size });

			// If different count of fields, fail
			if (link_type->fields.size != target_type->fields.size) {
				return false;
			}

			// Go through the link target and try to copy the fields
			unsigned int asset_index = 0;
			for (size_t index = 0; index < link_type->fields.size; index++) {
				ECS_ASSET_TYPE asset_type = FindAssetMetadataMacro(link_type->fields[index].tag);
				if (asset_type != ECS_ASSET_TYPE_COUNT) {
					if (asset_index >= asset_types.size) {
						// Too many assets, fail
						return false;
					}

					if (asset_types[asset_index] != asset_type) {
						// A mismatch, fail
						return false;
					}
					
					unsigned int* handle = (unsigned int*)function::OffsetPointer(link_data, link_type->fields[index].info.pointer_offset);
					*handle = asset_handles[asset_index];
					asset_index++;
				}
				else {
					// Not an asset. Check to see that they have identical types
					if (!link_type->fields[index].Compare(target_type->fields[index])) {
						return false;
					}

					const void* source = function::OffsetPointer(target_data, target_type->fields[index].info.pointer_offset);
					void* destination = function::OffsetPointer(link_data, link_type->fields[index].info.pointer_offset);
					if (allocator.allocator != nullptr) {
						CopyReflectionType(
							reflection_manager,
							link_type->fields[index].definition,
							source,
							destination,
							allocator
						);
					}
					else {
						memcpy(
							destination,
							source,
							link_type->fields[index].info.byte_size
						);
					}
				}
			}

			// Disable the restore
			restore_link.deallocator.link_type_size = 0;
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	bool ModuleLinkComponentToTarget(
		ModuleLinkComponentTarget module_link, 
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* link_type, 
		const Reflection::ReflectionType* target_type,
		const AssetDatabase* asset_database,
		const void* link_data, 
		void* target_data,
		AllocatorPolymorphic allocator
	)
	{
		// Determine if the link component has a DLL function
		bool needs_dll = Reflection::GetReflectionTypeLinkComponentNeedsDLL(link_type);
		if (needs_dll && (module_link.build_function == nullptr || module_link.reverse_function == nullptr)) {
			// Fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(Stream<void>, assets, 512);
		ECS_STACK_CAPACITY_STREAM(ECS_ASSET_TYPE, asset_types, 512);
		ConvertHandlesToAssets(asset_database, link_type, link_data, assets, asset_types);

		if (needs_dll) {
			// Build it using the function
			ModuleLinkComponentFunctionData build_data;
			build_data.assets = assets;
			build_data.component = target_data;
			build_data.link_component = link_data;
			module_link.build_function(&build_data);
		}
		else {
			if (link_type->fields.size != target_type->fields.size) {
				return false;
			}

			const size_t MAX_TARGET_SIZE = ECS_KB * 4;
			size_t target_size = Reflection::GetReflectionTypeByteSize(target_type);
			size_t target_data_storage[MAX_TARGET_SIZE / sizeof(size_t)];
			memcpy(target_data_storage, target_data, target_size);

			struct RestoreTarget {
				void operator()() {
					if (copy_size > 0) {
						memcpy(target_data, target_storage, copy_size);
					}
				}

				void* target_data;
				const void* target_storage;
				size_t copy_size;
			};

			StackScope<RestoreTarget> restore_stack({ target_data, target_data_storage, target_size });

			size_t asset_index = 0;
			auto copy_field = [&](size_t index) {
				const void* source = function::OffsetPointer(link_data, link_type->fields[index].info.pointer_offset);
				void* destination = function::OffsetPointer(target_data, target_type->fields[index].info.pointer_offset);

				if (allocator.allocator != nullptr) {
					CopyReflectionType(
						reflection_manager,
						target_type->fields[index].definition,
						source,
						destination,
						allocator
					);
				}
				else {
					memcpy(
						destination,
						source,
						target_type->fields[index].info.byte_size
					);
				}
			};
			for (size_t index = 0; index < target_type->fields.size; index++) {
				if (asset_index < assets.size) {
					ECS_SET_ASSET_TARGET_FIELD_RESULT result = SetAssetTargetFieldFromReflection(target_type, index, target_data, assets[asset_index], asset_types[asset_index]);
					if (result == ECS_SET_ASSET_TARGET_FIELD_FAILED) {
						return false;
					}
					else if (result == ECS_SET_ASSET_TARGET_FIELD_OK) {
						asset_index++;
					}
					else {
						if (target_type->fields[index].Compare(link_type->fields[index])) {
							return false;
						}
						copy_field(index);
					}
				}
				else {
					ECS_ASSET_TYPE type = FindAssetTargetField(target_type->fields[index].definition);
					if (type != ECS_ASSET_TYPE_COUNT) {
						// Too many asset types
						return false;
					}
					copy_field(index);
				}
			}


			restore_stack.deallocator.copy_size = 0;
		}
		
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------

}