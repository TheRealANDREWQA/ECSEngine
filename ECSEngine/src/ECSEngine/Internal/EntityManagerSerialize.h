#pragma once
#include "EntityManagerSerializeTypes.h"

namespace ECSEngine {

	struct EntityManager;

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeEntityManager(
		const EntityManager* entity_manager, 
		Stream<wchar_t> filename, 
		const SerializeEntityManagerComponentTable* component_table = nullptr,
		void* extra_component_table_data = nullptr,
		const SerializeEntityManagerSharedComponentTable* shared_component_table = nullptr,
		void* extra_shared_component_table_data = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		Stream<wchar_t> filename,
		const DeserializeEntityManagerComponentTable* component_table = nullptr,
		void* extra_component_table_size = nullptr,
		const DeserializeEntityManagerSharedComponentTable* shared_component_table = nullptr,
		void* extra_shared_component_table_data = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------------------------
	
}