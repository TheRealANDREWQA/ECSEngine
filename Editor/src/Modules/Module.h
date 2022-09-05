#pragma once
#include "ModuleDefinition.h"
#include "ECSEngineModule.h"

using namespace ECSEngine;

extern const wchar_t* ECS_RUNTIME_PDB_PATHS[];


struct EditorState;

enum EDITOR_LAUNCH_BUILD_COMMAND_STATUS : unsigned char {
	EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING,
	EDITOR_LAUNCH_BUILD_COMMAND_ALREADY_RUNNING,
	EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING,
	EDITOR_LAUNCH_BUILD_COMMAND_SKIPPED,
	EDITOR_LAUNCH_BUILD_COMMAND_COUNT
};

enum EDITOR_FINISH_BUILD_COMMAND_STATUS : unsigned char {
	EDITOR_FINISH_BUILD_COMMAND_WAITING,
	EDITOR_FINISH_BUILD_COMMAND_OK,
	EDITOR_FINISH_BUILD_COMMAND_FAILED
};

// Returns whether or not it succeeded in adding the module. It can fail if the solution or project file doesn't exist, 
// the project already is added to the ECSEngine project
bool AddModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, bool is_graphics_module = false);

// Runs on multiple threads
// It will report the status, in order, for each module - the launch status and optionally the build status.
// If the build status is used, this function will block the thread - it will wait until all the modules are compiled
void BuildModules(
	EditorState* editor_state,
	Stream<unsigned int> module_indices,
	EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_statuses = nullptr
);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command or if it is up to date
// The build status can be used to query
EDITOR_LAUNCH_BUILD_COMMAND_STATUS BuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_status = nullptr
);

// Returns true if the projects were built and the modules could be successfully loaded
bool BuildModulesAndLoad(
	EditorState* editor_state,
	Stream<unsigned int> module_indices,
	EDITOR_MODULE_CONFIGURATION* configurations
);

// Runs on multiple threads
// It will report the status, in order, for each module
void CleanModules(
	EditorState* editor_state,
	Stream<unsigned int> module_indices,
	EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_statuses = nullptr
);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS CleanModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_status = nullptr
);

// Runs on multiple threads
// It will report the status, in order, for each module
void RebuildModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_statuses = nullptr
);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS RebuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_FINISH_BUILD_COMMAND_STATUS* build_status = nullptr
);

void DeleteModuleFlagFiles(EditorState* editor_state);

bool IsEditorModuleLoaded(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

bool IsGraphicsModule(const EditorState* editor_state, unsigned int index);

unsigned int GetModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path);

unsigned int GetModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_Name);

void GetModuleBuildFlagFile(
	const EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	Stream<wchar_t> command,
	CapacityStream<wchar_t>& temp_file
);

void GetModuleBuildLogPath(
	const EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	Stream<wchar_t> command,
	Stream<wchar_t>& log_path
);

EditorModuleInfo* GetModuleInfo(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

void GetModulesFolder(const EditorState* editor_state, CapacityStream<wchar_t>& module_folder_path);

void GetModuleFolder(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

size_t GetModuleSolutionLastWrite(Stream<wchar_t> solution_path);

size_t GetModuleLibraryLastWrite(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration);

size_t GetModuleSolutionLastWrite(const EditorState* editor_state, unsigned int index);

size_t GetModuleLibraryLastWrite(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

// Returns whether or not it succeded in finding a corresponding source file - it will search for 
// "src", "Src", "source", "Source"
bool GetModuleReflectSolutionPath(const EditorState* editor_state, unsigned int index, CapacityStream<wchar_t>& path);

// Returns -1 if it fails
unsigned int GetModuleReflectionHierarchyIndex(const EditorState* editor_state, unsigned int module_index);

// The indices inside the normal reflection, not UI reflection
void GetModuleReflectionComponentIndices(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>* indices);

// The indices inside the normal reflection, not UI reflection
void GetModuleReflectionSharedComponentIndices(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>* indices);

// The indices inside the normal reflection, not UI reflection
void GetModuleReflectionAllComponentIndices(
	const EditorState* editor_state,
	unsigned int module_index,
	CapacityStream<unsigned int>* unique_indices,
	CapacityStream<unsigned int>* shared_indices
);

// Returns the most suitable configuration. The rules are like this. Find the first GOOD configuration (from distribution to debug).
// If found return it. If none is found, find the first OUT_OF_DATE configuration (also from distribution to debug). If found, return it.
// If all are not loaded, then return EDITOR_MODULE_CONFIGURATION_COUNT
EDITOR_MODULE_CONFIGURATION GetModuleLoadedConfiguration(const EditorState* editor_state, unsigned int module_index);

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration);

bool HasModuleFunction(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

// This function can also fail if the temporary dll could not be created
bool LoadEditorModule(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

// It will reflect the module - it will create the UI types aswell
void ReflectModule(EditorState* editor_state, unsigned int index);

// Deallocates the streams and releases the OS library handle
// If the configuration is defaulted, it will release all the configurations
void ReleaseModuleStreamsAndHandle(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT);

// Deallocates the streams and releases the OS library handle
// In addition, it will free the memory used by the path, the associated files will be also deleted and the
// reflection types. All the configurations will be released
void ReleaseModule(EditorState* editor_state, unsigned int index);

// Removes all the reflection types - not instances
void ReleaseModuleReflection(EditorState* editor_state, unsigned int index);

void RemoveModule(EditorState* editor_state, unsigned int index);

void RemoveModule(EditorState* editor_state, Stream<wchar_t> solution_path);

void RemoveModuleAssociatedFiles(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

void RemoveModuleAssociatedFiles(EditorState* editor_state, Stream<wchar_t> solution_path, EDITOR_MODULE_CONFIGURATION configuration);

// Releases all the modules and basically no module will be kept
void ResetModules(EditorState* editor_state);

void SetModuleLoadStatus(EditorModule* module, bool has_functions, EDITOR_MODULE_CONFIGURATION configuration);

// Returns whether or not the module has been modified; it updates both the solution and the library last write times
// If the configuration is defaulted, it will update all configurations for the library. In this case, it will return true
// if any of the configurations was updated
bool UpdateModuleLastWrite(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT);

// It updates both the solution and the library last write times
void UpdateModulesLastWrite(EditorState* editor_state);

bool UpdateModuleSolutionLastWrite(EditorState* editor_state, unsigned int index);

// If the configuration is defaulted, it will update all the configurations for the library. In this case, it will return true
// if any of the configurations was updated
bool UpdateModuleLibraryLastWrite(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT);

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state);

void DeleteVisualStudioLockedFiles(const EditorState* editor_state);