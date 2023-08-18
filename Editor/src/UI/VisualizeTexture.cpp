#include "editorpch.h"
#include "VisualizeTexture.h"
#include "../Editor/EditorState.h"

#define MAX_WINDOWS (8)

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------

struct AutomaticRefreshCallbackData {
	bool* flag;
};

void AutomaticRefreshCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	AutomaticRefreshCallbackData* data = (AutomaticRefreshCallbackData*)_data;
	UpdateVisualizeTextureWindowAutomaticRefresh(system, system->GetWindowData(system->GetWindowIndexFromBorder(dockspace, border_index)), *data->flag);
}

struct ComboCallbackData {
	void* visualize_texture_data;
	VisualizeTextureSelectElement* elements;
	unsigned char* flag_index;
};

void ComboCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;
	
	ComboCallbackData* data = (ComboCallbackData*)_data;
	VisualizeTextureSelectElement element = data->elements[*data->flag_index];
	ChangeVisualizeTextureWindowOptions(system, data->visualize_texture_data, &element);
}

void VisualizeTextureUIAdditionalDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer) {
	VisualizeTextureAdditionalDrawData* additional_data = (VisualizeTextureAdditionalDrawData*)window_data;
	EditorState* editor_state = (EditorState*)additional_data->private_data;

	if (additional_data->is_select_mode) {
		GetVisualizeTextureElements(editor_state, additional_data->select_elements);
	}
	else {
		// Automatic Refresh Checkbox
		additional_data->check_box_name = "Automatic Refresh";
		AutomaticRefreshCallbackData* callback_data = (AutomaticRefreshCallbackData*)additional_data->check_box_callback_memory->buffer;
		callback_data->flag = additional_data->check_box_flag;
		additional_data->check_box_callback = { AutomaticRefreshCallback, callback_data, sizeof(*callback_data) };
		
		// Combo context
		ECS_STACK_CAPACITY_STREAM(VisualizeTextureSelectElement, select_elements, ECS_KB);
		GetVisualizeTextureElements(editor_state, &select_elements);

		Stream<char> scene_starts_with = "Scene ";
		Stream<char> runtime_starts_with = "Runtime ";
		Stream<char> depth_ends_with = " depth";
		Stream<char> render_ends_with = " render";

		// Now filter the entries which belong to sandbox textures
		for (unsigned int index = 0; index < select_elements.size; index++) {
			Stream<char> name = select_elements[index].name;
			if (!name.StartsWith(scene_starts_with) && !name.StartsWith(runtime_starts_with) && !name.EndsWith(render_ends_with) &&
				!name.EndsWith(depth_ends_with)) {
				select_elements.RemoveSwapBack(index);
			}
		}
		ECS_ASSERT(additional_data->combo_labels->capacity >= select_elements.size);

		// Sort again the entries
		function::insertion_sort(select_elements.buffer, select_elements.size, 1, [](const VisualizeTextureSelectElement& left, const VisualizeTextureSelectElement& right) {
			return function::StringLexicographicCompare(left.name, right.name);
		});

		// Now put the entries into a temp buffer from the drawer and fill in the temp memory
		VisualizeTextureSelectElement* allocated_select_elements = (VisualizeTextureSelectElement*)additional_data->drawer->GetTempBuffer(
			sizeof(VisualizeTextureSelectElement) * select_elements.size
		);
		select_elements.CopyTo(allocated_select_elements);
		
		ComboCallbackData* combo_callback_data = (ComboCallbackData*)additional_data->combo_callback_memory->buffer;
		combo_callback_data->elements = allocated_select_elements;
		combo_callback_data->flag_index = additional_data->combo_index;
		combo_callback_data->visualize_texture_data = additional_data->window_data;
		additional_data->combo_callback = { ComboCallback, combo_callback_data, sizeof(*combo_callback_data) };
		for (unsigned int index = 0; index < select_elements.size; index++) {
			additional_data->combo_labels->Add(select_elements[index].name);
		}
	}
}

void VisualizeTextureUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int index = *(unsigned int*)stack_memory;

	CapacityStream<char>* window_name = (CapacityStream<char>*)function::OffsetPointer(stack_memory, sizeof(index));
	window_name->InitializeFromBuffer(function::OffsetPointer(window_name, sizeof(*window_name)), 0, 128);
	GetVisualizeTextureUIWindowName(index, *window_name);

	VisualizeTextureCreateData create_data;
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

		VisualizeTextureCreateData create_data;
		create_data.texture.tex = nullptr;
		create_data.window_name = window_name;
		create_data.additional_draw = VisualizeTextureUIAdditionalDraw;
		create_data.additional_draw_data = editor_state;
		create_data.select_mode = true;
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
	const VisualizeTextureCreateData* create_data
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
	const VisualizeTextureCreateData* create_data
)
{
	ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
	GetVisualizeTextureUIWindowName(visualize_index, window_name);
	ChangeVisualizeTextureUIWindowTarget(editor_state, window_name, create_data);
}

// ------------------------------------------------------------------------------------------------------------
