#include "ecspch.h"
#include "UIReflection.h"
#include "../Utilities/Mouse.h"
#include "../Utilities/Keyboard.h"

namespace ECSEngine {

	using namespace Reflection;

	const char* BasicTypeNames[] = {
		"x",
		"y",
		"z",
		"w",
		"t",
		"u",
		"v"
	};

	namespace Tools {

		constexpr size_t UI_INT_BASE = 55;
		constexpr size_t UI_INT8_T = (size_t)1 << UI_INT_BASE;
		constexpr size_t UI_UINT8_T = (size_t)1 << (UI_INT_BASE + 1);
		constexpr size_t UI_INT16_T = (size_t)1 << (UI_INT_BASE + 2);
		constexpr size_t UI_UINT16_T = (size_t)1 << (UI_INT_BASE + 3);
		constexpr size_t UI_INT32_T = (size_t)1 << (UI_INT_BASE + 4);
		constexpr size_t UI_UINT32_T = (size_t)1 << (UI_INT_BASE + 5);
		constexpr size_t UI_INT64_T = (size_t)1 << (UI_INT_BASE + 6);
		constexpr size_t UI_UINT64_T = (size_t)1 << (UI_INT_BASE + 7);

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2_EX(function, a, b, starting, ...) function(starting, __VA_ARGS__) \
		function(a, __VA_ARGS__) \
		function(b, __VA_ARGS__) \

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2(function, a, b, ...) ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2_EX(function, a, b, 0, __VA_ARGS__)

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_3(function, a, b, c, ...) function(0, __VA_ARGS__) \
		function(a, __VA_ARGS__) \
		function(b, __VA_ARGS__), \
		function(a | b, __VA_ARGS__) \
		function(c, __VA_ARGS__) \
		function(c | a, __VA_ARGS__) \
		function(c | b, __VA_ARGS__) \
		function(c | a | b, __VA_ARGS__) \

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_4(function, a, b, c, d, ...) function(0, __VA_ARGS__) \
		function(a, __VA_ARGS__) \
		function(b, __VA_ARGS__) \
		function(a | b, __VA_ARGS__) \
		function(c, __VA_ARGS__) \
		function(c | a, __VA_ARGS__) \
		function(c | b, __VA_ARGS__) \
		function(c | a | b, __VA_ARGS__) \
		function(d, __VA_ARGS__) \
		function(d | a, __VA_ARGS__) \
		function(d | b, __VA_ARGS__) \
		function(d | a | b, __VA_ARGS__) \
		function(d | c, __VA_ARGS__) \
		function(d | c | a, __VA_ARGS__) \
		function(d | c | b, __VA_ARGS__) \
		function(d | c | a | b, __VA_ARGS__) 

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_5(function, a, b, c, d, e, ...) function(0, __VA_ARGS__) \
	function(a, __VA_ARGS__) \
	function(b, __VA_ARGS__) \
	function(a | b, __VA_ARGS__) \
	function(c, __VA_ARGS__) \
	function(c | a, __VA_ARGS__) \
	function(c | b, __VA_ARGS__) \
	function(c | a | b, __VA_ARGS__) \
	function(d, __VA_ARGS__) \
	function(d | a, __VA_ARGS__) \
	function(d | b, __VA_ARGS__) \
	function(d | a | b, __VA_ARGS__) \
	function(d | c, __VA_ARGS__) \
	function(d | c | a, __VA_ARGS__) \
	function(d | c | b, __VA_ARGS__) \
	function(d | c | a | b, __VA_ARGS__) \
	function(e, __VA_ARGS__) \
	function(e | a, __VA_ARGS__) \
	function(e | b, __VA_ARGS__) \
	function(e | a | b, __VA_ARGS__) \
	function(e | c, __VA_ARGS__) \
	function(e | c | a, __VA_ARGS__) \
	function(e | c | b, __VA_ARGS__) \
	function(e | c | a | b, __VA_ARGS__) \
	function(e | d, __VA_ARGS__) \
	function(e | d | a, __VA_ARGS__) \
	function(e | d | b, __VA_ARGS__) \
	function(e | d | a | b, __VA_ARGS__) \
	function(e | d | c, __VA_ARGS__) \
	function(e | d | c | a, __VA_ARGS__) \
	function(e | d | c | b, __VA_ARGS__) \
	function(e | d | c | a | b, __VA_ARGS__) 

#define ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(function) switch(configuration_flags) { function }
#define ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(integer_type, type, predication_table, function, ...) case integer_type: ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(predication_table(function, __VA_ARGS__, type)) break;
#define ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(integer_flags, predication_table, function, ...) switch(integer_flags) { \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT8_T, int8_t, predication_table, function, __VA_ARGS__); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT8_T, uint8_t, predication_table, function, __VA_ARGS__); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT16_T, int16_t, predication_table, function, __VA_ARGS__); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT16_T, uint16_t, predication_table, function, __VA_ARGS__); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT32_T, int32_t, predication_table, function, __VA_ARGS__); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT32_T, uint32_t, predication_table, function, __VA_ARGS__); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT64_T, int64_t, predication_table, function, __VA_ARGS__); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT64_T, uint64_t, predication_table, function, __VA_ARGS__); \
		}

		//ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_5(UIReflectionFieldDrawImplementation, UIReflectionSliderInitialize, UIReflection);

		size_t ExtractIntFlags(size_t configuration_flags) {
			return (configuration_flags & 0xFF00000000000000) /*>> 55*/;
		}

		size_t RemoveIntFlags(size_t configuration_flags) {
			return configuration_flags & 0x00FFFFFFFFFFFFFF;
		}

		template<bool initialize>
		void UIReflectionFloatSlider(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionFloatSliderData* data = (UIReflectionFloatSliderData*)_data;

#define SLIDER(configuration) case configuration: drawer.FloatSlider<configuration>(config, data->name, data->value_to_modify, data->lower_bound, data->upper_bound, data->default_value, data->precision); break;
						
			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_4(SLIDER, UI_CONFIG_SLIDER_ENTER_VALUES, UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, UI_CONFIG_SLIDER_DEFAULT_VALUE));

#undef SLIDER
		}

		template ECSENGINE_API void UIReflectionFloatSlider<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionFloatSlider<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionDoubleSlider(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionDoubleSliderData* data = (UIReflectionDoubleSliderData*)_data;

#define SLIDER(configuration) case configuration: drawer.DoubleSlider<configuration>(config, data->name, data->value_to_modify, data->lower_bound, data->upper_bound, data->default_value, data->precision); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_4(SLIDER, UI_CONFIG_SLIDER_ENTER_VALUES, UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, UI_CONFIG_SLIDER_DEFAULT_VALUE));

#undef SLIDER
		}

		template ECSENGINE_API void UIReflectionDoubleSlider<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionDoubleSlider<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionIntSlider(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionIntSliderData* data = (UIReflectionIntSliderData*)_data;

			size_t int_flags = ExtractIntFlags(configuration_flags);
			configuration_flags = RemoveIntFlags(configuration_flags);
#define SLIDER_IMPLEMENTATION(configuration, integer_type) case configuration: drawer.IntSlider<configuration, integer_type>(config, data->name, (integer_type*)data->value_to_modify, *(integer_type*)data->lower_bound, *(integer_type*)data->upper_bound, *(integer_type*)data->default_value); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_4, SLIDER_IMPLEMENTATION, UI_CONFIG_SLIDER_ENTER_VALUES, UI_CONFIG_SLIDER_DEFAULT_VALUE, UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);

#undef SLIDER_IMPLEMENTATION
		}

		template ECSENGINE_API void UIReflectionIntSlider<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionIntSlider<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionFloatInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)_data;
#define INPUT(configuration) case configuration: drawer.FloatInput<configuration>(config, data->name, data->value, data->default_value, data->lower_bound, data->upper_bound); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2(INPUT, UI_CONFIG_NUMBER_INPUT_DEFAULT, UI_CONFIG_NUMBER_INPUT_NO_RANGE));

#undef INPUT
		}

		template ECSENGINE_API void UIReflectionFloatInput<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionFloatInput<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionDoubleInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)_data;
#define INPUT(configuration) case configuration: drawer.DoubleInput<configuration>(config, data->name, data->value, data->default_value, data->lower_bound, data->upper_bound); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2(INPUT, UI_CONFIG_NUMBER_INPUT_DEFAULT, UI_CONFIG_NUMBER_INPUT_NO_RANGE));

#undef INPUT
		}

		template ECSENGINE_API void UIReflectionDoubleInput<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionDoubleInput<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionIntInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionIntInputData* data = (UIReflectionIntInputData*)_data;
			size_t int_flags = ExtractIntFlags(configuration_flags);
			configuration_flags = RemoveIntFlags(configuration_flags);

#define INPUT(configuration, integer) case configuration: { integer default_value = *(integer*)data->default_value; \
		integer upper_bound = *(integer*)data->upper_bound; \
		integer lower_bound = *(integer*)data->lower_bound; \
	drawer.IntInput<configuration, integer>(config, data->name, (integer*)data->value_to_modify, default_value, lower_bound, upper_bound); } break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2_EX, INPUT, UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER, UI_CONFIG_NUMBER_INPUT_NO_RANGE | UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER, UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER);

#undef INPUT
		}

		template ECSENGINE_API void UIReflectionIntInput<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionIntInput<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionTextInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionTextInputData* data = (UIReflectionTextInputData*)_data;

#define INPUT(configuration) case configuration: drawer.TextInput<configuration>(config, data->name, data->characters); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2(INPUT, UI_CONFIG_TEXT_INPUT_HINT, UI_CONFIG_TEXT_INPUT_CALLBACK));

#undef INPUT
		}
		
		template ECSENGINE_API void UIReflectionTextInput<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionTextInput<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionColor(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionColorData* data = (UIReflectionColorData*)_data;

#define COLOR(configuration) case configuration: drawer.ColorInput<configuration>(config, data->name, data->color, data->default_color); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_5(COLOR, UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE, UI_CONFIG_COLOR_INPUT_RGB_SLIDERS, UI_CONFIG_COLOR_INPUT_HSV_SLIDERS, UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER, UI_CONFIG_COLOR_INPUT_CALLBACK));

#undef COLOR
		}

		template ECSENGINE_API void UIReflectionColor<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionColor<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionCheckBox(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionCheckBoxData* data = (UIReflectionCheckBoxData*)_data;
			drawer.CheckBox(data->name, data->value);
		}

		template ECSENGINE_API void UIReflectionCheckBox<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionCheckBox<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionComboBox(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionComboBoxData* data = (UIReflectionComboBoxData*)_data;
			drawer.ComboBox(data->name, data->labels, data->label_display_count, data->active_label);
		}

		template ECSENGINE_API void UIReflectionComboBox<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionComboBox<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionFloatSliderGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)_data;

#define GROUP(configuration) case configuration: drawer.FloatSliderGroup<configuration>(config, data->count, data->group_name, data->input_names, data->values, data->lower_bound, data->upper_bound, data->default_values, data->precision); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_4(GROUP, UI_CONFIG_SLIDER_ENTER_VALUES, UI_CONFIG_SLIDER_DEFAULT_VALUE, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, UI_CONFIG_SLIDER_MOUSE_DRAGGABLE));

#undef GROUP
		}

		template ECSENGINE_API void UIReflectionFloatSliderGroup<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionFloatSliderGroup<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionDoubleSliderGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)_data;

#define GROUP(configuration) case configuration: drawer.DoubleSliderGroup<configuration>(config, data->count, data->group_name, data->input_names, data->values, data->lower_bound, data->upper_bound, data->default_values, data->precision); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_4(GROUP, UI_CONFIG_SLIDER_ENTER_VALUES, UI_CONFIG_SLIDER_DEFAULT_VALUE, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, UI_CONFIG_SLIDER_MOUSE_DRAGGABLE));

#undef GROUP
		}

		template ECSENGINE_API void UIReflectionDoubleSliderGroup<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionDoubleSliderGroup<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionIntSliderGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)_data;
			size_t int_flags = ExtractIntFlags(configuration_flags);
			configuration_flags = RemoveIntFlags(configuration_flags);

#define GROUP(configuration, integer_type) case configuration: drawer.IntSliderGroup<configuration, integer_type>(config, data->count, data->group_name, data->input_names, (integer_type**)data->values, (const integer_type*)data->lower_bound, (const integer_type*)data->upper_bound, (const integer_type*)data->default_values); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_4, GROUP, UI_CONFIG_SLIDER_ENTER_VALUES, UI_CONFIG_SLIDER_DEFAULT_VALUE, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, UI_CONFIG_SLIDER_MOUSE_DRAGGABLE);

#undef GROUP
		}

		template ECSENGINE_API void UIReflectionIntSliderGroup<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionIntSliderGroup<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionFloatInputGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)_data;

#define GROUP(configuration) case configuration: drawer.FloatInputGroup<configuration>(config, data->count, data->group_name, data->input_names, data->values, data->lower_bound, data->upper_bound, data->default_values); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2(GROUP, UI_CONFIG_NUMBER_INPUT_DEFAULT, UI_CONFIG_NUMBER_INPUT_NO_RANGE));

#undef GROUP
		}

		template ECSENGINE_API void UIReflectionFloatInputGroup<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionFloatInputGroup<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionDoubleInputGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)_data;

#define GROUP(configuration) case configuration: drawer.DoubleInputGroup<configuration>(config, data->count, data->group_name, data->input_names, data->values, data->lower_bound, data->upper_bound, data->default_values); break;

			ECS_TOOLS_UI_REFLECT_SWITCH_TABLE(ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2(GROUP, UI_CONFIG_NUMBER_INPUT_DEFAULT, UI_CONFIG_NUMBER_INPUT_NO_RANGE));

#undef GROUP
		}

		template ECSENGINE_API void UIReflectionDoubleInputGroup<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionDoubleInputGroup<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		template<bool initialize>
		void UIReflectionIntInputGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* _data) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)_data;
			size_t int_flags = ExtractIntFlags(configuration_flags);
			configuration_flags = RemoveIntFlags(configuration_flags);

#define GROUP(configuration, integer_type) case configuration: drawer.IntInputGroup<configuration, integer_type>(config, data->count, data->group_name, data->input_names, (integer_type**)data->values, (const integer_type*)data->lower_bound, (const integer_type*)data->upper_bound, (const integer_type*)data->default_values); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2, GROUP, UI_CONFIG_NUMBER_INPUT_DEFAULT, UI_CONFIG_NUMBER_INPUT_NO_RANGE);

#undef GROUP
		}

		template ECSENGINE_API void UIReflectionIntInputGroup<false>(UIDrawer<false>&, UIDrawConfig&, size_t, void*);
		template ECSENGINE_API void UIReflectionIntInputGroup<true>(UIDrawer<true>&, UIDrawConfig&, size_t, void*);

		UIReflectionDrawer::UIReflectionDrawer(
			UIToolsAllocator* _allocator, 
			ReflectionManager* _reflection,
			size_t type_table_count,
			size_t instance_table_count
		) : reflection(_reflection), allocator(_allocator) {
			ECS_ASSERT(function::is_power_of_two(type_table_count));
			ECS_ASSERT(function::is_power_of_two(instance_table_count));
			type_definition.Initialize(allocator, type_table_count);
			instances.Initialize(allocator, instance_table_count);
		}

		void UIReflectionDrawer::AddTypeConfiguration(const char* type_name, const char* field_name, size_t field_configuration)
		{
			unsigned int field_index = GetTypeFieldIndex(type_name, field_name);

			UIReflectionType* type_def = GetTypePtr(type_name);
			type_def->fields[field_index].configuration |= field_configuration;

#if 1
			// get a mask with the valid configurations
			size_t mask = 0;
			switch (type_def->fields[field_index].reflection_index) {
			case UIReflectionIndex::FloatSlider:
			case UIReflectionIndex::DoubleSlider:
			case UIReflectionIndex::IntegerSlider:
				mask = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK | UI_CONFIG_SLIDER_DEFAULT_VALUE;
				break;
			case UIReflectionIndex::Color:
				mask = UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE | UI_CONFIG_COLOR_INPUT_RGB_SLIDERS | UI_CONFIG_COLOR_INPUT_HSV_SLIDERS | UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER | UI_CONFIG_COLOR_INPUT_CALLBACK;
				break;
			case UIReflectionIndex::DoubleInput:
			case UIReflectionIndex::FloatInput:
			case UIReflectionIndex::IntegerInput:
				mask = UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_NUMBER_INPUT_NO_RANGE;
				break;
			case UIReflectionIndex::TextInput:
				mask = UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_CALLBACK;
				break;
			case UIReflectionIndex::FloatSliderGroup:
			case UIReflectionIndex::DoubleSliderGroup:
			case UIReflectionIndex::IntegerSliderGroup:
				mask = UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE;
				break;
			case UIReflectionIndex::FloatInputGroup:
			case UIReflectionIndex::DoubleInputGroup:
			case UIReflectionIndex::IntegerInputGroup:
				mask = UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_NUMBER_INPUT_NO_RANGE;
				break;
			}

			// invert the mask
			mask = ~mask;

			// check for 1's in wrong places
			size_t new_mask = field_configuration & mask;

			// if it is different than 0, then it's an error
			ECS_ASSERT(new_mask == 0);
#endif
		}

		void UIReflectionDrawer::BindInstanceData(const char* instance_name, const char* field_name, void* field_data) {
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			BindInstanceData(instance, field_name, field_data);
		}

		void UIReflectionDrawer::BindInstanceData(UIReflectionInstance* instance, const char* field_name, void* field_data)
		{
			UIReflectionType type = GetType(instance->type_name);
			unsigned int field_index = GetTypeFieldIndex(type, field_name);

#define POINTERS(type) type* data = (type*)field_data; type* instance_data = (type*)instance->datas[field_index];

#define COPY_VALUES_BASIC memcpy(instance_data->default_value, data->default_value, instance_data->byte_size); \
			memcpy(instance_data->upper_bound, data->upper_bound, instance_data->byte_size); \
			memcpy(instance_data->lower_bound, data->lower_bound, instance_data->byte_size); \

			switch (type.fields[field_index].reflection_index) {
			case UIReflectionIndex::CheckBox:
			{
				POINTERS(UIReflectionCheckBoxData);
				instance_data->value = data->value;
			}
			break;
			case UIReflectionIndex::Color:
			{
				POINTERS(UIReflectionColorData);
				instance_data->color = data->color;
				instance_data->default_color = data->default_color;
			}
			break;
			case UIReflectionIndex::ComboBox:
			{
				POINTERS(UIReflectionComboBoxData);
				instance_data->active_label = data->active_label;
				instance_data->default_label = data->default_label;
			}
			break;
			case UIReflectionIndex::DoubleInput:
			{
				POINTERS(UIReflectionDoubleInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
			}
			break;
			case UIReflectionIndex::FloatInput:
			{
				POINTERS(UIReflectionFloatInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
			}
			break;
			case UIReflectionIndex::IntegerInput:
			{
				POINTERS(UIReflectionIntInputData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
			}
			break;
			case UIReflectionIndex::DoubleSlider:
			{
				POINTERS(UIReflectionDoubleSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
			}
			break;
			case UIReflectionIndex::FloatSlider:
			{
				POINTERS(UIReflectionFloatSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
			}
			break;
			case UIReflectionIndex::IntegerSlider:
			{
				POINTERS(UIReflectionIntSliderData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
			}
			break;
			case UIReflectionIndex::TextInput:
			{
				POINTERS(UIReflectionTextInputData);
				instance_data->characters = data->characters;
			}
			case UIReflectionIndex::DoubleInputGroup:
			case UIReflectionIndex::FloatInputGroup:
			case UIReflectionIndex::IntegerInputGroup:
			case UIReflectionIndex::DoubleSliderGroup:
			case UIReflectionIndex::FloatSliderGroup:
			case UIReflectionIndex::IntegerSliderGroup:
			{
				POINTERS(UIReflectionGroupData<void>);

				uintptr_t lower_bound_ptr = (uintptr_t)instance_data->lower_bound;
				uintptr_t upper_bound_ptr = (uintptr_t)instance_data->upper_bound;
				uintptr_t default_ptr = (uintptr_t)instance_data->default_values;

				uintptr_t lower_bound_data_ptr = (uintptr_t)data->lower_bound;
				uintptr_t upper_bound_data_ptr = (uintptr_t)data->upper_bound;
				uintptr_t default_data_ptr = (uintptr_t)data->default_values;

				for (size_t subindex = 0; subindex < instance_data->count; subindex++) {
					instance_data->values[subindex] = data->values[subindex];
					memcpy((void*)lower_bound_ptr, (void*)lower_bound_data_ptr, instance_data->byte_size);
					memcpy((void*)upper_bound_ptr, (void*)upper_bound_data_ptr, instance_data->byte_size);
					memcpy((void*)default_ptr, (void*)default_data_ptr, instance_data->byte_size);

					lower_bound_ptr += instance_data->byte_size;
					upper_bound_ptr += instance_data->byte_size;
					default_ptr += instance_data->byte_size;
					lower_bound_data_ptr += instance_data->byte_size;
					upper_bound_data_ptr += instance_data->byte_size;
					default_data_ptr += instance_data->byte_size;
				}

				instance_data->precision = data->precision;
			}
			break;
			}
#undef POINTERS
		}

		void UIReflectionDrawer::BindTypeDefaultData(const char* type_name, Stream<UIReflectionBindDefaultValue> data) {
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypeDefaultData(type, data);
		}

		void UIReflectionDrawer::BindTypeDefaultData(UIReflectionType* type, Stream<UIReflectionBindDefaultValue> data) {
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, data[index].field_name);

				switch (type->fields[field_index].reflection_index) {
				case UIReflectionIndex::DoubleInput:
				case UIReflectionIndex::DoubleSlider:
				{
					UIReflectionDoubleInputData* current_data = (UIReflectionDoubleInputData*)type->fields[field_index].data;
					current_data->default_value = *(double*)data[index].value;
				}
				break;
				case UIReflectionIndex::FloatInput:
				case UIReflectionIndex::FloatSlider:
				{
					UIReflectionFloatInputData* current_data = (UIReflectionFloatInputData*)type->fields[field_index].data;
					current_data->default_value = *(float*)data[index].value;
				}
				break;
				case UIReflectionIndex::IntegerInput:
				case UIReflectionIndex::IntegerSlider:
				{
					UIReflectionIntInputData* current_data = (UIReflectionIntInputData*)type->fields[field_index].data;
					memcpy(current_data->default_value, data[index].value, current_data->byte_size);
				}
				break;
				case UIReflectionIndex::DoubleInputGroup:
				case UIReflectionIndex::DoubleSliderGroup:
				case UIReflectionIndex::FloatInputGroup:
				case UIReflectionIndex::FloatSliderGroup:
				case UIReflectionIndex::IntegerInputGroup:
				case UIReflectionIndex::IntegerSliderGroup:
				{
					UIReflectionGroupData<void>* current_data = (UIReflectionGroupData<void>*)type->fields[field_index].data;
					memcpy((void*)current_data->default_values, data[index].value, current_data->byte_size * current_data->count);
				}
				break;
				}
			}
		}

		void UIReflectionDrawer::BindInstancePtrs(const char* instance_name, void* data)
		{
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			BindInstancePtrs(instance, data);
		}

		void UIReflectionDrawer::BindInstancePtrs(UIReflectionInstance* instance, void* data)
		{
			ReflectionType reflect = reflection->GetType(instance->type_name);

			UIReflectionType type = GetType(instance->type_name);
			uintptr_t ptr = (uintptr_t)data;

			for (size_t index = 0; index < type.fields.size; index++) {
				ptr = (uintptr_t)data + type.fields[index].pointer_offset;

				UIReflectionIndex reflected_index = type.fields[index].reflection_index;

				bool is_group = reflected_index == UIReflectionIndex::IntegerInputGroup || reflected_index == UIReflectionIndex::IntegerSliderGroup
					|| reflected_index == UIReflectionIndex::FloatInputGroup || reflected_index == UIReflectionIndex::FloatSliderGroup
					|| reflected_index == UIReflectionIndex::DoubleInputGroup || reflected_index == UIReflectionIndex::DoubleSliderGroup;
				if (!is_group) {
					void** reinterpretation = (void**)instance->datas[index];
					*reinterpretation = (void*)ptr;

					switch (reflected_index) {
					case UIReflectionIndex::IntegerInput:
					case UIReflectionIndex::IntegerSlider:
					{
						UIReflectionIntInputData* field_data = (UIReflectionIntInputData*)instance->datas[index];
						memcpy(field_data->default_value, field_data->value_to_modify, field_data->byte_size);
					}
					break;
					case UIReflectionIndex::FloatInput:
					case UIReflectionIndex::FloatSlider:
					{
						UIReflectionFloatInputData* field_data = (UIReflectionFloatInputData*)instance->datas[index];
						field_data->default_value = *field_data->value;
					}
					break;
					case UIReflectionIndex::DoubleInput:
					case UIReflectionIndex::DoubleSlider:
					{
						UIReflectionDoubleInputData* field_data = (UIReflectionDoubleInputData*)instance->datas[index];
						field_data->default_value = *field_data->value;
					}
					break;
					case UIReflectionIndex::Color:
					{
						UIReflectionColorData* field_data = (UIReflectionColorData*)instance->datas[index];
						field_data->default_color = *field_data->color;
					}
					break;
					case UIReflectionIndex::ComboBox:
					{
						UIReflectionComboBoxData* field_data = (UIReflectionComboBoxData*)instance->datas[index];
						field_data->default_label = *field_data->active_label;
					}
					break;
					}
				}
				else {
					UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance->datas[index];
					void* allocation = allocator->Allocate(sizeof(void*) * data->count);
					data->values = (void**)allocation;

					unsigned int type_byte_size = reflect.fields[index].info.byte_size / reflect.fields[index].info.basic_type_count;
					for (size_t index = 0; index < data->count; index++) {
						data->values[index] = (void*)ptr;
						memcpy((void*)((uintptr_t)data->default_values + index * type_byte_size), (const void*)ptr, type_byte_size);
						ptr += type_byte_size;
					}
				}
			}
		}

		void UIReflectionDrawer::ChangeInputToSlider(const char* type_name, const char* field_name)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			ChangeInputToSlider(type, field_name);
		}

		void UIReflectionDrawer::ChangeInputToSlider(UIReflectionType* type, const char* field_name)
		{
			unsigned int field_index = GetTypeFieldIndex(*type, field_name);

			void* field_data = type->fields[field_index].data;
			switch (type->fields[field_index].reflection_index) {
			case UIReflectionIndex::DoubleInput:
			{
				UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)field_data;
				auto new_allocation = (UIReflectionDoubleSliderData*)allocator->Allocate(sizeof(UIReflectionDoubleSliderData));
				new_allocation->default_value = data->default_value;
				new_allocation->lower_bound = data->lower_bound;
				new_allocation->name = data->name;
				new_allocation->upper_bound = data->upper_bound;
				new_allocation->value_to_modify = data->value;
				allocator->Deallocate(data);
				type->fields[field_index].data = new_allocation;

				type->fields[field_index].reflection_index = UIReflectionIndex::DoubleSlider;
			}
			break;
			case UIReflectionIndex::FloatInput:
			{
				UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)field_data;
				auto new_allocation = (UIReflectionFloatSliderData*)allocator->Allocate(sizeof(UIReflectionFloatSliderData));
				new_allocation->default_value = data->default_value;
				new_allocation->lower_bound = data->lower_bound;
				new_allocation->name = data->name;
				new_allocation->upper_bound = data->upper_bound;
				new_allocation->value_to_modify = data->value;
				allocator->Deallocate(data);
				type->fields[field_index].data = new_allocation;

				type->fields[field_index].reflection_index = UIReflectionIndex::FloatSlider;
			}
			break;
			// integer input needs no correction, except reflection_index
			case UIReflectionIndex::IntegerInput:
				type->fields[field_index].reflection_index = UIReflectionIndex::IntegerSlider;
				break;
				// groups needs no correction, except reflection_index
			case UIReflectionIndex::DoubleInputGroup:
				type->fields[field_index].reflection_index = UIReflectionIndex::DoubleSliderGroup;
				break;
				// groups needs no correction, except reflection_index
			case UIReflectionIndex::FloatInputGroup:
				type->fields[field_index].reflection_index = UIReflectionIndex::FloatSliderGroup;
				break;
				// groups needs no correction, except reflection_index
			case UIReflectionIndex::IntegerInputGroup:
				type->fields[field_index].reflection_index = UIReflectionIndex::IntegerSliderGroup;
				break;
			}
		}

		void UIReflectionDrawer::BindTypeData(const char* type_name, const char* field_name, void* field_data)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypeData(type, field_name, field_data);
		}

		void UIReflectionDrawer::BindTypeData(UIReflectionType* type, const char* field_name, void* field_data) {
			unsigned int field_index = GetTypeFieldIndex(*type, field_name);

#define POINTERS(type__) type__* data = (type__*)field_data; type__* instance_data = (type__*)type->fields[field_index].data;

#define COPY_VALUES_BASIC memcpy(instance_data->default_value, data->default_value, instance_data->byte_size); \
			memcpy(instance_data->upper_bound, data->upper_bound, instance_data->byte_size); \
			memcpy(instance_data->lower_bound, data->lower_bound, instance_data->byte_size); \

			switch (type->fields[field_index].reflection_index) {
			case UIReflectionIndex::CheckBox:
			{
				POINTERS(UIReflectionCheckBoxData);
				instance_data->value = data->value;
			}
			break;
			case UIReflectionIndex::Color:
			{
				POINTERS(UIReflectionColorData);
				instance_data->color = data->color;
				instance_data->default_color = data->default_color;
			}
			break;
			case UIReflectionIndex::ComboBox:
			{
				POINTERS(UIReflectionComboBoxData);
				instance_data->active_label = data->active_label;
				instance_data->default_label = data->default_label;
			}
			break;
			case UIReflectionIndex::DoubleInput:
			{
				POINTERS(UIReflectionDoubleInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
			}
			break;
			case UIReflectionIndex::FloatInput:
			{
				POINTERS(UIReflectionFloatInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
			}
			break;
			case UIReflectionIndex::IntegerInput:
			{
				POINTERS(UIReflectionIntInputData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
			}
			break;
			case UIReflectionIndex::DoubleSlider:
			{
				POINTERS(UIReflectionDoubleSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
			}
			break;
			case UIReflectionIndex::FloatSlider:
			{
				POINTERS(UIReflectionFloatSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
			}
			break;
			case UIReflectionIndex::IntegerSlider:
			{
				POINTERS(UIReflectionIntSliderData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
			}
			break;
			case UIReflectionIndex::TextInput:
			{
				POINTERS(UIReflectionTextInputData);
				instance_data->characters = data->characters;
			}
			case UIReflectionIndex::DoubleInputGroup:
			case UIReflectionIndex::FloatInputGroup:
			case UIReflectionIndex::IntegerInputGroup:
			case UIReflectionIndex::DoubleSliderGroup:
			case UIReflectionIndex::FloatSliderGroup:
			case UIReflectionIndex::IntegerSliderGroup:
			{
				POINTERS(UIReflectionGroupData<void>);

				uintptr_t lower_bound_ptr = (uintptr_t)instance_data->lower_bound;
				uintptr_t upper_bound_ptr = (uintptr_t)instance_data->upper_bound;
				uintptr_t default_ptr = (uintptr_t)instance_data->default_values;

				uintptr_t lower_bound_data_ptr = (uintptr_t)data->lower_bound;
				uintptr_t upper_bound_data_ptr = (uintptr_t)data->upper_bound;
				uintptr_t default_data_ptr = (uintptr_t)data->default_values;

				for (size_t subindex = 0; subindex < instance_data->count; subindex++) {
					instance_data->values[subindex] = data->values[subindex];
					memcpy((void*)lower_bound_ptr, (void*)lower_bound_data_ptr, instance_data->byte_size);
					memcpy((void*)upper_bound_ptr, (void*)upper_bound_data_ptr, instance_data->byte_size);
					memcpy((void*)default_ptr, (void*)default_data_ptr, instance_data->byte_size);

					lower_bound_ptr += instance_data->byte_size;
					upper_bound_ptr += instance_data->byte_size;
					default_ptr += instance_data->byte_size;
					lower_bound_data_ptr += instance_data->byte_size;
					upper_bound_data_ptr += instance_data->byte_size;
					default_data_ptr += instance_data->byte_size;
				}

				instance_data->precision = data->precision;
			}
			break;
			}
#undef POINTERS
		}

		UIReflectionType* UIReflectionDrawer::CreateType(const char* name)
		{
			{
				ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
				ECS_ASSERT(type_definition.Find(hash, identifier) == -1);
			}

			ReflectionType reflected_type = reflection->GetType(name);

			auto integer = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				if (reflection_field.info.basic_type_count == 1) {
					UIReflectionIntInputData* data = (UIReflectionIntInputData*)this->allocator->Allocate(sizeof(UIReflectionIntInputData));
					field.data = data;
					field.reflection_index = UIReflectionIndex::IntegerInput;
					field.byte_size = sizeof(*data);
					data->name = reflection_field.name;
					data->byte_size = 1 << ((unsigned int)reflection_field.info.basic_type / 2);

					// defaults
					void* allocation = allocator->Allocate(data->byte_size * 3);
					uintptr_t ptr = (uintptr_t)allocation;
					data->default_value = (void*)ptr;
					ptr += data->byte_size;
					data->lower_bound = (void*)ptr;
					ptr += data->byte_size;
					data->upper_bound = (void*)ptr;
					memset(data->default_value, 0, data->byte_size);

#define CASE(enum_type, type, val_min, val_max) case ReflectionBasicFieldType::enum_type: { type* min = (type*)data->lower_bound; type* max = (type*)data->upper_bound; *min = val_min; *max = val_max; } break;

					switch (reflection_field.info.basic_type) {
						CASE(Int8, int8_t, CHAR_MIN, CHAR_MAX);
						CASE(UInt8, uint8_t, 0, UCHAR_MAX);
						CASE(Int16, int16_t, SHORT_MIN, SHORT_MAX);
						CASE(UInt16, uint16_t, 0, USHORT_MAX);
						CASE(Int32, int32_t, INT_MIN, INT_MAX);
						CASE(UInt32, uint32_t, 0, UINT_MAX);
						CASE(Int64, int64_t, LLONG_MIN, LLONG_MAX);
						CASE(UInt64, uint64_t, 0, LLONG_MAX);
					}

#undef CASE

					data->value_to_modify = nullptr;
				}
				else {
					UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)this->allocator->Allocate(sizeof(UIReflectionGroupData<void>));
					field.data = data;
					field.reflection_index = UIReflectionIndex::IntegerInputGroup;
					data->group_name = reflection_field.name;

					data->count = reflection_field.info.basic_type_count;
					data->input_names = BasicTypeNames;
					data->byte_size = 1 << ((unsigned int)reflection_field.info.basic_type / 2);

					// defaults
					void* allocation = allocator->Allocate(data->byte_size * 3 * data->count);
					uintptr_t ptr = (uintptr_t)allocation;

					data->values = nullptr;
					data->default_values = (const void*)ptr;
					ptr += data->byte_size * data->count;
					data->lower_bound = (const void*)ptr;
					ptr += data->byte_size * data->count;
					data->upper_bound = (const void*)ptr;
				}
				field.configuration = (size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type) | UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER;
			};

			auto float_ = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				if (reflection_field.info.basic_type_count == 1) {
					UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)this->allocator->Allocate(sizeof(UIReflectionFloatInputData));
					field.data = data;
					field.reflection_index = UIReflectionIndex::FloatInput;
					field.byte_size = sizeof(*data);
					data->name = reflection_field.name;
					
					// defaults
					data->default_value = 0.0f;
					data->lower_bound = 0.0f;
					data->upper_bound = 0.0f;
					data->value = nullptr;
				}
				else {
					UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)this->allocator->Allocate(sizeof(UIReflectionGroupData<float>));
					field.data = data;
					field.reflection_index = UIReflectionIndex::FloatInputGroup;
					data->group_name = reflection_field.name;

					data->count = reflection_field.info.basic_type_count;
					data->input_names = BasicTypeNames;
					data->byte_size = sizeof(float);

					// defaults
					data->precision = 3;
					void* allocation = allocator->Allocate(data->byte_size * 3 * data->count);
					uintptr_t ptr = (uintptr_t)allocation;

					data->values = nullptr;
					data->default_values = (const float*)ptr;
					ptr += data->byte_size * data->count;
					data->lower_bound = (const float*)ptr;
					ptr += data->byte_size * data->count;
					data->upper_bound = (const float*)ptr;
				}
				field.configuration = 0;
			};

			auto double_ = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				if (reflection_field.info.basic_type_count == 1) {
					UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)this->allocator->Allocate(sizeof(UIReflectionDoubleInputData));
					field.data = data;
					field.reflection_index = UIReflectionIndex::DoubleInput;
					field.byte_size = sizeof(*data);
					data->name = reflection_field.name;

					// defaults
					data->value = nullptr;
					data->default_value = 0;
					data->lower_bound = 0;
					data->upper_bound = 0;
				}
				else {
					UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)this->allocator->Allocate(sizeof(UIReflectionGroupData<double>));
					field.data = data;
					field.reflection_index = UIReflectionIndex::DoubleInputGroup;
					data->group_name = reflection_field.name;

					data->count = reflection_field.info.basic_type_count;
					data->input_names = BasicTypeNames;
					data->byte_size = sizeof(double);

					// defaults
					data->precision = 3;
					void* allocation = allocator->Allocate(data->byte_size * 3 * data->count);
					uintptr_t ptr = (uintptr_t)allocation;

					data->values = nullptr;
					data->default_values = (const double*)ptr;
					ptr += data->byte_size * data->count;
					data->lower_bound = (const double*)ptr;
					ptr += data->byte_size * data->count;
					data->upper_bound = (const double*)ptr;
				}
				field.configuration = 0;
			};

			auto enum_ = [this](ReflectionEnum reflection_enum, UIReflectionTypeField& field) {
				UIReflectionComboBoxData* data = (UIReflectionComboBoxData*)this->allocator->Allocate(sizeof(UIReflectionComboBoxData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::ComboBox;
				field.byte_size = sizeof(*data);
				data->labels = reflection_enum.fields;
				data->label_display_count = reflection_enum.fields.size;
				data->name = reflection_enum.name;
			};

			auto text_input = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionTextInputData* data = (UIReflectionTextInputData*)this->allocator->Allocate(sizeof(UIReflectionTextInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::TextInput;
				field.configuration = 0;
				field.byte_size = sizeof(*data);
				data->name = reflection_field.name;
				data->characters = nullptr;
			};

			auto bool_ = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionCheckBoxData* data = (UIReflectionCheckBoxData*)this->allocator->Allocate(sizeof(UIReflectionCheckBoxData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::CheckBox;
				field.configuration = 0;
				field.byte_size = sizeof(*data);
				data->name = reflection_field.name;
				data->value = nullptr;
			};

			auto color_ = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionColorData* data = (UIReflectionColorData*)this->allocator->Allocate(sizeof(UIReflectionColorData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::Color;
				field.configuration = 0;
				field.byte_size = sizeof(*data);
				data->color = nullptr;
				data->default_color = Color(0, 0, 0);
				data->name = reflection_field.name;
			};

			UIReflectionType type;
			type.name = reflected_type.name;
			type.fields.Initialize(allocator, 0, reflected_type.fields.size);

			for (size_t index = 0; index < reflected_type.fields.size; index++) {
				ReflectionFieldInfo field_info = reflected_type.fields[index].info;

				bool value_written = false;
				type.fields[index].name = reflected_type.fields[index].name;
				if (IsIntegral(field_info.extended_type) || IsIntegralBasicType(field_info.extended_type)) {
					integer(reflected_type.fields[index], type.fields[type.fields.size]);
					type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
					value_written = true;
				}
				else if (IsFloatBasicType(field_info.extended_type) || field_info.extended_type == ReflectionExtendedFieldType::Float) {
					float_(reflected_type.fields[index], type.fields[type.fields.size]);
					type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
					value_written = true;
				}
				else if (IsDoubleBasicType(field_info.extended_type) || field_info.extended_type == ReflectionExtendedFieldType::Double) {
					double_(reflected_type.fields[index], type.fields[type.fields.size]);
					type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
					value_written = true;
				}
				else if (field_info.extended_type == ReflectionExtendedFieldType::Pointer) {
					// text input
					if (field_info.basic_type == ReflectionBasicFieldType::Int8 || field_info.basic_type == ReflectionBasicFieldType::UInt8) {
						text_input(reflected_type.fields[index], type.fields[type.fields.size]);
						type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
						value_written = true;
					}
				}
				else if (field_info.extended_type == ReflectionExtendedFieldType::Bool) {
					bool_(reflected_type.fields[index], type.fields[type.fields.size]);
					type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
					value_written = true;
				}
				else if (strcmp(reflected_type.fields[index].definition, "Color") == 0) {
					color_(reflected_type.fields[index], type.fields[type.fields.size]);
					type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
					value_written = true;
				}
				else if (IsEnum(field_info.extended_type)) {
					ReflectionEnum reflected_enum;
					bool success = reflection->TryGetEnum(reflected_type.fields[index].definition, reflected_enum);
					if (success) {
						enum_(reflected_enum, type.fields[type.fields.size]);
						type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
						value_written = true;
					}
				}

				type.fields.size += value_written;
			}

			if (type.fields.size < type.fields.capacity) {
				void* new_allocation = allocator->Allocate(sizeof(UIReflectionTypeField) * type.fields.size);
				memcpy(new_allocation, type.fields.buffer, sizeof(UIReflectionTypeField)* type.fields.size);
				allocator->Deallocate(type.fields.buffer);
				type.fields.buffer = (UIReflectionTypeField*)new_allocation;
			}

			ECS_RESOURCE_IDENTIFIER_WITH_HASH(type.name, UIReflectionStringHash);
			ECS_ASSERT(!type_definition.Insert(hash, type, identifier));
			return type_definition.GetValuePtr(hash, identifier);
		}

		UIReflectionInstance* UIReflectionDrawer::CreateInstance(const char* name, const char* type_name)
		{
			{
				ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
				ECS_ASSERT(instances.Find(hash, identifier) == -1);
			}

			UIReflectionType type = GetType(type_name);
			UIReflectionInstance instance;
			instance.type_name = type.name;
			
			size_t name_length = strlen(name) + 1;
			size_t total_memory = name_length + sizeof(void*) * type.fields.size;
			
			for (size_t index = 0; index < type.fields.size; index++) {
				total_memory += type.fields[index].byte_size;
			}

			void* allocation = allocator->Allocate(total_memory);
			uintptr_t buffer = (uintptr_t)allocation;
			buffer += sizeof(void*) * type.fields.size;
			instance.datas.buffer = (void**)allocation;

			for (size_t index = 0; index < type.fields.size; index++) {
				instance.datas[index] = (void*)buffer;
				buffer += type.fields[index].byte_size;
				memcpy(instance.datas[index], type.fields[index].data, type.fields[index].byte_size);

				switch (type.fields[index].reflection_index) {
				case UIReflectionIndex::DoubleInputGroup:
				case UIReflectionIndex::DoubleSliderGroup:
				case UIReflectionIndex::FloatInputGroup:
				case UIReflectionIndex::FloatSliderGroup:
				case UIReflectionIndex::IntegerInputGroup:
				case UIReflectionIndex::IntegerSliderGroup:
				{
					UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance.datas[index];
					data->values = (void**)allocator->Allocate(sizeof(void*) * data->count);
				}
				break;
				}
			}

			char* instance_name = (char*)buffer;
			memcpy(instance_name, name, name_length * sizeof(char));
			instance.name = instance_name;
			
			instance.datas.size = type.fields.size;

			ECS_RESOURCE_IDENTIFIER_WITH_HASH(instance.name, UIReflectionStringHash);
			ECS_ASSERT(!instances.Insert(hash, instance, identifier));
			return instances.GetValuePtr(hash, identifier);
		}

		void UIReflectionDrawer::DestroyInstance(const char* name)
		{
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
			UIReflectionInstance instance = GetInstance(name);

			UIReflectionType type = GetType(instance.type_name);
			for (size_t index = 0; index < type.fields.size; index++) {
				switch (type.fields[index].reflection_index) {
				case UIReflectionIndex::IntegerInputGroup:
				case UIReflectionIndex::IntegerSliderGroup:
				case UIReflectionIndex::DoubleInputGroup:
				case UIReflectionIndex::DoubleSliderGroup:
				case UIReflectionIndex::FloatInputGroup:
				case UIReflectionIndex::FloatSliderGroup:
				{
					UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance.datas[index];
					allocator->Deallocate(data->values);
				}
				break;
				}
			}

			allocator->Deallocate(instance.datas.buffer);
			instances.Erase(hash, identifier);
		}


		void UIReflectionDrawer::DestroyType(const char* name)
		{
			UIReflectionType type = GetType(name);

			Stream<UIReflectionInstance> instances_ = instances.GetValueStream();
			for (int64_t index = 0; index < (int64_t)instances_.size; index++) {
				if (instances.IsItemAt(index)) {
					if (strcmp(type.name, instances_[index].type_name) == 0) {
						DestroyInstance(instances_[index].name);
						index--;
					}
				}
			}

			for (size_t index = 0; index < type.fields.size; index++) {
				UIReflectionIndex reflect_index = type.fields[index].reflection_index;
				switch (reflect_index) {
				case UIReflectionIndex::IntegerInput:
				case UIReflectionIndex::IntegerSlider: 
				{
					UIReflectionIntInputData* data = (UIReflectionIntInputData*)type.fields[index].data;
					allocator->Deallocate(data->default_value);
				}
					break;
				case UIReflectionIndex::FloatInputGroup:
				case UIReflectionIndex::FloatSliderGroup:
				case UIReflectionIndex::DoubleInputGroup:
				case UIReflectionIndex::DoubleSliderGroup:
				case UIReflectionIndex::IntegerInputGroup:
				case UIReflectionIndex::IntegerSliderGroup:
				{
					UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)type.fields[index].data;
					allocator->Deallocate(data->default_values);
				}
					break;
				}
				allocator->Deallocate(type.fields[index].data);
			}
			allocator->Deallocate(type.fields.buffer);

			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
			type_definition.Erase(hash, identifier);
		}

		template<bool initialize>
		void UIReflectionDrawer::DrawInstance(
			const char* ECS_RESTRICT instance_name,
			UIDrawer<initialize>& drawer,
			UIDrawConfig& config,
			const char* ECS_RESTRICT default_value_button
		) {
			UIReflectionInstance instance = GetInstance(instance_name);
			
			UIReflectionType type = GetType(instance.type_name);

			UIDrawerMode draw_mode = drawer.draw_mode;
			unsigned int draw_mode_target = drawer.draw_mode_target;

			drawer.SetDrawMode(UIDrawerMode::NextRow);

			if (default_value_button != nullptr) {
				UIReflectionDefaultValueData default_data;
				default_data.instance = instance;
				default_data.type = type;
				drawer.Button(default_value_button, { UIReflectionDefaultValueAction, &default_data, sizeof(default_data) });
			}

			if constexpr (initialize) {
				for (size_t index = 0; index < type.fields.size; index++) {
					ui_reflection_field_initialize[(unsigned int)type.fields[index].reflection_index](drawer, config, type.fields[index].configuration, instance.datas[index]);
				}
			}
			else {
				for (size_t index = 0; index < type.fields.size; index++) {
					ui_reflection_field_draw[(unsigned int)type.fields[index].reflection_index](drawer, config, type.fields[index].configuration, instance.datas[index]);
				}
			}

			drawer.SetDrawMode(draw_mode, draw_mode_target);
		}

		template void ECSENGINE_API UIReflectionDrawer::DrawInstance<false>(const char* ECS_RESTRICT, UIDrawer<false>&, UIDrawConfig&, const char* ECS_RESTRICT);
		template void ECSENGINE_API UIReflectionDrawer::DrawInstance<true>(const char* ECS_RESTRICT, UIDrawer<true>&, UIDrawConfig&, const char* ECS_RESTRICT);

		UIReflectionType UIReflectionDrawer::GetType(const char* name) const
		{
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
			return type_definition.GetValue(hash, identifier);
		}

		UIReflectionType* UIReflectionDrawer::GetTypePtr(const char* name) const
		{
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
			return type_definition.GetValuePtr(hash, identifier);
		}
		
		UIReflectionInstance UIReflectionDrawer::GetInstance(const char* name) const {
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
			return instances.GetValue(hash, identifier);
		}

		UIReflectionInstance* UIReflectionDrawer::GetInstancePtr(const char* name) const
		{
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, UIReflectionStringHash);
			return instances.GetValuePtr(hash, identifier);
		}

		unsigned int UIReflectionDrawer::GetTypeFieldIndex(UIReflectionType type, const char* field_name) const
		{
			unsigned int field_index = 0;
			while ((field_index < type.fields.size) && strcmp(type.fields[field_index].name, field_name) != 0) {
				field_index++;
			}
			ECS_ASSERT(field_index < type.fields.size);
			return field_index;
		}

		unsigned int UIReflectionDrawer::GetTypeFieldIndex(const char* ECS_RESTRICT type_name, const char* ECS_RESTRICT field_name)
		{
			UIReflectionType type = GetType(type_name);
			return GetTypeFieldIndex(type, field_name);
		}

		void UIReflectionDrawer::OmitTypeField(const char* type_name, const char* field_name)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			OmitTypeField(type, field_name);
		}

		void UIReflectionDrawer::OmitTypeField(UIReflectionType* type, const char* field_name)
		{
			unsigned int field_index = GetTypeFieldIndex(*type, field_name);
			allocator->Deallocate(type->fields[field_index].data);
			type->fields.Remove(field_index);
		}

		void UIReflectionDrawer::OmitTypeFields(const char* type_name, Stream<const char*> fields)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			OmitTypeFields(type, fields);
		}

		void UIReflectionDrawer::OmitTypeFields(UIReflectionType* type, Stream<const char*> fields)
		{
			for (size_t index = 0; index < fields.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, fields[index]);
				allocator->Deallocate(type->fields[field_index].data);
				type->fields.Remove(field_index);
			}
		}

		void UIReflectionDefaultValueAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIReflectionDefaultValueData* default_data = (UIReflectionDefaultValueData*)_data;

#define CASE_START(type, data_type) case UIReflectionIndex::type: { data_type* data = (data_type*)default_data->instance.datas[index]; 
#define CASE_END } break;

			for (size_t index = 0; index < default_data->type.fields.size; index++) {
				switch (default_data->type.fields[index].reflection_index) {
					CASE_START(Color, UIReflectionColorData);
					*data->color = data->default_color;
					CASE_END

					CASE_START(FloatInput, UIReflectionFloatInputData);
					*data->value = data->default_value;
					CASE_END

					CASE_START(DoubleInput, UIReflectionDoubleInputData);
					*data->value = data->default_value;
					CASE_END

					CASE_START(IntegerInput, UIReflectionIntInputData);
					memcpy(data->value_to_modify, data->default_value, data->byte_size);
					CASE_END

					CASE_START(FloatSlider, UIReflectionFloatSliderData);
					*data->value_to_modify = data->default_value;
					CASE_END

					CASE_START(DoubleSlider, UIReflectionDoubleSliderData);
					*data->value_to_modify = data->default_value;
					CASE_END

					CASE_START(IntegerSlider, UIReflectionIntSliderData);
					memcpy(data->value_to_modify, data->default_value, data->byte_size);
					CASE_END

					CASE_START(FloatInputGroup, UIReflectionGroupData<float>);
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					CASE_START(DoubleInputGroup, UIReflectionGroupData<double>);
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					// doesn't matter what type we choose here, because it will simply get memcpy'ed
					CASE_START(IntegerInputGroup, UIReflectionGroupData<int8_t>);
					uintptr_t value_ptr = (uintptr_t)data->default_values;

					for (size_t subindex = 0; subindex < data->count; subindex++) {
						memcpy(data->values[subindex], (void*)value_ptr, data->byte_size);
						value_ptr += data->byte_size;
					}
					CASE_END

					CASE_START(FloatSliderGroup, UIReflectionGroupData<float>);
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					CASE_START(DoubleSliderGroup, UIReflectionGroupData<double>);
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					// doesn't matter what type we choose here, because it will simply get memcpy'ed
					CASE_START(IntegerSliderGroup, UIReflectionGroupData<int8_t>);
					uintptr_t value_ptr = (uintptr_t)data->default_values;

					for (size_t subindex = 0; subindex < data->count; subindex++) {
						memcpy(data->values[subindex], (void*)value_ptr, data->byte_size);
						value_ptr += data->byte_size;
					}
					CASE_END
				}
			}

#undef CASE_START
#undef CASE_END
		}

	}

}