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
	) : m_small_memory_manager(small_memory_manager), m_memory_manager(memory_manager), m_base_archetypes(GetAllocatorPolymorphic(m_small_memory_manager), 1), m_unique_infos(unique_infos)
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
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned short Archetype::CreateBaseArchetype(SharedComponentSignature components, unsigned int starting_size)
	{
		ECS_CRASH_RETURN_VALUE(components.count == m_shared_components.count, -1, "Trying to create a base archetype with the incorrect number "
			"of shared components. Expected {#}, got {#}.", m_shared_components.count, components.count);

		SharedInstance* shared_instances_allocation = (SharedInstance*)m_small_memory_manager->Allocate(sizeof(SharedInstance) * components.count, alignof(SharedInstance));
		for (size_t index = 0; index < components.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(components.indices[index]);
			ECS_CRASH_RETURN_VALUE(component_index != -1, -1, "Incorrect components when creating a base archetype. The component {#} could not be found.",
				components.indices[index].value);
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
		for (size_t base_index = 0; base_index < other->GetArchetypeBaseCount(); base_index++) {
			shared_signature.instances = (SharedInstance*)other->GetArchetypeBaseInstances(base_index);
			const ArchetypeBase* copy_base = other->GetArchetypeBase(base_index);
			CreateBaseArchetype(shared_signature, copy_base->GetCapacity());

			// Copy the data into the base archetype now
			ArchetypeBase* current_base = GetArchetypeBase(base_index);
			current_base->CopyOther(copy_base);
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

	void Archetype::DeallocateBase(unsigned short archetype_index) {
		m_base_archetypes[archetype_index].archetype.Deallocate();
		m_small_memory_manager->Deallocate(m_base_archetypes[archetype_index].shared_instances);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DestroyBase(unsigned short archetype_index, EntityPool* pool)
	{
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

	unsigned char Archetype::FindUniqueComponentIndex(Component component) const
	{
		for (size_t index = 0; index < m_unique_components.count; index++) {
			if (component == m_unique_components.indices[index]) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned char Archetype::FindSharedComponentIndex(Component component) const
	{
		for (size_t index = 0; index < m_shared_components.count; index++) {
			if (component == m_shared_components.indices[index]) {
				return index;
			}
		}
		return -1;
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
			signature.indices[index].value = component_index == (unsigned char)-1 ? (unsigned short)-1 : component_index;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::FindSharedInstances(unsigned short archetype_index, SharedComponentSignature signature) const
	{
		ECS_CRASH_RETURN(archetype_index < m_base_archetypes.size, "Incorrect base index {#} when finding shared instances.", archetype_index);

		// Find the component indices and then do the linear search
		for (size_t index = 0; index < signature.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(signature.indices[index]);
			signature.instances[index].value = component_index != -1 ? m_base_archetypes[archetype_index].shared_instances[component_index].value : -1;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::FindArchetypeBase(SharedComponentSignature shared_signature)
	{
		unsigned short index = FindArchetypeBaseIndex(shared_signature);
		return index != -1 ? &m_base_archetypes[index].archetype : nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::FindArchetypeBase(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances)
	{
		unsigned short index = FindArchetypeBaseIndex(shared_signature, shared_instances);
		return index != -1 ? &m_base_archetypes[index].archetype : nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned short Archetype::FindArchetypeBaseIndex(SharedComponentSignature shared_signature) const
	{
		unsigned char* temporary_mapping = (unsigned char*)ECS_STACK_ALLOC(sizeof(unsigned char) * shared_signature.count);
		for (size_t index = 0; index < shared_signature.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(shared_signature.indices[index]);
			if (component_index == -1) {
				return -1;
			}
			temporary_mapping[index] = component_index;
		}

		for (size_t index = 0; index < m_base_archetypes.size; index++) {
			size_t subindex = 0;
			for (; subindex < shared_signature.count; subindex++) {
				if (m_base_archetypes[index].shared_instances[temporary_mapping[subindex]] != shared_signature.instances[index]) {
					// Exit the loop, -1 doesn't work because the increment will bring it to 0
					subindex = -2;
				}
			}
			if (subindex != -1) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned short Archetype::FindArchetypeBaseIndex(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances)
	{
		VectorComponentSignature vector_components(m_shared_components);
		VectorComponentSignature vector_instances;
		for (size_t index = 0; index < m_base_archetypes.size; index++) {
			vector_instances = m_base_archetypes[index].vector_instances;
			if (SharedComponentSignatureHasInstances(vector_components, vector_instances, shared_signature, shared_instances)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance* Archetype::GetArchetypeBaseInstances(unsigned short index)
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve shared instances pointer from archetype.", index);
		return m_base_archetypes[index].shared_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

	const SharedInstance* Archetype::GetArchetypeBaseInstances(unsigned short index) const
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve shared instances pointer from archetype.", index);
		return m_base_archetypes[index].shared_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::GetArchetypeBase(unsigned short index)
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve archetype base pointer from archetype.", index);
		return &m_base_archetypes[index].archetype;
	}

	// --------------------------------------------------------------------------------------------------------------------

	const ArchetypeBase* Archetype::GetArchetypeBase(unsigned short index) const
	{
		ECS_CRASH_RETURN_VALUE(index < m_base_archetypes.size, nullptr, "Incorrect base index {#} when trying to retrieve archetype base pointer from archetype.", index);
		return &m_base_archetypes[index].archetype;
	}

	// --------------------------------------------------------------------------------------------------------------------

	ComponentSignature Archetype::GetUniqueSignature() const
	{
		return m_unique_components;
	}

	// --------------------------------------------------------------------------------------------------------------------

	ComponentSignature Archetype::GetSharedSignature() const
	{
		return m_shared_components;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedComponentSignature Archetype::GetSharedSignature(unsigned short base_index) const
	{
		ECS_CRASH_RETURN_VALUE(base_index < m_base_archetypes.size, {}, "Incorrect base index {#} when trying to retrieve shared signature from archetype.", base_index);
		return { (Component*)m_shared_components.indices, (SharedInstance*)m_base_archetypes[base_index].shared_instances, m_shared_components.count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<ArchetypeBase> Archetype::GetArchetypeStream() const
	{
		return { m_base_archetypes.buffer, m_base_archetypes.size };
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned short Archetype::GetArchetypeBaseCount() const
	{
		return m_base_archetypes.size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature Archetype::GetVectorInstances(unsigned short base_index) const
	{
		ECS_CRASH_RETURN_VALUE(base_index < m_base_archetypes.size, {}, "Incorrect base index {#} when trying to retrieve vector instances from archetype.", base_index);
		return m_base_archetypes[base_index].vector_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

}