#include "ecspch.h"
#include "UIDrawerWindows.h"
#include "../Internal/Multithreading/TaskManager.h"

namespace ECSEngine {

	namespace Tools {

		constexpr float2 CONSOLE_WINDOW_SIZE = { 1.0f, 0.4f };

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterReturnToDefaultButton(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIParameterWindowReturnToDefaultButtonData* data = (UIParameterWindowReturnToDefaultButtonData*)_data;
			if (data->is_system_theme) {
				memcpy(data->system_descriptor, data->default_descriptor, data->descriptor_size);
				if ((unsigned int)data->descriptor_index < (unsigned int)UIWindowDrawerDescriptorIndex::Count) {
					switch (data->descriptor_index) {
					case UIWindowDrawerDescriptorIndex::ColorTheme:
						system->FinalizeColorTheme();
						break;
					case UIWindowDrawerDescriptorIndex::Layout:
						system->FinalizeLayout();
						break;
					case UIWindowDrawerDescriptorIndex::Element:
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

		template<bool initialize>
		void WindowParameterColorTheme(UIWindowDrawerDescriptor* descriptor, UIDrawer<initialize>& drawer) {
			constexpr size_t input_configuration = UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE | UI_CONFIG_COLOR_INPUT_CALLBACK;

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
			button_data.descriptor_index = UIWindowDrawerDescriptorIndex::ColorTheme;
			button_data.is_system_theme = false;
			button_data.window_descriptor = descriptor;
			drawer.Button("Default values##0", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.ColorInput<input_configuration>(config, "Theme", &theme->theme, system_theme->theme);

			color_input_callback.callback = { WindowParameterColorInputCallback, descriptor, 0 };
			drawer.ColorInput<input_configuration>(config, "Text", &theme->default_text, system_theme->default_text);
			drawer.ColorInput<input_configuration>(config, "Graph hover line", &theme->graph_hover_line, system_theme->graph_hover_line);
			drawer.ColorInput<input_configuration>(config, "Graph line", &theme->graph_line, system_theme->graph_line);
			drawer.ColorInput<input_configuration>(config, "Graph sample circle", &theme->graph_sample_circle, system_theme->graph_sample_circle);
			drawer.ColorInput<input_configuration>(config, "Histogram", &theme->histogram_color, system_theme->histogram_color);
			drawer.ColorInput<input_configuration>(config, "Histogram hovered", &theme->histogram_hovered_color, system_theme->histogram_hovered_color);
			drawer.ColorInput<input_configuration>(config, "Histogram text", &theme->histogram_text_color, system_theme->histogram_text_color);
			drawer.ColorInput<input_configuration>(config, "Unavailable text", &theme->unavailable_text, system_theme->unavailable_text);
		}

		template ECSENGINE_API void WindowParameterColorTheme<true>(UIWindowDrawerDescriptor*, UIDrawer<true>&);
		template ECSENGINE_API void WindowParameterColorTheme<false>(UIWindowDrawerDescriptor*, UIDrawer<false>&);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void WindowParametersLayout(UIWindowDrawerDescriptor* descriptor, UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK

			UIDrawConfig config;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { WindowParameterLayoutCallback, descriptor, 0 };
			config.AddFlag(callback);
			auto system = drawer.GetSystem();

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_descriptors.window_layout;
			button_data.descriptor_size = sizeof(UILayoutDescriptor);
			button_data.descriptor_index = UIWindowDrawerDescriptorIndex::Layout;
			button_data.is_system_theme = false;
			button_data.window_descriptor = descriptor;
			drawer.Button("Default values##1", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			UILayoutDescriptor* layout = &descriptor->layout;
			const UILayoutDescriptor* system_layout = &system->m_descriptors.window_layout;
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Element x", &layout->default_element_x, 0.01f, 0.3f, system_layout->default_element_x, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Element y", &layout->default_element_y, 0.01f, 0.15f, system_layout->default_element_y, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Indentation", &layout->element_indentation, 0.0f, 0.05f, system_layout->element_indentation, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Next row padding", &layout->next_row_padding, 0.0f, 0.05f, system_layout->next_row_padding, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Next row offset", &layout->next_row_y_offset, 0.0f, 0.05f, system_layout->next_row_y_offset, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Node indentation", &layout->node_indentation, 0.0f, 0.05f, system_layout->node_indentation, 3);

#undef SLIDER_CONFIGURATION
		}

		template ECSENGINE_API void WindowParametersLayout<true>(UIWindowDrawerDescriptor*, UIDrawer<true>&);
		template ECSENGINE_API void WindowParametersLayout<false>(UIWindowDrawerDescriptor*, UIDrawer<false>&);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void WindowParametersElementDescriptor(UIWindowDrawerDescriptor* descriptor, UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK

			UIDrawConfig config;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { WindowParameterElementDescriptorCallback, descriptor, 0 };
			config.AddFlag(callback);
			auto system = drawer.GetSystem();

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_descriptors.element_descriptor;
			button_data.descriptor_size = sizeof(UIElementDescriptor);
			button_data.descriptor_index = UIWindowDrawerDescriptorIndex::Element;
			button_data.is_system_theme = false;
			button_data.window_descriptor = descriptor;
			drawer.Button("Default values##2", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			UIElementDescriptor* elements = &descriptor->element_descriptor;
			UIElementDescriptor* system_elements = &system->m_descriptors.element_descriptor;
			drawer.PushIdentifierStack("##");
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Color input padd", &elements->color_input_padd, 0.0f, 0.01f, system_elements->color_input_padd, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Combo box padd", &elements->combo_box_padding, 0.0f, 0.02f, system_elements->combo_box_padding, 3);
			float* float2_values[2];
			float float2_lower_bounds[1];
			float float2_upper_bounds[1];
			float* float2_default_values;
			const char* float2_names[2];

			auto float2_sliders = [&](const char* group_name, size_t index) {
				drawer.PushIdentifierStackRandom(index);
				drawer.FloatSliderGroup<SLIDER_CONFIGURATION | UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS>(
					config,
					2,
					group_name,
					float2_names,
					float2_values,
					float2_lower_bounds,
					float2_upper_bounds,
					float2_default_values,
					3
					);
				drawer.PopIdentifierStack();
			};

			float2_values[0] = &elements->graph_axis_bump.x;
			float2_values[1] = &elements->graph_axis_bump.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.01f;
			float2_default_values = &system_elements->graph_axis_bump.x;
			float2_names[0] = "x:";
			float2_names[1] = "y:";
			float2_sliders("Graph axis bump", 0);

			float2_values[0] = &elements->graph_axis_value_line_size.x;
			float2_values[1] = &elements->graph_axis_value_line_size.y;
			float2_upper_bounds[0] = 0.02f;
			float2_default_values = &system_elements->graph_axis_value_line_size.x;
			float2_sliders("Graph axis value line size", 1);

			float2_values[0] = &elements->graph_padding.x;
			float2_values[1] = &elements->graph_padding.y;
			float2_upper_bounds[0] = 0.02f;
			float2_default_values = &system_elements->graph_padding.x;
			float2_sliders("Graph padding", 2);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Graph reduce font",
				&elements->graph_reduce_font,
				0.5f,
				1.0f,
				system_elements->graph_reduce_font,
				3
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Graph sample circle size",
				&elements->graph_sample_circle_size,
				0.003f,
				0.02f,
				system_elements->graph_sample_circle_size,
				3
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Graph x axis space",
				&elements->graph_x_axis_space,
				0.005f,
				0.05f,
				system_elements->graph_x_axis_space,
				4
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Histogram bar min scale",
				&elements->histogram_bar_min_scale,
				0.01f,
				0.2f,
				system_elements->histogram_bar_min_scale
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Histogram bar spacing",
				&elements->histogram_bar_spacing,
				0.0028f,
				0.01f,
				system_elements->histogram_bar_spacing,
				3
				);

			float2_values[0] = &elements->histogram_padding.x;
			float2_values[1] = &elements->histogram_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.1f;
			float2_default_values = &system_elements->histogram_padding.x;
			float2_sliders("Histogram padding", 3);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Histogram reduce font",
				&elements->histogram_reduce_font,
				0.5f,
				1.0f,
				system_elements->histogram_reduce_font,
				3
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Menu button padding",
				&elements->menu_button_padding,
				0.0f,
				0.1f,
				system_elements->menu_button_padding,
				4
			);

			float2_values[0] = &elements->label_horizontal_padd;
			float2_values[1] = &elements->label_vertical_padd;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.2f;
			float2_default_values = &system_elements->label_horizontal_padd;
			float2_sliders("Label padding", 4);

			float2_values[0] = &elements->slider_length.x;
			float2_values[1] = &elements->slider_length.y;
			float2_lower_bounds[0] = 0.01f;
			float2_upper_bounds[0] = 0.1f;
			float2_default_values = &system_elements->slider_length.x;
			float2_sliders("Slider length", 5);

			float2_values[0] = &elements->slider_padding.x;
			float2_values[1] = &elements->slider_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.05f;
			float2_default_values = &system_elements->slider_padding.x;
			float2_sliders("Slider padding", 6);

			float2_values[0] = &elements->slider_shrink.x;
			float2_values[1] = &elements->slider_shrink.y;
			float2_default_values = &system_elements->slider_shrink.x;
			float2_sliders("Slider shrink", 7);

			float2_values[0] = &elements->text_input_padding.x;
			float2_values[1] = &elements->text_input_padding.y;
			float2_upper_bounds[0] = 0.1f;
			float2_default_values = &system_elements->text_input_padding.x;
			float2_sliders("Text input padding", 8);

			drawer.PopIdentifierStack();
#undef SLIDER_CONFIGURATION
		}

		template ECSENGINE_API void WindowParametersElementDescriptor<true>(UIWindowDrawerDescriptor*, UIDrawer<true>&);
		template ECSENGINE_API void WindowParametersElementDescriptor<false>(UIWindowDrawerDescriptor*, UIDrawer<false>&);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void WindowParameterDraw(void* window_data, void* drawer_descriptor) {
			UI_PREPARE_DRAWER(initialize);

			UIWindowDrawerDescriptor* descriptors = (UIWindowDrawerDescriptor*)window_data;
			drawer.SetDrawMode(UIDrawerMode::NextRow);
			auto color_theme_lambda = [&]() {
				WindowParameterColorTheme<initialize>(descriptors, drawer);
			};
			drawer.CollapsingHeader("Color theme", color_theme_lambda);

			auto layout_lambda = [&]() {
				WindowParametersLayout<initialize>(descriptors, drawer);
			};

			drawer.CollapsingHeader("Layout", layout_lambda);

			auto element_lambda = [&]() {
				WindowParametersElementDescriptor<initialize>(descriptors, drawer);
			};

			drawer.CollapsingHeader("Element descriptor", element_lambda);
		}

		template ECSENGINE_API void WindowParametersElementDescriptor<true>(UIWindowDrawerDescriptor*, UIDrawer<true>&);
		template ECSENGINE_API void WindowParametersElementDescriptor<false>(UIWindowDrawerDescriptor*, UIDrawer<false>&);

		// --------------------------------------------------------------------------------------------------------------

		void OpenWindowParameters(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			UIWindowDescriptor descriptor;
			descriptor.draw = WindowParameterDraw<false>;
			descriptor.initialize = WindowParameterDraw<true>;
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

			const char* window_name = system->GetWindowName(window_index);
			char* new_name = (char*)system->m_memory->Allocate(sizeof(char) * 64, alignof(char));
			size_t window_name_size = strlen(window_name);
			memcpy(new_name, window_name, window_name_size);
			new_name[window_name_size] = '\0';
			strcat(new_name, " Parameters");

			descriptor.window_name = new_name;
			window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW);

			// if it is desired to be destroyed when going out of focus
			UIPopUpWindowData system_handler_data;
			system_handler_data.is_fixed = true;
			system_handler_data.is_initialized = true;
			system_handler_data.destroy_at_first_click = false;
			system_handler_data.name = new_name;
			system_handler_data.reset_when_window_is_destroyed = true;
			system_handler_data.deallocate_name = true;
			UIActionHandler handler;
			handler.action = PopUpWindowSystemHandler;
			handler.data = &system_handler_data;
			handler.data_size = sizeof(system_handler_data);
			system->PushSystemHandler(handler);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParametersColorTheme(UIDrawer<initialize>& drawer) {
			constexpr size_t input_configuration = UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE | UI_CONFIG_COLOR_INPUT_CALLBACK;
			constexpr size_t slider_configuration = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK | UI_CONFIG_SLIDER_DEFAULT_VALUE;

			UIDrawConfig config;
			UIConfigColorInputCallback color_input_callback;

			auto system = drawer.GetSystem();
			UIColorThemeDescriptor* theme = &system->m_descriptors.color_theme;
			color_input_callback.callback = { SystemParameterColorInputThemeCallback, nullptr, 0 };
			config.AddFlag(color_input_callback);

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.color_theme;
			button_data.descriptor_size = sizeof(UIColorThemeDescriptor);
			button_data.descriptor_index = UIWindowDrawerDescriptorIndex::ColorTheme;
			button_data.is_system_theme = true;
			button_data.system_descriptor = theme;
			drawer.Button("Default values##0", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			const UIColorThemeDescriptor* startup_theme = &system->m_startup_descriptors.color_theme;
			drawer.ColorInput<input_configuration>(config, "Theme", &theme->theme, startup_theme->theme);

			color_input_callback.callback = { SystemParameterColorThemeCallback, nullptr, 0 };
			drawer.ColorInput<input_configuration>(config, "Text", &theme->default_text, startup_theme->default_text);
			drawer.ColorInput<input_configuration>(config, "Graph hover line", &theme->graph_hover_line, startup_theme->graph_hover_line);
			drawer.ColorInput<input_configuration>(config, "Graph line", &theme->graph_line, startup_theme->graph_line);
			drawer.ColorInput<input_configuration>(config, "Graph sample circle", &theme->graph_sample_circle, startup_theme->graph_sample_circle);
			drawer.ColorInput<input_configuration>(config, "Histogram", &theme->histogram_color, startup_theme->histogram_color);
			drawer.ColorInput<input_configuration>(config, "Histogram hovered", &theme->histogram_hovered_color, startup_theme->histogram_hovered_color);
			drawer.ColorInput<input_configuration>(config, "Histogram text", &theme->histogram_text_color, startup_theme->histogram_text_color);
			drawer.ColorInput<input_configuration>(config, "Unavailable text", &theme->unavailable_text, startup_theme->unavailable_text);
			drawer.ColorInput<input_configuration>(config, "Background", &theme->background, startup_theme->background);
			drawer.ColorInput<input_configuration>(config, "Borders", &theme->borders, startup_theme->borders);
			drawer.ColorInput<input_configuration>(config, "Collapse Triangle", &theme->collapse_triangle, startup_theme->collapse_triangle);
			drawer.ColorInput<input_configuration>(config, "Docking gizmo background", &theme->docking_gizmo_background, startup_theme->docking_gizmo_background);
			drawer.ColorInput<input_configuration>(config, "Docking gizmo border", &theme->docking_gizmo_border, startup_theme->docking_gizmo_border);
			drawer.ColorInput<input_configuration>(config, "Hierarchy drag node bar", &theme->hierarchy_drag_node_bar, startup_theme->hierarchy_drag_node_bar);
			drawer.ColorInput<input_configuration>(config, "Render sliders active part", &theme->render_sliders_active_part, startup_theme->render_sliders_active_part);
			drawer.ColorInput<input_configuration>(config, "Render sliders background", &theme->render_sliders_background, startup_theme->render_sliders_background);
			drawer.ColorInput<input_configuration>(config, "Region header X", &theme->region_header_x, startup_theme->region_header_x);
			drawer.ColorInput<input_configuration>(config, "Region header hovered X", &theme->region_header_hover_x, startup_theme->region_header_hover_x);

			config.flag_count = 0;
			UIConfigSliderChangedValueCallback callback;
			callback.handler = { SystemParameterColorThemeCallback, nullptr, 0 };
			config.AddFlag(callback);
			drawer.FloatSlider<slider_configuration>(config, "Check box factor", &theme->check_box_factor, 1.2f, 2.0f, startup_theme->check_box_factor, 3);
			drawer.FloatSlider<slider_configuration>(config, "Select text factor", &theme->select_text_factor, 1.1f, 2.0f, startup_theme->select_text_factor, 3);
			drawer.FloatSlider<slider_configuration>(config, "Darken hover factor", &theme->darken_hover_factor, 0.2f, 0.9f, startup_theme->darken_hover_factor, 3);
			drawer.FloatSlider<slider_configuration>(config, "Slider lighten factor", &theme->slider_lighten_factor, 1.1f, 2.5f, startup_theme->slider_lighten_factor, 3);
		}

		template ECSENGINE_API void SystemParametersColorTheme<false>(UIDrawer<false>& drawer);
		template ECSENGINE_API void SystemParametersColorTheme<true>(UIDrawer<true>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParametersLayout(UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_DEFAULT_VALUE | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK

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
			button_data.descriptor_index = UIWindowDrawerDescriptorIndex::Layout;
			button_data.is_system_theme = true;
			button_data.system_descriptor = layout;
			drawer.Button("Default values##1", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Element x", &layout->default_element_x, 0.01f, 0.3f, system_layout->default_element_x);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Element y", &layout->default_element_y, 0.01f, 0.15f, system_layout->default_element_y);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Indentation", &layout->element_indentation, 0.0f, 0.05f, system_layout->element_indentation);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Next row padding", &layout->next_row_padding, 0.0f, 0.05f, system_layout->next_row_padding);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Next row offset", &layout->next_row_y_offset, 0.0f, 0.05f, system_layout->next_row_y_offset);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Node indentation", &layout->node_indentation, 0.0f, 0.05f, system_layout->node_indentation);

#undef SLIDER_CONFIGURATION
		}

		template void ECSENGINE_API SystemParametersLayout<false>(UIDrawer<false>& drawer);
		template void ECSENGINE_API SystemParametersLayout<true>(UIDrawer<true>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParametersElementDescriptor(UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK

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
			button_data.descriptor_index = UIWindowDrawerDescriptorIndex::Element;
			button_data.is_system_theme = true;
			button_data.system_descriptor = elements;
			drawer.Button("Default values##20", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.PushIdentifierStack("##");
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Color input padd", &elements->color_input_padd, 0.0f, 0.01f, system_elements->color_input_padd, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Combo box padd", &elements->combo_box_padding, 0.0f, 0.02f, system_elements->combo_box_padding, 3);
			float* float2_values[2];
			float float2_lower_bounds[1];
			float float2_upper_bounds[1];
			float* float2_default_values;
			const char* float2_names[2];

			auto float2_sliders = [&](const char* group_name, size_t index) {
				drawer.PushIdentifierStackRandom(index);
				drawer.FloatSliderGroup<SLIDER_CONFIGURATION | UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS>(
					config,
					2,
					group_name,
					float2_names,
					float2_values,
					float2_lower_bounds,
					float2_upper_bounds,
					float2_default_values,
					3
					);
				drawer.PopIdentifierStack();
			};

			float2_values[0] = &elements->graph_axis_bump.x;
			float2_values[1] = &elements->graph_axis_bump.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.01f;
			float2_default_values = &system_elements->graph_axis_bump.x;
			float2_names[0] = "x:";
			float2_names[1] = "y:";
			float2_sliders("Graph axis bump", 0);

			float2_values[0] = &elements->graph_axis_value_line_size.x;
			float2_values[1] = &elements->graph_axis_value_line_size.y;
			float2_upper_bounds[0] = 0.02f;
			float2_default_values = &system_elements->graph_axis_value_line_size.x;
			float2_sliders("Graph axis value line size", 1);

			float2_values[0] = &elements->graph_padding.x;
			float2_values[1] = &elements->graph_padding.y;
			float2_upper_bounds[0] = 0.02f;
			float2_default_values = &system_elements->graph_padding.x;
			float2_sliders("Graph padding", 2);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Graph reduce font",
				&elements->graph_reduce_font,
				0.5f,
				1.0f,
				system_elements->graph_reduce_font,
				3
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Graph sample circle size",
				&elements->graph_sample_circle_size,
				0.003f,
				0.02f,
				system_elements->graph_sample_circle_size,
				3
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Graph x axis space",
				&elements->graph_x_axis_space,
				0.005f,
				0.05f,
				system_elements->graph_x_axis_space,
				4
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Histogram bar min scale",
				&elements->histogram_bar_min_scale,
				0.01f,
				0.2f,
				system_elements->histogram_bar_min_scale
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Histogram bar spacing",
				&elements->histogram_bar_spacing,
				0.0028f,
				0.01f,
				system_elements->histogram_bar_spacing,
				3
				);

			float2_values[0] = &elements->histogram_padding.x;
			float2_values[1] = &elements->histogram_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.1f;
			float2_default_values = &system_elements->histogram_padding.x;
			float2_sliders("Histogram padding", 3);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Histogram reduce font",
				&elements->histogram_reduce_font,
				0.5f,
				1.0f,
				system_elements->histogram_reduce_font,
				3
				);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(
				config,
				"Menu button padding",
				&elements->menu_button_padding,
				0.0f,
				0.1f,
				system_elements->menu_button_padding,
				4
			);

			float2_values[0] = &elements->label_horizontal_padd;
			float2_values[1] = &elements->label_vertical_padd;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.2f;
			float2_default_values = &system_elements->label_horizontal_padd;
			float2_sliders("Label padding", 4);

			float2_values[0] = &elements->slider_length.x;
			float2_values[1] = &elements->slider_length.y;
			float2_lower_bounds[0] = 0.01f;
			float2_upper_bounds[0] = 0.1f;
			float2_default_values = &system_elements->slider_length.x;
			float2_sliders("Slider length", 5);

			float2_values[0] = &elements->slider_padding.x;
			float2_values[1] = &elements->slider_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.05f;
			float2_default_values = &system_elements->slider_padding.x;
			float2_sliders("Slider padding", 6);

			float2_values[0] = &elements->slider_shrink.x;
			float2_values[1] = &elements->slider_shrink.y;
			float2_default_values = &system_elements->slider_shrink.x;
			float2_sliders("Slider shrink", 7);

			float2_values[0] = &elements->text_input_padding.x;
			float2_values[1] = &elements->text_input_padding.y;
			float2_upper_bounds[0] = 0.1f;
			float2_default_values = &system_elements->text_input_padding.x;
			float2_sliders("Text input padding", 8);

			drawer.PopIdentifierStack();
#undef SLIDER_CONFIGURATION
		}

		template void ECSENGINE_API SystemParametersElementDescriptor<false>(UIDrawer<false>& drawer);
		template void ECSENGINE_API SystemParametersElementDescriptor<true>(UIDrawer<true>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParameterFont(UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE

			UIDrawConfig config;
			auto system = drawer.GetSystem();

			UIFontDescriptor* font = &system->m_descriptors.font;
			const UIFontDescriptor* startup_font = &system->m_startup_descriptors.font;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.font;
			button_data.descriptor_size = sizeof(UIFontDescriptor);
			button_data.descriptor_index = UIWindowDrawerDescriptorIndex::Font;
			button_data.is_system_theme = true;
			button_data.system_descriptor = font;
			drawer.Button("Default values##32", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Character spacing", &font->character_spacing, 0.0f, 0.1f, startup_font->character_spacing, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Size", &font->size, 0.0009f, 0.003f, startup_font->size, 5);
#undef SLIDER_CONFIGURATION
		}

		template void ECSENGINE_API SystemParameterFont<false>(UIDrawer<false>& drawer);
		template void ECSENGINE_API SystemParameterFont<true>(UIDrawer<true>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParameterDockspace(UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE
			UIDrawConfig config;
			auto system = drawer.GetSystem();

			UIDockspaceDescriptor* dockspace = &system->m_descriptors.dockspaces;
			const UIDockspaceDescriptor* startup_dockspace = &system->m_startup_descriptors.dockspaces;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.dockspaces;
			button_data.descriptor_size = sizeof(UIDockspaceDescriptor);
			button_data.descriptor_index = (UIWindowDrawerDescriptorIndex)10;
			button_data.is_system_theme = true;
			button_data.system_descriptor = dockspace;
			drawer.Button("Default values##41", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.IntSlider<SLIDER_CONFIGURATION>(config, "Border clickable handler count", &dockspace->border_default_clickable_handler_count, (unsigned int)64, (unsigned int)1024, startup_dockspace->border_default_clickable_handler_count);
			drawer.IntSlider<SLIDER_CONFIGURATION>(config, "Border general handler count", &dockspace->border_default_general_handler_count, (unsigned int)16, (unsigned int)1024, startup_dockspace->border_default_general_handler_count);
			drawer.IntSlider<SLIDER_CONFIGURATION>(config, "Border hoverable handler count", &dockspace->border_default_hoverable_handler_count, (unsigned int)64, (unsigned int)1024, startup_dockspace->border_default_hoverable_handler_count);
			drawer.IntSlider<SLIDER_CONFIGURATION>(config, "Border sprite texture count", &dockspace->border_default_sprite_texture_count, (unsigned int)16, (unsigned int)1024, startup_dockspace->border_default_sprite_texture_count);
			drawer.IntSlider<SLIDER_CONFIGURATION>(config, "Dockspace count", &dockspace->count, (unsigned int)8, (unsigned int)64, startup_dockspace->count);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Max border count", &dockspace->max_border_count, 4, 16, startup_dockspace->max_border_count);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Max windows per border", &dockspace->max_windows_border, 4, 16, startup_dockspace->max_border_count);

			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Border margin", &dockspace->border_margin, 0.0f, 0.5f, startup_dockspace->border_margin);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Border minimum distance", &dockspace->border_minimum_distance, 0.0f, 0.5f, startup_dockspace->border_minimum_distance);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Border size", &dockspace->border_size, 0.0005f, 0.01f, startup_dockspace->border_size, 7);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Mininum scale", &dockspace->mininum_scale, 0.01f, 0.5f, startup_dockspace->mininum_scale, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Viewport padding x", &dockspace->viewport_padding_x, 0.0f, 0.003f, startup_dockspace->viewport_padding_x, 7);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Viewport padding y", &dockspace->viewport_padding_y, 0.0f, 0.003f, startup_dockspace->viewport_padding_y, 7);

#undef SLIDER_CONFIGURATION
		}

		template void ECSENGINE_API SystemParameterDockspace<false>(UIDrawer<false>& drawer);
		template void ECSENGINE_API SystemParameterDockspace<true>(UIDrawer<true>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParameterMaterial(UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_DEFAULT_VALUE | UI_CONFIG_SLIDER_ENTER_VALUES

			auto system = drawer.GetSystem();

			UIMaterialDescriptor* material = &system->m_descriptors.materials;
			const UIMaterialDescriptor* startup_material = &system->m_descriptors.materials;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.materials;
			button_data.descriptor_size = sizeof(UIMaterialDescriptor);
			button_data.descriptor_index = (UIWindowDrawerDescriptorIndex)10;
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

			drawer.CollapsingHeader("Vertex buffer normal phase", [&]() {
				UIDrawConfig config;
				drawer.PushIdentifierStack("##1");
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, names[index], &material->vertex_buffer_count[index], 256, 5'000'000, startup_material->vertex_buffer_count[index]);
				}
				drawer.PopIdentifierStack();
				});

			drawer.CollapsingHeader("Vertex buffer late phase", [&]() {
				UIDrawConfig config;
				drawer.PushIdentifierStack("##2");
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, names[index], &material->vertex_buffer_count[ECS_TOOLS_UI_MATERIALS + index], 256, 5'000'000, startup_material->vertex_buffer_count[ECS_TOOLS_UI_MATERIALS + index]);
				}
				drawer.PopIdentifierStack();
				});



#undef SLIDER_CONFIGURATION
		}

		template void ECSENGINE_API SystemParameterMaterial<false>(UIDrawer<false>& drawer);
		template void ECSENGINE_API SystemParameterMaterial<true>(UIDrawer<true>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParameterMiscellaneous(UIDrawer<initialize>& drawer) {
#define SLIDER_CONFIGURATION UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE
#define COLOR_CONFIGURATION UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE

			UIDrawConfig config;
			auto system = drawer.GetSystem();

			UIMiscellaneousDescriptor* misc = &system->m_descriptors.misc;
			UIMiscellaneousDescriptor* startup_misc = &system->m_startup_descriptors.misc;

			UIParameterWindowReturnToDefaultButtonData button_data;
			button_data.default_descriptor = &system->m_startup_descriptors.misc;
			button_data.descriptor_size = sizeof(UIMiscellaneousDescriptor);
			button_data.descriptor_index = (UIWindowDrawerDescriptorIndex)10;
			button_data.is_system_theme = true;
			button_data.system_descriptor = misc;
			drawer.Button("Default values##100", { WindowParameterReturnToDefaultButton, &button_data, sizeof(button_data) });

			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Drawer identifier memory", &misc->drawer_identifier_memory, 100, 1000, startup_misc->drawer_identifier_memory);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Drawer temp memory", &misc->drawer_temp_memory, 1'000, 1'000'000, startup_misc->drawer_temp_memory);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Hierarchy drag node hover time until drop", &misc->hierarchy_drag_node_hover_drop, 250, 5000, startup_misc->hierarchy_drag_node_hover_drop);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Hierarchy drag node time", &misc->hierarchy_drag_node_time, 100, 2'000, startup_misc->hierarchy_drag_node_time);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Slider bring back start time", &misc->slider_bring_back_start, 50, 2'000, startup_misc->slider_bring_back_start);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Slider enter values duration", &misc->slider_enter_value_duration, 100, 2'000, startup_misc->slider_enter_value_duration);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Text input caret display time", &misc->text_input_caret_display_time, 25, 5'000, startup_misc->text_input_caret_display_time);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Text input coallesce command time", &misc->text_input_coallesce_command, 25, 1'000, startup_misc->text_input_coallesce_command);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Text input start time", &misc->text_input_repeat_start_duration, 25, 1'000, startup_misc->text_input_repeat_start_duration);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Text input repeat time", &misc->text_input_repeat_time, 25, 1'000, startup_misc->text_input_repeat_time);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Thread temp memory", &misc->thread_temp_memory, 128, 1'000'000, startup_misc->thread_temp_memory);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned short>(config, "Window count", &misc->window_count, 8, 64, startup_misc->window_count);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, "Window handler revert command count", &misc->window_handler_revert_command_count, 32, 4096, startup_misc->window_handler_revert_command_count);
			drawer.IntSlider<SLIDER_CONFIGURATION, unsigned short>(config, "Window table default resource count", &misc->window_table_default_count, 64, 1024, startup_misc->window_table_default_count);

			const char* names[] = {
				"Solid color",
				"Text sprite",
				"Sprite",
				"Sprite cluster",
				"Line"
			};

			drawer.CollapsingHeader("System vertex buffer count", [&]() {
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					drawer.IntSlider<SLIDER_CONFIGURATION, unsigned int>(config, names[index], &misc->system_vertex_buffers[index], 128, 5'000'000, startup_misc->system_vertex_buffers[index]);
				}
				});

			float* float2_values[2];
			float float2_lower_bounds[2];
			float float2_upper_bounds[2];
			float* float2_default_values;
			const char* float2_names[2] = { "x:", "y:" };

			drawer.PushIdentifierStack("##");

			auto float2_lambda = [&](const char* group_name, size_t index) {
				drawer.PushIdentifierStackRandom(index);
				drawer.FloatSliderGroup<SLIDER_CONFIGURATION | UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS>(
					config,
					2,
					group_name,
					float2_names,
					float2_values,
					float2_lower_bounds,
					float2_upper_bounds,
					float2_default_values,
					3
					);
				drawer.PopIdentifierStack();
			};

			float2_values[0] = &misc->color_input_window_size_x;
			float2_values[1] = &misc->color_input_window_size_y;
			float2_lower_bounds[0] = 0.1f;
			float2_upper_bounds[0] = 1.0f;
			float2_default_values = &startup_misc->color_input_window_size_x;
			float2_lambda("Color input window size", 10);

			float2_values[0] = &misc->graph_hover_offset.x;
			float2_values[1] = &misc->graph_hover_offset.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.2f;
			float2_default_values = &startup_misc->graph_hover_offset.x;
			float2_lambda("Graph hover offset", 11);

			float2_values[0] = &misc->histogram_hover_offset.x;
			float2_values[1] = &misc->histogram_hover_offset.y;
			float2_default_values = &startup_misc->histogram_hover_offset.x;
			float2_lambda("Histogram hover offset", 12);

			float2_values[0] = &misc->render_slider_horizontal_size;
			float2_values[1] = &misc->render_slider_vertical_size;
			float2_lower_bounds[0] = 0.005f;
			float2_upper_bounds[1] = 0.05f;
			float2_default_values = &startup_misc->render_slider_horizontal_size;
			float2_lambda("Render slider size", 13);

			float2_values[0] = &misc->tool_tip_padding.x;
			float2_values[1] = &misc->tool_tip_padding.y;
			float2_lower_bounds[0] = 0.0f;
			float2_upper_bounds[0] = 0.1f;
			float2_default_values = &startup_misc->tool_tip_padding.x;
			float2_lambda("Tool tip padding", 14);

			drawer.PopIdentifierStack();

			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Menu x padd", &misc->menu_x_padd, 0.0f, 0.1f, startup_misc->menu_x_padd, 3);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Hierarchy drag node rectangle size", &misc->rectangle_hierarchy_drag_node_dimension, 0.005f, 0.01f, startup_misc->rectangle_hierarchy_drag_node_dimension, 4);
			drawer.FloatSlider<SLIDER_CONFIGURATION>(config, "Title y scale", &misc->title_y_scale, 0.01f, 0.1f, startup_misc->title_y_scale);

			drawer.ColorInput<COLOR_CONFIGURATION>(config, "Menu arrow color", &misc->menu_arrow_color, startup_misc->menu_arrow_color);
			drawer.ColorInput<COLOR_CONFIGURATION>(config, "Menu unavailable arrow color", &misc->menu_unavailable_arrow_color, startup_misc->menu_unavailable_arrow_color);
			drawer.ColorInput<COLOR_CONFIGURATION>(config, "Tool tip background", &misc->tool_tip_background_color, startup_misc->tool_tip_background_color);
			drawer.ColorInput<COLOR_CONFIGURATION>(config, "Tool tip border", &misc->tool_tip_border_color, startup_misc->tool_tip_border_color);
			drawer.ColorInput<COLOR_CONFIGURATION>(config, "Tool tip font", &misc->tool_tip_font_color, startup_misc->tool_tip_font_color);
			drawer.ColorInput<COLOR_CONFIGURATION>(config, "Tool tip unavailable font", &misc->tool_tip_unavailable_font_color, startup_misc->tool_tip_unavailable_font_color);

#undef COLOR_CONFIGURATION
#undef SLIDER_CONFIGURATION
		}

		template void ECSENGINE_API SystemParameterMiscellaneous<false>(UIDrawer<false>& drawer);
		template void ECSENGINE_API SystemParameterMiscellaneous<true>(UIDrawer<true>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void SystemParametersDraw(void* window_data, void* drawer_descriptor) {
			UI_PREPARE_DRAWER(initialize);

			UISystemDescriptors* descriptors = &drawer.GetSystem()->m_descriptors;
			drawer.SetDrawMode(UIDrawerMode::NextRow);

			auto color_theme_lambda = [&]() {
				SystemParametersColorTheme<initialize>(drawer);
			};

			drawer.CollapsingHeader("Color theme", color_theme_lambda);

			auto layout_lambda = [&]() {
				SystemParametersLayout<initialize>(drawer);
			};

			drawer.CollapsingHeader("Layout", layout_lambda);

			auto element_descriptor_lambda = [&]() {
				SystemParametersElementDescriptor<initialize>(drawer);
			};

			drawer.CollapsingHeader("Element descriptor", element_descriptor_lambda);

			auto material_lambda = [&]() {
				SystemParameterMaterial<initialize>(drawer);
			};

			drawer.CollapsingHeader("Materials", material_lambda);

			auto font_lambda = [&]() {
				SystemParameterFont<initialize>(drawer);
			};

			drawer.CollapsingHeader("Font", font_lambda);

			auto dockspace_lambda = [&]() {
				SystemParameterDockspace<initialize>(drawer);
			};

			drawer.CollapsingHeader("Dockspace", dockspace_lambda);

			auto misc_lambda = [&]() {
				SystemParameterMiscellaneous<initialize>(drawer);
			};

			drawer.CollapsingHeader("Miscellaneous", misc_lambda);
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, SystemParametersDraw, void*, void*);

		// --------------------------------------------------------------------------------------------------------------

		void OpenSystemParameters(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDescriptor descriptor;
			system->DestroyWindowIfFound("System Parameters");

			descriptor.draw = SystemParametersDraw<false>;
			descriptor.initialize = SystemParametersDraw<true>;
			descriptor.private_action = SkipAction;
			descriptor.window_data = nullptr;
			descriptor.window_data_size = 0;
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
			system->PushSystemHandler(handler);
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

			const char* window_name = system->GetWindowName(window_index);
			char new_name[256];
			size_t window_name_size = strlen(window_name);
			memcpy(new_name, window_name, window_name_size);
			new_name[window_name_size] = '\0';
			strcat(new_name, " Parameters");
			size_t total_name_size = window_name_size + 11;
			new_name[total_name_size] = '\0';

			unsigned int parameter_window_index = system->GetWindowFromName(new_name);
			data->is_parameter_window_opened = function::PredicateValue(parameter_window_index == 0xFFFFFFFF, false, true);

			int scroll_amount = mouse->MouseWheelScroll() - data->scroll;
			float total_scroll = 0.0f;
			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				if (keyboard_tracker->IsKeyPressed(HID::Key::Z)) {
					HandlerCommand command;
					if (data->revert_commands.Pop(command)) {
						action_data->data = command.handler.data;
						command.handler.action(action_data);
						system->m_memory->Deallocate(command.handler.data);
						system->RemoveWindowMemoryResource(window_index, command.handler.data);
					}
				}
				else if (IsPointInRectangle(mouse_position, position, scale) && data->last_frame == system->GetFrameIndex() - 1) {
					if (mouse_tracker->RightButton() != MBPRESSED && data->allow_zoom) {
						if (scroll_amount != 0.0f) {
							float2 before_zoom = system->m_windows[window_index].zoom;
							system->m_windows[window_index].zoom.x += scroll_amount * ECS_TOOLS_UI_DEFAULT_HANDLER_ZOOM_FACTOR;
							system->m_windows[window_index].zoom.y += scroll_amount * ECS_TOOLS_UI_DEFAULT_HANDLER_ZOOM_FACTOR;

							system->m_windows[window_index].zoom.x = function::ClampMin(
								system->m_windows[window_index].zoom.x,
								system->m_windows[window_index].min_zoom
							);
							system->m_windows[window_index].zoom.x = function::ClampMax(
								system->m_windows[window_index].zoom.x,
								system->m_windows[window_index].max_zoom
							);

							system->m_windows[window_index].zoom.y = function::ClampMin(
								system->m_windows[window_index].zoom.y,
								system->m_windows[window_index].min_zoom
							);
							system->m_windows[window_index].zoom.y = function::ClampMax(
								system->m_windows[window_index].zoom.y,
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
					dimming_value = function::PredicateValue(keyboard->IsKeyDown(HID::Key::LeftShift), 0.2f, 1.0f);
					dimming_value = function::PredicateValue(keyboard->IsKeyDown(HID::Key::RightShift), 0.02f, dimming_value);

					if (scroll_amount != 0.0f) {
						total_scroll = data->scroll_factor * ECS_TOOLS_UI_DEFAULT_HANDLER_SCROLL_FACTOR * scroll_amount * dimming_value;
						UIDrawerSlider* vertical_slider = (UIDrawerSlider*)data->vertical_slider;
						vertical_slider->interpolate_value = true;
						vertical_slider->slider_position -= total_scroll;

						vertical_slider->slider_position = function::PredicateValue(vertical_slider->slider_position > 1.0f, 1.0f, vertical_slider->slider_position);
						vertical_slider->slider_position = function::PredicateValue(vertical_slider->slider_position < 0.0f, 0.0f, vertical_slider->slider_position);
					}
				}
			}
			data->scroll = mouse->MouseWheelScroll();
			data->last_frame = system->GetFrameIndex();

			if (keyboard->IsKeyDown(HID::Key::LeftAlt) && mouse_tracker->LeftButton() == MBPRESSED &&
				IsPointInRectangle(mouse_position, system->m_windows[window_index].transform) &&
				!data->is_parameter_window_opened && !data->is_this_parameter_window) {
				data->is_parameter_window_opened = true;
				OpenWindowParameters(action_data);
			}

			if (keyboard->IsKeyDown(HID::Key::RightAlt) && mouse_tracker->LeftButton() == MBPRESSED
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
			data->commit_cursor = CursorType::Default;
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void DrawNothing(void* window_data, void* drawer_descriptor)
		{
			UI_PREPARE_DRAWER(initialize);
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, DrawNothing, void*, void*);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void ErrorMessageWindowDraw(void* window_data, void* drawer_descriptor) {
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
			drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "OK", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, ErrorMessageWindowDraw, void*, void*);

		// ------------------------------------------------------------------------------------

		unsigned int CreateErrorMessageWindow(UISystem* system, const char* description) {
			// capacity is not needed
			return CreateErrorMessageWindow(system, CapacityStream<char>(description, strlen(description), 0));
		}

		// ------------------------------------------------------------------------------------

		unsigned int CreateErrorMessageWindow(UISystem* system, Stream<char> description)
		{
			UIWindowDescriptor descriptor;
			descriptor.draw = ErrorMessageWindowDraw<false>;
			descriptor.initialize = ErrorMessageWindowDraw<true>;
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
				CloseXBorderClickableAction(action_data);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void ConfirmWindowDraw(void* window_data, void* drawer_descriptor) {
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
			drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "OK", { ConfirmWindowOKAction, data, 0, UIDrawPhase::System });

			config.flag_count = 0;
			transform.scale = drawer.GetLabelScale("Cancel");
			transform.position.x = drawer.GetAlignedToRight(transform.scale.x).x;
			config.AddFlag(transform);
			drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Cancel", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, ConfirmWindowDraw, void*, void*);

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateConfirmWindow(UISystem* system, Stream<char> description, UIActionHandler handler)
		{
			UIWindowDescriptor descriptor;
			descriptor.draw = ConfirmWindowDraw<false>;
			descriptor.initialize = ConfirmWindowDraw<true>;
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
				handler_memory = function::Copy(system->m_memory, handler.data, handler.data_size);
				data.handler.data = handler_memory;
			}

			descriptor.window_data = &data;
			descriptor.window_data_size = sizeof(data);
			descriptor.window_name = ECS_TOOLS_UI_CONFIRM_WINDOW_NAME;

			descriptor.destroy_action = ReleaseLockedWindow;

			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_LOCK_WINDOW
				| UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_DOCKSPACE_LOCK_WINDOW);
			system->AddWindowMemoryResource(sentence, window_index);
			if (handler_memory != nullptr) {
				system->AddWindowMemoryResource(handler_memory, window_index);
			}

			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = system->GetDockspaceFromWindow(window_index, border_index, type);
			system->SetPopUpWindowPosition(window_index, { AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.x), AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.y) });
			return window_index;
		}

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateConfirmWindow(UISystem* ECS_RESTRICT system, const char* ECS_RESTRICT description, UIActionHandler handler) {
			return CreateConfirmWindow(system, Stream<char>(description, strlen(description)), handler);
		}

		// -------------------------------------------------------------------------------------------------------

		unsigned int CreateChooseOptionWindow(UISystem* system, ChooseOptionWindowData data)
		{
			UIWindowDescriptor descriptor;
			descriptor.draw = ChooseOptionWindowDraw<false>;
			descriptor.initialize = ChooseOptionWindowDraw<true>;
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
			descriptor.window_name = ECS_TOOLS_UI_CHOOSE_WINDOW_NAME;

			descriptor.destroy_action = ReleaseLockedWindow;

			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_LOCK_WINDOW
				| UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_DOCKSPACE_LOCK_WINDOW);
			system->PopUpSystemHandler(ECS_TOOLS_UI_CHOOSE_WINDOW_NAME, false);
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
				CloseXBorderClickableAction(action_data);
			}
		}

		template<bool initialize>
		void ChooseOptionWindowDraw(void* window_data, void* drawer_descriptor) {
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
				drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_CACHE>(config, data->button_names[index], { ChooseOptionAction, &index_data, sizeof(index_data), UIDrawPhase::System });
				config.flag_count = 0;
			}

			transform.scale = drawer.GetLabelScale("Cancel");
			transform.position.x = drawer.GetAlignedToRight(transform.scale.x).x;
			config.AddFlag(transform);
			drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_CACHE>(config, "Cancel", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
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

		template<bool initialize>
		void TextInputWizard(void* window_data, void* drawer_descriptor)
		{
			UI_PREPARE_DRAWER(initialize);

			TextInputWizardData* data = (TextInputWizardData*)window_data;
			
			if constexpr (initialize) {
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
			UIConfigRelativeTransform relative_transform;
			relative_transform.scale.x = 5.0f;
			config.AddFlag(relative_transform);

			drawer.TextInput<UI_CONFIG_RELATIVE_TRANSFORM>(config, data->input_name, &data->input_stream);
			drawer.NextRow();

			UIConfigAbsoluteTransform absolute_transform;
			absolute_transform.scale = drawer.GetLabelScale("OK");
			absolute_transform.position = drawer.GetAlignedToBottom(absolute_transform.scale.y);
			config.flag_count = 0;
			config.AddFlag(absolute_transform);

			drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "OK", { TextInputWizardConfirmAction, window_data, 0, UIDrawPhase::System });

			absolute_transform.scale = drawer.GetLabelScale("Cancel");
			absolute_transform.position.x = drawer.GetAlignedToRight(absolute_transform.scale.x).x;
			config.flag_count = 0;
			config.AddFlag(absolute_transform);

			drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Cancel", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, TextInputWizard, void*, void*);

		// Additional data is the window data that is TextInputWizardData*
		void TextInputWizardPrivateHandler(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			if (keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
				action_data->data = action_data->additional_data;
				TextInputWizardConfirmAction(action_data);
			}
		}

		unsigned int CreateTextInputWizard(const TextInputWizardData* data, UISystem* system) {
			UIWindowDescriptor descriptor;
			
			descriptor.window_name = data->window_name;
			descriptor.draw = TextInputWizard<false>;
			descriptor.initialize = TextInputWizard<true>;
			
			descriptor.initial_position_x = 0.0f;
			descriptor.initial_position_y = 0.0f;
			descriptor.initial_size_x = 100.0f;
			descriptor.initial_size_y = 100.0f;

			descriptor.window_data = (void*)data;
			descriptor.window_data_size = sizeof(*data);
			descriptor.destroy_action = ReleaseLockedWindow;

			descriptor.private_action = TextInputWizardPrivateHandler;

			system->PopUpSystemHandler(data->window_name, false, false, false);
			return system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE
				| UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER | UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_LOCK_WINDOW | UI_DOCKSPACE_NO_DOCKING);
		}

		void CreateTextInputWizardAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			TextInputWizardData* data = (TextInputWizardData*)_data;
			CreateTextInputWizard(data, system);
		}

		// -------------------------------------------------------------------------------------------------------

		void DrawTextFile(UIDrawer<false>* drawer, const wchar_t* path, float2 border_padding, float next_row_y_offset) {
			float2 current_position = drawer->GetCurrentPositionNonOffset();
			drawer->OffsetNextRow(border_padding.x);
			drawer->OffsetX(border_padding.x);
			drawer->OffsetY(border_padding.y);

			UIDrawConfig config;
			std::ifstream file_stream(path);

			bool draw_border = false;
			if (file_stream.good()) {
				size_t file_size = function::GetFileByteSize(file_stream);
				if (file_size < ECS_MB) {
					char* allocation = (char*)_malloca(file_size + 1);
					file_stream.read(allocation, file_size);
					size_t read_bytes = file_stream.gcount();
					allocation[read_bytes] = '\0';
					float old_next_row_y_offset = drawer->layout.next_row_y_offset;
					drawer->OffsetY(old_next_row_y_offset);
					drawer->SetNextRowYOffset(next_row_y_offset);
					drawer->Sentence<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SENTENCE_FIT_SPACE>(config, allocation);
					drawer->SetNextRowYOffset(old_next_row_y_offset);
					draw_border = true;
					_freea(allocation);
				}
				else {
					drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "File is too big to be drawn");
				}
			}
			else {
				drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "File could not be opened.");
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
				drawer->Rectangle<UI_CONFIG_BORDER | UI_CONFIG_ABSOLUTE_TRANSFORM>(config);
			}
			drawer->OffsetNextRow(-border_padding.x);
			drawer->NextRow();
		}

		float2* DrawTextFileEx(UIDrawer<false>* drawer, const wchar_t* path, float2 border_padding, float next_row_y_offset) {
			constexpr const char* STABILIZE_STRING_NAME = "Stabilized render span";

			UISystem* system = drawer->system;
			bool exists = system->ExistsWindowResource(drawer->window_index, STABILIZE_STRING_NAME);
			float2* stabilized_render_span;
			if (exists) {
				stabilized_render_span = (float2*)system->FindWindowResource(drawer->window_index, STABILIZE_STRING_NAME, strlen(STABILIZE_STRING_NAME));
				system->IncrementWindowDynamicResource(drawer->window_index, STABILIZE_STRING_NAME);
			}
			else {
				system->AddWindowDrawerElement(drawer->window_index, STABILIZE_STRING_NAME);
				stabilized_render_span = (float2*)system->m_memory->Allocate(sizeof(float2), alignof(float2));
				system->AddWindowMemoryResource(stabilized_render_span, drawer->window_index);
				system->AddWindowMemoryResourceToTable(stabilized_render_span, ResourceIdentifier(ToStream(STABILIZE_STRING_NAME)), drawer->window_index);
				system->FinalizeWindowElement(drawer->window_index);
				*stabilized_render_span = { 0.0f, 0.0f };
			}

			float2 current_position = drawer->GetCurrentPositionNonOffset();
			drawer->OffsetNextRow(border_padding.x);
			drawer->OffsetX(border_padding.x);
			drawer->OffsetY(border_padding.y);

			UIDrawConfig config;
			std::ifstream file_stream(path);

			bool draw_border = false;
			if (file_stream.good()) {
				size_t file_size = function::GetFileByteSize(file_stream);
				if (file_size < ECS_MB) {
					char* allocation = (char*)_malloca(file_size + 1);
					file_stream.read(allocation, file_size);
					size_t read_bytes = file_stream.gcount();
					allocation[read_bytes] = '\0';
					float old_next_row_y_offset = drawer->layout.next_row_y_offset;
					drawer->OffsetY(old_next_row_y_offset);
					drawer->SetNextRowYOffset(next_row_y_offset);
					drawer->Sentence<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SENTENCE_FIT_SPACE>(config, allocation);
					drawer->SetNextRowYOffset(old_next_row_y_offset);
					draw_border = true;
					_freea(allocation);
				}
				else {
					drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "File is too big to be drawn");
				}
			}
			else {
				drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "File could not be opened.");
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
				drawer->Rectangle<UI_CONFIG_BORDER | UI_CONFIG_ABSOLUTE_TRANSFORM>(config);
			}
			drawer->OffsetNextRow(-border_padding.x);
			drawer->NextRow();

			drawer->StabilizeRenderSpan(*stabilized_render_span);
			return stabilized_render_span;
		}

		template<bool initialize>
		void TextFileDraw(void* window_data, void* drawer_descriptor)
		{
			UI_PREPARE_DRAWER(initialize);

			const TextFileDrawData* data = (const TextFileDrawData*)window_data;

			if constexpr (!initialize) {
				float2* stabilized_render_span = DrawTextFileEx(&drawer, data->path, data->border_padding, data->next_row_y_offset);
				*stabilized_render_span = (drawer.GetRenderSpan() + *stabilized_render_span) * float2(0.5f, 0.5f);
			}
		}

		unsigned int CreateTextFileWindow(TextFileDrawData data, UISystem* system, const char* window_name) {
			UIWindowDescriptor descriptor;
			
			descriptor.window_data = &data;
			descriptor.window_data_size = sizeof(data);
			descriptor.window_name = window_name;

			descriptor.draw = TextFileDraw<false>;
			descriptor.initialize = TextFileDraw<true>;
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

		void ConsoleAppendMessageToDump(std::ofstream& stream, unsigned int index, Console* console) {
			// temporarly swap the \0 to \n in order to have it formatted correctly
			char* mutable_ptr = (char*)console->messages[index].message.buffer;
			mutable_ptr[console->messages[index].message.size] = '\n';
			stream.write(mutable_ptr, console->messages[index].message.size + 1);
			mutable_ptr[console->messages[index].message.size] = '\0';
		}

		// ------------------------------------------------------------------------------------

		void ConsoleDump(unsigned int thread_index, World* world, void* _data) {
			ConsoleDumpData* data = (ConsoleDumpData*)_data;
			std::ofstream stream(data->console->dump_path, std::ios::trunc);

			ECS_ASSERT(stream.good());

			data->console->dump_lock.lock();

#pragma region Header

			SYSTEMTIME system_time;
			GetLocalTime(&system_time);

			const char header_annotation[] = "**********************************************\n";
			size_t annotation_size = std::size(header_annotation) - 1;
			stream.write(header_annotation, annotation_size);
			char time_characters[256];
			Stream<char> time_stream = Stream<char>(time_characters, 0);
			function::ConvertIntToChars(time_stream, system_time.wHour);
			time_stream.Add(':');
			function::ConvertIntToChars(time_stream, system_time.wMinute);
			time_stream.Add(':');
			function::ConvertIntToChars(time_stream, system_time.wSecond);
			time_stream.Add(':');
			function::ConvertIntToChars(time_stream, system_time.wMilliseconds);
			time_stream.Add(' ');
			function::ConvertIntToChars(time_stream, system_time.wDay);
			time_stream.Add('-');
			function::ConvertIntToChars(time_stream, system_time.wMonth);
			time_stream.Add('-');
			function::ConvertIntToChars(time_stream, system_time.wYear);
			time_stream.Add(' ');
			const char description[] = "Console output\n";
			time_stream.AddStream(Stream<char>(description, std::size(description) - 1));
			stream.write(time_characters, time_stream.size);
			stream.write(header_annotation, annotation_size);

#pragma endregion

#pragma region Messages

			for (size_t index = 0; index < data->console->messages.size; index++) {
				ConsoleAppendMessageToDump(stream, index, data->console);
			}

			data->console->dump_lock.unlock();
			data->console->last_dumped_message = data->console->messages.size;

#pragma endregion

			ECS_ASSERT(stream.good());

		}

		void ConsoleAppendToDump(unsigned int thread_index, World* world, void* _data)
		{
			ConsoleDumpData* data = (ConsoleDumpData*)_data;
			std::ofstream stream(data->console->dump_path, std::ios::ate | std::ios::_Nocreate);

			ECS_ASSERT(stream.good());

			data->console->dump_lock.lock();

			for (size_t index = data->starting_index; index < data->console->messages.size; index++) {
				ConsoleAppendMessageToDump(stream, index, data->console);
			}
			data->console->last_dumped_message += data->console->messages.size - data->starting_index;

			data->console->dump_lock.unlock();

			ECS_ASSERT(stream.good());
		}

		// -------------------------------------------------------------------------------------------------------

		void ConsoleClearAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			Console* console = (Console*)_data;
			console->Clear();
		}

		constexpr const wchar_t* ConsoleTextureIcons[] = {
			ECS_TOOLS_UI_TEXTURE_INFO_ICON,
			ECS_TOOLS_UI_TEXTURE_WARN_ICON,
			ECS_TOOLS_UI_TEXTURE_ERROR_ICON,
			ECS_TOOLS_UI_TEXTURE_TRACE_ICON
		};

		template<bool initialize>
		void ConsoleWindowDraw(void* window_data, void* drawer_descriptor) {
			UI_PREPARE_DRAWER(initialize);

			ConsoleWindowData* data = (ConsoleWindowData*)window_data;
			drawer.SetDrawMode(UIDrawerMode::FitSpace);
			drawer.layout.next_row_padding = 0.005f;

			if constexpr (initialize) {
				constexpr size_t unique_message_default_capacity = 256;
				data->unique_messages.Initialize(drawer.system->m_memory, unique_message_default_capacity);
				data->system_filter = (bool*)drawer.GetMainAllocatorBuffer(sizeof(bool) * data->console->system_filter_strings.size);
				memset(data->system_filter, 1, sizeof(bool) * data->console->system_filter_strings.size);
			}

			UIDrawConfig config;

#pragma region Recalculate counts

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
						(*ptrs[(unsigned int)data->console->messages[index].icon_type])++;
						ResourceIdentifier identifier = {
							data->console->messages[index].message.buffer + data->console->messages[index].client_message_start,
							(unsigned int)data->console->messages[index].message.size - data->console->messages[index].client_message_start
						};
						unsigned int hash = ConsoleUIHashFunction::Hash(identifier.ptr, identifier.size);

						int message_index = data->unique_messages.Find(hash, identifier);
						if (message_index == -1) {
							bool grow = data->unique_messages.Insert(hash, { data->console->messages[index], 1 }, identifier);
							if (grow) {
								size_t new_capacity = static_cast<size_t>(data->unique_messages.GetCapacity() * 1.5f + 1);
								void* new_allocation = drawer.GetMainAllocatorBuffer(data->unique_messages.MemoryOf(new_capacity));
								const void* old_allocation = data->unique_messages.Grow<ConsoleUIHashFunction>(new_allocation, new_capacity);
								drawer.RemoveAllocation(old_allocation);
							}
						}
						else {
							UniqueConsoleMessage* message_ptr = data->unique_messages.GetValuePtrFromIndex(message_index);
							message_ptr->counter++;
						}

						bool is_system_valid = (data->console->messages[index].system_filter & system_mask) != 0;
						bool is_type_valid = type_ptrs[(unsigned int)data->console->messages[index].icon_type];
						bool is_verbosity_valid = data->console->messages[index].verbosity <= data->console->verbosity_level;
						if (is_system_valid && is_type_valid && is_verbosity_valid) {
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
					data->unique_messages.Clear();
					data->filtered_message_indices.FreeBuffer();

					update_kernel(0);
				}
				data->last_frame_message_count = data->console->messages.size;
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
			drawer.Button<button_configuration>(config, "Clear", { ConsoleClearAction, data->console, 0 });
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Collapse");
			config.AddFlag(transform);
			drawer.StateButton<button_configuration>(config, "Collapse", &data->collapse);
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Clear on play");
			config.AddFlag(transform);
			drawer.StateButton<button_configuration>(config, "Clear on play", &data->clear_on_play);
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Pause on error");
			config.AddFlag(transform);
			drawer.StateButton<button_configuration>(config, "Pause on error", &data->console->pause_on_error);
			config.flag_count--;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Filter");
			config.AddFlag(transform);

			constexpr size_t filter_menu_configuration = button_configuration | UI_CONFIG_FILTER_MENU_ALL | UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE;

			const char* filter_labels[] = { "Info", "Warn", "Error", "Trace" };
			Stream<const char*> filter_stream = Stream<const char*>(filter_labels, std::size(filter_labels));
			UIConfigFilterMenuNotify menu_notify;
			menu_notify.notifier = &data->filter_message_type_changed;
			config.AddFlag(menu_notify);
			drawer.FilterMenu<filter_menu_configuration>(config, "Filter", filter_stream, &data->filter_info);
			config.flag_count -= 2;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("System");
			config.AddFlag(transform);
			menu_notify.notifier = &data->system_filter_changed;
			config.AddFlag(menu_notify);
			Stream<const char*> system_filter_stream = Stream<const char*>(data->console->system_filter_strings.buffer, data->console->system_filter_strings.size);
			drawer.FilterMenu<filter_menu_configuration>(config, "System", system_filter_stream, data->system_filter);
			config.flag_count -= 2;

			transform.position.x += transform.scale.x + border_thickness.x;
			transform.scale = drawer.GetLabelScale("Verbosity");
			config.AddFlag(transform);
			const char* verbosity_labels[] = { "Minimal", "Medium", "Detailed" };
			Stream<const char*> verbosity_label_stream = Stream<const char*>(verbosity_labels, std::size(verbosity_labels));

			UIConfigComboBoxPrefix verbosity_prefix;
			verbosity_prefix.prefix = "Verbosity: ";
			config.AddFlag(verbosity_prefix);
			drawer.ComboBox<button_configuration | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_DO_NOT_CACHE>(config, "Verbosity", verbosity_label_stream, verbosity_label_stream.size, &data->console->verbosity_level);
			config.flag_count -= 2;

			float2 verbosity_label_scale = drawer.GetLastSolidColorRectangleScale<UI_CONFIG_LATE_DRAW>(1);
			transform.position.x += verbosity_label_scale.x /*+ border_thickness.x*/;
			transform.scale = drawer.GetLabelScale("Dump");
			config.AddFlag(transform);
			const char* dump_label_ptrs[] = { "Count Messages", "On Call", "None" };
			Stream<const char*> dump_labels = Stream<const char*>(dump_label_ptrs, std::size(dump_label_ptrs));

			float2 get_position = { 0.0f, 0.0f }, get_scale = {0.0f, 0.0f};
			UIConfigGetTransform get_transform;
			get_transform.position = &get_position;
			get_transform.scale = &get_scale;
			config.AddFlag(get_transform);

			UIConfigComboBoxPrefix dump_prefix;
			dump_prefix.prefix = "Dump type: ";
			config.AddFlag(dump_prefix);

			drawer.ComboBox<button_configuration | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_GET_TRANSFORM>(
				config, "Dump", dump_labels, dump_labels.size, (unsigned char*)&data->console->dump_type
				);
			config.flag_count -= 3;

			transform.position.x = get_position.x + get_scale.x;

#pragma endregion

#pragma region Messages

			UIConfigTextParameters parameters;
			float2 icon_scale = drawer.GetSquareScale(drawer.layout.default_element_y);
			const bool* filter_ptr = &data->filter_info;

			size_t system_mask = GetSystemMaskFromConsoleWindowData(data);
			
			auto draw_sentence = [&](const ConsoleMessage& console_message) {
				drawer.NextRow();
				if (console_message.icon_type == ConsoleMessageType::None) {
					drawer.OffsetX(icon_scale.x + drawer.layout.element_indentation);
				}
				else {
					if (console_message.icon_type == ConsoleMessageType::Error) {
						drawer.SpriteRectangle<UI_CONFIG_MAKE_SQUARE>(config, ConsoleTextureIcons[(unsigned int)ConsoleMessageType::Error]);
					}
					else {
						drawer.SpriteRectangle<UI_CONFIG_MAKE_SQUARE>(config, ConsoleTextureIcons[(unsigned int)console_message.icon_type], console_message.color);
					}
				}
				parameters.color = console_message.color;

				config.AddFlag(parameters);
				drawer.Sentence<UI_CONFIG_SENTENCE_FIT_SPACE | UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE
					| UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_PARAMETERS>(config, console_message.message.buffer);
				config.flag_count--;
			};

			auto draw_sentence_collapsed = [&](const ConsoleMessage& console_message) {
				bool do_draw = true;
				unsigned char type_index = (unsigned char)console_message.icon_type;
				bool is_verbosity_valid = console_message.verbosity <= data->console->verbosity_level;
				bool is_system_valid = (console_message.system_filter & system_mask) != 0;
				do_draw = filter_ptr[type_index] && is_verbosity_valid && is_system_valid;

				if (do_draw) {
					draw_sentence(console_message);
				}

				return do_draw;
			};

			if (data->collapse) {
				// Draw only unique message alongside their  ounter
				Stream<UniqueConsoleMessage> unique_counters = data->unique_messages.GetValueStream();
				for (size_t index = 0; index < unique_counters.size; index++) {
					if (data->unique_messages.IsItemAt(index)) {
						bool do_draw = draw_sentence_collapsed(unique_counters[index].message);

						if (do_draw) {
							// draw the counter
							parameters.color = unique_counters[index].message.color;
							char temp_characters[256];
							Stream<char> temp_stream = Stream<char>(temp_characters, 0);
							function::ConvertIntToChars(temp_stream, unique_counters[index].counter);
							float2 label_scale = drawer.GetLabelScale(temp_stream);
							float2 aligned_position = drawer.GetAlignedToRightOverLimit(label_scale.x);

							UIConfigAbsoluteTransform absolute_transform;
							absolute_transform.position = aligned_position;
							absolute_transform.scale = label_scale;
							config.AddFlags(parameters, absolute_transform);
							drawer.TextLabel<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_BORDER | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE
								| UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_TEXT_PARAMETERS>(
									config, temp_characters
									);
							config.flag_count -= 2;
						}
					}
				}
			}
			else {
				// Draw each message one by one
				for (size_t index = 0; index < data->filtered_message_indices.size; index++) {
					draw_sentence(data->console->messages[data->filtered_message_indices[index]]);
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

			size_t initial_text_sprite_count = *drawer.HandleTextSpriteCount<UI_CONFIG_LATE_DRAW>();
			size_t initial_solid_color_count = *drawer.HandleSolidColorCount<UI_CONFIG_LATE_DRAW>();
			size_t initial_sprite_count = *drawer.HandleSpriteCount<UI_CONFIG_LATE_DRAW>();

			auto counter_backwards = [&](LPCWSTR texture, Color color, bool* filter_status, unsigned int counter) {
				constexpr size_t configuration = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_FIT_SPACE 
					| UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_LATE_DRAW;

				if (counter == 0 || !(*filter_status)) {
					color = drawer.color_theme.unavailable_text;
				}

				Stream<char> stream = Stream<char>(temp_characters, 0);
				function::ConvertIntToCharsFormatted(stream, static_cast<int64_t>(counter));
				float2 label_scale = drawer.GetLabelScale(temp_characters);

				float initial_x_position = transform.position.x;
				transform.position.x -= label_scale.x - drawer.element_descriptor.label_horizontal_padd;

				transform.position.y += drawer.element_descriptor.label_vertical_padd;
				config.AddFlag(transform);
				drawer.Text<configuration | UI_CONFIG_DO_NOT_CACHE>(config, temp_characters);
				transform.position.y -= drawer.element_descriptor.label_vertical_padd;
				config.flag_count--;

				transform.position.x -= drawer.element_descriptor.label_horizontal_padd + sprite_scale.x;
				transform.scale = sprite_scale;
				config.AddFlag(transform);
				drawer.SpriteRectangle<configuration>(config, texture, color);
				config.flag_count--;

				transform.position.x -= drawer.element_descriptor.label_horizontal_padd;
				transform.scale.x = { initial_x_position - transform.position.x };
				config.AddFlag(transform);

				drawer.SolidColorRectangle<configuration | UI_CONFIG_BORDER>(config, drawer.color_theme.theme);
				config.flag_count--;

				UIDrawerStateTableBoolClickable clickable_data;
				clickable_data.notifier = &data->filter_message_type_changed;
				clickable_data.state = filter_status;
				drawer.AddDefaultClickableHoverable(
					{ transform.position.x, transform.position.y - drawer.region_render_offset.y }, 
					transform.scale, 
					{ StateTableBoolClickable, &clickable_data, sizeof(clickable_data), UIDrawPhase::Late },
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
				transform.position.x += drawer.element_descriptor.label_horizontal_padd;
				transform.scale = sprite_scale;
				config.AddFlag(transform);
				drawer.SpriteRectangle<configuration>(config, texture, color);
				config.flag_count--;

				transform.position.x += sprite_scale.x + drawer.element_descriptor.label_horizontal_padd;
				transform.position.y += drawer.element_descriptor.label_vertical_padd;
				config.AddFlag(transform);
				drawer.Text<configuration | UI_CONFIG_DO_NOT_CACHE>(config, temp_characters);
				transform.position.y -= drawer.element_descriptor.label_vertical_padd;
				config.flag_count--;

				transform.position.x += label_scale.x - drawer.element_descriptor.label_horizontal_padd;
				transform.scale.x = { transform.position.x - initial_x_position };
				transform.position.x = initial_x_position;
				config.AddFlag(transform);

				drawer.SolidColorRectangle<configuration | UI_CONFIG_BORDER>(config, drawer.color_theme.theme);
				config.flag_count--;

				UIDrawerStateTableBoolClickable clickable_data;
				clickable_data.notifier = &data->filter_message_type_changed;
				clickable_data.state = filter_status;
				drawer.AddDefaultClickableHoverable(
					{ transform.position.x, transform.position.y - drawer.region_render_offset.y },
					transform.scale,
					{ StateTableBoolClickable, &clickable_data, sizeof(clickable_data), UIDrawPhase::Late },
					drawer.color_theme.theme
				);
				transform.position.x += border_thickness.x + transform.scale.x;
			};

			UIDrawerBufferState drawer_state = drawer.GetBufferState<UI_CONFIG_LATE_DRAW>();
			UIDrawerHandlerState handler_state = drawer.GetHandlerState();

			counter_backwards(ECS_TOOLS_UI_TEXTURE_TRACE_ICON, CONSOLE_TRACE_COLOR, &data->filter_trace, data->trace_count);
			counter_backwards(ECS_TOOLS_UI_TEXTURE_ERROR_ICON, ECS_COLOR_WHITE, &data->filter_error, data->error_count);
			counter_backwards(ECS_TOOLS_UI_TEXTURE_WARN_ICON, CONSOLE_WARN_COLOR, &data->filter_warn, data->warn_count);
			counter_backwards(ECS_TOOLS_UI_TEXTURE_INFO_ICON, CONSOLE_INFO_COLOR, &data->filter_info, data->info_count);

			// if it overpassed the bound, revert to the initial state and redo the drawing from the bound
			if (transform.position.x < counter_bound) {
				drawer.RestoreBufferState<UI_CONFIG_LATE_DRAW>(drawer_state);
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

		ECS_TEMPLATE_FUNCTION_BOOL(void, ConsoleWindowDraw, void*, void*);

		// -------------------------------------------------------------------------------------------------------

		size_t GetSystemMaskFromConsoleWindowData(const ConsoleWindowData* data)
		{
			size_t mask = 0;
			for (size_t index = 0; index < data->console->system_filter_strings.size; index++) {
				mask |= (size_t)data->system_filter[index] << index;
			}
			return mask;
		}

		unsigned int CreateConsoleWindow(UISystem* system, Console* console) {
			UIWindowDescriptor descriptor;

			descriptor.draw = ConsoleWindowDraw<false>;
			descriptor.initialize = ConsoleWindowDraw<true>;

			descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, CONSOLE_WINDOW_SIZE.x);
			descriptor.initial_size_x = CONSOLE_WINDOW_SIZE.x;
			descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, CONSOLE_WINDOW_SIZE.y);
			descriptor.initial_size_y = CONSOLE_WINDOW_SIZE.y;

			ConsoleWindowData window_data;
			CreateConsoleWindowData(window_data, console);

			descriptor.window_data = &window_data;
			descriptor.window_data_size = sizeof(window_data);
			descriptor.window_name = CONSOLE_WINDOW_NAME;

			return system->Create_Window(descriptor);
		}

		void CreateConsole(UISystem* system, Console* console) {
			unsigned int window_index = system->GetWindowFromName(CONSOLE_WINDOW_NAME);

			if (window_index == -1) {
				window_index = CreateConsoleWindow(system, console);
				float2 window_position = system->GetWindowPosition(window_index);
				float2 window_scale = system->GetWindowScale(window_index);
				system->CreateDockspace({ window_position, window_scale }, DockspaceType::FloatingHorizontal, window_index, false);
				console->Dump();
			}
			else {
				system->SetActiveWindow(window_index);
			}
		}

		void CreateConsoleAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			// Create console handles the case if it already exists
			Console* console = (Console*)_data;
			CreateConsole(system, console);
		}

		void CreateConsoleWindowData(ConsoleWindowData& data, Console* console)
		{
			data.console = console;
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
			data.filtered_message_indices = containers::ResizableStream<unsigned int, MemoryManager>(console->messages.allocator, 0);
		}

		Console::Console(MemoryManager* allocator, TaskManager* task_manager, const wchar_t* _dump_path) : allocator(allocator),
			pause_on_error(false), verbosity_level(CONSOLE_VERBOSITY_DETAILED), dump_type(ConsoleDumpType::CountMessages),
			task_manager(task_manager), last_dumped_message(0), dump_count_for_commit(1) {
			format = CONSOLE_HOUR | CONSOLE_MINUTES | CONSOLE_SECONDS;
			messages = containers::ResizableStream<ConsoleMessage, MemoryManager>(allocator, 0);
			system_filter_strings = containers::ResizableStream<const char*, MemoryManager>(allocator, 0);

			// Make a stable reference to the dump path
			size_t dump_path_size = wcslen(_dump_path) + 1;
			void* allocation = allocator->Allocate(sizeof(wchar_t) * dump_path_size, alignof(wchar_t));
			memcpy(allocation, _dump_path, sizeof(wchar_t) * dump_path_size);
			dump_path = (const wchar_t*)allocation;
		}

		void Console::Lock()
		{
			lock.lock();
		}

		void Console::Clear()
		{
			for (size_t index = 0; index < messages.size; index++) {
				allocator->Deallocate(messages[index].message.buffer);
			}
			messages.FreeBuffer();
			last_dumped_message = 0;
			Dump();
		}

		void Console::AddSystemFilterString(const char* string)
		{
			AddSystemFilterString(Stream<char>(string, strlen(string)));
		}

		void Console::AddSystemFilterString(Stream<char> string) {
			char* new_string = (char*)function::Copy(messages.allocator, string.buffer, (string.size + 1) * sizeof(char), alignof(char));
			new_string[string.size] = '\0';
			system_filter_strings.Add(new_string);
		}

		size_t Console::GetFormatCharacterCount() const
		{
			size_t count = 0;
			count += function::PredicateValue(function::HasFlag(format, CONSOLE_MILLISECONDS), 4, 0);
			count += function::PredicateValue(function::HasFlag(format, CONSOLE_SECONDS), 3, 0);
			count += function::PredicateValue(function::HasFlag(format, CONSOLE_MINUTES), 3, 0);
			count += function::PredicateValue(function::HasFlag(format, CONSOLE_HOUR), 2, 0);
			count += function::PredicateValue(function::HasFlag(format, CONSOLE_DAY), 3, 0);
			count += function::PredicateValue(function::HasFlag(format, CONSOLE_MONTH), 3, 0);
			count += function::PredicateValue(function::HasFlag(format, CONSOLE_YEAR), 5, 0);

			count += 3;
			return count;
		}

		void Console::ConvertToMessage(const char* message, ConsoleMessage& console_message)
		{
			size_t size = strlen(message);
			size_t format_character_count = GetFormatCharacterCount();

			void* allocation = allocator->Allocate(sizeof(char) * (size + 1 + format_character_count), alignof(char));

			Stream<char> stream = Stream<char>(allocation, 0);
			WriteFormatCharacters(stream);
			console_message.client_message_start = stream.size;

			memcpy((void*)((uintptr_t)allocation + stream.size), message, sizeof(char) * size);
			stream.size += size;
			stream[stream.size] = '\0';
			console_message.message = stream;
		}

		void Console::ConvertToMessage(Stream<char> message, ConsoleMessage& console_message)
		{
			size_t format_character_count = GetFormatCharacterCount();

			void* allocation = allocator->Allocate(sizeof(char) * (message.size + 1 + format_character_count), alignof(char));

			Stream<char> stream = Stream<char>(allocation, 0);
			WriteFormatCharacters(stream);
			console_message.client_message_start = stream.size;

			memcpy((void*)((uintptr_t)allocation + stream.size), message.buffer, sizeof(char) * message.size);
			stream.size += message.size;
			stream[stream.size] = '\0';
			console_message.message = stream;
		}

		void Console::ChangeDumpPath(const wchar_t* new_path)
		{
			ChangeDumpPath(ToStream(new_path));
		}

		void Console::ChangeDumpPath(Stream<wchar_t> new_path)
		{
			allocator->Deallocate(dump_path);
			size_t new_path_size = new_path.size + 1;
			void* allocation = allocator->Allocate(sizeof(wchar_t) * new_path_size, alignof(wchar_t));
			new_path.CopyTo(allocation);
			wchar_t* temp_char = (wchar_t*)allocation;
			temp_char[new_path.size] = L'\0';
			dump_path = (const wchar_t*)allocation;

			Dump();
		}

		void Console::AppendDump()
		{
			ConsoleDumpData dump_data;
			dump_data.console = this;
			dump_data.starting_index = last_dumped_message;

			ThreadTask task = { ConsoleAppendToDump, &dump_data };
			task_manager->AddDynamicTaskAndWake(task, sizeof(dump_data));
		}

		void Console::Dump()
		{
			ConsoleDumpData dump_data;
			dump_data.console = this;
			dump_data.starting_index = 0;

			ThreadTask task = { ConsoleDump, &dump_data };
			task_manager->AddDynamicTaskAndWake(task, sizeof(dump_data));
		}

		void Console::Message(const char* message, ConsoleMessageType type, size_t system_filter, unsigned char verbosity)
		{
			using WriteFunction = void (Console::*)(const char*, size_t, unsigned char);
			WriteFunction write_functions[] = { &Console::Info, &Console::Warn, &Console::Error, &Console::Trace, &Console::Info };
			(this->*write_functions[(unsigned int)type])(message, system_filter, verbosity);
		}

		void Console::Message(Stream<char> message, ConsoleMessageType type, size_t system_filter, unsigned char verbosity) {
			using WriteFunction = void (Console::*)(Stream<char>, size_t, unsigned char);
			WriteFunction write_functions[] = { &Console::Info, &Console::Warn, &Console::Error, &Console::Trace, &Console::Info };
			(this->*write_functions[(unsigned int)type])(message, system_filter, verbosity);
		}

		void Console::MessageAtomic(const char* message, ConsoleMessageType type, size_t system_filter, unsigned char verbosity)
		{
			using WriteFunction = void (Console::*)(const char*, size_t, unsigned char);
			WriteFunction write_functions[] = { &Console::InfoAtomic, &Console::WarnAtomic, &Console::ErrorAtomic, &Console::TraceAtomic, &Console::InfoAtomic };
			(this->*write_functions[(unsigned int)type])(message, system_filter, verbosity);
		}

		void Console::MessageAtomic(Stream<char> message, ConsoleMessageType type, size_t system_filter, unsigned char verbosity)
		{
			using WriteFunction = void (Console::*)(Stream<char>, size_t, unsigned char);
			WriteFunction write_functions[] = { &Console::InfoAtomic, &Console::WarnAtomic, &Console::ErrorAtomic, &Console::TraceAtomic, &Console::InfoAtomic };
			(this->*write_functions[(unsigned int)type])(message, system_filter, verbosity);
		}

		void Console::Write(const char* message, ConsoleMessage& console_message)
		{
			ConvertToMessage(message, console_message);
			messages.Add(console_message);
			if (dump_type == ConsoleDumpType::CountMessages) {
				if (messages.size > last_dumped_message) {
					if (messages.size - last_dumped_message >= dump_count_for_commit) {
						ConsoleDumpData dump_data;
						dump_data.console = this;
						dump_data.starting_index = last_dumped_message;

						ThreadTask task = { ConsoleAppendToDump, &dump_data };
						task_manager->AddDynamicTaskAndWake(task, sizeof(dump_data));
					}
				}
				else {
					ConsoleDumpData dump_data;
					dump_data.console = this;
					dump_data.starting_index = 0;

					ThreadTask task = { ConsoleDump, &dump_data };
					task_manager->AddDynamicTaskAndWake(task, sizeof(dump_data));
				}
			}
		}

		void Console::Write(Stream<char> stream, ConsoleMessage& console_message) {
			ConvertToMessage(stream, console_message);
			messages.Add(console_message);
			if (dump_type == ConsoleDumpType::CountMessages) {
				if (messages.size > last_dumped_message) {
					if (messages.size - last_dumped_message >= dump_count_for_commit) {
						ConsoleDumpData dump_data;
						dump_data.console = this;
						dump_data.starting_index = last_dumped_message;

						ThreadTask task = { ConsoleAppendToDump, &dump_data };
						task_manager->AddDynamicTaskAndWake(task, sizeof(dump_data));
					}
				}
				else {
					ConsoleDumpData dump_data;
					dump_data.console = this;
					dump_data.starting_index = 0;

					ThreadTask task = { ConsoleDump, &dump_data };
					task_manager->AddDynamicTaskAndWake(task, sizeof(dump_data));
				}
			}
		}

		void Console::WriteAtomic(const char* message, ConsoleMessage& console_message) {
			Lock();
			Write(message, console_message);
			Unlock();
		}

		void Console::WriteAtomic(Stream<char> message, ConsoleMessage& console_message) {
			Lock();
			Write(message, console_message);
			Unlock();
		}

		void Console::Info(const char* message, size_t system_filter, unsigned char verbosity) {
			Info(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::Info(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			ConsoleMessage console_message;
			console_message.icon_type = ConsoleMessageType::Info;
			console_message.system_filter = system_filter;
			console_message.verbosity = verbosity;
			console_message.color = CONSOLE_INFO_COLOR;
			Write(message, console_message);
		}

		void Console::InfoAtomic(const char* message, size_t system_filter, unsigned char verbosity) {
			InfoAtomic(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::InfoAtomic(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			Lock();
			Info(message, system_filter, verbosity);
			Unlock();
		}

		void Console::Warn(const char* message, size_t system_filter, unsigned char verbosity) {
			Warn(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::Warn(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			ConsoleMessage console_message;
			console_message.icon_type = ConsoleMessageType::Warn;
			console_message.system_filter = system_filter;
			console_message.verbosity = verbosity;
			console_message.color = CONSOLE_WARN_COLOR;
			Write(message, console_message);
		}

		void Console::WarnAtomic(const char* message, size_t system_filter, unsigned char verbosity) {
			WarnAtomic(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::WarnAtomic(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			Lock();
			Warn(message, system_filter, verbosity);
			Unlock();
		}

		void Console::Error(const char* message, size_t system_filter, unsigned char verbosity) {
			Error(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::Error(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			ConsoleMessage console_message;
			console_message.icon_type = ConsoleMessageType::Error;
			console_message.system_filter = system_filter;
			console_message.verbosity = verbosity;
			console_message.color = CONSOLE_ERROR_COLOR;
			Write(message, console_message);
			if (pause_on_error) {
				__debugbreak();
			}
		}

		void Console::ErrorAtomic(const char* message, size_t system_filter, unsigned char verbosity) {
			ErrorAtomic(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::ErrorAtomic(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			Lock();
			Error(message, system_filter, verbosity);
			Unlock();
		}

		void Console::Trace(const char* message, size_t system_filter, unsigned char verbosity) {
			Trace(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::Trace(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			ConsoleMessage console_message;
			console_message.icon_type = ConsoleMessageType::Trace;
			console_message.system_filter = system_filter;
			console_message.verbosity = verbosity;
			console_message.color = CONSOLE_TRACE_COLOR;
			Write(message, console_message);
		}

		void Console::TraceAtomic(const char* message, size_t system_filter, unsigned char verbosity) {
			TraceAtomic(Stream<char>(message, strlen(message)), system_filter, verbosity);
		}

		void Console::TraceAtomic(Stream<char> message, size_t system_filter, unsigned char verbosity) {
			Lock();
			Trace(message, system_filter, verbosity);
			Unlock();
		}

		void Console::WriteFormatCharacters(Stream<char>& characters)
		{
			characters.Add('[');

			SYSTEMTIME system_time;
			GetLocalTime(&system_time);

			auto flag = [&](size_t integer) {
				char temp[256];
				Stream<char> temp_stream = Stream<char>(temp, 0);
				function::ConvertIntToChars(temp_stream, integer);
				for (size_t index = 0; index < temp_stream.size; index++) {
					characters.Add(temp_stream[index]);
				}
			};

			bool has_hour = false;
			if (function::HasFlag(format, CONSOLE_HOUR)) {
				flag(system_time.wHour);
				has_hour = true;
			}

			bool has_minutes = false;
			if (function::HasFlag(format, CONSOLE_MINUTES)) {
				if (has_hour) {
					characters.Add(':');
				}
				has_minutes = true;
				flag(system_time.wMinute);
			}

			bool has_seconds = false;
			if (function::HasFlag(format, CONSOLE_SECONDS)) {
				if (has_minutes || has_hour) {
					characters.Add(':');
				}
				has_seconds = true;
				flag(system_time.wSecond);
			}

			bool has_milliseconds = false;
			if (function::HasFlag(format, CONSOLE_MILLISECONDS)) {
				if (has_hour || has_minutes || has_minutes) {
					characters.Add(':');
				}
				has_milliseconds = true;
				flag(system_time.wMilliseconds);
			}

			bool has_hour_minutes_seconds_milliseconds = has_hour || has_minutes || has_seconds || has_milliseconds;
			bool has_space_been_written = false;

			bool has_day = false;
			if (function::HasFlag(format, CONSOLE_DAY)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(' ');
					has_space_been_written = true;
				}
				has_day = true;
				flag(system_time.wDay);
			}

			bool has_month = false;
			if (function::HasFlag(format, CONSOLE_MONTH)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(' ');
					has_space_been_written = true;
				}
				if (has_day) {
					characters.Add('-');
				}
				has_month = true;
				flag(system_time.wMonth);
			}

			if (function::HasFlag(format, CONSOLE_YEAR)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(' ');
					has_space_been_written = true;
				}
				if (has_day || has_month) {
					characters.Add('-');
				}
				flag(system_time.wYear);
			}

			characters.Add(']');
			characters.Add(' ');
		}

		void Console::Unlock() {
			lock.unlock();
		}

		void Console::TryLock() {
			lock.try_lock();
		}

		void Console::SetDumpType(ConsoleDumpType type, unsigned int count)
		{
			dump_type = type;
			dump_count_for_commit = count;
		}

		void Console::SetFormat(size_t _format)
		{
			format = _format;
		}

		void Console::SetVerbosity(unsigned char new_level)
		{
			verbosity_level = new_level;
		}

	}

}