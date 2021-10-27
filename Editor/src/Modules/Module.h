#pragma once
#include "ModuleDefinition.h"
#include "ECSEngineModule.h"

using namespace ECSEngine;

using ProjectModules = containers::ResizableStream<EditorModule, MemoryManager>;
constexpr unsigned int GRAPHICS_MODULE_INDEX = 0;

struct EditorState;

// Returns whether or not it succeeded in adding the module. It can fail if the solution or project file doesn't exist, 
// the project already is added to the ECSEngine project or if the configuration is invalid
bool AddProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path, Stream<wchar_t> library_name, EditorModuleConfiguration configuration);

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

void ChangeProjectModuleConfiguration(EditorState* editor_state, unsigned int index, EditorModuleConfiguration new_configuration);

void InitializeModuleConfigurations(EditorState* editor_state);

unsigned int ProjectModuleIndex(const EditorState* editor_state, Stream<wchar_t> solution_path);

unsigned int ProjectModuleIndexFromName(const EditorState* editor_state, Stream<wchar_t> library_Name);

void GetProjectModuleBuildLogPath(const EditorState* editor_state, Stream<wchar_t>& log_path);

void GetProjectModuleThreadBuildLogPath(const EditorState* editor_state, Stream<wchar_t>& log_path, unsigned int thread_index);

void GetBaseModulePath(const EditorState* editor_state, CapacityStream<wchar_t>& module_folder_path);

void GetModulePath(const EditorState* editor_state, Stream<wchar_t> library_name, CapacityStream<wchar_t>& module_path);

size_t GetProjectModuleSolutionLastWrite(Stream<wchar_t> solution_path);

size_t GetProjectModuleLibraryLastWrite(const EditorState* editor_state, Stream<wchar_t> library_name);

size_t GetProjectModuleSolutionLastWrite(const EditorState* editor_state, unsigned int index);

size_t GetProjectModuleLibraryLastWrite(const EditorState* editor_state, unsigned int index);

unsigned char* GetModuleConfigurationPtr(EditorState* editor_state, unsigned int index);

unsigned char GetModuleConfigurationChar(const EditorState* editor_state, unsigned int index);

// The caller must use FreeModuleFunction in order to release the OS handle only if the call succeded
void LoadEditorModule(EditorState* editor_state, unsigned int index);

bool HasModuleFunction(const EditorState* editor_state, Stream<wchar_t> library_name);

bool HasModuleFunction(const EditorState* editor_state, unsigned int index);

bool HasGraphicsModule(const EditorState* editor_state);

bool ProjectModulesNeedsBuild(const EditorState* editor_state, unsigned int index);

// Returns true if the loading succeeded and the necessary function is found
bool LoadEditorModuleTasks(EditorState* editor_state, unsigned int index, containers::CapacityStream<char>* error_message = nullptr);

void RemoveProjectModule(EditorState* editor_state, unsigned int index);

void RemoveProjectModule(EditorState* editor_state, Stream<wchar_t> solution_path);

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