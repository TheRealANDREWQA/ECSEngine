#include "editorpch.h"
#include "Prefab.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxAccessor.h"
#include "../Editor/EditorScene.h"
#include "EditorSandboxAssets.h"
#include "../Sandbox/SandboxEntityOperations.h"

/*
	The prefab works like a normal scene but with only a single entity
	This allows for easy implementation using the existing API
*/

#define PREFAB_EXTENSION L".prefab"

void LoadPrefabAssets(EditorState* editor_state, unsigned int sandbox_index)
{
	// Loading the assets is just calling the load for the missing assets for the sandbox
	LoadSandboxAssets(editor_state, sandbox_index);
}

bool ReadPrefabFile(EditorState* editor_state, unsigned int sandbox_index, EntityManager* entity_manager, Stream<wchar_t> path) {
	// Load into a temporary entity manager and asset database
	// And then commit into the main scene
	// We can use large sizes since these will be virtual memory allocations anyways
	MemoryManager temporary_memory = MemoryManager(ECS_MB * 500, ECS_KB * 4, ECS_GB, { nullptr });
	EntityPool temporary_entity_pool(&temporary_memory, 4);

	EntityManagerDescriptor temporary_descriptor;
	temporary_descriptor.entity_pool = &temporary_entity_pool;
	temporary_descriptor.memory_manager = &temporary_memory;
	temporary_descriptor.deferred_action_capacity = 0;
	EntityManager temporary_manager(temporary_descriptor);

	AssetDatabaseReference temporary_database(editor_state->asset_database, GetAllocatorPolymorphic(&temporary_memory));

	// Create the pointer remap
	ECS_STACK_CAPACITY_STREAM_OF_STREAMS(AssetDatabaseReferencePointerRemap, pointer_remapping, ECS_ASSET_TYPE_COUNT, 512);
	bool success = LoadEditorSceneCore(editor_state, &temporary_manager, &temporary_database, path, pointer_remapping.buffer);
	if (success) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		EntityManager* active_entity_manager = ActiveEntityManager(editor_state, sandbox_index);
		active_entity_manager->AddEntityManager(&temporary_manager);

		// Add the handles from the temporary database into the sandbox database as well
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)index;
			temporary_database.ForEachAssetDuplicates(asset_type, [&](unsigned int handle) {
				sandbox->database.AddAsset(handle, asset_type);
			});
		}

		// Update the pointer remappings after everything was comitted into the sandbox
		UpdateEditorScenePointerRemappings(editor_state, sandbox_index, pointer_remapping.buffer);
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
		path_with_extension.AddStreamAssert(PREFAB_EXTENSION);
		path = path_with_extension;
	}

	// Create a subset of the entity manager containing just that entity
	// And use the normal scene serialize function
	// We can use large allocator sizes since these are virtual allocations anwyays
	MemoryManager temp_memory_manager = MemoryManager(ECS_GB, ECS_KB * 4, ECS_GB, { nullptr });
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
	Stream<char> entity_name = GetEntityName(editor_state, sandbox_index, entity, entity_name_storage);

	ECS_STACK_CAPACITY_STREAM(wchar_t, prefab_path, 512);
	prefab_path.CopyOther(editor_state->file_explorer_data->current_directory);
	prefab_path.Add(ECS_OS_PATH_SEPARATOR);
	ConvertASCIIToWide(prefab_path, entity_name);
	return SavePrefabFile(editor_state, sandbox_index, entity, prefab_path);
}
