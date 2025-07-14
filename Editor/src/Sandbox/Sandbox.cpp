#include "editorpch.h"
#include "Sandbox.h"
#include "SandboxScene.h"
#include "SandboxModule.h"
#include "SandboxCrashHandler.h"
#include "SandboxProfiling.h"
#include "SandboxFile.h"
#include "SandboxRecording.h"
#include "SandboxReplay.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorEvent.h"
#include "../Editor/EditorPalette.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Modules/Module.h"
#include "../Project/ProjectFolders.h"
#include "../Project/ProjectSettings.h"
#include "../Modules/ModuleSettings.h"

#include "../Editor/EditorScene.h"
#include "ECSEngineSerializationHelpers.h"
#include "ECSEngineAssets.h"
#include "ECSEngineECSRuntimeResources.h"
#include "ECSEngineECSRuntimeSystems.h"

// The UI needs to be included because we need to notify it when we destroy a sandbox
#include "../UI/Game.h"
#include "../UI/Scene.h"
#include "../UI/EntitiesUI.h"
#include "../UI/SandboxUI.h"

using namespace ECSEngine;

#define RUNTIME_SETTINGS_STRING_CAPACITY (128)
#define SCENE_PATH_STRING_CAPACITY (128)

#define LAZY_EVALUATION_RUNTIME_SETTINGS 500

#define SANDBOX_ALLOCATOR_EXTRA_CAPACITY ECS_MB * 100
#define SANDBOX_ALLOCATOR_BACKUP_EXTRA_CAPACITY ECS_MB * 50

Stream<char> EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY_STRINGS[] = {
	"CPU Usage",
	"GPU Usage",
	"RAM Usage",
	"VRAM Usage",
	"Simulation FPS/Time",
	"Overall FPS/Time",
	"GPU Simulation FPS/Time"
};

static_assert(std::size(EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY_STRINGS) == EDITOR_SANDBOX_STATISTIC_DISPLAY_COUNT);

// -----------------------------------------------------------------------------------------------------------------------------

ECS_INLINE static Stream<char> ColorDepthTextureString(bool color_texture) {
	return color_texture ? "render" : "depth";
}

ECS_INLINE static void GetVisualizeTextureName(Stream<char> viewport_description, unsigned int sandbox_index, bool color_texture, CapacityStream<char>& name) {
	FormatString(name, "{#} {#} {#}", viewport_description, sandbox_index, ColorDepthTextureString(color_texture));
}

// -----------------------------------------------------------------------------------------------------------------------------

ECS_INLINE static void GetVisualizeInstancedTextureName(unsigned int sandbox_index, bool color_texture, CapacityStream<char>& name) {
	FormatString(name, "Scene {#} instanced {#}", sandbox_index, ColorDepthTextureString(color_texture));
}

// -----------------------------------------------------------------------------------------------------------------------------

ECS_INLINE static bool IsSandboxRuntimePreinitialized(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandbox(editor_state, sandbox_index)->runtime_descriptor.graphics != nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------

// Restores the mouse state to visible and no raw input. Does not take into consideration the PAUSE_UNPASE flags.
static void RestoreMouseState(EditorState* editor_state) {
	Mouse* mouse = editor_state->Mouse();
	if (!mouse->IsVisible()) {
		mouse->SetCursorVisibility(true);
	}
	if (mouse->GetRawInputStatus()) {
		mouse->DisableRawInput();
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

struct ComponentFunctionsAutoGeneratorData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

static ComponentFunctions ComponentFunctionsAutoGenerator(EntityManagerAutoGenerateComponentFunctionsFunctorData* functor_data) {
	ComponentFunctionsAutoGeneratorData* data = (ComponentFunctionsAutoGeneratorData*)functor_data->user_data;
	return GetSandboxComponentFunctions(data->editor_state, data->sandbox_index, functor_data->component_name, functor_data->temporary_allocator, functor_data->compare_entry);
}

// -----------------------------------------------------------------------------------------------------------------------------

// Stores all the current state of the modules being used such that changes to them can be detected dynamically
static void RegisterSandboxModuleSnapshots(EditorState* editor_state, unsigned int sandbox_index) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	sandbox->runtime_module_snapshot_allocator.Clear();
	sandbox->runtime_module_snapshots.size = 0;
	sandbox->runtime_module_snapshots.capacity = 0;
	sandbox->runtime_module_snapshots.Resize(sandbox->modules_in_use.size);
	
	AllocatorPolymorphic snapshot_allocator = &sandbox->runtime_module_snapshot_allocator;

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
		snapshot.tried_loading = false;

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
			bool is_compiled = SearchBytes(modules_being_compiled.ToStream(), needed_but_missing_modules[index]) != -1;
			if (!is_compiled) {
				return false;
			}
		}
		return true;
	};

	bool is_waiting_compilation = HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);

	// Consider modules to be loaded only if they are not out of date
	bool are_modules_loaded = AreSandboxModulesLoaded(editor_state, sandbox_index, true);
	if (!are_modules_loaded) {
		if (!is_waiting_compilation) {
			// Compile all the modules which are not yet loaded - but skip those
			// For which we have already tried reloading and their dependencies
			// Are not satisfied
			ECS_STACK_CAPACITY_STREAM(unsigned int, missing_modules, 512);
			GetSandboxNeededButMissingModules(editor_state, sandbox_index, missing_modules, true);
			
			ECS_STACK_CAPACITY_STREAM(unsigned int, compile_module_indices, 512);
			ECS_STACK_CAPACITY_STREAM(EDITOR_MODULE_CONFIGURATION, compile_module_configuration, 512);

			for (unsigned int index = 0; index < missing_modules.size; index++) {
				unsigned int snapshot_index = FindString(
					editor_state->project_modules->buffer[missing_modules[index]].solution_path,
					sandbox->runtime_module_snapshots.ToStream(),
					[](const EditorSandboxModuleSnapshot& snapshot) {
						return snapshot.solution_path;
					}
				);
				if (snapshot_index != -1) {
					// Check to see if we have already tried compiling this and it failed
					if (!sandbox->runtime_module_snapshots[snapshot_index].tried_loading) {
						// Add it
						compile_module_indices.AddAssert(missing_modules[index]);
						compile_module_configuration.Add(sandbox->runtime_module_snapshots[snapshot_index].module_configuration);
						sandbox->runtime_module_snapshots[snapshot_index].tried_loading = true;
					}
				}
			}

			if (compile_module_indices.size > 0) {
				// We don't need this, but it is required to be passed
				ECS_STACK_CAPACITY_STREAM(EDITOR_LAUNCH_BUILD_COMMAND_STATUS, launch_status, 512);
				BuildModules(editor_state, compile_module_indices, compile_module_configuration.buffer, launch_status.buffer);
			}
		}

		// Check if the there are modules being compiled and wait for them if that is the case
		bool should_wait = are_out_of_date_or_failed_modules_being_compiled();
		if (should_wait) {
			return SOLVE_SANDBOX_MODULE_SNAPSHOT_WAIT;
		}

		// We need to halt the continuation - also re-register the snapshot
		ClearSandboxRuntimeWorldInfo(editor_state, sandbox_index);
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
							// Check to see if the load status was changed
							if (sandbox->runtime_module_snapshots[index].load_status != module->infos[snapshot_configuration].load_status) {
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
											unsigned int existing_setting = FindString(snapshot_settings[subindex].name, module_settings, [](EditorModuleReflectedSetting setting) {
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
	}

	if (snapshot_has_changed) {
		// The snapshot has changed, we need to restore the module settings and rebind the task scheduler and task manager
		ClearSandboxRuntimeWorldInfo(editor_state, sandbox_index);
		// We need to discard the current snapshot and recreate it
		RegisterSandboxModuleSnapshots(editor_state, sandbox_index);
		
		bool prepare_success = PrepareSandboxRuntimeWorldInfo(editor_state, sandbox_index);
		if (!prepare_success) {
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
	EditorSandboxAssetHandlesSnapshot& snapshot = sandbox->runtime_asset_handle_snapshot;

	if (initialize) {
		snapshot.allocator.Clear();
		// Clear and reset everything
		snapshot.handle_count = 0;
		snapshot.handle_capacity = 0;

		// When initializing, we can skip adding the handles at the start
		// Since the next iteration will pick up on this
	}
	else {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB * 8);
		AllocatorPolymorphic stack_allocator = &_stack_allocator;

		const AssetDatabase* database = editor_state->asset_database;
		SandboxReferenceCountsFromEntities asset_reference_counts = GetSandboxAssetReferenceCountsFromEntities(
			editor_state,
			sandbox_index,
			EDITOR_SANDBOX_VIEWPORT_RUNTIME,
			stack_allocator
		);

		// Determine the difference between the 2 snapshots
		// Allocate for each handle in the stored snapshot a boolean such that we mark those that were already found
		size_t was_found_size = snapshot.handle_count;
		bool* was_found = (bool*)_stack_allocator.Allocate(sizeof(bool) * was_found_size);
		memset(was_found, 0, sizeof(bool) * was_found_size);

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
			for (size_t subindex = 0; subindex < asset_reference_counts.counts[index].size; subindex++) {
				// Search for the handle in the stored snapshot
				unsigned int current_handle = database->GetAssetHandleFromIndex(subindex, current_type);
				size_t found_index = snapshot.FindHandle(current_handle, current_type);
				unsigned int current_reference_count = asset_reference_counts.counts[index][subindex];
				if (found_index != -1) {
					was_found[found_index] = true;
					// Compare the reference count
					unsigned int snapshot_reference_count = snapshot.reference_counts[found_index];

					if (snapshot_reference_count < current_reference_count) {
						// Increment the asset reference count
						IncrementAssetReferenceInSandbox(editor_state, current_handle, current_type, sandbox_index, current_reference_count - snapshot_reference_count);
						// Record the delta change
						snapshot.reference_counts_change[found_index] += current_reference_count - snapshot_reference_count;
						snapshot.reference_counts[found_index] = current_reference_count;
					}
					else if (snapshot_reference_count > current_reference_count) {
						unsigned int difference = snapshot_reference_count - current_reference_count;
						DecrementAssetReference(editor_state, current_handle, current_type, sandbox_index, difference);
						// Record the delta change
						snapshot.reference_counts_change[found_index] -= difference;
						snapshot.reference_counts[found_index] = current_reference_count;
					}
				}
				else {
					// The handle didn't exist before, now it exists - it will be added to the snapshot anyway
					// Increment the reference count
					if (current_reference_count > 0) {
						IncrementAssetReferenceInSandbox(editor_state, current_handle, current_type, sandbox_index, current_reference_count);
						SoAResizeIfFull(
							sandbox->GlobalMemoryManager(),
							snapshot.handle_count,
							snapshot.handle_capacity,
							&snapshot.handles,
							&snapshot.reference_counts,
							&snapshot.reference_counts_change,
							&snapshot.handle_types
						);

						unsigned int add_index = snapshot.handle_count;
						snapshot.handles[add_index] = current_handle;
						snapshot.reference_counts[add_index] = current_reference_count;
						snapshot.reference_counts_change[add_index] = current_reference_count;
						snapshot.handle_types[add_index] = current_type;
						snapshot.handle_count++;
					}
				}
			}
		}

		// Now check for asset handles that were previously in the snapshot but they are no longer
		size_t was_found_offset = 0;
		size_t missing_index = SearchBytes(was_found + was_found_offset, was_found_size, false);
		while (missing_index != -1) {
			size_t final_index = missing_index + was_found_offset;
			// Decrement all the reference counts from this sandbox
			unsigned int reference_count = snapshot.reference_counts[final_index];
			DecrementAssetReference(
				editor_state,
				snapshot.handles[final_index],
				snapshot.handle_types[final_index],
				sandbox_index,
				reference_count
			);
			// Take into account the existing delta change as well
			if (snapshot.reference_counts_change[final_index] > 0) {
				// We need to decrement these increments
				DecrementAssetReference(
					editor_state,
					snapshot.handles[final_index],
					snapshot.handle_types[final_index],
					sandbox_index,
					snapshot.reference_counts_change[final_index]
				);
			}
			else if (snapshot.reference_counts_change[final_index] < 0) {
				IncrementAssetReferenceInSandbox(
					editor_state, 
					snapshot.handles[final_index], 
					snapshot.handle_types[final_index], 
					sandbox_index, 
					(unsigned int)-snapshot.reference_counts_change[final_index]
				);
			}

			SoARemoveSwapBack(snapshot.handle_count, final_index, snapshot.handles, snapshot.reference_counts, snapshot.reference_counts_change, snapshot.handle_types);

			// Also, swap back the current entry for was found to keep them in sync
			was_found[final_index] = was_found[snapshot.handle_count];
			was_found_offset += missing_index;
			was_found_size -= missing_index + 1;

			missing_index = SearchBytes(was_found + was_found_offset, was_found_size, false);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

// Determines all the runtime GPU resources which are needed for the simulation. These do not include UI resources, only runtime resources
// and other GPU resources needed for sandboxes.
static Stream<void*> GetRuntimeGPUResources(const EditorState* editor_state, AllocatorPolymorphic allocator) {
	// Choose a default size, in order to avoid many small allocations
	ResizableStream<void*> resources;
	resources.Initialize(allocator, 128);

	// Start by protecting the render and depth textures for the runtime/scene
	SandboxAction(editor_state, -1, [editor_state, allocator, &resources](unsigned int sandbox_index) {
		// Don't include the debug drawer for the sandbox, since it will add many resources which will slow down
		// The protection/unprotection process, with a very small likelihood of actually being affected
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		resources.Add(sandbox->scene_viewport_instance_framebuffer.Interface());
		resources.Add(sandbox->scene_viewport_depth_stencil_framebuffer.Interface());
		for (size_t index = 0; index < EDITOR_SANDBOX_VIEWPORT_COUNT; index++) {
			resources.Add(sandbox->viewport_render_destination[index].render_view.Interface());
			resources.Add(sandbox->viewport_render_destination[index].depth_view.Interface());
			resources.Add(sandbox->viewport_render_destination[index].output_view.Interface());
			void* ua_view = sandbox->viewport_render_destination[index].render_ua_view.Interface();
			if (ua_view != nullptr) {
				resources.Add(ua_view);
			}
		}
	});

	// Add all the assets from the database - we want to preserve all of them.
	editor_state->asset_database->GetGPUResources(&resources);

	return resources;
}

// A helper that takes care of the protecting and unprotecting of GPU resources. You must specify the allocator
// And the boolean flag which tells whether or not it should protect the resources.
#define SCOPE_PROTECT_GPU_RESOURCES(allocator, protect_resources) \
Stream<void*> __gpu_resources; \
auto scope_unprotect_resources = StackScope([editor_state, &__gpu_resources]() { \
	if (__gpu_resources.size > 0) { \
		editor_state->RuntimeGraphics()->UnprotectResources(__gpu_resources); \
	} \
}); \
if (protect_resources) { \
	__gpu_resources = GetRuntimeGPUResources(editor_state, allocator);  \
	editor_state->RuntimeGraphics()->ProtectResources(__gpu_resources); \
}

// -------------------------------------------------------------------------------------------------------------

void AddSandboxDebugDrawComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, ECS_COMPONENT_TYPE component_type)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->enabled_debug_draw.Add({ component, component_type });
	// Clear the crashed flag for this component
	ClearModuleDebugDrawComponentCrashStatus(editor_state, sandbox_index, { component, component_type }, true);
}

// -------------------------------------------------------------------------------------------------------------

bool AreAllDefaultSandboxesRunning(const EditorState* editor_state) {
	// Exclude temporary sandboxes
	return !SandboxAction<true>(editor_state, -1, [editor_state](unsigned int index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, index);
		if (sandbox->should_play && sandbox->run_state != EDITOR_SANDBOX_RUNNING) {
			return true;
		}
		return false;
	}, true);
}

// -------------------------------------------------------------------------------------------------------------

bool AreAllDefaultSandboxesPaused(const EditorState* editor_state) {
	// Exclude temporary sandboxes
	return !SandboxAction<true>(editor_state, -1, [=](unsigned int index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, index);
		if (sandbox->should_pause && sandbox->run_state != EDITOR_SANDBOX_PAUSED) {
			return true;
		}
		return false;
	}, true);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool AreAllDefaultSandboxesNotStarted(const EditorState* editor_state)
{
	// Exclude temporary sandboxes
	return !SandboxAction<true>(editor_state, -1, [editor_state](unsigned int index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, index);
		if (sandbox->should_play && sandbox->run_state != EDITOR_SANDBOX_SCENE) {
			return true;
		}
		return false;
	}, true);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool AreSandboxesBeingRun(const EditorState* editor_state)
{
	if (CanSandboxesRun(editor_state)) {
		// Exclude temporary sandboxes
		return SandboxAction<true>(editor_state, -1, [editor_state](unsigned int sandbox_index) {
			const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			return sandbox->run_state == EDITOR_SANDBOX_RUNNING;
		}, true);
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool AreSandboxRuntimeTasksInitialized(const EditorState* editor_state, unsigned int sandbox_index)
{
	return HasFlag(GetSandbox(editor_state, sandbox_index)->flags, EDITOR_SANDBOX_FLAG_WORLD_INITIALIZED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void BindSandboxGraphicsSceneInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SystemManager* system_manager = sandbox->sandbox_world.system_manager;

	SetEditorRuntimeType(system_manager, viewport == EDITOR_SANDBOX_VIEWPORT_SCENE ? ECS_EDITOR_RUNTIME_SCENE : ECS_EDITOR_RUNTIME_GAME);

	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		SetSandboxCameraAspectRatio(editor_state, sandbox_index, viewport);
		Camera camera = GetSandboxCamera(editor_state, sandbox_index, viewport);
		SetRuntimeCamera(system_manager, &camera, true);

		SetEditorRuntimeSelectColor(system_manager, EDITOR_SELECT_COLOR);
		size_t selected_count = sandbox->selected_entities.size;
		CapacityStream<Entity> filtered_selected_entities;
		filtered_selected_entities.Initialize(editor_state->editor_allocator, 0, selected_count);
		GetSandboxSelectedEntitiesFiltered(editor_state, sandbox_index, &filtered_selected_entities);
		SetEditorRuntimeSelectedEntities(system_manager, filtered_selected_entities);

		ECS_STACK_CAPACITY_STREAM(TransformGizmo, transform_gizmos, ECS_KB);
		GetSandboxSelectedVirtualEntitiesTransformGizmos(editor_state, sandbox_index, &transform_gizmos);
		// Inject these controls into the system manager
		SetEditorExtraTransformGizmos(system_manager, transform_gizmos);

		if (selected_count > 0) {
			editor_state->editor_allocator->Deallocate(filtered_selected_entities.buffer);
		}
		
		ECSTransformToolEx transform_tool;
		transform_tool.tool = sandbox->transform_tool;
		transform_tool.display_axes = sandbox->transform_display_axes;
		if (transform_tool.display_axes) {
			transform_tool.space = sandbox->transform_keyboard_space;
		}
		else {
			transform_tool.space = sandbox->transform_space;
		}
		memcpy(transform_tool.is_selected, sandbox->transform_tool_selected, sizeof(sandbox->transform_tool_selected));
		bool entity_ids_are_valid = true;
		// If one entity id is invalid, all should be invalid since these are created and released all at once
		for (size_t index = 0; index < 3; index++) {
			transform_tool.entity_ids[index] = FindSandboxVirtualEntitySlot(
				editor_state, 
				sandbox_index, 
				(EDITOR_SANDBOX_ENTITY_SLOT)(EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_X + index)
			);
			if (!transform_tool.entity_ids[index].IsValid()) {
				entity_ids_are_valid = false;
			}
		}

		// Check to see if we need to recompute the virtual slots
		if (ShouldSandboxRecomputeVirtualEntitySlots(editor_state, sandbox_index) || !entity_ids_are_valid) {
			unsigned int slot_write_index = GetSandboxVirtualEntitySlots(
				editor_state, 
				sandbox_index, 
				{ transform_tool.entity_ids, std::size(transform_tool.entity_ids) }
			);
			for (unsigned int index = 0; index < std::size(transform_tool.entity_ids); index++) {
				EditorSandboxEntitySlot slot;
				slot.slot_type = (EDITOR_SANDBOX_ENTITY_SLOT)(EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_X + index);
				SetSandboxVirtualEntitySlotType(
					editor_state, 
					sandbox_index, 
					slot_write_index + index, 
					slot
				);
			}
		}

		SetEditorRuntimeTransformToolEx(system_manager, transform_tool);
		GraphicsBoundViews instanced_views;
		instanced_views.depth_stencil = sandbox->scene_viewport_depth_stencil_framebuffer;
		instanced_views.target = sandbox->scene_viewport_instance_framebuffer;
		SetEditorRuntimeInstancedFramebuffer(system_manager, &instanced_views);
	}
	else {
		// Set the camera aspect ratio for the camera component, if there is one
		EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
		CameraComponent* camera_component = (CameraComponent*)entity_manager->TryGetGlobalComponent<CameraComponent>();
		if (camera_component) {
			camera_component->value.aspect_ratio = GetSandboxViewportAspectRatio(editor_state, sandbox_index, viewport);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool CanSandboxesRun(const EditorState* editor_state) {
	return !EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) && !EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
}

// -----------------------------------------------------------------------------------------------------------------------------

void CallSandboxRuntimeCleanupFunctions(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	// Perform this only if the task scheduler element count is greater than 0
	// When a module fails to compile during runtime and then is fixed, this function
	// Will be called once again and the transfer data is lost since the element count is 0
	if (sandbox->sandbox_world.task_scheduler->elements.size > 0) {
		ECS_STACK_CAPACITY_STREAM(TaskSchedulerTransferStaticData, transfer_data, ECS_KB * 2);
		sandbox->sandbox_world_transfer_data_allocator.Clear();
		sandbox->sandbox_world.task_scheduler->CallCleanupFunctions(
			&sandbox->sandbox_world,
			&sandbox->sandbox_world_transfer_data_allocator,
			&transfer_data,
			true
		);
		if (transfer_data.size > 0) {
			// Allocate them from the same allocator
			sandbox->sandbox_world_transfer_data = transfer_data.Copy(&sandbox->sandbox_world_transfer_data_allocator);
		}
		else {
			sandbox->sandbox_world_transfer_data = {};
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearSandboxRuntimeTransferData(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->sandbox_world_transfer_data_allocator.Clear();
	sandbox->sandbox_world_transfer_data = {};
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

		const Reflection::ReflectionManager* reflection = editor_state->ModuleReflectionManager();
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

void ChangeSandboxDebugDrawComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component old_component, 
	Component new_component, 
	ECS_COMPONENT_TYPE type
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	unsigned int index = sandbox->enabled_debug_draw.Find(ComponentWithType{ old_component, type });
	if (index != -1) {
		sandbox->enabled_debug_draw[index].component = new_component;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearSandboxRuntimeWorldInfo(EditorState* editor_state, unsigned int sandbox_index)
{
	// Before resetting the task scheduler, we need to call the cleanup
	// Task functions and register any transfer data
	CallSandboxRuntimeCleanupFunctions(editor_state, sandbox_index);
	
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->sandbox_world.task_scheduler->Reset();
	sandbox->sandbox_world.task_manager->Reset();
	// We also need to reset the entity manager query cache
	sandbox->sandbox_world.entity_manager->ClearCache();
	sandbox->sandbox_world.system_manager->ClearSystemSettings();
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearSandboxDebugDrawComponents(EditorState* editor_state, unsigned int sandbox_index) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->enabled_debug_draw.Clear();
}

// -----------------------------------------------------------------------------------------------------------------------------

// This function will copy any sandbox related settings that can be copied, like the simulation speed
// Factor, the should_play flag, the enabled debug draw components and others
static void CopySandboxGeneralFields(EditorState* editor_state, unsigned int destination_index, unsigned int source_index) {
	EditorSandbox* destination_sandbox = GetSandbox(editor_state, destination_index);
	const EditorSandbox* source_sandbox = GetSandbox(editor_state, source_index);

	bool disable_sandbox_file = DisableEditorSandboxFileSave(editor_state);

	destination_sandbox->should_play = source_sandbox->should_play;
	destination_sandbox->should_pause = source_sandbox->should_pause;
	destination_sandbox->should_step = source_sandbox->should_step;
	destination_sandbox->simulation_speed_up_factor = source_sandbox->simulation_speed_up_factor;

	// Copy the camera positions as well
	memcpy(destination_sandbox->camera_parameters, source_sandbox->camera_parameters, sizeof(destination_sandbox->camera_parameters));
	memcpy(destination_sandbox->camera_saved_orientations, source_sandbox->camera_saved_orientations, sizeof(destination_sandbox->camera_saved_orientations));
	destination_sandbox->camera_wasd_speed = source_sandbox->camera_wasd_speed;

	// Copy the flags as well
	destination_sandbox->flags = source_sandbox->flags;
	
	// The statistics related information must be copied
	ChangeSandboxCPUStatisticsType(editor_state, destination_index, source_sandbox->cpu_statistics_type);
	ChangeSandboxGPUStatisticsType(editor_state, destination_index, source_sandbox->gpu_statistics_type);
	destination_sandbox->statistics_display = source_sandbox->statistics_display;
	
	ClearSandboxDebugDrawComponents(editor_state, destination_index);

	RestoreAndSaveEditorSandboxFile(editor_state, disable_sandbox_file);
}

static void CopySandboxRecordingAndReplayOptions(EditorState* editor_state, unsigned int destination_index, unsigned int source_index) {
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		EDITOR_SANDBOX_RECORDING_TYPE recording_type = (EDITOR_SANDBOX_RECORDING_TYPE)index;

		const EditorSandbox::Recorder* source_sandbox_recorder = GetSandboxRecordingInfo(editor_state, source_index, recording_type).recorder;
		EditorSandbox::Recorder* destination_sandbox_recorder = GetSandboxRecordingInfo(editor_state, destination_index, recording_type).recorder;
		destination_sandbox_recorder->file.CopyOther(source_sandbox_recorder->file);
		destination_sandbox_recorder->file_automatic_index = source_sandbox_recorder->file_automatic_index;
		destination_sandbox_recorder->is_file_valid = source_sandbox_recorder->is_file_valid;

		const EditorSandbox::ReplayPlayer* source_sandbox_replay = GetSandboxReplayInfo(editor_state, source_index, recording_type).replay;
		EditorSandbox::ReplayPlayer* destination_sandbox_replay = GetSandboxReplayInfo(editor_state, destination_index, recording_type).replay;
		destination_sandbox_replay->file.CopyOther(source_sandbox_replay->file);
		destination_sandbox_replay->is_file_valid = source_sandbox_replay->is_file_valid;
		destination_sandbox_replay->is_driving_delta_time = source_sandbox_replay->is_driving_delta_time;
	}
}

void CopySandbox(EditorState* editor_state, unsigned int destination_index, unsigned int source_index, bool copy_runtime_world)
{
	// First, the modules must be changed, while also copying their assigned settings
	// Then the scene must be set, alongside the runtime settings
	// At last, any scene related parameters must be copied as well, like the should_play
	// Or the speed up factor

	bool disable_sandbox_file = DisableEditorSandboxFileSave(editor_state);

	ClearSandboxModulesInUse(editor_state, destination_index);
	const EditorSandbox* source_sandbox = GetSandbox(editor_state, source_index);

	unsigned int source_module_count = source_sandbox->modules_in_use.size;
	for (unsigned int index = 0; index < source_module_count; index++) {
		const EditorSandboxModule& sandbox_module = source_sandbox->modules_in_use[index];
		AddSandboxModule(
			editor_state, 
			destination_index, 
			sandbox_module.module_index,
			sandbox_module.module_configuration
		);

		// This works even for the case where there are no assigned settings in the source
		ChangeSandboxModuleSettings(editor_state, destination_index, sandbox_module.module_index, sandbox_module.settings_name);
	}

	// The runtime settings must be loaded before the sandbox scene path is changed,
	// Otherwise the loaded data will be lost
	bool runtime_settings_success = ChangeSandboxRuntimeSettings(editor_state, destination_index, source_sandbox->runtime_settings);
	if (!runtime_settings_success) {
		ECS_FORMAT_TEMP_STRING(message, "Could not load runtime settings {#} during sandbox copy", source_sandbox->runtime_settings);
		EditorSetConsoleError(message);
	}

	// This version works as well, but it is slower since it needs to read the file and perform a full deserialization
	/*bool scene_success = ChangeSandboxScenePath(editor_state, destination_index, source_sandbox->scene_path);
	if (!scene_success) {
		ECS_FORMAT_TEMP_STRING(message, "Could not load scene {#} during sandbox copy", source_sandbox->scene_path);
		EditorSetConsoleError(message);
	}*/
	CopySandboxScenePath(editor_state, source_index, destination_index);

	// Copy the recorders and replayers
	CopySandboxRecordingAndReplayOptions(editor_state, destination_index, source_index);

	CopySandboxGeneralFields(editor_state, destination_index, source_index);

	// If the sandbox is currently in a runtime mode, copy the runtime world as well
	if (copy_runtime_world && GetSandboxState(editor_state, destination_index) != EDITOR_SANDBOX_SCENE) {
		CopySandboxRuntimeWorldFromOther(editor_state, source_index, destination_index);
	}

	// Render the sandbox now again
	RenderSandboxViewports(editor_state, destination_index);

	RestoreAndSaveEditorSandboxFile(editor_state, disable_sandbox_file);
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateSandbox(EditorState* editor_state, bool initialize_runtime) {
	unsigned int sandbox_index = editor_state->sandboxes.ReserveRange();
	ECS_ASSERT(sandbox_index <= EDITOR_MAX_SANDBOX_COUNT, "The maximum number of sandboxes was reached!");

	EditorSandbox* sandbox = editor_state->sandboxes.buffer + sandbox_index;
	// Zero out the memory since most fields require zero initialization
	memset(sandbox, 0, sizeof(*sandbox));

	sandbox->should_pause = true;
	sandbox->should_play = true;
	sandbox->should_step = true;
	sandbox->is_scene_dirty = false;
	sandbox->is_crashed = false;
	sandbox->transform_tool = ECS_TRANSFORM_TRANSLATION;
	sandbox->transform_space = ECS_TRANSFORM_LOCAL_SPACE;
	sandbox->transform_keyboard_space = ECS_TRANSFORM_LOCAL_SPACE;
	sandbox->transform_keyboard_tool = ECS_TRANSFORM_COUNT;
	sandbox->is_camera_wasd_movement = false;
	sandbox->is_camera_right_click_movement = false;
	sandbox->background_component_build_functions.store(0, ECS_RELAXED);
	sandbox->locked_components_lock.Unlock();
	sandbox->camera_wasd_speed = EDITOR_SANDBOX_CAMERA_WASD_DEFAULT_SPEED;
	memset(sandbox->transform_tool_selected, 0, sizeof(sandbox->transform_tool_selected));

	sandbox->simulation_speed_up_factor = 1.0f;
	sandbox->run_state = EDITOR_SANDBOX_SCENE;
	sandbox->locked_count = 0;
	sandbox->flags = 0;
	sandbox->before_play_active_window_in_border = {};

	// Initialize the runtime settings string
	sandbox->runtime_descriptor = GetDefaultWorldDescriptor();

	sandbox->runtime_settings.Initialize(editor_state->EditorAllocator(), 0, RUNTIME_SETTINGS_STRING_CAPACITY);
	sandbox->runtime_settings_last_write = 0;

	sandbox->scene_path.Initialize(editor_state->EditorAllocator(), 0, SCENE_PATH_STRING_CAPACITY);

	// The runtime snapshots and their allocator are being created in the preinitialize phase

	ResetSandboxCameras(editor_state, sandbox_index);
	ResetSandboxStatistics(editor_state, sandbox_index);

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

	RegisterInspectorSandboxCreation(editor_state);
	return sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateSandboxTemporary(EditorState* editor_state, bool initialize_runtime)
{
	// We need to increment the temporary count before disabling the statistics display
	// Such that the sandbox will be recognized as a temporary sandbox
	editor_state->sandboxes_temporary_count++;

	unsigned int sandbox_index = CreateSandbox(editor_state, initialize_runtime);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	// Disable profiling and automatic play just in case
	DisableSandboxStatisticsDisplay(editor_state, sandbox_index);
	sandbox->should_pause = false;
	sandbox->should_play = false;
	sandbox->should_step = false;
	return sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool ConstructSandboxSchedulingOrder(EditorState* editor_state, unsigned int sandbox_index, bool scene_order, const ConstructSandboxSchedulingOrderOptions& options)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, sandbox->modules_in_use.size);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated) {
			indices.Add(sandbox->modules_in_use[index].module_index);
		}
	}
	return ConstructSandboxSchedulingOrder(editor_state, sandbox_index, indices, scene_order, options);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool ConstructSandboxSchedulingOrder(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<unsigned int> module_indices,
	bool scene_order,
	const ConstructSandboxSchedulingOrderOptions& options
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	for (size_t index = 0; index < module_indices.size; index++) {
		const EditorModuleInfo* current_info = GetSandboxModuleInfo(editor_state, sandbox_index, module_indices[index]);

		if (current_info->load_status == EDITOR_MODULE_LOAD_FAILED) {
			if (!options.disable_error_messages) {
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
	if (scene_order) {
		// We need to remove the scene order in case we are in scene mode - since those
		// Tasks might need other tasks that are not yet loaded - since these probably
		// Are to be used during runtime visualization
		if (GetSandboxState(editor_state, sandbox_index) == EDITOR_SANDBOX_SCENE) {
			scene_order = false;
		}
	}
	for (unsigned int index = 0; index < editor_state->project_modules->size; index++) {
		unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, index);
		if (in_stream_index != -1) {
			const EditorModuleInfo* current_info = GetSandboxModuleInfo(editor_state, sandbox_index, in_stream_index);
			if (current_info->load_status != EDITOR_MODULE_LOAD_FAILED && current_info->ecs_module.debug_draw_task_elements.size > 0) {
				AddModuleDebugDrawTaskElementsToScheduler(sandbox->sandbox_world.task_scheduler, current_info->ecs_module.debug_draw_task_elements, scene_order);
			}
		}
	}

	if (options.include_ecs_runtime_systems) {
		// Add the runtime helper ECS systems as well
		RegisterECSRuntimeSystemsOptions ecs_runtime_systems_options;
		ecs_runtime_systems_options.reflection_manager = editor_state->GlobalReflectionManager();
		ecs_runtime_systems_options.project_path = editor_state->project_file->path;
		RegisterECSRuntimeSystems(sandbox->sandbox_world.task_scheduler, ecs_runtime_systems_options);
	}

	// Now try to solve the graph
	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	bool success = sandbox->sandbox_world.task_scheduler->Solve(options.disable_error_messages ? nullptr : &error_message);
	if (!success && error_message.size > 0) {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to solve scheduling order for sandbox {#}. Reason: {#}", sandbox_index, error_message);
		EditorSetConsoleError(console_message);
	}
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int DuplicateSandbox(EditorState* editor_state, unsigned int sandbox_index) {
	// Create a new sandbox - detect if it is a temporary sandbox and create another temporary accordingly
	unsigned int duplicated_sandbox_index = -1;
	if (IsSandboxTemporary(editor_state, sandbox_index)) {
		duplicated_sandbox_index = CreateSandboxTemporary(editor_state);
	}
	else {
		duplicated_sandbox_index = CreateSandbox(editor_state);
	}

	// Copy the sandbox - without the runtime state. That will be copied separately
	CopySandbox(editor_state, duplicated_sandbox_index, sandbox_index, false);

	// If the sandbox is in the run state, we need to start this sandbox as well - before
	// Copying the actual component data, in order to warm up all the necessary structures
	EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
	if (sandbox_state == EDITOR_SANDBOX_RUNNING) {
		StartSandboxWorld(editor_state, duplicated_sandbox_index);
	}
	else if (sandbox_state == EDITOR_SANDBOX_PAUSED) {
		StartSandboxWorld(editor_state, duplicated_sandbox_index);
		PauseSandboxWorld(editor_state, duplicated_sandbox_index, false);
	}

	// Finally, copy the runtime data
	if (sandbox_state == EDITOR_SANDBOX_RUNNING || sandbox_state == EDITOR_SANDBOX_PAUSED) {
		CopySandboxRuntimeWorldFromOther(editor_state, sandbox_index, duplicated_sandbox_index);
	}

	return duplicated_sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

void DecrementSandboxModuleComponentBuildCount(EditorState* editor_state, unsigned int sandbox_index)
{
	GetSandbox(editor_state, sandbox_index)->background_component_build_functions.fetch_sub(1, ECS_RELAXED);
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

static void DestroySandboxImpl(EditorState* editor_state, unsigned int sandbox_index) {
	// This must be performed before the scene is destroyed, otherwise buffers would try
	// To be deallocated, but the world is already destroyed
	RegisterInspectorSandboxDestroy(editor_state, sandbox_index);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	// Unload the sandbox assets
	UnloadSandboxAssets(editor_state, sandbox_index);

	// Finish any recordings that are still valid
	FinishSandboxRecordings(editor_state, sandbox_index);
	DeallocateSandboxReplays(editor_state, sandbox_index);

	// We also need to remove the prefab component references

	// We can also clear the modules in use - they use allocations from the global sandbox
	// And that will get cleared at the end. But since they might do something unrelated
	// To allocations in the future, let's call it
	ClearSandboxModulesInUse(editor_state, sandbox_index);

	// Before destroying the sandbox runtime we need to terminate the threads
	sandbox->sandbox_world.task_manager->TerminateThreads(true);

	// Destroy the sandbox runtime
	DestroySandboxRuntime(editor_state, sandbox_index);

	FreeSandboxRenderTextures(editor_state, sandbox_index);

	// Free the global sandbox allocator
	sandbox->GlobalMemoryManager()->Free();

	// Release the runtime settings path and the scene as well - it was allocated from the editor allocator
	editor_state->editor_allocator->Deallocate(sandbox->runtime_settings.buffer);
	sandbox->scene_path.Deallocate(editor_state->EditorAllocator());

	// Check to see if it belongs to the temporary sandboxes
	if (GetSandboxCount(editor_state, true) <= sandbox_index) {
		editor_state->sandboxes_temporary_count--;
	}
	editor_state->sandboxes.RemoveSwapBack(sandbox_index);
	unsigned int swapped_index = editor_state->sandboxes.size;

	// Notify the UI
	if (editor_state->sandboxes.size > 0) {
		// Destroy the associated windows first
		DestroySandboxWindows(editor_state, sandbox_index);
		UpdateGameUIWindowIndex(editor_state, swapped_index, sandbox_index);
		UpdateSceneUIWindowIndex(editor_state, swapped_index, sandbox_index);
		UpdateEntitiesUITargetSandbox(editor_state, swapped_index, sandbox_index);
		UpdateVisualizeTextureSandboxReferences(editor_state, swapped_index, sandbox_index);

		if (sandbox_index < editor_state->sandboxes.size) {
			// The entity manager auto generator data must be updated, only if the index is still valid
			ComponentFunctionsAutoGeneratorData* auto_generator_data = (ComponentFunctionsAutoGeneratorData*)editor_state->sandboxes[sandbox_index].entity_manager_auto_generated_data;
			auto_generator_data->sandbox_index = sandbox_index;

			// Change the thread debugging names as well for that sandbox' task manager
			ECS_FORMAT_TEMP_STRING(new_sandbox_thread_name, "Sandbox {#}", sandbox_index);
			GetSandbox(editor_state, sandbox_index)->sandbox_world.task_manager->SetDebuggingNames(new_sandbox_thread_name);
		}
	}
}

// In order to have the correct sandbox be destroyed even when multiple of these
// Are issued, use the sandbox allocator as an identifier - it is allocated from
// The editor allocator and is stable even when RemoveSwapBack are happening
struct DestroySandboxDeferredData {
	GlobalMemoryManager* sandbox_allocator;
};

EDITOR_EVENT(DestroySandboxDeferred) {
	DestroySandboxDeferredData* data = (DestroySandboxDeferredData*)_data;

	unsigned int index = -1;
	ECS_ASSERT(SandboxAction<true>(editor_state, -1, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->GlobalMemoryManager() == data->sandbox_allocator) {
			index = sandbox_index;
			return true;
		}
		return false;
	}));

	if (!IsSandboxLocked(editor_state, index)) {
		unsigned int sandbox_count = GetSandboxCount(editor_state);
		if (sandbox_count - 1 != index) {
			// Check to see that the last sandbox is unlocked
			if (IsSandboxLocked(editor_state, sandbox_count - 1)) {
				// We need to wait
				return true;
			}
		}
		DestroySandboxImpl(editor_state, index);
		return false;
	}
	return true;
}

void DestroySandbox(EditorState* editor_state, unsigned int sandbox_index, bool wait_unlocking, bool deferred_waiting) {
	auto register_deferred_destruction = [&]() {
		DestroySandboxDeferredData deferred_data;
		deferred_data.sandbox_allocator = GetSandbox(editor_state, sandbox_index)->GlobalMemoryManager();
		EditorAddEvent(editor_state, DestroySandboxDeferred, &deferred_data, sizeof(deferred_data));
	};

	if (wait_unlocking) {
		WaitSandboxUnlock(editor_state, sandbox_index);
	}
	else {
		if (!deferred_waiting) {
			ECS_ASSERT(!IsSandboxLocked(editor_state, sandbox_index));
		}
		else {
			if (IsSandboxLocked(editor_state, sandbox_index)) {
				// We need a deferred event to unlock
				register_deferred_destruction();
				return;
			}
		}
	}

	// There is another case here - if the last sandbox that is to be swapped
	// Is locked, we can't swap it since there might some references to it
	// So, we need to use deferred destruction here
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	if (sandbox_index != sandbox_count - 1) {
		if (IsSandboxLocked(editor_state, sandbox_count - 1)) {
			// Deferred destruction
			register_deferred_destruction();
			return;
		}
	}
	DestroySandboxImpl(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DisableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport] = false;
}

// ------------------------------------------------------------------------------------------------------------------------------

void DisableSandboxDebugDrawAll(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->enabled_debug_draw.FreeBuffer();
}

// -----------------------------------------------------------------------------------------------------------------------------

void DrawSandboxDebugDrawComponents(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (sandbox->enabled_debug_draw.size > 0) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		CapacityStream<ComponentWithType> valid_debug_draws;
		valid_debug_draws.Initialize(&stack_allocator, 0, sandbox->enabled_debug_draw.size);
		GetSandboxDisplayDebugDrawComponents(editor_state, sandbox_index, &valid_debug_draws);

		ModuleDebugDrawElementTyped* debug_elements = (ModuleDebugDrawElementTyped*)stack_allocator.Allocate(sizeof(ModuleDebugDrawElementTyped) * valid_debug_draws.size);
		for (unsigned int index = 0; index < valid_debug_draws.size; index++) {
			debug_elements[index].base.draw_function = nullptr;
		}

		// Match all the entries before that
		for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
			GetModuleMatchedDebugDrawComponents(
				editor_state, 
				sandbox->modules_in_use[index].module_index, 
				sandbox->modules_in_use[index].module_configuration, 
				valid_debug_draws,
				debug_elements
			);
		}

		// Also take into account the ecs debug draws
		ModuleMatchDebugDrawElements(editor_state, valid_debug_draws, editor_state->ecs_component_functions, debug_elements);

		EditorSetMainThreadCrashHandlerIntercept();

		DebugDrawer* debug_drawer = sandbox->sandbox_world.debug_drawer;
		const EntityManager* active_entity_manager = ActiveEntityManager(editor_state, sandbox_index);

		for (unsigned int index = 0; index < valid_debug_draws.size; index++) {
			__try {
				// If for some reason it couldn't be matched, skip it
				ModuleDebugDrawComponentFunction draw_function = debug_elements[index].base.draw_function;
				if (draw_function != nullptr) {
					ECS_COMPONENT_TYPE type = debug_elements[index].type.type;
					Stream<ComponentWithType> dependencies = debug_elements[index].base.Dependencies();

					auto draw_with_dependencies = [&]() {
						// Use signature based
						Component unique_optional_components[ECS_ARCHETYPE_MAX_COMPONENTS];
						Component shared_optional_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
						ArchetypeQueryDescriptor query_descriptor;
						if (type == ECS_COMPONENT_UNIQUE) {
							query_descriptor.unique = ComponentSignature{ &debug_elements[index].type.component, 1 };
							query_descriptor.shared = ComponentSignature();
						}
						else {
							query_descriptor.shared = ComponentSignature{ &debug_elements[index].type.component, 1 };
							query_descriptor.unique = ComponentSignature();
						}

						query_descriptor.unique_optional = { unique_optional_components, 0 };
						query_descriptor.shared_optional = { shared_optional_components, 0 };
						for (size_t subindex = 0; subindex < dependencies.size; subindex++) {
							if (dependencies[subindex].type == ECS_COMPONENT_SHARED) {
								query_descriptor.shared_optional[query_descriptor.shared_optional.count++] = dependencies[subindex].component;
							}
							else {
								query_descriptor.unique_optional[query_descriptor.unique_optional.count++] = dependencies[subindex].component;
							}
						}

						ForEachEntityCommitFunctor(&sandbox->sandbox_world, query_descriptor, [&](ForEachEntityUntypedFunctorData* for_each_data) {
							ModuleDebugDrawComponentFunctionData draw_data;
							draw_data.thread_id = 0;
							draw_data.debug_drawer = debug_drawer;

							unsigned int unique_index = 0;
							unsigned int shared_index = 0;
							if (type == ECS_COMPONENT_UNIQUE) {
								draw_data.component = for_each_data->unique_components[0];
								unique_index++;
							}
							else {
								draw_data.component = for_each_data->shared_components[0];
								shared_index++;
							}

							ECS_STACK_CAPACITY_STREAM(void*, dependency_components, ECS_ARCHETYPE_MAX_COMPONENTS);
							draw_data.dependency_components = (const void**)dependency_components.buffer;
							for (size_t index = 0; index < dependencies.size; index++) {
								draw_data.dependency_components[index] = dependencies[index].type == ECS_COMPONENT_UNIQUE ?
									for_each_data->unique_components[unique_index++] : for_each_data->shared_components[shared_index++];
							}
							draw_function(&draw_data);
							});
					};

					if (type == ECS_COMPONENT_UNIQUE) {
						if (dependencies.size == 0) {
							active_entity_manager->ForEachEntityComponent(debug_elements[index].type.component, [debug_drawer, draw_function](Entity entity, const void* data) {
								// At the moment just give the thread_id 0 when running in a single thread
								ModuleDebugDrawComponentFunctionData draw_data;
								draw_data.debug_drawer = debug_drawer;
								draw_data.component = data;
								draw_data.dependency_components = nullptr;
								draw_data.thread_id = 0;
								draw_function(&draw_data);
								});
						}
						else {
							draw_with_dependencies();
						}
					}
					else if (type == ECS_COMPONENT_SHARED) {
						if (dependencies.size == 0) {
							active_entity_manager->ForEachSharedInstance(debug_elements[index].type.component,
								[=](SharedInstance instance) {
									// At the moment just give the thread_id 0 when running in a single thread
									ModuleDebugDrawComponentFunctionData draw_data;
									draw_data.debug_drawer = debug_drawer;
									draw_data.component = active_entity_manager->GetSharedData(debug_elements[index].type.component, instance);
									draw_data.dependency_components = nullptr;
									draw_data.thread_id = 0;
									draw_function(&draw_data);
								});
						}
						else {
							draw_with_dependencies();
						}
					}
					else if (type == ECS_COMPONENT_GLOBAL) {
						// At the moment just give the thread_id 0 when running in a single thread
						ModuleDebugDrawComponentFunctionData draw_data;
						draw_data.debug_drawer = debug_drawer;
						draw_data.component = active_entity_manager->GetGlobalComponent(debug_elements[index].type.component);
						draw_data.dependency_components = nullptr;
						draw_data.thread_id = 0;
						draw_function(&draw_data);
					}
					else {
						ECS_ASSERT(false, "Invalid component type");
					}
				}
			}
			__except(OS::ExceptionHandlerFilterDefault(GetExceptionInformation())) {
				ECS_FORMAT_TEMP_STRING(console_message, "Debug draw for component {#} has crashed", 
					editor_state->editor_components.ComponentFromID(debug_elements[index].type.component, debug_elements[index].type.type));
				EditorSetConsoleError(console_message);
				SetModuleDebugDrawComponentCrashStatus(editor_state, sandbox_index, debug_elements[index].type, true);
			}
		}

		EditorClearMainThreadCrashHandlerIntercept();
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void EnableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	viewport = GetSandboxViewportOverride(editor_state, sandbox_index, viewport);
	GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport] = true;
}

// ------------------------------------------------------------------------------------------------------------------------------

void EnableSandboxDebugDrawAll(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->enabled_debug_draw.FreeBuffer();
	// Retrieve all debug draws for this sandbox
	GetSandboxAllPossibleDebugDrawComponents(editor_state, sandbox_index, &sandbox->enabled_debug_draw);
	for (unsigned int index = 0; index < sandbox->enabled_debug_draw.size; index++) {
		ClearModuleDebugDrawComponentCrashStatus(editor_state, sandbox_index, sandbox->enabled_debug_draw[index], true);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

struct EndSandboxWorldSimulationEventData {
	unsigned int sandbox_index;
};

static EDITOR_EVENT(EndSandboxWorldSimulationEvent) {
	EndSandboxWorldSimulationEventData* data = (EndSandboxWorldSimulationEventData*)_data;
	if (GetSandboxBackgroundComponentBuildFunctionCount(editor_state, data->sandbox_index) == 0) {
		EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, data->sandbox_index);
		if (sandbox_state == EDITOR_SANDBOX_PAUSED || sandbox_state == EDITOR_SANDBOX_RUNNING) {
			EndSandboxWorldSimulation(editor_state, data->sandbox_index);
		}
		return false;
	}
	return true;
}

void EndSandboxWorldSimulation(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_PAUSED || sandbox->run_state == EDITOR_SANDBOX_RUNNING);

	// Check to see if we need to move this call into an event due to background
	// Component build tasks
	if (GetSandboxBackgroundComponentBuildFunctionCount(editor_state, sandbox_index) > 0) {
		// If an event already exists, then don't add another one
		ECS_STACK_CAPACITY_STREAM(void*, existing_event_data, 512);
		EditorGetEventTypeData(editor_state, EndSandboxWorldSimulationEvent, &existing_event_data);
		bool exists = false;
		for (unsigned int index = 0; index < existing_event_data.size; index++) {
			EndSandboxWorldSimulationEventData* current_data = (EndSandboxWorldSimulationEventData*)existing_event_data[index];
			if (current_data->sandbox_index == sandbox_index) {
				exists = true;
				break;
			}
		}

		if (!exists) {
			EndSandboxWorldSimulationEventData event_data = { sandbox_index };
			EditorAddEvent(editor_state, EndSandboxWorldSimulationEvent, &event_data, sizeof(event_data));
		}
		return;
	}

	// We need now to decrement/increment the reference counts for this sandbox
	// Use the snapshot to do that
	for (unsigned int index = 0; index < sandbox->runtime_asset_handle_snapshot.handle_count; index++) {
		unsigned int current_handle = sandbox->runtime_asset_handle_snapshot.handles[index];
		int reference_count = sandbox->runtime_asset_handle_snapshot.reference_counts_change[index];
		// If it is positive, then we need to decrement. If it is negative, we need to increment
		if (reference_count < 0) {
			IncrementAssetReferenceInSandbox(
				editor_state, 
				current_handle, 
				sandbox->runtime_asset_handle_snapshot.handle_types[index], 
				sandbox_index, 
				(unsigned int)-reference_count
			);
		}
		else {
			DecrementAssetReference(editor_state, current_handle, sandbox->runtime_asset_handle_snapshot.handle_types[index], sandbox_index, reference_count);
		}
	}

	// Finish the sandbox recorders
	FinishSandboxRecordings(editor_state, sandbox_index);
	// Deallocate the sandbox replays as well
	DeallocateSandboxReplays(editor_state, sandbox_index);

	if (HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_WORLD_INITIALIZED)) {
		// Call the runtime ECS helper unregister function - only if we successfully initialized the world
		UnregisterECSRuntimeSystems(&sandbox->sandbox_world);
	}

	ClearWorld(&sandbox->sandbox_world);
	sandbox->run_state = EDITOR_SANDBOX_SCENE;
	// Clear the waiting compilation flag
	sandbox->flags = ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION, EDITOR_SANDBOX_FLAG_WORLD_INITIALIZED);
	// Reset the mouse
	sandbox->sandbox_world.mouse->Reset(false);
	// Reset the keyboard as well
	sandbox->sandbox_world.keyboard->Reset();
	// Clear the crashed status as well
	sandbox->is_crashed = false;
	// We can also clear the transfer data since it is no longer needed
	// There shouldn't be any, but just in case
	ClearSandboxRuntimeTransferData(editor_state, sandbox_index);
	
	if (EnableSceneUIRendering(editor_state, sandbox_index, false)) {
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	}
	if (EnableGameUIRendering(editor_state, sandbox_index, false)) {
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	}

	// Defocus the UI as well - it provides better experience
	DefocusUIOnSandbox(editor_state, sandbox_index);
	// Notify the profilers
	EndSandboxSimulationProfiling(editor_state, sandbox_index);
}

// ------------------------------------------------------------------------------------------------------------------------------

void EndSandboxWorldSimulations(EditorState* editor_state, bool paused_only, bool running_only)
{
	// Exclude temporary sandboxes
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		bool should_stop = false;
		if (paused_only) {
			should_stop = sandbox->run_state == EDITOR_SANDBOX_PAUSED;
		}
		else if (running_only) {
			should_stop = sandbox->run_state == EDITOR_SANDBOX_RUNNING;
		}
		else {
			should_stop = sandbox->run_state == EDITOR_SANDBOX_RUNNING || sandbox->run_state == EDITOR_SANDBOX_PAUSED;
		}

		if (sandbox->should_play && should_stop) {
			EndSandboxWorldSimulation(editor_state, sandbox_index);
		}
	}, true);
}

// ------------------------------------------------------------------------------------------------------------------------------

unsigned int FindSandboxSelectedEntityIndex(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return (unsigned int)SearchBytes(sandbox->selected_entities.ToStream(), entity);
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity FindSandboxVirtualEntitySlot(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_ENTITY_SLOT slot_type
)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	unsigned int slot_index = sandbox->virtual_entity_slot_type.Find(slot_type, [](EditorSandboxEntitySlot current_slot) {
		return current_slot.slot_type;
	});
	return slot_index != -1 ? sandbox->virtual_entities_slots[slot_index] : Entity::Invalid();
}

// -----------------------------------------------------------------------------------------------------------------------------

EditorSandboxEntitySlot FindSandboxVirtualEntitySlot(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity
)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	size_t slot_index = SearchBytes(sandbox->virtual_entities_slots.ToStream(), entity);
	if (slot_index != -1) {
		ECS_ASSERT(sandbox->virtual_entity_slot_type[slot_index].slot_type != EDITOR_SANDBOX_ENTITY_SLOT_COUNT);
		return sandbox->virtual_entity_slot_type[slot_index];
	}
	EditorSandboxEntitySlot slot;
	slot.slot_type = EDITOR_SANDBOX_ENTITY_SLOT_COUNT;
	return slot;
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

	// Use bypass protection calls to free the resources, because the main loop that runs
	// The sandboxes will protect them before calling the run function, which might trigger
	// A resize, which can get to this function while the resources are protected
	if (sandbox->viewport_render_destination[viewport].render_view.Interface() != nullptr) {
		runtime_graphics->FreeRenderDestinationBypassProtection(sandbox->viewport_render_destination[viewport]);

		ECS_STACK_CAPACITY_STREAM(char, visualize_texture_name, 512);	
		if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
			runtime_graphics->FreeViewBypassProtection(sandbox->scene_viewport_instance_framebuffer);
			runtime_graphics->FreeViewBypassProtection(sandbox->scene_viewport_depth_stencil_framebuffer);

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

// -------------------------------------------------------------------------------------------------------------------------

bool ExistsSandboxDebugDrawComponentFunction(const EditorState* editor_state, unsigned int sandbox_index, Component component, ECS_COMPONENT_TYPE type)
{
	// Check the ECS first
	ComponentWithType component_with_type = { component, type };
	if (ExistsModuleDebugDrawElementIn(editor_state, editor_state->ecs_component_functions, component_with_type)) {
		return true;
	}

	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		const EditorModuleInfo* info = GetSandboxModuleInfo(editor_state, sandbox_index, index);
		if (ExistsModuleDebugDrawElementIn(editor_state, info->ecs_module.component_functions, component_with_type)) {
			return true;
		}
	}

	return false;
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
		Stream<wchar_t> stem = PathStem(path);
		data->paths->AddAssert(StringCopy(data->allocator, stem));
		return true;
	});
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxAllPossibleDebugDrawComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	AdditionStream<ComponentWithType> components
)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		GetModuleDebugDrawComponents(editor_state, sandbox->modules_in_use[index].module_index, sandbox->modules_in_use[index].module_configuration, components, false);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxDisplayDebugDrawComponents(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	AdditionStream<ComponentWithType> components
) {
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		ECS_STACK_CAPACITY_STREAM(ComponentWithType, module_components, ECS_KB);
		GetModuleDebugDrawComponents(editor_state, sandbox->modules_in_use[index].module_index, sandbox->modules_in_use[index].module_configuration, &module_components, true);
		for (unsigned int subindex = 0; subindex < module_components.size; subindex++) {
			bool is_enabled = sandbox->enabled_debug_draw.Find(module_components[subindex]) != -1;
			if (is_enabled) {
				components.Add(module_components[subindex]);
			}
		}
	}

	// At the end, add all the ECSEngine components that are enabled, these cannot crash
	for (unsigned int index = 0; index < sandbox->enabled_debug_draw.size; index++) {
		if (editor_state->editor_components.IsECSEngineComponent(sandbox->enabled_debug_draw[index].component, sandbox->enabled_debug_draw[index].type)) {
			components.Add(sandbox->enabled_debug_draw[index]);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxSelectedEntitiesFiltered(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<Entity>* filtered_entities
)
{
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	Stream<Entity> virtual_entities = GetSandboxVirtualEntitySlots(editor_state, sandbox_index);
	ECS_ASSERT(filtered_entities->capacity == selected_entities.size);

	filtered_entities->CopyOther(selected_entities);
	for (size_t index = 0; index < virtual_entities.size && selected_entities.size > 0; index++) {
		size_t found_index = SearchBytes(filtered_entities->ToStream(), virtual_entities[index]);
		if (found_index != -1) {
			filtered_entities->RemoveSwapBack(found_index);
			filtered_entities->buffer[filtered_entities->size] = virtual_entities[index];
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void GetSandboxComponentTransformGizmos(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<GlobalComponentTransformGizmos>* components)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	unsigned int module_count = sandbox->modules_in_use.size;
	for (unsigned int index = 0; index < module_count; index++) {
		ModuleExtraInformation current_extra_information = GetModuleExtraInformation(
			editor_state,
			sandbox->modules_in_use[index].module_index,
			sandbox->modules_in_use[index].module_configuration
		);

		if (current_extra_information.IsValid()) {
			GetGlobalComponentTransformGizmos(current_extra_information, components);
		}
	}

	// Now check the ECSEngine side
	GetGlobalComponentTransformGizmos(editor_state->ecs_extra_information, components);
}

// -----------------------------------------------------------------------------------------------------------------------------

template<typename Functor>
static void ForEachSandboxSelectedVirtualEntity(const EditorState* editor_state, unsigned int sandbox_index, Functor&& functor) {
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	Stream<Entity> virtual_entities = GetSandboxVirtualEntitySlots(editor_state, sandbox_index);

	for (size_t index = 0; index < virtual_entities.size && selected_entities.size > 0; index++) {
		size_t found_index = SearchBytes(selected_entities, virtual_entities[index]);
		if (found_index != -1) {
			functor(virtual_entities[index]);
		}
	}
}

void GetSandboxSelectedVirtualEntities(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<Entity>* entities)
{
	ForEachSandboxSelectedVirtualEntity(editor_state, sandbox_index, [&](Entity entity) { entities->AddAssert(entity); });
}

// -----------------------------------------------------------------------------------------------------------------------------

// The functor receives (TransformGizmoPointers pointers, Entity entity) as parameters
template<typename Functor>
static void ForEachSandboxSelectedVirtualEntitiesTransformGizmoInfo(EditorState* editor_state, unsigned int sandbox_index, Functor&& functor) {
	// Pre-determine these buffers before the main iteration
	ECS_STACK_CAPACITY_STREAM(GlobalComponentTransformGizmos, transform_gizmos, ECS_KB);
	GetSandboxComponentTransformGizmos(editor_state, sandbox_index, &transform_gizmos);

	ECS_STACK_CAPACITY_STREAM(void*, transform_gizmos_data, ECS_KB);
	GetGlobalComponentTransformGizmosData(
		ActiveEntityManager(editor_state, sandbox_index),
		editor_state->editor_components.internal_manager,
		transform_gizmos,
		transform_gizmos_data
	);

	ECS_STACK_CAPACITY_STREAM(TransformGizmoPointers, transform_pointers, ECS_KB);
	GetGlobalComponentTransformGizmoPointers(
		editor_state->editor_components.internal_manager,
		transform_gizmos,
		transform_gizmos_data,
		&transform_pointers,
		false
	);

	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);

	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM(Entity, gizmo_entity_mapping, ECS_KB);
	// Map the gizmo transforms to the virtual entities as well
	Stream<Entity> virtual_entities = GetSandboxVirtualEntitySlots(editor_state, sandbox_index);
	for (unsigned int index = 0; index < transform_gizmos.size; index++) {
		if (transform_gizmos_data[index] != nullptr) {
			Component component = editor_state->editor_components.GetComponentID(transform_gizmos[index].component);
			if (component.Valid()) {
				for (unsigned int subindex = 0; subindex < sandbox->virtual_entity_slot_type.size; subindex++) {
					if (sandbox->virtual_entity_slot_type[subindex].slot_type == EDITOR_SANDBOX_ENTITY_SLOT_VIRTUAL_GLOBAL_COMPONENTS) {
						if (component == sandbox->virtual_entity_slot_type[subindex].Data<Component>()) {
							// Check to see if it is selected
							bool is_selected = SearchBytes(selected_entities, sandbox->virtual_entities_slots[subindex]) != -1;
							if (is_selected) {
								functor(transform_pointers[index], sandbox->virtual_entities_slots[subindex]);
							}
							break;
						}
					}
				}
			}
		}
	}
}

void GetSandboxSelectedVirtualEntitiesTransformPointers(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<TransformGizmoPointers>* pointers,
	CapacityStream<Entity>* entities
)
{
	ForEachSandboxSelectedVirtualEntitiesTransformGizmoInfo(editor_state, sandbox_index, [&](TransformGizmoPointers pointers_value, Entity virtual_entity) {
		pointers->AddAssert(pointers_value);
		if (entities != nullptr) {
			entities->AddAssert(virtual_entity);
		}
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxSelectedVirtualEntitiesTransformGizmos(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<TransformGizmo>* gizmos, 
	CapacityStream<Entity>* entities
)
{
	// It is safe to cast the pointer to mutable inside here since the function does not mutate anything
	// It takes the parameter as mutable reference since it returns pointers to values inside the entities,
	// But since we don't modify them it's safe to leave it as const pointer in the signature
	ForEachSandboxSelectedVirtualEntitiesTransformGizmoInfo((EditorState*)editor_state, sandbox_index, [&](TransformGizmoPointers pointers_value, Entity virtual_entity) {
		gizmos->AddAssert(pointers_value.ToValue());
		if (entities != nullptr) {
			entities->AddAssert(virtual_entity);
		}
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetSandboxVirtualEntitySlots(EditorState* editor_state, unsigned int sandbox_index, Stream<Entity> entities)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	// We need to retrieve the unused entity slots just for a single entity manager,
	// these are valid for both
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index);
	// Limit the bit count to the available space for the instance bitness for the instanced buffer
	ECS_ASSERT(entity_manager->m_entity_pool->GetVirtualEntities(
		entities, 
		sandbox->virtual_entities_slots.ToStream(),
		32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS - 1
	));
	unsigned int write_index = sandbox->virtual_entities_slots.AddStream(entities);
	if (sandbox->virtual_entities_slots.capacity != sandbox->virtual_entity_slot_type.capacity) {
		sandbox->virtual_entity_slot_type.Resize(sandbox->virtual_entities_slots.capacity);
		for (size_t index = 0; index < entities.size; index++) {
			// Default initialize to invalid value
			sandbox->virtual_entity_slot_type[write_index + index].slot_type = EDITOR_SANDBOX_ENTITY_SLOT_COUNT;
		}
	}
	sandbox->virtual_entity_slot_type.size = sandbox->virtual_entities_slots.size;

	return write_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

void InitializeSandboxes(EditorState* editor_state)
{
	// Set the allocator for the resizable strema
	editor_state->sandboxes.Initialize(editor_state->EditorAllocator(), 0);
	editor_state->sandboxes_temporary_count = 0;
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
		sandbox->sandbox_world.entity_manager->SetAutoGenerateComponentFunctionsFunctor(ComponentFunctionsAutoGenerator, sandbox->entity_manager_auto_generated_data, 0);
		editor_state->editor_components.SetManagerComponents(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxGizmoEntity(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	return FindSandboxVirtualEntitySlot(editor_state, sandbox_index, entity).slot_type != EDITOR_SANDBOX_ENTITY_SLOT_COUNT;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxDebugDrawComponentEnabled(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	ECS_COMPONENT_TYPE component_type
)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ComponentWithType component_with_type = { component, component_type };
	return sandbox->enabled_debug_draw.Find(component_with_type) != -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAnyDefaultSandboxRunning(const EditorState* editor_state)
{
	// Exclude temporary sandboxes
	return SandboxAction<true>(editor_state, -1, [editor_state](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->should_play && sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			return true;
		}
		return false;
	}, true);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAnyDefaultSandboxPaused(const EditorState* editor_state)
{
	// Exclude temporary sandboxes
	return SandboxAction<true>(editor_state, -1, [editor_state](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->should_pause && sandbox->run_state == EDITOR_SANDBOX_PAUSED) {
			return true;
		}
		return false;
	}, true);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAnyDefaultSandboxNotStarted(const EditorState* editor_state)
{
	// Exclude temporary sandboxes
	return SandboxAction<true>(editor_state, -1, [editor_state](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->should_play && sandbox->run_state == EDITOR_SANDBOX_SCENE) {
			return true;
		}
		return false;
	}, true);
}

// -----------------------------------------------------------------------------------------------------------------------------

void IncrementSandboxModuleComponentBuildCount(EditorState* editor_state, unsigned int sandbox_index)
{
	GetSandbox(editor_state, sandbox_index)->background_component_build_functions.fetch_add(1, ECS_RELAXED);
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

void PauseSandboxWorld(EditorState* editor_state, unsigned int index, bool restore_mouse_state)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, index);
	ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_RUNNING);

	PauseWorld(&sandbox->sandbox_world);

	// Clear the sandbox waiting compilation flag - since it might be still on
	sandbox->flags = ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);
	sandbox->run_state = EDITOR_SANDBOX_PAUSED;

	// Bring back the viewport rendering since it was disabled
	EnableSceneUIRendering(editor_state, index, false);
	EnableGameUIRendering(editor_state, index, false);

	if (restore_mouse_state) {
		RestoreMouseState(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void PauseSandboxWorlds(EditorState* editor_state, bool restore_mouse_state)
{
	// Exclude temporary sandboxes
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->should_pause && sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			PauseSandboxWorld(editor_state, sandbox_index, false);
		}
	}, true);

	if (restore_mouse_state) {
		RestoreMouseState(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void PauseUnpauseSandboxWorlds(EditorState* editor_state) {
	// If the run button state is not present, consider this an erroneous call
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING)) {
		return;
	}

	bool all_are_paused = true;
	bool all_are_running = true;
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->should_pause) {
			if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
				// Do not restore the mouse, we check the flags by hand after the loop
				PauseSandboxWorld(editor_state, sandbox_index, false);
				all_are_paused = false;
			}
			else if (sandbox->run_state == EDITOR_SANDBOX_PAUSED) {
				StartSandboxWorld(editor_state, sandbox_index);
				all_are_running = false;
			}
		}
	}, true);

	// Perform one additional step: the mouse visibility handling
	if (all_are_paused) {
		if (EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN)) {
			editor_state->Mouse()->SetCursorVisibility(false);
		}
		if (EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT)) {
			editor_state->Mouse()->EnableRawInput();
		}
	}
	else {
		if (!editor_state->Mouse()->IsVisible()) {
			if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN)) {
				EditorStateSetFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN);
			}
			editor_state->Mouse()->SetCursorVisibility(true);

			if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT)) {
				EditorStateSetFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT);
			}
			editor_state->Mouse()->DisableRawInput();
		}
		else {
			if (EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN)) {
				EditorStateClearFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN);
			}
			if (EditorStateHasFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT)) {
				EditorStateClearFlag(editor_state, EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool PrepareSandboxRuntimeWorldInfo(EditorState* editor_state, unsigned int sandbox_index)
{
	bool success = ConstructSandboxSchedulingOrder(editor_state, sandbox_index, false);
	if (success) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		bool tasks_were_initialized = AreSandboxRuntimeTasksInitialized(editor_state, sandbox_index);
		// We also need to prepare the world concurrency
		// And forward the transfer data
		TaskSchedulerSetManagerOptions set_options;
		set_options.call_initialize_functions = true;
		set_options.preserve_data_flag = tasks_were_initialized;
		set_options.transfer_data = sandbox->sandbox_world_transfer_data;
		if (!tasks_were_initialized) {
			// In this case, we also need to copy the entities from the scene to the
			// Runtime entities since that did not happen
			CopySceneEntitiesIntoSandboxRuntime(editor_state, sandbox_index);
			sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_WORLD_INITIALIZED);
		}
		// We must call this after the scene entities were copied into the runtime version
		PrepareWorldConcurrency(&sandbox->sandbox_world, &set_options);
		
		// We can now clear the transfer data
		ClearSandboxRuntimeTransferData(editor_state, sandbox_index);

		SetSandboxCrashHandlerWrappers(editor_state, sandbox_index);

		// Bind again the module settings
		BindSandboxRuntimeModuleSettings(editor_state, sandbox_index);

		// We also need to bind the scene info
		BindSandboxGraphicsSceneInfo(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
		return true;
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

void PreinitializeSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	size_t profiling_reserve_size = ReserveSandboxProfilersAllocatorSize();
	// Create a sandbox allocator - a global one - such that it can accomodate the default entity manager requirements
	GlobalMemoryManager* allocator = (GlobalMemoryManager*)editor_state->editor_allocator->Allocate(sizeof(GlobalMemoryManager));

	// The debug allocator size must be multiplied by 2 to account for the redirect allocator as well
	CreateGlobalMemoryManager(
		allocator,
		sandbox->runtime_descriptor.entity_manager_memory_size + SANDBOX_ALLOCATOR_EXTRA_CAPACITY + profiling_reserve_size + 
			DebugDrawer::DefaultAllocatorSize() * 2,
		ECS_KB * 4,
		sandbox->runtime_descriptor.entity_manager_memory_new_allocation_size + SANDBOX_ALLOCATOR_BACKUP_EXTRA_CAPACITY + 
			DebugDrawer::DefaultAllocatorSize() * 2
	);

	AllocatorPolymorphic sandbox_allocator = allocator;

	// Initialize the textures to nullptr - wait for the UI window to tell the sandbox what area it covers
	memset(sandbox->viewport_render_destination, 0, sizeof(sandbox->viewport_render_destination));

	sandbox->modules_in_use.Initialize(sandbox_allocator, 0);
	sandbox->database = AssetDatabaseReference(editor_state->asset_database, sandbox_allocator);

	// Create the entity manager auto generator
	ComponentFunctionsAutoGeneratorData* component_functions_auto_generator_data = (ComponentFunctionsAutoGeneratorData*)allocator->Allocate(sizeof(ComponentFunctionsAutoGeneratorData));
	component_functions_auto_generator_data->editor_state = editor_state;
	component_functions_auto_generator_data->sandbox_index = sandbox_index;
	sandbox->entity_manager_auto_generated_data = component_functions_auto_generator_data;

	// Create a graphics object
	sandbox->runtime_descriptor.graphics = editor_state->RuntimeGraphics();
	sandbox->runtime_descriptor.resource_manager = editor_state->RuntimeResourceManager();

	// Create the scene entity manager
	CreateEntityManagerWithPool(
		&sandbox->scene_entities,
		sandbox->runtime_descriptor.entity_manager_memory_size,
		sandbox->runtime_descriptor.entity_manager_memory_pool_count,
		sandbox->runtime_descriptor.entity_manager_memory_new_allocation_size,
		sandbox->runtime_descriptor.entity_pool_power_of_two,
		allocator
	);
	sandbox->scene_entities.SetAutoGenerateComponentFunctionsFunctor(ComponentFunctionsAutoGenerator, sandbox->entity_manager_auto_generated_data, 0);
	editor_state->editor_components.SetManagerComponents(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);

	// Create the task manager that is going to be reused across runtime plays
	TaskManager* task_manager = (TaskManager*)allocator->Allocate(sizeof(TaskManager));
	new (task_manager) TaskManager(
		std::thread::hardware_concurrency(),
		allocator,
		sandbox->runtime_descriptor.per_thread_temporary_memory_size
	);
	sandbox->runtime_descriptor.task_manager = task_manager;
	// Set the exception handler once when the task manager is created
	SetWorldCrashHandlerTaskManagerExceptionHandler(task_manager);

	// Create the threads for the task manager
	sandbox->runtime_descriptor.task_manager->CreateThreads();

	// Set the debugging names, after the threads are created
	ECS_FORMAT_TEMP_STRING(sandbox_name_string, "Sandbox {#}", sandbox_index);
	task_manager->SetDebuggingNames(sandbox_name_string);

	TaskScheduler* task_scheduler = (TaskScheduler*)allocator->Allocate(sizeof(TaskScheduler) + sizeof(MemoryManager));
	MemoryManager* task_scheduler_allocator = (MemoryManager*)OffsetPointer(task_scheduler, sizeof(TaskScheduler));
	TaskScheduler::DefaultAllocator(task_scheduler_allocator, allocator);

	new (task_scheduler) TaskScheduler(task_scheduler_allocator);
	sandbox->runtime_descriptor.task_scheduler = task_scheduler;

	// We also need to have separate mouse and keyboard controls
	// Such that each sandbox can be controlled independently
	// Coordinated inputs can be easily added the editor level
	Mouse* sandbox_mouse = (Mouse*)allocator->Allocate(sizeof(Mouse));
	*sandbox_mouse = Mouse();
	Keyboard* sandbox_keyboard = (Keyboard*)allocator->Allocate(sizeof(Keyboard));
	*sandbox_keyboard = Keyboard(allocator);
	sandbox->runtime_descriptor.mouse = sandbox_mouse;
	sandbox->runtime_descriptor.keyboard = sandbox_keyboard;

	// Set the mouse OS window handle
	sandbox_mouse->m_window_handle = editor_state->Mouse()->m_window_handle;

	// Create the sandbox world
	sandbox->sandbox_world = World(sandbox->runtime_descriptor);
	sandbox->sandbox_world.speed_up_factor = sandbox->simulation_speed_up_factor;
	sandbox->sandbox_world.task_manager->SetWorld(&sandbox->sandbox_world);
	sandbox->sandbox_world.entity_manager->SetAutoGenerateComponentFunctionsFunctor(ComponentFunctionsAutoGenerator, sandbox->entity_manager_auto_generated_data, 0);
	editor_state->editor_components.SetManagerComponents(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);

	// Initialize the selected entities
	sandbox->selected_entities.Initialize(sandbox_allocator, 0);
	sandbox->selected_entities_changed_counter = 0;

	// Initialize the debug draw components
	sandbox->enabled_debug_draw.Initialize(sandbox_allocator, 0);
	// Initialize the redirect drawer
	// Create a separate allocator for it, we don't need to store it
	// Inside the sandbox
	MemoryManager* redirect_drawer_allocator = (MemoryManager*)Allocate(sandbox_allocator, sizeof(MemoryManager));
	DebugDrawer::DefaultAllocator(redirect_drawer_allocator, allocator);
	sandbox->redirect_drawer.InitializeRedirect(redirect_drawer_allocator, std::thread::hardware_concurrency());

	// Initialize the runtime module snapshots and their allocator
	sandbox->runtime_module_snapshot_allocator = MemoryManager(ECS_KB * 8, 1024, ECS_KB * 256, sandbox_allocator);
	sandbox->runtime_module_snapshots.Initialize(&sandbox->runtime_module_snapshot_allocator, 0);

	// Initialize the asset handle snapshots and their allocator
	sandbox->runtime_asset_handle_snapshot.allocator = MemoryManager(ECS_KB * 128, ECS_KB, ECS_MB, sandbox_allocator);
	sandbox->runtime_asset_handle_snapshot.handle_count = 0;
	sandbox->runtime_asset_handle_snapshot.handle_capacity = 0;

	sandbox->virtual_entities_slots.Initialize(sandbox->GlobalMemoryManager(), 0);
	sandbox->virtual_entity_slot_type.Initialize(sandbox->GlobalMemoryManager(), 0);
	sandbox->virtual_entities_slots_recompute = false;

	// The locked components
	sandbox->locked_entity_components.Initialize(sandbox_allocator, 0);
	sandbox->locked_global_components.Initialize(sandbox_allocator, 0);

	// Initialize the transfer data allocator
	sandbox->sandbox_world_transfer_data_allocator = ResizableLinearAllocator(ECS_KB * 128, ECS_MB, sandbox->GlobalMemoryManager());
	sandbox->sandbox_world_transfer_data = {};

	InitializeSandboxProfilers(editor_state, sandbox_index);

	// Resize the textures for the viewport to a 1x1 texture such that rendering commands will fallthrough even
	// when the UI has not yet run to resize them
	for (size_t index = 0; index < EDITOR_SANDBOX_VIEWPORT_COUNT; index++) {
		ResizeSandboxRenderTextures(editor_state, sandbox_index, (EDITOR_SANDBOX_VIEWPORT)index, { 1, 1 });
	}

	ResetSandboxRecordings(editor_state, sandbox_index);
	ResetSandboxReplays(editor_state, sandbox_index);
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
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		if (UpdateSandboxRuntimeSettings(editor_state, sandbox_index)) {
			LoadSandboxRuntimeSettings(editor_state, sandbox_index);
		}
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

GraphicsBoundTarget RenderSandboxInitializeGraphics(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	GraphicsBoundTarget bound_target = editor_state->RuntimeGraphics()->GetBoundTarget();
	SetSandboxGraphicsTextures(editor_state, sandbox_index, viewport);
	BindSandboxGraphicsSceneInfo(editor_state, sandbox_index, viewport);
	return bound_target;
}

// -----------------------------------------------------------------------------------------------------------------------------

void RenderSandboxFinishGraphics(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport, const GraphicsBoundTarget& bound_target)
{
	// Remove from the system manager bound resources
	SystemManager* system_manager = GetSandbox(editor_state, sandbox_index)->sandbox_world.system_manager;
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		RemoveEditorRuntimeType(system_manager);
		RemoveRuntimeCamera(system_manager);
		RemoveEditorRuntimeSelectColor(system_manager);
		RemoveEditorRuntimeSelectedEntities(system_manager);
		RemoveEditorRuntimeTransformToolEx(system_manager);
		RemoveEditorRuntimeInstancedFramebuffer(system_manager);
		RemoveEditorExtraTransformGizmos(system_manager);
	}

	// Restore the graphics main texture
	editor_state->RuntimeGraphics()->ChangeMainRenderTargetToInitial(false);
	editor_state->RuntimeGraphics()->RestoreBoundTarget(bound_target);

	// Also, disable the depth to not disturb the UI
	editor_state->RuntimeGraphics()->DisableDepth();
}

// -----------------------------------------------------------------------------------------------------------------------------

struct WaitCompilationRenderSandboxEventData {
	unsigned int sandbox_index;
	EDITOR_SANDBOX_VIEWPORT viewport;
	uint2 new_texture_size;
};

static EDITOR_EVENT(WaitCompilationRenderSandboxEvent) {
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

static EDITOR_EVENT(WaitResourceLoadingRenderSandboxEvent) {
	WaitResourceLoadingRenderSandboxEventData* data = (WaitResourceLoadingRenderSandboxEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		RenderSandbox(editor_state, data->sandbox_index, data->viewport, data->new_texture_size);
		// Unlock the sandbox
		UnlockSandbox(editor_state, data->sandbox_index);
		return false;
	}
	return true;
}

struct WaitDebugDrawComponentsBuildEventData {
	unsigned int sandbox_index;
	EDITOR_SANDBOX_VIEWPORT viewport;
	uint2 new_texture_size;
};

static EDITOR_EVENT(WaitDebugDrawComponentsBuildEvent) {
	// This event will wait for the background build entries to finish
	// Such that we don't access data that is being constructed
	WaitDebugDrawComponentsBuildEventData* data = (WaitDebugDrawComponentsBuildEventData*)_data;
	if (GetSandboxBackgroundComponentBuildFunctionCount(editor_state, data->sandbox_index) == 0) {
		RenderSandbox(editor_state, data->sandbox_index, data->viewport, data->new_texture_size);
		// Unlock the sandbox
		UnlockSandbox(editor_state, data->sandbox_index);
		return false;
	}
	return true;
}

struct WaitInitializeWorldRenderEventData {
	unsigned int sandbox_index;
	uint2 new_texture_size;
};

static EDITOR_EVENT(WaitInitializeWorldRenderEvent) {
	WaitInitializeWorldRenderEventData* data = (WaitInitializeWorldRenderEventData*)_data;
	// If it has been initialized, render the sandbox
	if (AreSandboxRuntimeTasksInitialized(editor_state, data->sandbox_index)) {
		RenderSandbox(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, data->new_texture_size);
		// Unlock the sandbox
		UnlockSandbox(editor_state, data->sandbox_index);
		return false;
	}

	// If the sandbox was removed from the runtime state, remove this call
	if (GetSandboxState(editor_state, data->sandbox_index) == EDITOR_SANDBOX_SCENE) {
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

	// If the data source is the runtime one, while the viewport is the scene one 
	// and the world was not initialized, add a pending render call
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		if (GetSandboxActiveViewport(editor_state, sandbox_index) == EDITOR_SANDBOX_VIEWPORT_RUNTIME &&
			!AreSandboxRuntimeTasksInitialized(editor_state, sandbox_index)) {
			if (!EditorHasEventTest(editor_state, WaitInitializeWorldRenderEvent, [sandbox_index, new_texture_size](void* data) {
				WaitInitializeWorldRenderEventData* event_data = (WaitInitializeWorldRenderEventData*)data;
				if (event_data->sandbox_index == sandbox_index) {
					if (new_texture_size.x != 0 && new_texture_size.y != 0) {
						event_data->new_texture_size = new_texture_size;
					}
					return true;
				}
				return false;
			})) {
				WaitInitializeWorldRenderEventData event_data;
				event_data.sandbox_index = sandbox_index;
				event_data.new_texture_size = new_texture_size;

				// Lock the sandbox, such that we don't have it be removed underneath us
				LockSandbox(editor_state, sandbox_index);
				EditorAddEvent(editor_state, WaitInitializeWorldRenderEvent, &event_data, sizeof(event_data));
			}

			return true;
		}
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
		if (!EditorHasEventTest(editor_state, WaitResourceLoadingRenderSandboxEvent, [sandbox_index, viewport, new_texture_size](void* data) {
			// Do not consider the texture size
			WaitResourceLoadingRenderSandboxEventData* event_data = (WaitResourceLoadingRenderSandboxEventData*)data;
			if (event_data->sandbox_index == sandbox_index && event_data->viewport == viewport) {
				// We can change the texture size here directly
				if (new_texture_size.x != 0 && new_texture_size.y != 0) {
					event_data->new_texture_size = new_texture_size;
				}
				return true;
			}
			return false;
		})) {
			// Push an event to wait for the resource loading
			WaitResourceLoadingRenderSandboxEventData event_data;
			event_data.sandbox_index = sandbox_index;
			event_data.viewport = viewport;
			event_data.new_texture_size = new_texture_size;

			// Lock the sandbox such that it cannot be destroyed while we try to render to it
			LockSandbox(editor_state, sandbox_index);
			EditorAddEvent(editor_state, WaitResourceLoadingRenderSandboxEvent, &event_data, sizeof(event_data));
		}
		return true;
	}

	// Check background build functions. Even tho some might not affect the rendering,
	// It is easier as a mental model to always wait for it to finish
	if (GetSandboxBackgroundComponentBuildFunctionCount(editor_state, sandbox_index) > 0) {
		if (!EditorHasEventTest(editor_state, WaitDebugDrawComponentsBuildEvent, [&](void* _data) {
			WaitDebugDrawComponentsBuildEventData* data = (WaitDebugDrawComponentsBuildEventData*)_data;
			if (data->sandbox_index == sandbox_index && data->viewport == viewport) {
				if (new_texture_size.x != 0 && new_texture_size.y != 0) {
					data->new_texture_size = new_texture_size;
				}
				return true;
			}
			return false;
			})) {
			WaitDebugDrawComponentsBuildEventData event_data;
			event_data.sandbox_index = sandbox_index;
			event_data.viewport = viewport;
			event_data.new_texture_size = new_texture_size;

			// Lock the sandbox such that it cannot be destroyed while we try to render to it
			LockSandbox(editor_state, sandbox_index);
			EditorAddEvent(editor_state, WaitDebugDrawComponentsBuildEvent, &event_data, sizeof(event_data));
		}
		return true;
	}

	// Check to see if the module is being compiled right now - if it is, then push an event to try re-render after
	// it has finished compiling
	EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_module_index);
	bool is_being_compiled = IsModuleBeingCompiled(editor_state, sandbox_module->module_index, sandbox_module->module_configuration);
	
	if (!is_being_compiled) {
		// Unbind the resource slot 0, which is used by UI as the texture binding slot
		// and might contain a sandbox texture
		editor_state->RuntimeGraphics()->BindPixelResourceView(nullptr);

		// Check to see if we need to resize the textures
		if (new_texture_size.x != 0 && new_texture_size.y != 0) {
			ResizeSandboxRenderTextures(editor_state, sandbox_index, viewport, new_texture_size);
		}

		// TODO: Are these needed anymore?
		//ResourceManagerSnapshot resource_snapshot = editor_state->RuntimeResourceManager()->GetSnapshot(editor_state->EditorAllocator());
		//GraphicsResourceSnapshot graphics_snapshot = editor_state->RuntimeGraphics()->GetResourceSnapshot(editor_state->EditorAllocator());
		GraphicsBoundTarget bound_target = RenderSandboxInitializeGraphics(editor_state, sandbox_index, viewport);

		EntityManager* viewport_entity_manager = sandbox->sandbox_world.entity_manager;
		EntityManager* runtime_entity_manager = sandbox->sandbox_world.entity_manager;

		// Create a temporary task scheduler that will be bound to the sandbox world
		MemoryManager viewport_task_scheduler_allocator;
		TaskScheduler::DefaultAllocator(&viewport_task_scheduler_allocator, editor_state->GlobalMemoryManager());
		TaskScheduler viewport_task_scheduler(&viewport_task_scheduler_allocator);

		// Use the editor state task_manage in order to run the commands - at the end reset the static tasks
		TaskScheduler* runtime_task_scheduler = sandbox->sandbox_world.task_scheduler;
		TaskManager* runtime_task_manager = sandbox->sandbox_world.task_manager;
		float previous_sandbox_delta_time = sandbox->sandbox_world.delta_time;
		float previous_sandbox_elapsed_seconds = sandbox->sandbox_world.elapsed_seconds;
		size_t previous_sandbox_frame_count = sandbox->sandbox_world.elapsed_frames;

		sandbox->sandbox_world.task_manager = editor_state->render_task_manager;
		sandbox->sandbox_world.task_scheduler = &viewport_task_scheduler;

		// We need to set this up in case we have physical profiling activated
		if (sandbox->world_profiling.HasOption(ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
			ChangePhysicalMemoryProfilerGlobal(&sandbox->world_profiling.physical_memory_profiler);
			SetTaskManagerPhysicalMemoryProfilingExceptionHandler(sandbox->sandbox_world.task_manager);
		}

		MemoryManager runtime_query_cache_allocator;
		ArchetypeQueryCache::DefaultAllocator(&runtime_query_cache_allocator, editor_state->GlobalMemoryManager());
		ArchetypeQueryCache runtime_query_cache;

		// We need to record the query cache to restore it later
		sandbox->sandbox_world.entity_manager->CopyQueryCache(&runtime_query_cache, &runtime_query_cache_allocator);
		EDITOR_SANDBOX_VIEWPORT active_viewport = GetSandboxActiveViewport(editor_state, sandbox_index);
		if (active_viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
			viewport_entity_manager = &sandbox->scene_entities;

			// Bind to the sandbox the scene entity manager
			sandbox->sandbox_world.entity_manager = viewport_entity_manager;
		}

		auto deallocate_temp_resources_and_restore = [&]() {
			AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
			//resource_snapshot.Deallocate(editor_allocator);
			//graphics_snapshot.Deallocate(editor_allocator);
			viewport_task_scheduler_allocator.Free();
			runtime_query_cache_allocator.Free();

			sandbox->sandbox_world.task_manager = runtime_task_manager;
			sandbox->sandbox_world.entity_manager = runtime_entity_manager;
			sandbox->sandbox_world.task_scheduler = runtime_task_scheduler;

			if (sandbox->world_profiling.HasOption(ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
				RemoveTaskManagerPhysicalMemoryProfilingExceptionHandler(sandbox->sandbox_world.task_manager);
			}

			RenderSandboxFinishGraphics(editor_state, sandbox_index, viewport, bound_target);
		};

		// Prepare the task scheduler
		ConstructSandboxSchedulingOrderOptions construct_scheduling_options;
		construct_scheduling_options.disable_error_messages = disable_logging;
		// The ECS runtime systems should not be added
		construct_scheduling_options.include_ecs_runtime_systems = false;
		bool success = ConstructSandboxSchedulingOrder(editor_state, sandbox_index, { &in_stream_module_index, 1 }, true, construct_scheduling_options);
		if (!success) {
			deallocate_temp_resources_and_restore();
			return false;
		}

		// Prepare and run the world once
		// We need to disable the initialize from other tasks and set them manually
		// From the runtime task manager for the case when the sandbox is in PAUSED or RUNNING mode
		TaskSchedulerSetManagerOptions set_options;
		if (active_viewport != EDITOR_SANDBOX_VIEWPORT_SCENE) {
			set_options.assert_exists_initialize_from_other_task = false;
		}
		PrepareWorld(&sandbox->sandbox_world, &set_options);
		if (active_viewport != EDITOR_SANDBOX_VIEWPORT_SCENE) {
			sandbox->sandbox_world.task_scheduler->SetTasksDataFromOther(runtime_task_manager, true);
			RetrieveModuleDebugDrawTaskElementsInitializeData(sandbox->sandbox_world.task_scheduler, sandbox->sandbox_world.task_manager, runtime_task_manager);
			
			ECS_STACK_CAPACITY_STREAM(Stream<char>, enabled_debug_draw_tasks, 1024);
			AggregateSandboxModuleEnabledDebugDrawTasks(editor_state, sandbox_index, &enabled_debug_draw_tasks);
			SetEnabledModuleDebugDrawTaskElements(sandbox->sandbox_world.task_manager, enabled_debug_draw_tasks);
		}

		// Draw the sandbox debug elements before the actual frame
		DrawSandboxDebugDrawComponents(editor_state, sandbox_index);

		// Add all the redirected entries
		// For the scene viewport, for both running and paused states
		// For the game viewport, only for the paused state
		EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
		sandbox->sandbox_world.debug_drawer->redirect_drawer = nullptr;
		if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
			if (sandbox_state == EDITOR_SANDBOX_RUNNING || sandbox_state == EDITOR_SANDBOX_PAUSED) {
				sandbox->redirect_drawer.AddTo(sandbox->sandbox_world.debug_drawer);
			}
		}
		else if (viewport == EDITOR_SANDBOX_VIEWPORT_RUNTIME) {
			if (sandbox_state == EDITOR_SANDBOX_PAUSED) {
				sandbox->redirect_drawer.AddTo(sandbox->sandbox_world.debug_drawer);
			}
		}

		// Set the sandbox crash handler
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 4);
		SandboxCrashHandlerStackScope previous_crash_handler = SandboxSetCrashHandler(editor_state, sandbox_index, &stack_allocator);
		DoFrame(&sandbox->sandbox_world);
		previous_crash_handler.~SandboxCrashHandlerStackScope();

		// Update the mouse and the keyboard for this sandbox - even tho they will get overwritten by the master devices
		// But this action matters for the case when the input is replayed.
		sandbox->sandbox_world.mouse->Update();
		sandbox->sandbox_world.keyboard->Update();

		if (active_viewport != EDITOR_SANDBOX_VIEWPORT_SCENE) {
			// Record the changes for the enabled flag
			UpdateSandboxModuleEnabledDebugDrawTasks(editor_state, sandbox_index, sandbox->sandbox_world.task_manager);
		}

		sandbox->sandbox_world.entity_manager = runtime_entity_manager;
		sandbox->sandbox_world.task_manager = runtime_task_manager;
		sandbox->sandbox_world.task_scheduler = runtime_task_scheduler;

		sandbox->sandbox_world.entity_manager->RestoreQueryCache(&runtime_query_cache);

		// Deallocate the task scheduler and the entity manager query cache
		// Reset the task manager static tasks
		viewport_task_scheduler_allocator.Free();
		runtime_query_cache_allocator.Free();
		editor_state->render_task_manager->ClearTemporaryAllocators();
		editor_state->render_task_manager->ResetStaticTasks();

		sandbox->sandbox_world.SetDeltaTime(previous_sandbox_delta_time);
		sandbox->sandbox_world.elapsed_seconds = previous_sandbox_elapsed_seconds;
		sandbox->sandbox_world.elapsed_frames = previous_sandbox_frame_count;

		//ECS_STACK_CAPACITY_STREAM(char, snapshot_message, ECS_KB * 64);
		// Restore the resource manager first
		//editor_state->RuntimeResourceManager()->RestoreSnapshot(resource_snapshot, &snapshot_message);
		//resource_snapshot.Deallocate(editor_state->EditorAllocator());
		//if (snapshot_message.size > 0) {
		//	ECS_FORMAT_TEMP_STRING(message, "Encountered an error while restoring resource manager snapshot after rendering sandbox {#}, viewport {#}", sandbox_index, ViewportString(viewport));
		//	EditorSetConsoleError(message);
		//	EditorSetConsoleError(snapshot_message);
		//	snapshot_message.size = 0;
		//}

		//// Now the graphics snapshot will be restored as well
		//editor_state->RuntimeGraphics()->RestoreResourceSnapshot(graphics_snapshot, &snapshot_message);
		//if (snapshot_message.size > 0) {
		//	ECS_FORMAT_TEMP_STRING(message, "Encountered an error while restoring runtime graphics snapshot after rendering sandbox {#}, viewport {#}", sandbox_index, ViewportString(viewport));
		//	EditorSetConsoleError(message);
		//	EditorSetConsoleError(snapshot_message);
		//}
		
		RenderSandboxFinishGraphics(editor_state, sandbox_index, viewport, bound_target);
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

	sandbox->viewport_render_destination[viewport] = runtime_graphics->CreateRenderDestination(new_size);

	ECS_STACK_CAPACITY_STREAM(char, visualize_name, 512);

	Stream<char> viewport_description = ViewportString(viewport);
	GetVisualizeTextureName(viewport_description, sandbox_index, true, visualize_name);

	// Add the sandbox render and depth textures into the visualize textures
	VisualizeTextureSelectElement visualize_element;
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
		Texture2D instanced_depth_texture = runtime_graphics->CreateTexture(&instanced_depth_stencil_descriptor);
		sandbox->scene_viewport_depth_stencil_framebuffer = runtime_graphics->CreateDepthStencilView(instanced_depth_texture);

		visualize_name.size = 0;
		GetVisualizeInstancedTextureName(sandbox_index, false, visualize_name);
		visualize_element.name = visualize_name;
		visualize_element.texture = sandbox->scene_viewport_depth_stencil_framebuffer.GetResource();
		SetVisualizeTexture(editor_state, visualize_element);
	}
	else {
		SetSandboxCameraAspectRatio(editor_state, sandbox_index, viewport);
	}

	// Disable the pending resize
	sandbox->viewport_pending_resize[viewport] = { 0, 0 };
}

// ------------------------------------------------------------------------------------------------------------------------------

void ResetSandboxVirtualEntities(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->virtual_entities_slots.FreeBuffer();
	SignalSandboxVirtualEntitiesSlotsCounter(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxVirtualEntitiesSlot(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_ENTITY_SLOT slot_type)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	unsigned int size = sandbox->virtual_entities_slots.size;
	unsigned int offset = 0;

	auto find = [&]() {
		for (unsigned int index = 0; index < size; index++) {
			if (sandbox->virtual_entity_slot_type[offset + index].slot_type == slot_type) {
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
		sandbox->virtual_entities_slots.RemoveSwapBack(slot_index);
		sandbox->virtual_entity_slot_type.RemoveSwapBack(slot_index);
		slot_index = find();
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxDebugDrawComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	ECS_COMPONENT_TYPE component_type
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	unsigned int index = sandbox->enabled_debug_draw.Find(ComponentWithType{ component, component_type });
	if (index != -1) {
		sandbox->enabled_debug_draw.RemoveSwapBack(index);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool RunSandboxWorld(EditorState* editor_state, unsigned int sandbox_index, bool protect_resources, bool is_step)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (is_step) {
		ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_PAUSED);
	}
	else {
		ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_RUNNING);
	}

	// If the sandbox is crashed, early return in order to prevent running invalid sandboxes
	if (sandbox->is_crashed) {
		return false;
	}

	// Check the wait build function background counter
	unsigned int background_build_function_count = GetSandboxBackgroundComponentBuildFunctionCount(editor_state, sandbox_index);
	if (background_build_function_count > 0) {
		// Return immediately to signal that we are waiting for the build functions
		return true;
	}

	// Clear the sandbox waiting compilation flag - it will be reset if it is still valid
	bool was_waiting_compilation = HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);

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
		sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);
		return true;
	}
	else {
		// Clear the waiting flag - since we are no longer waiting
		sandbox->flags = ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION);
	}

	if (was_waiting_compilation && !HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION)) {
		// Clear the runtime info just in case there is something bound from before
		ClearSandboxRuntimeWorldInfo(editor_state, sandbox_index);

		// We need to reconstruct the scheduling order
		bool scheduling_success = PrepareSandboxRuntimeWorldInfo(editor_state, sandbox_index);
		if (!scheduling_success) {
			PauseSandboxWorld(editor_state, sandbox_index);
			return false;
		}
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

	// Retrieve the gpu resources after the render texture resize
	SCOPE_PROTECT_GPU_RESOURCES(&stack_allocator, protect_resources);
	ECS_PROTECT_UNPROTECT_ASSET_DATABASE_RESOURCES(protect_resources, editor_state->asset_database, editor_state->RuntimeResourceManager());

	// Add the sandbox debug elements
	DrawSandboxDebugDrawComponents(editor_state, sandbox_index);

	// Set the redirect drawer
	// Also, clear the redirect drawer
	sandbox->redirect_drawer.Clear();
	sandbox->sandbox_world.debug_drawer->redirect_drawer = &sandbox->redirect_drawer;
	sandbox->sandbox_world.debug_drawer->ActivateRedirect();

	GraphicsBoundTarget bound_target = RenderSandboxInitializeGraphics(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	auto restore_finish_graphics = StackScope([&]() {
		RenderSandboxFinishGraphics(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME, bound_target);
	});

	SandboxCrashHandlerStackScope previous_crash_handler = SandboxSetCrashHandler(editor_state, sandbox_index, &stack_allocator);

	//if (sandbox->sandbox_world.delta_time == 0.0f) {
	//	sandbox->sandbox_world.timer.SetNewStart();
	//}
	//// In case we are stepping, we need to restore the delta time
	//// In both cases, for fixed step for and the dynamic default mode
	//bool keep_delta_time = false;
	//if (is_step) {
	//	keep_delta_time = true;
	//	// Check to see if we are in fixed time step mode
	//	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_FIXED_STEP)) {
	//		// We need to change the delta time
	//		sandbox->sandbox_world.SetDeltaTime(editor_state->project_settings.fixed_timestep * sandbox->sandbox_world.speed_up_factor);
	//	}
	//}

	float delta_time = 0.0f;
	// Stop at the first replay type that drives the delta time. That case must be handled differently than the normal playback.
	EDITOR_SANDBOX_RECORDING_TYPE recording_enabled_type = EDITOR_SANDBOX_RECORDING_TYPE_COUNT;
	bool does_recording_drive_delta_time = false;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		if (DoesSandboxReplay(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index)) {
			recording_enabled_type = (EDITOR_SANDBOX_RECORDING_TYPE)index;
			if (DoesSandboxReplayDriveDeltaTime(editor_state, sandbox_index, recording_enabled_type)) {
				does_recording_drive_delta_time = true;
			}
			break;

			//SandboxReplayInfo replay_info = GetSandboxReplayInfo(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)recording_type);
			//delta_time = replay_info.replay->delta_reader.GetCurrentFrameDeltaTime();
			//// There is one special case - if this a state recording and it is the initial frame, add this delta to the world
			//// Elapsed seconds and use the delta of the next frame. This will ensure that the same state is reconstructed for each frame
			//// As the original replay, and it would make synching an input replay recorded at the same time as the state replay produce
			//// Identical values.
			//if (recording_type == EDITOR_SANDBOX_RECORDING_STATE && sandbox->sandbox_world.elapsed_frames == 0) {
			//	sandbox->sandbox_world.elapsed_seconds += delta_time;
			//	delta_time = replay_info.replay->delta_reader.GetCurrentFrameDeltaTime();
			//	break;
			//}
			//else {
			//	// If the delta reader frames have been exhausted, then don't use the value
			//	if (delta_time != 0.0f) {
			//		break;
			//	}
			//}
		}
	}

	struct SimulateFrameResults {
		// A flag which indicates whether or not the simulation step was successful or not
		bool success;
		// Set to true if the simulation signaled that it should itself, else false
		bool should_stop;
	};

	// Simulates the frame and does all the necessary bookkeeping needed before and after the frame simulation
	// Returns true if it succeeded, else false if an erorr has ocurred. The boolean parameter allows/blocks
	// Registering profiling samples
	auto simulate_frame = [&](bool record_profiling_info) -> SimulateFrameResults {
		SimulateFrameResults frame_results;
		frame_results.should_stop = false;
		frame_results.success = false;

		// Start the sandbox frame profiling after the recordings and replays - this will accurately
		// Report the simulation time, without including this overhead
		if (record_profiling_info) {
			StartSandboxFrameProfiling(editor_state, sandbox_index);
		}

		// Lastly, prepare the simulation stop flag for the Runtime
		// And record the mouse visibility
		SetStopSimulationStatus(sandbox->sandbox_world.system_manager, false);
		bool mouse_was_visible = sandbox->sandbox_world.mouse->IsVisible();
		bool mouse_had_raw_input = sandbox->sandbox_world.mouse->GetRawInputStatus();

		DoFrame(&sandbox->sandbox_world);
		//if (!keep_delta_time) {
		//	// We also need to update the delta time
		//	float new_delta_time = sandbox->sandbox_world.timer.GetDurationFloat(ECS_TIMER_DURATION_S);
		//	if (new_delta_time <= ECS_WORLD_DELTA_TIME_REUSE_THRESHOLD) {
		//		sandbox->sandbox_world.SetDeltaTime(new_delta_time * sandbox->sandbox_world.speed_up_factor);
		//	}
		//}
		//sandbox->sandbox_world.timer.SetNewStart();

		if (record_profiling_info) {
			EndSandboxFrameProfiling(editor_state, sandbox_index);
		}
		
		// Check to see if the visibility status of the mouse has changed
		if (mouse_was_visible != sandbox->sandbox_world.mouse->IsVisible()) {
			editor_state->Mouse()->SetCursorVisibility(sandbox->sandbox_world.mouse->IsVisible());
			// If this is the only sandbox, then focus on it
			FocusUIOnSandbox(editor_state, sandbox_index);
		}
		// Check to see if the raw input status of the mouse has changed
		if (mouse_had_raw_input != sandbox->sandbox_world.mouse->GetRawInputStatus()) {
			if (sandbox->sandbox_world.mouse->GetRawInputStatus()) {
				editor_state->Mouse()->EnableRawInput();
			}
			else {
				editor_state->Mouse()->DisableRawInput();
			}
			// If this is the only sandbox, then focus on it
			FocusUIOnSandbox(editor_state, sandbox_index);
		}

		// Print any graphics messages that have accumulated
		sandbox->sandbox_world.graphics->PrintRuntimeMessagesToConsole();

		// Check to see if a crash happened - do this after the snapshots were restored
		if (ECS_GLOBAL_CRASH_IN_PROGRESS.load(ECS_RELAXED) > 0) {
			// A crash has occured
			// Determine if a critical corruption happened
			// At the moment, critical corruptions can be verified only on the graphics object
			bool valid_internal_state = SandboxValidateStateAfterCrash(editor_state);
			if (!valid_internal_state) {
				// In case the internal state is not valid, just abort
				abort();
			}

			// We need to reset the crash variables before pausing since the threads
			// Might reference the crash counter
			ResetCrashHandlerGlobalVariables();

			// Pause the current sandbox
			PauseSandboxWorld(editor_state, sandbox_index);

			// At last set the is_crashed flag to indicate to the runtime
			// That this sandbox is invalid at this moment and should not be run
			sandbox->is_crashed = true;
			return frame_results;
		}

		// At last, verify if the runtime stop simulation flag was set
		if (GetStopSimulationStatus(sandbox->sandbox_world.system_manager) && !is_step) {
			PauseSandboxWorld(editor_state, sandbox_index);
			frame_results.should_stop = true;
		}

		frame_results.success = true;
		return frame_results;
	};

	// By default, we will have to re-render the scene view, but it can be omitted in some cases.
	bool should_rerender_scene_view = true;

	constexpr float FRAME_ZERO_DELTA_TIME = 1e-7;

	// The case where there is a recording that drives the delta time must be handled completely differently
	if (recording_enabled_type == EDITOR_SANDBOX_RECORDING_TYPE_COUNT || !does_recording_drive_delta_time) {
		// For the first frame, set a delta time that is really small, such that replays can distinguish between the first frame and second frame
		if (sandbox->sandbox_world.elapsed_frames == 0) {
			delta_time = FRAME_ZERO_DELTA_TIME;
		}
		else {
			if (is_step) {
				if (recording_enabled_type != EDITOR_SANDBOX_RECORDING_TYPE_COUNT) {
					SandboxReplayInfo replay_info = GetSandboxReplayInfo(editor_state, sandbox_index, recording_enabled_type);
					delta_time = replay_info.replay->delta_reader.GetFrameDeltaTime(replay_info.replay->delta_reader.GetFrameIndexFromElapsedSeconds(sandbox->sandbox_world.elapsed_seconds, true));
				}
				else {
					if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_FIXED_STEP)) {
						delta_time = editor_state->project_settings.fixed_timestep * sandbox->sandbox_world.speed_up_factor;
					}
					else {
						// In the other case, just use the previous delta time
						delta_time = sandbox->sandbox_world.delta_time;
					}
				}
			}
			else {
				// Use the editor delta time
				delta_time = editor_state->frame_delta_time * sandbox->sandbox_world.speed_up_factor;
			}
		}

		if (delta_time >= 1.0f) {
			// Consider very large deltas as debugging breakpoint stops, use the previous' frame delta time
			delta_time = 0.05f;
		}
		
		sandbox->sandbox_world.SetDeltaTime(delta_time);

		// Run the replays after the delta time is set
		if (!RunSandboxRecordings(editor_state, sandbox_index)) {
			sandbox->sandbox_world.debug_drawer->Clear();
			return false;
		}

		// If we have a state replay, we must add the delta before replaying the reader, because otherwise the state
		// Would be delayed by 1 frame, due to the way the samples are stored (because the first step for the state replay
		// Would be frame 0, meaning the initial state, without a simulation step run, while for a normal sandbox, that would
		// Be the step from frame 0 -> frame 1, this is why we need to add the delta time before the replay)
		if (DoesSandboxReplay(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_STATE)) {
			sandbox->sandbox_world.elapsed_seconds += sandbox->sandbox_world.delta_time;
			// In terms of elapsed frames, use the value from the sandbox replay, in order to have a better reference if playing in parallel another replay.
			// Except for the first frame, where we must increment, otherwise the simulation will be stuck with the small initial delta.
			if (sandbox->sandbox_world.elapsed_frames == 0) {
				sandbox->sandbox_world.elapsed_frames++;
			}
			else {
				// Note: This elapsed frames can get desynched at some times with an equivalent input simulation (if that replay uses substepping and this one doesn't) 
				// Due to floating point errors that accumulate in the reader's "advance_with_substeps_remainder". This is mostly in issue if the original replay
				// Has very small delta times, at larger values, it should be a rare occurence
				sandbox->sandbox_world.elapsed_frames = GetSandboxReplayInfo(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_STATE).replay->delta_reader.GetFrameIndexFromElapsedSeconds(sandbox->sandbox_world.elapsed_seconds, true);
			}
		}

		// If any of the replays failed, we should abort the frame render - especially since
		// The state replay might have now invalid data
		if (!RunSandboxReplays(editor_state, sandbox_index)) {
			// We must rollback some of the stuff that was done beforehand - to maintain
			// Consistency. At the moment, there is a little to be done.
			// Clear the debug drawer, to eliminate any draws that were made. This
			// Does not completely remove all draws, since the draw function could draw
			// Outside the debug drawer, but that is not the common case, and not worth dealing.
			sandbox->sandbox_world.debug_drawer->Clear();
			return false;
		}

		// Difference - if the sandbox state replay is active, then don't call the simulation.
		// Note: At the moment, allowing the simulation to run after the replay is done is not possible,
		// Since not all structures behave correctly with the deserialization (like skipped user functors/callbacks
		// Which might become incorrect after entire state deserialization). So, for the time being, don't allow the
		// Sandbox do to this.
		if (!IsSandboxReplayEnabled(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_STATE)) {
			SimulateFrameResults simulate_frame_results = simulate_frame(true);
			if (!simulate_frame_results.success) {
				return false;
			}
		}
		else {
			// The world frame index and elapsed seconds are updated before the replay, so they shouldn't be updated here.
			// We have to render the sandbox only
			if (DoesSandboxReplay(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_STATE)) {
				// Enable the viewport rendering for it and then disable it again
				EnableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);

				// Also, render the sandbox game window, since that doesn't happen automatically now
				RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);

				DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
			}
			else {
				// We can skip rendering the scene view, since nothing was changed
				should_rerender_scene_view = false;
			}
		}
	}
	else {
		// In case there is a simulation that drives the delta, we have to handle it completely differently.
		SandboxReplayInfo replay_info = GetSandboxReplayInfo(editor_state, sandbox_index, recording_enabled_type);
		ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	
		// Consider the speed up factor as well - although it might not be able to speed up over the maximum substep count
		delta_time = editor_state->frame_delta_time * sandbox->sandbox_world.speed_up_factor;
		// In order to maintain parity with the normal play, for the first frame, use the small initial delta.
		// Because this delta is so small, it can result in a frame not being run the first time and can induce a lag
		// Until a frame gets simulated. For this reason, adjust the delta time only if delta reader advance substeps
		// remainder is also empty.
		if (sandbox->sandbox_world.elapsed_frames == 0 && replay_info.replay->delta_reader.advance_with_substeps_remainder == 0.0f) {
			delta_time = FRAME_ZERO_DELTA_TIME;
		}

		if (delta_time >= 1.0f) {
			// Consider very large deltas as debugging breakpoint stops, use the previous' frame delta time
			delta_time = 0.05f;
		}

		// Start and end the sandbox profiling here - although it doesn't make too much sense in this case
		StartSandboxFrameProfiling(editor_state, sandbox_index);

		unsigned int substeps_advanced = 0;
		auto advance_substep_functor = [&](float substep_delta_time, CapacityStream<char>* error_message) -> DeltaStateReader::ADVANCE_WITH_SUBSTEPS_RETURN {
			// Set the world delta time
			sandbox->sandbox_world.SetDeltaTime(substep_delta_time);
			substeps_advanced++;

			// Run the recordings first
			RunSandboxRecordings(editor_state, sandbox_index);

			// We shouldn't call the replays now, since that is now handled by this one.
			// If this is a state replay, we don't have to do anything.
			if (recording_enabled_type != EDITOR_SANDBOX_RECORDING_STATE) {
				// The only other option, at the moment, is the input replay, for which we have to call the simulate function.
				// In this case, don't record the frame profiling samples for each substep, but rather, overall. This might have
				// Some consequences, like recording the duration of runiing the recordings, but that's rather an obscure case
				// And not worth being bothered about it
				SimulateFrameResults simulate_frame_results = simulate_frame(false);

				// After simulating the frame, for the input recording, we must tick the mouse. The keyboard
				// Should be handled by the delta itself, but the mouse previous position/scroll is not
				sandbox->sandbox_world.mouse->SetPreviousPositionAndScroll();

				if (!simulate_frame_results.success) {
					// Ignore for the moment the error message.
					return DeltaStateReader::ADVANCE_WITH_SUBSTEPS_ERROR;
				}
				else if (simulate_frame_results.should_stop) {
					return DeltaStateReader::ADVANCE_WITH_SUBSTEPS_PAUSE;
				}
			}
			else {
				// We have to manually update the sandbox elapsed seconds and elapsed frames
				sandbox->sandbox_world.elapsed_seconds += sandbox->sandbox_world.delta_time;
				sandbox->sandbox_world.elapsed_frames++;
			}

			return DeltaStateReader::ADVANCE_WITH_SUBSTEPS_OK;
		};

		// If this is a simulation step call, simulate only a substep, not an entire delta
		bool replay_advance_success = false;
		if (is_step) {
			replay_advance_success = replay_info.replay->delta_reader.AdvanceSingleSubstep(advance_substep_functor, &error_message);
		}
		else {
			replay_advance_success = replay_info.replay->delta_reader.AdvanceWithSubsteps(delta_time, 10000, advance_substep_functor, &error_message);
		}

		if (!replay_advance_success) {
			EndSandboxFrameProfiling(editor_state, sandbox_index);

			ECS_FORMAT_TEMP_STRING(console_message, "Failed to read/execute {#} replay at moment {#} for sandbox {#}. (Reason: {#})", replay_info.type_string, sandbox->sandbox_world.elapsed_seconds, sandbox_index, error_message);
			EditorSetConsoleError(console_message);
			return false;
		}

		EndSandboxFrameProfiling(editor_state, sandbox_index);

		// If at least one frame was rendered and this is a state replay, than we have to re-render the sandbox.
		if (substeps_advanced > 0) {
			// Enable the viewport rendering for it and then disable it again
			EnableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);

			// Also, render the sandbox game window, since that doesn't happen automatically now
			RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);

			DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
		}
		else {
			// The scene re-render can be disabled.
			should_rerender_scene_view = false;
		}
	}

	if (should_rerender_scene_view) {
		if (!is_step) {
			// Render the scene if it is visible - the runtime is rendered by the game now		
			EnableSceneUIRendering(editor_state, sandbox_index, true);
		}

		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);

		if (!is_step) {
			// Disable this irrespective if it was enabled here or not
			DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		}
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool RunSandboxWorlds(EditorState* editor_state, bool is_step)
{
	bool success = true;
	
	bool is_any_paused = IsAnyDefaultSandboxPaused(editor_state);
	if (is_any_paused) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 8);
		// Take care of protecting the resources outside the loop
		SCOPE_PROTECT_GPU_RESOURCES(&stack_allocator, true);
		ECS_PROTECT_UNPROTECT_ASSET_DATABASE_RESOURCES(true, editor_state->asset_database, editor_state->RuntimeResourceManager());

		// Exclude temporary sandboxes
		SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
			const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			if (sandbox->should_step && sandbox->run_state == EDITOR_SANDBOX_PAUSED) {
				// Do not protect the resources
				success &= RunSandboxWorld(editor_state, sandbox_index, false, is_step);
			}
		}, true);
	}
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

	sandbox->sandbox_world.graphics->ChangeMainRenderTarget(
		sandbox->viewport_render_destination[viewport].render_view, 
		sandbox->viewport_render_destination[viewport].depth_view
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

void SetSandboxVirtualEntitySlotType(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int slot_index,
	EditorSandboxEntitySlot slot
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->virtual_entity_slot_type[slot_index] = slot;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool ShouldSandboxRecomputeVirtualEntitySlots(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->virtual_entities_slots_recompute;
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

struct SignalVirtualEntitiesSlotsCounterEventData {
	unsigned int sandbox_index;
};

EDITOR_EVENT(SignalVirtualEntitiesSlotsCounterEvent) {
	SignalVirtualEntitiesSlotsCounterEventData* data = (SignalVirtualEntitiesSlotsCounterEventData*)_data;
	GetSandbox(editor_state, data->sandbox_index)->virtual_entities_slots_recompute = true;
	return false;
}

void SignalSandboxVirtualEntitiesSlotsCounter(EditorState* editor_state, unsigned int sandbox_index)
{
	SignalVirtualEntitiesSlotsCounterEventData event_data;
	event_data.sandbox_index = sandbox_index;
	EditorAddEvent(editor_state, SignalVirtualEntitiesSlotsCounterEvent, &event_data, sizeof(event_data));
}

// -----------------------------------------------------------------------------------------------------------------------------

bool StartSandboxWorld(EditorState* editor_state, unsigned int sandbox_index, bool disable_error_messages)
{
	// Don't allow starting a sandbox runtime if the reflection hierarchy for a module that it is referencing 
	// Could not be parsed. The reason is that automatic copy/deallocate/serialization/deserialization functions
	// Can be missing and that can introduce subtle bugs.

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_ASSERT(sandbox->run_state == EDITOR_SANDBOX_PAUSED || sandbox->run_state == EDITOR_SANDBOX_SCENE);

	// Check to see if the sandbox is crashed, if it is, we shouldn't let the start commence
	if (sandbox->is_crashed) {
		if (!disable_error_messages) {
			ECS_FORMAT_TEMP_STRING(message, "Cannot start sandbox {#} because it is crashed", sandbox_index);
			EditorSetConsoleError(message);
		}
		return false;
	}

	ECS_STACK_CAPACITY_STREAM(char, modules_reflected_error_message, 256);
	if (!AreSandboxModulesReflected(editor_state, sandbox_index, &modules_reflected_error_message)) {
		if (!disable_error_messages) {
			ECS_FORMAT_TEMP_STRING(message, "Cannot start sandbox {#}. Reason: {#}", sandbox_index, modules_reflected_error_message);
			EditorSetConsoleError(message);
		}
		return false;
	}

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
		// Try to initialize the recordings and the replays. If any of them fails, then abort the start.
		success = InitializeSandboxRecordings(editor_state, sandbox_index, true);
		if (!success) {
			return false;
		}

		success = InitializeSandboxReplays(editor_state, sandbox_index, true);
		if (!success) {
			return false;
		}

		if (!waiting_sandbox_compile) {
			ConstructSandboxSchedulingOrderOptions scheduling_options;
			scheduling_options.disable_error_messages = disable_error_messages;
			success = ConstructSandboxSchedulingOrder(editor_state, sandbox_index, true, scheduling_options);
		}
		if (success) {
			if (!waiting_sandbox_compile) {
				// Copy the entities from the scene to the runtime
				// We need to this only if we are not waiting modules
				// To be compiled since we rely on component functions
				// From there and if we are compiling, some of these functions
				// Might be unavailable since the module has been unloaded
				CopySceneEntitiesIntoSandboxRuntime(editor_state, sandbox_index);

				// Prepare the sandbox world
				PrepareWorld(&sandbox->sandbox_world);
				// Set the initialize task flag
				sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_WORLD_INITIALIZED);
			}
			else {
				// Only prepare the base world
				PrepareWorldBase(&sandbox->sandbox_world);
				// We also have to manually change the task manager wrapper to the task
				// Scheduler ones otherwise the next set wrappers call can fail because
				// Of not enough space
				sandbox->sandbox_world.task_scheduler->SetTaskManagerWrapper(sandbox->sandbox_world.task_manager);
			}

			// We have to set the crash wrappers when starting the
			// Simulation and after resuming after a module change
			SetSandboxCrashHandlerWrappers(editor_state, sandbox_index);

			HandleSandboxAssetHandlesSnapshotsChanges(editor_state, sandbox_index, true);

			// Clear the CPU / GPU frame profilers
			ClearSandboxProfilers(editor_state, sandbox_index);

			// Now we need to initialize the simulation profiling
			StartSandboxSimulationProfiling(editor_state, sandbox_index);
		}
	}
	else {
		// We need to delay the world timer in case the world is restarted quickly after it has been paused
		sandbox->sandbox_world.timer.DelayStart(10, ECS_TIMER_DURATION_S);
		// We still need to get the handle snapshot
		HandleSandboxAssetHandlesSnapshotsChanges(editor_state, sandbox_index, false);
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
		BindSandboxRuntimeModuleSettings(editor_state, sandbox_index);
		BindSandboxGraphicsSceneInfo(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	}
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool StartSandboxWorlds(EditorState* editor_state, bool paused_only)
{
	bool success = true;
	// Exclude temporary sandboxes
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
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
	}, true);
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool StartEndSandboxWorld(EditorState* editor_state) {
	bool success = true;
	bool all_are_stopped = true;

	// Exclude temporary sandboxes
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->should_play) {
			EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_index);
			if (state == EDITOR_SANDBOX_SCENE) {
				success &= StartSandboxWorld(editor_state, sandbox_index);
				all_are_stopped = false;
			}
			else {
				EndSandboxWorldSimulation(editor_state, sandbox_index);
			}
		}
	}, true);

	// Handle the mouse visibility and raw input
	if (all_are_stopped) {
		if (!editor_state->Mouse()->IsVisible()) {
			editor_state->Mouse()->SetCursorVisibility(true);
		}
		if (editor_state->Mouse()->GetRawInputStatus()) {
			editor_state->Mouse()->DisableRawInput();
		}
	}

	// If the console clear on play flag is 
	GetConsole()->ClearOnPlay();

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

void WaitSandboxUnlock(const EditorState* editor_state, unsigned int sandbox_index)
{
	SandboxAction(editor_state, sandbox_index, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		TickWait<'!'>(15, sandbox->locked_count, (unsigned char)0);
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void TickUpdateSandboxHIDInputs(EditorState* editor_state)
{
	const ProjectSettings* project_settings = &editor_state->project_settings;

	// If we have synchronized inputs, we need to splat early the update
	// Such that the tick will catch the modification
	unsigned int active_window = editor_state->ui_system->GetActiveWindow();
	bool was_input_synchronized = false;
	unsigned int active_sandbox_index = -1;
	SandboxAction<true>(editor_state, -1, [&](unsigned int sandbox_index) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			unsigned int game_ui_index = GetGameUIWindowIndex(editor_state, sandbox_index);
			// If we have synchronized input, we can enter at any sandbox
			if (project_settings->synchronized_sandbox_input || game_ui_index == active_window) {
				if (project_settings->synchronized_sandbox_input) {
					// Splat this sandbox' mouse input and/or keyboard input
					was_input_synchronized = true;
					SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
						EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
						// Update the HID inputs only if the sandbox does not have an active input replay.
						// If it has an input replay, just update the previous position and scroll to the current values (for the mouse)
						// The keyboard button change should be handled properly in that case (if a button must transition, it will
						// Be done so by the replay)
						if (!DoesSandboxReplay(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT)) {
							sandbox->sandbox_world.mouse->UpdateFromOther(editor_state->Mouse());
							if (!project_settings->unfocused_keyboard_input) {
								sandbox->sandbox_world.keyboard->UpdateFromOther(editor_state->Keyboard());
							}
						}
						else {
							// If the input replay is driving the delta time for the sandbox, we shouldn't do this,
							// It is done by the substepping function
							if (!DoesSandboxReplayDriveDeltaTime(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT)) {
								sandbox->sandbox_world.mouse->SetPreviousPositionAndScroll();
							}
						}
					});
				}
				else {
					// Update the HID inputs only if the sandbox does not have an active input replay
					// If it has an input replay, just update the previous position and scroll to the current values (for the mouse)
					// The keyboard button change should be handled properly in that case (if a button must transition, it will
					// Be done so by the replay)
					if (!DoesSandboxReplay(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT)) {
						sandbox->sandbox_world.mouse->UpdateFromOther(editor_state->Mouse());
						sandbox->sandbox_world.keyboard->UpdateFromOther(editor_state->Keyboard());
					}
					else {
						// If the input replay is driving the delta time for the sandbox, we shouldn't do this,
						// It is done by the substepping function
						if (!DoesSandboxReplayDriveDeltaTime(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT)) {
							sandbox->sandbox_world.mouse->SetPreviousPositionAndScroll();
						}
					}
				}
				active_sandbox_index = sandbox_index;
				return true;
			}
			else {
				// In case this is not the active window and it has an input replay, we need to update the mouse
				if (DoesSandboxReplay(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT) &&
					!DoesSandboxReplayDriveDeltaTime(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT)) {
					sandbox->sandbox_world.mouse->SetPreviousPositionAndScroll();
				}
			}
		}
		return false;
	});

	if (!was_input_synchronized) {
		SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			// Update the HID inputs only if the sandbox does not have an active input replay
			if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
				if (!DoesSandboxReplay(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT)) {
					if (active_sandbox_index != sandbox_index) {
						// We need to update release these controls
						sandbox->sandbox_world.mouse->UpdateRelease();
						if (project_settings->unfocused_keyboard_input) {
							sandbox->sandbox_world.keyboard->UpdateFromOther(editor_state->Keyboard());
						}
						else {
							sandbox->sandbox_world.keyboard->UpdateRelease();
						}
					}
				}
				else {
					// If the input replay is driving the delta time for the sandbox, we shouldn't do this,
					// It is done by the substepping function
					if (!DoesSandboxReplayDriveDeltaTime(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT)) {
						sandbox->sandbox_world.mouse->SetPreviousPositionAndScroll();
					}
				}
			}
		});
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TickSandboxesRuntimeSettings(EditorState* editor_state) {
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_RUNTIME_SETTINGS, LAZY_EVALUATION_RUNTIME_SETTINGS)) {
		ReloadSandboxRuntimeSettings(editor_state);
	}
}

void TickSandboxesSelectedEntities(EditorState* editor_state) {
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->selected_entities_changed_counter > 0) {
			RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
		}
		sandbox->selected_entities_changed_counter = SaturateSub(sandbox->selected_entities_changed_counter, (unsigned char)1);
	});
}

void TickSandboxesClearUnusedSlots(EditorState* editor_state) {
	SandboxAction(editor_state, -1, [editor_state](unsigned int sandbox_index) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		sandbox->virtual_entities_slots_recompute = false;
	});
}

void TickSandboxes(EditorState* editor_state)
{
	TickSandboxesRuntimeSettings(editor_state);
	TickSandboxesSelectedEntities(editor_state);
	TickSandboxesClearUnusedSlots(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

void TickSandboxRuntimes(EditorState* editor_state)
{
	// Update the frame timer
	editor_state->frame_delta_time = editor_state->frame_timer.GetDurationFloat(ECS_TIMER_DURATION_S);
	editor_state->frame_timer.SetNewStart();

	unsigned int sandbox_count = GetSandboxCount(editor_state);
	if (CanSandboxesRun(editor_state)) {
		// Allow temporary sandboxes here since they might want to enter the play state
		// Through some of their actions
		// Use a pre-check first to see if we have a running sandbox in order to protect the resources outside the loop
		bool is_any_running = SandboxAction<true>(editor_state, -1, [editor_state](unsigned int sandbox_index) {
			return GetSandboxState(editor_state, sandbox_index) == EDITOR_SANDBOX_RUNNING;
		});

		if (is_any_running) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 8);
			SCOPE_PROTECT_GPU_RESOURCES(&stack_allocator, true);
			ECS_PROTECT_UNPROTECT_ASSET_DATABASE_RESOURCES(true, editor_state->asset_database, editor_state->RuntimeResourceManager());

			SandboxAction<true>(editor_state, -1, [&](unsigned int sandbox_index) {
				EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
				// Check to see if the current sandbox is being run
				if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
					// Don't protect the resources, that is done outside the loop
					bool simulation_success = RunSandboxWorld(editor_state, sandbox_index, false);

					// If there was an error with the snapshot, then pause all other sandboxes
					if (!simulation_success) {
						for (unsigned int subindex = 0; subindex < sandbox_count; subindex++) {
							if (GetSandboxState(editor_state, subindex) == EDITOR_SANDBOX_RUNNING) {
								// Do not restore the mouse state, the crashing/invalid sandbox already did that 
								PauseSandboxWorld(editor_state, subindex, false);
							}
						}
						// We can exit the loop now
						return true;
					}
				}
				else if (sandbox->run_state == EDITOR_SANDBOX_PAUSED) {
					// In case it is paused and it has frame profiling, refresh the overall frame timer
					if (IsSandboxStatisticEnabled(editor_state, sandbox_index, ECS_WORLD_PROFILING_CPU)) {
						sandbox->world_profiling.cpu_profiler.RefreshOverallTime();
					}
				}
				return false;
			});
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TickSandboxUpdateMasterButtons(EditorState* editor_state)
{
	// Each frame check to see if we need to update the state of the master buttons
	bool are_all_default_running = AreAllDefaultSandboxesRunning(editor_state);

	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING)) {
		if (!are_all_default_running) {
			if (IsAnyDefaultSandboxNotStarted(editor_state)) {
				EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PLAYING);
				// One of them is terminated, terminate 
				EndSandboxWorldSimulations(editor_state);
				if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
					EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PAUSED);
				}
			}
			else {
				if (!EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
					EditorStateSetFlag(editor_state, EDITOR_STATE_IS_PAUSED);
				}
				// We need to pause all the other running sandboxes
				PauseSandboxWorlds(editor_state);
			}
		}
	}
	else {
		if (are_all_default_running) {
			EditorStateSetFlag(editor_state, EDITOR_STATE_IS_PLAYING);
		}
		else {
			bool is_any_running = IsAnyDefaultSandboxRunning(editor_state);
			if (is_any_running) {
				EndSandboxWorldSimulations(editor_state, false, true);
			}
		}
	}

	// These need to be refreshed since some action before might have changed the values
	are_all_default_running = AreAllDefaultSandboxesRunning(editor_state);
	bool are_all_default_paused = AreAllDefaultSandboxesPaused(editor_state);
	bool are_all_default_not_started = AreAllDefaultSandboxesNotStarted(editor_state);

	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
		if (!are_all_default_paused) {
			EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PAUSED);

			if (!are_all_default_running) {
				// If any of them is terminated, terminate all of them - if they are not already
				if (!are_all_default_not_started) {
					EndSandboxWorldSimulations(editor_state);
					if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING)) {
						EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PLAYING);
					}
				}
			}
			else {
				EditorStateSetFlag(editor_state, EDITOR_STATE_IS_PLAYING);
			}
		}
	}
	else {
		if (are_all_default_paused) {
			EditorStateSetFlag(editor_state, EDITOR_STATE_IS_PAUSED);
		}
		else {
			bool is_any_paused = IsAnyDefaultSandboxPaused(editor_state);
			if (is_any_paused) {
				EndSandboxWorldSimulations(editor_state, true);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------