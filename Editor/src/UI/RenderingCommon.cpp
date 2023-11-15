#include "editorpch.h"
#include "RenderingCommon.h"
#include "HelperWindows.h"
#include "../Sandbox/SandboxModule.h"
#include "../Sandbox/Sandbox.h"
#include "../Modules/Module.h"
#include "../Editor/EditorPalette.h"

// ------------------------------------------------------------------------------------------------------------

void DisplaySandboxTexture(EditorState* editor_state, UIDrawer& drawer, EDITOR_SANDBOX_VIEWPORT viewport, unsigned int sandbox_index)
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

	if (RenderSandboxIsPending(editor_state, sandbox_index, viewport)) {
		drawer.SpriteRectangle(
			UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE,
			config,
			ECS_TOOLS_UI_TEXTURE_MASK,
			Color((unsigned char)0, 0, 0, 100)
		);
	}
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

		// At the moment the background is disabled
		UIConfigSpriteButtonBackground background;
		background.overwrite_color = drawer.color_theme.theme;
		config.AddFlag(background);


		drawer.SpriteButton(
			UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION
			| UI_CONFIG_LATE_DRAW,
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

static void DisplayBottomText(UIDrawer& drawer, Stream<char> message, Color text_color, float font_size) {
	UIDrawerRowLayout row = drawer.GenerateRowLayout();
	row.font_scaling = font_size;

	row.SetVerticalAlignment(ECS_UI_ALIGN_BOTTOM);
	row.SetHorizontalAlignment(ECS_UI_ALIGN_MIDDLE);
	row.AddLabel(message);

	UIConfigTextParameters text_parameters;
	text_parameters.color = text_color;
	text_parameters.size *= float2::Splat(font_size);
	text_parameters.character_spacing *= font_size;

	UIDrawConfig crash_config;
	size_t crash_configuration = UI_CONFIG_TEXT_PARAMETERS;
	crash_config.AddFlag(text_parameters);

	row.GetTransform(crash_config, crash_configuration);

	drawer.Text(crash_configuration, crash_config, message);
}

void DisplayCrashedSandbox(UIDrawer& drawer, const EditorState* editor_state, unsigned int sandbox_index)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (sandbox->is_crashed) {
		DisplayBottomText(drawer, "Crashed", EDITOR_RED_COLOR, 3.0f);
	}
}

// ------------------------------------------------------------------------------------------------------------

void DisplayCompilingSandbox(UIDrawer& drawer, const EditorState* editor_state, unsigned int sandbox_index)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION)) {
		DisplayBottomText(drawer, "Compiling", EDITOR_YELLOW_COLOR, 3.0f);
	}
}

// ------------------------------------------------------------------------------------------------------------

void DisplaySandboxStatistics(UIDrawer& drawer, const EditorState* editor_state, unsigned int sandbox_index)
{
	const float font_scaling = 1.0f;
	drawer.element_descriptor.label_padd = float2::Splat(0.0f);

	// We need to keep these values the same for a small period of time otherwise
	// If we update every single draw, the values cannot be read
	struct DisplayValues {
		unsigned char cpu_utilization;
		unsigned char gpu_usage;
		size_t ram_usage;
		size_t vram_usage;
		float simulation_fps;
		float simulation_ms;
		float overall_fps;
		float overall_ms;
		float gpu_fps;
		float gpu_ms;
		Timer timer;
	};

	const char* RESOURCE_NAME = "__DISPLAY_VALUES";
	// In milliseconds
	const size_t DISPLAY_VALUES_UPDATE_TICK_MS = 200;
	DisplayValues* display_values = nullptr;
	if (drawer.initializer) {
		// Insert a structure to hold the display values for a small amount of time
		display_values = drawer.GetMainAllocatorBufferAndStoreAsResource<DisplayValues>(RESOURCE_NAME);
		display_values->timer.SetUninitialized();
	}
	else {
		display_values = (DisplayValues*)drawer.GetResource(RESOURCE_NAME);
	}

	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (sandbox->statistics_display.is_enabled) {
		ECS_STATISTIC_VALUE_TYPE statistic_value_type = ECS_STATISTIC_VALUE_SMALL_AVERAGE;
		UIDrawConfig config;
		size_t configuration = 0;

		// Verify if we need to update the values
		if (display_values->timer.GetDuration(ECS_TIMER_DURATION_MS) >= DISPLAY_VALUES_UPDATE_TICK_MS) {
			unsigned char cpu_usage = sandbox->cpu_frame_profiler.GetCPUUsage(-1, statistic_value_type);

			float simulation_ms = sandbox->cpu_frame_profiler.GetSimulationFrameTime(statistic_value_type);
			float simulation_fps = simulation_ms == 0.0f ? 0.0f : 1000.0f / simulation_ms;

			float overall_ms = sandbox->cpu_frame_profiler.GetOverallFrameTime(statistic_value_type);
			float overall_fps = overall_ms == 0.0f ? 0.0f : 1000.0f / overall_ms;

			display_values->cpu_utilization = cpu_usage;
			display_values->simulation_fps = simulation_fps;
			display_values->simulation_ms = simulation_ms;
			display_values->overall_ms = overall_ms;
			display_values->overall_fps = overall_fps;
			display_values->timer.SetNewStart();
		}

		UIConfigTextParameters text_parameters;
		text_parameters.size *= float2::Splat(font_scaling);
		text_parameters.character_spacing *= font_scaling;

		auto get_text_configuration = [&configuration]() {
			return configuration | UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_LABEL_TRANSPARENT;
		};

		auto draw_text_info = [&](Stream<char> base_label, Color base_color, auto get_value_display_string) {
			UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
			row_layout.font_scaling = font_scaling;
			row_layout.SetHorizontalAlignment(ECS_UI_ALIGN_RIGHT);

			ECS_STACK_CAPACITY_STREAM(char, value_label, 512);
			get_value_display_string(value_label);

			row_layout.AddLabel(base_label);
			row_layout.AddLabel(value_label);

			row_layout.GetTransform(config, configuration);
			text_parameters.color = base_color;
			config.AddFlag(text_parameters);

			drawer.TextLabel(get_text_configuration(), config, base_label);

			config.flag_count = 0;
			configuration = 0;
			row_layout.GetTransform(config, configuration);

			text_parameters.color = EDITOR_STATISTIC_TEXT_COLOR;
			config.AddFlag(text_parameters);
			drawer.TextLabel(get_text_configuration(), config, value_label);

			config.flag_count = 0;
			configuration = 0;
			drawer.NextRow();
		};

		auto draw_graph_info = [&](Stream<char> base_label, Color base_color, auto get_value_display_string) {

		};

		auto draw_entry = [&](EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY entry, Stream<char> base_label, Color base_color, auto get_value_display_string) {
			if (sandbox->statistics_display.should_display[entry]) {
				if (sandbox->statistics_display.display_form[entry] == EDITOR_SANDBOX_STATISTIC_DISPLAY_TEXT) {
					draw_text_info(base_label, base_color, get_value_display_string);
				}
				else {
					draw_graph_info(base_label, base_color, get_value_display_string);
				}
			}
		};

		draw_entry(EDITOR_SANDBOX_STATISTIC_CPU_USAGE, "CPU Usage", EDITOR_STATISTIC_CPU_USAGE_COLOR, [&](CapacityStream<char>& value_label) {
			ECS_FORMAT_STRING(value_label, "{#} %", display_values->cpu_utilization);
		});

		draw_entry(EDITOR_SANDBOX_STATISTIC_RAM_USAGE, "RAM Usage", EDITOR_STATISTIC_RAM_USAGE_COLOR, [&](CapacityStream<char>& value_label) {

		});

		/*draw_entry(EDITOR_SANDBOX_STATISTIC_CPU_USAGE, "GPU Usage", EDITOR_STATISTIC_GPU_USAGE_COLOR, [&](CapacityStream<char>& value_label) {

		});*/

		/*draw_entry(EDITOR_SANDBOX_STATISTIC_VRAM_USAGE, "VRAM Usage", EDITOR_STATISTIC_VRAM_USAGE_COLOR, [&](CapacityStream<char>& value_label) {

		});*/

		draw_entry(EDITOR_SANDBOX_STATISTIC_SANDBOX_TIME, "Framerate (Simulation)", EDITOR_STATISTIC_SANDBOX_TIME_COLOR, [&](CapacityStream<char>& value_label) {
			ECS_FORMAT_STRING(value_label, "{#} FPS ({#} ms)", display_values->simulation_fps, display_values->simulation_ms);
		});

		draw_entry(EDITOR_SANDBOX_STATISTIC_FRAME_TIME, "Framerate (Overall)", EDITOR_STATISTIC_FRAME_TIME_COLOR, [&](CapacityStream<char>& value_label) {
			ECS_FORMAT_STRING(value_label, "{#} FPS ({#} ms)", display_values->overall_fps, display_values->overall_ms);
		});

		/*draw_entry(EDITOR_SANDBOX_STATISTIC_GPU_SANDBOX_TIME, "Framerate (GPU)", EDITOR_STATISTIC_GPU_TIME_COLOR, [&](CapacityStream<char>& value_label) {

		});*/
		
	}
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

		RenderSandbox(editor_state, sandbox_index, viewport, new_size);
	}
}

// ------------------------------------------------------------------------------------------------------------