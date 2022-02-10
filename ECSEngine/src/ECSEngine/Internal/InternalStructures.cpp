#include "ecspch.h"
#include "../Utilities/Function.h"
#include "InternalStructures.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::ConvertComponents(ComponentSignature signature)
	{
		// Increment the component indices so as to not have 0 as a valid component index
		alignas(32) uint16_t increments[sizeof(__m256i) / sizeof(uint16_t)] = { 0 };
		for (size_t index = 0; index < signature.count; index++) {
			increments[index] = 1;
		}
		Vec16us increment;
		increment.load_a(increments);
		value = LoadPartial16(signature.indices, signature.count);
		// value.load_partial(signature.count, signature.indices);
		value += increment;
	}

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::ConvertInstances(const SharedInstance* instances, unsigned char count)
	{
		ConvertComponents({ (Component*)instances, count });
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL VectorComponentSignature::HasComponents(VectorComponentSignature components) const
	{
		Vec16us splatted_component;
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb is_zero;
		Vec16sb match;

#define LOOP_ITERATION(iteration)   splatted_component = Broadcast16<iteration>(components.value); \
									/* Test to see if the current element is zero */ \
									is_zero = splatted_component == zero_vector; \
									/* If we get to an element which is 0, that means all components have been matched */ \
									if (horizontal_and(is_zero)) { \
										return true; \
									} \
									match = splatted_component == value; \
									if (!horizontal_or(match)) { \
										return false; \
									} 

		LOOP_UNROLL(16, LOOP_ITERATION);

		return true;

#undef LOOP_ITERATION
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL VectorComponentSignature::ExcludesComponents(VectorComponentSignature components) const
	{
		Vec16us splatted_component;
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb is_zero;
		Vec16sb match;

#define LOOP_ITERATION(iteration)   splatted_component = Broadcast16<iteration>(components.value); \
									/* Test to see if the current element is zero */ \
									is_zero = splatted_component == zero_vector; \
									/* If we get to an element which is 0, that means all components have been matched */ \
									if (horizontal_and(is_zero)) { \
										return true; \
									} \
									match = splatted_component == value; \
									if (horizontal_or(match)) { \
										return false; \
									} 

		LOOP_UNROLL(16, LOOP_ITERATION);

#undef LOOP_ITERATION

		return true;

	}

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::InitializeSharedComponent(SharedComponentSignature shared_signature)
	{
		ConvertComponents(ComponentSignature(shared_signature.indices, shared_signature.count));
	}

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::InitializeSharedInstances(SharedComponentSignature shared_signature)
	{
		ConvertInstances(shared_signature.instances, shared_signature.count);
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityPool::EntityPool(
		MemoryManager* memory_manager,
		unsigned int pool_capacity,
		unsigned int pool_power_of_two
	) : m_memory_manager(memory_manager), m_pool_capacity(pool_capacity), m_pool_power_of_two(pool_power_of_two), m_entity_infos(GetAllocatorPolymorphic(memory_manager), 1) {}
	
	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::CreatePool() {
		void* allocation = m_memory_manager->Allocate(m_entity_infos.buffer->stream.MemoryOf(m_pool_capacity));
		// Walk through the entity info stream and look for an empty slot and place the allocation there
		// Cannot remove swap back chunks because the entity index calculation takes into consideration the initial
		// chunk position. Using reallocation tables that would offset chunk indices does not seem like a good idea
		// since adds latency to the lookup
		auto info_stream = StableReferenceStream<EntityInfo>(allocation, m_pool_capacity);
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

	Entity GetEntityFromPoolOffset(unsigned int pool_index, unsigned int pool_power_of_two, unsigned int index) {
		return (pool_index << pool_power_of_two) + index;
	}

	uint2 GetPoolAndEntityIndex(const EntityPool* entity_pool, Entity entity) {
		return { entity.value >> entity_pool->m_pool_power_of_two, entity.value & (entity_pool->m_pool_power_of_two - 1) };
	}

	template<bool has_info>
	ECS_INLINE Entity EntityPoolAllocateImplementation(EntityPool* entity_pool, EntityInfo info = {}) {
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			if (entity_pool->m_entity_infos[index].stream.size < entity_pool->m_entity_infos[index].stream.capacity) {
				if constexpr (has_info) {
					return GetEntityFromPoolOffset(index, entity_pool->m_pool_power_of_two, entity_pool->m_entity_infos[index].stream.Add(info));
				}
				else {
					return GetEntityFromPoolOffset(index, entity_pool->m_pool_power_of_two, entity_pool->m_entity_infos[index].stream.ReserveOne());
				}
			}
		}

		// All pools are full - create a new one and allocate from it
		entity_pool->CreatePool();
		if constexpr (has_info) {
			return GetEntityFromPoolOffset(
				entity_pool->m_entity_infos.size - 1,
				entity_pool->m_pool_power_of_two, 
				entity_pool->m_entity_infos[entity_pool->m_entity_infos.size - 1].stream.Add(info)
			);
		}
		else {
			return GetEntityFromPoolOffset(
				entity_pool->m_entity_infos.size - 1,
				entity_pool->m_pool_power_of_two,
				entity_pool->m_entity_infos[entity_pool->m_entity_infos.size - 1].stream.ReserveOne()
			);
		}
	}

	Entity EntityPool::Allocate()
	{
		return EntityPoolAllocateImplementation<false>(this);
	}

	// ------------------------------------------------------------------------------------------------------------

	Entity EntityPool::AllocateEx(EntityInfo info)
	{
		return EntityPoolAllocateImplementation<true>(this, info);
	}

	// ------------------------------------------------------------------------------------------------------------

	struct EntityPoolAllocateAdditionalData {
		EntityInfo* infos = nullptr;
		ushort2 archetype_indices;
		Stream<EntitySequence> chunk_positions = { nullptr,0 };
	};

	enum EntityPoolAllocateType {
		ENTITY_POOL_ALLOCATE_NO_DATA,
		ENTITY_POOL_ALLOCATE_WITH_INFOS,
		ENTITY_POOL_ALLOCATE_WITH_CHUNKS
	};
	
	template<EntityPoolAllocateType additional_data_type>
	ECS_INLINE void EntityPoolAllocateImplementation(EntityPool* entity_pool, Stream<Entity> entities, EntityPoolAllocateAdditionalData additional_data = {}) {
		// The assert will be done by the stream too
		// ECS_ASSERT(entities.size < m_pool_capacity);

		auto loop_iteration = [&entities, entity_pool, additional_data](unsigned int index) {
			if constexpr (additional_data_type == ENTITY_POOL_ALLOCATE_WITH_INFOS) {
				entity_pool->m_entity_infos[index].stream.AddStream(Stream<EntityInfo>(additional_data.infos, entities.size), (unsigned int*)entities.buffer);
			}
			else if constexpr (additional_data_type == ENTITY_POOL_ALLOCATE_WITH_CHUNKS) {
				unsigned int total_entity_count = 0;
				for (size_t chunk_index = 0; chunk_index < additional_data.chunk_positions.size; chunk_index++) {
					for (size_t entity_index = 0; entity_index < additional_data.chunk_positions[chunk_index].entity_count; entity_index++) {
						EntityInfo info;
						info.base_archetype = additional_data.archetype_indices.y;
						info.main_archetype = additional_data.archetype_indices.x;
						info.chunk = additional_data.chunk_positions[chunk_index].chunk_index;
						info.stream_index = additional_data.chunk_positions[chunk_index].stream_offset + entity_index;
						// This will be a handle that can be used to index into this specific chunk of the entity pool
						entities[total_entity_count++] = GetEntityFromPoolOffset(index, entity_pool->m_pool_power_of_two, entity_pool->m_entity_infos[index].stream.Add(info));
					}
				}
			}
			else {
				entity_pool->m_entity_infos[index].stream.Reserve(Stream<unsigned int>(entities.buffer, entities.size));
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

	void EntityPool::AllocateEx(Stream<Entity> entities, EntityInfo* infos)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_INFOS>(this, entities, {infos});
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::AllocateEx(Stream<Entity> entities, ushort2 archetype_indices, Stream<EntitySequence> chunk_positions)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_CHUNKS>(this, entities, { nullptr, archetype_indices, chunk_positions });
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Deallocate(Entity entity)
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		m_entity_infos[entity_indices.x].stream.Remove(entity_indices.y);
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

	EntityInfo EntityPool::GetInfo(Entity entity) const {
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		return m_entity_infos[entity_indices.x].stream[entity_indices.y];
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo* EntityPool::GetInfoPtr(Entity entity)
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		return m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetEntityInfo(Entity entity, EntityInfo new_info) {
		unsigned int pool_index = entity.value >> m_pool_power_of_two;
		unsigned int entity_index = entity.value & (m_pool_power_of_two - 1);
		m_entity_infos[pool_index].stream[entity_index] = new_info;
	}

	// ------------------------------------------------------------------------------------------------------------

	ArchetypeQuery::ArchetypeQuery() {}

	ArchetypeQuery::ArchetypeQuery(VectorComponentSignature _unique, VectorComponentSignature _shared) : unique(_unique), shared(_shared) {}

	bool ECS_VECTORCALL ArchetypeQuery::Verifies(VectorComponentSignature unique_to_compare, VectorComponentSignature shared_to_compare) const
	{
		return VerifiesUnique(unique_to_compare) && VerifiesShared(shared_to_compare);
	}

	bool ECS_VECTORCALL ArchetypeQuery::VerifiesUnique(VectorComponentSignature unique_to_compare) const
	{
		return unique_to_compare.HasComponents(unique);
	}

	bool ECS_VECTORCALL ArchetypeQuery::VerifiesShared(VectorComponentSignature shared_to_compare) const
	{
		return shared_to_compare.HasComponents(shared);
	}

	// ------------------------------------------------------------------------------------------------------------

	ArchetypeQueryExclude::ArchetypeQueryExclude() {}

	ArchetypeQueryExclude::ArchetypeQueryExclude(
		VectorComponentSignature _unique, 
		VectorComponentSignature _shared, 
		VectorComponentSignature _exclude_unique, 
		VectorComponentSignature _exclude_shared
	) : unique(_unique), shared(_shared), unique_excluded(_exclude_unique), shared_excluded(_exclude_shared) {}

	bool ECS_VECTORCALL ArchetypeQueryExclude::Verifies(VectorComponentSignature unique_to_compare, VectorComponentSignature shared_to_compare) const
	{
		return VerifiesUnique(unique_to_compare) && VerifiesShared(shared_to_compare);
	}

	bool ECS_VECTORCALL ArchetypeQueryExclude::VerifiesUnique(VectorComponentSignature unique_to_compare) const
	{
		return unique_to_compare.HasComponents(unique) && unique_to_compare.ExcludesComponents(unique_excluded);
	}

	bool ECS_VECTORCALL ArchetypeQueryExclude::VerifiesShared(VectorComponentSignature shared_to_compare) const
	{
		return shared_to_compare.HasComponents(shared) && shared_to_compare.ExcludesComponents(shared_excluded);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL SharedComponentSignatureHasInstances(
		VectorComponentSignature archetype_components,
		VectorComponentSignature archetype_instances,
		VectorComponentSignature query_components, 
		VectorComponentSignature query_instances
	)
	{
		Vec16us splatted_component;
		Vec16us splatted_instance;
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb is_zero;
		Vec16sb match;
		Vec16sb instance_match;

#define LOOP_ITERATION(iteration)	splatted_component = Broadcast16<iteration>(query_components.value); \
									/* Test to see if the current element is zero */ \
									is_zero = splatted_component == zero_vector; \
									/* If we get to an element which is 0, that means all components have been matched */ \
									if (horizontal_and(is_zero)) { \
										return true; \
									} \
									match = splatted_component == archetype_components.value; \
									if (!horizontal_or(match)) { \
										return false; \
									} \
									/* Test now the instance - splat the instance and keep the position where only the component was matched */ \
									splatted_instance = Broadcast16<iteration>(query_instances.value); \
									instance_match = splatted_instance == archetype_instances.value; \
									match &= instance_match; \
									if (!horizontal_or(match)) { \
										return false; \
									}
									

		LOOP_UNROLL(16, LOOP_ITERATION);

		return true;

#undef LOOP_ITERATION
	}

	// ------------------------------------------------------------------------------------------------------------

}
