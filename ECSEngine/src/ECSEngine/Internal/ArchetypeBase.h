#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/SequentialTable.h"
#include "InternalStructures.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	struct MemoryManager;

	struct ECSENGINE_API ArchetypeBase {
	public:
		// The small memory manager is used for the chunks resizable stream
		// In order to not put pressure and fragment the main memory manager
		ArchetypeBase(
			MemoryManager* memory_manager,
			unsigned int starting_size,
			const ComponentInfo* infos,
			ComponentSignature components
		);
			
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ArchetypeBase);
		
		// Only allocate space for that entity - no default values are set for the components
		// Returns the stream index
		unsigned int AddEntity(Entity entity);

		// Returns the chunk index in the x component and the stream index in the y component
		// Components and data must be synched
		unsigned int AddEntity(Entity entity, ComponentSignature components, const void** data);

		// Only allocates space for these entities - no values are written to these entities
		// It returns the offset at which these entities can be written to
		unsigned int AddEntities(Stream<Entity> entities);

		void CopyOther(const ArchetypeBase* other);

		// Splats the same value of the component to all entities
		void CopySplatComponents(
			uint2 copy_position,
			const void** data,
			ComponentSignature components
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

		unsigned char FindComponentIndex(Component component) const;

		// It will replace the absolute indices with indices that correspond to the current components
		// It will set the value to -1 for the components that are missing
		void FindComponents(ComponentSignature components) const;

		// It will fill in the buffers array
		void GetBuffers(void** buffers, ComponentSignature components);

		void* GetComponent(EntityInfo info, Component component);

		// The component index will be used to directly index into the buffers
		void* GetComponentByIndex(EntityInfo info, unsigned char component_index);

		// The component index will be used to directly index into the buffers
		void* GetComponentByIndex(unsigned int stream_index, unsigned char component_index);

		unsigned int GetSize() const;

		unsigned int GetCapacity() const;

		// The entities stream must have a capacity of at least chunk
		Stream<Entity> GetEntities() const;

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
		
		// It will update some entity infos for when the chunk is empty and it will be removed swap back-ed
		// by another chunk and these entities change their chunk index and for the entity that will replace
		// it inside the chunk
		void RemoveEntity(Entity entity, EntityPool* pool);

		// It will update some entity infos for when the chunk is empty and it will be removed swap back-ed
		// by another chunk and these entities change their chunk index and for the entity that will replace
		// it inside the chunk
		void RemoveEntity(unsigned int stream_index, EntityPool* pool);

		// Sets the entities previously allocated using reserve
		void SetEntities(Stream<Entity> entities, unsigned int copy_position);

		void UpdateComponent(unsigned int stream_index, Component component, const void* data);

		// It will index directly into the component and copy the data
		void UpdateComponentByIndex(unsigned int stream_index, unsigned char component_index, const void* data);

	//public:
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

