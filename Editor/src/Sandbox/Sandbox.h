#pragma once
#include "SandboxAccessor.h"
#include "ECSEngineRendering.h"
#include "ECSEngineResources.h"
#include "editorpch.h"
#include "../Modules/ModuleDefinition.h"

struct EditorState;

// -------------------------------------------------------------------------------------------------------------

void BindSandboxGraphicsSceneInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

// The settings name needs to be only the stem i.e. Settings - not the absolute path
// Returns true if it succeded in changing the settings. It can fail if the file doesn't exist or couldn't be deserialized
// If the settings_name is { nullptr, 0 }, it will use default values for the runtime descriptor and remove the settings name associated
bool ChangeSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> settings_name);

// -------------------------------------------------------------------------------------------------------------

void ClearSandboxTaskScheduler(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Can delay the initialization of the runtime for later on. It must be done manually to a call with InitializeSandboxRuntime
void CreateSandbox(EditorState* editor_state, bool initialize_runtime = true);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeeded in creating the scheduling order for the sandbox. Can disable the automatic
// printing of the error message if it fails.
bool ConstructSandboxSchedulingOrder(EditorState* editor_state, unsigned int sandbox_index, bool disable_error_message = false);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeeded in creating the scheduling order for the sandbox using the given mask for the modules to schedule.
// The module mask must index into the sandbox editor module indices, not into ProjectModules
// Can disable the automatic printing of the error message if it fails.
bool ConstructSandboxSchedulingOrder(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::Stream<unsigned int> module_indices, 
	bool disable_error_message = false
);

// -------------------------------------------------------------------------------------------------------------

void DestroySandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Can optionally disable the wait for the unlocking - by default you should leave this on
// unless you have a very specific reason to ignore the value
void DestroySandbox(EditorState* editor_state, unsigned int sandbox_index, bool wait_unlocking = true);

// -------------------------------------------------------------------------------------------------------------

void DisableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

// -------------------------------------------------------------------------------------------------------------

void EnableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

// -------------------------------------------------------------------------------------------------------------

// Returns -1 if the entity is not selected
unsigned int FindSandboxSelectedEntityIndex(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Entity entity
);

// -------------------------------------------------------------------------------------------------------------

// Returns -1 if it doesn't find slot with the given value
ECSEngine::Entity FindSandboxUnusedEntitySlot(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_ENTITY_SLOT slot_type,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

// Returns EDITOR_SANDBOX_ENTITY_SLOT_COUNT if the entity could not be found
// It will assert that the slot was assigned beforehand
EDITOR_SANDBOX_ENTITY_SLOT FindSandboxUnusedEntitySlotType(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

// Checks to see that the textures are actually created before freeing
// If the viewport is left unspecified, it will call this function for all viewports
void FreeSandboxRenderTextures(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

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

ECSEngine::RenderTargetView GetSandboxInstancedFramebuffer(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::DepthStencilView GetSandboxInstancedDepthFramebuffer(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::Stream<ECSEngine::Entity> GetSandboxSelectedEntities(
	const EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

// Fills in entity slots that are not yet used in the runtime
// or somewhere else in the editor. Returns an index that needs to be used
// to set the transform type afterwards with the function SetSandboxUnusedEntitySlotType
unsigned int GetSandboxUnusedEntitySlots(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<ECSEngine::Entity> entities,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

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
	ECSEngine::Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

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
void LockSandbox(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Launches the runtime for the selected runtime
// Prints an error message in case it couldn't do it
bool LaunchSandboxRuntime(EditorState* editor_state, unsigned int index);

// -------------------------------------------------------------------------------------------------------------

void PauseSandboxWorld(EditorState* editor_state, unsigned int index, bool wait_for_pause = true);

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
// Returns the current snapshot of the runtime graphics. It is allocated from the EditorAllocator()
ECSEngine::GraphicsResourceSnapshot RenderSandboxInitializeGraphics(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

// The snapshot must be given from the Initialize call
void RenderSandboxFinishGraphics(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::GraphicsResourceSnapshot snapshot, EDITOR_SANDBOX_VIEWPORT viewport);

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

void ResetSandboxUnusedEntities(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

void RunSandboxWorld(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Return true to early exit
template<bool early_exit = false, typename Functor>
void SandboxAction(const EditorState* editor_state, unsigned int sandbox_index, Functor&& functor) {
	if (sandbox_index == -1) {
		unsigned int count = editor_state->sandboxes.size;
		for (unsigned int index = 0; index < count; index++) {
			if constexpr (early_exit) {
				if (functor(index)) {
					break;
				}
			}
			else {
				functor(index);
			}
		}
	}
	else {
		functor(sandbox_index);
	}
}

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

// The slot index needs to be obtained through the GetSandboxUnusedEntitySlots
void SetSandboxUnusedEntitySlotType(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int slot_index,
	EDITOR_SANDBOX_ENTITY_SLOT slot_type,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

bool ShouldSandboxRecomputeEntitySlots(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

// This will add an event to increment the counter in order to be picked up by all systems
// next frame
void SignalSandboxSelectedEntitiesCounter(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// This will add an event to increment the counter in order to be picked up by all systems
// next frame
void SignalSandboxUnusedEntitiesSlotsCounter(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

// -------------------------------------------------------------------------------------------------------------

// Returns true if the runtime settings where changed from outside
bool UpdateSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if the runtime settings where changed from outside
bool UpdateRuntimeSettings(const EditorState* editor_state, ECSEngine::Stream<wchar_t> filename, size_t* last_write);

// -------------------------------------------------------------------------------------------------------------

// This is a counter, not a boolean, so it can be called multiple times and it is atomic.
void UnlockSandbox(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Waits until the given sandbox becomes unlocked. If the sandbox index is -1 then it will wait for all sandboxes
void WaitSandboxUnlock(const EditorState* editor_state, unsigned int sandbox_index = -1);

// -------------------------------------------------------------------------------------------------------------

void TickSandboxes(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------