#pragma once
#include "SandboxTypes.h"
#include "SandboxAccessor.h"

struct EditorState;

// -------------------------------------------------------------------------------------------------------------

void ActivateSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

void ActivateSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

void AddSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, EDITOR_MODULE_CONFIGURATION module_configuration);

// -------------------------------------------------------------------------------------------------------------

// Returns true when all the modules that are currently used by the sandbox are compiled
// Else false
bool AreSandboxModulesCompiled(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true when all the modules that are currently used by the sandbox are loaded into the editor
// Else false
bool AreSandboxModulesLoaded(const EditorState* editor_state, unsigned int sandbox_index, bool exclude_out_of_date);

// -------------------------------------------------------------------------------------------------------------

void AggregateSandboxModuleEnabledDebugDrawTasks(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<Stream<char>>* task_names
);

// -------------------------------------------------------------------------------------------------------------

void AddSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name);

// -------------------------------------------------------------------------------------------------------------

void AddSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_module_index, Stream<char> task_name);

// -------------------------------------------------------------------------------------------------------------

// Binds the module settings to the system manager in the runtime
void BindSandboxRuntimeModuleSettings(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, Stream<wchar_t> settings_name);

// -------------------------------------------------------------------------------------------------------------

void ChangeSandboxModuleConfiguration(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION module_configuration
);

// -------------------------------------------------------------------------------------------------------------

// It will deallocate the allocator but keep the path intact.
// If no module index is specified, it will clear all the modules
void ClearSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index = -1);

// -------------------------------------------------------------------------------------------------------------

// Removes all sandboxes that are currently in use
void ClearSandboxModulesInUse(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

// This is a helper function to clear this component for the sandbox's configuration
void ClearModuleDebugDrawComponentCrashStatus(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::ComponentWithType component_type,
	bool assert_not_found
);

// -------------------------------------------------------------------------------------------------------------

// It will clear any modules that are in the destination and add all modules from the source
// This function does not add the module settings from the source
void CopySandboxModulesFromAnother(EditorState* editor_state, unsigned int destination_index, unsigned int source_index);

// -------------------------------------------------------------------------------------------------------------

// For each module (with the same index and configuration) that appears in both sandboxes, it will clear the current
// Settings from the destination and replace them with the ones from the source
void CopySandboxModuleSettingsFromAnother(EditorState* editor_state, unsigned int destination_index, unsigned int source_index);

// -------------------------------------------------------------------------------------------------------------

// It just launches the compile command. Returns true if all modules are already compiled, else false
// At the moment, you cannot wait for all the modules to be compiled and then continue (although that it is not
// a good idea on the main thread since compiling can take a long amount of time)
bool CompileSandboxModules(EditorState* editor_state, unsigned int sandbox_index);

// -------------------------------------------------------------------------------------------------------------

void DeactivateSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

void DeactivateSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

void DisableSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name);

// -------------------------------------------------------------------------------------------------------------

void EnableSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name);

// -------------------------------------------------------------------------------------------------------------

// Helper function to help identify the module from where a sandbox use the debug draw component
// Returns the module index inside the project modules array
unsigned int FindSandboxDebugDrawComponentModuleIndex(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ComponentWithType component_with_type,
	EDITOR_MODULE_CONFIGURATION* configuration
);

// -------------------------------------------------------------------------------------------------------------

void FlipSandboxModuleDebugDrawTask(EditorState* editor_state, unsigned int sandbox_index, Stream<char> task_name);

// -------------------------------------------------------------------------------------------------------------

EditorSandboxModule* GetSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

const EditorSandboxModule* GetSandboxModule(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

const EditorModuleInfo* GetSandboxModuleInfo(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

// Returns -1 if it doesn't find the module
unsigned int GetSandboxModuleInStreamIndex(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxModuleSettingsPath(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, CapacityStream<wchar_t>& path);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxModuleSettingsPathByIndex(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int in_stream_module_index,
	CapacityStream<wchar_t>& path
);

// -------------------------------------------------------------------------------------------------------------

void GetSandboxGraphicsModules(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>& indices);

// -------------------------------------------------------------------------------------------------------------

// Returns the index inside the modules stream of the sandbox of the graphics module. If it doesn't exist or
// there are multiple graphics modules it returns -1. If the fail reason is set, it will make it false if there
// are no graphics, else true since there are multiple modules
unsigned int GetSandboxGraphicsModule(const EditorState* editor_state, unsigned int sandbox_index, bool* multiple_modules = nullptr);

// -------------------------------------------------------------------------------------------------------------

// Fills in the in_stream_indices of the modules that this sandbox targets and are currently being compiled
void GetSandboxModulesCompilingInProgress(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>& in_stream_indices);

// -------------------------------------------------------------------------------------------------------------

// Fills in the in_stream_indices of the modules that are not loaded but are being referenced, can optionally include out of date modules as well
void GetSandboxNeededButMissingModules(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<unsigned int>& in_stream_indices, 
	bool include_out_of_date
);

// -------------------------------------------------------------------------------------------------------------

// It will return the sandbox version of the component functions for the given component. Returns nullptr if it doesn't find it
const ModuleComponentFunctions* GetSandboxModuleComponentFunctions(const EditorState* editor_state, unsigned int sandbox_index, Stream<char> component_name);

// It will return the sandbox version of the component functions for the given component. Returns nullptr if it doesn't find it
const ModuleComponentFunctions* GetSandboxModuleComponentFunctions(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int module_index,
	Stream<char> component_name
);

// -------------------------------------------------------------------------------------------------------------

// It will return the component functions of the best fit module
const ModuleComponentFunctions* GetModuleComponentFunctionsBestFit(const EditorState* editor_state, Stream<char> component_name);

// -------------------------------------------------------------------------------------------------------------

bool HasSandboxModuleSettings(const EditorSandboxModule* sandbox_module);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxModuleDeactivated(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

bool IsSandboxModuleDeactivatedInStream(const EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

// Returns the given configuration of the module if it is being used, else COUNT to signal that it is not being used
EDITOR_MODULE_CONFIGURATION IsModuleUsedBySandbox(const EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

// Returns true if a sandbox references the given module info. If the configuration is COUNT, then it will return true
// if any configuration is used. If the running state is set to true, then it will return true if the sandbox is actually
// running/paused. If set to false, it will also report true when the sandbox is just in the scene state.
// If the dependent modules is specified, then it will fill in all the sandboxes that depend on that module (won't stop at the first occurence)
bool IsModuleInfoUsed(
	const EditorState* editor_state,
	unsigned int module_index,
	bool running_state,
	EDITOR_MODULE_CONFIGURATION configuration = EDITOR_MODULE_CONFIGURATION_COUNT,
	ECSEngine::CapacityStream<unsigned int>* dependent_sandboxes = nullptr
);

// -------------------------------------------------------------------------------------------------------------

// It will create the ReflectedSettings for that module and load the data from the file if a path is specified.
// If no path is specified, it will use the default values.
// It returns true if it succeeded, else false. (it can fail only if it cannot read the data from the file)
bool LoadSandboxModuleSettings(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int module_index
);

// -------------------------------------------------------------------------------------------------------------

void RemoveSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

// The last argument must be the index of the module inside the modules stream to be removed
void RemoveSandboxModuleInStream(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_stream_index);

// -------------------------------------------------------------------------------------------------------------

// All sandboxes will remove the given module from being used and deallocate any entities data stored
// that was coming from that module
void RemoveSandboxModuleForced(EditorState* editor_state, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

// It returns true if it succeeded, else false. (it can fail only if it cannot read the data from the file)
// If it fails, it will set the default values
bool ReloadSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

// This is a helper function to clear this component for the sandbox's configuration
void SetModuleDebugDrawComponentCrashStatus(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ComponentWithType component_type,
	bool assert_not_found
);

// -------------------------------------------------------------------------------------------------------------

void UpdateSandboxModuleEnabledDebugDrawTasks(EditorState* editor_state, unsigned int sandbox_index, const TaskManager* task_manager);

// -------------------------------------------------------------------------------------------------------------

// It will update the entity manager component functions based upon the loaded module functions
void UpdateSandboxComponentFunctionsForModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

// -------------------------------------------------------------------------------------------------------------

// Updates all sandboxes which reference this particular configuration to update their component functions
void UpdateSandboxesComponentFunctionsForModule(EditorState* editor_state, unsigned int module_index, EDITOR_MODULE_CONFIGURATION configuration);

// -------------------------------------------------------------------------------------------------------------

// Determines which settings have been changed in order to refresh the data
void TickModuleSettingsRefresh(EditorState* editor_state);

// -------------------------------------------------------------------------------------------------------------