#include "editorpch.h"
#include "MiscellaneousBar.h"
#include "ToolbarUI.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorPalette.h"
#include "../Editor/EditorEvent.h"
#include "../Modules/Module.h"
#include "../Sandbox/Sandbox.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr float TOOP_TIP_OFFSET = 0.01f;

// ----------------------------------------------------------------------------------------------------------------------

EDITOR_EVENT(StartUnstartedSandboxEvent) {
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) || EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		return true;
	}
	StartSandboxWorlds(editor_state);
	return false;
}

void RunProjectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING)) {
		EndSandboxWorldSimulations(editor_state);
		EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PLAYING);
		if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
			EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PAUSED);
		}
	}
	else {
		if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) || EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
			EditorAddEvent(editor_state, StartUnstartedSandboxEvent, nullptr, 0);
		}
		else {
			StartSandboxWorlds(editor_state);
		}
		EditorStateSetFlag(editor_state, EDITOR_STATE_IS_PLAYING);
	}
}

// ----------------------------------------------------------------------------------------------------------------------

EDITOR_EVENT(StartPausedSandboxEvent) {
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) || EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		return true;
	}
	StartSandboxWorlds(editor_state, true);
	return false;
}

void PauseProjectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
		EditorStateClearFlag(editor_state, EDITOR_STATE_IS_PAUSED);
		// In case there are assets being loaded, push an event to start the runtimes
		if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) || EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
			EditorAddEvent(editor_state, StartPausedSandboxEvent, nullptr, 0);
		}
		else {
			StartSandboxWorlds(editor_state, true);
		}
	}
	else {
		EditorStateSetFlag(editor_state, EDITOR_STATE_IS_PAUSED);
		PauseSandboxWorlds(editor_state);
	}
}

// ----------------------------------------------------------------------------------------------------------------------

EDITOR_EVENT(StepProjectEvent) {
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) || EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		return true;
	}
	RunSandboxWorlds(editor_state, true);
	return false;
}

void StepProjectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
		// In case there are assets being loaded, we need to push an event to run a step
		if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH) || EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
			EditorAddEvent(editor_state, StepProjectEvent, nullptr, 0);
		}
		else {
			RunSandboxWorlds(editor_state, true);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------

void MiscellaneousBarDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	drawer.DisablePaddingForRenderRegion();
	drawer.DisablePaddingForRenderSliders();
	drawer.DisableZoom();

	EditorState* editor_state = (EditorState*)window_data;

#pragma region Start, Pause, Frame
	
	UIDrawConfig config;

	// If the start button is active, then display a small black overlay to indicate to the user that we are in run mode
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING)) {
		UIConfigAbsoluteTransform overlay_transform;
		overlay_transform.position = { -1.0f, -1.0f };
		overlay_transform.scale = { 2.0f, 2.0f };
		config.AddFlag(overlay_transform);
		drawer.SpriteRectangle(
			UI_CONFIG_SYSTEM_DRAW | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION, 
			config, 
			ECS_TOOLS_UI_TEXTURE_MASK,
			Color((unsigned char)0, 0, 0, 25)
		);
		config.flag_count--;
	}

	const float button_scale_y = 0.05f;
	float2 button_scale = drawer.GetSquareScale(button_scale_y);

	float2 total_button_scale = { button_scale.x * 3, button_scale.y };
	float2 starting_position = drawer.GetAlignedToCenter(total_button_scale);


	UIConfigAbsoluteTransform transform;
	UIConfigBorder border;
	border.color = EDITOR_GREEN_COLOR;
	config.AddFlag(border);

	float border_size_horizontal = drawer.NormalizeHorizontalToWindowDimensions(border.thickness);

	transform.position = starting_position;
	transform.scale = button_scale;
	config.AddFlag(transform);

	const size_t configuration = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_BORDER | UI_CONFIG_RECTANGLE_TOOL_TIP;
	const float triangle_scale_factor = 0.8f;
	const float stop_scale_factor = 0.45f;

	Color theme_color = drawer.color_theme.theme;
	Color accent_color = EDITOR_GREEN_COLOR;

	// Start button
	UITooltipBaseData base_tool_tip;
	base_tool_tip.offset.y = TOOP_TIP_OFFSET;
	base_tool_tip.offset_scale.y = true;
	base_tool_tip.next_row_offset = 0.005f;

	bool is_playing = EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PLAYING);
	Color playing_theme_color = is_playing ? accent_color : theme_color;
	Color playing_accent_color = is_playing ? theme_color : accent_color;

	drawer.SolidColorRectangle(configuration, config, playing_theme_color);

	float2 scaled_scale;
	float2 scaled_position;
	if (is_playing) {
		scaled_position = ExpandRectangle(transform.position, transform.scale, { stop_scale_factor, stop_scale_factor }, scaled_scale);
		drawer.SpriteRectangle(configuration, scaled_position, scaled_scale, ECS_TOOLS_UI_TEXTURE_MASK, playing_accent_color);
		drawer.TextToolTip("Stop", transform.position, transform.scale, &base_tool_tip);

		// for the step button, must reestablish the scale
		scaled_position = ExpandRectangle(transform.position, transform.scale, { triangle_scale_factor, triangle_scale_factor }, scaled_scale);
	}
	else {
		scaled_position = ExpandRectangle(transform.position, transform.scale, { triangle_scale_factor, triangle_scale_factor }, scaled_scale);
		drawer.SpriteRectangle(configuration, scaled_position, scaled_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, playing_accent_color, { 1.0f, 0.0f }, { 0.0f, 1.0f });
		drawer.TextToolTip("Play", transform.position, transform.scale, &base_tool_tip);
	}
	
	float2 action_scale = { transform.scale.x - border_size_horizontal, transform.scale.y };
	drawer.AddDefaultClickableHoverable(0, transform.position, action_scale, { RunProjectAction, editor_state, 0 }, playing_theme_color);
	
	config.flag_count--;
	transform.position.x += button_scale.x;
	config.AddFlag(transform);

	bool is_paused = EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED);
	Color paused_theme_color = is_paused ? accent_color : theme_color;
	Color paused_accent_color = is_paused ? theme_color : accent_color;
	if (!is_playing) {
		paused_accent_color = drawer.color_theme.unavailable_text;
	}

	// Pause button - two bars as mask textures
	drawer.SolidColorRectangle(configuration, config, paused_theme_color);

	float bar_scale_x = button_scale.x / 8;
	const float bar_scale_y = button_scale_y * 0.55f;

	float2 bar_scale = { bar_scale_x, bar_scale_y };
	float2 bar_position = { AlignMiddle(transform.position.x, button_scale.x, bar_scale_x * 3), AlignMiddle(transform.position.y, button_scale.y, bar_scale_y) };
	drawer.SpriteRectangle(configuration, bar_position, bar_scale, ECS_TOOLS_UI_TEXTURE_MASK, paused_accent_color);
	drawer.SpriteRectangle(configuration, { bar_position.x + bar_scale.x * 2, bar_position.y }, bar_scale, ECS_TOOLS_UI_TEXTURE_MASK, paused_accent_color);
	if (is_playing) {
		drawer.AddDefaultClickableHoverable(0, transform.position, action_scale, { PauseProjectAction, editor_state, 0 }, paused_theme_color);
		drawer.TextToolTip("Pause", transform.position, transform.scale, &base_tool_tip);
	}

	transform.position.x += transform.scale.x;
	config.flag_count--;
	config.AddFlag(transform);

	// Frame button - triangle and bar
	drawer.SolidColorRectangle(configuration, config, drawer.color_theme.theme);
	float2 triangle_position = { scaled_position.x + transform.scale.x * 1.85f, scaled_position.y };

	Color frame_color = drawer.color_theme.unavailable_text;
	if (EditorStateHasFlag(editor_state, EDITOR_STATE_IS_PAUSED)) {
		frame_color = EDITOR_GREEN_COLOR;
		drawer.AddDefaultClickableHoverable(0, transform.position, action_scale, { StepProjectAction, editor_state, 0 });
		drawer.TextToolTip("Frame", transform.position, transform.scale, &base_tool_tip);
	}
	drawer.SpriteRectangle(configuration, triangle_position, scaled_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, frame_color, { 1.0f, 0.0f }, { 0.0f, 1.0f });
	drawer.SpriteRectangle(configuration, { triangle_position.x + scaled_scale.x, bar_position.y }, bar_scale, ECS_TOOLS_UI_TEXTURE_MASK, frame_color);

#pragma endregion
}

void MiscellaneousBarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	descriptor.draw = MiscellaneousBarDraw;

	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
	descriptor.window_name = MISCELLANEOUS_BAR_WINDOW_NAME;
}

void CreateMiscellaneousBar(EditorState* editor_state) {
	UISystem* ui_system = editor_state->ui_system;
	UIWindowDescriptor descriptor;

	descriptor.initial_position_y = -1.0f + TOOLBAR_SIZE_Y;
	descriptor.initial_position_x = -1.0f;
	descriptor.initial_size_x = 2.0f;
	descriptor.initial_size_y = MISCELLANEOUS_BAR_SIZE_Y;

	MiscellaneousBarSetDescriptor(descriptor, editor_state, nullptr);

	ui_system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_BORDER_NOTHING | UI_DOCKSPACE_FIXED 
		| UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_BACKGROUND);
}