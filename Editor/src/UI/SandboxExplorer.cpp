#include "editorpch.h"
#include "SandboxExplorer.h"
#include "../Editor/EditorState.h"
#include "../UI/HelperWindows.h"

#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxFile.h"
#include "Inspector.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

#define INSTANCE_NAME "Sandbox Explorer Instance"
#define MAX_SIZE_SETTING 128

// --------------------------------------------------------------------------------------------

struct SandboxExplorerData {
	EditorState* editor_state;
	unsigned int active_sandbox;
	LinearAllocator runtime_settings_allocator;
	WorldDescriptor world_descriptor;
	CapacityStream<wchar_t> selected_runtime_setting;
	size_t setting_last_write;
};

struct RemoveSandboxActionData {
	SandboxExplorerData* explorer_data;
	unsigned int index;
};

void RemoveSandboxAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RemoveSandboxActionData* data = (RemoveSandboxActionData*)_data;
	DestroySandbox(data->explorer_data->editor_state, data->index);
	if (data->explorer_data->active_sandbox == data->index) {
		// Indicate that no sandbox is selected
		data->explorer_data->active_sandbox = -1;
	}

	// Also rewrite the sandbox file
	SaveEditorSandboxFile(data->explorer_data->editor_state);
}

// --------------------------------------------------------------------------------------------

void RemoveSandboxPopupAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RemoveSandboxActionData* data = (RemoveSandboxActionData*)_data;
	ECS_FORMAT_TEMP_STRING(message, "Are you sure you want to remove sandbox {#}?", data->index);
	CreateConfirmWindow(system, message, { RemoveSandboxAction, data, sizeof(*data) });
}

// --------------------------------------------------------------------------------------------

void CreateSandboxAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	CreateSandbox(editor_state);
	SaveEditorSandboxFile(editor_state);
}

// --------------------------------------------------------------------------------------------

struct SelectSandboxActionData {
	SandboxExplorerData* explorer_data;
	unsigned int index;
};

// --------------------------------------------------------------------------------------------

void SelectSandboxAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectSandboxActionData* data = (SelectSandboxActionData*)_data;
	data->explorer_data->active_sandbox = data->index;
}

// --------------------------------------------------------------------------------------------

void SandboxExplorerDestroyAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SandboxExplorerData* data = (SandboxExplorerData*)_additional_data;

	data->editor_state->editor_allocator->Deallocate(data->runtime_settings_allocator.m_buffer);

	data->editor_state->editor_allocator->Deallocate(data->selected_runtime_setting.buffer);

	// Destroy the UI instance as well
	data->editor_state->ui_reflection->DestroyInstance(INSTANCE_NAME);
}

// --------------------------------------------------------------------------------------------

void CreateNewRuntimeSettingCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SandboxExplorerData* data = (SandboxExplorerData*)_data;
	CapacityStream<char>* input_name = (CapacityStream<char>*)_additional_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 512);
	ConvertASCIIToWide(wide_name, *input_name);

	WorldDescriptor default_descriptor = GetDefaultWorldDescriptor();
	// Try to save the default descriptor. If it succcededs, then we can swap to this new file
	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	bool success = SaveRuntimeSettings(data->editor_state, wide_name, &default_descriptor, &error_message);
	if (!success) {
		ECS_FORMAT_TEMP_STRING(console_message, "Could not save new runtime setting {#}. Detailed error: {#}.", *input_name, error_message);
		EditorSetConsoleError(console_message);
	}
	else {
		// Change the path and the world descriptor now
		data->selected_runtime_setting.CopyOther(wide_name);
		memcpy(&data->world_descriptor, &default_descriptor, sizeof(default_descriptor));
	}
}

// --------------------------------------------------------------------------------------------

struct OpenSandboxInspectorSettingsData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

void OpenSandboxInspectorSettings(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	OpenSandboxInspectorSettingsData* data = (OpenSandboxInspectorSettingsData*)_data;
	ChangeInspectorToSandboxSettings(data->editor_state, -1, data->sandbox_index);
}

// --------------------------------------------------------------------------------------------

void SandboxExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SandboxExplorerData* data = (SandboxExplorerData*)window_data;

	EditorState* editor_state = data->editor_state;

	if (initialize) {
		// Set the world descriptor to default - just to be sure
		data->world_descriptor = GetDefaultWorldDescriptor();

		// Initialize the UI instance
		UIReflectionInstance* instance = editor_state->ui_reflection->CreateInstance(INSTANCE_NAME, STRING(WorldDescriptor));
		editor_state->ui_reflection->BindInstancePtrs(instance, &data->world_descriptor);

		// Create the runtime setting buffer
		data->selected_runtime_setting.Initialize(editor_state->editor_allocator, 0, MAX_SIZE_SETTING);

		const size_t ALLOCATOR_SIZE = ECS_KB * 2;
		data->runtime_settings_allocator = LinearAllocator(editor_state->editor_allocator->Allocate(ALLOCATOR_SIZE), ALLOCATOR_SIZE);
	}
	
	UIDrawConfig config;

	UIConfigTextAlignment text_alignment;
	text_alignment.horizontal = ECS_UI_ALIGN_LEFT;
	
	// Exclude temporary sandboxes
	unsigned int sandbox_count = GetSandboxCount(editor_state, true);
	// Display the number of sandboxes
	if (sandbox_count > 0) {
		ECS_FORMAT_TEMP_STRING(count_string, "Project sandbox count: {#}", sandbox_count);
		drawer.Text(count_string.buffer);
		drawer.NextRow();

		UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
		row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
		row_layout.AddSquareLabel();
		row_layout.AddSquareLabel();

		ECS_STACK_CAPACITY_STREAM(char, display_labels, 128);
		display_labels.CopyOther("Sandbox ");

		size_t button_configuration = UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_TEXT_ALIGNMENT;
		size_t sprite_configuration = 0;

		UIDrawConfig button_config;
		UIDrawConfig sprite_config;

		row_layout.GetTransform(button_config, button_configuration);
		row_layout.GetTransform(sprite_config, sprite_configuration);

		button_config.AddFlag(text_alignment);

		size_t label_configuration = button_configuration;

		unsigned int base_display_size = display_labels.size;
		for (unsigned int index = 0; index < sandbox_count; index++) {
			display_labels.size = base_display_size;
			ConvertIntToChars(display_labels, index);

			label_configuration |= data->active_sandbox == index ? 0 : UI_CONFIG_LABEL_TRANSPARENT;
			
			SelectSandboxActionData select_data;
			select_data.explorer_data = data;
			select_data.index = index;
			drawer.Button(label_configuration, button_config, display_labels.buffer, { SelectSandboxAction, &select_data, sizeof(select_data) });
			label_configuration = button_configuration;

			// Draw the configuration button which will open up an inspector with that sandbox selected
			OpenSandboxInspectorSettingsData open_data;
			open_data.editor_state = editor_state;
			open_data.sandbox_index = index;
			drawer.SpriteButton(sprite_configuration, sprite_config, { OpenSandboxInspectorSettings, &open_data, sizeof(open_data), ECS_UI_DRAW_SYSTEM }, ECS_TOOLS_UI_TEXTURE_COG);

			RemoveSandboxActionData remove_data;
			remove_data.explorer_data = data;
			remove_data.index = index;

			UIConfigActiveState active_state;
			active_state.state = !IsSandboxLocked(editor_state, index);
			sprite_config.AddFlag(active_state);

			drawer.SpriteButton(
				sprite_configuration | UI_CONFIG_ACTIVE_STATE, 
				sprite_config, 
				{ RemoveSandboxPopupAction, &remove_data, sizeof(remove_data), ECS_UI_DRAW_SYSTEM }, 
				ECS_TOOLS_UI_TEXTURE_X
			);
			drawer.NextRow();
			sprite_config.flag_count--;
		}
	}
	else {
		drawer.Text("No sandbox created");
		drawer.NextRow();
	}

	// The create button now
	UIConfigWindowDependentSize create_transform;
	config.AddFlag(create_transform);

	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Create Sandbox", { CreateSandboxAction, data->editor_state, 0 });
	drawer.NextRow();

	drawer.CrossLine();

	// Runtime settings now
	
	// Clear the linear allocator
	data->runtime_settings_allocator.Clear();
	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, runtime_settings, 128);
	GetSandboxAvailableRuntimeSettings(editor_state, runtime_settings, GetAllocatorPolymorphic(&data->runtime_settings_allocator));
	runtime_settings.AssertCapacity();

	auto create_new_runtime_setting_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		SandboxExplorerData* data = (SandboxExplorerData*)_data;

		TextInputWizardData wizard_data;
		wizard_data.window_name = "Select a name";
		wizard_data.input_name = "Setting name";
		wizard_data.callback = CreateNewRuntimeSettingCallback;
		wizard_data.callback_data = data;
		wizard_data.callback_data_size = 0;
		
		action_data->data = &wizard_data;
		CreateTextInputWizardAction(action_data);
	};

	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Create Runtime Setting", { create_new_runtime_setting_action, data, 0, ECS_UI_DRAW_SYSTEM });
	drawer.NextRow();

	if (runtime_settings.size == 0) {
		drawer.Text("No runtime settings have been created.");
		drawer.NextRow();
	}
	else {
		drawer.Text("Project runtime settings:");
		drawer.NextRow();

		float2 square_scale = drawer.GetSquareScale(drawer.layout.default_element_y);
		config.flag_count = 0;
		float window_span = drawer.GetWindowScaleUntilBorder();
		float label_size = window_span - drawer.layout.element_indentation - square_scale.x;

		UIConfigWindowDependentSize label_transform;
		label_transform.scale_factor.x = drawer.GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, { label_size, 0.0f }).x;
		config.AddFlag(label_transform);

		config.AddFlag(text_alignment);

		struct SelectData {
			SandboxExplorerData* data;
			Stream<wchar_t> path;
		};

		auto select_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SelectData* data = (SelectData*)_data;
			// Copy the runtime descriptor
			ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
			bool success = LoadRuntimeSettings(data->data->editor_state, data->path, &data->data->world_descriptor, &error_message);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(
					console_message,
					"Could not change settings preview to {#}. Detailed error message: {#}",
					data->path,
					error_message
				);
				EditorSetConsoleError(console_message);
				return;
			}
			
			UpdateRuntimeSettings(data->data->editor_state, data->path, &data->data->setting_last_write);
			data->data->selected_runtime_setting.CopyOther(data->path);
		};

		struct DeleteData {
			SandboxExplorerData* data;
			Stream<wchar_t> path;
		};

		auto delete_setting_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			DeleteData* data = (DeleteData*)_data;
			if (data->data->selected_runtime_setting == data->path) {
				// Reset the path
				data->data->selected_runtime_setting.size = 0;
			}

			ECS_STACK_CAPACITY_STREAM(wchar_t, full_path, 512);
			GetSandboxRuntimeSettingsPath(data->data->editor_state, data->path, full_path);
			bool success = RemoveFile(full_path);
		};

		for (unsigned int index = 0; index < runtime_settings.size; index++) {
			size_t configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_TRANSPARENT 
				| UI_CONFIG_TEXT_ALIGNMENT;
			if (data->selected_runtime_setting == runtime_settings[index]) {
				configuration = ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
			}
			SelectData select_data;
			select_data.data = data;
			select_data.path = runtime_settings[index];
			drawer.ButtonWide(configuration, config, runtime_settings[index], { select_action, &select_data, sizeof(select_data) });

			DeleteData delete_data;
			delete_data.data = data;
			delete_data.path = runtime_settings[index];
			drawer.SpriteButton(UI_CONFIG_MAKE_SQUARE, config, { delete_setting_action, &delete_data, sizeof(delete_data) }, ECS_TOOLS_UI_TEXTURE_X);
			drawer.NextRow();
		}

		config.flag_count = 0;
	}

	drawer.CrossLine();

	// If there is a selected runtime setting
	if (data->selected_runtime_setting.size > 0) {
		drawer.Text("Selected runtime settings");
		drawer.NextRow();

		UIConfigWindowDependentSize dependent_size;
		config.AddFlag(dependent_size);
		UIConfigNamePadding name_padding;
		name_padding.alignment = ECS_UI_ALIGN_LEFT;
		name_padding.total_length = 0.32f;
		config.AddFlag(name_padding);

		UIReflectionDrawInstanceOptions options;
		options.drawer = &drawer;
		options.config = &config;
		options.global_configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_NAME_PADDING | UI_CONFIG_ELEMENT_NAME_FIRST;
		editor_state->ui_reflection->DrawInstance(INSTANCE_NAME, &options);

		// Draw a save button and a default button
		config.flag_count = 0;
		UIConfigAbsoluteTransform transform;
		transform.scale = drawer.GetLabelScale("Save");
		transform.position = drawer.GetAlignedToBottom(transform.scale.y);
		config.AddFlag(transform);

		auto save_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SandboxExplorerData* data = (SandboxExplorerData*)_data;
			ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
			// Validate the descriptor
			if (ValidateWorldDescriptor(&data->world_descriptor)) {
				bool success = SaveRuntimeSettings(data->editor_state, data->selected_runtime_setting, &data->world_descriptor, &error_message);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(console_message, "Saving runtime settings {#} failed. Detailed error: {#}.", data->selected_runtime_setting, error_message);
					EditorSetConsoleError(console_message);
				}
			}
			else {
				EditorSetConsoleError("The current runtime setting is not valid. Increase the global allocator size.");
				// Copy back all the old values
				ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
				bool success = LoadRuntimeSettings(data->editor_state, data->selected_runtime_setting, &data->world_descriptor, &error_message);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(console_message, "Failed to reload the selected runtime setting {#}. Detailed error: {#}.", data->selected_runtime_setting, error_message);
					EditorSetConsoleError(console_message);
				}
			}
		};

		size_t save_configuration = UI_CONFIG_ABSOLUTE_TRANSFORM;
		drawer.Button(save_configuration, config, "Save", { save_action, data, 0 });

		config.flag_count = 0;
		transform.scale = drawer.GetLabelScale("Default Values");
		transform.position.x = drawer.GetAlignedToRight(transform.scale.x).x;
		config.AddFlag(transform);
		
		auto default_values_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SandboxExplorerData* data = (SandboxExplorerData*)_data;
			data->world_descriptor = GetDefaultWorldDescriptor();
		};

		size_t default_values_configuration = UI_CONFIG_ABSOLUTE_TRANSFORM;
		drawer.Button(default_values_configuration, config, "Default Values", { default_values_action, data, 0 });
	}
}

// --------------------------------------------------------------------------------------------

void CreateSandboxExplorer(EditorState* editor_state) {
	CreateDockspaceFromWindow(SANDBOX_EXPLORER_WINDOW_NAME, editor_state, CreateSandboxExplorerWindow);
}

// --------------------------------------------------------------------------------------------

void CreateSandboxExplorerAction(ActionData* action_data) {
	CreateSandboxExplorer((EditorState*)action_data->data);
}

// --------------------------------------------------------------------------------------------

void SandboxExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	descriptor.draw = SandboxExplorerDraw;

	SandboxExplorerData* data = (SandboxExplorerData*)stack_memory;
	data->editor_state = editor_state;
	data->active_sandbox = -1;

	descriptor.window_name = SANDBOX_EXPLORER_WINDOW_NAME;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);

	descriptor.destroy_action = SandboxExplorerDestroyAction;
}

// --------------------------------------------------------------------------------------------

unsigned int CreateSandboxExplorerWindow(EditorState* editor_state) {
	constexpr float window_size_x = 0.8f;
	constexpr float window_size_y = 0.8f;
	float2 window_size = { window_size_x, window_size_y };

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, window_size.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, window_size.y);
	descriptor.initial_size_x = window_size.x;
	descriptor.initial_size_y = window_size.y;

	size_t stack_memory[128];
	SandboxExplorerSetDescriptor(descriptor, editor_state, stack_memory);

	return editor_state->ui_system->Create_Window(descriptor);
}

// --------------------------------------------------------------------------------------------