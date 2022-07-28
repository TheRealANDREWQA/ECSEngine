#include "ecspch.h"
#include "UIReflection.h"
#include "../../Utilities/Mouse.h"
#include "../../Utilities/Keyboard.h"
#include "../../Utilities/Reflection/ReflectionStringFunctions.h"

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

		// The first field must always be the pointer to the field
		struct UIReflectionFloatSliderData {
			float* value_to_modify;
			const char* name;
			float lower_bound;
			float upper_bound;
			float default_value;
			unsigned int precision = 2;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionDoubleSliderData {
			double* value_to_modify;
			const char* name;
			double lower_bound;
			double upper_bound;
			double default_value;
			unsigned int precision = 2;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionIntSliderData {
			void* value_to_modify;
			const char* name;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionFloatInputData {
			float* value;
			const char* name;
			float lower_bound;
			float upper_bound;
			float default_value;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionDoubleInputData {
			double* value;
			const char* name;
			double lower_bound;
			double upper_bound;
			double default_value;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionIntInputData {
			void* value_to_modify;
			const char* name;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionColorData {
			Color* color;
			const char* name;
			Color default_color;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionColorFloatData {
			ColorFloat* color;
			const char* name;
			ColorFloat default_color;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionCheckBoxData {
			bool* value;
			const char* name;
			bool default_value;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionComboBoxData {
			unsigned char* active_label;
			const char* name;
			Stream<const char*> labels;
			unsigned int label_display_count;
			unsigned char default_label;
		};

		// The first field must always be the pointer to the field
		template<typename Field>
		struct UIReflectionGroupData {
			Field** values;
			const char* group_name;
			const char** input_names;
			const Field* lower_bound;
			const Field* upper_bound;
			const Field* default_values;
			unsigned int count;
			unsigned int precision = 3;
			unsigned int byte_size;
		};
		
		struct UIReflectionDrawer;

		struct UIReflectionUserDefinedData {
			const char* instance_name;
			UIReflectionDrawer* ui_drawer;
		};

		// All the streams struct contain this as the first member variable
		struct UIInstanceFieldStream {
			// Mirrors the fields inside the stream into the target
			void WriteTarget() {
				// It is ok to alias the resizable stream with the capacity stream
				*target_memory = capacity->buffer;

				if (target_size_t_size) {
					size_t* size_ptr = (size_t*)function::OffsetPointer(target_memory, sizeof(void*));
					*size_ptr = capacity->size;
				}
				else if (target_uint_size) {
					unsigned int* size_ptr = (unsigned int*)function::OffsetPointer(target_memory, sizeof(void*));
					*size_ptr = capacity->size;
				}

				if (target_capacity) {
					unsigned int* capacity_ptr = (unsigned int*)function::OffsetPointer(target_memory, sizeof(void*) + sizeof(unsigned int));
					*capacity_ptr = capacity->capacity;
				}

				previous_size = capacity->size;
			}

			void CopyTarget() {
				if (!is_resizable) {
					if (previous_size == capacity->size) {
						if (target_size_t_size) {
							capacity->size = *(size_t*)function::OffsetPointer(target_memory, sizeof(void*));
						}
						else if (target_uint_size) {
							capacity->size = *(unsigned int*)function::OffsetPointer(target_memory, sizeof(void*));
						}

						if (target_capacity) {
							capacity->capacity = *(unsigned int*)function::OffsetPointer(target_memory, sizeof(void*) + sizeof(unsigned int));
						}
					}
				}
				else {
					auto resize = [=](unsigned int new_count) {
						resizable->size *= element_byte_size;
						resizable->capacity *= element_byte_size;
						resizable->ResizeNoCopy(new_count * element_byte_size);
						resizable->capacity /= element_byte_size;
						resizable->size /= element_byte_size;

						memcpy(resizable->buffer, *target_memory, new_count * element_byte_size);
					};

					if (target_capacity) {
						unsigned int current_capacity = *(unsigned int*)function::OffsetPointer(target_memory, sizeof(void*) + sizeof(unsigned int));
						if (current_capacity != resizable->capacity) {
							resize(current_capacity);
						}
					}

					size_t size = resizable->size;
					if (previous_size == capacity->size) {
						if (target_size_t_size) {
							size = *(size_t*)function::OffsetPointer(target_memory, sizeof(void*));
						}
						else if (target_uint_size) {
							size = *(unsigned int*)function::OffsetPointer(target_memory, sizeof(void*));
						}

						if (size > resizable->capacity) {
							resize(size);
						}
						resizable->size = size;
					}
				}
			}

			union {
				ResizableStream<void>* resizable;
				CapacityStream<void>* capacity;
			};
			// This is the address of the field of the reflected type
			void** target_memory;
			bool target_size_t_size : 1;
			bool target_uint_size : 1;
			bool target_capacity : 1;
			bool is_resizable : 1;
			unsigned short element_byte_size;
			// This is the size of the resizable/capacity that was recorded before
			// Calling Update(). It is used to determine if the change was done from the UI
			// or from the C++ side.
			unsigned int previous_size;
		};

		struct UIReflectionStreamBaseData {
			UIInstanceFieldStream stream;
			const char* name;
		};

		typedef UIReflectionStreamBaseData UIReflectionDirectoryInputData;
		typedef UIReflectionStreamBaseData UIReflectionFileInputData;
		typedef UIReflectionStreamBaseData UIReflectionTextInputData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamFloatInputData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamDoubleInputData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamIntInputData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamTextInputData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamColorData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamColorFloatData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamCheckBoxData;

		// The first field must always be the pointer to the field
		struct UIReflectionStreamComboBoxData {
			UIReflectionStreamBaseData base_data;
			Stream<const char*>* label_names;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionStreamInputGroupData {
			UIReflectionStreamBaseData base_data;
			unsigned int basic_type_count;
		};

		typedef UIReflectionStreamBaseData UIReflectionStreamDirectoryInputData;

		typedef UIReflectionStreamBaseData UIReflectionStreamFileInputData;

		struct UIReflectionStreamUserDefinedData {
			UIReflectionStreamBaseData base_data;
			const char* type_name;
			UIReflectionDrawer* ui_drawer;
		};

#define UI_INT_BASE (55)
#define UI_INT8_T ((size_t)1 << UI_INT_BASE)
#define UI_UINT8_T ((size_t)1 << (UI_INT_BASE + 1))
#define UI_INT16_T ((size_t)1 << (UI_INT_BASE + 2))
#define UI_UINT16_T ((size_t)1 << (UI_INT_BASE + 3))
#define UI_INT32_T ((size_t)1 << (UI_INT_BASE + 4))
#define UI_UINT32_T ((size_t)1 << (UI_INT_BASE + 5))
#define UI_INT64_T ((size_t)1 << (UI_INT_BASE + 6))
#define UI_UINT64_T ((size_t)1 << (UI_INT_BASE + 7))

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

		inline size_t ExtractIntFlags(size_t configuration_flags) {
			return (configuration_flags & 0xFF00000000000000) /*>> 55*/;
		}

		inline size_t RemoveIntFlags(size_t configuration_flags) {
			// Keep the highest bit for the resizable stream bit
			return configuration_flags & 0x80FFFFFFFFFFFFFF;
		}

		inline bool IsStream(UIReflectionStreamType type) {
			return type == UIReflectionStreamType::Capacity || type == UIReflectionStreamType::Resizable;
		}

		inline bool IsStreamInput(UIReflectionIndex index) {
			return index == UIReflectionIndex::TextInput || index == UIReflectionIndex::FileInput || index == UIReflectionIndex::DirectoryInput;
		}

		size_t GetTypeByteSize(UIReflectionType type, unsigned int field_index) {
			switch (type.fields[field_index].reflection_index) {
			case UIReflectionIndex::FloatInput:
			case UIReflectionIndex::FloatSlider:
				return sizeof(float);
			case UIReflectionIndex::DoubleInput:
			case UIReflectionIndex::DoubleSlider:
				return sizeof(double);
			case UIReflectionIndex::IntegerInput:
			case UIReflectionIndex::IntegerSlider:
			{
				if (type.fields[field_index].stream_type == UIReflectionStreamType::None) {
					UIReflectionIntInputData* data = (UIReflectionIntInputData*)type.fields[field_index].data;
					return data->byte_size;
				}
				else {
					size_t int_index = ExtractIntFlags(type.fields[field_index].configuration) >> UI_INT_BASE & (~0x1);
					switch (int_index) {
					case 0:
						return 1;
					case 2:
						return 2;
					case 4:
						return 4;
					case 6:
						return 8;
					}
					ECS_ASSERT(false);
					return 0;
				}
			}
			break;
			case UIReflectionIndex::CheckBox:
				return sizeof(bool);
			case UIReflectionIndex::Color:
				return sizeof(Color);
			case UIReflectionIndex::ColorFloat:
				return sizeof(ColorFloat);
			case UIReflectionIndex::ComboBox:
				return sizeof(unsigned char);
			case UIReflectionIndex::TextInput:
				return sizeof(CapacityStream<char>);
			case UIReflectionIndex::FloatInputGroup:
			case UIReflectionIndex::FloatSliderGroup:
			case UIReflectionIndex::DoubleInputGroup:
			case UIReflectionIndex::DoubleSliderGroup:
			case UIReflectionIndex::IntegerInputGroup:
			case UIReflectionIndex::IntegerSliderGroup:
			{
				if (type.fields[field_index].stream_type == UIReflectionStreamType::None) {
					UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)type.fields[field_index].data;
					return data->byte_size;
				}
				else {
					UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)type.fields[field_index].data;
					size_t base_count = data->basic_type_count;
					switch (type.fields[field_index].reflection_index) {
					case UIReflectionIndex::FloatInputGroup:
					case UIReflectionIndex::FloatSliderGroup:
						return base_count * sizeof(float);
					case UIReflectionIndex::DoubleInputGroup:
					case UIReflectionIndex::DoubleSliderGroup:
						return base_count * sizeof(double);
					case UIReflectionIndex::IntegerInputGroup:
					case UIReflectionIndex::IntegerSliderGroup:
						{
							size_t int_index = ExtractIntFlags(type.fields[field_index].configuration) >> UI_INT_BASE & (~0x1);
							switch (int_index) {
							case 0:
								return 1;
							case 2:
								return 2;
							case 4:
								return 4;
							case 6:
								return 8;
							}
							ECS_ASSERT(false);
							return 0;
						}
					}

					ECS_ASSERT(false);
					return 0;
				}
			}
			}

			ECS_ASSERT(false);
			return 0;
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

		void UIReflectionDrawField(
			UIDrawer& drawer,
			UIDrawConfig& config, 
			size_t configuration, 
			void* data,
			UIReflectionIndex reflection_index,
			UIReflectionStreamType stream_type
		) {
			if (stream_type == UIReflectionStreamType::None) {
				UI_REFLECTION_FIELD_BASIC_DRAW[(unsigned int)reflection_index](
					drawer,
					config,
					configuration,
					data
				);
			}
			else {
				UIReflectionStreamBaseData* base_data = (UIReflectionStreamBaseData*)data;
				base_data->stream.CopyTarget();

				UI_REFLECTION_FIELD_STREAM_DRAW[(unsigned int)reflection_index](
					drawer,
					config,
					configuration,
					data
				);

				base_data->stream.WriteTarget();
				drawer.NextRow(-1.0f);
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

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_DO_NOT_CACHE;
			configuration |= (data->lower_bound != -FLT_MAX && data->upper_bound != FLT_MAX) ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
			drawer.FloatInput(
				configuration,
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

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_DO_NOT_CACHE;
			configuration |= (data->lower_bound != -DBL_MAX && data->upper_bound != DBL_MAX) ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
			drawer.DoubleInput(
				configuration, 
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

			//data->stream.CopyTarget();
			drawer.TextInput(configuration | UI_CONFIG_DO_NOT_CACHE, config, data->name, (CapacityStream<char>*)data->stream.capacity);
			//data->stream.WriteTarget();
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

		void UIReflectionDirectoryInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionDirectoryInputData* data = (UIReflectionDirectoryInputData*)_data;

			data->stream.CopyTarget();
			drawer.DirectoryInput(configuration | UI_CONFIG_DO_NOT_CACHE, config, data->name, (CapacityStream<wchar_t>*)data->stream.capacity);
			data->stream.WriteTarget();
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionFileInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionFileInputData* data = (UIReflectionFileInputData*)_data;

			data->stream.CopyTarget();
			drawer.FileInput(configuration | UI_CONFIG_DO_NOT_CACHE, config, data->name, (CapacityStream<wchar_t>*)data->stream.capacity);
			data->stream.WriteTarget();
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

		const void* InputGroupGetBoundOrDefaultPointer(const void* pointer) {
			bool* has = (bool*)function::OffsetPointer(pointer, -1);
			return *has ? pointer : nullptr;
		}

		void UIReflectionFloatInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)_data;

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_DO_NOT_CACHE;

			const float* lower_bound = (const float*)InputGroupGetBoundOrDefaultPointer(data->lower_bound);
			const float* upper_bound = (const float*)InputGroupGetBoundOrDefaultPointer(data->upper_bound);
			const float* default_values = (const float*)InputGroupGetBoundOrDefaultPointer(data->default_values);
			drawer.FloatInputGroup(
				configuration,
				config,
				data->count,
				data->group_name,
				data->input_names,
				data->values,
				default_values,
				lower_bound,
				upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)_data;

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_DO_NOT_CACHE;

			const double* lower_bound = (const double*)InputGroupGetBoundOrDefaultPointer(data->lower_bound);
			const double* upper_bound = (const double*)InputGroupGetBoundOrDefaultPointer(data->upper_bound);
			const double* default_values = (const double*)InputGroupGetBoundOrDefaultPointer(data->default_values);

			drawer.DoubleInputGroup(
				configuration,
				config,
				data->count,
				data->group_name,
				data->input_names,
				data->values,
				default_values,
				lower_bound,
				upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)_data;
			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

			const void* lower_bound = InputGroupGetBoundOrDefaultPointer(data->lower_bound);
			const void* upper_bound = InputGroupGetBoundOrDefaultPointer(data->upper_bound);
			const void* default_values = InputGroupGetBoundOrDefaultPointer(data->default_values);

#define GROUP(integer_type) drawer.IntInputGroup<integer_type>( \
			configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_DEFAULT, \
			config, data->count, data->group_name, data->input_names, (integer_type**)data->values, (const integer_type*)default_values, (const integer_type*)lower_bound, (const integer_type*)upper_bound); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, GROUP);

#undef GROUP
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionUserDefined(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)_data;
			UIReflectionInstance* instance = data->ui_drawer->GetInstancePtr(data->instance_name);

			// The name is stored with a suffix in order to avoid hash table collisions.
			// Put a \0 before the suffix so the name will appear as just the field name
			char* mutable_name = (char*)instance->name;
			char* string_pattern = strstr(mutable_name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
			*string_pattern = '\0';

			data->ui_drawer->DrawInstance(instance, drawer, config);
			// Reset the string pattern
			string_pattern[0] = ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR;
		}

		// -------------------------------------------------------------  Basic ----------------------------------------------------------

		// ------------------------------------------------------------- Stream ----------------------------------------------------------

#define STREAM_SELECT_RESIZABLE(function_name, cast_type) if (data->stream.is_resizable) { \
			drawer.function_name(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, (ResizableStream<cast_type>*)data->stream.resizable, configuration, &config); \
		} \
		else { \
			drawer.function_name(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, (CapacityStream<cast_type>*)data->stream.capacity, configuration, &config); \
		}

		void UIReflectionStreamFloatInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionStreamFloatInputData* data = (UIReflectionStreamFloatInputData*)_data;
			UIDrawConfig temp_config;

			STREAM_SELECT_RESIZABLE(ArrayFloat, float);
			//drawer.ArrayFloat(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamDoubleInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamDoubleInputData* data = (UIReflectionStreamDoubleInputData*)_data;
			UIDrawConfig temp_config;

			STREAM_SELECT_RESIZABLE(ArrayDouble, double);
			//drawer.ArrayDouble(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamIntInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamIntInputData* data = (UIReflectionStreamIntInputData*)_data;
			UIDrawConfig temp_config;

			size_t int_flags = ExtractIntFlags(configuration);
			configuration = RemoveIntFlags(configuration);

#define FUNCTION(integer_type) if (data->stream.is_resizable) { \
				drawer.ArrayInteger<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, (ResizableStream<integer_type>*)data->stream.resizable, \
					configuration, &config); break; \
			} \
			else { \
				drawer.ArrayInteger<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, (CapacityStream<integer_type>*)data->stream.capacity, \
					configuration, &config); break; \
			}

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, FUNCTION);

#undef FUNCTION
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamTextInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamTextInputData* data = (UIReflectionStreamTextInputData*)_data;
			UIDrawConfig temp_config;

			STREAM_SELECT_RESIZABLE(ArrayTextInput, CapacityStream<char>);
			//drawer.ArrayTextInput(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamColor(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamColorData* data = (UIReflectionStreamColorData*)_data;
			UIDrawConfig temp_config;
			
			STREAM_SELECT_RESIZABLE(ArrayColor, Color);
			//drawer.ArrayColor(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamColorFloat(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionStreamColorFloatData* data = (UIReflectionStreamColorFloatData*)_data;
			UIDrawConfig temp_config;
			
			STREAM_SELECT_RESIZABLE(ArrayColorFloat, ColorFloat);
			//drawer.ArrayColorFloat(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamCheckBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamCheckBoxData* data = (UIReflectionStreamCheckBoxData*)_data;
			UIDrawConfig temp_config;

			STREAM_SELECT_RESIZABLE(ArrayCheckBox, bool);
			//drawer.ArrayCheckBox(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, data->values, configuration, &config);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamComboBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data) {
			UIReflectionStreamComboBoxData* data = (UIReflectionStreamComboBoxData*)_data;
			UIDrawConfig temp_config;
			CapacityStream<Stream<const char*>> label_names(data->label_names, data->base_data.stream.capacity->size, data->base_data.stream.capacity->capacity);

			if (data->base_data.stream.is_resizable) {
				drawer.ArrayComboBox(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, (ResizableStream<unsigned char>*)data->base_data.stream.resizable, label_names, configuration, &config);
			}
			else {
				drawer.ArrayComboBox(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, (CapacityStream<unsigned char>*)data->base_data.stream.capacity, label_names, configuration, &config);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamDirectoryInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionStreamDirectoryInputData* data = (UIReflectionStreamDirectoryInputData*)_data;
			UIDrawConfig temp_config;

			STREAM_SELECT_RESIZABLE(ArrayDirectoryInput, CapacityStream<wchar_t>);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamFileInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* _data)
		{
			UIReflectionStreamFileInputData* data = (UIReflectionStreamFileInputData*)_data;
			UIDrawConfig temp_config;

			if (data->stream.is_resizable) {
				drawer.ArrayFileInput(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, (ResizableStream<CapacityStream<wchar_t>>*)data->stream.resizable, { nullptr, 0 }, configuration, &config);
			}
			else {
				drawer.ArrayFileInput(UI_CONFIG_DO_NOT_CACHE, temp_config, data->name, (CapacityStream<CapacityStream<wchar_t>>*)data->stream.capacity, { nullptr, 0 }, configuration, &config);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------


#define FLOAT_DOUBLE_CASE(function_name, cast_type) if (data->base_data.stream.is_resizable) { \
			drawer.function_name(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, (ResizableStream<cast_type>*)data->base_data.stream.resizable, configuration, &config); \
		} \
		else { \
			drawer.function_name(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, (CapacityStream<cast_type>*)data->base_data.stream.capacity, configuration, &config); \
		}

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
				if (data->base_data.stream.is_resizable) { \
					if (data->basic_type_count == 2) { \
						drawer.ArrayInteger2Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, data->base_data.stream.resizable, configuration, &config); \
					} \
					else if (data->basic_type_count == 3) { \
						drawer.ArrayInteger3Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, data->base_data.stream.resizable, configuration, &config); \
					} \
					else { \
						drawer.ArrayInteger4Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, data->base_data.stream.resizable, configuration, &config); \
					} \
				} \
				else { \
					if (data->basic_type_count == 2) { \
						drawer.ArrayInteger2Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, data->base_data.stream.capacity, configuration, &config); \
					} \
					else if (data->basic_type_count == 3) { \
						drawer.ArrayInteger3Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, data->base_data.stream.capacity, configuration, &config); \
					} \
					else { \
						drawer.ArrayInteger4Infer<integer_type>(UI_CONFIG_DO_NOT_CACHE, temp_config, data->base_data.name, data->base_data.stream.capacity, configuration, &config); \
					} \
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

			field->configuration |= field->reflection_index == UIReflectionIndex::FloatInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionDoubleSliderData*)field->data;
			field_ptr->lower_bound = *(double*)data;

			field->configuration |= field->reflection_index == UIReflectionIndex::DoubleInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionIntSliderData*)field->data;
			memcpy(field_ptr->lower_bound, data, field_ptr->byte_size);

			field->configuration |= field->reflection_index == UIReflectionIndex::IntegerInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetGroupLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionGroupData<void>*)field->data;
			memcpy((void*)field_ptr->lower_bound, data, field_ptr->byte_size * field_ptr->count);

			bool* has_lower_bound = (bool*)function::OffsetPointer(field_ptr->lower_bound, -1);
			*has_lower_bound = true;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Upper bound

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetFloatUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionFloatSliderData*)field->data;
			field_ptr->upper_bound = *(float*)data;

			field->configuration |= field->reflection_index == UIReflectionIndex::FloatInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionDoubleSliderData*)field->data;
			field_ptr->upper_bound = *(double*)data;

			field->configuration |= field->reflection_index == UIReflectionIndex::DoubleInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionIntSliderData*)field->data;
			memcpy(field_ptr->upper_bound, data, field_ptr->byte_size);

			field->configuration |= field->reflection_index == UIReflectionIndex::IntegerInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetGroupUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionGroupData<void>*)field->data;
			memcpy((void*)field_ptr->upper_bound, data, field_ptr->byte_size * field_ptr->count);

			bool* has_upper_bound = (bool*)function::OffsetPointer(field_ptr->upper_bound, -1);
			*has_upper_bound = true;
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

			bool* has_default = (bool*)function::OffsetPointer(field_ptr->default_values, -1);
			*has_default = true;
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
			ECS_ASSERT(function::IsPowerOfTwo(type_table_count));
			ECS_ASSERT(function::IsPowerOfTwo(instance_table_count));
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

		void UIReflectionDrawer::AssignInstanceResizableAllocator(const char* instance_name, AllocatorPolymorphic allocator, bool allocate_inputs)
		{
			AssignInstanceResizableAllocator(GetInstancePtr(instance_name), allocator, allocate_inputs);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::AssignInstanceResizableAllocator(UIReflectionInstance* instance, AllocatorPolymorphic allocator, bool allocate_inputs)
		{
			const size_t INPUTS_ALLOCATION_SIZE = 256;

			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < type.fields.size; index++) {
				if (type.fields[index].reflection_index == UIReflectionIndex::DirectoryInput || type.fields[index].reflection_index == UIReflectionIndex::FileInput
					|| type.fields[index].reflection_index == UIReflectionIndex::TextInput) 
				{
					UIReflectionStreamBaseData* data = (UIReflectionStreamBaseData*)instance->data[index];
					if (data->stream.is_resizable) {
						if (data->stream.resizable->allocator.allocator != nullptr) {
							data->stream.resizable->FreeBuffer();
						}
						else {
							data->stream.resizable->buffer = nullptr;
							data->stream.resizable->size = 0;
							data->stream.resizable->capacity = 0;
						}
						data->stream.resizable->allocator = allocator;
						data->stream.is_resizable = true;
						data->stream.WriteTarget();
					}
					else if (allocate_inputs) {
						data->stream.capacity->Initialize(allocator, INPUTS_ALLOCATION_SIZE);
						data->stream.WriteTarget();
					}
				}
				else if (type.fields[index].stream_type == UIReflectionStreamType::Resizable) {
					UIReflectionStreamBaseData* data = (UIReflectionStreamBaseData*)instance->data[index];
					
					if (data->stream.resizable->allocator.allocator != nullptr) {
						data->stream.resizable->FreeBuffer();
					}
					else {
						data->stream.resizable->buffer = nullptr;
						data->stream.resizable->size = 0;
						data->stream.resizable->capacity = 0;
					}
					data->stream.resizable->allocator = allocator;
					data->stream.is_resizable = true;
					data->stream.WriteTarget();
				}
			}
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

#define POINTERS(type) type* data = (type*)field_data; type* instance_data = (type*)instance->data[field_index];

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
				ECS_ASSERT(false, "Decide what to do here");

				//POINTERS(UIReflectionTextInputData);
				//instance_data->stream.capacity = data->characters;
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

		void UIReflectionDrawer::ConvertTypeResizableStream(const char* type_name, Stream<const char*> field_names)
		{
			ConvertTypeResizableStream(GetTypePtr(type_name), field_names);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ConvertTypeResizableStream(UIReflectionType* type, Stream<const char*> field_names)
		{
			for (size_t index = 0; index < field_names.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(*type, field_names[index]);
				ECS_ASSERT(type->fields[field_index].stream_type == UIReflectionStreamType::Capacity);
				type->fields[field_index].stream_type = UIReflectionStreamType::Resizable;

				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)type->fields[field_index].data;
				field_stream->is_resizable = true;
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

		UIInstanceFieldStream UIReflectionDrawerCreateCapacityUIInstanceFieldStream(
			UIReflectionDrawer* drawer, 
			ReflectionStreamFieldType stream_type, 
			uintptr_t ptr,
			unsigned int basic_type_count,
			unsigned int element_byte_size
		) {
			UIInstanceFieldStream field_value;

			field_value.target_memory = (void**)ptr;
			field_value.target_capacity = false;
			field_value.target_size_t_size = false;
			field_value.target_uint_size = false;
			field_value.is_resizable = false;
			// Resizable streams can be safely aliased with capacity streams since the layout is identical, except the resizable stream has an allocator at the end
			if (stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream) {
				field_value.capacity = (CapacityStream<void>*)ptr;
				// The fields target capacity and target uint size should not be set because they will alias directly the memory. 
				// No need to mirror
			}
			else {
				if (stream_type == ReflectionStreamFieldType::Pointer || stream_type == ReflectionStreamFieldType::Stream)
				{
					CapacityStream<void>* allocation = (CapacityStream<void>*)drawer->allocator->Allocate(sizeof(CapacityStream<void>));
					field_value.capacity = allocation;

					allocation->buffer = *(void**)ptr;
					allocation->size = 0;
					allocation->capacity = 0;

					field_value.target_size_t_size = stream_type == ReflectionStreamFieldType::Stream;
				}
				else if (stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					CapacityStream<void>* allocation = (CapacityStream<void>*)drawer->allocator->Allocate(sizeof(CapacityStream<void>));
					field_value.capacity = allocation;

					allocation->buffer = *(void**)ptr;
					allocation->size = 0;
					allocation->capacity = basic_type_count;
				}
			}

			field_value.element_byte_size = element_byte_size;
			field_value.CopyTarget();
			return field_value;
		}

		void UIReflectionDrawer::BindInstancePtrs(UIReflectionInstance* instance, void* data, ReflectionType reflect)
		{
			UIReflectionType type = GetType(instance->type_name);
			uintptr_t ptr = (uintptr_t)data;

			for (size_t index = 0; index < type.fields.size; index++) {
				ptr = (uintptr_t)data + type.fields[index].pointer_offset;

				UIReflectionIndex reflected_index = type.fields[index].reflection_index;

				bool is_stream = IsStream(type.fields[index].stream_type);

				if (!is_stream) {
					bool is_group = reflected_index == UIReflectionIndex::IntegerInputGroup || reflected_index == UIReflectionIndex::IntegerSliderGroup
						|| reflected_index == UIReflectionIndex::FloatInputGroup || reflected_index == UIReflectionIndex::FloatSliderGroup
						|| reflected_index == UIReflectionIndex::DoubleInputGroup || reflected_index == UIReflectionIndex::DoubleSliderGroup;

					if (!is_group) {
						void** reinterpretation = (void**)instance->data[index];
						*reinterpretation = (void*)ptr;

						switch (reflected_index) {
						case UIReflectionIndex::IntegerInput:
						case UIReflectionIndex::IntegerSlider:
						{
							UIReflectionIntInputData* field_data = (UIReflectionIntInputData*)instance->data[index];
							memcpy(field_data->default_value, field_data->value_to_modify, field_data->byte_size);
						}
						break;
						case UIReflectionIndex::FloatInput:
						case UIReflectionIndex::FloatSlider:
						{
							UIReflectionFloatInputData* field_data = (UIReflectionFloatInputData*)instance->data[index];
							field_data->default_value = *field_data->value;
						}
						break;
						case UIReflectionIndex::DoubleInput:
						case UIReflectionIndex::DoubleSlider:
						{
							UIReflectionDoubleInputData* field_data = (UIReflectionDoubleInputData*)instance->data[index];
							field_data->default_value = *field_data->value;
						}
						break;
						case UIReflectionIndex::Color:
						{
							UIReflectionColorData* field_data = (UIReflectionColorData*)instance->data[index];
							field_data->default_color = *field_data->color;
						}
						break;
						case UIReflectionIndex::ColorFloat:
						{
							UIReflectionColorFloatData* field_data = (UIReflectionColorFloatData*)instance->data[index];
							field_data->default_color = *field_data->color;
						}
						break;
						case UIReflectionIndex::ComboBox:
						{
							UIReflectionComboBoxData* field_data = (UIReflectionComboBoxData*)instance->data[index];
							field_data->default_label = *field_data->active_label;
						}
						break;
						case UIReflectionIndex::DirectoryInput:
						case UIReflectionIndex::FileInput:
						case UIReflectionIndex::TextInput:
						{
							ReflectionStreamFieldType reflect_stream_type = reflect.fields[index].info.stream_type;
							UIInstanceFieldStream* field_value = (UIInstanceFieldStream*)instance->data[index];
							*field_value = UIReflectionDrawerCreateCapacityUIInstanceFieldStream(
								this, 
								reflect_stream_type,
								ptr, 
								reflect.fields[index].info.basic_type_count,
								field_value->element_byte_size
							);
						}
						break;
						}
					}
					else {
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance->data[index];
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
					ReflectionStreamFieldType reflect_stream_type = reflect.fields[index].info.stream_type;

					// There are other structs that contain this as the first member variable
					UIInstanceFieldStream* field_value = (UIInstanceFieldStream*)instance->data[index];

					if (type.fields[index].stream_type == UIReflectionStreamType::Capacity) {
						*field_value = UIReflectionDrawerCreateCapacityUIInstanceFieldStream(
							this, 
							reflect_stream_type,
							ptr, 
							reflect.fields[index].info.basic_type_count,
							field_value->element_byte_size
						);
					}
					else {
						field_value->target_capacity = false;
						field_value->target_size_t_size = false;
						field_value->target_uint_size = false;
						field_value->target_memory = (void**)ptr;

						// The resizable stream can be safely aliased
						if (reflect_stream_type == ReflectionStreamFieldType::ResizableStream) {
							field_value->resizable = (ResizableStream<void>*)ptr;
							// No need to mirror the results, keep the boolean bit fields cleared
						}
						// Only pointers and streams can be used with this - basic type array doesn't make sense since
						// its capacity is bound at compile time
						else if (reflect_stream_type == ReflectionStreamFieldType::Pointer || reflect_stream_type == ReflectionStreamFieldType::CapacityStream
							|| reflect_stream_type == ReflectionStreamFieldType::Stream) {
							field_value->resizable = (ResizableStream<void>*)allocator->Allocate(sizeof(ResizableStream<void>));
							field_value->resizable->buffer = nullptr;
							field_value->resizable->size = 0;
							field_value->resizable->capacity = 0;
							field_value->resizable->allocator = { nullptr };
							
							bool set_capacity = reflect_stream_type == ReflectionStreamFieldType::CapacityStream;
							field_value->target_size_t_size = reflect_stream_type == ReflectionStreamFieldType::Stream;
							field_value->target_uint_size = set_capacity;
							field_value->target_capacity = set_capacity;

							// Write the stream now
							field_value->WriteTarget();
						}
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
					CapacityStream<char>** input_data = (CapacityStream<char>**)instance->data[field_index];
					*input_data = data[index].stream;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceDirectoryInput(const char* instance_name, Stream<UIReflectionBindDirectoryInput> data)
		{
			BindInstanceDirectoryInput(GetInstancePtr(instance_name), data);
		}

		void UIReflectionDrawer::BindInstanceDirectoryInput(UIReflectionInstance* instance, Stream<UIReflectionBindDirectoryInput> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (type.fields[field_index].reflection_index == UIReflectionIndex::DirectoryInput) {
					CapacityStream<wchar_t>** input_data = (CapacityStream<wchar_t>**)instance->data[field_index];
					*input_data = data[index].stream;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceFileInput(const char* instance_name, Stream<UIReflectionBindFileInput> data)
		{
			BindInstanceFileInput(GetInstancePtr(instance_name), data);
		}

		void UIReflectionDrawer::BindInstanceFileInput(UIReflectionInstance* instance, Stream<UIReflectionBindFileInput> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (type.fields[field_index].reflection_index == UIReflectionIndex::FileInput) {
					CapacityStream<wchar_t>** input_data = (CapacityStream<wchar_t>**)instance->data[field_index];
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
				ECS_ASSERT(IsStream(type.fields[field_index].stream_type) || IsStreamInput(type.fields[field_index].reflection_index));

				// It is ok to alias the resizable stream with the capacity stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->capacity = data[index].capacity;
				field_stream->WriteTarget();
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
				ECS_ASSERT(IsStream(type.fields[field_index].stream_type));

				// It is ok to alias the capacity stream with the resizable stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->size = data[index].capacity;
				field_stream->WriteTarget();
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
				ECS_ASSERT(IsStream(type.fields[field_index].stream_type));

				// It is ok to alias the resizable stream with the capacity stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->buffer = data[index].new_buffer;
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceResizableStreamAllocator(const char* instance_name, Stream<UIReflectionBindResizableStreamAllocator> data)
		{
			BindInstanceResizableStreamAllocator(GetInstancePtr(instance_name), data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceResizableStreamAllocator(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamAllocator> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				ECS_ASSERT(type.fields[field_index].stream_type == UIReflectionStreamType::Resizable);

				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				if (field_stream->resizable->allocator.allocator != nullptr) {
					field_stream->resizable->FreeBuffer();
				}
				else {
					field_stream->resizable->buffer = nullptr;
					field_stream->resizable->size = 0;
					field_stream->resizable->capacity = 0;
				}
				field_stream->resizable->allocator = data[index].allocator;
				field_stream->is_resizable = true;
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceResizableStreamData(const char* instance_name, Stream<UIReflectionBindResizableStreamData> data)
		{
			BindInstanceResizableStreamData(GetInstancePtr(instance_name), data);
		}

		void UIReflectionDrawer::BindInstanceResizableStreamData(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamData> data)
		{
			UIReflectionType type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				ECS_ASSERT(type.fields[field_index].stream_type == UIReflectionStreamType::Resizable);

				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				size_t stream_byte_size = GetTypeByteSize(type, field_index);

				if (data[index].data.buffer != nullptr) {
					size_t resize_count = data[index].data.size * stream_byte_size;
					field_stream->resizable->ResizeNoCopy(resize_count);
					field_stream->resizable->capacity /= stream_byte_size;
					memcpy(field_stream->resizable->buffer, data[index].data.buffer, resize_count);

					field_stream->resizable->size = data[index].data.size;
				}
				else {
					field_stream->resizable->size *= stream_byte_size;
					field_stream->resizable->Resize(data[index].data.size * stream_byte_size);
					field_stream->resizable->capacity /= stream_byte_size;

					field_stream->resizable->size = data[index].data.size;
				}
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ConvertTypeStreamsToResizable(const char* type_name)
		{
			ConvertTypeStreamsToResizable(GetTypePtr(type_name));
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ConvertTypeStreamsToResizable(UIReflectionType* type)
		{
			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].stream_type == UIReflectionStreamType::Capacity) {
					type->fields[index].stream_type = UIReflectionStreamType::Resizable;
				}
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

			for (size_t index = 0; index < instance->data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (IsStream(type.fields[field_index].stream_type)) {
					// Get the byte size from the original reflected type
					size_t byte_size = GetTypeByteSize(type, field_index);

					UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
					// This is fine even for resizable streams, they have the same first 3 field layout
					memcpy(data[index].destination, field_stream->capacity->buffer, field_stream->capacity->size * byte_size);
					data[index].element_count = field_stream->capacity->size;
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
				// Remove the number range flag if present for the configuration
				type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
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
				// Remove the number range flag if present for the configuration
				type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
			}
			break;// Remove the number range flag if present for the configuration
			type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
			// integer_convert input needs no correction, except reflection_index
			case UIReflectionIndex::IntegerInput:
				type->fields[field_index].reflection_index = UIReflectionIndex::IntegerSlider;
				// Remove the number range flag if present for the configuration
				type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
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
			case UIReflectionIndex::FileInput:
			case UIReflectionIndex::DirectoryInput:
			{
				ECS_ASSERT(false, "Decide what to do here");

				//POINTERS(UIReflectionTextInputData);
				//instance_data->characters = data->characters;
			}
			break;
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

		void UIReflectionDrawer::ChangeDirectoryToFile(const char* type_name, const char* field_name)
		{
			ChangeDirectoryToFile(GetTypePtr(type_name), field_name);
		}

		void UIReflectionDrawer::ChangeDirectoryToFile(UIReflectionType* type, const char* field_name)
		{
			unsigned int field_index = GetTypeFieldIndex(*type, field_name);
			ECS_ASSERT(type->fields[field_index].reflection_index == UIReflectionIndex::DirectoryInput);
			type->fields[field_index].reflection_index = UIReflectionIndex::FileInput;
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

		// Embbeds in front of each buffer a boolean indicating whether or not the buffer should be used or not
		void CreateTypeInputGroupSetBuffers(UIReflectionGroupData<void>* data, uintptr_t ptr) {
			bool* has_default_values = (bool*)ptr;
			*has_default_values = false;

			ptr += 1;
			data->default_values = (const void*)ptr;
			ptr += data->byte_size * data->count;

			bool* has_lower_bound = (bool*)ptr;
			*has_lower_bound = false;
			ptr += 1;

			data->lower_bound = (const void*)ptr;
			ptr += data->byte_size * data->count;

			bool* has_upper_bound = (bool*)ptr;
			*has_upper_bound = false;
			ptr += 1;

			data->upper_bound = (const void*)ptr;
		}

		void ConvertGroupForType(
			UIReflectionDrawer* reflection, 
			ReflectionField reflection_field,
			UIReflectionTypeField& field,
			unsigned int type_byte_size,
			UIReflectionIndex reflection_index,
			size_t extra_configuration = 0
		) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)reflection->allocator->Allocate(sizeof(UIReflectionGroupData<void>));
			field.data = data;
			field.reflection_index = reflection_index;
			field.byte_size = sizeof(*data);
			data->group_name = reflection_field.name;

			data->count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
			data->input_names = BasicTypeNames;
			data->byte_size = type_byte_size;

			// defaults
			void* allocation = reflection->allocator->Allocate(data->byte_size * 3 * data->count + sizeof(bool) * 3);
			uintptr_t ptr = (uintptr_t)allocation;

			// Embbed in front of each buffer a boolean telling whether or not the buffer can be used or not
			data->values = nullptr;
			CreateTypeInputGroupSetBuffers(data, ptr);

			field.stream_type = UIReflectionStreamType::None;
			field.configuration = extra_configuration;
		};

		void ConvertStreamSingleForType(
			UIReflectionDrawer* reflection, 
			ReflectionField reflection_field, 
			UIReflectionTypeField& field, 
			UIReflectionIndex reflection_index,
			size_t extra_configuration = 0
		) {
			UIReflectionStreamBaseData* data = (UIReflectionStreamBaseData*)reflection->allocator->Allocate(sizeof(UIReflectionStreamBaseData));
			memset(data, 0, sizeof(*data));

			field.data = data;
			field.reflection_index = reflection_index;
			field.byte_size = sizeof(*data);

			data->name = reflection_field.name;
			data->stream.element_byte_size = reflection_field.info.stream_byte_size;
			data->stream.is_resizable = false;

			field.stream_type = UIReflectionStreamType::Capacity;
			field.configuration = extra_configuration;
		}

		void ConvertStreamGroupForType(
			UIReflectionDrawer* reflection,
			ReflectionField reflection_field,
			UIReflectionTypeField& field,
			UIReflectionIndex reflection_index,
			size_t extra_configuration = 0
		) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)reflection->allocator->Allocate(sizeof(UIReflectionStreamInputGroupData));
			memset(data, 0, sizeof(*data));

			field.data = data;
			field.reflection_index = reflection_index;
			field.byte_size = sizeof(*data);

			data->base_data.name = reflection_field.name;
			data->basic_type_count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
			data->base_data.stream.element_byte_size = reflection_field.info.stream_byte_size;

			field.stream_type = UIReflectionStreamType::Capacity;
			field.configuration = extra_configuration;
		}

		UIReflectionType* UIReflectionDrawer::CreateType(ReflectionType reflected_type)
		{
			auto integer_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionIntInputData* data = (UIReflectionIntInputData*)allocator->Allocate(sizeof(UIReflectionIntInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::IntegerInput;
				field.byte_size = sizeof(*data);
				data->name = reflection_field.name;
				data->byte_size = reflection_field.info.byte_size;

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
				ConvertGroupForType(
					this, 
					reflection_field,
					field, 
					1 << ((unsigned int)reflection_field.info.basic_type / 2),
					UIReflectionIndex::IntegerInputGroup, 
					(size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type)
				);
			};

			auto float_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)allocator->Allocate(sizeof(UIReflectionFloatInputData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::FloatInput;
				field.byte_size = sizeof(*data);
				data->name = reflection_field.name;

				// defaults
				data->default_value = 0.0f;
				data->lower_bound = -FLT_MAX;
				data->upper_bound = FLT_MAX;
				data->value = nullptr;

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto float_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertGroupForType(this, reflection_field, field, sizeof(float), UIReflectionIndex::FloatInputGroup);
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
				data->lower_bound = -DBL_MAX;
				data->upper_bound = DBL_MAX;

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto double_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertGroupForType(this, reflection_field, field, sizeof(double), UIReflectionIndex::DoubleInputGroup);
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
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::TextInput);
				field.stream_type = UIReflectionStreamType::None;
			};

			auto directory_input_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::DirectoryInput);
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

			auto user_defined_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)allocator->Allocate(sizeof(UIReflectionUserDefinedData));
				field.data = data;
				field.reflection_index = UIReflectionIndex::UserDefined;

				ReflectionType user_defined_type = reflection->GetType(reflection_field.definition);
				field.byte_size = Reflection::GetReflectionTypeByteSize(reflection, user_defined_type);

				// +1 for '\0'
				size_t string_size = strlen(reflection_field.definition) + ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT + strlen(reflection_field.name) + 1;
				// The instance name is of form - field_name##definition
				char* name = (char*)allocator->Allocate(string_size * sizeof(char), alignof(char));
				Stream<char> stream_name = { name, 0 };
				stream_name.AddStream(ToStream(reflection_field.name));
				stream_name.AddStream(ToStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT));
				stream_name.AddStream(ToStream(reflection_field.definition));
				stream_name[stream_name.size] = '\0';

				// Just the definition must be remembered here in the type.
				// The actual instance name will be remembered when the instance is created
				data->instance_name = name;
				data->ui_drawer = this;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;

				// If the type doesn't exist in the UI reflection, then create it
				if (GetTypePtr(reflection_field.definition) == nullptr) {
					CreateType(user_defined_type);
				}
			};

			auto integer_stream_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::IntegerInput, (size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type));
			};

			auto integer_stream_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamGroupForType(this, reflection_field, field, UIReflectionIndex::IntegerInputGroup, (size_t)1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type));
			};

			auto float_stream_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::FloatInput);
			};

			auto float_stream_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamGroupForType(this, reflection_field, field, UIReflectionIndex::FloatInputGroup);
			};

			auto double_stream_convert_single = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::DoubleInput);
			};

			auto double_stream_convert_group = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamGroupForType(this, reflection_field, field, UIReflectionIndex::DoubleInputGroup);
			};

			auto enum_stream_convert = [this](ReflectionEnum reflection_enum, UIReflectionTypeField& field) {
				UIReflectionStreamComboBoxData* data = (UIReflectionStreamComboBoxData*)allocator->Allocate(sizeof(UIReflectionStreamComboBoxData));
				memset(data, 0, sizeof(*data));

				field.data = data;
				field.reflection_index = UIReflectionIndex::ComboBox;
				field.byte_size = sizeof(*data);

				data->label_names = nullptr;
				data->base_data.name = reflection_enum.name;
				data->base_data.stream.element_byte_size = sizeof(char);

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Capacity;
			};

			auto text_input_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::TextInput);
			};

			auto directory_input_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::DirectoryInput);
			};

			auto bool_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::CheckBox);
			};

			auto color_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::Color);
			};

			auto color_float_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionIndex::ColorFloat);
			};

			auto user_defined_stream_convert = [this](ReflectionField reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamUserDefinedData* data = (UIReflectionStreamUserDefinedData*)allocator->Allocate(sizeof(UIReflectionStreamUserDefinedData));
				memset(data, 0, sizeof(*data));

				field.data = data;
				field.reflection_index = UIReflectionIndex::UserDefined;
				field.byte_size = sizeof(*data);

				data->ui_drawer = this;
				data->base_data.stream.element_byte_size = reflection_field.info.stream_byte_size;
				data->base_data.name = reflection_field.name;

				Stream<char> user_defined_type = GetUserDefinedTypeFromStreamUserDefined(reflection_field.definition, reflection_field.info.stream_type);
				Stream<char> allocated_type = function::StringCopy(GetAllocatorPolymorphic(allocator), user_defined_type);
				data->type_name = allocated_type.buffer;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Capacity;

				// If the user defined type doesn't exist in the UI, then create it
				if (GetTypePtr(data->type_name)) {
					CreateType(reflection->GetType(data->type_name));
				}
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
				else if (field_info.basic_type == ReflectionBasicFieldType::UserDefined) {
					user_defined_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
			};

			auto test_stream_type = [&](unsigned int index, ReflectionFieldInfo field_info, bool& value_written) {
				Stream<char> definition_stream = ToStream(reflected_type.fields[index].definition);
				if (function::CompareStrings(definition_stream, ToStream("Color"))) {
					color_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (function::CompareStrings(definition_stream, ToStream("ColorFloat"))) {
					color_float_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (function::CompareStrings(definition_stream, ToStream("Stream<CapacityStream<char>>")) 
					|| function::CompareStrings(definition_stream, ToStream("CapacityStream<CapacityStream<char>>"))
					|| function::CompareStrings(definition_stream, ToStream("ResizableStream<CapacityStream<char>>"))
				) {
					text_input_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (function::CompareStrings(definition_stream, ToStream("Stream<CapacityStream<wchar_t>>"))
					|| function::CompareStrings(definition_stream, ToStream("CapacityStream<CapacityStream<wchar_t>>"))
					|| function::CompareStrings(definition_stream, ToStream("ResizableStream<CapacityStream<wchar_t>"))
				) {
					directory_input_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				// text input - default for char and unsigned char
				else if (field_info.basic_type == ReflectionBasicFieldType::Int8 || field_info.basic_type == ReflectionBasicFieldType::UInt8) {
					text_input_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Wchar_t) {
					directory_input_convert(reflected_type.fields[index], type.fields[type.fields.size]);
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
				else if (field_info.basic_type == ReflectionBasicFieldType::UserDefined) {
					user_defined_stream_convert(reflected_type.fields[index], type.fields[type.fields.size]);
					value_written = true;
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
			UIReflectionType type = GetType(type_name);
			return CreateInstance(name, &type);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::CreateInstance(const char* name, const UIReflectionType* type)
		{
			{
				ECS_RESOURCE_IDENTIFIER(name);
				ECS_ASSERT(instances.Find(identifier) == -1);
			}

			UIReflectionInstance instance;
			instance.type_name = type->name;

			size_t name_length = strlen(name) + 1;
			size_t total_memory = name_length + sizeof(void*) * type->fields.size;

			for (size_t index = 0; index < type->fields.size; index++) {
				total_memory += type->fields[index].byte_size;
			}

			void* allocation = allocator->Allocate(total_memory);
			uintptr_t buffer = (uintptr_t)allocation;
			buffer += sizeof(void*) * type->fields.size;
			instance.data.buffer = (void**)allocation;

			for (size_t index = 0; index < type->fields.size; index++) {
				instance.data[index] = (void*)buffer;
				buffer += type->fields[index].byte_size;
				memcpy(instance.data[index], type->fields[index].data, type->fields[index].byte_size);

				if (type->fields[index].stream_type == UIReflectionStreamType::None) {
					switch (type->fields[index].reflection_index) {
					case UIReflectionIndex::DoubleInputGroup:
					case UIReflectionIndex::DoubleSliderGroup:
					case UIReflectionIndex::FloatInputGroup:
					case UIReflectionIndex::FloatSliderGroup:
					case UIReflectionIndex::IntegerInputGroup:
					case UIReflectionIndex::IntegerSliderGroup:
					{
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance.data[index];
						data->values = (void**)allocator->Allocate(sizeof(void*) * data->count);
					}
					break;
					case UIReflectionIndex::UserDefined:
					{
						UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)instance.data[index];
						// Must create an instance of that user defined type here
						ECS_STACK_CAPACITY_STREAM(char, instance_name, 512);
						// Append to the instance_name the current instance's name
						instance_name.Copy(ToStream(data->instance_name));
						instance_name.AddStreamSafe(Stream<char>(name, name_length));
						instance_name[instance_name.size] = '\0';

						// In order to determine the type name, just look for the separator from the type
						// and whatever its after it will be the type
						const char* type_name = strstr(data->instance_name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
						type_name += ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT;

						CreateInstance(instance_name.buffer, type_name);
					}
						break;
					}
				}
			}

			char* instance_name = (char*)buffer;
			memcpy(instance_name, name, name_length * sizeof(char));
			instance.name = instance_name;

			instance.data.size = type->fields.size;

			ECS_RESOURCE_IDENTIFIER(instance.name);
			ECS_ASSERT(!instances.Insert(instance, identifier));
			return instances.GetValuePtr(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

#pragma region CreateForHierarchy helpers

		void CreateForHierarchyCheckIncludeIndex(
			ReflectionType type, 
			size_t& include_index, 
			UIReflectionDrawerSearchOptions options
		) {
			if (options.include_tags.size > 0) {
				include_index = 0;
				for (; include_index < options.include_tags.size; include_index++) {
					if (type.HasTag(options.include_tags[include_index])) {
						break;
					}
				}

				// If it got to the last, increase such that it will be bigger than the size
				include_index += include_index == options.include_tags.size;
			}
		}

		void CreateForHierarchyCheckExcludeIndex(
			ReflectionType type,
			size_t& exclude_index,
			UIReflectionDrawerSearchOptions options
		) {
			if (options.exclude_tags.size > 0) {
				exclude_index = 0;
				for (; exclude_index < options.exclude_tags.size; exclude_index++) {
					if (type.HasTag(options.exclude_tags[exclude_index])) {
						// Exit the loop, after it will be -1
						exclude_index = -2;
					}
				}
			}
		}

		bool CreateForHierarchyIsValid(size_t include_index, size_t exclude_index, UIReflectionDrawerSearchOptions options) {
			return exclude_index == options.exclude_tags.size && include_index <= options.include_tags.size;
		}

		bool CreateForHierarchyVerifyIncludeExclude(ReflectionType type, UIReflectionDrawerSearchOptions options) {
			size_t include_index = options.include_tags.size;
			size_t exclude_index = options.exclude_tags.size;

			CreateForHierarchyCheckExcludeIndex(type, include_index, options);
			CreateForHierarchyCheckIncludeIndex(type, exclude_index, options);

			return CreateForHierarchyIsValid(include_index, exclude_index, options);
		}

		void CreateForHierarchyGetSuffixName(CapacityStream<char>& full_name, const char*& name, UIReflectionDrawerSearchOptions options) {
			if (options.suffix != nullptr) {
				full_name.Copy(ToStream(name));
				full_name.AddStreamSafe(ToStream(options.suffix));
				full_name[full_name.size] = '\0';

				name = full_name.buffer;
			}
		}

		void CreateForHierarchyAddIndex(unsigned int index, UIReflectionDrawerSearchOptions options) {
			if (options.indices != nullptr) {
				options.indices->AddSafe(index);
			}
		}

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::CreateTypesForHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options)
		{
			unsigned int count = 0;
			reflection->type_definitions.ForEach([&](const ReflectionType& type, ResourceIdentifier identifier) {
				if (type.folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(type, options)) {
						UIReflectionType* ui_type = CreateType(type);
						unsigned int type_index = ui_type - type_definition.m_values;
						CreateForHierarchyAddIndex(type_index, options);
						count++;
					}
				}
			});

			return count;
		}

		unsigned int UIReflectionDrawer::CreateTypesForHierarchy(const wchar_t* hierarchy, UIReflectionDrawerSearchOptions options)
		{
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				return CreateTypesForHierarchy(hierarchy_index, options);
			}
			ECS_ASSERT(false);
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::CreateInstanceForHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options)
		{
			unsigned int count = 0;
			type_definition.ForEachIndexConst([&](unsigned int index) {
				UIReflectionType ui_type = type_definition.GetValueFromIndex(index);
				ReflectionType type = reflection->GetType(ui_type.name);
				if (type.folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(type, options)) {
						ECS_STACK_CAPACITY_STREAM(char, full_name, 512);

						const char* instance_name = ui_type.name;
						CreateForHierarchyGetSuffixName(full_name, instance_name, options);

						CreateInstance(instance_name, &ui_type);
						CreateForHierarchyAddIndex(instances.Find(instance_name), options);
						count++;
					}
				}
			});

			return count;
		}

		unsigned int UIReflectionDrawer::CreateInstanceForHierarchy(const wchar_t* hierarchy, UIReflectionDrawerSearchOptions options)
		{
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				return CreateInstanceForHierarchy(hierarchy_index, options);
			}
			ECS_ASSERT(false);
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::CreateTypesAndInstancesForHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options) {
			unsigned int count = 0;

			reflection->type_definitions.ForEachIndexConst([&](unsigned int index) {
				ReflectionType type = reflection->type_definitions.GetValueFromIndex(index);
				if (type.folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(type, options)) {
						UIReflectionType* ui_type = CreateType(type);
						UIReflectionInstance* instance = CreateInstance(ui_type->name, ui_type);

						unsigned int instance_index = instance - instances.m_values;
						CreateForHierarchyAddIndex(instance_index, options);
						count++;
					}
				}
			});

			return count;
		}

		unsigned int UIReflectionDrawer::CreateTypesAndInstancesForHierarchy(const wchar_t* hierarchy, UIReflectionDrawerSearchOptions options) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				return CreateTypesAndInstancesForHierarchy(hierarchy_index, options);
			}
			ECS_ASSERT(false);
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyInstance(unsigned int index) {
			UIReflectionInstance instance = instances.GetValueFromIndex(index);

			UIReflectionType type = GetType(instance.type_name);
			// The individual instance.data[index] must not be deallocated
			// since they were coallesced into a single allocation
			for (size_t index = 0; index < type.fields.size; index++) {
				bool is_stream = IsStream(type.fields[index].stream_type);
				if (!is_stream) {
					switch (type.fields[index].reflection_index) {
					case UIReflectionIndex::IntegerInputGroup:
					case UIReflectionIndex::IntegerSliderGroup:
					case UIReflectionIndex::DoubleInputGroup:
					case UIReflectionIndex::DoubleSliderGroup:
					case UIReflectionIndex::FloatInputGroup:
					case UIReflectionIndex::FloatSliderGroup:
					{
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance.data[index];
						allocator->Deallocate(data->values);
					}
					break;
					case UIReflectionIndex::FileInput:
					case UIReflectionIndex::DirectoryInput:
					case UIReflectionIndex::TextInput:
					{
						UIInstanceFieldStream* data = (UIInstanceFieldStream*)instance.data[index];
						if (data->capacity != (CapacityStream<void>*)data->target_memory) {
							allocator->Deallocate(data->capacity);
						}
					}
					break;
					}
				}
				else {
					// It is fine to alias the resizable stream with the capacity stream
					if (type.fields[index].stream_type == UIReflectionStreamType::Resizable || type.fields[index].stream_type == UIReflectionStreamType::Capacity) {
						// All the structs contain this as the first member variable
						// Safe to alias with UIInstanceFieldStream

						UIInstanceFieldStream* data = (UIInstanceFieldStream*)instance.data[index];
						// if a buffer is allocated, deallocate it only for non aliasing streams
						if (type.fields[index].stream_type == UIReflectionStreamType::Resizable && data->resizable != (ResizableStream<void>*)data->target_memory) {
							if (data->resizable->buffer != nullptr) {
								data->resizable->FreeBuffer();
							}
						}

						if (data->capacity != (CapacityStream<void>*)data->target_memory) {
							allocator->Deallocate(data->capacity);
						}
					}
				}
			}

			allocator->Deallocate(instance.data.buffer);
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
					// Can compare the pointers directly since the instances will always alias their type's name
					if (type.name == instances_[index].type_name) {
						DestroyInstance(instances_[index].name);
						index--;
					}
				}
			}

			for (size_t index = 0; index < type.fields.size; index++) {
				bool is_stream = IsStream(type.fields[index].stream_type);

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
						// The boolean is prefixing this
						allocator->Deallocate(function::OffsetPointer(data->default_values, -sizeof(bool)));
					}
					break;
					case UIReflectionIndex::UserDefined: 
					{
						UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)type.fields[index].data;
						allocator->Deallocate(data->instance_name);
					}
						break;
					}
				}
				else {
					if (reflect_index == UIReflectionIndex::UserDefined) {
						UIReflectionStreamUserDefinedData* data = (UIReflectionStreamUserDefinedData*)type.fields[index].data;
						allocator->Deallocate(data->type_name);
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
			const char* instance_name,
			UIDrawer& drawer,
			UIDrawConfig& config,
			Stream<UIReflectionDrawConfig> additional_configs,
			const UIReflectionInstanceDrawCustomFunctors* custom_draw,
			const char* default_value_button
		) {
			DrawInstance(GetInstancePtr(instance_name), drawer, config, additional_configs, custom_draw, default_value_button);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DrawInstance(
			UIReflectionInstance* instance,
			UIDrawer& drawer,
			UIDrawConfig& config,
			Stream<UIReflectionDrawConfig> additional_configs,
			const UIReflectionInstanceDrawCustomFunctors* custom_draw,
			const char* default_value_button
		) {
			UIReflectionType type = GetType(instance->type_name);

			ECS_UI_DRAWER_MODE draw_mode = drawer.draw_mode;
			unsigned int draw_mode_target = drawer.draw_mode_target;

			bool pushed_string_identifier = drawer.PushIdentifierStackStringPattern();
			drawer.PushIdentifierStack(instance->name);

			drawer.SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

			if (default_value_button != nullptr) {
				UIReflectionDefaultValueData default_data;
				default_data.instance = *instance;
				default_data.type = type;
				drawer.Button(default_value_button, { UIReflectionDefaultValueAction, &default_data, sizeof(default_data) });
			}

			for (size_t index = 0; index < type.fields.size; index++) {
				if (custom_draw != nullptr) {
					// Check to see if a custom functor is provided for the type
					if (custom_draw->functions[(unsigned char)type.fields[index].reflection_index] != nullptr) {
						UIReflectionIndex reflection_index = type.fields[index].reflection_index;

						bool is_instance_field_stream = type.fields[index].stream_type != UIReflectionStreamType::None || IsStreamInput(reflection_index);
						if (is_instance_field_stream) 
						{
							UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[index];
							field_stream->CopyTarget();
						}

						// Call the custom functor
						// The struct data always contains as the first field the pointer to the data
						// It can be provided directly to the function
						void** field_data = (void**)instance->data[index];
						custom_draw->functions[(unsigned char)type.fields[index].reflection_index](
							drawer,
							config,
							*field_data,
							type.fields[index].name,
							type.fields[index].stream_type,
							custom_draw->extra_data
						);

						if (is_instance_field_stream) {
							// Mirror the stream values
							UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[index];
							field_stream->WriteTarget();
						}
						// Abort the continuation of this loop iteration
						continue;
					}
					// Proceed normally
				}

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

				UIReflectionDrawField(drawer, config, current_configuration, instance->data[index], type.fields[index].reflection_index, type.fields[index].stream_type);

				config.flag_count = config_flag_count;
			}

			if (pushed_string_identifier) {
				drawer.PopIdentifierStack();
			}
			drawer.PopIdentifierStack();

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

		void UIReflectionDrawer::DestroyAllInstancesFromFolderHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options)
		{
			unsigned int extended_capacity = instances.GetExtendedCapacity();
			if (options.suffix == nullptr) {
				instances.ForEachIndex([&](unsigned int index) {
					const UIReflectionInstance* instance = instances.GetValuePtrFromIndex(index);
					if (CreateForHierarchyVerifyIncludeExclude(reflection->GetType(instance->type_name), options)) {
						DestroyInstance(index);
						return true;
					}

					return false;
				});
			}
			else {
				instances.ForEachIndex([&](unsigned int index) {
					const UIReflectionInstance* instance = instances.GetValuePtrFromIndex(index);

					ECS_STACK_CAPACITY_STREAM(char, full_name, 256);
					const char* instance_name = instance->type_name;
					CreateForHierarchyGetSuffixName(full_name, instance_name, options);

					if (function::CompareStrings(full_name, ToStream(instance->name))) {
						if (CreateForHierarchyVerifyIncludeExclude(reflection->GetType(instance->type_name), options)) {
							DestroyInstance(index);
							return true;
						}
					}

					return false;
				});
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyAllInstancesFromFolderHierarchy(const wchar_t* hierarchy, UIReflectionDrawerSearchOptions options)
		{
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				DestroyAllInstancesFromFolderHierarchy(hierarchy_index, options);
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

				// It is fine to alias the resizable stream and the capacity stream
				CapacityStream<void>* field_stream = *(CapacityStream<void>**)instance->data[field_index];
				data[index].capacity = field_stream->size;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetHierarchyTypes(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options) {
			type_definition.ForEachIndexConst([&](unsigned int index) {
				UIReflectionType ui_type = type_definition.GetValueFromIndex(index);
				ReflectionType type = reflection->GetType(ui_type.name);
				if (type.folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(type, options)) {
						options.indices->AddSafe(index);
					}
				}
			});
		}


		void UIReflectionDrawer::GetHierarchyTypes(const wchar_t* hierarchy, UIReflectionDrawerSearchOptions options) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				GetHierarchyTypes(hierarchy_index, options);
				return;
			}
			ECS_ASSERT(false);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetHierarchyInstances(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options) {
			instances.ForEachIndexConst([&](unsigned int index) {
				UIReflectionInstance instance = instances.GetValueFromIndex(index);
				ReflectionType type = reflection->GetType(instance.type_name);
				if (type.folder_hierarchy_index == hierarchy_index && CreateForHierarchyVerifyIncludeExclude(type, options)) {
					if (options.suffix == nullptr) {
						options.indices->AddSafe(index);
					}
					else {
						ECS_STACK_CAPACITY_STREAM(char, full_name, 256);
						const char* instance_name = instance.type_name;
						CreateForHierarchyGetSuffixName(full_name, instance_name, options);

						if (function::CompareStrings(full_name, ToStream(instance.name))) {
							options.indices->AddSafe(index);
						}
					}
				}
			});
		}

		void UIReflectionDrawer::GetHierarchyInstances(const wchar_t* hierarchy, UIReflectionDrawerSearchOptions options) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				GetHierarchyInstances(hierarchy_index, options);
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
			unsigned int index = type_definition.Find(identifier);
			return index == -1 ? nullptr : type_definition.GetValuePtrFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::GetTypePtr(unsigned int index) const
		{
			return type_definition.IsItemAt(index) ? type_definition.GetValuePtrFromIndex(index) : nullptr;
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

		unsigned int UIReflectionDrawer::GetTypeFieldIndex(const char* type_name, const char* field_name)
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

#define CASE_START(type, data_type) case UIReflectionIndex::type: { data_type* data = (data_type*)default_data->instance.data[index]; 
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