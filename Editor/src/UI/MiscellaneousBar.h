#pragma once
#include "ECSEngineUI.h"

constexpr float MISCELLANEOUS_BAR_SIZE_Y = ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_Y * 2.0f;
constexpr const char* MISCELLANEOUS_BAR_WINDOW_NAME = "MiscellaneousBar";
constexpr size_t MISC_BAR_IS_PLAYING = 1 << 0;
constexpr size_t MISC_BAR_CAN_STEP = 1 << 1;

struct MiscellaneousBarData {
	size_t state;
};

struct EditorState;

// Stack memory size should be at least 512
void MiscellaneousBarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

template<bool initialize>
void MiscellaneousBarDraw(void* window_data, void* drawer_descriptor);

void CreateMiscellaneousBar(EditorState* editor_state);