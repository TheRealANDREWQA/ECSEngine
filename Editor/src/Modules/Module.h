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

void GetModuleStem(Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

void GetModuleFilename(Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

void GetModuleFilenameNoConfig(Stream<wchar_t> library_name, CapacityStream<wchar_t>& module_path);

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

// This will go through the .vcxproj file to read the imports
void GetModuleDLLImports(EditorState* editor_state, unsigned int index);

// It will fill in all the indices of the modules that are being used by this module
void GetModuleDLLImports(const EditorState* editor_state, unsigned int index, CapacityStream<unsigned int>& dll_imports);

void GetModuleDLLExternalReferences(
	const EditorState* editor_state, 
	unsigned int index,  
	CapacityStream<unsigned int>& external_references
);

size_t GetModuleSolutionLastWrite(Stream<wchar_t> solution_path);

size_t GetModuleLibraryLastWrite(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration);

size_t GetModuleSolutionLastWrite(const EditorState* editor_state, unsigned int index);

size_t GetModuleLibraryLastWrite(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

// Returns whether or not it succeded in finding a corresponding source file - it will search for 
// "src", "Src", "source", "Source"
bool GetModuleReflectSolutionPath(const EditorState* editor_state, unsigned int index, CapacityStream<wchar_t>& path);

// Returns -1 if it fails
unsigned int GetModuleReflectionHierarchyIndex(const EditorState* editor_state, unsigned int module_index);

// Returns the most suitable configuration. The rules are like this. Find the first GOOD configuration (from distribution to debug).
// If found return it. If all are not loaded or out of date, then return EDITOR_MODULE_CONFIGURATION_COUNT
EDITOR_MODULE_CONFIGURATION GetModuleLoadedConfiguration(const EditorState* editor_state, unsigned int module_index);

// Returns a structures with both functions nullptr in case there is no such component. (Fine for components
// that only expose handles to assets). If there is no dll loaded, then it will simply return
ModuleLinkComponentTarget GetModuleLinkComponentTarget(const EditorState* editor_state, unsigned int module_index, Stream<char> name);

// Fills in the indices of the modules that the types from the given module index depend upon
void GetModuleTypesDependencies(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& dependencies);

void GetModulesTypesDependentUpon(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& dependencies);

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration);

bool HasModuleFunction(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

void ModulesToAppliedModules(const EditorState* editor_state, CapacityStream<const AppliedModule*>& applied_modules);

// This function can also fail if the temporary dll could not be created
bool LoadEditorModule(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

// It will reflect the module - it will create the UI types aswell
void ReflectModule(EditorState* editor_state, unsigned int index);

// Does not check to see if the module is being used by a sandbox
// Deallocates the streams and releases the OS library handle
// If the configuration is defaulted, it will release all the configurations
void ReleaseModuleStreamsAndHandle(
	EditorState* editor_state, 
	unsigned int index, 
	EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT
);

// Does not check to see if the module is being used by a sandbox
// Deallocates the streams and releases the OS library handle
// In addition, it will free the memory used by the path, the associated files will be also deleted and the
// reflection types. All the configurations will be released
void ReleaseModule(EditorState* editor_state, unsigned int index);

// Removes all the reflection types - not instances
void ReleaseModuleReflection(EditorState* editor_state, unsigned int index);

// Does not check to see that a sandbox depends on this module
void RemoveModule(EditorState* editor_state, unsigned int index);

// Does not check to see that a sandbox depends on this module
void RemoveModule(EditorState* editor_state, Stream<wchar_t> solution_path);

void RemoveModuleAssociatedFiles(EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

void RemoveModuleAssociatedFiles(EditorState* editor_state, Stream<wchar_t> solution_path, EDITOR_MODULE_CONFIGURATION configuration);

// Releases all the modules and basically no module will be kept
void ResetModules(EditorState* editor_state);

void SetModuleLoadStatus(EditorState* editor_state, unsigned int module_index, bool is_failed, EDITOR_MODULE_CONFIGURATION configuration);

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

// Updates the DLL imports of all modules
void UpdateModulesDLLImports(EditorState* editor_state);

void TickUpdateModulesDLLImports(EditorState* editor_state);

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state);

void DeleteVisualStudioLockedFiles(const EditorState* editor_state);