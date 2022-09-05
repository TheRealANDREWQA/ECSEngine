#pragma once
#include "ArchetypeBase.h"
#include "VectorComponentSignature.h"

#ifndef ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT 
#define ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT (1 << 4)
#endif

namespace ECSEngine {

	struct ECSENGINE_API Archetype
	{
	public:
		Archetype(
			MemoryManager* small_memory_manager,
			MemoryManager* memory_manager,
			const ComponentInfo* unique_infos,
			ComponentSignature unique_components,
			ComponentSignature shared_components
		);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(Archetype);

		// The shared component order is needed in order to assign in the correct order the instances indices
		unsigned int CreateBaseArchetype(
			SharedComponentSignature shared_signature, 
			unsigned int archetype_starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT
		);

		void CopyOther(const Archetype* other);

		// It will deallocate all of its base archetypes and then itself
		void Deallocate();

		void DeallocateBase(unsigned int archetype_index);

		// Deallocates that archetype and the removes swap back in the base archetype stream
		// The entities that are moved into the slot of the base inside the base stream
		// Need to have their infos updated. Prefer paying this cost instead of a stable reference stream
		// Which makes expensive the iteration - which all this architecture is about
		void DestroyBase(unsigned int archetype_index, EntityPool* pool);

		unsigned char FindUniqueComponentIndex(Component component) const;

		unsigned char FindSharedComponentIndex(Component component) const;

		// It will replace the indices inside the signature with the equivalent indices for the current archetype's mask
		// If a component cannot be found, it will be set to -1
		void FindUniqueComponents(ComponentSignature signature) const;

		// It will replace the indices inside the signature with the equivalent indices for the current archetype's mask
		// If a component cannot be found, it will be set to -1
		void FindSharedComponents(ComponentSignature signature) const;

		// It will replace the values inside the signature.instances with the corresponding instance index
		// The signature must contain initially the shared index in order to identify the correct component
		// If a component cannot be found, a shared instance of -1 is returned
		void FindSharedInstances(unsigned int archetype_index, SharedComponentSignature signature) const;

		// If it fails it returns nullptr
		ArchetypeBase* FindBase(SharedComponentSignature shared_signature);

		// If it fails it returns nullptr
		ArchetypeBase* ECS_VECTORCALL FindBase(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances);

		// If it fails it returns -1
		unsigned int FindBaseIndex(SharedComponentSignature shared_signature) const;

		unsigned int ECS_VECTORCALL FindBaseIndex(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances);

		SharedInstance* GetBaseInstances(unsigned int index);

		const SharedInstance* GetBaseInstances(unsigned int index) const;

		ArchetypeBase* GetBase(unsigned int index);

		const ArchetypeBase* GetBase(unsigned int index) const;
		
		ComponentSignature GetUniqueSignature() const;

		ComponentSignature GetSharedSignature() const;

		SharedComponentSignature GetSharedSignature(unsigned int base_index) const;

		unsigned int GetBaseCount() const;

		VectorComponentSignature ECS_VECTORCALL GetVectorInstances(unsigned int base_index) const;

	//private:
		struct InternalBase {
			ArchetypeBase archetype;
			SharedInstance* shared_instances;
			VectorComponentSignature vector_instances;
		};

		MemoryManager* m_small_memory_manager;
		MemoryManager* m_memory_manager;
		ResizableStream<InternalBase> m_base_archetypes;
		const ComponentInfo* m_unique_infos;
		ComponentSignature m_unique_components;
		ComponentSignature m_shared_components;
	};

}

