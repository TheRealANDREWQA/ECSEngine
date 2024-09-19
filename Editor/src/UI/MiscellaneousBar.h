#pragma once
#include "ECSEngineUI.h"

#define MISCELLANEOUS_BAR_SIZE_Y (ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_Y * 2.0f)
#define MISCELLANEOUS_BAR_WINDOW_NAME "MiscellaneousBar"

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
void MiscellaneousBarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory);

void MiscellaneousBarDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

void CreateMiscellaneousBar(EditorState* editor_state);