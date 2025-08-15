#include "editorpch.h"
#include "ECSEngineAssets.h"

#include "../Editor/EditorEvent.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/Sandbox.h"
#include "../Project/ProjectFolders.h"
#include "EditorSandboxAssets.h"
#include "AssetManagement.h"

using namespace ECSEngine;

// -----------------------------------------------------------------------------------------------------------------------------

static void FromDeallocateAssetDependencyToUpdateAssetToComponentElement(
	CapacityStream<UpdateAssetToComponentElement>* update_assets, 
	Stream<DeallocateAssetDependency> deallocate_dependencies
) {
	for (size_t index = 0; index < deallocate_dependencies.size; index++) {
		UpdateAssetToComponentElement element;
		element.new_asset = deallocate_dependencies[index].new_pointer;
		element.old_asset = deallocate_dependencies[index].previous_pointer;
		element.type = deallocate_dependencies[index].type;
		update_assets->Add(element);
	}
}

struct DeallocateAssetWithRemappingEventData {
	Stream<Stream<unsigned int>> handles;
	bool force_execution;
};

static EDITOR_EVENT(DeallocateAssetWithRemappingEvent) {
	DeallocateAssetWithRemappingEventData* data = (DeallocateAssetWithRemappingEventData*)_data;
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) || data->force_execution) {
		size_t total_size = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			total_size += data->handles[index].size;
		}

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 4);
		CapacityStream<UpdateAssetToComponentElement> update_assets;
		update_assets.Initialize(&stack_allocator, 0, total_size);

		size_t update_index = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;

			for (size_t subindex = 0; subindex < data->handles[index].size; subindex++) {
				ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, deallocate_dependencies, 512);
				void* metadata = editor_state->asset_database->GetAsset(data->handles[index][subindex], current_type);

				// The function already emits an error if it fails to deallocate it
				DeallocateAsset(editor_state, metadata, current_type, true, &deallocate_dependencies);

				// Set the new_asset for those assets that have been changed
				update_assets.Expand(&stack_allocator, deallocate_dependencies.size);
				FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_assets, deallocate_dependencies);
			}
		}

		// Update the link components
		UpdateAssetsToComponents(editor_state, update_assets);
		return false;
	}
	else {
		return true;
	}
}

struct DeallocateAssetWithRemappingWithOldMetadataEventData {
	ECS_ASSET_TYPE type;
	bool force_execution;
private:
	char padding[6];
public:
	// Store the old_metadata directly here
	char old_metadata[AssetMetadataMaxByteSize()];
};

static EDITOR_EVENT(DeallocateAssetWithRemappingWithOldMetadataEvent) {
	DeallocateAssetWithRemappingWithOldMetadataEventData* data = (DeallocateAssetWithRemappingWithOldMetadataEventData*)_data;
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) || data->force_execution) {
		ECS_STACK_CAPACITY_STREAM(UpdateAssetToComponentElement, update_assets, 256);
		ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, deallocate_dependency, 256);

		// Deallocate the asset and then randomize it (already done in the Deallocate function)
		// The function already handles the case when it fails with an error message
		bool success = DeallocateAsset(editor_state, data->old_metadata, data->type, true, &deallocate_dependency);
		if (success) {
			FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_assets, deallocate_dependency);

			// Update the link components
			UpdateAssetsToComponents(editor_state, update_assets);
		}
		return false;
	}
	else {
		return true;
	}
}

struct DeallocateAssetWithRemappingMetadataChangeEventData {
	Stream<Stream<unsigned int>> handles;
	bool force_execution;
};

static EDITOR_EVENT(DeallocateAssetWithRemappingMetadataChangeEvent) {
	DeallocateAssetWithRemappingMetadataChangeEventData* data = (DeallocateAssetWithRemappingMetadataChangeEventData*)_data;
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) || data->force_execution) {
		size_t total_size = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			total_size += data->handles[index].size;
		}

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(update_assets_allocator, ECS_KB * 64, ECS_MB);
		CapacityStream<UpdateAssetToComponentElement> update_assets;
		update_assets.Initialize(&update_assets_allocator, 0, total_size);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_file_asset_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic file_asset_allocator = &_file_asset_allocator;

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;

			for (size_t subindex = 0; subindex < data->handles[index].size; subindex++) {
				void* metadata = editor_state->asset_database->GetAsset(data->handles[index][subindex], current_type);

				Stream<char> asset_name = GetAssetName(metadata, current_type);
				Stream<wchar_t> target_file = GetAssetFile(metadata, current_type);
				alignas(alignof(size_t)) char file_metadata[AssetMetadataMaxByteSize()];
				// Temporarly copy the name and the file
				CreateDefaultAsset(file_metadata, StringCopy(file_asset_allocator, asset_name), StringCopy(file_asset_allocator, target_file), current_type);
				bool success = editor_state->asset_database->ReadAssetFile(
					asset_name, 
					target_file, 
					file_metadata, 
					current_type,
					file_asset_allocator
				);

				auto fail = [=](const char* message_with_file, const char* message_without_file) {
					const char* type_string = ConvertAssetTypeString(current_type);

					ECS_STACK_CAPACITY_STREAM(char, console_message, 512);
					if (target_file.size > 0) {
						FormatString(console_message, message_with_file, asset_name, type_string, target_file);
					}
					else {
						FormatString(console_message, message_without_file, asset_name, type_string);
					}
					EditorSetConsoleError(console_message);
				};

				if (success) {
					if (!CompareAssetOptions(metadata, file_metadata, current_type)) {
						ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, external_dependencies, 256);

						// Deallocate the asset and then randomize it (already done in the Deallocate function)
						bool deallocate_success = DeallocateAsset(editor_state, metadata, current_type, true, &external_dependencies);
						if (deallocate_success) {
							ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, internal_dependencies, 256);
							GetAssetDependencies(metadata, current_type, &internal_dependencies);

							// Update the asset inside the asset database
							Stream<void> randomized_value = GetAssetFromMetadata(metadata, current_type);
							DeallocateAssetBase(metadata, current_type, editor_state->asset_database->Allocator());
							CopyAssetBase(metadata, file_metadata, current_type, editor_state->asset_database->Allocator());

							// Reinstate the old pointer value
							SetAssetToMetadata(metadata, current_type, randomized_value);

							// It is fine to clear the allocator since copying the old elements into the new buffer
							// is the same as copying in the same spot
							update_assets_allocator.Clear();
							update_assets.Expand(&update_assets_allocator, external_dependencies.size);
							FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_assets, external_dependencies);

							// Remove the assets that were previously dependencies for the metadata
							for (unsigned int dependency = 0; dependency < internal_dependencies.size; dependency++) {
								DecrementAssetReference(editor_state, internal_dependencies[dependency].handle, internal_dependencies[dependency].type);
							}
						}
					}
					else {
						// We don't need to remove the time stamps because they were not added
						editor_state->asset_database->RemoveAssetDependencies(file_metadata, current_type);
					}
				}
				else {
					fail(
						"Failed to read metadata file for asset {#}, type {#} and target file {#} after it changed. The in memory version was not changed.",
						"Failed to read metadata file for asset {#}, type {#} after it changed. The in memory version was not changed."
					);
				}
			}
		}

		// Update the link components
		UpdateAssetsToComponents(editor_state, update_assets);
		return false;
	}
	else {
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySandboxAssetReferences(EditorState* editor_state, unsigned int source_sandbox_index, unsigned int destination_sandbox_index) {
	const EditorSandbox* source_sandbox = GetSandbox(editor_state, source_sandbox_index);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		source_sandbox->database.ForEachAssetDuplicates((ECS_ASSET_TYPE)index, [&](unsigned int handle) {
			IncrementAssetReferenceInSandbox(editor_state, handle, (ECS_ASSET_TYPE)index, destination_sandbox_index);
		});
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetWithRemapping(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool commit)
{
	ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, handles, ECS_ASSET_TYPE_COUNT);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		handles[index] = { nullptr, 0 };
	}

	handles[type] = { &handle, 1 };
	handles.size = ECS_ASSET_TYPE_COUNT;
	DeallocateAssetsWithRemapping(editor_state, handles, commit);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetWithRemapping(EditorState* editor_state, const void* old_metadata, ECS_ASSET_TYPE type, bool commit)
{
	DeallocateAssetWithRemappingWithOldMetadataEventData event_data;
	event_data.type = type;
	memcpy(event_data.old_metadata, old_metadata, AssetMetadataByteSize(type));
	if (commit) {
		event_data.force_execution = true;
		DeallocateAssetWithRemappingWithOldMetadataEvent(editor_state, &event_data);
	}
	else {
		event_data.force_execution = false;
		EditorAddEvent(editor_state, DeallocateAssetWithRemappingWithOldMetadataEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit)
{
	DeallocateAssetWithRemappingEventData event_data;
	handles.size = ECS_ASSET_TYPE_COUNT;
	if (commit) {
		event_data.handles = handles;
		event_data.force_execution = true;
		DeallocateAssetWithRemappingEvent(editor_state, &event_data);
	}
	else {
		event_data.force_execution = false;
		event_data.handles = StreamCoalescedDeepCopy(handles, editor_state->EditorAllocator());
		EditorAddEvent(editor_state, DeallocateAssetWithRemappingEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetsWithRemappingMetadataChange(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit)
{
	DeallocateAssetWithRemappingMetadataChangeEventData event_data;
	handles.size = ECS_ASSET_TYPE_COUNT;
	if (commit) {
		event_data.handles = handles;
		event_data.force_execution = true;
		DeallocateAssetWithRemappingMetadataChangeEvent(editor_state, &event_data);
	}
	else {
		event_data.force_execution = false;
		event_data.handles = StreamCoalescedDeepCopy(handles, editor_state->EditorAllocator());
		EditorAddEvent(editor_state, DeallocateAssetWithRemappingMetadataChangeEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int FindAssetBeingLoaded(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	for (unsigned int index = 0; index < editor_state->loading_assets.size; index++) {
		if (editor_state->loading_assets[index].handle == handle && editor_state->loading_assets[index].type == type) {
			return index;
		}
	}
	return -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetAssetSandboxesInUse(
	const EditorState* editor_state, 
	const void* metadata, 
	ECS_ASSET_TYPE type, 
	CapacityStream<unsigned int>* sandboxes
)
{
	SandboxAction(editor_state, -1, [=](unsigned int sandbox_handle) {
		if (IsAssetReferencedInSandbox(editor_state, metadata, type, sandbox_handle)) {
			sandboxes->AddAssert(sandbox_handle);
		}
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxMissingAssets(const EditorState* editor_state, unsigned int sandbox_handle, CapacityStream<unsigned int>* missing_assets)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	const AssetDatabaseReference* reference = &sandbox->database;
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);
	GetDatabaseMissingAssets(editor_state->runtime_resource_manager, reference, missing_assets, assets_folder, true);

	// For each handle, verify to see if it is being already being loaded
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		for (unsigned int index = 0; index < missing_assets[asset_type].size; index++) {
			if (IsAssetBeingLoaded(editor_state, missing_assets[asset_type][index], (ECS_ASSET_TYPE)asset_type)) {
				missing_assets[asset_type].RemoveSwapBack(index);
				index--;
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

static void GetComponentWithAssetFieldsImpl(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search,
	ECS_COMPONENT_TYPE component_type
) {
	if (component_type == ECS_COMPONENT_UNIQUE) {
		entity_manager->ForEachComponent([&](Component component) {
			component_with_assets[component.value] = GetComponentWithAssetFieldForComponent(editor_state->editor_components.GetType(component, component_type), allocator, asset_types, deep_search);
		});
	}
	else if (component_type == ECS_COMPONENT_SHARED) {
		entity_manager->ForEachSharedComponent([&](Component component) {
			component_with_assets[component.value] = GetComponentWithAssetFieldForComponent(editor_state->editor_components.GetType(component, component_type), allocator, asset_types, deep_search);
		});
	}
	else if (component_type == ECS_COMPONENT_GLOBAL) {
		entity_manager->ForAllGlobalComponents([&](const void* global_data, Component component) {
			component_with_assets[component.value] = GetComponentWithAssetFieldForComponent(editor_state->editor_components.GetType(component, component_type), allocator, asset_types, deep_search);
		});
	}
	else {
		ECS_ASSERT(false, "Invalid component type");
	}
}

void GetComponentsWithAssetFieldsUnique(
	const EditorState* editor_state, 
	unsigned int sandbox_handle, 
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	GetComponentsWithAssetFieldsUnique(editor_state, ActiveEntityManager(editor_state, sandbox_handle), component_with_assets, allocator, asset_types, deep_search);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetComponentsWithAssetFieldsUnique(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	GetComponentWithAssetFieldsImpl(editor_state, entity_manager, component_with_assets, allocator, asset_types, deep_search, ECS_COMPONENT_UNIQUE);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetComponentsWithAssetFieldsShared(
	const EditorState* editor_state, 
	unsigned int sandbox_handle, 
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	GetComponentsWithAssetFieldsShared(editor_state, ActiveEntityManager(editor_state, sandbox_handle), component_with_assets, allocator, asset_types, deep_search);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetComponentsWithAssetFieldsShared(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	GetComponentWithAssetFieldsImpl(editor_state, entity_manager, component_with_assets, allocator, asset_types, deep_search, ECS_COMPONENT_SHARED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetComponentsWithAssetFieldsGlobal(
	const EditorState* editor_state, 
	unsigned int sandbox_handle, 
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator, 
	Stream<ECS_ASSET_TYPE> asset_types, 
	bool deep_search
)
{
	GetComponentsWithAssetFieldsGlobal(editor_state, ActiveEntityManager(editor_state, sandbox_handle), component_with_assets, allocator, asset_types, deep_search);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetComponentsWithAssetFieldsGlobal(
	const EditorState* editor_state, 
	const EntityManager* entity_manager, 
	ComponentWithAssetFields* component_with_assets,
	AllocatorPolymorphic allocator, 
	Stream<ECS_ASSET_TYPE> asset_types, 
	bool deep_search
)
{
	GetComponentWithAssetFieldsImpl(editor_state, entity_manager, component_with_assets, allocator, asset_types, deep_search, ECS_COMPONENT_GLOBAL);
}

// -----------------------------------------------------------------------------------------------------------------------------

SandboxReferenceCountsFromEntities GetSandboxAssetReferenceCountsFromEntities(
	const EditorState* editor_state,
	unsigned int sandbox_handle,
	EDITOR_SANDBOX_VIEWPORT viewport,
	AllocatorPolymorphic allocator
) {
	SandboxReferenceCountsFromEntities counts;
	counts.counts.Initialize(allocator, ECS_ASSET_TYPE_COUNT);
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_handle, viewport);
	GetAssetReferenceCountsFromEntitiesPrepare(counts.counts, allocator, editor_state->asset_database);
	GetAssetReferenceCountsFromEntities(entity_manager, editor_state->editor_components.internal_manager, editor_state->asset_database, counts.counts);
	return counts;
}

// -----------------------------------------------------------------------------------------------------------------------------

void IncrementAssetReference(unsigned int handle, ECS_ASSET_TYPE type, AssetDatabaseReference* reference, unsigned int count)
{
	for (unsigned int index = 0; index < count; index++) {
		reference->AddAsset(handle, type, true);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void IncrementAssetReferenceInSandbox(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, unsigned int sandbox_handle, unsigned int count)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	IncrementAssetReference(handle, type, &sandbox->database, count);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetReferencedInSandbox(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, unsigned int sandbox_handle)
{
	bool is_referenced = false;

	if (sandbox_handle == -1) {
		// We just need to return the find value from the main database - assets that have
		// dependencies will draw their dependencies into it as well
		is_referenced = editor_state->asset_database->FindAsset(name, file, type) != -1;
	}
	else {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
		AssetTypedHandle typed_handle = sandbox->database.FindDeep(name, file, type);
		is_referenced = typed_handle.handle != -1;
	}

	return is_referenced;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetReferencedInSandbox(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, unsigned int sandbox_handle)
{
	return IsAssetReferencedInSandbox(editor_state, GetAssetName(metadata, type), GetAssetFile(metadata, type), type, sandbox_handle);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetReferencedInSandboxEntities(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, unsigned int sandbox_handle)
{
	bool return_value = false;
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic allocator = &_stack_allocator;
	Stream<void> asset_pointer = GetAssetFromMetadata(metadata, type);

	SandboxAction<true>(editor_state, sandbox_handle, [&](unsigned int sandbox_handle) {
		const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_handle);
		unsigned int unique_component_count = entity_manager->GetMaxComponent().value + 1;
		unsigned int shared_component_count = entity_manager->GetMaxSharedComponent().value + 1;
		unsigned int global_component_count = entity_manager->GetGlobalComponentCount() + 1;

		unsigned int max_component_count = std::max(std::max(unique_component_count, shared_component_count), global_component_count);

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(ComponentWithAssetFields, component_with_assets, max_component_count);
		GetComponentsWithAssetFieldsUnique(editor_state, sandbox_handle, component_with_assets.buffer, allocator, { &type, 1 });

		unsigned char components_to_visit[ECS_ARCHETYPE_MAX_COMPONENTS];
		unsigned char component_to_visit_count = 0;
		const Reflection::ReflectionType* component_reflection_types[ECS_ARCHETYPE_MAX_COMPONENTS];

		SandboxForAllUniqueComponents(editor_state, sandbox_handle,
			[&](const Archetype* archetype) {
				component_to_visit_count = 0;
				ComponentSignature signature = archetype->GetUniqueSignature();
				for (unsigned char index = 0; index < signature.count; index++) {
					if (component_with_assets[signature[index]].asset_fields.size > 0) {
						components_to_visit[component_to_visit_count] = index;
						component_reflection_types[component_to_visit_count++] = editor_state->editor_components.GetType(signature[index], false);
						ECS_ASSERT(component_reflection_types[component_to_visit_count - 1] != nullptr);
					}
				}
			},
			[&](const Archetype* archetype, const ArchetypeBase* base, Entity entity, void** unique_components) {
				ComponentSignature signature = archetype->GetUniqueSignature();
				ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, 128);
				for (unsigned char index = 0; index < component_to_visit_count; index++) {
					Stream<ComponentAssetField> asset_fields = component_with_assets[signature[components_to_visit[index]]].asset_fields;
					GetComponentAssetDataDeep(
						component_reflection_types[index],
						unique_components[components_to_visit[index]],
						asset_fields,
						editor_state->asset_database,
						type,
						&asset_data
					);

					for (unsigned int subindex = 0; subindex < asset_data.size; subindex++) {
						if (CompareAssetPointers(asset_data[subindex].buffer, asset_pointer.buffer, type)) {
							return_value = true;
							return true;
						}
					}
				}
				return false;
			}
		);

		_stack_allocator.Clear();
		if (!return_value) {
			GetComponentsWithAssetFieldsShared(editor_state, sandbox_handle, component_with_assets.buffer, allocator, { &type, 1 });

			// Check the shared components as well
			SandboxForAllSharedComponents<true>(editor_state, sandbox_handle,
				[&](Component component) {
					component_reflection_types[0] = editor_state->editor_components.GetType(component, ECS_COMPONENT_SHARED);
					ECS_ASSERT(component_reflection_types[0] != nullptr);
				}, 
				[&](Component component, SharedInstance shared_instance) {
					ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, 128);
					Stream<ComponentAssetField> asset_fields = component_with_assets[component].asset_fields;
					GetComponentAssetDataDeep(
						component_reflection_types[0],
						entity_manager->GetSharedData(component, shared_instance),
						asset_fields,
						editor_state->asset_database,
						type,
						&asset_data
					);

					for (unsigned int subindex = 0; subindex < asset_data.size; subindex++) {
						if (CompareAssetPointers(asset_data[subindex].buffer, asset_pointer.buffer, type)) {
							return_value = true;
							return true;
						}
					}

					return return_value;
				}
			);
		}

		_stack_allocator.Clear();
		if (!return_value) {
			GetComponentsWithAssetFieldsGlobal(editor_state, sandbox_handle, component_with_assets.buffer, allocator, { &type, 1 });

			// Check the global components as well
			SandboxForAllGlobalComponents<true>(editor_state, sandbox_handle,
				[&](const void* data, Component component) {
					component_reflection_types[0] = editor_state->editor_components.GetType(component, ECS_COMPONENT_GLOBAL);
					ECS_ASSERT(component_reflection_types[0] != nullptr);
					ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, 128);
					Stream<ComponentAssetField> asset_fields = component_with_assets[component].asset_fields;
					GetComponentAssetDataDeep(
						component_reflection_types[0],
						data,
						asset_fields,
						editor_state->asset_database,
						type,
						&asset_data
					);

					for (unsigned int subindex = 0; subindex < asset_data.size; subindex++) {
						if (CompareAssetPointers(asset_data[subindex].buffer, asset_pointer.buffer, type)) {
							return_value = true;
							return true;
						}
					}

					return return_value;
				}
			);
		}

		return return_value;
	});

	return return_value;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetBeingLoaded(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	return FindAssetBeingLoaded(editor_state, handle, type) != -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_handle, CapacityStream<unsigned int>* missing_assets)
{
	LoadEditorAssetsOptionalData optional_data;
	LoadSandboxMissingAssets(editor_state, sandbox_handle, missing_assets, &optional_data);
}

void LoadSandboxMissingAssets(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	CapacityStream<unsigned int>* missing_assets, 
	LoadEditorAssetsOptionalData* optional_data
)
{
	optional_data->sandbox_handle = sandbox_handle;
	LoadEditorAssets(editor_state, missing_assets, optional_data);
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_handle)
{
	LoadEditorAssetsOptionalData optional_data;
	LoadSandboxAssets(editor_state, sandbox_handle, &optional_data);
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_handle, LoadEditorAssetsOptionalData* optional_data)
{
	const size_t HANDLES_PER_ASSET_TYPE = ECS_KB;
	ECS_STACK_CAPACITY_STREAM_OF_STREAMS(unsigned int, missing_assets, ECS_ASSET_TYPE_COUNT, HANDLES_PER_ASSET_TYPE);
	GetSandboxMissingAssets(editor_state, sandbox_handle, missing_assets.buffer);
	LoadSandboxMissingAssets(editor_state, sandbox_handle, missing_assets.buffer, optional_data);
}

// -----------------------------------------------------------------------------------------------------------------------------

// The registration needs to be moved into an event in case the resource manager is locked
bool RegisterSandboxAsset(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	Stream<char> name, 
	Stream<wchar_t> file, 
	ECS_ASSET_TYPE type, 
	const RegisterAssetTarget& asset_target,
	bool unregister_if_exists,
	UIActionHandler callback,
	bool callback_is_single_threaded
)
{
	return AddRegisterAssetEvent(editor_state, name, file, type, asset_target, sandbox_handle, unregister_if_exists, callback, callback_is_single_threaded);
}

// -----------------------------------------------------------------------------------------------------------------------------

static void FinishReloadAsset(EditorState* editor_state, Stream<UpdateAssetToComponentElement> update_elements, Stream<unsigned int> dirty_sandboxes) {
	if (update_elements.size > 0) {
		UpdateAssetsToComponents(editor_state, update_elements);

		// Now we need to get all sandboxes which use these elements and re-render them
		ECS_STACK_CAPACITY_STREAM(unsigned int, update_dirty_sandboxes, 512);
		for (size_t index = 0; index < update_elements.size; index++) {
			ECS_STACK_CAPACITY_STREAM(unsigned int, current_sandboxes, 512);
			unsigned int handle = editor_state->asset_database->FindAssetEx(update_elements[index].new_asset, update_elements[index].type);
			if (handle == -1) {
				handle = editor_state->asset_database->FindAssetEx(update_elements[index].old_asset, update_elements[index].type);
			}

			// It might happen that the asset is invalid before hand and it still is invalid
			// In that case, don't do anything
			if (handle != -1) {
				const void* metadata = editor_state->asset_database->GetAssetConst(handle, update_elements[index].type);
				GetAssetSandboxesInUse(editor_state, metadata, update_elements[index].type, &current_sandboxes);

				StreamAddUniqueSearchBytes(update_dirty_sandboxes, current_sandboxes);
			}
		}

		// Proceed with the re-rendering of the sandboxes which use these assets
		// Re-render both the scene and the runtime
		for (unsigned int index = 0; index < update_dirty_sandboxes.size; index++) {
			RenderSandboxViewports(editor_state, update_dirty_sandboxes[index]);
		}
	}
	
	// Now re-render the external dirty sandboxes
	for (unsigned int index = 0; index < dirty_sandboxes.size; index++) {
		RenderSandboxViewports(editor_state, dirty_sandboxes[index]);
	}
}

// Returns a component with type ECS_ASSET_TYPE_COUNT when there is nothing to be update
static UpdateAssetToComponentElement ReloadAssetTaskIteration(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE asset_type, Stream<wchar_t> assets_folder) {
	void* metadata = editor_state->asset_database->GetAsset(handle, asset_type);
	Stream<void> old_asset = GetAssetFromMetadata(metadata, asset_type);

	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, assets_to_be_removed, 512);
	bool2 success = ReloadAssetFromMetadata(editor_state->RuntimeResourceManager(), editor_state->asset_database, metadata, asset_type, assets_folder, &assets_to_be_removed);
	
	for (size_t index = 0; index < assets_to_be_removed.size; index++) {
		DecrementAssetReference(editor_state, assets_to_be_removed[index].handle, assets_to_be_removed[index].type);
	}

	if (!success.x || !success.y) {
		Stream<char> asset_name = GetAssetName(metadata, asset_type);
		Stream<wchar_t> asset_file = GetAssetFile(metadata, asset_type);
		Stream<char> asset_type_string = ConvertAssetTypeString(asset_type);
		ECS_STACK_CAPACITY_STREAM(char, console_message, 512);

		Stream<char> detailed_part = !success.x ? "The deallocation part failed." : "The creation part failed.";

		if (asset_file.size > 0) {
			FormatString(console_message, "Failed to reload asset {#}, target file {#}, type {#}. {#}", asset_name, asset_file, asset_type_string, detailed_part);
		}
		else {
			FormatString(console_message, "Failed to reload asset {#}, type {#}. {#}", asset_name, asset_type_string, detailed_part);
		}
		EditorSetConsoleError(console_message);

		if (success.x) {
			// The deallocation went through - randomize the pointer and update the component
			unsigned int randomized_value = editor_state->asset_database->RandomizePointer(handle, asset_type);
			UpdateAssetToComponentElement update_element;
			update_element.old_asset = old_asset;
			update_element.new_asset = { (void*)randomized_value, 0 };
			update_element.type = asset_type;

			return update_element;
		}
	}
	else {
		UpdateAssetToComponentElement update_element;
		update_element.old_asset = old_asset;
		update_element.new_asset = GetAssetFromMetadata(metadata, asset_type);
		update_element.type = asset_type;

		return update_element;
	}

	return { {}, {}, ECS_ASSET_TYPE_COUNT };
}

struct ReloadEventData {
	Stream<Stream<unsigned int>> asset_handles;
};

static EDITOR_EVENT(ReloadEvent) {
	ReloadEventData* data = (ReloadEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		ECS_STACK_CAPACITY_STREAM(UpdateAssetToComponentElement, update_elements, 1024);

		// Go through each asset and reload it individually
		for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)type;
			for (size_t index = 0; index < data->asset_handles[asset_type].size; index++) {
				unsigned int handle = data->asset_handles[asset_type][index];
				UpdateAssetToComponentElement update_element = ReloadAssetTaskIteration(editor_state, handle, asset_type, assets_folder);
				if (update_element.type != ECS_ASSET_TYPE_COUNT && update_element.IsAssetDifferent()) {
					update_elements.AddAssert(update_element);
				}
			}
		}

		FinishReloadAsset(editor_state, update_elements, {});
		// Remove the loading assets entries
		RemoveLoadingAssets(editor_state, data->asset_handles);

		data->asset_handles.Deallocate(editor_state->EditorAllocator());
		return false;
	}
	else {
		return true;
	}
}

void ReloadAssets(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload)
{
	// In order to make sure that there are no conflicts, for reloads take into considerations
	// Loads as well. In this way, we avoid load-reload conflicts (for assets that failed, this
	// is the only case where it can happen) but also reload-reload conflicts
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	bool has_entries = false;
	Stream<Stream<unsigned int>> not_loaded_assets = GetNotLoadedAssets(editor_state, &stack_allocator, assets_to_reload, &has_entries);

	if (has_entries) {
		// Add the entries to the load array
		AddLoadingAssets(editor_state, not_loaded_assets);

		ReloadEventData event_data;
		event_data.asset_handles = StreamCoalescedDeepCopy(not_loaded_assets, editor_state->EditorAllocator());
		EditorAddEvent(editor_state, ReloadEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

struct ReloadAssetsMetadataChangeEventData {
	Stream<Stream<unsigned int>> asset_handles;
};

static EDITOR_EVENT(ReloadAssetsMetadataChangeEvent) {
	ReloadAssetsMetadataChangeEventData* data = (ReloadAssetsMetadataChangeEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		ECS_STACK_CAPACITY_STREAM(UpdateAssetToComponentElement, update_elements, 1024);
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, old_dependencies, 512);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);

		// This is a separate allocator in order to use clear on the stack allocator inside the loop
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 8, ECS_MB);
		ECS_STACK_CAPACITY_STREAM(CapacityStream<unsigned int>, initial_assets, ECS_ASSET_TYPE_COUNT);
		ECS_STACK_CAPACITY_STREAM(CapacityStream<unsigned int>, new_assets_to_add, ECS_ASSET_TYPE_COUNT);
		for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
			initial_assets[type].Initialize(&temporary_allocator, 0, 64);
			initial_assets[type].CopyOther(data->asset_handles[type]);
			new_assets_to_add[type].Initialize(&temporary_allocator, 0, 64);
		}

		// Keep track of the sandboxes that use these assets and re-render them at the end
		ECS_STACK_CAPACITY_STREAM(unsigned int, dirty_sandboxes, 512);

		auto calculate_remaining_assets = [](Stream<CapacityStream<unsigned int>> asset_handles) {
			unsigned int total = 0;
			for (unsigned int index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				total += asset_handles[index].size;
			}
			return total;
		};

		auto loop = [&](Stream<CapacityStream<unsigned int>> asset_handles, Stream<CapacityStream<unsigned int>> assets_to_add, auto initial_reload) {
			// Go through each asset and reload it individually
			auto add_external_dependencies = [&](const void* metadata, ECS_ASSET_TYPE asset_type) {
				Stream<Stream<unsigned int>> external_dependencies = editor_state->asset_database->GetDependentAssetsFor(
					metadata,
					asset_type,
					&stack_allocator
				);
				for (size_t subtype = 0; subtype < ECS_ASSET_TYPE_COUNT; subtype++) {
					assets_to_add[subtype].AddStreamSafe(external_dependencies[subtype]);
				}
			};

			// Empty the assets to add
			for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
				assets_to_add[type].size = 0;
			}

			for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
				ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)type;

				for (size_t index = 0; index < asset_handles[asset_type].size; index++) {
					unsigned int handle = asset_handles[asset_type][index];
					void* metadata = editor_state->asset_database->GetAsset(handle, asset_type);

					// Add the referenced sandboxes to the re-render list
					ECS_STACK_CAPACITY_STREAM(unsigned int, current_asset_sandboxes, 512);
					GetAssetSandboxesInUse(editor_state, metadata, asset_type, &current_asset_sandboxes);
					StreamAddUniqueSearchBytes(dirty_sandboxes, current_asset_sandboxes);

					if constexpr (!initial_reload) {
						UpdateAssetToComponentElement update_element = ReloadAssetTaskIteration(editor_state, handle, asset_type, assets_folder);
						if (update_element.type != ECS_ASSET_TYPE_COUNT) {
							update_elements.AddAssert(update_element);

							// If the new pointer is not randomized, then it means that the reload was successful
							// In that case add the external dependencies to the assets_to_add
							if (IsAssetFromMetadataValid(metadata, asset_type)) {
								add_external_dependencies(metadata, asset_type);
							}
						}
					}
					else {
						Stream<void> old_asset = GetAssetFromMetadata(metadata, asset_type);

						old_dependencies.size = 0;
						GetAssetDependencies(metadata, asset_type, &old_dependencies);

						Stream<char> current_name = GetAssetName(metadata, asset_type);
						Stream<wchar_t> current_file = GetAssetFile(metadata, asset_type);

						alignas(alignof(void*)) char file_metadata[AssetMetadataMaxByteSize()];
						CreateDefaultAsset(file_metadata, current_name, current_file, asset_type);

						stack_allocator.Clear();

						// Read the metadata file
						bool success = editor_state->asset_database->ReadAssetFile(
							current_name,
							current_file,
							file_metadata,
							asset_type,
							&stack_allocator
						);
						if (success) {
							// Set the pointer of the file metadata to the one in the database and then update the database
							SetAssetToMetadata(file_metadata, asset_type, old_asset);

							if (ValidateAssetMetadataOptions(file_metadata, asset_type)) {
								ReloadAssetResult reload_result = ReloadAssetFromMetadata(
									editor_state->RuntimeResourceManager(),
									editor_state->asset_database,
									metadata,
									file_metadata,
									asset_type,
									assets_folder
								);
								if (reload_result.is_different) {
									if (!reload_result.success.x || !reload_result.success.y) {
										Stream<char> asset_type_string = ConvertAssetTypeString(asset_type);
										ECS_STACK_CAPACITY_STREAM(char, console_message, 512);

										Stream<char> detailed_part = !reload_result.success.x ? "The deallocation part failed." : "The creation part failed.";

										if (current_file.size > 0) {
											FormatString(console_message, "Failed to reload asset {#}, target file {#}, type {#}. {#}",
												current_name, current_file, asset_type_string, detailed_part);
										}
										else {
											FormatString(console_message, "Failed to reload asset {#}, type {#}. {#}", current_name, asset_type_string, detailed_part);
										}
										EditorSetConsoleError(console_message);

										if (reload_result.success.x) {
											// The assets has been deallocated, we need to randomize the pointer and update the components
											unsigned int randomized_value = editor_state->asset_database->RandomizePointer(handle, asset_type);
											UpdateAssetToComponentElement update_element;
											update_element.old_asset = old_asset;
											update_element.new_asset = { (void*)randomized_value, 0 };
											update_element.type = asset_type;

											update_elements.AddAssert(update_element);
										}

										// We need to remove the dependencies of the file metadata
										// We don't need to remove the time stamps because they were not added
										editor_state->asset_database->RemoveAssetDependencies(file_metadata, asset_type);
									}
									else {
										UpdateAssetToComponentElement update_element;
										update_element.old_asset = old_asset;
										update_element.new_asset = GetAssetFromMetadata(file_metadata, asset_type);
										update_element.type = asset_type;

										// For some assets the pointer might not change
										if (update_element.new_asset.buffer != old_asset.buffer) {
											update_elements.AddAssert(update_element);
										}

										// We need to remove the references to the old dependencies
										// If an asset was removed from the dependency list and is being kept alive
										// only by this reference it will cause an unnecessary unload. So update the metadata
										// in the database first before proceeding

										// Copy the asset into a temporary, copy the new asset fields and then deallocate the old one
										// otherwise the old data can be deallocated and when reallocating the values to be overwritten
										size_t temporary_metadata[AssetMetadataMaxSizetSize()];
										memcpy(temporary_metadata, metadata, AssetMetadataByteSize(asset_type));
										CopyAssetBase(metadata, file_metadata, asset_type, editor_state->asset_database->Allocator());

										DeallocateAssetBase(temporary_metadata, asset_type, editor_state->asset_database->Allocator());

										SetAssetToMetadata(metadata, asset_type, GetAssetFromMetadata(file_metadata, asset_type));

										for (unsigned int dependency = 0; dependency < old_dependencies.size; dependency++) {
											DecrementAssetReference(editor_state, old_dependencies[dependency].handle, old_dependencies[dependency].type);
										}

										// We also need to get the internal dependencies for the file metadata and insert time stamps for
										// assets that have been missing
										ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, file_internal_dependencies, 128);
										GetAssetDependencies(file_metadata, asset_type, &file_internal_dependencies);

										for (unsigned int subindex = 0; subindex < file_internal_dependencies.size; subindex++) {
											InsertAssetTimeStamp(editor_state, file_internal_dependencies[subindex].handle, file_internal_dependencies[subindex].type, true);
										}
									}
									
									// The external dependencies need to be added no matter what if the asset succeeded or not
									add_external_dependencies(metadata, asset_type);
								}
								else {
									// We need to remove the reference count additions made by the file read
									// We don't need to remove the time stamps because they were not added
									editor_state->asset_database->RemoveAssetDependencies(file_metadata, asset_type);
								}
							}
							else {
								// Remove the reference count increments for internal dependencies by the previous metadata
								// And deallocate the previous data. Also we need to create any dependencies that the new asset
								// brought with it and were not loaded before
								ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, deallocate_dependencies, 512);
								ECS_STACK_CAPACITY_STREAM(CreateAssetInternalDependenciesElement, create_dependencies, 512);

								DeallocateAsset(editor_state, metadata, asset_type, true, &deallocate_dependencies);
								FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_elements, deallocate_dependencies);

								bool internal_success = CreateAssetInternalDependencies(editor_state, file_metadata, asset_type, &create_dependencies);
								for (unsigned int dependency = 0; dependency < create_dependencies.size; dependency++) {
									if (create_dependencies[dependency].WasChanged()) {
										update_elements.AddAssert({
											create_dependencies[dependency].old_asset,
											create_dependencies[dependency].new_asset,
											create_dependencies[dependency].type
										});
									}
								}

								// This value should be randomized invalid pointer - since the asset was deallocated
								Stream<void> previous_asset_pointer = GetAssetFromMetadata(metadata, asset_type);

								// We don't need to remove the time stamps because they were not added
								editor_state->asset_database->RemoveAssetDependencies(metadata, asset_type);

								// Copy back into the database the new asset
								DeallocateAssetBase(metadata, asset_type, editor_state->asset_database->Allocator());
								CopyAssetBase(metadata, file_metadata, asset_type, editor_state->asset_database->Allocator());

								SetAssetToMetadata(metadata, asset_type, previous_asset_pointer);
							}
						}
					}
				}
			}
		};


		loop(initial_assets, new_assets_to_add, std::true_type{});

		// 0 means use initial assets for the main one, 1 means use new assets_to_add 
		unsigned char asset_handle_index = 1;
		unsigned int initial_assets_count = 0;
		unsigned int new_assets_to_add_count = calculate_remaining_assets(new_assets_to_add);
		while (initial_assets_count > 0 || new_assets_to_add_count > 0) {
			if (asset_handle_index == 0) {
				loop(initial_assets, new_assets_to_add, std::false_type{});

				initial_assets_count = 0;
				new_assets_to_add_count = calculate_remaining_assets(new_assets_to_add);
				asset_handle_index = 1;
			}
			else {
				loop(new_assets_to_add, initial_assets, std::false_type{});

				initial_assets_count = calculate_remaining_assets(initial_assets);
				new_assets_to_add_count = 0;
				asset_handle_index = 0;
			}
		}

		FinishReloadAsset(editor_state, update_elements, dirty_sandboxes);
		// Also remove the entries from the loading array
		RemoveLoadingAssets(editor_state, data->asset_handles);

		return false;
	}
	else {
		return true;
	}
}

void ReloadAssetsMetadataChange(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload)
{
	// The same as the other reload, check the loading assets entries to avoid load-reload or reload-reload conflicts
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	bool has_entries = false;
	Stream<Stream<unsigned int>> not_loaded_assets = GetNotLoadedAssets(editor_state, &stack_allocator, assets_to_reload, &has_entries);

	if (has_entries) {
		AddLoadingAssets(editor_state, not_loaded_assets);

		ReloadAssetsMetadataChangeEventData event_data;
		event_data.asset_handles = StreamCoalescedDeepCopy(not_loaded_assets, editor_state->MultithreadedEditorAllocator());
		EditorAddEvent(editor_state, ReloadAssetsMetadataChangeEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

struct ReplaySandboxAssetsCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_handle;
};

Stream<void> GetReplaySandboxAssetsCallbackData(EditorState* editor_state, unsigned int sandbox_handle, AllocatorPolymorphic temporary_allocator) {
	ReplaySandboxAssetsCallbackData* data = Allocate<ReplaySandboxAssetsCallbackData>(temporary_allocator);
	data->editor_state = editor_state;
	data->sandbox_handle = sandbox_handle;
	return data;
}

bool ReplaySandboxAssetsCallback(SceneDeltaReaderAssetCallbackData* functor_data) {
	ReplaySandboxAssetsCallbackData* data = (ReplaySandboxAssetsCallbackData*)functor_data->user_data.buffer;
	EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_handle);

	ECS_STACK_CAPACITY_STREAM_OF_STREAMS(unsigned int, assets_to_be_added, ECS_ASSET_TYPE_COUNT, ECS_KB);
	
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	ResizableStream<AssetTypedHandle> assets_to_be_unloaded(&stack_allocator, 16);

	// In order to simplify the code for the full snapshot case, obtain the current sandbox database full snapshot
	// And then determine the delta between the functor full snapshot and the current one. In this way we can have a common
	// Code path that uses only the delta change.

	AssetDatabaseDeltaChange computed_delta_change;
	const AssetDatabaseDeltaChange* delta_change = nullptr;
	if (!functor_data->is_delta_change) {
		AssetDatabaseFullSnapshot current_database_snapshot = sandbox->database.GetFullSnapshot(&stack_allocator);
		computed_delta_change = DetermineAssetDatabaseDeltaChange(current_database_snapshot, *functor_data->full_snapshot, &stack_allocator, true);
		delta_change = &computed_delta_change;
	}
	else {
		delta_change = functor_data->delta_change;
	}

	for (size_t type_index = 0; type_index < ECS_ASSET_TYPE_COUNT; type_index++) {
		ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)type_index;

		for (size_t index = 0; index < delta_change->entries[type_index].size; index++) {
			const auto& entry = delta_change->entries[type_index][index];
			if (entry.reference_count_delta < 0) {
				unsigned int asset_handle = data->editor_state->asset_database->FindAsset(entry.name, entry.file, asset_type);
				if (asset_handle != -1) {
					// Check to see if the reference count is matched - the normal unload will pick up on this,
					// But it will emit a weird error message that might be misinterpreted.
					unsigned int sandbox_reference_count = sandbox->database.GetReferenceCountInInstance(asset_handle, asset_type);
					if (sandbox_reference_count < (unsigned int)-entry.reference_count_delta) {
						// Emit a warning
						ECS_STACK_CAPACITY_STREAM(char, asset_string, 512);
						AssetToString(entry.name, entry.file, asset_string);
						ECS_FORMAT_TEMP_STRING(console_message, "Sandbox {#} replay wants to unload {#} {#}, but the reference count exceeds the current sandbox reference count", 
							data->sandbox_handle, ConvertAssetTypeString(asset_type), asset_string);
						EditorSetConsoleError(console_message);

						// TODO: Determine if we want to error here or not?
						return false;
					}
					else {
						// Iterate downwards - the same as iterating upwards but with the delta negated
						for (int reference_index = 0; reference_index > entry.reference_count_delta; reference_index--) {
							assets_to_be_unloaded.Add({ asset_handle, asset_type });
						}
					}
				}
				else {
					ECS_STACK_CAPACITY_STREAM(char, asset_string, 512);
					AssetToString(entry.name, entry.file, asset_string);
					ECS_FORMAT_TEMP_STRING(console_message, "Sandbox {#} replay wants to unload {#} {#}, but it doesn't exist.", 
						data->sandbox_handle, ConvertAssetTypeString(asset_type), asset_string);
					EditorSetConsoleError(console_message);

					// TODO: Determine if we want to error here or not?
					return false;
				}
			}
			else {
				// The asset is added. Don't use RegisterSandboxAsset to load it since that is single threaded.
				// Use LoadSandboxMissingAssets at the end since it does that multithreadedly
				// We have to add it to the database ourselves here. Do it once initially to obtain the handle value,
				// And then use the handle add for a reference_count - 1.
				bool is_loaded_now = false;
				unsigned asset_handle = sandbox->database.AddAsset(entry.name, entry.file, asset_type, &is_loaded_now);
				for (unsigned int reference_index = 0; reference_index < (unsigned int)entry.reference_count_delta - 1; reference_index++) {
					sandbox->database.AddAsset(asset_handle, asset_type, true);
				}
					
				// If it was loaded now, we must load the asset itself
				if (is_loaded_now) {
					assets_to_be_added[type_index].Add(asset_handle);
				}
			}
		}
	}

	// Perform the bulk asset removal and load

	// At the moment, there is no callback for this
	UnregisterSandboxAsset(data->editor_state, data->sandbox_handle, assets_to_be_unloaded);

	// No callback for this momentarily
	LoadSandboxMissingAssets(data->editor_state, data->sandbox_handle, assets_to_be_added.buffer);
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_handle, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback)
{
	AssetTypedHandle element = { handle, type };
	UnregisterSandboxAsset(editor_state, sandbox_handle, Stream<AssetTypedHandle>{ &element, 1 }, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_handle, Stream<AssetTypedHandle> elements, UIActionHandler callback)
{
	AddUnregisterAssetEvent(editor_state, elements, true, sandbox_handle, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_handle, Stream<Stream<unsigned int>> elements, UIActionHandler callback) 
{
	AddUnregisterAssetEventHomogeneous(editor_state, elements, true, sandbox_handle, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxLinkComponent(EditorState* editor_state, unsigned int sandbox_handle, const void* link_component, Stream<char> component_name)
{
	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
	ECS_ASSERT(type != nullptr);

	ECS_STACK_CAPACITY_STREAM(ComponentAssetField, asset_fields, 128);
	GetAssetFieldsFromComponent(type, asset_fields);

	ECS_STACK_CAPACITY_STREAM(unsigned int, handles, 128);
	GetComponentHandles(type, link_component, editor_state->asset_database, asset_fields, handles);

	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, unload_elements, 128);

	for (unsigned int index = 0; index < handles.size; index++) {
		if (handles[index] != -1) {
			// Unload it
			unload_elements.Add({ handles[index], asset_fields[index].type.type });
		}
	}

	UnregisterSandboxAsset(editor_state, sandbox_handle, unload_elements);
}

// -----------------------------------------------------------------------------------------------------------------------------

struct UnloadSandboxAssetsEventData {
	unsigned int sandbox_handle;
	Stream<unsigned int> asset_handles[ECS_ASSET_TYPE_COUNT];
};

EDITOR_EVENT(UnloadSandboxAssetsEvent) {
	UnloadSandboxAssetsEventData* data = (UnloadSandboxAssetsEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		for (size_t type_index = 0; type_index < ECS_ASSET_TYPE_COUNT; type_index++) {
			for (size_t index = 0; index < data->asset_handles[type_index].size; index++) {
				// An error message will be printed if an error has occured
				DecrementAssetReference(editor_state, data->asset_handles[type_index][index], (ECS_ASSET_TYPE)type_index);
			}

			// Deallocate the arrays
			if (data->asset_handles[type_index].size > 0) {
				data->asset_handles[type_index].Deallocate(editor_state->EditorAllocator());
			}
		}

		return false;
	}
	else {
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnloadSandboxAssets(EditorState* editor_state, unsigned int sandbox_handle)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	UnloadSandboxAssetsEventData event_data;
	event_data.sandbox_handle = sandbox_handle;
	UnloadSandboxAssetsEventData* event_data_ptr = (UnloadSandboxAssetsEventData*)EditorAddEvent(editor_state, UnloadSandboxAssetsEvent, &event_data, sizeof(event_data));

	// Copy the asset handles into separate allocations
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
		unsigned int current_count = sandbox->database.GetCount(current_type);
		event_data_ptr->asset_handles[current_type].Initialize(editor_state->editor_allocator, current_count);
		for (unsigned int subindex = 0; subindex < current_count; subindex++) {
			event_data_ptr->asset_handles[current_type][subindex] = sandbox->database.GetHandle(subindex, current_type);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateAssetToComponents(EditorState* editor_state, Stream<void> old_asset, Stream<void> new_asset, ECS_ASSET_TYPE asset_type, unsigned int sandbox_handle)
{
	UpdateAssetToComponentElement update_element;
	update_element.old_asset = old_asset;
	update_element.new_asset = new_asset;
	update_element.type = asset_type;
	UpdateAssetsToComponents(editor_state, { &update_element, 1 }, sandbox_handle);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateAssetsToComponents(EditorState* editor_state, Stream<UpdateAssetToComponentElement> elements, unsigned int sandbox_handle)
{
	if (elements.size == 0) {
		return;
	}

	SandboxAction(editor_state, -1, [&](unsigned int sandbox_handle) {
		// If we are in run mode, we need to update both entity managers. If in scene mode, only the scene entity manager
		EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_handle);
		EntityManager* scene_entity_manager = GetSandboxEntityManager(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		UpdateAssetsToComponents(editor_state, elements, scene_entity_manager);
		if (sandbox_state != EDITOR_SANDBOX_SCENE) {
			EntityManager* runtime_entity_manager = GetSandboxEntityManager(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
			UpdateAssetsToComponents(editor_state, elements, runtime_entity_manager);
		}
	});
}

void UpdateAssetsToComponents(EditorState* editor_state, Stream<UpdateAssetToComponentElement> elements, EntityManager* entity_manager)
{
	UpdateAssetsToComponents(editor_state->GlobalReflectionManager(), entity_manager, elements);
}

// -----------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------
