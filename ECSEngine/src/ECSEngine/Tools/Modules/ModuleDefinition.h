#pragma once
#include "../../Core.h"
#include "../../Internal/EntityManagerSerializeTypes.h"
#include "../../Internal/Multithreading/TaskSchedulerTypes.h"
#include "../../Internal/Resources/AssetMetadata.h"
#include "../../Tools/UI/UIStructures.h"
#include "../../Internal/World.h"

namespace ECSEngine {

	namespace Tools {
		struct UIDrawer;
		struct UIReflectionDrawer;
	}

	struct AssetDatabase;

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleTaskFunctionData {
		CapacityStream<TaskSchedulerElement>* tasks;
		AllocatorPolymorphic allocator;
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
		inline const void* GetAllocatedBuffer() const {
			return serialize_components.buffer;
		}

		Stream<SerializeEntityManagerComponentInfo> serialize_components;
		Stream<SerializeEntityManagerSharedComponentInfo> serialize_shared_components;
		Stream<DeserializeEntityManagerComponentInfo> deserialize_components;
		Stream<DeserializeEntityManagerSharedComponentInfo> deserialize_shared_components;
	};

	struct ModuleSerializeComponentFunctionData {
		CapacityStream<SerializeEntityManagerComponentInfo>* serialize_components;
		CapacityStream<SerializeEntityManagerSharedComponentInfo>* serialize_shared_components;
		CapacityStream<DeserializeEntityManagerComponentInfo>* deserialize_components;
		CapacityStream<DeserializeEntityManagerSharedComponentInfo>* deserialize_shared_components;
		AllocatorPolymorphic allocator;
	};

	// With this function the module can override the default serialization (bit blitting) of the components into a custom serialization.
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
	};

	typedef void (*ModuleLinkComponentFunction)(ModuleLinkComponentFunctionData* data);

	// The asset handles are in order of the target component's fields. They are given
	// such that they can be easily replaced
	struct ModuleLinkComponentReverseFunctionData {
		const void* component;
		void* link_component;
		Stream<unsigned int> asset_handles;
	};

	typedef void (*ModuleLinkComponentReverseFunction)(ModuleLinkComponentReverseFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleLinkComponentTarget {
		ModuleLinkComponentFunction build_function;
		ModuleLinkComponentReverseFunction reverse_function;
		Stream<char> component_name;
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

		HMODULE os_module_handle;
	};

	struct AppliedModule {
		Module base_module;

		// The streams for the module
		Stream<TaskSchedulerElement> tasks;
		Stream<UIWindowDescriptor> ui_descriptors;
		Stream<ModuleBuildAssetType> build_asset_types;
		Stream<ModuleLinkComponentTarget> link_components;
		ModuleSerializeComponentStreams serialize_streams;
	};

	// ----------------------------------------------------------------------------------------------------------------------

}