#include "editorpch.h"
#include "VisualizeTexture.h"
#include "../Editor/EditorState.h"

#define MAX_WINDOWS (8)

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------

void VisualizeTextureUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int index = *(unsigned int*)stack_memory;

	CapacityStream<char>* window_name = (CapacityStream<char>*)function::OffsetPointer(stack_memory, sizeof(index));
	window_name->InitializeFromBuffer(function::OffsetPointer(window_name, sizeof(*window_name)), 0, 128);
	GetVisualizeTextureUIWindowName(index, *window_name);

	VisualizeTextureActionData create_data;
	create_data.texture.tex = nullptr;
	create_data.window_name = *window_name;
	descriptor = VisualizeTextureWindowDescriptor(editor_state->ui_system, &create_data, function::OffsetPointer(window_name, sizeof(*window_name) + window_name->capacity));
}

// ------------------------------------------------------------------------------------------------------------

unsigned int CreateVisualizeTextureUIWindow(EditorState* editor_state)
{
	unsigned int max_window_index = GetMaxVisualizeTextureUIIndex(editor_state);
	if (max_window_index < MAX_WINDOWS - 1 || max_window_index == -1) {
		ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
		GetVisualizeTextureUIWindowName(max_window_index + 1, window_name);

		VisualizeTextureActionData create_data;
		create_data.texture.tex = nullptr;
		create_data.window_name = window_name;
		CreateVisualizeTextureWindow(editor_state->ui_system, &create_data);

		return max_window_index + 1;
	}
	else {
		CreateErrorMessageWindow(editor_state->ui_system, "Visualize Texture limit was reached");
		return -1;
	}
}

// ------------------------------------------------------------------------------------------------------------

void CreateVisualizeTextureUIWindowAction(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	CreateVisualizeTextureUIWindow(editor_state);
}

// ------------------------------------------------------------------------------------------------------------

void GetVisualizeTextureUIWindowName(unsigned int index, CapacityStream<char>& name)
{
	name.Copy(VISUALIZE_TEXTURE_WINDOW_NAME);
	name.AddStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
	function::ConvertIntToChars(name, index);
}

// ------------------------------------------------------------------------------------------------------------

unsigned int GetMaxVisualizeTextureUIIndex(const EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(char, window_name, 512);

	for (int index = MAX_WINDOWS - 1; index >= 0; index--) {
		GetVisualizeTextureUIWindowName(index, window_name);
		unsigned int window_index = editor_state->ui_system->GetWindowFromName(window_name);
		if (window_index != -1) {
			return (unsigned int)index;
		}
	}

	return -1;
}

// ------------------------------------------------------------------------------------------------------------
