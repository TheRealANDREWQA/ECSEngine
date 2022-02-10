#include "ecspch.h"
#include "Archetype.h"

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

	unsigned short Archetype::CreateBaseArchetype(SharedComponentSignature components, unsigned int archetype_chunk_size)
	{
		SharedInstance* shared_instances_allocation = (SharedInstance*)m_small_memory_manager->Allocate(sizeof(SharedInstance) * components.count, alignof(SharedInstance));
		for (size_t index = 0; index < components.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(components.indices[index]);
			ECS_ASSERT(component_index != -1);
			shared_instances_allocation[component_index] = components.instances[index];
		}

		unsigned int index = m_base_archetypes.Add({ 
			ArchetypeBase(
				m_small_memory_manager,
				m_memory_manager,
				archetype_chunk_size,
				m_unique_infos, 
				m_unique_components
			),
			shared_instances_allocation 
		});
		ECS_ASSERT(index < ECS_ARCHETYPE_MAX_BASE_ARCHETYPES);
		return index;
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

	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::DestroyBase(unsigned short archetype_index, EntityPool* pool)
	{
		m_base_archetypes[archetype_index].archetype.Deallocate();
		m_small_memory_manager->Deallocate(m_base_archetypes[archetype_index].shared_instances);
		m_base_archetypes.RemoveSwapBack(archetype_index);

		// Update the infos if the base_index differs from the last archetype
		if (m_base_archetypes.size != archetype_index) {
			for (size_t chunk_index = 0; chunk_index < m_base_archetypes[archetype_index].archetype.GetChunkCount(); chunk_index++) {
				for (size_t entity_index = 0; entity_index < m_base_archetypes[archetype_index].archetype.m_chunks[chunk_index].size; entity_index++) {
					EntityInfo* info = pool->GetInfoPtr(m_base_archetypes[archetype_index].archetype.m_chunks[chunk_index].entities[entity_index]);
					info->base_archetype = archetype_index;
				}
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
			ECS_ASSERT(component_index != -1);
			signature.indices[index] = { (unsigned short)component_index };
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::FindSharedComponents(ComponentSignature signature) const
	{
		for (size_t index = 0; index < signature.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(signature.indices[index]);
			ECS_ASSERT(component_index != -1);
			signature.indices[index] = { (unsigned short)component_index };
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void Archetype::FindSharedInstances(unsigned short archetype_index, SharedComponentSignature signature) const
	{
		ECS_ASSERT(archetype_index < m_base_archetypes.size, "Incorrect archetype index when constructing shared instances.");

		// Find the component indices and then do the linear search
		for (size_t index = 0; index < signature.count; index++) {
			unsigned char component_index = FindSharedComponentIndex(signature.indices[index]);
			ECS_ASSERT(component_index != -1);
			signature.instances[index] = m_base_archetypes[archetype_index].shared_instances[component_index];
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::FindArchetypeBase(SharedComponentSignature shared_signature)
	{
		unsigned short index = FindArchetypeBaseIndex(shared_signature);
		return index != -1 ? &m_base_archetypes[index].archetype : nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* ECS_VECTORCALL Archetype::FindArchetypeBase(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances)
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
			ECS_ASSERT(component_index != -1);
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

	unsigned short ECS_VECTORCALL Archetype::FindArchetypeBaseIndex(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances)
	{
		VectorComponentSignature vector_components(m_shared_components);
		VectorComponentSignature vector_instances;
		for (size_t index = 0; index < m_base_archetypes.size; index++) {
			shared_instances.InitializeSharedInstances({ m_shared_components.indices, (SharedInstance*)m_base_archetypes[index].shared_instances, m_shared_components.count });
			if (SharedComponentSignatureHasInstances(vector_components, vector_instances, shared_signature, shared_instances)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance* Archetype::GetArchetypeBaseInstances(unsigned short index)
	{
		ECS_ASSERT(index < m_base_archetypes.size);
		return m_base_archetypes[index].shared_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

	const SharedInstance* Archetype::GetArchetypeBaseInstances(unsigned short index) const
	{
		ECS_ASSERT(index < m_base_archetypes.size);
		return m_base_archetypes[index].shared_instances;
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* Archetype::GetArchetypeBase(unsigned short index)
	{
		ECS_ASSERT(index < m_base_archetypes.size);
		return &m_base_archetypes[index].archetype;
	}

	// --------------------------------------------------------------------------------------------------------------------

	const ArchetypeBase* Archetype::GetArchetypeBase(unsigned short index) const
	{
		ECS_ASSERT(index < m_base_archetypes.size);
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

}