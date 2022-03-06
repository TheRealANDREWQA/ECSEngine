#pragma once
#include "ECSEngineUI.h"

constexpr float MISCELLANEOUS_BAR_SIZE_Y = ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_Y * 2.0f;
constexpr const char* MISCELLANEOUS_BAR_WINDOW_NAME = "MiscellaneousBar";

struct EditorState;

// Stack memory size should be at least 512
void MiscellaneousBarSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void MiscellaneousBarDraw(void* window_data, void* drawer_descriptor, bool initialize);

void CreateMiscellaneousBar(EditorState* editor_state);