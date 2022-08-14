// ECS_REFLECT
#pragma once
#include "ECSEngineRendering.h"
#include "ECSEngineResources.h"
#include "editorpch.h"
#include "../Modules/ModuleDefinition.h"
#include "ECSEngineReflectionMacros.h"

#define MODULE_DEFAULT_SETTINGS_PATH L"Default.config"

struct EditorState;

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

	ECSEngine::Stream<EditorModuleReflectedSetting> reflected_settings; ECS_SKIP_REFLECTION(static_assert(sizeof(ECSEngine::Stream<EditorModuleReflectedSetting>) == 16))
	ECSEngine::MemoryManager settings_allocator; ECS_SKIP_REFLECTION(static_assert(sizeof(ECSEngine::MemoryManager) == 48))

	ECS_FIELDS_END_REFLECT;
};

// -------------------------------------------------------------------------------------------------------------

struct ECS_REFLECT EditorSandbox {
	inline ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return (ECSEngine::GlobalMemoryManager*)modules_in_use.allocator.allocator;
	}

	ECS_FIELDS_START_REFLECT;

	ECSEngine::ResizableStream<EditorSandboxModule> modules_in_use;

	// The settings used for creating the ECS world
	ECSEngine::CapacityStream<wchar_t> runtime_settings;
	ECSEngine::AssetDatabaseReference database;

	ECS_FIELDS_END_REFLECT;

	size_t runtime_settings_last_write;
	ECSEngine::WorldDescriptor runtime_descriptor;

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

// The handle must be the one given from the database
void AddSandboxAssetReference(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECSEngine::ECS_ASSET_TYPE asset_type);

// -------------------------------------------------------------------------------------------------------------

// This will take a handle from the database and assign it to this sandbox
void AddSandboxAssetReference(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<char> asset_name_or_path, ECSEngine::ECS_ASSET_TYPE asset_type);

// -------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, ECSEngine::Stream<wchar_t> settings_name);

// -------------------------------------------------------------------------------------------------------------

// The settings name needs to be only the stem i.e. Settings - not the absolute path
// Returns true if it succeded in changing the settings. It can fail if the file doesn't exist or couldn't be deserialized
// If the settings_name is { nullptr, 0 }, it will use default values for the runtime descriptor and remove the settings name associated
bool ChangeSandboxRuntimeSettings(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> settings_name);

// -------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleConfiguration(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION module_configuration
);

// -------------------------------------------------------------------------------------------------------------

// It will deallocate the allocator but keep the path intact.
// If no module index is specified, it will clear all the modules
void ClearSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index = -1);

// -------------------------------------------------------------------------------------------------------------

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void CopyCachedResourcesIntoSandbox(EditorState* editor_state, unsigned int sandbox_index);

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

void DestroySandbox(EditorState* editor_state, unsigned int index);

// -------------------------------------------------------------------------------------------------------------

EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

const EditorSandbox* GetSandbox(const EditorState* editor_state, unsigned int sandbox_index);

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

ECSEngine::WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxGraphicsModules(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<unsigned int>& indices);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxModuleDeactivated(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxModuleDeactivatedInStream(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

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

// Returns true if it succeded. On error it will print an error message
bool SaveEditorSandboxFile(const EditorState* editor_state);

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