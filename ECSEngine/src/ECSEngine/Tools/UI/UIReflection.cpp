#include "ecspch.h"
#include "UIReflection.h"
#include "../../Utilities/Mouse.h"
#include "../../Utilities/Keyboard.h"

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

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_0(function, a, ...) function(0, __VA_ARGS__)

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_1(function, a, ...) function(0, __VA_ARGS__) \
		function(a, __VA_ARGS__) 

#define ECS_TOOLS_UI_CREATE_PREDICATION_TABLE_2_EX(function, a, b, starting, ...) function(starting, __VA_ARGS__) \
		function(a, __VA_ARGS__) \
		function(b, __VA_ARGS__) \
		function(a | b, __VA_ARGS__)

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
#define ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(integer_type, type, function) case integer_type: function(type); break;
#define ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(integer_flags, function) switch(integer_flags) { \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT8_T, int8_t, function); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT8_T, uint8_t, function); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT16_T, int16_t, function); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT16_T, uint16_t, function); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT32_T, int32_t, function); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT32_T, uint32_t, function); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_INT64_T, int64_t, function); \
			ECS_TOOLS_UI_REFLECT_INT_SWITCH_CASE_IMPLEMENTATION(UI_UINT64_T, uint64_t, function); \
		}

		size_t ExtractIntFlags(size_t configuration_flags) {
			return (configuration_flags & 0xFF00000000000000) /*>> 55*/;
		}

		size_t RemoveIntFlags(size_t configuration_flags) {
			return configuration_flags & 0x00FFFFFFFFFFFFFF;
		}

		// ------------------------------------------------------------------------------------------------------------------------------
		
		void UIReflectionDrawConfigCopyToNormalConfig(const UIReflectionDrawConfig* ui_config, UIDrawConfig& config) {
			memcpy(config.associated_bits + config.flag_count, ui_config->associated_bits, ui_config->config_count * sizeof(size_t));
			for (size_t index = 0; index < ui_config->config_count; index++) {
				memcpy(config.parameters + config.parameter_start[config.flag_count], ui_config->configs[index], ui_config->config_size[index]);

				size_t parameter_count = ui_config->config_size[index] / sizeof(float) + (ui_config->config_size[index] % sizeof(float) != 0);
				config.parameter_start[config.flag_count + 1] = config.parameter_start[config.flag_count] + parameter_count;
				config.flag_count++;
			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------

		// ------------------------------------------------------------- Basic ----------------------------------------------------------

		void UIReflectionFloatSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionFloatSliderData* data = (UIReflectionFloatSliderData*)_data;

			drawer.FloatSlider(
				configuration | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE, 
				config,
				data->name,
				data->value_to_modify,
				data->lower_bound,
				data->upper_bound,
				data->default_value, 
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionDoubleSliderData* data = (UIReflectionDoubleSliderData*)_data;

			drawer.DoubleSlider(
				configuration | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE,
				config, 
				data->name,
				data->value_to_modify,
				data->lower_bound,
				data->upper_bound, 
				data->default_value,
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionIntSliderData* data = (UIReflectionIntSliderData*)_data;

			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

#define SLIDER_IMPLEMENTATION(integer_type) drawer.IntSlider<integer_type>(configuration | UI_CONFIG_SLIDER_ENTER_VALUES \
			| UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE, \
			config, data->name, (integer_type*)data->value_to_modify, *(integer_type*)data->lower_bound, *(integer_type*)data->upper_bound, *(integer_type*)data->default_value); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, SLIDER_IMPLEMENTATION);

#undef SLIDER_IMPLEMENTATION
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionFloatInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)_data;
			drawer.FloatInput(
				configuration | UI_CONFIG_NUMBER_INPUT_NO_RANGE | UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_DO_NOT_CACHE,
				config,
				data->name, 
				data->value, 
				data->default_value,
				data->lower_bound, 
				data->upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)_data;
			drawer.DoubleInput(
				configuration | UI_CONFIG_NUMBER_INPUT_NO_RANGE | UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_DO_NOT_CACHE, 
				config, 
				data->name, 
				data->value, 
				data->default_value, 
				data->lower_bound,
				data->upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionIntInputData* data = (UIReflectionIntInputData*)_data;
			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

#define INPUT(integer_convert) { integer_convert default_value = *(integer_convert*)data->default_value; \
			integer_convert upper_bound = *(integer_convert*)data->upper_bound; \
		integer_convert lower_bound = *(integer_convert*)data->lower_bound; \
		drawer.IntInput<integer_convert>( \
			configuration | UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER | UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_DO_NOT_CACHE, \
			config,  \
			data->name, \
			(integer_convert*)data->value_to_modify, \
			default_value, \
			lower_bound,  \
			upper_bound);  \
		} break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, INPUT);

#undef INPUT
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionTextInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionTextInputData* data = (UIReflectionTextInputData*)_data;

			drawer.TextInput(configuration | UI_CONFIG_DO_NOT_CACHE, config, data->name, data->characters);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionColor(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionColorData* data = (UIReflectionColorData*)_data;
			drawer.ColorInput(configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE, config, data->name, data->color, data->default_color);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionColorFloat(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionColorFloatData* data = (UIReflectionColorFloatData*)_data;
			drawer.ColorFloatInput(configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE, config, data->name, data->color, data->default_color);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionCheckBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionCheckBoxData* data = (UIReflectionCheckBoxData*)_data;
			drawer.CheckBox(configuration | UI_CONFIG_DO_NOT_CACHE, config, data->name, data->value);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionComboBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionComboBoxData* data = (UIReflectionComboBoxData*)_data;
			drawer.ComboBox(configuration | UI_CONFIG_DO_NOT_CACHE, config, data->name, data->labels, data->label_display_count, data->active_label);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionFloatSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)_data;

			drawer.FloatSliderGroup(
				configuration | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE,
				config,
				data->count, 
				data->group_name, 
				data->input_names, 
				data->values, 
				data->lower_bound, 
				data->upper_bound, 
				data->default_values, 
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)_data;

			drawer.DoubleSliderGroup(
				configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE,
				config,
				data->count, 
				data->group_name,
				data->input_names,
				data->values, 
				data->lower_bound, 
				data->upper_bound, 
				data->default_values,
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)_data;
			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

#define GROUP(integer_type) drawer.IntSliderGroup<integer_type>(configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE \
			| UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE, config, data->count, data->group_name, data->input_names, (integer_type**)data->values, (const integer_type*)data->lower_bound, (const integer_type*)data->upper_bound, (const integer_type*)data->default_values); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, GROUP);

#undef GROUP
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionFloatInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)_data;

			drawer.FloatInputGroup(
				configuration | UI_CONFIG_NUMBER_INPUT_NO_RANGE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_DEFAULT,
				config,
				data->count,
				data->group_name,
				data->input_names,
				data->values,
				data->default_values,
				data->lower_bound,
				data->upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)_data;

			drawer.DoubleInputGroup(
				configuration | UI_CONFIG_NUMBER_INPUT_NO_RANGE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_DEFAULT,
				config,
				data->count,
				data->group_name,
				data->input_names,
				data->values,
				data->default_values,
				data->lower_bound,
				data->upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)_data;
			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

#define GROUP(integer_type) drawer.IntInputGroup<integer_type>( \
			configuration | UI_CONFIG_NUMBER_INPUT_NO_RANGE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_DEFAULT, \
			config, data->count, data->group_name, data->input_names, (integer_type**)data->values, (const integer_type*)data->default_values, (const integer_type*)data->lower_bound, (const integer_type*)data->upper_bound); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, GROUP);

#undef GROUP
		}

		// -------------------------------------------------------------  Basic ----------------------------------------------------------

		// ------------------------------------------------------------- Stream ----------------------------------------------------------

		void UIReflectionStreamFloatInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionStreamFloatInputData* data = (UIReflectionStreamFloatInputData*)_data;
			UIDrawConfig temp_config;
			drawer.ArrayFloat(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamDoubleInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamDoubleInputData* data = (UIReflectionStreamDoubleInputData*)_data;
			UIDrawConfig temp_config;
			drawer.ArrayDouble(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamIntInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamIntInputData* data = (UIReflectionStreamIntInputData*)_data;
			UIDrawConfig temp_config;

			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

#define FUNCTION(integer_type) drawer.ArrayInteger<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, (CapacityStream<integer_type>*)data->values,\
			configuration, &config); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, FUNCTION);

#undef FUNCTION
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamTextInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamTextInputData* data = (UIReflectionStreamTextInputData*)_data;
			UIDrawConfig temp_config;
			drawer.ArrayTextInput(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamColor(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamColorData* data = (UIReflectionStreamColorData*)_data;
			UIDrawConfig temp_config;
			drawer.ArrayColor(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamColorFloat(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionStreamColorFloatData* data = (UIReflectionStreamColorFloatData*)_data;
			UIDrawConfig temp_config;
			drawer.ArrayColorFloat(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamCheckBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamCheckBoxData* data = (UIReflectionStreamCheckBoxData*)_data;
			UIDrawConfig temp_config;
			drawer.ArrayCheckBox(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamComboBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamComboBoxData* data = (UIReflectionStreamComboBoxData*)_data;
			UIDrawConfig temp_config;
			CapacityStream<Stream<const char*>> label_names(data->label_names, data->values->size, data->values->capacity);
			drawer.ArrayComboBox(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, label_names, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------


#define FLOAT_DOUBLE_CASE(function, cast_type) drawer.function(UI_CONFIG_DO_NOT_CACHE, temp_config, data->group_name, (CapacityStream<cast_type>*)data->values, configuration, &config);

		void UIReflectionStreamFloatInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)_data;
			UIDrawConfig temp_config;
			ECS_ASSERT(2 <= data->basic_type_count && data->basic_type_count <= 4);

			switch (data->basic_type_count) {
			case 2:
			{
				FLOAT_DOUBLE_CASE(ArrayFloat2, float2);
				break;
			}
			case 3:
			{
				FLOAT_DOUBLE_CASE(ArrayFloat3, float3);
				break;
			}
			case 4:
			{
				FLOAT_DOUBLE_CASE(ArrayFloat4, float4);
				break;
			}
			}
		}

		// ------------------------------------------------------------- Stream ----------------------------------------------------------

		void UIReflectionStreamDoubleInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)_data;
			UIDrawConfig temp_config;
			ECS_ASSERT(2 <= data->basic_type_count && data->basic_type_count <= 4);

			switch (data->basic_type_count) {
			case 2:
			{
				FLOAT_DOUBLE_CASE(ArrayDouble2, double2);
				break;
			}
			case 3:
			{
				FLOAT_DOUBLE_CASE(ArrayDouble3, double3);
				break;
			}
			case 4:
			{
				FLOAT_DOUBLE_CASE(ArrayDouble4, double4);
				break;
			}
			}
		}

#undef FLOAT_DOUBLE_CASE

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamIntInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)_data;
			UIDrawConfig temp_config;
			ECS_ASSERT(2 <= data->basic_type_count && data->basic_type_count <= 4);

			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

#define GROUP(integer_type) { \
				if (data->basic_type_count == 2) { \
					drawer.ArrayInteger2Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->group_name, data->values, configuration, &config); \
				} \
				else if (data->basic_type_count == 3) { \
					drawer.ArrayInteger3Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->group_name, data->values, configuration, &config); \
				} \
				else { \
					drawer.ArrayInteger4Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->group_name, data->values, configuration, &config); \
				} \
			}

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, GROUP);

#undef GROUP
		}

		// ------------------------------------------------------------- Stream ----------------------------------------------------------

#pragma region Lower bound

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetFloatLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionFloatSliderData*)field->data;
			field_ptr->lower_bound = *(float*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionDoubleSliderData*)field->data;
			field_ptr->lower_bound = *(double*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionIntSliderData*)field->data;
			memcpy(field_ptr->lower_bound, data, field_ptr->byte_size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetGroupLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionGroupData<void>*)field->data;
			memcpy((void*)field_ptr->lower_bound, data, field_ptr->byte_size * field_ptr->count);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Upper bound

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetFloatUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionFloatSliderData*)field->data;
			field_ptr->upper_bound = *(float*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionDoubleSliderData*)field->data;
			field_ptr->upper_bound = *(double*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionIntSliderData*)field->data;
			memcpy(field_ptr->upper_bound, data, field_ptr->byte_size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetGroupUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionGroupData<void>*)field->data;
			memcpy((void*)field_ptr->upper_bound, data, field_ptr->byte_size * field_ptr->count);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Default Data

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetFloatDefaultData(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionFloatSliderData*)field->data;
			field_ptr->default_value = *(float*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleDefaultData(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionDoubleSliderData*)field->data;
			field_ptr->default_value = *(double*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntDefaultData(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionIntSliderData*)field->data;
			memcpy(field_ptr->default_value, data, field_ptr->byte_size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetGroupDefaultData(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionGroupData<void>*)field->data;
			memcpy((void*)field_ptr->default_values, data, field_ptr->byte_size * field_ptr->count);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetColorDefaultData(UIReflectionTypeField* field, const void* data) {
			auto field_ptr = (UIReflectionColorData*)field->data;
			memcpy(&field_ptr->default_color, data, sizeof(Color));
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		// Currently it does nothing. It serves as a placeholder for the jump table. Maybe add support for it later on.
		void UIReflectionSetTextInputDefaultData(UIReflectionTypeField* field, const void* data) {
			
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetColorFloatDefaultData(UIReflectionTypeField* field, const void* data) {
			auto field_ptr = (UIReflectionColorFloatData*)field->data;
			memcpy(&field_ptr->default_color, data, sizeof(ColorFloat));
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetCheckBoxDefaultData(UIReflectionTypeField* field, const void* data) {
			auto field_ptr = (UIReflectionCheckBoxData*)field->data;
			field_ptr->default_value = *(bool*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetComboBoxDefaultData(UIReflectionTypeField* field, const void* data) {
			auto field_ptr = (UIReflectionComboBoxData*)field->data;
			field_ptr->default_label = *(unsigned char*)data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Range

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetFloatRange(UIReflectionTypeField* field, const void* min, const void* max)
		{
			UIReflectionSetFloatLowerBound(field, min);
			UIReflectionSetFloatUpperBound(field, max);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleRange(UIReflectionTypeField* field, const void* min, const void* max)
		{
			UIReflectionSetDoubleLowerBound(field, min);
			UIReflectionSetDoubleUpperBound(field, max);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntRange(UIReflectionTypeField* field, const void* min, const void* max)
		{
			UIReflectionSetIntLowerBound(field, min);
			UIReflectionSetIntUpperBound(field, max);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetGroupRange(UIReflectionTypeField* field, const void* min, const void* max)
		{
			UIReflectionSetGroupLowerBound(field, min);
			UIReflectionSetGroupUpperBound(field, max);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

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

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::AddTypeConfiguration(const char* type_name, const char* field_name, size_t field_configuration)
		{
			UIReflectionType* type_def = GetTypePtr(type_name);
			AddTypeConfiguration(type_def, field_name, field_configuration);
		}

		void UIReflectionDrawer::AddTypeConfiguration(UIReflectionType* type, const char* field_name, size_t field_configuration)
		{

			unsigned int field_index = GetTypeFieldIndex(*type, field_name);
			type->fields[field_index].configuration |= field_configuration;

#if 1
			// get a mask with the valid configurations
			size_t mask = 0;
			switch (type->fields[field_index].reflection_index) {
			case UIReflectionIndex::FloatSlider:
			case UIReflectionIndex::DoubleSlider:
			case UIReflectionIndex::IntegerSlider:
				mask = UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
				break;
			case UIReflectionIndex::Color:
				mask = UI_CONFIG_COLOR_INPUT_RGB_SLIDERS | UI_CONFIG_COLOR_INPUT_HSV_SLIDERS | UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER | UI_CONFIG_COLOR_INPUT_CALLBACK;
				break;
			case UIReflectionIndex::DoubleInput:
			case UIReflectionIndex::FloatInput:
			case UIReflectionIndex::IntegerInput:
				mask = 0;
				break;
			case UIReflectionIndex::TextInput:
				mask = UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_CALLBACK;
				break;
			case UIReflectionIndex::FloatSliderGroup:
			case UIReflectionIndex::DoubleSliderGroup:
			case UIReflectionIndex::IntegerSliderGroup:
				mask = UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
				break;
			case UIReflectionIndex::FloatInputGroup:
			case UIReflectionIndex::DoubleInputGroup:
			case UIReflectionIndex::IntegerInputGroup:
				mask = 0;
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

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceData(const char* instance_name, const char* field_name, void* field_data) {
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			BindInstanceData(instance, field_name, field_data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceData(UIReflectionInstance* instance, const char* field_name, void* field_data)
		{
			UIReflectionType type = GetType(instance->type_name);
			unsigned int field_index = GetTypeFieldIndex(type, field_name);

#define POINTERS(type) type* data = (type*)field_data; type* instance_data = (type*)instance->datas[field_index].struct_data;

#define COPY_VALUES_BASIC memcpy(instance_data->default_value, data->default_value, instance_data->byte_size); \
			memcpy(instance_data->upper_bound, data->upper_bound, instance_data->byte_size); \
			memcpy(instance_data->lower_bound, data->lower_bound, instance_data->byte_size); \

			switch (type.fields[field_index].reflection_index) {
			case UIReflectionIndex::CheckBox:
			{
				POINTERS(UIReflectionCheckBoxData);
				instance_data->value = data->value;
				break;
			}
			case UIReflectionIndex::Color:
			{
				POINTERS(UIReflectionColorData);
				instance_data->color = data->color;
				instance_data->default_color = data->default_color;
				break;
			}
			case UIReflectionIndex::ComboBox:
			{
				POINTERS(UIReflectionComboBoxData);
				instance_data->active_label = data->active_label;
				instance_data->default_label = data->default_label;
				break;
			}
			case UIReflectionIndex::DoubleInput:
			{
				POINTERS(UIReflectionDoubleInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
				break;
			}
			case UIReflectionIndex::FloatInput:
			{
				POINTERS(UIReflectionFloatInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
				break;
			}
			case UIReflectionIndex::IntegerInput:
			{
				POINTERS(UIReflectionIntInputData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
				break;
			}
			case UIReflectionIndex::DoubleSlider:
			{
				POINTERS(UIReflectionDoubleSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
				break;
			}
			case UIReflectionIndex::FloatSlider:
			{
				POINTERS(UIReflectionFloatSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
				break;
			}
			case UIReflectionIndex::IntegerSlider:
			{
				POINTERS(UIReflectionIntSliderData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
				break;
			}
			case UIReflectionIndex::TextInput:
			{
				POINTERS(UIReflectionTextInputData);
				instance_data->characters = data->characters;
				break;
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

				memcpy((void*)lower_bound_ptr, (void*)lower_bound_data_ptr, instance_data->byte_size * instance_data->count);
				memcpy((void*)upper_bound_ptr, (void*)upper_bound_data_ptr, instance_data->byte_size * instance_data->count);
				memcpy((void*)default_ptr, (void*)default_data_ptr, instance_data->byte_size * instance_data->count);

				for (size_t subindex = 0; subindex < instance_data->count; subindex++) {
					instance_data->values[subindex] = data->values[subindex];
				}

				instance_data->precision = data->precision;
				break;
			}
			}
#undef POINTERS
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeDefaultData(const char* type_name, Stream<UIReflectionBindDefaultValue> data) {
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypeDefaultData(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeDefaultData(UIReflectionType* type, Stream<UIReflectionBindDefaultValue> data) {
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, data[index].field_name);
				UI_REFLECTION_SET_DEFAULT_DATA[(unsigned int)type->fields[field_index].reflection_index](type->fields.buffer + field_index, data[index].value);
			}
		}

		void UIReflectionDrawer::BindTypeDefaultData(UIReflectionType* type, const void* data) {
			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].stream_type == UIReflectionStreamType::None) {
					UI_REFLECTION_SET_DEFAULT_DATA[(unsigned int)type->fields[index].reflection_index](
						type->fields.buffer + index,
						function::OffsetPointer(data, type->fields[index].pointer_offset)
					);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeRange(const char* type_name, Stream<UIReflectionBindRange> data)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypeRange(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeRange(UIReflectionType* type, Stream<UIReflectionBindRange> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, data[index].field_name);
				UI_REFLECTION_SET_RANGE[(unsigned int)type->fields[field_index].reflection_index](type->fields.buffer + field_index, data[index].min, data[index].max);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypePrecision(const char* type_name, Stream<UIReflectionBindPrecision> data)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypePrecision(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypePrecision(UIReflectionType* type, Stream<UIReflectionBindPrecision> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, data[index].field_name);
				switch (type->fields[field_index].reflection_index)
				{
				case UIReflectionIndex::FloatSlider:
				{
					UIReflectionFloatSliderData* field_data = (UIReflectionFloatSliderData*)type->fields[field_index].data;
					field_data->precision = data[index].precision;
					break;
				}
				case UIReflectionIndex::DoubleSlider:
				{
					UIReflectionDoubleSliderData* field_data = (UIReflectionDoubleSliderData*)type->fields[field_index].data;
					field_data->precision = data[index].precision;
					break;
				}
				case UIReflectionIndex::FloatSliderGroup:
				case UIReflectionIndex::DoubleSliderGroup:
				{
					UIReflectionGroupData<float>* field_data = (UIReflectionGroupData<float>*)type->fields[field_index].data;
					field_data->precision = data[index].precision;
					break;
				}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstancePtrs(const char* instance_name, void* data)
		{
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			BindInstancePtrs(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstancePtrs(UIReflectionInstance* instance, void* data)
		{
			ReflectionType reflect = reflection->GetType(instance->type_name);
			BindInstancePtrs(instance, data, reflect);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstancePtrs(UIReflectionInstance* instance, void* data, ReflectionType reflect)
		{
			UIReflectionType type = GetType(instance->type_name);
			uintptr_t ptr = (uintptr_t)data;

			for (size_t index = 0; index < type.fields.size; index++) {
				ptr = (uintptr_t)data + type.fields[index].pointer_offset;

				UIReflectionIndex reflected_index = type.fields[index].reflection_index;

				bool is_stream = type.fields[index].stream_type == UIReflectionStreamType::Basic;

				if (!is_stream) {
					bool is_group = reflected_index == UIReflectionIndex::IntegerInputGroup || reflected_index == UIReflectionIndex::IntegerSliderGroup
						|| reflected_index == UIReflectionIndex::FloatInputGroup || reflected_index == UIReflectionIndex::FloatSliderGroup
						|| reflected_index == UIReflectionIndex::DoubleInputGroup || reflected_index == UIReflectionIndex::DoubleSliderGroup;

					if (!is_group) {
						void** reinterpretation = (void**)instance->datas[index].struct_data;
						*reinterpretation = (void*)ptr;

						switch (reflected_index) {
						case UIReflectionIndex::IntegerInput:
						case UIReflectionIndex::IntegerSlider:
						{
							UIReflectionIntInputData* field_data = (UIReflectionIntInputData*)instance->datas[index].struct_data;
							memcpy(field_data->default_value, field_data->value_to_modify, field_data->byte_size);
						}
						break;
						case UIReflectionIndex::FloatInput:
						case UIReflectionIndex::FloatSlider:
						{
							UIReflectionFloatInputData* field_data = (UIReflectionFloatInputData*)instance->datas[index].struct_data;
							field_data->default_value = *field_data->value;
						}
						break;
						case UIReflectionIndex::DoubleInput:
						case UIReflectionIndex::DoubleSlider:
						{
							UIReflectionDoubleInputData* field_data = (UIReflectionDoubleInputData*)instance->datas[index].struct_data;
							field_data->default_value = *field_data->value;
						}
						break;
						case UIReflectionIndex::Color:
						{
							UIReflectionColorData* field_data = (UIReflectionColorData*)instance->datas[index].struct_data;
							field_data->default_color = *field_data->color;
						}
						break;
						case UIReflectionIndex::ColorFloat:
						{
							UIReflectionColorFloatData* field_data = (UIReflectionColorFloatData*)instance->datas[index].struct_data;
							field_data->default_color = *field_data->color;
						}
						break;
						case UIReflectionIndex::ComboBox:
						{
							UIReflectionComboBoxData* field_data = (UIReflectionComboBoxData*)instance->datas[index].struct_data;
							field_data->default_label = *field_data->active_label;
						}
						break;
						}
					}
					else {
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance->datas[index].struct_data;
						void* allocation = allocator->Allocate(sizeof(void*) * data->count);
						data->values = (void**)allocation;
		
						unsigned int type_byte_size = reflect.fields[index].info.byte_size / data->count;
						for (size_t subindex = 0; subindex < data->count; subindex++) {
							data->values[subindex] = (void*)ptr;
							memcpy((void*)((uintptr_t)data->default_values + subindex * type_byte_size), (const void*)ptr, type_byte_size);
							ptr += type_byte_size;
						}
					}
				}
				else {
					CapacityStream<void>** values = (CapacityStream<void>**)instance->datas[index].struct_data;
					// Resizable streams can be safely aliased with capacity streams since the layout is identical, except the resizable stream has an allocator at the end
					if (reflect.fields[index].info.stream_type == ReflectionStreamFieldType::CapacityStream || reflect.fields[index].info.stream_type == ReflectionStreamFieldType::ResizableStream) {
						*values = (CapacityStream<void>*)ptr;
					}
					else if (reflect.fields[index].info.stream_type == ReflectionStreamFieldType::Pointer) {
						instance->datas[index].stream_data = (CapacityStream<void>*)allocator->Allocate(sizeof(CapacityStream<void>));
						instance->datas[index].stream_data->buffer = *(void**)ptr;
						instance->datas[index].stream_data->size = 0;
						instance->datas[index].stream_data->capacity = 0;
						*values = instance->datas[index].stream_data;
					}
					else if (reflect.fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
						instance->datas[index].stream_data = (CapacityStream<void>*)allocator->Allocate(sizeof(CapacityStream<void>));
						instance->datas[index].stream_data->buffer = *(void**)ptr;
						instance->datas[index].stream_data->size = 0;
						instance->datas[index].stream_data->capacity = reflect.fields[index].info.basic_type_count;
						*values = instance->datas[index].stream_data;
					}
					// Streams should get here
					else {
						instance->datas[index].stream_data = (CapacityStream<void>*)allocator->Allocate(sizeof(CapacityStream<void>));
						instance->datas[index].stream_data->buffer = *(void**)ptr;
						instance->datas[index].stream_data->size = 0;
						instance->datas[index].stream_data->capacity = 0;
						*values = instance->datas[index].stream_data;
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceTextInput(const char* instance_name, Stream<UIReflectionBindTextInput> data)
		{
			UIReflectionInstance* instance_ptr = GetInstancePtr(instance_name);
			BindInstanceTextInput(instance_ptr, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceTextInput(UIReflectionInstance* instance, Stream<UIReflectionBindTextInput> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (type.fields[field_index].reflection_index == UIReflectionIndex::TextInput) {
					CapacityStream<char>** input_data = (CapacityStream<char>**)instance->datas[field_index].struct_data;
					*input_data = data[index].stream;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamCapacity(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data)
		{
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			BindInstanceStreamCapacity(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				instance->datas[field_index].stream_data->capacity = data[index].capacity;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamSize(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data)
		{
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			BindInstanceStreamSize(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamSize(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				instance->datas[field_index].stream_data->size = data[index].capacity;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamBuffer(const char* instance_name, Stream<UIReflectionBindStreamBuffer> data) {
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			BindInstanceStreamBuffer(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<UIReflectionBindStreamBuffer> data) {
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				instance->datas[field_index].stream_data->buffer = data[index].new_buffer;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::CopyInstanceStreams(const char* instance_name, Stream<UIReflectionStreamCopy> data)
		{
			UIReflectionInstance* instance = GetInstancePtr(instance_name);
			CopyInstanceStreams(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::CopyInstanceStreams(UIReflectionInstance* instance, Stream<UIReflectionStreamCopy> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			ReflectionType reflected_type = reflection->GetType(type.name);

			for (size_t index = 0; index < instance->datas.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (type.fields[field_index].stream_type == UIReflectionStreamType::Basic) {
					// Get the byte size from the original reflected type
					size_t byte_size = 0;
					for (size_t reflected_index = 0; reflected_index < reflected_type.fields.size; reflected_index++) {
						if (reflected_type.fields[reflected_index].name == type.fields[field_index].name) {
							byte_size = reflected_type.fields[reflected_index].info.stream_byte_size;
							break;
						}
					}
					memcpy(data[index].destination, instance->datas[field_index].stream_data->buffer, instance->datas[field_index].stream_data->size *
						byte_size);
					data[index].element_count = instance->datas[field_index].stream_data->size;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ChangeInputToSlider(const char* type_name, const char* field_name)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			ChangeInputToSlider(type, field_name);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

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
			// integer_convert input needs no correction, except reflection_index
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

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeData(const char* type_name, const char* field_name, void* field_data)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypeData(type, field_name, field_data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

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

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeLowerBounds(const char* type_name, Stream<UIReflectionBindLowerBound> data)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypeLowerBounds(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeLowerBounds(UIReflectionType* type, Stream<UIReflectionBindLowerBound> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, data[index].field_name);
				UI_REFLECTION_SET_LOWER_BOUND[(unsigned int)type->fields[field_index].reflection_index](type->fields.buffer + field_index, data[index].value);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeLowerBounds(UIReflectionType* type, const void* data) {
			for (size_t index = 0; index < type->fields.size; index++) {
				unsigned int int_index = (unsigned int)type->fields[index].reflection_index;
				if (int_index < std::size(UI_REFLECTION_SET_LOWER_BOUND) && type->fields[index].stream_type == UIReflectionStreamType::None) {
					UI_REFLECTION_SET_LOWER_BOUND[int_index](type->fields.buffer + index, function::OffsetPointer(data, type->fields[index].pointer_offset));
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeUpperBounds(const char* type_name, Stream<UIReflectionBindUpperBound> data)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			BindTypeUpperBounds(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeUpperBounds(UIReflectionType* type, Stream<UIReflectionBindUpperBound> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, data[index].field_name);
				UI_REFLECTION_SET_UPPER_BOUND[(unsigned int)type->fields[field_index].reflection_index](type->fields.buffer + field_index, data[index].value);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeUpperBounds(UIReflectionType* type, const void* data) {
			for (size_t index = 0; index < type->fields.size; index++) {
				unsigned int int_index = (unsigned int)type->fields[index].reflection_index;
				if (int_index < std::size(UI_REFLECTION_SET_UPPER_BOUND) && type->fields[index].stream_type == UIReflectionStreamType::None) {
					UI_REFLECTION_SET_UPPER_BOUND[int_index](type->fields.buffer + index, function::OffsetPointer(data, type->fields[index].pointer_offset));
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::CreateType(const char* name)
		{
			{
				ECS_RESOURCE_IDENTIFIER(name);
				ECS_ASSERT(type_definition.Find(identifier) == -1);
			}

			return CreateType(reflection->GetType(name));
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::CreateType(ReflectionType reflected_type)
		{
			auto integer_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionIntInputData* data = (UIReflectionIntInputData*)allocator->Allocate(sizeof(UIReflectionIntInputData));
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

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = (size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type) /*| UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER*/;
			};

			auto integer_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)allocator->Allocate(sizeof(UIReflectionGroupData<void>));
				field.data = data;
				field.reflection_index = UIReflectionIndex::IntegerInputGroup;
				field.byte_size = sizeof(*data);
				data->group_name = reflection_field.name;

				data->count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
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

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = (size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type) /*| UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER*/;
			};

			auto float_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)allocator->Allocate(sizeof(UIReflectionFloatInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::FloatInput;
				field.byte_size = sizeof(*data);
				data->name = reflection_field.name;

				// defaults
				data->default_value = 0.0f;
				data->lower_bound = 0.0f;
				data->upper_bound = 0.0f;
				data->value = nullptr;

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto float_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)allocator->Allocate(sizeof(UIReflectionGroupData<float>));
				field.data = data;
				field.reflection_index = UIReflectionIndex::FloatInputGroup;
				field.byte_size = sizeof(*data);
				data->group_name = reflection_field.name;

				data->count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
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

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto double_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)allocator->Allocate(sizeof(UIReflectionDoubleInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::DoubleInput;
				field.byte_size = sizeof(*data);
				data->name = reflection_field.name;

				// defaults
				data->value = nullptr;
				data->default_value = 0;
				data->lower_bound = 0;
				data->upper_bound = 0;

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto double_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)allocator->Allocate(sizeof(UIReflectionGroupData<double>));
				field.data = data;
				field.reflection_index = UIReflectionIndex::DoubleInputGroup;
				field.byte_size = sizeof(*data);
				data->group_name = reflection_field.name;

				data->count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
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

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto enum_convert = [this](ReflectionEnum reflection_enum, UIReflectionTypeField& field) {
				UIReflectionComboBoxData* data = (UIReflectionComboBoxData*)this->allocator->Allocate(sizeof(UIReflectionComboBoxData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::ComboBox;
				field.byte_size = sizeof(*data);
				data->labels = reflection_enum.fields;
				data->label_display_count = reflection_enum.fields.size;
				data->name = reflection_enum.name;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto text_input_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionTextInputData* data = (UIReflectionTextInputData*)allocator->Allocate(sizeof(UIReflectionTextInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::TextInput;
				field.byte_size = sizeof(*data);

				data->name = reflection_field.name;
				data->characters = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto bool_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionCheckBoxData* data = (UIReflectionCheckBoxData*)allocator->Allocate(sizeof(UIReflectionCheckBoxData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::CheckBox;
				field.byte_size = sizeof(*data);

				data->name = reflection_field.name;
				data->value = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto color_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionColorData* data = (UIReflectionColorData*)allocator->Allocate(sizeof(UIReflectionColorData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::Color;
				field.byte_size = sizeof(*data);

				data->color = nullptr;
				data->default_color = Color(0, 0, 0);
				data->name = reflection_field.name;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto color_float_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionColorFloatData* data = (UIReflectionColorFloatData*)allocator->Allocate(sizeof(UIReflectionColorFloatData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::ColorFloat;
				field.byte_size = sizeof(*data);

				data->color = nullptr;
				data->default_color = ColorFloat(0.0f, 0.0f, 0.0f, 1.0f);
				data->name = reflection_field.name;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto integer_stream_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamIntInputData* data = (UIReflectionStreamIntInputData*)this->allocator->Allocate(sizeof(UIReflectionIntInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::IntegerInput;
				field.byte_size = sizeof(*data);

				data->name = reflection_field.name;
				data->values = nullptr;

				field.stream_type = UIReflectionStreamType::Basic;
				field.configuration = (size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type) | UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER;
			};

			auto integer_stream_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)allocator->Allocate(sizeof(UIReflectionGroupData<void>));
				field.data = data;
				field.reflection_index = UIReflectionIndex::IntegerInputGroup;
				field.byte_size = sizeof(*data);

				data->group_name = reflection_field.name;
				data->basic_type_count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
				data->values = nullptr;

				field.stream_type = UIReflectionStreamType::Basic;
				field.configuration = (size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type) | UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER;
			};

			auto float_stream_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamFloatInputData* data = (UIReflectionStreamFloatInputData*)allocator->Allocate(sizeof(UIReflectionIntInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::FloatInput;
				field.byte_size = sizeof(*data);

				data->name = reflection_field.name;
				data->values = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto float_stream_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)allocator->Allocate(sizeof(UIReflectionGroupData<void>));
				field.data = data;
				field.reflection_index = UIReflectionIndex::FloatInputGroup;
				field.byte_size = sizeof(*data);

				data->group_name = reflection_field.name;
				data->basic_type_count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
				data->values = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto double_stream_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamDoubleInputData* data = (UIReflectionStreamDoubleInputData*)this->allocator->Allocate(sizeof(UIReflectionIntInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::DoubleInput;
				field.byte_size = sizeof(*data);

				data->name = reflection_field.name;
				data->values = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto double_stream_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)allocator->Allocate(sizeof(UIReflectionGroupData<void>));
				field.data = data;
				field.reflection_index = UIReflectionIndex::DoubleInputGroup;
				field.byte_size = sizeof(*data);

				data->group_name = reflection_field.name;
				data->basic_type_count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
				data->values = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto enum_stream_convert = [this](ReflectionEnum reflection_enum, UIReflectionTypeField& field) {
				UIReflectionStreamComboBoxData* data = (UIReflectionStreamComboBoxData*)allocator->Allocate(sizeof(UIReflectionComboBoxData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::ComboBox;
				field.byte_size = sizeof(*data);

				data->values = nullptr;
				data->label_names = nullptr;
				data->name = reflection_enum.name;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto text_input_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamTextInputData* data = (UIReflectionStreamTextInputData*)allocator->Allocate(sizeof(UIReflectionTextInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::TextInput;
				field.byte_size = sizeof(*data);

				data->name = reflection_field.name;
				data->values = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto bool_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamCheckBoxData* data = (UIReflectionStreamCheckBoxData*)allocator->Allocate(sizeof(UIReflectionCheckBoxData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::CheckBox;
				field.byte_size = sizeof(*data);

				data->name = reflection_field.name;
				data->values = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto color_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamColorData* data = (UIReflectionStreamColorData*)allocator->Allocate(sizeof(UIReflectionColorData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::Color;
				field.byte_size = sizeof(*data);

				data->values = nullptr;
				data->name = reflection_field.name;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			auto color_float_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamColorFloatData* data = (UIReflectionStreamColorFloatData*)allocator->Allocate(sizeof(UIReflectionColorFloatData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::ColorFloat;
				field.byte_size = sizeof(*data);

				data->values = nullptr;
				data->name = reflection_field.name;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Basic;
			};

			UIReflectionType type;
			type.name = reflected_type.name;
			type.fields.Initialize(allocator, 0, reflected_type.fields.size);

			auto test_basic_type = [&](unsigned int index, ReflectionFieldInfo field_info, bool& value_written) {
				if (strcmp(reflected_type.fields[index].definition, "Color") == 0) {
					color_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (strcmp(reflected_type.fields[index].definition, "ColorFloat") == 0) {
					color_float_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralSingleComponent(field_info.basic_type)) {
					integer_convert_single(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralMultiComponent(field_info.basic_type)) {
					integer_convert_group(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Float) {
					float_convert_single(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsFloatBasicTypeMultiComponent(field_info.basic_type)) {
					float_convert_group(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Double) {
					double_convert_single(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsDoubleBasicTypeMultiComponent(field_info.basic_type)) {
					double_convert_group(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Bool) {
					bool_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsEnum(field_info.basic_type)) {
					ReflectionEnum reflected_enum;
					bool success = reflection->TryGetEnum(reflected_type.fields[index].definition, reflected_enum);
					if (success) {
						enum_convert(reflected_enum, type.fields[type.fields.size]);
						value_written = true;
					}
				}
			};

			auto test_stream_type = [&](unsigned int index, ReflectionFieldInfo field_info, bool& value_written) {
				if (strcmp(reflected_type.fields[index].definition, "Color") == 0) {
					color_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (strcmp(reflected_type.fields[index].definition, "ColorFloat") == 0) {
					color_float_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				// text input - default for char and unsigned char
				else if (field_info.basic_type == ReflectionBasicFieldType::Int8 || field_info.basic_type == ReflectionBasicFieldType::UInt8) {
					text_input_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Float) {
					float_stream_convert_single(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsFloatBasicTypeMultiComponent(field_info.basic_type)) {
					float_stream_convert_group(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Double) {
					double_stream_convert_single(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsDoubleBasicTypeMultiComponent(field_info.basic_type)) {
					double_stream_convert_group(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralSingleComponent(field_info.basic_type)) {
					integer_stream_convert_single(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralMultiComponent(field_info.basic_type)) {
					integer_stream_convert_group(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Bool) {
					bool_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsEnum(field_info.basic_type)) {
					ReflectionEnum reflected_enum;
					bool success = reflection->TryGetEnum(reflected_type.fields[index].definition, reflected_enum);
					if (success) {
						enum_stream_convert(reflected_enum, type.fields[type.fields.size]);
						value_written = true;
					}
				}
			};

			for (size_t index = 0; index < reflected_type.fields.size; index++) {
				ReflectionFieldInfo field_info = reflected_type.fields[index].info;

				bool value_written = false;
				type.fields[index].name = reflected_type.fields[index].name;
				type.fields[type.fields.size].pointer_offset = reflected_type.fields[index].info.pointer_offset;
				if (field_info.stream_type == ReflectionStreamFieldType::Pointer) {
					// The basic type count tells the indirection count: e.g void* - indirection 1; void** indirection 2
					// Only reflect single indirection pointers
					if (field_info.basic_type_count == 1) {
						test_stream_type(index, field_info, value_written);
					}
				}
				else if (field_info.stream_type != Reflection::ReflectionStreamFieldType::Basic && field_info.stream_type != Reflection::ReflectionStreamFieldType::Unknown) {
					test_stream_type(index, field_info, value_written);
				}
				else {
					test_basic_type(index, field_info, value_written);
				}

				if (value_written && field_info.has_default_value) {
					UI_REFLECTION_SET_DEFAULT_DATA[(unsigned int)field_info.basic_type](type.fields.buffer + type.fields.size - 1, &field_info.default_bool);
				}

				type.fields.size += value_written;
			}

			if (type.fields.size < type.fields.capacity) {
				void* new_allocation = allocator->Allocate(sizeof(UIReflectionTypeField) * type.fields.size);
				memcpy(new_allocation, type.fields.buffer, sizeof(UIReflectionTypeField) * type.fields.size);
				allocator->Deallocate(type.fields.buffer);
				type.fields.buffer = (UIReflectionTypeField*)new_allocation;
			}

			ECS_RESOURCE_IDENTIFIER(type.name);
			ECS_ASSERT(!type_definition.Insert(type, identifier));
			return type_definition.GetValuePtr(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::CreateInstance(const char* name, const char* type_name)
		{
			{
				ECS_RESOURCE_IDENTIFIER(name);
				ECS_ASSERT(instances.Find(identifier) == -1);
			}

			UIReflectionType type = GetType(type_name);
			return CreateInstance(name, &type);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::CreateInstance(const char* name, const UIReflectionType* type)
		{
			UIReflectionInstance instance;
			instance.type_name = type->name;

			size_t name_length = strlen(name) + 1;
			size_t total_memory = name_length + sizeof(UIReflectionInstanceFieldData) * type->fields.size;

			for (size_t index = 0; index < type->fields.size; index++) {
				total_memory += type->fields[index].byte_size;
			}

			void* allocation = allocator->Allocate(total_memory);
			uintptr_t buffer = (uintptr_t)allocation;
			buffer += sizeof(UIReflectionInstanceFieldData) * type->fields.size;
			instance.datas.buffer = (UIReflectionInstanceFieldData*)allocation;

			for (size_t index = 0; index < type->fields.size; index++) {
				instance.datas[index].struct_data = (void*)buffer;
				buffer += type->fields[index].byte_size;
				memcpy(instance.datas[index].struct_data, type->fields[index].data, type->fields[index].byte_size);

				instance.datas[index].stream_data = nullptr;

				if (type->fields[index].stream_type == UIReflectionStreamType::None) {
					switch (type->fields[index].reflection_index) {
					case UIReflectionIndex::DoubleInputGroup:
					case UIReflectionIndex::DoubleSliderGroup:
					case UIReflectionIndex::FloatInputGroup:
					case UIReflectionIndex::FloatSliderGroup:
					case UIReflectionIndex::IntegerInputGroup:
					case UIReflectionIndex::IntegerSliderGroup:
					{
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance.datas[index].struct_data;
						data->values = (void**)allocator->Allocate(sizeof(void*) * data->count);
					}
					break;
					}
				}
			}

			char* instance_name = (char*)buffer;
			memcpy(instance_name, name, name_length * sizeof(char));
			instance.name = instance_name;

			instance.datas.size = type->fields.size;

			ECS_RESOURCE_IDENTIFIER(instance.name);
			ECS_ASSERT(!instances.Insert(instance, identifier));
			return instances.GetValuePtr(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::CreateTypesForHierarchy(unsigned int hierarchy_index, bool exclude_components)
		{
			auto types = reflection->type_definitions.GetValueStream();
			auto ui_types = type_definition.GetValueStream();

			unsigned int count = 0;
			for (size_t index = 0; index < types.size; index++) {
				if (reflection->type_definitions.IsItemAt(index)) {
					if (types[index].folder_hierarchy_index == hierarchy_index) {
						if (exclude_components) {
							if (!reflection->IsTypeTag(index, STRING(COMPONENT))) {
								count++;
								CreateType(types[index]);
							}
						}
						else {
							count++;
							CreateType(types[index]);
						}
					}
				}
			}

			return count;
		}

		unsigned int UIReflectionDrawer::CreateTypesForHierarchy(const wchar_t* hierarchy, bool exclude_components)
		{
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				return CreateTypesForHierarchy(hierarchy_index, exclude_components);
			}
			ECS_ASSERT(false);
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::CreateInstanceForHierarchy(unsigned int hierarchy_index)
		{
			auto types = type_definition.GetValueStream();

			unsigned int count = 0;
			for (size_t index = 0; index < types.size; index++) {
				if (type_definition.IsItemAt(index)) {
					ReflectionType type = reflection->GetType(types[index].name);
					if (type.folder_hierarchy_index == hierarchy_index) {
						CreateInstance(types[index].name, &types[index]);
					}
				}
			}

			return count;
		}

		unsigned int UIReflectionDrawer::CreateInstanceForHierarchy(const wchar_t* hierarchy)
		{
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				return CreateInstanceForHierarchy(hierarchy_index);
			}
			ECS_ASSERT(false);
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::CreateTypesAndInstancesForHierarchy(unsigned int hierarchy_index, bool exclude_components) {
			auto types = reflection->type_definitions.GetValueStream();
			auto ui_types = type_definition.GetValueStream();

			unsigned int count = 0;
			
			auto create_type_and_instance = [&](size_t index) {
				count++;
				UIReflectionType* type = CreateType(types[index]);
				CreateInstance(type->name, type);
			};

			for (size_t index = 0; index < types.size; index++) {
				if (reflection->type_definitions.IsItemAt(index)) {
					if (types[index].folder_hierarchy_index == hierarchy_index) {
						if (exclude_components) {
							if (!reflection->IsTypeTag(index, STRING(COMPONENT))) {
								create_type_and_instance(index);
							}
						}
						else {
							create_type_and_instance(index);
						}
					}
				}
			}

			return count;
		}

		unsigned int UIReflectionDrawer::CreateTypesAndInstancesForHierarchy(const wchar_t* hierarchy, bool exclude_components) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				return CreateTypesAndInstancesForHierarchy(hierarchy_index);
			}
			ECS_ASSERT(false);
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyInstance(unsigned int index) {
			UIReflectionInstance instance = instances.GetValueFromIndex(index);

			UIReflectionType type = GetType(instance.type_name);
			for (size_t index = 0; index < type.fields.size; index++) {
				bool is_stream = type.fields[index].stream_type == UIReflectionStreamType::Basic;
				if (!is_stream) {
					switch (type.fields[index].reflection_index) {
					case UIReflectionIndex::IntegerInputGroup:
					case UIReflectionIndex::IntegerSliderGroup:
					case UIReflectionIndex::DoubleInputGroup:
					case UIReflectionIndex::DoubleSliderGroup:
					case UIReflectionIndex::FloatInputGroup:
					case UIReflectionIndex::FloatSliderGroup:
					{
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance.datas[index].struct_data;
						allocator->Deallocate(data->values);
					}
					break;
					}
				}
				else {
					if (instance.datas[index].stream_data != nullptr) {
						allocator->Deallocate(instance.datas[index].stream_data);
					}
				}
			}

			allocator->Deallocate(instance.datas.buffer);
			instances.EraseFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyInstance(const char* name)
		{
			unsigned int instance_index = instances.Find(name);

			if (instance_index != -1) {
				DestroyInstance(instance_index);
				return;
			}

			ECS_ASSERT(false);
		}

		// -------------------------------------------------------------------------------------------------------------------------------

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
				bool is_stream = type.fields[index].stream_type == UIReflectionStreamType::Basic;

				UIReflectionIndex reflect_index = type.fields[index].reflection_index;
				if (!is_stream) {
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
				}
				allocator->Deallocate(type.fields[index].data);
			}
			allocator->Deallocate(type.fields.buffer);

			ECS_RESOURCE_IDENTIFIER(name);
			type_definition.Erase(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DrawInstance(
			const char* ECS_RESTRICT instance_name,
			UIDrawer& drawer,
			UIDrawConfig& config,
			Stream<UIReflectionDrawConfig> additional_configs,
			const char* ECS_RESTRICT default_value_button
		) {
			DrawInstance(GetInstancePtr(instance_name), drawer, config, additional_configs, default_value_button);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DrawInstance(
			UIReflectionInstance* instance,
			UIDrawer& drawer,
			UIDrawConfig& config,
			Stream<UIReflectionDrawConfig> additional_configs,
			const char* ECS_RESTRICT default_value_button
		) {
			UIReflectionType type = GetType(instance->type_name);

			UIDrawerMode draw_mode = drawer.draw_mode;
			unsigned int draw_mode_target = drawer.draw_mode_target;

			drawer.SetDrawMode(UIDrawerMode::NextRow);

			if (default_value_button != nullptr) {
				UIReflectionDefaultValueData default_data;
				default_data.instance = *instance;
				default_data.type = type;
				drawer.Button(default_value_button, { UIReflectionDefaultValueAction, &default_data, sizeof(default_data) });
			}

			for (size_t index = 0; index < type.fields.size; index++) {
				size_t config_flag_count = config.flag_count;
				size_t current_configuration = type.fields[index].configuration;
				for (size_t subindex = 0; subindex < additional_configs.size; subindex++) {
					for (size_t type_index = 0; type_index < additional_configs[subindex].index_count; type_index++) {
						if (additional_configs[subindex].index[type_index] == type.fields[index].reflection_index || 
							additional_configs[subindex].index[type_index] == UIReflectionIndex::Count) {
							current_configuration |= additional_configs[subindex].configurations;
							UIReflectionDrawConfigCopyToNormalConfig(additional_configs.buffer + subindex, config);
						}
					}
				}

				if (type.fields[index].stream_type == UIReflectionStreamType::None) {
					UI_REFLECTION_FIELD_BASIC_DRAW[(unsigned int)type.fields[index].reflection_index](
						drawer,
						config,
						current_configuration,
						instance->datas[index].struct_data
					);
				}
				else {
					UI_REFLECTION_FIELD_STREAM_DRAW[(unsigned int)type.fields[index].reflection_index](
						drawer,
						config, 
						current_configuration,
						instance->datas[index].struct_data
					);
					drawer.NextRow(-1.0f);
				}

				config.flag_count = config_flag_count;
			}

			drawer.SetDrawMode(draw_mode, draw_mode_target);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyAllFromFolderHierarchy(unsigned int hierarchy_index) {
			// Iterate through types and remove UI types and instances associated with the given hierarchy index
			auto types = reflection->type_definitions.GetValueStream();
			auto ui_types = type_definition.GetValueStream();

			// Record the positions of the UI types inside the hash table
			unsigned int* _type_mask = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * ui_types.size);
			Stream<unsigned int> type_mask(_type_mask, 0);
			for (size_t index = 0; index < ui_types.size; index++) {
				if (type_definition.IsItemAt(index)) {
					type_mask.Add(index);
				}
			}

			const char** _types_to_be_deleted = (const char**)ECS_STACK_ALLOC(sizeof(const char*) * type_mask.size);
			Stream<const char*> types_to_be_deleted(_types_to_be_deleted, 0);

			for (size_t index = 0; index < types.size; index++) {
				if (reflection->type_definitions.IsItemAt(index) && types[index].folder_hierarchy_index == hierarchy_index) {
					// Search through the UI types first and create a temporary array of those that match
					for (size_t subindex = 0; subindex < type_mask.size; subindex++) {
						if (function::CompareStrings(ui_types[type_mask[subindex]].name, types[index].name)) {
							types_to_be_deleted.Add(types[index].name);
							break;
						}
					}
				}
			}

			// Destroy the instance that have the base types the ones that need to be deleted
			auto instance_stream = instances.GetValueStream();
			for (int64_t index = 0; index < instance_stream.size; index++) {
				if (instances.IsItemAt(index)) {
					for (size_t subindex = 0; subindex < types_to_be_deleted.size; subindex++) {
						if (function::CompareStrings(instance_stream[index].type_name, types_to_be_deleted[subindex])) {
							// Decrement the index afterwards - the values will be shuffled after robin hood rebalancing
							DestroyInstance(index);
							index--;
							break;
						}
					}
				}
			}

			// Destroy all types now
			for (size_t index = 0; index < types_to_be_deleted.size; index++) {
				DestroyType(types_to_be_deleted[index]);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyAllFromFolderHierarchy(const wchar_t* hierarchy) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				DestroyAllFromFolderHierarchy(hierarchy_index);
				return;
			}
			ECS_ASSERT(false);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetInstanceStreamSizes(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data)
		{
			GetInstanceStreamSizes(GetInstancePtr(instance_name), data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetInstanceStreamSizes(const UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data)
		{
			UIReflectionType type = GetType(instance->name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				data[index].capacity = instance->datas[field_index].stream_data->size;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetHierarchyTypes(unsigned int hierarchy_index, CapacityStream<unsigned int>& indices) {
			auto types = type_definition.GetValueStream();

			for (size_t index = 0; index < types.size; index++) {
				if (type_definition.IsItemAt(index)) {
					ReflectionType type = reflection->GetType(types[index].name);
					if (type.folder_hierarchy_index == hierarchy_index) {
						indices.AddSafe(index);
					}
				}
			}
		}


		void UIReflectionDrawer::GetHierarchyTypes(const wchar_t* hierarchy, CapacityStream<unsigned int>& indices) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				GetHierarchyTypes(hierarchy_index, indices);
				return;
			}
			ECS_ASSERT(false);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetHierarchyInstances(unsigned int hierarchy_index, CapacityStream<unsigned int>& indices) {
			auto instance_stream = instances.GetValueStream();

			for (size_t index = 0; index < instance_stream.size; index++) {
				if (instances.IsItemAt(index)) {
					ReflectionType type = reflection->GetType(instance_stream[index].name);
					if (type.folder_hierarchy_index == hierarchy_index) {
						indices.AddSafe(index);
					}
				}
			}
		}

		void UIReflectionDrawer::GetHierarchyInstances(const wchar_t* hierarchy, CapacityStream<unsigned int>& indices) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				GetHierarchyInstances(hierarchy_index, indices);
				return;
			}
			ECS_ASSERT(false);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType UIReflectionDrawer::GetType(const char* name) const
		{
			ECS_RESOURCE_IDENTIFIER(name);
			return type_definition.GetValue(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType UIReflectionDrawer::GetType(unsigned int index) const
		{
			return type_definition.GetValueFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::GetTypePtr(const char* name) const
		{
			ECS_RESOURCE_IDENTIFIER(name);
			return type_definition.GetValuePtr(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::GetTypePtr(unsigned int index) const
		{
			return type_definition.GetValuePtrFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------
		
		UIReflectionInstance UIReflectionDrawer::GetInstance(const char* name) const {
			ECS_RESOURCE_IDENTIFIER(name);
			return instances.GetValue(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance UIReflectionDrawer::GetInstance(unsigned int index) const {
			return instances.GetValueFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::GetInstancePtr(const char* name) const
		{
			ECS_RESOURCE_IDENTIFIER(name);
			return instances.GetValuePtr(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::GetInstancePtr(unsigned int index) const
		{
			return instances.GetValuePtrFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::GetTypeFieldIndex(UIReflectionType type, const char* field_name) const
		{
			unsigned int field_index = 0;
			while ((field_index < type.fields.size) && strcmp(type.fields[field_index].name, field_name) != 0) {
				field_index++;
			}
			ECS_ASSERT(field_index < type.fields.size);
			return field_index;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::GetTypeFieldIndex(const char* ECS_RESTRICT type_name, const char* ECS_RESTRICT field_name)
		{
			UIReflectionType type = GetType(type_name);
			return GetTypeFieldIndex(type, field_name);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::GetTypeCount() const {
			return type_definition.GetCount();
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::GetInstanceCount() const {
			return instances.GetCount();
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeField(const char* type_name, const char* field_name)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			OmitTypeField(type, field_name);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeField(UIReflectionType* type, const char* field_name)
		{
			unsigned int field_index = GetTypeFieldIndex(*type, field_name);
			allocator->Deallocate(type->fields[field_index].data);
			type->fields.Remove(field_index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeFields(const char* type_name, Stream<const char*> fields)
		{
			UIReflectionType* type = GetTypePtr(type_name);
			OmitTypeFields(type, fields);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeFields(UIReflectionType* type, Stream<const char*> fields)
		{
			for (size_t index = 0; index < fields.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, fields[index]);
				allocator->Deallocate(type->fields[field_index].data);
				type->fields.Remove(field_index);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDefaultValueAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIReflectionDefaultValueData* default_data = (UIReflectionDefaultValueData*)_data;

#define CASE_START(type, data_type) case UIReflectionIndex::type: { data_type* data = (data_type*)default_data->instance.datas[index].struct_data; 
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

		// ------------------------------------------------------------------------------------------------------------------------------

		// ------------------------------------------------------------------------------------------------------------------------------

		// ------------------------------------------------------------------------------------------------------------------------------



	}

}