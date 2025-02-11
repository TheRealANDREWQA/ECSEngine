#include "editorpch.h"
#include "SandboxModule.h"
#include "../Editor/EditorState.h"
#include "../Modules/Module.h"
#include "../Modules/ModuleSettings.h"
#include "Sandbox.h"
#include "SandboxFile.h"
#include "ECSEngineComponentsAll.h"

#define MODULE_FILE_SETTING_EXTENSION L".config"
//#define MODULE_FILE_MANUAL_SETTING_EXTENSION L".mconfig"

#define MODULE_SETTINGS_ALLOCATOR_SIZE ECS_KB * 32
#define MODULE_SETTINGS_ALLOCATOR_POOL_COUNT 1024
#define MODULE_SETTINGS_ALLOCATOR_BACKUP_SIZE ECS_KB * 128

#define MODULE_ENABLED_DEBUG_TASKS_ALLOCATOR_SIZE ECS_KB * 4
#define MODULE_ENABLED_DEBUG_TASKS_POOL_COUNT ECS_KB

#define LAZY_EVALUATION_MS 100

using namespace ECSEngine;

static bool ExistsSandboxModuleEnabledDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name) {
	unsigned int module_index = GetDebugDrawTasksBelongingModule(editor_state, task_name);
	ECS_ASSERT(module_index != -1);
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);
	EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_index);
	return FindString(task_name, sandbox_module->enabled_debug_tasks.ToStream()) != -1;
}

static void RemoveSandboxModuleEnabledDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name) {
	unsigned int module_index = GetDebugDrawTasksBelongingModule(editor_state, task_name);
	ECS_ASSERT(module_index != -1);
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);
	EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_index);
	unsigned int enabled_index = FindString(task_name, sandbox_module->enabled_debug_tasks.ToStream());
	ECS_ASSERT(enabled_index != -1);
	sandbox_module->enabled_debug_tasks[enabled_index].Deallocate(sandbox_module->EnabledDebugTasksAllocator());
	sandbox_module->enabled_debug_tasks.RemoveSwapBack(enabled_index);
}

// It will update the debug draw tasks that are registered according to the state of the runtime task manager
static void UpdateSandboxModuleEnabledDebugDrawTasks(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int in_stream_module_index,
	const TaskManager* task_manager
) {
	bool change_happened = false;
	EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_module_index);
	const EditorModuleInfo* module_info = GetSandboxModuleInfo(editor_state, sandbox_index, in_stream_module_index);
	Stream<ModuleDebugDrawTaskElement> debug_draw_elements = module_info->ecs_module.debug_draw_task_elements;
	for (size_t index = 0; index < debug_draw_elements.size; index++) {
		Stream<char> task_name = debug_draw_elements[index].base_element.task_name;
		// Perform this operation just if the task exists in the task manager. It might not exist
		// In the case that the task is removed due to missing initialization dependencies
		if (task_manager->FindTask(task_name) != -1) {
			bool is_enabled = IsModuleDebugDrawTaskElementEnabled(task_manager, task_name);
			unsigned int existing_index = FindString(task_name, sandbox_module->enabled_debug_tasks.ToStream());
			if (is_enabled) {
				// Insert it if it doesn't exist
				if (existing_index == -1) {
					AddSandboxModuleDebugDrawTask(editor_state, sandbox_index, in_stream_module_index, task_name);
					change_happened = true;
				}
			}
			else {
				// Remove it if it exists
				if (existing_index != -1) {
					RemoveSandboxModuleEnabledDebugDrawTask(editor_state, sandbox_index, task_name);
					change_happened = true;
				}
			}
		}
	}

	if (change_happened) {
		SaveEditorSandboxFile(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void AddSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name) {
	unsigned int module_index = GetDebugDrawTasksBelongingModule(editor_state, task_name);
	ECS_ASSERT(module_index != -1);
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(in_stream_index != -1);
	AddSandboxModuleDebugDrawTask(editor_state, sandbox_index, in_stream_index, task_name);
}

// -----------------------------------------------------------------------------------------------------------------------------

void AddSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_module_index, Stream<char> task_name)
{
	EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_stream_module_index);
	sandbox_module->enabled_debug_tasks.Add(task_name.Copy(sandbox_module->EnabledDebugTasksAllocator()));
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

	unsigned int sandbox_module_index = sandbox->modules_in_use.ReserveRange();
	EditorSandboxModule* sandbox_module = sandbox->modules_in_use.buffer + sandbox_module_index;
	sandbox_module->module_index = module_index;
	sandbox_module->module_configuration = module_configuration;
	sandbox_module->is_deactivated = false;

	sandbox_module->settings_name.buffer = nullptr;
	sandbox_module->settings_name.size = 0;
	sandbox_module->reflected_settings = {};

	// Create the allocators
	sandbox_module->settings_allocator = MemoryManager(
		MODULE_SETTINGS_ALLOCATOR_SIZE, 
		MODULE_SETTINGS_ALLOCATOR_POOL_COUNT, 
		MODULE_SETTINGS_ALLOCATOR_BACKUP_SIZE, 
		sandbox->GlobalMemoryManager()
	);
	sandbox_module->enabled_debug_tasks_allocator = MemoryManager(
		MODULE_ENABLED_DEBUG_TASKS_ALLOCATOR_SIZE,
		MODULE_ENABLED_DEBUG_TASKS_POOL_COUNT,
		MODULE_ENABLED_DEBUG_TASKS_ALLOCATOR_SIZE,
		sandbox->GlobalMemoryManager()
	);

	sandbox_module->enabled_debug_tasks.Initialize(sandbox_module->SettingsAllocator(), 0);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool AreSandboxModulesCompiled(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated && UpdateModuleLastWrite(editor_state, sandbox->modules_in_use[index].module_index)) {
			return false;
		}
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool AreSandboxModulesLoaded(const EditorState* editor_state, unsigned int sandbox_index, bool exclude_out_of_date)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated) {
			const EditorModuleInfo* info = GetModuleInfo(editor_state, sandbox->modules_in_use[index].module_index, sandbox->modules_in_use[index].module_configuration);
			// There is a small window where a module can be in load status good but the module itself is not loaded
			// So, check for the base ecs module code as well
			if (info->ecs_module.base_module.code != ECS_GET_MODULE_OK) {
				return false;
			}
			if (exclude_out_of_date) {
				if (info->load_status != EDITOR_MODULE_LOAD_GOOD) {
					return false;
				}
			}
			else {
				if (info->load_status == EDITOR_MODULE_LOAD_FAILED) {
					return false;
				}
			}
		}
	}
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void AggregateSandboxModuleEnabledDebugDrawTasks(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<Stream<char>>* task_names)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		const EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, index);
		task_names->AddStreamAssert(sandbox_module->enabled_debug_tasks);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void BindSandboxRuntimeModuleSettings(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		Stream<EditorModuleReflectedSetting> reflected_settings = sandbox->modules_in_use[index].reflected_settings;
		if (reflected_settings.size > 0) {
			ECS_STACK_CAPACITY_STREAM(char, ascii_name, 512);
			ConvertWideCharsToASCII(editor_state->project_modules->buffer[sandbox->modules_in_use[index].module_index].library_name, ascii_name);

			ECS_STACK_CAPACITY_STREAM(SystemManagerSetting, system_settings, 512);
			ECS_ASSERT(system_settings.capacity >= reflected_settings.size);
			for (size_t subindex = 0; subindex < reflected_settings.size; subindex++) {
				system_settings[subindex].name = reflected_settings[subindex].name;
				system_settings[subindex].data = reflected_settings[subindex].data;
				// Here we indicate to the system manager that we want the data to be referenced instead of copied
				system_settings[subindex].byte_size = 0;
			}
			system_settings.size = reflected_settings.size;
			sandbox->sandbox_world.system_manager->BindSystemSettings(ascii_name, system_settings);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, Stream<wchar_t> settings_name)
{
	unsigned int sandbox_module_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	ECS_ASSERT(sandbox_module_index != -1);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	EditorSandboxModule* sandbox_module = sandbox->modules_in_use.buffer + module_index;

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_settings_path, 512);

	if (HasSandboxModuleSettings(sandbox_module)) {
		sandbox_module->settings_allocator.Deallocate(sandbox_module->settings_name.buffer);
		ClearSandboxModuleSettings(editor_state, sandbox_index, module_index);
	}

	// Change the path
	if (settings_name.size > 0) {
		sandbox_module->settings_name = StringCopy(sandbox_module->SettingsAllocator(), settings_name);
		bool success = LoadSandboxModuleSettings(editor_state, sandbox_index, module_index);
		if (!success) {
			Stream<wchar_t> library_name = editor_state->project_modules->buffer[module_index].library_name;
			ECS_FORMAT_TEMP_STRING(message, "Failed to read the settings {#} for module {#}", settings_name, library_name);
			EditorSetConsoleError(message);
		}
	}
	else {
		sandbox_module->settings_name = { nullptr, 0 };
	}
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
			settings_name.CopyOther(sandbox_module->settings_name);
		}

		sandbox_module->settings_allocator.Clear();

		// Make the streams nullptr for easier debugging
		if (settings_name.size > 0) {
			sandbox_module->settings_name = StringCopy(sandbox_module->SettingsAllocator(), settings_name);
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

void ClearSandboxModulesInUse(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	unsigned int destination_initial_module_count = sandbox->modules_in_use.size;
	for (unsigned int index = 0; index < destination_initial_module_count; index++) {
		RemoveSandboxModuleInStream(editor_state, sandbox_index, 0);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ClearModuleDebugDrawComponentCrashStatus(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ComponentWithType component_type,
	bool assert_not_found
) {
	// If this is an ECS component, skip the call, since it can't crash
	if (editor_state->editor_components.IsECSEngineComponent(component_type.component, component_type.type)) {
		return;
	}

	EDITOR_MODULE_CONFIGURATION configuration;
	unsigned int module_index = FindSandboxDebugDrawComponentModuleIndex(editor_state, sandbox_index, component_type, &configuration);
	ClearModuleDebugDrawComponentCrashStatus(editor_state, module_index, configuration, component_type, assert_not_found);
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySandboxModulesFromAnother(EditorState* editor_state, unsigned int destination_index, unsigned int source_index)
{
	const EditorSandbox* source = GetSandbox(editor_state, source_index);
	ClearSandboxModulesInUse(editor_state, destination_index);

	for (unsigned int index = 0; index < source->modules_in_use.size; index++) {
		AddSandboxModule(editor_state, destination_index, source->modules_in_use[index].module_index, source->modules_in_use[index].module_configuration);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void CopySandboxModuleSettingsFromAnother(EditorState* editor_state, unsigned int destination_index, unsigned int source_index)
{
	const EditorSandbox* source = GetSandbox(editor_state, source_index);
	for (unsigned int index = 0; index < source->modules_in_use.size; index++) {
		unsigned int destination_module_index = GetSandboxModuleInStreamIndex(editor_state, destination_index, source->modules_in_use[index].module_index);
		if (destination_module_index != -1) {
			ChangeSandboxModuleSettings(editor_state, destination_index, source->modules_in_use[index].module_index, source->modules_in_use[index].settings_name);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool CompileSandboxModules(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	ECS_STACK_CAPACITY_STREAM(EDITOR_MODULE_CONFIGURATION, compile_configurations, 512);
	ECS_STACK_CAPACITY_STREAM(unsigned int, compile_indices, 512);
	ECS_STACK_CAPACITY_STREAM(EDITOR_LAUNCH_BUILD_COMMAND_STATUS, launch_statuses, 512);
	unsigned int module_count = sandbox->modules_in_use.size;
	ECS_ASSERT(module_count <= 512);

	// Only compile the modules which are not deactivated
	module_count = 0;

	for (size_t index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated) {
			compile_indices[module_count] = sandbox->modules_in_use[index].module_index;
			compile_configurations[module_count++] = sandbox->modules_in_use[index].module_configuration;
		}
	}

	compile_indices.size = module_count;

	BuildModules(editor_state, compile_indices, compile_configurations.buffer, launch_statuses.buffer);

	bool are_all_skipped = true;
	for (unsigned int index = 0; index < module_count && are_all_skipped; index++) {
		if (launch_statuses[index] != EDITOR_LAUNCH_BUILD_COMMAND_SKIPPED) {
			are_all_skipped = false;
		}
	}

	return are_all_skipped;
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

void DisableSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	DisableModuleDebugDrawTaskElement(sandbox->sandbox_world.task_manager, task_name);
	if (ExistsSandboxModuleEnabledDebugDrawTask(editor_state, sandbox_index, task_name)) {
		RemoveSandboxModuleEnabledDebugDrawTask(editor_state, sandbox_index, task_name);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void EnableSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	EnableModuleDebugDrawTaskElement(sandbox->sandbox_world.task_manager, task_name);
	if (!ExistsSandboxModuleEnabledDebugDrawTask(editor_state, sandbox_index, task_name)) {
		AddSandboxModuleDebugDrawTask(editor_state, sandbox_index, task_name);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int FindSandboxDebugDrawComponentModuleIndex(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ComponentWithType component_with_type,
	EDITOR_MODULE_CONFIGURATION* configuration
) {
	ECS_STACK_CAPACITY_STREAM(unsigned int, module_indices, 512);
	ECS_STACK_CAPACITY_STREAM(EDITOR_MODULE_CONFIGURATION, configurations, 512);
	FindModuleDebugDrawComponentIndex(editor_state, component_with_type, &module_indices, &configurations);
	
	for (unsigned int index = 0; index < module_indices.size; index++) {
		EDITOR_MODULE_CONFIGURATION sandbox_configuration = IsModuleUsedBySandbox(editor_state, sandbox_index, module_indices[index]);
		if (sandbox_configuration == configurations[index]) {
			*configuration = sandbox_configuration;
			return module_indices[index];
		}
	}

	return -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

void FlipSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	bool is_set = FlipModuleDebugDrawTaskElement(sandbox->sandbox_world.task_manager, task_name);
	if (is_set) {
		AddSandboxModuleDebugDrawTask(editor_state, sandbox_index, task_name);
	}
	else {
		RemoveSandboxModuleEnabledDebugDrawTask(editor_state, sandbox_index, task_name);
	}
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

void GetSandboxModulesCompilingInProgress(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>& in_stream_indices)
{
	// We need to acquire the lock to inspect the modules which are being compiled right now
	ECS_STACK_CAPACITY_STREAM(unsigned int, active_sandbox_modules, 512);
	ECS_STACK_CAPACITY_STREAM(EDITOR_MODULE_CONFIGURATION, active_sandbox_module_configurations, 512);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated) {
			active_sandbox_modules.AddAssert(sandbox->modules_in_use[index].module_index);
			active_sandbox_module_configurations.Add(sandbox->modules_in_use[index].module_configuration);
		}
	}

	GetCompilingModules(editor_state, active_sandbox_modules, active_sandbox_module_configurations.buffer);

	for (unsigned int index = 0; index < active_sandbox_modules.size; index++) {
		unsigned int in_stream_module_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, active_sandbox_modules[index]);
		ECS_ASSERT(in_stream_module_index != -1);
		in_stream_indices.AddAssert(in_stream_module_index);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxNeededButMissingModules(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<unsigned int>& in_stream_indices, 
	bool include_out_of_date
)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated) {
			const EditorModuleInfo* info = GetModuleInfo(editor_state, sandbox->modules_in_use[index].module_index, sandbox->modules_in_use[index].module_configuration);
			if (info->load_status == EDITOR_MODULE_LOAD_FAILED) {
				in_stream_indices.AddAssert(index);
			}
			else if (include_out_of_date && info->load_status == EDITOR_MODULE_LOAD_OUT_OF_DATE) {
				in_stream_indices.AddAssert(index);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

static const ModuleComponentFunctions* GetSandboxModuleComponentFunctionsImpl(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	unsigned int in_stream_module_index,
	Stream<char> component_name
) {
	const EditorModuleInfo* info = GetSandboxModuleInfo(editor_state, sandbox_index, in_stream_module_index);
	Stream<ModuleComponentFunctions> component_functions = info->ecs_module.component_functions;
	size_t component_index = component_functions.Find(component_name, [](const ModuleComponentFunctions& functions) {
		return functions.component_name;
		});
	if (component_index != -1) {
		return &component_functions[component_index];
	}
	return nullptr;
}

const ModuleComponentFunctions* GetSandboxModuleComponentFunctions(const EditorState* editor_state, unsigned int sandbox_index, Stream<char> component_name) {
	// At the moment, we don't need to consider the ECS components which are shipped from the engine
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		if (!sandbox->modules_in_use[index].is_deactivated) {
			const ModuleComponentFunctions* component_functions = GetSandboxModuleComponentFunctionsImpl(editor_state, sandbox_index, index, component_name);
			if (component_functions != nullptr) {
				return component_functions;
			}
		}
	}
	return nullptr;
}

const ModuleComponentFunctions* GetSandboxModuleComponentFunctions(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	unsigned int module_index, 
	Stream<char> component_name
)
{
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	return GetSandboxModuleComponentFunctionsImpl(editor_state, sandbox_index, in_stream_index, component_name);
}

// -------------------------------------------------------------------------------------------------------------

const ModuleComponentFunctions* GetModuleComponentFunctionsBestFit(const EditorState* editor_state, Stream<char> component_name) {
	unsigned int module_count = editor_state->project_modules->size;
	for (unsigned int index = 0; index < module_count; index++) {
		EDITOR_MODULE_CONFIGURATION configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
			const EditorModuleInfo* module_info = GetModuleInfo(editor_state, index, configuration);
			Stream<ModuleComponentFunctions> component_functions = module_info->ecs_module.component_functions;
			size_t component_index = component_functions.Find(component_name, [](const ModuleComponentFunctions& functions) {
				return functions.component_name;
			});
			if (component_index != -1) {
				return &component_functions[component_index];
			}
		}
	}
	return nullptr;
}

// -------------------------------------------------------------------------------------------------------------

ComponentFunctions GetSandboxComponentFunctions(const EditorState* editor_state, unsigned int sandbox_index, Stream<char> component_name, AllocatorPolymorphic stack_allocator, SharedComponentCompareEntry* compare_entry) {
	ComponentFunctions component_functions;
	const ModuleComponentFunctions* module_component_functions = GetSandboxModuleComponentFunctions(editor_state, sandbox_index, component_name);
	if (module_component_functions == nullptr) {
		// Try again with the best fit module, if the sandbox
		// Does not have the module assigned to it
		module_component_functions = GetModuleComponentFunctionsBestFit(editor_state, component_name);
	}

	const Reflection::ReflectionType* component_type = editor_state->editor_components.GetType(component_name);
	if (module_component_functions != nullptr && module_component_functions->copy_function != nullptr && module_component_functions->deallocate_function != nullptr) {
		module_component_functions->SetComponentFunctionsTo(&component_functions, editor_state->editor_components.GetComponentAllocatorSize(component_name));
	}
	else {
		component_functions = GetReflectionTypeRuntimeComponentFunctions(editor_state->GlobalReflectionManager(), component_type, stack_allocator);
	}

	if (compare_entry != nullptr) {
		if (module_component_functions != nullptr && module_component_functions->compare_function != nullptr) {
			module_component_functions->SetCompareEntryTo(compare_entry);
		}
		else {
			*compare_entry = GetReflectionTypeRuntimeCompareEntry(editor_state->GlobalReflectionManager(), component_type, stack_allocator);
		}
	}

	return component_functions;
}

// -------------------------------------------------------------------------------------------------------------

bool HasSandboxModuleSettings(const EditorSandboxModule* sandbox_module) {
	return sandbox_module->settings_name.size > 0;
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
			SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
				EDITOR_MODULE_CONFIGURATION sandbox_configuration = IsModuleUsedBySandbox(editor_state, sandbox_index, module_index);
				if (sandbox_configuration == configuration) {
					EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
					if (running_state) {
						if (sandbox_state == EDITOR_SANDBOX_RUNNING || sandbox_state == EDITOR_SANDBOX_PAUSED) {
							is_used = true;
							sandbox_indices->AddAssert(sandbox_index);
						}
					}
					else {
						is_used = true;
						sandbox_indices->AddAssert(sandbox_index);
					}
				}
			});
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
			if (SandboxAction<true>(editor_state, -1, [&](unsigned int sandbox_index) {
				EDITOR_MODULE_CONFIGURATION sandbox_configuration = IsModuleUsedBySandbox(editor_state, sandbox_index, module_index);
				if (sandbox_configuration == configuration) {
					EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
					if (running_state) {
						if (sandbox_state == EDITOR_SANDBOX_RUNNING || sandbox_state == EDITOR_SANDBOX_PAUSED) {
							return true;
						}
					}
					else {
						return true;
					}
				}
				return false;
			})) {
				return true;
			}
		}
		return false;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
	EditorSandboxModule* sandbox_module = editor_state->sandboxes[sandbox_index].modules_in_use.buffer + in_stream_index;

	AllocatorPolymorphic allocator = &sandbox_module->settings_allocator;
	ECS_STACK_CAPACITY_STREAM(EditorModuleReflectedSetting, settings, 64);
	AllocateModuleSettings(editor_state, module_index, settings, allocator);

	if (sandbox_module->settings_name.buffer != nullptr) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, settings_path, 512);
		GetSandboxModuleSettingsPathByIndex(editor_state, sandbox_index, in_stream_index, settings_path);
		if (!LoadModuleSettings(editor_state, module_index, settings_path, settings, allocator)) {
			// Copy the settings path such that we can reset after we deallocate the allocator
			settings_path.CopyOther(sandbox_module->settings_name);

			sandbox_module->settings_allocator.Clear();
			sandbox_module->settings_name = StringCopy(sandbox_module->SettingsAllocator(), settings_path);
			return false;
		}
	}
	else {
		// Set the default values
		SetModuleDefaultSettings(editor_state, module_index, settings);
	}

	// Now copy the stream into the sandbox module
	sandbox_module->reflected_settings.Initialize(&sandbox_module->settings_allocator, settings.size);
	sandbox_module->reflected_settings.CopyOther(settings);
	return true;
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
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	EditorSandboxModule* sandbox_module = sandbox->modules_in_use.buffer + in_stream_index;
	// Deallocate the allocators
	sandbox_module->settings_allocator.Free();
	sandbox_module->enabled_debug_tasks_allocator.Free();

	// Destroy the reflected settings. This is related to the UI instances
	// Do that for all possible inspector indices
	for (size_t index = 0; index < MAX_INSPECTOR_WINDOWS; index++) {
		DestroyModuleSettings(
			editor_state,
			sandbox->modules_in_use[index].module_index,
			sandbox->modules_in_use[index].reflected_settings,
			index
		);
	}

	sandbox->modules_in_use.RemoveSwapBack(in_stream_index);
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

bool ReloadSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	ClearSandboxModuleSettings(editor_state, sandbox_index, module_index);
	bool success = LoadSandboxModuleSettings(editor_state, sandbox_index, module_index);
	if (!success) {
		// In case we failed, set the default values
		unsigned int in_stream_index = GetSandboxModuleInStreamIndex(editor_state, sandbox_index, module_index);
		EditorSandboxModule* sandbox_module = editor_state->sandboxes[sandbox_index].modules_in_use.buffer + in_stream_index;

		AllocatorPolymorphic allocator = sandbox_module->SettingsAllocator();
		ECS_STACK_CAPACITY_STREAM(EditorModuleReflectedSetting, settings, 64);
		AllocateModuleSettings(editor_state, module_index, settings, allocator);
		sandbox_module->reflected_settings = settings;
		SetModuleDefaultSettings(editor_state, module_index, sandbox_module->reflected_settings);
		return false;
	}
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetModuleDebugDrawComponentCrashStatus(EditorState* editor_state, unsigned int sandbox_index, ComponentWithType component_type, bool assert_not_found)
{
	if (editor_state->editor_components.IsECSEngineComponent(component_type.component, component_type.type)) {
		return;
	}

	EDITOR_MODULE_CONFIGURATION configuration;
	unsigned int module_index = FindSandboxDebugDrawComponentModuleIndex(editor_state, sandbox_index, component_type, &configuration);
	SetModuleDebugDrawComponentCrashStatus(editor_state, module_index, configuration, component_type, assert_not_found);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateSandboxModuleEnabledDebugDrawTasks(EditorState* editor_state, unsigned int sandbox_index, const TaskManager* task_manager)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < sandbox->modules_in_use.size; index++) {
		UpdateSandboxModuleEnabledDebugDrawTasks(editor_state, sandbox_index, index, task_manager);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateSandboxComponentFunctionsForModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index)
{
	// Start by seeing if the module has overrides for component functions
	EDITOR_MODULE_CONFIGURATION configuration = IsModuleUsedBySandbox(editor_state, sandbox_index, module_index);
	ECS_ASSERT_FORMAT(configuration != EDITOR_MODULE_CONFIGURATION_COUNT, "Trying to update component functions for sandbox {#} "
		"for module {#} which doesn't exist", sandbox_index, GetModuleLibraryName(editor_state, module_index));
	Stream<ModuleComponentFunctions> module_functions = GetModuleInfo(editor_state, module_index, configuration)->ecs_module.component_functions;

	if (module_functions.size > 0) {
		EntityManager* scene_manager = GetSandboxEntityManager(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		EntityManager* runtime_manager = GetSandboxEntityManager(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);

		for (size_t index = 0; index < module_functions.size; index++) {
			ECS_COMPONENT_TYPE component_type = editor_state->editor_components.GetComponentType(module_functions[index].component_name);
			Component component = editor_state->editor_components.GetComponentID(module_functions[index].component_name);

			ECS_STACK_LINEAR_ALLOCATOR(functions_stack_allocator, ECS_KB * 32);
			ComponentFunctions functions;
			const ModuleComponentFunctions& current_module_functions = module_functions[index];
			if (current_module_functions.copy_function == nullptr && current_module_functions.deallocate_function == nullptr) {
				// Auto generate the runtime versions
				functions = editor_state->editor_components.GetAutoGeneratedComponentFunctions(module_functions[index].component_name, &functions_stack_allocator);
			}
			else {
				module_functions[index].SetComponentFunctionsTo(&functions, editor_state->editor_components.GetComponentAllocatorSize(component, component_type));
			}

			if (component_type == ECS_COMPONENT_UNIQUE) {
				if (scene_manager->ExistsComponent(component)) {
					scene_manager->ChangeComponentFunctionsCommit(component, &functions);
				}
				if (runtime_manager->ExistsComponent(component)) {
					runtime_manager->ChangeComponentFunctionsCommit(component, &functions);
				}
			}
			else if (component_type == ECS_COMPONENT_SHARED) {
				ECS_STACK_LINEAR_ALLOCATOR(compare_stack_allocator, ECS_KB * 32);
				SharedComponentCompareEntry compare_entry;
				if (current_module_functions.compare_function == nullptr) {
					// Auto generate the runtime version
					compare_entry = editor_state->editor_components.GetAutoGeneratedCompareEntry(module_functions[index].component_name, &compare_stack_allocator);
				}
				else {
					module_functions[index].SetCompareEntryTo(&compare_entry);
				}
				if (scene_manager->ExistsSharedComponent(component)) {
					scene_manager->ChangeSharedComponentFunctionsCommit(component, &functions, compare_entry);
				}
				if (runtime_manager->ExistsSharedComponent(component)) {
					runtime_manager->ChangeSharedComponentFunctionsCommit(component, &functions, compare_entry);
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateSandboxesComponentFunctionsForModule(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	ECS_STACK_CAPACITY_STREAM(unsigned int, sandbox_indices, ECS_KB * 4);
	GetSandboxesForModule(editor_state, module_index, configuration, &sandbox_indices);

	for (unsigned int index = 0; index < sandbox_indices.size; index++) {
		UpdateSandboxComponentFunctionsForModule(editor_state, sandbox_indices[index], module_index);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TickModuleSettingsRefresh(EditorState* editor_state)
{
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_MODULE_SETTINGS_REFRESH, LAZY_EVALUATION_MS)) {
		struct ReferencedSetting {
			ECS_INLINE bool operator == (ReferencedSetting other) const {
				return name == other.name && module_index == other.module_index && time_stamp == other.time_stamp;
			}

			Stream<wchar_t> name;
			unsigned int module_index;
			size_t time_stamp;
		};
		// Do a pre-determination phase to get all the referenced settings
		ECS_STACK_CAPACITY_STREAM(ReferencedSetting, referenced_settings, 128);

		// Allow temporary sandboxes as well
		unsigned int sandbox_count = GetSandboxCount(editor_state);
		for (unsigned int index = 0; index < sandbox_count; index++) {
			const EditorSandbox* sandbox = GetSandbox(editor_state, index);
			for (unsigned int module_index = 0; module_index < sandbox->modules_in_use.size; module_index++) {
				ReferencedSetting referenced_setting{ 
					sandbox->modules_in_use[module_index].settings_name, 
					sandbox->modules_in_use[module_index].module_index,
					sandbox->modules_in_use[module_index].time_stamp
				};
				unsigned int existing_index = referenced_settings.Find(
					referenced_setting,
					[](auto value) {
					return value;
				});
				if (existing_index == -1) {
					referenced_settings.AddAssert(referenced_setting);
				}
			}
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, module_setting_path, 512);
		for (unsigned int index = 0; index < referenced_settings.size; index++) {
			module_setting_path.size = 0;
			GetModuleSettingsFilePath(editor_state, referenced_settings[index].module_index, referenced_settings[index].name, module_setting_path);
			size_t file_time_stamp = OS::GetFileLastWrite(module_setting_path);
			if (file_time_stamp != -1 && file_time_stamp > referenced_settings[index].time_stamp) {
				// Update all sandboxes that reference this setting
				for (unsigned int sandbox_index = 0; sandbox_index < sandbox_count; sandbox_index++) {
					EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
					for (unsigned int module_index = 0; module_index < sandbox->modules_in_use.size; module_index++) {
						if (sandbox->modules_in_use[module_index].settings_name == referenced_settings[index].name
							&& sandbox->modules_in_use[module_index].module_index == referenced_settings[index].module_index
							&& sandbox->modules_in_use[module_index].time_stamp == referenced_settings[index].time_stamp) {
							bool success = ReloadSandboxModuleSettings(editor_state, sandbox_index, sandbox->modules_in_use[module_index].module_index);
							if (!success) {
								Stream<wchar_t> module_name = GetModuleLibraryName(editor_state, referenced_settings[index].module_index);
								ECS_FORMAT_TEMP_STRING(message, "Failed to reload settings {#} for module {#}", referenced_settings[index].name, module_name);
								EditorSetConsoleError(message);
							}
						}
					}
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------