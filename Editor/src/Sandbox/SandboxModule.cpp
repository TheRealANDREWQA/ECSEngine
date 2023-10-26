#include "editorpch.h"
#include "SandboxModule.h"
#include "../Editor/EditorState.h"
#include "../Modules/Module.h"
#include "../Modules/ModuleSettings.h"
#include "Sandbox.h"
#include "SandboxFile.h"

#define MODULE_FILE_SETTING_EXTENSION L".config"
//#define MODULE_FILE_MANUAL_SETTING_EXTENSION L".mconfig"

#define MODULE_ALLOCATOR_SIZE ECS_KB * 32
#define MODULE_ALLOCATOR_POOL_COUNT 1024
#define MODULE_ALLOCATOR_BACKUP_SIZE ECS_KB * 128

using namespace ECSEngine;

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
	sandbox_module->reflected_settings = {};

	// Create the allocator
	sandbox_module->settings_allocator = MemoryManager(
		MODULE_ALLOCATOR_SIZE, 
		MODULE_ALLOCATOR_POOL_COUNT, 
		MODULE_ALLOCATOR_BACKUP_SIZE, 
		GetAllocatorPolymorphic(sandbox->GlobalMemoryManager())
	);
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
		sandbox_module->settings_name = StringCopy(sandbox_module->Allocator(), settings_name);
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
			sandbox_module->settings_name = StringCopy(sandbox_module->Allocator(), settings_name);
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

void GetSandboxModulesCompilingInProgress(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<unsigned int>& in_stream_indices)
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
	ECSEngine::CapacityStream<unsigned int>& in_stream_indices, 
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
			settings_path.CopyOther(sandbox_module->settings_name);

			sandbox_module->settings_allocator.Clear();
			sandbox_module->settings_name = StringCopy(sandbox_module->Allocator(), settings_path);
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