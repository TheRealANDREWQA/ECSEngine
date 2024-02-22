#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "InternalStructures.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	struct MemoryManager;

	struct ECSENGINE_API ArchetypeBase {
		ArchetypeBase();

		// The small memory manager is used for the chunks resizable stream
		// In order to not put pressure and fragment the main memory manager
		ArchetypeBase(
			MemoryManager* memory_manager,
			unsigned int starting_size,
			const ComponentInfo* infos,
			ComponentSignature components
		);
			
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ArchetypeBase);
		
		// Only allocates space for that entity - no default values are set for the components
		// It returns the offset at which these entities can be written to
		unsigned int AddEntity(Entity entity);

		// Components and data must be synched
		unsigned int AddEntity(Entity entity, ComponentSignature components, const void** data);

		// Only allocates space for these entities - no values are written to these entities
		// It returns the offset at which these entities can be written to
		unsigned int AddEntities(Stream<Entity> entities);

		// By default, it will deep copy the components but you can disable this
		void CopyOther(const ArchetypeBase* other, bool deep_copy = true);

		// Splats the same value of the component to all entities
		void CopySplatComponents(
			uint2 copy_position,
			const void** data,
			ComponentSignature components
		);

		// Copies all the components from another base archetype with the same components as this one
		void CopyFromAnotherArchetype(
			uint2 copy_position,
			const ArchetypeBase* source_archetype,
			const Entity* entities,
			const EntityPool* entity_pool
		);

		// Copies components from another archetype
		void CopyFromAnotherArchetype(
			uint2 copy_position,
			const ArchetypeBase* source_archetype,
			const Entity* entities, 
			const EntityPool* entity_pool,
			ComponentSignature components_to_copy
		);

		// Data parsed by entities ABC ABC ABC ABC ABC;
		// The list of pointers is flattened inside this single array so there is no need to allocate a seperate array for
		// the void** pointers
		void CopyByEntity(
			uint2 copy_position,
			const void** data,
			ComponentSignature signature
		);

		// Data parsed by entities ABC ABC ABC ABC ABC;
		// There should be total count of entities pointers inside data
		void CopyByEntityContiguous(
			uint2 copy_position,
			const void** data,
			ComponentSignature signature
		);

		// Data parsed by components layout AAAAA BBBBB CCCCC; It assumes that data is not contiguous
		// The list of pointers is flattened inside this single array so there is no need to allocate a seperate array for
		// the void** pointers
		void CopyByComponents(
			uint2 copy_position,
			const void** data,
			ComponentSignature signature
		);

		// Data parsed by components layout AAAAA BBBBB CCCCC; It use memcpy to blit the values into each chunk
		// There should be signature.count data pointers
		void CopyByComponentsContiguous(
			uint2 copy_position,
			const void** data,
			ComponentSignature signature
		);

		void Deallocate();

		ECS_INLINE unsigned int EntityCount() const {
			return m_size;
		}

		// Returns UCHAR_MAX if it doesn't find it
		unsigned char FindComponentIndex(Component component) const;

		// It will replace the absolute indices with indices that correspond to the current components
		// It will set the value to UCHAR_MAX for the components that are missing
		void FindComponents(ComponentSignature components) const;

		ECS_INLINE Entity GetEntityAtIndex(unsigned int stream_index) const {
			return m_entities[stream_index];
		}

		// It will fill in the buffers array
		void GetBuffers(void** buffers, ComponentSignature components);

		void* GetComponent(EntityInfo info, Component component);

		const void* GetComponent(EntityInfo info, Component component) const;

		// The component index will be used to directly index into the buffers
		ECS_INLINE void* GetComponentByIndex(EntityInfo info, unsigned char component_index) {
			return GetComponentByIndex(info.stream_index, component_index);
		}

		// The component index will be used to directly index into the buffers
		ECS_INLINE void* GetComponentByIndex(unsigned int stream_index, unsigned char component_index) {
			return OffsetPointer(m_buffers[component_index], stream_index * m_infos[m_components.indices[component_index].value].size);
		}

		ECS_INLINE const void* GetComponentByIndex(EntityInfo info, unsigned char component_index) const {
			return GetComponentByIndex(info.stream_index, component_index);
		}

		ECS_INLINE const void* GetComponentByIndex(unsigned int stream_index, unsigned char component_index) const {
			return OffsetPointer(m_buffers[component_index], stream_index * m_infos[m_components.indices[component_index].value].size);
		}

		// It will copy the entities - consider using the other variant since it will alias the 
		// values inside the chunks and no copies are needed
		void GetEntitiesCopy(Entity* entities) const;

		void Resize(unsigned int count);

		// It will grow the capacity in case there is not enough space
		// It will return an index where things can be written. It does not update the size THO!!!
		unsigned int Reserve(unsigned int count);

		// It will shrink the storage such that there are only supplementary_elements left. If the count
		// is greater than the current remaining elements, it will do nothing
		void ShrinkToFit(unsigned int supplementary_elements = 0);
		
		// It needs the EntityPool* to update the EntityInfo for the swapped entity.
		// It doesn't modify the entity info for the entity being removed
		void RemoveEntity(Entity entity, EntityPool* pool);

		// It needs the EntityPool* to update the EntityInfo for the swapped entity.
		// It doesn't modify the entity info for the entity being removed
		void RemoveEntity(unsigned int stream_index, EntityPool* pool);

		// Sets the entities previously allocated using reserve
		void SetEntities(Stream<Entity> entities, unsigned int copy_position);

		void UpdateComponent(unsigned int stream_index, Component component, const void* data);

		// It will index directly into the component and copy the data
		void UpdateComponentByIndex(unsigned int stream_index, unsigned char component_index, const void* data);

		MemoryManager* m_memory_manager;
		Entity* m_entities;
		void** m_buffers;
		unsigned int m_size;
		unsigned int m_capacity;

		// Unique infos - only reference
		const ComponentInfo* m_infos;
		// Unique components indices - only reference
		ComponentSignature m_components;
	};

}

