#include "editorpch.h"
#include "Module.h"
#include "ECSEngineUtilities.h"
#include "..\Editor\EditorState.h"
#include "..\Editor\EditorEvent.h"
#include "..\Project\ProjectOperations.h"
#include "ECSEngineWorld.h"

using namespace ECSEngine;
ECS_CONTAINERS;

constexpr const char* CMB_BUILD_SYSTEM_PATH = "cd C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE";
constexpr const char* CMD_BUILD_SYSTEM = "devenv";
constexpr const char* BUILD_PROJECT_STRING = "/build";
constexpr const char* CLEAN_PROJECT_STRING = "/clean";
constexpr const char* REBUILD_PROJECT_STRING = "/rebuild";
constexpr const char* CMD_BUILD_SYSTEM_LOG_FILE_COMMAND = "/out";
constexpr const wchar_t* CMD_BUILD_SYSTEM_LOG_FILE_PATH = L"\\Debug\\";
constexpr const wchar_t* CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION = L".txt";

constexpr const wchar_t* CMD_BUILD_SYSTEM_WIDE = L"devenv";
constexpr const wchar_t* BUILD_PROJECT_STRING_WIDE = L"/build";
constexpr const wchar_t* CLEAN_PROJECT_STRING_WIDE = L"/clean";
constexpr const wchar_t* REBUILD_PROJECT_STRING_WIDE = L"/rebuild";
constexpr const wchar_t* CMD_BUILD_SYSTEM_LOG_FILE_COMMAND_WIDE = L"/out";

constexpr const char* CMB_BUILD_SYSTEM_INCORRECT_PARAMETER = "The operation could not be completed. The parameter is incorrect.";

constexpr const wchar_t* VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER = L".pdb.locked";
constexpr const wchar_t* MODULE_SOURCE_FILES[] = {
	L"src",
	L"Src",
	L"Source",
	L"source"
};

constexpr unsigned int CHECK_FILE_STATUS_THREAD_SLEEP_TICK = 250;

#define MODULE_BUILD_USING_SHELL

// -------------------------------------------------------------------------------------------------------------------------

bool PrintCommandStatus(EditorState* editor_state, Stream<wchar_t> log_path) {
	EDITOR_STATE(editor_state);
	Stream<char> contents = ReadWholeFileText(log_path, GetAllocatorPolymorphic(editor_allocator, AllocationType::MultiThreaded));

	if (contents.buffer != nullptr) {
		contents[contents.size - 1] = '\0';
		char* failed_character = strrchr(contents.buffer, 'f');
		failed_character -= 2;

		char _number_characters[256];
		Stream<char> number_characters(_number_characters, 0);
		while (function::IsNumberCharacter(*failed_character)) {
			number_characters.Add(*failed_character);
			failed_character--;
		}
		number_characters.SwapContents();
		size_t number = function::ConvertCharactersToInt<size_t>(number_characters);

		editor_allocator->Deallocate_ts(contents.buffer);
		if (number > 0) {
			ECS_FORMAT_TEMP_STRING(error_message, "{0} module/s have failed to build. Check the {1} log.", number, CMD_BUILD_SYSTEM);
			EditorSetConsoleError(editor_state, error_message);
			return false;
		}
		else {
			size_t string_size = strlen(CMB_BUILD_SYSTEM_INCORRECT_PARAMETER);
			if (function::CompareStrings(Stream<char>(contents.buffer + 1, string_size), Stream<char>(CMB_BUILD_SYSTEM_INCORRECT_PARAMETER, string_size))) {
				ECS_FORMAT_TEMP_STRING(error_message, "{0} could not complete the command. Incorrect parameter. Most likely, the module is missing the current configuration. Open the {1} log for more information.", CMD_BUILD_SYSTEM, CMD_BUILD_SYSTEM);
				EditorSetConsoleError(editor_state, error_message);
				return false;
			}
			return true;
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(error_message, "Could not open {0} to read the log. Open the file externally to check the command status.", log_path);
		EditorSetConsoleWarn(editor_state, error_message);
		return false;
	}
}

// -------------------------------------------------------------------------------------------------------------------------

bool AddProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, EditorModuleConfiguration configuration) {
	EDITOR_STATE(editor_state);
	ProjectModules* project_modules = editor_state->project_modules;

	if (!ExistsFileOrFolder(solution_path)) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Project module {0} solution does not exist.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(editor_state, error_message);
		return false;
	}

	unsigned int module_index = ProjectModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Project module {0} already exists.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(editor_state, error_message);
		return false;
	}

	if ((unsigned int)configuration >= (unsigned int)EditorModuleConfiguration::Count) {
		ECS_FORMAT_TEMP_STRING(
			error_message,
			"Project module {0} has configuration {1} which is invalid. Expected {2}, {3} or {4}.",
			solution_path,
			(unsigned char)configuration,
			STRING(EditorModuleConfiguration::Debug),
			STRING(EditorModuleConfiguration::Release),
			STRING(EditorModuleConfiguration::Distribution)
		);
		EditorSetConsoleError(editor_state, error_message);
		return false;
	}

	project_modules->ReserveNewElements(1);
	EditorModule* module = project_modules->buffer + project_modules->size;

	size_t total_size = (solution_path.size + library_name.size + 2) * sizeof(wchar_t);
	void* allocation = editor_allocator->Allocate(total_size, alignof(wchar_t));
	uintptr_t buffer = (uintptr_t)allocation;
	module->solution_path.InitializeFromBuffer(buffer, solution_path.size);
	buffer += sizeof(wchar_t);
	module->library_name.InitializeFromBuffer(buffer, library_name.size);
	buffer += sizeof(wchar_t);
	module->configuration = configuration;
	module->load_status = EditorModuleLoadStatus::Failed;

	module->solution_path.Copy(solution_path);
	module->solution_path[module->solution_path.size] = L'\0';
	module->library_name.Copy(library_name);
	module->library_name[module->library_name.size] = L'\0';
	project_modules->size++;

	// Add the module to the reflection table
	ECS_TEMP_STRING(source_path, 512);
	bool success = GetModuleReflectSolutionPath(editor_state, project_modules->size - 1, source_path);
	if (success) {
		unsigned int folder_hierarchy = editor_state->module_reflection->reflection->CreateFolderHierarchy(source_path.buffer);
		// Launch thread tasks to process this new entry
		success = editor_state->module_reflection->reflection->ProcessFolderHierarchy(folder_hierarchy, editor_state->task_manager);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(console_message, "Could not reflect the new added module {0} at {1}.", module->library_name, module->solution_path);
			EditorSetConsoleWarn(editor_state, console_message);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Could not find source folder for module {0} at {1}.", module->library_name, module->solution_path);
		EditorSetConsoleWarn(editor_state, console_message);
	}

	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

struct BuildThreadTaskData {
	EditorState* editor_state;
	unsigned int index;
	ConditionVariable* condition_variable;
};

template<void (*function)(EditorState* editor_state, unsigned int index)>
void BuildThreadTask(unsigned int thread_index, World* world, void* _data) {
	BuildThreadTaskData* data = (BuildThreadTaskData*)_data;
	 function(data->editor_state, data->index);
	data->condition_variable->Notify();
}

// -------------------------------------------------------------------------------------------------------------------------

struct CheckBuildStatusThreadData {
	EditorState* editor_state;
	Stream<wchar_t> path;
};

void CheckBuildStatusThreadTask(unsigned int thread_index, World* world, void* _data) {
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
	log_path.AddStream(ToStream(CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION));
	
	bool succeded = PrintCommandStatus(data->editor_state, log_path);
	if (succeded) {
		// Extract the library name and the configuration
		Stream<wchar_t> filename = function::PathFilename(data->path);
		const wchar_t* underscore = wcschr(filename.buffer, '_');
		if (underscore != nullptr) {
			Stream<wchar_t> library_name(filename.buffer, underscore - filename.buffer);
			Stream<wchar_t> configuration(underscore + 1, extension.buffer - underscore - 1);
			Stream<wchar_t> command(extension.buffer + 1, extension.size - 1);

			ECS_FORMAT_TEMP_STRING(console_string, "Command {0} for module {1} with configuration {2} completed successfully.", command, library_name, configuration);
			EditorSetConsoleInfo(data->editor_state, console_string);
		}
		else {
			EditorSetConsoleWarn(data->editor_state, ToStream("Could not deduce build command type, library name or configuration."));
			EditorSetConsoleInfo(data->editor_state, ToStream("A module finished building successfully."));
		}
	}

	// The path must be deallocated
	data->editor_state->editor_allocator->Deallocate_ts(data->path.buffer);
	ECS_ASSERT(RemoveFile(data->path));
}

// -------------------------------------------------------------------------------------------------------------------------

// Returns the index of the thread that is executing the task
unsigned int PushBuildThreadTask(EditorState* editor_state, unsigned int index, ConditionVariable* condition_variable, ThreadFunction function) {
	EDITOR_STATE(editor_state);
	BuildThreadTaskData data = { editor_state, index, condition_variable };
	return task_manager->AddDynamicTaskAndWake({ function, &data }, sizeof(data));
}

// -------------------------------------------------------------------------------------------------------------------------

void ForEachProjectModule(EditorState* editor_state, ThreadFunction function) {
	constexpr size_t MAX_THREADS = 128;
	EDITOR_STATE(editor_state);

	// Clear the log file first
	const ProjectModules* project_modules = (const ProjectModules*)editor_state->project_modules;

	// Create a condition variable that will be used by the main thread (this one) 
	// in order to sleep while the threads are doing their task/s
	ConditionVariable condition_variable;

	// Prepare thread indices for log file merge at the end and failed task indices
	size_t module_count = project_modules->size;
	unsigned int thread_indices[MAX_THREADS];
	// The graphics module located at 1 must be skipped if it is not set
	bool starting_index = project_modules->buffer[GRAPHICS_MODULE_INDEX].solution_path.size == 0;
	for (size_t index = starting_index; index < module_count; index++) {
		thread_indices[index] = PushBuildThreadTask(editor_state, index, &condition_variable, function);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

// For system() - the CRT function
void CommandLineString(CapacityStream<char>& string, Stream<wchar_t> solution_path, Stream<char> command, Stream<char> configuration, Stream<wchar_t> log_file) {
	string.Copy(ToStream(CMB_BUILD_SYSTEM_PATH));
	string.AddStream(ToStream(" && "));
	string.AddStream(ToStream(CMD_BUILD_SYSTEM));
	string.Add(' ');
	function::ConvertWideCharsToASCII(solution_path, string);
	string.Add(' ');
	string.AddStream(command);
	string.Add(' ');
	string.AddStream(configuration);
	string.Add(' ');
	string.AddStream(ToStream(CMD_BUILD_SYSTEM_LOG_FILE_COMMAND));
	string.Add(' ');
	function::ConvertWideCharsToASCII(log_file, string);
	string[string.size] = '\0';
}

// For ShellExecute() - Win32 API
void CommandLineString(const EditorState* editor_state, CapacityStream<wchar_t>& string, unsigned int module_index, Stream<wchar_t> command, Stream<wchar_t> log_file) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	
	string.Copy(ToStream(L"/c "));
	string.AddStream(ToStream(CMD_BUILD_SYSTEM_WIDE));
	string.Add(L' ');
	string.AddStream(modules->buffer[module_index].solution_path);
	string.Add(L' ');
	string.AddStream(command);
	string.Add(L' ');
	string.AddStream(ToStream(MODULE_CONFIGURATIONS_WIDE[(unsigned int)modules->buffer[module_index].configuration]));
	string.Add(L' ');
	string.AddStream(ToStream(CMD_BUILD_SYSTEM_LOG_FILE_COMMAND_WIDE));
	string.Add(L' ');
	string.AddStreamSafe(log_file);
	string.AddStream(ToStream(L" & cd . > "));
	GetProjectDebugFolder(editor_state, string);
	string.Add(ECS_OS_PATH_SEPARATOR);
	GetProjectModuleBuildFlagFile(editor_state, module_index, command, string);
	string[string.size] = '\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void RunCmdCommand(EditorState* editor_state, unsigned int index, Stream<wchar_t> command) {
	EDITOR_STATE(editor_state);

	// Construct the system string
#ifdef MODULE_BUILD_USING_CRT
	ECS_TEMP_ASCII_STRING(command_string, 512);
#else
	ECS_TEMP_STRING(command_string, 512);
#endif

	wchar_t _log_path[512];
	Stream<wchar_t> log_path(_log_path, 0);
	GetProjectModuleBuildLogPath(editor_state, index, command, log_path);
	// Clear the log file
	ClearFile(log_path);

#ifdef MODULE_BUILD_USING_CRT
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	Stream<wchar_t> solution_path = modules->buffer[index].solution_path;
	Stream<wchar_t> library_name = modules->buffer[index].library_name;
	Stream<char> configuration = editor_state->module_configuration_definitions[(unsigned int)modules->buffer[index].configuration];

	CommandLineString(command_string, solution_path, command, configuration, log_path);
#else
	CommandLineString(editor_state, command_string, index, command, log_path);
#endif
	
	// Run the command
#ifdef MODULE_BUILD_USING_CRT
	system(command_string.buffer);

	// Log the command status
	bool succeded = PrintCommandStatus(editor_state, log_path);

	if (succeded) {
		ECS_FORMAT_TEMP_STRING(console_string, "Command {0} for module {1} ({2}) with configuration {3} completed successfully.", command, library_name, solution_path, configuration);
		EditorSetConsoleInfoFocus(editor_state, console_string);
	}
#else
	HINSTANCE value = ShellExecute(NULL, L"runas", L"C:\\Windows\\System32\\cmd.exe", command_string.buffer, L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE", SW_SHOW);
	if ((uint64_t)value < 32) {
		EditorSetConsoleError(editor_state, ToStream("An error occured when creating the command prompt that builds the module."));
	}
	else {
		ECS_TEMP_STRING(flag_file, 256);
		GetProjectDebugFolder(editor_state, flag_file);
		flag_file.Add(ECS_OS_PATH_SEPARATOR);
		GetProjectModuleBuildFlagFile(editor_state, index, command, flag_file);

		// Log the command status
		CheckBuildStatusThreadData check_data;
		check_data.editor_state = editor_state;
		check_data.path = function::StringCopyTs(editor_state->editor_allocator, flag_file);
		task_manager->AddDynamicTaskAndWake({ CheckBuildStatusThreadTask, &check_data }, sizeof(check_data));
	}
#endif
}

// -------------------------------------------------------------------------------------------------------------------------

void BuildProjectModule(EditorState* editor_state, unsigned int index) {
	ProjectModules* modules = editor_state->project_modules;
	if (UpdateProjectModuleLastWrite(editor_state, index) || modules->buffer[index].load_status != EditorModuleLoadStatus::Good) {
		RunCmdCommand(editor_state, index, ToStream(BUILD_PROJECT_STRING_WIDE));
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Module {0} with configuration {1} is up to date.", 
			modules->buffer[index].library_name, MODULE_CONFIGURATIONS[(unsigned int)modules->buffer[index].configuration]);
		EditorSetConsoleInfo(editor_state, console_message);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void BuildProjectModules(EditorState* editor_state)
{
	ForEachProjectModule(editor_state, BuildThreadTask<BuildProjectModule>);
}

// -------------------------------------------------------------------------------------------------------------------------

void CleanProjectModule(EditorState* editor_state, unsigned int index) {
	RunCmdCommand(editor_state, index, ToStream(CLEAN_PROJECT_STRING_WIDE));
}

// -------------------------------------------------------------------------------------------------------------------------

void CleanProjectModules(EditorState* editor_state) {
	ForEachProjectModule(editor_state, BuildThreadTask<CleanProjectModule>);
}

// -------------------------------------------------------------------------------------------------------------------------

void RebuildProjectModule(EditorState* editor_state, unsigned int index) {
	RunCmdCommand(editor_state, index, ToStream(REBUILD_PROJECT_STRING_WIDE));
}

// -------------------------------------------------------------------------------------------------------------------------

void RebuildProjectModules(EditorState* editor_state) {
	ForEachProjectModule(editor_state, BuildThreadTask<RebuildProjectModule>);
}

// -------------------------------------------------------------------------------------------------------------------------

void DeleteProjectModuleFlagFiles(EditorState* editor_state)
{
	ECS_TEMP_STRING(debug_path, 512);
	GetProjectDebugFolder(editor_state, debug_path);

	auto functor = [](const wchar_t* path, void* _data) {
		bool success = RemoveFile(path);
		if (!success) {
			EditorSetConsoleWarn((EditorState*)_data, ToStream("Could not delete a build flag file. A manual deletion should be done."));
		}
		return true;
	};

	const wchar_t* extensions[] = {
		L".build",
		L".clean",
		L".rebuild"
	};
	ForEachFileInDirectoryWithExtension(debug_path, { extensions, std::size(extensions) }, editor_state, functor);
}

// -------------------------------------------------------------------------------------------------------------------------

void ChangeProjectModuleConfiguration(EditorState* editor_state, unsigned int index, EditorModuleConfiguration new_configuration)
{
	ProjectModules* project_modules = editor_state->project_modules;
	project_modules->buffer[index].configuration = new_configuration;
}

// -------------------------------------------------------------------------------------------------------------------------

void InitializeModuleConfigurations(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);
	
	constexpr size_t count = (unsigned int)EditorModuleConfiguration::Count;
	size_t total_memory = sizeof(Stream<char>) * count;

	size_t string_sizes[count];
	string_sizes[(unsigned int)EditorModuleConfiguration::Debug] = strlen(MODULE_CONFIGURATION_DEBUG);
	string_sizes[(unsigned int)EditorModuleConfiguration::Release] = strlen(MODULE_CONFIGURATION_RELEASE);
	string_sizes[(unsigned int)EditorModuleConfiguration::Distribution] = strlen(MODULE_CONFIGURATION_DISTRIBUTION);

	for (size_t index = 0; index < count; index++) {
		total_memory += string_sizes[index];
	}

	void* allocation = editor_allocator->Allocate(total_memory);
	uintptr_t buffer = (uintptr_t)allocation;
	editor_state->module_configuration_definitions.InitializeFromBuffer(buffer, count);
	
	for (size_t index = 0; index < count; index++) {
		editor_state->module_configuration_definitions[index].InitializeFromBuffer(buffer, string_sizes[index]);
		editor_state->module_configuration_definitions[index].Copy(Stream<char>(MODULE_CONFIGURATIONS[index], string_sizes[index]));
	}
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned int ProjectModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path)
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

unsigned int ProjectModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_name) {
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

void GetProjectModuleBuildFlagFile(const EditorState* editor_state, unsigned int module_index, Stream<wchar_t> command, CapacityStream<wchar_t>& temp_file)
{
	command.buffer++;
	command.size--;

	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	temp_file.AddStreamSafe(modules->buffer[module_index].library_name);
	temp_file.Add(L'_');
	temp_file.AddStream(ToStream(MODULE_CONFIGURATIONS_WIDE[(unsigned int)modules->buffer[module_index].configuration]));
	temp_file.Add(L'.');
	temp_file.AddStream(command);
	temp_file[temp_file.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetProjectModuleBuildLogPath(const EditorState* editor_state, unsigned int index, Stream<wchar_t> command, Stream<wchar_t>& log_path)
{
	EDITOR_STATE(editor_state);

	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	log_path.Copy(project_file->path);
	log_path.AddStream(ToStream(CMD_BUILD_SYSTEM_LOG_FILE_PATH));
	log_path.AddStream(modules->buffer[index].library_name);
	log_path.Add(L'_');
	log_path.AddStream(ToStream(MODULE_CONFIGURATIONS_WIDE[(unsigned int)modules->buffer[index].configuration]));
	wchar_t* replace_slash_with_underscore = log_path.buffer + log_path.size;
	log_path.AddStream(command);
	*replace_slash_with_underscore = L'_';
	log_path.AddStream(ToStream(CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION));
	log_path[log_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModulesFolder(const EditorState* editor_state, CapacityStream<wchar_t>& path) {
	EDITOR_STATE(editor_state);

	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStreamSafe(ToStream(PROJECT_MODULES_RELATIVE_PATH));
	path[path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, EditorModuleConfiguration configuration, CapacityStream<wchar_t>& module_path)
{
	GetModulesFolder(editor_state, module_path);
	module_path.Add(ECS_OS_PATH_SEPARATOR);
	module_path.AddStream(ToStream(MODULE_CONFIGURATIONS_WIDE[(unsigned int)configuration]));
	module_path.Add(ECS_OS_PATH_SEPARATOR);
	module_path.AddStream(library_name);
	module_path.AddStreamSafe(ToStream(ECS_MODULE_EXTENSION));
	module_path[module_path.size] = L'\0';
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetProjectModuleSolutionLastWrite(Stream<wchar_t> solution_path)
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
		data.source_names[index] = ToStream(MODULE_SOURCE_FILES[index]);
	}
	data.last_write = 0;
	 
	auto folder_functor = [](const wchar_t* path, void* _data) {
		FolderData* data = (FolderData*)_data;

		Stream<wchar_t> current_path = ToStream(path);
		Stream<wchar_t> filename = function::PathFilename(current_path);

		if (function::IsStringInStream(filename, data->source_names) != -1) {
			bool success = OS::GetFileTimes(current_path.buffer, nullptr, nullptr, &data->last_write);
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

size_t GetProjectModuleLibraryLastWrite(const EditorState* editor_state, Stream<wchar_t> library_name, EditorModuleConfiguration configuration) {
	ECS_TEMP_STRING(module_path, 256);
	GetModulePath(editor_state, library_name, configuration, module_path);

	size_t last_write = 0;
	bool success = OS::GetFileTimes(module_path.buffer, nullptr, nullptr, &last_write);
	return last_write;
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetProjectModuleSolutionLastWrite(const EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return modules->buffer[index].solution_last_write_time;
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetProjectModuleLibraryLastWrite(const EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return modules->buffer[index].library_last_write_time;
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned char* GetModuleConfigurationPtr(EditorState* editor_state, unsigned int index)
{
	ProjectModules* modules = editor_state->project_modules;
	return (unsigned char*)&modules->buffer[index].configuration;
}

// -------------------------------------------------------------------------------------------------------------------------

unsigned char GetModuleConfigurationChar(const EditorState* editor_state, unsigned int index)
{
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return (unsigned char)modules->buffer[index].configuration;
}

// -------------------------------------------------------------------------------------------------------------------------

bool GetModuleReflectSolutionPath(const EditorState* editor_state, unsigned int index, containers::CapacityStream<wchar_t>& path)
{
	Stream<wchar_t> solution_path = editor_state->project_modules->buffer[index].solution_path;
	Stream<wchar_t> solution_parent = function::PathParent(solution_path);
	path.Copy(solution_parent);
	path.Add(ECS_OS_PATH_SEPARATOR);
	
	size_t base_path_size = path.size;
	for (size_t index = 0; index < std::size(MODULE_SOURCE_FILES); index++) {
		path.AddStream(ToStream(MODULE_SOURCE_FILES[index]));
		if (ExistsFileOrFolder(path)) {
			path[path.size] = L'\0';
			return true;
		}
		path.size = base_path_size;
	}
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

void LoadEditorModule( EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	ECS_TEMP_STRING(library_path, 256);
	GetModulePath(editor_state, modules->buffer[index].library_name, modules->buffer[index].configuration, library_path);
	modules->buffer[index].ecs_module = LoadModule(library_path);
}

// -------------------------------------------------------------------------------------------------------------------------

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name, EditorModuleConfiguration configuration)
{
	ECS_TEMP_STRING(library_path, 256);
	GetModulePath(editor_state, library_name, configuration, library_path);
	return IsModule(library_path);
}

// -------------------------------------------------------------------------------------------------------------------------

bool HasModuleFunction(const EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return HasModuleFunction(editor_state, modules->buffer[index].library_name, modules->buffer[index].configuration);
}

// -------------------------------------------------------------------------------------------------------------------------

bool HasGraphicsModule(const EditorState* editor_state)
{
	const ProjectModules* project_modules = (const ProjectModules*)editor_state->project_modules;
	return project_modules->buffer[GRAPHICS_MODULE_INDEX].library_name.size > 0 && project_modules->buffer[GRAPHICS_MODULE_INDEX].solution_path.size > 0;
}

// -------------------------------------------------------------------------------------------------------------------------

bool ProjectModulesNeedsBuild(const EditorState* editor_state, unsigned int index)
{
	size_t new_last_write = GetProjectModuleSolutionLastWrite(editor_state, index);
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return modules->buffer[index].solution_last_write_time > new_last_write || modules->buffer[index].library_last_write_time == 0;
}

// -------------------------------------------------------------------------------------------------------------------------

bool LoadEditorModuleTasks(EditorState* editor_state, unsigned int index, CapacityStream<char>* error_message) {
	EDITOR_STATE(editor_state);
	ProjectModules* project_modules = editor_state->project_modules;

	if (project_modules->buffer[index].ecs_module.code != ECS_GET_MODULE_OK) {
		ECS_TEMP_STRING(module_path, 256);
		GetModulePath(editor_state, project_modules->buffer[index].library_name, project_modules->buffer[index].configuration, module_path);
		project_modules->buffer[index].ecs_module = LoadModule(module_path, editor_state->ActiveWorld(), GetAllocatorPolymorphic(editor_allocator));
	}

	if (project_modules->buffer[index].ecs_module.tasks.buffer != nullptr) {
		if (project_modules->buffer[index].library_last_write_time >= project_modules->buffer[index].solution_last_write_time) {
			project_modules->buffer[index].load_status = EditorModuleLoadStatus::Good;
		}
		else {
			project_modules->buffer[index].load_status = EditorModuleLoadStatus::OutOfDate;
		}
		project_task_graph->Add(project_modules->buffer[index].ecs_module.tasks);
		return true;
	}
	project_modules->buffer[index].load_status = EditorModuleLoadStatus::Failed;
	project_modules->buffer[index].ecs_module.tasks = { nullptr, 0 };
	return false;
}

// -------------------------------------------------------------------------------------------------------------------------

bool UpdateProjectModuleLastWrite(EditorState* editor_state, unsigned int index)
{
	ProjectModules* modules = editor_state->project_modules;
	bool is_solution_updated = UpdateProjectModuleSolutionLastWrite(editor_state, index);
	bool is_library_updated = UpdateProjectModuleLibraryLastWrite(editor_state, index);
	return is_solution_updated || is_library_updated;
}

// -------------------------------------------------------------------------------------------------------------------------

void UpdateProjectModulesLastWrite(EditorState* editor_state) {
	ProjectModules* modules = editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		UpdateProjectModuleLastWrite(editor_state, index);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

bool UpdateProjectModuleSolutionLastWrite(EditorState* editor_state, unsigned int index) {
	ProjectModules* modules = editor_state->project_modules;
	
	size_t solution_last_write = modules->buffer[index].solution_last_write_time;
	modules->buffer[index].solution_last_write_time = GetProjectModuleSolutionLastWrite(modules->buffer[index].solution_path);
	bool needs_update = solution_last_write < modules->buffer[index].solution_last_write_time || modules->buffer[index].solution_last_write_time == 0;

	// Update the reflection types if it necessary
	if (solution_last_write < modules->buffer[index].solution_last_write_time) {
		ECS_TEMP_STRING(source_path, 512);
		bool success = GetModuleReflectSolutionPath(editor_state, index, source_path);
		if (success) {
			success = editor_state->module_reflection->reflection->UpdateFolderHierarchy(source_path.buffer, editor_state->task_manager);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(console_message, "An error occured during reflection update for module {0}.", modules->buffer[index].solution_path);
				EditorSetConsoleWarn(editor_state, console_message);
			}
		}
		else {
			ECS_FORMAT_TEMP_STRING(console_message, "Could not find module's {0} source folder.", modules->buffer[index].library_name);
			EditorSetConsoleWarn(editor_state, console_message);
		}
	}

	return needs_update;
}

// -------------------------------------------------------------------------------------------------------------------------

bool UpdateProjectModuleLibraryLastWrite(EditorState* editor_state, unsigned int index) {
	ProjectModules* modules = editor_state->project_modules;

	size_t library_last_write = modules->buffer[index].library_last_write_time;
	modules->buffer[index].library_last_write_time = GetProjectModuleLibraryLastWrite(editor_state, modules->buffer[index].library_name, modules->buffer[index].configuration);
	return library_last_write < modules->buffer[index].library_last_write_time || modules->buffer[index].library_last_write_time == 0;
}

// -------------------------------------------------------------------------------------------------------------------------

void ReleaseProjectModule(EditorState* editor_state, unsigned int index) {
	EDITOR_STATE(editor_state);

	ProjectModules* modules = editor_state->project_modules;
	editor_allocator->Deallocate(modules->buffer[index].solution_path.buffer);

	// Remove the module from the reflection
	ECS_TEMP_STRING(source_path, 512);
	bool success = GetModuleReflectSolutionPath(editor_state, index, source_path);
	if (success) {
		// Release the UI instances and types
		unsigned int hierarchy_index = editor_state->ui_reflection->reflection->GetHierarchyIndex(source_path.buffer);
		ECS_ASSERT(hierarchy_index != -1);
		editor_state->ui_reflection->DestroyAllFromFolderHierarchy(hierarchy_index);
		editor_state->ui_reflection->reflection->RemoveFolderHierarchy(hierarchy_index);
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Could not find module's {0} source folder.", modules->buffer[index].library_name);
		EditorSetConsoleWarn(editor_state, console_message);
	}

	if (modules->buffer[index].load_status != EditorModuleLoadStatus::Failed) {
		ReleaseModule(modules->buffer[index].ecs_module, GetAllocatorPolymorphic(editor_allocator));

		RemoveProjectModuleAssociatedFiles(editor_state, index);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveProjectModule(EditorState* editor_state, unsigned int index) {
	EDITOR_STATE(editor_state);

	ProjectModules* modules = editor_state->project_modules;
	ReleaseProjectModule(editor_state, index);
	if (index != GRAPHICS_MODULE_INDEX) {
		modules->Remove(index);
	}
	else {
		// Reset the graphics module
		ResetProjectGraphicsModule(editor_state);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path)
{
	EDITOR_STATE(editor_state);

	unsigned int module_index = ProjectModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		RemoveProjectModule(editor_state, module_index);
		return;
	}
	ECS_TEMP_ASCII_STRING(error_message, 256);
	error_message.size = function::FormatString(error_message.buffer, "Removing project module {0} failed. No such module exists.", solution_path);
	EditorSetError(editor_state, error_message);
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveProjectModuleAssociatedFiles(EditorState* editor_state, unsigned int module_index)
{
	EDITOR_STATE(editor_state);
	ProjectModules* modules = editor_state->project_modules;

	// Delete the associated files
	Stream<const wchar_t*> associated_file_extensions(MODULE_ASSOCIATED_FILES, std::size(MODULE_ASSOCIATED_FILES));

	ECS_TEMP_STRING(path, 256);
	GetModulePath(editor_state, modules->buffer[module_index].library_name, modules->buffer[module_index].configuration, path);
	size_t path_size = path.size;

	for (size_t index = 0; index < std::size(MODULE_ASSOCIATED_FILES); index++) {
		path.size = path_size;
		path.AddStreamSafe(ToStream(MODULE_ASSOCIATED_FILES[index]));
		if (ExistsFileOrFolder(path)) {
			OS::DeleteFileWithError(path, console);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void RemoveProjectModuleAssociatedFiles(EditorState* editor_state, Stream<wchar_t> solution_path)
{
	unsigned int module_index = ProjectModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		RemoveProjectModuleAssociatedFiles(editor_state, module_index);
		ECS_FORMAT_TEMP_STRING(error_message, "Could not find module with solution path {0}.", solution_path);
		EditorSetConsoleError(editor_state, error_message);
	}
}

// -------------------------------------------------------------------------------------------------------------------------

void ResetProjectModules(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	ProjectModules* project_modules = editor_state->project_modules;
	for (size_t index = 0; index < project_modules->size; index++) {
		ReleaseProjectModule(editor_state, index);
	}
	project_modules->size = 1;
}

// -------------------------------------------------------------------------------------------------------------------------

void ResetProjectGraphicsModule(EditorState* editor_state)
{
	ProjectModules* modules = editor_state->project_modules;

	modules->buffer[GRAPHICS_MODULE_INDEX].library_last_write_time = 0;
	modules->buffer[GRAPHICS_MODULE_INDEX].solution_last_write_time = 0;
	modules->buffer[GRAPHICS_MODULE_INDEX].library_name = { nullptr, 0 };
	modules->buffer[GRAPHICS_MODULE_INDEX].solution_path = { nullptr, 0 };
	modules->buffer[GRAPHICS_MODULE_INDEX].load_status = EditorModuleLoadStatus::Failed;
	modules->buffer[GRAPHICS_MODULE_INDEX].configuration = EditorModuleConfiguration::Debug;
	modules->buffer[GRAPHICS_MODULE_INDEX].ecs_module.tasks = { nullptr, 0 };
	modules->buffer[GRAPHICS_MODULE_INDEX].ecs_module.code = ECS_GET_MODULE_FAULTY_PATH;
	modules->buffer[GRAPHICS_MODULE_INDEX].ecs_module.function = nullptr;
	modules->buffer[GRAPHICS_MODULE_INDEX].ecs_module.os_module_handle = nullptr;
}

// -------------------------------------------------------------------------------------------------------------------------

void SetModuleLoadStatus(EditorModule* module, bool has_functions)
{
	bool library_write_greater_than_solution = module->library_last_write_time >= module->solution_last_write_time;
	module->load_status = (EditorModuleLoadStatus)((has_functions + library_write_greater_than_solution) * has_functions);
}

// -------------------------------------------------------------------------------------------------------------------------

bool SetProjectGraphicsModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, EditorModuleConfiguration configuration)
{
	EDITOR_STATE(editor_state);
	ProjectModules* project_modules = editor_state->project_modules;

	if (!ExistsFileOrFolder(solution_path)) {
		ECS_TEMP_ASCII_STRING(error_message, 512);
		error_message.size = function::FormatString(error_message.buffer, "Graphics module {0} solution does not exist.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(editor_state, error_message);
		return false;
	}

	unsigned int module_index = ProjectModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Graphics module {0} already exists.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(editor_state, error_message);
		return false;
	}

	if ((unsigned int)configuration >= (unsigned int)EditorModuleConfiguration::Count) {
		ECS_FORMAT_TEMP_STRING(
			error_message,
			"Graphics module {0} has configuration {1} which is invalid. Expected {2}, {3} or {4}.",
			solution_path,
			(unsigned char)configuration,
			STRING(EditorModuleConfiguration::Debug),
			STRING(EditorModuleConfiguration::Release),
			STRING(EditorModuleConfiguration::Distribution)
		);
		EditorSetConsoleError(editor_state, error_message);
		return false;
	}

	ReleaseProjectModule(editor_state, GRAPHICS_MODULE_INDEX);
	EditorModule* module = project_modules->buffer + GRAPHICS_MODULE_INDEX;

	size_t total_size = (solution_path.size + library_name.size) * sizeof(wchar_t);
	void* allocation = editor_allocator->Allocate(total_size, alignof(wchar_t));
	uintptr_t buffer = (uintptr_t)allocation;
	module->solution_path.InitializeFromBuffer(buffer, solution_path.size);
	module->library_name.InitializeFromBuffer(buffer, library_name.size);
	module->configuration = configuration;
	module->load_status = EditorModuleLoadStatus::Failed;

	module->solution_path.Copy(solution_path);
	module->library_name.Copy(library_name);
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state)
{
	ECS_TEMP_STRING(module_path, 256);
	GetModulesFolder(editor_state, module_path);

	auto file_functor = [](const wchar_t* path, void* data) {
		Stream<wchar_t> filename = function::PathFilename(ToStream(path));
		if (wcsstr(filename.buffer, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER) != nullptr) {
			size_t* count = (size_t*)data;
			*count += GetFileByteSize(path);
		}
		return true;
	};

	size_t count = 0;
	ForEachFileInDirectoryRecursive(module_path.buffer, &count, file_functor);
	return count;
}

// -------------------------------------------------------------------------------------------------------------------------

void DeleteVisualStudioLockedFiles(const EditorState* editor_state) {
	ECS_TEMP_STRING(module_path, 256);
	GetModulesFolder(editor_state, module_path);

	auto file_functor = [](const wchar_t* path, void* data) {
		Stream<wchar_t> filename = function::PathFilename(ToStream(path));
		if (wcsstr(filename.buffer, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER) != nullptr) {
			RemoveFile(path);
		}
		return true;
	};
	ForEachFileInDirectoryRecursive(module_path.buffer, nullptr, file_functor);
}

// -------------------------------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------------------