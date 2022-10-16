#pragma once
#include "UIDrawer.h"
#include "../../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	namespace Tools {

#define UI_REFLECTION_DRAWER_TYPE_TABLE_COUNT (128)
#define UI_REFLECTION_DRAWER_INSTANCE_TABLE_COUNT (256)

#define ECS_UI_OMIT_FIELD_REFLECT

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
			UIReflectionStreamType stream_type;
			UIReflectionIndex reflection_index;
			unsigned short byte_size;
			unsigned int pointer_offset;
		};

		// Only drawer will be called, the initializer is not involved
		using UIReflectionFieldDraw = void (*)(UIDrawer& drawer, UIDrawConfig& config, size_t configuration, void* data);

		// Used for TextInput, FileInput and DirectoryInput to not write the stream to the target
		// (useful for example if it is desired to have a callback do a change)
		constexpr size_t UI_CONFIG_REFLECTION_INPUT_DONT_WRITE_STREAM = (size_t)1 << 63;

		struct UIReflectionType {
			Stream<char> name;
			CapacityStream<UIReflectionTypeField> fields;
		};

		struct UIReflectionInstance {
			Stream<char> type_name;
			Stream<char> name;
			Stream<void*> data;
		};

		using UIReflectionTypeTable = HashTableDefault<UIReflectionType>;
		using UIReflectionInstanceTable = HashTableDefault<UIReflectionInstance>;

		struct UIReflectionBindDefaultValue {
			Stream<char> field_name;
			void* value;
		};

		struct UIReflectionBindRange {
			Stream<char> field_name;
			void* min;
			void* max;
		};

		using UIReflectionBindLowerBound = UIReflectionBindDefaultValue;
		using UIReflectionBindUpperBound = UIReflectionBindDefaultValue;
		
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
			UIReflectionIndex index[ECS_UI_REFLECTION_DRAW_CONFIG_MAX_COUNT];
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

		// Before calling this function, if nothing is set make sure that the index count is 0
		// Returns the number of used indices. The stack allocation should be 512 bytes or greater
		ECSENGINE_API size_t UIReflectionDrawConfigSplatCallback(
			Stream<UIReflectionDrawConfig> ui_config, 
			UIActionHandler handler,
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT splat_type,
			void* stack_allocation
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

		// These replace a certain UIReflectionIndex default draw
		struct UIReflectionInstanceDrawCustomFunctors {
			UIReflectionInstanceDrawCustom functions[(unsigned char)UIReflectionIndex::Count] = { nullptr };
			void* extra_data = nullptr;
		};

		struct UIReflectionDrawer;

		// Any buffers that are allocated need to be placed into the buffers capacity stream
		typedef void (*UIReflectionInstanceInitializeOverride)(UIReflectionDrawer* drawer, Stream<char> name, void* data, void* global_data);

#define ECS_UI_REFLECTION_INSTANCE_INITIALIZE_OVERRIDE(name) void name( \
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

		struct UIReflectionDrawerSearchOptions {
			Stream<Stream<char>> include_tags = { nullptr, 0 };
			Stream<Stream<char>> exclude_tags = { nullptr, 0 };
			Stream<char> suffix = { nullptr, 0 };
			CapacityStream<unsigned int>* indices = nullptr;
		};

		struct UIReflectionDrawInstanceOptions {
			UIDrawer* drawer;
			UIDrawConfig* config;
			size_t global_configuration = 0;
			Stream<UIReflectionDrawConfig> additional_configs = { nullptr, 0 };
			const UIReflectionInstanceDrawCustomFunctors* custom_draw = nullptr;
			Stream<char> default_value_button = { nullptr, 0 };
		};

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

			void AddTypeConfiguration(Stream<char> type_name, Stream<char> field_name, size_t field_configuration);
			void AddTypeConfiguration(UIReflectionType* type, Stream<char> field_name, size_t field_configuration);

			// For every resizable stream, it will assign the allocator. If allocate inputs is true, it will allocate
			// for every text input, directory input or file input a default sized buffer (256 element long)
			void AssignInstanceResizableAllocator(Stream<char> instance_name, AllocatorPolymorphic allocator, bool allocate_inputs = true);
			// For every resizable stream, it will assign the allocator. If allocate inputs is true, it will allocate
			// for every text input, directory input or file input a default sized buffer (256 element long)
			void AssignInstanceResizableAllocator(UIReflectionInstance* instance, AllocatorPolymorphic allocator, bool allocate_inputs = true);

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
			void BindInstanceTextInput(UIReflectionInstance* instance, Stream<unsigned int> field_indices, CapacityStream<char>* inputs);

			void BindInstanceDirectoryInput(Stream<char> instance_name, Stream<UIReflectionBindDirectoryInput> data);
			void BindInstanceDirectoryInput(UIReflectionInstance* instance, Stream<UIReflectionBindDirectoryInput> data);
			void BindInstanceDirectoryInput(UIReflectionInstance* instance, Stream<unsigned int> field_indices, CapacityStream<wchar_t>* inputs);

			void BindInstanceFileInput(Stream<char> instance_name, Stream<UIReflectionBindFileInput> data);
			void BindInstanceFileInput(UIReflectionInstance* instance, Stream<UIReflectionBindFileInput> data);
			void BindInstanceFileInput(UIReflectionInstance* instance, Stream<unsigned int> field_indices, CapacityStream<wchar_t>* inputs);

			void BindInstanceStreamCapacity(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamCapacity(UIReflectionInstance* instance, Stream<unsigned int> field_indices, unsigned int* capacities);

			void BindInstanceStreamSize(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamSize(UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);
			void BindInstanceStreamSize(UIReflectionInstance* instance, Stream<unsigned int> field_indices, unsigned int* sizes);

			void BindInstanceStreamBuffer(Stream<char> instance_name, Stream<UIReflectionBindStreamBuffer> data);
			void BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<UIReflectionBindStreamBuffer> data);
			void BindInstanceStreamBuffer(UIReflectionInstance* instance, Stream<unsigned int> field_indices, void** buffers);

			void BindInstanceResizableStreamAllocator(Stream<char> instance_name, Stream<UIReflectionBindResizableStreamAllocator> data);
			void BindInstanceResizableStreamAllocator(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamAllocator> data);
			void BindInstanceResizableStreamAllocator(UIReflectionInstance* instance, Stream<unsigned int> field_indices, AllocatorPolymorphic allocator);

			void BindInstanceResizableStreamData(Stream<char> instance_name, Stream<UIReflectionBindResizableStreamData> data);
			void BindInstanceResizableStreamData(UIReflectionInstance* instance, Stream<UIReflectionBindResizableStreamData> data);

			// This will be called for all fields of that override type
			void BindInstanceFieldOverride(Stream<char> instance_name, Stream<char> tag, UIReflectionInstanceModifyOverride modify_override, void* user_data);
			// This will be called for all fields of that override type
			void BindInstanceFieldOverride(UIReflectionInstance* instance, Stream<char> tag, UIReflectionInstanceModifyOverride modify_override, void* user_data);

			void ConvertTypeResizableStream(Stream<char> type_name, Stream<Stream<char>> field_names);
			void ConvertTypeResizableStream(UIReflectionType* type, Stream<Stream<char>> field_names);

			void ConvertTypeStreamsToResizable(Stream<char> type_name);
			void ConvertTypeStreamsToResizable(UIReflectionType* type);

			// It will fill in the count for each field. Does not work on inputs
			void CopyInstanceStreams(Stream<char> instance_name, Stream<UIReflectionStreamCopy> data);
			// It will fill in the count for each field. Does not work on inputs
			void CopyInstanceStreams(UIReflectionInstance* instance, Stream<UIReflectionStreamCopy> data);

			void ChangeInputToSlider(Stream<char> type_name, Stream<char> field_name);
			void ChangeInputToSlider(UIReflectionType* type, Stream<char> field_name);

			void ChangeDirectoryToFile(Stream<char> type_name, Stream<char> field_name);
			void ChangeDirectoryToFile(UIReflectionType* type, Stream<char> field_name);

			UIReflectionType* CreateType(Stream<char> name);
			UIReflectionType* CreateType(const Reflection::ReflectionType* type);

			UIReflectionInstance* CreateInstance(Stream<char> name, Stream<char> type_name);
			UIReflectionInstance* CreateInstance(Stream<char> name, const UIReflectionType* type);

			// It will create a type for each reflected type from the given hierarchy.
			// Returns how many types were created
			// Options used: all except the suffix.
			unsigned int CreateTypesForHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options = {});
			// It will create a type for each reflected type from the given hierarchy.
			// Returns how many types were created
			// Options used: all except the suffix.
			unsigned int CreateTypesForHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options = {});

			// It will create an instance for each type with the given hierarchy.
			// Returns how many instances were created
			// Options used: all.
			unsigned int CreateInstanceForHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options = {});
			// It will create an instance for each type with the given hierarchy.
			// Returns how many instances were created
			// Options used: all.
			unsigned int CreateInstanceForHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options = {});

			// It will create a type and an instance for each reflected type from the given hierarchy.
			// The name of the instance is identical to that of the type
			// Returns how many instances were created
			// Options used: all except the suffix. The indices buffer will be populated with the instances' indices
			unsigned int CreateTypesAndInstancesForHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options = {});

			// It will create a type and an instance for each reflected type from the given hierarchy.
			// The name of the instance is identical to that of the type
			// Returns how many instances were created
			// Options used: all except the suffix. The indices buffer will be populated with the instances' indices
			unsigned int CreateTypesAndInstancesForHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options = {});

			// Disables the default write of the values into the target
			// Useful if you want to have a callback do the actual job
			// If all writes is set to true, then Stream inputs (aka array inputs)
			// will also be disabled. If set to false only TextInput, FileInput and
			// DirectoryInput will be disabled
			void DisableInputWrites(UIReflectionType* type, bool all_writes = true);
			
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

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(unsigned int hierarchy_index);

			// Destroys all instances and types that originate from the given hierarchy
			void DestroyAllFromFolderHierarchy(Stream<wchar_t> hierarchy);

			// Destroys all instances from the given hierarchy. If a suffix is provided, only those instances
			// that match the name of the type with the suffix appended will be deleted.
			// The include and exclude tags are used for the instance's parent type tags
			void DestroyAllInstancesFromFolderHierarchy(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options = {});
			
			// Destroys all instances from the given hierarchy. If a suffix is provided, only those instances
			// that match the name of the type with the suffix appended will be deleted.
			// The include and exclude tags are used for the instance's parent type tags
			void DestroyAllInstancesFromFolderHierarchy(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options = {});

			// It will fill in the capacity field
			void GetInstanceStreamSizes(Stream<char> instance_name, Stream<UIReflectionBindStreamCapacity> data);
			// It will fill in the capacity field
			void GetInstanceStreamSizes(const UIReflectionInstance* instance, Stream<UIReflectionBindStreamCapacity> data);

			void GetHierarchyTypes(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options);
			void GetHierarchyTypes(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options);

			// If a suffix is provided, only those instances which match a type name with the appended suffix will be provided
			void GetHierarchyInstances(unsigned int hierarchy_index, UIReflectionDrawerSearchOptions options);
			// If a suffix is provided, only those instances which match a type name with the appended suffix will be provided
			void GetHierarchyInstances(Stream<wchar_t> hierarchy, UIReflectionDrawerSearchOptions options);

			UIReflectionInstance GetInstance(Stream<char> name) const;
			UIReflectionInstance GetInstance(unsigned int index) const;
			
			// It will assert that it exists
			UIReflectionType GetType(Stream<char> name) const;
			// It will assert that it exists
			UIReflectionType GetType(unsigned int index) const;

			// Returns nullptr if it doesn't exist.
			UIReflectionType* GetTypePtr(Stream<char> name);
			// Returns nullptr if it doesn't exist
			UIReflectionType* GetTypePtr(unsigned int index);

			unsigned int GetTypeCount() const;
			unsigned int GetInstanceCount() const;

			// Returns nullptr if it doesn't exist
			UIReflectionInstance* GetInstancePtr(Stream<char> name);
			// Returns nullptr if it doesn't exist
			UIReflectionInstance* GetInstancePtr(unsigned int index);

			void GetTypeMatchingFields(UIReflectionType type, UIReflectionIndex index, CapacityStream<unsigned int>& indices) const;
			void GetTypeMatchingFields(UIReflectionType type, UIReflectionStreamType stream_type, CapacityStream<unsigned int>& indices) const;

			// Returns the current bound stream for the TextInput/DirectoryInput/FileInput
			CapacityStream<void> GetInputStream(const UIReflectionInstance* instance, unsigned int field_index) const;
			
			unsigned int GetTypeFieldIndex(UIReflectionType type, Stream<char> field_name) const;
			unsigned int GetTypeFieldIndex(Stream<char> type_name, Stream<char> field_name);

			void OmitTypeField(Stream<char> type_name, Stream<char> field_name);
			void OmitTypeField(UIReflectionType* type, Stream<char> field_name);

			void OmitTypeFields(Stream<char> type_name, Stream<Stream<char>> fields);
			void OmitTypeFields(UIReflectionType* type, Stream<Stream<char>> fields);

			// Call this if the given instance already was bound to a pointer
			void RebindInstancePtrs(UIReflectionInstance* instance, void* data);

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

		void UIReflectionDefaultValueAction(ActionData* action_data);

	}
}