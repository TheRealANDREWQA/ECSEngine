#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/SequentialTable.h"
#include "InternalStructures.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	using namespace containers;

	struct MemoryManager;

	struct ECSENGINE_API ArchetypeBase {
	public:
		// The small memory manager is used for the chunks resizable stream
		// In order to not put pressure and fragment the main memory manager
		ArchetypeBase(
			MemoryManager* small_memory_manager,
			MemoryManager* memory_manager,
			unsigned int chunk_size,
			const ComponentInfo* infos,
			ComponentSignature components
		);
			
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ArchetypeBase);
		
		// Only allocate space for that entity - no default values are set for the components
		// Returns the chunk index in the x component and the stream index in the y component
		uint2 AddEntity(Entity entity);

		// Returns the chunk index in the x component and the stream index in the y component
		// Components and data must be synched
		uint2 AddEntity(Entity entity, ComponentSignature components, const void** data);

		// Only allocates space for these entities - no values are written to these entities
		// Pass the chunk_positions stream to a copy function which will do the actual write of the values
		void AddEntities(Stream<Entity> entities, CapacityStream<EntitySequence>& chunk_positions);

		// Splats the same value of the component to all entities
		void CopySplatComponents(
			Stream<EntitySequence> chunk_positions,
			const void** data,
			ComponentSignature components
		);

		// Copies components from another archetype
		void CopyFromAnotherArchetype(
			Stream<EntitySequence> chunk_positions,
			const ArchetypeBase* source_archetype,
			const Entity* entities, 
			const EntityPool* entity_pool,
			ComponentSignature components_to_copy
		);

		// Data parsed by entities ABC ABC ABC ABC ABC;
		// The list of pointers is flattened inside this single array so there is no need to allocate a seperate array for
		// the void** pointers
		void CopyByEntity(
			Stream<EntitySequence> chunk_positions,
			const void** data,
			ComponentSignature signature
		);

		// Data parsed by entities ABC ABC ABC ABC ABC;
		// There should be total count of entities pointers inside data
		void CopyByEntityContiguous(
			Stream<EntitySequence> chunk_positions,
			const void** data,
			ComponentSignature signature
		);

		// Data parsed by components layout AAAAA BBBBB CCCCC; It assumes that data is not contiguous
		// The list of pointers is flattened inside this single array so there is no need to allocate a seperate array for
		// the void** pointers
		void CopyByComponents(
			Stream<EntitySequence> chunk_positions,
			const void** data,
			ComponentSignature signature
		);

		// Data parsed by components layout AAAAA BBBBB CCCCC; It use memcpy to blit the values into each chunk
		// There should be signature.count data pointers
		void CopyByComponentsContiguous(
			Stream<EntitySequence> chunk_positions,
			const void** data,
			ComponentSignature signature
		);

		void CreateChunk();

		void CreateChunks(unsigned int count);

		void ClearChunk(unsigned int chunk_index);

		size_t ChunkMemorySize() const;

		void Deallocate();

		void DeallocateChunk(unsigned int index);

		// Deallocates the chunk, the pool is needed to update the entity infos for those entities that got swapped
		void DestroyChunk(unsigned int index, EntityPool* pool);

		unsigned char FindComponentIndex(Component component) const;

		// It will replace the absolute indices with indices that correspond to the current components
		void FindComponents(ComponentSignature components) const;

		// buffers must have size of at least chunk count
		void GetBuffers(void*** buffers) const;

		// buffers must have size of at least chunk count
		// It will fill in buffers in order of chunks and then components
		void GetBuffers(void*** buffers, size_t* chunk_sizes, ComponentSignature components) const;

		void* GetComponent(EntityInfo info, Component component) const;

		// The component index will be used to directly index into the buffers
		void* GetComponentByIndex(EntityInfo info, unsigned char component_index) const;

		unsigned int GetChunkCount() const;

		unsigned int GetEntityCount() const;

		// The entities stream must have a capacity of at least chunk
		void GetEntities(Stream<Entity>* entities) const;

		// It will copy the entities - consider using the other variant since it will alias the 
		// values inside the chunks and no copies are needed
		void GetEntities(CapacityStream<Entity>& entities) const;

		void Reserve(unsigned int count);

		void Reserve(unsigned int count, CapacityStream<EntitySequence>& chunk_positions);
		
		// It will update some entity infos for when the chunk is empty and it will be removed swap back-ed
		// by another chunk and these entities change their chunk index and for the entity that will replace
		// it inside the chunk
		void RemoveEntity(Entity entity, EntityPool* pool);

		void RemoveEntities(Stream<Entity> entities, EntityPool* pool);

		// Sets the entities previously allocated using reserve
		void SetEntities(const Entity* entities, Stream<EntitySequence> chunk_positions);

		void UpdateComponent(EntityInfo info, Component component, const void* data);

		// It will index directly into the component and copy the data
		void UpdateComponentByIndex(EntityInfo info, unsigned char component_index, const void* data);

	//public:
		struct Chunk {
			Entity* entities;
			void** buffers;
			unsigned int size;
		};

		MemoryManager* m_memory_manager;
		ResizableStream<Chunk> m_chunks;
		unsigned int m_chunk_size;
		// The global entity count
		unsigned int m_entity_count;
		// Unique infos - only reference
		const ComponentInfo* m_infos;
		// Unique components indices - only reference
		ComponentSignature m_components;
	};

}

