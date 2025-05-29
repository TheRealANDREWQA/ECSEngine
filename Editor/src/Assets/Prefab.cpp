#include "editorpch.h"
#include "ECSEngineComponents.h"
#include "ECSEngineEntities.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/Sandbox.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorScene.h"
#include "EditorSandboxAssets.h"

#define LAZY_ACTIVE_IDS_MS 5000
#define LAZY_FILE_CHANGE_MS 300

ECS_INLINE static AllocatorPolymorphic PrefabAllocator(EditorState* editor_state) {
	return &editor_state->prefabs_allocator;
}

static Stream<wchar_t> GetPrefabAbsolutePath(const EditorState* editor_state, Stream<wchar_t> path, CapacityStream<wchar_t> storage) {
	GetProjectAssetsFolder(editor_state, storage);
	storage.Add(ECS_OS_PATH_SEPARATOR);
	unsigned int offset = storage.AddStreamAssert(path);
	Stream<wchar_t> replace_path = storage.SliceAt(offset);
	ReplaceCharacter(replace_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
	return storage;
}

unsigned int AddPrefabID(EditorState* editor_state, Stream<wchar_t> path) {
	unsigned int existing_id = FindPrefabID(editor_state, path);
	if (existing_id == -1) {
		// We need to create a new entry
		path = path.Copy(PrefabAllocator(editor_state));
		ReplaceCharacter(path, ECS_OS_PATH_SEPARATOR, ECS_OS_PATH_SEPARATOR_REL);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);
		Stream<wchar_t> absolute_path = GetPrefabAbsolutePath(editor_state, path, absolute_path_storage);

		size_t last_write = -1;
		Stream<void> prefab_data = {};
		if (ExistsFileOrFolder(absolute_path)) {
			last_write = OS::GetFileLastWrite(absolute_path);
			prefab_data = ReadWholeFileBinary(absolute_path, PrefabAllocator(editor_state));
		}

		return editor_state->prefabs.Add({ path, last_write, prefab_data });
	}
	else {
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
	AddSandboxEntityComponent(editor_state, sandbox_index, entity, STRING(PrefabComponent));
	PrefabComponent* component = GetSandboxEntityComponent<PrefabComponent>(editor_state, sandbox_index, entity);
	component->id = id;
	component->detached = false;
}

bool ExistsPrefabFile(const EditorState* editor_state, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);
	Stream<wchar_t> absolute_path = GetPrefabAbsolutePath(editor_state, path, absolute_path_storage);
	return ExistsFileOrFolder(absolute_path);
}

bool ExistsPrefabID(const EditorState* editor_state, unsigned int id)
{
	return editor_state->prefabs.set.Exists(id);
}

unsigned int FindPrefabID(const EditorState* editor_state, Stream<wchar_t> path) {
	return editor_state->prefabs.Find(PrefabInstance{ path });
}

Stream<wchar_t> GetPrefabPath(const EditorState* editor_state, unsigned int id)
{
	return editor_state->prefabs[id].path;
}

Stream<wchar_t> GetPrefabAbsolutePath(const EditorState* editor_state, unsigned int id, CapacityStream<wchar_t> storage)
{
	return GetPrefabAbsolutePath(editor_state, GetPrefabPath(editor_state, id), storage);
}

Stream<Entity> GetPrefabEntitiesForSandbox(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	unsigned int prefab_id, 
	AllocatorPolymorphic allocator
)
{
	const EntityManager* active_entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	unsigned int entity_count = active_entity_manager->GetEntityCountForComponent(PrefabComponent::ID());
	Stream<Entity> entities;
	entities.Initialize(allocator, entity_count);
	entities.size = 0;

	Component prefab_component = PrefabComponent::ID();
	active_entity_manager->ForEachEntityComponent(prefab_component, [&](Entity entity, const void* data) {
		const PrefabComponent* component = (const PrefabComponent*)data;
		if (!component->detached && component->id == prefab_id) {
			entities.Add(entity);
		}
	});

	return entities;
}

void RenamePrefabID(EditorState* editor_state, unsigned int id, Stream<wchar_t> new_path)
{
	PrefabInstance& instance = editor_state->prefabs[id];
	instance.path.Deallocate(PrefabAllocator(editor_state));
	instance.path = new_path.Copy(PrefabAllocator(editor_state));
}

void RemovePrefabID(EditorState* editor_state, Stream<wchar_t> path) {
	unsigned int id = FindPrefabID(editor_state, path);
	ECS_ASSERT(id != -1);
	return RemovePrefabID(editor_state, id);
}

void RemovePrefabID(EditorState* editor_state, unsigned int id) {
	PrefabInstance& instance = editor_state->prefabs[id];
	instance.path.Deallocate(PrefabAllocator(editor_state));
	if (instance.prefab_file_data.size > 0) {
		Deallocate(PrefabAllocator(editor_state), instance.prefab_file_data.buffer);
	}
	editor_state->prefabs.RemoveSwapBack(id);
}

void TickPrefabUpdateActiveIDs(EditorState* editor_state)
{
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_PREFAB_ACTIVE_IDS, LAZY_ACTIVE_IDS_MS)) {
		// We need to refresh the ids in use. Those that are not referenced, need to be elimiated
		// And add a consistency check to ensure that there is no invalid prefab id inside the entities
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 16);
		ResizableStream<unsigned int> prefab_ids_in_use(&stack_allocator, 16);
		AdditionStream<unsigned int> addition_prefab_ids_in_use = &prefab_ids_in_use;

		SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
			for (size_t viewport = 0; viewport < EDITOR_SANDBOX_VIEWPORT_COUNT; viewport++) {
				GetSandboxActivePrefabIDs(editor_state, sandbox_index, addition_prefab_ids_in_use, (EDITOR_SANDBOX_VIEWPORT)viewport);
			}
		});

		for (unsigned int index = 0; index < editor_state->prefabs.set.size; index++) {
			unsigned int id = editor_state->prefabs.GetHandleFromIndex(index);
			bool exists_id = SearchBytes(prefab_ids_in_use.ToStream(), id) != -1;
			if (!exists_id) {
				RemovePrefabID(editor_state, id);
			}
		}

		// This is an optional consistency check to ensure that no entity references an invalid prefab ID
		for (unsigned int index = 0; index < prefab_ids_in_use.size; index++) {
			if (!ExistsPrefabID(editor_state, prefab_ids_in_use[index])) {
				ECS_FORMAT_TEMP_STRING(message, "Prefab ID {#} is referenced inside a sandbox but it is not registered", prefab_ids_in_use[index]);
				EditorSetConsoleWarn(message);
			}
		}
	}
}

void TickPrefabFileChange(EditorState* editor_state) {
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_PREFAB_FILE_CHANGE, LAZY_FILE_CHANGE_MS)) {
		// Check the prefab file time stamp, if it changed, we need to update all of its instances
		Stream<PrefabInstance> prefabs = editor_state->prefabs.ToStream();
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);

		for (size_t index = 0; index < prefabs.size; index++) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);
			Stream<wchar_t> absolute_path = GetPrefabAbsolutePath(editor_state, prefabs[index].path, absolute_path_storage);
			if (ExistsFileOrFolder(absolute_path)) {
				// Do this only if the file exists, if it previously existed and now it doesn't,
				// Don't do anything - the change set would be the same as removing all prefab components
				// But the user probably doesn't want that
				size_t last_write_stamp = OS::GetFileLastWrite(absolute_path);
				if (last_write_stamp != - 1 && (last_write_stamp > prefabs[index].write_stamp || prefabs[index].write_stamp == -1)) {
					prefabs[index].write_stamp = last_write_stamp;

					Stream<void> prefab_data = ReadWholeFileBinary(absolute_path, PrefabAllocator(editor_state));
					if (prefab_data.size > 0) {
						// Create 2 temporary entity managers and asset database references
						// In one of them deserialize the previous scene data and in the other
						// The new data and then produce the change set - which components
						// Have been removed, which have been added and which need to be updated
						
						// We need a large allocator size since the components might have large allocator sizes
						GlobalMemoryManager temporary_global_manager;
						CreateGlobalMemoryManager(&temporary_global_manager, ECS_GB * 32, ECS_KB * 4, ECS_GB * 64);
						auto temporary_manager_deallocate = StackScope([&]() {
							temporary_global_manager.Free();
						});

						EntityManager previous_data_manager;
						EntityManager new_data_manager;
						AssetDatabaseReference previous_data_asset_reference(editor_state->asset_database, &temporary_global_manager);
						AssetDatabaseReference new_data_asset_reference(editor_state->asset_database, &temporary_global_manager);

						CreateEntityManagerWithPool(&previous_data_manager, ECS_GB * 15, ECS_KB * 4, ECS_GB * 31, 4, &temporary_global_manager);
						CreateEntityManagerWithPool(&new_data_manager, ECS_GB * 15, ECS_KB * 4, ECS_GB * 31, 4, &temporary_global_manager);

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
							for (unsigned int asset_type = 0; asset_type < pointer_remapping.size; asset_type++) {
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

								stack_allocator.Clear();
								// Disable changes to unique components
								Entity prefab_entity = GetPrefabEntityFromSingle();
								ECS_STACK_CAPACITY_STREAM(EntityChange, prefab_changes, 512);
								DetermineEntityChanges(
									editor_state->GlobalReflectionManager(),
									&previous_data_manager, 
									&new_data_manager,
									prefab_entity,
									prefab_entity,
									&prefab_changes,
									&stack_allocator
								);

								stack_allocator.SetMarker();

								// Remove all prefab updates related to Translation, Rotation, Scale and Name (all unique components). They don't make sense
								// To be splatted to all entities by default.
								for (unsigned int change_index = 0; change_index < prefab_changes.size; change_index++) {
									if (prefab_changes[change_index].type == ECS_CHANGE_SET_UPDATE && !prefab_changes[change_index].is_shared) {
										Component component = prefab_changes[change_index].component;
										if (component.Is<Translation>() || component.Is<Rotation>() || component.Is<Scale>() || component.Is<Name>()) {
											prefab_changes.RemoveSwapBack(change_index);
											change_index--;
										}
									}
								}

								if (prefab_changes.size > 0) {
									ECS_STACK_CAPACITY_STREAM(const void*, prefab_components, ECS_ARCHETYPE_MAX_COMPONENTS);
									ECS_STACK_CAPACITY_STREAM(const void*, prefab_shared_components, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
									ComponentSignature prefab_unique_signature = new_data_manager.GetEntitySignatureStable(prefab_entity);
									SharedComponentSignature prefab_shared_signature = new_data_manager.GetEntitySharedSignatureStable(prefab_entity);
									new_data_manager.GetComponent(prefab_entity, prefab_unique_signature, (void**)prefab_components.buffer);
									for (unsigned char signature_index = 0; signature_index < prefab_shared_signature.count; signature_index++) {
										prefab_shared_components[signature_index] = new_data_manager.GetSharedData(
											prefab_shared_signature.indices[signature_index],
											prefab_shared_signature.instances[signature_index]
										);
									}

									// We need to apply the changes to all sandboxes
									SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
										AllocatorPolymorphic entities_allocator = editor_state->GlobalMemoryManager();
										// Do the update for the scene data every time.
										// For the runtime version, update it only if the sandbox is paused or running
										for (size_t viewport_index = 0; viewport_index < EDITOR_SANDBOX_VIEWPORT_COUNT; viewport_index++) {
											EDITOR_SANDBOX_VIEWPORT viewport = (EDITOR_SANDBOX_VIEWPORT)viewport_index;

											if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE || 
												GetSandboxState(editor_state, sandbox_index) != EDITOR_SANDBOX_SCENE) {
												Stream<Entity> prefab_entities = GetPrefabEntitiesForSandbox(
													editor_state,
													sandbox_index,
													viewport,
													editor_state->prefabs.GetHandleFromIndex(index),
													entities_allocator
												);

												// For the scene case, we need to get the handle snapshot from the entities
												// Before and after the operation, and update the reference counts
												// Based on the difference between these 2 snapshots
												SandboxReferenceCountsFromEntities before_apply_asset_counts;
												const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
												stack_allocator.ReturnToMarker();
												if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
													before_apply_asset_counts = GetSandboxAssetReferenceCountsFromEntities(
														editor_state, 
														sandbox_index, 
														EDITOR_SANDBOX_VIEWPORT_SCENE, 
														&stack_allocator
													);
												}
												SandboxApplyEntityChanges(
													editor_state,
													sandbox_index,
													viewport,
													prefab_entities,
													prefab_unique_signature,
													prefab_components.buffer,
													prefab_shared_signature.ComponentSignature(),
													prefab_shared_components.buffer,
													prefab_changes
												);
												prefab_entities.Deallocate(entities_allocator);
												if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
													SandboxReferenceCountsFromEntities current_asset_reference_counts = GetSandboxAssetReferenceCountsFromEntities(
														editor_state,
														sandbox_index,
														EDITOR_SANDBOX_VIEWPORT_SCENE,
														&stack_allocator
													);
													ForEachSandboxAssetReferenceDifference(
														editor_state,
														before_apply_asset_counts,
														current_asset_reference_counts,
														[&](ECS_ASSET_TYPE type, unsigned int handle, int reference_count_change) {
															if (reference_count_change < 0) {
																DecrementAssetReference(editor_state, handle, type, sandbox_index, -reference_count_change);
															}
															else {
																IncrementAssetReferenceInSandbox(editor_state, handle, type, sandbox_index, reference_count_change);
															}
														}
													);
													SetSandboxSceneDirty(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
												}
												// Load any missing assets
												LoadSandboxAssets(editor_state, sandbox_index);
											}
											// Also, trigger a redraw for this viewport
											RenderSandbox(editor_state, sandbox_index, viewport);
										}
									});
								}

								// Update the prefab loaded data
								prefabs[index].prefab_file_data.Deallocate(PrefabAllocator(editor_state));
								prefabs[index].prefab_file_data = prefab_data;

								// Clear the references from the 2 loaded databases
								previous_data_asset_reference.Reset(true);
								new_data_asset_reference.Reset(true);
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