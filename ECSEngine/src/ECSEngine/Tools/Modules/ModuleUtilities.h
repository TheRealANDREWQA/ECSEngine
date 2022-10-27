#pragma once
#include "ModuleDefinition.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct AssetDatabase;

	// Returns true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	// If the allocator is not provided, then it will only reference the fields (it will not make a deep copy)
	ECSENGINE_API bool ModuleFromTargetToLinkComponent(
		ModuleLinkComponentTarget module_link,
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* target_type,
		const Reflection::ReflectionType* link_type,
		const AssetDatabase* asset_database,
		const void* target_data,
		void* link_data,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// Return true if the conversion is successful. It can fail if the component needs a DLL function
	// but none is provided or if an asset is incorrectly represented (streams at the moment cannot be represented)
	// If the allocator is not provided, then it will only reference the streams (it will not make a deep copy)
	ECSENGINE_API bool ModuleLinkComponentToTarget(
		ModuleLinkComponentTarget module_link,
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* link_type,
		const Reflection::ReflectionType* target_type,
		const AssetDatabase* asset_database,
		const void* link_data,
		void* target_data,
		AllocatorPolymorphic allocator = { nullptr }
	);
	
}