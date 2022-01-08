#pragma once
#include "UIDrawer.h"
#include "../../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	namespace Tools {

		constexpr size_t ui_reflection_drawer_type_table_count = 128;
		constexpr size_t ui_reflection_drawer_instance_table_count = 256;

		enum class UIReflectionIndex : unsigned char {
			FloatSlider,
			DoubleSlider,
			IntegerSlider,
			FloatInput,
			DoubleInput,
			IntegerInput,
			FloatSliderGroup,
			DoubleSliderGroup,
			IntegerSliderGroup,
			FloatInputGroup,
			DoubleInputGroup,
			IntegerInputGroup,
			TextInput,
			Color,
			ColorFloat,
			CheckBox,
			ComboBox,
			Count
		};

		enum class UIReflectionStreamType : unsigned char {
			None,
			Basic,
			Count
		};

		struct ECSENGINE_API UIReflectionTypeField {
			size_t configuration;
			void* data;
			const char* name;
			UIReflectionStreamType stream_type;
			UIReflectionIndex reflection_index;
			unsigned short byte_size;
			unsigned int pointer_offset;
		};

		// Only drawer will be called, the initializer is not involved
		using UIReflectionFieldDraw = void (*)(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		// ------------------------------------------------------------ Basic ----------------------------------------------------------------------

		ECSENGINE_API void UIReflectionFloatSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionDoubleSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionIntSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionFloatInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionDoubleInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionIntInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionTextInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionColor(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionColorFloat(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionCheckBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionComboBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionFloatSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionDoubleSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionIntSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionFloatInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionDoubleInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionIntInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		constexpr UIReflectionFieldDraw UI_REFLECTION_FIELD_BASIC_DRAW[] = {
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
			UIReflectionComboBox
		};

		// ------------------------------------------------------------ Basic ----------------------------------------------------------------------

		// ------------------------------------------------------------ Stream ----------------------------------------------------------------------

		ECSENGINE_API void UIReflectionStreamFloatInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamDoubleInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamIntInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamTextInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamColor(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamColorFloat(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamCheckBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		// Probably doesn't make too much sense to have a stream of combo boxes
		ECSENGINE_API void UIReflectionStreamComboBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamFloatInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamDoubleInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		ECSENGINE_API void UIReflectionStreamIntInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		constexpr UIReflectionFieldDraw UI_REFLECTION_FIELD_STREAM_DRAW[] = {
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
			UIReflectionComboBox
		};

		// ------------------------------------------------------------ Stream ----------------------------------------------------------------------

		using UIReflectionSetLowerBound = void (*)(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetFloatLowerBound(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetDoubleLowerBound(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetIntLowerBound(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetGroupLowerBound(UIReflectionTypeField* field, const void* data);

		constexpr UIReflectionSetLowerBound UI_REFLECTION_SET_LOWER_BOUND[] = {
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

		using UIReflectionSetUpperBound = UIReflectionSetLowerBound;

		ECSENGINE_API void UIReflectionSetFloatUpperBound(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetDoubleUpperBound(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetIntUpperBound(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetGroupUpperBound(UIReflectionTypeField* field, const void* data);

		constexpr UIReflectionSetUpperBound UI_REFLECTION_SET_UPPER_BOUND[] = {
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

		using UIReflectionSetDefaultData = UIReflectionSetLowerBound;

		ECSENGINE_API void UIReflectionSetFloatDefaultData(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetDoubleDefaultData(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetIntDefaultData(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetGroupDefaultData(UIReflectionTypeField* field, const void* data);

		constexpr UIReflectionSetDefaultData UI_REFLECTION_SET_DEFAULT_DATA[] = {
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
			UIReflectionSetGroupDefaultData
		};

		using UIReflectionSetRange = void (*)(UIReflectionTypeField* field, const void* min, const void* max);

		ECSENGINE_API void UIReflectionSetFloatRange(UIReflectionTypeField* field, const void* min, const void* max);

		ECSENGINE_API void UIReflectionSetDoubleRange(UIReflectionTypeField* field, const void* min, const void* max);

		ECSENGINE_API void UIReflectionSetIntRange(UIReflectionTypeField* field, const void* min, const void* max);

		ECSENGINE_API void UIReflectionSetGroupRange(UIReflectionTypeField* field, const void* min, const void* max);

		constexpr UIReflectionSetRange UI_REFLECTION_SET_RANGE[] = {
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

		struct ECSENGINE_API UIReflectionType {
			const char* name;
			CapacityStream<UIReflectionTypeField> fields;
		};

		struct ECSENGINE_API UIReflectionFloatSliderData {
			float* value_to_modify;
			const char* name;
			float lower_bound;
			float upper_bound;
			float default_value;
			unsigned int precision = 2;
		};

		struct ECSENGINE_API UIReflectionDoubleSliderData {
			double* value_to_modify;
			const char* name;
			double lower_bound;
			double upper_bound;
			double default_value;
			unsigned int precision = 2;
		};

		struct ECSENGINE_API UIReflectionIntSliderData {
			void* value_to_modify;
			const char* name;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
		};

		struct ECSENGINE_API UIReflectionFloatInputData {
			float* value;
			const char* name;
			float lower_bound;
			float upper_bound;
			float default_value;
		};

		struct ECSENGINE_API UIReflectionDoubleInputData {
			double* value;
			const char* name;
			double lower_bound;
			double upper_bound;
			double default_value;
		};

		struct ECSENGINE_API UIReflectionIntInputData {
			void* value_to_modify;
			const char* name;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
		};

		struct ECSENGINE_API UIReflectionTextInputData {
			CapacityStream<char>* characters;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionColorData {
			Color* color;
			const char* name;
			Color default_color;
		};

		struct ECSENGINE_API UIReflectionColorFloatData {
			ColorFloat* color;
			const char* name;
			ColorFloat default_color;
		};

		struct ECSENGINE_API UIReflectionCheckBoxData {
			bool* value;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionComboBoxData {
			unsigned char* active_label;
			const char* name;
			Stream<const char*> labels;
			unsigned int label_display_count;
			unsigned int default_label;
		};

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

		struct ECSENGINE_API UIReflectionStreamFloatInputData {
			CapacityStream<float>* values;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionStreamDoubleInputData {
			CapacityStream<double>* values;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionStreamIntInputData {
			CapacityStream<void>* values;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionStreamTextInputData {
			CapacityStream<CapacityStream<char>>* values;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionStreamColorData {
			CapacityStream<Color>* values;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionStreamColorFloatData {
			CapacityStream<ColorFloat>* values;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionStreamCheckBoxData {
			CapacityStream<bool>* values;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionStreamComboBoxData {
			CapacityStream<unsigned char>* values;
			Stream<const char*>* label_names;
			const char* name;
		};

		struct UIReflectionStreamInputGroupData {
			CapacityStream<void>* values;
			const char* group_name;
			unsigned int basic_type_count;
		};

		struct UIReflectionInstanceFieldData {
			void* struct_data;
			CapacityStream<void>* stream_data;
		};

		struct ECSENGINE_API UIReflectionInstance {
			const char* type_name;
			const char* name;
			Stream<UIReflectionInstanceFieldData> datas;
		};

		using UIReflectionHash = HashFunctionMultiplyString;
		using UIReflectionTypeTable = containers::HashTable<UIReflectionType, ResourceIdentifier, HashFunctionPowerOfTwo, UIReflectionHash>;
		using UIReflectionInstanceTable = containers::HashTable<UIReflectionInstance, ResourceIdentifier, HashFunctionPowerOfTwo, UIReflectionHash>;

		struct ECSENGINE_API UIReflectionBindDefaultValue {
			const char* field_name;
			void* value;
		};

		struct ECSENGINE_API UIReflectionBindRange {
			const char* field_name;
			void* min;
			void* max;
		};

		using UIReflectionBindLowerBound = UIReflectionBindDefaultValue;
		using UIReflectionBindUpperBound = UIReflectionBindDefaultValue;
		
		struct ECSENGINE_API UIReflectionBindPrecision {
			const char* field_name;
			unsigned int precision;
		};

		struct ECSENGINE_API UIReflectionBindTextInput {
			const char* field_name;
			ECSEngine::containers::CapacityStream<char>* stream;
		};

		struct ECSENGINE_API UIReflectionBindStreamCapacity {
			const char* field_name;
			size_t capacity;
		};

		struct ECSENGINE_API UIReflectionStreamCopy {
			const char* field_name;
			void* destination;
			size_t element_count;
		};

		// Responsible for creating type definitions and drawing of C++ types
		struct ECSENGINE_API UIReflectionDrawer {
			UIReflectionDrawer(
				UIToolsAllocator* allocator,
				Reflection::ReflectionManager* reflection,
				size_t type_table_count = ui_reflection_drawer_type_table_count,
				size_t instance_table_count = ui_reflection_drawer_instance_table_count
			);

			UIReflectionDrawer(const UIReflectionDrawer& other) = default;
			UIReflectionDrawer& operator = (const UIReflectionDrawer& other) = default;

			void AddTypeConfiguration(const char* type_name, const char* field_name, size_t field_configuration);

			void BindInstanceData(const char* instance_name, const char* field_name, void* field_data);
			void BindInstanceData(UIReflectionInstance* instance, const char* field_name, void* field_data);

			void BindTypeData(const char* type_name, const char* field_name, void* field_data);
			void BindTypeData(UIReflectionType* type, const char* field_name, void* field_data);

			void BindTypeLowerBounds(const char* type_name, Stream<UIReflectionBindLowerBound> data);
			void BindTypeLowerBounds(UIReflectionType* type, Stream<UIReflectionBindLowerBound> data);

			void BindTypeUpperBounds(const char* type_name, Stream<UIReflectionBindUpperBound> data);
			void BindTypeUpperBounds(UIReflectionType* type, Stream<UIReflectionBindUpperBound> data);

			void BindTypeDefaultData(const char* type_name, Stream<UIReflectionBindDefaultValue> data);
			void BindTypeDefaultData(UIReflectionType* type, Stream<UIReflectionBindDefaultValue> data);

			void BindTypeRange(const char* type_name, Stream<UIReflectionBindRange> data);
			void BindTypeRange(UIReflectionType* type, Stream<UIReflectionBindRange> data);

			void BindTypePrecision(const char* type_name, Stream<UIReflectionBindPrecision> data);
			void BindTypePrecision(UIReflectionType* type, Stream<UIReflectionBindPrecision> data);

			// It requires access to a ReflectionType
			void BindInstancePtrs(const char* instance_name, void* data);
			// It requires access to a ReflectionType
			void BindInstancePtrs(UIReflectionInstance* instance, void* data);

			// The ReflectionType if it is created outside
			void BindInstancePtrs(UIReflectionInstance* instance, void* data, Reflection::ReflectionType type);

			void BindInstanceTextInput(const char* instance_name, Stream<UIReflectionBindTextInput> data);
			void BindInstanceTextInput(UIReflectionInstance* instance, Stream<UIReflectionBindTextInput> data);

			void BindInstanceStreamCapacity(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);

			void BindInstanceStreamInitialSize(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamInitialSize(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);

			// It will fill in the count for each field
			void CopyInstanceStreams(const char* instance_name, Stream<UIReflectionStreamCopy> data);
			// It will fill in the count for each field
			void CopyInstanceStreams(UIReflectionInstance* instance, Stream<UIReflectionStreamCopy> data);

			void ChangeInputToSlider(const char* type_name, const char* field_name);
			void ChangeInputToSlider(UIReflectionType* type, const char* field_name);

			UIReflectionType* CreateType(const char* name);
			UIReflectionType* CreateType(Reflection::ReflectionType type);
			UIReflectionInstance* CreateInstance(const char* name, const char* type_name);
			UIReflectionInstance* CreateInstance(const char* name, const UIReflectionType* type);

			void DestroyInstance(unsigned int index);
			void DestroyInstance(const char* name);

			void DestroyType(const char* name);

			void DrawInstance(const char* ECS_RESTRICT instance_name, UIDrawer& drawer, UIDrawConfig& config, const char* ECS_RESTRICT default_value_button = nullptr);

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(unsigned int hierarchy_index);

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(const wchar_t* hierarchy);

			// It will fill in the capacity field
			void GetInstanceStreamSizes(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data);
			// It will fill in the capacity field
			void GetInstanceStreamSizes(const UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);

			UIReflectionType GetType(const char* name) const;
			UIReflectionType* GetTypePtr(const char* name) const;

			UIReflectionInstance GetInstance(const char* name) const;
			UIReflectionInstance* GetInstancePtr(const char* name) const;
			
			unsigned int GetTypeFieldIndex(UIReflectionType type, const char* field_name) const;
			unsigned int GetTypeFieldIndex(const char* ECS_RESTRICT type_name, const char* ECS_RESTRICT field_name);

			void OmitTypeField(const char* type_name, const char* field_name);
			void OmitTypeField(UIReflectionType* type, const char* field_name);

			void OmitTypeFields(const char* type_name, Stream<const char*> fields);
			void OmitTypeFields(UIReflectionType* type, Stream<const char*> fields);

			Reflection::ReflectionManager* reflection;
			UIToolsAllocator* allocator;
			UIReflectionTypeTable type_definition;
			UIReflectionInstanceTable instances;
		};

		struct ECSENGINE_API UIReflectionDefaultValueData {
			UIReflectionType type;
			UIReflectionInstance instance;
		};

		void UIReflectionDefaultValueAction(ActionData* action_data);

	}
}