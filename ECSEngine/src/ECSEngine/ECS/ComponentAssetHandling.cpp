#include "ecspch.h"
#include "ComponentAssetHandling.h"
#include "../Resources/AssetDatabase.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../ECS/EntityManager.h"
#include "ComponentHelpers.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void GetComponentMissingAssetFields(
		const Reflection::ReflectionType* previous_type,
		const Reflection::ReflectionType* new_type,
		CapacityStream<ComponentAssetField>* missing_fields
	)
	{
		ECS_STACK_CAPACITY_STREAM(size_t, missing_field_indices, 512);
		Reflection::GetReflectionTypesDifferentFields(previous_type, new_type, &missing_field_indices);

		for (unsigned int index = 0; index < missing_field_indices.size; index++) {
			AssetTargetFieldFromReflection asset_field = GetAssetTargetFieldFromReflection(previous_type->fields[missing_field_indices[index]], nullptr);
			AssetTypeEx missing_type_ex;
			missing_type_ex.type = ECS_ASSET_TYPE_COUNT;
			AssetTypeEx asset_type = asset_field.success ? asset_field.type : missing_type_ex;

			if (asset_type.type != ECS_ASSET_TYPE_COUNT) {
				missing_fields->AddAssert({ (unsigned int)missing_field_indices[index], asset_type });
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	bool GetAssetFieldsFromComponent(const Reflection::ReflectionType* type, CapacityStream<ComponentAssetField>& field_indices)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			AssetTargetFieldFromReflection asset_field = GetAssetTargetFieldFromReflection(type->fields[index], nullptr);

			if (!asset_field.success) {
				return false;
			}
			else if (asset_field.type.type != ECS_ASSET_TYPE_COUNT) {
				field_indices.Add({ (unsigned int)index, asset_field.type });
			}
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool HasAssetFieldsComponent(const Reflection::ReflectionType* type)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			AssetTargetFieldFromReflection asset_field = GetAssetTargetFieldFromReflection(type->fields[index], nullptr);

			if (!asset_field.success) {
				return false;
			}
			else if (asset_field.type.type != ECS_ASSET_TYPE_COUNT) {
				return true;
			}
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	AssetTargetFieldFromReflection GetAssetTargetFieldFromReflection(const Reflection::ReflectionField& field, const void* data)
	{
		AssetTargetFieldFromReflection result;

		result.type.type = ECS_ASSET_TYPE_COUNT;

		// 2 means there is no match. It will be set to 1 if there is a match with a succesful
		// type or 0 if there is match but incorrect field type
		bool success = true;

		Stream<char> definition = field.definition;

		size_t mapping_count = ECS_ASSET_TARGET_FIELD_NAMES_SIZE();
		for (size_t index = 0; index < mapping_count; index++) {
			if (definition == ECS_ASSET_TARGET_FIELD_NAMES[index].name) {
				result.type = ECS_ASSET_TARGET_FIELD_NAMES[index].type;
				if (data != nullptr) {
					result.asset = GetAssetTargetFieldFromReflection(field, data, result.type.type);
					success = result.asset.size != -1;
				}
				else {
					success = true;
				}
				break;
			}
		}

		result.success = success;
		return result;
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<void> GetAssetTargetFieldFromReflection(const Reflection::ReflectionField& field, const void* data, ECS_ASSET_TYPE asset_type)
	{
		auto get_pointer_from_field = [data](const Reflection::ReflectionFieldInfo* info, const void** pointer) {
			if (info->stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				unsigned int indirection = Reflection::GetReflectionFieldPointerIndirection(*info);
				if (indirection > 1) {
					// Fail
					return false;
				}
				*pointer = *(void**)OffsetPointer(data, info->pointer_offset);
			}
			else if (info->stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				*pointer = (void*)OffsetPointer(data, info->pointer_offset);
			}
			else {
				// Fail
				return false;
			}
			return true;
			};

		const void* pointer = nullptr;
		size_t pointer_size = -1;

		bool success = get_pointer_from_field(&field.info, &pointer);

		if (success) {
			if (asset_type == ECS_ASSET_TEXTURE || asset_type == ECS_ASSET_GPU_SAMPLER || asset_type == ECS_ASSET_SHADER) {
				// The interface is one level deeper - unless the pointer is nullptr
				pointer = *(const void**)pointer;
			}
			else if (asset_type == ECS_ASSET_MISC) {
				// Extract the size and the pointer is deeper
				pointer_size = *(size_t*)OffsetPointer(pointer, sizeof(void*));
				pointer = *(void**)pointer;
			}

			// Materials and meshes should have the pointer extraction correctly applied

			if (asset_type != ECS_ASSET_MISC) {
				// Indicate that the retrieval was successful
				pointer_size = 0;
			}
		}

		return { pointer, pointer_size };
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Comparator>
	static ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflectionImpl(
		const Reflection::ReflectionField& field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type,
		Comparator&& comparator
	) {
		ECS_ASSET_TYPE current_field_type = FindAssetTargetField(field.definition).type;
		if (field_type != current_field_type) {
			return ECS_SET_ASSET_TARGET_FIELD_NONE;
		}

		void* ptr_to_write = OffsetPointer(data, field.info.pointer_offset);

		auto copy_value = [&](const void* ptr_to_copy, size_t copy_size) {
			if (!comparator(ptr_to_write)) {
				return ECS_SET_ASSET_TARGET_FIELD_OK;
			}

			if ((unsigned int)ptr_to_copy >= ECS_ASSET_RANDOMIZED_ASSET_LIMIT) {
				memcpy(ptr_to_write, ptr_to_copy, copy_size);
			}
			else {
				memset(ptr_to_write, 0, copy_size);
				unsigned int* randomized_value = (unsigned int*)ptr_to_write;
				*randomized_value = (unsigned int)ptr_to_copy;
			}
			return ECS_SET_ASSET_TARGET_FIELD_MATCHED;
			};

		auto set_pointer = [&](void* ptr_to_set) {
			void** ptr = (void**)ptr_to_write;
			if (!comparator(*ptr)) {
				return ECS_SET_ASSET_TARGET_FIELD_OK;
			}
			*ptr = ptr_to_set;
			return ECS_SET_ASSET_TARGET_FIELD_MATCHED;
			};

		auto copy_and_pointer = [&](size_t copy_size) {
			if (field.info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				// Copy the data
				return copy_value(field_data.buffer, copy_size);
			}
			else if (field.info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				if (Reflection::GetReflectionFieldPointerIndirection(field.info) == 1) {
					return set_pointer(field_data.buffer);
				}
				else {
					return ECS_SET_ASSET_TARGET_FIELD_FAILED;
				}
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
			return ECS_SET_ASSET_TARGET_FIELD_OK;
			};

		// Same type
		switch (field_type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_MATERIAL:
		{
			if (field.info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				return set_pointer(field_data.buffer);
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
		}
		break;
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		{
			if (field.info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				return set_pointer(field_data.buffer);
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
		}
		break;
		case ECS_ASSET_MISC:
		{
			return copy_and_pointer(field_data.size);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return ECS_SET_ASSET_TARGET_FIELD_OK;
	}

	ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflection(
		const Reflection::ReflectionField& field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type
	)
	{
		return SetAssetTargetFieldFromReflectionImpl(field, data, field_data, field_type, [](const void* ptr) { return true; });
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflectionIfMatches(
		const Reflection::ReflectionField& field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type,
		Stream<void> comparator
	)
	{
		return SetAssetTargetFieldFromReflectionImpl(field, data, field_data, field_type, [=](void* ptr) {
			switch (field_type) {
			case ECS_ASSET_MESH:
			case ECS_ASSET_MATERIAL:
			case ECS_ASSET_TEXTURE:
			case ECS_ASSET_GPU_SAMPLER:
			case ECS_ASSET_SHADER:
			{
				return ptr == comparator.buffer;
			}
			case ECS_ASSET_MISC:
			{
				Stream<void> current_ptr = *(Stream<void>*)ptr;
				return current_ptr.buffer == comparator.buffer && current_ptr.size == comparator.size;
			}
			break;
			}

			// Shouldn't reach here
			return false;
			});
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static void GetComponentHandlesImpl(
		const Reflection::ReflectionType* type,
		const void* component,
		const AssetDatabase* asset_database,
		size_t asset_field_count,
		CapacityStream<unsigned int>& handles,
		Functor&& functor
	) {
		for (size_t index = 0; index < asset_field_count; index++) {
			unsigned int field_index = functor(index);
			AssetTargetFieldFromReflection asset_result = GetAssetTargetFieldFromReflection(type->fields[field_index], component);
			if (asset_result.success) {
				handles.AddAssert(asset_database->FindAssetEx(asset_result.asset, asset_result.type.type));
			}
		}
	}

	void GetComponentHandles(
		const Reflection::ReflectionType* type,
		const void* component,
		const AssetDatabase* asset_database,
		Stream<unsigned int> field_indices,
		CapacityStream<unsigned int>& handles
	)
	{
		GetComponentHandlesImpl(type, component, asset_database, field_indices.size, handles, [field_indices](size_t index) {
			return field_indices[index];
			});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetComponentHandles(
		const Reflection::ReflectionType* type,
		const void* component,
		const AssetDatabase* asset_database,
		Stream<ComponentAssetField> field_indices,
		CapacityStream<unsigned int>& handles
	)
	{
		GetComponentHandlesImpl(type, component, asset_database, field_indices.size, handles, [field_indices](size_t index) {
			return field_indices[index].field_index;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static void GetComponentHandlePtrsImpl(
		const Reflection::ReflectionType* type,
		const void* component,
		const AssetDatabase* asset_database,
		size_t asset_field_count,
		CapacityStream<unsigned int*>& pointers,
		Functor&& functor
	) {
		for (size_t index = 0; index < asset_field_count; index++) {
			unsigned int field_index = functor(index);
			AssetTargetFieldFromReflection asset_result = GetAssetTargetFieldFromReflection(type, field_index, component);
			if (asset_result.success) {
				pointers.AddAssert(asset_database->FindAssetEx(asset_result.asset, asset_result.type.type));
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetComponentAssetData(
		const Reflection::ReflectionType* type,
		const void* component,
		Stream<ComponentAssetField> asset_fields,
		CapacityStream<Stream<void>>* field_data
	)
	{
		ECS_ASSERT(field_data->capacity - field_data->size >= asset_fields.size);

		for (size_t index = 0; index < asset_fields.size; index++) {
			AssetTargetFieldFromReflection target_field = GetAssetTargetFieldFromReflection(type->fields[asset_fields[index].field_index], component);
			ECS_ASSERT(target_field.type.type == asset_fields[index].type.type && target_field.success);
			field_data->Add(target_field.asset);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetComponentAssetDataDeep(
		const Reflection::ReflectionType* type,
		const void* component,
		Stream<ComponentAssetField> asset_fields,
		const AssetDatabase* database,
		ECS_ASSET_TYPE asset_type,
		CapacityStream<Stream<void>>* field_data
	) {
		for (size_t index = 0; index < asset_fields.size; index++) {
			AssetTargetFieldFromReflection target_field = GetAssetTargetFieldFromReflection(type->fields[asset_fields[index].field_index], component);
			ECS_ASSERT(target_field.type.type == asset_fields[index].type.type && target_field.success);
			if (asset_fields[index].type.type == asset_type) {
				field_data->AddAssert(target_field.asset);
			}
			else {
				// If the asset type is a texture or gpu sampler or shader, it can be referenced by a material
				if (IsAssetTypeReferenceable(asset_type) && IsAssetTypeWithDependencies(asset_fields[index].type.type)) {
					unsigned int handle = database->FindAssetEx(target_field.asset, target_field.type.type);
					if (handle != -1) {
						// Normally, if the asset is created, all handles should be valid (aka -1)
						// But let's be conservative and still check for it. Also if it is set and the asset
						// doesn't exist in the database skip it
						ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
						GetAssetDependencies(database->GetAssetConst(handle, target_field.type.type), target_field.type.type, &dependencies);

						for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
							unsigned int current_handle = dependencies[subindex].handle;
							if (dependencies[subindex].type == asset_type && current_handle != -1) {
								if (database->Exists(current_handle, asset_type)) {
									field_data->AddAssert(GetAssetFromMetadata(database->GetAssetConst(current_handle, asset_type), asset_type));
								}
							}
						}
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		AssetDatabase* asset_database
	)
	{
		// Allocate space for the reference counts and initialize them to 0
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);

		ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, asset_reference_counts, ECS_ASSET_TYPE_COUNT);
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			asset_reference_counts[index].Initialize(&stack_allocator, asset_database->GetAssetCount((ECS_ASSET_TYPE)index));
			memset(asset_reference_counts[index].buffer, 0, asset_reference_counts[index].MemoryOf(asset_reference_counts[index].size));
		}
		asset_reference_counts.size = asset_reference_counts.capacity;

		// Get the reference counts
		GetAssetReferenceCountsFromEntities(entity_manager, reflection_manager, asset_database, asset_reference_counts);

		unsigned int max_asset_count = asset_database->GetMaxAssetCount();

		CapacityStream<unsigned int> handles_to_be_removed;
		handles_to_be_removed.Initialize(&stack_allocator, 0, max_asset_count);

		// Perform the reference count change for those assets that are still in use
		// Those that need to be removed cache them in separate buffer and perform the removal afterwards
		for (size_t asset_type_index = 0; asset_type_index < ECS_ASSET_TYPE_COUNT; asset_type_index++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)asset_type_index;
			handles_to_be_removed.size = 0;
			for (unsigned int index = 0; index < asset_reference_counts[asset_type].size; index++) {
				unsigned int handle = asset_database->GetAssetHandleFromIndex(index, asset_type);

				// Get the standalone reference count - subtract the difference between these 2 counts
				unsigned int standalone_reference_count = asset_database->GetReferenceCountStandalone(handle, asset_type);
				unsigned int difference = standalone_reference_count - asset_reference_counts[asset_type][index];

				if (difference == standalone_reference_count) {
					unsigned int reference_count = asset_database->GetReferenceCount(handle, asset_type);
					if (reference_count == standalone_reference_count) {
						// Add it to the removal buffer - can be safely removed
						handles_to_be_removed.Add(handle);
					}
					else {
						// Reduce the reference count
						asset_database->SetAssetReferenceCount(handle, asset_type, reference_count - difference);
					}
				}
				else {
					// Update the reference count
					asset_database->SetAssetReferenceCount(handle, asset_type, asset_reference_counts[asset_type][index]);
				}
			}

			// Remove the handles now
			AssetDatabaseRemoveInfo remove_info;
			remove_info.remove_dependencies = true;
			for (unsigned int index = 0; index < handles_to_be_removed.size; index++) {
				asset_database->RemoveAssetForced(handles_to_be_removed[index], asset_type, &remove_info);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	// The functor receives as parameters (ECS_ASSET_TYPE type, unsigned int asset_index, unsigned int reference_count)
	template<typename ElementType, typename IncrementFunctor>
	static void GetAssetReferenceCountsFromEntitiesImpl(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		Stream<Stream<ElementType>> asset_fields_reference_count,
		GetAssetReferenceCountsFromEntitiesOptions options,
		IncrementFunctor&& increment_functor
	) {
		ECS_ASSERT(asset_fields_reference_count.size == ECS_ASSET_TYPE_COUNT);

		// Make sure that there are enough slots for each asset type
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSERT(asset_fields_reference_count[index].size >= asset_database->GetAssetCount((ECS_ASSET_TYPE)index));
		}

		ECS_STACK_CAPACITY_STREAM(unsigned int, unique_types, ECS_KB * 16);
		ECS_STACK_CAPACITY_STREAM(unsigned int, shared_types, ECS_KB * 16);
		ECS_STACK_CAPACITY_STREAM(unsigned int, global_types, ECS_KB * 4);

		GetHierarchyComponentTypes(reflection_manager, -1, &unique_types, &shared_types, &global_types);

		auto increment_reference_counts = [&](
			Stream<ComponentAssetField> asset_fields,
			const Reflection::ReflectionType* reflection_type,
			const void* data,
			unsigned int increment_count
			) {
				ECS_STACK_CAPACITY_STREAM(unsigned int, handles, 512);
				GetComponentHandles(reflection_type, data, asset_database, asset_fields, handles);

				// For each handle increase the reference count for that asset type
				for (unsigned int index = 0; index < asset_fields.size; index++) {
					if (handles[index] != -1) {
						unsigned int asset_index = asset_database->GetIndexFromAssetHandle(handles[index], asset_fields[index].type.type);
						increment_functor(asset_fields[index].type.type, asset_index, increment_count);
					}
				}
			};

		auto get_corresponding_reflection_type = [&](Component component, Stream<unsigned int> type_indices) {
			const Reflection::ReflectionType* reflection_type = nullptr;
			for (unsigned int index = 0; index < type_indices.size; index++) {
				const Reflection::ReflectionType* current_reflection_type = reflection_manager->GetType(type_indices[index]);
				Component reflection_component = GetReflectionTypeComponent(current_reflection_type);
				if (reflection_component == component) {
					reflection_type = current_reflection_type;
					break;
				}
			}

			return reflection_type;
			};

		// Unique components
		entity_manager->ForEachComponent([&](Component component) {
			// For each component determine its reflection type and the asset fields
			const Reflection::ReflectionType* reflection_type = get_corresponding_reflection_type(component, unique_types);

			ECS_ASSERT(reflection_type != nullptr);

			ECS_STACK_CAPACITY_STREAM(ComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromComponent(reflection_type, asset_fields);

			if (asset_fields.size > 0) {
				// If it has asset fields, then retrieve them and keep the count of reference counts
				entity_manager->ForEachEntityComponent(component, [&](Entity entity, const void* data) {
					increment_reference_counts(asset_fields, reflection_type, data, 1);
					});
			}
			});

		// Shared components
		entity_manager->ForEachSharedComponent([&](Component component) {
			// For each component determine its reflection type and the asset fields
			const Reflection::ReflectionType* reflection_type = get_corresponding_reflection_type(component, shared_types);
			ECS_ASSERT(reflection_type != nullptr);

			ECS_STACK_CAPACITY_STREAM(ComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromComponent(reflection_type, asset_fields);

			if (asset_fields.size > 0) {
				// Now go for each shared instance
				entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
					const void* data = entity_manager->GetSharedData(component, instance);
					unsigned int increase_count = 1;
					if (!options.shared_instance_only_once) {
						increase_count = entity_manager->GetNumberOfEntitiesForSharedInstance(component, instance);
					}
					// If the options is activated, count the number of entities that reference this instance
					increment_reference_counts(asset_fields, reflection_type, data, increase_count);
					});
			}
			});

		// Global components
		entity_manager->ForAllGlobalComponents([&](const void* global_data, Component component) {
			// For each component determine its reflection type and the asset fields
			const Reflection::ReflectionType* reflection_type = get_corresponding_reflection_type(component, global_types);

			ECS_ASSERT(reflection_type != nullptr);

			ECS_STACK_CAPACITY_STREAM(ComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromComponent(reflection_type, asset_fields);

			if (asset_fields.size > 0) {
				// If it has asset fields, then retrieve them and keep the count of reference counts
				increment_reference_counts(asset_fields, reflection_type, global_data, 1);
			}
			});

		if (options.add_reference_counts_to_dependencies) {
			// After we retrieved the reference counts for all assets from the scene, we need to increase reference counts
			// for assets that are being referenced by other assets
			ForEach(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [&](ECS_ASSET_TYPE asset_type) {
				ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, asset_dependencies, 512);
				for (size_t index = 0; index < asset_fields_reference_count[asset_type].size; index++) {
					asset_dependencies.size = 0;
					asset_database->GetDependencies(asset_database->GetAssetHandleFromIndex(index, asset_type), asset_type, &asset_dependencies);
					for (unsigned int subindex = 0; subindex < asset_dependencies.size; subindex++) {
						unsigned int asset_index = asset_database->GetIndexFromAssetHandle(asset_dependencies[subindex].handle, asset_dependencies[subindex].type);
						increment_functor(asset_dependencies[subindex].type, asset_index, 1);
					}
				}
				});
		}
	}

	void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		Stream<Stream<unsigned int>> asset_fields_reference_count,
		GetAssetReferenceCountsFromEntitiesOptions options
	)
	{
		GetAssetReferenceCountsFromEntitiesImpl(
			entity_manager,
			reflection_manager,
			asset_database,
			asset_fields_reference_count,
			options,
			[&asset_fields_reference_count](ECS_ASSET_TYPE type, unsigned int asset_index, unsigned int increment_count) {
				asset_fields_reference_count[type][asset_index] += increment_count;
			}
		);
	}

	void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		Stream<Stream<uint2>> asset_fields_reference_count,
		GetAssetReferenceCountsFromEntitiesOptions options
	)
	{
		GetAssetReferenceCountsFromEntitiesImpl(
			entity_manager,
			reflection_manager,
			asset_database,
			asset_fields_reference_count,
			options,
			[&asset_fields_reference_count](ECS_ASSET_TYPE type, unsigned int asset_index, unsigned int increment_count) {
				asset_fields_reference_count[type][asset_index].y += increment_count;
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetAssetReferenceCountsFromEntitiesPrepare(
		Stream<Stream<unsigned int>> asset_fields_reference_counts,
		AllocatorPolymorphic allocator,
		const AssetDatabase* asset_database
	)
	{
		ECS_ASSERT(asset_fields_reference_counts.size == ECS_ASSET_TYPE_COUNT);

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
			unsigned int count = asset_database->GetAssetCount(current_type);
			asset_fields_reference_counts[index].Initialize(allocator, count);
			memset(asset_fields_reference_counts[index].buffer, 0, asset_fields_reference_counts[index].MemoryOf(count));
		}
	}

	void GetAssetReferenceCountsFromEntitiesPrepare(
		Stream<Stream<uint2>> asset_fields_reference_counts,
		AllocatorPolymorphic allocator,
		const AssetDatabase* asset_database
	) {
		ECS_ASSERT(asset_fields_reference_counts.size == ECS_ASSET_TYPE_COUNT);

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
			unsigned int count = asset_database->GetAssetCount(current_type);
			asset_fields_reference_counts[index].Initialize(allocator, count);
			for (unsigned int subindex = 0; subindex < count; subindex++) {
				asset_fields_reference_counts[index][subindex].x = asset_database->GetAssetHandleFromIndex(subindex, current_type);
				asset_fields_reference_counts[index][subindex].y = 0;
			}
		}
	}

	void DeallocateAssetReferenceCountsFromEntities(Stream<Stream<unsigned int>> reference_counts, AllocatorPolymorphic allocator) {
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			reference_counts[index].Deallocate(allocator);
		}
	}

	void DeallocateAssetReferenceCountsFromEntities(Stream<Stream<uint2>> reference_counts, AllocatorPolymorphic allocator) {
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			reference_counts[index].Deallocate(allocator);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ForEachAssetReferenceDifference(
		const AssetDatabase* database,
		Stream<Stream<unsigned int>> previous_counts,
		Stream<Stream<unsigned int>> current_counts,
		ForEachAssetReferenceDifferenceFunctor functor,
		void* functor_data
	) {
		for (size_t asset_index = 0; asset_index < ECS_ASSET_TYPE_COUNT; asset_index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)asset_index;
			ECS_ASSERT(previous_counts[current_type].size == current_counts[current_type].size);
			for (size_t subindex = 0; subindex < previous_counts[current_type].size; subindex++) {
				unsigned int previous = previous_counts[current_type][subindex];
				unsigned int current = current_counts[current_type][subindex];
				if (previous != current) {
					functor(current_type, database->GetAssetHandleFromIndex(subindex, current_type), (int)(current - previous), functor_data);
				}
			}
		}
	}

}