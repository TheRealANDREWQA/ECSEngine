// ECS_REFLECT
#pragma once
#include "ECSEngineInternal.h"
#include "ECSEngineUI.h"
#include "../OSFunctions.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr const wchar_t* PROJECT_ASSETS_RELATIVE_PATH = L"Assets";
constexpr const char* PROJECT_ASSETS_RELATIVE_PATH_ASCII = "Assets";
constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH = L"Modules";
constexpr const char* PROJECT_MODULES_RELATIVE_PATH_ASCII = "Modules";

constexpr const wchar_t PROJECT_EXTENSION[] = L".ecsproj";
constexpr const wchar_t* PROJECT_DIRECTORIES[] = {
	L"Debug",
	L"Assets",
	L"UI",
	L"Modules"
};

struct ECS_REFLECT ProjectFile {
	char version_description[32];
	char metadata[32];
	char platform_description[32];
	size_t version;
	size_t platform;
	WorldDescriptor ECS_REGISTER_ONLY_NAME_REFLECT(120) world_descriptor;
	CapacityStream<wchar_t> ECS_STREAM_REFLECT(2) project_name;
	CapacityStream<wchar_t> ECS_STREAM_REFLECT(2) source_dll_name;
	CapacityStream<wchar_t> ECS_STREAM_REFLECT(2) path;
};

struct EditorState;

struct ProjectOperationData {
	containers::CapacityStream<char> error_message;
	EditorState* editor_state;
	ProjectFile* file_data;
};

struct ProjectEntityData {

};

struct CreateProjectWizardData {
	ProjectOperationData project_data;
	bool copy_project_name_to_source_dll = true;
};

void CreateProject(ProjectOperationData* data);

void CreateProjectAction(ActionData* action_data);

void CreateProjectWizardDestroyWindowAction(ActionData* action_data);

void CreateProjectWizardAction(ActionData* action_data);

template<bool initialize>
void CreateProjectWizardDraw(void* window_data, void* drawer_descriptor);

void CreateProjectWizard(UISystem* system, CreateProjectWizardData* wizard_data);

bool CheckProjectDirectoryIntegrity(const ProjectFile* project);

bool CheckProjectIntegrity(const ProjectFile* project);

void DeallocateCurrentProject(EditorState* editor_state);

bool ExistsProjectInFolder(const ProjectFile* project_file);

// It will make the concatenation between path and project name
void GetProjectFilePath(wchar_t* characters, const ProjectFile* project_file, size_t max_character_count = 256);

// It will make the concatenation between path and project name
void GetProjectFilePath(CapacityStream<wchar_t>& characters, const ProjectFile* project_file);

void GetProjectCurrentUI(wchar_t* characters, const ProjectFile* project_file, size_t max_character_count = 256);

void GetProjectCurrentUI(CapacityStream<wchar_t>& characters, const ProjectFile* project_file);

// Error message needs to have memory allocated or nullptr to skip it
bool OpenProjectFile(ProjectOperationData data);

void OpenProjectFileAction(ActionData* action_data);

bool OpenProjectContents(ProjectOperationData data);

void OpenProjectContentsAction(ActionData* action_data);

bool OpenProject(ProjectOperationData data);

void OpenProjectAction(ActionData* action_data);

// Error message needs to have memory allocated or nullptr to skip it
bool SaveProjectFile(ProjectOperationData data);

void SaveProjectFileAction(ActionData* action_data);

bool SaveProjectUI(ProjectOperationData data);

void SaveProjectUIAction(ActionData* action_data);

bool SaveProjectContents(ProjectOperationData data);

// Internal use
bool SaveCurrentProjectConverted(EditorState* _editor_state, bool (*function)(ProjectOperationData));

void SaveProjectContentsAction(ActionData* action_data);

bool SaveProject(ProjectOperationData data);

void SaveProjectAction(ActionData* action_data);

bool SaveCurrentProject(EditorState* editor_state);

// Data is editor_state
void SaveCurrentProjectAction(ActionData* action_data);

bool SaveCurrentProjectFile(EditorState* editor_state);

// Data is editor_state
void SaveCurrentProjectFileAction(ActionData* action_data);

bool SaveCurrentProjectContents(EditorState* editor_state);

// Data is editor_state
void SaveCurrentProjectContentsAction(ActionData* action_data);

void SaveCurrentProjectWithConfirmation(EditorState* editor_state, Stream<char> description, UIActionHandler callback);

void ConsoleSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);