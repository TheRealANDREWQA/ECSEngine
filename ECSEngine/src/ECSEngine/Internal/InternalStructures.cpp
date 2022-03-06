#include "ecspch.h"
#include "../Utilities/Function.h"
#include "InternalStructures.h"
#include "VectorComponentSignature.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	EntityPool::EntityPool(
		MemoryManager* memory_manager,
		unsigned int pool_power_of_two
	) : m_memory_manager(memory_manager), m_pool_power_of_two(pool_power_of_two), m_entity_infos(GetAllocatorPolymorphic(memory_manager), 1) {}
	
	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::CreatePool() {
		unsigned int pool_capacity = 1 << m_pool_power_of_two;

		void* allocation = m_memory_manager->Allocate(m_entity_infos.buffer->stream.MemoryOf(pool_capacity));
		// Walk through the entity info stream and look for an empty slot and place the allocation there
		// Cannot remove swap back chunks because the entity index calculation takes into consideration the initial
		// chunk position. Using reallocation tables that would offset chunk indices does not seem like a good idea
		// since adds latency to the lookup
		auto info_stream = StableReferenceStream<EntityInfo, true>(allocation, pool_capacity);
		for (size_t index = 0; index < m_entity_infos.size; index++) {
			if (!m_entity_infos[index].is_in_use) {
				m_entity_infos[index].is_in_use = true;
				m_entity_infos[index].stream = info_stream;
				return;
			}
		}
		m_entity_infos.Add({info_stream, true});
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::CopyEntities(const EntityPool* entity_pool)
	{
		// Assume that everything is deallocated before
		// Everything can be just memcpy'ed after the correct number of pools have been allocated
		ECS_ASSERT(m_entity_infos.size == 0);
		ECS_ASSERT(m_pool_power_of_two == entity_pool->m_pool_power_of_two);

		for (size_t index = 0; index < entity_pool->m_entity_infos.size; index++) {
			CreatePool();
			// Set the is in use flag
			m_entity_infos[index].is_in_use = entity_pool->m_entity_infos[index].is_in_use;

			if (m_entity_infos[index].is_in_use) {
				m_entity_infos[index].stream.Copy(entity_pool->m_entity_infos[index].stream);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int GetEntityIndexFromPoolOffset(unsigned int pool_index, unsigned int pool_power_of_two, unsigned int index) {
		return (pool_index << pool_power_of_two) + index;
	}

	uint2 GetPoolAndEntityIndex(const EntityPool* entity_pool, Entity entity) {
		return { entity.index >> entity_pool->m_pool_power_of_two, entity.index & (entity_pool->m_pool_power_of_two - 1) };
	}

	Entity EntityPoolAllocateImplementation(EntityPool* entity_pool, unsigned short archetype = -1, unsigned short base_archetype = -1, unsigned int stream_index = -1) {	
		EntityInfo info;
		info.main_archetype = archetype;
		info.base_archetype = base_archetype;
		info.stream_index = stream_index;
		info.layer = 0;
		info.tags = 0;

		auto construct_info = [=](unsigned int pool_index) {
			unsigned int entity_index = entity_pool->m_entity_infos[pool_index].stream.ReserveOne();
			EntityInfo* current_info = entity_pool->m_entity_infos[pool_index].stream.ElementPointer(entity_index);
			unsigned char new_generation_count = current_info->generation_count + 1;
			memcpy(current_info, &info, sizeof(info));
			current_info->generation_count = new_generation_count == 0 ? 1 : new_generation_count;

			entity_index = GetEntityIndexFromPoolOffset(pool_index, entity_pool->m_pool_power_of_two, entity_index);
			return Entity(entity_index, current_info->generation_count);
		};
		
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			if (entity_pool->m_entity_infos[index].stream.size < entity_pool->m_entity_infos[index].stream.capacity) {
				return construct_info(index);
			}
		}

		// All pools are full - create a new one and allocate from it
		entity_pool->CreatePool();
		return construct_info(entity_pool->m_entity_infos.size - 1);
	}

	Entity EntityPool::Allocate()
	{
		return EntityPoolAllocateImplementation(this);
	}

	// ------------------------------------------------------------------------------------------------------------

	Entity EntityPool::AllocateEx(unsigned short archetype, unsigned short base_archetype, unsigned int stream_index)
	{
		return EntityPoolAllocateImplementation(this, archetype, base_archetype, stream_index);
	}

	// ------------------------------------------------------------------------------------------------------------

	struct EntityPoolAllocateAdditionalData {
		const unsigned short* archetypes;
		const unsigned short* base_archetypes;
		const unsigned int* stream_indices;
		ushort2 archetype_indices;
		unsigned int copy_position;
	};

	enum EntityPoolAllocateType {
		ENTITY_POOL_ALLOCATE_NO_DATA,
		ENTITY_POOL_ALLOCATE_WITH_INFOS,
		ENTITY_POOL_ALLOCATE_WITH_POSITION
	};
	
	template<EntityPoolAllocateType additional_data_type>
	ECS_INLINE void EntityPoolAllocateImplementation(EntityPool* entity_pool, Stream<Entity> entities, EntityPoolAllocateAdditionalData additional_data = {}) {
		auto loop_iteration = [&entities, entity_pool, additional_data](unsigned int index) {
			if constexpr (additional_data_type == ENTITY_POOL_ALLOCATE_WITH_INFOS) {
				entity_pool->m_entity_infos[index].stream.Reserve({ entities.buffer, entities.size });
				// Increase the generation count for those entities and generate the appropriate infos
				for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
					EntityInfo* info = entity_pool->m_entity_infos[index].stream.ElementPointer(entities[entity_index].value);
					info->generation_count++;
					info->generation_count = info->generation_count == 0 ? 1 : info->generation_count;

					info->tags = 0;
					info->layer = 0;
					info->base_archetype = additional_data.base_archetypes[entity_index];
					info->main_archetype = additional_data.archetypes[entity_index];
					info->stream_index = additional_data.stream_indices[entity_index];

					// Set the appropriate entity now
					entities[entity_index].index = GetEntityIndexFromPoolOffset(index, entity_pool->m_pool_power_of_two, entities[entity_index].value);
					entities[entity_index].generation_count = info->generation_count;
				}
			}
			else if constexpr (additional_data_type == ENTITY_POOL_ALLOCATE_WITH_POSITION) {
				for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
					unsigned int info_index = entity_pool->m_entity_infos[index].stream.ReserveOne();

					EntityInfo* current_info = entity_pool->m_entity_infos[index].stream.ElementPointer(info_index);
					current_info->base_archetype = additional_data.archetype_indices.y;
					current_info->main_archetype = additional_data.archetype_indices.x;
					current_info->stream_index = additional_data.copy_position + entity_index;

					current_info->tags = 0;
					current_info->layer = 0;
					current_info->generation_count++;
					current_info->generation_count = current_info->generation_count == 0 ? 1 : current_info->generation_count;
					// This will be a handle that can be used to index into this specific chunk of the entity pool
					entities[entity_index].index = GetEntityIndexFromPoolOffset(index, entity_pool->m_pool_power_of_two, info_index);
					entities[entity_index].generation_count = current_info->generation_count;
				}
			}
			else {
				entity_pool->m_entity_infos[index].stream.Reserve({ entities.buffer, entities.size });
				// Increase the generation count for those entities and generate the appropriate infos
				for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
					EntityInfo* info = entity_pool->m_entity_infos[index].stream.ElementPointer(entities[entity_index].value);
					info->generation_count++;
					info->generation_count = info->generation_count == 0 ? 1 : info->generation_count;

					info->tags = 0;
					info->layer = 0;
					info->base_archetype = -1;
					info->main_archetype = -1;
					info->stream_index = -1;

					// Set the appropriate entity now
					entities[entity_index].index = GetEntityIndexFromPoolOffset(index, entity_pool->m_pool_power_of_two, entities[entity_index].value);
					entities[entity_index].generation_count = info->generation_count;
				}
			}
		};

		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			if (entity_pool->m_entity_infos[index].stream.size + entities.size < entity_pool->m_entity_infos[index].stream.capacity) {
				loop_iteration(index);
				return;
			}
		}

		// All pools are full - create a new one and allocate from it
		entity_pool->CreatePool();
		unsigned int pool_index = entity_pool->m_entity_infos.size - 1;
		loop_iteration(pool_index);
	}

	void EntityPool::Allocate(Stream<Entity> entities)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_NO_DATA>(this, entities);
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::AllocateEx(Stream<Entity> entities, const unsigned short* archetypes, const unsigned short* base_archetypes, const unsigned int* stream_indices)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_INFOS>(this, entities, {archetypes, base_archetypes, stream_indices});
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::AllocateEx(Stream<Entity> entities, ushort2 archetype_indices, unsigned int copy_position)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_POSITION>(this, entities, { nullptr, nullptr, nullptr, archetype_indices, copy_position });
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo GetInfoCrashCheck(
		const EntityPool* entity_pool,
		Entity entity,
		const char* file,
		const char* function,
		unsigned int line
	) {
		uint2 entity_indices = GetPoolAndEntityIndex(entity_pool, entity);
		ECS_CRASH_RETURN_VALUE_EX(
			entity_indices.x < entity_pool->m_entity_infos.size && entity_pool->m_entity_infos[entity_indices.x].is_in_use,
			{},
			"Invalid entity {#} when trying to retrieve EntityInfo.",
			file,
			function,
			line,
			entity.index
		);

		EntityInfo info = entity_pool->m_entity_infos[entity_indices.x].stream[entity_indices.y];
		bool is_valid = info.generation_count == entity.generation_count;
		if (!is_valid) {
			if (info.generation_count == 0) {
				ECS_CRASH_EX(
					"Generation counter mismatch for entity {#}. The entity is deleted.",
					file,
					function,
					line,
					entity.index
				);
				return {};
			}
			else {
				ECS_CRASH_EX(
					"Generation counter mismatch for entity {#}. Entity counter {#}, info counter {#}.",
					file,
					function,
					line,
					entity.index,
					entity.generation_count,
					info.generation_count
				);
				return {};
			}
		}

		return info;
	}

	EntityInfo* GetInfoCrashCheck(
		EntityPool* entity_pool,
		Entity entity, 
		const char* file,
		const char* function,
		unsigned int line
	) {
		uint2 entity_indices = GetPoolAndEntityIndex(entity_pool, entity);
		ECS_CRASH_RETURN_VALUE_EX(
			entity_indices.x < entity_pool->m_entity_infos.size && entity_pool->m_entity_infos[entity_indices.x].is_in_use,
			nullptr,
			"Invalid entity {#} when trying to retrieve EntityInfo.",
			file,
			function,
			line,
			entity.index
		);

		EntityInfo* info = entity_pool->m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
		bool is_valid = info->generation_count == entity.generation_count;
		if (!is_valid) {
			if (info->generation_count == 0) {
				ECS_CRASH_EX(
					"Generation counter mismatch for entity {#}. The entity is deleted.",
					file,
					function,
					line,
					entity.index
				);
				return nullptr;
			}
			else {
				ECS_CRASH_EX(
					"Generation counter mismatch for entity {#}. Entity counter {#}, info counter {#}.",
					file,
					function,
					line,
					entity.index,
					entity.generation_count,
					info->generation_count
				);
				return nullptr;
			}
		}

		return info;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::ClearTag(Entity entity, unsigned char tag) {
		EntityInfo* info = GetInfoCrashCheck(
			this, 
			entity, 
			__FILE__,
			__FUNCTION__,
			__LINE__
		);
		info->tags &= ~(1 << tag);
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Deallocate(Entity entity)
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		ECS_CRASH_RETURN(
			entity_indices.x < m_entity_infos.size&& m_entity_infos[entity_indices.x].is_in_use,
			"Incorrect entity {2} when trying to delete it.",
			entity.index
		);

		EntityInfo* info = m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
		// Check that they have the same generation counter
		ECS_CRASH_RETURN(info->generation_count == entity.generation_count, "Trying to delete an entity {2} which has already been deleted.", entity.index);
		m_entity_infos[entity_indices.x].stream.Remove(entity_indices.y);
		// Signal that there is no entity allocated in this position
		info->generation_count = 0;

		if (m_entity_infos[entity_indices.x].stream.size == 0) {
			// Deallocate the buffer and mark as unused
			m_memory_manager->Deallocate(m_entity_infos[entity_indices.x].stream.buffer);
			// When iterating for an allocation, since the size and the capacity are both set to 0
			// The iteration will just skip this block - as it should
			m_entity_infos[entity_indices.x].stream.buffer = nullptr;
			m_entity_infos[entity_indices.x].stream.free_list = nullptr;
			m_entity_infos[entity_indices.x].stream.size = 0;
			m_entity_infos[entity_indices.x].stream.capacity = 0;
			m_entity_infos[entity_indices.x].is_in_use = false;
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Deallocate(Stream<Entity> entities)
	{
		for (size_t index = 0; index < entities.size; index++) {
			Deallocate(entities[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	bool EntityPool::IsValid(Entity entity) {
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		if (entity_indices.x >= m_entity_infos.size || m_entity_infos[entity_indices.y].is_in_use) {
			return false;
		}
		EntityInfo info = m_entity_infos[entity_indices.x].stream[entity_indices.y];
		return info.generation_count == entity.generation_count;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool EntityPool::HasTag(Entity entity, unsigned char tag) const
	{

		EntityInfo info = GetInfoCrashCheck(
			this, 
			entity, 
			__FILE__,
			__FUNCTION__,
			__LINE__
		);

		return (info.tags & tag) != 0;
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo EntityPool::GetInfo(Entity entity) const {
		return GetInfoCrashCheck(
			this,
			entity,
			__FILE__,
			__FUNCTION__,
			__LINE__
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo EntityPool::GetInfoNoChecks(Entity entity) const
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		return m_entity_infos[entity_indices.x].stream[entity_indices.y];
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo* EntityPool::GetInfoPtr(Entity entity)
	{
		return GetInfoCrashCheck(
			this,
			entity,
			__FILE__,
			__FUNCTION__,
			__LINE__
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo* EntityPool::GetInfoPtrNoChecks(Entity entity)
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		return m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
	}
	
	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetEntityInfo(Entity entity, unsigned short archetype, unsigned short base_archetype, unsigned int stream_index) {
		EntityInfo* info = GetInfoCrashCheck(this, entity, __FILE__, __FUNCTION__, __LINE__);

		info->base_archetype = base_archetype;
		info->main_archetype = archetype;
		info->stream_index = stream_index;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetTag(Entity entity, unsigned char tag) {
		EntityInfo* info = GetInfoCrashCheck(this, entity, __FILE__, __FUNCTION__, __LINE__);
		info->tags |= 1 << tag;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetLayer(Entity entity, unsigned int layer) {
		EntityInfo* info = GetInfoCrashCheck(this, entity, __FILE__, __FUNCTION__, __LINE__);
		info->layer = layer;
	}

	// ------------------------------------------------------------------------------------------------------------

	// Target this memory manager to be able to hold half a million objects without going to the global allocator
	// Add a small number of half a million to accomodate smaller allocations, like the one for the resizable stream
	// Support another half a million for each global allocator callback
	MemoryManager DefaultEntityPoolManager(GlobalMemoryManager* global_memory)
	{
		size_t whole_chunk_memory = StableReferenceStream<EntityInfo>::MemoryOf(505'000);
		return MemoryManager(whole_chunk_memory, 4096, whole_chunk_memory, global_memory);
	}

	// ------------------------------------------------------------------------------------------------------------

}
