#include "editorpch.h"
#include "EditorSandbox.h"
#include "EditorState.h"
#include "EditorEvent.h"
#include "EditorPalette.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Modules/Module.h"
#include "../Project/ProjectFolders.h"
#include "../Modules/ModuleSettings.h"

#include "EditorScene.h"
#include "ECSEngineSerializationHelpers.h"
#include "ECSEngineAssets.h"
#include "ECSEngineECSRuntimeResources.h"

// The UI needs to be included because we need to notify it when we destroy a sandbox
#include "../UI/Game.h"
#include "../UI/Scene.h"
#include "../UI/EntitiesUI.h"
#include "../UI/Common.h"

using namespace ECSEngine;

#define MODULE_FILE_SETTING_EXTENSION L".config"
//#define MODULE_FILE_MANUAL_SETTING_EXTENSION L".mconfig"

#define MODULE_ALLOCATOR_SIZE ECS_KB * 32
#define MODULE_ALLOCATOR_POOL_COUNT 1024
#define MODULE_ALLOCATOR_BACKUP_SIZE ECS_KB * 128

#define RUNTIME_SETTINGS_STRING_CAPACITY (128)
#define SCENE_PATH_STRING_CAPACITY (128)

#define SANDBOX_FILE_HEADER_VERSION (0)
#define SANDBOX_FILE_MAX_SANDBOXES (16)

#define LAZY_EVALUATION_RUNTIME_SETTINGS 500

struct SandboxFileHeader {
	size_t version;
	size_t count;
};

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxScenePathImpl(const EditorState* editor_state, Stream<wchar_t> scene_path, CapacityStream<wchar_t>& absolute_path) {
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStream(scene_path);
	absolute_path[absolute_path.size] = L'\0';
	absolute_path.AssertCapacity();
}

// -----------------------------------------------------------------------------------------------------------------------------

EditorSandbox* GetSandbox(EditorState* editor_state, unsigned int sandbox_index) {
	return editor_state->sandboxes.buffer + sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

const EditorSandbox* GetSandbox(const EditorState* editor_state, unsigned int sandbox_index) {
	return editor_state->sandboxes.buffer + sandbox_index;
}

// -----------------------------------------------------------------------------------------------------------------------------

EditorSandboxModule* GetSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index)
{
	return &GetSandbox(editor_state, sandbox_index)->modules_in_use[in_stream_index];
}

// -----------------------------------------------------------------------------------------------------------------------------

const EditorSandboxModule* GetSandboxModule(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index)
{
	return &GetSandbox(editor_state, sandbox_index)->modules_in_use[in_stream_index];
}

// -----------------------------------------------------------------------------------------------------------------------------

const EditorModuleInfo* GetSandboxModuleInfo(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index)
{
	const EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_index);
	return GetModuleInfo(editor_state, sandbox_module->module_index, sandbox_module->module_configuration);
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_STATE GetSandboxState(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->run_state;
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_SANDBOX_VIEWPORT GetSandboxCurrentViewport(const EditorState* editor_state, unsigned int sandbox_index)
{
	EDITOR_SANDBOX_STATE state = GetSandboxState(editor_state, sandbox_index);
	if (state == EDITOR_SANDBOX_SCENE) {
		return EDITOR_SANDBOX_VIEWPORT_SCENE;
	}
	else if (state == EDITOR_SANDBOX_PAUSED || state == EDITOR_SANDBOX_RUNNING) {
		return EDITOR_SANDBOX_VIEWPORT_RUNTIME;
	}
	else {
		ECS_ASSERT(false);
		return EDITOR_SANDBOX_VIEWPORT_COUNT;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxRuntimePreinitialized(const EditorState* editor_state, unsigned int sandbox_index) {
	return GetSandbox(editor_state, sandbox_index)->runtime_descriptor.graphics != nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ActivateSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);
	ActivateSandboxModuleInStream(editor_state, sandbox_index, module_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void ActivateSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index)
{
	editor_state->sandboxes[sandbox_index].modules_in_use[in_stream_index].is_deactivated = false;
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
	sandbox_module->is_deactivated = false;

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
		if (!sandbox->modules_in_use[index].is_deactivated && UpdateModuleLastWrite(editor_state, sandbox->modules_in_use[index].module_index)) {
			return false;
		}
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void BindSandboxGraphicsSceneInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SetSandboxCameraAspectRatio(editor_state, sandbox_index, viewport);
	Camera camera = Camera(sandbox->camera_parameters);
	camera.translation.z -= 5.0f;

	SystemManager* system_manager = sandbox->sandbox_world.system_manager;
	SetRuntimeCamera(system_manager, &camera);
	SetEditorRuntime(system_manager);
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		SetEditorRuntimeSelectColor(system_manager, EDITOR_SELECT_COLOR);
		SetEditorRuntimeSelectedEntities(system_manager, sandbox->selected_entities);
		SetEditorRuntimeTransformTool(system_manager, sandbox->transform_tool);
		GraphicsBoundViews instanced_views;
		instanced_views.depth_stencil = sandbox->scene_viewport_depth_stencil_framebuffer;
		instanced_views.target = sandbox->scene_viewport_instance_framebuffer;
		SetEditorRuntimeInstancedFramebuffer(system_manager, &instanced_views);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, ECSEngine::Stream<wchar_t> settings_name)
{
	unsigned int sandbox_module_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(sandbox_module_index != -1);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	EditorSandboxModule* sandbox_module = sandbox->modules_in_use.buffer + module_index;

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_settings_path, 512);

	if (sandbox_module->settings_name.size > 0) {
		sandbox_module->settings_allocator.Deallocate(sandbox_module->settings_name.buffer);
	}
	
	// Change the path
	if (settings_name.size > 0) {
		sandbox_module->settings_name = function::StringCopy(sandbox_module->Allocator(), settings_name);
	}
	else {
		sandbox_module->settings_name = { nullptr, 0 };
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
		sandbox->runtime_settings.Copy(settings_name);
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

bool ChangeSandboxScenePath(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> new_scene)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	
	ClearSandboxScene(editor_state, sandbox_index);
	sandbox->scene_path.size = 0;

	if (new_scene.size == 0) {
		// Setting the components needs to be done right before exiting
		editor_state->editor_components.SetManagerComponents(&sandbox->scene_entities);
		return true;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetSandboxScenePathImpl(editor_state, new_scene, absolute_path);

	bool success = LoadEditorSceneCore(editor_state, sandbox_index, absolute_path);
	if (success) {
		// Copy the contents
		sandbox->scene_path.Copy(new_scene);
		sandbox->is_scene_dirty = false;

		// Load the assets
		LoadSandboxAssets(editor_state, sandbox_index);
	}

	// If the load failed, the scene will be reset with an empty value
	editor_state->editor_components.SetManagerComponents(&sandbox->scene_entities);

	return success;
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

void ClearSandboxScene(EditorState* editor_state, unsigned int sandbox_index)
{
	// Unload the assets currently used by this sandbox and reset the scene entities
	UnloadSandboxAssets(editor_state, sandbox_index);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->scene_entities.Reset();
	sandbox->database.Reset();
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

void ClearSandboxTaskScheduler(EditorState* editor_state, unsigned int sandbox_index)
{
	GetSandbox(editor_state, sandbox_index)->sandbox_world.task_scheduler->Reset();
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySceneEntitiesIntoSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->sandbox_world.entity_manager->CopyOther(&sandbox->scene_entities);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool CompileSandboxModules(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	unsigned int module_count = sandbox->modules_in_use.size;
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(EDITOR_MODULE_CONFIGURATION, compile_configurations, module_count);
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, compile_indices, module_count);

	// Only compile the modules which are not deactivated
	module_count = 0;

	for (size_t index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated) {
			compile_indices[module_count] = sandbox->modules_in_use[index].module_index;
			compile_configurations[module_count++] = sandbox->modules_in_use[index].module_configuration;
		}
	}

	compile_indices.size = module_count;
	return BuildModulesAndLoad(editor_state, compile_indices, compile_configurations.buffer);
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

	sandbox->run_state = EDITOR_SANDBOX_SCENE;
	sandbox->locked_count = 0;

	// Initialize the runtime settings string
	sandbox->runtime_descriptor = GetDefaultWorldDescriptor();
	sandbox->runtime_descriptor.mouse = editor_state->Mouse();
	sandbox->runtime_descriptor.keyboard = editor_state->Keyboard();

	sandbox->runtime_settings.Initialize(editor_state->EditorAllocator(), 0, RUNTIME_SETTINGS_STRING_CAPACITY);
	sandbox->runtime_settings_last_write = 0;

	sandbox->scene_path.Initialize(editor_state->EditorAllocator(), 0, SCENE_PATH_STRING_CAPACITY);

	ResetSandboxCamera(editor_state, sandbox_index);

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

void DeactivateSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);
	DeactivateSandboxModuleInStream(editor_state, sandbox_index, module_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeactivateSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index)
{
	editor_state->sandboxes[sandbox_index].modules_in_use[in_stream_index].is_deactivated = true;
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
	GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport] = false;
}

// -----------------------------------------------------------------------------------------------------------------------------

void EnableSandboxViewportRendering(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport] = true;
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
		if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
			runtime_graphics->FreeView(sandbox->scene_viewport_instance_framebuffer);
			runtime_graphics->FreeView(sandbox->scene_viewport_depth_stencil_framebuffer);
		}
	}

	runtime_graphics->GetContext()->ClearState();
	runtime_graphics->GetContext()->Flush();
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

void GetSandboxScenePath(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<wchar_t>& path)
{
	GetSandboxScenePathImpl(editor_state, GetSandbox(editor_state, sandbox_index)->scene_path, path);
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetSandboxCount(const EditorState* editor_state)
{
	return editor_state->sandboxes.size;
}

// -----------------------------------------------------------------------------------------------------------------------------

WorldDescriptor* GetSandboxWorldDescriptor(EditorState* editor_state, unsigned int sandbox_index)
{
	return &GetSandbox(editor_state, sandbox_index)->runtime_descriptor;
}

// -----------------------------------------------------------------------------------------------------------------------------

OrientedPoint GetSandboxCameraPoint(const EditorState* editor_state, unsigned int sandbox_index)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return { sandbox->camera_parameters.translation, sandbox->camera_parameters.rotation };
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxGraphicsModules(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>& indices)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	const ProjectModules* modules = editor_state->project_modules;
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (modules->buffer[sandbox->modules_in_use[index].module_index].is_graphics_module) {
			indices.Add(index);
		}
	}

	indices.AssertCapacity();
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int GetSandboxGraphicsModule(const EditorState* editor_state, unsigned int sandbox_index, bool* multiple_modules)
{
	ECS_STACK_CAPACITY_STREAM(unsigned int, indices, 512);
	GetSandboxGraphicsModules(editor_state, sandbox_index, indices);

	if (indices.size == 1) {
		return indices[0];
	}

	if (multiple_modules != nullptr) {
		*multiple_modules = indices.size > 0;
	}
	return -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxModuleDeactivated(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);
	return IsSandboxModuleDeactivatedInStream(editor_state, sandbox_index, in_stream_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxModuleDeactivatedInStream(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index)
{
	return GetSandbox(editor_state, sandbox_index)->modules_in_use[in_stream_index].is_deactivated;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxLocked(const EditorState* editor_state, unsigned int sandbox_index)
{
	return GetSandbox(editor_state, sandbox_index)->locked_count.load(ECS_RELAXED) > 0;
}

// -----------------------------------------------------------------------------------------------------------------------------

EDITOR_MODULE_CONFIGURATION IsModuleUsedBySandbox(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (sandbox->modules_in_use[index].module_index == module_index) {
			return sandbox->modules_in_use[index].module_configuration;
		}
	}
	return EDITOR_MODULE_CONFIGURATION_COUNT;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsSandboxIndexValid(const EditorState* editor_state, unsigned int sandbox_index)
{
	return sandbox_index < GetSandboxCount(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsModuleInfoUsed(
	const EditorState* editor_state, 
	unsigned int module_index, 
	bool running_state, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	CapacityStream<unsigned int>* sandbox_indices
)
{
	if (sandbox_indices != nullptr) {
		if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
			bool is_used = false;
			for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
				is_used |= IsModuleInfoUsed(editor_state, module_index, running_state, (EDITOR_MODULE_CONFIGURATION)configuration_index, sandbox_indices);
			}
			return is_used;
		}
		else {
			bool is_used = false;
			unsigned int sandbox_count = editor_state->sandboxes.size;
			for (unsigned int index = 0; index < sandbox_count; index++) {
				EDITOR_MODULE_CONFIGURATION sandbox_configuration = IsModuleUsedBySandbox(editor_state, index, module_index);
				if (sandbox_configuration == configuration) {
					EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, index);
					if (running_state) {
						if (sandbox_state == EDITOR_SANDBOX_RUNNING || sandbox_state == EDITOR_SANDBOX_PAUSED) {
							is_used = true;
							sandbox_indices->AddAssert(index);
						}
					}
					else {
						is_used = true;
						sandbox_indices->AddAssert(index);
					}
				}
			}
			return is_used;
		}
	}
	else {
		if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
			for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
				if (IsModuleInfoUsed(editor_state, module_index, (EDITOR_MODULE_CONFIGURATION)configuration_index)) {
					return true;
				}
			}
		}
		else {
			unsigned int sandbox_count = editor_state->sandboxes.size;
			for (unsigned int index = 0; index < sandbox_count; index++) {
				EDITOR_MODULE_CONFIGURATION sandbox_configuration = IsModuleUsedBySandbox(editor_state, index, module_index);
				if (sandbox_configuration == configuration) {
					EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, index);
					if (running_state) {
						if (sandbox_state == EDITOR_SANDBOX_RUNNING || sandbox_state == EDITOR_SANDBOX_PAUSED) {
							return true;
						}
					}
					else {
						return true;
					}
				}
			}
		}
		return false;
	}
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

bool IsSandboxViewportRendering(const EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return GetSandbox(editor_state, sandbox_index)->viewport_enable_rendering[viewport];
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

bool LoadEditorSandboxFile(EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, sandbox_file_path, 512);
	GetProjectSandboxFile(editor_state, sandbox_file_path);

	const Reflection::ReflectionManager* reflection_manager = editor_state->ReflectionManager();

	const size_t STACK_ALLOCATION_CAPACITY = ECS_KB * 128;
	const size_t BACKUP_CAPACITY = ECS_MB;
	void* stack_allocation = ECS_STACK_ALLOC(STACK_ALLOCATION_CAPACITY);
	
	// Use malloc for extra large allocations
	ResizableLinearAllocator linear_allocator(stack_allocation, STACK_ALLOCATION_CAPACITY, BACKUP_CAPACITY, { nullptr });
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&linear_allocator);
	Stream<void> contents = ReadWholeFileBinary(sandbox_file_path, allocator);

	struct DeallocateAllocator {
		void operator() () {
			allocator->ClearBackup();
		}

		ResizableLinearAllocator* allocator;
	};

	StackScope<DeallocateAllocator> deallocate_scope({ &linear_allocator });

	if (contents.buffer != nullptr) {
		// Read the header
		uintptr_t ptr = (uintptr_t)contents.buffer;
		
		SandboxFileHeader header;
		Read<true>(&ptr, &header, sizeof(header));

		if (header.version != SANDBOX_FILE_HEADER_VERSION) {
			// Error invalid version
			ECS_FORMAT_TEMP_STRING(console_error, "Failed to read sandbox file. Expected version {#} but the file has {#}.", SANDBOX_FILE_HEADER_VERSION, header.version);
			EditorSetConsoleError(console_error);
			return false;
		}

		if (header.count > SANDBOX_FILE_MAX_SANDBOXES) {
			// Error, too many sandboxes
			ECS_FORMAT_TEMP_STRING(console_error, "Sandbox file has been corrupted. Maximum supported count is {#}, but file says {#}.", SANDBOX_FILE_MAX_SANDBOXES, header.count);
			EditorSetConsoleError(console_error);
			return false;
		}

		// Deserialize the type table now
		DeserializeFieldTable field_table = DeserializeFieldTableFromData(ptr, allocator);
		if (field_table.types.size == 0) {
			// Error
			EditorSetConsoleError("The field table in the sandbox file is corrupted.");
			return false;
		}

		EditorSandbox* sandboxes = (EditorSandbox*)ECS_STACK_ALLOC(sizeof(EditorSandbox) * header.count);
		DeserializeOptions options;
		options.field_table = &field_table;
		options.field_allocator = allocator;
		options.backup_allocator = allocator;
		options.read_type_table = false;

		const Reflection::ReflectionType* type = reflection_manager->GetType(STRING(EditorSandbox));
		for (size_t index = 0; index < header.count; index++) {
			ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, type, sandboxes + index, ptr, &options);
			if (code != ECS_DESERIALIZE_OK) {
				ECS_STACK_CAPACITY_STREAM(char, console_error, 512);

				if (code == ECS_DESERIALIZE_CORRUPTED_FILE) {
					ECS_FORMAT_STRING(console_error, "Failed to deserialize sandbox {#}. It is corrupted.", index);
				}
				else if (code == ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES) {
					ECS_FORMAT_STRING(console_error, "Failed to deserialize sandbox {#}. It is missing its dependent types.", index);
				}
				else if (code == ECS_DESERIALIZE_FIELD_TYPE_MISMATCH) {
					ECS_FORMAT_STRING(console_error, "Failed to deserialize sandbox {#}. There was a field type mismatch.", index);
				}
				else {
					ECS_FORMAT_STRING(console_error, "Unknown error happened when deserializing sandbox {#}.", index);
				}
				EditorSetConsoleError(console_error);
				return false;
			}
		}

		// Deallocate all the current sandboxes if any
		size_t initial_sandbox_count = editor_state->sandboxes.size;
		for (size_t index = 0; index < initial_sandbox_count; index++) {
			DestroySandbox(editor_state, 0);
		}

		// Now create the "real" sandboxes
		for (size_t index = 0; index < header.count; index++) {
			CreateSandbox(editor_state, false);

			EditorSandbox* sandbox = GetSandbox(editor_state, index);
			// Set the runtime settings path - this will also create the runtime
			// If it fails, default initialize the runtime
			if (!ChangeSandboxRuntimeSettings(editor_state, index, sandboxes[index].runtime_settings)) {
				// Print an error message
				ECS_FORMAT_TEMP_STRING(console_message, "The runtime settings {#} doesn't exist or is corrupted when trying to deserialize sandbox {#}. "
					"The sandbox will revert to default settings.", sandboxes[index].runtime_settings, index);
				EditorSetConsoleWarn(console_message);

				ChangeSandboxRuntimeSettings(editor_state, index, { nullptr, 0 });
			}

			if (!ChangeSandboxScenePath(editor_state, index, sandboxes[index].scene_path)) {
				ECS_FORMAT_TEMP_STRING(console_message, "The scene path {#} doesn't exist or is invalid when trying to deserialize sandbox {#}. "
					"The sandbox will revert to no scene.", sandboxes[index].scene_path, index);
				EditorSetConsoleWarn(console_message);

				ChangeSandboxScenePath(editor_state, index, { nullptr, 0 });
			}

			// Copy the camera positions and parameters
			sandbox->camera_parameters = sandboxes[index].camera_parameters;
			memcpy(sandbox->camera_saved_orientations, sandboxes[index].camera_saved_orientations, sizeof(sandbox->camera_saved_orientations));

			// Now the modules
			for (unsigned int subindex = 0; subindex < sandboxes[index].modules_in_use.size; subindex++) {
				unsigned int module_index = sandboxes[index].modules_in_use[subindex].module_index;
				// Only if it is in bounds
				if (module_index < editor_state->project_modules->size) {
					AddSandboxModule(editor_state, index, module_index, sandboxes[index].modules_in_use[subindex].module_configuration);
					ChangeSandboxModuleSettings(editor_state, index, module_index, sandboxes[index].modules_in_use[subindex].settings_name);

					if (sandboxes[index].modules_in_use[subindex].is_deactivated) {
						DeactivateSandboxModuleInStream(editor_state, index, subindex);
					}
				}
			}
		}

		return true;
	}
	
	ECS_FORMAT_TEMP_STRING(console_error, "Failed to read or open sandbox file. Path is {#}.", sandbox_file_path);
	EditorSetConsoleError(console_error);
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

void LockSandbox(EditorState* editor_state, unsigned int sandbox_index)
{
	GetSandbox(editor_state, sandbox_index)->locked_count.fetch_add(1, ECS_RELAXED);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LaunchSandboxRuntime(EditorState* editor_state, unsigned int index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, index);
	// Compile all modules in use by this sandbox
	if (!AreSandboxModulesCompiled(editor_state, index)) {
		// Compile the modules
		if (!CompileSandboxModules(editor_state, index)) {
			return false;
		}
	}

	// Now initialize the task scheduler with the tasks retrieved
	TaskScheduler* task_scheduler = sandbox->sandbox_world.task_scheduler;
	task_scheduler->Reset();

	// Push all the tasks into it
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		const EditorModuleInfo* info = GetModuleInfo(editor_state, sandbox->modules_in_use[index].module_index, sandbox->modules_in_use[index].module_configuration);	
		task_scheduler->Add(info->ecs_module.tasks);
	}

	ECS_STACK_CAPACITY_STREAM(char, solve_error_message, 2048);
	solve_error_message.Copy("Could not solve task dependencies. ");

	// Try to solve the scheduling order
	bool success = task_scheduler->Solve(&solve_error_message);
	if (!success) {
		EditorSetConsoleError(solve_error_message);
		return false;
	}

	// Now write the module settings into the system manager
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		EditorModule* editor_module = &editor_state->project_modules->buffer[sandbox->modules_in_use[index].module_index];
		Stream<wchar_t> library_name = editor_module->library_name;
		ECS_STACK_CAPACITY_STREAM(char, ascii_name, 512);
		function::ConvertWideCharsToASCII(library_name, ascii_name);

		size_t reflected_settings_size = sandbox->modules_in_use[index].reflected_settings.size;
		ECS_STACK_CAPACITY_STREAM(SystemManagerSetting, settings, 32);

		for (size_t setting_index = 0; setting_index < reflected_settings_size; setting_index++) {
			settings[index].data = sandbox->modules_in_use[index].reflected_settings[setting_index].data;
			settings[index].name = sandbox->modules_in_use[index].reflected_settings[setting_index].name;
			settings[index].byte_size = 0;
		}

		settings.size = reflected_settings_size;
		sandbox->sandbox_world.system_manager->BindSystemSettings(ascii_name, settings);
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void PreinitializeSandboxRuntime(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	// Create a sandbox allocator - a global one - such that it can accomodate the default entity manager requirements
	GlobalMemoryManager* allocator = (GlobalMemoryManager*)editor_state->editor_allocator->Allocate(sizeof(GlobalMemoryManager));
	*allocator = GlobalMemoryManager(
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

	// Resize the textures for the viewport to a 1x1 texture such that rendering commands will fallthrough even
	// when the UI has not yet run to resize them
	for (size_t index = 0; index < EDITOR_SANDBOX_VIEWPORT_COUNT; index++) {
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

void RegisterSandboxCameraTransform(EditorState* editor_state, unsigned int sandbox_index, unsigned int camera_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ECS_ASSERT(camera_index < std::size(sandbox->camera_saved_orientations));

	EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
	if (sandbox_state == EDITOR_SANDBOX_SCENE) {
		sandbox->camera_saved_orientations[camera_index] = { sandbox->camera_parameters.translation, sandbox->camera_parameters.rotation };
	}
	else {
		// TODO: Implement the runtime version
		ECS_ASSERT(false);
	}
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

void RemoveSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index) {
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);

	RemoveSandboxModuleInStream(editor_state, sandbox_index, in_stream_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index)
{
	EditorSandboxModule* sandbox_module = editor_state->sandboxes[sandbox_index].modules_in_use.buffer + in_stream_index;
	// Deallocate the reflected settings allocator
	sandbox_module->settings_allocator.Free();

	editor_state->sandboxes[sandbox_index].modules_in_use.RemoveSwapBack(in_stream_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxModuleForced(EditorState* editor_state, unsigned int module_index)
{
	// Find all sandboxes that depend on that module
	ECS_STACK_CAPACITY_STREAM(unsigned int, sandbox_indices, 512);
	IsModuleInfoUsed(editor_state, module_index, false, EDITOR_MODULE_CONFIGURATION_COUNT, &sandbox_indices);

	if (sandbox_indices.size > 0) {
		unsigned int loaded_module_index = editor_state->editor_components.ModuleIndexFromReflection(editor_state, module_index);
		ECS_ASSERT(loaded_module_index != -1);

		// Remove the module from the sandbox
		// And also update its scene to have any components from the given module be removed
		for (unsigned int index = 0; index < sandbox_indices.size; index++) {
			// Remove the module from the entity manager
			// Do it for the scene entities and also for the runtime entities
			editor_state->editor_components.RemoveModuleFromManager(editor_state, sandbox_indices[index], EDITOR_SANDBOX_VIEWPORT_SCENE, loaded_module_index);
			editor_state->editor_components.RemoveModuleFromManager(editor_state, sandbox_indices[index], EDITOR_SANDBOX_VIEWPORT_RUNTIME, loaded_module_index);
			
			// Set the scene dirty for that sandbox
			SetSandboxSceneDirty(editor_state, sandbox_indices[index], EDITOR_SANDBOX_VIEWPORT_SCENE);

			RemoveSandboxModule(editor_state, sandbox_indices[index], module_index);
		}

		// Save the sandbox file
		bool success = SaveEditorSandboxFile(editor_state);
		if (!success) {
			EditorSetConsoleError("Failed to save editor sandbox file.");
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
	RemoveEditorRuntime(system_manager);
	if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
		RemoveEditorRuntimeSelectColor(system_manager);
		RemoveEditorRuntimeSelectedEntities(system_manager);
		RemoveEditorRuntimeTransformTool(system_manager);
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
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	// If the rendering is disabled skip
	if (!IsSandboxViewportRendering(editor_state, sandbox_index, viewport)) {
		return true;
	}

	// We assume that the Graphics module won't modify any entities
	unsigned int in_stream_module_index = GetSandboxGraphicsModule(editor_state, sandbox_index);
	if (in_stream_module_index == -1) {
		return false;
	}

	// Check to see if we already have a pending event - if we do, skip the call
	bool pending_event = EditorHasEvent(editor_state, WaitCompilationRenderSandboxEvent);
	if (pending_event) {
		return true;
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

		MemoryManager runtime_query_cache_allocator = ArchetypeQueryCache::DefaultAllocator(editor_state->GlobalMemoryManager());
		ArchetypeQueryCache runtime_query_cache;

		if (viewport == EDITOR_SANDBOX_VIEWPORT_SCENE) {
			viewport_entity_manager = &sandbox->scene_entities;

			// Bind to the sandbox the scene entity manager
			sandbox->sandbox_world.entity_manager = viewport_entity_manager;
		}

		// We need to record the query cache to restore it later
		sandbox->sandbox_world.entity_manager->CopyQueryCache(&runtime_query_cache, GetAllocatorPolymorphic(&runtime_query_cache_allocator));

		auto deallocate_temp_resources = [&]() {
			AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
			resource_snapshot.Deallocate(editor_allocator);
			graphics_snapshot.Deallocate(editor_allocator);
			viewport_task_scheduler_allocator.Free();
			runtime_query_cache_allocator.Free();
		};

		// Prepare the task scheduler
		bool success = ConstructSandboxSchedulingOrder(editor_state, sandbox_index, { &in_stream_module_index, 1 }, disable_logging);
		if (!success) {
			deallocate_temp_resources();
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
	// If the rendering is disabled also return is pending
	if (!IsSandboxViewportRendering(editor_state, sandbox_index, viewport)) {
		return true;
	}

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
	EditorStateWaitFlag(25, editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING, false);

	Graphics* runtime_graphics = editor_state->RuntimeGraphics();
	// Free the current render destination and create a new one
	FreeSandboxRenderTextures(editor_state, sandbox_index, viewport);

	GraphicsRenderDestinationOptions destination_options;
	destination_options.render_misc = ECS_GRAPHICS_MISC_SHARED;
	sandbox->viewport_render_destination[viewport] = runtime_graphics->CreateRenderDestination(new_size, destination_options);

	// Now transfer the texture from the RuntimeGraphics to the UIGraphics
	sandbox->viewport_transferred_texture[viewport] = editor_state->UIGraphics()->TransferGPUView(sandbox->viewport_render_destination[viewport].output_view);

	// Also create the instanced framebuffers
	Texture2DDescriptor instanced_framebuffer_descriptor;
	instanced_framebuffer_descriptor.format = ECS_GRAPHICS_FORMAT_R32_UINT;
	instanced_framebuffer_descriptor.bind_flag = ECS_GRAPHICS_BIND_RENDER_TARGET;
	instanced_framebuffer_descriptor.mip_levels = 1;
	instanced_framebuffer_descriptor.size = new_size;
	Texture2D instanced_framebuffer_texture = runtime_graphics->CreateTexture(&instanced_framebuffer_descriptor);
	sandbox->scene_viewport_instance_framebuffer = runtime_graphics->CreateRenderTargetView(instanced_framebuffer_texture);
	
	Texture2DDescriptor instanced_depth_stencil_descriptor;
	instanced_depth_stencil_descriptor.format = ECS_GRAPHICS_FORMAT_D32_FLOAT;
	instanced_depth_stencil_descriptor.bind_flag = ECS_GRAPHICS_BIND_DEPTH_STENCIL;
	instanced_depth_stencil_descriptor.mip_levels = 1;
	instanced_depth_stencil_descriptor.size = new_size;
	Texture2D instanced_depth_texture = runtime_graphics->CreateTexture(&instanced_depth_stencil_descriptor);
	sandbox->scene_viewport_depth_stencil_framebuffer = runtime_graphics->CreateDepthStencilView(instanced_depth_texture);

	runtime_graphics->m_window_size = new_size;
}

// -----------------------------------------------------------------------------------------------------------------------------

void RotateSandboxCamera(EditorState* editor_state, unsigned int sandbox_index, float3 rotation)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters.rotation += rotation;
}

// -----------------------------------------------------------------------------------------------------------------------------

void ResetSandboxCamera(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters.Default();
	for (size_t index = 0; index < EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT; index++) {
		sandbox->camera_saved_orientations[index].ToOrigin();
	}
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

// ------------------------------------------------------------------------------------------------------------------------------

bool SaveSandboxScene(EditorState* editor_state, unsigned int sandbox_index)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	GetSandboxScenePath(editor_state, sandbox_index, absolute_path);
	bool success = SaveEditorScene(editor_state, &sandbox->scene_entities, &sandbox->database, absolute_path);
	if (success) {
		sandbox->is_scene_dirty = false;
	}
	return success;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveEditorSandboxFile(const EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectSandboxFile(editor_state, absolute_path);

	// Allocate a buffer of ECS_KB * 128 and write into it
	// Assert that the overall size is less
	const size_t STACK_CAPACITY = ECS_KB * 128;
	void* stack_buffer = ECS_STACK_ALLOC(STACK_CAPACITY);
	uintptr_t ptr = (uintptr_t)stack_buffer;

	SandboxFileHeader header;
	header.count = editor_state->sandboxes.size;
	header.version = SANDBOX_FILE_HEADER_VERSION;

	// Write the header first
	Write<true>(&ptr, &header, sizeof(header));

	const Reflection::ReflectionManager* reflection_manager = editor_state->ui_reflection->reflection;

	// Write the type table
	const Reflection::ReflectionType* sandbox_type = reflection_manager->GetType(STRING(EditorSandbox));
	SerializeFieldTable(reflection_manager, sandbox_type, ptr);

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	SerializeOptions options;
	options.write_type_table = false;
	options.error_message = &error_message;

	for (size_t index = 0; index < editor_state->sandboxes.size; index++) {
		ECS_SERIALIZE_CODE code = Serialize(reflection_manager, sandbox_type, editor_state->sandboxes.buffer + index, ptr, &options);
		if (code != ECS_SERIALIZE_OK) {
			ECS_FORMAT_TEMP_STRING(console_message, "Could not save sandbox file. Faulty sandbox {#}. Detailed error message: {#}.", index, error_message);
			EditorSetConsoleError(console_message);
			return false;
		}
	}

	size_t bytes_written = ptr - (uintptr_t)stack_buffer;
	ECS_ASSERT(bytes_written <= STACK_CAPACITY);
	// Now commit to the file
	ECS_FILE_STATUS_FLAGS status = WriteBufferToFileBinary(absolute_path, { stack_buffer, bytes_written });
	if (status != ECS_FILE_STATUS_OK) {
		ECS_FORMAT_TEMP_STRING(console_message, "Could not save sandbox file. Failed to write to path {#}.", absolute_path);
		EditorSetConsoleError(console_message);

		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxCameraAspectRatio(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	ResourceView view = sandbox->viewport_render_destination[viewport].output_view;

	uint2 dimensions = GetTextureDimensions(view.AsTexture2D());
	sandbox->camera_parameters.aspect_ratio = (float)dimensions.x / (float)dimensions.y;
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

// -----------------------------------------------------------------------------------------------------------------------------

void TranslateSandboxCamera(EditorState* editor_state, unsigned int sandbox_index, float3 translation)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->camera_parameters.translation += translation;
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
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	for (unsigned int index = 0; index < sandbox_count; index++) {
		EditorSandbox* sandbox = GetSandbox(editor_state, index);
		if (sandbox->selected_entities_changed_counter > 0) {
			RenderSandbox(editor_state, index, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
		}
		sandbox->selected_entities_changed_counter = function::SaturateSub(sandbox->selected_entities_changed_counter, (unsigned char)1);
	}
}

void TickSandboxes(EditorState* editor_state)
{
	TickSandboxesRuntimeSettings(editor_state);
	TickSandboxesSelectedEntities(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------