#pragma once
#include "ArchetypeBase.h"
#include "VectorComponentSignature.h"

#ifndef ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT 
#define ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT (1 << 4)
#endif

namespace ECSEngine {

	struct ECSENGINE_API Archetype
	{
		Archetype(
			MemoryManager* small_memory_manager,
			MemoryManager* memory_manager,
			const ComponentInfo* unique_infos,
			ComponentSignature unique_components,
			ComponentSignature shared_components
		);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(Archetype);

		// Deallocates the buffers of components that contain them in all base archetypes
		void CallEntityDeallocate();

		// Deallocates all buffers in a given base archetype
		void CallEntityDeallocate(unsigned int base_index);

		// Deallocates all buffers of the given entity
		void CallEntityDeallocate(EntityInfo info);

		// Deallocate the buffers that are contained by that given entity. Deallocate index is the index
		// in the m_unique_components_to_deallocate
		void CallEntityDeallocate(unsigned char deallocate_index, EntityInfo info);

		// Deallocates all buffers of the given entity for the given components
		void CallEntityDeallocate(EntityInfo info, ComponentSignature signature);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		void CallEntityCopy(EntityInfo info, unsigned char deallocate_index, const void* source_data, bool deallocate_previous);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		// If the component doesn't have buffers, it will be skipped
		void CallEntityCopy(EntityInfo info, Component component, const void* source_data, bool deallocate_previous);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		// The source data must correspond to the deallocate_index
		void CallEntityCopy(EntityInfo info, const void** source_data, bool deallocate_previous);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		// The source data must correspond to the signature
		void CallEntityCopy(EntityInfo info, ComponentSignature signature, const void** source_data, bool deallocate_previous);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		void CallEntityCopy(unsigned int stream_index, unsigned int base_index, unsigned char deallocate_index, const void* source_data, bool deallocate_previous);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		// If the component doesn't have buffers, it will be skipped
		void CallEntityCopy(unsigned int stream_index, unsigned int base_index, Component component, const void* source_data, bool deallocate_previous);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		// The source data must correspond to the deallocate_index
		void CallEntityCopy(unsigned int stream_index, unsigned int base_index, const void** source_data, bool deallocate_previous);

		// It will deallocate (or reallocate) the current buffer and replace it with the new given data
		// The source data must correspond to the signature
		void CallEntityCopy(unsigned int stream_index, unsigned int base_index, ComponentSignature signature, const void** source_data, bool deallocate_previous);

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

		ECS_INLINE unsigned char FindUniqueComponentIndex(Component component) const {
			return m_unique_components.Find(component);
		}

		ECS_INLINE unsigned char FindSharedComponentIndex(Component component) const {
			return m_shared_components.Find(component);
		}

		// Applies only for unique components. Returns UCHAR_MAX if it doesn't exist
		unsigned char FindCopyDeallocateComponentIndex(Component component) const;

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

		// Does not use the SIMD search. Returns -1 if the component doesn't exist
		SharedInstance GetBaseInstance(Component component, unsigned int base_index) const;

		// This version checks to see if the shared component index is in bounds
		SharedInstance GetBaseInstance(unsigned char shared_component_index, unsigned int base_index) const;

		// This version does not check to se if the shared component index is in bounds
		ECS_INLINE SharedInstance GetBaseInstanceUnsafe(unsigned char shared_component_index, unsigned int base_index) const {
			return GetBaseInstances(base_index)[shared_component_index];
		}

		ArchetypeBase* GetBase(unsigned int index);

		const ArchetypeBase* GetBase(unsigned int index) const;
		
		ECS_INLINE ComponentSignature GetUniqueSignature() const {
			return m_unique_components;
		}

		ECS_INLINE ComponentSignature GetSharedSignature() const {
			return m_shared_components;
		}

		SharedComponentSignature GetSharedSignature(unsigned int base_index) const;

		ECS_INLINE unsigned int GetBaseCount() const {
			return m_base_archetypes.size;
		}

		VectorComponentSignature ECS_VECTORCALL GetVectorInstances(unsigned int base_index) const;

		// Returns the total amount of entities in all base archetypes
		unsigned int GetEntityCount() const;

		// It will remove the component as a user defined copy/deallocate component, if it exists
		void RemoveCopyDeallocateComponent(Component component);

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

		// These are kept in order to speed up the deallocation of the buffer when an entity is destroyed
		ArchetypeUserDefinedComponents m_user_defined_components;
	};

}

