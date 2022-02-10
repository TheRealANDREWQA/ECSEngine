#pragma once
#include "ArchetypeBase.h"

#ifndef ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE 
#define ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE (1 << 9)
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

		ECS_INLINE bool ECS_VECTORCALL CheckArchetypeQuery(ArchetypeQuery query) const {
			VectorComponentSignature unique_signature(m_unique_components);
			VectorComponentSignature shared_signature(m_shared_components);
			return unique_signature.HasComponents(query.unique) && unique_signature.HasComponents(query.shared);
		}

		ECS_INLINE bool ECS_VECTORCALL CheckArchetypeQuery(ArchetypeQueryExclude query) const {
			VectorComponentSignature unique_signature(m_unique_components);
			VectorComponentSignature shared_signature(m_shared_components);
			return unique_signature.HasComponents(query.unique) && unique_signature.ExcludesComponents(query.unique_excluded) &&
				shared_signature.HasComponents(query.shared) && shared_signature.ExcludesComponents(query.shared_excluded);
		}

		// The shared component order is needed in order to assign in the correct order the instances indices
		unsigned short CreateBaseArchetype(
			SharedComponentSignature shared_signature, 
			unsigned int archetype_chunk_size = ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE
		);

		// It will deallocate all of its base archetypes and then itself
		void Deallocate();

		void DeallocateBase(unsigned short archetype_index);

		// Deallocates that archetype and the removes swap back in the base archetype stream
		// The entities that are moved into the slot of the base inside the base stream
		// Need to have their infos updated. Prefer paying this cost instead of a stable reference stream
		// Which makes expensive the iteration - which all this architecture is about
		void DestroyBase(unsigned short archetype_index, EntityPool* pool);

		unsigned char FindUniqueComponentIndex(Component component) const;

		unsigned char FindSharedComponentIndex(Component component) const;

		// It will replace the indices inside the signature with the equivalent indices for the current archetype's mask
		void FindUniqueComponents(ComponentSignature signature) const;

		// It will replace the indices inside the signature with the equivalent indices for the current archetype's mask
		void FindSharedComponents(ComponentSignature signature) const;

		// It will replace the values inside the signature.instances with the corresponding instance index
		// The signature must contain initially the shared index in order to identify the correct component
		void FindSharedInstances(unsigned short archetype_index, SharedComponentSignature signature) const;

		// If it fails it returns nullptr
		ArchetypeBase* FindArchetypeBase(SharedComponentSignature shared_signature);

		// If it fails it returns nullptr
		ArchetypeBase* ECS_VECTORCALL FindArchetypeBase(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances);

		// If it fails it returns -1
		unsigned short FindArchetypeBaseIndex(SharedComponentSignature shared_signature) const;

		unsigned short ECS_VECTORCALL FindArchetypeBaseIndex(VectorComponentSignature shared_signature, VectorComponentSignature shared_instances);

		SharedInstance* GetArchetypeBaseInstances(unsigned short index);

		const SharedInstance* GetArchetypeBaseInstances(unsigned short index) const;

		ArchetypeBase* GetArchetypeBase(unsigned short index);

		const ArchetypeBase* GetArchetypeBase(unsigned short index) const;
		
		ComponentSignature GetUniqueSignature() const;

		ComponentSignature GetSharedSignature() const;

		SharedComponentSignature GetSharedSignature(unsigned short base_index) const;

		Stream<ArchetypeBase> GetArchetypeStream() const;

		unsigned short GetArchetypeBaseCount() const;

	//private:
		struct InternalBase {
			ArchetypeBase archetype;
			SharedInstance* shared_instances;
		};

		MemoryManager* m_small_memory_manager;
		MemoryManager* m_memory_manager;
		ResizableStream<InternalBase> m_base_archetypes;
		const ComponentInfo* m_unique_infos;
		ComponentSignature m_unique_components;
		ComponentSignature m_shared_components;
	};

}

