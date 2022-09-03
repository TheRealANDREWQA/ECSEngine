#include "editorpch.h"
#include "Sandbox.h"
#include "../Editor/EditorState.h"
#include "../HelperWindows.h"

#include "../Editor/EditorSandbox.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

// -------------------------------------------------------------------------------------------------

struct SandboxUIData {
	EditorState* editor_state;
};

void SandboxUIDestroy(ActionData* action_data) {
	
}

// -------------------------------------------------------------------------------------------------

void SandboxUIDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SandboxUIData* data = (SandboxUIData*)window_data;

	EDITOR_STATE(data->editor_state);
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_index = GetSandboxUIIndexFromName(drawer.system->GetWindowName(drawer.window_index));

	if (initialize) {
		
	}
}

// -------------------------------------------------------------------------------------------------

void CreateSandboxUI(EditorState* editor_state, unsigned int sandbox_index) {
	EDITOR_STATE(editor_state);

	unsigned int window_index = GetSandboxUIWindowIndex(editor_state, sandbox_index);
	if (window_index == -1) {
		window_index = CreateSandboxUIWindow(editor_state, sandbox_index);

		UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
		ui_system->CreateDockspace(window_transform, DockspaceType::FloatingVertical, window_index, false);
	}
	else {
		ui_system->SetActiveWindow(window_index);
	}
}

// -------------------------------------------------------------------------------------------------

void CreateSandboxUIAction(ActionData* action_data) {
	CreateSandboxUIActionData* data = (CreateSandboxUIActionData*)action_data->data;
	CreateSandboxUI(data->editor_state, data->sandbox_index);
}

// -------------------------------------------------------------------------------------------------

void SandboxUISetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int sandbox_index = *(unsigned int*)stack_memory;

	descriptor.draw = SandboxUIDraw;

	SandboxUIData* data = (SandboxUIData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));
	data->editor_state = editor_state;

	CapacityStream<char> window_name(function::OffsetPointer(data, sizeof(*data)), 0, 128);
	GetSandboxUIWindowName(sandbox_index, window_name);

	descriptor.window_name = window_name.buffer;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);

	descriptor.destroy_action = SandboxUIDestroy;
}

// -------------------------------------------------------------------------------------------------

unsigned int CreateSandboxUIWindow(EditorState* editor_state, unsigned int sandbox_index) {
	EDITOR_STATE(editor_state);

	ECS_ASSERT(sandbox_index < MAX_SANDBOX_UI_WINDOWS);

	constexpr float window_size_x = 0.8f;
	constexpr float window_size_y = 0.8f;
	float2 window_size = { window_size_x, window_size_y };

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, window_size.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, window_size.y);
	descriptor.initial_size_x = window_size.x;
	descriptor.initial_size_y = window_size.y;

	size_t stack_memory[128];
	unsigned int* stack_sandbox_index = (unsigned int*)stack_memory;
	*stack_sandbox_index = sandbox_index;
	SandboxUISetDescriptor(descriptor, editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
}

// -------------------------------------------------------------------------------------------------

void GetSandboxUIWindowName(unsigned int sandbox_index, CapacityStream<char>& name)
{
	name.Copy(SANDBOX_UI_WINDOW_NAME);
	function::ConvertIntToChars(name, sandbox_index);
}

// -------------------------------------------------------------------------------------------------

unsigned int GetSandboxUIWindowIndex(const EditorState* editor_state, unsigned int sandbox_index)
{
	const UISystem* system = editor_state->ui_system;
	ECS_STACK_CAPACITY_STREAM(char, window_name, 128);
	GetSandboxUIWindowName(sandbox_index, window_name);

	return system->GetWindowFromName(window_name);
}

// -------------------------------------------------------------------------------------------------

unsigned int GetSandboxUIIndexFromName(Stream<char> name)
{
	return function::ConvertCharactersToInt(Stream<char>(name.buffer + sizeof(SANDBOX_UI_WINDOW_NAME) - 1, name.size - sizeof(SANDBOX_UI_WINDOW_NAME) + 1));
}

// -------------------------------------------------------------------------------------------------

void UpdateSandboxUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index)
{
	unsigned int window_index = GetSandboxUIWindowIndex(editor_state, old_index);
	if (window_index != -1) {
		ECS_STACK_CAPACITY_STREAM(char, new_window_name, 64);
		GetSandboxUIWindowName(new_index, new_window_name);
		editor_state->ui_system->SetWindowName(window_index, new_window_name);
	}
}

// -------------------------------------------------------------------------------------------------
