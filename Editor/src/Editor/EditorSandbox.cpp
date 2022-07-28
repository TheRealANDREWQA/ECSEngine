#include "editorpch.h"
#include "EditorSandbox.h"
#include "EditorState.h"
#include "../Modules/Module.h"
#include "../Project/ProjectFolders.h"
#include "../Modules/ModuleSettings.h"

using namespace ECSEngine;

#define MODULE_FILE_SETTING_EXTENSION L".config"
//#define MODULE_FILE_MANUAL_SETTING_EXTENSION L".mconfig"

#define MODULE_ALLOCATOR_SIZE ECS_KB * 32
#define MODULE_ALLOCATOR_POOL_COUNT 1024
#define MODULE_ALLOCATOR_BACKUP_SIZE ECS_KB * 128

EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_index) {
	return editor_state->sandboxes.buffer + sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

void AddSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, EDITOR_MODULE_CONFIGURATION module_configuration)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	unsigned int sandbox_module_index = sandbox->modules_in_use.ReserveNewElement();
	sandbox->modules_in_use.size++;
	EditorSandboxModule* sandbox_module = sandbox->modules_in_use.buffer + sandbox_module_index;
	sandbox_module->module_index = module_index;
	sandbox_module->module_configuration = module_configuration;

	sandbox_module->settings_name.buffer = nullptr;
	sandbox_module->settings_name.size = 0;

	// Create the allocator
	sandbox_module->settings_allocator = MemoryManager(MODULE_ALLOCATOR_SIZE, MODULE_ALLOCATOR_POOL_COUNT, MODULE_ALLOCATOR_BACKUP_SIZE, sandbox->GlobalMemoryManager());
}

// -----------------------------------------------------------------------------------------------------------------------------

bool AreSandboxModulesCompiled(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	for (size_t index = 0; index < sandbox->modules_in_use.size; index++) {
		if (UpdateModuleLastWrite(editor_state, sandbox->modules_in_use[index].module_index)) {
			return false;
		}
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, ECSEngine::Stream<wchar_t> settings_name)
{
	unsigned int sandbox_module_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(sandbox_module_index != -1);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	EditorSandboxModule* sandbox_module = sandbox->modules_in_use.buffer + module_index;

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_settings_path, 512);

	if (sandbox_module->settings_name.buffer != nullptr) {
		sandbox_module->settings_allocator.Deallocate(sandbox_module->settings_name.buffer);
	}
	
	// Change the path
	sandbox_module->settings_name = function::StringCopy(sandbox_module->Allocator(), settings_name);
}

// -----------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleConfiguration(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION module_configuration
) {
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);
	editor_state->sandboxes[sandbox_index].modules_in_use[in_stream_index].module_configuration = module_configuration;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (module_index != -1) {
		unsigned int in_stream_module_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
		ECS_ASSERT(in_stream_module_index != -1);

		EditorSandboxModule* sandbox_module = sandbox->modules_in_use.buffer + in_stream_module_index;

		ECS_STACK_CAPACITY_STREAM(wchar_t, settings_name, 512);
		if (sandbox_module->settings_name.buffer != nullptr) {
			settings_name.Copy(sandbox_module->settings_name);
		}

		sandbox_module->settings_allocator.Clear();

		// Make the streams nullptr for easier debugging
		if (settings_name.size > 0) {
			sandbox_module->settings_name = function::StringCopy(sandbox_module->Allocator(), settings_name);
		}
		else {
			sandbox_module->settings_name = { nullptr, 0 };
		}
		sandbox_module->reflected_settings = { nullptr, 0 };
	}
	else {
		// A little bit of overhead, but it is still low
		for (size_t index = 0; index < sandbox->modules_in_use.size; index++) {
			ClearSandboxModuleSettings(editor_state, sandbox_index, sandbox->modules_in_use[index].module_index);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->sandbox_world.entity_manager->CopyOther(&sandbox->scene_entities);
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopyCachedResourcesIntoSandbox(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->sandbox_world.resource_manager->InheritResources(editor_state->cache_resource_manager);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool CompileSandboxModules(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	/*unsigned int module_count = sandbox->modules_in_use.size;
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(EDITOR_MODULE_CONFIGURATION, compile_configurations, module_count);
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned char, compile_)*/

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void CreateSandbox(EditorState* editor_state) {
	unsigned int sandbox_index = editor_state->sandboxes.ReserveNewElement();
	EditorSandbox* sandbox = editor_state->sandboxes.buffer + sandbox_index;
	editor_state->sandboxes.size++;

	sandbox->world_descriptor = GetDefaultWorldDescriptor();
	sandbox->world_descriptor.mouse = editor_state->Mouse();
	sandbox->world_descriptor.keyboard = editor_state->Keyboard();

	// Create a sandbox allocator - a global one - such that it can accomodate the default
	// entity manager requirements
	GlobalMemoryManager* allocator = (GlobalMemoryManager*)editor_state->editor_allocator->Allocate(sizeof(GlobalMemoryManager));
	*allocator = GlobalMemoryManager(
		sandbox->world_descriptor.entity_manager_memory_size + ECS_MB * 10,
		1024,
		sandbox->world_descriptor.entity_manager_memory_new_allocation_size + ECS_MB * 2
	);

	AllocatorPolymorphic sandbox_allocator = GetAllocatorPolymorphic(allocator);

	// Initialize the textures to nullptr - wait for the UI window to tell the sandbox what area it covers
	sandbox->viewport_render_target.view = nullptr;
	sandbox->viewport_texture.view = nullptr;
	sandbox->viewport_texture_depth.view = nullptr;
	sandbox->task_dependencies.elements.Initialize(sandbox_allocator, 0);

	sandbox->modules_in_use.Initialize(sandbox_allocator, 0);
	sandbox->database = AssetDatabaseReference(&editor_state->asset_database, sandbox_allocator);

	// Create a graphics object that will inherit all the resources
	Graphics* sandbox_graphics = (Graphics*)allocator->Allocate(sizeof(Graphics));
	MemoryManager* sandbox_graphics_allocator = (MemoryManager*)allocator->Allocate(sizeof(MemoryManager));
	*sandbox_graphics_allocator = DefaultGraphicsAllocator(allocator);
	*sandbox_graphics = Graphics(editor_state->CacheGraphics(), nullptr, nullptr, sandbox_graphics_allocator);
	sandbox->world_descriptor.graphics = sandbox_graphics;

	// Create the scene entity manager
	sandbox->scene_entities = CreateEntityManagerWithPool(
		sandbox->world_descriptor.entity_manager_memory_size,
		sandbox->world_descriptor.entity_manager_memory_pool_count,
		sandbox->world_descriptor.entity_manager_memory_new_allocation_size,
		sandbox->world_descriptor.entity_pool_power_of_two,
		allocator
	);

	// Create the sandbox world
	sandbox->sandbox_world = World(sandbox->world_descriptor);
	sandbox->copy_world_status.unlock();

	// Copy all the resources from the cache resource manager into the newly created resource manager
	CopyCachedResourcesIntoSandbox(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DestroySandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = editor_state->sandboxes.buffer + sandbox_index;

	// Evict all cached resources from the resource manager
	sandbox->sandbox_world.resource_manager->EvictResourcesFrom(editor_state->cache_resource_manager);

	// Can safely now destroy the runtime world
	DestroyWorld(&sandbox->sandbox_world);

	editor_state->sandboxes.RemoveSwapBack(sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DestroySandbox(EditorState* editor_state, unsigned int sandbox_index) {
	EditorSandbox* sandbox = editor_state->sandboxes.buffer + sandbox_index;
	
	// Destroy the reflected settings
	for (size_t index = 0; index < sandbox->modules_in_use.size; index++) {
		DestroyModuleSettings(
			editor_state,
			sandbox->modules_in_use[index].module_index,
			sandbox->modules_in_use[index].reflected_settings,
			index
		);
	}

	// Destroy the sandbox runtime
	DestroySandboxRuntime(editor_state, sandbox_index);

	// Free the global sandbox allocator
	sandbox->GlobalMemoryManager()->ReleaseResources();
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetSandboxModuleInStreamIndex(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	auto sandbox_modules = editor_state->sandboxes[sandbox_index].modules_in_use;
	for (unsigned int index = 0; index < sandbox_modules.size; index++) {
		if (sandbox_modules[index].module_index == module_index) {
			return index;
		}
	}
	return -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxModuleSettingsPath(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, CapacityStream<wchar_t>& path)
{
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);

	GetSandboxModuleSettingsPathByIndex(editor_state, sandbox_index, in_stream_index, path);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxModuleSettingsPathByIndex(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_module_index, ECSEngine::CapacityStream<wchar_t>& path)
{
	unsigned int module_index = editor_state->sandboxes[sandbox_index].modules_in_use[in_stream_module_index].module_index;
	GetModuleSettingsFilePath(editor_state, module_index, editor_state->sandboxes[sandbox_index].modules_in_use[in_stream_module_index].settings_name, path);
}

// -----------------------------------------------------------------------------------------------------------------------------

void InitializeSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = editor_state->sandboxes.buffer + sandbox_index;

	// The graphics object must be manually recreated
	*sandbox->world_descriptor.graphics = Graphics(
		editor_state->cache_graphics,
		sandbox->viewport_render_target,
		sandbox->viewport_texture_depth,
		sandbox->world_descriptor.graphics->m_allocator
	);

	// Recreate the world
	sandbox->sandbox_world = World(sandbox->world_descriptor);

	// Place the cached resources into the resource manager
	// It will also copy any GPU cached resources to the new GPU instance
	CopyCachedResourcesIntoSandbox(editor_state, sandbox_index);

	// Copy the entities
	CopySceneEntitiesIntoSandboxRuntime(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	EditorSandboxModule* sandbox_module = editor_state->sandboxes[sandbox_index].modules_in_use.buffer + in_stream_index;

	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&sandbox_module->settings_allocator);
	ECS_STACK_CAPACITY_STREAM(EditorModuleReflectedSetting, settings, 64);
	AllocateModuleSettings(editor_state, module_index, settings, allocator);

	if (sandbox_module->settings_name.buffer != nullptr) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, settings_path, 512);
		GetSandboxModuleSettingsPathByIndex(editor_state, sandbox_index, in_stream_index, settings_path);
		if (!LoadModuleSettings(editor_state, module_index, settings_path, settings, allocator)) {
			// Copy the settings path such that we can reset after we deallocate the allocator
			settings_path.Copy(sandbox_module->settings_name);

			sandbox_module->settings_allocator.Clear();
			sandbox_module->settings_name = function::StringCopy(sandbox_module->Allocator(), settings_path);
			return false;
		}
	}
	else {
		// Set the default values
		SetModuleDefaultSettings(editor_state, module_index, settings);
	}

	// Now copy the stream into the sandbox module
	sandbox_module->reflected_settings.Initialize(&sandbox_module->settings_allocator, settings.size);
	sandbox_module->reflected_settings.Copy(settings);
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ReinitializeSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	DestroySandboxRuntime(editor_state, sandbox_index);
	InitializeSandboxRuntime(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index) {
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);

	EditorSandboxModule* sandbox_module = editor_state->sandboxes[sandbox_index].modules_in_use.buffer + in_stream_index;
	// Deallocate the reflected settings allocator
	sandbox_module->settings_allocator.Free();
}

// -----------------------------------------------------------------------------------------------------------------------------