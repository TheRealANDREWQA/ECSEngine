#pragma once
#include "ModuleDefinition.h"
#include "ECSEngineModule.h"

using namespace ECSEngine;

constexpr unsigned int GRAPHICS_MODULE_INDEX = 0;

struct EditorState;

enum EDITOR_LAUNCH_BUILD_COMMAND_STATUS {
	EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING,
	EDITOR_LAUNCH_BUILD_COMMAND_ALREADY_RUNNING,
	EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING,
	EDITOR_LAUNCH_BUILD_COMMAND_SKIPPED,
	EDITOR_LAUNCH_BUILD_COMMAND_COUNT
};

// Returns whether or not it succeeded in adding the module. It can fail if the solution or project file doesn't exist, 
// the project already is added to the ECSEngine project or if the configuration is invalid
bool AddProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, EditorModuleConfiguration configuration);

// Runs on multiple threads
// It will report the status, in order, for each module - the launch status and optionally the build status.
// If the build status is used, this function will block the thread - it will wait until all the modules are compiled
void BuildProjectModules(EditorState* editor_state, EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses, unsigned char* build_statuses = nullptr);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command or if it is up to date
EDITOR_LAUNCH_BUILD_COMMAND_STATUS BuildProjectModule(EditorState* editor_state, unsigned int index, unsigned char* build_status = nullptr);

// Returns true if the projects were built and the modules could be successfully loaded
bool BuildProjectModulesAndLoad(EditorState* editor_state);

// Runs on multiple threads
// It will report the status, in order, for each module
void CleanProjectModules(EditorState* editor_state, EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses, unsigned char* build_statuses = nullptr);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS CleanProjectModule(EditorState* editor_state, unsigned int index, unsigned char* build_status = nullptr);

// Runs on multiple threads
// It will report the status, in order, for each module
void RebuildProjectModules(EditorState* editor_state, EDITOR_LAUNCH_BUILD_COMMAND_STATUS* launch_statuses, unsigned char* build_statuses = nullptr);

// Editor state is needed in order to print to console
// Returns whether or not the command will actually will execute. It can be skipped 
// if the module is in flight running on the same configuration a build command
EDITOR_LAUNCH_BUILD_COMMAND_STATUS RebuildProjectModule(EditorState* editor_state, unsigned int index, unsigned char* build_status = nullptr);

void DeleteProjectModuleFlagFiles(EditorState* editor_state);

void ChangeProjectModuleConfiguration(EditorState* editor_state, unsigned int index, EditorModuleConfiguration new_configuration);

void InitializeModuleConfigurations(EditorState* editor_state);

bool IsEditorModuleLoaded(const EditorState* editor_state, unsigned int index);

unsigned int ProjectModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path);

unsigned int ProjectModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_Name);

void GetProjectModuleBuildFlagFile(const EditorState* editor_state, unsigned int module_index, Stream<wchar_t> command, CapacityStream<wchar_t>& temp_file);

void GetProjectModuleBuildLogPath(const EditorState* editor_state, unsigned int module_index, Stream<wchar_t> command, Stream<wchar_t>& log_path);

void GetModulesFolder(const EditorState* editor_state, CapacityStream<wchar_t>& module_folder_path);

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, EditorModuleConfiguration configuration, CapacityStream<wchar_t>& module_path);

size_t GetProjectModuleSolutionLastWrite(Stream<wchar_t> solution_path);

size_t GetProjectModuleLibraryLastWrite(const EditorState* editor_state, Stream<wchar_t> library_name, EditorModuleConfiguration configuration);

size_t GetProjectModuleSolutionLastWrite(const EditorState* editor_state, unsigned int index);

size_t GetProjectModuleLibraryLastWrite(const EditorState* editor_state, unsigned int index);

unsigned char* GetModuleConfigurationPtr(EditorState* editor_state, unsigned int index);

unsigned char GetModuleConfigurationChar(const EditorState* editor_state, unsigned int index);

// Returns whether or not it succeded in finding a corresponding source file - it will search for 
// "src", "Src", "source", "Source"
bool GetModuleReflectSolutionPath(const EditorState* editor_state, unsigned int index, CapacityStream<wchar_t>& path);

// Returns -1 if it fails
unsigned int GetModuleReflectionHierarchyIndex(const EditorState* editor_state, unsigned int module_index);

// This function can also fail if the temporary dll could not be created
bool LoadEditorModule(EditorState* editor_state, unsigned int index);

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name, EditorModuleConfiguration configuration);

bool HasModuleFunction(const EditorState* editor_state, unsigned int index);

bool HasGraphicsModule(const EditorState* editor_state);

bool ProjectModulesNeedsBuild(const EditorState* editor_state, unsigned int index);

// Returns true if the loading succeeded and the necessary function is found
bool LoadEditorModuleTasks(EditorState* editor_state, unsigned int index, CapacityStream<char>* error_message = nullptr);

void ReleaseProjectModuleTasks(EditorState* editor_state, unsigned int index);

void RemoveProjectModule(EditorState* editor_state, unsigned int index);

void RemoveProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path);

void RemoveProjectModuleAssociatedFiles(EditorState* editor_state, unsigned int index);

void RemoveProjectModuleAssociatedFiles(EditorState* editor_state, Stream<wchar_t> solution_path);

void ResetProjectModules(EditorState* editor_state);

void ResetProjectGraphicsModule(EditorState* editor_state);

void SetModuleLoadStatus(EditorModule* module, bool has_functions);

// Returns whether or not it succeeded in adding the module. It can fail if the solution or project file doesn't exist, 
// the project already is added to the ECSEngine project or if the configuration is invalid
bool SetProjectGraphicsModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, EditorModuleConfiguration configuration);

// Returns whether or not the module has been modified; it updates both the solution and
// the library last write times
bool UpdateProjectModuleLastWrite(EditorState* editor_state, unsigned int index);

// It updates both the solution and the library last write times
void UpdateProjectModulesLastWrite(EditorState* editor_state);

bool UpdateProjectModuleSolutionLastWrite(EditorState* editor_state, unsigned int index);

bool UpdateProjectModuleLibraryLastWrite(EditorState* editor_state, unsigned int index);

size_t GetVisualStudioLockedFilesSize(const EditorState* editor_state);

void DeleteVisualStudioLockedFiles(const EditorState* editor_state);