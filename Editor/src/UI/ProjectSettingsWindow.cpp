#include "editorpch.h"
#include "ProjectSettingsWindow.h"
#include "../Editor/EditorState.h"
#include "HelperWindows.h"

const float2 WINDOW_SIZE = { 0.5f, 0.5f };

ECS_TOOLS;

#define INSTANCE_NAME "Project Settings"

static void ChangeSettingsCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	bool success = WriteProjectSettings(editor_state);
	if (!success) {
		EditorSetConsoleWarn("Failed to write project settings");
	}
}

static void ProjectSettingsDestroyAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	// The instance will be destroyed as well
	editor_state->ui_reflection->DestroyType(STRING(ProjectSettings));
}

void ProjectSettingsWindowSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.window_name = PROJECT_SETTINGS_WINDOW_NAME;

	descriptor.draw = ProjectSettingsDraw;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
}

void ProjectSettingsDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	if (initialize) {
		UIReflectionType* ui_type = editor_state->ui_reflection->CreateType(STRING(ProjectSettings));

		ProjectSettings lower_bound = ProjectSettingsLowerBound();
		ProjectSettings upper_bound = ProjectSettingsUpperBound();
		editor_state->ui_reflection->BindTypeLowerBounds(ui_type, &lower_bound);
		editor_state->ui_reflection->BindTypeUpperBounds(ui_type, &upper_bound);

		UIReflectionInstance* instance = editor_state->ui_reflection->CreateInstance(INSTANCE_NAME, ui_type);
		editor_state->ui_reflection->BindInstancePtrs(instance, &editor_state->project_settings);
	}

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 4);
	ECS_STACK_CAPACITY_STREAM(UIReflectionDrawConfig, ui_draw_configs, (unsigned int)UIReflectionElement::Count);
	memset(ui_draw_configs.buffer, 0, ui_draw_configs.MemoryOf(ui_draw_configs.capacity));
	ui_draw_configs.size = ui_draw_configs.capacity;
	ui_draw_configs.size = UIReflectionDrawConfigSplatCallback(
		ui_draw_configs,
		{ ChangeSettingsCallback, editor_state, 0 }, 
		ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ALL, 
		stack_memory.buffer
	);

	UIDrawConfig config;
	UIConfigNamePadding name_padding;
	name_padding.total_length = 0.12f;
	config.AddFlag(name_padding);

	UIConfigWindowDependentSize dependent_size;
	config.AddFlag(dependent_size);

	UIReflectionDrawInstanceOptions draw_options;
	draw_options.drawer = &drawer;
	draw_options.config = &config;
	draw_options.global_configuration = UI_CONFIG_NAME_PADDING | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_WINDOW_DEPENDENT_SIZE;
	draw_options.additional_configs = ui_draw_configs;
	draw_options.default_value_button = "Default Values";

	editor_state->ui_reflection->DrawInstance(INSTANCE_NAME, &draw_options);
}

unsigned int CreateProjectSettingsWindowOnly(EditorState* editor_state) {
	UIWindowDescriptor descriptor;

	ProjectSettingsWindowSetDescriptor(descriptor, editor_state, nullptr);
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	return editor_state->ui_system->Create_Window(descriptor);
}

void CreateProjectSettingsWindow(EditorState* editor_state) {
	CreateDockspaceFromWindow(PROJECT_SETTINGS_WINDOW_NAME, editor_state, CreateProjectSettingsWindowOnly);
}

void CreateProjectSettingsWindowAction(ActionData* action_data) {
	CreateProjectSettingsWindow((EditorState*)action_data->data);
}