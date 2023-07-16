#include "editorpch.h"
#include "VisualizeTexture.h"
#include "../Editor/EditorState.h"

#define MAX_WINDOWS (8)

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------

void VisualizeTextureUIAdditionalDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer) {
	VisualizeTextureAdditionalDrawData* additional_data = (VisualizeTextureAdditionalDrawData*)window_data;
	EditorState* editor_state = (EditorState*)additional_data->private_data;

	if (additional_data->select_elements != nullptr && additional_data->select_elements->capacity > 0) {
		GetVisualizeTextureElements(editor_state, additional_data->select_elements);
	}
}

void VisualizeTextureUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int index = *(unsigned int*)stack_memory;

	CapacityStream<char>* window_name = (CapacityStream<char>*)function::OffsetPointer(stack_memory, sizeof(index));
	window_name->InitializeFromBuffer(function::OffsetPointer(window_name, sizeof(*window_name)), 0, 128);
	GetVisualizeTextureUIWindowName(index, *window_name);

	VisualizeTextureActionData create_data;
	create_data.texture.tex = nullptr;
	create_data.window_name = *window_name;
	create_data.additional_draw = VisualizeTextureUIAdditionalDraw;
	create_data.additional_draw_data = editor_state;
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

void ChangeVisualizeTextureUIWindowTarget(
	EditorState* editor_state, 
	Stream<char> window_name, 
	const VisualizeTextureActionData* create_data
)
{

	ChangeVisualizeTextureWindowOptions(editor_state->ui_system, window_name, create_data);
	// If allowed, unbind the render target view and the depth view from the runtime graphics in case these texture
	// are to be viewed
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		Graphics* runtime_graphics = editor_state->RuntimeGraphics();
		runtime_graphics->BindRenderTargetView(nullptr, nullptr);
	}
}

// ------------------------------------------------------------------------------------------------------------

void ChangeVisualizeTextureUIWindowTarget(
	EditorState* editor_state, 
	unsigned int visualize_index, 
	const VisualizeTextureActionData* create_data
)
{
	ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
	GetVisualizeTextureUIWindowName(visualize_index, window_name);
	ChangeVisualizeTextureUIWindowTarget(editor_state, window_name, create_data);
}

// ------------------------------------------------------------------------------------------------------------
