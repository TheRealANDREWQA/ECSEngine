#pragma once
#include "ModuleDefinition.h"

using namespace ECSEngine;

using ProjectModules = containers::ResizableStream<Module, MemoryManager>;

struct EditorState;

void AddProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, ModuleConfiguration configuration);

// Runs on multiple threads
void BuildProjectModules(EditorState* editor_state);

// Editor state is needed in order to print to console; thread_index -1 means
// that it runs in a single threaded environment and that it will clear the log file
// and write in it
// Returns whether or not it succeded
bool BuildProjectModule(EditorState* editor_state, unsigned int index, unsigned int thread_index = -1);

// Runs on multiple threads
void CleanProjectModules(EditorState* editor_state);

// Editor state is needed in order to print to console; thread_index -1 means
// that it runs in a single threaded environment and that it will clear the log file
// and write in it; 
// Returns whether or not it succeded
bool CleanProjectModule(EditorState* editor_state, unsigned int index, unsigned int thread_index = -1);

// Runs on multiple threads
void RebuildProjectModules(EditorState* editor_state);

// Editor state is needed in order to print to console; thread_index -1 means
// that it runs in a single threaded environment and that it will clear the log file
// and write in it
// Returns whether or not it succeded
bool RebuildProjectModule(EditorState* editor_state, unsigned int index, unsigned int thread_index = -1);

void ChangeProjectModuleConfiguration(EditorState* editor_state, unsigned int index, ModuleConfiguration new_configuration);

void InitializeModuleConfigurations(EditorState* editor_state);

unsigned int ProjectModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path);

unsigned int ProjectModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_Name);

void GetProjectModuleBuildLogPath(const EditorState* editor_state, Stream<wchar_t>& log_path);

void GetProjectModuleThreadBuildLogPath(const EditorState* editor_state, Stream<wchar_t>& log_path, unsigned int thread_index);

void GetProjectModulePath(const EditorState* editor_state, CapacityStream<wchar_t>& module_folder_path);

size_t GetProjectModuleLastWrite(Stream<wchar_t> solution_path);

size_t GetProjectModuleLastWrite(const EditorState* editor_state, unsigned int index);

unsigned char* GetProjectModuleConfigurationPtr(EditorState* editor_state, unsigned int index);

unsigned char GetProjectModuleConfigurationChar(const EditorState* editor_state, unsigned int index);

enum class GetModuleFunctionReturnCode : unsigned char {
	OK,
	FaultyModulePath,
	ModuleFunctionMissing
};

struct GetModuleFunctionData {
	GetModuleFunctionReturnCode code;
	ModuleFunction function;
	HMODULE os_module_handle;
};

// The caller must use FreeLibrary on the os_module_handle in order to free the library only if
// the call succeded
GetModuleFunctionData GetModuleFunction(const EditorState* editor_state, containers::Stream<wchar_t> library_name);

// The caller must use FreeLibrary on the os_module_handle in order to free the library only if
// the call succeded
GetModuleFunctionData GetModuleFunction(const EditorState* editor_state, unsigned int index);

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name);

bool HasModuleFunction(const EditorState* editor_state, unsigned int index);

bool ProjectModulesNeedsBuild(const EditorState* editor_state, unsigned int index);

// Returns true if the loading succeded and the necessary function is found
bool GetModuleTasks(EditorState* editor_state, containers::Stream<wchar_t> library_name, containers::Stream<TaskGraphElement>& tasks);

// Returns true if the loading succeded and the necessary function is found
bool GetModuleTasks(EditorState* editor_state, containers::Stream<wchar_t> library_name, containers::Stream<TaskGraphElement>& tasks, containers::CapacityStream<char>& error_message);

// Returns true if the loading succeded and the necessary function is found
bool LoadModuleTasks(EditorState* editor_state, containers::Stream<wchar_t> library_name, containers::CapacityStream<char>* error_message = nullptr);

void RemoveProjectModule(EditorState* editor_state, unsigned int index);

void RemoveProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path);

void ResetProjectModules(EditorState* editor_state);

// Returns whether or not the module has been modified
bool UpdateProjectModuleLastWrite(EditorState* editor_state, unsigned int index);

void UpdateProjectModulesLastWrite(EditorState* editor_state);

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state);

void DeleteVisualStudioLockedFiles(const EditorState* editor_state);