#pragma once
#include "../../Core.h"
#include "../../Internal/EntityManagerSerializeTypes.h"
#include "../../Internal/Multithreading/TaskDependencies.h"
#include "../../Internal/Resources/AssetMetadata.h"

namespace ECSEngine {

	struct World;
	struct AssetDatabase;
	namespace Tools {
		struct UIDrawer;
		struct UIWindowDescriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleTaskFunctionData {
		World* world;
		CapacityStream<TaskDependencyElement>& tasks;
	};

	typedef void (*ModuleTaskFunction)(ModuleTaskFunctionData* data);

//#define ECS_MODULE_TASK_FUNCTION void ModuleTaskFunction(ECSEngine::World* world, ECSEngine::CapacityStream<ECSEngine::TaskDependencyElement>& tasks)

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleUIFunctionData {
		World* world;
		CapacityStream<Tools::UIWindowDescriptor>& window_descriptors;
	};

	typedef void (*ModuleUIFunction)(ModuleUIFunctionData* data);

//#define ECS_MODULE_UI_FUNCTION void ModuleUIFunction(ECSEngine::World* world, ECSEngine::CapacityStream<ECSEngine::Tools::UIWindowDescriptor>& window_descriptors)

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleSerializeExtractComponent {
		SerializeEntityManagerExtractComponent function;
		Component component;
	};

	struct ModuleSerializeExtractSharedComponent {
		DeserializeEntityManagerExtractSharedComponent function;
		Component component;
	};

	struct ModuleDeserializeExtractComponent {
		DeserializeEntityManagerExtractComponent function;
		Component component;
	};

	struct ModuleDeserializeExtractSharedComponent {
		DeserializeEntityManagerExtractSharedComponent function;
		Component component;
	};

	struct ModuleSerializeComponentFunctionData {
		World* world;
		CapacityStream<ModuleSerializeExtractComponent>& serialize_components;
		CapacityStream<ModuleSerializeExtractSharedComponent>& serialize_shared_components;
		CapacityStream<ModuleDeserializeExtractComponent>& deserialize_components;
		CapacityStream<ModuleDeserializeExtractSharedComponent>& deserialize_shared_components;
		CapacityStream<void>& extra_data;
	};

	typedef void (*ModuleSerializeComponentFunction)(
		ModuleSerializeComponentFunctionData* data
	);

//#define ECS_MODULE_SERIALIZE_COMPONENT_FUNCTION void ModuleSerializeComponentFunction( \
//		ECSEngine::World* world, \
//		ECSEngine::CapacityStream<ECSEngine::ModuleSerializeExtractComponent>& serialize_components, \
//		ECSEngine::CapacityStream<ECSEngine::ModuleSerializeExtractSharedComponent>& serialize_shared_components, \
//		ECSEngine::CapacityStream<ECSEngine::ModuleDeserializeExtractComponent>& deserialize_components, \
//		ECSEngine::CapacityStream<ECSEngine::ModuleDeserializeExtractSharedComponent>& deserialize_shared_components, \
//		ECSEngine::CapacityStream<void>& extra_data \
//	)

	// ----------------------------------------------------------------------------------------------------------------------

	enum ECS_MODULE_BUILD_COMMAND_DEPENDENCY : unsigned char {
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_MESH,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_TEXTURE,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_GPU_BUFFER,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_GPU_SAMPLER,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_SHADER,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_MATERIAL,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_FILE,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_SETTING,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_MULTIPLE,
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY_COUNT
	};

	// Only the path or the name can be active a time
	// Use the use_path flag to indicate which one to use
	// If the path is used (for files this is always the case), then whenever that file changes, 
	// the dependency will trigger
	// If the name is used (for assets that support this, the material only this allows),
	// the dependecy will trigger when the metadata associated to that file or the file itself changes
	struct ModuleBuildDependencyType {
		ECS_MODULE_BUILD_COMMAND_DEPENDENCY dependency;
		union {
			Stream<char> name;
			Stream<wchar_t> path;
			Stream<ModuleBuildDependencyType> multiple;
		};
	};

	struct ModuleSettingName {
		void* settings;
		Stream<char> name;
	};

	union ModuleBuildAssetDependencyMetadata {
		MeshMetadata mesh;
		TextureMetadata texture;
		GPUBufferMetadata gpu_buffer;
		GPUSamplerMetadata gpu_sampler;
		ShaderMetadata shader;
		Material material;
	};

	struct ModuleBuildAssetFunctionData {
		Stream<void> previous_dependency_data;
		Stream<void> new_dependency_data;
		ModuleBuildAssetDependencyMetadata previous_dependency_metadata;
		ModuleBuildAssetDependencyMetadata new_dependency_metadata;

		ModuleBuildDependencyType dependency_type;
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
	};

	typedef Stream<void> (*ModuleLoadAssetFunction)(ModuleLoadAssetFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleWriteAssetFunctionData {
		Stream<void> current_asset_data;
		Stream<wchar_t> path;
	};

	typedef bool (*ModuleWriteAssetFunction)(ModuleWriteAssetFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleLinkComponentFunctionData {
		const void* link_component;
		void* component;
		Entity entity;
		World* world;
	};

	typedef void (*ModuleLinkComponentFunction)(ModuleLinkComponentFunctionData* data);

	// ----------------------------------------------------------------------------------------------------------------------

	struct ModuleBuildAssetType {
		const wchar_t* extension;
		ModuleLoadAssetFunction load_function;
		ModuleWriteAssetFunction write_function;
		ModuleBuildAssetFunction build_function;

		unsigned short load_function_tasks;
		unsigned short write_function_tasks;
		unsigned short build_function_tasks;

		Stream<ModuleBuildDependencyType> dependency_types;
	};

	typedef void (*ModuleBuildFunctions)(CapacityStream<ModuleBuildAssetType>& asset_types);

	// ----------------------------------------------------------------------------------------------------------------------

}