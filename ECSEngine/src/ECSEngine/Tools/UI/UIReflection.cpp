#include "ecspch.h"
#include "UIReflection.h"
#include "../../Input/Mouse.h"
#include "../../Input/Keyboard.h"
#include "../../Utilities/Reflection/ReflectionStringFunctions.h"

#define UI_IGNORE_RANGE_OR_PARAMETERS_TAG "_"

namespace ECSEngine {

	using namespace Reflection;

	namespace Tools {

		// ------------------------------------------------------------ Basic ----------------------------------------------------------------------

		void UIReflectionFloatSlider(UIReflectionFieldDrawData* draw_data);

		void UIReflectionDoubleSlider(UIReflectionFieldDrawData* draw_data);

		void UIReflectionIntSlider(UIReflectionFieldDrawData* draw_data);

		void UIReflectionFloatInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionDoubleInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionIntInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionTextInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionColor(UIReflectionFieldDrawData* draw_data);

		void UIReflectionColorFloat(UIReflectionFieldDrawData* draw_data);

		void UIReflectionCheckBox(UIReflectionFieldDrawData* draw_data);

		void UIReflectionComboBox(UIReflectionFieldDrawData* draw_data);

		void UIReflectionDirectoryInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionFileInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionFloatSliderGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionDoubleSliderGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionIntSliderGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionFloatInputGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionDoubleInputGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionIntInputGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionUserDefined(UIReflectionFieldDrawData* draw_data);

		void UIReflectionOverride(UIReflectionFieldDrawData* draw_data);

		// ------------------------------------------------------------ Basic ----------------------------------------------------------------------

		// ------------------------------------------------------------ Stream ----------------------------------------------------------------------

		void UIReflectionStreamFloatInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamDoubleInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamIntInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamTextInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamColor(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamColorFloat(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamCheckBox(UIReflectionFieldDrawData* draw_data);

		// It doesn't make too much sense to have a stream of combo boxes
		void UIReflectionStreamComboBox(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamDirectoryInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamFileInput(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamFloatInputGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamDoubleInputGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamIntInputGroup(UIReflectionFieldDrawData* draw_data);

		void UIReflectionStreamNotImplemented(UIReflectionFieldDrawData* draw_data) {
			ECS_ASSERT(false, "This UIReflection stream field is invalid");
		}

		// ------------------------------------------------------------ Stream ----------------------------------------------------------------------

		typedef void (*UIReflectionSetLowerBound)(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetFloatLowerBound(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetDoubleLowerBound(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetIntLowerBound(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetGroupLowerBound(UIReflectionTypeField* field, const void* data);

		typedef UIReflectionSetLowerBound UIReflectionSetUpperBound;

		void UIReflectionSetFloatUpperBound(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetDoubleUpperBound(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetIntUpperBound(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetGroupUpperBound(UIReflectionTypeField* field, const void* data);

		typedef UIReflectionSetLowerBound UIReflectionSetDefaultData;

		void UIReflectionSetFloatDefaultData(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetDoubleDefaultData(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetIntDefaultData(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetGroupDefaultData(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetColorDefaultData(UIReflectionTypeField* field, const void* data);

		// Currently it does nothing. It serves as a placeholder for the jump table. Maybe add support for it later on.
		void UIReflectionSetTextInputDefaultData(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetColorFloatDefaultData(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetCheckBoxDefaultData(UIReflectionTypeField* field, const void* data);

		void UIReflectionSetComboBoxDefaultData(UIReflectionTypeField* field, const void* data);

		typedef void (*UIReflectionSetRange)(UIReflectionTypeField* field, const void* min, const void* max);

		void UIReflectionSetFloatRange(UIReflectionTypeField* field, const void* min, const void* max);

		void UIReflectionSetDoubleRange(UIReflectionTypeField* field, const void* min, const void* max);

		void UIReflectionSetIntRange(UIReflectionTypeField* field, const void* min, const void* max);

		void UIReflectionSetGroupRange(UIReflectionTypeField* field, const void* min, const void* max);

#pragma region Jump Tables

		UIReflectionFieldDraw UI_REFLECTION_FIELD_BASIC_DRAW[] = {
			UIReflectionFloatSlider,
			UIReflectionDoubleSlider,
			UIReflectionIntSlider,
			UIReflectionFloatInput,
			UIReflectionDoubleInput,
			UIReflectionIntInput,
			UIReflectionFloatSliderGroup,
			UIReflectionDoubleSliderGroup,
			UIReflectionIntSliderGroup,
			UIReflectionFloatInputGroup,
			UIReflectionDoubleInputGroup,
			UIReflectionIntInputGroup,
			UIReflectionTextInput,
			UIReflectionColor,
			UIReflectionColorFloat,
			UIReflectionCheckBox,
			UIReflectionComboBox,
			UIReflectionDirectoryInput,
			UIReflectionFileInput,
			UIReflectionUserDefined,
			UIReflectionOverride
		};

		static_assert((unsigned int)UIReflectionElement::Count == std::size(UI_REFLECTION_FIELD_BASIC_DRAW));

		UIReflectionFieldDraw UI_REFLECTION_FIELD_STREAM_DRAW[] = {
			UIReflectionStreamFloatInput,
			UIReflectionStreamDoubleInput,
			UIReflectionStreamIntInput,
			UIReflectionStreamFloatInput,
			UIReflectionStreamDoubleInput,
			UIReflectionStreamIntInput,
			UIReflectionStreamFloatInputGroup,
			UIReflectionStreamDoubleInputGroup,
			UIReflectionStreamIntInputGroup,
			UIReflectionStreamFloatInputGroup,
			UIReflectionStreamDoubleInputGroup,
			UIReflectionStreamIntInputGroup,
			UIReflectionStreamTextInput,
			UIReflectionStreamColor,
			UIReflectionStreamColorFloat,
			UIReflectionStreamCheckBox,
			UIReflectionStreamComboBox,
			UIReflectionStreamDirectoryInput,
			UIReflectionStreamFileInput,
			UIReflectionStreamNotImplemented,
			UIReflectionStreamNotImplemented
		};

		static_assert((unsigned int)UIReflectionElement::Count == std::size(UI_REFLECTION_FIELD_STREAM_DRAW));

		UIReflectionSetLowerBound UI_REFLECTION_SET_LOWER_BOUND[] = {
			UIReflectionSetFloatLowerBound,
			UIReflectionSetDoubleLowerBound,
			UIReflectionSetIntLowerBound,
			UIReflectionSetFloatLowerBound,
			UIReflectionSetDoubleLowerBound,
			UIReflectionSetIntLowerBound,
			UIReflectionSetGroupLowerBound,
			UIReflectionSetGroupLowerBound,
			UIReflectionSetGroupLowerBound,
			UIReflectionSetGroupLowerBound,
			UIReflectionSetGroupLowerBound,
			UIReflectionSetGroupLowerBound
		};

		UIReflectionSetUpperBound UI_REFLECTION_SET_UPPER_BOUND[] = {
			UIReflectionSetFloatUpperBound,
			UIReflectionSetDoubleUpperBound,
			UIReflectionSetIntUpperBound,
			UIReflectionSetFloatUpperBound,
			UIReflectionSetDoubleUpperBound,
			UIReflectionSetIntUpperBound,
			UIReflectionSetGroupUpperBound,
			UIReflectionSetGroupUpperBound,
			UIReflectionSetGroupUpperBound,
			UIReflectionSetGroupUpperBound,
			UIReflectionSetGroupUpperBound,
			UIReflectionSetGroupUpperBound
		};

		UIReflectionSetDefaultData UI_REFLECTION_SET_DEFAULT_DATA[] = {
			UIReflectionSetFloatDefaultData,
			UIReflectionSetDoubleDefaultData,
			UIReflectionSetIntDefaultData,
			UIReflectionSetFloatDefaultData,
			UIReflectionSetDoubleDefaultData,
			UIReflectionSetIntDefaultData,
			UIReflectionSetGroupDefaultData,
			UIReflectionSetGroupDefaultData,
			UIReflectionSetGroupDefaultData,
			UIReflectionSetGroupDefaultData,
			UIReflectionSetGroupDefaultData,
			UIReflectionSetGroupDefaultData,
			UIReflectionSetTextInputDefaultData,
			UIReflectionSetColorDefaultData,
			UIReflectionSetColorFloatDefaultData,
			UIReflectionSetCheckBoxDefaultData,
			UIReflectionSetComboBoxDefaultData
		};

		UIReflectionSetRange UI_REFLECTION_SET_RANGE[] = {
			UIReflectionSetFloatRange,
			UIReflectionSetDoubleRange,
			UIReflectionSetIntRange,
			UIReflectionSetFloatRange,
			UIReflectionSetDoubleRange,
			UIReflectionSetIntRange,
			UIReflectionSetGroupRange,
			UIReflectionSetGroupRange,
			UIReflectionSetGroupRange,
			UIReflectionSetGroupRange,
			UIReflectionSetGroupRange,
			UIReflectionSetGroupRange
		};

#pragma endregion

		Stream<char> BasicTypeNames[] = {
			"x",
			"y",
			"z",
			"w",
			"t",
			"u",
			"v"
		};

		
		// ------------- Structures for the draw part - They need to have as the first member a pointer to the field value ----------------

		// The data needed by the override will be placed immediately after this
		struct OverrideAllocationData {
			ECS_INLINE void* GetData() {
				return function::OffsetPointer(this, sizeof(*this));
			}

			void* field_data;
			UIReflectionInstanceDrawCustom draw_function;
			unsigned int override_index;
			AllocatorPolymorphic initialize_allocator;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionFloatSliderData {
			float* value_to_modify;
			float lower_bound;
			float upper_bound;
			float default_value;
			unsigned int precision = 2;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionDoubleSliderData {
			double* value_to_modify;
			double lower_bound;
			double upper_bound;
			double default_value;
			unsigned int precision = 2;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionIntSliderData {
			void* value_to_modify;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
			unsigned int int_flags;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionFloatInputData {
			float* value;
			float lower_bound;
			float upper_bound;
			float default_value;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionDoubleInputData {
			double* value;
			double lower_bound;
			double upper_bound;
			double default_value;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionIntInputData {
			void* value_to_modify;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
			unsigned int int_flags;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionColorData {
			Color* color;
			Color default_color;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionColorFloatData {
			ColorFloat* color;
			ColorFloat default_color;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionCheckBoxData {
			bool* value;
			bool default_value;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionComboBoxData {
			unsigned char* active_label;
			Stream<Stream<char>> labels;
			unsigned int label_display_count;
			unsigned char default_label;
		};

		// The first field must always be the pointer to the field
		template<typename Field>
		struct UIReflectionGroupData {
			Field** values;
			Stream<char>* input_names;
			const Field* lower_bound;
			const Field* upper_bound;
			const Field* default_values;
			unsigned int count;
			unsigned int precision = 3;
			unsigned int byte_size;
			unsigned int int_flags;
		};

		// ------------- Structures for the draw part - They need to have as the first member a pointer to the field value ----------------

		struct UIReflectionUserDefinedData {
			// We don't need this pointer, but we need to have one
			// Since, when using BindInstancePtrs, the first field will always
			void* field_pointer;
			
			// This is filled by CreateType
			// It is a combination of field_name##type_name
			Stream<char> type_and_field_name;
			UIReflectionDrawer* ui_drawer;

			// This is filled by CreateInstance
			UIReflectionInstance* instance;
		};

		// All the streams struct contain this as the first member variable
		struct UIInstanceFieldStream {
			// Mirrors the fields inside the stream into the target
			// Needs to be used in conjunction with CopyTarget()
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

			// Needs to be used in conjunction with the WriteTarget. If not using that function, call
			// CopyTargetStandalone()
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
					if (target_capacity) {
						unsigned int current_capacity = *(unsigned int*)function::OffsetPointer(target_memory, sizeof(void*) + sizeof(unsigned int));
						if (current_capacity != resizable->capacity) {
							resizable->ResizeNoCopy(current_capacity, element_byte_size);
							memcpy(resizable->buffer, *target_memory, current_capacity * element_byte_size);
						}
					}

					unsigned int size = resizable->size;
					if (previous_size == capacity->size) {
						if (target_size_t_size) {
							size = *(size_t*)function::OffsetPointer(target_memory, sizeof(void*));
						}
						else if (target_uint_size) {
							size = *(unsigned int*)function::OffsetPointer(target_memory, sizeof(void*));
						}

						if (size > resizable->capacity) {
							resizable->ResizeNoCopy(size, element_byte_size);
							memcpy(resizable->buffer, *target_memory, size * element_byte_size);
						}
						resizable->size = size;
					}
				}
			}

			// Should be used when wanting to keep a separate UI stream from the targeted stream
			void CopyTargetStandalone() {
				unsigned int target_size = 0;
				if (target_size_t_size) {
					target_size = *(size_t*)function::OffsetPointer(target_memory, sizeof(void*));
				}
				else if (target_uint_size) {
					target_size = *(unsigned int*)function::OffsetPointer(target_memory, sizeof(void*));
				}

				if (previous_size == capacity->size) {
					if (!is_resizable) {
						target_size = std::min(target_size, capacity->capacity);

						// The UI side hasn't changed
						// Check the standalone data
						if (standalone_data.size != target_size) {
							// It changed
							capacity->size = target_size;
							memcpy(capacity->buffer, *target_memory, element_byte_size * target_size);
							// Resize the standalone data
							standalone_data.ResizeNoCopy(target_size, element_byte_size);
							standalone_data.size = target_size;
							memcpy(standalone_data.buffer, *target_memory, element_byte_size * target_size);
						}
						else {
							if (memcmp(standalone_data.buffer, *target_memory, element_byte_size * target_size) != 0) {
								// It changed
								capacity->size = target_size;
								memcpy(capacity->buffer, *target_memory, element_byte_size * target_size);

								// Copy the new data
								memcpy(standalone_data.buffer, *target_memory, element_byte_size * target_size);
							}
						}
					}
					else {
						if (target_size != standalone_data.size) {
							// It changed
							resizable->ResizeNoCopy(target_size, element_byte_size);
							memcpy(resizable->buffer, *target_memory, target_size * element_byte_size);
							standalone_data.ResizeNoCopy(target_size, element_byte_size);
							standalone_data.size = target_size;
							memcpy(standalone_data.buffer, *target_memory, target_size * element_byte_size);
						}
						else {
							if (memcmp(standalone_data.buffer, *target_memory, element_byte_size * target_size) != 0) {
								// It changed
								resizable->size = target_size;
								memcpy(resizable->buffer, *target_memory, element_byte_size * target_size);

								// Copy the new data
								memcpy(standalone_data.buffer, *target_memory, element_byte_size * target_size);
							}
						}
					}
				}
				else {
					// The UI has changed
					// But we should not copy into it
				}

				previous_size = capacity->size;
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
			// or from the C++ side. In the CopyTarget and WriteTarget pair there is no need
			// for the standalone data. But when using the standalone copy it is needed because
			// there is no way to make the distinction when the data is changed on the C++ side
			// or UI side.
			unsigned int previous_size;
			ResizableStream<void> standalone_data;
		};

		struct UIReflectionStreamBaseData {
			UIInstanceFieldStream stream;
		};

		typedef UIReflectionStreamBaseData UIReflectionDirectoryInputData;
		typedef UIReflectionStreamBaseData UIReflectionFileInputData;
		typedef UIReflectionStreamBaseData UIReflectionTextInputData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamFloatInputData;

		// The first field must always be the pointer to the field
		typedef UIReflectionStreamBaseData UIReflectionStreamDoubleInputData;

		// The first field must always be the pointer to the field
		struct UIReflectionStreamIntInputData {
			UIReflectionStreamBaseData stream;
			unsigned int int_flags;
		};

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
			Stream<Stream<char>>* label_names;
		};

		// The first field must always be the pointer to the field
		struct UIReflectionStreamInputGroupData {
			UIReflectionStreamBaseData base_data;
			unsigned int basic_type_count;
			unsigned int int_flags;
		};

		typedef UIReflectionStreamBaseData UIReflectionStreamDirectoryInputData;

		typedef UIReflectionStreamBaseData UIReflectionStreamFileInputData;

		struct UIReflectionStreamUserDefinedData {
			UIReflectionStreamBaseData base_data;
			Stream<char> type_name;
			UIReflectionDrawer* ui_drawer;
		};

#define UI_INT_BASE (0)
#define UI_INT8_T ((unsigned int)1 << UI_INT_BASE)
#define UI_UINT8_T ((unsigned int)1 << (UI_INT_BASE + 1))
#define UI_INT16_T ((unsigned int)1 << (UI_INT_BASE + 2))
#define UI_UINT16_T ((unsigned int)1 << (UI_INT_BASE + 3))
#define UI_INT32_T ((unsigned int)1 << (UI_INT_BASE + 4))
#define UI_UINT32_T ((unsigned int)1 << (UI_INT_BASE + 5))
#define UI_INT64_T ((unsigned int)1 << (UI_INT_BASE + 6))
#define UI_UINT64_T ((unsigned int)1 << (UI_INT_BASE + 7))

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

		ECS_INLINE bool IsStream(UIReflectionStreamType type) {
			return type == UIReflectionStreamType::Capacity || type == UIReflectionStreamType::Resizable;
		}

		ECS_INLINE bool IsStreamInput(UIReflectionElement index) {
			return index == UIReflectionElement::TextInput || index == UIReflectionElement::FileInput || index == UIReflectionElement::DirectoryInput;
		}

		static size_t GetTypeByteSize(const UIReflectionType* type, unsigned int field_index) {
			switch (type->fields[field_index].element_index) {
			case UIReflectionElement::FloatInput:
			case UIReflectionElement::FloatSlider:
				return sizeof(float);
			case UIReflectionElement::DoubleInput:
			case UIReflectionElement::DoubleSlider:
				return sizeof(double);
			case UIReflectionElement::IntegerInput:
			case UIReflectionElement::IntegerSlider:
			{
				if (type->fields[field_index].stream_type == UIReflectionStreamType::None) {
					UIReflectionIntInputData* data = (UIReflectionIntInputData*)type->fields[field_index].data;
					return data->byte_size;
				}
				else {
					UIReflectionStreamIntInputData* data = (UIReflectionStreamIntInputData*)type->fields[field_index].data;
					return data->stream.stream.element_byte_size;
				}
			}
			break;
			case UIReflectionElement::CheckBox:
				return sizeof(bool);
			case UIReflectionElement::Color:
				return sizeof(Color);
			case UIReflectionElement::ColorFloat:
				return sizeof(ColorFloat);
			case UIReflectionElement::ComboBox:
				return sizeof(unsigned char);
			case UIReflectionElement::TextInput:
				return sizeof(CapacityStream<char>);
			case UIReflectionElement::FloatInputGroup:
			case UIReflectionElement::FloatSliderGroup:
			case UIReflectionElement::DoubleInputGroup:
			case UIReflectionElement::DoubleSliderGroup:
			case UIReflectionElement::IntegerInputGroup:
			case UIReflectionElement::IntegerSliderGroup:
			{
				if (type->fields[field_index].stream_type == UIReflectionStreamType::None) {
					UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)type->fields[field_index].data;
					return data->byte_size;
				}
				else {
					UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)type->fields[field_index].data;
					size_t base_count = data->basic_type_count;
					/*switch (type.fields[field_index].element_index) {
					case UIReflectionElement::FloatInputGroup:
					case UIReflectionElement::FloatSliderGroup:
						return base_count * sizeof(float);
					case UIReflectionElement::DoubleInputGroup:
					case UIReflectionElement::DoubleSliderGroup:
						return base_count * sizeof(double);
					case UIReflectionElement::IntegerInputGroup:
					case UIReflectionElement::IntegerSliderGroup:
					}*/
					return base_count * data->base_data.stream.element_byte_size;


					ECS_ASSERT(false);
					return 0;
				}
			}
			}

			ECS_ASSERT(false);
			return 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		// Extracts the name from example WorldDescriptor::entity_pool_size to entity_pool_size such that it can be compared
		// against the field name stored
		static Stream<char> ExtractTypedFieldName(Stream<char> field_name) {
			Stream<char> colon = function::FindFirstCharacter(field_name, ':');
			if (colon.buffer != nullptr) {
				colon.buffer += 2;
				colon.size -= 2;
				return colon;
			}
			return field_name;
		}

		// ------------------------------------------------------------------------------------------------------------------------------
		
		void UIReflectionDrawConfigCopyToNormalConfig(const UIReflectionDrawConfig* ui_config, UIDrawConfig& config) {
			memcpy(config.associated_bits + config.flag_count, ui_config->associated_bits, ui_config->config_count * sizeof(size_t));
			for (size_t index = 0; index < ui_config->config_count; index++) {
				memcpy(config.parameters + config.parameter_start[config.flag_count], ui_config->configs[index], ui_config->config_size[index]);

				size_t parameter_count = ui_config->config_size[index];
				config.parameter_start[config.flag_count + 1] = config.parameter_start[config.flag_count] + parameter_count;
				config.flag_count++;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		size_t UIReflectionDrawConfigSplatCallback(
			Stream<UIReflectionDrawConfig> ui_config, 
			UIActionHandler handler, 
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT splat_type,
			void* stack_memory,
			bool trigger_only_on_release
		)
		{
			size_t index = 0;

			ECS_STACK_CAPACITY_STREAM(size_t, used_indices, 32);
			size_t last_index = 0;

			auto find_index = [&](Stream<UIReflectionElement> indices) {
				size_t count_type = -1;
				for (size_t config_index = 0; config_index < ui_config.size; config_index++) {
					if (ui_config[config_index].index_count > 0) {
						if (function::SearchBytes(indices.buffer, indices.size, (size_t)ui_config[config_index].index[0], sizeof(indices[0])) != -1) {
							count_type = config_index;
							break;
						}
					}
				}

				if (count_type == -1) {
					count_type = index;
				}
				ECS_ASSERT(count_type <= ui_config.size);
				last_index = std::max(last_index, count_type);
				index++;

				return count_type;
			};

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ARRAY_ADD)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::Count
				};
				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index[0] = UIReflectionElement::Count;
				ui_config[count_type].index_count = 1;

				UIConfigArrayAddCallback* add_callback = (UIConfigArrayAddCallback*)stack_memory;
				add_callback->handler = handler;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*add_callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, add_callback);

				if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ARRAY_REMOVE)) {
					UIConfigArrayRemoveCallback remove_callback;
					remove_callback.handler = handler;
					UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, &remove_callback);
					splat_type = (ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT)function::ClearFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ARRAY_REMOVE);
				}
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ARRAY_REMOVE)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::Count
				};
				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index[0] = UIReflectionElement::Count;
				ui_config[count_type].index_count = 1;

				UIConfigArrayRemoveCallback* remove_callback = (UIConfigArrayRemoveCallback*)stack_memory;
				remove_callback->handler = handler;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*remove_callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, remove_callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_CHECK_BOX)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::CheckBox
				};
				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index[0] = UIReflectionElement::CheckBox;
				ui_config[count_type].index_count = 1;
				
				UIConfigCheckBoxCallback* callback = (UIConfigCheckBoxCallback*)stack_memory;
				callback->handler = handler;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_COLOR_FLOAT)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::ColorFloat
				};

				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index[0] = UIReflectionElement::ColorFloat;
				ui_config[count_type].index_count = 1;

				UIConfigColorFloatCallback* callback = (UIConfigColorFloatCallback*)stack_memory;
				callback->callback = handler;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_COLOR_INPUT)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::Color
				};

				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index[0] = UIReflectionElement::Color;
				ui_config[count_type].index_count = 1;

				UIConfigColorInputCallback* callback = (UIConfigColorInputCallback*)stack_memory;
				callback->callback = handler;
				callback->final_callback.action = nullptr;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_COMBO_BOX)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::ComboBox
				};

				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index[0] = UIReflectionElement::ComboBox;
				ui_config[count_type].index_count = 1;

				UIConfigComboBoxCallback* callback = (UIConfigComboBoxCallback*)stack_memory;
				callback->handler = handler;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_NUMBER_INPUTS)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::IntegerInput,
					UIReflectionElement::FloatInput,
					UIReflectionElement::DoubleInput,
					UIReflectionElement::IntegerInputGroup,
					UIReflectionElement::FloatInputGroup,
					UIReflectionElement::DoubleInputGroup
				};

				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index_count = std::size(indices);
				memcpy(ui_config[count_type].index, indices, sizeof(indices));

				UIConfigTextInputCallback* callback = (UIConfigTextInputCallback*)stack_memory;
				callback->handler = handler;
				callback->trigger_only_on_release = trigger_only_on_release;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_SLIDERS)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::IntegerSlider,
					UIReflectionElement::FloatSlider,
					UIReflectionElement::DoubleSlider,
					UIReflectionElement::IntegerSliderGroup,
					UIReflectionElement::FloatSliderGroup,
					UIReflectionElement::DoubleSliderGroup
				};

				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index_count = std::size(indices);
				memcpy(ui_config[count_type].index, indices, sizeof(indices));

				UIConfigSliderChangedValueCallback* callback = (UIConfigSliderChangedValueCallback*)stack_memory;
				callback->handler = handler;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_PATH_INPUT)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::DirectoryInput,
					UIReflectionElement::FileInput
				};

				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index_count = std::size(indices);
				memcpy(ui_config[count_type].index, indices, sizeof(indices));

				UIConfigPathInputCallback* callback = (UIConfigPathInputCallback*)stack_memory;
				callback->callback = handler;
				callback->trigger_on_release = trigger_only_on_release;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			if (function::HasFlag(splat_type, ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_TEXT_INPUT)) {
				UIReflectionElement indices[] = {
					UIReflectionElement::TextInput,
				};

				size_t count_type = find_index({ indices, std::size(indices) });
				ui_config[count_type].index_count = std::size(indices);
				memcpy(ui_config[count_type].index, indices, sizeof(indices));

				UIConfigTextInputCallback* callback = (UIConfigTextInputCallback*)stack_memory;
				callback->handler = handler;
				callback->trigger_only_on_release = trigger_only_on_release;
				stack_memory = function::OffsetPointer(stack_memory, sizeof(*callback));

				UIReflectionDrawConfigAddConfig(ui_config.buffer + count_type, callback);
			}

			return last_index + 1;
		}

		void UIReflectionDrawField(
			UIDrawer& drawer,
			UIDrawConfig& config, 
			size_t configuration,
			Stream<char> name,
			void* data,
			UIReflectionElement element_index,
			UIReflectionStreamType stream_type,
			const UIReflectionDrawInstanceOptions* draw_options
		) {
			UIReflectionFieldDrawData draw_data;
			draw_data.config = &config;
			draw_data.drawer = &drawer;
			draw_data.configuration = configuration;
			draw_data.name = name;
			draw_data.data = data;
			draw_data.draw_options = draw_options;

			if (stream_type == UIReflectionStreamType::None) {
				UI_REFLECTION_FIELD_BASIC_DRAW[(unsigned int)element_index](&draw_data);
			}
			else {
				UIReflectionStreamBaseData* base_data = (UIReflectionStreamBaseData*)data;
				base_data->stream.CopyTarget();

				UI_REFLECTION_FIELD_STREAM_DRAW[(unsigned int)element_index](&draw_data);

				base_data->stream.WriteTarget();
				drawer.NextRow(-1.0f);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		// This macro was created because previously I had the parameters given to the function,
		// Not to the structure and to make porting easier, we can just forward them
#define FORWARD_FIELD_DRAW_DATA		Stream<char> name = draw_data->name; \
									UIDrawConfig& config = *draw_data->config; \
									UIDrawer& drawer = *draw_data->drawer; \
									size_t configuration = draw_data->configuration

		// ------------------------------------------------------------- Basic ----------------------------------------------------------

		void UIReflectionFloatSlider(UIReflectionFieldDrawData* draw_data) {
			UIReflectionFloatSliderData* data = (UIReflectionFloatSliderData*)draw_data->data;

			FORWARD_FIELD_DRAW_DATA;

			drawer.FloatSlider(
				configuration | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE, 
				config,
				name,
				data->value_to_modify,
				data->lower_bound,
				data->upper_bound,
				data->default_value, 
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleSlider(UIReflectionFieldDrawData* draw_data) {
			UIReflectionDoubleSliderData* data = (UIReflectionDoubleSliderData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			drawer.DoubleSlider(
				configuration | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE,
				config, 
				name,
				data->value_to_modify,
				data->lower_bound,
				data->upper_bound, 
				data->default_value,
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntSlider(UIReflectionFieldDrawData* draw_data) {
			UIReflectionIntSliderData* data = (UIReflectionIntSliderData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			unsigned int int_flags = data->int_flags;

#define SLIDER_IMPLEMENTATION(integer_type) drawer.IntSlider<integer_type>(configuration | UI_CONFIG_SLIDER_ENTER_VALUES \
			| UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE, \
			config, name, (integer_type*)data->value_to_modify, *(integer_type*)data->lower_bound, *(integer_type*)data->upper_bound, *(integer_type*)data->default_value); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, SLIDER_IMPLEMENTATION);

#undef SLIDER_IMPLEMENTATION
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionFloatInput(UIReflectionFieldDrawData* draw_data) {
			UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)draw_data->data;			
			FORWARD_FIELD_DRAW_DATA;

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT;
			configuration |= (data->lower_bound != -FLT_MAX && data->upper_bound != FLT_MAX) ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
			drawer.FloatInput(
				configuration,
				config,
				name, 
				data->value, 
				data->default_value,
				data->lower_bound, 
				data->upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleInput(UIReflectionFieldDrawData* draw_data) {
			UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT;
			configuration |= (data->lower_bound != -DBL_MAX && data->upper_bound != DBL_MAX) ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
			drawer.DoubleInput(
				configuration, 
				config, 
				name, 
				data->value, 
				data->default_value, 
				data->lower_bound,
				data->upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntInput(UIReflectionFieldDrawData* draw_data) {
			UIReflectionIntInputData* data = (UIReflectionIntInputData*)draw_data->data;
			unsigned int int_flags = data->int_flags;
			FORWARD_FIELD_DRAW_DATA;

#define INPUT(integer_convert) { integer_convert default_value = *(integer_convert*)data->default_value; \
			integer_convert upper_bound = *(integer_convert*)data->upper_bound; \
		integer_convert lower_bound = *(integer_convert*)data->lower_bound; \
		drawer.IntInput<integer_convert>( \
			configuration | UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER | UI_CONFIG_NUMBER_INPUT_DEFAULT, \
			config,  \
			name, \
			(integer_convert*)data->value_to_modify, \
			default_value, \
			lower_bound,  \
			upper_bound);  \
		} break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, INPUT);

#undef INPUT
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionTextInput(UIReflectionFieldDrawData* draw_data) {
			UIReflectionTextInputData* data = (UIReflectionTextInputData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			bool disable_write = function::HasFlag(configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM);
			configuration = function::ClearFlag(configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM);

			if (!disable_write) {
				data->stream.CopyTarget();
			}
			else {
				data->stream.CopyTargetStandalone();
			}
			drawer.TextInput(configuration, config, name, (CapacityStream<char>*)data->stream.capacity);
			if (!disable_write) {
				data->stream.WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionColor(UIReflectionFieldDrawData* draw_data) {
			UIReflectionColorData* data = (UIReflectionColorData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;
			drawer.ColorInput(configuration | UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE, config, name, data->color, data->default_color);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionColorFloat(UIReflectionFieldDrawData* draw_data) {
			UIReflectionColorFloatData* data = (UIReflectionColorFloatData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;
			drawer.ColorFloatInput(configuration | UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE, config, name, data->color, data->default_color);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionCheckBox(UIReflectionFieldDrawData* draw_data) {
			UIReflectionCheckBoxData* data = (UIReflectionCheckBoxData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;
			drawer.CheckBox(configuration, config, name, data->value);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionComboBox(UIReflectionFieldDrawData* draw_data) {
			UIReflectionComboBoxData* data = (UIReflectionComboBoxData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;
			drawer.ComboBox(configuration, config, name, data->labels, data->label_display_count, data->active_label);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDirectoryInput(UIReflectionFieldDrawData* draw_data)
		{
			UIReflectionDirectoryInputData* data = (UIReflectionDirectoryInputData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			bool disable_write = function::HasFlag(configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM);
			configuration = function::ClearFlag(configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM);

			if (!disable_write) {
				data->stream.CopyTarget();
			}
			else {
				data->stream.CopyTargetStandalone();
			}
			drawer.DirectoryInput(configuration, config, name, (CapacityStream<wchar_t>*)data->stream.capacity);
			if (!disable_write) {
				data->stream.WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionFileInput(UIReflectionFieldDrawData* draw_data)
		{
			UIReflectionFileInputData* data = (UIReflectionFileInputData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			bool disable_write = function::HasFlag(configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM);
			configuration = function::ClearFlag(configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM);

			if (!disable_write) {
				data->stream.CopyTarget();
			}
			else {
				data->stream.CopyTargetStandalone();
			}
			drawer.FileInput(configuration, config, name, (CapacityStream<wchar_t>*)data->stream.capacity);
			if (!disable_write) {
				data->stream.WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionFloatSliderGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			drawer.FloatSliderGroup(
				configuration | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE,
				config,
				data->count, 
				name, 
				data->input_names, 
				data->values, 
				data->lower_bound, 
				data->upper_bound, 
				data->default_values, 
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleSliderGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			drawer.DoubleSliderGroup(
				configuration | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_DEFAULT_VALUE,
				config,
				data->count, 
				name,
				data->input_names,
				data->values, 
				data->lower_bound, 
				data->upper_bound, 
				data->default_values,
				data->precision
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntSliderGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)draw_data->data;
			unsigned int int_flags = data->int_flags;
			FORWARD_FIELD_DRAW_DATA;

#define GROUP(integer_type) drawer.IntSliderGroup<integer_type>(configuration | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE \
			| UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_SLIDER_DEFAULT_VALUE, config, data->count, name, data->input_names, (integer_type**)data->values, (const integer_type*)data->lower_bound, (const integer_type*)data->upper_bound, (const integer_type*)data->default_values); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, GROUP);

#undef GROUP
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		static const void* InputGroupGetBoundOrDefaultPointer(const void* pointer) {
			bool* has = (bool*)function::OffsetPointer(pointer, -1);
			return *has ? pointer : nullptr;
		}

		void UIReflectionFloatInputGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionGroupData<float>* data = (UIReflectionGroupData<float>*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT;

			const float* lower_bound = (const float*)InputGroupGetBoundOrDefaultPointer(data->lower_bound);
			const float* upper_bound = (const float*)InputGroupGetBoundOrDefaultPointer(data->upper_bound);
			const float* default_values = (const float*)InputGroupGetBoundOrDefaultPointer(data->default_values);
			drawer.FloatInputGroup(
				configuration,
				config,
				data->count,
				name,
				data->input_names,
				data->values,
				default_values,
				lower_bound,
				upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDoubleInputGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionGroupData<double>* data = (UIReflectionGroupData<double>*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;

			configuration |= UI_CONFIG_NUMBER_INPUT_DEFAULT;

			const double* lower_bound = (const double*)InputGroupGetBoundOrDefaultPointer(data->lower_bound);
			const double* upper_bound = (const double*)InputGroupGetBoundOrDefaultPointer(data->upper_bound);
			const double* default_values = (const double*)InputGroupGetBoundOrDefaultPointer(data->default_values);

			drawer.DoubleInputGroup(
				configuration,
				config,
				data->count,
				name,
				data->input_names,
				data->values,
				default_values,
				lower_bound,
				upper_bound
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionIntInputGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)draw_data->data;
			unsigned int int_flags = data->int_flags;
			FORWARD_FIELD_DRAW_DATA;

			const void* lower_bound = InputGroupGetBoundOrDefaultPointer(data->lower_bound);
			const void* upper_bound = InputGroupGetBoundOrDefaultPointer(data->upper_bound);
			const void* default_values = InputGroupGetBoundOrDefaultPointer(data->default_values);

#define GROUP(integer_type) drawer.IntInputGroup<integer_type>( \
			configuration | UI_CONFIG_NUMBER_INPUT_DEFAULT, \
			config, data->count, name, data->input_names, (integer_type**)data->values, (const integer_type*)default_values, (const integer_type*)lower_bound, (const integer_type*)upper_bound); break;

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, GROUP);

#undef GROUP
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionUserDefined(UIReflectionFieldDrawData* draw_data)
		{
			UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)draw_data->data;
			UIReflectionInstance* instance = data->instance;

			// The name is stored with a suffix in order to avoid hash table collisions.
			Stream<char> string_pattern = function::FindFirstToken(instance->name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);

			Stream<char> original_instance_name = instance->name;
			instance->name = { instance->name.buffer, function::PointerDifference(string_pattern.buffer, instance->name.buffer) };
			UIReflectionDrawInstanceOptions options;
			if (draw_data->draw_options) {
				options = *draw_data->draw_options;
			}
			else {
				options.drawer = draw_data->drawer;
				options.config = draw_data->config;
			}

			data->ui_drawer->DrawInstance(instance, &options);

			instance->name = original_instance_name;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionOverride(UIReflectionFieldDrawData* draw_data)
		{
			OverrideAllocationData* data = (OverrideAllocationData*)draw_data->data;
			FORWARD_FIELD_DRAW_DATA;
			void* override_data = data->GetData();
			data->draw_function(&drawer, &config, configuration, data->field_data, name, UIReflectionStreamType::None, override_data);
		}

		// -------------------------------------------------------------  Basic ----------------------------------------------------------

		// ------------------------------------------------------------- Stream ----------------------------------------------------------

		static void GetArrayCallback(UIDrawConfig& array_config, size_t& array_configuration, UIDrawConfig& element_config, size_t& element_configuration) {
			if (function::HasFlag(element_configuration, UI_CONFIG_ARRAY_ADD_CALLBACK)) {
				array_configuration |= UI_CONFIG_ARRAY_ADD_CALLBACK;
				element_configuration = function::ClearFlag(element_configuration, UI_CONFIG_ARRAY_ADD_CALLBACK);

				const UIConfigArrayAddCallback* callback = (const UIConfigArrayAddCallback*)element_config.GetParameter(UI_CONFIG_ARRAY_ADD_CALLBACK);
				array_config.AddFlag(*callback);
			}

			if (function::HasFlag(element_configuration, UI_CONFIG_ARRAY_REMOVE_CALLBACK)) {
				array_configuration |= UI_CONFIG_ARRAY_REMOVE_CALLBACK;
				element_configuration = function::ClearFlag(element_configuration, UI_CONFIG_ARRAY_REMOVE_CALLBACK);

				const UIConfigArrayRemoveCallback* callback = (const UIConfigArrayRemoveCallback*)element_config.GetParameter(UI_CONFIG_ARRAY_REMOVE_CALLBACK);
				array_config.AddFlag(*callback);
			}
		}

#define STREAM_SELECT_RESIZABLE(function_name, cast_type) if (data->stream.is_resizable) { \
			drawer.function_name(array_configuration, array_config, name, (ResizableStream<cast_type>*)data->stream.resizable, configuration, &config); \
		} \
		else { \
			drawer.function_name(array_configuration, array_config, name, (CapacityStream<cast_type>*)data->stream.capacity, configuration, &config); \
		}

		void UIReflectionStreamFloatInput(UIReflectionFieldDrawData* draw_data)
		{
			UIReflectionStreamFloatInputData* data = (UIReflectionStreamFloatInputData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			STREAM_SELECT_RESIZABLE(ArrayFloat, float);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamDoubleInput(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamDoubleInputData* data = (UIReflectionStreamDoubleInputData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			STREAM_SELECT_RESIZABLE(ArrayDouble, double);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamIntInput(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamIntInputData* data = (UIReflectionStreamIntInputData*)draw_data->data;
			unsigned int int_flags = data->int_flags;
			FORWARD_FIELD_DRAW_DATA;

			UIDrawConfig array_config;
			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);

#define FUNCTION(integer_type) if (data->stream.stream.is_resizable) { \
				drawer.ArrayInteger<integer_type>(array_configuration, array_config, name, (ResizableStream<integer_type>*)data->stream.stream.resizable, \
					configuration, &config); break; \
			} \
			else { \
				drawer.ArrayInteger<integer_type>(array_configuration, array_config, name, (CapacityStream<integer_type>*)data->stream.stream.capacity, \
					configuration, &config); break; \
			}

			ECS_TOOLS_UI_REFLECT_INT_SWITCH_TABLE(int_flags, FUNCTION);

#undef FUNCTION
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamTextInput(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamTextInputData* data = (UIReflectionStreamTextInputData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			STREAM_SELECT_RESIZABLE(ArrayTextInput, CapacityStream<char>);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamColor(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamColorData* data = (UIReflectionStreamColorData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			STREAM_SELECT_RESIZABLE(ArrayColor, Color);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamColorFloat(UIReflectionFieldDrawData* draw_data)
		{
			UIReflectionStreamColorFloatData* data = (UIReflectionStreamColorFloatData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			STREAM_SELECT_RESIZABLE(ArrayColorFloat, ColorFloat);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamCheckBox(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamCheckBoxData* data = (UIReflectionStreamCheckBoxData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			STREAM_SELECT_RESIZABLE(ArrayCheckBox, bool);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamComboBox(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamComboBoxData* data = (UIReflectionStreamComboBoxData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);

			CapacityStream<Stream<Stream<char>>> label_names(data->label_names, data->base_data.stream.capacity->size, data->base_data.stream.capacity->capacity);

			if (data->base_data.stream.is_resizable) {
				drawer.ArrayComboBox(
					array_configuration,
					array_config, 
					name,
					(ResizableStream<unsigned char>*)data->base_data.stream.resizable,
					label_names,
					configuration,
					&config
				);
			}
			else {
				drawer.ArrayComboBox(
					array_configuration,
					array_config, 
					name, 
					(CapacityStream<unsigned char>*)data->base_data.stream.capacity, 
					label_names, 
					configuration,
					&config
				);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamDirectoryInput(UIReflectionFieldDrawData* draw_data)
		{
			UIReflectionStreamDirectoryInputData* data = (UIReflectionStreamDirectoryInputData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			STREAM_SELECT_RESIZABLE(ArrayDirectoryInput, CapacityStream<wchar_t>);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamFileInput(UIReflectionFieldDrawData* draw_data)
		{
			UIReflectionStreamFileInputData* data = (UIReflectionStreamFileInputData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
			if (data->stream.is_resizable) {
				drawer.ArrayFileInput(
					array_configuration, 
					array_config, 
					name, 
					(ResizableStream<CapacityStream<wchar_t>>*)data->stream.resizable,
					{ nullptr, 0 },
					configuration, 
					&config
				);
			}
			else {
				drawer.ArrayFileInput(
					array_configuration,
					array_config,
					name, 
					(CapacityStream<CapacityStream<wchar_t>>*)data->stream.capacity,
					{ nullptr, 0 },
					configuration,
					&config
				);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------


#define FLOAT_DOUBLE_CASE(function_name, cast_type) if (data->base_data.stream.is_resizable) { \
			drawer.function_name(array_configuration, array_config, name, (ResizableStream<cast_type>*)data->base_data.stream.resizable, configuration, &config); \
		} \
		else { \
			drawer.function_name(array_configuration, array_config, name, (CapacityStream<cast_type>*)data->base_data.stream.capacity, configuration, &config); \
		}

		void UIReflectionStreamFloatInputGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
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

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionStreamDoubleInputGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)draw_data->data;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);
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

		void UIReflectionStreamIntInputGroup(UIReflectionFieldDrawData* draw_data) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)draw_data->data;
			unsigned int int_flags = data->int_flags;
			UIDrawConfig array_config;
			FORWARD_FIELD_DRAW_DATA;

			size_t array_configuration = 0;
			GetArrayCallback(array_config, array_configuration, config, configuration);

			ECS_ASSERT(2 <= data->basic_type_count && data->basic_type_count <= 4);

#define GROUP(integer_type) { \
				if (data->base_data.stream.is_resizable) { \
					if (data->basic_type_count == 2) { \
						drawer.ArrayInteger2Infer<integer_type>(array_configuration, array_config, name, data->base_data.stream.resizable, configuration, &config); \
					} \
					else if (data->basic_type_count == 3) { \
						drawer.ArrayInteger3Infer<integer_type>(array_configuration, array_config, name, data->base_data.stream.resizable, configuration, &config); \
					} \
					else { \
						drawer.ArrayInteger4Infer<integer_type>(array_configuration, array_config, name, data->base_data.stream.resizable, configuration, &config); \
					} \
				} \
				else { \
					if (data->basic_type_count == 2) { \
						drawer.ArrayInteger2Infer<integer_type>(array_configuration, array_config, name, data->base_data.stream.capacity, configuration, &config); \
					} \
					else if (data->basic_type_count == 3) { \
						drawer.ArrayInteger3Infer<integer_type>(array_configuration, array_config, name, data->base_data.stream.capacity, configuration, &config); \
					} \
					else { \
						drawer.ArrayInteger4Infer<integer_type>(array_configuration, array_config, name, data->base_data.stream.capacity, configuration, &config); \
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

			field->configuration |= field->element_index == UIReflectionElement::FloatInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionDoubleSliderData*)field->data;
			field_ptr->lower_bound = *(double*)data;

			field->configuration |= field->element_index == UIReflectionElement::DoubleInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntLowerBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionIntSliderData*)field->data;
			memcpy(field_ptr->lower_bound, data, field_ptr->byte_size);

			field->configuration |= field->element_index == UIReflectionElement::IntegerInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
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

			field->configuration |= field->element_index == UIReflectionElement::FloatInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetDoubleUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionDoubleSliderData*)field->data;
			field_ptr->upper_bound = *(double*)data;

			field->configuration |= field->element_index == UIReflectionElement::DoubleInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionSetIntUpperBound(UIReflectionTypeField* field, const void* data)
		{
			auto field_ptr = (UIReflectionIntSliderData*)field->data;
			memcpy(field_ptr->upper_bound, data, field_ptr->byte_size);

			field->configuration |= field->element_index == UIReflectionElement::IntegerInput ? UI_CONFIG_NUMBER_INPUT_RANGE : 0;
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
			overrides.Initialize(GetAllocatorPolymorphic(allocator), 0);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::AddTypeConfiguration(Stream<char> type_name, Stream<char> field_name, size_t field_configuration)
		{
			UIReflectionType* type_def = GetType(type_name);
			AddTypeConfiguration(type_def, field_name, field_configuration);
		}

		void UIReflectionDrawer::AddTypeConfiguration(UIReflectionType* type, Stream<char> field_name, size_t field_configuration)
		{

			unsigned int field_index = GetTypeFieldIndex(type, field_name);
			type->fields[field_index].configuration |= field_configuration;

#if 1
			// get a mask with the valid configurations
			size_t mask = 0;
			switch (type->fields[field_index].element_index) {
			case UIReflectionElement::FloatSlider:
			case UIReflectionElement::DoubleSlider:
			case UIReflectionElement::IntegerSlider:
				mask = UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
				break;
			case UIReflectionElement::Color:
				mask = UI_CONFIG_COLOR_INPUT_CALLBACK;
				break;
			case UIReflectionElement::DoubleInput:
			case UIReflectionElement::FloatInput:
			case UIReflectionElement::IntegerInput:
				mask = 0;
				break;
			case UIReflectionElement::TextInput:
				mask = UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_CALLBACK;
				break;
			case UIReflectionElement::FloatSliderGroup:
			case UIReflectionElement::DoubleSliderGroup:
			case UIReflectionElement::IntegerSliderGroup:
				mask = UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
				break;
			case UIReflectionElement::FloatInputGroup:
			case UIReflectionElement::DoubleInputGroup:
			case UIReflectionElement::IntegerInputGroup:
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

		void UIReflectionDrawer::AddTypeGrouping(UIReflectionType* type, UIReflectionTypeFieldGrouping grouping)
		{
			// At the moment there is a limit to how many groupings an instance can have
			ECS_ASSERT(type->groupings.size < UI_REFLECTION_MAX_GROUPINGS_PER_TYPE);
			grouping.name = function::StringCopy(GetAllocatorPolymorphic(allocator), grouping.name);
			if (grouping.per_element_name.size > 0) {
				grouping.per_element_name = function::StringCopy(GetAllocatorPolymorphic(allocator), grouping.per_element_name);
			}

			type->groupings.AddResize(grouping, GetAllocatorPolymorphic(allocator));
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::AssignInstanceResizableAllocator(Stream<char> instance_name, AllocatorPolymorphic allocator, bool allocate_inputs)
		{
			AssignInstanceResizableAllocator(GetInstance(instance_name), allocator, allocate_inputs);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::AssignInstanceResizableAllocator(UIReflectionInstance* instance, AllocatorPolymorphic allocator, bool allocate_inputs)
		{
			const size_t INPUTS_ALLOCATION_SIZE = 256;

			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].element_index == UIReflectionElement::DirectoryInput || type->fields[index].element_index == UIReflectionElement::FileInput
					|| type->fields[index].element_index == UIReflectionElement::TextInput) 
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
				else if (type->fields[index].stream_type == UIReflectionStreamType::Resizable) {
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

		void UIReflectionDrawer::BindInstanceData(Stream<char> instance_name, Stream<char> field_name, void* field_data) {
			UIReflectionInstance* instance = GetInstance(instance_name);
			BindInstanceData(instance, field_name, field_data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceData(UIReflectionInstance* instance, Stream<char> field_name, void* field_data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			unsigned int field_index = GetTypeFieldIndex(type, field_name);

#define POINTERS(type) type* data = (type*)field_data; type* instance_data = (type*)instance->data[field_index];

#define COPY_VALUES_BASIC memcpy(instance_data->default_value, data->default_value, instance_data->byte_size); \
			memcpy(instance_data->upper_bound, data->upper_bound, instance_data->byte_size); \
			memcpy(instance_data->lower_bound, data->lower_bound, instance_data->byte_size); \

			switch (type->fields[field_index].element_index) {
			case UIReflectionElement::CheckBox:
			{
				POINTERS(UIReflectionCheckBoxData);
				instance_data->value = data->value;
				break;
			}
			case UIReflectionElement::Color:
			{
				POINTERS(UIReflectionColorData);
				instance_data->color = data->color;
				instance_data->default_color = data->default_color;
				break;
			}
			case UIReflectionElement::ComboBox:
			{
				POINTERS(UIReflectionComboBoxData);
				instance_data->active_label = data->active_label;
				instance_data->default_label = data->default_label;
				break;
			}
			case UIReflectionElement::DoubleInput:
			{
				POINTERS(UIReflectionDoubleInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
				break;
			}
			case UIReflectionElement::FloatInput:
			{
				POINTERS(UIReflectionFloatInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
				break;
			}
			case UIReflectionElement::IntegerInput:
			{
				POINTERS(UIReflectionIntInputData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
				break;
			}
			case UIReflectionElement::DoubleSlider:
			{
				POINTERS(UIReflectionDoubleSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
				break;
			}
			case UIReflectionElement::FloatSlider:
			{
				POINTERS(UIReflectionFloatSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
				break;
			}
			case UIReflectionElement::IntegerSlider:
			{
				POINTERS(UIReflectionIntSliderData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
				break;
			}
			case UIReflectionElement::TextInput:
			{
				ECS_ASSERT(false, "Decide what to do here");

				//POINTERS(UIReflectionTextInputData);
				//instance_data->stream.capacity = data->characters;
				break;
			}
			case UIReflectionElement::DoubleInputGroup:
			case UIReflectionElement::FloatInputGroup:
			case UIReflectionElement::IntegerInputGroup:
			case UIReflectionElement::DoubleSliderGroup:
			case UIReflectionElement::FloatSliderGroup:
			case UIReflectionElement::IntegerSliderGroup:
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

		void UIReflectionDrawer::BindTypeDefaultData(Stream<char> type_name, Stream<UIReflectionBindDefaultValue> data) {
			UIReflectionType* type = GetType(type_name);
			BindTypeDefaultData(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeDefaultData(UIReflectionType* type, Stream<UIReflectionBindDefaultValue> data) {
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, ExtractTypedFieldName(data[index].field_name));
				UI_REFLECTION_SET_DEFAULT_DATA[(unsigned int)type->fields[field_index].element_index](type->fields.buffer + field_index, data[index].value);
			}
		}

		void UIReflectionDrawer::BindTypeDefaultData(UIReflectionType* type, const void* data) {
			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].stream_type == UIReflectionStreamType::None) {
					UI_REFLECTION_SET_DEFAULT_DATA[(unsigned int)type->fields[index].element_index](
						type->fields.buffer + index,
						function::OffsetPointer(data, type->fields[index].pointer_offset)
					);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeRange(Stream<char> type_name, Stream<UIReflectionBindRange> data)
		{
			UIReflectionType* type = GetType(type_name);
			BindTypeRange(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeRange(UIReflectionType* type, Stream<UIReflectionBindRange> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, ExtractTypedFieldName(data[index].field_name));
				UI_REFLECTION_SET_RANGE[(unsigned int)type->fields[field_index].element_index](type->fields.buffer + field_index, data[index].min, data[index].max);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypePrecision(Stream<char> type_name, Stream<UIReflectionBindPrecision> data)
		{
			UIReflectionType* type = GetType(type_name);
			BindTypePrecision(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypePrecision(UIReflectionType* type, Stream<UIReflectionBindPrecision> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, ExtractTypedFieldName(data[index].field_name));
				switch (type->fields[field_index].element_index)
				{
				case UIReflectionElement::FloatSlider:
				{
					UIReflectionFloatSliderData* field_data = (UIReflectionFloatSliderData*)type->fields[field_index].data;
					field_data->precision = data[index].precision;
					break;
				}
				case UIReflectionElement::DoubleSlider:
				{
					UIReflectionDoubleSliderData* field_data = (UIReflectionDoubleSliderData*)type->fields[field_index].data;
					field_data->precision = data[index].precision;
					break;
				}
				case UIReflectionElement::FloatSliderGroup:
				case UIReflectionElement::DoubleSliderGroup:
				{
					UIReflectionGroupData<float>* field_data = (UIReflectionGroupData<float>*)type->fields[field_index].data;
					field_data->precision = data[index].precision;
					break;
				}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ConvertTypeResizableStream(Stream<char> type_name, Stream<Stream<char>> field_names)
		{
			ConvertTypeResizableStream(GetType(type_name), field_names);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ConvertTypeResizableStream(UIReflectionType* type, Stream<Stream<char>> field_names)
		{
			for (size_t index = 0; index < field_names.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, field_names[index]);
				ECS_ASSERT(type->fields[field_index].stream_type == UIReflectionStreamType::Capacity);
				type->fields[field_index].stream_type = UIReflectionStreamType::Resizable;

				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)type->fields[field_index].data;
				field_stream->is_resizable = true;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstancePtrs(Stream<char> instance_name, void* data)
		{
			UIReflectionInstance* instance = GetInstance(instance_name);
			BindInstancePtrs(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstancePtrs(UIReflectionInstance* instance, void* data)
		{
			const UIReflectionType* ui_type = GetType(instance->type_name);
			const ReflectionType* reflect = reflection->GetType(ui_type->name);
			BindInstancePtrs(instance, data, reflect);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIInstanceFieldStream UIReflectionDrawerCreateCapacityUIInstanceFieldStream(
			UIReflectionDrawer* drawer, 
			ReflectionStreamFieldType stream_type, 
			uintptr_t ptr,
			unsigned int basic_type_count,
			unsigned int element_byte_size,
			bool disable_writes
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

			field_value.previous_size = 0;
			field_value.element_byte_size = element_byte_size;
			field_value.CopyTarget();
			if (!disable_writes) {
				field_value.standalone_data.Initialize({ nullptr }, 0);
			}
			else {
				field_value.standalone_data.Initialize(GetAllocatorPolymorphic(drawer->allocator), 0);
			}

			return field_value;
		}

		void UIReflectionDrawer::BindInstancePtrs(UIReflectionInstance* instance, void* data, const ReflectionType* reflect)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			uintptr_t ptr = (uintptr_t)data;

			for (size_t index = 0; index < type->fields.size; index++) {
				ptr = (uintptr_t)data + type->fields[index].pointer_offset;

				UIReflectionElement element_index = type->fields[index].element_index;
				unsigned short reflected_type_index = type->fields[index].reflection_type_index;

				bool is_stream = IsStream(type->fields[index].stream_type);

				if (!is_stream) {
					bool is_group = element_index == UIReflectionElement::IntegerInputGroup || element_index == UIReflectionElement::IntegerSliderGroup
						|| element_index == UIReflectionElement::FloatInputGroup || element_index == UIReflectionElement::FloatSliderGroup
						|| element_index == UIReflectionElement::DoubleInputGroup || element_index == UIReflectionElement::DoubleSliderGroup;

					if (!is_group) {
						void** reinterpretation = (void**)instance->data[index];
						*reinterpretation = (void*)ptr;

						switch (element_index) {
						case UIReflectionElement::IntegerInput:
						case UIReflectionElement::IntegerSlider:
						{
							UIReflectionIntInputData* field_data = (UIReflectionIntInputData*)instance->data[index];
							memcpy(field_data->default_value, field_data->value_to_modify, field_data->byte_size);
						}
						break;
						case UIReflectionElement::FloatInput:
						case UIReflectionElement::FloatSlider:
						{
							UIReflectionFloatInputData* field_data = (UIReflectionFloatInputData*)instance->data[index];
							field_data->default_value = *field_data->value;
						}
						break;
						case UIReflectionElement::DoubleInput:
						case UIReflectionElement::DoubleSlider:
						{
							UIReflectionDoubleInputData* field_data = (UIReflectionDoubleInputData*)instance->data[index];
							field_data->default_value = *field_data->value;
						}
						break;
						case UIReflectionElement::Color:
						{
							UIReflectionColorData* field_data = (UIReflectionColorData*)instance->data[index];
							field_data->default_color = *field_data->color;
						}
						break;
						case UIReflectionElement::ColorFloat:
						{
							UIReflectionColorFloatData* field_data = (UIReflectionColorFloatData*)instance->data[index];
							field_data->default_color = *field_data->color;
						}
						break;
						case UIReflectionElement::ComboBox:
						{
							UIReflectionComboBoxData* field_data = (UIReflectionComboBoxData*)instance->data[index];
							field_data->default_label = *field_data->active_label;
						}
						break;
						case UIReflectionElement::DirectoryInput:
						case UIReflectionElement::FileInput:
						case UIReflectionElement::TextInput:
						{
							ReflectionStreamFieldType reflect_stream_type = reflect->fields[reflected_type_index].info.stream_type;
							UIInstanceFieldStream* field_value = (UIInstanceFieldStream*)instance->data[index];
							*field_value = UIReflectionDrawerCreateCapacityUIInstanceFieldStream(
								this, 
								reflect_stream_type,
								ptr, 
								reflect->fields[reflected_type_index].info.basic_type_count,
								field_value->element_byte_size,
								function::HasFlag(type->fields[index].configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM)
							);
						}
						break;
						case UIReflectionElement::Override:
						{
							OverrideAllocationData* allocated_data = (OverrideAllocationData*)instance->data[index];
							// Initialize the field
							void* override_data = allocated_data->GetData();
							if (overrides[index].draw_data_size > 0) {
								memset(override_data, 0, overrides[index].draw_data_size);
								if (overrides[index].initialize_function != nullptr) {
									overrides[index].initialize_function(
										allocated_data->initialize_allocator,
										this,
										type->fields[index].name,
										override_data,
										overrides[index].global_data
									);
								}
							}
						}
						break;
						case UIReflectionElement::UserDefined:
						{
							UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)instance->data[index];
							// Must create an instance of that user defined type here
							ECS_STACK_CAPACITY_STREAM(char, nested_instance_name, 512);
							// Append to the type_and_field_name the current instance name
							nested_instance_name.Copy(data->type_and_field_name);
							nested_instance_name.AddStreamSafe(instance->name);

							// In order to determine the type name, just look for the separator from the type
							// and whatever its after it will be the type
							Stream<char> type_name = function::FindFirstToken(data->type_and_field_name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
							type_name.buffer += ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT;
							type_name.size -= ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT;

							data->instance = CreateInstance(nested_instance_name, type_name);
							BindInstancePtrs(data->instance, (void*)ptr);
						}
						break;
						}
					}
					else {
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance->data[index];
						void* allocation = allocator->Allocate(sizeof(void*) * data->count);
						data->values = (void**)allocation;
		
						unsigned int type_byte_size = reflect->fields[reflected_type_index].info.byte_size / data->count;
						for (size_t subindex = 0; subindex < data->count; subindex++) {
							data->values[subindex] = (void*)ptr;
							memcpy((void*)((uintptr_t)data->default_values + subindex * type_byte_size), (const void*)ptr, type_byte_size);
							ptr += type_byte_size;
						}
					}
				}
				else {
					ReflectionStreamFieldType reflect_stream_type = reflect->fields[reflected_type_index].info.stream_type;

					// There are other structs that contain this as the first member variable
					UIInstanceFieldStream* field_value = (UIInstanceFieldStream*)instance->data[index];

					if (type->fields[index].stream_type == UIReflectionStreamType::Capacity) {
						*field_value = UIReflectionDrawerCreateCapacityUIInstanceFieldStream(
							this, 
							reflect_stream_type,
							ptr, 
							reflect->fields[reflected_type_index].info.basic_type_count,
							field_value->element_byte_size,
							function::HasFlag(type->fields[index].configuration, UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM)
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

		void UIReflectionDrawer::BindInstanceTextInput(Stream<char> instance_name, Stream<UIReflectionBindTextInput> data)
		{
			UIReflectionInstance* instance_ptr = GetInstance(instance_name);
			BindInstanceTextInput(instance_ptr, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceTextInput(UIReflectionInstance* instance, Stream<UIReflectionBindTextInput> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (type->fields[field_index].element_index == UIReflectionElement::TextInput) {
					CapacityStream<char>** input_data = (CapacityStream<char>**)instance->data[field_index];
					*input_data = data[index].stream;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceTextInput(UIReflectionInstance* instance, Stream<unsigned int> field_indices, CapacityStream<char>* inputs)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < field_indices.size; index++) {
				unsigned int field_index = field_indices[index];
				if (type->fields[field_indices[index]].element_index == UIReflectionElement::TextInput) {
					CapacityStream<char>** input_data = (CapacityStream<char>**)instance->data[field_index];
					*input_data = inputs + index;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceDirectoryInput(Stream<char> instance_name, Stream<UIReflectionBindDirectoryInput> data)
		{
			BindInstanceDirectoryInput(GetInstance(instance_name), data);
		}

		void UIReflectionDrawer::BindInstanceDirectoryInput(UIReflectionInstance* instance, Stream<UIReflectionBindDirectoryInput> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (type->fields[field_index].element_index == UIReflectionElement::DirectoryInput) {
					CapacityStream<wchar_t>** input_data = (CapacityStream<wchar_t>**)instance->data[field_index];
					*input_data = data[index].stream;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceDirectoryInput(UIReflectionInstance* instance, Stream<unsigned int> field_indices, CapacityStream<wchar_t>* inputs)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < field_indices.size; index++) {
				unsigned int field_index = field_indices[index];
				if (type->fields[field_index].element_index == UIReflectionElement::DirectoryInput) {
					CapacityStream<wchar_t>** input_data = (CapacityStream<wchar_t>**)instance->data[field_index];
					*input_data = inputs + index;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceFileInput(Stream<char> instance_name, Stream<UIReflectionBindFileInput> data)
		{
			BindInstanceFileInput(GetInstance(instance_name), data);
		}

		void UIReflectionDrawer::BindInstanceFileInput(UIReflectionInstance* instance, Stream<UIReflectionBindFileInput> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (type->fields[field_index].element_index == UIReflectionElement::FileInput) {
					CapacityStream<wchar_t>** input_data = (CapacityStream<wchar_t>**)instance->data[field_index];
					*input_data = data[index].stream;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceFileInput(UIReflectionInstance* instance, Stream<unsigned int> field_indices, CapacityStream<wchar_t>* inputs)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < field_indices.size; index++) {
				unsigned int field_index = field_indices[index];
				if (type->fields[field_index].element_index == UIReflectionElement::FileInput) {
					CapacityStream<wchar_t>** input_data = (CapacityStream<wchar_t>**)instance->data[field_index];
					*input_data = inputs + index;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamCapacity(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data)
		{
			UIReflectionInstance* instance = GetInstance(instance_name);
			BindInstanceStreamCapacity(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				ECS_ASSERT(IsStream(type->fields[field_index].stream_type) || IsStreamInput(type->fields[field_index].element_index));

				// It is ok to alias the resizable stream with the capacity stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->capacity = data[index].capacity;
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<unsigned int> field_indices, unsigned int* capacities)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < field_indices.size; index++) {
				unsigned int field_index = field_indices[index];
				ECS_ASSERT(IsStream(type->fields[field_index].stream_type) || IsStreamInput(type->fields[field_index].element_index));

				// It is ok to alias the resizable stream with the capacity stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->capacity = capacities[index];
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamSize(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data)
		{
			UIReflectionInstance* instance = GetInstance(instance_name);
			BindInstanceStreamSize(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamSize(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				ECS_ASSERT(IsStream(type->fields[field_index].stream_type));

				// It is ok to alias the capacity stream with the resizable stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->size = data[index].capacity;
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamSize(UIReflectionInstance* instance, Stream<unsigned int> field_indices, unsigned int* sizes)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < field_indices.size; index++) {
				unsigned int field_index = field_indices[index];
				ECS_ASSERT(IsStream(type->fields[field_index].stream_type));

				// It is ok to alias the capacity stream with the resizable stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->size = sizes[index];
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamBuffer(Stream<char> instance_name, Stream<UIReflectionBindStreamBuffer> data) {
			UIReflectionInstance* instance = GetInstance(instance_name);
			BindInstanceStreamBuffer(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<UIReflectionBindStreamBuffer> data) {
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				ECS_ASSERT(IsStream(type->fields[field_index].stream_type));

				// It is ok to alias the resizable stream with the capacity stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->buffer = data[index].new_buffer;
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<unsigned int> field_indices, void** buffers)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < field_indices.size; index++) {
				unsigned int field_index = field_indices[index];
				ECS_ASSERT(IsStream(type->fields[field_index].stream_type));

				// It is ok to alias the resizable stream with the capacity stream, same layout
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				field_stream->capacity->buffer = buffers[index];
				field_stream->WriteTarget();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceResizableStreamAllocator(Stream<char> instance_name, Stream<UIReflectionBindResizableStreamAllocator> data)
		{
			BindInstanceResizableStreamAllocator(GetInstance(instance_name), data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceResizableStreamAllocator(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamAllocator> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				ECS_ASSERT(type->fields[field_index].stream_type == UIReflectionStreamType::Resizable);

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

		void UIReflectionDrawer::BindInstanceResizableStreamAllocator(UIReflectionInstance* instance, AllocatorPolymorphic allocator, Stream<unsigned int> field_indices)
		{
			const UIReflectionType* type = GetType(instance->type_name);

			auto field_action = [=](unsigned int field_index) {
				UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[field_index];
				if (field_stream->resizable->allocator.allocator != nullptr) {
					field_stream->resizable->FreeBuffer();
				}
				else {
					field_stream->resizable->buffer = nullptr;
					field_stream->resizable->size = 0;
					field_stream->resizable->capacity = 0;
				}
				field_stream->resizable->allocator = allocator;
				field_stream->is_resizable = true;
				field_stream->WriteTarget();
			};
			if (field_indices.size > 0) {
				for (size_t index = 0; index < field_indices.size; index++) {
					unsigned int field_index = field_indices[index];
					ECS_ASSERT(type->fields[field_index].stream_type == UIReflectionStreamType::Resizable);

					field_action(field_index);
				}
			}
			else {
				for (unsigned int index = 0; index < type->fields.size; index++) {
					if (type->fields[index].stream_type == UIReflectionStreamType::Resizable) {
						field_action(index);
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceResizableStreamData(Stream<char> instance_name, Stream<UIReflectionBindResizableStreamData> data)
		{
			BindInstanceResizableStreamData(GetInstance(instance_name), data);
		}

		void UIReflectionDrawer::BindInstanceResizableStreamData(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamData> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				ECS_ASSERT(type->fields[field_index].stream_type == UIReflectionStreamType::Resizable);

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

		void UIReflectionDrawer::BindInstanceFieldOverride(Stream<char> instance_name, Stream<char> tag, UIReflectionInstanceModifyOverride modify_override, void* user_data) {
			BindInstanceFieldOverride(GetInstance(instance_name), tag, modify_override, user_data);
		}
		

		void UIReflectionDrawer::BindInstanceFieldOverride(UIReflectionInstance* instance, Stream<char> tag, UIReflectionInstanceModifyOverride modify_override, void* user_data) {
			unsigned int override_index = FindFieldOverride(tag);
			ECS_ASSERT(override_index != -1);

			const UIReflectionType* type = GetType(instance->type_name);

			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].element_index == UIReflectionElement::Override) {
					OverrideAllocationData* data = (OverrideAllocationData*)instance->data[index];
					if (data->override_index == override_index) {
						// Same type, can proceed
						modify_override(GetAllocatorPolymorphic(allocator), data->GetData(), overrides[override_index].global_data, user_data);
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindInstanceFieldOverride(
			void* override_data, 
			Stream<char> tag, 
			UIReflectionInstanceModifyOverride modify_override, 
			void* user_data,
			AllocatorPolymorphic override_allocator
		)
		{
			unsigned int override_index = FindFieldOverride(tag);
			ECS_ASSERT(override_index != -1);

			if (override_allocator.allocator == nullptr) {
				override_allocator = GetAllocatorPolymorphic(allocator);
			}

			modify_override(override_allocator, override_data, overrides[override_index].global_data, user_data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ConvertTypeStreamsToResizable(Stream<char> type_name)
		{
			ConvertTypeStreamsToResizable(GetType(type_name));
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

		void UIReflectionDrawer::CopyInstanceStreams(Stream<char> instance_name, Stream<UIReflectionStreamCopy> data)
		{
			UIReflectionInstance* instance = GetInstance(instance_name);
			CopyInstanceStreams(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::CopyInstanceStreams(UIReflectionInstance* instance, Stream<UIReflectionStreamCopy> data)
		{
			const UIReflectionType* type = GetType(instance->type_name);

			for (size_t index = 0; index < instance->data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, data[index].field_name);
				if (IsStream(type->fields[field_index].stream_type)) {
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

		void UIReflectionDrawer::ChangeInputToSlider(Stream<char> type_name, Stream<char> field_name)
		{
			UIReflectionType* type = GetType(type_name);
			ChangeInputToSlider(type, field_name);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ChangeInputToSlider(UIReflectionType* type, Stream<char> field_name)
		{
			unsigned int field_index = GetTypeFieldIndex(type, field_name);

			void* field_data = type->fields[field_index].data;
			switch (type->fields[field_index].element_index) {
			case UIReflectionElement::DoubleInput:
			{
				UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)field_data;
				auto new_allocation = (UIReflectionDoubleSliderData*)allocator->Allocate(sizeof(UIReflectionDoubleSliderData));
				new_allocation->default_value = data->default_value;
				new_allocation->lower_bound = data->lower_bound;
				new_allocation->upper_bound = data->upper_bound;
				new_allocation->value_to_modify = data->value;
				allocator->Deallocate(data);
				type->fields[field_index].data = new_allocation;

				type->fields[field_index].element_index = UIReflectionElement::DoubleSlider;
				// Remove the number range flag if present for the configuration
				type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
			}
			break;
			case UIReflectionElement::FloatInput:
			{
				UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)field_data;
				auto new_allocation = (UIReflectionFloatSliderData*)allocator->Allocate(sizeof(UIReflectionFloatSliderData));
				new_allocation->default_value = data->default_value;
				new_allocation->lower_bound = data->lower_bound;
				new_allocation->upper_bound = data->upper_bound;
				new_allocation->value_to_modify = data->value;
				allocator->Deallocate(data);
				type->fields[field_index].data = new_allocation;

				type->fields[field_index].element_index = UIReflectionElement::FloatSlider;
				// Remove the number range flag if present for the configuration
				type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
			}
			break;// Remove the number range flag if present for the configuration
			type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
			// integer_convert input needs no correction, except element_index
			case UIReflectionElement::IntegerInput:
				type->fields[field_index].element_index = UIReflectionElement::IntegerSlider;
				// Remove the number range flag if present for the configuration
				type->fields[field_index].configuration = function::ClearFlag(type->fields[field_index].configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
				break;
				// groups needs no correction, except element_index
			case UIReflectionElement::DoubleInputGroup:
				type->fields[field_index].element_index = UIReflectionElement::DoubleSliderGroup;
				break;
				// groups needs no correction, except element_index
			case UIReflectionElement::FloatInputGroup:
				type->fields[field_index].element_index = UIReflectionElement::FloatSliderGroup;
				break;
				// groups needs no correction, except element_index
			case UIReflectionElement::IntegerInputGroup:
				type->fields[field_index].element_index = UIReflectionElement::IntegerSliderGroup;
				break;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeData(Stream<char> type_name, Stream<char> field_name, void* field_data)
		{
			UIReflectionType* type = GetType(type_name);
			BindTypeData(type, field_name, field_data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeData(UIReflectionType* type, Stream<char> field_name, void* field_data) {
			unsigned int field_index = GetTypeFieldIndex(type, field_name);

#define POINTERS(type__) type__* data = (type__*)field_data; type__* instance_data = (type__*)type->fields[field_index].data;

#define COPY_VALUES_BASIC memcpy(instance_data->default_value, data->default_value, instance_data->byte_size); \
			memcpy(instance_data->upper_bound, data->upper_bound, instance_data->byte_size); \
			memcpy(instance_data->lower_bound, data->lower_bound, instance_data->byte_size); \

			switch (type->fields[field_index].element_index) {
			case UIReflectionElement::CheckBox:
			{
				POINTERS(UIReflectionCheckBoxData);
				instance_data->value = data->value;
			}
			break;
			case UIReflectionElement::Color:
			{
				POINTERS(UIReflectionColorData);
				instance_data->color = data->color;
				instance_data->default_color = data->default_color;
			}
			break;
			case UIReflectionElement::ComboBox:
			{
				POINTERS(UIReflectionComboBoxData);
				instance_data->active_label = data->active_label;
				instance_data->default_label = data->default_label;
			}
			break;
			case UIReflectionElement::DoubleInput:
			{
				POINTERS(UIReflectionDoubleInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
			}
			break;
			case UIReflectionElement::FloatInput:
			{
				POINTERS(UIReflectionFloatInputData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value = data->value;
			}
			break;
			case UIReflectionElement::IntegerInput:
			{
				POINTERS(UIReflectionIntInputData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
			}
			break;
			case UIReflectionElement::DoubleSlider:
			{
				POINTERS(UIReflectionDoubleSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
			}
			break;
			case UIReflectionElement::FloatSlider:
			{
				POINTERS(UIReflectionFloatSliderData);
				instance_data->default_value = data->default_value;
				instance_data->lower_bound = data->lower_bound;
				instance_data->upper_bound = data->upper_bound;
				instance_data->value_to_modify = data->value_to_modify;
				instance_data->precision = data->precision;
			}
			break;
			case UIReflectionElement::IntegerSlider:
			{
				POINTERS(UIReflectionIntSliderData);
				COPY_VALUES_BASIC;
				instance_data->value_to_modify = data->value_to_modify;
			}
			break;
			case UIReflectionElement::TextInput:
			case UIReflectionElement::FileInput:
			case UIReflectionElement::DirectoryInput:
			{
				ECS_ASSERT(false, "Decide what to do here");

				//POINTERS(UIReflectionTextInputData);
				//instance_data->characters = data->characters;
			}
			break;
			case UIReflectionElement::DoubleInputGroup:
			case UIReflectionElement::FloatInputGroup:
			case UIReflectionElement::IntegerInputGroup:
			case UIReflectionElement::DoubleSliderGroup:
			case UIReflectionElement::FloatSliderGroup:
			case UIReflectionElement::IntegerSliderGroup:
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

		void UIReflectionDrawer::BindTypeLowerBounds(Stream<char> type_name, Stream<UIReflectionBindLowerBound> data)
		{
			UIReflectionType* type = GetType(type_name);
			BindTypeLowerBounds(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeLowerBounds(UIReflectionType* type, Stream<UIReflectionBindLowerBound> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, ExtractTypedFieldName(data[index].field_name));
				UI_REFLECTION_SET_LOWER_BOUND[(unsigned int)type->fields[field_index].element_index](type->fields.buffer + field_index, data[index].value);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeLowerBounds(UIReflectionType* type, const void* data) {
			for (size_t index = 0; index < type->fields.size; index++) {
				unsigned int int_index = (unsigned int)type->fields[index].element_index;
				if (int_index < std::size(UI_REFLECTION_SET_LOWER_BOUND) && type->fields[index].stream_type == UIReflectionStreamType::None) {
					UI_REFLECTION_SET_LOWER_BOUND[int_index](type->fields.buffer + index, function::OffsetPointer(data, type->fields[index].pointer_offset));
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeUpperBounds(Stream<char> type_name, Stream<UIReflectionBindUpperBound> data)
		{
			UIReflectionType* type = GetType(type_name);
			BindTypeUpperBounds(type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeUpperBounds(UIReflectionType* type, Stream<UIReflectionBindUpperBound> data)
		{
			for (size_t index = 0; index < data.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, ExtractTypedFieldName(data[index].field_name));
				UI_REFLECTION_SET_UPPER_BOUND[(unsigned int)type->fields[field_index].element_index](type->fields.buffer + field_index, data[index].value);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::BindTypeUpperBounds(UIReflectionType* type, const void* data) {
			for (size_t index = 0; index < type->fields.size; index++) {
				unsigned int int_index = (unsigned int)type->fields[index].element_index;
				if (int_index < std::size(UI_REFLECTION_SET_UPPER_BOUND) && type->fields[index].stream_type == UIReflectionStreamType::None) {
					UI_REFLECTION_SET_UPPER_BOUND[int_index](type->fields.buffer + index, function::OffsetPointer(data, type->fields[index].pointer_offset));
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::ChangeDirectoryToFile(Stream<char> type_name, Stream<char> field_name)
		{
			ChangeDirectoryToFile(GetType(type_name), field_name);
		}

		void UIReflectionDrawer::ChangeDirectoryToFile(UIReflectionType* type, Stream<char> field_name)
		{
			unsigned int field_index = GetTypeFieldIndex(type, field_name);
			ECS_ASSERT(type->fields[field_index].element_index == UIReflectionElement::DirectoryInput);
			type->fields[field_index].element_index = UIReflectionElement::FileInput;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::CreateType(Stream<char> name, UIReflectionDrawerCreateTypeOptions* options)
		{
			ResourceIdentifier identifier(name);
			ECS_ASSERT(type_definition.Find(identifier) == -1);

			return CreateType(reflection->GetType(name), options);
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
			UIReflectionElement element_index,
			unsigned int int_flags = 0
		) {
			UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)reflection->allocator->Allocate(sizeof(UIReflectionGroupData<void>));
			field.data = data;
			field.element_index = element_index;
			field.byte_size = sizeof(*data);

			data->count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
			data->input_names = BasicTypeNames;
			data->byte_size = type_byte_size;

			// defaults
			void* allocation = reflection->allocator->Allocate(data->byte_size * 3 * data->count + sizeof(bool) * 3);
			uintptr_t ptr = (uintptr_t)allocation;

			// Embbed in front of each buffer a boolean telling whether or not the buffer can be used or not
			data->values = nullptr;
			data->int_flags = int_flags;
			CreateTypeInputGroupSetBuffers(data, ptr);

			field.stream_type = UIReflectionStreamType::None;
			field.configuration = 0;
		};

		void ConvertStreamSingleForType(
			UIReflectionDrawer* reflection, 
			const ReflectionField& reflection_field, 
			UIReflectionTypeField& field, 
			UIReflectionElement element_index,
			unsigned int int_flags = 0
		) {
			size_t size_to_allocate = int_flags == 0 ? sizeof(UIReflectionStreamBaseData) : sizeof(UIReflectionStreamIntInputData);
			UIReflectionStreamBaseData* data = (UIReflectionStreamBaseData*)reflection->allocator->Allocate(size_to_allocate);
			memset(data, 0, sizeof(*data));

			field.data = data;
			field.element_index = element_index;
			field.byte_size = sizeof(*data);

			data->stream.element_byte_size = reflection_field.info.stream_byte_size;
			data->stream.is_resizable = false;

			if (int_flags > 0) {
				UIReflectionStreamIntInputData* int_data = (UIReflectionStreamIntInputData*)data;
				int_data->int_flags = int_flags;
			}

			field.stream_type = UIReflectionStreamType::Capacity;
			field.configuration = 0;
		}

		void ConvertStreamGroupForType(
			UIReflectionDrawer* reflection,
			ReflectionField reflection_field,
			UIReflectionTypeField& field,
			UIReflectionElement element_index,
			unsigned int int_flags = 0
		) {
			UIReflectionStreamInputGroupData* data = (UIReflectionStreamInputGroupData*)reflection->allocator->Allocate(sizeof(UIReflectionStreamInputGroupData));
			memset(data, 0, sizeof(*data));

			field.data = data;
			field.element_index = element_index;
			field.byte_size = sizeof(*data);

			data->basic_type_count = Reflection::BasicTypeComponentCount(reflection_field.info.basic_type);
			data->base_data.stream.element_byte_size = reflection_field.info.stream_byte_size;

			data->int_flags = int_flags;

			field.stream_type = UIReflectionStreamType::Capacity;
			field.configuration = 0;
		}

		UIReflectionType* UIReflectionDrawer::CreateType(const ReflectionType* reflected_type, UIReflectionDrawerCreateTypeOptions* options)
		{
			Stream<char> identifier_name = {};
			Stream<unsigned int> ignore_fields = {};
			if (options != nullptr) {
				identifier_name = options->identifier_name;
				ignore_fields = options->ignore_fields;
			}

			auto integer_convert_single = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionIntInputData* data = (UIReflectionIntInputData*)allocator->Allocate(sizeof(UIReflectionIntInputData));
				field.data = data;
				field.element_index = UIReflectionElement::IntegerInput;
				field.byte_size = sizeof(*data);
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
				data->int_flags = 1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type);

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto integer_convert_group = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertGroupForType(
					this, 
					reflection_field,
					field, 
					1 << ((unsigned int)reflection_field.info.basic_type / 2),
					UIReflectionElement::IntegerInputGroup, 
					1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type)
				);
			};

			auto float_convert_single = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionFloatInputData* data = (UIReflectionFloatInputData*)allocator->Allocate(sizeof(UIReflectionFloatInputData));
				field.data = data;
				field.element_index = UIReflectionElement::FloatInput;
				field.byte_size = sizeof(*data);

				// defaults
				data->default_value = 0.0f;
				data->lower_bound = -FLT_MAX;
				data->upper_bound = FLT_MAX;
				data->value = nullptr;

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto float_convert_group = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertGroupForType(this, reflection_field, field, sizeof(float), UIReflectionElement::FloatInputGroup);
			};

			auto double_convert_single = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionDoubleInputData* data = (UIReflectionDoubleInputData*)allocator->Allocate(sizeof(UIReflectionDoubleInputData));
				field.data = data;
				field.element_index = UIReflectionElement::DoubleInput;
				field.byte_size = sizeof(*data);

				// defaults
				data->value = nullptr;
				data->default_value = 0;
				data->lower_bound = -DBL_MAX;
				data->upper_bound = DBL_MAX;

				field.stream_type = UIReflectionStreamType::None;
				field.configuration = 0;
			};

			auto double_convert_group = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertGroupForType(this, reflection_field, field, sizeof(double), UIReflectionElement::DoubleInputGroup);
			};

			auto enum_convert = [this](const ReflectionEnum& reflection_enum, UIReflectionTypeField& field) {
				UIReflectionComboBoxData* data = (UIReflectionComboBoxData*)this->allocator->Allocate(sizeof(UIReflectionComboBoxData));
				field.data = data;
				field.element_index = UIReflectionElement::ComboBox;
				field.byte_size = sizeof(*data);
				data->labels = reflection_enum.fields;
				data->label_display_count = reflection_enum.fields.size;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto text_input_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::TextInput);
				field.stream_type = UIReflectionStreamType::None;
			};

			auto directory_input_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::DirectoryInput);
				field.stream_type = UIReflectionStreamType::None;
			};

			auto bool_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionCheckBoxData* data = (UIReflectionCheckBoxData*)allocator->Allocate(sizeof(UIReflectionCheckBoxData));
				field.data = data;
				field.element_index = UIReflectionElement::CheckBox;
				field.byte_size = sizeof(*data);

				data->value = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto color_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionColorData* data = (UIReflectionColorData*)allocator->Allocate(sizeof(UIReflectionColorData));
				field.data = data;
				field.element_index = UIReflectionElement::Color;
				field.byte_size = sizeof(*data);

				data->color = nullptr;
				data->default_color = Color(0, 0, 0);

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto color_float_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionColorFloatData* data = (UIReflectionColorFloatData*)allocator->Allocate(sizeof(UIReflectionColorFloatData));
				field.data = data;
				field.element_index = UIReflectionElement::ColorFloat;
				field.byte_size = sizeof(*data);

				data->color = nullptr;
				data->default_color = ColorFloat(0.0f, 0.0f, 0.0f, 1.0f);

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;
			};

			auto user_defined_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)allocator->Allocate(sizeof(UIReflectionUserDefinedData));
				field.data = data;
				field.element_index = UIReflectionElement::UserDefined;

				const ReflectionType* user_defined_type = reflection->GetType(reflection_field.definition);
				field.byte_size = Reflection::GetReflectionTypeByteSize(user_defined_type);

				// +1 for '\0'
				size_t string_size = reflection_field.definition.size + ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT + reflection_field.name.size + 1;
				// The instance name is of form - field_name##definition
				char* name = (char*)allocator->Allocate(string_size * sizeof(char), alignof(char));
				Stream<char> stream_name = { name, 0 };
				stream_name.AddStream(reflection_field.name);
				stream_name.AddStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
				stream_name.AddStream(reflection_field.definition);
				stream_name[stream_name.size] = '\0';

				// Just the definition must be remembered here in the type.
				// The actual instance name will be remembered when the instance is created
				data->type_and_field_name = name;
				data->ui_drawer = this;
				data->field_pointer = nullptr;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::None;

				// If the type doesn't exist in the UI reflection, then create it
				if (GetType(reflection_field.definition) == nullptr) {
					CreateType(user_defined_type);
				}
			};

			// Returns true if it found an override, else false.
			// It allocates a structure of type OverrideAllocationData as storage because extra metadata needs to
			// be kept about the index of the override and the name of the field which it will alias
			auto check_override_type = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				for (size_t index = 0; index < overrides.size; index++) {
					if (reflection_field.Is(overrides[index].tag)) {
						// We found a match
						unsigned int allocate_size = overrides[index].draw_data_size + sizeof(OverrideAllocationData);
						OverrideAllocationData* allocated_data = (OverrideAllocationData*)allocator->Allocate(allocate_size);;
						field.data = allocated_data;
						allocated_data->override_index = index;
						// At the moment we don't know the field data
						allocated_data->field_data = nullptr;
						allocated_data->draw_function = overrides[index].draw_function;

						field.element_index = UIReflectionElement::Override;
						field.byte_size = sizeof(OverrideAllocationData) + overrides[index].draw_data_size;
						field.configuration = 0;
						field.stream_type = UIReflectionStreamType::None;
						return true;
					}
				}
				return false;
			};

			auto integer_stream_convert_single = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::IntegerInput, 1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type));
			};

			auto integer_stream_convert_group = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamGroupForType(this, reflection_field, field, UIReflectionElement::IntegerInputGroup, 1 << (UI_INT_BASE + (unsigned int)reflection_field.info.basic_type));
			};

			auto float_stream_convert_single = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::FloatInput);
			};

			auto float_stream_convert_group = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamGroupForType(this, reflection_field, field, UIReflectionElement::FloatInputGroup);
			};

			auto double_stream_convert_single = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::DoubleInput);
			};

			auto double_stream_convert_group = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamGroupForType(this, reflection_field, field, UIReflectionElement::DoubleInputGroup);
			};

			auto enum_stream_convert = [this](const ReflectionEnum& reflection_enum, UIReflectionTypeField& field) {
				UIReflectionStreamComboBoxData* data = (UIReflectionStreamComboBoxData*)allocator->Allocate(sizeof(UIReflectionStreamComboBoxData));
				memset(data, 0, sizeof(*data));

				field.data = data;
				field.element_index = UIReflectionElement::ComboBox;
				field.byte_size = sizeof(*data);

				data->label_names = nullptr;
				data->base_data.stream.element_byte_size = sizeof(char);

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Capacity;
			};

			auto text_input_stream_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::TextInput);
			};

			auto directory_input_stream_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::DirectoryInput);
			};

			auto bool_stream_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::CheckBox);
			};

			auto color_stream_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::Color);
			};

			auto color_float_stream_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				ConvertStreamSingleForType(this, reflection_field, field, UIReflectionElement::ColorFloat);
			};

			auto user_defined_stream_convert = [this](const ReflectionField& reflection_field, UIReflectionTypeField& field) {
				UIReflectionStreamUserDefinedData* data = (UIReflectionStreamUserDefinedData*)allocator->Allocate(sizeof(UIReflectionStreamUserDefinedData));
				memset(data, 0, sizeof(*data));

				field.data = data;
				field.element_index = UIReflectionElement::UserDefined;
				field.byte_size = sizeof(*data);

				data->ui_drawer = this;
				data->base_data.stream.element_byte_size = reflection_field.info.stream_byte_size;

				Stream<char> user_defined_type = GetUserDefinedTypeFromStreamUserDefined(reflection_field.definition, reflection_field.info.stream_type);
				Stream<char> allocated_type = function::StringCopy(GetAllocatorPolymorphic(allocator), user_defined_type);
				data->type_name = allocated_type.buffer;

				field.configuration = 0;
				field.stream_type = UIReflectionStreamType::Capacity;

				// If the user defined type doesn't exist in the UI, then create it
				if (GetType(data->type_name)) {
					CreateType(reflection->GetType(data->type_name));
				}
			};

			ECS_ASSERT(type_definition.Find(reflected_type->name) == -1);

			UIReflectionType type;
			type.name = reflected_type->name;
			type.fields.Initialize(allocator, 0, reflected_type->fields.size);
			type.has_overriden_identifier = false;
			type.groupings = { nullptr, 0 };

			auto test_basic_type = [&](unsigned int index, ReflectionFieldInfo field_info, bool& value_written) {
				const ReflectionField* field = &reflected_type->fields[index];

				// Check the override first
				value_written = check_override_type(*field, type.fields[type.fields.size]);
				if (value_written) {
					return;
				}
				if (function::CompareStrings(field->definition, "Color")) {
					color_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (function::CompareStrings(field->definition, "ColorFloat")) {
					color_float_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralSingleComponent(field_info.basic_type)) {
					integer_convert_single(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralMultiComponent(field_info.basic_type)) {
					integer_convert_group(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Float) {
					float_convert_single(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsFloatBasicTypeMultiComponent(field_info.basic_type)) {
					float_convert_group(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Double) {
					double_convert_single(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsDoubleBasicTypeMultiComponent(field_info.basic_type)) {
					double_convert_group(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Bool) {
					bool_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsEnum(field_info.basic_type)) {
					ReflectionEnum reflected_enum;
					bool success = reflection->TryGetEnum(field->definition, reflected_enum);
					if (success) {
						enum_convert(reflected_enum, type.fields[type.fields.size]);
						value_written = true;
					}
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// Only if it doesn't have the ECS_GIVE_SIZE_REFLECTION macro
					if (!field->Has(STRING(ECS_GIVE_SIZE_REFLECTION))) {
						user_defined_convert(*field, type.fields[type.fields.size]);
						value_written = true;
					}
				}
			};

			auto test_stream_type = [&](unsigned int index, ReflectionFieldInfo field_info, bool& value_written) {
				const ReflectionField* field = &reflected_type->fields[index];

				Stream<char> definition_stream = field->definition;
				if (function::CompareStrings(definition_stream, "Color")) {
					color_stream_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (function::CompareStrings(definition_stream, "ColorFloat")) {
					color_float_stream_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (function::CompareStrings(definition_stream, "Stream<CapacityStream<char>>") 
					|| function::CompareStrings(definition_stream, "CapacityStream<CapacityStream<char>>")
					|| function::CompareStrings(definition_stream, "ResizableStream<CapacityStream<char>>")
				) {
					text_input_stream_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (function::CompareStrings(definition_stream, "Stream<CapacityStream<wchar_t>>")
					|| function::CompareStrings(definition_stream, "CapacityStream<CapacityStream<wchar_t>>")
					|| function::CompareStrings(definition_stream, "ResizableStream<CapacityStream<wchar_t>")
				) {
					directory_input_stream_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				// text input - default for char and unsigned char
				else if (field_info.basic_type == ReflectionBasicFieldType::Int8 || field_info.basic_type == ReflectionBasicFieldType::UInt8) {
					text_input_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Wchar_t) {
					directory_input_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Float) {
					float_stream_convert_single(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsFloatBasicTypeMultiComponent(field_info.basic_type)) {
					float_stream_convert_group(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Double) {
					double_stream_convert_single(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsDoubleBasicTypeMultiComponent(field_info.basic_type)) {
					double_stream_convert_group(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralSingleComponent(field_info.basic_type)) {
					integer_stream_convert_single(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsIntegralMultiComponent(field_info.basic_type)) {
					integer_stream_convert_group(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::Bool) {
					bool_stream_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
				else if (IsEnum(field_info.basic_type)) {
					ReflectionEnum reflected_enum;
					bool success = reflection->TryGetEnum(field->definition, reflected_enum);
					if (success) {
						enum_stream_convert(reflected_enum, type.fields[type.fields.size]);
						value_written = true;
					}
				}
				else if (field_info.basic_type == ReflectionBasicFieldType::UserDefined) {
					user_defined_stream_convert(*field, type.fields[type.fields.size]);
					value_written = true;
				}
			};

			auto is_field_omitted = [&](unsigned int index) {
				return function::SearchBytes(ignore_fields.buffer, ignore_fields.size, (size_t)index, sizeof(index)) != -1;
			};

			for (size_t index = 0; index < reflected_type->fields.size; index++) {
				bool is_omitted = is_field_omitted(index);

				if (!is_omitted) {
					const ReflectionFieldInfo& field_info = reflected_type->fields[index].info;

					bool value_written = false;
					type.fields[type.fields.size].name = reflected_type->fields[index].name;
					type.fields[type.fields.size].pointer_offset = reflected_type->fields[index].info.pointer_offset;
					type.fields[type.fields.size].reflection_type_index = index;
					type.fields[type.fields.size].tags = reflected_type->fields[index].tag;
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

					if (value_written) {
						UIReflectionElement ui_reflection_index = type.fields[type.fields.size].element_index;
						if (field_info.has_default_value && ui_reflection_index != UIReflectionElement::Override) {
							UI_REFLECTION_SET_DEFAULT_DATA[(unsigned int)ui_reflection_index](type.fields.buffer + type.fields.size, &field_info.default_bool);
						}

						// Check the range tag first
						Stream<char> range_tag = reflected_type->fields[index].GetTag(STRING(ECS_UI_RANGE_REFLECT));
						unsigned int element_index = (unsigned int)ui_reflection_index;
						ReflectionBasicFieldType basic_type = field_info.basic_type;

						auto parse_default = [&](Stream<char>* parse_tag) {
							ECS_STACK_VOID_STREAM(conversion_storage, sizeof(double4));
							double4 default_value = function::ParseDouble4(parse_tag, ',', ',', Stream<char>(UI_IGNORE_RANGE_OR_PARAMETERS_TAG));

							if (default_value.x != DBL_MAX) {
								if (ui_reflection_index == UIReflectionElement::Color) {
									if (default_value.x < 1.0 || default_value.y < 1.0 || default_value.z < 1.0 || default_value.w < 1.0) {
										// The values need to be in the 0 - 255 range
										double range = 255;
										default_value.x *= range;
										default_value.y *= range;
										default_value.z *= range;
										default_value.w *= range;
									}

									Color color_value = (double*)&default_value;
									UI_REFLECTION_SET_DEFAULT_DATA[element_index](type.fields.buffer + index, &color_value);
								}
								else if (ui_reflection_index == UIReflectionElement::ColorFloat) {
									ColorFloat color_value = (double*)&default_value;
									UI_REFLECTION_SET_DEFAULT_DATA[element_index](type.fields.buffer + index, &color_value);
								}
								else {
									// Convert to the corresponding type
									ConvertFromDouble4ToBasic(basic_type, default_value, conversion_storage.buffer);
									UI_REFLECTION_SET_DEFAULT_DATA[element_index](type.fields.buffer + index, conversion_storage.buffer);
								}
							}
						};

						auto parse_lower_upper_bound = [&](Stream<char>* range_tag) {
							ECS_STACK_VOID_STREAM(conversion_storage, sizeof(double4));
							double4 lower_bound = function::ParseDouble4(range_tag, ',', ',', Stream<char>(UI_IGNORE_RANGE_OR_PARAMETERS_TAG));
							double4 upper_bound = function::ParseDouble4(range_tag, ')', ',', Stream<char>(UI_IGNORE_RANGE_OR_PARAMETERS_TAG));

							if (lower_bound.x != DBL_MAX) {
								ConvertFromDouble4ToBasic(basic_type, lower_bound, conversion_storage.buffer);
								UI_REFLECTION_SET_LOWER_BOUND[element_index](
									type.fields.buffer + index,
									conversion_storage.buffer
									);
							}

							if (upper_bound.x != DBL_MAX) {
								ConvertFromDouble4ToBasic(basic_type, upper_bound, conversion_storage.buffer);
								UI_REFLECTION_SET_UPPER_BOUND[element_index](
									type.fields.buffer + index,
									conversion_storage.buffer
								);
							}
						};

						if (range_tag.size > 0) {
							if (element_index < std::size(UI_REFLECTION_SET_LOWER_BOUND)) {
								range_tag = function::FindFirstCharacter(range_tag, '(');
								range_tag.Advance();
								parse_lower_upper_bound(&range_tag);
							}
						}
						else {
							Stream<char> parameters_tag = reflected_type->fields[index].GetTag(STRING(ECS_UI_PARAMETERS_REFLECT));
							if (parameters_tag.size > 0) {
								// Get the default, lower bound and upper bound values
								parameters_tag = function::FindFirstCharacter(parameters_tag, '(');
								parameters_tag.Advance();

								parse_default(&parameters_tag);
								if (element_index < std::size(UI_REFLECTION_SET_LOWER_BOUND)) {
									parse_lower_upper_bound(&parameters_tag);
								}
							}
							else {
								// Try the default tag now
								Stream<char> default_tag = reflected_type->fields[index].GetTag(STRING(ECS_UI_DEFAULT_REFLECT));
								if (default_tag.size > 0) {
									default_tag = function::FindFirstCharacter(default_tag, '(');
									default_tag.Advance();
									parse_default(&default_tag);
								}
							}
						}

						type.fields.size++;
					}
				}
			}

			if (type.fields.size < type.fields.capacity) {
				void* new_allocation = allocator->Allocate(sizeof(UIReflectionTypeField) * type.fields.size);
				memcpy(new_allocation, type.fields.buffer, sizeof(UIReflectionTypeField) * type.fields.size);
				allocator->Deallocate(type.fields.buffer);
				type.fields.buffer = (UIReflectionTypeField*)new_allocation;
			}

			unsigned int inserted_position = 0;
			ResourceIdentifier identifier(type.name);
			if (identifier_name.size > 0) {
				identifier_name = function::StringCopy(GetAllocatorPolymorphic(allocator), identifier_name);
				type.has_overriden_identifier = true;
				identifier = identifier_name;
			}

			ECS_ASSERT(!type_definition.Insert(type, identifier, inserted_position));
			return type_definition.GetValuePtrFromIndex(inserted_position);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::CreateInstance(Stream<char> name, Stream<char> type_name, AllocatorPolymorphic user_defined_allocator)
		{
			const UIReflectionType* type = GetType(type_name);
			return CreateInstance(name, type, { nullptr, 0 }, user_defined_allocator);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::CreateInstance(
			Stream<char> name, 
			const UIReflectionType* type, 
			Stream<char> identifier_name, 
			AllocatorPolymorphic user_defined_allocator
		)
		{
			user_defined_allocator = user_defined_allocator.allocator == nullptr ? GetAllocatorPolymorphic(allocator) : user_defined_allocator;

			ResourceIdentifier identifier(name);
			ECS_ASSERT(instances.Find(identifier) == -1);

			Stream<char> type_name = type->name;
			if (identifier_name.size > 0) {
				unsigned int type_index = type_definition.Find(identifier_name);
				if (type_index != -1) {
					ResourceIdentifier stable_identifier = type_definition.GetIdentifierFromIndex(type_index);
					type_name = { stable_identifier.ptr, stable_identifier.size };
				}
			}

			UIReflectionInstance instance;
			instance.type_name = type_name;

			size_t total_memory = name.size + sizeof(void*) * type->fields.size;

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
					switch (type->fields[index].element_index) {
					case UIReflectionElement::DoubleInputGroup:
					case UIReflectionElement::DoubleSliderGroup:
					case UIReflectionElement::FloatInputGroup:
					case UIReflectionElement::FloatSliderGroup:
					case UIReflectionElement::IntegerInputGroup:
					case UIReflectionElement::IntegerSliderGroup:
					{
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance.data[index];
						data->values = (void**)allocator->Allocate(sizeof(void*) * data->count);
					}
					break;
					case UIReflectionElement::UserDefined:
					{
						// Here we shouldn't do anything. We should perform the instance creation on ptr bind
						// Since the Rebind function firstly deletes the instance and we need to reconstruct one
						// New in the bind instance ptrs
					}
					break;
					case UIReflectionElement::Override:
					{
						OverrideAllocationData* allocated_data = (OverrideAllocationData*)instance.data[index];
						allocated_data->initialize_allocator = user_defined_allocator;
					}
					break;
					}
				}
			}

			instance.name.InitializeAndCopy(buffer, name);
			instance.data.size = type->fields.size;
			identifier = instance.name;
			memset(instance.grouping_open_state, 0, sizeof(instance.grouping_open_state));

			unsigned int position = 0;
			ECS_ASSERT(!instances.Insert(instance, identifier, position));
			return instances.GetValuePtrFromIndex(position);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

#pragma region CreateForHierarchy helpers

		void CreateForHierarchyCheckIncludeIndex(
			const ReflectionType* type, 
			size_t& include_index, 
			UIReflectionDrawerSearchOptions options
		) {
			if (options.include_tags.size > 0) {
				include_index = 0;
				for (; include_index < options.include_tags.size; include_index++) {
					if (options.include_tags[include_index].has_compare) {
						if (type->HasTag(options.include_tags[include_index].tag)) {
							break;
						}
					}
					else {
						if (type->IsTag(options.include_tags[include_index].tag)) {
							break;
						}
					}
				}

				// If it got to the last, increase such that it will be bigger than the size
				include_index += include_index == options.include_tags.size;
			}
		}

		void CreateForHierarchyCheckExcludeIndex(
			const ReflectionType* type,
			size_t& exclude_index,
			UIReflectionDrawerSearchOptions options
		) {
			if (options.exclude_tags.size > 0) {
				exclude_index = 0;
				for (; exclude_index < options.exclude_tags.size; exclude_index++) {
					if (options.exclude_tags[exclude_index].has_compare) {
						if (type->HasTag(options.exclude_tags[exclude_index].tag)) {
							exclude_index = -1;
							break;
						}
					}
					else {
						if (type->IsTag(options.exclude_tags[exclude_index].tag)) {
							exclude_index = -1;
							break;
						}
					}
				}
			}
		}

		bool CreateForHierarchyIsValid(size_t include_index, size_t exclude_index, UIReflectionDrawerSearchOptions options) {
			return exclude_index == options.exclude_tags.size && include_index <= options.include_tags.size;
		}

		bool CreateForHierarchyVerifyIncludeExclude(const ReflectionType* type, UIReflectionDrawerSearchOptions options) {
			size_t include_index = options.include_tags.size;
			size_t exclude_index = options.exclude_tags.size;

			CreateForHierarchyCheckExcludeIndex(type, exclude_index, options);
			CreateForHierarchyCheckIncludeIndex(type, include_index, options);

			return CreateForHierarchyIsValid(include_index, exclude_index, options);
		}

		// Returns the full name
		Stream<char> CreateForHierarchyGetSuffixName(CapacityStream<char>& full_name, Stream<char> name, UIReflectionDrawerSearchOptions options) {
			if (options.suffix.size > 0) {
				full_name.Copy(name);
				full_name.AddStreamSafe(options.suffix);
				full_name[full_name.size] = '\0';

				return full_name;
			}
			return name;
		}

		void CreateForHierarchyAddIndex(unsigned int index, UIReflectionDrawerSearchOptions options) {
			if (options.indices != nullptr) {
				options.indices->AddAssert(index);
			}
		}

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::CreateTypesForHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options)
		{
			unsigned int count = 0;
			reflection->type_definitions.ForEach([&](const ReflectionType& type, ResourceIdentifier identifier) {
				if (type.folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(&type, options)) {
						UIReflectionType* ui_type = CreateType(&type);
						unsigned int type_index = type_definition.ValuePtrIndex(ui_type);
						CreateForHierarchyAddIndex(type_index, options);
						count++;
					}
				}
			});

			return count;
		}

		unsigned int UIReflectionDrawer::CreateTypesForHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options)
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
				const ReflectionType* type = reflection->GetType(ui_type.name);
				if (type->folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(type, options)) {
						ECS_STACK_CAPACITY_STREAM(char, full_name, 512);

						Stream<char> instance_name = ui_type.name;
						instance_name = CreateForHierarchyGetSuffixName(full_name, instance_name, options);

						CreateInstance(instance_name, &ui_type);
						CreateForHierarchyAddIndex(instances.Find(instance_name), options);
						count++;
					}
				}
			});

			return count;
		}

		unsigned int UIReflectionDrawer::CreateInstanceForHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options)
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
				const ReflectionType* type = reflection->type_definitions.GetValuePtrFromIndex(index);
				if (type->folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(type, options)) {
						UIReflectionType* ui_type = CreateType(type);
						UIReflectionInstance* instance = CreateInstance(ui_type->name, ui_type);

						unsigned int instance_index = instances.ValuePtrIndex(instance);
						CreateForHierarchyAddIndex(instance_index, options);
						count++;
					}
				}
			});

			return count;
		}

		unsigned int UIReflectionDrawer::CreateTypesAndInstancesForHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				return CreateTypesAndInstancesForHierarchy(hierarchy_index, options);
			}
			ECS_ASSERT(false);
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DisableInputWrites(UIReflectionType* type, bool all_writes)
		{
			for (size_t index = 0; index < type->fields.size; index++) {
				UIReflectionElement element_index = type->fields[index].element_index;
				if (element_index == UIReflectionElement::TextInput || element_index == UIReflectionElement::DirectoryInput
					|| element_index == UIReflectionElement::FileInput) {
					type->fields[index].configuration |= UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM;
				}
				else if (all_writes) {
					if (IsStream(type->fields[index].stream_type)) {
						type->fields[index].configuration |= UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM;
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DeallocateFieldOverride(Stream<char> tag, void* data, AllocatorPolymorphic override_allocator)
		{
			unsigned int override_index = FindFieldOverride(tag);
			ECS_ASSERT(override_index != -1);
			
			if (overrides[override_index].deallocate_function != nullptr) {
				if (override_allocator.allocator == nullptr) {
					override_allocator = GetAllocatorPolymorphic(allocator);
				}
				overrides[override_index].deallocate_function(override_allocator, data, overrides[override_index].global_data);
			}
			allocator->Deallocate(data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DeallocateInstance(UIReflectionInstance* instance) {
			DeallocateInstanceFields(instance);
			allocator->Deallocate(instance->data.buffer);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DeallocateInstanceFields(UIReflectionInstance* instance)
		{
			const UIReflectionType* type = GetType(instance->type_name);
			// The individual instance.data[index] must not be deallocated
			// since they were coallesced into a single allocation
			for (size_t index = 0; index < type->fields.size; index++) {
				bool is_stream = IsStream(type->fields[index].stream_type);
				if (!is_stream) {
					switch (type->fields[index].element_index) {
					case UIReflectionElement::IntegerInputGroup:
					case UIReflectionElement::IntegerSliderGroup:
					case UIReflectionElement::DoubleInputGroup:
					case UIReflectionElement::DoubleSliderGroup:
					case UIReflectionElement::FloatInputGroup:
					case UIReflectionElement::FloatSliderGroup:
					{
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)instance->data[index];
						allocator->Deallocate(data->values);
					}
					break;
					case UIReflectionElement::FileInput:
					case UIReflectionElement::DirectoryInput:
					case UIReflectionElement::TextInput:
					{
						UIInstanceFieldStream* data = (UIInstanceFieldStream*)instance->data[index];
						if (data->capacity != (CapacityStream<void>*)data->target_memory) {
							// It might get bound from outside so do a deallocate without assert
							if (allocator->Belongs(data->capacity)) {
								allocator->Deallocate(data->capacity);
							}
						}
						if (data->standalone_data.allocator.allocator != nullptr) {
							data->standalone_data.FreeBuffer();
						}
					}
					break;
					case UIReflectionElement::Override:
					{
						OverrideAllocationData* data = (OverrideAllocationData*)instance->data[index];
						if (overrides[data->override_index].deallocate_function != nullptr) {
							overrides[data->override_index].deallocate_function(data->initialize_allocator, data->GetData(), overrides[data->override_index].global_data);
						}
					}
					break;
					case UIReflectionElement::UserDefined:
					{
						UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)instance->data[index];
						DestroyInstance(data->instance->name);
					}
					break;
					}
				}
				else {
					// It is fine to alias the resizable stream with the capacity stream
					if (type->fields[index].stream_type == UIReflectionStreamType::Resizable || type->fields[index].stream_type == UIReflectionStreamType::Capacity) {
						// All the structs contain this as the first member variable
						// Safe to alias with UIInstanceFieldStream

						UIInstanceFieldStream* data = (UIInstanceFieldStream*)instance->data[index];
						// if a buffer is allocated, deallocate it only for non aliasing streams
						if (type->fields[index].stream_type == UIReflectionStreamType::Resizable && data->resizable != (ResizableStream<void>*)data->target_memory) {
							if (data->resizable->buffer != nullptr) {
								data->resizable->FreeBuffer();
							}
						}

						if (data->capacity != (CapacityStream<void>*)data->target_memory) {
							if (allocator->Belongs(data->capacity)) {
								allocator->Deallocate(data->capacity);
							}
							if (data->standalone_data.allocator.allocator != nullptr) {
								data->standalone_data.FreeBuffer();
							}
						}
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyInstance(unsigned int index) {
			UIReflectionInstance instance = instances.GetValueFromIndex(index);
			DeallocateInstance(&instance);
			instances.EraseFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyInstance(Stream<char> name)
		{
			unsigned int instance_index = instances.Find(name);

			if (instance_index != -1) {
				DestroyInstance(instance_index);
				return;
			}

			ECS_ASSERT(false);
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyType(Stream<char> name)
		{
			unsigned int type_index = type_definition.Find(name);
			ECS_ASSERT(type_index != -1);
			UIReflectionType type = type_definition.GetValueFromIndex(type_index);

			instances.ForEachIndex([&](unsigned int index) {
				UIReflectionInstance instance = instances.GetValueFromIndex(index);

				// Check for both cases, when an instance references the name of its type
				// or when it the type has an overriden identifier
				if (function::CompareStrings(type.name, instance.type_name) || type_definition.Find(instance.type_name) == type_index) {
					DestroyInstance(instance.name);
					return true;
				}
				return false;
			});

			for (size_t index = 0; index < type.fields.size; index++) {
				bool is_stream = IsStream(type.fields[index].stream_type);

				UIReflectionElement reflect_index = type.fields[index].element_index;
				if (!is_stream) {
					switch (reflect_index) {
					case UIReflectionElement::IntegerInput:
					case UIReflectionElement::IntegerSlider:
					{
						UIReflectionIntInputData* data = (UIReflectionIntInputData*)type.fields[index].data;
						allocator->Deallocate(data->default_value);
					}
					break;
					case UIReflectionElement::FloatInputGroup:
					case UIReflectionElement::FloatSliderGroup:
					case UIReflectionElement::DoubleInputGroup:
					case UIReflectionElement::DoubleSliderGroup:
					case UIReflectionElement::IntegerInputGroup:
					case UIReflectionElement::IntegerSliderGroup:
					{
						UIReflectionGroupData<void>* data = (UIReflectionGroupData<void>*)type.fields[index].data;
						// The boolean is prefixing this
						allocator->Deallocate(function::OffsetPointer(data->default_values, -sizeof(bool)));
					}
					break;
					case UIReflectionElement::UserDefined: 
					{
						UIReflectionUserDefinedData* data = (UIReflectionUserDefinedData*)type.fields[index].data;
						allocator->Deallocate(data->type_and_field_name.buffer);
					}
						break;
					}
				}
				else {
					if (reflect_index == UIReflectionElement::UserDefined) {
						UIReflectionStreamUserDefinedData* data = (UIReflectionStreamUserDefinedData*)type.fields[index].data;
						allocator->Deallocate(data->type_name.buffer);
					}
				}
				allocator->Deallocate(type.fields[index].data);
			}
			
			if (type.fields.size > 0) {
				allocator->Deallocate(type.fields.buffer);
			}

			if (type.groupings.size > 0) {
				for (size_t index = 0; index < type.groupings.size; index++) {
					allocator->Deallocate(type.groupings[index].name.buffer);
					if (type.groupings[index].per_element_name.size > 0) {
						allocator->Deallocate(type.groupings[index].per_element_name.buffer);
					}
				}
				allocator->Deallocate(type.groupings.buffer);
			}

			ResourceIdentifier identifier(name);
			unsigned int index = type_definition.Find(name);
			if (type.has_overriden_identifier) {
				ResourceIdentifier table_identifier = type_definition.GetIdentifierFromIndex(index);
				allocator->Deallocate(table_identifier.ptr);
			}
			type_definition.EraseFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DrawInstance(
			Stream<char> instance_name,
			const UIReflectionDrawInstanceOptions* options
		) {
			DrawInstance(GetInstance(instance_name), options);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDefaultValueAction(ActionData* action_data);

		void UIReflectionDrawer::DrawInstance(
			UIReflectionInstance* instance,
			const UIReflectionDrawInstanceOptions* options
		) {
			const UIReflectionType* type = GetType(instance->type_name);

			UIDrawer* drawer = options->drawer;

			ECS_UI_DRAWER_MODE draw_mode = drawer->draw_mode;
			unsigned int draw_mode_target = drawer->draw_mode_target;

			bool pushed_string_identifier = drawer->PushIdentifierStackStringPattern();
			drawer->PushIdentifierStackEx(instance->name);

			drawer->SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

			if (options->default_value_button.size > 0) {
				UIReflectionDefaultValueData default_data;
				default_data.instance = *instance;
				default_data.type = *type;
				drawer->Button(options->default_value_button, { UIReflectionDefaultValueAction, &default_data, sizeof(default_data) });
			}

			size_t global_grouping_index = 0;

			for (size_t index = 0; index < type->fields.size; index++) {
				void* custom_draw_data = nullptr;
				UIReflectionInstanceDrawCustom custom_drawer = nullptr;

				UIReflectionTypeFieldGrouping current_grouping;
				current_grouping.name = { nullptr, 0 };
				current_grouping.range = 1;
				current_grouping.per_element_name = { nullptr, 0 };
				for (size_t grouping_index = 0; grouping_index < type->groupings.size; grouping_index++) {
					if (type->groupings[grouping_index].field_index == index) {
						current_grouping = type->groupings[grouping_index];
						global_grouping_index++;
						break;
					}
				}

				if (options->custom_draw != nullptr) {
					// Check to see if a custom functor is provided for the type
					if (options->custom_draw->functions[(unsigned char)type->fields[index].element_index] != nullptr) {
						custom_drawer = options->custom_draw->functions[(unsigned char)type->fields[index].element_index];
						custom_draw_data = options->custom_draw->extra_data;
					}
				}

				auto draw_grouping = [&]() {
					for (size_t grouping_index = 0; grouping_index < current_grouping.range; grouping_index++) {
						Stream<char> current_field_name = type->fields[index].name;
						ECS_STACK_CAPACITY_STREAM(char, current_name_storage, 512);
						if (current_grouping.per_element_name.size > 0) {
							ECS_FORMAT_STRING(current_name_storage, "{#} {#}", current_grouping.per_element_name, grouping_index);
							current_field_name = current_name_storage;
						}

						if (custom_drawer != nullptr) {
							UIReflectionElement element_index = type->fields[index].element_index;

							bool is_instance_field_stream = type->fields[index].stream_type != UIReflectionStreamType::None || IsStreamInput(element_index);
							if (is_instance_field_stream)
							{
								UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[index];
								field_stream->CopyTarget();
							}

							// Call the custom functor
							// The struct data always contains as the first field the pointer to the data
							// It can be provided directly to the function
							void** field_data = (void**)instance->data[index];
							custom_drawer(
								drawer,
								options->config,
								options->global_configuration,
								*field_data,
								current_field_name,
								type->fields[index].stream_type,
								custom_draw_data
							);

							if (is_instance_field_stream) {
								// Mirror the stream values
								UIInstanceFieldStream* field_stream = (UIInstanceFieldStream*)instance->data[index];
								field_stream->WriteTarget();
							}
							// Go to the next field since it was overriden
							continue;
						}

						UIReflectionElement element_index = type->fields[index].element_index;
						size_t current_configuration = type->fields[index].configuration;
						size_t config_flag_count = options->config->flag_count;

						bool disable_additional_configs = false;
						if (options->field_tag_options.size > 0) {
							for (size_t subindex = 0; subindex < options->field_tag_options.size; subindex++) {
								bool matches = false;
								if (options->field_tag_options[subindex].is_tag) {
									matches = type->fields[index].tags == options->field_tag_options[subindex].tag;
								}
								else {
									matches = function::FindFirstToken(type->fields[index].tags, options->field_tag_options[subindex].tag).size > 0;
								}

								auto matched = [&]() {
									current_configuration |= options->field_tag_options[subindex].draw_config.configurations;
									UIReflectionDrawConfigCopyToNormalConfig(&options->field_tag_options[subindex].draw_config, *options->config);
									disable_additional_configs = options->field_tag_options[subindex].disable_additional_configs;
									// Check the callback
									if (options->field_tag_options[subindex].callback != nullptr) {
										UIReflectionDrawInstanceFieldTagCallbackData callback_data;
										callback_data.element = element_index;
										callback_data.field_data = instance->data[index];
										callback_data.field_name = type->fields[index].name;
										callback_data.user_data = options->field_tag_options[subindex].callback_data;
										callback_data.stream_type = type->fields[index].stream_type;
										options->field_tag_options[subindex].callback(&callback_data);
									}
								};

								if (options->field_tag_options[subindex].exclude_match) {
									if (!matches) {
										matched();
									}
								}
								else {
									if (matches) {
										matched();
									}
								}
							}
						}

						if (!disable_additional_configs) {
							for (size_t subindex = 0; subindex < options->additional_configs.size; subindex++) {
								for (size_t type_index = 0; type_index < options->additional_configs[subindex].index_count; type_index++) {
									if (options->additional_configs[subindex].index[type_index] == element_index ||
										options->additional_configs[subindex].index[type_index] == UIReflectionElement::Count) {
										current_configuration |= options->additional_configs[subindex].configurations;

										UIReflectionDrawConfigCopyToNormalConfig(options->additional_configs.buffer + subindex, *options->config);
									}
								}
							}
						}

						if (type->fields[index].element_index == UIReflectionElement::Override && options->override_additional_configs.size > 0) {
							OverrideAllocationData* override_data = (OverrideAllocationData*)instance->data[index];
							Stream<char> override_tag = overrides[override_data->override_index].tag;
							for (size_t subindex = 0; subindex < options->override_additional_configs.size; subindex++) {
								unsigned int tag_index = function::FindString(
									override_tag,
									Stream<Stream<char>>(options->override_additional_configs[subindex].override_tag, options->override_additional_configs[subindex].override_tag_count)
								);
								if (tag_index != -1) {
									// We have a match
									current_configuration |= options->override_additional_configs[subindex].base_config.configurations;
									UIReflectionDrawConfigCopyToNormalConfig(&options->override_additional_configs[subindex].base_config, *options->config);
								}
							}
						}

						current_configuration |= options->global_configuration;
						UIReflectionDrawField(
							*drawer,
							*options->config,
							current_configuration,
							current_field_name,
							instance->data[index],
							element_index,
							type->fields[index].stream_type,
							options
						);

						options->config->flag_count = config_flag_count;
						index++;
					}
					index--;
				};
				
				if (current_grouping.name.size > 0) {
					drawer->PushIdentifierStack(current_grouping.name);

					size_t current_index = index;
					drawer->CollapsingHeader(current_grouping.name, &instance->grouping_open_state[global_grouping_index - 1], draw_grouping);
					index = current_index + current_grouping.range - 1;

					drawer->PopIdentifierStack();
				}
				else {
					draw_grouping();
				}
			}

			if (pushed_string_identifier) {
				drawer->PopIdentifierStack();
			}
			drawer->PopIdentifierStack();

			drawer->SetDrawMode(draw_mode, draw_mode_target);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DrawFieldOverride(
			Stream<char> tag,
			void* data,
			void* field_data,
			UIDrawer* drawer,
			size_t configuration,
			UIDrawConfig* config,
			Stream<char> name,
			UIReflectionStreamType stream_type,
			AllocatorPolymorphic allocator
		)
		{
			unsigned int override_index = FindFieldOverride(tag);
			ECS_ASSERT(override_index != -1);

			overrides[override_index].draw_function(drawer, config, configuration, field_data, name, stream_type, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyAllFromFolderHierarchy(unsigned int hierarchy_index) {
			// Iterate through types and remove UI types and instances associated with the given hierarchy index
			// Record the positions of the UI types inside the hash table
			unsigned int ui_type_count = type_definition.GetCount();
			unsigned int* _type_mask = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * ui_type_count);
			Stream<unsigned int> type_mask(_type_mask, 0);

			type_definition.ForEachIndexConst([&](unsigned int index) {
				type_mask.Add(index);
			});

			Stream<char>* _types_to_be_deleted = (Stream<char>*)ECS_STACK_ALLOC(sizeof(Stream<char>) * type_mask.size);
			Stream<Stream<char>> types_to_be_deleted(_types_to_be_deleted, 0);
			reflection->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
				if (type.folder_hierarchy_index == hierarchy_index) {
					// Search through the UI types first and create a temporary array of those that match
					for (size_t subindex = 0; subindex < type_mask.size; subindex++) {
						UIReflectionType ui_type = type_definition.GetValueFromIndex(type_mask[subindex]);

						if (function::CompareStrings(ui_type.name, type.name)) {
							types_to_be_deleted.Add(type.name);
							break;
						}
					}
				}
			});

			// Destroy the instance that have the base types the ones that need to be deleted
			instances.ForEachIndex([&](unsigned int index) {
				UIReflectionInstance instance = instances.GetValueFromIndex(index);
				for (size_t subindex = 0; subindex < types_to_be_deleted.size; subindex++) {
					if (function::CompareStrings(instance.type_name, types_to_be_deleted[subindex])) {
						// Decrement the index afterwards - the values will be shuffled after robin hood rebalancing
						DestroyInstance(index);
						return true;
					}
				}

				return false;
			});

			// Destroy all types now
			for (size_t index = 0; index < types_to_be_deleted.size; index++) {
				DestroyType(types_to_be_deleted[index]);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::DestroyAllFromFolderHierarchy(Stream<wchar_t> hierarchy) {
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
			if (options.suffix.size == 0) {
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
					Stream<char> instance_name = instance->type_name;
					instance_name = CreateForHierarchyGetSuffixName(full_name, instance_name, options);

					if (function::CompareStrings(full_name, instance->name)) {
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

		void UIReflectionDrawer::DestroyAllInstancesFromFolderHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options)
		{
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				DestroyAllInstancesFromFolderHierarchy(hierarchy_index, options);
				return;
			}
			ECS_ASSERT(false);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::FindFieldOverride(Stream<char> tag) const
		{
			for (unsigned int index = 0; index < overrides.size; index++) {
				if (function::CompareStrings(overrides[index].tag, tag)) {
					return index;
				}
			}
			return -1;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetInstanceStreamSizes(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data)
		{
			GetInstanceStreamSizes(GetInstance(instance_name), data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetInstanceStreamSizes(const UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data)
		{
			const UIReflectionType* type = GetType(instance->name);
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
				const ReflectionType* type = reflection->GetType(ui_type.name);
				if (type->folder_hierarchy_index == hierarchy_index) {
					if (CreateForHierarchyVerifyIncludeExclude(type, options)) {
						options.indices->AddAssert(index);
					}
				}
			});
		}


		void UIReflectionDrawer::GetHierarchyTypes(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options) {
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
				const ReflectionType* type = reflection->GetType(instance.type_name);
				if (type->folder_hierarchy_index == hierarchy_index && CreateForHierarchyVerifyIncludeExclude(type, options)) {
					if (options.suffix.size == 0) {
						options.indices->AddAssert(index);
					}
					else {
						ECS_STACK_CAPACITY_STREAM(char, full_name, 256);
						Stream<char> instance_name = instance.type_name;
						instance_name = CreateForHierarchyGetSuffixName(full_name, instance_name, options);

						if (function::CompareStrings(full_name, instance.name)) {
							options.indices->AddAssert(index);
						}
					}
				}
			});
		}

		void UIReflectionDrawer::GetHierarchyInstances(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options) {
			unsigned int hierarchy_index = reflection->GetHierarchyIndex(hierarchy);
			if (hierarchy_index != -1) {
				GetHierarchyInstances(hierarchy_index, options);
				return;
			}
			ECS_ASSERT(false);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		const UIReflectionType* UIReflectionDrawer::GetType(Stream<char> name) const
		{
			ResourceIdentifier identifier(name);
			return type_definition.GetValuePtr(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		const UIReflectionType* UIReflectionDrawer::GetType(unsigned int index) const
		{
			return type_definition.GetValuePtrFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::GetType(Stream<char> name)
		{
			ResourceIdentifier identifier(name);
			unsigned int index = type_definition.Find(identifier);
			return index == -1 ? nullptr : type_definition.GetValuePtrFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionType* UIReflectionDrawer::GetType(unsigned int index)
		{
			return type_definition.IsItemAt(index) ? type_definition.GetValuePtrFromIndex(index) : nullptr;
		}

		// ------------------------------------------------------------------------------------------------------------------------------
		
		const UIReflectionInstance* UIReflectionDrawer::GetInstance(Stream<char> name) const {
			ResourceIdentifier identifier(name);
			return instances.GetValuePtr(identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		const UIReflectionInstance* UIReflectionDrawer::GetInstance(unsigned int index) const {
			return instances.GetValuePtrFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::GetInstance(Stream<char> name)
		{
			ResourceIdentifier identifier(name);
			unsigned int index = instances.Find(identifier);
			return index == -1 ? nullptr : GetInstance(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		UIReflectionInstance* UIReflectionDrawer::GetInstance(unsigned int index)
		{
			return instances.GetValuePtrFromIndex(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetTypeMatchingFields(const UIReflectionType* type, UIReflectionElement element_index, CapacityStream<unsigned int>& indices) const {
			for (unsigned int index = 0; index < type->fields.size; index++) {
				if (type->fields[index].element_index == element_index) {
					indices.AddAssert(index);
				}
			}
		}
		
		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::GetTypeMatchingFields(const UIReflectionType* type, UIReflectionStreamType stream_type, CapacityStream<unsigned int>& indices) const {
			for (unsigned int index = 0; index < type->fields.size; index++) {
				if (type->fields[index].stream_type == stream_type) {
					indices.AddAssert(index);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		CapacityStream<void> UIReflectionDrawer::GetInputStream(const UIReflectionInstance* instance, unsigned int field_index) const
		{
			const UIReflectionType* type = GetType(instance->type_name);
			ECS_ASSERT(
				type->fields[field_index].element_index == UIReflectionElement::TextInput ||
				type->fields[field_index].element_index == UIReflectionElement::DirectoryInput ||
				type->fields[field_index].element_index == UIReflectionElement::FileInput
			);

			CapacityStream<void>* field_stream = *(CapacityStream<void>**)instance->data[field_index];
			return *field_stream;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::GetTypeFieldIndex(const UIReflectionType* type, Stream<char> field_name) const
		{
			unsigned int field_index = 0;
			while ((field_index < type->fields.size) && type->fields[field_index].name != field_name) {
				field_index++;
			}
			ECS_ASSERT(field_index < type->fields.size);
			return field_index;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIReflectionDrawer::GetTypeFieldIndex(Stream<char> type_name, Stream<char> field_name)
		{
			const UIReflectionType* type = GetType(type_name);
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

		void* UIReflectionDrawer::InitializeFieldOverride(Stream<char> tag, Stream<char> name, AllocatorPolymorphic user_defined_allocator)
		{
			user_defined_allocator = user_defined_allocator.allocator == nullptr ? GetAllocatorPolymorphic(allocator) : user_defined_allocator;

			unsigned int override_index = FindFieldOverride(tag);
			ECS_ASSERT(override_index != -1);

			void* allocation = allocator->Allocate(overrides[override_index].draw_data_size);
			memset(allocation, 0, overrides[override_index].draw_data_size);
			overrides[override_index].initialize_function(user_defined_allocator, this, name, allocation, overrides[override_index].global_data);

			return allocation;
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeField(Stream<char> type_name, Stream<char> field_name)
		{
			UIReflectionType* type = GetType(type_name);
			OmitTypeField(type, field_name);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeField(UIReflectionType* type, Stream<char> field_name)
		{
			unsigned int field_index = GetTypeFieldIndex(type, field_name);
			allocator->Deallocate(type->fields[field_index].data);
			type->fields.Remove(field_index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeFields(Stream<char> type_name, Stream<Stream<char>> fields)
		{
			UIReflectionType* type = GetType(type_name);
			OmitTypeFields(type, fields);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::OmitTypeFields(UIReflectionType* type, Stream<Stream<char>> fields)
		{
			for (size_t index = 0; index < fields.size; index++) {
				unsigned int field_index = GetTypeFieldIndex(type, fields[index]);
				allocator->Deallocate(type->fields[field_index].data);
				type->fields.Remove(field_index);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::RebindInstancePtrs(UIReflectionInstance* instance, void* data)
		{
			DeallocateInstanceFields(instance);
			BindInstancePtrs(instance, data);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDrawer::SetFieldOverride(const UIReflectionFieldOverride* override)
		{
			Stream<char> tag = function::StringCopy(GetAllocatorPolymorphic(allocator), override->tag);
			UIReflectionFieldOverride new_override;
			memcpy(&new_override, override, sizeof(new_override));
			new_override.tag = tag;
			new_override.global_data = function::CopyNonZero(GetAllocatorPolymorphic(allocator), new_override.global_data, new_override.global_data_size);

			overrides.Add(&new_override);
		}

		// ------------------------------------------------------------------------------------------------------------------------------

		void UIReflectionDefaultValueAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIReflectionDefaultValueData* default_data = (UIReflectionDefaultValueData*)_data;

#define CASE_START(type, data_type) case UIReflectionElement::type: { data_type* data = (data_type*)default_data->instance.data[index]; 
#define CASE_END } break;

			for (size_t index = 0; index < default_data->type.fields.size; index++) {
				switch (default_data->type.fields[index].element_index) {
					CASE_START(Color, UIReflectionColorData)
					*data->color = data->default_color;
					CASE_END

					CASE_START(FloatInput, UIReflectionFloatInputData)
					*data->value = data->default_value;
					CASE_END

					CASE_START(DoubleInput, UIReflectionDoubleInputData)
					*data->value = data->default_value;
					CASE_END

					CASE_START(IntegerInput, UIReflectionIntInputData)
					memcpy(data->value_to_modify, data->default_value, data->byte_size);
					CASE_END

					CASE_START(FloatSlider, UIReflectionFloatSliderData)
					*data->value_to_modify = data->default_value;
					CASE_END

					CASE_START(DoubleSlider, UIReflectionDoubleSliderData)
					*data->value_to_modify = data->default_value;
					CASE_END

					CASE_START(IntegerSlider, UIReflectionIntSliderData)
					memcpy(data->value_to_modify, data->default_value, data->byte_size);
					CASE_END

					CASE_START(FloatInputGroup, UIReflectionGroupData<float>)
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					CASE_START(DoubleInputGroup, UIReflectionGroupData<double>)
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					// doesn't matter what type we choose here, because it will simply get memcpy'ed
					CASE_START(IntegerInputGroup, UIReflectionGroupData<int8_t>)
					uintptr_t value_ptr = (uintptr_t)data->default_values;

					for (size_t subindex = 0; subindex < data->count; subindex++) {
						memcpy(data->values[subindex], (void*)value_ptr, data->byte_size);
						value_ptr += data->byte_size;
					}
					CASE_END

					CASE_START(FloatSliderGroup, UIReflectionGroupData<float>)
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					CASE_START(DoubleSliderGroup, UIReflectionGroupData<double>)
					for (size_t subindex = 0; subindex < data->count; subindex++) {
						*data->values[subindex] = data->default_values[subindex];
					}
					CASE_END

					// doesn't matter what type we choose here, because it will simply get memcpy'ed
					CASE_START(IntegerSliderGroup, UIReflectionGroupData<int8_t>)
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