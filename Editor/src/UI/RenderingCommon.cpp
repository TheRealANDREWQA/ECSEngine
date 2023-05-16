#include "editorpch.h"
#include "RenderingCommon.h"
#include "HelperWindows.h"
#include "Editor/EditorSandbox.h"
#include "../Modules/Module.h"
#include "../Editor/EditorPalette.h"

// ------------------------------------------------------------------------------------------------------------

void DisplaySandboxTexture(const EditorState* editor_state, UIDrawer& drawer, EDITOR_SANDBOX_VIEWPORT viewport, unsigned int sandbox_index)
{
	sandbox_index = sandbox_index == -1 ? GetWindowNameIndex(drawer) : sandbox_index;
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	UIConfigAbsoluteTransform output_transform = drawer.GetWholeRegionTransform();
	UIDrawConfig config;
	config.AddFlag(output_transform);

	// Draw the output texture
	drawer.SpriteRectangle(
		UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE,
		config,
		sandbox->viewport_transferred_texture[viewport]
	);
}

// ------------------------------------------------------------------------------------------------------------

void DisplayGraphicsModuleRecompilationWarning(EditorState* editor_state, unsigned int sandbox_index, unsigned int in_sandbox_module_index, UIDrawer& drawer)
{
	// Determine if the graphics module is out of date to display a warning
	bool is_out_of_date = GetSandboxModuleInfo(editor_state, sandbox_index, in_sandbox_module_index)->load_status == EDITOR_MODULE_LOAD_OUT_OF_DATE;
	if (is_out_of_date) {
		UIDrawConfig config;
		float2 warning_scale = drawer.GetSquareScale(0.1f);

		// Display a warning in the bottom right corner
		float2 warning_position = drawer.GetCornerRectangle(ECS_UI_ALIGN_RIGHT, ECS_UI_ALIGN_BOTTOM, warning_scale);

		UIConfigAbsoluteTransform warning_transform;
		warning_transform.position = warning_position;
		warning_transform.scale = warning_scale;

		config.AddFlag(warning_transform);

		struct ClickableData {
			EditorState* editor_state;
			unsigned int module_index;
			EDITOR_MODULE_CONFIGURATION configuration;
		};

		auto warning_clickable = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ClickableData* data = (ClickableData*)_data;
			EDITOR_LAUNCH_BUILD_COMMAND_STATUS command_status = BuildModule(data->editor_state, data->module_index, data->configuration);
			PrintConsoleMessageForBuildCommand(data->editor_state, data->module_index, data->configuration, command_status);
		};

		const EditorSandboxModule* module_info = GetSandboxModule(editor_state, sandbox_index, in_sandbox_module_index);

		ClickableData clickable_data;
		clickable_data.editor_state = editor_state;
		clickable_data.module_index = module_info->module_index;
		clickable_data.configuration = module_info->module_configuration;

		UIConfigSpriteButtonBackground background;
		background.overwrite_color = drawer.color_theme.theme;
		config.AddFlag(background);
		drawer.SpriteButton(
			UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_SPRITE_BUTTON_BACKGROUND
			| UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND,
			config,
			{ warning_clickable, &clickable_data, sizeof(clickable_data) },
			ECS_TOOLS_UI_TEXTURE_WARN_ICON,
			EDITOR_YELLOW_COLOR
		);
	}
}

// ------------------------------------------------------------------------------------------------------------

void DisplayNoGraphicsModule(UIDrawer& drawer, bool multiple_graphics_modules)
{
	UIDrawConfig config;
	// Inform the user that there is no graphics module
	UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
	row_layout.SetHorizontalAlignment(ECS_UI_ALIGN_MIDDLE);
	row_layout.SetVerticalAlignment(ECS_UI_ALIGN_MIDDLE);

	Stream<char> message = multiple_graphics_modules ? "Multiple Graphics Modules Assigned" : "No Graphics Module Assigned";
	row_layout.AddLabel(message);

	size_t configuration = 0;
	row_layout.GetTransform(config, configuration);

	drawer.Text(configuration, config, message);
}

// ------------------------------------------------------------------------------------------------------------

void ResizeSandboxTextures(
	EditorState* editor_state, 
	const UIDrawer& drawer, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	ECSEngine::uint2* previous_size, 
	unsigned int threshold,
	unsigned int sandbox_index
)
{
	uint2 new_size = drawer.system->GetWindowTexelSize(drawer.window_index);
	uint2 difference = AbsoluteDifference(new_size, *previous_size);
	if (difference.x >= threshold || difference.y >= threshold) {
		*previous_size = new_size;
		sandbox_index = sandbox_index == -1 ? GetWindowNameIndex(drawer) : sandbox_index;

		ResizeSandboxRenderTextures(editor_state, sandbox_index, viewport, new_size);
		RenderSandbox(editor_state, sandbox_index, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------