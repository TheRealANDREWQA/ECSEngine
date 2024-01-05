#pragma once
#include "../../Core.h"
#include "../../ECS/EntityManagerSerializeTypes.h"
#include "../../Multithreading/TaskSchedulerTypes.h"
#include "../../Resources/AssetMetadata.h"
#include "../../Tools/UI/UIStructures.h"
#include "../../Input/InputMapping.h"

namespace ECSEngine {

	namespace Tools {
		struct UIDrawer;
		struct UIReflectionDrawer;
		struct UIWindowDescriptor;
	}

	struct AssetDatabase;
	struct TaskManager;
	struct EntityManager;

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleTaskFunctionData {
		CapacityStream<TaskSchedulerElement>* tasks;
		AllocatorPolymorphic allocator;

		// If this flag is set to true, then it will add dependencies
		// To the given tasks such that the order is maintained in the same group
		bool maintain_order_in_group = false;
	};

	// The mandatory function of the module. These tasks are used so that the runtime knows what the module
	// actually wants to run. If you want to send data to the tasks and that data needs to be allocated,
	// use the allocator given for that.
	typedef void (*ModuleTaskFunction)(ModuleTaskFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleUIFunctionData {
		CapacityStream<Tools::UIWindowDescriptor>* window_descriptors;
		AllocatorPolymorphic allocator;
	};

	// The module can implement custom menus, debugging utilities and or visualization tools
	// If data needs to be allocated (names or other types of data for the window) use the given allocator
	typedef void (*ModuleUIFunction)(ModuleUIFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	// The reflection drawer can be used to draw the component
	struct ModuleUIComponentDrawData {
		Tools::UIReflectionDrawer* reflection_drawer;
		Tools::UIDrawer* drawer;
		void* component;
		bool* has_changed;
		void* extra_data;
	};

	// When a value is changed, make sure to set the has_changed flag such that the editor can react accordingly
	typedef void (*ModuleUIComponentDraw)(ModuleUIComponentDrawData* data);

	struct ModuleUIComponentDrawFunctionsElement {
		ModuleUIComponentDraw draw_function;
		const char* component_name;
		AssetDatabase* asset_database;
		void* extra_data;
		size_t extra_data_size = 0;
	};

	// Allocate the elements using the given allocator
	struct ModuleUIComponentDrawFunctionsData {
		CapacityStream<ModuleUIComponentDrawFunctionsElement>* elements;
		AllocatorPolymorphic allocator;
	};

	typedef void (*ModuleUIComponentDrawFunctions)(ModuleUIComponentDrawFunctionsData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleSerializeComponentStreams {
		ECS_INLINE const void* GetAllocatedBuffer() const {
			return serialize_components.buffer;
		}

		Stream<SerializeEntityManagerComponentInfo> serialize_components;
		Stream<SerializeEntityManagerSharedComponentInfo> serialize_shared_components;
		Stream<SerializeEntityManagerGlobalComponentInfo> serialize_global_components;
		Stream<DeserializeEntityManagerComponentInfo> deserialize_components;
		Stream<DeserializeEntityManagerSharedComponentInfo> deserialize_shared_components;
		Stream<DeserializeEntityManagerGlobalComponentInfo> deserialize_global_components;
	};

	struct ModuleSerializeComponentFunctionData {
		CapacityStream<SerializeEntityManagerComponentInfo>* serialize_components;
		CapacityStream<SerializeEntityManagerSharedComponentInfo>* serialize_shared_components;
		CapacityStream<SerializeEntityManagerGlobalComponentInfo>* serialize_global_components;
		CapacityStream<DeserializeEntityManagerComponentInfo>* deserialize_components;
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>* deserialize_shared_components;
		CapacityStream<DeserializeEntityManagerGlobalComponentInfo>* deserialize_global_components;
		AllocatorPolymorphic allocator;
	};

	// With this function the module can override the default serialization (reflection serialization) of the components into a custom serialization.
	// The names must be used to refer to the component that is being overridden
	typedef void (*ModuleSerializeComponentFunction)(
		ModuleSerializeComponentFunctionData* data
	);

	// ----------------------------------------------------------------------------------------------------------------------

	enum ECS_MODULE_BUILD_DEPENDENCY : unsigned char {
		ECS_MODULE_BUILD_DEPENDENCY_MESH,
		ECS_MODULE_BUILD_DEPENDENCY_TEXTURE,
		ECS_MODULE_BUILD_DEPENDENCY_GPU_SAMPLER,
		ECS_MODULE_BUILD_DEPENDENCY_SHADER,
		ECS_MODULE_BUILD_DEPENDENCY_MATERIAL,
		ECS_MODULE_BUILD_DEPENDENCY_MISC_ASSET,
		ECS_MODULE_BUILD_DEPENDENCY_SETTING,
		ECS_MODULE_BUILD_DEPENDENCY_COUNT
	};

	// Only the path or the name can be active a time
	// If the path is used (for files this is always the case), then whenever that file changes, 
	// the dependency will trigger
	// If the name is used (for assets that support this - the material - and the setting),
	// the dependecy will trigger when the metadata associated to that file or the file itself changes
	struct ModuleBuildDependencyType {
		ECS_MODULE_BUILD_DEPENDENCY dependency;
		union {
			Stream<char> name;
			Stream<wchar_t> path;
		};
	};

	struct ModuleSettingName {
		void* settings;
		Stream<char> name;
	};

	union ModuleBuildAssetDependencyMetadata {
		MeshMetadata mesh;
		TextureMetadata texture;
		GPUSamplerMetadata gpu_sampler;
		ShaderMetadata shader;
		PBRMaterial material;
		MiscAsset misc_asset;
	};

	struct ModuleBuildAssetDependencyData {
		Stream<void> data;
		ModuleBuildAssetDependencyMetadata metadata;
	};

	struct ModuleBuildAssetFunctionData {
		Stream<ModuleBuildAssetDependencyMetadata> dependency_data;
		
		Stream<void> current_asset_data;
		Stream<ModuleSettingName> settings;
		AllocatorPolymorphic allocator;
	};

	typedef Stream<void> (*ModuleBuildAssetFunction)(ModuleBuildAssetFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleLoadAssetFunctionData {
		Stream<wchar_t> path;
		Stream<ModuleSettingName> settings;
		AllocatorPolymorphic allocator;
		TaskManager* task_manager;
	};

	typedef Stream<void> (*ModuleLoadAssetFunction)(ModuleLoadAssetFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleWriteAssetFunctionData {
		Stream<void> current_asset_data;
		Stream<wchar_t> path;
		TaskManager* task_manager;
	};

	typedef bool (*ModuleWriteAssetFunction)(ModuleWriteAssetFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleRegisterAssetDependenciesData {
		Stream<void> data;
		CapacityStream<ModuleBuildDependencyType>& dependencies;
	};

	typedef void (*ModuleRegisterAssetDependencies)(ModuleRegisterAssetDependenciesData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleBuildAssetType {
		// This is the name when searching for the metadata name
		// It can be nullptr if there is no metadata needed, only the dependencies
		Stream<char> asset_metadata_name = { nullptr, 0 };

		// This is the name that will be displayed in the Editor
		// If nullptr, the type name must be specified
		Stream<char> asset_editor_name = { nullptr, 0 };

		// This is the extension that will be used to identify this type of asset
		Stream<wchar_t> extension;
		
		// This function is mandatory
		ModuleBuildAssetFunction build_function;
		// If this function is nullptr, it means that it will look for all stream types of char/wchar_t in the type_name or metadata_name and
		// use those in order to register the dependencies. The dependency types must be specified in this order
		ModuleRegisterAssetDependencies register_function = nullptr;

		// If this function is nullptr, the data will be blitted or if the type name is specified
		// it will be loaded using the binary deserializer
		ModuleLoadAssetFunction load_function = nullptr;
		// Same as above, if the name is specified it will be written using the binary serializer
		ModuleWriteAssetFunction write_function = nullptr;

		// If this function is nullptr and the asset metadata name is specified it will use the binary serializer
		ModuleLoadAssetFunction metadata_load_function = nullptr;
		// Same as above.
		ModuleWriteAssetFunction metadata_write_function = nullptr;

		// The types of dependencies that this asset has
		Stream<ECS_MODULE_BUILD_DEPENDENCY> dependencies;
	};

	struct ModuleBuildFunctionsData {
		CapacityStream<ModuleBuildAssetType>* asset_types;
		// This memory is used to transfer the asset names, extension and dependencies if those are not stable
		AllocatorPolymorphic allocator;
	};

	// This function can be used to inform the editor build system about module's assets
	typedef void (*ModuleBuildFunctions)(ModuleBuildFunctionsData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	// The assets are given in the order of the link component's fields. They are given
	// such that the component can be easily created. All types besides the misc type have only the buffer set.
	// The size is set only for the misc type. Use the ExtractLinkComponentFunctionAsset functions
	// to type cast the pointers (in ModuleUtilities.h)
	struct ModuleLinkComponentFunctionData {
		const void* link_component;
		void* component;
		Stream<Stream<void>> assets;

		// These are optional. They will be provided when a value is changed through the
		// Inspector panel, else they cannot be provided
		const void* previous_link_component;
		const void* previous_component;
	};

	typedef void (*ModuleLinkComponentFunction)(ModuleLinkComponentFunctionData* data);

	// The asset handles are in order of the target component's fields. They are given
	// such that they can be easily replaced
	struct ModuleLinkComponentReverseFunctionData {
		const void* component;
		void* link_component;
		Stream<unsigned int> asset_handles;

		// These are optional. They will be provided when a value is changed through the
		// Inspector panel, else they cannot be provided
		const void* previous_component;
		const void* previous_link_component;
	};

	typedef void (*ModuleLinkComponentReverseFunction)(ModuleLinkComponentReverseFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleLinkComponentApplyModifierFieldsFunctionData {
		const void* link_component;
		void* component;
		Stream<Stream<void>> asset_handles;

		const void* previous_link_component;
		const void* previous_component;
	};

	// This function will be called when a modifier field (or if you activated the button when it is pressed)
	// Such that the values from the link can be applied to the target
	typedef void (*ModuleLinkComponentApplyModifierFunction)(ModuleLinkComponentApplyModifierFieldsFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleLinkComponentTarget {
		ModuleLinkComponentFunction build_function;
		ModuleLinkComponentReverseFunction reverse_function;
		Stream<char> component_name;

		ModuleLinkComponentApplyModifierFunction apply_modifier = nullptr;
		bool apply_modifier_needs_button = false;
	};

	struct ModuleRegisterLinkComponentFunctionData {
		CapacityStream<ModuleLinkComponentTarget>* functions;
		// The name of the functions either have to be stable - i.e. hard coded - or allocated from this allocator
		AllocatorPolymorphic allocator;
	};

	// This function can be used to make link components - components that are front facing
	// and are transformed into runtime components
	typedef void (*ModuleRegisterLinkComponentFunction)(ModuleRegisterLinkComponentFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	// With this function the module is informed about which world it is currently working in
	typedef void (*ModuleSetCurrentWorld)(World* world);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleMiscInfo {
		ECS_INLINE size_t CopySize() const {
			return key.CopySize() + StreamCoalescedDeepCopySize(value);
		}

		ECS_INLINE ModuleMiscInfo CopyTo(uintptr_t& ptr) const {
			ModuleMiscInfo copy;

			copy.key.InitializeAndCopy(ptr, key);
			copy.value = StreamCoalescedDeepCopy(value, ptr);

			return copy;
		}

		Stream<char> key;
		Stream<Stream<void>> value;
	};

	// This structure can be used to inform the Editor/Runtime About extra miscellaneous requirements of info. 
	// The values as stored as pairs of key and value in order to ease the finding
	struct ModuleExtraInformation {
		ECS_INLINE Stream<Stream<void>> Find(Stream<char> key) const {
			size_t index = pairs.Find(key, [](ModuleMiscInfo info) {
				return info.key;
			});
			return index == -1 ? Stream<Stream<void>>(nullptr, 0) : pairs[index].value;
		}

		// Returns true if it has at least one entry
		ECS_INLINE bool IsValid() const {
			return pairs.size > 0;
		}

		Stream<ModuleMiscInfo> pairs;
	};

	struct ModuleRegisterExtraInformationFunctionData {
		ResizableStream<ModuleMiscInfo> extra_information;
		// The name of the functions either have to be stable - i.e. hard coded - or allocated from this allocator
		AllocatorPolymorphic allocator;
	};

	typedef void (*ModuleRegisterExtraInformationFunction)(ModuleRegisterExtraInformationFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct DebugDrawer;

	struct ModuleDebugDrawComponentFunctionData {
		const void* component;
		unsigned int thread_id;
		DebugDrawer* debug_drawer;

		// These are filled in the same order as you provide them
		// May be null if the entity doesn't have it
		const void** dependency_components;
	};

	typedef void (*ModuleDebugDrawComponentFunction)(ModuleDebugDrawComponentFunctionData* data);

	// A dependency component may or may not be there - you should check to see if the pointer
	// Is valid before accesing it
	struct ModuleDebugDrawElement {
		ECS_INLINE void AddDependency(ComponentWithType component_with_type) {
			ECS_ASSERT(dependency_component_count < std::size(dependency_components));
			dependency_components[dependency_component_count++] = component_with_type;
		}

		ECS_INLINE Stream<ComponentWithType> Dependencies() const {
			return { dependency_components, dependency_component_count };
		}

		ModuleDebugDrawComponentFunction draw_function = nullptr;
		Component component = -1;
		ECS_COMPONENT_TYPE component_type = ECS_COMPONENT_TYPE_COUNT;

		unsigned char dependency_component_count = 0;
		ComponentWithType dependency_components[4];
	};

	struct ModuleRegisterDebugDrawFunctionData {
		CapacityStream<ModuleDebugDrawElement>* elements;
	};

	typedef void (*ModuleRegisterDebugDrawFunction)(ModuleRegisterDebugDrawFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleDebugDrawTaskElement {
		ECS_INLINE size_t CopySize() const {
			return base_element.CopySize();
		}

		ECS_INLINE ModuleDebugDrawTaskElement CopyTo(uintptr_t& ptr) const {
			ModuleDebugDrawTaskElement copy;
			copy.base_element = base_element.CopyTo(ptr);
			copy.scene_only = scene_only;
			copy.input_element = input_element;
			return copy;
		}

		// The component query is not used, the rest of the fields are
		// The initialize function cannot output data, it can only inherit it
		TaskSchedulerElement base_element;
		bool scene_only = true;
		// You can set an input mapping that activates/deactivates the state for this element
		InputMappingElement input_element;
	};
	
	struct ModuleRegisterDebugDrawTaskElementsData {
		CapacityStream<ModuleDebugDrawTaskElement>* elements;
	};

	// This function will register all tasks that are considered to be debug draw tasks
	// These are useful for the in editor context where they can be enabled and disabled
	// without the task having to do that explicitely. They are treated like normal tasks
	typedef void (*ModuleRegisterDebugDrawTaskElementsFunction)(ModuleRegisterDebugDrawTaskElementsData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleComponentBuildFunctionData {
		EntityManager* entity_manager;
		Entity entity;
		void* component;
		// This memory can be used for the returned thread task data
		CapacityStream<void>* stack_memory;
	};

	// The build function must deallocate the component in case there is data in it
	// The return type can be a task that can be run asynchronously in case it takes a long time
	typedef ThreadTask (*ModuleComponentBuildFunction)(ModuleComponentBuildFunctionData* data);

	struct ModuleComponentBuildEntry {
		ModuleComponentBuildFunction function;
		Stream<Stream<char>> component_dependencies;
	};

	struct ModuleComponentFunctions {
		ECS_INLINE ModuleComponentFunctions() {
			memset(this, 0, sizeof(*this));
		}

		ECS_INLINE size_t CopySize() const {
			return component_name.CopySize() + copy_deallocate_data.CopySize() + compare_function_data.CopySize();
		}

		ECS_INLINE ModuleComponentFunctions Copy(AllocatorPolymorphic allocator) const {
			ModuleComponentFunctions copy = *this;
			copy.component_name = component_name.Copy(allocator);
			Stream<void> new_copy_deallocate_data = copy_deallocate_data.size == 0 ? copy_deallocate_data : copy_deallocate_data.Copy(allocator);
			Stream<void> new_compare_data = compare_function_data.size == 0 ? compare_function_data : compare_function_data.Copy(allocator);
			copy.copy_deallocate_data = new_copy_deallocate_data;
			copy.compare_function_data = new_compare_data;
			return copy;
		}

		ECS_INLINE ModuleComponentFunctions CopyTo(uintptr_t& ptr) const {
			ModuleComponentFunctions copy = *this;
			copy.component_name = component_name.CopyTo(ptr);
			Stream<void> new_copy_deallocate_data = copy_deallocate_data.size == 0 ? copy_deallocate_data : copy_deallocate_data.CopyTo(ptr);
			Stream<void> new_compare_data = compare_function_data.size == 0 ? compare_function_data : compare_function_data.CopyTo(ptr);
			copy.copy_deallocate_data = new_copy_deallocate_data;
			copy.compare_function_data = new_compare_data;
			return copy;
		}

		ECS_INLINE void SetComponentFunctionsTo(ComponentFunctions* functions) const {
			functions->allocator_size = allocator_size;
			functions->data = copy_deallocate_data;
			functions->copy_function = copy_function;
			functions->deallocate_function = deallocate_function;
		}

		ECS_INLINE void SetCompareFunctionTo(SharedComponentCompareFunction* function, Stream<void>* function_data) const {
			*function = compare_function;
			*function_data = compare_function_data;
		}

		size_t allocator_size;
		ModuleComponentBuildFunction reset_function;
		ComponentCopyFunction copy_function;
		ComponentDeallocateFunction deallocate_function;
		Stream<void> copy_deallocate_data;
		Stream<char> component_name;

		// Only valid for shared components
		SharedComponentCompareFunction compare_function;
		Stream<void> compare_function_data;
	};

	struct ModuleRegisterComponentFunctionsData {
		CapacityStream<ModuleComponentFunctions>* functions;
	};

	typedef void (*ModuleRegisterComponentFunctionsFunction)(ModuleRegisterComponentFunctionsData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	// Module function missing is returned for either graphics function missing
	enum ECS_MODULE_STATUS : unsigned char {
		ECS_GET_MODULE_OK,
		ECS_GET_MODULE_FAULTY_PATH,
		ECS_GET_MODULE_FUNCTION_MISSING
	};

	struct Module {
		ECS_MODULE_STATUS code;

		// The function pointers
		ModuleTaskFunction task_function;
		ModuleUIFunction ui_function;
		ModuleBuildFunctions build_functions;
		ModuleSerializeComponentFunction serialize_function;
		ModuleRegisterLinkComponentFunction link_components;
		ModuleSetCurrentWorld set_world;
		ModuleRegisterExtraInformationFunction extra_information;
		ModuleRegisterDebugDrawFunction debug_draw;
		ModuleRegisterDebugDrawTaskElementsFunction debug_draw_tasks;
		ModuleRegisterComponentFunctionsFunction component_functions;

		void* os_module_handle;
	};

	struct AppliedModule {
		Module base_module;

		// The streams for the module
		Stream<TaskSchedulerElement> tasks;
		Stream<Tools::UIWindowDescriptor> ui_descriptors;
		Stream<ModuleBuildAssetType> build_asset_types;
		Stream<ModuleLinkComponentTarget> link_components;
		ModuleSerializeComponentStreams serialize_streams;
		ModuleExtraInformation extra_information;
		Stream<ModuleDebugDrawElement> debug_draw_elements;
		Stream<ModuleDebugDrawTaskElement> debug_draw_task_elements;
		Stream<ModuleComponentFunctions> component_functions;
	};

	// ----------------------------------------------------------------------------------------------------------------------

}