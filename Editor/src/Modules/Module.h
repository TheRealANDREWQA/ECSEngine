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
	EDITOR_LAUNCH_BUILD_COMMAND_LOCKED,
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
void BuildModules(
	EditorState* editor_state,
	Stream<unsigned int> module_indices,
	const EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_statuses = nullptr,
	bool disable_logging = false
);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command or if it is up to date
// The build status can be used to query. If the force build flag is set to true, it will build
// the module even when the load status is GOOD
EDITOR_LAUNCH_BUILD_COMMAND_STATUS BuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_status = nullptr,
	bool disable_logging = false,
	bool force_build = false
);

// Runs on multiple threads
// It will report the status, in order, for each module
void CleanModules(
	EditorState* editor_state,
	Stream<unsigned int> module_indices,
	const EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_statuses = nullptr,
	bool disable_logging = false
);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS CleanModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_status = nullptr,
	bool disable_logging = false
);

// Runs on multiple threads
// It will report the status, in order, for each module
void RebuildModules(
	EditorState* editor_state,
	Stream<unsigned int> indices,
	const EDITOR_MODULE_CONFIGURATION* configurations,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_statuses = nullptr,
	bool disable_logging = false
);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS RebuildModule(
	EditorState* editor_state,
	unsigned int index,
	EDITOR_MODULE_CONFIGURATION configuration,
	std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>* build_status = nullptr,
	bool disable_logging = false
);

void PrintConsoleMessageForBuildCommand(
	EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS command_status
);

void ClearModuleDebugDrawComponentCrashStatus(
	EditorState* editor_state, 
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration, 
	ECSEngine::ComponentWithType component_type, 
	bool assert_not_found
);

void ClearModuleAllDebugDrawComponentCrashStatus(
	EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration
);

void ClearModuleAllBuildEntriesCrashStatus(
	EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration
);

void ClearModuleAllCrashStatuses(
	EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration
);

void DecrementModuleInfoLockCount(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration);

void DeleteModuleFlagFiles(EditorState* editor_state);

bool ExistsModuleDebugDrawElementIn(
	const EditorState* editor_state,
	Stream<ModuleComponentFunctions> component_functions,
	ComponentWithType component_type
);

// Fills in the modules and their respective configurations that have this debug draw component loaded
void FindModuleDebugDrawComponentIndex(
	const EditorState* editor_state, 
	ComponentWithType component_type, 
	CapacityStream<unsigned int>* module_indices,
	CapacityStream<EDITOR_MODULE_CONFIGURATION>* configurations
);

ModuleDebugDrawElement* FindModuleDebugDrawComponentPtr(
	EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	ComponentWithType component_type
);

const ModuleDebugDrawElement* FindModuleDebugDrawComponentPtr(
	const EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	ComponentWithType component_type
);

bool IsEditorModuleLoaded(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

bool IsGraphicsModule(const EditorState* editor_state, unsigned int index);

// The last argument tells the function whether or not the module compilation lock should be acquired or not
// By default, it will acquire it, but if the caller already has done that, it needs to set the last argument to false
// In order for the function to work
bool IsModuleBeingCompiled(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration, bool acquire_lock = true);

// Returns true if any of the given modules in currently being compiled
// The last argument tells the function whether or not the module compilation lock should be acquired or not
// By default, it will acquire it, but if the caller already has done that, it needs to set the last argument to false
// In order for the function to work
bool IsAnyModuleBeingCompiled(EditorState* editor_state, Stream<unsigned int> module_indices, const EDITOR_MODULE_CONFIGURATION* configurations, bool acquire_lock = true);

// Returns true if the given configuration of the module is being used in at least one sandbox, else false
// If the configuration is left to COUNT, then it will look for all configurations
bool IsModuleUsedBySandboxes(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT);

// Modifies the list of indices and configurations such that only the modules which are currently being compiled are kept
void GetCompilingModules(EditorState* editor_state, CapacityStream<unsigned int>& module_indices, EDITOR_MODULE_CONFIGURATION* configurations);

// Fills in all the sandboxes that reference the given module configuration
// Returns true if at least a sandbox reference was added
bool GetSandboxesForModule(
	const EditorState* editor_state, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	CapacityStream<unsigned int>* sandboxes
);

unsigned int GetModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path);

unsigned int GetModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_name);

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

EDITOR_MODULE_LOAD_STATUS GetModuleLoadStatus(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

// If the configuration is COUNT, it will use the most suitable loaded module
// If you want to know the actual configuration that was matched, you can fill in
// The last parameter
// Returns nullptr if there is no such function
ModuleComponentBuildEntry* GetModuleComponentBuildEntry(
	const EditorState* editor_state, 
	unsigned int index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	Stream<char> component_name,
	EDITOR_MODULE_CONFIGURATION* matched_configuration = nullptr
);

struct EditorModuleComponentBuildEntry {
	ModuleComponentBuildEntry* entry;
	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION module_configuration;
};

// Convenience function - it will search through all modules and return the first found.
// Returns nullptr if there is no such function
EditorModuleComponentBuildEntry GetModuleComponentBuildEntry(
	const EditorState* editor_state,
	Stream<char> component_name
);

void GetModuleStem(Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

void GetModuleFilename(Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

void GetModuleFilenameNoConfig(Stream<wchar_t> library_name, CapacityStream<wchar_t>& module_path);

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration, CapacityStream<wchar_t>& module_path);

Stream<wchar_t> GetModuleLibraryName(const EditorState* editor_state, unsigned int module_index);

// This will go through the .vcxproj file to read the imports and through
// The source files to look for includes
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
// that only expose handles to assets). If there is no dll loaded, then it will simply return nullptrs
ModuleLinkComponentTarget GetModuleLinkComponentTarget(const EditorState* editor_state, unsigned int module_index, Stream<char> name);

// Returns nullptrs if it doesn't exist
ModuleLinkComponentTarget GetEngineLinkComponentTarget(const EditorState* editor_state, Stream<char> name);

// Searches firstly in the engine side, then in the module side.
// Returns nullptrs if it doesn't exist
ModuleLinkComponentTarget GetModuleLinkComponentTarget(const EditorState* editor_state, Stream<char> name);

// Fills in the indices of the modules that the types from the given module index depend upon
void GetModuleTypesDependencies(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& dependencies);

void GetModulesTypesDependentUpon(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& dependencies);

// Returns { nullptr, 0 } if there is no entry that matches or if there is no function defined
Stream<void> GetModuleExtraInformation(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration, Stream<char> key);

ModuleExtraInformation GetModuleExtraInformation(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration);

void GetModuleDebugDrawComponents(
	const EditorState* editor_state, 
	unsigned int module_index, 
	EDITOR_MODULE_CONFIGURATION configuration, 
	AdditionStream<ComponentWithType> components,
	bool exclude_crashed
);

// Returns -1 if there is no module matched. If the configuration is COUNT, it will choose
// The most "optimal" configuration for each module
unsigned int GetDebugDrawTasksBelongingModule(
	const EditorState* editor_state,
	Stream<char> task_name,
	EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT
);

// The debug_elements must have components.size entries in it
// In case a component is not matched, it will leave it as it is
void GetModuleMatchedDebugDrawComponents(
	const EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	Stream<ComponentWithType> components,
	ModuleDebugDrawElementTyped* debug_elements
);

bool IsModuleInfoLocked(const EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration);

// Returns true if any module info is locked, else false
bool IsAnyModuleInfoLocked(const EditorState* editor_state);

bool IsModuleDebugDrawComponentCrashed(
	const EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	Component component,
	ECS_COMPONENT_TYPE component_type
);

void IncrementModuleInfoLockCount(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration);

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name, EDITOR_MODULE_CONFIGURATION configuration);

bool HasModuleFunction(const EditorState* editor_state, unsigned int index, EDITOR_MODULE_CONFIGURATION configuration);

void ModulesToAppliedModules(const EditorState* editor_state, CapacityStream<const AppliedModule*>& applied_modules);

void ModuleMatchDebugDrawElements(
	const EditorState* editor_state,
	Stream<ComponentWithType> components, 
	Stream<ModuleComponentFunctions> component_functions, 
	ModuleDebugDrawElementTyped* output_elements
);

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

// The strings will reference the entries from the module data
void RetrieveModuleComponentBuildDependentEntries(const EditorState* editor_state, Stream<char> component_name, CapacityStream<Stream<char>>* dependent_components);

// The strings will reference the entries from the module data
// To deallocate, just deallocate the returned buffer
Stream<Stream<char>> RetrieveModuleComponentBuildDependentEntries(
	const EditorState* editor_state,
	Stream<char> component_name,
	AllocatorPolymorphic allocator
);

void SetModuleDebugDrawComponentCrashStatus(
	EditorState* editor_state,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	ECSEngine::ComponentWithType component_type,
	bool assert_not_found
);

void SetModuleLoadStatus(EditorState* editor_state, unsigned int module_index, bool is_failed, EDITOR_MODULE_CONFIGURATION configuration);

// This should be called once when the project is loaded since all the modules and their .pdbs
// Are located in the same location
void SetCrashHandlerPDBPaths(const EditorState* editor_state);

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

// Given a list of module indices and their respective configurations, it will wait for all of these to finish their compilation
// (If they are not being compiled or it has finished, it won't wait)
void WaitModulesCompilation(EditorState* editor_state, Stream<unsigned int> module_indices, const EDITOR_MODULE_CONFIGURATION* configurations);

void TickUpdateModulesDLLImports(EditorState* editor_state);

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state);

void DeleteVisualStudioLockedFiles(const EditorState* editor_state);