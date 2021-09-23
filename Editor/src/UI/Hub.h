#pragma once
#include "ECSEngineUI.h"
#include "..\Project\ProjectOperations.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

struct EditorState;

struct HubProject {
	const wchar_t* path;
	ProjectFile data;
	const char* error_message = nullptr;
};

struct HubData {
	CapacityStream<HubProject> projects;
	UIReflectionDrawer* ui_reflection;
};

void AddHubProject(EditorState* editor_state, const wchar_t* path);

void AddHubProject(EditorState* editor_state, Stream<wchar_t> path);

void DeallocateHubProjects(EditorState* editor_state);

void LoadHubProjects(EditorState* editor_state);

void ReloadHubProjects(EditorState* editor_state);

void ReloadHubProjectsAction(ActionData* action_data);

void ResetHubData(EditorState* editor_state);

void RemoveHubProject(EditorState* editor_state, const wchar_t* path);

void RemoveHubProject(EditorState* editor_state, Stream<wchar_t> path);

void SortHubProjects(EditorState* editor_state);

template<bool initialize>
void HubDraw(void* window_data, void* drawer_descriptor);

// Taking editor state as void* ptr in order to avoid including it in the header file
// and avoid recompiling when modifying the editor state
void Hub(EditorState* editor_state);