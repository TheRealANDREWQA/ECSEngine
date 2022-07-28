#pragma once
#include "ECSEngineRendering.h"
#include "ECSEngineResources.h"
#include "editorpch.h"
#include "../Modules/ModuleDefinition.h"

#define MODULE_DEFAULT_SETTINGS_PATH L"Default.config"

struct EditorState;

// -------------------------------------------------------------------------------------------------------------

struct EditorSandboxModule {
	inline ECSEngine::AllocatorPolymorphic Allocator() {
		return ECSEngine::GetAllocatorPolymorphic(&settings_allocator);
	}

	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION module_configuration;

	// These are needed for reflection of the settings
	ECSEngine::Stream<wchar_t> settings_name;
	ECSEngine::Stream<EditorModuleReflectedSetting> reflected_settings;

	ECSEngine::MemoryManager settings_allocator;
};

// -------------------------------------------------------------------------------------------------------------

struct EditorSandbox {
	inline ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return (ECSEngine::GlobalMemoryManager*)modules_in_use.allocator.allocator;
	}

	ECSEngine::ResourceView viewport_texture;
	ECSEngine::DepthStencilView viewport_texture_depth;
	ECSEngine::RenderTargetView viewport_render_target;
	ECSEngine::TaskScheduler task_dependencies;

	ECSEngine::ResizableStream<EditorSandboxModule> modules_in_use;

	ECSEngine::AssetDatabaseReference database;
	ECSEngine::WorldDescriptor world_descriptor;

	ECSEngine::EntityManager scene_entities;
	ECSEngine::World sandbox_world;
	ECSEngine::SpinLock copy_world_status;
};

// -------------------------------------------------------------------------------------------------------------

void AddSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, EDITOR_MODULE_CONFIGURATION module_configuration);

// -------------------------------------------------------------------------------------------------------------

// Returns true when all the modules that are currently used by the sandbox are compiled
// Else false
bool AreSandboxModulesCompiled(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, ECSEngine::Stream<wchar_t> settings_name);

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

void CreateSandbox(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------

void DestroySandboxRuntime(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void DestroySandbox(EditorState* editor_state, unsigned int index);

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

void ReinitializeSandboxRuntime(
	EditorState* editor_state,
	unsigned int sandbox_index
);

// -------------------------------------------------------------------------------------------------------------

void RemoveSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------