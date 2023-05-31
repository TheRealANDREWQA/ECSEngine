#include "editorpch.h"
#include "EditorScene.h"
#include "EditorState.h"
#include "EditorEvent.h"

#include "../Project/ProjectFolders.h"
#include "Modules/Module.h"
#include "ECSEngineScene.h"
#include "../Assets/EditorSandboxAssets.h"

using namespace ECSEngine;

// ----------------------------------------------------------------------------------------------

bool CreateEmptyScene(const EditorState* editor_state, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStreamSafe(path);

	return ECSEngine::CreateEmptyScene(absolute_path);
}

// ----------------------------------------------------------------------------------------------

bool ExistScene(const EditorState* editor_state, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStreamSafe(path);

	return ExistsFileOrFolder(absolute_path);
}

// ----------------------------------------------------------------------------------------------

// It can fail if the link components cannot be found
bool GetLoadSceneDataBase(
	LoadSceneData* load_data,
	EditorState* editor_state, 
	EntityManager* entity_manager, 
	const AssetDatabaseReference* database,
	Stream<wchar_t> filename,
	AllocatorPolymorphic stack_allocator
) {
	CapacityStream<const AppliedModule*> applied_modules;
	applied_modules.Initialize(stack_allocator, 0, 512);
	CapacityStream<DeserializeEntityManagerComponentInfo> unique_overrides;
	unique_overrides.Initialize(stack_allocator, 0, 256);
	CapacityStream<DeserializeEntityManagerSharedComponentInfo> shared_overrides;
	shared_overrides.Initialize(stack_allocator, 0, 256);

	// We need to manually create the standalone asset database because the link components will wrongly
	// take the handles to the master database that will not coincide with the standalone database
	AssetDatabase* standalone_database = (AssetDatabase*)Allocate(stack_allocator, sizeof(AssetDatabase));
	database->ToStandalone(stack_allocator, standalone_database);

	ModulesToAppliedModules(editor_state, applied_modules);
	ModuleGatherDeserializeOverrides(applied_modules, unique_overrides);
	ModuleGatherDeserializeSharedOverrides(applied_modules, shared_overrides);
	bool link_success = ModuleGatherLinkDeserializeUniqueAndSharedOverrides(
		applied_modules,
		editor_state->GlobalReflectionManager(),
		standalone_database,
		stack_allocator,
		unique_overrides,
		shared_overrides
	);
	if (!link_success) {
		return false;
	}

	load_data->file = filename;
	load_data->database = standalone_database;
	load_data->entity_manager = entity_manager;
	load_data->reflection_manager = editor_state->editor_components.internal_manager;
	load_data->unique_overrides = unique_overrides;
	load_data->shared_overrides = shared_overrides;
	load_data->randomize_assets = true;
	return true;
}

bool LoadEditorSceneCore(
	EditorState* editor_state, 
	EntityManager* entity_manager, 
	AssetDatabaseReference* database, 
	Stream<wchar_t> filename,
	CapacityStream<AssetDatabaseReferencePointerRemap>* pointer_remap
)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	LoadSceneData load_data;
	bool initialize_data = GetLoadSceneDataBase(&load_data, editor_state, entity_manager, database, filename, stack_allocator);
	if (!initialize_data) {
		_stack_allocator.ClearBackup();
		return false;
	}

	bool success = LoadScene(&load_data);

	if (success) {
		// Now update the assets that have dependencies or that could have changed in the meantime
		load_data.database->UpdateAssetsWithDependenciesFromFiles();
		
		// Update the asset database to reflect the assets from the entity manager
		GetAssetReferenceCountsFromEntities(entity_manager, load_data.reflection_manager, load_data.database);

		// Now we need to convert from standalone database to the reference one
		database->FromStandalone(load_data.database, { nullptr, pointer_remap });

		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);
		ReloadAssetMetadataFromFilesParameters(editor_state->RuntimeResourceManager(), database, assets_folder);
	}

	_stack_allocator.ClearBackup();
	return success;
}

// ----------------------------------------------------------------------------------------------

bool LoadEditorSceneCore(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> filename)
{
	// Create the pointer remap
	ECS_STACK_CAPACITY_STREAM_OF_STREAMS(AssetDatabaseReferencePointerRemap, pointer_remapping, ECS_ASSET_TYPE_COUNT, 512);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	bool success = LoadEditorSceneCore(editor_state, &sandbox->scene_entities, &sandbox->database, filename, pointer_remapping.buffer);
	if (success) {
		// Check the pointer remap - we need to update the link components
		size_t total_remapping_count = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			total_remapping_count += pointer_remapping[index].size;
		}

		CapacityStream<UpdateAssetToComponentElement> update_elements;
		update_elements.Initialize(editor_state->EditorAllocator(), 0, total_remapping_count);

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			for (unsigned int subindex = 0; subindex < pointer_remapping[index].size; subindex++) {
				const auto remapping = pointer_remapping[index][subindex];
				// Update the link components
				update_elements.Add({ { remapping.old_pointer, 0 }, { remapping.new_pointer, 0 }, (ECS_ASSET_TYPE)index });
			}
		}

		UpdateAssetsToComponents(editor_state, update_elements, sandbox_index);
	}

	return success;
}

// ----------------------------------------------------------------------------------------------

bool SaveEditorScene(const EditorState* editor_state, EntityManager* entity_manager, const AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	// Convert the asset database reference into a standalone
	AssetDatabase standalone_database;
	database->ToStandalone(stack_allocator, &standalone_database);

	SaveSceneData save_data;
	save_data.file = filename;
	save_data.asset_database = &standalone_database;
	save_data.entity_manager = entity_manager;
	save_data.reflection_manager = editor_state->GlobalReflectionManager();
	
	ECS_STACK_CAPACITY_STREAM(const AppliedModule*, applied_modules, 512);
	ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerComponentInfo, unique_overrides, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerSharedComponentInfo, shared_overrides, ECS_KB);

	ModulesToAppliedModules(editor_state, applied_modules);
	ModuleGatherSerializeOverrides(applied_modules, unique_overrides);
	ModuleGatherSerializeSharedOverrides(applied_modules, shared_overrides);
	bool link_success = ModuleGatherLinkSerializeUniqueAndSharedOverrides(applied_modules, save_data.reflection_manager, &standalone_database, stack_allocator, unique_overrides, shared_overrides);
	if (!link_success) {
		_stack_allocator.ClearBackup();
		return false;
	}

	save_data.unique_overrides = unique_overrides;
	save_data.shared_overrides = shared_overrides;

	entity_manager->DestroyArchetypesBaseEmptyCommit(true);

	bool success = SaveScene(&save_data);
	_stack_allocator.ClearBackup();
	return success;
}

// ----------------------------------------------------------------------------------------------

bool SaveEditorScene(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> filename)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return SaveEditorScene(editor_state, &sandbox->scene_entities, &sandbox->database, filename);
}

// ----------------------------------------------------------------------------------------------

bool SaveEditorSceneRuntime(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> filename)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return SaveEditorScene(editor_state, sandbox->sandbox_world.entity_manager, &sandbox->database, filename);
}

// ----------------------------------------------------------------------------------------------
