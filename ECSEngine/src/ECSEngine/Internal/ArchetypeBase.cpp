#include "ecspch.h"
#include "ArchetypeBase.h"
#include "../Utilities/Function.h"
#include "../Allocators/MemoryManager.h"

#define TRIM_COUNT 10

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase::ArchetypeBase(
		MemoryManager* small_memory_manager,
		MemoryManager* memory_manager,
		unsigned int chunk_size,
		const ComponentInfo* infos,
		ComponentSignature components
	) : m_chunk_size(chunk_size), m_memory_manager(memory_manager), m_entity_count(0), m_chunks(GetAllocatorPolymorphic(small_memory_manager), 0),
		m_infos(infos), m_components(components)
	{
		// Creating an initial chunk
		CreateChunk();
	}

	// --------------------------------------------------------------------------------------------------------------------

	uint2 ArchetypeBase::AddEntity(Entity entity)
	{
		EntitySequence position;
		CapacityStream<EntitySequence> chunk_position(&position, 0, 1);
		AddEntities({ &entity, 1 }, chunk_position);
		return { position.chunk_index, position.stream_offset };
	}

	// --------------------------------------------------------------------------------------------------------------------

	uint2 ArchetypeBase::AddEntity(Entity entity, ComponentSignature components, const void** data)
	{
		EntitySequence position;
		CapacityStream<EntitySequence> chunk_position(&position, 0, 1);
		AddEntities({ &entity, 1 }, chunk_position);
		CopyByComponents(chunk_position, data, components);
		return { position.chunk_index, position.stream_offset };
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::AddEntities(Stream<Entity> entities, CapacityStream<EntitySequence>& chunk_positions)
	{
		Reserve(entities.size, chunk_positions);
		unsigned int written_count = 0;
		for (unsigned int index = 0; index < chunk_positions.size; index++) {
			memcpy(
				m_chunks[chunk_positions[index].chunk_index].entities + chunk_positions[index].stream_offset,
				entities.buffer + written_count, 
				chunk_positions[index].entity_count * sizeof(Entity)
			);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	void CopyEntitiesInternal(ArchetypeBase* archetype, ComponentSignature components, Stream<EntitySequence> chunk_positions, Functor&& functor) {
		for (size_t index = 0; index < components.count; index++) {
			unsigned char component_index = archetype->FindComponentIndex(components.indices[index]);
			ECS_ASSERT(component_index != -1);

			unsigned short component_size = archetype->m_infos[components.indices[index].value].size;
			unsigned int entity_pivot = 0;
			for (size_t chunk_index = 0; chunk_index < chunk_positions.size; chunk_index++) {
				void* component_data = function::OffsetPointer(
					archetype->m_chunks[chunk_positions[chunk_index].chunk_index].buffers[component_index], 
					chunk_positions[chunk_index].stream_offset * component_size
				);
				functor(index, chunk_positions[chunk_index].entity_count, entity_pivot, component_data, component_size);
				entity_pivot += chunk_positions[chunk_index].entity_count;
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopySplatComponents(Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components)
	{
		CopyEntitiesInternal(this, components, chunk_positions, [=](size_t component_index, unsigned int entity_count, unsigned int entity_pivot, void* component_data, unsigned short component_size) {
			for (unsigned int index = 0; index < entity_count; index++) {
				memcpy(component_data, data[component_index], component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyFromAnotherArchetype(
		Stream<EntitySequence> chunk_positions, 
		const ArchetypeBase* source_archetype,
		const Entity* entities, 
		const EntityPool* entity_pool,
		ComponentSignature components_to_copy
	)
	{
		ComponentSignature source_components = source_archetype->m_components;

		for (size_t index = 0; index < components_to_copy.count; index++) {
			// Determine what the component index is in both archetypes
			unsigned char component_index = FindComponentIndex(components_to_copy.indices[index]);
			// If the components was not found fail
			ECS_ASSERT(component_index != -1);

			unsigned short component_size = m_infos[m_components.indices[component_index].value].size;

			unsigned char archetype_to_copy_component_index = -1;
			for (size_t subindex = 0; subindex < source_components.count; subindex++) {
				if (source_components.indices[subindex] == components_to_copy.indices[index]) {
					archetype_to_copy_component_index = subindex;
					// Exit the loop by setting subindex to archetype_components.count
					subindex = source_components.count;
				}
			}
			// If no match was found, fail
			ECS_ASSERT(archetype_to_copy_component_index != -1);

			size_t total_entity_index = 0;
			for (size_t chunk_index = 0; chunk_index < chunk_positions.size; chunk_index++) {
				void* component_data = function::OffsetPointer(
					m_chunks[chunk_positions[chunk_index].chunk_index].buffers[component_index],
					component_size * chunk_positions[chunk_index].stream_offset
				);

				for (size_t entity_index = 0; entity_index < chunk_positions[chunk_index].entity_count; entity_index++) {
					EntityInfo entity_info = entity_pool->GetInfo(entities[total_entity_index]);
					const void* data_to_copy = function::OffsetPointer(
						source_archetype->m_chunks[entity_info.chunk].buffers[archetype_to_copy_component_index], 
						component_size * entity_info.stream_index
					);
					memcpy(component_data, data_to_copy, component_size);

					component_data = function::OffsetPointer(component_data, component_size);
					total_entity_index++;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByEntity(Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components)
	{
		CopyEntitiesInternal(this, components, chunk_positions, [=](size_t component_index, unsigned int entity_count, unsigned int entity_pivot, void* component_data, unsigned short component_size) {
			// Try to use the write combine effect of the cache line when writing these components even tho 
			// the read of the data pointer is going to be cold (if the prefetcher is sufficiently smart it could
			// detect this pattern and prefetch it for us)
			for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
				memcpy(component_data, data[(entity_pivot + entity_index) * components.count + component_index], component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByEntityContiguous(Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components)
	{
		unsigned short* component_sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * components.count);
		for (size_t index = 0; index < components.count; index++) {
			component_sizes[index] = m_infos[components.indices[index].value].size;
		}

		CopyEntitiesInternal(this, components, chunk_positions, [=](size_t component_index, unsigned int entity_count, unsigned int entity_pivot, void* component_data, unsigned short component_size) {
			unsigned short current_component_offset = 0;
			for (size_t index = 0; index < component_index; index++) {
				current_component_offset += component_sizes[index];
			}

			for (size_t entity_index = 0; entity_index, entity_count; entity_index++) {
				memcpy(component_data, function::OffsetPointer(data[entity_pivot + entity_index], current_component_offset), component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByComponents(Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components)
	{
		// Determine the total entity count from the chunk positions
		unsigned int total_entity_count = 0;
		for (size_t index = 0; index < chunk_positions.size; index++) {
			total_entity_count += chunk_positions[index].entity_count;
		}

		CopyEntitiesInternal(this, components, chunk_positions, [=](size_t component_index, unsigned int entity_count, unsigned int entity_pivot, void* component_data, unsigned short component_size) {
			// Try to use the write combine effect of the cache line when writing these components even tho 
			// the read of the data pointer is going to be cold (if the prefetcher is sufficiently smart it could
			// detect this pattern and prefetch it for us)
			unsigned int component_offset = component_index * total_entity_count;
			for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
				memcpy(component_data, data[entity_pivot + entity_index + component_offset], component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByComponentsContiguous(Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature signature)
	{
		CopyEntitiesInternal(this, signature, chunk_positions, [=](size_t index, unsigned int entity_count, unsigned int entity_pivot, void* component_data, unsigned short component_size) {
			memcpy(component_data, data[index], component_size * entity_count);
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CreateChunk() {
		// adding 8 bytes for table buffer alignment
		void* allocation = m_memory_manager->Allocate(ChunkMemorySize());
		uintptr_t buffer = (uintptr_t)allocation;

		Entity* entities = (Entity*)buffer;
		buffer += sizeof(Entity) * m_chunk_size;

		void** chunk_buffers = (void**)buffer;
		buffer += sizeof(void*) * m_components.count;
		for (size_t index = 0; index < m_components.count; index++) {
			buffer = function::align_pointer(buffer, ECS_CACHE_LINE_SIZE);
			chunk_buffers[index] = (void*)buffer;
			buffer += m_infos[m_components.indices[index].value].size * m_chunk_size;
		}

		unsigned int chunk_index = m_chunks.Add({ entities, chunk_buffers, 0 });
		ECS_ASSERT(chunk_index < ECS_ARCHETYPE_MAX_CHUNKS);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CreateChunks(unsigned int count) {
		for (size_t index = 0; index < count; index++) {
			CreateChunk();
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::ClearChunk(unsigned int chunk_index) {
		ECS_ASSERT(chunk_index < m_chunks.size);
		m_entity_count -= m_chunks[chunk_index].size;
		m_chunks[chunk_index].size = 0;
	}

	// --------------------------------------------------------------------------------------------------------------------

	size_t ArchetypeBase::ChunkMemorySize() const {
		size_t per_entity_size = 0;
		for (size_t index = 0; index < m_components.count; index++) {
			per_entity_size = m_infos[m_components.indices[index].value].size;
		}
		// A void* must be allocated for each component, also add + ECS_CACHE_LINE_SIZE for alignment purposes
		return per_entity_size * m_chunk_size + (sizeof(void*) + ECS_CACHE_LINE_SIZE) * m_components.count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::Deallocate() {
		for (size_t index = 0; index < m_chunks.size; index++) {
			DeallocateChunk(index);
		}
		m_chunks.FreeBuffer();
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::DeallocateChunk(unsigned int chunk_index)
	{
		ECS_ASSERT(chunk_index < m_chunks.size);
		m_memory_manager->Deallocate(m_chunks[chunk_index].entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::DestroyChunk(unsigned int chunk_index, EntityPool* pool) {
		DeallocateChunk(chunk_index);
		m_chunks.RemoveSwapBack(chunk_index);

		// Update the pool infos for the entities that got swapped
		// Do this only if the index is different from the last element
		if (m_chunks.size != chunk_index) {
			for (size_t index = 0; index < m_chunks[chunk_index].size; index++) {
				EntityInfo* info_ptr = pool->GetInfoPtr(m_chunks[chunk_index].entities[index]);
				info_ptr->chunk = chunk_index;
			}
		}

		// TODO: Determine if trimming is helpful
		// Might not be worth the fragmentation cost
		m_chunks.TrimThreshold(TRIM_COUNT);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned char ArchetypeBase::FindComponentIndex(Component component) const
	{
		for (size_t index = 0; index < m_components.count; index++) {
			if (m_components.indices[index] == component) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::FindComponents(ComponentSignature components) const
	{
		for (size_t index = 0; index < components.count; index++) {
			components.indices[index].value = FindComponentIndex(components.indices[index]);
			ECS_ASSERT(components.indices[index].value != -1);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::GetBuffers(void*** buffers) const {
		for (size_t chunk_index = 0; chunk_index < m_chunks.size; chunk_index++) {
			buffers[chunk_index] = m_chunks[chunk_index].buffers;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::GetBuffers(void*** buffers, size_t* chunk_sizes, ComponentSignature signature) const {
		size_t buffer_index = 0;

		for (size_t chunk_index = 0; chunk_index < m_chunks.size; chunk_index++) {
			for (size_t component = 0; component <= signature.count; component++) {
				buffers[chunk_index][component] = m_chunks[chunk_index].buffers[signature.indices[component].value];
			}
			chunk_sizes[chunk_index] = m_chunks[chunk_index].size;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int ArchetypeBase::GetChunkCount() const {
		return m_chunks.size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* ArchetypeBase::GetComponent(EntityInfo info, Component component) const
	{
		unsigned char component_index = FindComponentIndex(component);
		ECS_ASSERT(component_index != -1);
		return GetComponentByIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* ArchetypeBase::GetComponentByIndex(EntityInfo info, unsigned char component_index) const
	{
		return function::OffsetPointer(m_chunks[info.chunk].buffers[component_index], info.stream_index * m_infos[m_components.indices[component_index].value].size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int ArchetypeBase::GetEntityCount() const {
		return m_entity_count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::GetEntities(Stream<Entity>* entities) const
	{
		for (size_t index = 0; index < m_chunks.size; index++) {
			entities[index] = { m_chunks[index].entities, m_chunks[index].size };
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::GetEntities(CapacityStream<Entity>& entities) const
	{
		for (size_t chunk_index = 0; chunk_index < m_chunks.size; chunk_index++) {
			for (size_t entity_index = 0; entity_index < m_chunks[chunk_index].size; entity_index++) {
				entities.AddSafe(m_chunks[chunk_index].entities[entity_index]);
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::Reserve(unsigned int count)
	{
		m_entity_count += count;
		for (size_t index = 0; index < m_chunks.size; index++) {
			unsigned int available_positions = m_chunk_size - m_chunks[index].size;
			if (available_positions >= count) {
				m_chunks[index].size += count;
				return;
			}
			else {
				count -= available_positions;
				m_chunks[index].size = m_chunk_size;
			}
		}

		// There is not enough space in the current chunks, allocate new ones
		// Determine how many chunks are necessary
		unsigned int before_chunk_size = m_chunks.size;

		unsigned int chunk_remainder = count % m_chunk_size;
		unsigned int chunks_necessary = count / m_chunk_size + (chunk_remainder != 0);
		CreateChunks(chunks_necessary);

		for (size_t index = before_chunk_size; index < m_chunks.size - 1; index++) {
			m_chunks[index].size = m_chunk_size;
		}
		m_chunks[m_chunks.size - 1].size = chunk_remainder;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::Reserve(unsigned int count, CapacityStream<EntitySequence>& chunk_positions)
	{
		m_entity_count += count;
		for (size_t index = 0; index < m_chunks.size; index++) {
			unsigned int available_positions = m_chunk_size - m_chunks[index].size;
			if (available_positions >= count) {
				chunk_positions.AddSafe({ (unsigned short)index, m_chunks[index].size, count });
				m_chunks[index].size += count;
				return;
			}
			else {
				chunk_positions.AddSafe({ (unsigned short)index, m_chunks[index].size, available_positions });
				count -= available_positions;
				m_chunks[index].size = m_chunk_size;
			}
		}

		// There is not enough space in the current chunks, allocate new ones
		// Determine how many chunks are necessary
		unsigned int before_chunk_size = m_chunks.size;

		unsigned int chunk_remainder = count % m_chunk_size;
		unsigned int chunks_necessary = count / m_chunk_size + (chunk_remainder != 0);
		CreateChunks(chunks_necessary);

		for (size_t index = before_chunk_size; index < m_chunks.size - 1; index++) {
			chunk_positions.AddSafe({ (unsigned short)index, 0, m_chunk_size });
			m_chunks[index].size = m_chunk_size;
		}
		m_chunks[m_chunks.size - 1].size = chunk_remainder;
		chunk_positions.AddSafe({ (unsigned short)(m_chunks.size - 1), 0, chunk_remainder });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::RemoveEntity(Entity entity, EntityPool* pool)
	{
		EntityInfo info = pool->GetInfo(entity);
		
		m_chunks[info.chunk].size--;
		if (m_chunks[info.chunk].size == 0) {
			// There are no other entities left in this chunk - deallocate it and swap back into the slot
			// Also update the entity infos for those entities
			DestroyChunk(info.chunk, pool);
			return;
		}
		// If it is the last entity, skip the update of the components and that of the entity info
		else if (info.stream_index == m_chunks[info.chunk].size) {
			return;
		}

		// Every component content must be manually moved
		EntityInfo last_info = info;
		last_info.stream_index = m_chunks[info.chunk].size;
		for (size_t index = 0; index < m_components.count; index++) {
			const void* last_element_data = GetComponentByIndex(last_info, index);
			UpdateComponentByIndex(info, index, last_element_data);
		}
		// Also the entity must be updated
		m_chunks[info.chunk].entities[info.stream_index] = m_chunks[info.chunk].entities[m_chunks[info.chunk].size];
		EntityInfo* last_entity_info = pool->GetInfoPtr(m_chunks[info.chunk].entities[info.stream_index]);
		last_entity_info->stream_index = info.stream_index;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::RemoveEntities(Stream<Entity> entities, EntityPool* pool)
	{
		for (size_t index = 0; index < entities.size; index++) {
			RemoveEntity(entities[index], pool);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::SetEntities(const Entity* entities, Stream<EntitySequence> chunk_positions)
	{
		unsigned int total_entity_count = 0;
		for (size_t index = 0; index < chunk_positions.size; index++) {
			for (size_t subindex = 0; subindex < chunk_positions[index].entity_count; subindex++) {
				m_chunks[chunk_positions[index].chunk_index].entities[chunk_positions[index].stream_offset + subindex] = entities[total_entity_count];
				total_entity_count++;
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::UpdateComponent(EntityInfo info, Component component, const void* data)
	{
		unsigned char component_index = FindComponentIndex(component);
		ECS_ASSERT(component_index != -1);
		UpdateComponentByIndex(info, component_index, data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::UpdateComponentByIndex(EntityInfo info, unsigned char component_index, const void* data)
	{
		void* component_data = GetComponentByIndex(info, component_index);
		memcpy(component_data, data, m_infos[m_components.indices[component_index].value].size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

}