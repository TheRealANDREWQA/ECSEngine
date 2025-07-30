#pragma once
#include "UIDrawer.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "UIReflectionMacros.h"

namespace ECSEngine {

	namespace Tools {

#define UI_REFLECTION_DRAWER_TYPE_TABLE_COUNT (128)
#define UI_REFLECTION_DRAWER_INSTANCE_TABLE_COUNT (256)
#define UI_REFLECTION_MAX_GROUPINGS_PER_TYPE 8

		enum class UIReflectionElement : unsigned char {
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
			DirectoryInput,
			FileInput,
			// User defined it means that it is a type that is understood by the reflection type
			// and can recurse on it
			UserDefined,
			// The field was override by one of the drawer's functions
			Override,
			Count
		};

		enum class UIReflectionStreamType : unsigned char {
			None,
			Capacity,
			Resizable,
			Count
		};

		struct UIReflectionTypeField {
			size_t configuration;
			void* data;
			Stream<char> name;
			// This field will only reference the value from the reflection type
			Stream<char> tags;
			UIReflectionStreamType stream_type;
			UIReflectionElement element_index;
			unsigned short byte_size;
			unsigned short pointer_offset;
			unsigned short reflection_type_index;
		};

		struct UIReflectionDrawInstanceOptions;

		struct UIReflectionFieldDrawData {
			UIDrawer* drawer;
			UIDrawConfig* config;
			size_t configuration;
			Stream<char> name;
			void* data;
			const UIReflectionDrawInstanceOptions* draw_options;
		};

		// Only drawer will be called, the initializer is not involved
		typedef void (*UIReflectionFieldDraw)(UIReflectionFieldDrawData* options);

		// Used for TextInput, FileInput and DirectoryInput to not write the stream to the target
		// (useful for example if it is desired to have a callback do a change)
		constexpr size_t UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM = (size_t)1 << 63;

		struct UIReflectionTypeFieldGrouping {
			unsigned int range;
			unsigned int field_index;
			Stream<char> name;

			// If this field is set to { nullptr, 0 }, then it will keep the names for the rows the same
			Stream<char> per_element_name = { nullptr, 0 };
		};

		struct UIReflectionType {
			ECS_INLINE unsigned int FindField(Stream<char> field_name) const {
				return fields.Find(field_name, [](const UIReflectionTypeField& field) {
					return field.name;
				});
			}

			Stream<char> name;
			CapacityStream<UIReflectionTypeField> fields;
			bool has_overriden_identifier;
			Stream<UIReflectionTypeFieldGrouping> groupings;
		};

		struct UIReflectionInstance {
			Stream<char> type_name;
			Stream<char> name;
			Stream<void*> data;
			bool grouping_open_state[UI_REFLECTION_MAX_GROUPINGS_PER_TYPE];
		};

		typedef HashTableDefault<UIReflectionType> UIReflectionTypeTable;
		typedef HashTableDefault<UIReflectionInstance> UIReflectionInstanceTable;

		struct UIReflectionBindDefaultValue {
			Stream<char> field_name;
			void* value;
		};

		struct UIReflectionBindRange {
			Stream<char> field_name;
			void* min;
			void* max;
		};

		typedef UIReflectionBindDefaultValue UIReflectionBindLowerBound;
		typedef UIReflectionBindDefaultValue UIReflectionBindUpperBound;
		
		struct UIReflectionBindPrecision {
			Stream<char> field_name;
			unsigned int precision;
		};

		struct UIReflectionBindTextInput {
			Stream<char> field_name;
			CapacityStream<char>* stream;
		};

		struct UIReflectionBindDirectoryInput {
			Stream<char> field_name;
			CapacityStream<wchar_t>* stream;
		};

		typedef UIReflectionBindDirectoryInput UIReflectionBindFileInput;

		struct UIReflectionBindStreamCapacity {
			Stream<char> field_name;
			unsigned int capacity;
		};

		// If only the capacity is to be changed, leave the data pointer nullptr
		// If the data pointer is not nullptr, then the data will be copied into the stream
		struct UIReflectionBindResizableStreamData {
			Stream<char> field_name;
			Stream<void> data = { nullptr, 0 };
		};

		struct UIReflectionBindStreamBuffer {
			Stream<char> field_name;
			void* new_buffer;
		};
		
		struct UIReflectionStreamCopy {
			Stream<char> field_name;
			void* destination;
			size_t element_count;
		};

		struct UIReflectionBindResizableStreamAllocator {
			Stream<char> field_name;
			AllocatorPolymorphic allocator;
		};

#define ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT 8

		struct UIReflectionDrawConfig {
			UIReflectionElement index[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			void* configs[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			size_t config_size[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			size_t associated_bits[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
			size_t configurations = 0;
			// Indicates how many configs are currently applied - from the config arrays
			unsigned char config_count = 0;
			// This indicates how many indices it affects - deduced from the index static array
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

		struct UIReflectionOverrideDrawConfig {
			UIReflectionDrawConfig base_config;
			Stream<char> override_tag[8];
			unsigned char override_tag_count = 1;
		};

		enum ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT {
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ARRAY_ADD = 1 << 0,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ARRAY_REMOVE = 1 << 1,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_NUMBER_INPUTS = 1 << 2,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_SLIDERS = 1 << 3,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_CHECK_BOX = 1 << 4,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_TEXT_INPUT = 1 << 5,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_COLOR_INPUT = 1 << 6,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_COMBO_BOX = 1 << 7,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_COLOR_FLOAT = 1 << 8,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_PATH_INPUT = 1 << 9,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ALL = (1 << 10) - 1
		};

		// It will fill in the ui_config buffer that you give to this function
		// Before calling this function, if nothing is set make sure that the index count is 0
		// There must be at least 15 slots in the ui_config array.
		// Returns the number of used indices. The stack allocation should be 512 bytes or greater
		ECSENGINE_API size_t UIReflectionDrawConfigSplatCallback(
			Stream<UIReflectionDrawConfig> ui_config, 
			UIActionHandler handler,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT splat_type,
			void* stack_allocation,
			bool trigger_only_on_release = true
		);

		typedef void (*UIReflectionInstanceDrawCustom)(
			UIDrawer* drawer,
			UIDrawConfig* config,
			size_t configuration,
			void* field_data, 
			Stream<char> field_name,
			UIReflectionStreamType stream_type,
			void* extra_data
		);

#define ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(name) void name( \
		ECSEngine::Tools::UIDrawer* drawer, \
		ECSEngine::Tools::UIDrawConfig* config, \
		size_t configuration, \
		void* field_data, \
		ECSEngine::Stream<char> field_name, \
		ECSEngine::Tools::UIReflectionStreamType stream_type, \
		void* extra_data \
		)

		// These replace a certain UIReflectionElement default draw
		struct UIReflectionInstanceDrawCustomFunctors {
			UIReflectionInstanceDrawCustom functions[(unsigned char)UIReflectionElement::Count] = { nullptr };
			void* extra_data = nullptr;
		};

		struct UIReflectionDrawer;

		typedef void (*UIReflectionInstanceInitializeOverride)(AllocatorPolymorphic allocator, UIReflectionDrawer* drawer, Stream<char> name, void* data, void* global_data);

#define ECS_UI_REFLECTION_INSTANCE_INITIALIZE_OVERRIDE(name) void name( \
		ECSEngine::AllocatorPolymorphic allocator, \
		ECSEngine::Tools::UIReflectionDrawer* drawer, \
		ECSEngine::Stream<char> name, \
		void* _data, \
		void* _global_data \
		)

		typedef void (*UIReflectionInstanceDeallocateOverride)(AllocatorPolymorphic allocator, void* data, void* global_data);

#define ECS_UI_REFLECTION_INSTANCE_DEALLOCATE_OVERRIDE(name) void name( \
		ECSEngine::AllocatorPolymorphic allocator, \
		void* _data, \
		void* _global_data \
		)

		typedef void (*UIReflectionInstanceModifyOverride)(AllocatorPolymorphic allocator, void* data, void* global_data, void* user_data);

#define ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(name) void name( \
		ECSEngine::AllocatorPolymorphic allocator, \
		void* _data, \
		void* _global_data, \
		void* user_data \
		)

		// The initialize function can be made nullptr if you don't want to initialize anything.
		// Can optionally have some data set that is accessible globally for all functions
		struct UIReflectionFieldOverride {
			UIReflectionInstanceInitializeOverride initialize_function = nullptr;
			UIReflectionInstanceDeallocateOverride deallocate_function = nullptr;
			UIReflectionInstanceDrawCustom draw_function;
			Stream<char> tag;
			unsigned int draw_data_size;
			unsigned int global_data_size = 0;
			void* global_data = nullptr;
		};

		struct UIReflectionDrawerTag {
			Stream<char> tag;
			bool has_compare;
		};

		struct UIReflectionDrawerSearchOptions {
			Stream<UIReflectionDrawerTag> include_tags = { nullptr, 0 };
			Stream<UIReflectionDrawerTag> exclude_tags = { nullptr, 0 };
			Stream<char> suffix = { nullptr, 0 };
			CapacityStream<unsigned int>* indices = nullptr;
		};

		struct UIReflectionDrawInstanceFieldTagCallbackData {
			void* field_data;
			void* user_data;
			Stream<char> field_name;
			UIReflectionElement element;
			UIReflectionStreamType stream_type;
		};

		typedef void (*UIReflectionDrawInstanceFieldTagCallback)(UIReflectionDrawInstanceFieldTagCallbackData* data);

		// The config will be applied if the tag is matched by that field
		struct UIReflectionDrawInstanceFieldTagOption {
			// If this is set to true, then the matching is done with Is instead of Has
			bool is_tag = false;
			// If this is set to true, all fields that don't match the tag will receive this config
			bool exclude_match = false;
			// If this is set to true, a field that matches the tag won't receive additional draw configs
			// From the additional_configs field of the UIReflectionDrawInstanceOptions
			bool disable_additional_configs = false;
			Stream<char> tag;
			UIReflectionDrawConfig draw_config;

			// Can optionally provide a callback when a field is matched
			UIReflectionDrawInstanceFieldTagCallback callback = nullptr;
			void* callback_data = nullptr;
		};

		struct UIReflectionDrawInstanceFieldGeneralCallbackFunctionData {
			// This is information about the field itself
			Stream<char> tag;
			Stream<char> field_name;
			Stream<char> type_name;
			UIReflectionStreamType stream_type;
			UIReflectionElement element_index;
			// These are extra fields to help distinguish between UI elements that might target the same underlying data
			Reflection::ReflectionBasicFieldType reflection_basic_field_type;
			Reflection::ReflectionStreamFieldType reflection_stream_field_type;
			void* address;

			// These are the fields that should be modified in case the field needs to be handled differently
			size_t* configuration;
			UIDrawConfig* config;

			// If the functor requires some stack memory for some config parameter, it can allocate it from here
			AllocatorPolymorphic temporary_allocator;

			// This is user provided data
			void* user_data;
		};

		// Should return true if the callback added configs, else false
		typedef bool (*UIReflectionDrawInstanceFieldGeneralCallbackFunction)(UIReflectionDrawInstanceFieldGeneralCallbackFunctionData* data);

		struct UIReflectionDrawInstanceFieldGeneralCallback {
			UIReflectionDrawInstanceFieldGeneralCallbackFunction function;
			void* user_data;
		};

		struct UIReflectionDrawInstanceOptions {
			UIDrawer* drawer;
			UIDrawConfig* config;
			size_t global_configuration = 0;
			Stream<UIReflectionDrawConfig> additional_configs = { nullptr, 0 };
			Stream<UIReflectionOverrideDrawConfig> override_additional_configs = { nullptr, 0 };
			const UIReflectionInstanceDrawCustomFunctors* custom_draw = nullptr;
			Stream<char> default_value_button = { nullptr, 0 };
			Stream<UIReflectionDrawInstanceFieldTagOption> field_tag_options = { nullptr, 0 };
			Stream<UIReflectionDrawInstanceFieldGeneralCallback> field_general_callbacks = { nullptr, 0 };
		};

		struct UIReflectionDrawerCreateTypeOptions {
			// By default, it will assert that the type doesn't exist
			bool assert_that_it_doesnt_exist = true;
			Stream<char> identifier_name = { nullptr, 0 };
			Stream<unsigned int> ignore_fields = { nullptr, 0 };
		};

		struct ECSENGINE_API UIReflectionDrawer {
			UIReflectionDrawer(
				UIToolsAllocator* allocator,
				Reflection::ReflectionManager* reflection,
				size_t type_table_count = UI_REFLECTION_DRAWER_TYPE_TABLE_COUNT,
				size_t instance_table_count = UI_REFLECTION_DRAWER_INSTANCE_TABLE_COUNT
			);

			UIReflectionDrawer(const UIReflectionDrawer& other) = default;
			UIReflectionDrawer& operator = (const UIReflectionDrawer& other) = default;

			void AddTypeConfiguration(Stream<char> type_name, Stream<char> field_name, size_t field_configuration);
			void AddTypeConfiguration(UIReflectionType* type, Stream<char> field_name, size_t field_configuration);

			void AddTypeGrouping(UIReflectionType* type, UIReflectionTypeFieldGrouping grouping);

			// For every resizable stream, it will assign the allocator. If allocate inputs is true, it will allocate
			// for every text input, directory input or file input a default sized buffer (256 element long)
			void AssignInstanceResizableAllocator(Stream<char> instance_name, AllocatorPolymorphic allocator, bool allocate_inputs = true, bool recurse = true);
			// For every resizable stream, it will assign the allocator. If allocate inputs is true, it will allocate
			// for every text input, directory input or file input a default sized buffer (256 element long)
			void AssignInstanceResizableAllocator(UIReflectionInstance* instance, AllocatorPolymorphic allocator, bool allocate_inputs = true, bool recurse = true);

			void BindInstanceData(Stream<char> instance_name, Stream<char> field_name, void* field_data);
			void BindInstanceData(UIReflectionInstance* instance, Stream<char> field_name, void* field_data);

			void BindTypeData(Stream<char> type_name, Stream<char> field_name, void* field_data);
			void BindTypeData(UIReflectionType* type, Stream<char> field_name, void* field_data);

			void BindTypeLowerBounds(Stream<char> type_name, Stream<UIReflectionBindLowerBound> data);
			void BindTypeLowerBounds(UIReflectionType* type, Stream<UIReflectionBindLowerBound> data);
			// It will take the values directly from the type - only for those fields which allow
			void BindTypeLowerBounds(UIReflectionType* type, const void* data);

			void BindTypeUpperBounds(Stream<char> type_name, Stream<UIReflectionBindUpperBound> data);
			void BindTypeUpperBounds(UIReflectionType* type, Stream<UIReflectionBindUpperBound> data);
			// It will take the values directly from the type - only for those fields which allow
			void BindTypeUpperBounds(UIReflectionType* type, const void* data);

			void BindTypeDefaultData(Stream<char> type_name, Stream<UIReflectionBindDefaultValue> data);
			void BindTypeDefaultData(UIReflectionType* type, Stream<UIReflectionBindDefaultValue> data);
			// It will take the values directly from the type - only for those fields which allow
			void BindTypeDefaultData(UIReflectionType* type, const void* data);

			void BindTypeRange(Stream<char> type_name, Stream<UIReflectionBindRange> data);
			void BindTypeRange(UIReflectionType* type, Stream<UIReflectionBindRange> data);

			void BindTypePrecision(Stream<char> type_name, Stream<UIReflectionBindPrecision> data);
			void BindTypePrecision(UIReflectionType* type, Stream<UIReflectionBindPrecision> data);

			// It requires access to a ReflectionType
			void BindInstancePtrs(Stream<char> instance_name, void* data);
			// It requires access to a ReflectionType
			void BindInstancePtrs(UIReflectionInstance* instance, void* data);

			// The ReflectionType if it is created outside
			void BindInstancePtrs(UIReflectionInstance* instance, void* data, const Reflection::ReflectionType* type);

			void BindInstanceTextInput(Stream<char> instance_name, Stream<UIReflectionBindTextInput> data);
			void BindInstanceTextInput(UIReflectionInstance* instance, Stream<UIReflectionBindTextInput> data);
			// There must be field_indices.size inputs
			void BindInstanceTextInput(UIReflectionInstance* instance, Stream<Reflection::ReflectionNestedFieldIndex> field_indices, CapacityStream<char>* inputs);

			void BindInstanceDirectoryInput(Stream<char> instance_name, Stream<UIReflectionBindDirectoryInput> data);
			void BindInstanceDirectoryInput(UIReflectionInstance* instance, Stream<UIReflectionBindDirectoryInput> data);
			// There must be field_indices.size inputs
			void BindInstanceDirectoryInput(UIReflectionInstance* instance, Stream<Reflection::ReflectionNestedFieldIndex> field_indices, CapacityStream<wchar_t>* inputs);

			void BindInstanceFileInput(Stream<char> instance_name, Stream<UIReflectionBindFileInput> data);
			void BindInstanceFileInput(UIReflectionInstance* instance, Stream<UIReflectionBindFileInput> data);
			// There must be field_indices.size inputs
			void BindInstanceFileInput(UIReflectionInstance* instance, Stream<Reflection::ReflectionNestedFieldIndex> field_indices, CapacityStream<wchar_t>* inputs);

			void BindInstanceStreamCapacity(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<Reflection::ReflectionNestedFieldIndex> field_indices, unsigned int* capacities);

			void BindInstanceStreamSize(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamSize(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamSize(UIReflectionInstance* instance, Stream<Reflection::ReflectionNestedFieldIndex> field_indices, unsigned int* sizes);

			void BindInstanceStreamBuffer(Stream<char> instance_name, Stream<UIReflectionBindStreamBuffer> data);
			void BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<UIReflectionBindStreamBuffer> data);
			void BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<Reflection::ReflectionNestedFieldIndex> field_indices, void** buffers);

			void BindInstanceResizableStreamAllocator(Stream<char> instance_name, Stream<UIReflectionBindResizableStreamAllocator> data);
			void BindInstanceResizableStreamAllocator(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamAllocator> data);
			// If the field indices is not specified, then all streams that are resizable are going to have the allocator set
			void BindInstanceResizableStreamAllocator(UIReflectionInstance* instance, AllocatorPolymorphic allocator, Stream<unsigned int> field_indices = { nullptr, 0 });

			void BindInstanceResizableStreamData(Stream<char> instance_name, Stream<UIReflectionBindResizableStreamData> data);
			void BindInstanceResizableStreamData(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamData> data);

			// This will be called for all fields of that override type
			void BindInstanceFieldOverride(Stream<char> instance_name, Stream<char> tag, UIReflectionInstanceModifyOverride modify_override, void* user_data);
			// This will be called for all fields of that override type
			void BindInstanceFieldOverride(UIReflectionInstance* instance, Stream<char> tag, UIReflectionInstanceModifyOverride modify_override, void* user_data);
			
			// If the allocator is left unspecified, then it will use the allocator from this UIReflectionDrawer
			void BindInstanceFieldOverride(
				void* override_data, 
				Stream<char> tag, 
				UIReflectionInstanceModifyOverride modify_override, 
				void* user_data,
				AllocatorPolymorphic allocator = nullptr
			);

			void ConvertTypeResizableStream(Stream<char> type_name, Stream<Stream<char>> field_names);
			void ConvertTypeResizableStream(UIReflectionType* type, Stream<Stream<char>> field_names);

			void ConvertTypeStreamsToResizable(Stream<char> type_name, bool recurse = true);
			void ConvertTypeStreamsToResizable(UIReflectionType* type, bool recurse = true);

			// It will fill in the count for each field. Does not work on inputs
			void CopyInstanceStreams(Stream<char> instance_name, Stream<UIReflectionStreamCopy> data);
			// It will fill in the count for each field. Does not work on inputs
			void CopyInstanceStreams(UIReflectionInstance* instance, Stream<UIReflectionStreamCopy> data);

			void ChangeInputToSlider(Stream<char> type_name, Stream<char> field_name);
			void ChangeInputToSlider(UIReflectionType* type, Stream<char> field_name);

			void ChangeDirectoryToFile(Stream<char> type_name, Stream<char> field_name);
			void ChangeDirectoryToFile(UIReflectionType* type, Stream<char> field_name);

			// Gets the type from the internal reflection
			UIReflectionType* CreateType(Stream<char> name, UIReflectionDrawerCreateTypeOptions* options = nullptr);
			// Can optionally specify a name by which it will be identified
			UIReflectionType* CreateType(const Reflection::ReflectionType* type, UIReflectionDrawerCreateTypeOptions* options = nullptr);

			// Can optionally give a user defined allocator to be used for allocations inside the user defined fields.
			// By default it will pass on the allocator of this UIReflectionDrawer. It will store this allocator and give
			// it back to the deallocate function unless overriden. The allocation of the field overrides actually happens
			// when the function BindInstancePtrs is called - not on this call
			UIReflectionInstance* CreateInstance(Stream<char> name, Stream<char> type_name, AllocatorPolymorphic user_defined_allocator = nullptr);
			// If the identifier name is specified, it will use that as the type_name instead of type->name
			// Can optionally give a user defined allocator to be used for allocations inside the user defined fields.
			// By default it will pass on the allocator of this UIReflectionDrawer. It will store this allocator and give
			// it back to the deallocate function unless overriden. The allocation of the field overrides actually happens
			// when the function BindInstancePtrs is called - not on this call
			UIReflectionInstance* CreateInstance(
				Stream<char> name, 
				const UIReflectionType* type, 
				Stream<char> identifier_name = { nullptr, 0 }, 
				AllocatorPolymorphic user_defined_allocator = nullptr
			);

			// It will create a type for each reflected type from the given hierarchy.
			// Returns how many types were created
			// Options used: all except the suffix.
			unsigned int CreateTypesForHierarchy(unsigned int hierarchy_index, const UIReflectionDrawerSearchOptions& options = {});
			// It will create a type for each reflected type from the given hierarchy.
			// Returns how many types were created
			// Options used: all except the suffix.
			unsigned int CreateTypesForHierarchy(Stream<wchar_t> hierarchy, const UIReflectionDrawerSearchOptions& options = {});

			// It will create an instance for each type with the given hierarchy.
			// Returns how many instances were created
			// Options used: all.
			unsigned int CreateInstanceForHierarchy(unsigned int hierarchy_index, const UIReflectionDrawerSearchOptions& options = {});
			// It will create an instance for each type with the given hierarchy.
			// Returns how many instances were created
			// Options used: all.
			unsigned int CreateInstanceForHierarchy(Stream<wchar_t> hierarchy, const UIReflectionDrawerSearchOptions& options = {});

			// It will create a type and an instance for each reflected type from the given hierarchy.
			// The name of the instance is identical to that of the type
			// Returns how many instances were created
			// Options used: all except the suffix. The indices buffer will be populated with the instances' indices
			unsigned int CreateTypesAndInstancesForHierarchy(unsigned int hierarchy_index, const UIReflectionDrawerSearchOptions& options = {});

			// It will create a type and an instance for each reflected type from the given hierarchy.
			// The name of the instance is identical to that of the type
			// Returns how many instances were created
			// Options used: all except the suffix. The indices buffer will be populated with the instances' indices
			unsigned int CreateTypesAndInstancesForHierarchy(Stream<wchar_t> hierarchy, const UIReflectionDrawerSearchOptions& options = {});

			// Disables the default write of the values into the target
			// Useful if you want to have a callback do the actual job
			// If all writes is set to true, then Stream inputs (aka array inputs)
			// will also be disabled. If set to false only TextInput, FileInput and
			// DirectoryInput will be disabled. If recurse is set to true, nested
			// Types will also be modified
			void DisableInputWrites(UIReflectionType* type, bool all_writes = true, bool recurse = true);
			
			// User facing method. If the allocator is left unspecified then it will use the
			// allocator from the UIReflectionDrawer. Be careful what allocator you give (the same one
			// as when initializing)
			void DeallocateFieldOverride(Stream<char> tag, void* data, AllocatorPolymorphic allocator = nullptr);

			void DeallocateInstance(UIReflectionInstance* instance);
			void DeallocateInstanceFields(UIReflectionInstance* instance);

			void DestroyInstance(unsigned int index);
			void DestroyInstance(Stream<char> name);

			void DestroyType(Stream<char> name);

			// The additional configuration will be applied to all fields
			// It can be used to set size configs, text parameters, alignments
			// This is the default draw type - basic. For a customized draw, 
			// Or you can provide functors to override the drawing of certain field types
			// use the other variant
			void DrawInstance(
				Stream<char> instance_name,
				const UIReflectionDrawInstanceOptions* options
			);

			// The additional configuration will be applied to all fields
			// It can be used to set size configs, text parameters, alignments
			// Or you can provide functors to override the drawing of certain field types
			void DrawInstance(
				UIReflectionInstance* instance,
				const UIReflectionDrawInstanceOptions* options
			);

			void DrawFieldOverride(
				Stream<char> tag,
				void* data,
				void* field_data,
				UIDrawer* drawer,
				size_t configuration,
				UIDrawConfig* config,
				Stream<char> name,
				UIReflectionStreamType stream_type = UIReflectionStreamType::None
			);

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(unsigned int hierarchy_index);

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(Stream<wchar_t> hierarchy);

			// Destroys all instances from the given hierarchy. If a suffix is provided, only those instances
			// that match the name of the type with the suffix appended will be deleted.
			// The include and exclude tags are used for the instance's parent type tags
			void DestroyAllInstancesFromFolderHierarchy(unsigned int hierarchy_index, const UIReflectionDrawerSearchOptions& options = {});
			
			// Destroys all instances from the given hierarchy. If a suffix is provided, only those instances
			// that match the name of the type with the suffix appended will be deleted.
			// The include and exclude tags are used for the instance's parent type tags
			void DestroyAllInstancesFromFolderHierarchy(Stream<wchar_t> hierarchy, const UIReflectionDrawerSearchOptions& options = {});

			// Returns -1 if it doesn't find it
			unsigned int FindFieldOverride(Stream<char> tag) const;

			// It will fill in the capacity field
			void GetInstanceStreamSizes(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data);
			// It will fill in the capacity field
			void GetInstanceStreamSizes(const UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);

			void GetHierarchyTypes(unsigned int hierarchy_index, const UIReflectionDrawerSearchOptions& options);
			void GetHierarchyTypes(Stream<wchar_t> hierarchy, const UIReflectionDrawerSearchOptions& options);

			// If a suffix is provided, only those instances which match a type name with the appended suffix will be provided
			void GetHierarchyInstances(unsigned int hierarchy_index, const UIReflectionDrawerSearchOptions& options);
			// If a suffix is provided, only those instances which match a type name with the appended suffix will be provided
			void GetHierarchyInstances(Stream<wchar_t> hierarchy, const UIReflectionDrawerSearchOptions& options);

			// Returns nullptr if it doesn't exist
			UIReflectionInstance* GetInstance(Stream<char> name);
			// Returns nullptr if it doesn't exist
			UIReflectionInstance* GetInstance(unsigned int index);

			const UIReflectionInstance* GetInstance(Stream<char> name) const;
			const UIReflectionInstance* GetInstance(unsigned int index) const;
			
			// Returns nullptr if it doesn't exist.
			UIReflectionType* GetType(Stream<char> name);
			// Returns nullptr if it doesn't exist
			UIReflectionType* GetType(unsigned int index);

			// It will assert that it exists
			const UIReflectionType* GetType(Stream<char> name) const;
			// It will assert that it exists
			const UIReflectionType* GetType(unsigned int index) const;

			unsigned int GetTypeCount() const;
			unsigned int GetInstanceCount() const;

			// If the recurse flag is false, only top most fields are explored
			void GetTypeMatchingFields(
				const UIReflectionType* type, 
				UIReflectionElement index, 
				CapacityStream<Reflection::ReflectionNestedFieldIndex>& indices, 
				bool recurse = true
			) const;
			// If the recurse flag is false, only top most fields are explored
			void GetTypeMatchingFields(
				const UIReflectionType* type,
				UIReflectionStreamType stream_type, 
				CapacityStream<Reflection::ReflectionNestedFieldIndex>& indices, 
				bool recurse = true
			) const;

			// Returns the current bound stream for the TextInput/DirectoryInput/FileInput
			CapacityStream<void> GetInputStream(const UIReflectionInstance* instance, unsigned int field_index) const;
			// Returns the current bound stream for the TextInput/DirectoryInput/FileInput
			// This works only for embedded types i.e. not the fields of user defined types
			// That come from buffers
			CapacityStream<void> GetInputStreamNested(const UIReflectionInstance* instance, Reflection::ReflectionNestedFieldIndex field_index) const;

			// The field_name must respect the struct accessing naming rules
			// i.e. first.second.third if you want to look for the field third
			// Inside a struct second that belongs to a field first from the initial type
			// By default, it will not consider fields of user defined types that belong
			// To a stream field, but you can change the behaviour
			Reflection::ReflectionNestedFieldIndex GetTypeFieldIndexNested(const UIReflectionType* type, Stream<char> field_name, bool include_stream_types = false) const;

			// It fills in the name of the nested field such that it respects the addressing rules
			Stream<char> GetTypeFieldNestedName(const UIReflectionType* type, Reflection::ReflectionNestedFieldIndex field_index, CapacityStream<char>& name) const;

			// User facing method
			// Can optionally give a user defined allocator to be used for allocations inside the user defined fields.
			// By default it will pass on the allocator of this UIReflectionDrawer
			void* InitializeFieldOverride(Stream<char> tag, Stream<char> name, AllocatorPolymorphic allocator = nullptr);

			// It will set the buffer of the standalone inputs to { nullptr, 0 }
			// This is useful if you want to deactivate the deallocation for the field
			void InvalidateStandaloneInstanceInputs(Stream<char> instance_name, bool recurse = true);
			void InvalidateStandaloneInstanceInputs(UIReflectionInstance* instance, bool recurse = true);
			// This function will be applied for each instance
			void InvalidateStandaloneInstanceInputs(bool recurse = true);

			void OmitTypeField(Stream<char> type_name, Stream<char> field_name);
			void OmitTypeField(UIReflectionType* type, Stream<char> field_name);

			void OmitTypeFields(Stream<char> type_name, Stream<Stream<char>> fields);
			void OmitTypeFields(UIReflectionType* type, Stream<Stream<char>> fields);

			// Call this if the given instance already was bound to a pointer
			void RebindInstancePtrs(UIReflectionInstance* instance, void* data);

			// If you set an allocator using AssignInstanceResizableAllocator for an instance but itself got deallocated,
			// This function will reset the allocator and the buffers to empty (without trying to deallocate data that would
			// Normally trigger a deallocation). The allocate_inputs and recurse booleans should have the same value as when
			// The assign call was made.
			void ResetInstanceResizableAllocator(UIReflectionInstance* instance, bool allocate_inputs = true, bool recurse = true);

			void SetFieldOverride(const UIReflectionFieldOverride* override);

			Reflection::ReflectionManager* reflection;
			UIToolsAllocator* allocator;
			UIReflectionTypeTable type_definition;
			UIReflectionInstanceTable instances;
			ResizableStream<UIReflectionFieldOverride> overrides;
		};

		struct UIReflectionDefaultValueData {
			UIReflectionType type;
			UIReflectionInstance instance;
		};

		// Returns true if the type is completely omitted from the UI side
		ECSENGINE_API bool IsUIReflectionTypeOmitted(const Reflection::ReflectionType* type);

	}

}