#pragma once
#include "EntityManager.h"

namespace ECSEngine {

	ECSENGINE_API bool SerializeEntityManager(const EntityManager* entity_manager, Stream<wchar_t> filename, AllocatorPolymorphic allocator = { nullptr });

	ECSENGINE_API void SerializeEntityManager(const EntityManager* entity_manager, uintptr_t stream);

	ECSENGINE_API bool DeserializeEntityManager(EntityManager* entity_manager, Stream<wchar_t> filename, AllocatorPolymorphic allocator = { nullptr });

	ECSENGINE_API void DeserializeEntityManager(EntityManager* entity_manager, uintptr_t stream);

}