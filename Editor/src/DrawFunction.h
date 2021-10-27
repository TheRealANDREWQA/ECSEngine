//#pragma once
//#include "ECSEngine.h"
//
//using namespace ECSEngine;
//using namespace ECSEngine::Tools;
//
//static void YEA(ECSEngine::Tools::ActionData* action_data) {
//	UI_UNPACK_ACTION_DATA;
//
//	unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
//	system->m_windows[window_index].zoom.x += 0.02f;
//}
//
//static void ExportDescriptors(ActionData* action_data) {
//	UI_UNPACK_ACTION_DATA;
//
//	system->WriteDescriptorsFile((const wchar_t*)_data);
//}
//
//static void ExportUIFile(ActionData* action_data) {
//	UI_UNPACK_ACTION_DATA;
//	
//	system->WriteUIFile((const wchar_t*)_data);
//}
//
//static void ImportUIFile(ActionData* action_data) {
//	UI_UNPACK_ACTION_DATA;
//	
//	const char* ptrs[23];
//	Stream<const char*> ptrsss = Stream<const char*>(ptrs, 0);
//	system->LoadUIFile((const wchar_t*)_data, ptrsss);
//}
//
//template<bool initializer>
//void DrawSomething(void* window_data, void* drawer_descriptor) {
//	using namespace ECSEngine::Tools;
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
//	constexpr size_t configuration = UI_CONFIG_DO_NOT_CACHE;
//	ECSEngine::Tools::UIDrawConfig config;
//	drawer.Button<configuration>(config, Pog, { YEA, nullptr, 0 });
//	constexpr size_t configuration2 =/* UI_CONFIG_DO_NOT_CACHE |*/ UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_TEXT_PARAMETERS;
//	constexpr size_t configuration_slider = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
//	ECSEngine::Tools::UIDrawConfig config2, config_slider;
//	ECSEngine::Tools::UIConfigWindowDependentSize size;
//	ECSEngine::Tools::UIConfigTextAlignment alignment;
//	ECSEngine::Tools::UIConfigTextParameters text_params;
//	ECSEngine::Tools::UIConfigWindowDependentSize slider_trans;
//	slider_trans.scale_factor.x = 0.8f;
//	slider_trans.scale_factor.y = 1.0f;
//	slider_trans.type = ECSEngine::Tools::WindowSizeTransformType::Horizontal;
//	text_params.color = ECSEngine::Color(20, 160, 0);
//	text_params.size.x = 0.0018f;
//	float fl = 1.0f;
//	double dod = 1.0;
//	int64_t big = 1000;
//	config_slider.AddFlag(slider_trans);
//	//size.type = ECSEngine::Tools::WindowSizeTransformType::Vertical;
//	size.type = ECSEngine::Tools::WindowSizeTransformType::Both;
//	size.scale_factor.y = 0.5f;
//	alignment.vertical = ECSEngine::Tools::TextAlignment::Middle;
//	alignment.horizontal = ECSEngine::Tools::TextAlignment::Left;
//	config2.AddFlag(size);
//	config2.AddFlag(text_params);
//	config2.AddFlag(alignment);
//	drawer.NextRow();
//	drawer.FloatSlider<configuration_slider>(config_slider, "Float Slider##2", &fl, -100.0f, 100.0f);
//	//drawer.NextRow();
//	drawer.DoubleSlider("Double slider", &dod, -100, 100);
//	//drawer.NextRow();
//	drawer.IntSlider("Int Slider", &big, (int64_t)500, (int64_t)2500);
//	drawer.NextRow();
//	//drawer.ChangeThemeColor(ECS_COLOR_GREEN);
//	drawer.TextLabel<configuration>(config, "Another Window");
//	size.scale_factor.y = 2.0f;
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
//	drawer.NextRow();
//	
//	char** file_paths = (char**)window_data;
//	drawer.Button("Export descriptors", { ExportDescriptors, file_paths[0], 0 });
//
//	drawer.Button("Export UI file", { ExportUIFile, file_paths[1], 0 });
//	drawer.Button("Import UI file", { ImportUIFile, file_paths[1], 0 });
//
//	drawer.NextRow();
//	static float value = 0.0f;
//	value += 0.003f;
//	float bar_value = sin(value);
//	drawer.ProgressBar("", 1.0f, 1.5f);
//
//	//static float 
//	//auto system = drawer.GetSystem();
//}