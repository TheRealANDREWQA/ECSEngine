// ECS_REFLECT
#pragma once
#include "ECSEngineRendering.h"
#include "ECSEngineResources.h"
#include "editorpch.h"
#include "../Modules/ModuleDefinition.h"
#include "ECSEngineReflectionMacros.h"

struct EditorState;

#define EDITOR_SCENE_EXTENSION L".scene"

enum EDITOR_SANDBOX_STATE {
	EDITOR_SANDBOX_SCENE,
	EDITOR_SANDBOX_RUNNING,
	EDITOR_SANDBOX_PAUSED
};

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
	inline ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return (ECSEngine::GlobalMemoryManager*)modules_in_use.allocator.allocator;
	}

	inline EditorSandbox& operator = (const EditorSandbox& other) {
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

	ECS_FIELDS_END_REFLECT;

	EDITOR_SANDBOX_STATE run_state;
	bool is_scene_dirty;
	std::atomic<unsigned char> locked_count;

	size_t runtime_settings_last_write;
	ECSEngine::WorldDescriptor runtime_descriptor;
	ECSEngine::AssetDatabaseReference database;

	ECSEngine::ResourceView viewport_texture;
	ECSEngine::DepthStencilView viewport_texture_depth;
	ECSEngine::RenderTargetView viewport_render_target;
	ECSEngine::TaskScheduler task_dependencies;

	ECSEngine::EntityManager scene_entities;
	ECSEngine::World sandbox_world;
	ECSEngine::SpinLock copy_world_status;
};

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

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if it succeded, else false.
bool CompileSandboxModules(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Can delay the initialization of the runtime for later on. It must be done manually to a call with InitializeSandboxRuntime
void CreateSandbox(EditorState* editor_state, bool initialize_runtime = true);

// -------------------------------------------------------------------------------------------------------------

void DeactivateSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

void DeactivateSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

void DestroySandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// This does not check to see if the sandbox is locked - you must ensure that before calling this function
// that the sandbox is unlocked
void DestroySandbox(EditorState* editor_state, unsigned int index);

// -------------------------------------------------------------------------------------------------------------

EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

const EditorSandbox* GetSandbox(const EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns in which state the sandbox is currently in
EDITOR_SANDBOX_STATE GetSandboxState(const EditorState* editor_state, unsigned int sandbox_index);

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

ECSEngine::WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxGraphicsModules(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<unsigned int>& indices);

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

void RemoveSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

// The last argument must be the index of the module inside the modules stream to be removed
void RemoveSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

// All sandboxes will remove the given module from being used and deallocate any entities data stored
// that was coming from that module
void RemoveSandboxModuleForced(EditorState* editor_state, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

template<typename Functor>
void SandboxAction(const EditorState* editor_state, unsigned int sandbox_index, Functor&& functor) {
	if (sandbox_index == -1) {
		unsigned int count = editor_state->sandboxes.size;
		for (unsigned int index = 0; index < count; index++) {
			functor(index);
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

void SetSandboxSceneDirty(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Doesn't save to the file after the change. Must manually call the save function
void SetSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, const ECSEngine::WorldDescriptor& descriptor);

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