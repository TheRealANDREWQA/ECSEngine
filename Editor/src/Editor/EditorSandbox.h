// ECS_REFLECT
#pragma once
#include "ECSEngineRendering.h"
#include "ECSEngineResources.h"
#include "ECSEngineRuntime.h"
#include "editorpch.h"
#include "../Modules/ModuleDefinition.h"
#include "ECSEngineReflectionMacros.h"
#include "EditorSandboxTypes.h"

struct EditorState;

#define EDITOR_SCENE_EXTENSION L".scene"
#define EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT ECS_CONSTANT_REFLECT(8)

// -------------------------------------------------------------------------------------------------------------

struct ECS_REFLECT EditorSandboxModule {
	inline ECSEngine::AllocatorPolymorphic Allocator() {
		return ECSEngine::GetAllocatorPolymorphic(&settings_allocator);
	}

	ECS_FIELDS_START_REFLECT;

	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION module_configuration;
	bool is_deactivated;

	// These are needed for reflection of the settings
	ECSEngine::Stream<wchar_t> settings_name;

	ECSEngine::Stream<EditorModuleReflectedSetting> reflected_settings; ECS_SKIP_REFLECTION()
	ECSEngine::MemoryManager settings_allocator; ECS_SKIP_REFLECTION(static_assert(sizeof(ECSEngine::MemoryManager) == 48))

	ECS_FIELDS_END_REFLECT;
};

// -------------------------------------------------------------------------------------------------------------

struct ECS_REFLECT EditorSandbox {
	ECS_INLINE ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return (ECSEngine::GlobalMemoryManager*)modules_in_use.allocator.allocator;
	}

	ECS_INLINE EditorSandbox& operator = (const EditorSandbox& other) {
		memcpy(this, &other, sizeof(*this));
		return *this;
	}

	ECS_FIELDS_START_REFLECT;

	ECSEngine::ResizableStream<EditorSandboxModule> modules_in_use;

	// Stored as relative path from the assets folder
	ECSEngine::CapacityStream<wchar_t> scene_path;

	// The settings used for creating the ECS world
	ECSEngine::CapacityStream<wchar_t> runtime_settings;

	// When the play button is clicked, if this sandbox should run
	bool should_play;

	// When the pause button is clicked, if this sandbox should pause
	bool should_pause;

	// When the step button is clicked, if this sandbox should step
	bool should_step;

	ECSEngine::ECS_TRANSFORM_TOOL transform_tool;

	ECSEngine::CameraParametersFOV camera_parameters[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::OrientedPoint camera_saved_orientations[EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT];

	ECS_FIELDS_END_REFLECT;

	EDITOR_SANDBOX_STATE run_state;
	bool is_scene_dirty;
	std::atomic<unsigned char> locked_count;

	size_t runtime_settings_last_write;
	ECSEngine::WorldDescriptor runtime_descriptor;
	ECSEngine::AssetDatabaseReference database;

	ECSEngine::RenderDestination viewport_render_destination[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::ResourceView viewport_transferred_texture[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::RenderTargetView scene_viewport_instance_framebuffer;
	ECSEngine::DepthStencilView scene_viewport_depth_stencil_framebuffer;
	// These are set used to make calls to RenderSandbox ignore the request if the output is
	// not going to be visualized
	bool viewport_enable_rendering[EDITOR_SANDBOX_VIEWPORT_COUNT];
	
	// Indicate which axis of the transform tool is currently selected
	bool transform_tool_selected[3];

	ECSEngine::EntityManager scene_entities;
	ECSEngine::World sandbox_world;

	ECSEngine::ResizableStream<ECSEngine::Entity> selected_entities;
	unsigned char selected_entities_changed_counter;

	ECSEngine::ResizableStream<ECSEngine::Entity> unused_entities_slots[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::ResizableStream<EDITOR_SANDBOX_ENTITY_SLOT> unused_entity_slot_type[EDITOR_SANDBOX_VIEWPORT_COUNT];
	// These flags indicate when the unused slots need to be recomputed (after a clear/reset for example)
	bool unused_entities_slots_recompute[EDITOR_SANDBOX_VIEWPORT_COUNT];
};

// -------------------------------------------------------------------------------------------------------------

// When it is not playing it returns the scene entities manager, when playing the runtime one
const ECSEngine::EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_index);

// When it is not playing it returns the scene entities manager, when playing the runtime one
ECSEngine::EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void ActivateSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

void ActivateSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

void AddSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, EDITOR_MODULE_CONFIGURATION module_configuration);

// -------------------------------------------------------------------------------------------------------------

// Returns true when all the modules that are currently used by the sandbox are compiled
// Else false
bool AreSandboxModulesCompiled(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void BindSandboxGraphicsSceneInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, ECSEngine::Stream<wchar_t> settings_name);

// -------------------------------------------------------------------------------------------------------------

// The settings name needs to be only the stem i.e. Settings - not the absolute path
// Returns true if it succeded in changing the settings. It can fail if the file doesn't exist or couldn't be deserialized
// If the settings_name is { nullptr, 0 }, it will use default values for the runtime descriptor and remove the settings name associated
bool ChangeSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> settings_name);

// -------------------------------------------------------------------------------------------------------------

// The new scene needs to be the relative path from the assets folder. A special case is new_scene { nullptr, 0 } which means reset
// Returns true if the scene could be loaded with success. It will deserialize into a temporary entity manager and asset database
// If that succeeds, then it will copy into the sandbox allocator.
bool ChangeSandboxScenePath(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> new_scene);

// -------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleConfiguration(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION module_configuration
);

// -------------------------------------------------------------------------------------------------------------

// Clears the entity manager and empties the asset database reference
void ClearSandboxScene(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// It will deallocate the allocator but keep the path intact.
// If no module index is specified, it will clear all the modules
void ClearSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index = -1);

// -------------------------------------------------------------------------------------------------------------

void ClearSandboxTaskScheduler(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeded, else false.
bool CompileSandboxModules(EditorState* editor_state, unsigned int sandbox_index);

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

void DeactivateSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

void DeactivateSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

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

EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

const EditorSandbox* GetSandbox(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

EditorSandboxModule* GetSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

const EditorSandboxModule* GetSandboxModule(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

const EditorModuleInfo* GetSandboxModuleInfo(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

// Returns in which state the sandbox is currently in
EDITOR_SANDBOX_STATE GetSandboxState(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns which viewport is currently displayed by the scene window
EDITOR_SANDBOX_VIEWPORT GetSandboxActiveViewport(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns the viewport of the sandbox if the given viewport is COUNT or the viewport again else
EDITOR_SANDBOX_VIEWPORT GetSandboxViewportOverride(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::EntityManager* GetSandboxEntityManager(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

const ECSEngine::EntityManager* GetSandboxEntityManager(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);


// -------------------------------------------------------------------------------------------------------------

// Returns -1 if it doesn't find the module
unsigned int GetSandboxModuleInStreamIndex(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxModuleSettingsPath(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, ECSEngine::CapacityStream<wchar_t>& path);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxModuleSettingsPathByIndex(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	unsigned int in_stream_module_index, 
	ECSEngine::CapacityStream<wchar_t>& path
);

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

// Fills in the absolute path of the sandbox scene path
void GetSandboxScenePath(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::CapacityStream<wchar_t>& path
);

// -------------------------------------------------------------------------------------------------------------

unsigned int GetSandboxCount(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::OrientedPoint GetSandboxCameraPoint(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

ECSEngine::Camera GetSandboxSceneCamera(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxGraphicsModules(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<unsigned int>& indices);

// -------------------------------------------------------------------------------------------------------------

// Returns the index inside the modules stream of the sandbox of the graphics module. If it doesn't exist or
// there are multiple graphics modules it returns -1. If the fail reason is set, it will make it false if there
// are no graphics, else true since there are multiple modules
unsigned int GetSandboxGraphicsModule(const EditorState* editor_state, unsigned int sandbox_index, bool* multiple_modules = nullptr);

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

bool IsSandboxModuleDeactivated(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxModuleDeactivatedInStream(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if at least a task/process requested the sandbox to be locked
bool IsSandboxLocked(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns the given configuration of the module if it is being used, else COUNT to signal that it is not being used
EDITOR_MODULE_CONFIGURATION IsModuleUsedBySandbox(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxIndexValid(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if a sandbox references the given module info. If the configuration is COUNT, then it will return true
// if any configuration is used. If the running state is set to true, then it will return true if the sandbox is actually
// running/paused. If set to false, it will also report true when the sandbox is just in the scene state.
// If the dependent modules is specified, then it will fill in all the sandboxes that depend on that module (won't stop at the first occurence)
bool IsModuleInfoUsed(
	const EditorState* editor_state, 
	unsigned int module_index, 
	bool running_state, 
	EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT,
	ECSEngine::CapacityStream<unsigned int>* dependent_sandboxes = nullptr
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

bool IsSandboxViewportRendering(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxGizmoEntity(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	ECSEngine::Entity entity, 
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// -------------------------------------------------------------------------------------------------------------

// It will create the ReflectedSettings for that module and load the data from the file if a path is specified.
// If no path is specified, it will use the default values.
// It returns true if it succeded, else false. (it can fail only if it cannot the data from the file)
bool LoadSandboxModuleSettings(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int module_index = -1
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

bool LoadEditorSandboxFile(
	EditorState* editor_state
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

// Records the current position of the camera into the given slot
void RegisterSandboxCameraTransform(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int camera_index,
	EDITOR_SANDBOX_VIEWPORT viewport
);

// -------------------------------------------------------------------------------------------------------------

// If there is an out-of-date runtime settings then it will try to update it
void ReloadSandboxRuntimeSettings(
	EditorState* editor_state
);

// -------------------------------------------------------------------------------------------------------------

void RemoveSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

// The last argument must be the index of the module inside the modules stream to be removed
void RemoveSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

// All sandboxes will remove the given module from being used and deallocate any entities data stored
// that was coming from that module
void RemoveSandboxModuleForced(EditorState* editor_state, unsigned int module_index);

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

void RotateSandboxCamera(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::float3 rotation, EDITOR_SANDBOX_VIEWPORT viewport);


// -------------------------------------------------------------------------------------------------------------

// Defaults the values for the camera and saved positions
void ResetSandboxCameras(EditorState* editor_state, unsigned int sandbox_index);

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

// Returns true if it managed to save the sandbox scene, else false.
// If it succeeds it updates the is_dirty flag to be false.
bool SaveSandboxScene(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeded. On error it will print an error message
bool SaveEditorSandboxFile(const EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

// Determines the aspect ration of the camera given size of the textures
void SetSandboxCameraAspectRatio(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport);

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

void TranslateSandboxCamera(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::float3 translation, EDITOR_SANDBOX_VIEWPORT viewport);

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