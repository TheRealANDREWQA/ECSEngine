#pragma once
#include "editorpch.h"
#include "ECSEngineUI.h"

using namespace ECSEngine;
ECS_TOOLS;

constexpr const wchar_t* PROJECT_UI_TEMPLATE_EXTENSION = L".uit";
constexpr const wchar_t* PROJECT_CURRENT_UI_TEMPLATE = L"UI\\current.uit";

struct EditorState;

struct ProjectUITemplate {
	CapacityStream<wchar_t> ui_file;
};

struct LoadProjectUITemplateData {
	EditorState* editor_state;
	ProjectUITemplate ui_template;
};

struct SaveProjectUITemplateData {
	ProjectUITemplate ui_template;
};

struct ProjectOperationData;

void CreateProjectDefaultUI(EditorState* editor_state);

UIDockspace* CreateProjectBackgroundDockspace(UISystem* system);

bool OpenProjectUI(ProjectOperationData data);

void OpenProjectUIAction(ActionData* action_data);

bool SaveCurrentProjectUI(EditorState* editor_state);

// Data is editor_state
void SaveCurrentProjectUIAction(ActionData* action_data);

bool LoadProjectUITemplate(EditorState* editor_state, ProjectUITemplate _template, CapacityStream<char>& error_message);

void LoadProjectUITemplateAction(ActionData* action_data);

bool SaveProjectUITemplate(UISystem* system, ProjectUITemplate _template, CapacityStream<char>& error_message);

void SaveProjectUITemplateAction(ActionData* action_data);