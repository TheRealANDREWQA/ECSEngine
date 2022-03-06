#pragma once
#include "UIDrawer.h"
#include "../../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	namespace Tools {

		constexpr size_t UI_REFLECTION_DRAWER_TYPE_TABLE_COUNT = 128;
		constexpr size_t UI_REFLECTION_DRAWER_INSTANCE_TABLE_COUNT = 256;

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

		struct UIReflectionTypeField {
			size_t configuration;
			void* data;
			const char* name;
			UIReflectionStreamType stream_type;
			UIReflectionIndex reflection_index;
			unsigned short byte_size;
			unsigned int pointer_offset;
		};

		// Only drawer will be called, the initializer is not involved
		using UIReflectionFieldDraw = void (*)(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		// ------------------------------------------------------------ Basic ----------------------------------------------------------------------

		ECSENGINE_API void UIReflectionFloatSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionDoubleSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionIntSlider(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionFloatInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionDoubleInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionIntInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionTextInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionColor(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionColorFloat(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionCheckBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionComboBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionFloatSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionDoubleSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionIntSliderGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionFloatInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionDoubleInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionIntInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

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

		ECSENGINE_API void UIReflectionStreamFloatInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamDoubleInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamIntInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamTextInput(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamColor(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamColorFloat(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamCheckBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		// Probably doesn't make too much sense to have a stream of combo boxes
		ECSENGINE_API void UIReflectionStreamComboBox(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamFloatInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamDoubleInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		ECSENGINE_API void UIReflectionStreamIntInputGroup(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

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

		ECSENGINE_API void UIReflectionSetColorDefaultData(UIReflectionTypeField* field, const void* data);

		// Currently it does nothing. It serves as a placeholder for the jump table. Maybe add support for it later on.
		ECSENGINE_API void UIReflectionSetTextInputDefaultData(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetColorFloatDefaultData(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetCheckBoxDefaultData(UIReflectionTypeField* field, const void* data);

		ECSENGINE_API void UIReflectionSetComboBoxDefaultData(UIReflectionTypeField* field, const void* data);

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
			UIReflectionSetGroupDefaultData,
			UIReflectionSetTextInputDefaultData,
			UIReflectionSetColorDefaultData,
			UIReflectionSetColorFloatDefaultData,
			UIReflectionSetCheckBoxDefaultData,
			UIReflectionSetComboBoxDefaultData
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

		struct UIReflectionType {
			const char* name;
			CapacityStream<UIReflectionTypeField> fields;
		};

		struct UIReflectionFloatSliderData {
			float* value_to_modify;
			const char* name;
			float lower_bound;
			float upper_bound;
			float default_value;
			unsigned int precision = 2;
		};

		struct UIReflectionDoubleSliderData {
			double* value_to_modify;
			const char* name;
			double lower_bound;
			double upper_bound;
			double default_value;
			unsigned int precision = 2;
		};

		struct UIReflectionIntSliderData {
			void* value_to_modify;
			const char* name;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
		};

		struct UIReflectionFloatInputData {
			float* value;
			const char* name;
			float lower_bound;
			float upper_bound;
			float default_value;
		};

		struct UIReflectionDoubleInputData {
			double* value;
			const char* name;
			double lower_bound;
			double upper_bound;
			double default_value;
		};

		struct UIReflectionIntInputData {
			void* value_to_modify;
			const char* name;
			void* lower_bound;
			void* upper_bound;
			void* default_value;
			unsigned int byte_size;
		};

		struct UIReflectionTextInputData {
			CapacityStream<char>* characters;
			const char* name;
		};

		struct UIReflectionColorData {
			Color* color;
			const char* name;
			Color default_color;
		};

		struct UIReflectionColorFloatData {
			ColorFloat* color;
			const char* name;
			ColorFloat default_color;
		};

		struct UIReflectionCheckBoxData {
			bool* value;
			const char* name;
			bool default_value;
		};

		struct UIReflectionComboBoxData {
			unsigned char* active_label;
			const char* name;
			Stream<const char*> labels;
			unsigned int label_display_count;
			unsigned char default_label;
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

		struct UIReflectionStreamFloatInputData {
			CapacityStream<float>* values;
			const char* name;
		};

		struct UIReflectionStreamDoubleInputData {
			CapacityStream<double>* values;
			const char* name;
		};

		struct UIReflectionStreamIntInputData {
			CapacityStream<void>* values;
			const char* name;
		};

		struct UIReflectionStreamTextInputData {
			CapacityStream<CapacityStream<char>>* values;
			const char* name;
		};

		struct UIReflectionStreamColorData {
			CapacityStream<Color>* values;
			const char* name;
		};

		struct UIReflectionStreamColorFloatData {
			CapacityStream<ColorFloat>* values;
			const char* name;
		};

		struct UIReflectionStreamCheckBoxData {
			CapacityStream<bool>* values;
			const char* name;
		};

		struct UIReflectionStreamComboBoxData {
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

		struct UIReflectionInstance {
			const char* type_name;
			const char* name;
			Stream<UIReflectionInstanceFieldData> datas;
		};

		using UIReflectionHash = HashFunctionMultiplyString;
		using UIReflectionTypeTable = HashTable<UIReflectionType, ResourceIdentifier, HashFunctionPowerOfTwo, UIReflectionHash>;
		using UIReflectionInstanceTable = HashTable<UIReflectionInstance, ResourceIdentifier, HashFunctionPowerOfTwo, UIReflectionHash>;

		struct UIReflectionBindDefaultValue {
			const char* field_name;
			void* value;
		};

		struct UIReflectionBindRange {
			const char* field_name;
			void* min;
			void* max;
		};

		using UIReflectionBindLowerBound = UIReflectionBindDefaultValue;
		using UIReflectionBindUpperBound = UIReflectionBindDefaultValue;
		
		struct UIReflectionBindPrecision {
			const char* field_name;
			unsigned int precision;
		};

		struct UIReflectionBindTextInput {
			const char* field_name;
			ECSEngine::CapacityStream<char>* stream;
		};

		struct UIReflectionBindStreamCapacity {
			const char* field_name;
			size_t capacity;
		};

		struct UIReflectionBindStreamBuffer {
			const char* field_name;
			void* new_buffer;
		};
		
		struct UIReflectionStreamCopy {
			const char* field_name;
			void* destination;
			size_t element_count;
		};

#define ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT 8

		struct UIReflectionDrawConfig {
			UIReflectionIndex index[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			void* configs[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			size_t config_size[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			size_t associated_bits[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			size_t configurations = 0;
			unsigned char config_count = 0;
			unsigned char index_count = 1;
		};

		template<typename Config>
		void UIReflectionDrawConfigAddConfig(UIReflectionDrawConfig* draw_config, Config* config) {
			ECS_ASSERT(draw_config->config_count < ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT);
			draw_config->configs[draw_config->config_count] = config;
			size_t associated_bit = config->GetAssociatedBit();
			draw_config->associated_bits[draw_config->config_count] = associated_bit;
			draw_config->config_size[draw_config->config_count] = sizeof(Config);
			draw_config->configurations |= draw_config->associated_bits[draw_config->config_count];

			draw_config->config_count++;
		}

		ECSENGINE_API void UIReflectionDrawConfigCopyToNormalConfig(const UIReflectionDrawConfig* ui_config, UIDrawConfig& config);

		// Responsible for creating type definitions and drawing of C++ types
		struct ECSENGINE_API UIReflectionDrawer {
			UIReflectionDrawer(
				UIToolsAllocator* allocator,
				Reflection::ReflectionManager* reflection,
				size_t type_table_count = UI_REFLECTION_DRAWER_TYPE_TABLE_COUNT,
				size_t instance_table_count = UI_REFLECTION_DRAWER_INSTANCE_TABLE_COUNT
			);

			UIReflectionDrawer(const UIReflectionDrawer& other) = default;
			UIReflectionDrawer& operator = (const UIReflectionDrawer& other) = default;

			void AddTypeConfiguration(const char* type_name, const char* field_name, size_t field_configuration);
			void AddTypeConfiguration(UIReflectionType* type, const char* field_name, size_t field_configuration);

			void BindInstanceData(const char* instance_name, const char* field_name, void* field_data);
			void BindInstanceData(UIReflectionInstance* instance, const char* field_name, void* field_data);

			void BindTypeData(const char* type_name, const char* field_name, void* field_data);
			void BindTypeData(UIReflectionType* type, const char* field_name, void* field_data);

			void BindTypeLowerBounds(const char* type_name, Stream<UIReflectionBindLowerBound> data);
			void BindTypeLowerBounds(UIReflectionType* type, Stream<UIReflectionBindLowerBound> data);
			// It will take the values directly from the type - only for those fields which allow
			void BindTypeLowerBounds(UIReflectionType* type, const void* data);

			void BindTypeUpperBounds(const char* type_name, Stream<UIReflectionBindUpperBound> data);
			void BindTypeUpperBounds(UIReflectionType* type, Stream<UIReflectionBindUpperBound> data);
			// It will take the values directly from the type - only for those fields which allow
			void BindTypeUpperBounds(UIReflectionType* type, const void* data);

			void BindTypeDefaultData(const char* type_name, Stream<UIReflectionBindDefaultValue> data);
			void BindTypeDefaultData(UIReflectionType* type, Stream<UIReflectionBindDefaultValue> data);
			// It will take the values directly from the type - only for those fields which allow
			void BindTypeDefaultData(UIReflectionType* type, const void* data);

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

			void BindInstanceStreamSize(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamSize(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);

			void BindInstanceStreamBuffer(const char* instance_name, Stream<UIReflectionBindStreamBuffer> data);
			void BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<UIReflectionBindStreamBuffer> data);

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

			// It will create a type for each reflected type from the given hierarchy.
			// Returns how many types were created
			unsigned int CreateTypesForHierarchy(unsigned int hierarchy_index, bool exclude_components = false);
			// It will create a type for each reflected type from the given hierarchy.
			// Returns how many types were created
			unsigned int CreateTypesForHierarchy(const wchar_t* hierarchy, bool exclude_components = false);

			// It will create an instance for each type with the given hierarchy. The name of the instance
			// is identical to that of the type
			// Returns how many instances were created
			unsigned int CreateInstanceForHierarchy(unsigned int hierarchy_index);
			// It will create an instance for each type with the given hierarchy. The name of the instance
			// is identical to that of the type
			// Returns how many instances were created
			unsigned int CreateInstanceForHierarchy(const wchar_t* hierarchy);

			// It will create a type and an instance for each reflected type from the given hierarchy.
			// The name of the instance is identical to that of the type
			// Returns how many instances were created
			unsigned int CreateTypesAndInstancesForHierarchy(unsigned int hierarchy_index, bool exclude_components = false);

			// It will create a type and an instance for each reflected type from the given hierarchy.
			// The name of the instance is identical to that of the type
			// Returns how many instances were created
			unsigned int CreateTypesAndInstancesForHierarchy(const wchar_t* hierarchy, bool exclude_components = false);

			void DestroyInstance(unsigned int index);
			void DestroyInstance(const char* name);

			void DestroyType(const char* name);

			// The additional configuration will be applied to all fields
			// It can be used to set size configs, text parameters, alignments
			void DrawInstance(
				const char* ECS_RESTRICT instance_name,
				UIDrawer& drawer, 
				UIDrawConfig& config,
				Stream<UIReflectionDrawConfig> additional_configs = {nullptr, 0},
				const char* ECS_RESTRICT default_value_button = nullptr
			);

			// The additional configuration will be applied to all fields
			// It can be used to set size configs, text parameters, alignments
			void DrawInstance(
				UIReflectionInstance* instance,
				UIDrawer& drawer,
				UIDrawConfig& config,
				Stream<UIReflectionDrawConfig> additional_configs = { nullptr, 0 },
				const char* default_value_button = nullptr
			);

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(unsigned int hierarchy_index);

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(const wchar_t* hierarchy);

			// It will fill in the capacity field
			void GetInstanceStreamSizes(const char* instance_name, Stream<UIReflectionBindStreamCapacity> data);
			// It will fill in the capacity field
			void GetInstanceStreamSizes(const UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);

			void GetHierarchyTypes(unsigned int hierarchy_index, CapacityStream<unsigned int>& indices);
			void GetHierarchyTypes(const wchar_t* hierarchy, CapacityStream<unsigned int>& indices);

			void GetHierarchyInstances(unsigned int hierarchy_index, CapacityStream<unsigned int>& indices);
			void GetHierarchyInstances(const wchar_t* hierarchy, CapacityStream<unsigned int>& indices);

			UIReflectionType GetType(const char* name) const;
			UIReflectionType GetType(unsigned int index) const;

			UIReflectionType* GetTypePtr(const char* name) const;
			UIReflectionType* GetTypePtr(unsigned int index) const;

			unsigned int GetTypeCount() const;
			unsigned int GetInstanceCount() const;

			UIReflectionInstance GetInstance(const char* name) const;
			UIReflectionInstance GetInstance(unsigned int index) const;

			UIReflectionInstance* GetInstancePtr(const char* name) const;
			UIReflectionInstance* GetInstancePtr(unsigned int index) const;
			
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

		struct UIReflectionDefaultValueData {
			UIReflectionType type;
			UIReflectionInstance instance;
		};

		void UIReflectionDefaultValueAction(ActionData* action_data);

	}
}