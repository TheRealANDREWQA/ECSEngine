#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
ECS_CONTAINERS;
ECS_TOOLS;

constexpr const char* INSPECTOR_WINDOW_NAME = "Inspector";

struct EditorState;

void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

template<bool initialize>
void InspectorWindowDraw(void* window_data, void* drawer_descriptor);

unsigned int CreateInspectorWindow(EditorState* editor_state);

void CreateInspector(EditorState* editor_state);

void CreateInspectorAction(ActionData* action_data);

void ChangeInspectorToFolder(EditorState* editor_state, Stream<wchar_t> path);

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path);

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index);