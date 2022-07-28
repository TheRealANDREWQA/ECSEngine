#pragma once
#include "editorpch.h"
#include "ECSEngineInternal.h"
#include "ECSEngineUI.h"
#include "ProjectFile.h"
#include "ProjectFolders.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

struct EditorState;

struct ProjectOperationData {
	CapacityStream<char> error_message;
	EditorState* editor_state;
	ProjectFile* file_data;
};

struct CreateProjectWizardData {
	ProjectOperationData project_data;
	bool copy_project_name_to_source_dll = true;
};

void CreateProject(ProjectOperationData* data);

void CreateProjectAction(ActionData* action_data);

void CreateProjectWizardAction(ActionData* action_data);

void CreateProjectWizardDraw(void* window_data, void* drawer_descriptor, bool initialize);

void CreateProjectWizard(UISystem* system, CreateProjectWizardData* wizard_data);

bool CheckProjectDirectoryIntegrity(const ProjectFile* project);

void DeallocateCurrentProject(EditorState* editor_state);

bool ExistsProjectInFolder(const ProjectFile* project_file);

void GetProjectCurrentUI(wchar_t* characters, const ProjectFile* project_file, size_t max_character_count = 256);

void GetProjectCurrentUI(CapacityStream<wchar_t>& characters, const ProjectFile* project_file);

// Error message needs to have memory allocated or nullptr to skip it
bool OpenProjectFile(ProjectOperationData data);

void OpenProjectFileAction(ActionData* action_data);

bool OpenProject(ProjectOperationData data);

void OpenProjectAction(ActionData* action_data);

// Goes through the list of directories that a project should have, and if they don't exist,
// it creates them
void RepairProjectAuxiliaryDirectories(ProjectOperationData data);

// Error message needs to have memory allocated or nullptr to skip it
bool SaveProjectFile(ProjectOperationData data);

void SaveProjectFileAction(ActionData* action_data);

bool SaveProjectUI(ProjectOperationData data);

void SaveProjectUIAction(ActionData* action_data);

// Internal use
bool SaveCurrentProjectConverted(EditorState* _editor_state, bool (*function)(ProjectOperationData));

bool SaveProject(ProjectOperationData data);

void SaveProjectAction(ActionData* action_data);

bool SaveCurrentProject(EditorState* editor_state);

// Data is editor_state
void SaveCurrentProjectAction(ActionData* action_data);

bool SaveCurrentProjectFile(EditorState* editor_state);

// Data is editor_state
void SaveCurrentProjectFileAction(ActionData* action_data);

void SaveCurrentProjectWithConfirmation(EditorState* editor_state, Stream<char> description, UIActionHandler callback);

void ConsoleSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);