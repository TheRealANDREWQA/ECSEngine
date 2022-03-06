#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
;
ECS_TOOLS;

constexpr const char* INSPECTOR_WINDOW_NAME = "Inspector";

struct EditorState;

void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void InspectorWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

unsigned int CreateInspectorWindow(EditorState* editor_state);

void CreateInspector(EditorState* editor_state);

void CreateInspectorAction(ActionData* action_data);

void ChangeInspectorToNothing(EditorState* editor_state);

void ChangeInspectorToFolder(EditorState* editor_state, Stream<wchar_t> path);

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path);

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index);

void ChangeInspectorToGraphicsModule(EditorState* editor_state);

void ChangeInspectorToModuleConfigurationGroup(EditorState* editor_state, unsigned int index);

void LockInspector(EditorState* editor_state);

void UnlockInspector(EditorState* editor_state);