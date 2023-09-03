#include "ecspch.h"
#include "Archetype.h"
#include "../Utilities/Crash.h"

#define TRIM_COUNT 10

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------------------------

	Archetype::Archetype( 
		MemoryManager* small_memory_manager,
		MemoryManager* memory_manager,
		const ComponentInfo* unique_infos,
		ComponentSignature unique_components,
		ComponentSignature shared_components
	) : m_small_memory_manager(small_memory_manager), m_memory_manager(memory_manager), m_base_archetypes(GetAllocatorPolymorphic(m_small_memory_manager), 1),
		m_unique_infos(unique_infos), m_unique_components_to_deallocate_count(0)
	{
		size_t allocation_size = sizeof(Component) * (unique_components.count + shared_components.count);
		void* allocation = small_memory_manager->Allocate(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;

		m_unique_components.indices = (Component*)buffer;
		m_unique_components.count = unique_components.count;
		memcpy((void*)buffer, unique_components.indices, sizeof(Component) * unique_components.count);
		buffer += sizeof(Component) * unique_components.count;

		m_shared_components.indices = (Component*)buffer;
		m_shared_components.count = shared_components.count;
		memcpy((void*)buffer, shared_components.indices, sizeof(Component) * shared_components.count);
		buffer += sizeof(Component) * shared_components.count;

		// Look to see which components have buffers to be deallocated
		for (unsigned char index = 0; index < unique_components.count; index++) {
			if (unique_infos[unique_components[index]].component_buffers_count > 0) {
				m_unique_components_to_deallocate[m_unique_components_to_deallocate_count++] = index;
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int Archetype::CreateBaseArchetype(SharedComponentSignature components, unsigned int starting_size)
	{
		ECS_CRASH_RETURN_VALUE(components.count == m_shared_components.count, -1, "Trying to create a base archetype with the incorrect number "
			"of shared components. Expected {#}, got {#}.", m_shared_components.count, components.count);

		SharedInstance* shared_instances_allocation = (SharedInstance*)m_small_memory_manager->Allocate(sizeof(SharedInstance) * components.count, alignof(SharedInstance));
		for (size_t index = 0; index < components.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(components.indices[index]);
			ECS_CRASH_RETURN_VALUE(component_index != -1, -1, "Incorrect components when creating a base archetype. The component {#} could not be found.",
				components.indices[index]);
			shared_instances_allocation[component_index] = components.instances[index];
		}

		VectorComponentSignature vector_instances;
		vector_instances.ConvertInstances(shared_instances_allocation, components.count);
		unsigned int index = m_base_archetypes.Add({ 
			ArchetypeBase(
				m_memory_manager,
				starting_size,
				m_unique_infos, 
				m_unique_components
			),
			shared_instances_allocation,
			vector_instances
		});
		return index;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::CopyOther(const Archetype* other)
	{
		SharedComponentSignature shared_signature;
		shared_signature.count = other->m_shared_components.count;
		shared_signature.indices = other->m_shared_components.indices;
		// Create the base archetypes now
		for (size_t base_index = 0; base_index < other->GetBaseCount(); base_index++) {
			shared_signature.instances = (SharedInstance*)other->GetBaseInstances(base_index);
			const ArchetypeBase* copy_base = other->GetBase(base_index);
			CreateBaseArchetype(shared_signature, copy_base->m_capacity);

			// Copy the data into the base archetype now
			ArchetypeBase* current_base = GetBase(base_index);
			current_base->CopyOther(copy_base);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::CopyEntityBuffers(EntityInfo info, unsigned char deallocate_index, void* target_memory) const
	{
		CopyEntityBuffers(info.stream_index, info.base_archetype, deallocate_index, target_memory);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::CopyEntityBuffers(EntityInfo info, void** target_buffers) const
	{
		for (unsigned int deallocate_index = 0; deallocate_index < m_unique_components_to_deallocate_count; deallocate_index++) {
			CopyEntityBuffers(info, deallocate_index, target_buffers[deallocate_index]);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::CopyEntityBuffers(unsigned int stream_index, unsigned int base_index, unsigned char deallocate_index, void* target_memory) const
	{
		const ArchetypeBase* base = GetBase(base_index);
		const void* component = base->GetComponentByIndex(stream_index, m_unique_components_to_deallocate[deallocate_index]);
		const ComponentInfo* component_info = m_unique_infos + m_unique_components[m_unique_components_to_deallocate[deallocate_index]].value;
		MemoryArena* arena = component_info->allocator;

		for (unsigned int buffer_index = 0; buffer_index < component_info->component_buffers_count; buffer_index++) {
			const void* current_buffer = function::OffsetPointer(component, component_info->component_buffers[buffer_index].pointer_offset);
			ComponentBufferCopy(component_info->component_buffers[buffer_index], arena, current_buffer, target_memory);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::CopyEntityBuffers(unsigned int stream_index, unsigned int base_index, void** target_buffers) const
	{
		for (unsigned int deallocate_index = 0; deallocate_index < m_unique_components_to_deallocate_count; deallocate_index++) {
			CopyEntityBuffers(stream_index, base_index, deallocate_index, target_buffers[deallocate_index]);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::Deallocate()
	{
		for (size_t index = 0; index < m_base_archetypes.size; index++) {
			DeallocateBase(index);
		}
		m_small_memory_manager->Deallocate(m_unique_components.indices);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DeallocateBase(unsigned int archetype_index) {
		m_base_archetypes[archetype_index].archetype.Deallocate();
		m_small_memory_manager->Deallocate(m_base_archetypes[archetype_index].shared_instances);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void DeallocateEntityBuffersImpl(const ComponentInfo* current_info, MemoryArena* arena, const void* current_component, unsigned int buffer_count) {
		for (unsigned int buffer_index = 0; buffer_index < buffer_count; buffer_index++) {
			const void* current_buffer = function::OffsetPointer(current_component, current_info->component_buffers[buffer_index].pointer_offset);
			ComponentBufferDeallocate(current_info->component_buffers[buffer_index], arena, current_buffer);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DeallocateEntityBuffers() const
	{
		// Keep iterating over the same component because it will help with cache coherency
		for (unsigned char deallocate_index = 0; deallocate_index < m_unique_components_to_deallocate_count; deallocate_index++) {
			unsigned char signature_index = m_unique_components_to_deallocate[deallocate_index];
			const ComponentInfo* current_info = &m_unique_infos[m_unique_components[signature_index].value];
			MemoryArena* arena = current_info->allocator;
			
			unsigned int buffer_count = current_info->component_buffers_count;

			unsigned int base_count = GetBaseCount();
			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				const ArchetypeBase* base = GetBase(base_index);
				unsigned int entity_count = base->EntityCount();
				for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
					const void* current_component = base->GetComponentByIndex(entity_index, signature_index);
					DeallocateEntityBuffersImpl(current_info, arena, current_component, buffer_count);
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DeallocateEntityBuffers(unsigned int base_index) const
	{
		for (unsigned char deallocate_index = 0; deallocate_index < m_unique_components_to_deallocate_count; deallocate_index++) {
			unsigned char signature_index = m_unique_components_to_deallocate[deallocate_index];
			const ComponentInfo* current_info = &m_unique_infos[m_unique_components[signature_index].value];
			MemoryArena* arena = current_info->allocator;

			unsigned int buffer_count = current_info->component_buffers_count;

			unsigned int base_count = GetBaseCount();
			const ArchetypeBase* base = GetBase(base_index);
			unsigned int entity_count = base->EntityCount();
			for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
				const void* current_component = base->GetComponentByIndex(entity_index, signature_index);
				DeallocateEntityBuffersImpl(current_info, arena, current_component, buffer_count);
			}

		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DeallocateEntityBuffers(EntityInfo info) const {
		for (unsigned int deallocate_index = 0; deallocate_index < m_unique_components_to_deallocate_count; deallocate_index++) {
			DeallocateEntityBuffers(deallocate_index, info);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DeallocateEntityBuffers(unsigned char deallocate_index, EntityInfo info) const
	{
		unsigned char signature_index = m_unique_components_to_deallocate[deallocate_index];
		const ComponentInfo* current_info = &m_unique_infos[m_unique_components[signature_index].value];
		MemoryArena* arena = current_info->allocator;

		unsigned int buffer_count = current_info->component_buffers_count;
		const ArchetypeBase* base = GetBase(info.base_archetype);
		const void* current_component = base->GetComponentByIndex(info.stream_index, signature_index);
		DeallocateEntityBuffersImpl(current_info, arena, current_component, buffer_count);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DestroyBase(unsigned int archetype_index, EntityPool* pool)
	{
		// Firstly deallocate all the component buffers
		DeallocateEntityBuffers(archetype_index);
		// Then we can deallocate the actual base
		DeallocateBase(archetype_index);
		m_base_archetypes.RemoveSwapBack(archetype_index);

		// Update the infos if the base_index differs from the last archetype
		if (m_base_archetypes.size != archetype_index) {
			for (size_t entity_index = 0; entity_index < m_base_archetypes[archetype_index].archetype.m_size; entity_index++) {
				EntityInfo* info = pool->GetInfoPtr(m_base_archetypes[archetype_index].archetype.m_entities[entity_index]);
				info->base_archetype = archetype_index;
			}
		}

		// TODO: Determine if the trimming is helpful
		// Might not be worth the fragmentation cost
		m_base_archetypes.Trim(TRIM_COUNT);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned char Archetype::FindDeallocateComponentIndex(Component component) const
	{
		for (unsigned char index = 0; index < m_unique_components_to_deallocate_count; index++) {
			if (m_unique_components[m_unique_components_to_deallocate[index]] == component) {
				return index;
			}
		}
		return UCHAR_MAX;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::FindUniqueComponents(ComponentSignature signature) const
	{
		for (size_t index = 0; index < signature.count; index++) {
			unsigned char component_index = FindUniqueComponentIndex(signature.indices[index]);
			signature.indices[index].value = component_index == (unsigned char)-1 ? (unsigned short)-1 : component_index;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::FindSharedComponents(ComponentSignature signature) const
	{
		for (size_t index = 0; index < signature.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(signature.indices[index]);
			signature.indices[index].value = component_index == UCHAR_MAX ? -1 : component_index;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::FindSharedInstances(unsigned int archetype_index, SharedComponentSignature signature) const
	{
		ECS_CRASH_RETURN(archetype_index < m_base_archetypes.size, "Incorrect base index {#} when finding shared instances.", archetype_index);

		// Find the component indices and then do the linear search
		for (size_t index = 0; index < signature.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(signature.indices[index]);
			signature.instances[index].value = component_index != UCHAR_MAX ? m_base_archetypes[archetype_index].shared_instances[component_index].value : -1;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::FindBase(SharedComponentSignature shared_signature)
	{
		unsigned int index = FindBaseIndex(shared_signature);
		return index != -1 ? &m_base_archetypes[index].archetype : nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::FindBase(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances)
	{
		unsigned int index = FindBaseIndex(shared_signature, shared_instances);
		return index != -1 ? &m_base_archetypes[index].archetype : nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int Archetype::FindBaseIndex(SharedComponentSignature shared_signature) const
	{
		unsigned char* temporary_mapping = (unsigned char*)ECS_STACK_ALLOC(sizeof(unsigned char) * shared_signature.count);
		for (size_t index = 0; index < shared_signature.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(shared_signature.indices[index]);
			if (component_index == UCHAR_MAX) {
				return -1;
			}
			temporary_mapping[index] = component_index;
		}

		for (unsigned int index = 0; index < m_base_archetypes.size; index++) {
			size_t subindex = 0;
			for (; subindex < shared_signature.count; subindex++) {
				if (m_base_archetypes[index].shared_instances[temporary_mapping[subindex]] != shared_signature.instances[subindex]) {
					break;
				}
			}
			if (subindex == shared_signature.count) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int Archetype::FindBaseIndex(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances)
	{
		VectorComponentSignature vector_components(m_shared_components);
		VectorComponentSignature vector_instances;
		for (unsigned int index = 0; index < m_base_archetypes.size; index++) {
			vector_instances = m_base_archetypes[index].vector_instances;
			if (SharedComponentSignatureHasInstances(vector_components, vector_instances, shared_signature, shared_instances)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance* Archetype::GetBaseInstances(unsigned int index)
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve shared instances pointer from archetype.", index);
		return m_base_archetypes[index].shared_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

	const SharedInstance* Archetype::GetBaseInstances(unsigned int index) const
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve shared instances pointer from archetype.", index);
		return m_base_archetypes[index].shared_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance Archetype::GetBaseInstance(Component component, unsigned int base_index) const
	{
		SharedComponentSignature signature = GetSharedSignature(base_index);
		for (unsigned int index = 0; index < signature.count; index++) {
			if (component == signature.indices[index]) {
				return signature.instances[index];
			}
		}
		return { -1 };
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::GetBase(unsigned int index)
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve archetype base pointer from archetype.", index);
		return &m_base_archetypes[index].archetype;
	}

	// --------------------------------------------------------------------------------------------------------------------

	const ArchetypeBase* Archetype::GetBase(unsigned int index) const
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve archetype base pointer from archetype.", index);
		return &m_base_archetypes[index].archetype;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedComponentSignature Archetype::GetSharedSignature(unsigned int base_index) const
	{
		ECS_CRASH_RETURN_VALUE(base_index < m_base_archetypes.size, {}, "Incorrect base index {#} when trying to retrieve shared signature from archetype.", base_index);
		return { m_shared_components.indices, m_base_archetypes[base_index].shared_instances, m_shared_components.count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature Archetype::GetVectorInstances(unsigned int base_index) const
	{
		ECS_CRASH_RETURN_VALUE(base_index < m_base_archetypes.size, {}, "Incorrect base index {#} when trying to retrieve vector instances from archetype.", base_index);
		return m_base_archetypes[base_index].vector_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int Archetype::GetEntityCount() const
	{
		unsigned int entity_count = 0;
		unsigned int base_count = GetBaseCount();
		for (unsigned int index = 0; index < base_count; index++) {
			entity_count += GetBase(index)->EntityCount();
		}

		return entity_count;
	}

	// --------------------------------------------------------------------------------------------------------------------

}