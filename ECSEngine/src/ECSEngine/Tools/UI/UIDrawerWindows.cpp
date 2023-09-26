#include "ecspch.h"
#include "UIDrawerWindows.h"
#include "../../Multithreading/TaskManager.h"
#include "../../Utilities/OSFunctions.h"
#include "../../Utilities/File.h"
#include "UIDrawerActions.h"

namespace ECSEngine {

	namespace Tools {

		constexpr float2 CONSOLE_WINDOW_SIZE = { 1.0f, 0.4f };

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterReturnToDefaultButton(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIParameterWindowReturnToDefaultButtonData* data = (UIParameterWindowReturnToDefaultButtonData*)_data;
			if (data->is_system_theme) {
				memcpy(data->system_descriptor, data->default_descriptor, data->descriptor_size);
				if (data->descriptor_index < ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT) {
					switch (data->descriptor_index) {
					case ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COLOR_THEME:
						system->FinalizeColorTheme();
						break;
					case ECS_UI_WINDOW_DRAWER_DESCRIPTOR_LAYOUT:
						system->FinalizeLayout();
						break;
					case ECS_UI_WINDOW_DRAWER_DESCRIPTOR_ELEMENT:
						system->FinalizeElementDescriptor();
						break;
					}
				}
			}
			else {
				data->window_descriptor->configured[(unsigned int)data->descriptor_index] = false;
				void* copy_ptr[] = { &data->window_descriptor->color_theme, &data->window_descriptor->layout, &data->window_descriptor->font, &data->window_descriptor->element_descriptor };
				memcpy(copy_ptr[(unsigned int)data->descriptor_index], data->default_descriptor, data->descriptor_size);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterColorTheme(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer) {
			size_t input_configuration = UI_CONFIG_COLOR_INPUT_CALLBACK;

			UIDrawConfig config;
			UIConfigColorInputCallback color_input_callback;
			UIColorThemeDescriptor* theme = &descriptor->color_theme;
			color_input_callback.callback = { WindowParameterColorInputThemeCallback, descriptor, 0 };
			config.AddFlag(color_input_callback);

			auto system = drawer.GetSystem();
			const UIColorThemeDescriptor* system_theme = &system->m_descriptors.color_theme;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_descriptors.color_theme;
			button_data.descriptor_size = sizeof(UIColorThemeDescriptor);
			button_data.descriptor_index = ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COLOR_THEME;
			button_data.is_system_theme = false;
			button_data.window_descriptor = descriptor;
			drawer.Button("Default values##0", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.ColorInput(input_configuration, config, "Theme", &theme->theme);

			color_input_callback.callback = { WindowParameterColorInputCallback, descriptor, 0 };
			drawer.ColorInput(input_configuration, config, "Text", &theme->text);
			drawer.ColorInput(input_configuration, config, "Graph hover line", &theme->graph_hover_line);
			drawer.ColorInput(input_configuration, config, "Graph line", &theme->graph_line);
			drawer.ColorInput(input_configuration, config, "Graph sample circle", &theme->graph_sample_circle);
			drawer.ColorInput(input_configuration, config, "Histogram", &theme->histogram_color);
			drawer.ColorInput(input_configuration, config, "Histogram hovered", &theme->histogram_hovered_color);
			drawer.ColorInput(input_configuration, config, "Histogram text", &theme->histogram_text_color);
			drawer.ColorInput(input_configuration, config, "Unavailable text", &theme->unavailable_text);
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParametersLayout(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer) {
			size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;

			UIDrawConfig config;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { WindowParameterLayoutCallback, descriptor, 0 };
			config.AddFlag(callback);
			UISystem* system = drawer.GetSystem();

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_descriptors.window_layout;
			button_data.descriptor_size = sizeof(UILayoutDescriptor);
			button_data.descriptor_index = ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_LAYOUT;
			button_data.is_system_theme = false;
			button_data.window_descriptor = descriptor;
			drawer.Button("Default values##1", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			UILayoutDescriptor* layout = &descriptor->layout;
			const UILayoutDescriptor* system_layout = &system->m_descriptors.window_layout;
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Element x", &layout->default_element_x, 0.01f, 0.3f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Element y", &layout->default_element_y, 0.01f, 0.15f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Indentation", &layout->element_indentation, 0.0f, 0.05f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Next row padding", &layout->next_row_padding, 0.0f, 0.05f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Next row offset", &layout->next_row_y_offset, 0.0f, 0.05f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Node indentation", &layout->node_indentation, 0.0f, 0.05f, 3);

		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParametersElementDescriptor(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer) {
			size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;

			UIDrawConfig config;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { WindowParameterElementDescriptorCallback, descriptor, 0 };
			config.AddFlag(callback);
			auto system = drawer.GetSystem();

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_descriptors.element_descriptor;
			button_data.descriptor_size = sizeof(UIElementDescriptor);
			button_data.descriptor_index = ECS_UI_WINDOW_DRAWER_DESCRIPTOR_ELEMENT;
			button_data.is_system_theme = false;
			button_data.window_descriptor = descriptor;
			drawer.Button("Default values##2", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			UIElementDescriptor* elements = &descriptor->element_descriptor;
			UIElementDescriptor* system_elements = &system->m_descriptors.element_descriptor;
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Color input padd", &elements->color_input_padd, 0.0f, 0.01f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Combo box padd", &elements->combo_box_padding, 0.0f, 0.02f, 3);
			float* float2_values[2];
			float float2_lower_bounds[1];
			float float2_upper_bounds[1];
			Stream<char> float2_names[2];

			auto float2_sliders = [&](Stream<char> group_name, size_t index) {
				drawer.FloatSliderGroup(
					SLIDER_CONFIGURATION | UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS,
					config,
					2,
					group_name,
					float2_names,
					float2_values,
					float2_lower_bounds,
					float2_upper_bounds
				);
			};

			float2_values[0] = &elements->graph_axis_bump.x;
			float2_values[1] = &elements->graph_axis_bump.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.01f;
			float2_names[0] = "x:";
			float2_names[1] = "y:";
			float2_sliders("Graph axis bump", 0);

			float2_values[0] = &elements->graph_axis_value_line_size.x;
			float2_values[1] = &elements->graph_axis_value_line_size.y;
			float2_upper_bounds[0] = 0.02f;
			float2_sliders("Graph axis value line size", 1);

			float2_values[0] = &elements->graph_padding.x;
			float2_values[1] = &elements->graph_padding.y;
			float2_upper_bounds[0] = 0.02f;
			float2_sliders("Graph padding", 2);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Graph reduce font",
				&elements->graph_reduce_font,
				0.5f,
				1.0f,
				3
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Graph sample circle size",
				&elements->graph_sample_circle_size,
				0.003f,
				0.02f,
				3
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Graph x axis space",
				&elements->graph_x_axis_space,
				0.005f,
				0.05f,
				4
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Histogram bar min scale",
				&elements->histogram_bar_min_scale,
				0.01f,
				0.2f
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Histogram bar spacing",
				&elements->histogram_bar_spacing,
				0.0028f,
				0.01f,
				3
			);

			float2_values[0] = &elements->histogram_padding.x;
			float2_values[1] = &elements->histogram_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.1f;
			float2_sliders("Histogram padding", 3);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Histogram reduce font",
				&elements->histogram_reduce_font,
				0.5f,
				1.0f,
				3
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Menu button padding",
				&elements->menu_button_padding,
				0.0f,
				0.1f,
				4
			);

			float2_values[0] = &elements->label_padd.x;
			float2_values[1] = &elements->label_padd.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.2f;
			float2_sliders("Label padding", 4);

			float2_values[0] = &elements->slider_length.x;
			float2_values[1] = &elements->slider_length.y;
			float2_lower_bounds[0] = 0.01f;
			float2_upper_bounds[0] = 0.1f;
			float2_sliders("Slider length", 5);

			float2_values[0] = &elements->slider_shrink.x;
			float2_values[1] = &elements->slider_shrink.y;
			float2_sliders("Slider shrink", 7);
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			UIWindowDrawerDescriptor* descriptors = (UIWindowDrawerDescriptor*)window_data;
			drawer.SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);
			auto color_theme_lambda = [&]() {
				WindowParameterColorTheme(descriptors, drawer);
			};
			drawer.CollapsingHeader("Color theme", nullptr, color_theme_lambda);

			auto layout_lambda = [&]() {
				WindowParametersLayout(descriptors, drawer);
			};

			drawer.CollapsingHeader("Layout", nullptr, layout_lambda);

			auto element_lambda = [&]() {
				WindowParametersElementDescriptor(descriptors, drawer);
			};

			drawer.CollapsingHeader("Element descriptor", nullptr, element_lambda);
		}

		// --------------------------------------------------------------------------------------------------------------

		void OpenWindowParameters(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			UIWindowDescriptor descriptor;
			descriptor.draw = WindowParameterDraw;
			descriptor.private_action = SkipAction;
			descriptor.window_data = system->m_windows[window_index].descriptors;
			descriptor.window_data_size = 0;
			descriptor.initial_position_x = mouse_position.x - 0.1f;
			descriptor.initial_position_y = mouse_position.y - 0.1f;

			descriptor.initial_position_x = function::ClampMin(descriptor.initial_position_x, -1.0f);
			descriptor.initial_position_y = function::ClampMin(descriptor.initial_position_y, -1.0f);

			descriptor.initial_size_x = ECS_TOOLS_UI_WINDOW_PARAMETER_INITIAL_SIZE_X;
			descriptor.initial_size_y = ECS_TOOLS_UI_WINDOW_PARAMETER_INITIAL_SIZE_Y;

			descriptor.initial_position_x = function::ClampMax(descriptor.initial_position_x + descriptor.initial_size_x, 1.0f) - descriptor.initial_size_x;
			descriptor.initial_position_y = function::ClampMax(descriptor.initial_position_y + descriptor.initial_size_y, 1.0f) - descriptor.initial_size_y;

			Stream<char> window_name = system->GetWindowName(window_index);
			char* new_name = (char*)system->m_memory->Allocate(sizeof(char) * 64, alignof(char));
			descriptor.window_name.buffer = new_name;

			window_name.CopyTo(new_name);
			new_name += window_name.size;
			memcpy(new_name, " Parameters", sizeof(" Parameters") - 1);

			descriptor.window_name.size = window_name.size + sizeof(" Parameters") - 1;
	
			window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW);

			// if it is desired to be destroyed when going out of focus
			UIPopUpWindowData system_handler_data;
			system_handler_data.is_fixed = true;
			system_handler_data.is_initialized = true;
			system_handler_data.destroy_at_first_click = false;
			system_handler_data.name = descriptor.window_name;
			system_handler_data.reset_when_window_is_destroyed = true;
			system_handler_data.deallocate_name = true;
			UIActionHandler handler;
			handler.action = PopUpWindowSystemHandler;
			handler.data = &system_handler_data;
			handler.data_size = sizeof(system_handler_data);
			system->PushFrameHandler(handler);
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParametersColorTheme(UIDrawer& drawer) {
			const size_t input_configuration = UI_CONFIG_COLOR_INPUT_CALLBACK | UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE;
			const size_t slider_configuration = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;

			UIDrawConfig config;
			UIConfigColorInputCallback color_input_callback;

			auto system = drawer.GetSystem();
			UIColorThemeDescriptor* theme = &system->m_descriptors.color_theme;
			color_input_callback.callback = { SystemParameterColorInputThemeCallback, nullptr, 0 };
			config.AddFlag(color_input_callback);

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.color_theme;
			button_data.descriptor_size = sizeof(UIColorThemeDescriptor);
			button_data.descriptor_index = ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COLOR_THEME;
			button_data.is_system_theme = true;
			button_data.system_descriptor = theme;
			drawer.Button("Default values##0", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			const UIColorThemeDescriptor* startup_theme = &system->m_startup_descriptors.color_theme;
			drawer.ColorInput(input_configuration, config, "Theme", &theme->theme, theme->theme);

			color_input_callback.callback = { SystemParameterColorThemeCallback, nullptr, 0 };
			drawer.ColorInput(input_configuration, config, "Text", &theme->text, theme->text);
			drawer.ColorInput(input_configuration, config, "Graph hover line", &theme->graph_hover_line, theme->graph_hover_line);
			drawer.ColorInput(input_configuration, config, "Graph line", &theme->graph_line, theme->graph_line);
			drawer.ColorInput(input_configuration, config, "Graph sample circle", &theme->graph_sample_circle, theme->graph_sample_circle);
			drawer.ColorInput(input_configuration, config, "Histogram", &theme->histogram_color, theme->histogram_color);
			drawer.ColorInput(input_configuration, config, "Histogram hovered", &theme->histogram_hovered_color, theme->histogram_hovered_color);
			drawer.ColorInput(input_configuration, config, "Histogram text", &theme->histogram_text_color, theme->histogram_text_color);
			drawer.ColorInput(input_configuration, config, "Unavailable text", &theme->unavailable_text, theme->unavailable_text);
			drawer.ColorInput(input_configuration, config, "Background", &theme->background, theme->background);
			drawer.ColorInput(input_configuration, config, "Borders", &theme->borders, theme->borders);
			drawer.ColorInput(input_configuration, config, "Collapse Triangle", &theme->collapse_triangle, theme->collapse_triangle);
			drawer.ColorInput(input_configuration, config, "Docking gizmo background", &theme->docking_gizmo_background, theme->docking_gizmo_background);
			drawer.ColorInput(input_configuration, config, "Docking gizmo border", &theme->docking_gizmo_border, theme->docking_gizmo_border);
			drawer.ColorInput(input_configuration, config, "Hierarchy drag node bar", &theme->hierarchy_drag_node_bar, theme->hierarchy_drag_node_bar);
			drawer.ColorInput(input_configuration, config, "Render sliders active part", &theme->render_sliders_active_part, theme->render_sliders_active_part);
			drawer.ColorInput(input_configuration, config, "Render sliders background", &theme->render_sliders_background, theme->render_sliders_background);
			drawer.ColorInput(input_configuration, config, "Region header X", &theme->region_header_x, theme->region_header_x);
			drawer.ColorInput(input_configuration, config, "Region header hovered X", &theme->region_header_hover_x, theme->region_header_hover_x);

			config.flag_count = 0;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { SystemParameterColorThemeCallback, nullptr, 0 };
			config.AddFlag(callback);
			drawer.FloatSlider(slider_configuration, config, "Check box factor", &theme->check_box_factor, 1.2f, 2.0f, 3);
			drawer.FloatSlider(slider_configuration, config, "Select text factor", &theme->select_text_factor, 1.1f, 2.0f, 3);
			drawer.FloatSlider(slider_configuration, config, "Darken hover factor", &theme->darken_hover_factor, 0.2f, 0.9f, 3);
			drawer.FloatSlider(slider_configuration, config, "Slider lighten factor", &theme->slider_lighten_factor, 1.1f, 2.5f, 3);
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParametersLayout(UIDrawer& drawer) {
			const size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;

			UIDrawConfig config;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { SystemParameterLayoutCallback, nullptr, 0 };
			config.AddFlag(callback);
			auto system = drawer.GetSystem();

			UILayoutDescriptor* layout = &system->m_descriptors.window_layout;
			const UILayoutDescriptor* system_layout = &system->m_startup_descriptors.window_layout;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.window_layout;
			button_data.descriptor_size = sizeof(UILayoutDescriptor);
			button_data.descriptor_index = ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_LAYOUT;
			button_data.is_system_theme = true;
			button_data.system_descriptor = layout;
			drawer.Button("Default values##1", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Element x", &layout->default_element_x, 0.01f, 0.3f, system_layout->default_element_x);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Element y", &layout->default_element_y, 0.01f, 0.15f, system_layout->default_element_y);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Indentation", &layout->element_indentation, 0.0f, 0.05f, system_layout->element_indentation);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Next row padding", &layout->next_row_padding, 0.0f, 0.05f, system_layout->next_row_padding);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Next row offset", &layout->next_row_y_offset, 0.0f, 0.05f, system_layout->next_row_y_offset);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Node indentation", &layout->node_indentation, 0.0f, 0.05f, system_layout->node_indentation);
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParametersElementDescriptor(UIDrawer& drawer) {
			const size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;

			UIDrawConfig config;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { SystemParameterElementDescriptorCallback, nullptr, 0 };
			config.AddFlag(callback);
			auto system = drawer.GetSystem();

			UIElementDescriptor* elements = &system->m_descriptors.element_descriptor;
			UIElementDescriptor* system_elements = &system->m_startup_descriptors.element_descriptor;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.element_descriptor;
			button_data.descriptor_size = sizeof(UIElementDescriptor);
			button_data.descriptor_index = ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_ELEMENT;
			button_data.is_system_theme = true;
			button_data.system_descriptor = elements;
			drawer.Button("Default values##20", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.PushIdentifierStack(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Color input padd", &elements->color_input_padd, 0.0f, 0.01f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Combo box padd", &elements->combo_box_padding, 0.0f, 0.02f, 3);
			float* float2_values[2];
			// Uniform bounds
			float float2_lower_bounds[1];
			// Uniform bounds
			float float2_upper_bounds[1];
			Stream<char> float2_names[2];

			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.01f;

			auto float2_sliders = [&](Stream<char> group_name) {
				drawer.FloatSliderGroup(
					SLIDER_CONFIGURATION | UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS,
					config,
					2,
					group_name,
					float2_names,
					float2_values,
					float2_lower_bounds,
					float2_upper_bounds
				);
			};

			float2_values[0] = &elements->graph_axis_bump.x;
			float2_values[1] = &elements->graph_axis_bump.y;
			float2_names[0] = "x:";
			float2_names[1] = "y:";
			float2_sliders("Graph axis bump");

			float2_values[0] = &elements->graph_axis_value_line_size.x;
			float2_values[1] = &elements->graph_axis_value_line_size.y;
			float2_upper_bounds[0] = 0.02f;
			float2_sliders("Graph axis value line size");

			float2_values[0] = &elements->graph_padding.x;
			float2_values[1] = &elements->graph_padding.y;
			float2_upper_bounds[0] = 0.02f;
			float2_sliders("Graph padding");

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Graph reduce font",
				&elements->graph_reduce_font,
				0.5f,
				1.0f
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Graph sample circle size",
				&elements->graph_sample_circle_size,
				0.003f,
				0.02f
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Graph x axis space",
				&elements->graph_x_axis_space,
				0.005f,
				0.05f,
				4
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Histogram bar min scale",
				&elements->histogram_bar_min_scale,
				0.01f,
				0.2f
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Histogram bar spacing",
				&elements->histogram_bar_spacing,
				0.0028f,
				0.01f
			);

			float2_values[0] = &elements->histogram_padding.x;
			float2_values[1] = &elements->histogram_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.1f;
			float2_sliders("Histogram padding");

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Histogram reduce font",
				&elements->histogram_reduce_font,
				0.5f,
				1.0f
			);

			drawer.FloatSlider(
				SLIDER_CONFIGURATION,
				config,
				"Menu button padding",
				&elements->menu_button_padding,
				0.0f,
				0.1f,
				4
			);

			float2_values[0] = &elements->label_padd.x;
			float2_values[1] = &elements->label_padd.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.2f;
			float2_sliders("Label padding");

			float2_values[0] = &elements->slider_length.x;
			float2_values[1] = &elements->slider_length.y;
			float2_lower_bounds[0] = 0.01f;
			float2_upper_bounds[0] = 0.1f;
			float2_sliders("Slider length");

			float2_lower_bounds[0] = 0.0005f;
			float2_upper_bounds[0] = 0.005f;
			float2_values[0] = &elements->slider_shrink.x;
			float2_values[1] = &elements->slider_shrink.y;
			float2_sliders("Slider shrink");

			drawer.PopIdentifierStack();
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterFont(UIDrawer& drawer) {
			const size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES;

			UIDrawConfig config;
			auto system = drawer.GetSystem();

			UIFontDescriptor* font = &system->m_descriptors.font;
			const UIFontDescriptor* startup_font = &system->m_startup_descriptors.font;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.font;
			button_data.descriptor_size = sizeof(UIFontDescriptor);
			button_data.descriptor_index = ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_FONT;
			button_data.is_system_theme = true;
			button_data.system_descriptor = font;
			drawer.Button("Default values##32", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Character spacing", &font->character_spacing, 0.0f, 0.1f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Size", &font->size, 0.0007f, 0.003f, 5);
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterDockspace(UIDrawer& drawer) {
			const size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES;
			UIDrawConfig config;
			auto system = drawer.GetSystem();

			UIDockspaceDescriptor* dockspace = &system->m_descriptors.dockspaces;
			const UIDockspaceDescriptor* startup_dockspace = &system->m_startup_descriptors.dockspaces;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.dockspaces;
			button_data.descriptor_size = sizeof(UIDockspaceDescriptor);
			button_data.descriptor_index = (ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX)10;
			button_data.is_system_theme = true;
			button_data.system_descriptor = dockspace;
			drawer.Button("Default values##41", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.IntSlider(SLIDER_CONFIGURATION, config, "Border clickable left handler count", &dockspace->border_default_left_clickable_handler_count, (unsigned int)64, (unsigned int)1024);
			drawer.IntSlider(SLIDER_CONFIGURATION, config, "Border clickable misc handler count", &dockspace->border_default_misc_clickable_handler_count, (unsigned int)16, (unsigned int)1024);
			drawer.IntSlider(SLIDER_CONFIGURATION, config, "Border general handler count", &dockspace->border_default_general_handler_count, (unsigned int)16, (unsigned int)1024);
			drawer.IntSlider(SLIDER_CONFIGURATION, config, "Border hoverable handler count", &dockspace->border_default_hoverable_handler_count, (unsigned int)64, (unsigned int)1024);
			drawer.IntSlider(SLIDER_CONFIGURATION, config, "Border sprite texture count", &dockspace->border_default_sprite_texture_count, (unsigned int)16, (unsigned int)1024);
			drawer.IntSlider(SLIDER_CONFIGURATION, config, "Dockspace count", &dockspace->count, (unsigned int)8, (unsigned int)64);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Max border count", &dockspace->max_border_count, 4, 16);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Max windows per border", &dockspace->max_windows_border, 4, 16);

			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Border margin", &dockspace->border_margin, 0.0f, 0.5f, 0.0f, 4);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Border minimum distance", &dockspace->border_minimum_distance, 0.0f, 0.5f, 0.0f, 5);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Border size", &dockspace->border_size, 0.0005f, 0.01f, 0.0f, 5);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Mininum scale", &dockspace->mininum_scale, 0.01f, 0.5f, 0.0f, 5);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Viewport padding x", &dockspace->viewport_padding_x, 0.0f, 0.003f, 0.0f, 5);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Viewport padding y", &dockspace->viewport_padding_y, 0.0f, 0.003f, 0.0f, 5);
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterMaterial(UIDrawer& drawer) {
			const size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES;

			auto system = drawer.GetSystem();

			UIMaterialDescriptor* material = &system->m_descriptors.materials;
			const UIMaterialDescriptor* startup_material = &system->m_descriptors.materials;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.materials;
			button_data.descriptor_size = sizeof(UIMaterialDescriptor);
			button_data.descriptor_index = (ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX)10;
			button_data.is_system_theme = true;
			button_data.system_descriptor = material;
			drawer.Button("Default values##53", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			const char* names[] = {
				"Solid color count",
				"Text sprites count",
				"Sprites count",
				"Sprite cluster count",
				"Line count"
			};

			drawer.CollapsingHeader("Vertex buffer normal phase", nullptr, [&]() {
				UIDrawConfig config;
				drawer.PushIdentifierStack("##1");
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, names[index], &material->vertex_buffer_count[index], 256, 5'000'000);
				}
				drawer.PopIdentifierStack();
				});

			drawer.CollapsingHeader("Vertex buffer late phase", nullptr, [&]() {
				UIDrawConfig config;
				drawer.PushIdentifierStack("##2");
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, names[index], &material->vertex_buffer_count[ECS_TOOLS_UI_MATERIALS + index], 256, 5'000'000);
				}
				drawer.PopIdentifierStack();
			});
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterMiscellaneous(UIDrawer& drawer) {
			const size_t SLIDER_CONFIGURATION = UI_CONFIG_SLIDER_ENTER_VALUES;

			UIDrawConfig config;
			auto system = drawer.GetSystem();

			UIMiscellaneousDescriptor* misc = &system->m_descriptors.misc;
			UIMiscellaneousDescriptor* startup_misc = &system->m_startup_descriptors.misc;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.misc;
			button_data.descriptor_size = sizeof(UIMiscellaneousDescriptor);
			button_data.descriptor_index = (ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX)10;
			button_data.is_system_theme = true;
			button_data.system_descriptor = misc;
			drawer.Button("Default values##100", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Drawer identifier memory", &misc->drawer_identifier_memory, 100, 1000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Drawer temp memory", &misc->drawer_temp_memory, 1'000, 1'000'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Hierarchy drag node hover time until drop", &misc->hierarchy_drag_node_hover_drop, 250, 5000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Hierarchy drag node time", &misc->hierarchy_drag_node_time, 100, 2'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Slider bring back start time", &misc->slider_bring_back_start, 50, 2'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Slider enter values duration", &misc->slider_enter_value_duration, 100, 2'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Text input caret display time", &misc->text_input_caret_display_time, 25, 5'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Text input coalesce command time", &misc->text_input_coalesce_command, 25, 1'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Text input start time", &misc->text_input_repeat_start_duration, 25, 1'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Text input repeat time", &misc->text_input_repeat_time, 25, 1'000);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Thread temp memory", &misc->thread_temp_memory, 128, 1'000'000);
			drawer.IntSlider<unsigned short>(SLIDER_CONFIGURATION, config, "Window count", &misc->window_count, 8, 64);
			drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, "Window handler revert command count", &misc->window_handler_revert_command_count, 32, 4096);
			drawer.IntSlider<unsigned short>(SLIDER_CONFIGURATION, config, "Window table default resource count", &misc->window_table_default_count, 64, 1024);

			const char* names[] = {
				"Solid color",
				"Text sprite",
				"Sprite",
				"Sprite cluster",
				"Line"
			};

			drawer.CollapsingHeader("System vertex buffer count", nullptr, [&]() {
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					drawer.IntSlider<unsigned int>(SLIDER_CONFIGURATION, config, names[index], &misc->system_vertex_buffers[index], 128, 5'000'000);
				}
			});

			float* float2_values[2];
			float float2_lower_bounds[2];
			float float2_upper_bounds[2];
			Stream<char> float2_names[2] = { "x:", "y:" };

			drawer.PushIdentifierStack(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);

			auto float2_lambda = [&](Stream<char> group_name, size_t index) {
				drawer.PushIdentifierStackRandom(index);
				drawer.FloatSliderGroup(
					SLIDER_CONFIGURATION | UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS,
					config,
					2,
					group_name,
					float2_names,
					float2_values,
					float2_lower_bounds,
					float2_upper_bounds
				);
				drawer.PopIdentifierStack();
			};

			float2_values[0] = &misc->color_input_window_size_x;
			float2_values[1] = &misc->color_input_window_size_y;
			float2_lower_bounds[0] = 0.1f;
			float2_upper_bounds[0] = 1.0f;
			float2_lambda("Color input window size", 10);

			float2_values[0] = &misc->graph_hover_offset.x;
			float2_values[1] = &misc->graph_hover_offset.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.2f;
			float2_lambda("Graph hover offset", 11);

			float2_values[0] = &misc->histogram_hover_offset.x;
			float2_values[1] = &misc->histogram_hover_offset.y;
			float2_lambda("Histogram hover offset", 12);

			float2_values[0] = &misc->render_slider_horizontal_size;
			float2_values[1] = &misc->render_slider_vertical_size;
			float2_lower_bounds[0] = 0.005f;
			float2_upper_bounds[1] = 0.05f;
			float2_lambda("Render slider size", 13);

			float2_values[0] = &misc->tool_tip_padding.x;
			float2_values[1] = &misc->tool_tip_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.1f;
			float2_lambda("Tool tip padding", 14);

			drawer.PopIdentifierStack();

			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Menu x padd", &misc->menu_x_padd, 0.0f, 0.1f, 3);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Hierarchy drag node rectangle size", &misc->rectangle_hierarchy_drag_node_dimension, 0.005f, 0.01f, 4);
			drawer.FloatSlider(SLIDER_CONFIGURATION, config, "Title y scale", &misc->title_y_scale, 0.01f, 0.1f);

			drawer.ColorInput("Menu arrow color", &misc->menu_arrow_color);
			drawer.ColorInput("Menu unavailable arrow color", &misc->menu_unavailable_arrow_color);
			drawer.ColorInput("Tool tip background", &misc->tool_tip_background_color);
			drawer.ColorInput("Tool tip border", &misc->tool_tip_border_color);
			drawer.ColorInput("Tool tip font", &misc->tool_tip_font_color);
			drawer.ColorInput("Tool tip unavailable font", &misc->tool_tip_unavailable_font_color);
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParametersDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			UISystemDescriptors* descriptors = &drawer.GetSystem()->m_descriptors;
			drawer.SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

			auto color_theme_lambda = [&]() {
				SystemParametersColorTheme(drawer);
			};

			drawer.CollapsingHeader("Color theme", nullptr, color_theme_lambda);

			auto layout_lambda = [&]() {
				SystemParametersLayout(drawer);
			};

			drawer.CollapsingHeader("Layout", nullptr, layout_lambda);

			auto element_descriptor_lambda = [&]() {
				SystemParametersElementDescriptor(drawer);
			};

			drawer.CollapsingHeader("Element descriptor", nullptr, element_descriptor_lambda);

			auto material_lambda = [&]() {
				SystemParameterMaterial(drawer);
			};

			drawer.CollapsingHeader("Materials", nullptr, material_lambda);

			auto font_lambda = [&]() {
				SystemParameterFont(drawer);
			};

			drawer.CollapsingHeader("Font", nullptr, font_lambda);

			auto dockspace_lambda = [&]() {
				SystemParameterDockspace(drawer);
			};

			drawer.CollapsingHeader("Dockspace", nullptr, dockspace_lambda);

			auto misc_lambda = [&]() {
				SystemParameterMiscellaneous(drawer);
			};

			drawer.CollapsingHeader("Miscellaneous", nullptr, misc_lambda);
		}

		// --------------------------------------------------------------------------------------------------------------

		void OpenSystemParameters(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDescriptor descriptor;
			system->DestroyWindowIfFound("System Parameters");

			descriptor.draw = SystemParametersDraw;

			descriptor.resource_count = 512;
			descriptor.initial_position_x = mouse_position.x - 0.1f;
			descriptor.initial_position_y = mouse_position.y - 0.1f;

			descriptor.initial_position_x = function::ClampMin(descriptor.initial_position_x, -1.0f);
			descriptor.initial_position_y = function::ClampMin(descriptor.initial_position_y, -1.0f);

			descriptor.initial_size_x = ECS_TOOLS_UI_WINDOW_PARAMETER_INITIAL_SIZE_X;
			descriptor.initial_size_y = ECS_TOOLS_UI_WINDOW_PARAMETER_INITIAL_SIZE_Y;

			descriptor.initial_position_x = function::ClampMax(descriptor.initial_position_x + descriptor.initial_size_x, 1.0f) - descriptor.initial_size_x;
			descriptor.initial_position_y = function::ClampMax(descriptor.initial_position_y + descriptor.initial_size_y, 1.0f) - descriptor.initial_size_y;

			descriptor.window_name = "System Parameters";
			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW);

			void* dummy_allocation = system->m_memory->Allocate(1, 1);

			// if it is desired to be destroyed when going out of focus
			UIPopUpWindowData system_handler_data;
			system_handler_data.is_fixed = true;
			system_handler_data.is_initialized = true;
			system_handler_data.destroy_at_first_click = false;
			system_handler_data.flag_destruction = (bool*)dummy_allocation;
			system_handler_data.name = "System Parameters";
			system_handler_data.reset_when_window_is_destroyed = true;

			UIActionHandler handler;
			handler.action = PopUpWindowSystemHandler;
			handler.data = &system_handler_data;
			handler.data_size = sizeof(system_handler_data);
			system->PushFrameHandler(handler);
		}

		// --------------------------------------------------------------------------------------------------------------

		void CenterWindowDescriptor(UIWindowDescriptor& descriptor, float2 size)
		{
			descriptor.initial_size_x = size.x;
			descriptor.initial_size_y = size.y;
			descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, size.x);
			descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, size.y);
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowHandler(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDefaultWindowHandler* data = (UIDefaultWindowHandler*)_data;
			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);

			Stream<char> window_name = system->GetWindowName(window_index);
			ECS_STACK_CAPACITY_STREAM(char, new_name, 256);
			new_name.CopyOther(window_name);
			new_name.AddStream(" Parameters");

			unsigned int parameter_window_index = system->GetWindowFromName(new_name);
			data->is_parameter_window_opened = parameter_window_index == 0xFFFFFFFF ? false : true;

			int scroll_amount = mouse->GetScrollValue() - data->scroll;
			float total_scroll = 0.0f;
			if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
				if (keyboard->IsPressed(ECS_KEY_Z)) {
					if (system->GetActiveWindow() == window_index) {
						HandlerCommand command;
						bool continue_loop = true;
						while (continue_loop && data->revert_commands.Pop(command)) {
							// Verify that the command is still valid
							if (system->ExistsWindowMemoryResource(window_index, command.owning_pointer)) {
								action_data->data = command.handler.data;
								command.handler.action(action_data);
								system->m_memory->Deallocate(command.handler.data);
								system->RemoveWindowMemoryResource(window_index, command.handler.data);
								continue_loop = false;
							}
						}
					}
				}
				else if (IsPointInRectangle(mouse_position, position, scale) && data->last_frame == system->GetFrameIndex() - 1) {
					if (!mouse->IsPressed(ECS_MOUSE_RIGHT) && data->allow_zoom) {
						if (scroll_amount != 0.0f) {
							float2 before_zoom = system->m_windows[window_index].zoom;
							system->m_windows[window_index].zoom.x += scroll_amount * ECS_TOOLS_UI_DEFAULT_HANDLER_ZOOM_FACTOR;
							system->m_windows[window_index].zoom.y += scroll_amount * ECS_TOOLS_UI_DEFAULT_HANDLER_ZOOM_FACTOR;

							system->m_windows[window_index].zoom.x = function::Clamp(
								system->m_windows[window_index].zoom.x,
								system->m_windows[window_index].min_zoom,
								system->m_windows[window_index].max_zoom
							);

							system->m_windows[window_index].zoom.y = function::Clamp(
								system->m_windows[window_index].zoom.y,
								system->m_windows[window_index].min_zoom,
								system->m_windows[window_index].max_zoom
							);

							system->m_windows[window_index].descriptors->UpdateZoom(before_zoom, system->m_windows[window_index].zoom);
						}
					}
					else {
						float2 before_zoom = system->m_windows[window_index].zoom;
						system->m_windows[window_index].zoom.x = 1.0f;
						system->m_windows[window_index].zoom.y = 1.0f;
						system->m_windows[window_index].descriptors->UpdateZoom(before_zoom, system->m_windows[window_index].zoom);
					}
				}
			}
			else {
				if (data->last_frame == system->GetFrameIndex() - 1 && IsPointInRectangle(mouse_position, position, scale)) {
					float dimming_value = 1.0f;
					dimming_value = keyboard->IsDown(ECS_KEY_LEFT_SHIFT) ? 0.2f : 1.0f;
					dimming_value = keyboard->IsDown(ECS_KEY_RIGHT_SHIFT) ? 0.02f : dimming_value;

					if (scroll_amount != 0.0f) {
						if (system->m_windows[window_index].drawer_draw_difference.y < ECS_TOOLS_UI_DEFAULT_HANDLER_SCROLL_THRESHOLD) {
							total_scroll = ECS_TOOLS_UI_DEFAULT_HANDLER_SCROLL_STEP / system->m_windows[window_index].drawer_draw_difference.y * scroll_amount
								* dimming_value;
						}
						else {
							total_scroll = data->scroll_factor * ECS_TOOLS_UI_DEFAULT_HANDLER_SCROLL_FACTOR * scroll_amount * dimming_value;
						}
						UIDrawerSlider* vertical_slider = (UIDrawerSlider*)data->vertical_slider;
						vertical_slider->interpolate_value = true;
						vertical_slider->slider_position -= total_scroll;

						vertical_slider->slider_position = vertical_slider->slider_position > 1.0f ? 1.0f : vertical_slider->slider_position;
						vertical_slider->slider_position = vertical_slider->slider_position < 0.0f ? 0.0f : vertical_slider->slider_position;
					}
				}
			}
			data->scroll = mouse->GetScrollValue();
			data->last_frame = system->GetFrameIndex();

			if (keyboard->IsDown(ECS_KEY_LEFT_ALT) && mouse->IsPressed(ECS_MOUSE_LEFT) &&
				IsPointInRectangle(mouse_position, system->m_windows[window_index].transform) &&
				!data->is_parameter_window_opened && !data->is_this_parameter_window) {
				data->is_parameter_window_opened = true;
				OpenWindowParameters(action_data);
			}

			if (keyboard->IsDown(ECS_KEY_RIGHT_ALT) && mouse->IsPressed(ECS_MOUSE_LEFT)
				&& IsPointInRectangle(mouse_position, system->m_windows[window_index].transform)) {
				OpenSystemParameters(action_data);
			}

			if (system->m_windows[window_index].private_handler.action != nullptr) {
				action_data->data = system->m_windows[window_index].private_handler.data;
				action_data->additional_data = system->m_windows[window_index].window_data;
				system->m_windows[window_index].private_handler.action(action_data);
			}

			if (IsPointInRectangle(mouse_position, position, scale) && !system->m_execute_events) {
				system->m_application->ChangeCursor(data->commit_cursor);
			}
			data->commit_cursor = ECS_CURSOR_DEFAULT;
		}

		// --------------------------------------------------------------------------------------------------------------

		void DrawNothing(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize)
		{
			UI_PREPARE_DRAWER(initialize);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ErrorMessageWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			drawer.DisablePaddingForRenderSliders();
			char* text = (char*)window_data;
			drawer.Sentence(text);
			drawer.NextRow();

			UIDrawConfig config;
			UIConfigAbsoluteTransform transform;
			float2 label_size = drawer.GetLabelScale("OK");
			transform.position = drawer.GetAlignedToBottom(label_size.y);
			transform.position.x = drawer.GetAlignedToCenterX(label_size.x);
			transform.scale = label_size;

			config.AddFlag(transform);
			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "OK", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
		}

		// ------------------------------------------------------------------------------------

		unsigned int CreateErrorMessageWindow(UISystem* system, Stream<char> description)
		{
			UIWindowDescriptor descriptor;
			descriptor.draw = ErrorMessageWindowDraw;
			descriptor.initial_position_x = 0.0f;
			descriptor.initial_position_y = 0.0f;
			descriptor.initial_size_x = 10000.0f;
			descriptor.initial_size_y = 10000.0f;

			descriptor.window_data = system->m_memory->Allocate(sizeof(char) * description.size + 1);
			memcpy(descriptor.window_data, description.buffer, description.size);
			char* window_data_ptr = (char*)descriptor.window_data;
			window_data_ptr[description.size] = '\0';

			descriptor.window_data_size = 0;
			descriptor.window_name = ECS_TOOLS_UI_ERROR_MESSAGE_WINDOW_NAME;

			descriptor.destroy_action = ReleaseLockedWindow;

			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_LOCK_WINDOW
				| UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_DOCKSPACE_LOCK_WINDOW);
			system->AddWindowMemoryResource(descriptor.window_data, window_index);

			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = system->GetDockspaceFromWindow(window_index, border_index, type);
			system->SetPopUpWindowPosition(window_index, { AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.x), AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.y) });
			return window_index;
		}

		struct ConfirmWindowData {
			const char* sentence;
			UIActionHandler handler;
		};

		void ConfirmWindowOKAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			unsigned int window_index = system->GetWindowFromName(ECS_TOOLS_UI_CONFIRM_WINDOW_NAME);
			ConfirmWindowData* data = (ConfirmWindowData*)_data;
			action_data->data = data->handler.data;
			data->handler.action(action_data);

			// In case the window gets destroyed, e.g. an UI file is being loaded
			if (function::CompareStrings(system->m_windows[window_index].name, ECS_TOOLS_UI_CONFIRM_WINDOW_NAME)) {
				// When pressing enter, the dockspace and the border index will be missing
				// Set these here
				if (dockspace == nullptr) {
					unsigned int border_index;
					DockspaceType dockspace_type;
					dockspace = system->GetDockspaceFromWindow(window_index, border_index, dockspace_type);

					action_data->dockspace = dockspace;
					action_data->border_index = border_index;
					action_data->type = dockspace_type;
				}
				DestroyCurrentActionWindow(action_data);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void ConfirmWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			drawer.DisablePaddingForRenderSliders();
			ConfirmWindowData* data = (ConfirmWindowData*)window_data;
			drawer.Sentence(data->sentence);
			drawer.NextRow();

			UIDrawConfig config;
			UIConfigAbsoluteTransform transform;
			float2 label_size = drawer.GetLabelScale("OK");
			transform.position = drawer.GetAlignedToBottom(label_size.y);
			transform.scale = label_size;

			config.AddFlag(transform);
			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "OK", { ConfirmWindowOKAction, data, 0, ECS_UI_DRAW_SYSTEM });

			config.flag_count = 0;
			transform.scale = drawer.GetLabelScale("Cancel");
			transform.position.x = drawer.GetAlignedToRight(transform.scale.x).x;
			config.AddFlag(transform);
			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Cancel", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });

			// If enter is pressed, confirm the action
			if (drawer.system->m_keyboard->IsPressed(ECS_KEY_ENTER)) {
				auto system_handler_wrapper = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					ConfirmWindowOKAction(action_data);
					system->PopFrameHandler();
				};

				drawer.system->PushFrameHandler({ system_handler_wrapper, data, 0 });
			}
		}

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateConfirmWindow(UISystem* system, Stream<char> description, UIActionHandler handler)
		{
			UIWindowDescriptor descriptor;
			descriptor.draw = ConfirmWindowDraw;
			descriptor.initial_position_x = 0.0f;
			descriptor.initial_position_y = 0.0f;
			descriptor.initial_size_x = 10000.0f;
			descriptor.initial_size_y = 10000.0f;

			ConfirmWindowData data;
			char* sentence = (char*)system->m_memory->Allocate(sizeof(char) * description.size + 1);
			memcpy(sentence, description.buffer, description.size);
			sentence[description.size] = '\0';
			data.sentence = sentence;
			data.handler = handler;
			
			void* handler_memory = nullptr;
			if (handler.data_size > 0) {
				handler_memory = function::Copy(system->Allocator(), handler.data, handler.data_size);
				data.handler.data = handler_memory;
			}

			descriptor.window_data = &data;
			descriptor.window_data_size = sizeof(data);
			descriptor.window_name = ECS_TOOLS_UI_CONFIRM_WINDOW_NAME;

			descriptor.destroy_action = ReleaseLockedWindow;

			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL);
			system->AddWindowMemoryResource(sentence, window_index);
			if (handler_memory != nullptr) {
				system->AddWindowMemoryResource(handler_memory, window_index);
			}

			/*unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = system->GetDockspaceFromWindow(window_index, border_index, type);
			system->SetPopUpWindowPosition(window_index, { AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.x), AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.y) });*/
			return window_index;
		}

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateChooseOptionWindow(UISystem* system, ChooseOptionWindowData data)
		{
			UIWindowDescriptor descriptor;
			descriptor.draw = ChooseOptionWindowDraw;
			descriptor.initial_position_x = 0.0f;
			descriptor.initial_position_y = 0.0f;
			descriptor.initial_size_x = 10000.0f;
			descriptor.initial_size_y = 10000.0f;

			// coallesce allocation
			size_t total_size = sizeof(UIActionHandler) * data.handlers.size + data.description.size + 1 + sizeof(const char*) * data.handlers.size;
			size_t button_sizes[128];
			ECS_ASSERT(data.handlers.size < 128);

			for (size_t index = 0; index < data.handlers.size; index++) {
				button_sizes[index] = strlen(data.button_names[index]) + 1;
				total_size += button_sizes[index];
			}

			void* allocation = system->m_memory->Allocate(total_size);
			uintptr_t ptr = (uintptr_t)allocation;
			memcpy(allocation, data.handlers.buffer, data.handlers.size * sizeof(UIActionHandler));
			ptr += sizeof(UIActionHandler) * data.handlers.size;

			for (size_t index = 0; index < data.handlers.size; index++) {
				memcpy((void*)ptr, data.button_names[index], button_sizes[index] * sizeof(char));
				data.button_names[index] = (const char*)ptr;
				ptr += sizeof(char) * button_sizes[index];
			}
			data.handlers.buffer = (UIActionHandler*)allocation;

			memcpy((void*)ptr, data.button_names, sizeof(const char*) * data.handlers.size);
			ptr += sizeof(const char*) * data.handlers.size;

			data.description.CopyTo((void*)ptr);
			char* char_ptr = (char*)ptr;
			char_ptr[data.description.size] = '\0';
			data.description.buffer = char_ptr;

			descriptor.window_data = &data;
			descriptor.window_data_size = sizeof(data);
			descriptor.window_name =  data.window_name.size == 0 ? ECS_TOOLS_UI_CHOOSE_WINDOW_NAME : data.window_name;

			descriptor.destroy_action = ReleaseLockedWindow;

			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_LOCK_WINDOW
				| UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_DOCKSPACE_LOCK_WINDOW);
			system->PopUpFrameHandler(ECS_TOOLS_UI_CHOOSE_WINDOW_NAME, false);
			system->AddWindowMemoryResource(allocation, window_index);

			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = system->GetDockspaceFromWindow(window_index, border_index, type);
			system->SetPopUpWindowPosition(window_index, { AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.x), AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.y) });
			return window_index;
		}
		
		struct ChooseOptionActionData {
			ChooseOptionWindowData* data;
			unsigned int index;
		};

		void ChooseOptionAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			ChooseOptionActionData* data = (ChooseOptionActionData*)_data;
			action_data->data = data->data->handlers[data->index].data;
			data->data->handlers[data->index].action(action_data);

			if (function::CompareStrings(system->m_windows[window_index].name, ECS_TOOLS_UI_CHOOSE_WINDOW_NAME)) {
				DestroyCurrentActionWindow(action_data);
			}
		}

		void ChooseOptionWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			drawer.DisablePaddingForRenderSliders();
			ChooseOptionWindowData* data = (ChooseOptionWindowData*)window_data;
			drawer.Sentence(data->description.buffer);
			drawer.NextRow();

			UIDrawConfig config;
			UIConfigAbsoluteTransform transform;

			for (size_t index = 0; index < data->handlers.size; index++) {
				float2 label_size = drawer.GetLabelScale(data->button_names[index]);
				transform.position = drawer.GetAlignedToBottom(label_size.y);
				transform.scale = label_size;

				config.AddFlag(transform);
				ChooseOptionActionData index_data;
				index_data.data = data;
				index_data.index = index;
				drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, data->button_names[index], { ChooseOptionAction, &index_data, sizeof(index_data), ECS_UI_DRAW_SYSTEM });
				config.flag_count = 0;
			}

			transform.scale = drawer.GetLabelScale("Cancel");
			transform.position.x = drawer.GetAlignedToRight(transform.scale.x).x;
			config.AddFlag(transform);
			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Cancel", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
		}

		// -------------------------------------------------------------------------------------------------------

		constexpr size_t TEXT_INPUT_WIZARD_STREAM_CAPACITY = 512;

		void TextInputWizardConfirmAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			TextInputWizardData* data = (TextInputWizardData*)_data;
			action_data->data = data->callback_data;
			action_data->additional_data = &data->input_stream;
			data->callback(action_data);
			CloseXBorderClickableAction(action_data);
		}

		void TextInputWizard(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize)
		{
			UI_PREPARE_DRAWER(initialize);

			TextInputWizardData* data = (TextInputWizardData*)window_data;
			
			if (initialize) {
				size_t input_name_size = strlen(data->input_name);
				void* allocation = drawer.GetMainAllocatorBuffer(sizeof(char) * (input_name_size + 1));
				memcpy(allocation, data->input_name, sizeof(char) * (input_name_size + 1));
				data->input_name = (const char*)allocation;

				allocation = drawer.GetMainAllocatorBuffer(sizeof(char) * TEXT_INPUT_WIZARD_STREAM_CAPACITY);
				data->input_stream.InitializeFromBuffer(allocation, 0, TEXT_INPUT_WIZARD_STREAM_CAPACITY);

				if (data->callback_data_size > 0) {
					allocation = drawer.GetMainAllocatorBuffer(data->callback_data_size);
					memcpy(allocation, data->callback_data, data->callback_data_size);
					data->callback_data = allocation;
				}
			}

			UIDrawConfig config;
			UIConfigWindowDependentSize transform;
			config.AddFlag(transform);

			drawer.TextInput(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, data->input_name, &data->input_stream);
			drawer.NextRow();

			ActionData extra_action_data = drawer.GetDummyActionData();
			for (unsigned char index = 0; index < data->extra_draw_element_count; index++) {
				extra_action_data.data = &drawer;
				if (data->extra_draw_elements_data[index] == nullptr) {
					extra_action_data.additional_data = function::OffsetPointer(data->extra_draw_elements_storage, data->extra_draw_elements_data_offset[index]);
				}
				else {
					extra_action_data.additional_data = data->extra_draw_elements_data[index];
				}
				data->extra_draw_elements[index](&extra_action_data);
				drawer.NextRow();
			}

			UIConfigAbsoluteTransform absolute_transform;
			absolute_transform.scale = drawer.GetLabelScale("OK");
			absolute_transform.position = drawer.GetAlignedToBottom(absolute_transform.scale.y);
			config.flag_count = 0;
			config.AddFlag(absolute_transform);

			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "OK", { TextInputWizardConfirmAction, window_data, 0, ECS_UI_DRAW_SYSTEM });

			absolute_transform.scale = drawer.GetLabelScale("Cancel");
			absolute_transform.position.x = drawer.GetAlignedToRight(absolute_transform.scale.x).x;
			config.flag_count = 0;
			config.AddFlag(absolute_transform);

			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Cancel", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
		}

		// Additional data is the window data that is TextInputWizardData*
		void TextInputWizardPrivateHandler(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			if (keyboard->IsPressed(ECS_KEY_ENTER)) {
				action_data->data = action_data->additional_data;
				TextInputWizardConfirmAction(action_data);
			}
		}

		unsigned int CreateTextInputWizard(const TextInputWizardData* data, UISystem* system) {
			UIWindowDescriptor descriptor;
			
			descriptor.window_name = data->window_name;
			descriptor.draw = TextInputWizard;
			
			descriptor.initial_position_x = 0.0f;
			descriptor.initial_position_y = 0.0f;
			descriptor.initial_size_x = 0.7f;
			descriptor.initial_size_y = 0.7f;

			descriptor.window_data = (void*)data;
			descriptor.window_data_size = sizeof(*data);
			descriptor.destroy_action = ReleaseLockedWindow;

			descriptor.private_action = TextInputWizardPrivateHandler;

			system->PopUpFrameHandler(data->window_name, false, false, false);
			return system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL);
		}

		void CreateTextInputWizardAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			TextInputWizardData* data = (TextInputWizardData*)_data;
			CreateTextInputWizard(data, system);
		}

		// -------------------------------------------------------------------------------------------------------

		void DrawTextFile(UIDrawer* drawer, Stream<wchar_t> path, float2 border_padding, float next_row_y_offset) {
			float2 current_position = drawer->GetCurrentPositionNonOffset();
			drawer->OffsetNextRow(border_padding.x);
			drawer->OffsetX(border_padding.x);
			drawer->OffsetY(border_padding.y);

			UIDrawConfig config;
			Stream<char> contents = ReadWholeFileText(path);

			bool draw_border = false;
			if (contents.buffer != nullptr) {
				contents[contents.size - 1] = '\0';
				float old_next_row_y_offset = drawer->layout.next_row_y_offset;
				drawer->OffsetY(old_next_row_y_offset);
				drawer->SetNextRowYOffset(next_row_y_offset);
				drawer->Sentence(UI_CONFIG_SENTENCE_FIT_SPACE, config, contents.buffer);
				drawer->SetNextRowYOffset(old_next_row_y_offset);
				draw_border = true;
				free(contents.buffer);
			}
			else {
				drawer->Text("File could not be opened.");
			}

			if (draw_border) {
				drawer->NextRow();
				float2 end_position = drawer->GetCurrentPositionNonOffset();
				UIConfigAbsoluteTransform transform;
				transform.position = current_position;
				transform.scale = { drawer->region_limit.x - current_position.x, end_position.y - current_position.y + next_row_y_offset - drawer->layout.next_row_y_offset };
				config.AddFlag(transform);

				UIConfigBorder border;
				border.color = drawer->color_theme.borders;
				config.AddFlag(border);
				drawer->Rectangle(UI_CONFIG_BORDER | UI_CONFIG_ABSOLUTE_TRANSFORM, config);
			}
			drawer->OffsetNextRow(-border_padding.x);
			drawer->NextRow();
		}

		void DrawTextFileEx(UIDrawer* drawer, Stream<wchar_t> path, float2 border_padding, float next_row_y_offset) {
			constexpr const char* STABILIZE_STRING_NAME = "Stabilized render span";

			float2 current_position = drawer->GetCurrentPositionNonOffset();
			drawer->OffsetNextRow(border_padding.x);
			drawer->OffsetX(border_padding.x);
			drawer->OffsetY(border_padding.y);

			UIDrawConfig config;
			Stream<char> contents = ReadWholeFileText(path);

			bool draw_border = false;
			if (contents.buffer) {
				contents[contents.size - 1] = '\0';
				float old_next_row_y_offset = drawer->layout.next_row_y_offset;
				drawer->OffsetY(old_next_row_y_offset);
				drawer->SetNextRowYOffset(next_row_y_offset);
				drawer->Sentence(UI_CONFIG_SENTENCE_FIT_SPACE, config, contents.buffer);
				drawer->SetNextRowYOffset(old_next_row_y_offset);
				draw_border = true;
				free(contents.buffer);
			}
			else {
				drawer->Text("File could not be opened.");
			}

			if (draw_border) {
				drawer->NextRow();
				float2 end_position = drawer->GetCurrentPositionNonOffset();
				UIConfigAbsoluteTransform transform;
				transform.position = current_position;
				transform.scale = { drawer->region_limit.x - current_position.x, end_position.y - current_position.y + next_row_y_offset - drawer->layout.next_row_y_offset };
				config.AddFlag(transform);

				UIConfigBorder border;
				border.color = drawer->color_theme.borders;
				config.AddFlag(border);
				drawer->Rectangle(UI_CONFIG_BORDER | UI_CONFIG_ABSOLUTE_TRANSFORM, config);
			}
			drawer->OffsetNextRow(-border_padding.x);
			drawer->NextRow();
		}

		void TextFileDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize)
		{
			UI_PREPARE_DRAWER(initialize);

			const TextFileDrawData* data = (const TextFileDrawData*)window_data;

			if (!initialize) {
				DrawTextFileEx(&drawer, data->path, data->border_padding, data->next_row_y_offset);
			}
		}

		unsigned int CreateTextFileWindow(TextFileDrawData data, UISystem* system, Stream<char> window_name) {
			UIWindowDescriptor descriptor;
			
			descriptor.window_data = &data;
			descriptor.window_data_size = sizeof(data);
			descriptor.window_name = window_name;

			descriptor.draw = TextFileDraw;
			descriptor.initial_size_x = 1.0f;
			descriptor.initial_size_y = 1.5f;
			descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, descriptor.initial_size_x);
			descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, descriptor.initial_size_y);

			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_POP_UP_WINDOW);
			TextFileDrawData* window_data = (TextFileDrawData*)system->GetWindowData(window_index);

			size_t path_size = wcslen(data.path) + 1;
			wchar_t* allocation = (wchar_t*)system->m_memory->Allocate(path_size * sizeof(wchar_t), alignof(wchar_t));
			memcpy(allocation, data.path, sizeof(wchar_t) * path_size);
			window_data->path = allocation;

			return window_index;
		}

		void CreateTextFileWindowAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			const TextFileDrawActionData* data = (const TextFileDrawActionData*)_data;
			CreateTextFileWindow(data->draw_data, system, data->window_name);
		}

		// -------------------------------------------------------------------------------------------------------

		void ConsoleClearAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ECSEngine::Console* console = (ECSEngine::Console*)_data;
			console->Clear();
		}

		// -------------------------------------------------------------------------------------------------------

		void ConsoleWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			ConsoleWindowData* data = (ConsoleWindowData*)window_data;
			drawer.SetDrawMode(ECS_UI_DRAWER_FIT_SPACE);
			drawer.layout.next_row_padding = 0.005f;

			if (initialize) {
				const size_t unique_message_default_capacity = 128;
				size_t hash_table_byte_size = data->unique_messages.MemoryOf(unique_message_default_capacity);
				void* hash_table_allocation = drawer.GetMainAllocatorBuffer(hash_table_byte_size);
				data->unique_messages.InitializeFromBuffer(hash_table_allocation, unique_message_default_capacity);

				data->system_filter.buffer = (bool*)drawer.GetMainAllocatorBuffer(sizeof(bool) * data->console->system_filter_strings.size);
				memset(data->system_filter.buffer, 1, sizeof(bool) * data->console->system_filter_strings.size);
				data->system_filter.size = data->console->system_filter_strings.size;
			}
			else {
				// Check to see if a new system was added such that we can readjust the system filter array
				if (data->system_filter.size != data->console->system_filter_strings.size) {
					drawer.RemoveAllocation(data->system_filter.buffer);
					data->system_filter.buffer = (bool*)drawer.GetMainAllocatorBuffer(sizeof(bool) * data->console->system_filter_strings.size);
					// Just set everything to 1
					memset(data->system_filter.buffer, 1, sizeof(bool) * data->console->system_filter_strings.size);
					data->system_filter.size = data->console->system_filter_strings.size;
				}
			}

			UIDrawConfig config;

#pragma region Recalculate counts

			if (!initialize) {
				ConsoleFilterMessages(data, drawer);
			}

#pragma endregion

#pragma region State buttons

			constexpr size_t button_configuration = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LATE_DRAW
				| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_BORDER;

			UIConfigAbsoluteTransform transform;
			UIConfigBorder border;
			border.color = drawer.color_theme.borders;
			float2 border_thickness = drawer.GetSquareScale(border.thickness);

			config.AddFlag(border);
			transform.position = drawer.GetRegionPosition() + float2(0.0f, drawer.region_render_offset.y);
			transform.position.y += drawer.system->m_descriptors.misc.title_y_scale + ECS_TOOLS_UI_ONE_PIXEL_Y;
			// small offset to move it out from the border
			transform.position.x += 0.0015f;

			transform.scale = drawer.GetLabelScale("Clear");

			config.AddFlag(transform);
			drawer.Button(button_configuration, config, "Clear", { ConsoleClearAction, data->console, 0 });
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Collapse");
			config.AddFlag(transform);
			drawer.StateButton(button_configuration, config, "Collapse", &data->collapse);
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Clear on play");
			config.AddFlag(transform);
			drawer.StateButton(button_configuration, config, "Clear on play", &data->clear_on_play);
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Pause on error");
			config.AddFlag(transform);
			drawer.StateButton(button_configuration, config, "Pause on error", &data->console->pause_on_error);
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Filter");
			config.AddFlag(transform);

			constexpr size_t filter_menu_configuration = button_configuration | UI_CONFIG_FILTER_MENU_ALL | UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE;

			Stream<char> filter_labels[] = { "Info", "Warn", "Error", "Trace" };
			Stream<Stream<char>> filter_stream = Stream<Stream<char>>(filter_labels, std::size(filter_labels));
			UIConfigFilterMenuNotify menu_notify;
			menu_notify.notifier = &data->filter_message_type_changed;
			config.AddFlag(menu_notify);
			drawer.FilterMenu(filter_menu_configuration | UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES, config, "Filter", filter_stream, &data->filter_info);
			config.flag_count -= 2;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("System");
			config.AddFlag(transform);
			menu_notify.notifier = &data->system_filter_changed;
			config.AddFlag(menu_notify);
			Stream<Stream<char>> system_filter_stream = Stream<Stream<char>>(data->console->system_filter_strings.buffer, data->console->system_filter_strings.size);
			drawer.FilterMenu(filter_menu_configuration, config, "System", system_filter_stream, data->system_filter.buffer);
			config.flag_count -= 2;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Verbosity");
			config.AddFlag(transform);
			Stream<char> verbosity_labels[] = { "Minimal", "Medium", "Detailed" };
			Stream<Stream<char>> verbosity_label_stream = Stream<Stream<char>>(verbosity_labels, std::size(verbosity_labels));

			UIConfigComboBoxPrefix verbosity_prefix;
			verbosity_prefix.prefix = "Verbosity: ";
			config.AddFlag(verbosity_prefix);
			drawer.ComboBox(
				button_configuration | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_COMBO_BOX_NO_NAME,
				config, 
				"Verbosity",
				verbosity_label_stream, 
				verbosity_label_stream.size,
				&data->console->verbosity_level
			);
			config.flag_count -= 2;

			float2 verbosity_label_scale = drawer.GetLastSolidColorRectangleScale(UI_CONFIG_LATE_DRAW, 1);
			transform.position.x += verbosity_label_scale.x /*+ border_thickness.x*/;
			transform.scale = drawer.GetLabelScale("Dump");
			config.AddFlag(transform);
			Stream<char> dump_label_ptrs[] = { "Count Messages", "On Call", "None" };
			Stream<Stream<char>> dump_labels = Stream<Stream<char>>(dump_label_ptrs, std::size(dump_label_ptrs));

			float2 get_position = { 0.0f, 0.0f }, get_scale = {0.0f, 0.0f};
			UIConfigGetTransform get_transform;
			get_transform.position = &get_position;
			get_transform.scale = &get_scale;
			config.AddFlag(get_transform);

			UIConfigComboBoxPrefix dump_prefix;
			dump_prefix.prefix = "Dump type: ";
			config.AddFlag(dump_prefix);

			drawer.ComboBox(
				button_configuration | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_GET_TRANSFORM,
				config, 
				"Dump", 
				dump_labels, 
				dump_labels.size, 
				(unsigned char*)&data->console->dump_type
			);
			config.flag_count -= 3;

			transform.position.x = get_position.x + get_scale.x;

#pragma endregion

#pragma region Messages
			
			drawer.NextRow();

			// Display a warning if there are too many messages
			if (data->console->messages.size == data->console->messages.capacity && data->console->messages.capacity > data->console->MaxMessageCount()) {
				drawer.SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, CONSOLE_TEXTURE_ICONS[ECS_CONSOLE_WARN], CONSOLE_COLORS[ECS_CONSOLE_WARN]);
				drawer.Text("Too many console messages - the console should be cleared otherwise incoming messages will be ignored");
				drawer.NextRow();
			}

			UIConfigTextParameters parameters;
			float2 icon_scale = drawer.GetSquareScale(drawer.layout.default_element_y);
			const bool* filter_ptr = &data->filter_info;

			size_t system_mask = GetSystemMaskFromConsoleWindowData(data);

			auto draw_sentence = [&](const ConsoleMessage& console_message, unsigned int index) {
				drawer.NextRow();

				if (console_message.type == ECS_CONSOLE_MESSAGE_COUNT) {
					drawer.OffsetX(icon_scale.x + drawer.layout.element_indentation);
				}
				else {
					if (console_message.type == ECS_CONSOLE_ERROR) {
						drawer.SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, CONSOLE_TEXTURE_ICONS[(unsigned int)ECS_CONSOLE_ERROR]);
					}
					else {
						drawer.SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, CONSOLE_TEXTURE_ICONS[(unsigned int)console_message.type], CONSOLE_COLORS[(unsigned int)console_message.type]);
					}
				}
				parameters.color = CONSOLE_COLORS[(unsigned int)console_message.type];

				config.AddFlag(parameters);
				drawer.Sentence(
					UI_CONFIG_SENTENCE_FIT_SPACE | UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE | UI_CONFIG_TEXT_PARAMETERS, 
					config, 
					console_message.message.buffer
				);
				config.flag_count--;
			};

			auto draw_sentence_collapsed = [&](const ConsoleMessage& console_message, unsigned int index) {
				bool do_draw = true;
				unsigned char type_index = (unsigned char)console_message.type;
				bool is_verbosity_valid = console_message.verbosity <= data->console->verbosity_level;
				bool is_system_valid = (console_message.system_filter & system_mask) != 0;
				do_draw = filter_ptr[type_index] && is_verbosity_valid && is_system_valid;

				if (do_draw) {
					draw_sentence(console_message, index);
				}

				return do_draw;
			};

			drawer.NextRow(0.25f);
			if (data->collapse) {
				// Draw only unique message alongside their counter
				data->unique_messages.ForEachIndexConst([&](unsigned int index) {
					UniqueConsoleMessage message = data->unique_messages.GetValueFromIndex(index);

					bool do_draw = draw_sentence_collapsed(message.message, index);

					if (do_draw) {
						// draw the counter
						parameters.color = CONSOLE_COLORS[(unsigned int)message.message.type];
						char temp_characters[256];
						Stream<char> temp_stream = Stream<char>(temp_characters, 0);
						function::ConvertIntToChars(temp_stream, message.counter);
						float2 label_scale = drawer.GetLabelScale(temp_stream);
						float2 aligned_position = drawer.GetAlignedToRightOverLimit(label_scale.x);

						UIConfigAbsoluteTransform absolute_transform;
						absolute_transform.position = aligned_position;
						absolute_transform.scale = label_scale;
						config.AddFlags(parameters, absolute_transform);
						drawer.TextLabel(
							UI_CONFIG_BORDER | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE
							| UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_TEXT_PARAMETERS,
							config,
							temp_characters
						);
						config.flag_count -= 2;
					}
				});
			}
			else {
				// Draw each message one by one
				for (size_t index = 0; index < data->filtered_message_indices.size; index++) {
					draw_sentence(data->console->messages[data->filtered_message_indices[index]], index);
				}
			}

#pragma endregion

#pragma region Counters

			float2 sprite_scale = drawer.GetSquareScale(transform.scale.y);
			
			// temp characters should be used at max 20; for conversion for numbers
			char temp_characters[1024];

			constexpr float COUNTER_BOUND_PADD = 0.005f;
			// if the counter rectangle passes this bound, abort the rendering and redoit with an offset
			float counter_bound = transform.position.x + border_thickness.x + COUNTER_BOUND_PADD;
			
			transform.position.x = drawer.region_position.x + drawer.region_scale.x + drawer.region_render_offset.x - ECS_TOOLS_UI_ONE_PIXEL_X;
			if (drawer.system->m_windows[drawer.window_index].is_vertical_render_slider) {
				transform.position.x -= drawer.system->m_descriptors.misc.render_slider_vertical_size + ECS_TOOLS_UI_ONE_PIXEL_X;
			}

			size_t initial_text_sprite_count = *drawer.HandleTextSpriteCount(UI_CONFIG_LATE_DRAW);
			size_t initial_solid_color_count = *drawer.HandleSolidColorCount(UI_CONFIG_LATE_DRAW);
			size_t initial_sprite_count = *drawer.HandleSpriteCount(UI_CONFIG_LATE_DRAW);

			auto counter_backwards = [&](LPCWSTR texture, Color color, bool* filter_status, unsigned int counter) {
				const size_t configuration = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_FIT_SPACE 
					| UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_LATE_DRAW;

				if (counter == 0 || !(*filter_status)) {
					color = drawer.color_theme.unavailable_text;
				}

				Stream<char> stream = Stream<char>(temp_characters, 0);
				function::ConvertIntToCharsFormatted(stream, static_cast<int64_t>(counter));
				float2 label_scale = drawer.GetLabelScale(temp_characters);

				float initial_x_position = transform.position.x;
				transform.position.x -= label_scale.x - drawer.element_descriptor.label_padd.x;

				transform.position.y += drawer.element_descriptor.label_padd.y;
				config.AddFlag(transform);
				drawer.Text(configuration, config, temp_characters);
				transform.position.y -= drawer.element_descriptor.label_padd.y;
				config.flag_count--;

				transform.position.x -= drawer.element_descriptor.label_padd.x + sprite_scale.x;
				transform.scale = sprite_scale;
				config.AddFlag(transform);
				drawer.SpriteRectangle(configuration, config, texture, color);
				config.flag_count--;

				transform.position.x -= drawer.element_descriptor.label_padd.x;
				transform.scale.x = { initial_x_position - transform.position.x };
				config.AddFlag(transform);

				drawer.SolidColorRectangle(configuration | UI_CONFIG_BORDER, config, drawer.color_theme.theme);
				config.flag_count--;

				UIDrawerStateTableBoolClickable clickable_data;
				clickable_data.notifier = &data->filter_message_type_changed;
				clickable_data.state = filter_status;
				drawer.AddDefaultClickableHoverable(
					configuration,
					{ transform.position.x, transform.position.y - drawer.region_render_offset.y }, 
					transform.scale, 
					{ StateTableBoolClickable, &clickable_data, sizeof(clickable_data), ECS_UI_DRAW_LATE },
					drawer.color_theme.theme
				);
				transform.position.x -= border_thickness.x;
			};
			
			auto counter_forwards = [&](LPCWSTR texture, Color color, bool* filter_status, unsigned int counter) {
				constexpr size_t configuration = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_FIT_SPACE
					| UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_LATE_DRAW;

				if (counter == 0 || !(*filter_status)) {
					color = drawer.color_theme.unavailable_text;
				}

				Stream<char> stream = Stream<char>(temp_characters, 0);
				function::ConvertIntToCharsFormatted(stream, static_cast<int64_t>(counter));
				float2 label_scale = drawer.GetLabelScale(temp_characters);

				float initial_x_position = transform.position.x;
				transform.position.x += drawer.element_descriptor.label_padd.x;
				transform.scale = sprite_scale;
				config.AddFlag(transform);
				drawer.SpriteRectangle(configuration, config, texture, color);
				config.flag_count--;

				transform.position.x += sprite_scale.x + drawer.element_descriptor.label_padd.x;
				transform.position.y += drawer.element_descriptor.label_padd.y;
				config.AddFlag(transform);
				drawer.Text(configuration, config, temp_characters);
				transform.position.y -= drawer.element_descriptor.label_padd.y;
				config.flag_count--;

				transform.position.x += label_scale.x - drawer.element_descriptor.label_padd.x;
				transform.scale.x = { transform.position.x - initial_x_position };
				transform.position.x = initial_x_position;
				config.AddFlag(transform);

				drawer.SolidColorRectangle(configuration | UI_CONFIG_BORDER, config, drawer.color_theme.theme);
				config.flag_count--;

				UIDrawerStateTableBoolClickable clickable_data;
				clickable_data.notifier = &data->filter_message_type_changed;
				clickable_data.state = filter_status;
				drawer.AddDefaultClickableHoverable(
					configuration,
					{ transform.position.x, transform.position.y - drawer.region_render_offset.y },
					transform.scale,
					{ StateTableBoolClickable, &clickable_data, sizeof(clickable_data), ECS_UI_DRAW_LATE },
					drawer.color_theme.theme
				);
				transform.position.x += border_thickness.x + transform.scale.x;
			};

			UIDrawerBufferState drawer_state = drawer.GetBufferState(UI_CONFIG_LATE_DRAW);
			UIDrawerHandlerState handler_state = drawer.GetHandlerState();

			counter_backwards(ECS_TOOLS_UI_TEXTURE_TRACE_ICON, CONSOLE_TRACE_COLOR, &data->filter_trace, data->trace_count);
			counter_backwards(ECS_TOOLS_UI_TEXTURE_ERROR_ICON, ECS_COLOR_WHITE, &data->filter_error, data->error_count);
			counter_backwards(ECS_TOOLS_UI_TEXTURE_WARN_ICON, CONSOLE_WARN_COLOR, &data->filter_warn, data->warn_count);
			counter_backwards(ECS_TOOLS_UI_TEXTURE_INFO_ICON, CONSOLE_INFO_COLOR, &data->filter_info, data->info_count);

			// if it overpassed the bound, revert to the initial state and redo the drawing from the bound
			if (transform.position.x < counter_bound) {
				drawer.RestoreBufferState(UI_CONFIG_LATE_DRAW, drawer_state);
				drawer.RestoreHandlerState(handler_state);

				transform.position.x = counter_bound;
				counter_forwards(ECS_TOOLS_UI_TEXTURE_INFO_ICON, CONSOLE_INFO_COLOR, &data->filter_info, data->info_count);
				counter_forwards(ECS_TOOLS_UI_TEXTURE_WARN_ICON, CONSOLE_WARN_COLOR, &data->filter_warn, data->warn_count);
				counter_forwards(ECS_TOOLS_UI_TEXTURE_ERROR_ICON, ECS_COLOR_WHITE, &data->filter_error, data->error_count);
				counter_forwards(ECS_TOOLS_UI_TEXTURE_TRACE_ICON, CONSOLE_TRACE_COLOR, &data->filter_trace, data->trace_count);
			}

			data->previous_verbosity_level = data->console->verbosity_level;

#pragma endregion

		}

		// -------------------------------------------------------------------------------------------------------

		void ConsoleFilterMessages(ConsoleWindowData* data, UIDrawer& drawer)
		{
			bool recalculate_counts = (data->console->messages.size != data->last_frame_message_count) || data->filter_message_type_changed
				|| (data->console->verbosity_level != data->previous_verbosity_level) || data->system_filter_changed;
			if (recalculate_counts) {
				unsigned int dummy;
				unsigned int* ptrs[] = { &data->info_count, &data->warn_count, &data->error_count, &data->trace_count, &dummy };
				bool* type_ptrs = &data->filter_info;
				data->filter_message_type_changed = false;
				data->system_filter_changed = false;

				auto update_kernel = [&](size_t starting_index) {
					size_t system_mask = GetSystemMaskFromConsoleWindowData(data);

					for (size_t index = starting_index; index < data->console->messages.size; index++) {
						(*ptrs[(unsigned int)data->console->messages[index].type])++;
						ResourceIdentifier identifier = {
							data->console->messages[index].message.buffer + data->console->messages[index].client_message_start,
							(unsigned int)data->console->messages[index].message.size - data->console->messages[index].client_message_start
						};

						int message_index = data->unique_messages.Find(identifier);
						if (message_index == -1) {
							auto grow_unique_messages = [&](size_t new_capacity) {
								void* new_allocation = drawer.GetMainAllocatorBuffer(data->unique_messages.MemoryOf(new_capacity));
								const void* old_allocation = data->unique_messages.Grow(new_allocation, new_capacity);
								if (old_allocation != nullptr) {
									drawer.RemoveAllocation(old_allocation);
								}
							};
							if (data->unique_messages.GetCapacity() == 0) {
								grow_unique_messages(16);
							}

							bool grow = data->unique_messages.Insert({ data->console->messages[index], 1 }, identifier);
							if (grow) {
								size_t new_capacity = data->unique_messages.NextCapacity(data->unique_messages.GetCapacity());
								grow_unique_messages(new_capacity);
							}
						}
						else {
							UniqueConsoleMessage* message_ptr = data->unique_messages.GetValuePtrFromIndex(message_index);
							message_ptr->counter++;
						}

						bool is_system_valid = (data->console->messages[index].system_filter & system_mask) != 0;
						bool is_type_valid = type_ptrs[(unsigned int)data->console->messages[index].type];
						bool is_verbosity_valid = data->console->messages[index].verbosity <= data->console->verbosity_level;
						if (is_system_valid && is_type_valid && is_verbosity_valid) {
							if (data->filtered_message_indices.IsFull()) {
								size_t new_capacity = (size_t)(data->filtered_message_indices.capacity * 1.5f + 1);
								void* new_buffer = drawer.GetMainAllocatorBuffer(data->filtered_message_indices.MemoryOf(new_capacity));
								void* old_buffer = data->filtered_message_indices.Expand(new_buffer, new_capacity);
								if (old_buffer != nullptr) {
									drawer.RemoveAllocation(old_buffer);
								}
							}
							data->filtered_message_indices.Add(index);
						}

					}
				};

				// if more messages have been added, only account for them
				if (data->last_frame_message_count < data->console->messages.size) {
					update_kernel(data->last_frame_message_count);
				}
				// else start from scratch; console has been cleared, type filter changed, verbosity filter changed,
				// or system filter changed
				else {
					*ptrs[0] = *ptrs[1] = *ptrs[2] = *ptrs[3] = 0;
					// Remove the allocations and reinitialize with 0
					if (data->filtered_message_indices.size > 0) {
						drawer.RemoveAllocation(data->filtered_message_indices.buffer);
					}
					if (data->unique_messages.GetCapacity() > 0) {
						drawer.RemoveAllocation(data->unique_messages.GetAllocatedBuffer());
					}

					data->unique_messages.Reset();
					data->filtered_message_indices.Reset();

					update_kernel(0);
				}
				data->last_frame_message_count = data->console->messages.size;
			}
		}

		// -------------------------------------------------------------------------------------------------------

		size_t GetSystemMaskFromConsoleWindowData(const ConsoleWindowData* data)
		{
			size_t mask = 0;
			for (size_t index = 0; index < data->console->system_filter_strings.size; index++) {
				mask |= (size_t)data->system_filter[index] << index;
			}
			return mask;
		}

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateConsoleWindow(UISystem* system) {
			UIWindowDescriptor descriptor;

			descriptor.draw = ConsoleWindowDraw;

			descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, CONSOLE_WINDOW_SIZE.x);
			descriptor.initial_size_x = CONSOLE_WINDOW_SIZE.x;
			descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, CONSOLE_WINDOW_SIZE.y);
			descriptor.initial_size_y = CONSOLE_WINDOW_SIZE.y;

			ConsoleWindowData window_data;
			CreateConsoleWindowData(window_data);

			descriptor.window_data = &window_data;
			descriptor.window_data_size = sizeof(window_data);
			descriptor.window_name = CONSOLE_WINDOW_NAME;

			return system->Create_Window(descriptor);
		}

		// -------------------------------------------------------------------------------------------------------

		void CreateConsole(UISystem* system) {
			unsigned int window_index = system->GetWindowFromName(CONSOLE_WINDOW_NAME);

			if (window_index == -1) {
				window_index = CreateConsoleWindow(system);
				float2 window_position = system->GetWindowPosition(window_index);
				float2 window_scale = system->GetWindowScale(window_index);
				system->CreateDockspace({ window_position, window_scale }, DockspaceType::FloatingHorizontal, window_index, false);
				GetConsole()->Dump();
			}
			else {
				system->SetActiveWindow(window_index);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void CreateConsoleAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			CreateConsole(system);
		}

		// -------------------------------------------------------------------------------------------------------

		void CreateConsoleWindowData(ConsoleWindowData& data)
		{
			data.console = GetConsole();
			data.clear_on_play = false;
			data.collapse = false;
			data.filter_all = true;
			data.filter_error = true;
			data.filter_info = true;
			data.filter_trace = true;
			data.filter_warn = true;
			data.error_count = 0;
			data.info_count = 0;
			data.warn_count = 0;
			data.trace_count = 0;
			data.last_frame_message_count = 0;
			data.filtered_message_indices.InitializeFromBuffer(nullptr, 0, 0);
		}

		// -------------------------------------------------------------------------------------------------------

		void InjectValuesWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer) {
			UI_PREPARE_DRAWER(initializer);

			InjectWindowData* data = (InjectWindowData*)window_data;
			if (initializer) {
				// Make a coalesced allocation for all streams
				size_t allocation_size = 0;
				allocation_size += sizeof(InjectWindowSection) * data->sections.size;
				for (size_t index = 0; index < data->sections.size; index++) {
					allocation_size += sizeof(InjectWindowElement) * data->sections[index].elements.size;
					allocation_size += sizeof(char) * data->sections[index].name.size;
					for (size_t subindex = 0; subindex < data->sections[index].elements.size; subindex++) {
						allocation_size += data->sections[index].elements[subindex].name.size;
					}
				}

				void* allocation = drawer.GetMainAllocatorBuffer(allocation_size);
				InjectWindowSection* sections = (InjectWindowSection*)allocation;
				uintptr_t buffer = (uintptr_t)allocation;
				data->sections.CopyTo(buffer);
				for (size_t index = 0; index < data->sections.size; index++) {
					sections[index].elements.InitializeAndCopy(buffer, data->sections[index].elements);
				}
				for (size_t index = 0; index < data->sections.size; index++) {
					sections[index].name = (const char*)buffer;
					size_t section_name_size = data->sections[index].name.size;
					memcpy((void*)buffer, data->sections[index].name.buffer, sizeof(char) * section_name_size);
					buffer += section_name_size;

					for (size_t subindex = 0; subindex < data->sections[index].elements.size; subindex++) {
						sections[index].elements[subindex].name = (const char*)buffer;
						size_t name_size = data->sections[index].elements[subindex].name.size;
						memcpy((void*)buffer, data->sections[index].elements[subindex].name.buffer, sizeof(char) * name_size);
						buffer += name_size;
					}
				}

				data->sections.buffer = sections;

				Reflection::ReflectionField field;
				Reflection::ReflectionType type;
				type.fields = Stream<Reflection::ReflectionField>(&field, 1);

				// One type and instance must be created separetely for each element - cannot create a type and an instance
				// for each sections because the data is provided through pointers for each element
				for (size_t index = 0; index < data->sections.size; index++) {
					for (size_t subindex = 0; subindex < sections[index].elements.size; subindex++) {
						type.name = data->sections[index].elements[subindex].name;
						type.fields[0].name = sections[index].elements[subindex].name;
						type.fields[0].definition = sections[index].elements[subindex].basic_type_string;

						if (data->sections[index].elements[subindex].stream_type == Reflection::ReflectionStreamFieldType::Basic) {
							type.fields[0].info = Reflection::GetReflectionFieldInfo(data->ui_reflection->reflection, sections[index].elements[subindex].basic_type_string).info;
						}
						else {
							type.fields[0].info = Reflection::GetReflectionFieldInfoStream(
								data->ui_reflection->reflection, 
								sections[index].elements[subindex].basic_type_string, 
								sections[index].elements[subindex].stream_type
							).info;
						}
						
						type.fields[0].info.pointer_offset = 0;
						// Single indirection for pointers
						if (data->sections[index].elements[subindex].stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
							type.fields[0].info.basic_type_count = 1;
						}
						type.fields[0].info.has_default_value = false;

						UIReflectionType* ui_type = data->ui_reflection->CreateType(&type);
						data->ui_reflection->ConvertTypeStreamsToResizable(ui_type);
						UIReflectionInstance* instance = data->ui_reflection->CreateInstance(type.name, ui_type);

						// When binding the pointer data, the resizable stream will mirror the nullptr buffer
						// Record the value to be populated later
						void* pointer_data = data->sections[index].elements[subindex].data;
						void* bind_instance_ptr = pointer_data;
						if (data->sections[index].elements[subindex].stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
							bind_instance_ptr = &data->sections[index].elements[subindex].data;
						}
						data->ui_reflection->BindInstancePtrs(instance, bind_instance_ptr, &type);

						// Bind the stream capacity - if different from capacity stream
						if (data->sections[index].elements[subindex].stream_type != Reflection::ReflectionStreamFieldType::CapacityStream && 
							data->sections[index].elements[subindex].stream_type != Reflection::ReflectionStreamFieldType::Basic &&
							data->sections[index].elements[subindex].stream_type != Reflection::ReflectionStreamFieldType::Unknown
						) {
							bool is_text_input = memcmp(data->sections[index].elements[subindex].basic_type_string.buffer, STRING(char), strlen(STRING(char))) == 0;
							bool is_path_input = memcmp(data->sections[index].elements[subindex].basic_type_string.buffer, STRING(wchar_t), strlen(STRING(wchar_t))) == 0;
							if (!is_text_input && !is_path_input)
							{
								UIReflectionBindResizableStreamAllocator bind;
								bind.allocator = GetAllocatorPolymorphic(data->ui_reflection->allocator);
								bind.field_name = data->sections[index].elements[subindex].name;
								data->ui_reflection->BindInstanceResizableStreamAllocator(instance, { &bind, 1 });

								if (data->sections[index].elements[subindex].stream_size > 0) {
									UIReflectionBindResizableStreamData resize_data;
									resize_data.field_name = data->sections[index].elements[subindex].name;
									resize_data.data = { pointer_data, data->sections[index].elements[subindex].stream_size };
									data->ui_reflection->BindInstanceResizableStreamData(instance, { &resize_data, 1 });
								}
							}
							else {
								if (is_text_input) {
									void* allocation = malloc(sizeof(CapacityStream<char>) + sizeof(char) * 128);
									UIReflectionBindTextInput bind;
									bind.field_name = data->sections[index].elements[subindex].name;
									bind.stream = (CapacityStream<char>*)allocation;
									bind.stream->buffer = (char*)function::OffsetPointer(allocation, sizeof(CapacityStream<char>));
									bind.stream->size = 0;
									bind.stream->capacity = 128;

									if (data->sections[index].elements[subindex].stream_size > 0) {
										memcpy(bind.stream->buffer, pointer_data, sizeof(char)* data->sections[index].elements[subindex].stream_size);
										bind.stream->size = data->sections[index].elements[subindex].stream_size;
									}
									data->ui_reflection->BindInstanceTextInput(instance, { &bind, 1 });
								}
								else {
									void* allocation = malloc(sizeof(CapacityStream<wchar_t>) + sizeof(wchar_t) * 256);
									UIReflectionBindDirectoryInput bind;
									bind.field_name = data->sections[index].elements[subindex].name;
									bind.stream = (CapacityStream<wchar_t>*)allocation;
									bind.stream->buffer = (wchar_t*)function::OffsetPointer(allocation, sizeof(CapacityStream<wchar_t>));
									bind.stream->size = 0;
									bind.stream->capacity = 256;

									if (data->sections[index].elements[subindex].stream_size > 0) {
										memcpy(bind.stream->buffer, pointer_data, sizeof(wchar_t) * data->sections[index].elements[subindex].stream_size);
										bind.stream->size = data->sections[index].elements[subindex].stream_size;
									}
									data->ui_reflection->BindInstanceDirectoryInput(instance, { &bind, 1 });
								}
							}
						}
					}
				}
			}

			UIDrawConfig config;
			for (size_t index = 0; index < data->sections.size; index++) {
				drawer.CollapsingHeader(data->sections[index].name, nullptr, [&]() {
					if (!initializer) {
						for (size_t subindex = 0; subindex < data->sections[index].elements.size; subindex++) {
							UIReflectionDrawInstanceOptions options;
							options.drawer = &drawer;
							options.config = &config;
							data->ui_reflection->DrawInstance(data->sections[index].elements[subindex].name, &options);
						}
					}
				});
			}

		}

		// -------------------------------------------------------------------------------------------------------

		void InjectWindowDestroyAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			// Window data is stored in additional data
			InjectWindowData* data = (InjectWindowData*)_additional_data;
			for (size_t index = 0; index < data->sections.size; index++) {
				// Release the instances and types
				for (size_t subindex = 0; subindex < data->sections[index].elements.size; subindex++) {
					data->ui_reflection->DestroyInstance(data->sections[index].elements[subindex].name);
					data->ui_reflection->DestroyType(data->sections[index].elements[subindex].name);
				}
			}
		}

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateInjectValuesWindow(UISystem* system, InjectWindowData data, Stream<char> window_name, bool is_pop_up_window) {
			constexpr float2 SIZE = { 1.0f, 1.0f };

			UIWindowDescriptor descriptor;

			descriptor.draw = InjectValuesWindowDraw;
			descriptor.destroy_action = InjectWindowDestroyAction;

			descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, SIZE.x);
			descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, SIZE.y);
			descriptor.initial_size_x = SIZE.x;
			descriptor.initial_size_y = SIZE.y;

			descriptor.window_data = &data;
			descriptor.window_data_size = sizeof(data);
			descriptor.window_name = window_name;

			return system->CreateWindowAndDockspace(descriptor, is_pop_up_window ? UI_DOCKSPACE_POP_UP_WINDOW : 0);
		}

		// -------------------------------------------------------------------------------------------------------

		void CreateInjectValuesAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			InjectValuesActionData* data = (InjectValuesActionData*)_data;
			unsigned int window_index = system->GetWindowFromName(data->name);
			if (window_index == -1) {
				CreateInjectValuesWindow(system, data->data, data->name, data->is_pop_up_window);
			}
			else {
				system->SetActiveWindow(window_index);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void TextInputWizardData::AddExtraElement(Action extra_element, void* data, size_t data_size)
		{
			ECS_ASSERT(extra_draw_element_count < ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_CAPACITY);

			extra_draw_elements[extra_draw_element_count] = extra_element;
			if (data_size == 0) {
				extra_draw_elements_data[extra_draw_element_count] = data;
			}
			else {
				extra_draw_elements_data[extra_draw_element_count] = nullptr;
				memcpy(function::OffsetPointer(extra_draw_elements_storage, extra_draw_element_storage_offset), data, data_size);
				extra_draw_elements_data_offset[extra_draw_element_count] = extra_draw_element_storage_offset;
				extra_draw_element_storage_offset += data_size;
				ECS_ASSERT(extra_draw_element_storage_offset <= ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_STORAGE_CAPACITY);
			}
			extra_draw_element_count++;
		}

		// -------------------------------------------------------------------------------------------------------

		struct VisualizeTextureWindowData {
			ECS_INLINE Texture2D TextureToCopy() const {
				return transferred_texture.Interface() != nullptr ? transferred_texture : original_texture;
			}

			ECS_INLINE void* AdditionalData() const {
				return is_additional_draw_data_ptr ? additional_draw_data_ptr : (void*)additional_draw_data;
			}

			void TransitionSelectionMode() {
				select_mode = !select_mode;
				if (select_mode) {
					select_texture_filter.size = 0;
					selected_element.size = 0;
				}
			}

			Texture2D original_texture;
			ResourceView texture_view;
			Texture2D transferred_texture;
			VisualizeTextureOptions visualize_options;

			bool automatic_update;
			bool is_additional_draw_data_ptr;
			bool display_options;
			bool select_mode;
			unsigned char additional_draw_combo_index;
			bool additional_draw_check_box_flag;
			CapacityStream<char> select_texture_filter;
			CapacityStream<char> selected_element;

			WindowDraw additional_draw;
			union {
				// Make the additional_draw_data embedded here such that to avoid
				// A new allocation being made
				size_t additional_draw_data[16];
				void* additional_draw_data_ptr;
			};
		};

		// -------------------------------------------------------------------------------------------------------

		static void UpdateVisualizeTextureWindowAdditionalDraw(
			VisualizeTextureWindowData* window_data,
			WindowDraw additional_draw,
			void* additional_draw_data,
			size_t additional_draw_data_size
		) {
			if (additional_draw_data != nullptr) {
				ECS_ASSERT(additional_draw_data_size < sizeof(window_data->additional_draw_data), "Additional draw for Visualize Texture too much byte data");

				if (additional_draw_data_size > 0) {
					memcpy(window_data->additional_draw_data, additional_draw_data, additional_draw_data_size);
				}
				else {
					window_data->additional_draw_data_ptr = additional_draw_data;
				}
				window_data->is_additional_draw_data_ptr = additional_draw_data_size == 0;
			}
			window_data->additional_draw = additional_draw != nullptr ? additional_draw : window_data->additional_draw;
		}

		// -------------------------------------------------------------------------------------------------------

		static void VisualizeTextureDeallocateTexture(UISystem* system, VisualizeTextureWindowData* data) {
			if (data->original_texture.Interface() != nullptr) {
				if (data->transferred_texture.Interface() != nullptr) {
					system->m_graphics->FreeResource(data->transferred_texture);
				}
				if (data->texture_view.Interface() != nullptr) {
					system->m_graphics->RemovePossibleResourceFromTracking(data->texture_view.Interface());
					system->m_graphics->RemovePossibleResourceFromTracking(data->texture_view.AsTexture2D().Interface());
					ReleaseGraphicsView(data->texture_view);
				}
			}
		}

		// -------------------------------------------------------------------------------------------------------

		static void VisualizeTextureCreateTexture(UISystem* system, VisualizeTextureWindowData* data, Texture2D target_texture, bool transfer_texture) {
			if (target_texture.Interface() != nullptr) {
				Texture2D texture_to_copy = target_texture;
				if (transfer_texture) {
					data->transferred_texture = system->m_graphics->TransferGPUResource(target_texture);
					texture_to_copy = data->transferred_texture;
				}
				else {
					data->transferred_texture.tex = nullptr;
				}

				Texture2D copied_texture = ConvertTextureToVisualize(system->m_graphics, texture_to_copy, &data->visualize_options);
				data->texture_view = CreateVisualizeTextureView(system->m_graphics, copied_texture);
				data->original_texture = target_texture;
			}
			else {
				data->texture_view = nullptr;
				data->transferred_texture.tex = nullptr;
				data->original_texture.tex = nullptr;
			}
		}

		// -------------------------------------------------------------------------------------------------------

		static void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			VisualizeTextureWindowData* window_data,
			const VisualizeTextureCreateData* create_data
		) {
			if (create_data->options != nullptr) {
				window_data->visualize_options = *create_data->options;
				window_data->visualize_options.copy_texture_if_same_format = true;
			}
			else {
				// Disable the previous override format
				window_data->visualize_options.override_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
			}

			if (create_data->texture.Interface() != nullptr && (create_data->texture.Interface() != window_data->original_texture.Interface())) {
				// We must free the old resources and create a new texture and view
				VisualizeTextureDeallocateTexture(system, window_data);
				VisualizeTextureCreateTexture(system, window_data, create_data->texture, create_data->transfer_texture_to_ui_graphics);
			}
			else if (window_data->original_texture.Interface() != nullptr) {
				ConvertTextureToVisualize(system->m_graphics, window_data->TextureToCopy(), window_data->texture_view.AsTexture2D(), &window_data->visualize_options);
			}

			UpdateVisualizeTextureWindowAdditionalDraw(window_data, create_data->additional_draw, create_data->additional_draw_data, create_data->additional_draw_data_size);
		}

		// -------------------------------------------------------------------------------------------------------

		void VisualizeTextureWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer) {
			UI_PREPARE_DRAWER(initializer);

			VisualizeTextureWindowData* data = (VisualizeTextureWindowData*)window_data;
			// Allow for a very large zoom
			// Do this every frame since when recovering from file this zoom will not be set
			drawer.system->SetWindowMaxZoom(drawer.window_index, 100.0f);

			/*const char* COMBO_INDEX_NAME = "COMBO_INDEX";

			unsigned char* combo_index = nullptr;
			if (initializer) {
				combo_index = (unsigned char*)drawer.GetMainAllocatorBufferAndStoreAsResource(COMBO_INDEX_NAME, sizeof(*combo_index));
				*combo_index = 0;
			}
			else {
				combo_index = (unsigned char*)drawer.GetResource(COMBO_INDEX_NAME);
			}*/

			if (initializer) {
				const size_t SELECTION_FILTER_CAPACITY = 256;
				void* selection_filter_memory = drawer.GetMainAllocatorBuffer(sizeof(char) * SELECTION_FILTER_CAPACITY);
				data->select_texture_filter.InitializeFromBuffer(selection_filter_memory, 0, SELECTION_FILTER_CAPACITY);

				const size_t SELECTED_ELEMENT_CAPACITY = 256;
				void* selected_element_memory = drawer.GetMainAllocatorBuffer(sizeof(char) * SELECTED_ELEMENT_CAPACITY);
				data->selected_element.InitializeFromBuffer(selected_element_memory, 0, SELECTED_ELEMENT_CAPACITY);
			}

			Stream<char> SELECT_BUTTON_CHARACTERS = "Select...";

			VisualizeTextureAdditionalDrawData additional_data;
			additional_data.can_display_texture = true;

			ECS_STACK_CAPACITY_STREAM(char, combo_label_memory, 512);
			ECS_STACK_CAPACITY_STREAM(Stream<char>, combo_labels, 32);
			ECS_STACK_VOID_STREAM(combo_callback_memory, 512);
			ECS_STACK_VOID_STREAM(check_box_callback_memory, 512);

			// Allocate these from the temp buffer - these will survive until the next window draw
			// So the data is stable for the clickable detection
			const size_t SELECT_ELEMENT_CAPACITY = ECS_KB;
			void* select_elements_buffer = drawer.GetTempBuffer(sizeof(VisualizeTextureSelectElement) * SELECT_ELEMENT_CAPACITY);
			const size_t SELECT_ELEMENT_NAME_CAPACITY = ECS_KB * 16;
			void* select_elements_name_buffer = drawer.GetTempBuffer(sizeof(char) * SELECT_ELEMENT_NAME_CAPACITY);

			CapacityStream<VisualizeTextureSelectElement> select_elements;
			select_elements.InitializeFromBuffer(select_elements_buffer, 0, SELECT_ELEMENT_CAPACITY);
			CapacityStream<char> select_elements_name;
			select_elements_name.InitializeFromBuffer(select_elements_name_buffer, 0, SELECT_ELEMENT_NAME_CAPACITY);

			if (data->additional_draw != nullptr) {
				// Reduce the capacity by one in case we need to include the select label
				combo_labels.capacity--;

				additional_data.window_data = data;
				additional_data.drawer = &drawer;
				additional_data.private_data = data->AdditionalData();
				additional_data.window_name = drawer.system->GetWindowName(drawer.window_index);

				if (data->select_mode) {
					additional_data.select_elements = &select_elements;
					additional_data.select_element_name_memory = &select_elements_name;
					additional_data.combo_index = nullptr;
					additional_data.combo_labels = nullptr;
					additional_data.combo_memory = nullptr;
					additional_data.combo_callback_memory = nullptr;
					additional_data.check_box_callback_memory = nullptr;
					additional_data.check_box_flag = nullptr;
					additional_data.is_select_mode = true;
				}
				else {
					additional_data.select_elements = nullptr;
					additional_data.select_element_name_memory = nullptr;
					additional_data.combo_index = &data->additional_draw_combo_index;
					additional_data.combo_labels = &combo_labels;
					additional_data.combo_memory = &combo_label_memory;
					additional_data.combo_callback_memory = &combo_callback_memory;
					additional_data.check_box_callback_memory = &check_box_callback_memory;
					additional_data.check_box_flag = &data->additional_draw_check_box_flag;
					additional_data.is_select_mode = false;
				}

				data->additional_draw(&additional_data, nullptr, initializer);

				if (additional_data.include_select_label) {
					combo_labels.AddSafe(SELECT_BUTTON_CHARACTERS);
				}
			}

			if (!initializer) {
				UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();

				UIDrawConfig config;
				if (data->select_mode) {
					UIConfigBorder border;
					border.color = drawer.color_theme.borders;

					UIConfigWindowDependentSize dependent_size;
					drawer.SetCurrentPositionToHeader();
					dependent_size.scale_factor = drawer.GetWindowSizeScaleUntilBorder(true);

					UIConfigTextInputHint filter_hint;
					filter_hint.characters = "Search";

					config.AddFlag(border);
					config.AddFlag(dependent_size);
					config.AddFlag(filter_hint);
					drawer.TextInput(
						UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_INPUT_NO_NAME
						| UI_CONFIG_BORDER,
						config,
						"TextInput",
						&data->select_texture_filter
					);

					drawer.SetCurrentPositionToStart();
					drawer.NextRow(2.0f);

					// Draw each entry
					drawer.SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

					UIConfigWindowDependentSize label_size;
					config.flag_count = 0;
					config.AddFlag(label_size);

					struct SelectActionData {
						VisualizeTextureWindowData* window_data;
						Stream<VisualizeTextureSelectElement> elements;
						unsigned int select_index;
					};

					auto select_action = [](ActionData* action_data) {
						UI_UNPACK_ACTION_DATA;

						SelectActionData* data = (SelectActionData*)_data;

						VisualizeTextureSelectElement select_element = data->elements[data->select_index];
						ChangeVisualizeTextureWindowOptions(system, data->window_data, &select_element);
					};

					SelectActionData select_data;
					select_data.elements = select_elements;
					select_data.window_data = data;
					select_data.select_index = 0;

					UIActionHandler select_handler = { select_action, &select_data, sizeof(select_data) };

					bool is_active_window = drawer.IsActiveWindow();
					bool up_key = is_active_window && drawer.system->m_keyboard->IsDown(ECS_KEY_UP);
					bool down_key = is_active_window && drawer.system->m_keyboard->IsDown(ECS_KEY_DOWN);

					unsigned int found_selected = -1;
					unsigned int previous_filtered = -1;
					bool get_down_label = false;

					for (unsigned int index = 0; index < select_elements.size; index++) {
						bool matches_filter = data->select_texture_filter.size == 0 ?
							true :
							function::FindFirstToken(select_elements[index].name, data->select_texture_filter).size > 0;
						if (matches_filter) {
							if (found_selected == -1) {
								if (select_elements[index].name == data->selected_element) {
									found_selected = index;
									if (up_key && previous_filtered != -1) {
										data->selected_element.CopyOther(select_elements[previous_filtered].name);
									}
									else if (down_key) {
										get_down_label = true;
									}
								}
							}
							previous_filtered = index;
							if (get_down_label) {
								data->selected_element.CopyOther(select_elements[index].name);
								get_down_label = false;
							}

							size_t label_transparent = found_selected == index ? 0 : UI_CONFIG_LABEL_TRANSPARENT;
							drawer.Button(label_transparent | UI_CONFIG_WINDOW_DEPENDENT_SIZE
								| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, select_elements[index].name, select_handler);
						}
						select_data.select_index++;
					}

					// Check if the ENTER key is pressed when we are active window
					if (is_active_window && drawer.system->m_keyboard->IsPressed(ECS_KEY_ENTER)) {
						// Verify if we have an active label
						if (found_selected != -1) {
							ChangeVisualizeTextureWindowOptions(drawer.system, data, &select_elements[found_selected]);
						}
					}

					if (select_elements.size == 0) {
						drawer.Text("No select entries");
					}
				}
				else {
					if (data->original_texture.Interface() != nullptr && data->texture_view.Interface() != nullptr) {
						if (data->automatic_update) {
							ConvertTextureToVisualize(drawer.system->m_graphics, data->TextureToCopy(), data->texture_view.AsTexture2D(), &data->visualize_options);
						}

						if (additional_data.can_display_texture) {
							UIConfigAbsoluteTransform whole_transform = drawer.GetWholeRegionTransform(true);
							config.AddFlag(whole_transform);

							drawer.SpriteRectangle(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, config, data->texture_view);
							config.flag_count = 0;
						}

						if (data->display_options) {
							bool has_additional_draw = data->additional_draw != nullptr;

							UIConfigSpriteButtonBackground sprite_background;
							sprite_background.overwrite_color = drawer.color_theme.theme;
							const float sprite_background_size = drawer.GetRelativeElementSize(sprite_background.scale_factor).y;

							// Red
							row_layout.AddSquareLabel();
							//row_layout.AddBorderToLastElement();
							// Green
							row_layout.AddSquareLabel();
							//row_layout.AddBorderToLastElement();
							// Blue
							row_layout.AddSquareLabel();
							//row_layout.AddBorderToLastElement();
							// Alpha
							row_layout.AddSquareLabel();
							//row_layout.AddBorderToLastElement();

							if (has_additional_draw) {
								// Optional control
								if (additional_data.check_box_name.size > 0) {
									row_layout.AddCheckBox(additional_data.check_box_name);
								}

								// Optional control
								if (!additional_data.hide_select_button) {
									row_layout.AddLabel(SELECT_BUTTON_CHARACTERS, ECS_UI_ALIGN_MIDDLE);
								}

								// Optional control
								if (combo_labels.size > 0) {
									row_layout.AddComboBox(combo_labels, {}, {}, ECS_UI_ALIGN_RIGHT);
								}
							}

							struct ChangeChannelFlagActionData {
								VisualizeTextureWindowData* data;
								unsigned char flag_offset;
							};

							auto change_channel_flag_action = [](ActionData* action_data) {
								UI_UNPACK_ACTION_DATA;

								ChangeChannelFlagActionData* data = (ChangeChannelFlagActionData*)_data;
								bool* flags = &data->data->visualize_options.enable_red;
								flags[data->flag_offset] = !flags[data->flag_offset];
								ConvertTextureToVisualize(system->m_graphics, data->data->TextureToCopy(), data->data->texture_view.AsTexture2D(), &data->data->visualize_options);
							};

							ChangeChannelFlagActionData change_channel_data;
							change_channel_data.data = data;
							change_channel_data.flag_offset = 0;
							UIActionHandler change_flag_handler = { change_channel_flag_action, &change_channel_data, sizeof(change_channel_data) };

							size_t configuration = 0;

							auto draw_flag = [&](Color color) {
								bool* flags = &data->visualize_options.enable_red;
								if (flags[change_channel_data.flag_offset]) {
									config.AddFlag(sprite_background);
									configuration |= UI_CONFIG_SPRITE_BUTTON_BACKGROUND;
								}

								row_layout.GetTransform(config, configuration);
								drawer.SpriteButton(configuration, config, change_flag_handler, ECS_TOOLS_UI_TEXTURE_MASK, color);

								configuration = 0;
								config.flag_count = 0;
								change_channel_data.flag_offset++;
							};

							draw_flag(Color(255, 0, 0));
							draw_flag(Color(0, 255, 0));
							draw_flag(Color(0, 0, 255));
							draw_flag(Color(255, 255, 255));
							// For the alpha control always draw with a background since on white images it will not be distinguishable

							if (has_additional_draw) {
								if (additional_data.check_box_name.size > 0) {
									configuration = 0;
									config.flag_count = 0;

									ECS_ASSERT(additional_data.check_box_callback.action != nullptr);
									UIConfigCheckBoxCallback callback;
									callback.handler = additional_data.check_box_callback;
									config.AddFlag(callback);
									configuration |= UI_CONFIG_CHECK_BOX_CALLBACK;

									row_layout.GetTransform(config, configuration);
									drawer.CheckBox(configuration | UI_CONFIG_LATE_DRAW, config, additional_data.check_box_name, &data->additional_draw_check_box_flag);
								}

								if (!additional_data.hide_select_button) {
									configuration = 0;
									config.flag_count = 0;

									auto select_button = [](ActionData* action_data) {
										UI_UNPACK_ACTION_DATA;

										VisualizeTextureWindowData* data = (VisualizeTextureWindowData*)_data;
										data->TransitionSelectionMode();
									};

									row_layout.GetTransform(config, configuration);
									drawer.Button(
										configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LATE_DRAW,
										config,
										SELECT_BUTTON_CHARACTERS,
										{ select_button, data, 0 }
									);
								}

								if (combo_labels.size > 0) {
									configuration = 0;
									config.flag_count = 0;

									struct ComboWrapperData {
										VisualizeTextureWindowData* window_data;
										UIActionHandler user_handler;
										unsigned char* flag_index;
										unsigned char last_label_index;
										bool has_include_select;
									};

									auto combo_wrapper = [](ActionData* action_data) {
										UI_UNPACK_ACTION_DATA;

										ComboWrapperData* data = (ComboWrapperData*)_data;
										if (*data->flag_index == data->last_label_index && data->has_include_select) {
											// We just need to transition to the SelectMode
											data->window_data->TransitionSelectionMode();
										}
										else {
											// Call the user_handler
											void* user_data = data->user_handler.data_size == 0 ?
												data->user_handler.data : function::OffsetPointer(data, sizeof(*data));
											action_data->data = user_data;
											data->user_handler.action(action_data);
										}
									};

									size_t _combo_wrapper_data[128];
									ComboWrapperData* combo_wrapper_data = (ComboWrapperData*)_combo_wrapper_data;
									combo_wrapper_data->flag_index = &data->additional_draw_combo_index;
									combo_wrapper_data->has_include_select = additional_data.include_select_label;
									combo_wrapper_data->last_label_index = additional_data.combo_labels->size - 1;
									combo_wrapper_data->window_data = data;
									combo_wrapper_data->user_handler = additional_data.combo_callback;
									unsigned int copy_size = sizeof(*combo_wrapper_data);
									if (additional_data.combo_callback.data_size > 0) {
										ECS_ASSERT(additional_data.combo_callback.data_size + sizeof(*combo_wrapper_data) <= sizeof(_combo_wrapper_data));
										memcpy(
											function::OffsetPointer(combo_wrapper_data, sizeof(*combo_wrapper_data)),
											additional_data.combo_callback.data,
											additional_data.combo_callback.data_size
										);
										copy_size += additional_data.combo_callback.data_size;
									}
									configuration |= UI_CONFIG_COMBO_BOX_CALLBACK;

									UIConfigComboBoxCallback combo_callback;
									combo_callback.handler = { combo_wrapper, combo_wrapper_data, copy_size };
									combo_callback.copy_on_initialization = false;
									config.AddFlag(combo_callback);
									row_layout.GetTransform(config, configuration);

									drawer.ComboBox(
										configuration | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_LATE_DRAW,
										config,
										"Combo",
										additional_data.combo_labels->ToStream(),
										16,
										&data->additional_draw_combo_index
									);
								}
							}
						}
					}
					else {
						drawer.Text("No texture selected");
					}
				}
			}
		}

		void VisualizeTextureDestroyAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIActionHandler* data = (UIActionHandler*)_data;
			VisualizeTextureWindowData* window_data = (VisualizeTextureWindowData*)_additional_data;
			VisualizeTextureDeallocateTexture(system, window_data);

			if (data->action != nullptr) {
				action_data->data = data->data_size > 0 ? function::OffsetPointer(data, sizeof(*data)) : data->data;
				data->action(action_data);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void VisualizeTexturePrivateWindow(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;
			
			VisualizeTextureWindowData* window_data = (VisualizeTextureWindowData*)_additional_data;
			unsigned int active_window_index = system->GetActiveWindow();
			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			if (window_index == active_window_index) {
				if (keyboard->IsPressed(ECS_KEY_O)) {
					window_data->display_options = !window_data->display_options;
				}
			}
		}

		// -------------------------------------------------------------------------------------------------------

		UIWindowDescriptor VisualizeTextureWindowDescriptor(UISystem* system, const VisualizeTextureCreateData* create_data, void* stack_memory)
		{
			constexpr float2 SIZE = { 1.0f, 1.0f };

			UIWindowDescriptor descriptor;

			descriptor.draw = VisualizeTextureWindowDraw;
			descriptor.destroy_action = VisualizeTextureDestroyAction;
			descriptor.private_action = VisualizeTexturePrivateWindow;

			descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, SIZE.x);
			descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, SIZE.y);
			descriptor.initial_size_x = SIZE.x;
			descriptor.initial_size_y = SIZE.y;

			// Allow the texture to be nullptr in order to have it be reconstructed from an UI file
			VisualizeTextureWindowData* data = (VisualizeTextureWindowData*)stack_memory;
			data->texture_view = nullptr;
			data->transferred_texture = (ID3D11Texture2D*)nullptr;
			data->additional_draw = nullptr;
			data->automatic_update = false;
			data->additional_draw_combo_index = 0;
			data->display_options = true;
			data->select_mode = create_data->select_mode;
			data->additional_draw_check_box_flag = false;
			if (create_data->texture.Interface() != nullptr) {
				if (create_data->options != nullptr) {
					data->visualize_options = *create_data->options;
				}
				else {
					data->visualize_options = VisualizeTextureOptions();
				}
				data->visualize_options.copy_texture_if_same_format = true;
				data->automatic_update = create_data->automatic_update;

				VisualizeTextureCreateTexture(system, data, create_data->texture, create_data->transfer_texture_to_ui_graphics);
			}
			else {
				data->visualize_options = VisualizeTextureOptions();
				data->visualize_options.copy_texture_if_same_format = true;
			}

			UpdateVisualizeTextureWindowAdditionalDraw(data, create_data->additional_draw, create_data->additional_draw_data, create_data->additional_draw_data_size);

			descriptor.window_data = data;
			descriptor.window_data_size = sizeof(*data);
			descriptor.window_name = create_data->window_name;

			// Embed the data directly here if needed
			size_t destroy_data_size = sizeof(UIActionHandler) + create_data->destroy_window_handler.data_size;

			void* destroy_data = function::OffsetPointer(stack_memory, sizeof(*data));
			memcpy(destroy_data, &create_data->destroy_window_handler, sizeof(create_data->destroy_window_handler));
			if (create_data->destroy_window_handler.data_size > 0) {
				memcpy(function::OffsetPointer(destroy_data, sizeof(create_data->destroy_window_handler)), create_data->destroy_window_handler.data, create_data->destroy_window_handler.data_size);
			}

			descriptor.destroy_action_data = destroy_data;
			descriptor.destroy_action_data_size = destroy_data_size;

			return descriptor;
		}

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateVisualizeTextureWindow(UISystem* system, const VisualizeTextureCreateData* create_data)
		{
			size_t stack_memory[256];
			UIWindowDescriptor descriptor = VisualizeTextureWindowDescriptor(system, create_data, stack_memory);

			unsigned int window_index = system->CreateWindowAndDockspace(
				descriptor, 
				(create_data->is_pop_up_window ? UI_DOCKSPACE_POP_UP_WINDOW : 0)
			);
			return window_index;
		}

		// -------------------------------------------------------------------------------------------------------

		void CreateVisualizeTextureAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			const VisualizeTextureCreateData* data = (const VisualizeTextureCreateData*)_data;
			unsigned int window_index = system->GetWindowFromName(data->window_name);
			if (window_index == -1) {
				CreateVisualizeTextureWindow(system, data);
			}
			else {
				system->SetActiveWindow(window_index);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void ChangeVisualizeTextureWindowOptions(
			UISystem* system, 
			Stream<char> window_name, 
			const VisualizeTextureCreateData* create_data
		)
		{
			unsigned int window_index = system->GetWindowFromName(window_name);
			if (window_index != -1) {
				ChangeVisualizeTextureWindowOptions(system, window_index, create_data);
			}
			else {
				ECS_ASSERT(false, "Invalid window name for VisualizeTexture UI window");
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			unsigned int window_index,
			const VisualizeTextureCreateData* create_data
		) {
			VisualizeTextureWindowData* window_data = (VisualizeTextureWindowData*)system->GetWindowData(window_index);
			ChangeVisualizeTextureWindowOptions(system, window_data, create_data);
		}

		// -------------------------------------------------------------------------------------------------------

		void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			Stream<char> window_name,
			const VisualizeTextureSelectElement* select_element
		) {
			ChangeVisualizeTextureWindowOptions(system, system->GetWindowFromName(window_name), select_element);
		}
		
		// -------------------------------------------------------------------------------------------------------

		void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			unsigned int window_index,
			const VisualizeTextureSelectElement* select_element
		) {
			ChangeVisualizeTextureWindowOptions(system, system->GetWindowData(window_index), select_element);
		}

		void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			void* window_data,
			const VisualizeTextureSelectElement* select_element
		) {
			VisualizeTextureWindowData* data = (VisualizeTextureWindowData*)window_data;

			VisualizeTextureCreateData create_data;
			create_data.texture = select_element->texture;
			create_data.transfer_texture_to_ui_graphics = select_element->transfer_texture_to_ui_graphics;
			create_data.additional_draw = data->additional_draw;

			VisualizeTextureOptions visualize_options = data->visualize_options;
			create_data.options = &visualize_options;
			visualize_options.format_conversion = select_element->format_conversion;
			visualize_options.override_format = select_element->override_format;
			ChangeVisualizeTextureWindowOptions(system, data, &create_data);

			if (data->select_mode) {
				data->TransitionSelectionMode();
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void* GetVisualizeTextureWindowAdditionalData(UISystem* system, Stream<char> window_name)
		{
			unsigned int window_index = system->GetWindowFromName(window_name);
			if (window_index != -1) {
				return GetVisualizeTextureWindowAdditionalData(system, window_index);
			}
			else {
				ECS_ASSERT(false, "Invalid window name for VisualizeTexture UI window");
				return nullptr;
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void* GetVisualizeTextureWindowAdditionalData(UISystem* system, unsigned int window_index)
		{
			VisualizeTextureWindowData* window_data = (VisualizeTextureWindowData*)system->GetWindowData(window_index);
			return window_data->AdditionalData();
		}

		// -------------------------------------------------------------------------------------------------------

		void UpdateVisualizeTextureWindowAdditionalDraw(
			UISystem* system, 
			Stream<char> window_name, 
			WindowDraw additional_draw, 
			void* additional_draw_data, 
			size_t additional_draw_data_size
		)
		{
			unsigned int window_index = system->GetWindowFromName(window_name);
			if (window_index != -1) {
				UpdateVisualizeTextureWindowAdditionalDraw(system, window_index, additional_draw, additional_draw_data, additional_draw_data_size);
			}
			else {
				ECS_ASSERT(false, "Invalid window name for VisualizeTexture UI window");
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void UpdateVisualizeTextureWindowAdditionalDraw(
			UISystem* system, 
			unsigned int window_index, 
			WindowDraw additional_draw,
			void* additional_draw_data,
			size_t additional_draw_data_size
		)
		{
			VisualizeTextureWindowData* window_data = (VisualizeTextureWindowData*)system->GetWindowData(window_index);
			UpdateVisualizeTextureWindowAdditionalDraw(window_data, additional_draw, additional_draw_data, additional_draw_data_size);
		}

		// -------------------------------------------------------------------------------------------------------

		void UpdateVisualizeTextureWindowAutomaticRefresh(UISystem* system, void* window_data, bool automatic_refresh)
		{
			VisualizeTextureWindowData* data = (VisualizeTextureWindowData*)window_data;
			data->automatic_update = automatic_refresh;
			if (automatic_refresh) {
				if (data->original_texture.Interface() != nullptr) {
					ConvertTextureToVisualize(system->m_graphics, data->TextureToCopy(), data->texture_view.AsTexture2D(), &data->visualize_options);
				}
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void TransitionVisualizeTextureWindowSelection(
			void* window_data
		) {
			VisualizeTextureWindowData* data = (VisualizeTextureWindowData*)window_data;
			data->TransitionSelectionMode();
		}

		// -------------------------------------------------------------------------------------------------------

		void TransitionVisualizeTextureWindowSelection(
			UISystem* system,
			unsigned int window_index
		) {
			TransitionVisualizeTextureWindowSelection(system->GetWindowData(window_index));
		}

		// -------------------------------------------------------------------------------------------------------

		void UIDrawerOKCancelRow(UIDrawer& drawer, Stream<char> ok_label, Stream<char> cancel_label, UIActionHandler ok_handler, UIActionHandler cancel_handler)
		{
			UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();

			row_layout.AddLabel(ok_label);
			row_layout.AddLabel(cancel_label, ECS_UI_ALIGN_RIGHT);

			UIDrawConfig config;
			size_t configuration = 0;

			row_layout.GetTransform(config, configuration);

			struct WrapperData {
				UIActionHandler handler_data;
			};

			auto wrapper = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				WrapperData* wrapper_data = (WrapperData*)_data;
				void* callback_data = wrapper_data->handler_data.data_size == 0 ? wrapper_data->handler_data.data : function::OffsetPointer(wrapper_data, sizeof(wrapper_data));
				action_data->data = callback_data;
				wrapper_data->handler_data.action(action_data);
				system->PushDestroyWindowHandler(system->GetWindowIndexFromBorder(dockspace, border_index));
			};

			ECS_STACK_CAPACITY_STREAM(size_t, wrapper_data_storage, 512);
			WrapperData* wrapper_data = (WrapperData*)wrapper_data_storage.buffer;
			wrapper_data->handler_data = ok_handler;
			if (ok_handler.data_size > 0) {
				memcpy(function::OffsetPointer(wrapper_data, sizeof(*wrapper_data)), ok_handler.data, ok_handler.data_size);
			}
			drawer.Button(configuration, config, ok_label, UIActionHandler{wrapper, wrapper_data, (unsigned int)sizeof(*wrapper_data) + ok_handler.data_size });

			wrapper_data->handler_data = cancel_handler;
			if (cancel_handler.data_size > 0) {
				memcpy(function::OffsetPointer(wrapper_data, sizeof(*wrapper_data)), cancel_handler.data, cancel_handler.data_size);
			}

			config.flag_count = 0;
			configuration = 0;
			row_layout.GetTransform(config, configuration);
			drawer.Button(configuration, config, cancel_label, UIActionHandler{ wrapper, wrapper_data, (unsigned int)sizeof(*wrapper_data) + cancel_handler.data_size });
		}

		// -------------------------------------------------------------------------------------------------------

	}

}