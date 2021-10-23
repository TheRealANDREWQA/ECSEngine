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
constexpr const wchar_t* CMD_BUILD_SYSTEM_LOG_FILE_PATH = L"\\Debug\\Build.txt";
constexpr const wchar_t* CMD_BUILD_SYSTEM_LOG_FILENAME = L"\\Debug\\Build";
constexpr const wchar_t* CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION = L".txt";

constexpr const char* CMB_BUILD_SYSTEM_INCORRECT_PARAMETER = "The operation could not be completed. The parameter is incorrect.";

constexpr const wchar_t* VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER = L".pdb.locked";


void AddProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, ModuleConfiguration configuration) {
	EDITOR_STATE(editor_state);

	if (!ExistsFileOrFolder(solution_path)) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Project module {0} solution does not exist.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(editor_state, error_message);
		return;
	}

	unsigned int module_index = ProjectModuleIndex(editor_state, solution_path);
	if (module_index != -1) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Project module {0} already exists.", solution_path);
		error_message.AssertCapacity();
		EditorSetConsoleError(editor_state, error_message);
		return;
	}

	if ((unsigned int)configuration >= (unsigned int)ModuleConfiguration::Count) {
		ECS_FORMAT_TEMP_STRING(
			error_message,
			"Project module {0} has configuration {1} which is invalid. Expected {2}, {3} or {4}.",
			solution_path,
			(unsigned char)configuration,
			STRING(ModuleConfiguration::Debug),
			STRING(ModuleConfiguration::Release),
			STRING(ModuleConfiguration::Distribution)
		);
		EditorSetConsoleError(editor_state, error_message);
		return;
	}

	ProjectModules* project_modules = (ProjectModules*)editor_state->project_modules;
	project_modules->ReserveNewElements(1);
	Module* module = project_modules->buffer + project_modules->size;
	
	size_t total_size = (solution_path.size + library_name.size) * sizeof(wchar_t);
	void* allocation = editor_allocator->Allocate(total_size, alignof(wchar_t));
	uintptr_t buffer = (uintptr_t)allocation;
	module->solution_path.InitializeFromBuffer(buffer, solution_path.size);
	module->library_name.InitializeFromBuffer(buffer, library_name.size);
	module->configuration = configuration;

	module->solution_path.Copy(solution_path);
	module->library_name.Copy(library_name);
	project_modules->size++;
}

void WriteModuleLogDelimitator(std::ofstream& stream) {
	constexpr const char* STRING = "\n------------------------------------------- Module end -------------------------------------------------\n\n";
	stream.write(STRING, strlen(STRING));
}

void WriteModuleLogDelimitator(Stream<wchar_t> log_path) {
	std::ofstream stream(log_path.buffer, std::ios::app);

	if (stream.good()) {
		WriteModuleLogDelimitator(stream);
	}
}

struct ThreadTaskData {
	EditorState* editor_state;
	unsigned int index;
	ConditionVariable* condition_variable;
	bool* success;
};

template<bool (*function)(EditorState* editor_state, unsigned int index, unsigned int thread_index)>
void BuildThreadTask(unsigned int thread_index, World* world, void* _data) {
	ThreadTaskData* data = (ThreadTaskData*)_data;
	bool success = function(data->editor_state, data->index, thread_index);
	data->condition_variable->Notify();
	*data->success = success;
}

// Returns the index of the thread that is executing the task
unsigned int PushBuildThreadTask(EditorState* editor_state, unsigned int index, ConditionVariable* condition_variable, bool* success_status, ThreadFunction function) {
	EDITOR_STATE(editor_state);
	ThreadTaskData data = { editor_state, index, condition_variable, success_status };
	return task_manager->AddDynamicTaskAndWake({ function, &data }, sizeof(data));
}

void ForEachProjectModule(EditorState* editor_state, ThreadFunction function) {
	constexpr size_t MAX_THREADS = 128;
	EDITOR_STATE(editor_state);

	// Clear the log file first
	const ProjectModules* project_modules = (const ProjectModules*)editor_state->project_modules;
	wchar_t temp_memory[256];
	Stream<wchar_t> log_path(temp_memory, 0);
	GetProjectModuleBuildLogPath(editor_state, log_path);
	ClearFile(log_path);

	// Create a condition variable that will be used by the main thread (this one) 
	// in order to sleep while the threads are doing their task/s
	ConditionVariable condition_variable;

	// Prepare thread indices for log file merge at the end and failed task indices
	size_t module_count = project_modules->size;
	unsigned int thread_indices[MAX_THREADS];
	bool successful_tasks[MAX_THREADS];
	for (size_t index = 0; index < module_count; index++) {
		successful_tasks[index] = true;
		thread_indices[index] = PushBuildThreadTask(editor_state, index, &condition_variable, successful_tasks + index, function);
	}

	// Wait while the threads are doing their task/s
	condition_variable.Wait(module_count);

	// Check failed modules and print the console output accordingly
	// This avoids pedantic console messages that add up when many modules are being
	// built; only successful tasks need to be printed because the build task
	// will print the error message 
	ECS_TEMP_ASCII_STRING(console_output, 512);
	unsigned int successful_task_count = 0;
	for (size_t index = 0; index < module_count; index++) {
		successful_task_count += successful_tasks[index];
	}
	console_output.size = function::FormatString(console_output.buffer, "{0} commands have executed successfully. The modules are:", successful_task_count);
	for (size_t index = 0; index < module_count; index++) {
		if (successful_tasks[index]) {
			console_output.Add(' ');
			function::ConvertWideCharsToASCII(project_modules->buffer[index].library_name, console_output);
			console_output.size += project_modules->buffer[index].library_name.size;
			console_output.AddSafe(',');
		}
	}
	// Last character will always be a ,
	console_output[console_output.size - 1] = '.';
	if (successful_task_count > 0) {
		EditorSetConsoleInfoFocus(editor_state, console_output);
	}

	// Merge thread build outputs to the main log
	std::ofstream log_stream(log_path.buffer, std::ios::trunc);
	wchar_t _thread_log_path[256];
	Stream<wchar_t> thread_log_path(_thread_log_path, 0);

	unsigned int thread_count = task_manager->GetThreadCount();
	unsigned int iteration_count = function::PredicateValue<unsigned int>(module_count > thread_count, thread_count, module_count);
	for (size_t index = 0; index < iteration_count; index++) {
		thread_log_path.size = 0;
		GetProjectModuleThreadBuildLogPath(editor_state, thread_log_path, thread_indices[index]);
		if (ExistsFileOrFolder(thread_log_path)) {
			std::ifstream thread_stream(thread_log_path.buffer);
			size_t file_size = function::GetFileByteSize(thread_stream);
			char* allocation = (char*)_malloca(file_size);
			thread_stream.read(allocation, file_size);
			size_t read_count = thread_stream.gcount();
			log_stream.write(allocation, read_count);

			thread_stream.close();
			OS::DeleteFileWithError(thread_log_path, console);
		}
	}
}

void CommandLineString(CapacityStream<char> string, Stream<wchar_t> solution_path, Stream<char> command, Stream<char> configuration, Stream<wchar_t> log_file) {
	string.Copy(ToStream(CMB_BUILD_SYSTEM_PATH));
	string.AddStream(ToStream(" && "));
	string.AddStream(ToStream(CMD_BUILD_SYSTEM));
	string.Add(' ');
	function::ConvertWideCharsToASCII(solution_path, string);
	string.size += solution_path.size;
	string.Add(' ');
	string.AddStream(command);
	string.Add(' ');
	string.AddStream(configuration);
	string.Add(' ');
	string.AddStream(ToStream(CMD_BUILD_SYSTEM_LOG_FILE_COMMAND));
	string.Add(' ');
	function::ConvertWideCharsToASCII(log_file, string);
	string.size += log_file.size;
	string.AddSafe('\0');
}

bool PrintCommandStatus(EditorState* editor_state, Stream<char> configuration, Stream<wchar_t> log_path) {
	std::ifstream stream(std::wstring(log_path.buffer, log_path.buffer + log_path.size));

	if (stream.good()) {
		EDITOR_STATE(editor_state);
		
		size_t file_size = function::GetFileByteSize(stream);
		char* allocation = (char*)editor_allocator->Allocate_ts(file_size + 1);
		stream.read(allocation, file_size);

		size_t gcount = stream.gcount();
		allocation[file_size] = '\0';
		char* failed_character = strrchr(allocation, 'f');
		failed_character -= 2;

		char _number_characters[256];
		Stream<char> number_characters(_number_characters, 0);
		while (function::IsNumberCharacter(*failed_character)) {
			number_characters.Add(*failed_character);
			failed_character--;
		}
		number_characters.SwapContents();
		size_t number = function::ConvertCharactersToInt<size_t>(number_characters);

		editor_allocator->Deallocate_ts(allocation);
		if (number > 0) {
			ECS_FORMAT_TEMP_STRING(error_message, "{0} module/s have failed to build. Check the {1} log.", number, CMD_BUILD_SYSTEM);
			EditorSetConsoleError(editor_state, error_message);
			return false;
		}
		else {
			size_t string_size = strlen(CMB_BUILD_SYSTEM_INCORRECT_PARAMETER);
			if (function::CompareStrings(Stream<char>(allocation + 1, string_size), Stream<char>(CMB_BUILD_SYSTEM_INCORRECT_PARAMETER, string_size))) {
				ECS_FORMAT_TEMP_STRING(error_message, "{0} could not complete the command. Incorrect parameter. Most likely, the module is missing the configuration {1}. Open the {2} log for more information.", CMD_BUILD_SYSTEM, configuration, CMD_BUILD_SYSTEM);
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

bool launchDebugger()
{
	// Get System directory, typically c:\windows\system32
	std::wstring systemDir(MAX_PATH + 1, '\0');
	UINT nChars = GetSystemDirectoryW(&systemDir[0], systemDir.length());
	if (nChars == 0) return false; // failed to get system directory
	systemDir.resize(nChars);

	// Get process ID and create the command line
	DWORD pid = GetCurrentProcessId();
	std::wostringstream s;
	s << systemDir << L"\\vsjitdebugger.exe -p " << pid;
	std::wstring cmdLine = s.str();

	// Start debugger process
	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcessW(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return false;

	// Close debugger process handles to eliminate resource leak
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	// Wait for the debugger to attach
	while (!IsDebuggerPresent()) Sleep(100);

	// Stop execution so the debugger can take over
	DebugBreak();
	return true;
}

bool RunCmdCommand(EditorState* editor_state, unsigned int index, Stream<char> command, unsigned int thread_index) {
	EDITOR_STATE(editor_state);

	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	Stream<wchar_t> solution_path = modules->buffer[index].solution_path;
	Stream<wchar_t> library_name = modules->buffer[index].library_name;
	Stream<char> configuration = editor_state->module_configurations[(unsigned int)modules->buffer[index].configuration];
	// Construct the system string
	ECS_TEMP_ASCII_STRING(command_string, 512);
	wchar_t _log_path[512];
	Stream<wchar_t> log_path(_log_path, 0);
	if (thread_index == -1) {
		GetProjectModuleBuildLogPath(editor_state, log_path);
		// Clear the log file
		ClearFile(log_path);
	}
	else {
		GetProjectModuleThreadBuildLogPath(editor_state, log_path, thread_index);
	}
	CommandLineString(command_string, solution_path, command, configuration, log_path);
	
	//launchDebugger();
	
	// Run the command
	system(command_string.buffer);
	
	// Log the command status
	bool succeded = PrintCommandStatus(editor_state, configuration, log_path);

	if (succeded && thread_index == -1) {
		ECS_FORMAT_TEMP_STRING(console_string, "Command {0} for module {1} ({2}) with configuration {3} completed successfully.", command, library_name, solution_path, configuration);
		EditorSetConsoleInfoFocus(editor_state, console_string);
	}

	WriteModuleLogDelimitator(log_path);
	return succeded;
}

bool BuildProjectModule(EditorState* editor_state, unsigned int index, unsigned int thread_index) {
	if (UpdateProjectModuleLastWrite(editor_state, index) || !HasModuleFunction(editor_state, index)) {
		return RunCmdCommand(editor_state, index, ToStream(BUILD_PROJECT_STRING), thread_index);
	}
	return true;
}

void BuildProjectModules(EditorState* editor_state)
{
	ForEachProjectModule(editor_state, BuildThreadTask<BuildProjectModule>);
}

bool CleanProjectModule(EditorState* editor_state, unsigned int index, unsigned int thread_index) {
	return RunCmdCommand(editor_state, index, ToStream(CLEAN_PROJECT_STRING), thread_index);
}

void CleanProjectModules(EditorState* editor_state) {
	ForEachProjectModule(editor_state, BuildThreadTask<CleanProjectModule>);
}

bool RebuildProjectModule(EditorState* editor_state, unsigned int index, unsigned int thread_index) {
	return RunCmdCommand(editor_state, index, ToStream(REBUILD_PROJECT_STRING), thread_index);
}

void RebuildProjectModules(EditorState* editor_state) {
	ForEachProjectModule(editor_state, BuildThreadTask<RebuildProjectModule>);
}

void ChangeProjectModuleConfiguration(EditorState* editor_state, unsigned int index, ModuleConfiguration new_configuration)
{
	ProjectModules* project_modules = (ProjectModules*)editor_state->project_modules;
	project_modules->buffer[index].configuration = new_configuration;
}

void InitializeModuleConfigurations(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);
	
	constexpr size_t count = (unsigned int)ModuleConfiguration::Count;
	size_t total_memory = sizeof(Stream<char>) * count;

	size_t string_sizes[count];
	string_sizes[(unsigned int)ModuleConfiguration::Debug] = strlen(MODULE_CONFIGURATION_DEBUG);
	string_sizes[(unsigned int)ModuleConfiguration::Release] = strlen(MODULE_CONFIGURATION_RELEASE);
	string_sizes[(unsigned int)ModuleConfiguration::Distribution] = strlen(MODULE_CONFIGURATION_DISTRIBUTION);

	for (size_t index = 0; index < count; index++) {
		total_memory += string_sizes[index];
	}

	void* allocation = editor_allocator->Allocate(total_memory);
	uintptr_t buffer = (uintptr_t)allocation;
	editor_state->module_configurations.InitializeFromBuffer(buffer, count);
	
	for (size_t index = 0; index < count; index++) {
		editor_state->module_configurations[index].InitializeFromBuffer(buffer, string_sizes[index]);
		editor_state->module_configurations[index].Copy(Stream<char>(MODULE_CONFIGURATIONS[index], string_sizes[index]));
	}
}

unsigned int ProjectModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path)
{
	EDITOR_STATE(editor_state);

	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		if (function::CompareStrings(modules->buffer[index].solution_path, solution_path)) {
			return index;
		}
	}
	return -1;
}

unsigned int ProjectModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_name) {
	EDITOR_STATE(editor_state);

	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		if (function::CompareStrings(modules->buffer[index].library_name, library_name)) {
			return index;
		}
	}
	return -1;
}

void GetProjectModuleBuildLogPath(const EditorState* editor_state, Stream<wchar_t>& log_path)
{
	EDITOR_STATE(editor_state);

	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	log_path.Copy(project_file->path);
	log_path.AddStream(ToStream(CMD_BUILD_SYSTEM_LOG_FILE_PATH));
	log_path[log_path.size] = L'\0';
}

void GetProjectModuleThreadBuildLogPath(const EditorState* editor_state, Stream<wchar_t>& log_path, unsigned int thread_index)
{
	EDITOR_STATE(editor_state);
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	log_path.Copy(project_file->path);
	log_path.AddStream(ToStream(CMD_BUILD_SYSTEM_LOG_FILENAME));
	function::ConvertIntToChars(log_path, thread_index);
	log_path.AddStream(ToStream(CMB_BUILD_SYSTEM_LOG_FILE_EXTENSION));
	log_path[log_path.size] = L'\0';
}

void GetProjectModulePath(const EditorState* editor_state, CapacityStream<wchar_t>& path) {
	EDITOR_STATE(editor_state);

	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStreamSafe(ToStream(PROJECT_MODULES_RELATIVE_PATH));
	path[path.size] = L'\0';
}

size_t GetProjectModuleLastWrite(Stream<wchar_t> solution_path)
{
	ECS_TEMP_STRING(null_terminated_string, 256);
	null_terminated_string.Copy(solution_path);
	null_terminated_string.AddSafe(L'\0');

	size_t last_write;
	bool success = OS::GetRelativeFileTimes(null_terminated_string.buffer, nullptr, nullptr, &last_write);
	return last_write;
}

size_t GetProjectModuleLastWrite(const EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return modules->buffer[index].last_write_time;
}

unsigned char* GetProjectModuleConfigurationPtr(EditorState* editor_state, unsigned int index)
{
	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;
	return (unsigned char*)&modules->buffer[index].configuration;
}

unsigned char GetProjectModuleConfigurationChar(const EditorState* editor_state, unsigned int index)
{
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return (unsigned char)modules->buffer[index].configuration;
}

GetModuleFunctionData GetModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name) {
	GetModuleFunctionData data;
	EDITOR_STATE(editor_state);

	ECS_TEMP_STRING(library_path, 256);
	GetProjectModulePath(editor_state, library_path);
	library_path.Add(ECS_OS_PATH_SEPARATOR);
	library_path.AddStream(library_name);
	library_path.AddStreamSafe(ToStream(MODULE_EXTENSION));
	library_path[library_path.size] = L'\0';

	HMODULE module_handle = LoadLibrary(library_path.buffer);

	if (module_handle == nullptr) {
		data.code = GetModuleFunctionReturnCode::FaultyModulePath;
		data.function = nullptr;
		data.os_module_handle = nullptr;
		return data;
	}

	data.function = (ModuleFunction)GetProcAddress(module_handle, MODULE_FUNCTION_NAME);
	if (data.function == nullptr) {
		FreeLibrary(module_handle);
		data.code = GetModuleFunctionReturnCode::ModuleFunctionMissing;
		data.function = nullptr;
		data.os_module_handle = nullptr;
		return data;
	}

	data.code = GetModuleFunctionReturnCode::OK;
	data.os_module_handle = module_handle;
	return data;
}

GetModuleFunctionData GetModuleFunction(const EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return GetModuleFunction(editor_state, modules->buffer[index].library_name);
}

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name)
{
	auto data = GetModuleFunction(editor_state, library_name);
	if (data.code == GetModuleFunctionReturnCode::OK) {
		FreeLibrary(data.os_module_handle);
		return true;
	}
	return false;
}

bool HasModuleFunction(const EditorState* editor_state, unsigned int index) {
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return HasModuleFunction(editor_state, modules->buffer[index].library_name);
}

bool ProjectModulesNeedsBuild(const EditorState* editor_state, unsigned int index)
{
	size_t new_last_write = GetProjectModuleLastWrite(editor_state, index);
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;
	return modules->buffer[index].last_write_time > new_last_write;
}

bool GetModuleTasks(EditorState* editor_state, containers::Stream<wchar_t> library_name, containers::Stream<TaskGraphElement>& tasks) {
	auto data = GetModuleFunction(editor_state, library_name);
	if (data.code != GetModuleFunctionReturnCode::OK) {
		return false;
	}

	EDITOR_STATE(editor_state);
	data.function(editor_state->ActiveWorld(), tasks);

	FreeLibrary(data.os_module_handle);
	return true;
}

bool GetModuleTasks(EditorState* editor_state, containers::Stream<wchar_t> library_name, containers::Stream<TaskGraphElement>& tasks, CapacityStream<char>& error_message) {
	auto data = GetModuleFunction(editor_state, library_name);
	if (data.code != GetModuleFunctionReturnCode::OK) {
		if (data.code == GetModuleFunctionReturnCode::FaultyModulePath) {
			error_message.size = function::FormatString(error_message.buffer, "Could not find module {0}. The module does not exist, wrong path or access denied.", library_name);
		}
		else  if (data.code == GetModuleFunctionReturnCode::ModuleFunctionMissing) {
			error_message.size = function::FormatString(error_message.buffer, "Module {0} was loaded successfully but the module function is missing.", library_name);
		}
		error_message.AssertCapacity();
		return false;
	}

	EDITOR_STATE(editor_state);
	data.function(editor_state->ActiveWorld(), tasks);

	FreeLibrary(data.os_module_handle);
	return true;
}

bool LoadModuleTasks(EditorState* editor_state, Stream<wchar_t> library_name, CapacityStream<char>* error_message) {
	auto data = GetModuleFunction(editor_state, library_name);

	if (data.code != GetModuleFunctionReturnCode::OK) {
		if (error_message != nullptr) {
			if (data.code == GetModuleFunctionReturnCode::FaultyModulePath) {
				error_message->size = function::FormatString(error_message->buffer, "Could not find module at {0}. The module does not exist, wrong path or access denied.", library_name);
			}
			else  if (data.code == GetModuleFunctionReturnCode::ModuleFunctionMissing) {
				error_message->size = function::FormatString(error_message->buffer, "Module {0} was loaded successfully but the module function is missing.", library_name);
			}
			error_message->AssertCapacity();
		}
		return false;
	}

	EDITOR_STATE(editor_state);
	TaskGraphElement _task_elements[128];
	Stream<TaskGraphElement> task_elements(_task_elements, 0);
	data.function(editor_state->ActiveWorld(), task_elements);

	project_task_graph->Add(task_elements);

	FreeLibrary(data.os_module_handle);
	return true;
}

bool UpdateProjectModuleLastWrite(EditorState* editor_state, unsigned int index)
{
	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;
	size_t last_write = modules->buffer[index].last_write_time;
	modules->buffer[index].last_write_time = GetProjectModuleLastWrite(modules->buffer[index].solution_path);
	return last_write > modules->buffer[index].last_write_time;
}

void UpdateProjectModulesLastWrite(EditorState* editor_state) {
	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;
	for (size_t index = 0; index < modules->size; index++) {
		UpdateProjectModuleLastWrite(editor_state, index);
	}
}

void RemoveProjectModule(EditorState* editor_state, unsigned int index) {
	EDITOR_STATE(editor_state);

	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;
	editor_allocator->Deallocate(modules->buffer[index].solution_path.buffer);
	modules->Remove(index);
}

void RemoveProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path)
{
	EDITOR_STATE(editor_state);

	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;
	size_t module_count = modules->size;
	for (size_t index = 0; index < module_count; index++) {
		if (function::CompareStrings(modules->buffer[index].solution_path, solution_path)) {
			RemoveProjectModule(editor_state, index);
			return;
		}
	}
	ECS_TEMP_ASCII_STRING(error_message, 256);
	error_message.size = function::FormatString(error_message.buffer, "Remove project module {0} failed. No such module exists.", solution_path);
	EditorSetError(editor_state, error_message);
}

void ResetProjectModules(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	ProjectModules* project_modules = (ProjectModules*)editor_state->project_modules;
	for (size_t index = 0; index < project_modules->size; index++) {
		editor_allocator->Deallocate(project_modules->buffer[index].solution_path.buffer);
	}
	project_modules->Reset();
}


size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state)
{
	ECS_TEMP_STRING(module_path, 256);
	GetProjectModulePath(editor_state, module_path);

	auto file_functor = [](const std::filesystem::path& std_path, void* data) {
		const wchar_t* filename = std_path.c_str();
		filename = function::PathFilename(ToStream(filename)).buffer;
		if (wcsstr(filename, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER) != nullptr) {
			size_t* count = (size_t*)data;
			*count += std::filesystem::file_size(std_path);
		}
		return true;
	};

	size_t count = 0;
	ForEachFileInDirectory(module_path.buffer, &count, file_functor);
	return count;
}

void DeleteVisualStudioLockedFiles(const EditorState* editor_state) {
	ECS_TEMP_STRING(module_path, 256);
	GetProjectModulePath(editor_state, module_path);

	auto file_functor = [](const std::filesystem::path& std_path, void* data) {
		const wchar_t* filename = std_path.c_str();
		filename = function::PathFilename(ToStream(filename)).buffer;
		if (wcsstr(filename, VISUAL_STUDIO_LOCKED_FILE_IDENTIFIER) != nullptr) {
			RemoveFile(std_path.c_str());
		}
		return true;
	};
	ForEachFileInDirectory(module_path.buffer, nullptr, file_functor);
}