#pragma once
#include "ECSEngineUI.h"
#include "..\Project\ProjectOperations.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

struct EditorState;

void AddHubProject(EditorState* editor_state, Stream<wchar_t> path);

void DeallocateHubProject(EditorState* editor_state, size_t project_hub_index);

void DeallocateHubProjects(EditorState* editor_state);

void LoadHubProjects(EditorState* editor_state);

void ReloadHubProjects(EditorState* editor_state);

void ReloadHubProjectsAction(ActionData* action_data);

void ResetHubData(EditorState* editor_state);

void RemoveHubProject(EditorState* editor_state, Stream<wchar_t> path);

void SortHubProjects(EditorState* editor_state);

void HubTick(EditorState* editor_state);

void HubDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

void Hub(EditorState* editor_state);