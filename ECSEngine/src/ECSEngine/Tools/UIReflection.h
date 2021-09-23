#pragma once
#include "UIDrawer.h"
#include "../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	namespace Tools {

		constexpr size_t ui_reflection_drawer_type_table_count = 128;
		constexpr size_t ui_reflection_drawer_instance_table_count = 256;

		enum class ECSENGINE_API UIReflectionIndex : unsigned char {
			FloatSlider,
			DoubleSlider,
			IntegerSlider,
			FloatInput,
			DoubleInput,
			IntegerInput,
			TextInput,
			Color,
			CheckBox,
			ComboBox,
			FloatSliderGroup,
			DoubleSliderGroup,
			IntegerSliderGroup,
			FloatInputGroup,
			DoubleInputGroup,
			IntegerInputGroup,
			Count
		};

		template<bool initialize>
		using UIReflectionFieldDraw = void (*)(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionFloatSlider(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionDoubleSlider(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionIntSlider(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionFloatInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionDoubleInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionIntInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionTextInput(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionColor(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionCheckBox(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionComboBox(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionFloatSliderGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionDoubleSliderGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionIntSliderGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionFloatInputGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionDoubleInputGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		template<bool initialize>
		void ECSENGINE_API UIReflectionIntInputGroup(UIDrawer<initialize>& drawer, UIDrawConfig& config, size_t configuration_flags, void* data);

		constexpr UIReflectionFieldDraw<false> ui_reflection_field_draw[] = {
			UIReflectionFloatSlider<false>,
			UIReflectionDoubleSlider<false>,
			UIReflectionIntSlider<false>,
			UIReflectionFloatInput<false>,
			UIReflectionDoubleInput<false>,
			UIReflectionIntInput<false>,
			UIReflectionTextInput<false>,
			UIReflectionColor<false>,
			UIReflectionCheckBox<false>,
			UIReflectionComboBox<false>,
			UIReflectionFloatSliderGroup<false>,
			UIReflectionDoubleSliderGroup<false>,
			UIReflectionIntSliderGroup<false>,
			UIReflectionFloatInputGroup<false>,
			UIReflectionDoubleInputGroup<false>,
			UIReflectionIntInputGroup<false>
		};

		constexpr UIReflectionFieldDraw<true> ui_reflection_field_initialize[] = {
			UIReflectionFloatSlider<true>,
			UIReflectionDoubleSlider<true>,
			UIReflectionIntSlider<true>,
			UIReflectionFloatInput<true>,
			UIReflectionDoubleInput<true>,
			UIReflectionIntInput<true>,
			UIReflectionTextInput<true>,
			UIReflectionColor<true>,
			UIReflectionCheckBox<true>,
			UIReflectionComboBox<true>,
			UIReflectionFloatSliderGroup<true>,
			UIReflectionDoubleSliderGroup<true>,
			UIReflectionIntSliderGroup<true>,
			UIReflectionFloatInputGroup<true>,
			UIReflectionDoubleInputGroup<true>,
			UIReflectionIntInputGroup<true>
		};

		struct ECSENGINE_API UIReflectionTypeField {
			size_t configuration;
			void* data;
			const char* name;
			UIReflectionIndex reflection_index;
			unsigned short byte_size;
			unsigned int pointer_offset;
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

		struct ECSENGINE_API UIReflectionCheckBoxData {
			bool* value;
			const char* name;
		};

		struct ECSENGINE_API UIReflectionComboBoxData {
			unsigned char* active_label;
			const char* name;
			ECSEngine::containers::Stream<const char*> labels;
			unsigned int label_display_count;
			unsigned int default_label;
		};

		template<typename Field>
		struct UIReflectionGroupData {
			Field** values;
			const char* group_name;
			const char** input_names;
			const Field* default_values;
			const Field* lower_bound;
			const Field* upper_bound;
			unsigned int count;
			unsigned int precision = 3;
			unsigned int byte_size;
		};

		struct ECSENGINE_API UIReflectionInstance {
			const char* type_name;
			const char* name;
			Stream<void*> datas;
		};

		using UIReflectionStringHash = HashFunctionMultiplyString;
		using UIReflectionTypeTable = containers::IdentifierHashTable<UIReflectionType, ResourceIdentifier, HashFunctionPowerOfTwo>;
		using UIReflectionInstanceTable = containers::IdentifierHashTable<UIReflectionInstance, ResourceIdentifier, HashFunctionPowerOfTwo>;

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

			void BindInstancePtrs(const char* instance_name, void* data);
			void BindInstancePtrs(UIReflectionInstance* instance, void* data);

			void BindInstanceTextInput(const char* instance_name, Stream<UIReflectionBindTextInput> data);
			void BindInstanceTextInput(UIReflectionInstance* instance, Stream<UIReflectionBindTextInput> data);

			void ChangeInputToSlider(const char* type_name, const char* field_name);
			void ChangeInputToSlider(UIReflectionType* type, const char* field_name);

			UIReflectionType* CreateType(const char* name);
			UIReflectionInstance* CreateInstance(const char* name, const char* type_name);

			void DestroyInstance(const char* name);

			void DestroyType(const char* name);

			template<bool initialize>
			void DrawInstance(const char* ECS_RESTRICT instance_name, UIDrawer<initialize>& drawer, UIDrawConfig& config, const char* ECS_RESTRICT default_value_button = nullptr);

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