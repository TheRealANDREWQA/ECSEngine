#pragma once
#include "SandboxAccessor.h"
#include "ECSEngineRendering.h"
#include "ECSEngineResources.h"
#include "../Modules/ModuleDefinition.h"

struct EditorState;

// -------------------------------------------------------------------------------------------------------------

void AddSandboxDebugDrawComponent(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Component component, ECSEngine::ECS_COMPONENT_TYPE component_type);

// -------------------------------------------------------------------------------------------------------------

// Returns true if all of the sandboxes that want to be run by the master button are running, else false
bool AreAllDefaultSandboxesRunning(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Returns true if all of the sandboxes that want to be run by the master button are paused, else false
bool AreAllDefaultSandboxesPaused(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Returns true if all of the sandboxes that want to be run by the master button are in the scene state (not running or paused), else false
bool AreAllDefaultSandboxesNotStarted(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Returns true if there are sandboxes that are to be run - default or non default
bool AreSandboxesBeingRun(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

bool AreSandboxRuntimeTasksInitialized(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void BindSandboxGraphicsSceneInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

// Returns true if the sandboxes are allowed to run, else false
bool CanSandboxesRun(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// It will call the cleanup functions for the runtime sandbox and register any transfer data that there is
void CallSandboxRuntimeCleanupFunctions(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void ClearSandboxRuntimeTransferData(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// The settings name needs to be only the stem i.e. Settings - not the absolute path
// Returns true if it succeded in changing the settings. It can fail if the file doesn't exist or couldn't be deserialized
// If the settings_name is { nullptr, 0 }, it will use default values for the runtime descriptor and remove the settings name associated
bool ChangeSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> settings_name);

// -------------------------------------------------------------------------------------------------------------

// It changes the value of a stored component to be another one of the same type
// It performs the change only if the old component actually exists
void ChangeSandboxDebugDrawComponent(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Component old_component,
	ECSEngine::Component new_component,
	ECSEngine::ECS_COMPONENT_TYPE type
);

// -------------------------------------------------------------------------------------------------------------

// Resets the task manager and the task scheduler, and also clears the entity manager query cache and 
// the system manager system settings 
void ClearSandboxRuntimeWorldInfo(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void ClearSandboxDebugDrawComponents(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// It will copy all the state of the source sandbox to the destination
// It will re-render the viewport as well. The last flag controls whether
// The runtime world should be copied if the source sandbox is in a run or paused state
void CopySandbox(EditorState* editor_state, unsigned int destination_index, unsigned int source_index, bool copy_runtime_world = true);

// -------------------------------------------------------------------------------------------------------------

// Can delay the initialization of the runtime for later on. It must be done manually to a call with InitializeSandboxRuntime
// Returns the index of the created sandbox
unsigned int CreateSandbox(EditorState* editor_state, bool initialize_runtime = true);

// -------------------------------------------------------------------------------------------------------------

// Returns the sandbox index of that temporary sandbox
unsigned int CreateSandboxTemporary(EditorState* editor_state, bool initialize_runtime = true);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeeded in creating the scheduling order for the sandbox. Can disable the automatic
// printing of the error message if it fails.
bool ConstructSandboxSchedulingOrder(EditorState* editor_state, unsigned int sandbox_index, bool scene_order, bool disable_error_message = false);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeeded in creating the scheduling order for the sandbox using the given mask for the modules to schedule.
// The module mask must index into the sandbox editor module indices, not into ProjectModules
// Can disable the automatic printing of the error message if it fails.
bool ConstructSandboxSchedulingOrder(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::Stream<unsigned int> module_indices,
	bool scene_order,
	bool disable_error_message = false
);

// -------------------------------------------------------------------------------------------------------------

// It will duplicate the given sandbox, without any UI being created for it. Returns the newly created sandbox' index
unsigned int DuplicateSandbox(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void DecrementSandboxModuleComponentBuildCount(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void DestroySandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Can optionally disable the wait for the unlocking - by default you should leave this on
// unless you have a very specific reason to ignore the value. Or you can choose to have an
// Event destroy the sandbox in a deferred fashion
void DestroySandbox(EditorState* editor_state, unsigned int sandbox_index, bool wait_unlocking = true, bool deferred_waiting = false);

// -------------------------------------------------------------------------------------------------------------

void DisableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

// -------------------------------------------------------------------------------------------------------------

void DisableSandboxDebugDrawAll(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Determines which components have debug drawing activated, and it will add the debug primitives to the
// World to be rendered
void DrawSandboxDebugDrawComponents(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

void EnableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

// -------------------------------------------------------------------------------------------------------------

void EnableSandboxDebugDrawAll(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Restores the scene world into the runtime world
void EndSandboxWorldSimulation(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Can choose to end simulations only for sandboxes that are only paused, only running, or either paused or running
void EndSandboxWorldSimulations(EditorState* editor_state, bool paused_only = false, bool running_only = false);

// -------------------------------------------------------------------------------------------------------------

// Returns -1 if the entity is not selected
unsigned int FindSandboxSelectedEntityIndex(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Entity entity
);

// -------------------------------------------------------------------------------------------------------------

// Returns -1 if it doesn't find slot with the given value
ECSEngine::Entity FindSandboxVirtualEntitySlot(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_ENTITY_SLOT slot_type
);

// -------------------------------------------------------------------------------------------------------------

// Returns EDITOR_SANDBOX_ENTITY_SLOT_COUNT if the entity could not be found
// It will assert that the slot was assigned beforehand
EditorSandboxEntitySlot FindSandboxVirtualEntitySlot(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Entity entity
);

// -------------------------------------------------------------------------------------------------------------

// Checks to see that the textures are actually created before freeing
// If the viewport is left unspecified, it will call this function for all viewports
void FreeSandboxRenderTextures(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

// -------------------------------------------------------------------------------------------------------------

// Returns true if the given component has a debug draw function on it
bool ExistsSandboxDebugDrawComponentFunction(const EditorState* editor_state, unsigned int sandbox_index, Component component, ECS_COMPONENT_TYPE type);

// -------------------------------------------------------------------------------------------------------------

// This is the absolute path
void GetSandboxRuntimeSettingsPath(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<wchar_t>& path);

// -------------------------------------------------------------------------------------------------------------

// This will get the absolute path
void GetSandboxRuntimeSettingsPath(const EditorState* editor_state, ECSEngine::Stream<wchar_t> filename, ECSEngine::CapacityStream<wchar_t>& path);

// -------------------------------------------------------------------------------------------------------------

// Returns all the available runtime settings paths for a sandbox to choose
// The name that gets copied is only the stem of the file. Example:
// C:\Users\MyName\Project\Configuration\Runtime\Default.config -> Default (Only the stem)
void GetSandboxAvailableRuntimeSettings(
	const EditorState* editor_state, 
	ECSEngine::CapacityStream<ECSEngine::Stream<wchar_t>>& paths, 
	ECSEngine::AllocatorPolymorphic allocator
);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE ECSEngine::RenderTargetView GetSandboxInstancedFramebuffer(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandbox(editor_state, sandbox_index)->scene_viewport_instance_framebuffer;
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE ECSEngine::DepthStencilView GetSandboxInstancedDepthFramebuffer(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandbox(editor_state, sandbox_index)->scene_viewport_depth_stencil_framebuffer;
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE ECSEngine::Stream<ECSEngine::Entity> GetSandboxSelectedEntities(
	const EditorState* editor_state,
	unsigned int sandbox_index
) {
	return GetSandbox(editor_state, sandbox_index)->selected_entities.ToStream();
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE size_t GetSandboxSelectedEntitiesCount(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandboxSelectedEntities(editor_state, sandbox_index).size;
}

// -------------------------------------------------------------------------------------------------------------

void GetSandboxAllPossibleDebugDrawComponents(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::AdditionStream<ECSEngine::ComponentWithType> components
);

// -------------------------------------------------------------------------------------------------------------

// It will not fill in debug draw components that are enabled, but have crashed beforehand
void GetSandboxDisplayDebugDrawComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::AdditionStream<ECSEngine::ComponentWithType> components
);

// -------------------------------------------------------------------------------------------------------------

// This version will filter any virtual entities (unused entity slots) that appear here
// The filtered entities must have a capacity equal or greater than the selected entities size
// The rejected entities will be placed at the end after the filtered ones. To iterate over them,
// Just iterate starting from size until capacity
void GetSandboxSelectedEntitiesFiltered(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::CapacityStream<ECSEngine::Entity>* filtered_entities
);

// -------------------------------------------------------------------------------------------------------------

// Fills in all the components that are to be controlled using transform widgets
void GetSandboxComponentTransformGizmos(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::CapacityStream<ECSEngine::GlobalComponentTransformGizmos>* components
);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxSelectedVirtualEntities(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<ECSEngine::Entity>* entities);

// -------------------------------------------------------------------------------------------------------------

// Can optionally give a pointer with the virtual entities that are selected with gizmo pointers
void GetSandboxSelectedVirtualEntitiesTransformPointers(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::CapacityStream<ECSEngine::TransformGizmoPointers>* pointers,
	ECSEngine::CapacityStream<ECSEngine::Entity>* entities = nullptr
);

// -------------------------------------------------------------------------------------------------------------

// Can optionally give a pointer with the virtual entities that are selected with gizmo pointers
void GetSandboxSelectedVirtualEntitiesTransformGizmos(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::CapacityStream<ECSEngine::TransformGizmo>* gizmos,
	ECSEngine::CapacityStream<ECSEngine::Entity>* entities = nullptr
);

// -------------------------------------------------------------------------------------------------------------

// Fills in entity slots that are not yet used in the runtime
// or somewhere else in the editor. Returns an index that needs to be used
// to set the transform type afterwards with the function SetSandboxUnusedEntitySlotType
unsigned int GetSandboxVirtualEntitySlots(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<ECSEngine::Entity> entities
);

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE ECSEngine::Stream<ECSEngine::Entity> GetSandboxVirtualEntitySlots(
	const EditorState* editor_state,
	unsigned int sandbox_index
) {
	return GetSandbox(editor_state, sandbox_index)->virtual_entities_slots.ToStream();
}

// -------------------------------------------------------------------------------------------------------------

ECS_INLINE unsigned int GetSandboxBackgroundComponentBuildFunctionCount(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandbox(editor_state, sandbox_index)->background_component_build_functions.load(ECS_RELAXED);
}

// -------------------------------------------------------------------------------------------------------------

// Called during the initialization of the editor state to set the allocator
// and to create the cache graphics and cache resource manager
void InitializeSandboxes(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

void InitializeSandboxRuntime(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxGizmoEntity(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Entity entity
);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxDebugDrawComponentEnabled(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Component component,
	ECSEngine::ECS_COMPONENT_TYPE component_type
);

// -------------------------------------------------------------------------------------------------------------

// Returns true if any of the sandboxes that want to be run on the global run button is currently running
bool IsAnyDefaultSandboxRunning(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Returns true if any of the sandboxes that want to be run on the global run button is currently paused
bool IsAnyDefaultSandboxPaused(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Returns true if any of the sandboxes that want to be run on the global run button is currently not even started
bool IsAnyDefaultSandboxNotStarted(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

void IncrementSandboxModuleComponentBuildCount(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns the world descriptor from the settings assigned.
// Returns true if the deserialization succeded, if it fails it prints
// an error message
bool LoadSandboxRuntimeSettings(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

// The filename should only be Default (without extension). It will construct the absolute path automatically
bool LoadRuntimeSettings(
	const EditorState* editor_state,
	ECSEngine::Stream<wchar_t> filename,
	ECSEngine::WorldDescriptor* descriptor,
	ECSEngine::CapacityStream<char>* error_message = nullptr
);

// -------------------------------------------------------------------------------------------------------------

// Useful for example for not letting the sandbox be destroyed while a load operation is in progress
// This can be called multiple times, it is a counter, not a boolean and it is atomic.
ECS_INLINE void LockSandbox(EditorState* editor_state, unsigned int sandbox_index) {
	GetSandbox(editor_state, sandbox_index)->locked_count.fetch_add(1, ECS_RELAXED);
}

// -------------------------------------------------------------------------------------------------------------

// By default, it will restore the mouse status, but you can disable this feature with the last parameter
void PauseSandboxWorld(EditorState* editor_state, unsigned int index, bool restore_mouse_status = true);

// -------------------------------------------------------------------------------------------------------------

// Pauses all sandboxes that want to be paused using the general button
void PauseSandboxWorlds(EditorState* editor_state, bool restore_mouse_status = true);

// -------------------------------------------------------------------------------------------------------------

// Pauses any running sandbox that wants to be paused using the general button
// And starts any sandbox that is currently paused
void PauseUnpauseSandboxWorlds(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// It will construct the scheduling order, prepare the world concurrency, bind module settings
// And the graphics scene info.
// It returns true if it succeeded, else false (if the scheduling order could not be solved)
bool PrepareSandboxRuntimeWorldInfo(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Some objects need to be created just once and used accros runtime executions
void PreinitializeSandboxRuntime(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

void ReinitializeSandboxRuntime(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

// If there is an out-of-date runtime settings then it will try to update it
void ReloadSandboxRuntimeSettings(
	EditorState* editor_state
);

// -------------------------------------------------------------------------------------------------------------

// Sets all the required graphics options needed to run the given viewport
// Returns a structure that must be given to FinishGraphics
GraphicsBoundTarget RenderSandboxInitializeGraphics(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

void RenderSandboxFinishGraphics(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport, const GraphicsBoundTarget& bound_target);

// -------------------------------------------------------------------------------------------------------------

// If the new size is specified, it will also resize the textures before rendering
// This call will push an event that will render the sandbox at the end of the frame
// such that multiple call to this function in the same frame will resolve to a single draw
// (it will take into account if it cannot right now because of the resource loading)
bool RenderSandbox(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	ECSEngine::uint2 new_size = { 0, 0 }, 
	bool disable_logging = false
);

// Returns true whether there is already a pending request or the module is being built
// Else false
bool RenderSandboxIsPending(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport
);

// -------------------------------------------------------------------------------------------------------------

// It will render all viewports of that sandbox. Returns true if all renders succeeded
bool RenderSandboxViewports(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::uint2 new_size = { 0, 0 },
	bool disable_logging = false
);

// -------------------------------------------------------------------------------------------------------------

// The new size needs to be specified in texels
void ResizeSandboxRenderTextures(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport, ECSEngine::uint2 new_size);

// -------------------------------------------------------------------------------------------------------------

void ResetSandboxVirtualEntities(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

void RemoveSandboxVirtualEntitiesSlot(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_ENTITY_SLOT slot_type
);

// -------------------------------------------------------------------------------------------------------------

// Performs the removal only if the component exists
void RemoveSandboxDebugDrawComponent(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Component component,
	ECSEngine::ECS_COMPONENT_TYPE component_type
);

// -------------------------------------------------------------------------------------------------------------

// Returns true if the simulation was successful, else false. It prints the according error messages inside
// The bool protect_resources tells the function whether or not it should protect GPU resources and resource manager entries.
// It should be off if this is done beforehand, else let it true.
bool RunSandboxWorld(EditorState* editor_state, unsigned int sandbox_index, bool protect_resources, bool is_step = false);

// -------------------------------------------------------------------------------------------------------------

// Returns true if all sandboxes that want to be stepped using the general button managed to perform their step
// Else it returns false
bool RunSandboxWorlds(EditorState* editor_state, bool is_step = false);

// -------------------------------------------------------------------------------------------------------------

// Return true to early exit. You have the option when iterating over all sandboxes whether or not it
// should exclude temporary sandboxes
// Returns true if it early exited, else false
template<bool early_exit = false, typename Functor>
bool SandboxAction(const EditorState* editor_state, unsigned int sandbox_index, Functor&& functor, bool exclude_temporary_sandboxes = false) {
	if (sandbox_index == -1) {
		unsigned int count = GetSandboxCount(editor_state, exclude_temporary_sandboxes);
		for (unsigned int index = 0; index < count; index++) {
			if constexpr (early_exit) {
				if (functor(index)) {
					return true;
				}
			}
			else {
				functor(index);
			}
		}
	}
	else {
		if constexpr (early_exit) {
			return functor(sandbox_index);
		}
		else {
			functor(sandbox_index);
		}
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeded. On error it will print an error message
bool SaveSandboxRuntimeSettings(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeded. Can optionally specify an error message to be filled
bool SaveRuntimeSettings(
	const EditorState* editor_state, 
	ECSEngine::Stream<wchar_t> filename,
	const ECSEngine::WorldDescriptor* descriptor,
	ECSEngine::CapacityStream<char>* error_message = nullptr
);

// -------------------------------------------------------------------------------------------------------------

// If the viewport is specified it will override the selection based on the sandbox state
void SetSandboxSceneDirty(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

// -------------------------------------------------------------------------------------------------------------

// Doesn't save to the file after the change. Must manually call the save function
void SetSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, const ECSEngine::WorldDescriptor& descriptor);

// -------------------------------------------------------------------------------------------------------------

void SetSandboxGraphicsTextures(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

// The slot index needs to be obtained through the GetSandboxVirtualEntitySlots
void SetSandboxVirtualEntitySlotType(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int slot_index,
	EditorSandboxEntitySlot slot
);

// -------------------------------------------------------------------------------------------------------------

bool ShouldSandboxRecomputeVirtualEntitySlots(
	const EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

// This will add an event to increment the counter in order to be picked up by all systems
// next frame
void SignalSandboxSelectedEntitiesCounter(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// This will add an event to increment the counter in order to be picked up by all systems
// next frame
void SignalSandboxVirtualEntitiesSlotsCounter(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it managed to launch it, else false (it can fail, for example, if the scheduling order could not be created)
bool StartSandboxWorld(EditorState* editor_state, unsigned int sandbox_index, bool disable_error_message = false);

// -------------------------------------------------------------------------------------------------------------

// Returns true if all sandboxes that wanted to be run successfully started, else false
// This function checks to see if the sandbox wants to be run using the global button
// And starts it if it is in accordance. Can optionally allow the start to start those sandboxes
// That are paused only
bool StartSandboxWorlds(EditorState* editor_state, bool only_paused = false);

// -------------------------------------------------------------------------------------------------------------

// This function will start any sandbox that respond to the run button and are in the scene
// And will stop any sandbox that is in the running or paused state
bool StartEndSandboxWorld(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Returns true if the runtime settings where changed from outside
bool UpdateSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if the runtime settings where changed from outside
bool UpdateRuntimeSettings(const EditorState* editor_state, ECSEngine::Stream<wchar_t> filename, size_t* last_write);

// -------------------------------------------------------------------------------------------------------------

// This is a counter, not a boolean, so it can be called multiple times and it is atomic.
ECS_INLINE void UnlockSandbox(EditorState* editor_state, unsigned int sandbox_index) {
	GetSandbox(editor_state, sandbox_index)->locked_count.fetch_sub(1, ECS_RELAXED);
}

// -------------------------------------------------------------------------------------------------------------

// Waits until the given sandbox becomes unlocked. If the sandbox index is -1 then it will wait for all sandboxes
void WaitSandboxUnlock(const EditorState* editor_state, unsigned int sandbox_index = -1);

// -------------------------------------------------------------------------------------------------------------

void TickSandboxes(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

void TickUpdateSandboxHIDInputs(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Runs all the worlds of active sandboxes
void TickSandboxRuntimes(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

void TickSandboxUpdateMasterButtons(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------