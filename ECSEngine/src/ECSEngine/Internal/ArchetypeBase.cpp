#include "ecspch.h"
#include "ArchetypeBase.h"
#include "../Utilities/Function.h"
#include "../Allocators/MemoryManager.h"
#include "../Utilities/Crash.h"

#define GROW_FACTOR (1.5f)

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase::ArchetypeBase() {}

	ArchetypeBase::ArchetypeBase(
		MemoryManager* memory_manager,
		unsigned int starting_size,
		const ComponentInfo* infos,
		ComponentSignature components
	) : m_memory_manager(memory_manager), m_infos(infos), m_components(components), m_size(0), m_capacity(0), m_entities(nullptr), m_buffers(nullptr)
	{
		if (starting_size > 0) {
			Reserve(starting_size);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int ArchetypeBase::AddEntity(Entity entity)
	{
		return AddEntities({ &entity, 1 });
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int ArchetypeBase::AddEntity(Entity entity, ComponentSignature components, const void** data)
	{
		unsigned int copy_position = AddEntities({ &entity, 1 });
		CopyByComponents({copy_position, 1}, data, components);
		return copy_position;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int ArchetypeBase::AddEntities(Stream<Entity> entities)
	{
		unsigned int copy_position = Reserve(entities.size);
		entities.CopyTo(m_entities + copy_position);
		m_size += entities.size;

		return copy_position;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyOther(const ArchetypeBase* other)
	{
		// Assert they have the same capacity
		ECS_CRASH_RETURN(
			other->m_capacity == m_capacity, 
			"Trying to copy a base archetype with different capacity. Expected {#}, got {#}.", 
			m_capacity,
			other->m_capacity
		);

		// Normally, we would have to assert that they have the same component order, but that should already be taken care of
		// E.g. don't call copy other if they are from different archetypes

		unsigned int other_size = other->m_size;
		m_size = other->m_size;
		// Copy the entities
		memcpy(m_entities, other->m_entities, sizeof(Entity) * other_size);
		// Copy the components now
		for (size_t index = 0; index < m_components.count; index++) {
			unsigned short component_byte_size = m_infos[m_components.indices[index].value].size;
			memcpy(m_buffers[index], other->m_buffers[index], component_byte_size * other_size);
			// If the component has buffers, we need to make a deep copy of them
			unsigned int buffer_count = m_infos[m_components.indices[index]].component_buffers_count;
			if (buffer_count > 0) {
				const ComponentBuffer* component_buffers = m_infos[m_components.indices[index]].component_buffers;
				for (unsigned int buffer_index = 0; buffer_index < buffer_count; buffer_index++) {
					MemoryArena* component_allocator = m_infos[m_components.indices[index]].allocator;
					void* current_buffer = m_buffers[index];

					auto entity_loop = [=](auto is_data_pointer) {
						for (unsigned int entity_index = 0; entity_index < m_size; entity_index++) {
							void* current_component = function::OffsetPointer(current_buffer, component_byte_size * entity_index);
							if constexpr (is_data_pointer) {
								ComponentBufferCopyDataPointer(component_buffers[buffer_index], component_allocator, current_component, current_component);
							}
							else {
								ComponentBufferCopyStream(component_buffers[buffer_index], component_allocator, current_component, current_component);
							}
						}
					};

					if (component_buffers[buffer_index].is_data_pointer) {
						entity_loop(std::true_type{});
					}
					else {
						entity_loop(std::false_type{});
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	void CopyEntitiesInternal(ArchetypeBase* archetype, ComponentSignature components, unsigned int copy_position, Functor&& functor) {
		for (size_t index = 0; index < components.count; index++) {
			unsigned char component_index = archetype->FindComponentIndex(components.indices[index]);
			ECS_CRASH_RETURN(component_index != UCHAR_MAX, "Incorrect component {#} when trying to copy entities. The component is missing from the base archetype.", components.indices[index].value);

			unsigned short component_size = archetype->m_infos[components.indices[index].value].size;

			void* component_data = function::OffsetPointer(
				archetype->m_buffers[component_index],
				copy_position * component_size
			);
			functor(index, component_data, component_size);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopySplatComponents(uint2 copy_position, const void** data, ComponentSignature components)
	{
		CopyEntitiesInternal(this, components, copy_position.x, [=](size_t component_index, void* component_data, unsigned short component_size) {
			for (unsigned int index = 0; index < copy_position.y; index++) {
				memcpy(component_data, data[component_index], component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyFromAnotherArchetype(
		uint2 copy_position, 
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
			ECS_CRASH_RETURN(
				component_index != -1, 
				"Could not find component {#} in destination archetype when copying entities from another base archetype.", 
				components_to_copy.indices[index].value
			);

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
			ECS_CRASH_RETURN(
				archetype_to_copy_component_index != -1,
				"Could not find component {#} in source archetype when copying entities from another base archetype.",
				components_to_copy.indices[index]
			);

			void* component_data = function::OffsetPointer(
				m_buffers[component_index],
				component_size * copy_position.x
			);

			for (size_t entity_index = 0; entity_index < copy_position.y; entity_index++) {
				EntityInfo entity_info = entity_pool->GetInfo(entities[entity_index]);
				const void* data_to_copy = function::OffsetPointer(
					source_archetype->m_buffers[archetype_to_copy_component_index], 
					component_size * entity_info.stream_index
				);
				memcpy(component_data, data_to_copy, component_size);

				component_data = function::OffsetPointer(component_data, component_size);
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByEntity(uint2 copy_position, const void** data, ComponentSignature components)
	{
		CopyEntitiesInternal(this, components, copy_position.x, [=](size_t component_index, void* component_data, unsigned short component_size) {
			// Try to use the write combine effect of the cache line when writing these components even tho 
			// the read of the data pointer is going to be cold (if the prefetcher is sufficiently smart it could
			// detect this pattern and prefetch it for us)
			for (size_t entity_index = 0; entity_index < copy_position.y; entity_index++) {
				memcpy(component_data, data[entity_index * components.count + component_index], component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByEntityContiguous(uint2 copy_position, const void** data, ComponentSignature components)
	{
		unsigned short* component_sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * components.count);
		for (size_t index = 0; index < components.count; index++) {
			component_sizes[index] = m_infos[components.indices[index].value].size;
		}

		CopyEntitiesInternal(this, components, copy_position.x, [=](size_t component_index, void* component_data, unsigned short component_size) {
			unsigned short current_component_offset = 0;
			for (size_t index = 0; index < component_index; index++) {
				current_component_offset += component_sizes[index];
			}

			for (size_t entity_index = 0; entity_index, copy_position.y; entity_index++) {
				memcpy(component_data, function::OffsetPointer(data[entity_index], current_component_offset), component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByComponents(uint2 copy_position, const void** data, ComponentSignature components)
	{
		CopyEntitiesInternal(this, components, copy_position.x, [=](size_t component_index, void* component_data, unsigned short component_size) {
			// Try to use the write combine effect of the cache line when writing these components even tho 
			// the read of the data pointer is going to be cold (if the prefetcher is sufficiently smart it could
			// detect this pattern and prefetch it for us)
			unsigned int component_offset = component_index * copy_position.y;
			for (size_t entity_index = 0; entity_index < copy_position.y; entity_index++) {
				memcpy(component_data, data[entity_index + component_offset], component_size);
				component_data = function::OffsetPointer(component_data, component_size);
			}
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::CopyByComponentsContiguous(uint2 copy_position, const void** data, ComponentSignature signature)
	{
		CopyEntitiesInternal(this, signature, copy_position.x, [=](size_t index, void* component_data, unsigned short component_size) {
			memcpy(component_data, data[index], component_size * copy_position.y);
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Only a single allocation is made
	void ArchetypeBase::Deallocate() {
		if (m_entities != nullptr && m_buffers != nullptr) {
			m_memory_manager->Deallocate(m_buffers);
		}
		m_size = 0;
		m_capacity = 0;
		m_entities = nullptr;
		m_buffers = nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int ArchetypeBase::EntityCount() const
	{
		return m_size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned char ArchetypeBase::FindComponentIndex(Component component) const
	{
		for (size_t index = 0; index < m_components.count; index++) {
			if (m_components.indices[index] == component) {
				return index;
			}
		}
		return UCHAR_MAX;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::FindComponents(ComponentSignature components) const
	{
		for (size_t index = 0; index < components.count; index++) {
			unsigned char component_index = FindComponentIndex(components.indices[index]);
			components.indices[index].value = component_index == (unsigned char)-1 ? (unsigned short)-1 : component_index;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::GetBuffers(void** buffers, ComponentSignature signature) {
		for (size_t component = 0; component <= signature.count; component++) {
			unsigned char component_index = FindComponentIndex(signature.indices[component]);
			buffers[component] = m_buffers[component_index];
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* ArchetypeBase::GetComponent(EntityInfo info, Component component)
	{
		unsigned char component_index = FindComponentIndex(component);
		ECS_CRASH_RETURN_VALUE(component_index != -1, nullptr, "The entity {#} does not have component {#} when trying to retrieve it.", m_entities[info.stream_index].value);
		return GetComponentByIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* ArchetypeBase::GetComponent(EntityInfo info, Component component) const
	{
		unsigned char component_index = FindComponentIndex(component);
		ECS_CRASH_RETURN_VALUE(component_index != -1, nullptr, "The entity {#} does not have component {#} when trying to retrieve it.", m_entities[info.stream_index].value);
		return GetComponentByIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* ArchetypeBase::GetComponentByIndex(EntityInfo info, unsigned char component_index)
	{
		return GetComponentByIndex(info.stream_index, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* ArchetypeBase::GetComponentByIndex(unsigned int stream_index, unsigned char component_index)
	{
		return function::OffsetPointer(m_buffers[component_index], stream_index * m_infos[m_components.indices[component_index].value].size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* ArchetypeBase::GetComponentByIndex(EntityInfo info, unsigned char component_index) const
	{
		return GetComponentByIndex(info.stream_index, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* ArchetypeBase::GetComponentByIndex(unsigned int stream_index, unsigned char component_index) const
	{
		return function::OffsetPointer(m_buffers[component_index], stream_index * m_infos[m_components.indices[component_index].value].size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::GetEntitiesCopy(Entity* entities) const
	{
		memcpy(entities, m_entities, sizeof(Entity) * m_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::Resize(unsigned int count) {
		// TODO: Small copies could be handled by first copying into a temporary stack buffer,
		// deallocating the buffers and then allocate the new block. But the problem is that because
		// of different alignment it might copy invalid blocks. Aligning the stack buffer to the same
		// current alignment of the buffer is a solution but it might not be worthwhile doing that if
		// so much work is required

		size_t per_entity_size = 0;

		size_t new_total_byte_size = sizeof(Entity) * count + sizeof(void*) * m_components.count;
		for (size_t component_index = 0; component_index < m_components.count; component_index++) {
			per_entity_size += m_infos[m_components.indices[component_index].value].size;
		}
		// Add a cache line for component alignment
		new_total_byte_size += per_entity_size * count + m_components.count * ECS_CACHE_LINE_SIZE;

		void** old_buffers = m_buffers;

		void* allocation = m_memory_manager->Allocate(new_total_byte_size);
		uintptr_t ptr = (uintptr_t)allocation;
		m_buffers = (void**)ptr;
		ptr += sizeof(void*) * m_components.count;

		// Copy the entities first
		m_entities = (Entity*)ptr;

		ptr += sizeof(Entity) * count;
		ptr = function::AlignPointer(ptr, ECS_CACHE_LINE_SIZE);

		if (m_size > 0) {
			memcpy(m_entities, function::OffsetPointer(old_buffers, sizeof(void*) * m_components.count), sizeof(Entity) * m_size);
		}

		// Now copy the components
		for (size_t component_index = 0; component_index < m_components.count; component_index++) {
			unsigned short component_size = m_infos[m_components.indices[component_index].value].size;
			m_buffers[component_index] = (void*)ptr;
			if (m_size > 0) {
				memcpy(m_buffers[component_index], old_buffers[component_index], component_size * m_size);
			}

			ptr = function::AlignPointer(ptr + component_size * count, ECS_CACHE_LINE_SIZE);
		}

		// Now set the new capacity
		if (m_capacity > 0) {
			m_memory_manager->Deallocate(old_buffers);
		}
		m_capacity = count;
	}

	unsigned int ArchetypeBase::Reserve(unsigned int count)
	{
		if (m_size + count > m_capacity) {
			unsigned int default_reserve = (unsigned int)((float)m_capacity * GROW_FACTOR + 3);
			// This can happen for small sizes
			Resize(default_reserve < count ? count : default_reserve);
		}
		return m_size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::ShrinkToFit(unsigned int supplementary_elements)
	{
		if (m_size + supplementary_elements < m_capacity) {
			// Shrink the vector
			Resize(m_size + supplementary_elements);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::RemoveEntity(Entity entity, EntityPool* pool)
	{
		EntityInfo info = pool->GetInfo(entity);
		RemoveEntity(info.stream_index, pool);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::RemoveEntity(unsigned int stream_index, EntityPool* pool)
	{
		m_size--;
		// If it is the last entity, skip the update of the components and that of the entity info
		if (stream_index == m_size) {
			return;
		}

		// Every component content must be manually moved
		for (size_t index = 0; index < m_components.count; index++) {
			const void* last_element_data = GetComponentByIndex(m_size, index);
			UpdateComponentByIndex(stream_index, index, last_element_data);
		}
		// Also the entity must be updated
		m_entities[stream_index] = m_entities[m_size];
		EntityInfo* last_entity_info = pool->GetInfoPtr(m_entities[stream_index]);
		last_entity_info->stream_index = stream_index;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::SetEntities(Stream<Entity> entities, unsigned int copy_position)
	{
		memcpy(m_entities + copy_position, entities.buffer, sizeof(Entity) * entities.size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::UpdateComponent(unsigned int stream_index, Component component, const void* data)
	{
		unsigned char component_index = FindComponentIndex(component);
		ECS_CRASH_RETURN(component_index != -1, "The entity {#} does not have component {#} when trying to update it.", m_entities[stream_index].value, component.value);
		UpdateComponentByIndex(stream_index, component_index, data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void ArchetypeBase::UpdateComponentByIndex(unsigned int stream_index, unsigned char component_index, const void* data)
	{
		void* component_data = GetComponentByIndex(stream_index, component_index);
		memcpy(component_data, data, m_infos[m_components.indices[component_index].value].size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

}