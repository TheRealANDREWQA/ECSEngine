#include "editorpch.h"
#include "Module.h"
#include "ECSEngineUtilities.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorEvent.h"
#include "../Project/ProjectOperations.h"
#include "ECSEngineWorld.h"
#include "ECSEngineLinkComponents.h"
#include "ModuleSettings.h"
#include "../UI/Inspector.h"
#include "../Sandbox/SandboxModule.h"

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
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Debug\\ECSEngine",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Debug\\Editor",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Release\\ECSEngine",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Release\\Editor",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Distribution\\ECSEngine",
	L"C:\\Users\\Andrei\\C++\\ECSEngine\\bin\\Distribution\\Editor"
};

constexpr unsigned int CHECK_FILE_STATUS_THREAD_SLEEP_TICK = 200;
#define BLOCK_THREAD_FOR_MODULE_STATUS_SLEEP_TICK 20

#define MODULE_BUILD_USING_SHELL
#define LAZY_UPDATE_MODULE_DLL_IMPORTS_MS 300

// -------------------------------------------------------------------------------------------------------------------------

static void OpenModuleLogFile(Stream<wchar_t> log_path) {
	// TODO: Should we change the name of this function?
	// Now it opens the file and determines the errors and prints
	// Them in the ECS console
	//OS::LaunchFileExplorerWithError(log_path);

	auto print_errors = [](Stream<char> messages_start) {
		// There are 2 or 3 \n after the messages block
		Stream<char> messages_end = FindFirstToken(messages_start, "\n\n");
		messages_start.Advance();
		Stream<char> line_end = FindFirstCharacter(messages_start, '\n');
		while (line_end.size > 0 && line_end.buffer <= messages_end.buffer) {
			Stream<char> current_line = messages_start.StartDifference(line_end);
			Stream<char> error_token = FindFirstToken(current_line, " error ");
			if (error_token.size > 0) {
				// The line contains an error
				EditorSetConsoleError(current_line);
			}
			messages_start = line_end.AdvanceReturn();
			line_end = FindFirstCharacter(messages_start, '\n');
		}
	};

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);
	Stream<char> file_content = ReadWholeFileText(log_path, &stack_allocator);
	// Look for the Build FAILED token. Then the errors are listed there
	Stream<char> build_failed = FindFirstToken(file_content, "Build FAILED");
	if (build_failed.size > 0) {
		// There are 2 things that we are interested in
		// Compiler errors and linker errors
		// Compiler errors and warnings are listed on a separate line after a line with (ClCompile target) ->
		// Linker errors are listed on a separate line after a line with (Link target) ->
		// There can be multiple of (Link target) -> and (ClCompile target)
		Stream<char> COMPILE_STRING = "(ClCompile target) ->";
		Stream<char> LINKER_STRING = "(Link target) ->";

		Stream<char> compiler_errors_or_warnings = FindFirstToken(build_failed, COMPILE_STRING);
		while (compiler_errors_or_warnings.size > 0) {
			Stream<char> compiler_messages_start = FindFirstCharacter(compiler_errors_or_warnings, '\n');
			if (compiler_messages_start.size > 0) {
				print_errors(compiler_messages_start);
			}
			compiler_errors_or_warnings.Advance(COMPILE_STRING.size);
			compiler_errors_or_warnings = FindFirstToken(compiler_errors_or_warnings, COMPILE_STRING);
		}

		Stream<char> linker_errors = FindFirstToken(build_failed, LINKER_STRING);
		while (linker_errors.size > 0) {
			Stream<char> linker_messages_start = FindFirstCharacter(linker_errors, '\n');
			if (linker_messages_start.size > 0) {
				print_errors(linker_messages_start);
			}
			linker_errors.Advance(LINKER_STRING.size);
			linker_errors = FindFirstToken(linker_errors, LINKER_STRING);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(message, "Could not open and read log file {#}", log_path);
		EditorSetConsoleWarn(message);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

static bool PrintCommandStatus(EditorState* editor_state, Stream<wchar_t> log_path, bool disable_logging) {
	Stream<char> contents = ReadWholeFileText(log_path, editor_state->MultithreadedEditorAllocator());

	if (contents.buffer != nullptr) {
		auto contents_deallocator = StackScope([editor_state, contents]() {
			contents.Deallocate(editor_state->MultithreadedEditorAllocator());
		});

		Stream<char> build_FAILED = FindFirstToken(contents, "Build FAILED.");

		if (build_FAILED.size > 0) {
			// Extract the module name from the log path
			Stream<wchar_t> debug_ptr = FindFirstToken(log_path, CMD_BUILD_SYSTEM_LOG_FILE_PATH);
			if (debug_ptr.size > 0) {
				debug_ptr.Advance(wcslen(CMD_BUILD_SYSTEM_LOG_FILE_PATH));
				Stream<wchar_t> underscore_after_library_name = FindFirstCharacter(debug_ptr, L'_');
				if (underscore_after_library_name.size > 0) {
					Stream<wchar_t> configuration_start = underscore_after_library_name.AdvanceReturn();
					Stream<wchar_t> underscore_after_configuration = FindFirstCharacter(configuration_start, L'_');
					if (underscore_after_configuration.size > 0) {
						if (!disable_logging) {
							ECS_FORMAT_TEMP_STRING(error_message, "Module {#} with configuration {#} has failed to build. Check the {#} log.",
								Stream<wchar_t>(debug_ptr.buffer, underscore_after_library_name.buffer - debug_ptr.buffer),
								Stream<wchar_t>(configuration_start.buffer, underscore_after_configuration.buffer - configuration_start.buffer),
								CMD_BUILD_SYSTEM
							);
							EditorSetConsoleError(error_message);
							
							OpenModuleLogFile(log_path);
						}
					}
					else {
						if (!disable_logging) {
							ECS_FORMAT_TEMP_STRING(error_message, "Module {#} has failed to build. Check the {#} log.",
								Stream<wchar_t>(debug_ptr.buffer, underscore_after_library_name.buffer - debug_ptr.buffer), CMD_BUILD_SYSTEM);
							EditorSetConsoleError(error_message);
							OpenModuleLogFile(log_path);
						}
					}
					return false;
				}
				else {
					if (!disable_logging) {
						ECS_FORMAT_TEMP_STRING(error_message, "A module failed to build. Could not deduce library name (it's missing the underscore)."
							" Check the {#} log.", CMD_BUILD_SYSTEM);
						EditorSetConsoleError(error_message);
					}
					return false;
				}
			}
			else {
				if (!disable_logging) {
					ECS_FORMAT_TEMP_STRING(error_message, "A module failed to build. Could not deduce library name. Check the {#} log.", CMD_BUILD_SYSTEM);
					EditorSetConsoleError(error_message);
					OpenModuleLogFile(log_path);
				}
				return false;
			}
		}
	}
	else {
		if (!disable_logging) {
			ECS_FORMAT_TEMP_STRING(error_message, "Could not open {#} to read the log. Open the file externally to check the command status.", log_path);
			EditorSetConsoleWarn(error_message);
			OpenModuleLogFile(log_path);
		}
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

	// The .pdbs are all located in the Modules folder
	size_t path_count = 3;
	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, pdb_paths, 3);
	pdb_paths[0] = ECS_RUNTIME_PDB_PATHS[ecs_runtime_pdb_index];
	pdb_paths[1] = ECS_RUNTIME_PDB_PATHS[ecs_runtime_pdb_index + 1];

	ECS_STACK_CAPACITY_STREAM(wchar_t, pdb_characters, ECS_KB);
	// Also add the modules folder
	GetProjectModulesFolder(editor_state, pdb_characters);
	pdb_paths[2] = pdb_characters;

	pdb_paths.size = pdb_paths.capacity;
	OS::SetSymbolicLinksPaths(pdb_paths);
}

// -------------------------------------------------------------------------------------------------------------------------

// Only the implementation part, no locking
void AddProjectModuleToLaunchedCompilationNoLock(EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration) {
	editor_state->launched_module_compilation[configuration].Add(StringCopy(editor_state->MultithreadedEditorAllocator(), library_name));
}

void AddProjectModuleToLaunchedCompilation(EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration) {
	// Allocate the string from the multithreaded because the deallocate would have interfered with the normal allocations
	// since the deallocation would be done from a different thread
	editor_state->launched_module_compilation_lock.Lock();
	AddProjectModuleToLaunchedCompilationNoLock(editor_state, library_name, configuration);
	editor_state->launched_module_compilation_lock.Unlock();
}

// Returns true if it was found, else false
bool RemoveProjectModuleFromLaunchedCompilation(EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration) {
	editor_state->launched_module_compilation_lock.Lock();

	AllocatorPolymorphic multithreaded_allocator = editor_state->MultithreadedEditorAllocator();
	unsigned int string_index = FindString(library_name, editor_state->launched_module_compilation[configuration]);
	if (string_index != -1) {
		Deallocate(multithreaded_allocator, editor_state->launched_module_compilation[configuration][string_index].buffer);
		editor_state->launched_module_compilation[configuration].RemoveSwapBack(string_index);
	}

	editor_state->launched_module_compilation_lock.Unlock();

	return string_index != -1;
}

// -------------------------------------------------------------------------------------------------------------------------

bool AddModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, bool is_graphics_module) {
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
	ProjectModules* project_modules = editor_state->project_modules;

	if (!ExistsFileOrFolder(solution_path)) {
		ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
		error_message.size = FormatString(error_message.buffer, "Project module {#} solution does not exist.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(error_message);
		return false;
	}

	unsigned int module_index = GetModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
		error_message.size = FormatString(error_message.buffer, "Project module {#} already exists.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(error_message);
		return false;
	}

	module_index = project_modules->ReserveRange();
	EditorModule* module = project_modules->buffer + module_index;

	size_t total_size = (solution_path.size + library_name.size + 2) * sizeof(wchar_t);
	void* allocation = Allocate(editor_allocator, total_size, alignof(wchar_t));
	uintptr_t buffer = (uintptr_t)allocation;
	module->solution_path.InitializeFromBuffer(buffer, solution_path.size);
	buffer += sizeof(wchar_t);
	module->library_name.InitializeFromBuffer(buffer, library_name.size);
	buffer += sizeof(wchar_t);

	module->solution_path.CopyOther(solution_path);
	module->solution_path[module->solution_path.size] = L'\0';
	module->library_name.CopyOther(library_name);
	module->library_name[module->library_name.size] = L'\0';
	module->solution_last_write_time = 0;
	module->is_graphics_module = is_graphics_module;
	module->dll_imports = { nullptr, 0 };

	for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
		module->infos[index].load_status = EDITOR_MODULE_LOAD_FAILED;
		module->infos[index].ecs_module.base_module.code = ECS_GET_MODULE_FAULTY_PATH;

		module->infos[index].lock_count.store(0, ECS_RELAXED);
		module->infos[index].library_last_write_time = 0;
	}

	UpdateModuleLastWrite(editor_state, module_index);

	// Update the console such that it contains as filter string the module
	Console* console = GetConsole();
	ECS_STACK_CAPACITY_STREAM(char, ascii_name, 256);
	ConvertWideCharsToASCII(module->library_name, ascii_name);
	console->AddSystemFilterString(ascii_name);

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

struct CheckBuildStatusEventData {
	EditorState* editor_state;
	Stream<wchar_t> path;
	// This was previously atomic since it ran on a background thread
	// But now it has been switched to an event
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* report_status;
	bool disable_logging;
	Timer timer;
};

EDITOR_EVENT(CheckBuildStatusEvent) {
	CheckBuildStatusEventData* data = (CheckBuildStatusEventData*)_data;

	if (data->timer.GetDuration(ECS_TIMER_DURATION_MS) >= CHECK_FILE_STATUS_THREAD_SLEEP_TICK) {
		data->timer.SetNewStart();
		if (!ExistsFileOrFolder(data->path)) {
			return true;
		}
	}
	else {
		return true;
	}

	// Change the extension from .build/.rebuild/.clean to txt
	Stream<wchar_t> extension = PathExtension(data->path);
	ECS_STACK_CAPACITY_STREAM(wchar_t, log_path, 512);

	log_path.CopyOther(data->path);
	Stream<wchar_t> log_extension = PathExtension(log_path);
	// Make the dot into an underscore
	log_extension[0] = L'_';
	log_path.AddStream(CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION);

	bool succeded = PrintCommandStatus(data->editor_state, log_path, data->disable_logging);
	// Extract the library name and the configuration
	Stream<wchar_t> filename = PathFilename(data->path);
	const wchar_t* underscore = wcschr(filename.buffer, '_');

	Stream<wchar_t> library_name(filename.buffer, underscore - filename.buffer);
	Stream<wchar_t> configuration(underscore + 1, extension.buffer - underscore - 1);
	EDITOR_MODULE_CONFIGURATION int_configuration = EDITOR_MODULE_CONFIGURATION_COUNT;

	if (configuration == L"Debug") {
		int_configuration = EDITOR_MODULE_CONFIGURATION_DEBUG;
	}
	else if (configuration == L"Release") {
		int_configuration = EDITOR_MODULE_CONFIGURATION_RELEASE;
	}
	else if (configuration == L"Distribution") {
		int_configuration = EDITOR_MODULE_CONFIGURATION_DISTRIBUTION;
	}
	else {
		if (!data->disable_logging) {
			ECS_FORMAT_TEMP_STRING(console_string, "An error has occured when determining the configuration of module {#}", library_name);
			EditorSetConsoleError(console_string);
		}
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
			if (!data->disable_logging) {
				ECS_FORMAT_TEMP_STRING(console_string, "An error has occured when checking status of module {#}, configuration {#}. Could not locate module"
					"(was it removed?)", library_name, configuration);
				EditorSetConsoleError(console_string);
			}
		}

		if (underscore != nullptr) {
			Stream<wchar_t> command(extension.buffer + 1, extension.size - 1);

			if (!data->disable_logging) {
				ECS_FORMAT_TEMP_STRING(console_string, "Command {#} for module {#} with configuration {#} completed successfully", command, library_name, configuration);
				EditorSetConsoleInfo(console_string);
			}
			if (command ==  L"build" || command == L"rebuild") {
				bool success = LoadEditorModule(data->editor_state, module_index, int_configuration);
				if (!success) {
					if (!data->disable_logging) {
						ECS_FORMAT_TEMP_STRING(error_message, "Could not reload module {#} with configuration {#}, command {#} after successfully executing the command", library_name, configuration, command);
						EditorSetConsoleError(error_message);
					}
				}
			}
		}
		else {
			if (!data->disable_logging) {
				EditorSetConsoleWarn("Could not deduce build command type, library name or configuration for a module compilation");
				EditorSetConsoleInfo("A module finished building successfully");
			}
		}

		if (data->report_status != nullptr) {
			data->report_status->store(EDITOR_FINISH_BUILD_COMMAND_OK, ECS_RELAXED);
		}

	}
	else if (data->report_status != nullptr) {
		data->report_status->store(EDITOR_FINISH_BUILD_COMMAND_FAILED, ECS_RELAXED);
	}

	if (int_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
		Stream<wchar_t> launched_compilation_string(library_name.buffer, configuration.buffer - library_name.buffer - 1);
		// Remove the module from the launched compilation stream
		bool was_found = RemoveProjectModuleFromLaunchedCompilation(data->editor_state, launched_compilation_string, int_configuration);

		// Warn if the module could not be found in the launched module compilation
		if (!was_found && !data->disable_logging) {
			ECS_FORMAT_TEMP_STRING(console_string, "Module {#} with configuration {#} could not be found in the compilation list", library_name, configuration);
			EditorSetConsoleWarn(console_string);
		}
	}

	// The path must be deallocated
	data->editor_state->editor_allocator->Deallocate(data->path.buffer);
	ECS_ASSERT(RemoveFile(data->path));
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

typedef EDITOR_LAUNCH_BUILD_COMMAND_STATUS (*ForEachProjectModuleFunction)(
	EditorState*, 
	unsigned int, 
	EDITOR_MODULE_CONFIGURATION, 
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>*, 
	bool
);

static void ForEachProjectModule(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	const EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* statuses,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_statuses,
	bool disable_logging,
	ForEachProjectModuleFunction build_function
) {
	// Clear the log file first
	const ProjectModules* project_modules = (const ProjectModules*)editor_state->project_modules;

	if (build_statuses) {
		for (unsigned int index = 0; index < indices.size; index++) {
			build_statuses[index].store(EDITOR_FINISH_BUILD_COMMAND_WAITING, ECS_RELAXED);
			statuses[index] = build_function(editor_state, indices[index], configurations[index], build_statuses + index, disable_logging);
		}
	}
	else {
		for (unsigned int index = 0; index < indices.size; index++) {
			statuses[index] = build_function(editor_state, indices[index], configurations[index], nullptr, disable_logging);
		}
	}

}

// -------------------------------------------------------------------------------------------------------------------------

// For system() - the CRT function
void CommandLineString(CapacityStream<char>& string, Stream<wchar_t> solution_path, Stream<char> command, Stream<char> configuration, Stream<wchar_t> log_file) {
	string.CopyOther(CMB_BUILD_SYSTEM_PATH);
	string.AddStream(" && ");
	string.AddStream(CMD_BUILD_SYSTEM);
	string.Add(' ');
	ConvertWideCharsToASCII(solution_path, string);
	string.Add(' ');
	string.AddStream(command);
	string.Add(' ');
	string.AddStream(configuration);
	string.Add(' ');
	string.AddStream(CMD_BUILD_SYSTEM_LOG_FILE_COMMAND);
	string.Add(' ');
	ConvertWideCharsToASCII(log_file, string);
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

	string.CopyOther(L"/c ");
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

struct ReloadModuleEventData {
	bool allocated_finish_status;
	EDITOR_MODULE_CONFIGURATION configuration;
	// This is a single value - not an array
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* finish_status;
	// These are allocated inside the event
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* dependencies_finish_status;
	Stream<unsigned int> dependencies;
};

// This is used to reload modules that have been unloaded during a build/reload/clean
EDITOR_EVENT(ReloadModuleEvent) {
	ReloadModuleEventData* data = (ReloadModuleEventData*)_data;

	if (data->finish_status->load(ECS_RELAXED) != EDITOR_FINISH_BUILD_COMMAND_WAITING) {
		// Only do the reloading if the current compilation succeeded, otherwise it will trigger a useless build command
		if (data->finish_status->load(ECS_RELAXED) == EDITOR_FINISH_BUILD_COMMAND_OK) {
			data->dependencies_finish_status = (std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>*)editor_state->editor_allocator->Allocate(
				sizeof(std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>) * data->dependencies.size
			);

			// No matter what the outcome is, we need to rebuild the old modules
			for (size_t index = 0; index < data->dependencies.size; index++) {
				const EditorModuleInfo* info = GetModuleInfo(editor_state, data->dependencies[index], data->configuration);
				EDITOR_LAUNCH_BUILD_COMMAND_STATUS build_status = BuildModule(
					editor_state,
					data->dependencies[index],
					data->configuration,
					data->dependencies_finish_status + index
				);
				if (build_status == EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING) {
					ECS_FORMAT_TEMP_STRING(console_message, "Failed to reload module {#} after unloading it to let a build/rebuild operation be performed.",
						editor_state->project_modules->buffer[data->dependencies[index]].library_name);
					EditorSetConsoleError(console_message);
				}
			}
		}

		if (data->dependencies.size > 0) {
			editor_state->editor_allocator->Deallocate(data->dependencies.buffer);
		}
		if (data->allocated_finish_status) {
			editor_state->editor_allocator->Deallocate(data->finish_status);
		}
		return false;
	}
	else if (data->dependencies_finish_status != nullptr) {
		// Verify the launch statuses
		bool have_finished = true;
		for (size_t index = 0; index < data->dependencies.size && have_finished; index++) {
			have_finished &= data->dependencies_finish_status[index].load(ECS_RELAXED) != EDITOR_FINISH_BUILD_COMMAND_WAITING;
		}
		if (have_finished) {
			// Deallocate the buffer
			editor_state->editor_allocator->Deallocate(data->dependencies_finish_status);
			editor_state->editor_allocator->Deallocate(data->dependencies.buffer);
		}
		return !have_finished;
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_LAUNCH_BUILD_COMMAND_STATUS RunCmdCommand(
	EditorState* editor_state,
	unsigned int index,
	Stream<wchar_t> command,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* report_status,
	bool disable_logging
);

// -------------------------------------------------------------------------------------------------------------------------

struct RunCmdCommandDLLImportData {
	struct Dependency {
		unsigned int module_index;
		std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS> status;
		bool verified_once;
	};

	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION configuration;
	bool disable_logging;
	Stream<wchar_t> command;
	Stream<Dependency> dependencies;
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* original_status;
};

EDITOR_EVENT(RunCmdCommandDLLImport) {
	RunCmdCommandDLLImportData* data = (RunCmdCommandDLLImportData*)_data;

	editor_state->launched_module_compilation_lock.Lock();

	bool have_not_finished = false;
	bool has_failed = false;
	// Check to see if they have a command pending
	for (size_t index = 0; index < data->dependencies.size; index++) {
		if (!data->dependencies[index].verified_once) {
			Stream<wchar_t> library_name = editor_state->project_modules->buffer[data->dependencies[index].module_index].library_name;
			unsigned int launched_index = FindString(library_name, editor_state->launched_module_compilation[data->configuration]);
			if (launched_index == -1) {
				const EditorModuleInfo* info = GetModuleInfo(editor_state, data->dependencies[index].module_index, data->configuration);
				if (data->command == BUILD_PROJECT_STRING_WIDE) {
					if (info->load_status != EDITOR_MODULE_LOAD_GOOD) {
						editor_state->launched_module_compilation_lock.Unlock();
						BuildModule(editor_state, data->dependencies[index].module_index, data->configuration, &data->dependencies[index].status, data->disable_logging);
						editor_state->launched_module_compilation_lock.Lock();
						have_not_finished = true;
					}
				}
				else if (data->command == CLEAN_PROJECT_STRING_WIDE) {
					if (info->load_status != EDITOR_MODULE_LOAD_FAILED) {
						editor_state->launched_module_compilation_lock.Unlock();
						CleanModule(editor_state, data->dependencies[index].module_index, data->configuration, &data->dependencies[index].status, data->disable_logging);
						editor_state->launched_module_compilation_lock.Lock();
						have_not_finished = true;
					}
				}
				else if (data->command == REBUILD_PROJECT_STRING_WIDE) {
					// Here we need to perform the command no matter what
					editor_state->launched_module_compilation_lock.Unlock();
					RebuildModule(editor_state, data->dependencies[index].module_index, data->configuration, &data->dependencies[index].status, data->disable_logging);
					editor_state->launched_module_compilation_lock.Lock();
					have_not_finished = true;
				}
				else {
					ECS_ASSERT(false);
				}
				data->dependencies[index].verified_once = true;
			}
			else {
				have_not_finished = true;
			}
		}
		else {
			if (data->dependencies[index].status == EDITOR_FINISH_BUILD_COMMAND_WAITING) {
				have_not_finished = true;
			}
			else if (data->dependencies[index].status == EDITOR_FINISH_BUILD_COMMAND_FAILED) {
				has_failed = true;
			}
		}
	}

	editor_state->launched_module_compilation_lock.Unlock();

	if (!have_not_finished) {
		// If all the build finished with ok status, then continue
		if (!has_failed) {
			// We need to push the project to the compilation list
			AddProjectModuleToLaunchedCompilation(editor_state, GetModuleLibraryName(editor_state, data->module_index), data->configuration);
			RunCmdCommand(editor_state, data->module_index, data->command, data->configuration, data->original_status, data->disable_logging);
		}

		editor_state->editor_allocator->Deallocate(data->dependencies.buffer);
		return false;
	}
	return true;
}


struct RunCmdCommandAfterExternalDependencyData {
	unsigned int module_index;
	Stream<wchar_t> command;
	EDITOR_MODULE_CONFIGURATION configuration;
	bool disable_logging;
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* report_status;
	Stream<unsigned int> dependencies;
	ResizableStream<unsigned int> unloaded_dependencies;
};

EDITOR_EVENT(RunCmdCommandAfterExternalDependency) {
	RunCmdCommandAfterExternalDependencyData* data = (RunCmdCommandAfterExternalDependencyData*)_data;

	// Determine if all the dependencies have finished their respective actions
	editor_state->launched_module_compilation_lock.Lock();

	for (size_t index = 0; index < data->dependencies.size; index++) {
		// Check if it has an already pending action on it and wait for it then
		unsigned int already_launched_index = FindString(
			editor_state->project_modules->buffer[data->dependencies[index]].library_name, 
			editor_state->launched_module_compilation[data->configuration]
		);
		if (already_launched_index == -1) {
			EditorModuleInfo* current_info = GetModuleInfo(editor_state, data->dependencies[index], data->configuration);
			// See if the module is loaded - if it is then we need to unload it
			if (current_info->load_status != EDITOR_MODULE_LOAD_FAILED) {
				ReleaseModuleStreamsAndHandle(editor_state, data->dependencies[index], data->configuration);
				data->unloaded_dependencies.Add(data->dependencies[index]);
			}
			data->dependencies.RemoveSwapBack(index);
			index--;
		}
	}

	editor_state->launched_module_compilation_lock.Unlock();

	if (data->dependencies.size == 0) {
		editor_state->editor_allocator->Deallocate(data->dependencies.buffer);
		bool allocated_report_status = false;
		// We need to allocate the report_status only if there are actual unloaded dependencies
		if (data->report_status == nullptr && data->unloaded_dependencies.size > 0) {
			data->report_status = (std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>*)editor_state->editor_allocator->Allocate(
				sizeof(std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>)
			);
			data->report_status->store(EDITOR_FINISH_BUILD_COMMAND_WAITING, ECS_RELAXED);
			allocated_report_status = true;
		}
		RunCmdCommand(editor_state, data->module_index, data->command, data->configuration, data->report_status, data->disable_logging);

		if (data->command != CLEAN_PROJECT_STRING_WIDE) {
			if (data->unloaded_dependencies.size > 0) {
				ReloadModuleEventData reload_data;
				reload_data.configuration = data->configuration;
				reload_data.dependencies = data->unloaded_dependencies.ToStream();
				reload_data.finish_status = data->report_status;
				reload_data.allocated_finish_status = allocated_report_status;
				reload_data.dependencies_finish_status = nullptr;
				EditorAddEvent(editor_state, ReloadModuleEvent, &reload_data, sizeof(reload_data));
			}
			else {
				data->unloaded_dependencies.FreeBuffer();
			}
		}
		else {
			data->unloaded_dependencies.FreeBuffer();
		}

		return false;
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

// The project needs to be added to the launched compilation registry before this function call
EDITOR_LAUNCH_BUILD_COMMAND_STATUS RunCmdCommand(
	EditorState* editor_state,
	unsigned int index,
	Stream<wchar_t> command,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* report_status,
	bool disable_logging
) {
	Stream<wchar_t> library_name = editor_state->project_modules->buffer[index].library_name;

	// Construct the system string
#ifdef MODULE_BUILD_USING_CRT
	ECS_STACK_CAPACITY_STREAM(char, command_string, 512);
#else
	ECS_STACK_CAPACITY_STREAM(wchar_t, command_string, 512);
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
		if (!disable_logging) {
			EditorSetConsoleError("An error occured when creating the command prompt that builds the module.");
		}
		if (report_status != nullptr) {
			report_status->store(EDITOR_FINISH_BUILD_COMMAND_FAILED, ECS_RELAXED);
		}
		return EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING;
	}
	else {
		ECS_STACK_CAPACITY_STREAM(wchar_t, flag_file, 256);
		GetProjectDebugFolder(editor_state, flag_file);
		flag_file.Add(ECS_OS_PATH_SEPARATOR);
		GetModuleBuildFlagFile(editor_state, index, configuration, command, flag_file);

		// Log the command status
		CheckBuildStatusEventData check_data;
		check_data.editor_state = editor_state;
		check_data.path.buffer = (wchar_t*)Allocate(editor_state->EditorAllocator(), sizeof(wchar_t) * flag_file.size);
		check_data.path.CopyOther(flag_file);
		check_data.report_status = report_status;
		check_data.disable_logging = disable_logging;
		check_data.timer.SetUninitialized();

		EditorAddEvent(editor_state, CheckBuildStatusEvent, &check_data, sizeof(check_data));
	}
	return EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING;
#endif
}

// Returns whether or not the command actually will be executed. The command can be skipped if the module is in flight
// running another command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS RunBuildCommand(
	EditorState* editor_state,
	unsigned int index,
	Stream<wchar_t> command,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* report_status,
	bool disable_logging
) {
	Stream<wchar_t> library_name = editor_state->project_modules->buffer[index].library_name;

	// First check that the module is not executing another build command - it doesn't necessarly 
	// have to be the same type. A clean cannot be executed while a build is running
	// The lock must be acquired so a thread that wants to remove an element 
	// does not interfere with this reading.
	editor_state->launched_module_compilation_lock.Lock();
	unsigned int already_launched_index = FindString(library_name, editor_state->launched_module_compilation[configuration]);
	if (already_launched_index != -1) {
		editor_state->launched_module_compilation_lock.Unlock();
		return EDITOR_LAUNCH_BUILD_COMMAND_ALREADY_RUNNING;
	}
	AddProjectModuleToLaunchedCompilationNoLock(editor_state, library_name, configuration);
	editor_state->launched_module_compilation_lock.Unlock();

	// We should release the streams and handles here since we know for sure that this call will result
	// In a compilation. Before hand this call was used in the Build call even when the build would be skipped
	// And that could result in a race condition where the background thread updates the module but the already executing
	// Main thread would release the streams
	ReleaseModuleStreamsAndHandle(editor_state, index, configuration);

	ECS_STACK_CAPACITY_STREAM(unsigned int, external_references, 512);
	// Get the external references. We need to unload these before trying to procede
	GetModuleDLLExternalReferences(editor_state, index, external_references);

	// Returns true if there are import to be made
	auto execute_import = [&]() {
		ECS_STACK_CAPACITY_STREAM(unsigned int, import_references, 512);
		GetModuleDLLImports(editor_state, index, import_references);

		if (import_references.size > 0) {
			// We also need the dll imports to be finished
			RunCmdCommandDLLImportData import_data;
			import_data.command = command;
			import_data.configuration = configuration;
			import_data.module_index = index;
			import_data.original_status = report_status;
			import_data.dependencies.Initialize(editor_state->EditorAllocator(), import_references.size);
			import_data.disable_logging = disable_logging;
			for (unsigned int subindex = 0; subindex < import_references.size; subindex++) {
				import_data.dependencies[subindex].module_index = import_references[subindex];
				import_data.dependencies[subindex].verified_once = false;
				import_data.dependencies[subindex].status = EDITOR_FINISH_BUILD_COMMAND_WAITING;
			}

			// Remove the project from compilation list since it will make the dependencies deadlock waiting
			// for this to finish compiling such that they can unload the module while this will wait for them
			// to finish compiling
			RemoveProjectModuleFromLaunchedCompilation(editor_state, library_name, configuration);

			EditorAddEvent(editor_state, RunCmdCommandDLLImport, &import_data, sizeof(import_data));
			return true;
		}
		return false;
	};

	if (external_references.size > 0) {
		RunCmdCommandAfterExternalDependencyData wait_data;
		wait_data.command = command;
		wait_data.configuration = configuration;
		wait_data.module_index = index;
		wait_data.report_status = report_status;
		wait_data.unloaded_dependencies.Initialize(editor_state->EditorAllocator(), 0);
		wait_data.dependencies.InitializeAndCopy(editor_state->EditorAllocator(), external_references);
		wait_data.disable_logging = disable_logging;
		EditorAddEvent(editor_state, RunCmdCommandAfterExternalDependency, &wait_data, sizeof(wait_data));

		execute_import();

		return EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING;
	}
	else if (execute_import()) {
		return EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING;
	}

	return RunCmdCommand(editor_state, index, command, configuration, report_status, disable_logging);
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_LAUNCH_BUILD_COMMAND_STATUS BuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* report_status,
	bool disable_logging,
	bool force_build
) {
	ProjectModules* modules = editor_state->project_modules;
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);

	if (IsModuleInfoLocked(editor_state,index, configuration)) {
		return EDITOR_LAUNCH_BUILD_COMMAND_LOCKED;
	}
	if (UpdateModuleLastWrite(editor_state, index, configuration) || info->load_status != EDITOR_MODULE_LOAD_GOOD || force_build) {
		return RunBuildCommand(editor_state, index, BUILD_PROJECT_STRING_WIDE, configuration, report_status, disable_logging);
	}
	if (report_status != nullptr) {
		report_status->store(EDITOR_FINISH_BUILD_COMMAND_OK, ECS_RELAXED);
	}
	// The callback will check the status of this function and report accordingly to the console
	return EDITOR_LAUNCH_BUILD_COMMAND_SKIPPED;
}

static EDITOR_LAUNCH_BUILD_COMMAND_STATUS BuildModuleNoForce(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* report_status,
	bool disable_logging
) {
	return BuildModule(editor_state, index, configuration, report_status, disable_logging);
}

// -------------------------------------------------------------------------------------------------------------------------

void BuildModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	const EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_statuses,
	bool disable_logging
)
{
	ForEachProjectModule(editor_state, indices, configurations, launch_statuses, build_statuses, disable_logging, BuildModuleNoForce);
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_LAUNCH_BUILD_COMMAND_STATUS CleanModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_status,
	bool disable_logging
) {
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	if (IsModuleInfoLocked(editor_state, index, configuration)) {
		return EDITOR_LAUNCH_BUILD_COMMAND_LOCKED;
	}
	info->load_status = EDITOR_MODULE_LOAD_FAILED;
	return RunBuildCommand(editor_state, index, CLEAN_PROJECT_STRING_WIDE, configuration, build_status, disable_logging);
}

// -------------------------------------------------------------------------------------------------------------------------

void CleanModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	const EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_statuses,
	bool disable_logging
) {
	ForEachProjectModule(editor_state, indices, configurations, launch_statuses, build_statuses, disable_logging, CleanModule);
}

// -------------------------------------------------------------------------------------------------------------------------

EDITOR_LAUNCH_BUILD_COMMAND_STATUS RebuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_status,
	bool disable_logging
) {
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	if (IsModuleInfoLocked(editor_state,index,configuration)) {
		return EDITOR_LAUNCH_BUILD_COMMAND_LOCKED;
	}
	info->load_status = EDITOR_MODULE_LOAD_FAILED;
	return RunBuildCommand(editor_state, index, REBUILD_PROJECT_STRING_WIDE, configuration, build_status, disable_logging);
}

// -------------------------------------------------------------------------------------------------------------------------

void PrintConsoleMessageForBuildCommand(
	EditorState* editor_state, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS command_status
)
{
	Stream<wchar_t> library_name = editor_state->project_modules->buffer[module_index].library_name;
	Stream<char> configuration_string = MODULE_CONFIGURATIONS[configuration];
	ECS_STACK_CAPACITY_STREAM(char, console_message, 256);
	switch (command_status) {
	case EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING:
		ECS_FORMAT_STRING(console_message, "Command for module {#} with configuration {#} launched successfully.", library_name, configuration_string);
		EditorSetConsoleInfo(console_message);
		break;
	case EDITOR_LAUNCH_BUILD_COMMAND_SKIPPED:
		ECS_FORMAT_STRING(console_message, "The module {#} with configuration {#} is up to date. The command is skipped", library_name, configuration_string);
		EditorSetConsoleInfo(console_message);
		break;
	case EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING:
		ECS_FORMAT_STRING(console_message, "An error has occured when launching the command line for module {#} with configuration {#}. The command is aborted.", library_name, configuration_string);
		EditorSetConsoleError(console_message);
		break;
	case EDITOR_LAUNCH_BUILD_COMMAND_ALREADY_RUNNING:
		ECS_FORMAT_STRING(console_message, "The module {#} with configuration {#} is already executing a command.", library_name, configuration_string);
		EditorSetConsoleError(console_message);
		break;
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void DecrementModuleInfoLockCount(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	GetModuleInfo(editor_state, module_index, configuration)->lock_count.fetch_sub(1, ECS_RELAXED);
}

// -------------------------------------------------------------------------------------------------------------------------

void RebuildModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	const EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_statuses,
	bool disable_logging
) {
	ForEachProjectModule(editor_state, indices, configurations, launch_statuses, build_statuses, disable_logging, RebuildModule);
}

// -------------------------------------------------------------------------------------------------------------------------

void DeleteModuleFlagFiles(EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, debug_path, 512);
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

bool ExistsModuleDebugDrawElementIn(
	const EditorState* editor_state,
	Stream<ModuleComponentFunctions> component_functions,
	ComponentWithType component_type
) {
	size_t found_index = component_functions.Find(component_type, [editor_state](const ModuleComponentFunctions& element) {
		if (element.debug_draw.draw_function != nullptr) {
			Component component = editor_state->editor_components.GetComponentID(element.component_name);
			ECS_COMPONENT_TYPE type = editor_state->editor_components.GetComponentType(element.component_name);
			return ComponentWithType{ component, type };
		}
		else {
			return ComponentWithType{ -1, ECS_COMPONENT_TYPE_COUNT };
		}
	});
	return found_index != -1;
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsEditorModuleLoaded(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	return info->ecs_module.base_module.code == ECS_GET_MODULE_OK;
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsGraphicsModule(const EditorState* editor_state, unsigned int index)
{
	return editor_state->project_modules->buffer[index].is_graphics_module;
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsModuleBeingCompiled(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	return IsAnyModuleBeingCompiled(editor_state, { &module_index, 1 }, &configuration);
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsAnyModuleBeingCompiled(EditorState* editor_state, Stream<unsigned int> module_indices, const EDITOR_MODULE_CONFIGURATION* configurations)
{
	ECS_STACK_CAPACITY_STREAM(bool, is_compiled, 512);
	ECS_ASSERT(module_indices.size <= is_compiled.capacity);

	editor_state->launched_module_compilation_lock.Lock();
	for (size_t index = 0; index < module_indices.size; index++) {
		const EditorModule* module = editor_state->project_modules->buffer + module_indices[index];
		is_compiled[index] = FindString(module->library_name, editor_state->launched_module_compilation[configurations[index]].ToStream()) != -1;
	}
	editor_state->launched_module_compilation_lock.Unlock();

	// Check for events as well - retrieve these outside the loop
	ECS_STACK_CAPACITY_STREAM(void*, dll_import_events_data, 512);
	EditorGetEventTypeData(editor_state, RunCmdCommandDLLImport, &dll_import_events_data);

	ECS_STACK_CAPACITY_STREAM(void*, external_dependency_events_data, 512);
	EditorGetEventTypeData(editor_state, RunCmdCommandAfterExternalDependency, &external_dependency_events_data);

	for (size_t index = 0; index < module_indices.size; index++) {
		if (is_compiled[index]) {
			return true;
		}

		for (unsigned int subindex = 0; subindex < dll_import_events_data.size; subindex++) {
			RunCmdCommandDLLImportData* data = (RunCmdCommandDLLImportData*)dll_import_events_data[subindex];
			if (data->module_index == module_indices[index] && data->configuration == configurations[index]) {
				return true;
			}
		}

		for (unsigned int subindex = 0; subindex < external_dependency_events_data.size; subindex++) {
			RunCmdCommandAfterExternalDependencyData* data = (RunCmdCommandAfterExternalDependencyData*)external_dependency_events_data[subindex];
			if (data->module_index == module_indices[index] && data->configuration == configurations[index]) {
				return true;
			}
		}
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsModuleUsedBySandboxes(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	for (unsigned int index = 0; index < sandbox_count; index++) {
		EDITOR_MODULE_CONFIGURATION configuration_used = IsModuleUsedBySandbox(editor_state, index, module_index);
		if (configuration_used != EDITOR_MODULE_CONFIGURATION_COUNT) {
			if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT || configuration_used == configuration) {
				return true;
			}
		}
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

void GetCompilingModules(EditorState* editor_state, CapacityStream<unsigned int>& module_indices, EDITOR_MODULE_CONFIGURATION* configurations)
{
	// Check for events as well - retrieve these outside the loop
	ECS_STACK_CAPACITY_STREAM(void*, dll_import_events_data, 512);
	EditorGetEventTypeData(editor_state, RunCmdCommandDLLImport, &dll_import_events_data);

	ECS_STACK_CAPACITY_STREAM(void*, external_dependency_events_data, 512);
	EditorGetEventTypeData(editor_state, RunCmdCommandAfterExternalDependency, &external_dependency_events_data);

	editor_state->launched_module_compilation_lock.Lock();

	for (unsigned int index = 0; index < module_indices.size; index++) {
		const EditorModule* module = editor_state->project_modules->buffer + module_indices[index];
		bool is_compiled = FindString(module->library_name, editor_state->launched_module_compilation[configurations[index]].ToStream()) != -1;
		if (!is_compiled) {
			// Check the events now
			unsigned int subindex = 0;
			for (; subindex < dll_import_events_data.size; subindex++) {
				RunCmdCommandDLLImportData* data = (RunCmdCommandDLLImportData*)dll_import_events_data[subindex];
				if (data->module_index == module_indices[index] && data->configuration == configurations[index]) {
					break;
				}
			}

			if (subindex == dll_import_events_data.size) {
				subindex = 0;
				for (; subindex < external_dependency_events_data.size; subindex++) {
					RunCmdCommandAfterExternalDependencyData* data = (RunCmdCommandAfterExternalDependencyData*)external_dependency_events_data[subindex];
					if (data->module_index == module_indices[index] && data->configuration == configurations[index]) {
						break;
					}
				}

				if (subindex == external_dependency_events_data.size) {
					module_indices.RemoveSwapBack(index);
					configurations[index] = configurations[module_indices.size];
					// Decrement the index since the value is swapped in this place
					index--;
				}
			}
		}
	}

	editor_state->launched_module_compilation_lock.Unlock();
}

// -------------------------------------------------------------------------------------------------------------------------

bool GetSandboxesForModule(
	const EditorState* editor_state, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	CapacityStream<unsigned int>* sandboxes
)
{
	unsigned int initial_size = sandboxes->size;

	unsigned int sandbox_count = GetSandboxCount(editor_state);
	for (unsigned int index = 0; index < sandbox_count; index++) {
		EDITOR_MODULE_CONFIGURATION configuration_used = IsModuleUsedBySandbox(editor_state, index, module_index);
		if (configuration_used == configuration) {
			sandboxes->AddAssert(index);
		}
	}
	return sandboxes->size > initial_size;
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned int GetModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path)
{
	ProjectModules* modules = editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		if (modules->buffer[index].solution_path == solution_path) {
			return index;
		}
	}
	return -1;
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned int GetModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_name) {
	ProjectModules* modules = editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		if (modules->buffer[index].library_name == library_name) {
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
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	log_path.CopyOther(project_file->path);
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

EDITOR_MODULE_LOAD_STATUS GetModuleLoadStatus(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration)
{
	return GetModuleInfo(editor_state, index, configuration)->load_status;
}

// -------------------------------------------------------------------------------------------------------------------------

ModuleComponentBuildEntry GetModuleComponentBuildEntry(
	const EditorState* editor_state, 
	unsigned int index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	Stream<char> component_name,
	EDITOR_MODULE_CONFIGURATION* matched_configuration
)
{
	if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
		configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (matched_configuration != nullptr) {
			*matched_configuration = configuration;
		}
		if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
			return { nullptr };
		}
	}

	const EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);
	return GetModuleComponentBuildEntry(&info->ecs_module, component_name);
}

// -------------------------------------------------------------------------------------------------------------------------

EditorModuleComponentBuildEntry GetModuleComponentBuildEntry(
	const EditorState* editor_state,
	Stream<char> component_name
) {
	unsigned int module_count = editor_state->project_modules->size;
	for (unsigned int index = 0; index < module_count; index++) {
		EDITOR_MODULE_CONFIGURATION matched_configuration;
		ModuleComponentBuildEntry entry = GetModuleComponentBuildEntry(editor_state, index, EDITOR_MODULE_CONFIGURATION_COUNT, component_name, &matched_configuration);
		if (entry.function != nullptr) {
			return { entry, index, matched_configuration };
		}
	}
	return { nullptr };
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleStem(Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path)
{
	module_path.AddStream(library_name);
	module_path.Add(L'_');
	module_path.AddStream(MODULE_CONFIGURATIONS_WIDE[(unsigned int)configuration]);
	module_path[module_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleFilename(Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path)
{
	GetModuleStem(library_name, configuration, module_path);
	module_path.AddStreamSafe(ECS_MODULE_EXTENSION);
	module_path[module_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleFilenameNoConfig(Stream<wchar_t> library_name, CapacityStream<wchar_t>& module_path)
{
	module_path.AddStream(library_name);
	module_path.AddStream(ECS_MODULE_EXTENSION);
	module_path[module_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path)
{
	GetProjectModulesFolder(editor_state, module_path);
	module_path.Add(ECS_OS_PATH_SEPARATOR);
	GetModuleFilename(library_name, configuration, module_path);
}

// -------------------------------------------------------------------------------------------------------------------------

Stream<wchar_t> GetModuleLibraryName(const EditorState* editor_state, unsigned int module_index)
{
	return editor_state->project_modules->buffer[module_index].library_name;
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleDLLImports(EditorState* editor_state, unsigned int index)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 64, ECS_MB);

	editor_state->project_modules->buffer[index].dll_imports.Deallocate(editor_state->EditorAllocator());
	editor_state->project_modules->buffer[index].dll_imports = GetModuleDLLDependenciesFromVCX(
		editor_state->project_modules->buffer[index].solution_path, 
		&_stack_allocator, 
		true
	);

	Stream<char> search_token = "_$(Configuration)";
	AllocatorPolymorphic stack_allocator = &_stack_allocator;

	Stream<Stream<char>> dll_imports = editor_state->project_modules->buffer[index].dll_imports;

	for (size_t index = 0; index < dll_imports.size; index++) {
		Stream<char> token = FindFirstToken(dll_imports[index], search_token);
		if (token.size > 0) {
			dll_imports[index] = ReplaceToken(dll_imports[index], search_token, "", stack_allocator);
		}
		else {
			dll_imports[index] = dll_imports[index];
		}
	}

	editor_state->project_modules->buffer[index].dll_imports = StreamCoalescedDeepCopy(dll_imports, editor_state->EditorAllocator());
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleDLLImports(const EditorState* editor_state, unsigned int index, CapacityStream<unsigned int>& dll_imports)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 64, ECS_MB);
	const size_t WIDE_IMPORT_CAPACITY = 512;
	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, wide_imports, WIDE_IMPORT_CAPACITY);

	Stream<Stream<char>> dll_dependencies = editor_state->project_modules->buffer[index].dll_imports;

	ECS_ASSERT(dll_dependencies.size <= WIDE_IMPORT_CAPACITY);
	for (size_t import_index = 0; import_index < dll_dependencies.size; import_index++) {
		wide_imports[import_index].Initialize(&_stack_allocator, dll_dependencies[import_index].size);
		CapacityStream<wchar_t> temp_wide = wide_imports[import_index];
		temp_wide.size = 0;
		ConvertASCIIToWide(temp_wide, dll_dependencies[import_index]);
	}
	wide_imports.size = dll_dependencies.size;

	for (unsigned int subindex = 0; subindex < editor_state->project_modules->size; subindex++) {
		if (subindex != index) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, dll_library_name, 512);
			GetModuleFilenameNoConfig(editor_state->project_modules->buffer[subindex].library_name, dll_library_name);
			if (FindString(dll_library_name, wide_imports) != -1) {
				dll_imports.AddAssert(subindex);
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleDLLExternalReferences(
	const EditorState* editor_state, 
	unsigned int index, 
	CapacityStream<unsigned int>& external_references
)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, library_name, 512);
	ECS_STACK_CAPACITY_STREAM(char, ascii_library_name, 512);
	GetModuleFilenameNoConfig(editor_state->project_modules->buffer[index].library_name, library_name);
	ConvertWideCharsToASCII(library_name, ascii_library_name);

	for (unsigned int subindex = 0; subindex < editor_state->project_modules->size; subindex++) {
		if (subindex != index) {
			if (FindString(ascii_library_name, editor_state->project_modules->buffer[subindex].dll_imports) != -1) {
				external_references.AddAssert(subindex);
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetModuleSolutionLastWrite(Stream<wchar_t> solution_path)
{
	Stream<wchar_t> parent_folder = PathParent(solution_path);

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

		Stream<wchar_t> filename = PathFilename(path);

		if (FindString(filename, data->source_names) != -1) {
			bool success = OS::GetFileTimes(path, nullptr, nullptr, &data->last_write);
			return false;
		}

		return true;
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, null_terminated_path, 256);
	null_terminated_path.CopyOther(solution_path);
	null_terminated_path.AddAssert(L'\0');
	size_t solution_last_write = 0;

	bool success = OS::GetFileTimes(null_terminated_path.buffer, nullptr, nullptr, &solution_last_write);
	ForEachDirectory(parent_folder, &data, folder_functor);
	return std::max(data.last_write, solution_last_write);
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetModuleLibraryLastWrite(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, module_path, 256);
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
	Stream<wchar_t> solution_parent = PathParent(solution_path);
	path.CopyOther(solution_parent);
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

bool CreateEditorModuleTemporaryDLL(CapacityStream<wchar_t> library_path, CapacityStream<wchar_t>& temporary_path) {
	temporary_path.CopyOther(library_path);
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
	}

	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	Stream<wchar_t> library_name = modules->buffer[index].library_name;
	ECS_STACK_CAPACITY_STREAM(wchar_t, library_path, 512);
	GetModulePath(editor_state, library_name, configuration, library_path);

	ECS_STACK_CAPACITY_STREAM(wchar_t, temporary_library, 512);
	if (ExistsFileOrFolder(library_path)) {
		// Copy the .dll to a temporary dll such that it will allow building the module again
		bool copy_success = CreateEditorModuleTemporaryDLL(library_path, temporary_library);
		if (copy_success) {
			// No need to verify if the symbols load failed or succeeded
			bool load_debugging_symbols = false;
			info->ecs_module.base_module = LoadModule(temporary_library, &load_debugging_symbols);
			// The load succeded, now try to retrieve the streams for this module
			if (info->ecs_module.base_module.code == ECS_GET_MODULE_OK) {
				AllocatorPolymorphic allocator = editor_state->EditorAllocator();

				ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB * 16);
				ECS_STACK_CAPACITY_STREAM(Stream<char>, component_names, ECS_KB * 16);
				editor_state->editor_components.GetAllComponentNames(&component_names, ECS_COMPONENT_TYPE_COUNT);
				LoadAppliedModule(&info->ecs_module, allocator, component_names, &error_message);
				if (error_message.size > 0) {
					// At the moment just warn
					ECS_FORMAT_TEMP_STRING(
						message,
						"Module {#} with configuration {#} has failed validation for some parameters",
						library_name,
						MODULE_CONFIGURATIONS[configuration]
					);
					EditorSetConsoleWarn(message);
					EditorSetConsoleWarn(error_message);
				}
				
				// We must update the sandboxes that reference this configuration
				// To update their component functions
				UpdateSandboxesComponentFunctionsForModule(editor_state, index, configuration);

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

	Reflection::ReflectionManager* module_reflection = editor_state->ModuleReflectionManager();

	ECS_STACK_CAPACITY_STREAM(wchar_t, source_path, 512);
	bool success = GetModuleReflectSolutionPath(editor_state, index, source_path);
	if (success) {
		unsigned int folder_hierarchy = module_reflection->GetHierarchyIndex(source_path);
		if (folder_hierarchy == -1) {
			folder_hierarchy = module_reflection->CreateFolderHierarchy(source_path);
		}
		else {
			ReleaseModuleReflection(editor_state, index);
		}

		ECS_STACK_CAPACITY_STREAM(char, error_message, 2048);

		// Launch thread tasks to process this new entry
		success = module_reflection->ProcessFolderHierarchy(folder_hierarchy, editor_state->task_manager, &error_message);

		// We'll need the name for the editor components module
		ECS_STACK_CAPACITY_STREAM(char, ascii_name, 512);
		ConvertWideCharsToASCII(module->library_name, ascii_name);

		if (!success) {
			ECS_FORMAT_TEMP_STRING(
				console_message, 
				"Could not reflect the newly added module {#} at {#}. Detailed error: {#}",
				module->library_name, 
				module->solution_path,
				error_message
			);
			EditorSetConsoleWarn(console_message);

			// Remove the module from the editor components - if it was already loaded
			unsigned int editor_component_module_index = editor_state->editor_components.FindModule(ascii_name);
			if (editor_component_module_index != -1) {
				editor_state->editor_components.RemoveModule(editor_state, editor_component_module_index);
			}
		}
		else {
			// Create all the link types for the components inside the reflection manager
			CreateLinkTypesForComponents(module_reflection, folder_hierarchy);

			// Don't create the UI types here for components because they might contain references to assets and fail
			UIReflectionDrawerTag component_tags[] = {
				{ ECS_COMPONENT_TAG, false },
				{ ECS_LINK_COMPONENT_TAG, true }
			};
			ECS_STACK_CAPACITY_STREAM(unsigned int, type_indices, 512);
			UIReflectionDrawerSearchOptions search_options;
			//search_options.exclude_tags = { component_tags, std::size(component_tags) };
			search_options.indices = &type_indices;
			unsigned int types_created = editor_state->module_reflection->CreateTypesForHierarchy(folder_hierarchy, search_options);

			// Convert all stream types to resizable
			for (unsigned int index = 0; index < type_indices.size; index++) {
				UIReflectionType* type = editor_state->module_reflection->GetType(type_indices[index]);
				editor_state->module_reflection->ConvertTypeStreamsToResizable(type);
			}

			// Inform the inspectors about the change
			UpdateInspectorUIModuleSettings(editor_state, index);

			// Update the engine components
			editor_state->editor_components.UpdateComponents(editor_state, module_reflection, folder_hierarchy, ascii_name);
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

	return EDITOR_MODULE_CONFIGURATION_COUNT;
}

// -------------------------------------------------------------------------------------------------------------------------

ModuleLinkComponentTarget GetModuleLinkComponentTarget(const EditorState* editor_state, unsigned int module_index, Stream<char> name)
{
	EDITOR_MODULE_CONFIGURATION loaded_configuration = GetModuleLoadedConfiguration(editor_state, module_index);
	if (loaded_configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
		// There is no dll loaded, return nullptr
		return { nullptr, nullptr };
	}
	const EditorModuleInfo* info = GetModuleInfo(editor_state, module_index, loaded_configuration);
	return ECSEngine::GetModuleLinkComponentTarget(&info->ecs_module, name);
}

// -------------------------------------------------------------------------------------------------------------------------

ModuleLinkComponentTarget GetEngineLinkComponentTarget(const EditorState* editor_state, Stream<char> name)
{
	for (size_t index = 0; index < editor_state->ecs_link_components.size; index++) {
		ModuleLinkComponentTarget target = editor_state->ecs_link_components[index];
		if (target.component_name == name) {
			return target;
		}
	}
	return { nullptr, nullptr };
}

// -------------------------------------------------------------------------------------------------------------------------

ModuleLinkComponentTarget GetModuleLinkComponentTarget(const EditorState* editor_state, Stream<char> name)
{
	ModuleLinkComponentTarget target = GetEngineLinkComponentTarget(editor_state, name);
	if (target.build_function != nullptr) {
		return target;
	}
	else {
		unsigned int module_count = editor_state->project_modules->size;
		for (unsigned int index = 0; index < module_count; index++) {
			target = GetModuleLinkComponentTarget(editor_state, index, name);
			if (target.build_function != nullptr) {
				return target;
			}
		}
	}
	return { nullptr, nullptr };
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleTypesDependencies(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& dependencies)
{
	editor_state->editor_components.GetModuleTypesDependencies(editor_state->editor_components.ModuleIndexFromReflection(editor_state, module_index), &dependencies, editor_state);
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModulesTypesDependentUpon(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& dependencies)
{
	editor_state->editor_components.GetModulesTypesDependentUpon(editor_state->editor_components.ModuleIndexFromReflection(editor_state, module_index), &dependencies, editor_state);
}

// -------------------------------------------------------------------------------------------------------------------------

Stream<void> GetModuleExtraInformation(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration, Stream<char> key)
{
	ModuleExtraInformation extra_information = GetModuleExtraInformation(editor_state, module_index, configuration);
	if (extra_information.pairs.size > 0) {
		return extra_information.Find(key);
	}
	return {};
}

// -------------------------------------------------------------------------------------------------------------------------

ModuleExtraInformation GetModuleExtraInformation(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	const EditorModule* module = editor_state->project_modules->buffer + module_index;
	if (module->infos[configuration].load_status == EDITOR_MODULE_LOAD_GOOD || module->infos[configuration].load_status == EDITOR_MODULE_LOAD_OUT_OF_DATE) {
		return module->infos[configuration].ecs_module.extra_information;
	}
	return { {} };
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleDebugDrawComponents(
	const EditorState* editor_state, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	AdditionStream<ComponentWithType> components
)
{
	const EditorModuleInfo* info = GetModuleInfo(editor_state, module_index, configuration);
	Stream<ModuleComponentFunctions> elements = info->ecs_module.component_functions;
	components.AssertNewItems(elements.size);
	for (size_t index = 0; index < elements.size; index++) {
		Component component = editor_state->editor_components.GetComponentID(elements[index].component_name);
		ECS_COMPONENT_TYPE type = editor_state->editor_components.GetComponentType(elements[index].component_name);
		components.Add({ component, type });
	}
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned int GetDebugDrawTasksBelongingModule(const EditorState* editor_state, Stream<char> task_name, EDITOR_MODULE_CONFIGURATION configuration)
{
	unsigned int module_count = editor_state->project_modules->size;
	for (unsigned int index = 0; index < module_count; index++) {
		EDITOR_MODULE_CONFIGURATION optimal_configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (optimal_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
			const EditorModuleInfo* module_info = GetModuleInfo(editor_state, index, optimal_configuration);
			bool exists = module_info->ecs_module.debug_draw_task_elements.Find(task_name, [](const ModuleDebugDrawTaskElement& element) {
				return element.base_element.task_name;
			}) != -1;
			if (exists) {
				return index;
			}
		}
	}
	return -1;
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModuleMatchedDebugDrawComponents(
	const EditorState* editor_state, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	Stream<ComponentWithType> components, 
	ModuleDebugDrawElementTyped* debug_elements
)
{
	const EditorModuleInfo* info = GetModuleInfo(editor_state, module_index, configuration);
	ModuleMatchDebugDrawElements(editor_state, components, info->ecs_module.component_functions, debug_elements);
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsModuleInfoLocked(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	return GetModuleInfo(editor_state, module_index, configuration)->lock_count.load(ECS_RELAXED) > 0;
}

// -------------------------------------------------------------------------------------------------------------------------

bool IsAnyModuleInfoLocked(const EditorState* editor_state)
{
	unsigned int module_count = editor_state->project_modules->size;
	for (unsigned int index = 0; index < module_count; index++) {
		for (size_t configuration = 0; configuration < EDITOR_MODULE_CONFIGURATION_COUNT; configuration++) {
			if (IsModuleInfoLocked(editor_state, index, (EDITOR_MODULE_CONFIGURATION)configuration)) {
				return true;
			}
		}
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

void IncrementModuleInfoLockCount(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	GetModuleInfo(editor_state, module_index, configuration)->lock_count.fetch_add(1, ECS_RELAXED);
}

// -------------------------------------------------------------------------------------------------------------------------

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, library_path, 256);
	GetModulePath(editor_state, library_name, configuration, library_path);
	return FindModule(library_path);
}

// -------------------------------------------------------------------------------------------------------------------------

bool HasModuleFunction(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return HasModuleFunction(editor_state, modules->buffer[index].library_name, configuration);
}

// -------------------------------------------------------------------------------------------------------------------------

void ModulesToAppliedModules(const EditorState* editor_state, CapacityStream<const AppliedModule*>& applied_modules)
{
	unsigned int count = editor_state->project_modules->size;
	for (unsigned int index = 0; index < count; index++) {
		EDITOR_MODULE_CONFIGURATION configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
			applied_modules.AddAssert(&GetModuleInfo(editor_state, index, configuration)->ecs_module);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void ModuleMatchDebugDrawElements(
	const EditorState* editor_state,
	Stream<ComponentWithType> components, 
	Stream<ModuleComponentFunctions> component_functions, 
	ModuleDebugDrawElementTyped* output_elements
) {
	for (size_t index = 0; index < components.size; index++) {
		size_t found_index = component_functions.Find(components[index], [editor_state](const ModuleComponentFunctions& element) {
			if (element.debug_draw.draw_function != nullptr) {
				Component component = editor_state->editor_components.GetComponentID(element.component_name);
				ECS_COMPONENT_TYPE type = editor_state->editor_components.GetComponentType(element.component_name);
				return ComponentWithType{ component, type };
			}
			else {
				return ComponentWithType{ -1, ECS_COMPONENT_TYPE_COUNT };
			}
		});
		if (found_index != -1) {
			output_elements[index] = { component_functions[found_index].debug_draw, components[index] };
		}
	}
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
	AllocatorPolymorphic allocator = editor_state->editor_allocator;

	// Also release the debugging symbols for the module
	// Don't verify if it succeeded or not since it is of not much relevance
	bool release_debugging_symbols = false;
	ReleaseAppliedModule(&info->ecs_module, allocator, &release_debugging_symbols);
	info->load_status = EDITOR_MODULE_LOAD_FAILED;

	// TODO: Should we reset the component functions for the sandboxes that reference this
	// Specific module configuration? The assumption is that a sandbox cannot continue
	// Running when one of its modules is not loaded and, as such, it doesn't really add
	// Anything of substance. It could help a little bit with debugging but if we reset
	// The component functions, we will reset the allocator for that component and lose the buffer
	// Data. For this last point, there is not really a solution at the moment
}

// -------------------------------------------------------------------------------------------------------------------------

void ReleaseModuleStreamsAndHandle(EditorState* editor_state, Stream<unsigned int> indices, EDITOR_MODULE_CONFIGURATION configuration)
{
	for (size_t index = 0; index < indices.size; index++) {
		ReleaseModuleStreamsAndHandle(editor_state, indices[index], configuration);
	}
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
	size_t new_last_write = GetModuleSolutionLastWrite(modules->buffer[index].solution_path);
	modules->buffer[index].solution_last_write_time = new_last_write;
	bool is_updated = solution_last_write < new_last_write || new_last_write == 0;
	if (is_updated) {
		// Make the modules already loaded out of date if that is the case
		// Also do the same for the external dependencies
		for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
			EditorModuleInfo* info = GetModuleInfo(editor_state, index, (EDITOR_MODULE_CONFIGURATION)configuration_index);
			if (info->library_last_write_time < new_last_write && info->load_status == EDITOR_MODULE_LOAD_GOOD) {
				info->load_status = EDITOR_MODULE_LOAD_OUT_OF_DATE;
			}
		}

		ECS_STACK_CAPACITY_STREAM(unsigned int, external_dependencies, 512);
		GetModuleDLLExternalReferences(editor_state, index, external_dependencies);
		for (unsigned int subindex = 0; subindex < external_dependencies.size; subindex++) {
			for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
				EditorModuleInfo* info = GetModuleInfo(editor_state, external_dependencies[subindex], (EDITOR_MODULE_CONFIGURATION)configuration_index);
				if (info->library_last_write_time < new_last_write && info->load_status == EDITOR_MODULE_LOAD_GOOD) {
					info->load_status = EDITOR_MODULE_LOAD_OUT_OF_DATE;
				}
			}
		}
	}
	return is_updated;
}

// -------------------------------------------------------------------------------------------------------------------------

bool UpdateModuleLibraryLastWrite(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration) {
	if (configuration == EDITOR_MODULE_CONFIGURATION_COUNT) {
		// Update all
		bool success = true;
		for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
			success &= UpdateModuleLibraryLastWrite(editor_state, index, (EDITOR_MODULE_CONFIGURATION)configuration_index);
		}
		return success;
	}

	ProjectModules* modules = editor_state->project_modules;
	EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);

	size_t library_last_write = info->library_last_write_time;
	info->library_last_write_time = GetModuleLibraryLastWrite(editor_state, modules->buffer[index].library_name, configuration);
	return library_last_write < info->library_last_write_time || info->library_last_write_time == 0;
}

// -------------------------------------------------------------------------------------------------------------------------

void ReleaseModule(EditorState* editor_state, unsigned int index) {
	ProjectModules* modules = editor_state->project_modules;
	Deallocate(editor_state->EditorAllocator(), modules->buffer[index].solution_path.buffer);

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
	ECS_STACK_CAPACITY_STREAM(wchar_t, source_path, 512);
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
	ProjectModules* modules = editor_state->project_modules;
	ReleaseModule(editor_state, index);

	// Remove the module from the editor components as well
	unsigned int editor_component_index = editor_state->editor_components.ModuleIndexFromReflection(editor_state, index);
	editor_state->editor_components.RemoveModule(editor_state, editor_component_index);

	modules->Remove(index);
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveModule(EditorState* editor_state, Stream<wchar_t> solution_path)
{
	unsigned int module_index = GetModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		RemoveModule(editor_state, module_index);
		return;
	}
	ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
	error_message.size = FormatString(error_message.buffer, "Removing project module {#} failed. No such module exists.", solution_path);
	EditorSetConsoleError(error_message);
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveModuleAssociatedFiles(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration)
{
	ProjectModules* modules = editor_state->project_modules;

	// Delete the associated files
	Stream<Stream<wchar_t>> associated_file_extensions(MODULE_ASSOCIATED_FILES, MODULE_ASSOCIATED_FILES_SIZE());

	ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
	GetProjectModulesFolder(editor_state, path);
	GetModuleStem(modules->buffer[module_index].library_name, configuration, path);
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
	ProjectModules* project_modules = editor_state->project_modules;
	for (size_t index = 0; index < project_modules->size; index++) {
		ReleaseModule(editor_state, index);
	}
	project_modules->Reset();
}

// -------------------------------------------------------------------------------------------------------------------------

void RetrieveModuleComponentBuildDependentEntries(const EditorState* editor_state, Stream<char> component_name, CapacityStream<Stream<char>>* dependent_components)
{
	ECS_STACK_CAPACITY_STREAM(const AppliedModule*, applied_modules, 512);
	ModulesToAppliedModules(editor_state, applied_modules);
	ModuleRetrieveComponentBuildDependentEntries(applied_modules, component_name, dependent_components);
}

Stream<Stream<char>> RetrieveModuleComponentBuildDependentEntries(const EditorState* editor_state, Stream<char> component_name, AllocatorPolymorphic allocator)
{
	ECS_STACK_CAPACITY_STREAM(const AppliedModule*, applied_modules, 512);
	ModulesToAppliedModules(editor_state, applied_modules);

	return ModuleRetrieveComponentBuildDependentEntries(applied_modules, component_name, allocator);
}

// -------------------------------------------------------------------------------------------------------------------------

void SetModuleLoadStatus(EditorState* editor_state, unsigned int module_index, bool success, EDITOR_MODULE_CONFIGURATION configuration)
{
	EditorModuleInfo* info = GetModuleInfo(editor_state, module_index, configuration);

	bool library_write_greater_than_solution = info->library_last_write_time >= editor_state->project_modules->buffer[module_index].solution_last_write_time;
	info->load_status = (EDITOR_MODULE_LOAD_STATUS)((success + library_write_greater_than_solution) * success);
}

// -------------------------------------------------------------------------------------------------------------------------

void UpdateModulesDLLImports(EditorState* editor_state)
{
	for (unsigned int index = 0; index < editor_state->project_modules->size; index++) {
		GetModuleDLLImports(editor_state, index);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void WaitModulesCompilation(EditorState* editor_state, Stream<unsigned int> module_indices, EDITOR_MODULE_CONFIGURATION* configurations)
{
	while (true) {
		bool is_any_modules_being_compiled = IsAnyModuleBeingCompiled(editor_state, module_indices, configurations);
		if (!is_any_modules_being_compiled) {
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(15));
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void TickUpdateModulesDLLImports(EditorState* editor_state)
{
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_UPDATE_MODULE_DLL_IMPORTS, LAZY_UPDATE_MODULE_DLL_IMPORTS_MS)) {
		UpdateModulesDLLImports(editor_state);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, module_path, 256);
	GetProjectModulesFolder(editor_state, module_path);

	auto file_functor = [](Stream<wchar_t> path, void* data) {
		Stream<wchar_t> filename = PathFilename(path);
		if (FindFirstToken(filename, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER).buffer != nullptr) {
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
	ECS_STACK_CAPACITY_STREAM(wchar_t, module_path, 256);
	GetProjectModulesFolder(editor_state, module_path);

	auto file_functor = [](Stream<wchar_t> path, void* data) {
		Stream<wchar_t> filename = PathFilename(path);
		if (FindFirstToken(filename, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER).buffer != nullptr) {
			RemoveFile(path);
		}
		return true;
	};
	ForEachFileInDirectoryRecursive(module_path, nullptr, file_functor);
}

// -------------------------------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------------------