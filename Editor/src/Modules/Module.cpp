#include "editorpch.h"
#include "Module.h"
#include "ECSEngineUtilities.h"
#include "..\Editor\EditorState.h"
#include "..\Editor\EditorEvent.h"
#include "..\Project\ProjectOperations.h"
#include "ECSEngineWorld.h"
#include "ModuleSettings.h"
#include "..\UI\Inspector.h"

using namespace ECSEngine;

constexpr const char* CMB_BUILD_SYSTEM_PATH = "cd C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE";
constexpr const char* CMD_BUILD_SYSTEM = "MSBuild.exe";
constexpr const char* BUILD_PROJECT_STRING = "/t:build";
constexpr const char* CLEAN_PROJECT_STRING = "/t:clean";
constexpr const char* REBUILD_PROJECT_STRING = "/t:rebuild";
constexpr const char* CMD_BUILD_SYSTEM_LOG_FILE_COMMAND = "/out";
constexpr const wchar_t* CMD_BUILD_SYSTEM_LOG_FILE_PATH = L"\\Debug\\";
constexpr const wchar_t* CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION = L".log";

constexpr const wchar_t* CMD_BUILD_SYSTEM_WIDE = L"MSBuild.exe";
constexpr const wchar_t* BUILD_PROJECT_STRING_WIDE = L"/t:build";
constexpr const wchar_t* CLEAN_PROJECT_STRING_WIDE = L"/t:clean";
constexpr const wchar_t* REBUILD_PROJECT_STRING_WIDE = L"/t:rebuild";
constexpr const wchar_t* CMD_BUILD_SYSTEM_LOG_FILE_COMMAND_WIDE = L"/out";

constexpr const char* CMB_BUILD_SYSTEM_INCORRECT_PARAMETER = "The operation could not be completed. The parameter is incorrect.";

constexpr const wchar_t* VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER = L".pdb.locked";
constexpr const wchar_t* MODULE_SOURCE_FILES[] = {
	L"src",
	L"Src",
	L"Source",
	L"source"
};

const wchar_t* ECS_RUNTIME_PDB_PATHS[] = {
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Debug-windows-x86_64\\ECSEngine",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Debug-windows-x86_64\\Editor",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Release-windows-x86_64\\ECSEngine",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Release-windows-x86_64\\Editor",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Distribution-windows-x86_64\\ECSEngine",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Distribution-windows-x86_64\\Editor"
};

constexpr unsigned int CHECK_FILE_STATUS_THREAD_SLEEP_TICK = 200;
#define BLOCK_THREAD_FOR_MODULE_STATUS_SLEEP_TICK 20

#define MODULE_BUILD_USING_SHELL

// -------------------------------------------------------------------------------------------------------------------------

bool PrintCommandStatus(EditorState* editor_state, Stream<wchar_t> log_path) {
	EDITOR_STATE(editor_state);
	Stream<char> contents = ReadWholeFileText(log_path, GetAllocatorPolymorphic(editor_allocator, ECS_ALLOCATION_MULTI));

	if (contents.buffer != nullptr) {
		contents[contents.size - 1] = '\0';
		const char* build_FAILED = strstr(contents.buffer, "Build FAILED.");

		editor_allocator->Deallocate_ts(contents.buffer);
		if (build_FAILED != nullptr) {
			// Extract the module name from the log path
			const wchar_t* debug_ptr = wcsstr(log_path.buffer, CMD_BUILD_SYSTEM_LOG_FILE_PATH);
			if (debug_ptr != nullptr) {
				debug_ptr += wcslen(CMD_BUILD_SYSTEM_LOG_FILE_PATH);
				const wchar_t* underscore_after_library_name = wcschr(debug_ptr, L'_');
				if (underscore_after_library_name != nullptr) {
					ECS_FORMAT_TEMP_STRING(error_message, "Module {#} have failed to build. Check the {#} log.",
						Stream<wchar_t>(debug_ptr, underscore_after_library_name - debug_ptr), CMD_BUILD_SYSTEM);
					EditorSetConsoleError(error_message);
					return false;
				}
				else {
					ECS_FORMAT_TEMP_STRING(error_message, "A module failed to build. Could not deduce library name (it's missing the underscore)."
						" Check the {#} log.", CMD_BUILD_SYSTEM);
					EditorSetConsoleError(error_message);
					return false;
				}
			}
			else {
				ECS_FORMAT_TEMP_STRING(error_message, "A module failed to build. Could not deduce library name. Check the {#} log.", CMD_BUILD_SYSTEM);
				EditorSetConsoleError(error_message);
				return false;
			}
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(error_message, "Could not open {#} to read the log. Open the file externally to check the command status.", log_path);
		EditorSetConsoleWarn(error_message);
		return false;
	}

	return true;
}

void SetCrashHandlerPDBPaths(const EditorState* editor_state) {
	unsigned int ecs_runtime_pdb_index = 0;
#ifdef ECSENGINE_RELEASE
	ecs_runtime_pdb_index = 2;
#endif

#ifdef ECSENGINE_DISTRIBUTION
	ecs_runtime_pdb_index = 4;
#endif

	// Every module will have all of its configurations added to the search path
	size_t path_count = 2 + editor_state->project_modules->size * (size_t)EDITOR_MODULE_CONFIGURATION_COUNT;

	// Update the pdb paths in order to get stack unwinding for the crash handler
	Stream<wchar_t>* pdb_paths = (Stream<wchar_t>*)ECS_STACK_ALLOC(sizeof(Stream<wchar_t>) * path_count);
	pdb_paths[0] = ECS_RUNTIME_PDB_PATHS[ecs_runtime_pdb_index];
	pdb_paths[1] = ECS_RUNTIME_PDB_PATHS[ecs_runtime_pdb_index + 1];

	ECS_STACK_CAPACITY_STREAM(wchar_t, pdb_characters, ECS_KB * 8);

	size_t pdb_index = 2;
	for (size_t index = 0; index < editor_state->project_modules->size; index++) {
		const wchar_t* current_path = pdb_characters.buffer + pdb_characters.size;

		// Add all the configurations as search paths - they will be different based on the name
		const EditorModule* current_module = editor_state->project_modules->buffer + index;
		for (size_t configuration_index = 0; configuration_index < (size_t)EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
			GetModuleFolder(editor_state, current_module->library_name, (EDITOR_MODULE_CONFIGURATION)index, pdb_characters);
			// Advance the '\0'
			pdb_characters.size++;
			pdb_paths[pdb_index++] = current_path;
		}
	}
	OS::SetSymbolicLinksPaths({ pdb_paths, path_count });
}

// -------------------------------------------------------------------------------------------------------------------------

void AddProjectModuleToLaunchedCompilation(EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration) {
	EDITOR_STATE(editor_state);
	// Allocate the string from the multithreaded because the deallocate would have interfered with the normal allocations
	// since the deallocation would be done from a different thread
	// Concatenate the library_name with _ and then the configuration string
	Stream<wchar_t> configuration_string = MODULE_CONFIGURATIONS_WIDE[(unsigned int)configuration];

	void* allocation = multithreaded_editor_allocator->Allocate_ts(sizeof(wchar_t) * (library_name.size + configuration_string.size + 1));
	uintptr_t buffer = (uintptr_t)allocation;
	library_name.CopyTo(buffer);
	wchar_t* underscore = (wchar_t*)buffer;
	*underscore = L'_';
	buffer += sizeof(wchar_t);

	configuration_string.CopyTo(buffer);

	editor_state->launched_module_compilation_lock.lock();
	editor_state->launched_module_compilation[configuration].Add({ allocation, library_name.size + configuration_string.size + 1 });
	editor_state->launched_module_compilation_lock.unlock();
}

// -------------------------------------------------------------------------------------------------------------------------

bool AddModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, bool is_graphics_module) {
	EDITOR_STATE(editor_state);
	ProjectModules* project_modules = editor_state->project_modules;

	if (!ExistsFileOrFolder(solution_path)) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Project module {#} solution does not exist.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(error_message);
		return false;
	}

	unsigned int module_index = GetModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Project module {#} already exists.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(error_message);
		return false;
	}

	module_index = project_modules->ReserveNewElement();
	project_modules->size++;
	EditorModule* module = project_modules->buffer + module_index;

	size_t total_size = (solution_path.size + library_name.size + 2) * sizeof(wchar_t);
	void* allocation = editor_allocator->Allocate(total_size, alignof(wchar_t));
	uintptr_t buffer = (uintptr_t)allocation;
	module->solution_path.InitializeFromBuffer(buffer, solution_path.size);
	buffer += sizeof(wchar_t);
	module->library_name.InitializeFromBuffer(buffer, library_name.size);
	buffer += sizeof(wchar_t);

	module->solution_path.Copy(solution_path);
	module->solution_path[module->solution_path.size] = L'\0';
	module->library_name.Copy(library_name);
	module->library_name[module->library_name.size] = L'\0';
	module->solution_last_write_time = 0;
	module->is_graphics_module = is_graphics_module;

	for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
		module->infos[index].load_status = EDITOR_MODULE_LOAD_FAILED;
		module->infos[index].ecs_module.code = ECS_GET_MODULE_FAULTY_PATH;

		module->infos[index].library_last_write_time = 0;
	}

	UpdateModuleLastWrite(editor_state, module_index);

	// Update the console such that it contains as filter string the module
	Console* console = GetConsole();
	ECS_STACK_CAPACITY_STREAM(char, ascii_name, 256);
	function::ConvertWideCharsToASCII(module->library_name, ascii_name);
	console->AddSystemFilterString(ascii_name);

	SetCrashHandlerPDBPaths(editor_state);

	// Try to load the module - if it is built already
	// We can ignore the return since here it is not mandatory that the load succeed

	ReflectModule(editor_state, module_index);

	// Try to load all configurations
	for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
		LoadEditorModule(editor_state, module_index, (EDITOR_MODULE_CONFIGURATION)index);
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, module_settings_folder, 512);
	GetModuleSettingsFolderPath(editor_state, module_index, module_settings_folder);
	if (!ExistsFileOrFolder(module_settings_folder)) {
		ECS_ASSERT(CreateFolder(module_settings_folder));
	}

	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

struct CheckBuildStatusThreadData {
	EditorState* editor_state;
	Stream<wchar_t> path;
	EDITOR_FINISH_BUILD_COMMAND_STATUS* report_status;
};

ECS_THREAD_TASK(CheckBuildStatusThreadTask) {
	CheckBuildStatusThreadData* data = (CheckBuildStatusThreadData*)_data;

	while (!ExistsFileOrFolder(data->path)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_FILE_STATUS_THREAD_SLEEP_TICK));
	}
	// Change the extension from .build/.rebuild/.clean to txt
	Stream<wchar_t> extension = function::PathExtension(data->path);
	ECS_TEMP_STRING(log_path, 512);

	log_path.Copy(data->path);
	Stream<wchar_t> log_extension = function::PathExtension(log_path);
	// Make the dot into an underscore
	log_extension[0] = L'_';
	log_path.AddStream(CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION);

	bool succeded = PrintCommandStatus(data->editor_state, log_path);
	// Extract the library name and the configuration
	Stream<wchar_t> filename = function::PathFilename(data->path);
	const wchar_t* underscore = wcschr(filename.buffer, '_');

	Stream<wchar_t> library_name(filename.buffer, underscore - filename.buffer);
	Stream<wchar_t> configuration(underscore + 1, extension.buffer - underscore - 1);
	EDITOR_MODULE_CONFIGURATION int_configuration = EDITOR_MODULE_CONFIGURATION_COUNT;

	if (function::CompareStrings(configuration, L"Debug")) {
		int_configuration = EDITOR_MODULE_CONFIGURATION_DEBUG;
	}
	else if (function::CompareStrings(configuration, L"Release")) {
		int_configuration = EDITOR_MODULE_CONFIGURATION_RELEASE;
	}
	else if (function::CompareStrings(configuration, L"Distribution")) {
		int_configuration = EDITOR_MODULE_CONFIGURATION_DISTRIBUTION;
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_string, "An error has occured when determining the configuration of module {#}.", library_name);
		EditorSetConsoleError(console_string);
	}

	if (succeded) {
		// Release the project streams and handle and reload it if is a build or rebuild command
		unsigned int module_index = GetModuleIndexFromName(data->editor_state, library_name);
		if (module_index != -1) {
			if (int_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
				ReleaseModuleStreamsAndHandle(data->editor_state, module_index, int_configuration);
			}
		}
		else {
			ECS_FORMAT_TEMP_STRING(console_string, "An error has occured when checking status of module {#}, configuration {#}. Could not locate module."
				"(was it removed?)", library_name, configuration);
			EditorSetConsoleError(console_string);
		}

		if (underscore != nullptr) {
			Stream<wchar_t> command(extension.buffer + 1, extension.size - 1);

			ECS_FORMAT_TEMP_STRING(console_string, "Command {#} for module {#} with configuration {#} completed successfully.", command, library_name, configuration);
			EditorSetConsoleInfo(console_string);
			if (function::CompareStrings(command, L"build") || function::CompareStrings(command, L"rebuild")) {
				bool success = LoadEditorModule(data->editor_state, module_index, int_configuration);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(error_message, "Could not reload module {#} with configuration {#}, command {#} after successfully executing the command.", library_name, configuration, command);
					EditorSetConsoleError(error_message);
				}
			}
		}
		else {
			EditorSetConsoleWarn("Could not deduce build command type, library name or configuration for a module compilation.");
			EditorSetConsoleInfo("A module finished building successfully.");
		}

		if (data->report_status != nullptr) {
			*data->report_status = EDITOR_FINISH_BUILD_COMMAND_OK;
		}

	}
	else if (data->report_status != nullptr) {
		*data->report_status = EDITOR_FINISH_BUILD_COMMAND_FAILED;
	}

	if (int_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
		Stream<wchar_t> launched_compilation_string(library_name.buffer, configuration.buffer + configuration.size - library_name.buffer);
		// Remove the module from the launched compilation stream
		data->editor_state->launched_module_compilation_lock.lock();
		unsigned int index = 0;
		unsigned int launched_module_count = data->editor_state->launched_module_compilation[int_configuration].size;
		for (; index < launched_module_count; index++) {
			if (function::CompareStrings(launched_compilation_string, data->editor_state->launched_module_compilation[int_configuration][index])) {
				// Deallocate the string
				data->editor_state->multithreaded_editor_allocator->Deallocate_ts(data->editor_state->launched_module_compilation[int_configuration][index].buffer);
				data->editor_state->launched_module_compilation[int_configuration].RemoveSwapBack(index);
				break;
			}
		}
		data->editor_state->launched_module_compilation_lock.unlock();

		// Warn if the module could not be found in the launched module compilation
		if (index == launched_module_count) {
			ECS_FORMAT_TEMP_STRING(console_string, "Module {#} with configuration {#} could not be found in the compilation list.", library_name, configuration);
			EditorSetConsoleWarn(console_string);
		}
	}

	// The path must be deallocated
	data->editor_state->multithreaded_editor_allocator->Deallocate_ts(data->path.buffer);
	ECS_ASSERT(RemoveFile(data->path));
}

// -------------------------------------------------------------------------------------------------------------------------

void ForEachProjectModule(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* statuses,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_statuses,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS(*build_function)(EditorState*, unsigned int, EDITOR_MODULE_CONFIGURATION, EDITOR_FINISH_BUILD_COMMAND_STATUS*)
) {
	EDITOR_STATE(editor_state);

	// Clear the log file first
	const ProjectModules* project_modules = (const ProjectModules*)editor_state->project_modules;

	if (build_statuses) {
		for (unsigned int index = 0; index < indices.size; index++) {
			build_statuses[index] = EDITOR_FINISH_BUILD_COMMAND_WAITING;
			statuses[index] = build_function(editor_state, indices[index], configurations[index], build_statuses + index);
		}

		auto have_finished = [build_statuses, indices]() {
			for (size_t index = 0; index < indices.size; index++) {
				if (build_statuses[index] == -1) {
					return false;
				}
			}
			return true;
		};

		while (!have_finished()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(BLOCK_THREAD_FOR_MODULE_STATUS_SLEEP_TICK));
		}
	}
	else {
		for (unsigned int index = 0; index < indices.size; index++) {
			statuses[index] = build_function(editor_state, indices[index], configurations[index], nullptr);
		}
	}

}

// -------------------------------------------------------------------------------------------------------------------------

// For system() - the CRT function
void CommandLineString(CapacityStream<char>& string, Stream<wchar_t> solution_path, Stream<char> command, Stream<char> configuration, Stream<wchar_t> log_file) {
	string.Copy(CMB_BUILD_SYSTEM_PATH);
	string.AddStream(" && ");
	string.AddStream(CMD_BUILD_SYSTEM);
	string.Add(' ');
	function::ConvertWideCharsToASCII(solution_path, string);
	string.Add(' ');
	string.AddStream(command);
	string.Add(' ');
	string.AddStream(configuration);
	string.Add(' ');
	string.AddStream(CMD_BUILD_SYSTEM_LOG_FILE_COMMAND);
	string.Add(' ');
	function::ConvertWideCharsToASCII(log_file, string);
	string[string.size] = '\0';
}

// For ShellExecute() - Win32 API
void CommandLineString(
	const EditorState* editor_state,
	CapacityStream<wchar_t>& string,
	unsigned int module_index,
	Stream<wchar_t> command,
	Stream<wchar_t> log_file,
	EDITOR_MODULE_CONFIGURATION configuration
) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;

	string.Copy(L"/c ");
	string.AddStream(CMD_BUILD_SYSTEM_WIDE);
	string.Add(L' ');
	string.AddStream(modules->buffer[module_index].solution_path);
	string.Add(L' ');
	string.AddStream(command);
	string.AddStream(L" /p:Configuration=");
	string.AddStream(MODULE_CONFIGURATIONS_WIDE[(unsigned int)configuration]);
	string.AddStream(L" /p:Platform=x64 /flp:logfile=");
	string.AddStream(log_file);
	string.AddStream(L";verbosity=normal & cd . > ");
	GetProjectDebugFolder(editor_state, string);
	string.Add(ECS_OS_PATH_SEPARATOR);
	GetModuleBuildFlagFile(editor_state, module_index, configuration, command, string);
	string[string.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

// Returns whether or not the command actually will be executed. The command can be skipped if the module is in flight
// running another command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS RunCmdCommand(
	EditorState* editor_state,
	unsigned int index,
	Stream<wchar_t> command,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* report_status = nullptr
) {
	EDITOR_STATE(editor_state);

	Stream<wchar_t> library_name = editor_state->project_modules->buffer[index].library_name;

	// First check that the module is not executing another build command - it doesn't necessarly 
	// have to be the same type. A clean cannot be executed while a build is running
	// The lock must be acquired so a thread that wants to remove an element 
	// does not interfere with this reading
	editor_state->launched_module_compilation_lock.lock();
	for (size_t index = 0; index < editor_state->launched_module_compilation[configuration].size; index++) {
		if (memcmp(library_name.buffer, editor_state->launched_module_compilation[configuration][index].buffer, sizeof(wchar_t) * library_name.size) == 0) {
			editor_state->launched_module_compilation_lock.unlock();
			return EDITOR_LAUNCH_BUILD_COMMAND_ALREADY_RUNNING;
		}
	}
	editor_state->launched_module_compilation_lock.unlock();

	// Add the module to the launched module compilation
	AddProjectModuleToLaunchedCompilation(editor_state, library_name, configuration);

	// Construct the system string
#ifdef MODULE_BUILD_USING_CRT
	ECS_TEMP_ASCII_STRING(command_string, 512);
#else
	ECS_TEMP_STRING(command_string, 512);
#endif

	wchar_t _log_path[512];
	Stream<wchar_t> log_path(_log_path, 0);
	GetModuleBuildLogPath(editor_state, index, configuration, command, log_path);

#ifdef MODULE_BUILD_USING_CRT
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	Stream<wchar_t> solution_path = modules->buffer[index].solution_path;
	Stream<wchar_t> library_name = modules->buffer[index].library_name;
	Stream<char> configuration = editor_state->module_configuration_definitions[(unsigned int)configuration];

	CommandLineString(command_string, solution_path, command, configuration, log_path);
#else
	CommandLineString(editor_state, command_string, index, command, log_path, configuration);
#endif

	// Run the command
#ifdef MODULE_BUILD_USING_CRT
	system(command_string.buffer);

	// Log the command status
	bool succeded = PrintCommandStatus(editor_state, log_path);

	if (succeded) {
		ECS_FORMAT_TEMP_STRING(console_string, "Command {#} for module {#} ({#}) with configuration {#} completed successfully.", command, library_name, solution_path, configuration);
		EditorSetConsoleInfoFocus(editor_state, console_string);
	}
#else
	HINSTANCE value = ShellExecute(NULL, L"runas", L"C:\\Windows\\System32\\cmd.exe", command_string.buffer, L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\MSBuild\\Current\\Bin", SW_HIDE);
	if ((uint64_t)value < 32) {
		EditorSetConsoleError("An error occured when creating the command prompt that builds the module.");
		return EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING;
	}
	else {
		ECS_TEMP_STRING(flag_file, 256);
		GetProjectDebugFolder(editor_state, flag_file);
		flag_file.Add(ECS_OS_PATH_SEPARATOR);
		GetModuleBuildFlagFile(editor_state, index, configuration, command, flag_file);

		// Log the command status
		CheckBuildStatusThreadData check_data;
		check_data.editor_state = editor_state;
		check_data.path.buffer = (wchar_t*)multithreaded_editor_allocator->Allocate_ts(sizeof(wchar_t) * (flag_file.size + 1));
		check_data.path.Copy(flag_file);
		check_data.report_status = report_status;

		task_manager->AddDynamicTaskAndWake(ECS_THREAD_TASK_NAME(CheckBuildStatusThreadTask, &check_data, sizeof(check_data)));
	}
	return EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING;
#endif
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_LAUNCH_BUILD_COMMAND_STATUS BuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* report_status
) {
	ProjectModules* modules = editor_state->project_modules;
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);

	if (UpdateModuleLastWrite(editor_state, index, configuration) || info->load_status != EDITOR_MODULE_LOAD_GOOD) {
		info->load_status = EDITOR_MODULE_LOAD_FAILED;
		return RunCmdCommand(editor_state, index, BUILD_PROJECT_STRING_WIDE, configuration, report_status);
	}
	// The callback will check the status of this function and report accordingly to the console
	return EDITOR_LAUNCH_BUILD_COMMAND_SKIPPED;
}

// -------------------------------------------------------------------------------------------------------------------------

bool BuildModulesAndLoad(EditorState* editor_state, Stream<unsigned int> module_indices, EDITOR_MODULE_CONFIGURATION* configurations)
{
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses = (EDITOR_LAUNCH_BUILD_COMMAND_STATUS*)ECS_STACK_ALLOC(sizeof(EDITOR_LAUNCH_BUILD_COMMAND_STATUS) * module_indices.size);
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_status = (EDITOR_FINISH_BUILD_COMMAND_STATUS*)ECS_STACK_ALLOC(sizeof(EDITOR_FINISH_BUILD_COMMAND_STATUS) * module_indices.size);

	BuildModules(editor_state, module_indices, configurations, launch_statuses, build_status);

	for (size_t index = 0; index < module_indices.size; index++) {
		if (launch_statuses[index] == EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING || build_status[index] == 0) {
			return false;
		}
	}

	// Go through all modules and try to load their modules
	for (size_t index = 0; index < module_indices.size; index++) {
		bool load_success = LoadEditorModule(editor_state, module_indices[index], configurations[index]);
		if (!load_success) {
			return false;
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

void BuildModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_statuses
)
{
	ForEachProjectModule(editor_state, indices, configurations, launch_statuses, build_statuses, BuildModule);
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_LAUNCH_BUILD_COMMAND_STATUS CleanModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_status
) {
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	info->load_status = EDITOR_MODULE_LOAD_STATUS::EDITOR_MODULE_LOAD_FAILED;
	return RunCmdCommand(editor_state, index, CLEAN_PROJECT_STRING_WIDE, configuration);
}

// -------------------------------------------------------------------------------------------------------------------------

void CleanModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_statuses
) {
	ForEachProjectModule(editor_state, indices, configurations, launch_statuses, build_statuses, CleanModule);
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_LAUNCH_BUILD_COMMAND_STATUS RebuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_status
) {
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	info->load_status = EDITOR_MODULE_LOAD_FAILED;
	return RunCmdCommand(editor_state, index, REBUILD_PROJECT_STRING_WIDE, configuration);
}

// -------------------------------------------------------------------------------------------------------------------------

void RebuildModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_statuses
) {
	ForEachProjectModule(editor_state, indices, configurations, launch_statuses, build_statuses, RebuildModule);
}

// -------------------------------------------------------------------------------------------------------------------------

void DeleteModuleFlagFiles(EditorState* editor_state)
{
	ECS_TEMP_STRING(debug_path, 512);
	GetProjectDebugFolder(editor_state, debug_path);

	auto functor = [](Stream<wchar_t> path, void* _data) {
		bool success = RemoveFile(path);
		if (!success) {
			EditorSetConsoleWarn("Could not delete a build flag file. A manual deletion should be done.");
		}
		return true;
	};

	Stream<wchar_t> extensions[] = {
		L".build",
		L".clean",
		L".rebuild"
	};
	ForEachFileInDirectoryWithExtension(debug_path, { extensions, std::size(extensions) }, editor_state, functor);
}

// -------------------------------------------------------------------------------------------------------------------------

void InitializeModuleConfigurations(EditorState* editor_state)
{
	/*EDITOR_STATE(editor_state);

	constexpr size_t count = (unsigned int)EDITOR_MODULE_CONFIGURATION::EDITOR_MODULE_CONFIGURATION_COUNT;
	size_t total_memory = sizeof(Stream<char>) * count;

	size_t string_sizes[count];
	string_sizes[EDITOR_MODULE_CONFIGURATION_DEBUG] = strlen(MODULE_CONFIGURATION_DEBUG);
	string_sizes[EDITOR_MODULE_CONFIGURATION_RELEASE] = strlen(MODULE_CONFIGURATION_RELEASE);
	string_sizes[EDITOR_MODULE_CONFIGURATION_DISTRIBUTION] = strlen(MODULE_CONFIGURATION_DISTRIBUTION);

	for (size_t index = 0; index < count; index++) {
		total_memory += string_sizes[index];
	}

	void* allocation = editor_allocator->Allocate(total_memory);
	uintptr_t buffer = (uintptr_t)allocation;
	editor_state->module_configuration_definitions.InitializeFromBuffer(buffer, count);

	for (size_t index = 0; index < count; index++) {
		editor_state->module_configuration_definitions[index].InitializeFromBuffer(buffer, string_sizes[index]);
		editor_state->module_configuration_definitions[index].Copy(Stream<char>(MODULE_CONFIGURATIONS[index], string_sizes[index]));
	}*/
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsEditorModuleLoaded(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	return info->ecs_module.code == ECS_GET_MODULE_OK;
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsGraphicsModule(const EditorState* editor_state, unsigned int index)
{
	return editor_state->project_modules->buffer[index].is_graphics_module;
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned int GetModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path)
{
	EDITOR_STATE(editor_state);

	ProjectModules* modules = editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		if (function::CompareStrings(modules->buffer[index].solution_path, solution_path)) {
			return index;
		}
	}
	return -1;
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned int GetModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_name) {
	EDITOR_STATE(editor_state);

	ProjectModules* modules = editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		if (function::CompareStrings(modules->buffer[index].library_name, library_name)) {
			return index;
		}
	}
	return -1;
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleBuildFlagFile(
	const EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	Stream<wchar_t> command,
	CapacityStream<wchar_t>& temp_file
)
{
	command.buffer++;
	command.size--;

	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	temp_file.AddStreamSafe(modules->buffer[module_index].library_name);
	temp_file.Add(L'_');
	temp_file.AddStream(MODULE_CONFIGURATIONS_WIDE[configuration]);
	temp_file.Add(L'.');
	temp_file.AddStream(Stream<wchar_t>(command.buffer + 2, command.size - 2));
	temp_file[temp_file.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleBuildLogPath(
	const EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	Stream<wchar_t> command,
	Stream<wchar_t>& log_path
)
{
	EDITOR_STATE(editor_state);

	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	log_path.Copy(project_file->path);
	log_path.AddStream(CMD_BUILD_SYSTEM_LOG_FILE_PATH);
	log_path.AddStream(modules->buffer[index].library_name);
	log_path.Add(L'_');
	log_path.AddStream(MODULE_CONFIGURATIONS_WIDE[configuration]);
	wchar_t* replace_slash_with_underscore = log_path.buffer + log_path.size;
	log_path.AddStream(Stream<wchar_t>(command.buffer + 2, command.size - 2));
	*replace_slash_with_underscore = L'_';
	log_path.AddStream(CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION);
	log_path[log_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

EditorModuleInfo* GetModuleInfo(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration)
{
	return &editor_state->project_modules->buffer[index].infos[configuration];
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModulesFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path) {
	EDITOR_STATE(editor_state);

	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStreamSafe(PROJECT_MODULES_RELATIVE_PATH);
	path[path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path)
{
	GetModuleFolder(editor_state, library_name, configuration, module_path);
	module_path.Add(ECS_OS_PATH_SEPARATOR);
	module_path.AddStream(library_name);
	module_path.AddStreamSafe(ECS_MODULE_EXTENSION);
	module_path[module_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleFolder(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path)
{
	GetModulesFolder(editor_state, module_path);
	module_path.Add(ECS_OS_PATH_SEPARATOR);
	// This is the folder part
	module_path.AddStream(library_name);
	module_path.Add(ECS_OS_PATH_SEPARATOR);
	module_path.AddStream(MODULE_CONFIGURATIONS_WIDE[(unsigned int)configuration]);
	module_path[module_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetModuleSolutionLastWrite(Stream<wchar_t> solution_path)
{
	Stream<wchar_t> parent_folder = function::PathParent(solution_path);

	struct FolderData {
		Stream<Stream<wchar_t>> source_names;
		size_t last_write;
	};

	Stream<wchar_t> _source_names[std::size(MODULE_SOURCE_FILES)];
	FolderData data;
	data.source_names = Stream<Stream<wchar_t>>(_source_names, std::size(MODULE_SOURCE_FILES));
	for (size_t index = 0; index < data.source_names.size; index++) {
		data.source_names[index] = MODULE_SOURCE_FILES[index];
	}
	data.last_write = 0;

	auto folder_functor = [](Stream<wchar_t> path, void* _data) {
		FolderData* data = (FolderData*)_data;

		Stream<wchar_t> filename = function::PathFilename(path);

		if (function::FindString(filename, data->source_names) != -1) {
			bool success = OS::GetFileTimes(path, nullptr, nullptr, &data->last_write);
			return false;
		}

		return true;
	};

	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(solution_path);
	null_terminated_path.AddSafe(L'\0');
	size_t solution_last_write = 0;

	bool success = OS::GetFileTimes(null_terminated_path.buffer, nullptr, nullptr, &solution_last_write);
	ForEachDirectory(parent_folder, &data, folder_functor);
	return std::max(data.last_write, solution_last_write);
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetModuleLibraryLastWrite(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration) {
	ECS_TEMP_STRING(module_path, 256);
	GetModulePath(editor_state, library_name, configuration, module_path);

	size_t last_write = 0;
	bool success = OS::GetFileTimes(module_path.buffer, nullptr, nullptr, &last_write);
	return last_write;
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetModuleSolutionLastWrite(const EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return modules->buffer[index].solution_last_write_time;
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetModuleLibraryLastWrite(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	return GetModuleInfo(editor_state, index, configuration)->library_last_write_time;
}

// -------------------------------------------------------------------------------------------------------------------------

bool GetModuleReflectSolutionPath(const EditorState* editor_state, unsigned int index, CapacityStream<wchar_t>& path)
{
	Stream<wchar_t> solution_path = editor_state->project_modules->buffer[index].solution_path;
	Stream<wchar_t> solution_parent = function::PathParent(solution_path);
	path.Copy(solution_parent);
	path.Add(ECS_OS_PATH_SEPARATOR);

	size_t base_path_size = path.size;
	for (size_t index = 0; index < std::size(MODULE_SOURCE_FILES); index++) {
		path.AddStream(MODULE_SOURCE_FILES[index]);
		if (ExistsFileOrFolder(path)) {
			path[path.size] = L'\0';
			return true;
		}
		path.size = base_path_size;
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned int GetModuleReflectionHierarchyIndex(const EditorState* editor_state, unsigned int module_index)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, reflection_path, 512);
	bool success = GetModuleReflectSolutionPath(editor_state, module_index, reflection_path);
	if (success) {
		return editor_state->module_reflection->reflection->GetHierarchyIndex(reflection_path);
	}
	return -1;
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleReflectionComponentIndices(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>* indices)
{
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	ECS_ASSERT(hierarchy_index != -1);

	Stream<char> tag = ECS_COMPONENT_TAG;
	Reflection::ReflectionManagerGetQuery query;
	query.indices = indices;
	query.strict_compare = true;
	query.tags = { &tag, 1 };
	editor_state->module_reflection->reflection->GetHierarchyTypes(hierarchy_index, query);
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleReflectionSharedComponentIndices(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>* indices)
{
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	ECS_ASSERT(hierarchy_index != -1);

	Stream<char> tag = ECS_COMPONENT_TAG;
	Reflection::ReflectionManagerGetQuery query;
	query.indices = indices;
	query.strict_compare = true;
	query.tags = { &tag, 1 };
	editor_state->module_reflection->reflection->GetHierarchyTypes(hierarchy_index, query);
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleReflectionAllComponentIndices(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>* unique_indices, CapacityStream<unsigned int>* shared_indices)
{
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	ECS_ASSERT(hierarchy_index != -1);

	Stream<char> tags[] = {
		ECS_COMPONENT_TAG,
		ECS_SHARED_COMPONENT_TAG
	};

	CapacityStream<unsigned int> indices[] = {
		*unique_indices,
		*shared_indices
	};

	Stream<char> tag = ECS_COMPONENT_TAG;
	Reflection::ReflectionManagerGetQuery query;
	query.stream_indices = { indices, std::size(indices) };
	query.use_stream_indices = true;
	query.strict_compare = true;
	query.tags = { tags, std::size(tags) };

	editor_state->module_reflection->reflection->GetHierarchyTypes(hierarchy_index, query);

	// We need to update the sizes of the indices now
	unique_indices->size = query.stream_indices[0].size;
	shared_indices->size = query.stream_indices[1].size;
}

// -------------------------------------------------------------------------------------------------------------------------

bool CreateEditorModuleTemporaryDLL(CapacityStream<wchar_t> library_path, CapacityStream<wchar_t>& temporary_path) {
	temporary_path.Copy(library_path);
	temporary_path.size -= wcslen(ECS_MODULE_EXTENSION);
	temporary_path.AddStream(L"_temp");
	temporary_path.AddStreamSafe(ECS_MODULE_EXTENSION);

	// Copy the .dll to a temporary dll such that it will allow building the module again
	return FileCopy(library_path, temporary_path, false, true);
}

// -------------------------------------------------------------------------------------------------------------------------

bool LoadEditorModule(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);

	// Check to see if the module is out of date or not yet loaded
	if (info->load_status == EDITOR_MODULE_LOAD_GOOD) {
		// Update the library and the solution
		bool needs_update = UpdateModuleLastWrite(editor_state, index, configuration);
		if (!needs_update) {
			// The module is already up to date and no modifications need to be done
			return true;
		}
	}

	// If the module is already loaded, release it
	if (IsEditorModuleLoaded(editor_state, index, configuration)) {
		ReleaseModuleStreamsAndHandle(editor_state, index, configuration);
		info->ecs_module.code = ECS_GET_MODULE_FAULTY_PATH;
	}

	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	Stream<wchar_t> library_name = modules->buffer[index].library_name;
	ECS_TEMP_STRING(library_path, 512);
	GetModulePath(editor_state, library_name, configuration, library_path);

	ECS_TEMP_STRING(temporary_library, 512);
	if (ExistsFileOrFolder(library_path)) {
		// Copy the .dll to a temporary dll such that it will allow building the module again
		bool copy_success = CreateEditorModuleTemporaryDLL(library_path, temporary_library);
		if (copy_success) {
			info->ecs_module = LoadModule(temporary_library);
			// The load succeded, now try to retrieve the streams for this module
			if (info->ecs_module.code == ECS_GET_MODULE_OK) {
				AllocatorPolymorphic allocator = GetAllocatorPolymorphic(editor_state->editor_allocator);
				const Module* ecs_module = &info->ecs_module;

				info->build_asset_types = LoadModuleBuildAssetTypes(ecs_module, allocator);
				info->link_components = LoadModuleLinkComponentTargets(ecs_module, allocator);
				info->serialize_streams = LoadModuleSerializeComponentFunctors(ecs_module, allocator);
				info->tasks = LoadModuleTasks(ecs_module, allocator);
				info->ui_descriptors = LoadModuleUIDescriptors(ecs_module, allocator);

				info->load_status = EDITOR_MODULE_LOAD_GOOD;
				return true;
			}
			info->load_status = EDITOR_MODULE_LOAD_FAILED;
			return false;
		}
		else {
			ECS_FORMAT_TEMP_STRING(error_message, "Could not create temporary module for {#}. The module tasks have not been loaded.", library_path);
			EditorSetConsoleError(error_message);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(error_message, "The dll for module {#} with configuration {#} is missing.", library_name, MODULE_CONFIGURATIONS[configuration]);
		EditorSetConsoleWarn(error_message);
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

void ReflectModule(EditorState* editor_state, unsigned int index)
{
	EditorModule* module = editor_state->project_modules->buffer + index;

	ECS_TEMP_STRING(source_path, 512);
	bool success = GetModuleReflectSolutionPath(editor_state, editor_state->project_modules->size - 1, source_path);
	if (success) {
		unsigned int folder_hierarchy = editor_state->module_reflection->reflection->GetHierarchyIndex(source_path);
		if (folder_hierarchy == -1) {
			folder_hierarchy = editor_state->module_reflection->reflection->CreateFolderHierarchy(source_path);
		}
		else {
			ReleaseModuleReflection(editor_state, index);
		}

		ECS_STACK_CAPACITY_STREAM(char, error_message, 2048);

		// Launch thread tasks to process this new entry
		success = editor_state->module_reflection->reflection->ProcessFolderHierarchy(folder_hierarchy, editor_state->task_manager, &error_message);

		if (!success) {
			ECS_FORMAT_TEMP_STRING(
				console_message, 
				"Could not reflect the new added module {#} at {#}. Detailed error: {#}",
				module->library_name, 
				module->solution_path,
				error_message
			);
			EditorSetConsoleWarn(console_message);
		}
		else {
			unsigned int types_created = editor_state->module_reflection->CreateTypesForHierarchy(folder_hierarchy);

			// Convert all stream types to resizable
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, type_indices, types_created);
			UIReflectionDrawerSearchOptions options;
			options.indices = &type_indices;
			editor_state->module_reflection->GetHierarchyTypes(folder_hierarchy, options);

			for (unsigned int index = 0; index < type_indices.size; index++) {
				UIReflectionType* type = editor_state->module_reflection->GetTypePtr(type_indices[index]);
				editor_state->module_reflection->ConvertTypeStreamsToResizable(type);
			}

			// Inform the inspectors about the change
			UpdateInspectorUIModuleSettings(editor_state, index);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Could not find source folder for module {#} at {#}.", module->library_name, module->solution_path);
		EditorSetConsoleWarn(console_message);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_MODULE_CONFIGURATION GetModuleLoadedConfiguration(const EditorState* editor_state, unsigned int module_index)
{
	const EditorModule* module = editor_state->project_modules->buffer + module_index;
	for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
		if (module->infos[EDITOR_MODULE_CONFIGURATION_COUNT - 1 - index].load_status == EDITOR_MODULE_LOAD_GOOD) {
			return (EDITOR_MODULE_CONFIGURATION)(EDITOR_MODULE_CONFIGURATION_COUNT - 1 - index);
		}
	}

	for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
		if (module->infos[EDITOR_MODULE_CONFIGURATION_COUNT - 1 - index].load_status == EDITOR_MODULE_LOAD_OUT_OF_DATE) {
			return (EDITOR_MODULE_CONFIGURATION)(EDITOR_MODULE_CONFIGURATION_COUNT - 1 - index);
		}
	}

	return EDITOR_MODULE_CONFIGURATION_COUNT;
}

// -------------------------------------------------------------------------------------------------------------------------

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration)
{
	ECS_TEMP_STRING(library_path, 256);
	GetModulePath(editor_state, library_name, configuration, library_path);
	return IsModule(library_path);
}

// -------------------------------------------------------------------------------------------------------------------------

bool HasModuleFunction(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return HasModuleFunction(editor_state, modules->buffer[index].library_name, configuration);
}

// -------------------------------------------------------------------------------------------------------------------------

void ReleaseModuleStreamsAndHandle(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration)
{
	if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
		ReleaseModuleStreamsAndHandle(editor_state, index, EDITOR_MODULE_CONFIGURATION_DEBUG);
		ReleaseModuleStreamsAndHandle(editor_state, index, EDITOR_MODULE_CONFIGURATION_RELEASE);
		ReleaseModuleStreamsAndHandle(editor_state, index, EDITOR_MODULE_CONFIGURATION_DISTRIBUTION);
		return;
	}

	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(editor_state->editor_allocator);

	if (info->tasks.buffer != nullptr) {
		Deallocate(allocator, info->tasks.buffer);
		info->tasks.buffer = nullptr;
	}

	if (info->ui_descriptors.buffer != nullptr) {
		Deallocate(allocator, info->ui_descriptors.buffer);
		info->ui_descriptors.buffer = nullptr;
	}

	if (info->build_asset_types.buffer != nullptr) {
		Deallocate(allocator, info->build_asset_types.buffer);
		info->build_asset_types.buffer = nullptr;
	}

	if (info->link_components.buffer != nullptr) {
		Deallocate(allocator, info->link_components.buffer);
		info->link_components.buffer = nullptr;
	}

	if (info->serialize_streams.GetAllocatedBuffer() != nullptr) {
		Deallocate(allocator, info->serialize_streams.GetAllocatedBuffer());
		info->serialize_streams.serialize_components.buffer = nullptr;
	}

	ReleaseModule(&info->ecs_module);
}

// -------------------------------------------------------------------------------------------------------------------------

bool UpdateModuleLastWrite(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration)
{
	ProjectModules* modules = editor_state->project_modules;
	bool is_solution_updated = UpdateModuleSolutionLastWrite(editor_state, index);
	bool is_library_updated = UpdateModuleLibraryLastWrite(editor_state, index, configuration);
	return is_solution_updated || is_library_updated;
}

// -------------------------------------------------------------------------------------------------------------------------

void UpdateModulesLastWrite(EditorState* editor_state) {
	ProjectModules* modules = editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		UpdateModuleLastWrite(editor_state, index);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

bool UpdateModuleSolutionLastWrite(EditorState* editor_state, unsigned int index) {
	ProjectModules* modules = editor_state->project_modules;

	size_t solution_last_write = modules->buffer[index].solution_last_write_time;
	modules->buffer[index].solution_last_write_time = GetModuleSolutionLastWrite(modules->buffer[index].solution_path);
	return solution_last_write < modules->buffer[index].solution_last_write_time || modules->buffer[index].solution_last_write_time == 0;
}

// -------------------------------------------------------------------------------------------------------------------------

bool UpdateModuleLibraryLastWrite(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
		// Update all
		return UpdateModuleLibraryLastWrite(editor_state, index, EDITOR_MODULE_CONFIGURATION_DEBUG) ||
			UpdateModuleLibraryLastWrite(editor_state, index, EDITOR_MODULE_CONFIGURATION_RELEASE) ||
			UpdateModuleLibraryLastWrite(editor_state, index, EDITOR_MODULE_CONFIGURATION_DISTRIBUTION);
	}

	ProjectModules* modules = editor_state->project_modules;
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);

	size_t library_last_write = info->library_last_write_time;
	info->library_last_write_time = GetModuleLibraryLastWrite(editor_state, modules->buffer[index].library_name, configuration);
	return library_last_write < info->library_last_write_time || info->library_last_write_time == 0;
}

// -------------------------------------------------------------------------------------------------------------------------

void ReleaseModule(EditorState* editor_state, unsigned int index) {
	EDITOR_STATE(editor_state);

	ProjectModules* modules = editor_state->project_modules;
	editor_allocator->Deallocate(modules->buffer[index].solution_path.buffer);

	for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
		if (IsEditorModuleLoaded(editor_state, index, (EDITOR_MODULE_CONFIGURATION)configuration_index)) {
			ReleaseModuleStreamsAndHandle(editor_state, index);
		}
		RemoveModuleAssociatedFiles(editor_state, index, (EDITOR_MODULE_CONFIGURATION)configuration_index);
	}

	ReleaseModuleReflection(editor_state, index);
	SetCrashHandlerPDBPaths(editor_state);
}

// -------------------------------------------------------------------------------------------------------------------------

void ReleaseModuleReflection(EditorState* editor_state, unsigned int index)
{
	// Destroy all the types for this module
	ECS_TEMP_STRING(source_path, 512);
	bool success = GetModuleReflectSolutionPath(editor_state, index, source_path);
	if (success) {
		// Release the UI types
		unsigned int hierarchy_index = editor_state->module_reflection->reflection->GetHierarchyIndex(source_path);
		ECS_ASSERT(hierarchy_index != -1);
		editor_state->module_reflection->DestroyAllFromFolderHierarchy(hierarchy_index);
		editor_state->module_reflection->reflection->FreeFolderHierarchy(hierarchy_index);
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Could not find module's {#} source folder.", editor_state->project_modules->buffer[index].library_name);
		EditorSetConsoleWarn(console_message);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveModule(EditorState* editor_state, unsigned int index) {
	EDITOR_STATE(editor_state);

	ProjectModules* modules = editor_state->project_modules;
	ReleaseModule(editor_state, index);
	modules->Remove(index);
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveModule(EditorState* editor_state, Stream<wchar_t> solution_path)
{
	EDITOR_STATE(editor_state);

	unsigned int module_index = GetModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		RemoveModule(editor_state, module_index);
		return;
	}
	ECS_TEMP_ASCII_STRING(error_message, 256);
	error_message.size = function::FormatString(error_message.buffer, "Removing project module {#} failed. No such module exists.", solution_path);
	EditorSetConsoleError(error_message);
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveModuleAssociatedFiles(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	EDITOR_STATE(editor_state);
	ProjectModules* modules = editor_state->project_modules;

	// Delete the associated files
	Stream<Stream<wchar_t>> associated_file_extensions(MODULE_ASSOCIATED_FILES, MODULE_ASSOCIATED_FILES_SIZE());

	ECS_TEMP_STRING(path, 256);
	GetModuleFolder(editor_state, modules->buffer[module_index].library_name, configuration, path);
	size_t path_size = path.size;

	for (size_t index = 0; index < MODULE_ASSOCIATED_FILES_SIZE(); index++) {
		path.size = path_size;
		path.AddStreamSafe(MODULE_ASSOCIATED_FILES[index]);
		if (ExistsFileOrFolder(path)) {
			OS::DeleteFileWithError(path);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveModuleAssociatedFiles(EditorState* editor_state, Stream<wchar_t> solution_path, EDITOR_MODULE_CONFIGURATION configuration)
{
	unsigned int module_index = GetModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		RemoveModuleAssociatedFiles(editor_state, module_index, configuration);
		ECS_FORMAT_TEMP_STRING(error_message, "Could not find module with solution path {#}.", solution_path);
		EditorSetConsoleError(error_message);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void ResetModules(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	ProjectModules* project_modules = editor_state->project_modules;
	for (size_t index = 0; index < project_modules->size; index++) {
		ReleaseModule(editor_state, index);
	}
	project_modules->Reset();
}

// -------------------------------------------------------------------------------------------------------------------------

void SetModuleLoadStatus(EditorModule* module, bool has_functions, EDITOR_MODULE_CONFIGURATION configuration)
{
	EditorModuleInfo* info = &module->infos[configuration];

	bool library_write_greater_than_solution = info->library_last_write_time >= module->solution_last_write_time;
	info->load_status = (EDITOR_MODULE_LOAD_STATUS)((has_functions + library_write_greater_than_solution) * has_functions);
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state)
{
	ECS_TEMP_STRING(module_path, 256);
	GetModulesFolder(editor_state, module_path);

	auto file_functor = [](Stream<wchar_t> path, void* data) {
		Stream<wchar_t> filename = function::PathFilename(path);
		if (function::FindFirstToken(filename, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER).buffer != nullptr) {
			size_t* count = (size_t*)data;
			*count += GetFileByteSize(path);
		}
		return true;
	};

	size_t count = 0;
	ForEachFileInDirectoryRecursive(module_path, &count, file_functor);
	return count;
}

// -------------------------------------------------------------------------------------------------------------------------

void DeleteVisualStudioLockedFiles(const EditorState* editor_state) {
	ECS_TEMP_STRING(module_path, 256);
	GetModulesFolder(editor_state, module_path);

	auto file_functor = [](Stream<wchar_t> path, void* data) {
		Stream<wchar_t> filename = function::PathFilename(path);
		if (function::FindFirstToken(filename, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER).buffer != nullptr) {
			RemoveFile(path);
		}
		return true;
	};
	ForEachFileInDirectoryRecursive(module_path, nullptr, file_functor);
}

// -------------------------------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------------------