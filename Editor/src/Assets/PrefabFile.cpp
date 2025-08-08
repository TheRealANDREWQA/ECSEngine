#include "editorpch.h"
#include "PrefabFile.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxAccessor.h"
#include "../Editor/EditorScene.h"
#include "EditorSandboxAssets.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Project/ProjectFolders.h"

/*
	The prefab works like a normal scene but with only a single entity
	This allows for an easy implementation using the existing API
*/

bool AddPrefabToSandbox(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> path, Entity* created_entity, bool add_prefab_component)
{
	bool success = ReadPrefabFile(editor_state, sandbox_index, path, created_entity, add_prefab_component);
	if (success) {
		LoadPrefabAssets(editor_state, sandbox_index);
	}
	return success;
}

void LoadPrefabAssets(EditorState* editor_state, unsigned int sandbox_index)
{
	// Loading the assets is just calling the load for the missing assets for the sandbox
	LoadSandboxAssets(editor_state, sandbox_index);
}

bool ReadPrefabFile(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> path, Entity* created_entity, bool add_prefab_component) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	AssetDatabaseReference* database_reference = &sandbox->database;

	// Load into a temporary entity manager and asset database
	// And then commit into the main scene
	// We can use large sizes since these will be virtual memory allocations anyways
	MemoryManager temporary_memory(ECS_MB * 500, ECS_KB * 4, ECS_GB, ECS_MALLOC_ALLOCATOR);
	EntityPool temporary_entity_pool(&temporary_memory, 4);

	EntityManagerDescriptor temporary_descriptor;
	temporary_descriptor.entity_pool = &temporary_entity_pool;
	temporary_descriptor.memory_manager = &temporary_memory;
	temporary_descriptor.deferred_action_capacity = 0;
	EntityManager temporary_manager(temporary_descriptor);

	// We need a temporary database since the load scene core will
	// Erase everything that is stored in the reference
	AssetDatabaseReference temporary_database(editor_state->asset_database, &temporary_memory);

	bool success = LoadEditorSceneCore(editor_state, &temporary_manager, &temporary_database, path);
	if (success) {
		ECS_STACK_CAPACITY_STREAM(Entity, created_entity_stream, 1);
		entity_manager->AddEntityManager(&temporary_manager, &created_entity_stream);
		if (created_entity != nullptr) {
			*created_entity = created_entity_stream[0];
		}
		if (add_prefab_component) {
			Stream<wchar_t> relative_assets_path = GetProjectAssetRelativePath(editor_state, path);
			ECS_ASSERT(relative_assets_path.size > 0);
			// If the entity already has a prefab component, then change its component values
			// Else add a new component to it
			PrefabComponent* existing_prefab = entity_manager->TryGetComponent<PrefabComponent>(created_entity_stream[0]);
			if (existing_prefab == nullptr) {
				AddPrefabComponentToEntity(editor_state, sandbox_index, created_entity_stream[0], relative_assets_path);
			}
			else {
				existing_prefab->id = AddPrefabID(editor_state, relative_assets_path);
				existing_prefab->detached = false;
			}
		}

		// Add the handles from the temporary database into the sandbox database as well
		database_reference->AddOther(&temporary_database);
	}

	// We need to clear the memory manager only to release the temporary data
	temporary_memory.Free();
	return success;
}

bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, path_with_extension, 512);
	// If the path doesn't have an extension, append ours
	if (PathExtensionBoth(path).size == 0) {
		path_with_extension.CopyOther(path);
		path_with_extension.AddStreamAssert(PREFAB_FILE_EXTENSION);
		path = path_with_extension;
	}

	// Create a subset of the entity manager containing just that entity
	// And use the normal scene serialize function
	// We can use large allocator sizes since these are virtual allocations anwyays
	MemoryManager temp_memory_manager(ECS_GB, ECS_KB * 4, ECS_GB, ECS_MALLOC_ALLOCATOR);
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	const EntityManager* active_entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	EntityManager temporary_manager = active_entity_manager->CreateSubset({ &entity, 1 }, &temp_memory_manager);
	bool success = SaveEditorScene(editor_state, &temporary_manager, &sandbox->database, path);
	// We just need to release the temporary memory manager to clean the subset
	temp_memory_manager.Free();
	return success;
}

bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 512);
	Stream<char> entity_name = GetSandboxEntityName(editor_state, sandbox_index, entity, entity_name_storage);

	ECS_STACK_CAPACITY_STREAM(wchar_t, prefab_path, 512);
	prefab_path.CopyOther(editor_state->file_explorer_data->current_directory);
	prefab_path.Add(ECS_OS_PATH_SEPARATOR);
	ConvertASCIIToWide(prefab_path, entity_name);
	return SavePrefabFile(editor_state, sandbox_index, entity, prefab_path);
}
