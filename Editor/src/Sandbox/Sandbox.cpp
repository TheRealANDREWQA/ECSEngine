#include "editorpch.h"
#include "Sandbox.h"
#include "SandboxScene.h"
#include "SandboxModule.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorEvent.h"
#include "../Editor/EditorPalette.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Modules/Module.h"
#include "../Project/ProjectFolders.h"
#include "../Modules/ModuleSettings.h"

#include "../Editor/EditorScene.h"
#include "ECSEngineSerializationHelpers.h"
#include "ECSEngineAssets.h"
#include "ECSEngineECSRuntimeResources.h"

// The UI needs to be included because we need to notify it when we destroy a sandbox
#include "../UI/Game.h"
#include "../UI/Scene.h"
#include "../UI/EntitiesUI.h"
#include "../UI/Common.h"

using namespace ECSEngine;

#define RUNTIME_SETTINGS_STRING_CAPACITY (128)
#define SCENE_PATH_STRING_CAPACITY (128)

#define LAZY_EVALUATION_RUNTIME_SETTINGS 500

// -----------------------------------------------------------------------------------------------------------------------------

static ECS_INLINE Stream<char> ColorDepthTextureString(bool color_texture) {
	return color_texture ? "render" : "depth";
}

static ECS_INLINE void GetVisualizeTextureName(Stream<char> viewport_description, unsigned int sandbox_index, bool color_texture, CapacityStream<char>& name) {
	ECS_FORMAT_STRING(name, "{#} {#} {#}", viewport_description, sandbox_index, ColorDepthTextureString(color_texture));
}

// -----------------------------------------------------------------------------------------------------------------------------

static ECS_INLINE void GetVisualizeInstancedTextureName(unsigned int sandbox_index, bool color_texture, CapacityStream<char>& name) {
	ECS_FORMAT_STRING(name, "Scene {#} instanced {#}", sandbox_index, ColorDepthTextureString(color_texture));
}

// -----------------------------------------------------------------------------------------------------------------------------

static ECS_INLINE bool IsSandboxRuntimePreinitialized(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandbox(editor_state, sandbox_index)->runtime_descriptor.graphics != nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------

template<bool early_exit = false, typename Functor>
static bool ForEachSandbox(const EditorState* editor_state, Functor&& functor) {
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	for (unsigned int index = 0; index < sandbox_count; index++) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, index);
		if constexpr (early_exit) {
			if (functor(sandbox, index)) {
				return true;
			}
		}
		else {
			functor(sandbox, index);
		}
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

template<bool early_exit = false, typename Functor>
static bool ForEachSandbox(EditorState* editor_state, Functor&& functor) {
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	for (unsigned int index = 0; index < sandbox_count; index++) {
		EditorSandbox* sandbox = GetSandbox(editor_state, index);
		if constexpr (early_exit) {
			if (functor(sandbox, index)) {
				return true;
			}
		}
		else {
			functor(sandbox, index);
		}
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

// Stores all the current state of the modules being used such that changes to them can be detected dynamically
static void RegisterSandboxModuleSnapshots(EditorState* editor_state, unsigned int sandbox_index) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	sandbox->runtime_module_snapshot_allocator.Clear();
	sandbox->runtime_module_snapshots.size = 0;
	sandbox->runtime_module_snapshots.capacity = 0;
	sandbox->runtime_module_snapshots.Resize(sandbox->modules_in_use.size);
	
	AllocatorPolymorphic snapshot_allocator = GetAllocatorPolymorphic(&sandbox->runtime_module_snapshot_allocator);

	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		// We need to copy the library name and the solution since the module index can change under us
		const EditorModule* module = &editor_state->project_modules->buffer[sandbox->modules_in_use[index].module_index];
		const EditorModuleInfo* module_info = &module->infos[sandbox->modules_in_use[index].module_configuration];

		EditorSandboxModuleSnapshot snapshot;
		snapshot.library_name.InitializeAndCopy(snapshot_allocator, module->library_name);
		snapshot.solution_path.InitializeAndCopy(snapshot_allocator, module->solution_path);
		snapshot.load_status = module_info->load_status;
		snapshot.module_configuration = sandbox->modules_in_use[index].module_configuration;
		snapshot.reflected_settings = StreamDeepCopy(sandbox->modules_in_use[index].reflected_settings, snapshot_allocator);
		snapshot.library_timestamp = module_info->library_last_write_time;

		sandbox->runtime_module_snapshots.Add(&snapshot);
	}
}

enum SOLVE_SANDBOX_MODULE_SNAPSHOT_RESULT : unsigned char {
	SOLVE_SANDBOX_MODULE_SNAPSHOT_OK,
	SOLVE_SANDBOX_MODULE_SNAPSHOT_WAIT,
	SOLVE_SANDBOX_MODULE_SNAPSHOT_FAILURE
};

// Returns OK if the changes could be resolved, WAIT if some modules need to be waited while their compilation is in progress
// or FAILURE if the simulation cannot continue because of a conflict. It will print an error message in that case
static SOLVE_SANDBOX_MODULE_SNAPSHOT_RESULT SolveSandboxModuleSnapshotsChanges(EditorState* editor_state, unsigned int sandbox_index) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	auto reset_concurrency_and_module_settings = [&]() {
		sandbox->sandbox_world.task_scheduler->Reset();
		sandbox->sandbox_world.task_manager->Reset();
		// We also need to reset the entity manager query cache
		sandbox->sandbox_world.entity_manager->ClearCache();
		sandbox->sandbox_world.system_manager->ClearSystemSettings();
	};

	// Returns true if all out of date or failed modules are being compiled, else false
	// Can be used to issue a wait instead of failure call
	auto are_out_of_date_or_failed_modules_being_compiled = [&]() {
		ECS_STACK_CAPACITY_STREAM(unsigned int, modules_being_compiled, 512);
		GetSandboxModulesCompilingInProgress(editor_state, sandbox_index, modules_being_compiled);
		
		ECS_STACK_CAPACITY_STREAM(unsigned int, needed_but_missing_modules, 512);
		GetSandboxNeededButMissingModules(editor_state, sandbox_index, needed_but_missing_modules, true);

		if (needed_but_missing_modules.size > modules_being_compiled.size) {
			return false;
		}

		for (unsigned int index = 0; index < needed_but_missing_modules.size; index++) {
			bool is_compiled = function::SearchBytes(modules_being_compiled.buffer, modules_being_compiled.size, needed_but_missing_modules[index], sizeof(unsigned int)) != -1;
			if (!is_compiled) {
				return false;
			}
		}
		return true;
	};

	bool are_modules_loaded = AreSandboxModulesLoaded(editor_state, sandbox_index, false);
	if (!are_modules_loaded) {
		// Check if the there are modules being compiled and wait for them if that is the case
		bool should_wait = are_out_of_date_or_failed_modules_being_compiled();
		if (should_wait) {
			return SOLVE_SANDBOX_MODULE_SNAPSHOT_WAIT;
		}

		// We need to halt the continuation - also re-register the snapshot
		reset_concurrency_and_module_settings();
		RegisterSandboxModuleSnapshots(editor_state, sandbox_index);
		return SOLVE_SANDBOX_MODULE_SNAPSHOT_FAILURE;
	}

	bool snapshot_has_changed = false;
	// Determine if there is a change in the number of sandbox modules or in the actual settings
	if (sandbox->modules_in_use.size != sandbox->runtime_module_snapshots.size) {
		snapshot_has_changed = true;
	}
	else {
		// Now check each snapshot to see if the module has changed in the meantime
		for (unsigned int index = 0; index < sandbox->runtime_module_snapshots.size && !snapshot_has_changed; index++) {
			unsigned int module_index = GetModuleIndex(editor_state, sandbox->runtime_module_snapshots[index].solution_path);
			if (module_index == -1) {
				snapshot_has_changed = true;
			}
			else {
				// Check to see that the library is the same
				const EditorModule* module = &editor_state->project_modules->buffer[module_index];
				if (module->library_name != sandbox->runtime_module_snapshots[index].library_name) {
					snapshot_has_changed = true;
				}
				else {
					// Check to see if the sandbox still uses the module
					unsigned int in_stream_module_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
					if (in_stream_module_index == -1) {
						snapshot_has_changed = true;
					}
					else {
						// Check to see if the configuration is the same
						EDITOR_MODULE_CONFIGURATION snapshot_configuration = sandbox->runtime_module_snapshots[index].module_configuration;
						if (snapshot_configuration != sandbox->modules_in_use[in_stream_module_index].module_configuration) {
							snapshot_has_changed = true;
						}
						else {
							// Check to see if the settings library timestamp has changed
							if (module->infos[snapshot_configuration].library_last_write_time > sandbox->runtime_module_snapshots[index].library_timestamp) {
								snapshot_has_changed = true;
							}
							else {
								// Check to see if the module settings have changed
								const EditorSandboxModule* sandbox_module = &sandbox->modules_in_use[in_stream_module_index];
								Stream<EditorModuleReflectedSetting> snapshot_settings = sandbox->runtime_module_snapshots[index].reflected_settings;
								Stream<EditorModuleReflectedSetting> module_settings = sandbox_module->reflected_settings;
								if (snapshot_settings.size != module_settings.size) {
									snapshot_has_changed = true;
								}
								else {
									for (size_t subindex = 0; subindex < snapshot_settings.size && !snapshot_has_changed; subindex++) {
										size_t existing_setting = function::FindString(snapshot_settings[subindex].name, module_settings, [](EditorModuleReflectedSetting setting) {
											return setting.name;
										});
										if (existing_setting == -1) {
											snapshot_has_changed = true;
										}
										else {
											// Check to see if the pointer has changed
											if (snapshot_settings[subindex].data != module_settings[existing_setting].data) {
												snapshot_has_changed = true;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (snapshot_has_changed) {
		// The snapshot has changed, we need to restore the module settings and rebind the task scheduler and task manager
		reset_concurrency_and_module_settings();
		// We need to discard the current snapshot and recreate it
		RegisterSandboxModuleSnapshots(editor_state, sandbox_index);
		
		bool success = ConstructSandboxSchedulingOrder(editor_state, sandbox_index);
		if (success) {
			// We also need to prepare the world concurrency
			PrepareWorldConcurrency(&sandbox->sandbox_world);
			
			// Bind again the module settings
			BindSandboxRuntimeModuleSettings(editor_state, sandbox_index);

			// We also need to bind the scene info
			BindSandboxGraphicsSceneInfo(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
		}
		else {
			return SOLVE_SANDBOX_MODULE_SNAPSHOT_FAILURE;
		}
	}
	// Nothing was changed
	return SOLVE_SANDBOX_MODULE_SNAPSHOT_OK;
}

// -------------------------------------------------------------------------------------------------------------

// This will automatically change the reference counts accordingly
static void HandleSandboxAssetHandlesSnapshotsChanges(EditorState* editor_state, unsigned int sandbox_index, bool initialize) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB * 8);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	const AssetDatabase* database = editor_state->asset_database;
	ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, asset_reference_counts, ECS_ASSET_TYPE_COUNT);
	GetAssetReferenceCountsFromEntitiesPrepare(asset_reference_counts.buffer, stack_allocator, database);

	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index);
	GetAssetReferenceCountsFromEntities(entity_manager, editor_state->editor_components.internal_manager, database, asset_reference_counts.buffer);

	if (!initialize) {
		// Determine the difference between the 2 snapshots
		// Allocate for each handle in the stored snapshot a boolean such that we mark those that were already found
		size_t snapshot_size = sandbox->runtime_asset_handle_snapshot.total_size;
		bool* was_found = (bool*)editor_state->editor_allocator->Allocate(sizeof(bool) * snapshot_size);
		memset(was_found, 0, sizeof(bool) * snapshot_size);

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
			size_t handle_start_offset = sandbox->runtime_asset_handle_snapshot.StartOffset(current_type);
			size_t handle_snapshot_count = sandbox->runtime_asset_handle_snapshot.asset_type_count[current_type];
			for (size_t subindex = 0; subindex < asset_reference_counts[index].size; subindex++) {
				// Search for the handle in the stored snapshot
				unsigned int current_handle = database->GetAssetHandleFromIndex(subindex, current_type);
				size_t found_index = function::SearchBytes(
					sandbox->runtime_asset_handle_snapshot.handles + handle_start_offset,
					handle_snapshot_count,
					current_handle,
					sizeof(current_handle)
				);

				unsigned int current_reference_count = asset_reference_counts[index][subindex];
				if (found_index != -1) {
					found_index += handle_start_offset;
					was_found[found_index] = true;
					// Compare the reference count
					unsigned int snapshot_reference_count = sandbox->runtime_asset_handle_snapshot.reference_counts[found_index];

					if (snapshot_reference_count < current_reference_count) {
						// Increment the asset reference count
						IncrementAssetReferenceInSandbox(editor_state, current_handle, current_type, sandbox_index, current_reference_count - snapshot_reference_count);
					}
					else if (snapshot_reference_count > current_reference_count) {
						unsigned int difference = snapshot_reference_count - current_reference_count;
						for (unsigned int reference_index = 0; reference_index < difference; reference_index++) {
							DecrementAssetReference(editor_state, current_handle, current_type, sandbox_index);
						}
					}
				}
				else {
					// The handle didn't exist before, now it exists - it will be added to the snapshot anyway
					// Increment the reference count
					IncrementAssetReferenceInSandbox(editor_state, current_handle, current_type, sandbox_index, current_reference_count);
				}
			}
		}

		// Now check for asset handles that were previously in the snapshot but they are no longer
		size_t was_found_size = snapshot_size;
		size_t was_found_offset = 0;
		size_t missing_index = function::SearchBytes(was_found + was_found_offset, was_found_size, false, sizeof(bool));
		while (missing_index != -1) {
			size_t final_index = missing_index + was_found_offset;
			// Decrement all the reference counts from this sandbox
			unsigned int reference_count = sandbox->runtime_asset_handle_snapshot.reference_counts[final_index];
			for (unsigned int index = 0; index < reference_count; index++) {
				DecrementAssetReference(
					editor_state, 
					sandbox->runtime_asset_handle_snapshot.handles[final_index],
					sandbox->runtime_asset_handle_snapshot.TypeFromIndex(final_index),
					sandbox_index
				);
			}

			was_found_offset += missing_index;
			was_found_size -= missing_index + 1;

			missing_index = function::SearchBytes(was_found + was_found_offset, was_found_size, false, sizeof(bool));
		}
	}

	size_t total_count = 0;
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		size_t starting_total_count = total_count;
		for (size_t subindex = 0; subindex < asset_reference_counts[index].size; subindex++) {
			total_count += asset_reference_counts[index][subindex] > 0;
		}
		sandbox->runtime_asset_handle_snapshot.asset_type_count[index] = total_count - starting_total_count;
	}

	sandbox->runtime_asset_handle_snapshot.allocator.Clear();
	SoAInitialize(
		GetAllocatorPolymorphic(&sandbox->runtime_asset_handle_snapshot.allocator),
		total_count,
		&sandbox->runtime_asset_handle_snapshot.handles,
		&sandbox->runtime_asset_handle_snapshot.reference_counts
	);
	// We will increase the size in the loop
	sandbox->runtime_asset_handle_snapshot.total_size = 0;

	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
		for (size_t subindex = 0; subindex < asset_reference_counts[index].size; subindex++) {
			if (asset_reference_counts[index][subindex] > 0) {
				// Only add elements that have a reference count greater than 0
				size_t current_index = sandbox->runtime_asset_handle_snapshot.total_size;
				sandbox->runtime_asset_handle_snapshot.handles[current_index] = database->GetAssetHandleFromIndex(subindex, current_type);
				sandbox->runtime_asset_handle_snapshot.reference_counts[current_index] = asset_reference_counts[index][subindex];
				sandbox->runtime_asset_handle_snapshot.total_size++;
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

bool AreAllDefaultSandboxesRunning(const EditorState* editor_state) {
	return !ForEachSandbox<true>(editor_state, [](const EditorSandbox* sandbox, unsigned int index) {
		if (sandbox->should_play && sandbox->run_state != EDITOR_SANDBOX_RUNNING) {
			return true;
		}
		return false;
	});
}

// -------------------------------------------------------------------------------------------------------------

bool AreAllDefaultSandboxesPaused(const EditorState* editor_state) {
	return !ForEachSandbox<true>(editor_state, [](const EditorSandbox* sandbox, unsigned int index) {
		if (sandbox->should_pause && sandbox->run_state != EDITOR_SANDBOX_PAUSED) {
			return true;
		}
		return false;
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

bool AreAllDefaultSandboxesNotStarted(const EditorState* editor_state)
{
	return !ForEachSandbox<true>(editor_state, [](const EditorSandbox* sandbox, unsigned int index) {
		if (sandbox->should_play && sandbox->run_state != EDITOR_SANDBOX_SCENE) {
			return true;
		}
		return false;
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void BindSandboxGraphicsSceneInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SetSandboxCameraAspectRatio(editor_state, sandbox_index, viewport);
	Camera camera = GetSandboxCamera(editor_state, sandbox_index, viewport);

	SystemManager* system_manager = sandbox->sandbox_world.system_manager;
	SetRuntimeCamera(system_manager, &camera);
	SetEditorRuntimeType(system_manager, viewport == EDITOR_SANDBOX_VIEWPORT_SCENE ? ECS_EDITOR_RUNTIME_SCENE : ECS_EDITOR_RUNTIME_GAME);
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		SetEditorRuntimeSelectColor(system_manager, EDITOR_SELECT_COLOR);
		SetEditorRuntimeSelectedEntities(system_manager, sandbox->selected_entities);
		
		ECSTransformToolEx transform_tool;
		transform_tool.tool = sandbox->transform_tool;
		transform_tool.display_axes = sandbox->transform_display_axes;
		if (transform_tool.display_axes) {
			// Odd number of key presses means keep the same transform space as the global one
			// Even means invert it
			transform_tool.space = sandbox->transform_keyboard_space;
		}
		else {
			transform_tool.space = sandbox->transform_space;
		}
		memcpy(transform_tool.is_selected, sandbox->transform_tool_selected, sizeof(sandbox->transform_tool_selected));
		bool entity_ids_are_valid = true;
		// If one entity id is invalid, all should be invalid since these are created and released all at once
		for (size_t index = 0; index < 3; index++) {
			transform_tool.entity_ids[index] = FindSandboxUnusedEntitySlot(
				editor_state, 
				sandbox_index, 
				(EDITOR_SANDBOX_ENTITY_SLOT)(EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_X + index), 
				viewport
			);
			if (!transform_tool.entity_ids[index].Valid()) {
				entity_ids_are_valid = false;
			}
		}

		// Check to see if we need to recompute the unused slots
		if (ShouldSandboxRecomputeEntitySlots(editor_state, sandbox_index, viewport) || !entity_ids_are_valid) {
			unsigned int slot_write_index = GetSandboxUnusedEntitySlots(
				editor_state, 
				sandbox_index, 
				{ transform_tool.entity_ids, std::size(transform_tool.entity_ids) }, 
				viewport
			);
			for (unsigned int index = 0; index < std::size(transform_tool.entity_ids); index++) {
				SetSandboxUnusedEntitySlotType(
					editor_state, 
					sandbox_index, 
					slot_write_index + index, 
					(EDITOR_SANDBOX_ENTITY_SLOT)(EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_X + index), 
					viewport
				);
			}
		}

		SetEditorRuntimeTransformToolEx(system_manager, transform_tool);
		GraphicsBoundViews instanced_views;
		instanced_views.depth_stencil = sandbox->scene_viewport_depth_stencil_framebuffer;
		instanced_views.target = sandbox->scene_viewport_instance_framebuffer;
		SetEditorRuntimeInstancedFramebuffer(system_manager, &instanced_views);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool ChangeSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> settings_name)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_ASSERT(sandbox->runtime_settings.capacity >= settings_name.size);

	if (settings_name.size > 0) {
		// Try to load the world descriptor for that file
		WorldDescriptor file_descriptor;
		ECS_STACK_CAPACITY_STREAM(wchar_t, file_path, 512);
		GetSandboxRuntimeSettingsPath(editor_state, settings_name, file_path);

		const Reflection::ReflectionManager* reflection = editor_state->ReflectionManager();
		ECS_DESERIALIZE_CODE code = Deserialize(reflection, reflection->GetType(STRING(WorldDescriptor)), &file_descriptor, file_path);
		if (code != ECS_DESERIALIZE_OK) {
			return false;
		}

		// No need to deallocate and reallocate since the buffer is fixed from the creation
		sandbox->runtime_settings.CopyOther(settings_name);
		sandbox->runtime_settings[settings_name.size] = L'\0';

		// This will also reinitialize the runtime
		SetSandboxRuntimeSettings(editor_state, sandbox_index, file_descriptor);

		// Get the last write
		UpdateSandboxRuntimeSettings(editor_state, sandbox_index);
	}
	else {
		sandbox->runtime_settings.size = 0;
		SetSandboxRuntimeSettings(editor_state, sandbox_index, GetDefaultWorldDescriptor());
		sandbox->runtime_settings_last_write = 0;
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearSandboxTaskScheduler(EditorState* editor_state, unsigned int sandbox_index)
{
	GetSandbox(editor_state, sandbox_index)->sandbox_world.task_scheduler->Reset();
}

// -----------------------------------------------------------------------------------------------------------------------------

void CreateSandbox(EditorState* editor_state, bool initialize_runtime) {
	unsigned int sandbox_index = editor_state->sandboxes.ReserveNewElement();
	EditorSandbox* sandbox = editor_state->sandboxes.buffer + sandbox_index;
	editor_state->sandboxes.size++;
	// Zero out the memory since most fields require zero initialization
	memset(sandbox, 0, sizeof(*sandbox));

	sandbox->should_pause = true;
	sandbox->should_play = true;
	sandbox->should_step = true;
	sandbox->is_scene_dirty = false;
	sandbox->transform_tool = ECS_TRANSFORM_TRANSLATION;
	sandbox->transform_space = ECS_TRANSFORM_LOCAL_SPACE;
	sandbox->transform_keyboard_space = ECS_TRANSFORM_LOCAL_SPACE;
	sandbox->transform_keyboard_tool = ECS_TRANSFORM_COUNT;
	sandbox->is_camera_wasd_movement = false;
	sandbox->camera_wasd_speed = EDITOR_SANDBOX_CAMERA_WASD_DEFAULT_SPEED;
	memset(sandbox->transform_tool_selected, 0, sizeof(sandbox->transform_tool_selected));

	sandbox->run_state = EDITOR_SANDBOX_SCENE;
	sandbox->locked_count = 0;
	sandbox->flags = 0;

	// Initialize the runtime settings string
	sandbox->runtime_descriptor = GetDefaultWorldDescriptor();
	sandbox->runtime_descriptor.mouse = editor_state->Mouse();
	sandbox->runtime_descriptor.keyboard = editor_state->Keyboard();

	sandbox->runtime_settings.Initialize(editor_state->EditorAllocator(), 0, RUNTIME_SETTINGS_STRING_CAPACITY);
	sandbox->runtime_settings_last_write = 0;

	sandbox->scene_path.Initialize(editor_state->EditorAllocator(), 0, SCENE_PATH_STRING_CAPACITY);

	// The runtime snapshots and their allocator are being created in the preinitialize phase

	ResetSandboxCameras(editor_state, sandbox_index);

	if (initialize_runtime) {
		if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
			// Lock the sandbox and put the preinitialization on an editor event
			LockSandbox(editor_state, sandbox_index);
			EditorAddEventFunctorWaitFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING, false, [&]() {
				PreinitializeSandboxRuntime(editor_state, sandbox_index);
				// We also need to unlock the sandbox
				UnlockSandbox(editor_state, sandbox_index);
			});
		}
		else {
			// Can preinitialize now
			PreinitializeSandboxRuntime(editor_state, sandbox_index);
		}
	}
	else {
		sandbox->runtime_descriptor.graphics = nullptr;
		sandbox->runtime_descriptor.task_manager = nullptr;
		sandbox->runtime_descriptor.task_scheduler = nullptr;
		sandbox->runtime_descriptor.resource_manager = nullptr;
	}

	RegisterInspectorSandboxChange(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool ConstructSandboxSchedulingOrder(EditorState* editor_state, unsigned int sandbox_index, bool disable_error_message)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, sandbox->modules_in_use.size);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		indices[index] = sandbox->modules_in_use[index].module_index;
	}
	indices.size = sandbox->modules_in_use.size;
	return ConstructSandboxSchedulingOrder(editor_state, sandbox_index, indices, disable_error_message);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool ConstructSandboxSchedulingOrder(EditorState* editor_state, unsigned int sandbox_index, Stream<unsigned int> module_indices, bool disable_error_message)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	for (size_t index = 0; index < module_indices.size; index++) {
		const EditorModuleInfo* current_info = GetSandboxModuleInfo(editor_state, sandbox_index, module_indices[index]);

		if (current_info->load_status == EDITOR_MODULE_LOAD_FAILED) {
			if (!disable_error_message) {
				const EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, index);
				const EditorModule* editor_module = &editor_state->project_modules->buffer[sandbox_module->module_index];
				ECS_FORMAT_TEMP_STRING(console_message, "The module {#} has not yet been loaded when trying to create the scheduling graph.", editor_module->library_name);
				EditorSetConsoleError(console_message);
			}
			sandbox->sandbox_world.task_scheduler->Reset();
			return false;
		}
		sandbox->sandbox_world.task_scheduler->Add(current_info->ecs_module.tasks);
	}

	// Now try to solve the graph
	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	return sandbox->sandbox_world.task_scheduler->Solve(disable_error_message ? nullptr : &error_message);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DestroySandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (IsSandboxRuntimePreinitialized(editor_state, sandbox_index)) {
		// Can safely now destroy the runtime world
		DestroyWorld(&sandbox->sandbox_world);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DestroySandbox(EditorState* editor_state, unsigned int sandbox_index, bool wait_unlocking) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (wait_unlocking) {
		WaitSandboxUnlock(editor_state, sandbox_index);
	}
	else {
		ECS_ASSERT(!IsSandboxLocked(editor_state, sandbox_index));
	}

	// Unload the sandbox assets
	UnloadSandboxAssets(editor_state, sandbox_index);

	// Destroy the reflected settings
	for (size_t index = 0; index < sandbox->modules_in_use.size; index++) {
		DestroyModuleSettings(
			editor_state,
			sandbox->modules_in_use[index].module_index,
			sandbox->modules_in_use[index].reflected_settings,
			index
		);
	}

	// Before destroying the sandbox runtime we need to destroy the threads
	sandbox->sandbox_world.task_manager->TerminateThreads(true);

	// Destroy the sandbox runtime
	DestroySandboxRuntime(editor_state, sandbox_index);

	FreeSandboxRenderTextures(editor_state, sandbox_index);

	// Free the global sandbox allocator
	sandbox->GlobalMemoryManager()->Free();

	// Release the runtime settings path as well - it was allocated from the editor allocator
	editor_state->editor_allocator->Deallocate(sandbox->runtime_settings.buffer);

	unsigned int previous_count = editor_state->sandboxes.size;
	editor_state->sandboxes.RemoveSwapBack(sandbox_index);

	// Notify the UI and the inspector
	if (editor_state->sandboxes.size > 0) {
		// Destroy the associated windows first
		DestroySandboxWindows(editor_state, sandbox_index);
		FixInspectorSandboxReference(editor_state, previous_count, sandbox_index);
		UpdateGameUIWindowIndex(editor_state, previous_count, sandbox_index);
		UpdateSceneUIWindowIndex(editor_state, previous_count, sandbox_index);
		UpdateEntitiesUITargetSandbox(editor_state, previous_count, sandbox_index);
	}

	RegisterInspectorSandboxChange(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DisableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport] = false;
}

// -----------------------------------------------------------------------------------------------------------------------------

void EnableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport] = true;
}

// ------------------------------------------------------------------------------------------------------------------------------

void EndSandboxWorldSimulation(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_PAUSED || sandbox->run_state == EDITOR_SANDBOX_RUNNING);

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 4);

	// Before clearing the world, we need to retrieve all the asset references of the scene world and restore those counts
	AssetDatabase* database = editor_state->asset_database;
	ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, asset_handles, ECS_ASSET_TYPE_COUNT);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
		unsigned int asset_count = database->GetAssetCount(current_type);
		asset_handles[index].Initialize(GetAllocatorPolymorphic(&stack_allocator), asset_count);
		memset(asset_handles[index].buffer, 0, asset_handles[index].MemoryOf(asset_handles[index].size));
	}
	GetAssetReferenceCountsFromEntitiesOptions get_counts_options;
	get_counts_options.add_reference_counts_to_dependencies = true;
	GetAssetReferenceCountsFromEntities(&sandbox->scene_entities, editor_state->editor_components.internal_manager, database, asset_handles.buffer, get_counts_options);

	// We need now to decrement the reference counts for this sandbox
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
		for (size_t subindex = 0; subindex < asset_handles[index].size; subindex++) {
			unsigned int handle = database->GetAssetHandleFromIndex(subindex, current_type);
			unsigned int current_reference_count = database->GetReferenceCount(handle, current_type);
			unsigned int scene_reference_count = asset_handles[index][subindex];
			if (current_reference_count > scene_reference_count) {
				for (unsigned int reference_index = 0; reference_index < current_reference_count - scene_reference_count; reference_index++) {
					DecrementAssetReference(editor_state, handle, current_type, sandbox_index);
				}
			}
			else if (current_reference_count < scene_reference_count) {
				IncrementAssetReferenceInSandbox(editor_state, handle, current_type, sandbox_index, scene_reference_count - current_reference_count);
			}
		}
	}

	ClearWorld(&sandbox->sandbox_world);
	sandbox->run_state = EDITOR_SANDBOX_SCENE;
	// Clear the waiting compilation flag
	sandbox->flags = function::ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);

	if (EnableSceneUIRendering(editor_state, sandbox_index)) {
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	}
	if (EnableGameUIRendering(editor_state, sandbox_index)) {
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void EndSandboxWorldSimulations(EditorState* editor_state)
{
	ForEachSandbox(editor_state, [&](EditorSandbox* sandbox, unsigned int sandbox_index) {
		bool paused_or_running = sandbox->run_state == EDITOR_SANDBOX_RUNNING || sandbox->run_state == EDITOR_SANDBOX_PAUSED;
		if (sandbox->should_play && paused_or_running) {
			EndSandboxWorldSimulation(editor_state, sandbox_index);
		}
	});
}

// ------------------------------------------------------------------------------------------------------------------------------

unsigned int FindSandboxSelectedEntityIndex(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return sandbox->selected_entities.Find(entity, [](Entity entity) {
		return entity;
	});
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity FindSandboxUnusedEntitySlot(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_ENTITY_SLOT slot_type,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	unsigned int slot_index = sandbox->unused_entity_slot_type[viewport].Find(slot_type, [](EDITOR_SANDBOX_ENTITY_SLOT current_slot) {
		return current_slot;
	});

	return slot_index != -1 ? sandbox->unused_entities_slots[viewport][slot_index] : Entity(-1);
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_ENTITY_SLOT FindSandboxUnusedEntitySlotType(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::Entity entity, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	size_t slot_index = function::SearchBytes(sandbox->unused_entities_slots[viewport].buffer, sandbox->unused_entities_slots[viewport].size, entity.value, sizeof(entity));
	if (slot_index != -1) {
		ECS_ASSERT(sandbox->unused_entity_slot_type[viewport][slot_index] != EDITOR_SANDBOX_ENTITY_SLOT_COUNT);
		return sandbox->unused_entity_slot_type[viewport][slot_index];
	}
	return EDITOR_SANDBOX_ENTITY_SLOT_COUNT;
}

// -----------------------------------------------------------------------------------------------------------------------------

void FreeSandboxRenderTextures(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	if (viewport == EDITOR_SANDBOX_VIEWPORT_COUNT) {
		for (size_t viewport_index = 0; viewport_index < EDITOR_SANDBOX_VIEWPORT_COUNT; viewport_index++) {
			FreeSandboxRenderTextures(editor_state, sandbox_index, (EDITOR_SANDBOX_VIEWPORT)viewport_index);
		}
		return;
	}

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	Graphics* runtime_graphics = editor_state->RuntimeGraphics();

	if (sandbox->viewport_render_destination[viewport].render_view.Interface() != nullptr) {
		runtime_graphics->FreeRenderDestination(sandbox->viewport_render_destination[viewport]);
		ReleaseGraphicsView(sandbox->viewport_transferred_texture[viewport]);

		ECS_STACK_CAPACITY_STREAM(char, visualize_texture_name, 512);	
		if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
			runtime_graphics->FreeView(sandbox->scene_viewport_instance_framebuffer);
			runtime_graphics->FreeView(sandbox->scene_viewport_depth_stencil_framebuffer);

			GetVisualizeInstancedTextureName(sandbox_index, true, visualize_texture_name);
			RemoveVisualizeTexture(editor_state, visualize_texture_name);
			visualize_texture_name.size = 0;

			GetVisualizeInstancedTextureName(sandbox_index, false, visualize_texture_name);
			RemoveVisualizeTexture(editor_state, visualize_texture_name);
			visualize_texture_name.size = 0;
		}

		Stream<char> viewport_string = ViewportString(viewport);
		GetVisualizeTextureName(viewport_string, sandbox_index, true, visualize_texture_name);
		RemoveVisualizeTexture(editor_state, visualize_texture_name);

		visualize_texture_name.size = 0;
		GetVisualizeTextureName(viewport_string, sandbox_index, false, visualize_texture_name);
		RemoveVisualizeTexture(editor_state, visualize_texture_name);
	}

	runtime_graphics->GetContext()->ClearState();
	runtime_graphics->GetContext()->Flush();
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxRuntimeSettingsPath(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<wchar_t>& path)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	GetSandboxRuntimeSettingsPath(editor_state, sandbox->runtime_settings, path);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxRuntimeSettingsPath(const EditorState* editor_state, Stream<wchar_t> filename, CapacityStream<wchar_t>& path)
{
	GetProjectConfigurationRuntimeFolder(editor_state, path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(filename);
	path.AddStream(PROJECT_RUNTIME_SETTINGS_FILE_EXTENSION);
	path[path.size] = L'\0';
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxAvailableRuntimeSettings(
	const EditorState* editor_state, 
	CapacityStream<Stream<wchar_t>>& paths,
	AllocatorPolymorphic allocator
)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, runtime_settings_folder, 512);
	GetProjectConfigurationRuntimeFolder(editor_state, runtime_settings_folder);

	struct FunctorData {
		CapacityStream<Stream<wchar_t>>* paths;
		AllocatorPolymorphic allocator;
	};

	FunctorData functor_data = { &paths, allocator };
	ForEachFileInDirectory(runtime_settings_folder, &functor_data, [](Stream<wchar_t> path, void* _data) {
		FunctorData* data = (FunctorData*)_data;
		Stream<wchar_t> stem = function::PathStem(path);
		data->paths->AddAssert(function::StringCopy(data->allocator, stem));
		return true;
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

ECSEngine::RenderTargetView GetSandboxInstancedFramebuffer(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->scene_viewport_instance_framebuffer;
}

// -----------------------------------------------------------------------------------------------------------------------------

ECSEngine::DepthStencilView GetSandboxInstancedDepthFramebuffer(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->scene_viewport_depth_stencil_framebuffer;
}

// ------------------------------------------------------------------------------------------------------------------------------

Stream<Entity> GetSandboxSelectedEntities(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->selected_entities.ToStream();
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetSandboxUnusedEntitySlots(EditorState* editor_state, unsigned int sandbox_index, Stream<Entity> entities, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	// Limit the bit count to the available space for the instance bitness for the instanced buffer
	ECS_ASSERT(entity_manager->m_entity_pool->GetUnusedEntities(
		entities, 
		sandbox->unused_entities_slots[viewport].ToStream(), 
		32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS - 1
	));
	unsigned int write_index = sandbox->unused_entities_slots[viewport].AddStream(entities);
	if (sandbox->unused_entities_slots[viewport].capacity != sandbox->unused_entity_slot_type[viewport].capacity) {
		sandbox->unused_entity_slot_type[viewport].Resize(sandbox->unused_entities_slots[viewport].capacity);
		for (size_t index = 0; index < entities.size; index++) {
			// Default initialize to invalid value
			sandbox->unused_entity_slot_type[viewport][write_index + index] = EDITOR_SANDBOX_ENTITY_SLOT_COUNT;
		}
		sandbox->unused_entity_slot_type[viewport].size = sandbox->unused_entities_slots[viewport].size;
	}

	return write_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

void InitializeSandboxes(EditorState* editor_state)
{
	// Set the allocator for the resizable strema
	editor_state->sandboxes.Initialize(editor_state->EditorAllocator(), 0);
}

// -----------------------------------------------------------------------------------------------------------------------------

void InitializeSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = editor_state->sandboxes.buffer + sandbox_index;

	// Check to see if the sandbox was pre-initialized before
	if (!IsSandboxRuntimePreinitialized(editor_state, sandbox_index)) {
		// Need to preinitialize
		PreinitializeSandboxRuntime(editor_state, sandbox_index);
	}
	else {
		// The task manager doesn't need to be recreated

		// Recreate the world
		sandbox->sandbox_world = World(sandbox->runtime_descriptor);
		sandbox->sandbox_world.task_manager->SetWorld(&sandbox->sandbox_world);
		editor_state->editor_components.SetManagerComponents(sandbox->sandbox_world.entity_manager);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxGizmoEntity(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return FindSandboxUnusedEntitySlotType(editor_state, sandbox_index, entity, viewport) != EDITOR_SANDBOX_ENTITY_SLOT_COUNT;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAnyDefaultSandboxRunning(const EditorState* editor_state)
{
	return ForEachSandbox<true>(editor_state, [](const EditorSandbox* sandbox, unsigned int sandbox_index) {
		if (sandbox->should_play && sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			return true;
		}
		return false;
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAnyDefaultSandboxPaused(const EditorState* editor_state)
{
	return ForEachSandbox<true>(editor_state, [](const EditorSandbox* sandbox, unsigned int sandbox_index) {
		if (sandbox->should_pause && sandbox->run_state == EDITOR_SANDBOX_PAUSED) {
			return true;
		}
		return false;
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAnyDefaultSandboxNotStarted(const EditorState* editor_state)
{
	return ForEachSandbox<true>(editor_state, [](const EditorSandbox* sandbox, unsigned int sandbox_index) {
		if (sandbox->should_play && sandbox->run_state == EDITOR_SANDBOX_SCENE) {
			return true;
		}
		return false;
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (sandbox->runtime_settings.size > 0) {
		ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
		bool success = LoadRuntimeSettings(editor_state, sandbox->runtime_settings, &sandbox->runtime_descriptor, &error_message);

		if (!success) {
			// Display an error message
			ECS_FORMAT_TEMP_STRING(
				console_message,
				"An error has occured when trying to load the runtime settings {#} for the sandbox {#}. Detailed error: {#}",
				sandbox->runtime_settings,
				sandbox_index,
				error_message
			);
			EditorSetConsoleError(console_message);
			return false;
		}
	}
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadRuntimeSettings(const EditorState* editor_state, Stream<wchar_t> filename, WorldDescriptor* descriptor, CapacityStream<char>* error_message)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetSandboxRuntimeSettingsPath(editor_state, filename, absolute_path);

	WorldDescriptor new_descriptor;
	const Reflection::ReflectionManager* reflection_manager = editor_state->ui_reflection->reflection;

	DeserializeOptions options;
	options.error_message = error_message;

	bool success = Deserialize(reflection_manager, reflection_manager->GetType(STRING(WorldDescriptor)), &new_descriptor, absolute_path, &options) == ECS_DESERIALIZE_OK;

	if (success) {
		// Can copy into the descriptor now
		memcpy(descriptor, &new_descriptor, sizeof(new_descriptor));
		return true;
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

void LockSandbox(EditorState* editor_state, unsigned int sandbox_index)
{
	GetSandbox(editor_state, sandbox_index)->locked_count.fetch_add(1, ECS_RELAXED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void PauseSandboxWorld(EditorState* editor_state, unsigned int index, bool wait_for_pause)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, index);
	ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_RUNNING);

	if (wait_for_pause) {
		PauseWorld(&sandbox->sandbox_world);
	}

	// Clear the sandbox waiting compilation flag - since it might be still on
	sandbox->flags = function::ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);
	sandbox->run_state = EDITOR_SANDBOX_PAUSED;

	// Bring back the viewport rendering since it was disabled
	EnableSceneUIRendering(editor_state, index);
	EnableGameUIRendering(editor_state, index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void PauseSandboxWorlds(EditorState* editor_state)
{
	ForEachSandbox(editor_state, [&](EditorSandbox* sandbox, unsigned int sandbox_index) {
		if (sandbox->should_pause && sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			// We don't need to wait for the pause
			PauseSandboxWorld(editor_state, sandbox_index, false);
		}
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void PreinitializeSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	// Create a sandbox allocator - a global one - such that it can accomodate the default entity manager requirements
	GlobalMemoryManager* allocator = (GlobalMemoryManager*)editor_state->editor_allocator->Allocate(sizeof(GlobalMemoryManager));
	*allocator = CreateGlobalMemoryManager(
		sandbox->runtime_descriptor.entity_manager_memory_size + ECS_MB * 10,
		1024,
		sandbox->runtime_descriptor.entity_manager_memory_new_allocation_size + ECS_MB * 2
	);

	AllocatorPolymorphic sandbox_allocator = GetAllocatorPolymorphic(allocator);

	// Initialize the textures to nullptr - wait for the UI window to tell the sandbox what area it covers
	memset(sandbox->viewport_render_destination, 0, sizeof(sandbox->viewport_render_destination));

	sandbox->modules_in_use.Initialize(sandbox_allocator, 0);
	sandbox->database = AssetDatabaseReference(editor_state->asset_database, sandbox_allocator);

	// Create a graphics object
	sandbox->runtime_descriptor.graphics = editor_state->RuntimeGraphics();
	sandbox->runtime_descriptor.resource_manager = editor_state->RuntimeResourceManager();

	// Create the scene entity manager
	sandbox->scene_entities = CreateEntityManagerWithPool(
		sandbox->runtime_descriptor.entity_manager_memory_size,
		sandbox->runtime_descriptor.entity_manager_memory_pool_count,
		sandbox->runtime_descriptor.entity_manager_memory_new_allocation_size,
		sandbox->runtime_descriptor.entity_pool_power_of_two,
		allocator
	);
	editor_state->editor_components.SetManagerComponents(&sandbox->scene_entities);

	// Create the task manager that is going to be reused accros runtime plays
	TaskManager* task_manager = (TaskManager*)allocator->Allocate(sizeof(TaskManager));
	new (task_manager) TaskManager(
		std::thread::hardware_concurrency(),
		allocator,
		sandbox->runtime_descriptor.per_thread_temporary_memory_size
	);
	sandbox->runtime_descriptor.task_manager = task_manager;

	// Create the threads for the task manager
	sandbox->runtime_descriptor.task_manager->CreateThreads();

	TaskScheduler* task_scheduler = (TaskScheduler*)allocator->Allocate(sizeof(TaskScheduler) + sizeof(MemoryManager));
	MemoryManager* task_scheduler_allocator = (MemoryManager*)function::OffsetPointer(task_scheduler, sizeof(TaskScheduler));
	*task_scheduler_allocator = TaskScheduler::DefaultAllocator(allocator);

	new (task_scheduler) TaskScheduler(task_scheduler_allocator);

	sandbox->runtime_descriptor.task_scheduler = task_scheduler;

	// Wait until the graphics initialization is finished, otherwise the debug drawer can fail
	EditorStateWaitFlag(50, editor_state, EDITOR_STATE_RUNTIME_GRAPHICS_INITIALIZATION_FINISHED);

	// Create the sandbox world
	sandbox->sandbox_world = World(sandbox->runtime_descriptor);
	sandbox->sandbox_world.task_manager->SetWorld(&sandbox->sandbox_world);
	editor_state->editor_components.SetManagerComponents(sandbox->sandbox_world.entity_manager);

	// Initialize the selected entities
	sandbox->selected_entities.Initialize(sandbox_allocator, 0);
	sandbox->selected_entities_changed_counter = 0;

	// Initialize the runtime module snapshots and their allocator
	sandbox->runtime_module_snapshot_allocator = MemoryManager(ECS_KB * 8, 1024, ECS_KB * 256, sandbox_allocator);
	sandbox->runtime_module_snapshots.Initialize(GetAllocatorPolymorphic(&sandbox->runtime_module_snapshot_allocator), 0);

	// Initialize the asset handle snapshots and their allocator
	sandbox->runtime_asset_handle_snapshot.allocator = MemoryManager(ECS_KB * 128, ECS_KB, ECS_MB, sandbox_allocator);
	sandbox->runtime_asset_handle_snapshot.total_size = 0;

	// Resize the textures for the viewport to a 1x1 texture such that rendering commands will fallthrough even
	// when the UI has not yet run to resize them
	for (size_t index = 0; index < EDITOR_SANDBOX_VIEWPORT_COUNT; index++) {
		sandbox->unused_entities_slots[index].Initialize(GetAllocatorPolymorphic(sandbox->GlobalMemoryManager()), 0);
		sandbox->unused_entity_slot_type[index].Initialize(GetAllocatorPolymorphic(sandbox->GlobalMemoryManager()), 0);
		sandbox->unused_entities_slots_recompute[index] = false;
		ResizeSandboxRenderTextures(editor_state, sandbox_index, (EDITOR_SANDBOX_VIEWPORT)index, { 1, 1 });
	}

}

// -----------------------------------------------------------------------------------------------------------------------------

void ReinitializeSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	DestroySandboxRuntime(editor_state, sandbox_index);
	InitializeSandboxRuntime(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void ReloadSandboxRuntimeSettings(EditorState* editor_state)
{
	for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
		if (UpdateSandboxRuntimeSettings(editor_state, index)) {
			LoadSandboxRuntimeSettings(editor_state, index);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

GraphicsResourceSnapshot RenderSandboxInitializeGraphics(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	SetSandboxGraphicsTextures(editor_state, sandbox_index, viewport);
	BindSandboxGraphicsSceneInfo(editor_state, sandbox_index, viewport);
	return editor_state->RuntimeGraphics()->GetResourceSnapshot(editor_state->EditorAllocator());
}

// -----------------------------------------------------------------------------------------------------------------------------

void RenderSandboxFinishGraphics(EditorState* editor_state, unsigned int sandbox_index, GraphicsResourceSnapshot snapshot, EDITOR_SANDBOX_VIEWPORT viewport)
{
	// Restore the graphics snapshot and deallocate it
	bool are_resources_valid = editor_state->RuntimeGraphics()->RestoreResourceSnapshot(snapshot);
	if (!are_resources_valid) {
		ECS_FORMAT_TEMP_STRING(console_message, "Restoring graphics resources after sandbox {#} failed.", sandbox_index);
		EditorSetConsoleError(console_message);
	}
	snapshot.Deallocate(editor_state->EditorAllocator());

	// Remove from the system manager bound resources
	SystemManager* system_manager = GetSandbox(editor_state, sandbox_index)->sandbox_world.system_manager;
	RemoveRuntimeCamera(system_manager);
	RemoveEditorRuntimeType(system_manager);
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		RemoveEditorRuntimeSelectColor(system_manager);
		RemoveEditorRuntimeSelectedEntities(system_manager);
		RemoveEditorRuntimeTransformToolEx(system_manager);
		RemoveEditorRuntimeInstancedFramebuffer(system_manager);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

struct WaitCompilationRenderSandboxEventData {
	unsigned int sandbox_index;
	EDITOR_SANDBOX_VIEWPORT viewport;
	uint2 new_texture_size;
};

EDITOR_EVENT(WaitCompilationRenderSandboxEvent) {
	WaitCompilationRenderSandboxEventData* data = (WaitCompilationRenderSandboxEventData*)_data;

	// We assume that the Graphics module won't modify any entities
	unsigned int in_stream_module_index = GetSandboxGraphicsModule(editor_state, data->sandbox_index);
	if (in_stream_module_index != -1) {
		EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, data->sandbox_index, in_stream_module_index);
		bool is_being_compiled = IsModuleBeingCompiled(editor_state, sandbox_module->module_index, sandbox_module->module_configuration);
		if (!is_being_compiled) {
			RenderSandbox(editor_state, data->sandbox_index, data->viewport, data->new_texture_size);
			// Unlock the sandbox
			UnlockSandbox(editor_state, data->sandbox_index);
			return false;
		}
		return true;
	}

	// Unlock the sandbox
	UnlockSandbox(editor_state, data->sandbox_index);
	return false;
}

struct WaitResourceLoadingRenderSandboxEventData {
	unsigned int sandbox_index;
	EDITOR_SANDBOX_VIEWPORT viewport;
	uint2 new_texture_size;
};

EDITOR_EVENT(WaitResourceLoadingRenderSandboxEvent) {
	WaitResourceLoadingRenderSandboxEventData* data = (WaitResourceLoadingRenderSandboxEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		RenderSandbox(editor_state, data->sandbox_index, data->viewport, data->new_texture_size);
		// Unlock the sandbox
		UnlockSandbox(editor_state, data->sandbox_index);
		return false;
	}
	return true;
}

bool RenderSandbox(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport, uint2 new_texture_size, bool disable_logging)
{
	// TODO: The messages are kinda annoying since they are called many times. Is this fine to always disable them?
	disable_logging = true;

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	// Change the new_texture_size if we have a pending request and this is { 0, 0 }
	if (new_texture_size.x == 0 && new_texture_size.y == 0) {
		new_texture_size = sandbox->viewport_pending_resize[viewport];
	}

	// If the rendering is disabled skip
	if (!IsSandboxViewportRendering(editor_state, sandbox_index, viewport)) {
		if (new_texture_size.x != 0 && new_texture_size.y != 0) {
			// Put the resize on hold
			sandbox->viewport_pending_resize[viewport] = new_texture_size;
		}
		return true;
	}

	// We assume that the Graphics module won't modify any entities
	unsigned int in_stream_module_index = GetSandboxGraphicsModule(editor_state, sandbox_index);
	if (in_stream_module_index == -1) {
		if (new_texture_size.x != 0 && new_texture_size.y != 0) {
			// Put the resize on hold
			sandbox->viewport_pending_resize[viewport] = new_texture_size;
		}
		return false;
	}

	// Check to see if we already have a pending event - if we do, skip the call
	ECS_STACK_CAPACITY_STREAM(void*, events_data, 256);
	EditorGetEventTypeData(editor_state, WaitCompilationRenderSandboxEvent, &events_data);
	for (unsigned int index = 0; index < events_data.size; index++) {
		WaitCompilationRenderSandboxEventData* data = (WaitCompilationRenderSandboxEventData*)events_data[index];
		if (data->sandbox_index == sandbox_index) {
			if (new_texture_size.x != 0 && new_texture_size.y != 0) {
				// Change the resize value
				data->new_texture_size = new_texture_size;
			}
			return true;
		}
	}

	// Check for the prevent loading resource flag
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		// Push an event to wait for the resource loading
		WaitResourceLoadingRenderSandboxEventData event_data;
		event_data.sandbox_index = sandbox_index;
		event_data.viewport = viewport;
		event_data.new_texture_size = new_texture_size;

		// Lock the sandbox such that it cannot be destroyed while we try to render to it
		LockSandbox(editor_state, sandbox_index);
		EditorAddEvent(editor_state, WaitResourceLoadingRenderSandboxEvent, &event_data, sizeof(event_data));
		return true;
	}

	// Check to see if the module is being compiled right now - if it is, then push an event to try re-render after
	// it has finished compiling
	EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_module_index);
	bool is_being_compiled = IsModuleBeingCompiled(editor_state, sandbox_module->module_index, sandbox_module->module_configuration);
	
	if (!is_being_compiled) {
		// Check to see if we need to resize the textures
		if (new_texture_size.x != 0 && new_texture_size.y != 0) {
			ResizeSandboxRenderTextures(editor_state, sandbox_index, viewport, new_texture_size);
		}

		// Get the resource manager and graphics snapshot
		ResourceManagerSnapshot resource_snapshot = editor_state->RuntimeResourceManager()->GetSnapshot(editor_state->EditorAllocator());
		GraphicsResourceSnapshot graphics_snapshot = RenderSandboxInitializeGraphics(editor_state, sandbox_index, viewport);

		EntityManager* viewport_entity_manager = sandbox->sandbox_world.entity_manager;
		EntityManager* runtime_entity_manager = sandbox->sandbox_world.entity_manager;

		// Create a temporary task scheduler that will be bound to the sandbox world
		MemoryManager viewport_task_scheduler_allocator = TaskScheduler::DefaultAllocator(editor_state->GlobalMemoryManager());
		TaskScheduler viewport_task_scheduler(&viewport_task_scheduler_allocator);

		// Use the editor state task_manage in order to run the commands - at the end reset the static tasks
		TaskScheduler* runtime_task_scheduler = sandbox->sandbox_world.task_scheduler;
		TaskManager* runtime_task_manager = sandbox->sandbox_world.task_manager;

		sandbox->sandbox_world.task_manager = editor_state->render_task_manager;
		sandbox->sandbox_world.task_scheduler = &viewport_task_scheduler;

		MemoryManager runtime_query_cache_allocator = ArchetypeQueryCache::DefaultAllocator(GetAllocatorPolymorphic(editor_state->GlobalMemoryManager()));
		ArchetypeQueryCache runtime_query_cache;

		EDITOR_SANDBOX_VIEWPORT active_viewport = GetSandboxActiveViewport(editor_state, sandbox_index);
		if (active_viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
			viewport_entity_manager = &sandbox->scene_entities;

			// Bind to the sandbox the scene entity manager
			sandbox->sandbox_world.entity_manager = viewport_entity_manager;
		}

		// We need to record the query cache to restore it later
		sandbox->sandbox_world.entity_manager->CopyQueryCache(&runtime_query_cache, GetAllocatorPolymorphic(&runtime_query_cache_allocator));

		auto deallocate_temp_resources_and_restore = [&]() {
			AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
			resource_snapshot.Deallocate(editor_allocator);
			graphics_snapshot.Deallocate(editor_allocator);
			viewport_task_scheduler_allocator.Free();
			runtime_query_cache_allocator.Free();

			sandbox->sandbox_world.task_manager = runtime_task_manager;
			sandbox->sandbox_world.entity_manager = runtime_entity_manager;
			sandbox->sandbox_world.task_scheduler = runtime_task_scheduler;
		};

		// Prepare the task scheduler
		bool success = ConstructSandboxSchedulingOrder(editor_state, sandbox_index, { &in_stream_module_index, 1 }, disable_logging);
		if (!success) {
			deallocate_temp_resources_and_restore();
			return false;
		}

		// Prepare and run the world once
		PrepareWorld(&sandbox->sandbox_world);

		DoFrame(&sandbox->sandbox_world);

		sandbox->sandbox_world.entity_manager = runtime_entity_manager;
		sandbox->sandbox_world.task_manager = runtime_task_manager;
		sandbox->sandbox_world.task_scheduler = runtime_task_scheduler;

		// Deallocate the task scheduler and the entity manager query cache
		// Reset the task manager static tasks
		viewport_task_scheduler_allocator.Free();
		runtime_query_cache_allocator.Free();
		editor_state->render_task_manager->ClearTemporaryAllocators();
		editor_state->render_task_manager->ResetStaticTasks();

		// Restore the resource manager first
		editor_state->RuntimeResourceManager()->RestoreSnapshot(resource_snapshot);
		resource_snapshot.Deallocate(editor_state->EditorAllocator());

		// Now the graphics snapshot will be restored as well
		RenderSandboxFinishGraphics(editor_state, sandbox_index, graphics_snapshot, viewport);

		return true;
	}
	else {
		// Wait for the compilation
		WaitCompilationRenderSandboxEventData event_data;
		event_data.sandbox_index = sandbox_index;
		event_data.viewport = viewport;
		event_data.new_texture_size = new_texture_size;

		// Also lock this sandbox such that it cannot be destroyed while we try to render to it
		LockSandbox(editor_state, sandbox_index);
		EditorAddEvent(editor_state, WaitCompilationRenderSandboxEvent, &event_data, sizeof(event_data));
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool RenderSandboxIsPending(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	// If the module has rendering disabled don't report as pending

	// Check to see if we already have a pending event - if we do, skip the call
	bool pending_event = EditorHasEvent(editor_state, WaitCompilationRenderSandboxEvent);
	if (pending_event) {
		return true;
	}

	// We assume that the Graphics module won't modify any entities
	unsigned int in_stream_module_index = GetSandboxGraphicsModule(editor_state, sandbox_index);
	if (in_stream_module_index == -1) {
		return false;
	}

	// Check to see if the module is being compiled
	const EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_module_index);
	bool is_being_compiled = IsModuleBeingCompiled(editor_state, sandbox_module->module_index, sandbox_module->module_configuration);
	return is_being_compiled;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool RenderSandboxViewports(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::uint2 new_size, bool disable_logging)
{
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_VIEWPORT_COUNT; index++) {
		success &= RenderSandbox(editor_state, sandbox_index, (EDITOR_SANDBOX_VIEWPORT)index, new_size, disable_logging);
	}
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ResizeSandboxRenderTextures(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport, uint2 new_size)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	
	// If the resource loading is active we need to wait
	EditorStateWaitFlag(20, editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING, false);

	Graphics* runtime_graphics = editor_state->RuntimeGraphics();
	// Free the current textures and create new ones
	FreeSandboxRenderTextures(editor_state, sandbox_index, viewport);

	GraphicsRenderDestinationOptions destination_options;
	destination_options.render_misc = ECS_GRAPHICS_MISC_SHARED;
	destination_options.depth_misc = ECS_GRAPHICS_MISC_SHARED;
	sandbox->viewport_render_destination[viewport] = runtime_graphics->CreateRenderDestination(new_size, destination_options);

	// Now transfer the texture from the RuntimeGraphics to the UIGraphics
	sandbox->viewport_transferred_texture[viewport] = editor_state->UIGraphics()->TransferGPUView(sandbox->viewport_render_destination[viewport].output_view);

	ECS_STACK_CAPACITY_STREAM(char, visualize_name, 512);

	Stream<char> viewport_description = ViewportString(viewport);
	GetVisualizeTextureName(viewport_description, sandbox_index, true, visualize_name);

	// Add the sandbox render and depth textures into the visualize textures
	VisualizeTextureSelectElement visualize_element;
	visualize_element.transfer_texture_to_ui_graphics = true;
	visualize_element.name = visualize_name;
	visualize_element.texture = sandbox->viewport_render_destination[viewport].output_view.AsTexture2D();
	visualize_element.override_format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB;
	SetVisualizeTexture(editor_state, visualize_element);

	visualize_name.size = 0;
	GetVisualizeTextureName(viewport_description, sandbox_index, false, visualize_name);
	visualize_element.name = visualize_name;
	visualize_element.texture = sandbox->viewport_render_destination[viewport].depth_view.GetResource();
	visualize_element.override_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
	SetVisualizeTexture(editor_state, visualize_element);

	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		// Also create the instanced framebuffers
		Texture2DDescriptor instanced_framebuffer_descriptor;
		instanced_framebuffer_descriptor.format = ECS_GRAPHICS_FORMAT_R32_UINT;
		instanced_framebuffer_descriptor.bind_flag = ECS_GRAPHICS_BIND_RENDER_TARGET | ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		instanced_framebuffer_descriptor.mip_levels = 1;
		instanced_framebuffer_descriptor.size = new_size;
		instanced_framebuffer_descriptor.misc_flag = ECS_GRAPHICS_MISC_SHARED;
		Texture2D instanced_framebuffer_texture = runtime_graphics->CreateTexture(&instanced_framebuffer_descriptor);
		sandbox->scene_viewport_instance_framebuffer = runtime_graphics->CreateRenderTargetView(instanced_framebuffer_texture);

		visualize_name.size = 0;
		GetVisualizeInstancedTextureName(sandbox_index, true, visualize_name);
		visualize_element.name = visualize_name;
		visualize_element.texture = sandbox->scene_viewport_instance_framebuffer.GetResource();
		SetVisualizeTexture(editor_state, visualize_element);

		Texture2DDescriptor instanced_depth_stencil_descriptor;
		instanced_depth_stencil_descriptor.format = ECS_GRAPHICS_FORMAT_D32_FLOAT;
		instanced_depth_stencil_descriptor.bind_flag = ECS_GRAPHICS_BIND_DEPTH_STENCIL;
		instanced_depth_stencil_descriptor.mip_levels = 1;
		instanced_depth_stencil_descriptor.size = new_size;
		instanced_depth_stencil_descriptor.misc_flag = ECS_GRAPHICS_MISC_SHARED;
		Texture2D instanced_depth_texture = runtime_graphics->CreateTexture(&instanced_depth_stencil_descriptor);
		sandbox->scene_viewport_depth_stencil_framebuffer = runtime_graphics->CreateDepthStencilView(instanced_depth_texture);

		visualize_name.size = 0;
		GetVisualizeInstancedTextureName(sandbox_index, false, visualize_name);
		visualize_element.name = visualize_name;
		visualize_element.texture = sandbox->scene_viewport_depth_stencil_framebuffer.GetResource();
		SetVisualizeTexture(editor_state, visualize_element);
	}

	// Disable the pending resize
	sandbox->viewport_pending_resize[viewport] = { 0, 0 };
}

// ------------------------------------------------------------------------------------------------------------------------------

void ResetSandboxUnusedEntities(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->unused_entities_slots[viewport].FreeBuffer();
	SignalSandboxUnusedEntitiesSlotsCounter(editor_state, sandbox_index, viewport);
}

// -----------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxUnusedEntitiesSlot(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_ENTITY_SLOT slot_type, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);

	unsigned int size = sandbox->unused_entities_slots[viewport].size;
	unsigned int offset = 0;

	auto find = [&]() {
		for (unsigned int index = 0; index < size; index++) {
			if (sandbox->unused_entity_slot_type[viewport][offset + index] == slot_type) {
				unsigned int return_value = offset + index;
				offset += index + 1;
				size -= index + 1;
				return return_value;
			}
		}
		return (unsigned int)-1;
	};

	unsigned int slot_index = find();
	while (slot_index != -1) {
		sandbox->unused_entities_slots[viewport].RemoveSwapBack(slot_index);
		sandbox->unused_entity_slot_type[viewport].RemoveSwapBack(slot_index);
		slot_index = find();
	}
	SignalSandboxUnusedEntitiesSlotsCounter(editor_state, sandbox_index, viewport);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool RunSandboxWorld(EditorState* editor_state, unsigned int sandbox_index, bool is_step)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 256, ECS_MB * 8);
	ECS_STACK_CAPACITY_STREAM(char, snapshot_message, ECS_KB * 64);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (is_step) {
		ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_PAUSED);
	}
	else {
		ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_RUNNING);
	}

	// Clear the sandbox waiting compilation flag - it will be reset if it is still valid
	sandbox->flags = function::ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);

	// Before running the simulation, we need to check to see if the modules are still valid - they might have been changed
	// That's why check the snapshot
	SOLVE_SANDBOX_MODULE_SNAPSHOT_RESULT solve_module_snapshot_result = SolveSandboxModuleSnapshotsChanges(editor_state, sandbox_index);
	if (solve_module_snapshot_result == SOLVE_SANDBOX_MODULE_SNAPSHOT_FAILURE) {
		// The simulation needs to be halted
		// We don't need to wait for the pause since the world is not running
		PauseSandboxWorld(editor_state, sandbox_index);
		return false;
	}
	else if (solve_module_snapshot_result == SOLVE_SANDBOX_MODULE_SNAPSHOT_WAIT) {
		sandbox->flags = function::SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);
		return true;
	}
	// If it returned OK, then we can proceed as normal

	// Before actually running, we also need to check the asset reference snapshot
	HandleSandboxAssetHandlesSnapshotsChanges(editor_state, sandbox_index, false);

	// Perform the world simulation - before that we need to set the graphics buffers up
	// And retrieve the graphics snapshot and the resource manager snapshot
	// If any resources that were loaded by the editor are missing after the simulation has run,
	// Stop all simulations and notify the user

	// If we have a pending texture resize, perform it
	if (sandbox->viewport_pending_resize[EDITOR_SANDBOX_VIEWPORT_RUNTIME].x != 0 && sandbox->viewport_pending_resize[EDITOR_SANDBOX_VIEWPORT_RUNTIME].y != 0) {
		ResizeSandboxRenderTextures(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME, sandbox->viewport_pending_resize[EDITOR_SANDBOX_VIEWPORT_RUNTIME]);
	}

	GraphicsResourceSnapshot graphics_snapshot = RenderSandboxInitializeGraphics(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	ResourceManagerSnapshot resource_snapshot = editor_state->RuntimeResourceManager()->GetSnapshot(GetAllocatorPolymorphic(&stack_allocator));
	DoFrame(&sandbox->sandbox_world);

	bool graphics_snapshot_success = editor_state->RuntimeGraphics()->RestoreResourceSnapshot(graphics_snapshot, &snapshot_message);
	if (snapshot_message.size > 0) {
		ECS_FORMAT_TEMP_STRING(console_message, "There are mismatches in the graphics resource snapshot after running sandbox {#}", sandbox_index);
		EditorSetConsoleError(console_message);
		EditorSetConsoleError(snapshot_message);
	}

	snapshot_message.size = 0;
	bool resource_snapshot_success = editor_state->RuntimeResourceManager()->RestoreSnapshot(resource_snapshot, &snapshot_message);
	if (snapshot_message.size > 0) {
		ECS_FORMAT_TEMP_STRING(console_message, "There are mismatches in the resource manager snapshot after running sandbox {#}", sandbox_index);
		EditorSetConsoleError(console_message);
		EditorSetConsoleError(snapshot_message);
	}

	// The graphics snapshot was allocated from the editor allocator, we need to deallocate it
	graphics_snapshot.Deallocate(editor_state->EditorAllocator());

	// Render the scene if it is visible - the runtime is rendered by the game now		
	EnableSceneUIRendering(editor_state, sandbox_index);

	RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);

	// Disable this irrespective if it was enabled here or not
	DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);

	return graphics_snapshot_success && resource_snapshot_success;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool RunSandboxWorlds(EditorState* editor_state, bool is_step)
{
	bool success = true;
	ForEachSandbox(editor_state, [&](EditorSandbox* sandbox, unsigned int sandbox_index) {
		if (sandbox->should_step && sandbox->run_state == EDITOR_SANDBOX_PAUSED) {
			success &= RunSandboxWorld(editor_state, sandbox_index, is_step);
		}
	});
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveSandboxRuntimeSettings(const EditorState* editor_state, unsigned int sandbox_index)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	bool success = SaveRuntimeSettings(editor_state, sandbox->runtime_settings, &sandbox->runtime_descriptor, &error_message);
	if (!success) {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to save sandbox {#} runtime settings ({#}). Detailed message: {#}.", sandbox_index, sandbox->runtime_settings, error_message);
		EditorSetConsoleError(console_message);
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveRuntimeSettings(const EditorState* editor_state, Stream<wchar_t> filename, const WorldDescriptor* descriptor, CapacityStream<char>* error_message)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetSandboxRuntimeSettingsPath(editor_state, filename, absolute_path);

	SerializeOptions options;
	options.error_message = error_message;

	const Reflection::ReflectionManager* reflection_manager = editor_state->ui_reflection->reflection;
	ECS_SERIALIZE_CODE code = Serialize(reflection_manager, reflection_manager->GetType(STRING(WorldDescriptor)), descriptor, absolute_path, &options);
	if (code != ECS_SERIALIZE_OK) {
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxSceneDirty(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	// If it is paused, then we don't need to make the scene dirty since the change is made on the runtime sandbox world
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE || GetSandboxState(editor_state, sandbox_index) == EDITOR_SANDBOX_SCENE) {
		sandbox->is_scene_dirty = true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, const WorldDescriptor& descriptor)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	WorldDescriptor old_descriptor = sandbox->runtime_descriptor;

	// Remember the graphics, task manager and task scheduler
	// Also the mouse and keyboard
	Graphics* sandbox_graphics = sandbox->runtime_descriptor.graphics;
	TaskManager* task_manager = sandbox->runtime_descriptor.task_manager;
	TaskScheduler* task_scheduler = sandbox->runtime_descriptor.task_scheduler;
	Mouse* mouse = sandbox->runtime_descriptor.mouse;
	Keyboard* keyboard = sandbox->runtime_descriptor.keyboard;

	memcpy(&sandbox->runtime_descriptor, &descriptor, sizeof(WorldDescriptor));

	sandbox->runtime_descriptor.graphics = sandbox_graphics;
	sandbox->runtime_descriptor.task_manager = task_manager;
	sandbox->runtime_descriptor.task_scheduler = task_scheduler;
	sandbox->runtime_descriptor.mouse = mouse;
	sandbox->runtime_descriptor.keyboard = keyboard;

	// Reinitialize the runtime
	ReinitializeSandboxRuntime(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxGraphicsTextures(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	
	// Clear the depth view
	editor_state->RuntimeGraphics()->ClearDepthStencil(sandbox->viewport_render_destination[viewport].depth_view);

	sandbox->sandbox_world.graphics->ChangeInitialRenderTarget(
		sandbox->viewport_render_destination[viewport].render_view, 
		sandbox->viewport_render_destination[viewport].depth_view
	);

	// Get the texel size for the textures and bind a viewport according to that
	GraphicsViewport graphics_viewport = GetGraphicsViewportForTexture(sandbox->viewport_render_destination[viewport].render_view.GetResource());
	sandbox->sandbox_world.graphics->BindViewport(graphics_viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

void SetSandboxUnusedEntitySlotType(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int slot_index,
	EDITOR_SANDBOX_ENTITY_SLOT slot_type,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	sandbox->unused_entity_slot_type[viewport][slot_index] = slot_type;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool ShouldSandboxRecomputeEntitySlots(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	return sandbox->unused_entities_slots_recompute[viewport];
}

// -----------------------------------------------------------------------------------------------------------------------------

struct SignalSelectedEntitiesCounterEventData {
	unsigned int sandbox_index;
};

EDITOR_EVENT(SignalSelectedEntitiesCounterEvent) {
	SignalSelectedEntitiesCounterEventData* data = (SignalSelectedEntitiesCounterEventData*)_data;
	GetSandbox(editor_state, data->sandbox_index)->selected_entities_changed_counter++;
	return false;
}

void SignalSandboxSelectedEntitiesCounter(EditorState* editor_state, unsigned int sandbox_index)
{
	SignalSelectedEntitiesCounterEventData event_data;
	event_data.sandbox_index = sandbox_index;
	EditorAddEvent(editor_state, SignalSelectedEntitiesCounterEvent, &event_data, sizeof(event_data));
}

// -----------------------------------------------------------------------------------------------------------------------------

struct SignalUnusedEntitiesSlotsCounterEventData {
	unsigned int sandbox_index;
	EDITOR_SANDBOX_VIEWPORT viewport;
};

EDITOR_EVENT(SignalUnusedEntitiesSlotsCounterEvent) {
	SignalUnusedEntitiesSlotsCounterEventData* data = (SignalUnusedEntitiesSlotsCounterEventData*)_data;
	EDITOR_SANDBOX_VIEWPORT viewport = GetSandboxViewportOverride(editor_state, data->sandbox_index, data->viewport);
	GetSandbox(editor_state, data->sandbox_index)->unused_entities_slots_recompute[viewport] = true;
	return false;
}

void SignalSandboxUnusedEntitiesSlotsCounter(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	SignalUnusedEntitiesSlotsCounterEventData event_data;
	event_data.sandbox_index = sandbox_index;
	event_data.viewport = viewport;
	EditorAddEvent(editor_state, SignalUnusedEntitiesSlotsCounterEvent, &event_data, sizeof(event_data));
}

// -----------------------------------------------------------------------------------------------------------------------------

bool StartSandboxWorld(EditorState* editor_state, unsigned int sandbox_index, bool disable_error_messages)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_PAUSED || sandbox->run_state == EDITOR_SANDBOX_SCENE);

	// Check to see if the modules referenced by the sandbox are loaded
	bool success = AreSandboxModulesLoaded(editor_state, sandbox_index, true);
	bool waiting_sandbox_compile = false;
	if (!success) {
		// Launch the build command for those modules that are not yet updated
		bool all_are_skipped = CompileSandboxModules(editor_state, sandbox_index);
		// Set the success to true even tho we don't know if the modules have actually compiled.
		// We let the runtime decide later to stop this world when the compilation ends
		success = true;
		waiting_sandbox_compile = !all_are_skipped;
	}

	// If we are in the scene state, we need to construct the scheduling order and prepare the world
	if (sandbox->run_state == EDITOR_SANDBOX_SCENE) {
		if (!waiting_sandbox_compile) {
			success = ConstructSandboxSchedulingOrder(editor_state, sandbox_index, disable_error_messages);
		}
		if (success) {
			// Copy the entities from the scene to the runtime
			CopySceneEntitiesIntoSandboxRuntime(editor_state, sandbox_index);
			// Prepare the sandbox world
			PrepareWorld(&sandbox->sandbox_world);

			// Here we also need to double the reference counts of the assets stored in the runtime
			// Since when returning to the scene representation, we can have the scene assets still loaded
			AssetDatabaseReference* asset_database = &sandbox->database;
			asset_database->IncrementReferenceCounts(true);
		}
	}
	// Else if we are in the paused state we just need to change the state
	if (success) {
		// Disable the rendering of the scene and game windows - we will draw them every frame
		DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
		sandbox->run_state = EDITOR_SANDBOX_RUNNING;

		// We need to record the snapshot of the current sandbox state
		// We also need to bind the module runtime settings
		RegisterSandboxModuleSnapshots(editor_state, sandbox_index);
		HandleSandboxAssetHandlesSnapshotsChanges(editor_state, sandbox_index, true);
		BindSandboxRuntimeModuleSettings(editor_state, sandbox_index);
		BindSandboxGraphicsSceneInfo(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	}
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool StartSandboxWorlds(EditorState* editor_state, bool paused_only)
{
	bool success = true;
	ForEachSandbox(editor_state, [&](EditorSandbox* sandbox, unsigned int sandbox_index) {
		if (paused_only) {
			if (sandbox->should_pause && sandbox->run_state == EDITOR_SANDBOX_PAUSED) {
				success &= StartSandboxWorld(editor_state, sandbox_index);
			}
		}
		else {
			if (sandbox->should_play && (sandbox->run_state == EDITOR_SANDBOX_PAUSED || sandbox->run_state == EDITOR_SANDBOX_SCENE)) {
				success &= StartSandboxWorld(editor_state, sandbox_index);
			}
		}
	});
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool UpdateSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return UpdateRuntimeSettings(editor_state, sandbox->runtime_settings, &sandbox->runtime_settings_last_write);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool UpdateRuntimeSettings(const EditorState* editor_state, Stream<wchar_t> filename, size_t* last_write)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetSandboxRuntimeSettingsPath(editor_state, filename, absolute_path);
	size_t new_last_write = OS::GetFileLastWrite(absolute_path);
	if (new_last_write > *last_write) {
		*last_write = new_last_write;
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnlockSandbox(EditorState* editor_state, unsigned int sandbox_index)
{
	GetSandbox(editor_state, sandbox_index)->locked_count.fetch_sub(1, ECS_RELAXED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void WaitSandboxUnlock(const EditorState* editor_state, unsigned int sandbox_index)
{
	if (sandbox_index == -1) {
		for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
			const EditorSandbox* sandbox = GetSandbox(editor_state, index);
			TickWait<'!'>(15, sandbox->locked_count, (unsigned char)0);
		}
	}
	else {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		TickWait<'!'>(15, sandbox->locked_count, (unsigned char)0);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TickSandboxesRuntimeSettings(EditorState* editor_state) {
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_RUNTIME_SETTINGS, LAZY_EVALUATION_RUNTIME_SETTINGS)) {
		ReloadSandboxRuntimeSettings(editor_state);
	}
}

void TickSandboxesSelectedEntities(EditorState* editor_state) {
	ForEachSandbox(editor_state, [&](EditorSandbox* sandbox, unsigned int sandbox_index) {
		if (sandbox->selected_entities_changed_counter > 0) {
			RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
		}
		sandbox->selected_entities_changed_counter = function::SaturateSub(sandbox->selected_entities_changed_counter, (unsigned char)1);
	});
}

void TickSandboxesClearUnusedSlots(EditorState* editor_state) {
	ForEachSandbox(editor_state, [](EditorSandbox* sandbox, unsigned int sandbox_index) {
		for (size_t viewport = 0; viewport < EDITOR_SANDBOX_VIEWPORT_COUNT; viewport++) {
			sandbox->unused_entities_slots_recompute[viewport] = function::SaturateSub<unsigned char>(sandbox->unused_entities_slots_recompute[viewport], 1);
		}
	});
}

void TickSandboxes(EditorState* editor_state)
{
	TickSandboxesRuntimeSettings(editor_state);
	TickSandboxesSelectedEntities(editor_state);
	TickSandboxesClearUnusedSlots(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UISetInstantFramePacing(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
	system->RemoveFrameHandler(UISetInstantFramePacing, nullptr);
}

void TickSandboxRuntimes(EditorState* editor_state)
{
	// Add a system frame handler to the UI that will set the frame pacing to INSTANT no matter what
	bool added_instant_frame_handle = false;

	unsigned int sandbox_count = GetSandboxCount(editor_state);
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) && !EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		ForEachSandbox<true>(editor_state, [&](EditorSandbox* sandbox, unsigned int sandbox_index) {
			// Check to see if the current sandbox is being run
			if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
				bool simulation_success = RunSandboxWorld(editor_state, sandbox_index);

				// If there was an error with the snapshot, then pause all other sandboxes
				if (!simulation_success) {
					for (unsigned int subindex = 0; subindex < sandbox_count; subindex++) {
						if (GetSandboxState(editor_state, subindex) == EDITOR_SANDBOX_RUNNING) {
							// Don't wait for each one to wait. The threads will simply stop when they are finished
							PauseSandboxWorld(editor_state, subindex, false);
						}
					}
					// We can exit the loop now
					return true;
				}
				else {
					if (!added_instant_frame_handle) {
						added_instant_frame_handle = true;
						editor_state->ui_system->PushFrameHandler({ UISetInstantFramePacing, nullptr, 0 });
					}
				}
			}
			return false;
		});
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TickSandboxUpdateMasterButtons(EditorState* editor_state)
{
	// Each frame check to see if we need to update the state of the master buttons
	bool are_all_default_running = AreAllDefaultSandboxesRunning(editor_state);
	bool are_all_default_paused = AreAllDefaultSandboxesPaused(editor_state);
	bool are_all_default_not_started = AreAllDefaultSandboxesNotStarted(editor_state);

	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING)) {
		if (!are_all_default_running) {
			EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PLAYING);

			if (!are_all_default_paused) {
				// If any of them is terminated, terminate all of them - if the are not already
				if (!are_all_default_not_started) {
					EndSandboxWorldSimulations(editor_state);
				}
			}
		}
	}
	else {
		bool is_any_running = IsAnyDefaultSandboxRunning(editor_state);
		if (is_any_running) {
			EndSandboxWorldSimulations(editor_state);
		}
	}

	// These need to be refreshed since some action before might have changed the values
	are_all_default_running = AreAllDefaultSandboxesRunning(editor_state);
	are_all_default_paused = AreAllDefaultSandboxesPaused(editor_state);
	are_all_default_not_started = AreAllDefaultSandboxesNotStarted(editor_state);

	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
		if (!are_all_default_paused) {
			EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PAUSED);

			if (!are_all_default_running) {
				// If any of them is terminated, terminate all of them - if they are not already
				if (!are_all_default_not_started) {
					EndSandboxWorldSimulations(editor_state);
				}
			}
		}
	}
	else {
		bool is_any_paused = IsAnyDefaultSandboxPaused(editor_state);
		if (is_any_paused) {
			RunSandboxWorlds(editor_state);
			if (!EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING)) {
				EditorStateSetFlag(editor_state, EDITOR_STATE_IS_PLAYING);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------