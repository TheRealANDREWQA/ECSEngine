#include "ecspch.h"
#include "ModuleUtilities.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../Internal/Resources/AssetDatabase.h"

namespace ECSEngine {

	// Returns true if the type has conformant field types else false
	bool ConvertTargetAssetsToHandles(
		const AssetDatabase* database,
		const Reflection::ReflectionType* target_type,
		const void* target_data,
		CapacityStream<unsigned int>& handles,
		CapacityStream<ECS_ASSET_TYPE>& types
	) {
		auto get_pointer_from_field = [target_data](const Reflection::ReflectionFieldInfo* info, const void** pointer) {
			if (info->stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				unsigned int indirection = Reflection::GetReflectionFieldPointerIndirection(*info);
				if (indirection > 1) {
					// Fail
					return false;
				}
				*pointer = *(void**)function::OffsetPointer(target_data, info->pointer_offset);
			}
			else if (info->stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				*pointer = (void*)function::OffsetPointer(target_data, info->pointer_offset);
			}
			else {
				// Fail
				return false;
			}
			return true;
		};

		for (size_t index = 0; index < target_type->fields.size; index++) {
			const void* pointer = nullptr;
			size_t pointer_size = 0;
			ECS_ASSET_TYPE asset_type = ECS_ASSET_TYPE_COUNT;

			// 2 means there is no match. It will be set to 1 if there is a match with a succesful
			// type or 0 if there is match but incorrect field type
			bool success = true;

			Stream<char> definition = target_type->fields[index].definition;
			const Reflection::ReflectionFieldInfo* info = &target_type->fields[index].info;
			if (function::CompareStrings(definition, STRING(CoallescedMesh))) {
				success = get_pointer_from_field(info, &pointer);
				asset_type = ECS_ASSET_MESH;
			}
			else if (function::CompareStrings(definition, STRING(ResourceView))) {
				success = get_pointer_from_field(info, &pointer);
				if (success) {
					// The interface is one level deeper
					pointer = *(const void**)pointer;
				}
				asset_type = ECS_ASSET_TEXTURE;
			}
			else if (function::CompareStrings(definition, STRING(SamplerState))) {
				success = get_pointer_from_field(info, &pointer);
				if (success) {
					// The interface is one level deeper
					pointer = *(const void**)pointer;
				}
				asset_type = ECS_ASSET_GPU_SAMPLER;
			}
			else if (function::CompareStrings(definition, STRING(VertexShader)) || function::CompareStrings(definition, STRING(PixelShader))
				|| function::CompareStrings(definition, STRING(ComputeShader))) {
				success = get_pointer_from_field(info, &pointer);
				if (success) {
					// The interface is one level deeper
					pointer = *(const void**)pointer;
				}
				asset_type = ECS_ASSET_SHADER;
			}
			else if (function::CompareStrings(definition, STRING(Material))) {
				success = get_pointer_from_field(info, &pointer);
				asset_type = ECS_ASSET_MATERIAL;
			}
			else if (function::CompareStrings(definition, STRING(Stream<void>))) {
				success = get_pointer_from_field(info, &pointer);
				if (success) {
					// Extract the size and the pointer is deeper
					pointer_size = *(size_t*)function::OffsetPointer(pointer, sizeof(void*));
					pointer = *(void**)pointer;
				}
				asset_type = ECS_ASSET_MISC;
			}

			if (!success) {
				return false;
			}
			else if (asset_type != ECS_ASSET_TYPE_COUNT) {
				handles.AddSafe(database->FindAssetEx({ pointer, pointer_size }, asset_type));
				types.AddSafe(asset_type);
			}
		}
	}

	// Returns true if the type has conformant field types else false
	bool ConvertHandlesToAssets(
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

	void ModuleResetLinkComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* link_component
	)
	{
		reflection_manager->SetInstanceDefaultData(type, link_component);
		// Make all handles -1
		for (size_t index = 0; index < type->fields.size; index++) {
			if (FindAssetMetadataMacro(type->fields[index].tag) != ECS_ASSET_TYPE_COUNT) {
				// It is a handle
				unsigned int* handle = (unsigned int*)function::OffsetPointer(link_component, type->fields[index].info.pointer_offset);
				*handle = -1;
			}
		}
	}

	bool ModuleFromTargetToLinkComponent(
		ModuleLinkComponentTarget module_link, 
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* target_type, 
		const Reflection::ReflectionType* link_type, 
		const AssetDatabase* asset_database, 
		const void* target_data, 
		void* link_data
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

					memcpy(
						function::OffsetPointer(link_data, link_type->fields[index].info.pointer_offset),
						function::OffsetPointer(target_data, target_type->fields[index].info.pointer_offset),
						link_type->fields[index].info.byte_size
					);
				}
			}

			// Disable the restore
			restore_link.deallocator.link_type_size = 0;
		}

		return true;
	}

	bool ModuleLinkComponentToTarget(
		ModuleLinkComponentTarget module_link, 
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* link_type, 
		const Reflection::ReflectionType* target_type,
		const AssetDatabase* asset_database,
		const void* link_data, 
		void* target_data
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
		bool success = ConvertHandlesToAssets(asset_database, link_type, link_data, assets, asset_types);
		if (!success) {
			return false;
		}

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

			for (size_t index = 0; index < target_type->fields.size; index++) {
				if (function::CompareStrings(target_type->fields.size))
			}


			restore_stack.deallocator.copy_size = 0;
		}
		

		return true;
	}

}