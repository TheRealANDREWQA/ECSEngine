#include "editorpch.h"
#include "ECSEngineScene.h"
#include "ECSEngineSerializationHelpers.h"
#include "EditorScene.h"
#include "EditorState.h"
#include "EditorEvent.h"

#include "../Project/ProjectFolders.h"
#include "Modules/Module.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Assets/Prefab.h"
#include "../Sandbox/SandboxAccessor.h"

using namespace ECSEngine;

// ----------------------------------------------------------------------------------------------

#define PREFAB_CHUNK_INDEX 0
#define PREFAB_CHUNK_VERSION 1

// Immediately after the chunk header the ids are written - we need those in case
// There are gaps in between the ids or from the iteration order inside the entity manager
// and we need to update the entities that are still referencing the previous id
// After which the path sizes are written (as ushorts)
// And then the paths themselves

struct ScenePrefabChunkHeader {
	unsigned char version;
	unsigned char reserved[7];
	size_t id_count;
	size_t reserved_[4];
};

// In this chunk, we need to remap the written prefabs to the current values inside the editor state
// In case a prefab is missing, we must add it
static bool LoadScenePrefabChunk(LoadSceneChunkFunctionData* function_data) {
	// If the chunk size is 0, then skip this part
	if (function_data->chunk_data.size == 0) {
		return true;
	}

	EditorState* editor_state = (EditorState*)function_data->user_data;

	uintptr_t ptr = (uintptr_t)function_data->chunk_data.buffer;
	ScenePrefabChunkHeader header;
	Read<true>(&ptr, &header, sizeof(header));

	if (header.version != PREFAB_CHUNK_VERSION) {
		return false;
	}

	// Gather all prefab components from the entity manager such that we don't have
	// To iterate the entity manager every time. We can use the global memory manager
	// From the entity manager to preallocate for the entity count
	unsigned int entity_count = function_data->entity_manager->GetEntityCount();
	PrefabComponent** prefab_components = (PrefabComponent**)function_data->entity_manager->m_memory_manager->Allocate(sizeof(PrefabComponent*) * entity_count);
	unsigned int prefab_component_count = 0;
	function_data->entity_manager->ForEachEntityComponent(PrefabComponent::ID(), [&](Entity entity, void* data) {
		prefab_components[prefab_component_count++] = (PrefabComponent*)data;
	});

	unsigned int* id_remapping = (unsigned int*)ptr;
	ptr += sizeof(unsigned int) * header.id_count;

	const unsigned short* path_sizes = (const unsigned short*)ptr;
	ptr += sizeof(unsigned short) * header.id_count;

	// Returns the count of occurences for that id
	auto update_id = [&](unsigned int old_id, unsigned int new_id) {
		unsigned int count = 0;
		if (old_id != new_id) {
			for (unsigned int index = 0; index < prefab_component_count; index++) {
				if (prefab_components[index]->id == old_id) {
					prefab_components[index]->id = new_id;
					count++;
				}
			}
		}
		return count;
	};

	for (size_t index = 0; index < header.id_count; index++) {
		Stream<wchar_t> current_path = { (wchar_t*)ptr, path_sizes[index] };
		ptr += sizeof(wchar_t) * path_sizes[index];

		unsigned int existing_id = AddPrefabID(editor_state, current_path);
		update_id(id_remapping[index], existing_id);
	}

	// Make the deallocation
	function_data->entity_manager->m_memory_manager->Deallocate(prefab_components);
	return true;
}

static bool SaveScenePrefabChunk(SaveSceneChunkFunctionData* function_data) {
	EditorState* editor_state = (EditorState*)function_data->user_data;

	// Get all the prefabs that are referenced inside the entity manager
	unsigned int prefab_count = editor_state->prefabs.set.size;
	Stream<unsigned int> prefabs_in_use;
	prefabs_in_use.Initialize(editor_state->EditorAllocator(), prefab_count);
	prefabs_in_use.size = 0;
	function_data->entity_manager->ForEachEntityComponent(PrefabComponent::ID(), [&](Entity entity, const void* data) {
		const PrefabComponent* component = (const PrefabComponent*)data;
		bool exists = SearchBytes(prefabs_in_use, component->id) != -1;
		if (!exists) {
			prefabs_in_use.Add(component->id);
		}
	});

	size_t initial_header_allocation_size = (sizeof(unsigned short) + sizeof(unsigned int)) * prefabs_in_use.size + sizeof(ScenePrefabChunkHeader);
	void* initial_header_allocation = editor_state->editor_allocator->Allocate(initial_header_allocation_size);
	ScenePrefabChunkHeader* header = (ScenePrefabChunkHeader*)initial_header_allocation;
	header->version = PREFAB_CHUNK_VERSION;
	header->id_count = prefabs_in_use.size;

	unsigned int* scene_ids = (unsigned int*)OffsetPointer(header, sizeof(*header));
	prefabs_in_use.CopyTo(scene_ids);

	unsigned short* path_sizes = (unsigned short*)OffsetPointer(scene_ids, sizeof(unsigned int) * prefabs_in_use.size);
	size_t total_path_size = 0;
	// We need to write the sizes first, and then the path characters
	for (size_t index = 0; index < prefabs_in_use.size; index++) {
		Stream<wchar_t> current_path = GetPrefabPath(editor_state, prefabs_in_use[index]);
		path_sizes[index] = current_path.size;
		total_path_size += current_path.size;
	}

	bool success = WriteFile(function_data->write_handle, { initial_header_allocation, initial_header_allocation_size });
	editor_state->editor_allocator->Deallocate(initial_header_allocation);
	if (!success) {
		prefabs_in_use.Deallocate(editor_state->EditorAllocator());
		return false;
	}

	if (total_path_size > 0) {
		void* path_characters = editor_state->editor_allocator->Allocate(sizeof(wchar_t) * total_path_size);
		void* path_characters_allocation = path_characters;
		for (size_t index = 0; index < prefabs_in_use.size; index++) {
			Stream<wchar_t> current_path = GetPrefabPath(editor_state, prefabs_in_use[index]);
			current_path.CopyTo(path_characters);
			path_characters = OffsetPointer(path_characters, sizeof(wchar_t) * current_path.size);
		}

		success = WriteFile(function_data->write_handle, Stream<void>(path_characters_allocation, sizeof(wchar_t) * total_path_size));
		editor_state->editor_allocator->Deallocate(path_characters_allocation);
	}
	prefabs_in_use.Deallocate(editor_state->EditorAllocator());
	return success;
}

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
// Does not set the data source, you must do that manually
static bool GetLoadSceneDataBase(
	LoadSceneData* load_data,
	EditorState* editor_state, 
	EntityManager* entity_manager, 
	const AssetDatabaseReference* database,
	AllocatorPolymorphic stack_allocator
) {
	CapacityStream<const AppliedModule*> applied_modules;
	applied_modules.Initialize(stack_allocator, 0, 512);
	CapacityStream<DeserializeEntityManagerComponentInfo> unique_overrides;
	unique_overrides.Initialize(stack_allocator, 0, 256);
	CapacityStream<DeserializeEntityManagerSharedComponentInfo> shared_overrides;
	shared_overrides.Initialize(stack_allocator, 0, 256);
	CapacityStream<DeserializeEntityManagerGlobalComponentInfo> global_overrides;
	global_overrides.Initialize(stack_allocator, 0, 256);

	// We need to manually create the standalone asset database because the link components will wrongly
	// take the handles to the master database that will not coincide with the standalone database
	AssetDatabase* standalone_database = (AssetDatabase*)Allocate(stack_allocator, sizeof(AssetDatabase));
	database->ToStandalone(stack_allocator, standalone_database);

	ModulesToAppliedModules(editor_state, applied_modules);
	ModuleGatherDeserializeAllOverrides(applied_modules, unique_overrides, shared_overrides, global_overrides);
	bool link_success = ModuleGatherLinkDeserializeAllOverrides(
		applied_modules,
		editor_state->GlobalReflectionManager(),
		standalone_database,
		stack_allocator,
		unique_overrides,
		shared_overrides,
		global_overrides,
		editor_state->ecs_link_components
	);
	if (!link_success) {
		return false;
	}

	load_data->database = standalone_database;
	load_data->entity_manager = entity_manager;
	load_data->reflection_manager = editor_state->GlobalReflectionManager();
	load_data->unique_overrides = unique_overrides;
	load_data->shared_overrides = shared_overrides;
	load_data->global_overrides = global_overrides;
	load_data->randomize_assets = true;
	load_data->detailed_error_string = (CapacityStream<char>*)Allocate(stack_allocator, sizeof(CapacityStream<char>));
	load_data->detailed_error_string->Initialize(stack_allocator, 0, 512);
	load_data->allow_missing_components = true;
	load_data->module_component_functions = ModuleAggregateComponentFunctions(applied_modules, stack_allocator);

	// Set the extra scene chunks - at the moment, only the prefab chunk is needed
	load_data->chunk_functors[PREFAB_CHUNK_INDEX] = { LoadScenePrefabChunk, editor_state };
	return true;
}

// The SetDataSourceFunctor receives as parameter (LoadSceneData*)
template<typename SetDataSourceFunctor>
static bool LoadEditorSceneCoreImpl(
	EditorState* editor_state,
	EntityManager* entity_manager,
	AssetDatabaseReference* database,
	Stream<CapacityStream<AssetDatabaseReferencePointerRemap>> pointer_remap,
	SetDataSourceFunctor&& set_data_source
) {
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic stack_allocator = &_stack_allocator;

	LoadSceneData load_data;
	bool initialize_data = GetLoadSceneDataBase(&load_data, editor_state, entity_manager, database, stack_allocator);
	if (!initialize_data) {
		return false;
	}

	set_data_source(&load_data);
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
		ReloadAssetMetadataFromFilesOptions reload_options;
		reload_options.mount_point = assets_folder;
		reload_options.material_change_dependencies.database = database->database;
		ReloadAssetMetadataFromFilesParameters(editor_state->RuntimeResourceManager(), database, &reload_options);
	}
	else {
		// Print the error message
		Stream<wchar_t> filename = load_data.is_file_data ? load_data.file : L"In Memory";
		ECS_FORMAT_TEMP_STRING(error_message, "The scene {#} could not be loaded. Reason: {#}", filename, *load_data.detailed_error_string);
		EditorSetConsoleError(error_message);
	}

	return success;
}

bool LoadEditorSceneCore(
	EditorState* editor_state, 
	EntityManager* entity_manager, 
	AssetDatabaseReference* database, 
	Stream<wchar_t> filename,
	Stream<CapacityStream<AssetDatabaseReferencePointerRemap>> pointer_remap
)
{
	return LoadEditorSceneCoreImpl(editor_state, entity_manager, database, pointer_remap, [&](LoadSceneData* load_data) {
		load_data->file = filename;
	});
}

// ----------------------------------------------------------------------------------------------

bool LoadEditorSceneCoreInMemory(
	EditorState* editor_state, 
	EntityManager* entity_manager, 
	AssetDatabaseReference* database, 
	Stream<void> in_memory_data, 
	Stream<CapacityStream<AssetDatabaseReferencePointerRemap>> pointer_remap
)
{
	return LoadEditorSceneCoreImpl(editor_state, entity_manager, database, pointer_remap, [&](LoadSceneData* load_data) {
		load_data->in_memory_data = in_memory_data;
		load_data->is_file_data = false;
	});
}

// ----------------------------------------------------------------------------------------------

bool LoadEditorSceneCore(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> filename)
{
	// Create the pointer remap
	ECS_STACK_CAPACITY_STREAM_OF_STREAMS(AssetDatabaseReferencePointerRemap, pointer_remapping, ECS_ASSET_TYPE_COUNT, 512);
	pointer_remapping.size = pointer_remapping.capacity;

	// TODO: At the moment, this function call ignores the speed up factor in the scene file
	// It might be relevant later on
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	bool success = LoadEditorSceneCore(editor_state, &sandbox->scene_entities, &sandbox->database, filename, pointer_remapping);
	if (success) {
		// Check the pointer remap - we need to update the link components
		UpdateEditorScenePointerRemappings(editor_state, sandbox_index, pointer_remapping);
	}
	return success;
}

// ----------------------------------------------------------------------------------------------

bool SaveEditorScene(const EditorState* editor_state, EntityManager* entity_manager, const AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
	AllocatorPolymorphic stack_allocator = &_stack_allocator;

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
	ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerGlobalComponentInfo, global_overrides, ECS_KB);

	ModulesToAppliedModules(editor_state, applied_modules);
	ModuleGatherSerializeAllOverrides(applied_modules, unique_overrides, shared_overrides, global_overrides);
	bool link_success = ModuleGatherLinkSerializeAllOverrides(
		applied_modules, 
		save_data.reflection_manager, 
		&standalone_database, 
		stack_allocator, 
		unique_overrides, 
		shared_overrides,
		global_overrides,
		editor_state->ecs_link_components
	);
	if (!link_success) {
		return false;
	}

	save_data.unique_overrides = unique_overrides;
	save_data.shared_overrides = shared_overrides;
	save_data.global_overrides = global_overrides;

	// Set the extra chunks - at the moment, only the prefab chunk is needed
	save_data.chunk_functors[PREFAB_CHUNK_INDEX] = { SaveScenePrefabChunk, (void*)editor_state, true };

	entity_manager->DestroyArchetypesBaseEmptyCommit(true);
	return SaveScene(&save_data);
}

// ----------------------------------------------------------------------------------------------

bool SaveEditorScene(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> filename)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return SaveEditorScene(editor_state, &sandbox->scene_entities, &sandbox->database, filename);
}

// ----------------------------------------------------------------------------------------------

bool SaveEditorSceneRuntime(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> filename)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return SaveEditorScene(editor_state, sandbox->sandbox_world.entity_manager, &sandbox->database, filename);
}

// ----------------------------------------------------------------------------------------------

bool GetEditorSceneSerializeOverrides(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<SerializeEntityManagerComponentInfo>* unique_overrides, 
	CapacityStream<SerializeEntityManagerSharedComponentInfo>* shared_overrides, 
	CapacityStream<SerializeEntityManagerGlobalComponentInfo>* global_overrides, 
	AllocatorPolymorphic temporary_allocator
)
{
	ECS_STACK_CAPACITY_STREAM(const AppliedModule*, applied_modules, 512);
	ModulesToAppliedModules(editor_state, applied_modules);
	ModuleGatherSerializeAllOverrides(applied_modules, *unique_overrides, *shared_overrides, *global_overrides);
	return ModuleGatherLinkSerializeAllOverrides(
		applied_modules,
		editor_state->GlobalReflectionManager(),
		editor_state->asset_database,
		temporary_allocator,
		*unique_overrides,
		*shared_overrides,
		*global_overrides,
		editor_state->ecs_link_components
	);
}

// ----------------------------------------------------------------------------------------------

bool GetEditorSceneDeserializeOverrides(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<DeserializeEntityManagerComponentInfo>* unique_overrides, 
	CapacityStream<DeserializeEntityManagerSharedComponentInfo>* shared_overrides, 
	CapacityStream<DeserializeEntityManagerGlobalComponentInfo>* global_overrides, 
	AllocatorPolymorphic temporary_allocator
)
{
	ECS_STACK_CAPACITY_STREAM(const AppliedModule*, applied_modules, 512);
	ModulesToAppliedModules(editor_state, applied_modules);
	ModuleGatherDeserializeAllOverrides(applied_modules, *unique_overrides, *shared_overrides, *global_overrides);
	return ModuleGatherLinkDeserializeAllOverrides(
		applied_modules,
		editor_state->GlobalReflectionManager(),
		editor_state->asset_database,
		temporary_allocator,
		*unique_overrides,
		*shared_overrides,
		*global_overrides,
		editor_state->ecs_link_components
	);
}

// ----------------------------------------------------------------------------------------------

void UpdateEditorScenePointerRemappings(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<CapacityStream<AssetDatabaseReferencePointerRemap>> pointer_remapping
)
{
	UpdateEditorScenePointerRemappings(editor_state, ActiveEntityManager(editor_state, sandbox_index), pointer_remapping);
}

// ----------------------------------------------------------------------------------------------

void UpdateEditorScenePointerRemappings(
	EditorState* editor_state, 
	EntityManager* entity_manager, 
	Stream<CapacityStream<AssetDatabaseReferencePointerRemap>> pointer_remapping
)
{
	ECS_ASSERT(pointer_remapping.size == ECS_ASSET_TYPE_COUNT);
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

	UpdateAssetsToComponents(editor_state, update_elements, entity_manager);
	update_elements.Deallocate(editor_state->EditorAllocator());
}

// ----------------------------------------------------------------------------------------------
