#include "ecspch.h"
#include "EntityManagerSerialize.h"
#include "../Utilities/Serialization/Binary/SerializeMultisection.h"

#define ENTITY_MANAGER_SERIALIZE_VERSION 1

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool SerializeEntityManager(const EntityManager* entity_manager, Stream<wchar_t> filename, AllocatorPolymorphic allocator)
	{
		unsigned int serialize_version = ENTITY_MANAGER_SERIALIZE_VERSION;
		// The number of multisections needed is equal to the number of archetypes inside the entity manager
		SerializeMultisectionData* multisections = (SerializeMultisectionData*)ECS_STACK_ALLOC(sizeof(SerializeMultisectionData) * entity_manager->m_archetypes.size);

		// Now fill in the multisections
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			// For each archetype, we must serialize:
			// 1. The components
			// 2. The shared components
			// 3. The number of base archetypes
			
			// For each base archetype, we must serialize:
			// 1. The shared instances
			// 3. The number of entities
			// 4. The entities
			// 5. The component data
			// 6. The component byte sizes


		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void SerializeEntityManager(const EntityManager* entity_manager, uintptr_t stream)
	{
		
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool DeserializeEntityManager(EntityManager* entity_manager, Stream<wchar_t> filename, AllocatorPolymorphic allocator)
	{
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void DeserializeEntityManager(EntityManager* entity_manager, uintptr_t stream)
	{
		
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

}