#include "editorpch.h"
#include "NotificationBar.h"
#include "../Editor/EditorState.h"

using namespace ECSEngine;
ECS_TOOLS;

// Create an internal allocator to not put extra pressure on the editor allocator
struct NotificationBarData {
	EditorState* editor_state;
};

constexpr size_t NOTIFICATION_HISTORY_STRING_FORMAT = ECS_LOCAL_TIME_FORMAT_HOUR | ECS_LOCAL_TIME_FORMAT_MINUTES | ECS_LOCAL_TIME_FORMAT_SECONDS;


void NotificationBarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	NotificationBarData* data = (NotificationBarData*)stack_memory;
	data->editor_state = editor_state;

	descriptor.draw = NotificationBarDraw;
	descriptor.window_name = NOTIFICATION_BAR_WINDOW_NAME;

	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
}

void FocusConsole(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	// Create console handles the case when the window already exists
	// If it doesn't, create it
	CreateConsole(system, editor_state->console);
}

void NotificationBarDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	NotificationBarData* data = (NotificationBarData*)window_data;
	EDITOR_STATE(data->editor_state);

	drawer.DisablePaddingForRenderRegion();
	drawer.DisablePaddingForRenderSliders();
	drawer.DisableZoom();

	constexpr float NOTIFICATION_MESSAGE_SIZE = 1.9f;
	constexpr float TEXT_LABEL_Y_SIZE = NOTIFICATION_BAR_WINDOW_SIZE - 0.015f;
	constexpr float REGION_PADDING = 0.01f;

	unsigned int message_index = console->messages.size - 1;
	unsigned int console_window = drawer.system->GetWindowFromName(CONSOLE_WINDOW_NAME);

	if (console_window != -1) {
		ConsoleWindowData* console_data = (ConsoleWindowData*)drawer.system->GetWindowData(console_window);

		// During initialization console data might be nullptr
		// Because of lazy evaluation filtered message indices might be updated later than the actual messages
		if (console_data != nullptr) {
			// Tick the console filtering process since it may not be called when it is not visible
			if (!initialize && !drawer.system->IsWindowDrawing(console_window)) {
				ConsoleFilterMessages(console_data, drawer);
			}

			if (console_data->filtered_message_indices.size > 0 && console->messages.size > 0) {
				message_index = console_data->filtered_message_indices[console_data->filtered_message_indices.size - 1];
			}
			else {
				message_index = -1;
			}
		}
	}

	UIDrawConfig config;
	drawer.current_y = drawer.region_position.y;
	drawer.current_x = drawer.region_position.x + REGION_PADDING;

	// Display only the last notification - if any
	if (message_index != -1) {
		float2 action_position = drawer.GetCurrentPositionNonOffset();
		const ConsoleMessage* message = console->messages.buffer + message_index;

		UIConfigRelativeTransform relative_transform;
		relative_transform.scale.y = TEXT_LABEL_Y_SIZE / drawer.layout.default_element_y;
		relative_transform.offset.y = (NOTIFICATION_BAR_WINDOW_SIZE - TEXT_LABEL_Y_SIZE) * 0.5f;
		config.AddFlag(relative_transform);
		Color sprite_color = ECS_COLOR_WHITE;
		if (message->icon_type != ConsoleMessageType::Error) {
			sprite_color = CONSOLE_COLORS[(unsigned int)message->icon_type];
		}

		drawer.SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_MAKE_SQUARE, config, CONSOLE_TEXTURE_ICONS[(unsigned int)message->icon_type], sprite_color);

		UIConfigWindowDependentSize text_size;
		text_size.scale_factor = drawer.GetWindowSizeFactors(text_size.type, { NOTIFICATION_MESSAGE_SIZE, NOTIFICATION_BAR_WINDOW_SIZE });
		config.AddFlag(text_size);

		UIConfigTextParameters text_params;
		text_params.character_spacing = drawer.font.character_spacing;
		text_params.size = drawer.GetFontSize();
		text_params.color = CONSOLE_COLORS[(unsigned int)message->icon_type];
		config.AddFlag(text_params);

		UIConfigTextAlignment text_alignment;
		text_alignment.horizontal = TextAlignment::Left;
		config.AddFlag(text_alignment);

		drawer.TextLabel(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_TEXT_PARAMETERS
			| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT, config, message->message.buffer);

		float2 action_scale = { drawer.current_x - action_position.x, TEXT_LABEL_Y_SIZE };
		drawer.AddClickable(action_position, action_scale, { FocusConsole, data->editor_state, 0 });

		float2 notification_size = drawer.GetLabelScale(message->message.buffer);
		if (notification_size.x > NOTIFICATION_MESSAGE_SIZE) {			
			drawer.Indent(-1.0f);
			drawer.SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_HORIZONTAL_DOTS, drawer.color_theme.default_text);
		}

	}
}

// It creates the dockspace and the window
void CreateNotificationBar(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	unsigned int window_index = CreateNotificationBarWindow(editor_state);
	ECS_ASSERT(window_index != -1);
	float2 window_position = ui_system->GetWindowPosition(window_index);
	float2 window_scale = ui_system->GetWindowScale(window_index);
	ui_system->CreateFixedDockspace({ window_position, window_scale }, DockspaceType::FloatingHorizontal, window_index, false, UI_DOCKSPACE_BACKGROUND
		| UI_DOCKSPACE_BORDER_NOTHING | UI_DOCKSPACE_NO_DOCKING);
}

// It creates the dockspace and the window
void CreateNotificationBarAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateNotificationBar((EditorState*)action_data->data);
}

unsigned int CreateNotificationBarWindow(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	float2 window_size = { 2.0f - ui_system->m_descriptors.dockspaces.border_size, NOTIFICATION_BAR_WINDOW_SIZE };
	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = -1.0f;
	descriptor.initial_position_y = 1.0f - NOTIFICATION_BAR_WINDOW_SIZE - ui_system->m_descriptors.dockspaces.border_size;
	descriptor.initial_size_x = window_size.x;
	descriptor.initial_size_y = window_size.y;

	size_t stack_memory[128];
	NotificationBarSetDescriptor(descriptor, editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
}