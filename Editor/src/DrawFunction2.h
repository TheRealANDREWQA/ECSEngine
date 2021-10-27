//#pragma once
//#include "ECSEngine.h"
//
//using namespace ECSEngine;
//using namespace ECSEngine::Tools;
//
//
//template<bool initializer>
//void HierarchyDraw(void* drawer_ptr, void* window_data, void* hierarchy_ptr) {
//	using namespace ECSEngine::Tools;
//	using namespace ECSEngine;
//
//	UI_HIERARCHY_NODE_FUNCTION(initializer);
//	drawer->Text("Amazing");
//}
//
//template<bool initializer>
//void Hierarchy2(void* drawer_ptr, void* window_data, void* hierarchy_ptr) {
//	using namespace ECSEngine::Tools;
//	using namespace ECSEngine;
//
//	UI_HIERARCHY_NODE_FUNCTION(initializer);
//	drawer->Text("Element name unknown, #Add file");
//}
//
//template<bool initializer>
//void Hierarchy3(void* drawer_ptr, void* window_data, void* hierarchy_ptr) {
//	using namespace ECSEngine::Tools;
//	using namespace ECSEngine;
//
//	UI_HIERARCHY_NODE_FUNCTION(initializer);
//	drawer->SolidColorRectangle(ECS_COLOR_BROWN);
//
//	drawer->NextRow();
//	drawer->CollapsingHeader("You are handsome", [&]() {
//		drawer->SpriteRectangle(ECS_TOOLS_UI_TEXTURE_CIRCLE);
//		drawer->NextRow();
//		drawer->Button("Remove node", { RemoveNode, hierarchy_ptr, 0 });
//		});
//}
//
//template<bool initializer>
//void List1(void* drawer_ptr, void* window_data, void* list_ptr) {
//	UI_LIST_NODE_FUNCTION(initializer);
//
//	drawer->Text("This is illegal");
//	drawer->ListFinalizeNode(list);
//}
//
//template<bool initializer>
//void List2(void* drawer_ptr, void* window_data, void* list_ptr) {
//	UI_LIST_NODE_FUNCTION(initializer);
//
//	drawer->Text("Item was not found");
//	drawer->ListFinalizeNode(list);
//}
//
//template<bool initializer>
//void List3(void* drawer_ptr, void* window_data, void* list_ptr) {
//	UI_LIST_NODE_FUNCTION(initializer);
//
//	drawer->TextLabel("Report completed: You are OP");
//	drawer->ListFinalizeNode(list);
//}
//
//template<bool initializer>
//void List4(void* drawer_ptr, void* window_data, void* list_ptr) {
//	UI_LIST_NODE_FUNCTION(initializer);
//
//	drawer->TextLabel("New node");
//	//drawer->NextRow();
//
//	UIDrawConfig config;
//	UIConfigRelativeTransform transform;
//	transform.scale.x = 2.0f;
//	transform.scale.y = 1.5f;
//
//	config.AddFlag(transform);
//	drawer->TextLabel<UI_CONFIG_RELATIVE_TRANSFORM>(config, "New stuff huhe");
//	drawer->ListFinalizeNode(list);
//}
//
//template<bool initializer>
//void HierarchyInside(void* drawer_ptr, void* window_data, void* hierarchy_ptr) {
//	using namespace ECSEngine::Tools;
//	using namespace ECSEngine;
//
//	UI_HIERARCHY_NODE_FUNCTION(initializer);
//
//	UIDrawConfig config;
//
//	UIConfigHierarchySelectable selectable;
//	selectable.pointer = (unsigned int*)((uintptr_t)window_data + 184);
//	config.AddFlag(selectable);
//	if constexpr (initializer) {
//		hierarchy->SetChildData(config);
//	}
//	hierarchy = drawer->Hierarchy<UI_CONFIG_HIERARCHY_DRAG_NODE | UI_CONFIG_HIERARCHY_CHILD>(config, "Interesting");
//	if constexpr (initializer) {
//		hierarchy->SetData(hierarchy);
//		hierarchy->AddNode("General", Hierarchy2<false>, Hierarchy2<true>);
//		hierarchy->AddNode("Lieutenant", Hierarchy2<false>, Hierarchy2<true>);
//		hierarchy->AddNode("Soldier", Hierarchy3<false>, Hierarchy3<true>);
//	}
//}
//
//static void RemoveNode(ECSEngine::Tools::ActionData* action_data) {
//	UI_UNPACK_ACTION_DATA;
//
//	ECSEngine::Tools::UIDrawerHierarchy* hierarchy = (ECSEngine::Tools::UIDrawerHierarchy*)_data;
//	hierarchy->RemoveNode("Nothing important");
//}
//
//static void HorizontalTag0(ActionData* action_data) {
//	UI_UNPACK_ACTION_DATA;
//
//	UIDrawerGraphTagData* data = (UIDrawerGraphTagData*)_data;
//	UIDrawer<false>* drawer = (UIDrawer<false>*)data->drawer;
//
//	UIConfigAbsoluteTransform transform;
//	transform.position = data->position;
//	transform.scale = data->graph_scale;
//	transform.scale.y = ECS_TOOLS_UI_DOCKSPACE_BORDER_SIZE;
//	UIDrawConfig cross_config;
//	cross_config.AddFlag(transform);
//
//	UIConfigColor coloru;
//	coloru.color = Color(150, 50, 60);
//	cross_config.AddFlag(coloru);
//	drawer->CrossLine<UI_CONFIG_CROSS_LINE_ALIGN_LEFT | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_COLOR | UI_CONFIG_CROSS_LINE_DO_NOT_INFER>(cross_config);
//}
//
//template<bool initializer>
//void DrawSomething2(void* window_data, void* drawer_descriptor) {
//	using namespace ECSEngine::Tools;
//	using namespace ECSEngine;
//
//	UI_PREPARE_DRAWER(initializer);
//
//	static char* Pog = "This is some useful text";
//
//	/*if (Pog[0] == 'T') {
//		Pog = "Count: undefined";
//	}
//	else {
//		Pog = "Total count: undefined";
//	}*/
//
//	if constexpr (!initializer) {
//		/*drawer.SetZoomXFactor(0.5f);
//		drawer.SetZoomYFactor(0.5f);*/
//	}
//
//	constexpr size_t button_configuration = UI_CONFIG_BORDER/* UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_ALIGN | UI_CONFIG_RELATIVE_TRANSFORM*/ /*| UI_CONFIG_WINDOW_DEPENDENT_SIZE*/;
//	constexpr size_t configuration = 0;
//	ECSEngine::Tools::UIDrawConfig button_config;
//	UIConfigBorder border;
//	border.color = ECS_COLOR_LIME;
//	border.is_interior = false;
//	border.thickness = ECS_TOOLS_UI_DOCKSPACE_BORDER_SIZE;
//
//	button_config.AddFlag(border);
//
//	UIConfigWindowDependentSize button_size;
//	UIConfigTextAlignment align;
//	UIConfigRelativeTransform button_transform;
//	button_transform.scale.x = 5.0f;
//	button_transform.scale.y = 5.0f;
//	align.horizontal = TextAlignment::Right;
//	align.vertical = TextAlignment::Bottom;
//	button_size.x_scale_factor = 0.5f;
//	button_size.y_scale_factor = 0.5f;
//	button_size.type = WindowSizeTransformType::Both;
//	button_config.AddFlag(button_size);
//	button_config.AddFlag(align);
//	button_config.AddFlag(button_transform);
//	ECSEngine::Tools::UIDrawConfig config;
//	drawer.Button<button_configuration>(button_config, Pog, { YEA, nullptr, 0 });
//
//	UIConfigRelativeTransform tt;
//	tt.scale.x = 1.5f;
//	tt.scale.y = 1.5f;
//	UIConfigTextAlignment alignu;
//	alignu.horizontal = TextAlignment::Middle;
//	alignu.vertical = TextAlignment::Bottom;
//	UIDrawConfig configu;
//	configu.AddFlags(tt, alignu);
//
//	drawer.TextLabel<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_RELATIVE_TRANSFORM>(configu, "Heya");
//	constexpr size_t configuration2 =/* UI_CONFIG_DO_NOT_CACHE |*/ UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_TEXT_PARAMETERS;
//	constexpr size_t configuration_slider = 0;
//	ECSEngine::Tools::UIDrawConfig config2, config_slider;
//	ECSEngine::Tools::UIConfigWindowDependentSize size;
//	ECSEngine::Tools::UIConfigTextAlignment alignment;
//	ECSEngine::Tools::UIConfigTextParameters text_params;
//	ECSEngine::Tools::UIConfigWindowDependentSize slider_trans;
//	slider_trans.x_scale_factor = 0.6f;
//	slider_trans.y_scale_factor = 1.0f;
//	slider_trans.type = ECSEngine::Tools::WindowSizeTransformType::Horizontal;
//	text_params.color = ECSEngine::Color(20, 160, 0);
//	text_params.size.x = 0.0018f;
//	float fl = 1.0f;
//	int64_t big = 2;
//	config_slider.AddFlag(slider_trans);
//	//size.type = ECSEngine::Tools::WindowSizeTransformType::Vertical;
//	size.type = ECSEngine::Tools::WindowSizeTransformType::Both;
//	size.y_scale_factor = 0.5f;
//	alignment.vertical = ECSEngine::Tools::TextAlignment::Middle;
//	alignment.horizontal = ECSEngine::Tools::TextAlignment::Left;
//	config2.AddFlag(size);
//	config2.AddFlag(text_params);
//	config2.AddFlag(alignment);
//	drawer.NextRow();
//	drawer.FloatSlider<configuration_slider>(config_slider, "Float Slider##2", &fl, -100.0f, 100.0f);
//	//drawer.NextRow();
//
//	CapacityStream<char>* characters = (CapacityStream<char>*)window_data;
//	double* dod = (double*)((uintptr_t)characters + 128);
//	constexpr size_t config_slider2 = UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_RELATIVE_TRANSFORM;
//	UIDrawConfig configerinho;
//	UIConfigRelativeTransform transform__;
//	transform__.scale.x = 2.0f;
//	configerinho.AddFlag(transform__);
//	drawer.DoubleSlider<UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE>(configerinho, "Double slider", dod, -100, 100, 0, 0);
//	//drawer.NextRow();
//	drawer.IntSlider<0>(configerinho, "Int Slider", &big, (int64_t)-1, (int64_t)3);
//	drawer.NextRow();
//	double pog_champu = 1.0f;
//	drawer.DoubleSlider<config_slider2 | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE /*| UI_CONFIG_ELEMENT_NAME_FIRST*/>(configerinho, "Double slider mouse", (double*)((uintptr_t)characters + 64), -100, 100);
//	drawer.NextRow();
//	unsigned int ch = 0;
//	drawer.IntSlider("Int Slider##1", &ch, (unsigned int)0, (unsigned int)2'000'000);
//	drawer.NextRow();
//
//	const char* stack_memory[64];
//	Stream<const char*> labels;
//	labels.buffer = stack_memory;
//	labels.size = 5;
//	labels[0] = "me gucci";
//	labels[1] = "GOOD";
//	labels[2] = "TOLD YA";
//	labels[3] = "HAHAHAH";
//	labels[4] = "VERY LONG TEXT TO BE HONESTU";
//	unsigned char* active_label = (unsigned char*)((uintptr_t)characters + 164);
//
//	UIDrawConfig combo_config;
//	UIConfigColor coloru;
//	coloru.color = *(Color*)((uintptr_t)characters + 160);
//	combo_config.AddFlag(coloru);
//	UIConfigTextParameters combo_font;
//	combo_font.color = ECS_COLOR_BLACK;
//	combo_font.size = { ECS_TOOLS_UI_FONT_SIZE * 0.6f, ECS_TOOLS_UI_FONT_SIZE * 1.2f };
//	//combo_config.AddFlag(combo_font);
//	drawer.ComboBox<UI_CONFIG_COLOR>(combo_config, "YO", labels, 5, active_label);
//	drawer.NextRow();
//	drawer.CheckBox<0>(combo_config, "POG##2", (bool*)((uintptr_t)characters + 200));
//
//	constexpr size_t color_configuration = UI_CONFIG_COLOR_INPUT_RGB_SLIDERS | UI_CONFIG_COLOR_INPUT_HSV_SLIDERS
//		| UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER | UI_CONFIG_COLOR_INPUT_HEX_INPUT;
//	Color* color = (Color*)((uintptr_t)characters + 160);
//	//drawer.IntSlider("Int Slider##2", &color->red, (unsigned char)0, (unsigned char)255);
//	//drawer.ColorInput("Color", color);
//	drawer.ColorInput<color_configuration | UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE>(config, "Color", color);
//	drawer.NextRow();
//
//	constexpr size_t input_configuration = UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_COLOR | UI_CONFIG_TEXT_PARAMETERS;
//	UIDrawConfig input_config;
//	UIConfigRelativeTransform input_transform;
//	UIConfigTextInputHint hint;
//	UIConfigTextParameters text_parameters;
//	if constexpr (initializer) {
//		Color* ptr = (Color*)((uintptr_t)characters + 1200);
//		*ptr = Color(150, 20, 180);
//	}
//	text_parameters.color = *(Color*)((uintptr_t)characters + 1200);
//	hint.characters = "Password";
//	input_transform.scale.x = 6.0f;
//	input_config.AddFlag(input_transform);
//	input_config.AddFlag(hint);
//	input_config.AddFlag(coloru);
//	input_config.AddFlag(text_parameters);
//	characters->capacity = 128;
//	drawer.TextInput<input_configuration | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_TEXT_INPUT_HINT>(input_config, "Text input", characters);
//	drawer.NextRow();
//	drawer.CrossLine<UI_CONFIG_CROSS_LINE_ALIGN_LEFT>(input_config);
//
//	drawer.CollapsingHeader("I THINK I AM POG", [&]() {
//		drawer.CollapsingHeader("DUDUDUDU", [&]() {
//			drawer.TextLabel("I AM more Pog foo");
//			drawer.NextRow();
//			drawer.TextLabel("tha's illegal");
//			});
//		drawer.NextRow(-1.0f);
//
//		});
//
//	//drawer.ChangeThemeColor(ECS_COLOR_GREEN);
//	drawer.TextLabel<configuration>(config, "Another Window");
//	size.y_scale_factor = 1.0f;
//	constexpr size_t configuration3 = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_VERTICAL;
//	ECSEngine::Tools::UIConfigRelativeTransform transform;
//	transform.scale.y = 0.0f;
//	transform.scale.x = 0.0f;
//	//transform.scale.x = ECSEngine::function::Select(true, 0.0f, 1.0f);
//	ECSEngine::Tools::UIDrawConfig config3;
//	config3.AddFlag(transform);
//	drawer.TextLabel<configuration3>(config3, "Demo Window");
//	//drawer.TextLabel<configuration2>(config2, "I wish you well##");
//	drawer.NextRow();
//	//drawer.TextLabel<configuration2>(config2, "I wish you well##1");
//	drawer.NextRow();
//	drawer.Text("YEA DUDEU##20320");
//
//	drawer.NextRow();
//
//	UIConfigHierarchySelectable selectable;
//	selectable.pointer = (unsigned int*)((uintptr_t)characters + 180);
//
//	input_config.AddFlag(selectable);
//	UIDrawerHierarchy* hierarchy = drawer.Hierarchy<UI_CONFIG_HIERARCHY_DRAG_NODE>(input_config, "Inspector");
//	if constexpr (initializer) {
//		hierarchy->AddNode("I THINK I AM MORE POG##22", HierarchyDraw<false>, HierarchyDraw<true>);
//		hierarchy->AddNode("Nothing important", Hierarchy2<false>, Hierarchy2<true>);
//		hierarchy->AddNode("Third is with luck", Hierarchy3<false>, Hierarchy3<true>);
//		hierarchy->AddNode("Recursive", HierarchyInside<false>, HierarchyInside<true>);
//	}
//	drawer.NextRow();
//
//	UIConfigHierarchySpriteTexture list_texture;
//	list_texture.texture = ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE;
//	list_texture.scale_factor = { 0.5f, 0.5f };
//
//	UIDrawConfig list_config;
//	list_config.AddFlag(list_texture);
//	UIDrawerList* list = drawer.List<UI_CONFIG_HIERARCHY_SPRITE_TEXTURE>(list_config, "new list");
//	if constexpr (initializer) {
//		list->AddNode("General2.0", List1<false>, List1<true>);
//		list->AddNode("Lieutenant2.0", List2<false>, List2<true>);
//		list->AddNode("Solider2.0", List3<false>, List3<true>);
//		list->AddNode("PogChamp2.0", List4<false>, List4<true>);
//	}
//
//	drawer.Sentence<UI_CONFIG_SENTENCE_FIT_SPACE | UI_CONFIG_UNAVAILABLE_TEXT_COLOR>(list_config, "This is so much fun that I haven't expected! This is hard to implement\nto be honest, very fiddly");
//	drawer.Sentence<UI_CONFIG_SENTENCE_FIT_SPACE>(list_config, "Unknown command: Please specify a valid argument");
//	drawer.NextRow();
//	drawer.Sentence<UI_CONFIG_SENTENCE_FIT_SPACE>(list_config, "Error when resizing the back buffer. The application will exit.");
//	drawer.NextRow();
//	drawer.Sentence<UI_CONFIG_SENTENCE_FIT_SPACE | UI_CONFIG_TEXT_PARAMETERS>(input_config, "[13:25 21-04-2021]: Creating the back buffer failed.");
//
//	drawer.NextRow();
//	char* table_labels[100];
//	unsigned char* value_char = (unsigned char*)((uintptr_t)characters + 160);
//	if (*value_char % 2 == 0) {
//		for (size_t index = 0; index < 100; index++) {
//			if (index % 4 == 0) {
//				table_labels[index] = "I think i've done great so far";
//			}
//			else if (index % 4 == 1) {
//				table_labels[index] = "PogChamp";
//			}
//			else if (index % 4 == 2) {
//				table_labels[index] = "Interesting";
//			}
//			else {
//				table_labels[index] = "Long text";
//			}
//		}
//	}
//	else {
//		for (size_t index = 0; index < 100; index++) {
//			if (index % 4 == 0) {
//				table_labels[index] = "Lullaby";
//			}
//			else if (index % 4 == 1) {
//				table_labels[index] = "Failed to render";
//			}
//			else if (index % 4 == 2) {
//				table_labels[index] = "Note";
//			}
//			else {
//				table_labels[index] = "Shadow map extents overreached";
//			}
//		}
//	}
//	drawer.TextTable<UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_UNAVAILABLE_TEXT_COLOR>(input_config, "Boring name", 10, 10, (const char**)table_labels);
//	drawer.NextRow();
//
//	UIDrawConfig rectangle_config;
//
//	bool* unavailable_rows = (bool*)((uintptr_t)characters + 250);
//	if constexpr (initializer) {
//		for (size_t index = 0; index < 7; index++) {
//			unavailable_rows[index] = false;
//		}
//		unavailable_rows[1] = true;
//		unavailable_rows[5] = true;
//	}
//
//	float* sample_offset = (float*)((uintptr_t)characters + 300);
//	drawer.FloatSlider("Sample offset", sample_offset, -10.0f, 10.0f);
//	constexpr size_t sample_count = 33;
//
//	constexpr size_t function_count = 4;
//	unsigned char* function_type = (unsigned char*)((uintptr_t)characters + 196);
//	if constexpr (initializer) {
//		*function_type = 0;
//	}
//	const char* graph_function[function_count];
//	Stream<const char*> graph_function_labels = Stream<const char*>(graph_function, function_count);
//	graph_function_labels[0] = "Sin";
//	graph_function_labels[1] = "Cos";
//	graph_function_labels[2] = "Identity";
//	graph_function_labels[3] = "Power of two";
//	drawer.ComboBox("Graph function", graph_function_labels, function_count, function_type);
//
//	float2 temp_values[sample_count];
//	for (int64_t index = 0; index < sample_count; index++) {
//		/*float y = -1.0f;
//		if (index % 2 == 1) {
//			y = 10.0f;
//		}
//		temp_values[index] = { static_cast<float>(index), y };*/
//		//temp_values[index] = { static_cast<float>(index), static_cast<float>(index) };
//		//temp_values[index] = { PI / 36 * static_cast<float>(index), cos(PI / 36 * static_cast<float>(index)) };
//		switch (*function_type) {
//		case 0:
//			temp_values[index] = function::SampleFunction(static_cast<float>(index) * PI / 16 + *sample_offset, [](float value) {
//				return sin(value);
//				});
//			break;
//		case 1:
//			temp_values[index] = function::SampleFunction(static_cast<float>(index) * PI / 16 + *sample_offset, [](float value) {
//				return cos(value);
//				});
//			break;
//		case 2:
//			temp_values[index] = function::SampleFunction(static_cast<float>(index) * PI / 16 + *sample_offset, [](float value) {
//				return value;
//				});
//			break;
//		case 3:
//			temp_values[index] = function::SampleFunction(static_cast<float>(index) * PI / 16 + *sample_offset, [](float value) {
//				return value * value;
//				});
//			break;
//		}
//		/*temp_values[index] = function::SampleFunction(static_cast<float>(index - (int64_t)sample_count / 2), [](float value) {
//			return value * value * value;
//		});*/
//	}
//	Stream<float2> samples = Stream<float2>(temp_values, sample_count);
//
//	UIDrawConfig graph_config;
//	UIConfigRelativeTransform graph_transform;
//	graph_transform.scale.x = 5.0f;
//	graph_transform.scale.y = 5.0f;
//
//	UIConfigWindowDependentSize graph_size;
//	graph_size.type = WindowSizeTransformType::Both;
//	graph_size.x_scale_factor = 0.6f;
//	graph_size.y_scale_factor = 0.5f;
//
//	//graph_config.AddFlag(graph_transform);
//	graph_config.AddFlag(graph_size);
//
//	UIConfigGraphMinY min_y;
//	min_y.value = 0.0f;
//
//	graph_config.AddFlag(min_y);
//
//	UIConfigGraphTags tags;
//	tags.horizontal_tag_count = 2;
//	tags.horizontal_values[0] = 0.3f;
//	tags.horizontal_values[1] = 0.6f;
//	tags.horizontal_tags[0] = HorizontalTag0;
//	tags.horizontal_tags[1] = HorizontalTag0;
//	UIConfigGraphKeepResolutionY resolution_y;
//	resolution_y.max_x_scale = 1.5f;
//	graph_config.AddFlag(resolution_y);
//	graph_config.AddFlag(tags);
//	drawer.Graph<UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_GRAPH_NO_NAME | UI_CONFIG_GRAPH_TAGS
//		| UI_CONFIG_GRAPH_X_AXIS | UI_CONFIG_GRAPH_Y_AXIS | UI_CONFIG_GRAPH_REDUCE_FONT
//		| UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y | UI_CONFIG_GRAPH_DROP_COLOR | UI_CONFIG_GRAPH_SAMPLE_CIRCLES>(graph_config, samples);
//
//	float2 second_sample_values[sample_count];
//	Stream<float2> second_samples = Stream<float2>(second_sample_values, 0);
//	unsigned int* animate_value = (unsigned int*)((uintptr_t)characters + 192);
//	if constexpr (initializer) {
//		*animate_value = 0;
//	}
//	else {
//		(*animate_value)++;
//	}
//	for (size_t index = 0; index < sample_count; index++) {
//		second_samples.Add(function::SampleFunction(static_cast<float>(index) * PI / 16 + static_cast<float>(*animate_value) * 0.05f, [](float value) {
//			return sin(value);
//			}));
//	}
//
//	drawer.NextRow();
//	drawer.Graph<UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_GRAPH_REDUCE_FONT
//		/*| UI_CONFIG_GRAPH_KEEP_RESOLUTION_X*/ /*| UI_CONFIG_GRAPH_DROP_COLOR*/ | UI_CONFIG_GRAPH_SAMPLE_CIRCLES>(graph_config, second_samples, "Animated graph");
//
//	constexpr size_t histogram_sample_count = 10;
//	float histogram_values[histogram_sample_count];
//	Stream<float> histogram_stream = Stream<float>(histogram_values, histogram_sample_count);
//	for (size_t index = 0; index < histogram_sample_count; index++) {
//		histogram_stream[index] = static_cast<float>(index) * 0.05f - 0.25f;
//	}
//	histogram_stream[histogram_sample_count - 1] = 1.0f;
//	histogram_stream[2] = -2.253f;
//	histogram_stream[3] = 0.3845f;
//
//	UIDrawConfig histogram_config;
//	UIConfigWindowDependentSize histogram_size;
//	histogram_size.type = WindowSizeTransformType::Both;
//	histogram_size.x_scale_factor = 0.5f;
//	histogram_size.y_scale_factor = 0.5f;
//
//	drawer.NextRow();
//	histogram_config.AddFlag(histogram_size);
//	drawer.Histogram<UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_HISTOGRAM_REDUCE_FONT>(histogram_config, histogram_stream);
//
//	drawer.NextRow();
//
//	UIDrawConfig multigraph_config;
//	UIConfigRelativeTransform multigraph_transform;
//	multigraph_transform.scale.x = 20.0f;
//	multigraph_transform.scale.y = 7.0f;
//	constexpr size_t multigraph_sample_count = 32;
//	constexpr size_t multigraph_subsample_count = 2;
//
//	UIDrawerMultiGraphSample<multigraph_sample_count> multigraph_samples[multigraph_sample_count];
//	for (size_t index = 0; index < multigraph_sample_count; index++) {
//		float x = static_cast<float>(index) * PI / multigraph_sample_count * 2;
//		multigraph_samples[index].x = x;
//
//		float sin_value = fabs(sin(x));
//		for (size_t subindex = 0; subindex < multigraph_subsample_count; subindex++) {
//			multigraph_samples[index].y[subindex] = sin_value * (1.0f - static_cast<float>(subindex) / multigraph_subsample_count);
//		}
//	}
//
//	multigraph_config.AddFlag(multigraph_transform);
//	constexpr size_t multigraph_configuration = UI_CONFIG_RELATIVE_TRANSFORM;
//	Stream<UIDrawerMultiGraphSample<multigraph_sample_count>> multisamples = Stream<UIDrawerMultiGraphSample<multigraph_sample_count>>(multigraph_samples, multigraph_sample_count);
//	drawer.MultiGraph<multigraph_configuration>(multigraph_config, multisamples, multigraph_subsample_count);
//
//	drawer.NextRow();
//
//	UIDrawConfig menu_config;
//
//	constexpr size_t first_menu_count = 6;
//	UIDrawerMenuState state;
//	state.left_characters = "New project\nSave project\nOpen project\nBuild source\nClean source\nReduce source";
//	state.row_count = first_menu_count;
//	state.submenu_index = 0;
//	state.click_handlers = (UIActionHandler*)((uintptr_t)characters + 440);
//	state.row_has_submenu = (bool*)((uintptr_t)characters + 1008);
//	state.unavailables = (bool*)((uintptr_t)characters + 1000);
//	state.row_has_submenu[0] = true;
//	state.row_has_submenu[1] = false;
//	state.row_has_submenu[2] = false;
//	state.row_has_submenu[3] = false;
//	state.row_has_submenu[4] = false;
//	state.row_has_submenu[5] = false;
//	if constexpr (initializer) {
//		for (size_t index = 0; index < first_menu_count; index++) {
//			state.click_handlers[index] = { SkipAction, nullptr, 0 };
//			state.unavailables[index] = false;
//		}
//		state.unavailables[1] = true;
//		state.unavailables[3] = true;
//	}
//
//	UIDrawerMenuState* second_state;
//
//	if constexpr (initializer) {
//		second_state = drawer.GetMainAllocatorBufferAndStoreAsResource<UIDrawerMenuState>("NANA");
//		second_state->left_characters = "Components\nInternal";
//		second_state->row_count = 2;
//		second_state->submenu_index = 1;
//		second_state->click_handlers = (UIActionHandler*)((uintptr_t)characters + 440);
//		second_state->row_has_submenu = nullptr;
//		second_state->right_characters = nullptr;
//		second_state->submenues = nullptr;
//		second_state->unavailables = nullptr;
//	}
//	else {
//		second_state = (UIDrawerMenuState*)drawer.GetResource("NANA");
//	}
//
//	state.submenues = second_state;
//	drawer.Menu<0>(menu_config, "Menu", &state);
//
//	UIDrawConfig menu_hover_config;
//	UIConfigHoverableAction hoverable;
//
//	UIDrawerMenuRightClickData menu_call_data;
//	menu_call_data.name = "Menu##2";
//	menu_call_data.window_index = drawer.GetWindowIndex();
//	menu_call_data.state = state;
//	hoverable.handler = { RightClickMenu, &menu_call_data, sizeof(menu_call_data), UIDrawPhase::System };
//	UIConfigColor rect_color;
//	rect_color.color = ECS_TOOLS_UI_THEME_COLOR;
//	menu_hover_config.AddFlag(rect_color);
//	menu_hover_config.AddFlag(hoverable);
//	drawer.Rectangle<UI_CONFIG_HOVERABLE_ACTION | UI_CONFIG_COLOR>(menu_hover_config);
//
//	drawer.NextRow();
//
//	constexpr size_t temps_count = 3;
//	float* temps1[temps_count];
//	float temps2[temps_count];
//	float temps3[temps_count];
//	float temps4[temps_count];
//	const char* temp_names[temps_count];
//
//	temps1[0] = (float*)((uintptr_t)characters + 1000);
//	temps1[1] = (float*)((uintptr_t)characters + 1004);
//	temps1[2] = (float*)((uintptr_t)characters + 1008);
//	//temps1[3] = (float*)((uintptr_t)characters + 1012);
//	//temps1[4] = (float*)((uintptr_t)characters + 1016);
//	//temps1[5] = (float*)((uintptr_t)characters + 1020);
//	temps2[0] = 1.0f;
//	temps2[1] = 2.0f;
//	temps2[2] = 3.0f;
//	temps3[0] = 10.0f;
//	temps3[1] = 9.0f;
//	temps3[2] = 8.0f;
//	temps4[0] = 5.0f;
//	temps4[1] = 5.0f;
//	temps4[2] = 5.0f;
//	temp_names[0] = "x:";
//	temp_names[1] = "y:";
//	temp_names[2] = "z:";
//	//temp_names[3] = "x:##0";
//	//temp_names[4] = "y:##0";
//	//temp_names[5] = "z:##0";
//	drawer.FloatSliderGroup<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ELEMENT_NAME_FIRST>(input_config, temps_count, "Translation", temp_names, temps1, temps2, temps3, temps4);
//}