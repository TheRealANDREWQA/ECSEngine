#pragma once
#include "ModuleDefinition.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct AssetDatabase;

	ECS_INLINE CoallescedMesh* ExtractLinkComponentFunctionMesh(void* buffer) {
		return (CoallescedMesh*)buffer;
	}

	ECS_INLINE ResourceView ExtractLinkComponentFunctionTexture(void* buffer) {
		return (ID3D11ShaderResourceView*)buffer;
	}

	ECS_INLINE SamplerState ExtractLinkComponentFunctionGPUSampler(void* buffer) {
		return (ID3D11SamplerState*)buffer;
	}

	template<typename ShaderType>
	ECS_INLINE ShaderType ExtractLinkComponentFunctionShader(void* buffer) {
		return ShaderType::FromInterface(buffer);
	}

	ECS_INLINE Material* ExtractLinkComponentFunctionMaterial(void* buffer) {
		return (Material*)buffer;
	}

	ECSENGINE_API void ModuleResetLinkComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* link_component
	);

	// Returns true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	ECSENGINE_API bool ModuleFromTargetToLinkComponent(
		ModuleLinkComponentTarget module_link,
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* target_type,
		const Reflection::ReflectionType* link_type,
		const AssetDatabase* asset_database,
		const void* target_data,
		void* link_data
	);

	// Return true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	ECSENGINE_API bool ModuleLinkComponentToTarget(
		ModuleLinkComponentTarget module_link,
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* link_type,
		const Reflection::ReflectionType* target_type,
		const AssetDatabase* asset_database,
		const void* link_data,
		void* target_data
	);
	
}