#include "editorpch.h"
#include "../Editor/EditorState.h"
#include "ECSEngineComponents.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorScene.h"

#define LAZY_CHANGE_MS 300

ECS_INLINE static AllocatorPolymorphic PrefabAllocator(const EditorState* editor_state) {
	return GetAllocatorPolymorphic(&editor_state->prefabs_allocator);
}

static Stream<wchar_t> GetPrefabAbsolutePath(const EditorState* editor_state, Stream<wchar_t> path, CapacityStream<wchar_t> storage) {
	GetProjectAssetsFolder(editor_state, storage);
	storage.Add(ECS_OS_PATH_SEPARATOR);
	storage.AddStreamAssert(path);
	return storage;
}

unsigned int AddPrefabID(EditorState* editor_state, Stream<wchar_t> path, unsigned int increment_count) {
	unsigned int existing_id = FindPrefabID(editor_state, path);
	if (existing_id == -1) {
		// We need to create a new entry
		path = path.Copy(PrefabAllocator(editor_state));
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);
		Stream<wchar_t> absolute_path = GetPrefabAbsolutePath(editor_state, path, absolute_path_storage);

		size_t last_write = -1;
		Stream<void> prefab_data = {};
		if (ExistsFileOrFolder(absolute_path)) {
			last_write = OS::GetFileLastWrite(absolute_path);
			prefab_data = ReadWholeFileBinary(absolute_path, PrefabAllocator(editor_state));
		}

		return editor_state->prefabs.Add({ 1, path, last_write, prefab_data });
	}
	else {
		IncrementPrefabID(editor_state, existing_id, increment_count);
		return existing_id;
	}
}

void AddPrefabComponentToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<wchar_t> relative_assets_path)
{
	unsigned int id = AddPrefabID(editor_state, relative_assets_path);
	AddSandboxEntityComponent(editor_state, sandbox_index, entity, STRING(PrefabComponent));
	PrefabComponent* component = GetSandboxEntityComponent<PrefabComponent>(editor_state, sandbox_index, entity);
	component->id = id;
	component->detached = false;
}

void AddPrefabComponentToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int id)
{
	IncrementPrefabID(editor_state, id);
	AddSandboxEntityComponent(editor_state, sandbox_index, entity, STRING(PrefabComponent));
	PrefabComponent* component = GetSandboxEntityComponent<PrefabComponent>(editor_state, sandbox_index, entity);
	component->id = id;
	component->detached = false;
}

unsigned int DecrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count)
{
	editor_state->prefabs[id].reference_count -= decrement_count;
	return editor_state->prefabs[id].reference_count;
}

bool ExistsPrefabFile(const EditorState* editor_state, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);
	Stream<wchar_t> absolute_path = GetPrefabAbsolutePath(editor_state, path, absolute_path_storage);
	return ExistsFileOrFolder(absolute_path);
}

unsigned int FindPrefabID(const EditorState* editor_state, Stream<wchar_t> path) {
	return editor_state->prefabs.Find(PrefabInstance{ 0, path });
}

Stream<wchar_t> GetPrefabPath(const EditorState* editor_state, unsigned int id)
{
	return editor_state->prefabs[id].path;
}

Stream<wchar_t> GetPrefabAbsolutePath(const EditorState* editor_state, unsigned int id, CapacityStream<wchar_t> storage)
{
	return GetPrefabAbsolutePath(editor_state, GetPrefabPath(editor_state, id), storage);
}

unsigned int IncrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int increment_count) {
	editor_state->prefabs[id].reference_count += increment_count;
	return editor_state->prefabs[id].reference_count;
}

unsigned int RemovePrefabID(EditorState* editor_state, Stream<wchar_t> path, unsigned int decrement_count) {
	unsigned int id = FindPrefabID(editor_state, path);
	ECS_ASSERT(id != -1);
	return RemovePrefabID(editor_state, id);
}

unsigned int RemovePrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count) {
	PrefabInstance& instance = editor_state->prefabs[id];
	ECS_ASSERT(instance.reference_count >= decrement_count);
	unsigned int reference_count = DecrementPrefabID(editor_state, id, decrement_count);
	if (reference_count == 0) {
		instance.path.Deallocate(PrefabAllocator(editor_state));
		if (instance.prefab_file_data.size > 0) {
			Deallocate(PrefabAllocator(editor_state), instance.prefab_file_data.buffer);
		}
		editor_state->prefabs.RemoveSwapBack(id);
	}
	return instance.reference_count;
}

#define PREFAB_CHANGE_MAX_FIELDS_CHANGE 32

enum PREFAB_CHANGE_TYPE : unsigned char {
	PREFAB_ADD,
	PREFAB_REMOVE,
	PREFAB_UPDATE
};

struct PrefabChange {
	Component component;
	PREFAB_CHANGE_TYPE type;
	bool is_shared;
	// This is relevant only for the update case, where we will record the change set
	Stream<Reflection::ReflectionTypeChange> updated_fields;
};

// The allocator is needed to make the allocations for the update set
static bool DetermineUpdateChangesForComponent(
	const EditorState* editor_state,
	const Reflection::ReflectionType* component_type,
	const void* previous_data,
	const void* new_data,
	PrefabChange* change,
	AllocatorPolymorphic allocator
) {
	ResizableStream<Reflection::ReflectionTypeChange> changes{ allocator, 0 };
	AdditionStream<Reflection::ReflectionTypeChange> addition_stream = &changes;
	Reflection::DetermineReflectionTypeInstanceUpdates(
		editor_state->GlobalReflectionManager(),
		component_type,
		previous_data,
		new_data,
		addition_stream
	);
	if (addition_stream.Size() > 0) {
		change->type = PREFAB_UPDATE;
		change->updated_fields = changes.ToStream();
		return true;
	}
	else {
		return false;
	}
}

// The allocator is needed to make the allocations for the update set
static void DeterminePrefabChanges(
	const EditorState* editor_state,
	const EntityManager* previous_entity_manager,
	const EntityManager* new_entity_manager,
	CapacityStream<PrefabChange>* changes,
	AllocatorPolymorphic allocator
) {
	Entity prefab_entity = GetPrefabEntityFromSingle();
	
	// Firstly, determine the components that were added/removed and then
	// Those that have had their fields changed
	ComponentSignature previous_unique_signature = previous_entity_manager->GetEntitySignatureStable(prefab_entity);
	ComponentSignature previous_shared_signature = previous_entity_manager->GetEntitySharedSignatureStable(prefab_entity).ComponentSignature();

	ComponentSignature new_unique_signature = new_entity_manager->GetEntitySignatureStable(prefab_entity);
	ComponentSignature new_shared_signature = new_entity_manager->GetEntitySharedSignatureStable(prefab_entity).ComponentSignature();

	// Loop through the new components and determine the additions
	auto register_additions = [changes](ComponentSignature previous_components, ComponentSignature new_components, bool is_shared) {
		for (unsigned char index = 0; index < new_components.count; index++) {
			if (previous_components.Find(new_components[index]) == UCHAR_MAX) {
				unsigned int change_index = changes->Reserve();
				changes->buffer[change_index].type = PREFAB_ADD;
				changes->buffer[change_index].is_shared = is_shared;
				changes->buffer[change_index].component = new_components[index];
			}
		}
	};
	register_additions(previous_unique_signature, new_unique_signature, false);
	register_additions(previous_shared_signature, new_shared_signature, true);

	// Loop through the previous components and determine the removals
	auto register_removals = [changes](ComponentSignature previous_components, ComponentSignature new_components, bool is_shared) {
		for (unsigned char index = 0; index < previous_components.count; index++) {
			if (new_components.Find(previous_components[index]) == UCHAR_MAX) {
				unsigned int change_index = changes->Reserve();
				changes->buffer[change_index].type = PREFAB_REMOVE;
				changes->buffer[change_index].is_shared = is_shared;
				changes->buffer[change_index].component = previous_components[index];
			}
		}
	};
	register_removals(previous_unique_signature, new_unique_signature, false);
	register_removals(previous_shared_signature, new_shared_signature, true);

	// Now determine the updates. We need separate algorithms for unique and shared
	// Since the shared part is a little bit more involved
	auto register_updates = [&](ComponentSignature previous_components, ComponentSignature new_components, bool is_shared) {
		for (unsigned int index = 0; index < previous_components.count; index++) {
			unsigned char new_index = new_components.Find(previous_components[index]);
			if (new_index != UCHAR_MAX) {
				const void* previous_data = nullptr;
				const void* new_data = nullptr;
				const Reflection::ReflectionType* component_type = nullptr;
				PrefabChange prefab_change;
				prefab_change.is_shared = is_shared;
				prefab_change.component = new_components[new_index];

				if (is_shared) {
					previous_data = previous_entity_manager->GetSharedComponent(prefab_entity, previous_components[index]);
					new_data = new_entity_manager->GetSharedComponent(prefab_entity, new_components[new_index]);
					component_type = editor_state->GlobalReflectionManager()->GetType(editor_state->editor_components.ComponentFromID(
						new_components[new_index], 
						ECS_COMPONENT_UNIQUE
					));
				}
				else {
					previous_data = previous_entity_manager->GetComponent(prefab_entity, previous_components[index]);
					new_data = new_entity_manager->GetComponent(prefab_entity, new_components[new_index]);
					component_type = editor_state->GlobalReflectionManager()->GetType(editor_state->editor_components.ComponentFromID(
						new_components[new_index], 
						ECS_COMPONENT_SHARED
					));
				}

				if (DetermineUpdateChangesForComponent(editor_state, component_type, previous_data, new_data, &prefab_change, allocator)) {
					changes->AddAssert(prefab_change);
				}
			}
		}
	};
	register_updates(previous_unique_signature, new_unique_signature, false);
	register_updates(previous_shared_signature, new_shared_signature, true);
}

void TickPrefabFileChange(EditorState* editor_state) {
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_PREFAB_FILE_CHANGE, LAZY_CHANGE_MS)) {
		// Check the prefab file time stamp, if it changed, we need to update all of its instances
		Stream<PrefabInstance> prefabs = editor_state->prefabs.ToStream();
		for (size_t index = 0; index < prefabs.size; index++) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);
			Stream<wchar_t> absolute_path = GetPrefabAbsolutePath(editor_state, prefabs[index].path, absolute_path_storage);
			if (ExistsFileOrFolder(absolute_path)) {
				// Do this only if the file exists, if it previously existed and now it doesn't,
				// Don't do anything - the change set would be the same as removing all prefab components
				// But the user probably doesn't want that
				size_t last_write_stamp = OS::GetFileLastWrite(absolute_path);
				if (last_write_stamp > prefabs[index].write_stamp || prefabs[index].write_stamp == -1) {
					Stream<void> prefab_data = ReadWholeFileBinary(absolute_path, PrefabAllocator(editor_state));
					if (prefab_data.size > 0) {
						// Create 2 temporary entity managers and asset database references
						// In one of them deserialize the previous scene data and in the other
						// The new data and then produce the change set - which components
						// Have been removed, which have been added and which need to be updated
						
						// We need a large allocator size since the components might have large allocator sizes
						GlobalMemoryManager temporary_global_manager = CreateGlobalMemoryManager(ECS_MB * 500, ECS_KB * 4, ECS_GB);
						auto temporary_manager_deallocate = StackScope([&]() {
							temporary_global_manager.Free();
						});

						EntityManager previous_data_manager;
						EntityManager new_data_manager;
						AssetDatabaseReference previous_data_asset_reference(editor_state->asset_database, GetAllocatorPolymorphic(&temporary_global_manager));
						AssetDatabaseReference new_data_asset_reference(editor_state->asset_database, GetAllocatorPolymorphic(&temporary_global_manager));

						CreateEntityManagerWithPool(&previous_data_manager, ECS_MB * 200, ECS_KB * 4, ECS_MB * 400, 4, &temporary_global_manager);
						CreateEntityManagerWithPool(&new_data_manager, ECS_MB * 200, ECS_KB * 4, ECS_MB * 400, 4, &temporary_global_manager);

						// This can be reused among the 2 loads
						ECS_STACK_CAPACITY_STREAM_OF_STREAMS(AssetDatabaseReferencePointerRemap, pointer_remapping, ECS_ASSET_TYPE_COUNT, 512);
						pointer_remapping.size = pointer_remapping.capacity;
						bool previous_data_success = LoadEditorSceneCoreInMemory(
							editor_state,
							&previous_data_manager,
							&previous_data_asset_reference,
							prefabs[index].prefab_file_data,
							pointer_remapping
						);
						if (previous_data_success) {
							// Update the pointer remappings now, such that we can reuse these
							UpdateEditorScenePointerRemappings(editor_state, &previous_data_manager, pointer_remapping);
							for (unsigned int asset_type = 0; asset_type < pointer_remapping.size; index++) {
								pointer_remapping[asset_type].size = 0;
							}

							bool new_data_success = LoadEditorSceneCoreInMemory(
								editor_state,
								&new_data_manager,
								&new_data_asset_reference,
								prefab_data,
								pointer_remapping
							);
							if (new_data_success) {
								// Update the pointer remappings and start the compare
								UpdateEditorScenePointerRemappings(editor_state, &new_data_manager, pointer_remapping);

								ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
								ECS_STACK_CAPACITY_STREAM(PrefabChange, prefab_changes, 512);
								DeterminePrefabChanges(editor_state, &previous_data_manager, &new_data_manager, &prefab_changes, GetAllocatorPolymorphic(&stack_allocator));

								if (prefab_changes.size > 0) {

								}
							}
							else {
								ECS_FORMAT_TEMP_STRING(message, "Failed to load new prefab data for id {#}, path {#}",
									editor_state->prefabs.GetHandleFromIndex(index), prefabs[index].path);
								EditorSetConsoleError(message);
								// Need to deallocate the prefab data and remove the loaded database references
								Deallocate(PrefabAllocator(editor_state), prefab_data.buffer);
								previous_data_asset_reference.Reset(true);
							}
						}
						else {
							// Give an error message
							ECS_FORMAT_TEMP_STRING(message, "Failed to deserialize in memory prefab data when file change has occured for id {#}, path {#}",
								editor_state->prefabs.GetHandleFromIndex(index), prefabs[index].path);
							EditorSetConsoleError(message);
							// Need to deallocate the prefab data as well
							Deallocate(PrefabAllocator(editor_state), prefab_data.buffer);
						}
					}
				}
			}
		}
	}
}