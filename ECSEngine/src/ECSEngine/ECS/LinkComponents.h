#pragma once
#include "../Tools/Modules/ModuleDefinition.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct AssetDatabase;

	// Returns { nullptr, 0 } if there is no target specified
	ECSENGINE_API Stream<char> GetReflectionTypeLinkComponentTarget(const Reflection::ReflectionType* type);

	// If it ends in Link, it will return the name without that suffix
	ECSENGINE_API Stream<char> GetReflectionTypeLinkNameBase(Stream<char> name);

	// Returns the name for the link component (e.g. Translation -> TranslationLink)
	ECSENGINE_API Stream<char> GetReflectionTypeLinkComponentName(Stream<char> name, CapacityStream<char>& link_name);

	// ------------------------------------------------------------------------------------------------------------

	// Returns the name of the type if there is a link type that targets the given type (using the ECS_REFLECT_LINK_COMPONENT()). 
	// If suffixed_names is given then it will also search for types that have a certain suffix for the given target.
	// If there is none it will return { nullptr, 0 }
	ECSENGINE_API Stream<char> GetLinkComponentForTarget(
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<char> target, 
		Stream<char> suffixed_name = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	// This version is the same as the single variant the difference being that it will fill in the link names
	// in the link_names pointer. There must be targets.size spots allocated in the link_names
	ECSENGINE_API void GetLinkComponentForTargets(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<Stream<char>> targets,
		Stream<char>* link_names,
		Stream<char> suffixed_name = { nullptr, 0 }
	);

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE CoalescedMesh* ExtractLinkComponentFunctionMesh(void* buffer) {
		return (CoalescedMesh*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE ResourceView ExtractLinkComponentFunctionTexture(void* buffer) {
		return (ID3D11ShaderResourceView*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE SamplerState ExtractLinkComponentFunctionGPUSampler(void* buffer) {
		return (ID3D11SamplerState*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename ShaderType>
	ECS_INLINE ShaderType ExtractLinkComponentFunctionShader(void* buffer) {
		return ShaderType::FromInterface(buffer);
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE Material* ExtractLinkComponentFunctionMaterial(void* buffer) {
		return (Material*)buffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void ResetLinkComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* link_component
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all the link components which target a unique component
	ECSENGINE_API void GetUniqueLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all the link components which target a shared component
	ECSENGINE_API void GetSharedLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all the link components which target a global component
	ECSENGINE_API void GetGlobalLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	// Fills in all link components which target a shared component or unique component
	ECSENGINE_API void GetAllLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& unique_link_types,
		CapacityStream<const Reflection::ReflectionType*>& shared_link_types,
		CapacityStream<const Reflection::ReflectionType*>& global_link_types
	);

	// ------------------------------------------------------------------------------------------------------------

	struct ConvertToAndFromLinkBaseData {
		// ----------------- Mandatory ---------------------------
		ModuleLinkComponentTarget module_link;
		const Reflection::ReflectionManager* reflection_manager;
		const Reflection::ReflectionType* target_type;
		const Reflection::ReflectionType* link_type;
		const AssetDatabase* asset_database;


		// ------------------ Optional ---------------------------
		AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR;	
	};

	// Returns true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	// If the allocator is not provided, then it will only reference the fields (it will not make a deep copy)
	ECSENGINE_API bool ConvertFromTargetToLinkComponent(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* target_data,
		void* link_data,
		const void* previous_target_data,
		const void* previous_link_data
	);

	// ------------------------------------------------------------------------------------------------------------

	// Return true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	// If the allocator is not provided, then it will only reference the streams (it will not make a deep copy)
	ECSENGINE_API bool ConvertLinkComponentToTarget(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* link_data,
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data
	);

	// ------------------------------------------------------------------------------------------------------------

	// It will ignore non asset fields. Return true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	ECSENGINE_API bool ConvertLinkComponentToTargetAssetsOnly(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* link_data,
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data
	);

	// ------------------------------------------------------------------------------------------------------------

}