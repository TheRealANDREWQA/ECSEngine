#include "editorpch.h"
#include "../Editor/EditorState.h"
#include "ECSEngineComponents.h"
#include "ECSEngineEntities.h"
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

								Entity prefab_entity = GetPrefabEntityFromSingle();
								ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
								ECS_STACK_CAPACITY_STREAM(EntityChange, prefab_changes, 512);
								DetermineEntityChanges(
									editor_state->GlobalReflectionManager(),
									&previous_data_manager, 
									&new_data_manager,
									prefab_entity,
									prefab_entity,
									&prefab_changes,
									GetAllocatorPolymorphic(&stack_allocator)
								);

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