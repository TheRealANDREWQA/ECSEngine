#include "editorpch.h"
#include "ProjectUITemplatePreview.h"
#include "ProjectUITemplate.h"
#include "../Editor/EditorState.h"
#include "../UI/ToolbarUI.h"
#include "../UI/MiscellaneousBar.h"
#include "../UI/DirectoryExplorer.h"
#include "../UI/FileExplorer.h"
#include "../Editor/EditorParameters.h"
#include "../UI/SandboxExplorer.h"
#include "../UI/Hub.h"
#include "../HelperWindows.h"
#include "../UI/ModuleExplorer.h"
#include "../UI/Inspector.h"
#include "../UI/NotificationBar.h"
#include "../UI/Backups.h"
#include "../UI/Scene.h"
#include "../UI/Game.h"
#include "../UI/EntitiesUI.h"
#include "../UI/AssetExplorer.h"

using namespace ECSEngine;
ECS_TOOLS;

constexpr const char* SAVE_LAYOUT_WINDOW_NAME = "Save Layout";
constexpr float2 SAVE_LAYOUT_WINDOW_SIZE = { 0.5f, 0.25f };

// --------------------------------------------------------------------------------------------------------

void MiscellaneousBarNoActions(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	drawer.DisablePaddingForRenderRegion();
	drawer.DisablePaddingForRenderSliders();
	drawer.DisableZoom();

#pragma region Start, Pause, Frame

	constexpr float button_scale_y = 0.04f;
	float2 button_scale = drawer.GetSquareScale(button_scale_y);

	float2 total_button_scale = { button_scale.x * 3, button_scale.y };
	float2 starting_position = drawer.GetAlignedToCenter(total_button_scale);

	UIDrawConfig config;

	UIConfigAbsoluteTransform transform;
	UIConfigBorder border;
	border.color = Color(30, 140, 70);
	config.AddFlag(border);

	float border_size_horizontal = drawer.NormalizeHorizontalToWindowDimensions(border.thickness);

	transform.position = starting_position;
	transform.scale = button_scale;
	config.AddFlag(transform);

	const size_t configuration = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_BORDER | UI_CONFIG_RECTANGLE_TOOL_TIP;
	const float triangle_scale_factor = 0.8f;
	const float stop_scale_factor = 0.45f;

	// Start button
	UITooltipBaseData base_tool_tip;
	base_tool_tip.offset.y = 0.01f;
	base_tool_tip.offset_scale.y = true;
	base_tool_tip.next_row_offset = 0.005f;

	drawer.SolidColorRectangle(configuration, config, drawer.color_theme.theme);

	float2 scaled_scale;
	float2 scaled_position;

	scaled_position = ExpandRectangle(transform.position, transform.scale, { triangle_scale_factor, triangle_scale_factor }, scaled_scale);
	drawer.SpriteRectangle(configuration, scaled_position, scaled_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, border.color, { 1.0f, 0.0f }, { 0.0f, 1.0f });

	scaled_position = ExpandRectangle(transform.position, transform.scale, { triangle_scale_factor, triangle_scale_factor }, scaled_scale);

	config.flag_count--;
	transform.position.x += button_scale.x;
	config.AddFlag(transform);

	// Pause button - two bars as mask textures
	drawer.SolidColorRectangle(configuration, config, drawer.color_theme.theme);

	float bar_scale_x = button_scale.x / 8;
	constexpr float bar_scale_y = button_scale_y * 0.55f;

	float2 bar_scale = { bar_scale_x, bar_scale_y };
	float2 bar_position = { AlignMiddle(transform.position.x, button_scale.x, bar_scale_x * 3), AlignMiddle(transform.position.y, button_scale.y, bar_scale_y) };
	drawer.SpriteRectangle(configuration, bar_position, bar_scale, ECS_TOOLS_UI_TEXTURE_MASK, border.color);
	drawer.SpriteRectangle(configuration, { bar_position.x + bar_scale.x * 2, bar_position.y }, bar_scale, ECS_TOOLS_UI_TEXTURE_MASK, border.color);
	drawer.TextToolTip("Pause", transform.position, transform.scale, &base_tool_tip);

	transform.position.x += transform.scale.x;
	config.flag_count--;
	config.AddFlag(transform);

	// Frame button - triangle and bar
	drawer.SolidColorRectangle(configuration, config, drawer.color_theme.theme);
	float2 triangle_position = { scaled_position.x + transform.scale.x * 1.85f, scaled_position.y };

	Color frame_color = drawer.color_theme.unavailable_text;
	drawer.SpriteRectangle(configuration, triangle_position, scaled_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, frame_color, { 1.0f, 0.0f }, { 0.0f, 1.0f });
	drawer.SpriteRectangle(configuration, { triangle_position.x + scaled_scale.x, bar_position.y }, bar_scale, ECS_TOOLS_UI_TEXTURE_MASK, frame_color);

#pragma endregion

}

// --------------------------------------------------------------------------------------------------------

void CreateMiscellaneousBarNoActions(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;

	size_t stack_memory[128];
	MiscellaneousBarSetDescriptor(descriptor, editor_state, stack_memory);
	
	descriptor.initial_position_x = -1.0f;
	descriptor.initial_position_y = -1.0f + TOOLBAR_SIZE_Y;
	descriptor.initial_size_x = 2.0f - ECS_TOOLS_UI_ONE_PIXEL_X;
	descriptor.initial_size_y = MISCELLANEOUS_BAR_SIZE_Y;
	descriptor.window_data = nullptr;
	descriptor.window_data_size = 0;

	descriptor.draw = MiscellaneousBarNoActions;

	ui_system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_BACKGROUND | UI_DOCKSPACE_FIXED | UI_DOCKSPACE_NO_DOCKING
	 | UI_DOCKSPACE_BORDER_NOTHING);
}

// --------------------------------------------------------------------------------------------------------

void DeferredSystemClear(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ResetHubData((EditorState*)action_data->data);
	system->Clear();
	Hub((EditorState*)action_data->data);
};

// --------------------------------------------------------------------------------------------------------

void SaveLayoutWindowDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	auto exit = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		system->PushFrameHandler({ DeferredSystemClear, action_data->data, 0 });
	};

	auto save_layout = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		ECS_TEMP_ASCII_STRING(error_message, 256);
		wchar_t path_chars[256];
		CapacityStream<wchar_t> path(path_chars, 0, 256);
		path.Copy(EDITOR_DEFAULT_PROJECT_UI_TEMPLATE);
		path.AddStreamSafe(PROJECT_UI_TEMPLATE_EXTENSION);
		path[path.size] = L'\0';
		bool success = system->WriteUIFile(path.buffer, error_message);
		if (!success) {
			CreateErrorMessageWindow(system, error_message);
		}
		else {
			system->PushFrameHandler({ DeferredSystemClear, action_data->data, 0 });
		}
	};

	UIDrawConfig config;
	UIConfigWindowDependentSize size;
	size.type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
	size.scale_factor.x = 1.0f;

	config.AddFlag(size);

	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, "Save", { save_layout, window_data, 0, ECS_UI_DRAW_SYSTEM });
	drawer.NextRow();
	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, "Exit", { exit, window_data, 0 });
}

// --------------------------------------------------------------------------------------------------------

void CreateSaveLayoutWindow(void* _editor_state) {
	UIWindowDescriptor descriptor;
	
	CenterWindowDescriptor(descriptor, SAVE_LAYOUT_WINDOW_SIZE);

	descriptor.window_name = SAVE_LAYOUT_WINDOW_NAME;
	descriptor.window_data = _editor_state;
	descriptor.window_data_size = 0;

	descriptor.draw = SaveLayoutWindowDraw;

	EDITOR_STATE(_editor_state);

	ui_system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X);
}

// --------------------------------------------------------------------------------------------------------

struct ToolbarPlaceholderData {
	UIActionHandler handlers[TOOLBAR_WINDOW_MENU_COUNT];
	EditorState* editor_state;
};

unsigned int CreatePlaceholderWindow(EditorState* editor_state, const char* window_name, float2 size) {
	UIWindowDescriptor descriptor;

	CenterWindowDescriptor(descriptor, size);

	descriptor.draw = DrawNothing;

	descriptor.window_name = window_name;
	descriptor.window_data = nullptr;
	descriptor.window_data_size = 0;

	EDITOR_STATE(editor_state);
	return ui_system->Create_Window(descriptor);
}

// --------------------------------------------------------------------------------------------------------

void CreatePlaceholderWindowAndDockspace(EditorState* editor_state, const char* window_name, float2 size) {
	EDITOR_STATE(editor_state);
	unsigned int window_index = CreatePlaceholderWindow(editor_state, window_name, size);
	UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
	ui_system->CreateDockspace(window_transform, DockspaceType::FloatingVertical, window_index, false);
}

// --------------------------------------------------------------------------------------------------------

struct PlaceholderDockspaceActionData {
	EditorState* editor_state;
	const char* window_name;
	float2 size;
};

void CreatePlaceholderDockspaceAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	if (IsClickableTrigger(action_data)) {
		PlaceholderDockspaceActionData* data = (PlaceholderDockspaceActionData*)_data;

		unsigned int window_index = system->GetWindowFromName(data->window_name);

		if (window_index == -1) {
			CreatePlaceholderWindowAndDockspace(data->editor_state, data->window_name, data->size);
		}
		else {
			system->SetActiveWindow(window_index);
		}
	}
}

// --------------------------------------------------------------------------------------------------------

void ToolbarUIPlaceholderWindowDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	drawer.DisablePaddingForRenderRegion();
	drawer.DisablePaddingForRenderSliders();
	drawer.DisableZoom();

	drawer.SetNextRowYOffset(0.0f);
	drawer.SetRowPadding(0.0f);

	ToolbarPlaceholderData* data = (ToolbarPlaceholderData*)window_data;

	if (initialize) {
		size_t total_size = sizeof(PlaceholderDockspaceActionData) * TOOLBAR_WINDOW_MENU_COUNT;
		void* allocation = drawer.GetMainAllocatorBuffer(total_size);

		PlaceholderDockspaceActionData* action_data = (PlaceholderDockspaceActionData*)allocation;

		action_data[TOOLBAR_WINDOW_MENU_CONSOLE] = { data->editor_state, CONSOLE_WINDOW_NAME, {1.0f, 0.5f} };
		action_data[TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER] = { data->editor_state, DIRECTORY_EXPLORER_WINDOW_NAME, {0.6f, 1.0f} };
		action_data[TOOLBAR_WINDOW_MENU_FILE_EXPLORER] = { data->editor_state, FILE_EXPLORER_WINDOW_NAME, {0.6f, 1.0f} };
		action_data[TOOLBAR_WINDOW_MENU_MODULE_EXPLORER] = { data->editor_state, MODULE_EXPLORER_WINDOW_NAME, {0.6f, 1.0f} };
		action_data[TOOLBAR_WINDOW_MENU_SANDBOX_EXPLORER] = { data->editor_state, SANDBOX_EXPLORER_WINDOW_NAME, { 0.6f, 1.0f } };
		action_data[TOOLBAR_WINDOW_MENU_ASSET_EXPLORER] = { data->editor_state, ASSET_EXPLORER_WINDOW_NAME, { 0.6f, 1.0f } };
		action_data[TOOLBAR_WINDOW_MENU_BACKUPS] = { data->editor_state, BACKUPS_WINDOW_NAME, {0.4f, 0.7f} };

#define CONCAT(a,b) a #b

		const char* inspector_name = CONCAT(INSPECTOR_WINDOW_NAME, 0);
		const char* game_name = CONCAT(GAME_WINDOW_NAME, 0);
		const char* scene_name = CONCAT(SCENE_WINDOW_NAME, 0);
		const char* entities_name = CONCAT(ENTITIES_UI_WINDOW_NAME, 0);

		action_data[TOOLBAR_WINDOW_MENU_INSPECTOR] = { data->editor_state, inspector_name, {0.6f, 1.0f} };
		action_data[TOOLBAR_WINDOW_MENU_ENTITIES_UI] = { data->editor_state, entities_name, { 0.7f, 1.0f } };
		action_data[TOOLBAR_WINDOW_MENU_GAME_UI] = { data->editor_state, game_name, { 0.6f, 1.0f } };
		action_data[TOOLBAR_WINDOW_MENU_SCENE_UI] = { data->editor_state, scene_name, { 0.6f, 1.0f } };

#undef CONCAT

#define SET_HANDLER(index) data->handlers[index] = {CreatePlaceholderDockspaceAction, action_data + index, 0, ECS_UI_DRAW_SYSTEM}

		SET_HANDLER(TOOLBAR_WINDOW_MENU_CONSOLE);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_FILE_EXPLORER);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_MODULE_EXPLORER);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_SANDBOX_EXPLORER);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_ASSET_EXPLORER);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_INSPECTOR);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_BACKUPS);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_ENTITIES_UI);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_GAME_UI);
		SET_HANDLER(TOOLBAR_WINDOW_MENU_SCENE_UI);

#undef SET_HANDLER
	}

	UIDrawerMenuState state;
	state.click_handlers = data->handlers;
	state.row_count = TOOLBAR_WINDOW_MENU_COUNT;
	state.left_characters = TOOLBAR_WINDOWS_MENU_CHAR_DESCRIPTION;
	state.separation_lines[0] = TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER - 1;
	state.separation_lines[1] = TOOLBAR_WINDOW_MENU_ASSET_EXPLORER;
	state.separation_line_count = 2;
	state.submenu_index = 0;
	
	UIConfigWindowDependentSize size;
	size.type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;

	UIDrawConfig config;
	config.AddFlag(size);

	drawer.Menu(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, "Window", &state);
}

void CreateToolbarUIPlaceholder(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;
	size_t stack_memory[128];
	ToolbarSetDescriptor(descriptor, editor_state, stack_memory);

	descriptor.initial_position_x = -1.0f;
	descriptor.initial_position_y = -1.0f;
	descriptor.initial_size_x = 2.0f - ECS_TOOLS_UI_ONE_PIXEL_X;
	descriptor.initial_size_y = TOOLBAR_SIZE_Y;

	ToolbarPlaceholderData data;
	data.editor_state = editor_state;

	descriptor.window_data = &data;
	descriptor.window_data_size = sizeof(data);

	descriptor.draw = ToolbarUIPlaceholderWindowDraw;

	ui_system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_BACKGROUND | UI_DOCKSPACE_FIXED | UI_DOCKSPACE_NO_DOCKING
		| UI_DOCKSPACE_BORDER_NOTHING);
}

// --------------------------------------------------------------------------------------------------------

void CreateProjectUITemplatePreview(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	ui_system->Clear();

	CreateToolbarUIPlaceholder(editor_state);
	CreateMiscellaneousBarNoActions(editor_state);
	CreateNotificationBar(editor_state);
	UIDockspace* main_dockspace = CreateProjectBackgroundDockspace(ui_system);
	unsigned int viewport_window_index = CreatePlaceholderWindow(editor_state, SANDBOX_EXPLORER_WINDOW_NAME, {1.0f, 1.0f});
	ui_system->AddWindowToDockspaceRegion(viewport_window_index, main_dockspace, 0);
	
	CreateSaveLayoutWindow(editor_state);
}

// --------------------------------------------------------------------------------------------------------

void CreateProjectUITemplatePreviewAction(ActionData* action_data) {
	CreateProjectUITemplatePreview((EditorState*)action_data->data);
}

// --------------------------------------------------------------------------------------------------------