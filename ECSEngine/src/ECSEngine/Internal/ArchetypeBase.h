#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/SequentialTable.h"
#include "InternalStructures.h"

#ifndef ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_COUNT
#define ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_COUNT 8
#endif

#ifndef ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_SIZE
#define ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_SIZE 1024
#endif

#ifndef ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_INITIAL_CHUNK_COUNT
#define ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_INITIAL_CHUNK_COUNT 1
#endif

#ifndef ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_SEQUENCE_COUNT
#define ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_SEQUENCE_COUNT 64
#endif

#ifndef ECS_ARCHETYPE_BASE_REBALANCE_BUFFER
#define ECS_ARCHETYPE_BASE_REBALANCE_BUFFER 5000
#endif

namespace ECSEngine {

	using namespace containers;

	struct MemoryManager;

	class ECSENGINE_API ArchetypeBase {
	public:
		ArchetypeBase(
			unsigned int max_chunk_count,
			unsigned int chunk_size,
			unsigned int max_sequences,
			const Stream<ComponentInfo>& _component_infos,
			MemoryManager* memory_manager,
			unsigned int initial_chunk_count = 0
		);
		ArchetypeBase(
			void* initial_allocation,
			unsigned int max_chunk_count,
			unsigned int chunk_size,
			unsigned int max_sequences,
			const Stream<ComponentInfo>& _component_infos,
			MemoryManager* memory_manager,
			unsigned int initial_chunk_count = 0
		);
		ArchetypeBase(
			ArchetypeBaseDescriptor descriptor,
			const Stream<ComponentInfo>& _component_infos,
			MemoryManager* memory_manager
		);
		ArchetypeBase(
			void* initial_allocation,
			ArchetypeBaseDescriptor descriptor,
			const Stream<ComponentInfo>& _component_infos,
			MemoryManager* memory_manager
		);
			
		ArchetypeBase& operator = (const ArchetypeBase& other) = default;
		
		void AddEntities(unsigned int first, unsigned int size);

		// Can serve as initialization function for multithreaded functions
		void AddEntities(unsigned int first, unsigned int size, Stream<unsigned int>& buffer_offsets, unsigned int* sizes);

		// Can serve as initialization function for multithreaded functions
		void AddEntities(unsigned int first, unsigned int size, Stream<unsigned int>& buffer_offsets, unsigned int* sizes, unsigned int* sequence_index);

		// supports contiguous insertions if the requested size is smaller than the chunk size
		void AddEntities(unsigned int first, unsigned int size, bool contiguous);

		// Can serve as initialization function for multithreaded functions
		// supports contiguous insertions if the requested size is smaller than the chunk size
		void AddEntities(unsigned int first, unsigned int size, unsigned int& offset, unsigned int& chunk_index, bool contiguous);

		// Can serve as initialization function for multithreaded functions
		// supports contiguous insertions if the requested size is smaller than the chunk size
		void AddEntities(unsigned int first, unsigned int size, unsigned int& offset, unsigned int& chunk_index, unsigned int& sequence_index, bool contiguous);

		// data coming from multiple sources, not contiguous; data -> components layout: AAAAA BBBBB CCCCC
		// SINGLE THREADED
		void AddEntities(
			unsigned int first, 
			unsigned int size, 
			const unsigned char** component_order,
			const void*** data,
			unsigned int source_count
		);

		// data coming from multiple sources, not contiguous; data -> components layout: AAAAA BBBBB CCCCC
		// MULTITHREADED; job for a worker thread
		void AddEntities_mt(
			unsigned int first_copy_index,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int chunk_offset,
			unsigned int chunk_index,
			unsigned int total_entity_count,
			const Stream<unsigned int>& source_indices
		);

		// data coming from multiple sources, not contiguous; data -> entity layout: ABC ABC ABC ABC ABC
		// SINGLE THREADED
		void AddEntities(
			unsigned int first, 
			unsigned int size, 
			const unsigned char** component_order, 
			const void*** data, 
			unsigned int source_count,
			bool parsed_by_entity
		);

		// data coming from multiple sources, not contiguous; data -> entity layout: ABC ABC ABC ABC ABC
		// MULTITHREADED; job for a worker thread
		void AddEntities_mt(
			unsigned int first_copy_index,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int chunk_offset,
			unsigned int chunk_index,
			const Stream<unsigned int>& source_indices
		);

		// the prefered way of adding entities since memcpy can use higher bit copy constructs, parsed by components 
		// SINGLE THREADED
		void AddEntitiesSequential(
			unsigned int first,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int source_count
		);

		// the prefered way of adding entities since memcpy can use higher bit copy constructs, parsed by components 
		// MULTITHREADED; job for a worker thread
		void AddEntitiesSequential_mt(
			unsigned int first_copy_index,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int chunk_offset,
			unsigned int chunk_index,
			const Stream<unsigned int>& source_indices
		);

		// data coming from multiple sources, not contiguous, must be placed in the same chunk; 
		// data -> components layout: AAAAA BBBBB CCCCC; 
		// SINGLE THREADED
		void AddEntitiesSingleChunk(
			unsigned int first,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int source_count
		);

		// data coming from multiple sources, not contiguous, must be placed in the same chunk; 
		// data -> components layout: AAAAA BBBBB CCCCC; 
		// MULTITHREADED; job for a worker thread
		void AddEntitiesSingleChunk_mt(
			unsigned int first_copy_index,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int chunk_offset,
			unsigned int chunk_index,
			unsigned int total_entity_count,
			const Stream<unsigned int>& source_indices
		);

		// data coming from multiple sources, not contiguous, must be placed in the same chunk; 
		// data -> entity layout: ABC ABC ABC ABC ABC; 
		// SINGLE THREADED
		void AddEntitiesSingleChunk(
			unsigned int first,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int source_count,
			bool parsed_by_entity
		);

		// data coming from multiple sources, not contiguous, must be placed in the same chunk; 
		// data -> entity layout: ABC ABC ABC ABC ABC; 
		// MULTITHREADED; job for a worker thread
		void AddEntitiesSingleChunk_mt(
			unsigned int first_copy_index,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int chunk_offset,
			unsigned int chunk_index,
			const Stream<unsigned int>& source_indices
		);

		// the prefered way of adding entities since memcpy can use higher bit copy constructs, parsed by components 
		// single chunk insertion
		// SINGLE THREADED
		void AddEntitiesSequentialSingleChunk(
			unsigned int first,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int source_count
		);

		// the prefered way of adding entities since memcpy can use higher bit copy constructs, parsed by components 
		// single chunk insertion
		// MULTITHREADED; job for worker thread
		void AddEntitiesSequentialSingleChunk_mt(
			unsigned int first_copy_index,
			unsigned int size,
			const unsigned char** component_order,
			const void*** data,
			unsigned int chunk_offset,
			unsigned int chunk_index,
			unsigned int total_entity_count,
			const Stream<unsigned int>& source_indices
		);

		// data parsed by components, last_copy_index is not included; layout: AAAAA BBBBB CCCCC
		void CopyKernel(
			const void** data, 
			const unsigned char* component_order, 
			unsigned int chunk_index,
			unsigned int chunk_offset,
			unsigned int first_copy_index,
			unsigned int last_copy_index,
			unsigned int data_size
		);

		// data parsed by entities, last_copy_index is not included; layout: ABC ABC ABC ABC ABC
		void CopyKernel(
			const void** data,
			const unsigned char* component_order,
			unsigned int chunk_index,
			unsigned int chunk_offset,
			unsigned int first_copy_index,
			unsigned int last_copy_index,
			bool parsed_by_entities
		);

		// data parsed by components, last_copy_index is not included; layout AAAAA BBBBB CCCCC
		void CopyKernelSequential(
			const void** data,
			const unsigned char* component_order,
			unsigned int chunk_index,
			unsigned int chunk_offset,
			unsigned int first_copy_index,
			unsigned int last_copy_index
		);

		void CreateChunk();

		void CreateChunks(unsigned int count);

		void Deallocate();

		void DeleteSequence(unsigned int chunk_index, unsigned int sequence_index);

		// deallocates the chunk 
		void DestroyChunk(unsigned int index);

		// linear search over infos
		unsigned char FindComponentIndex(unsigned char component) const;

		// frees the chunk without allocating a new one
		void FreeChunk(unsigned int chunk_index);

		// no need to allocate memory since it will point to the same buffer
		void GetBuffers(Stream<void*>& buffers) const;

		// need to allocate memory
		void GetBuffers(Stream<void*>& buffers, const unsigned char* components) const;

		unsigned int GetChunkCount() const;

		// must search every chunk and every sequence in order to find the entity; DO NOT USE IN TIGHT LOOPS
		void* GetComponent(unsigned int entity, unsigned char component) const;

		// the preferred way of getting a component since it requires mostly reading the table buffer
		void* GetComponent(unsigned int entity, unsigned char component, EntityInfo info) const;

		// last parameter is dummy
		void* GetComponent(unsigned int entity, unsigned char component_index, bool is_component_index) const;

		// last parameter is dummy
		void* GetComponent(unsigned int entity, unsigned char component_index, EntityInfo info, bool is_component_index) const;

		// last parameter is dummy
		void* GetComponent(unsigned int entity_index, unsigned char component_index, EntityInfo info, unsigned int entity_index_and_component_index) const;

		// if all components need to be pointed
		void GetComponent(unsigned int entity, void** data) const;

		// if some components need to be pointed
		void GetComponent(unsigned int entity, const unsigned char* components, void** data) const;

		// if all components need to be pointed
		void GetComponent(unsigned int entity, EntityInfo info, void** data) const;

		// if some components need to be pointed
		void GetComponent(unsigned int entity, const unsigned char* components, EntityInfo info, void** data) const;

		unsigned char GetComponentCount() const;

		void GetComponentInfo(Stream<ComponentInfo>& info) const;

		// using stream of sequences since it can point directly to the table sequence buffer
		void GetComponentOffsets(const unsigned char* components, Stream<void*>& buffers, Stream<Stream<Sequence>>& sequences) const;

		EntityInfo GetEntityInfo(unsigned int entity) const;

		unsigned int GetEntityCount() const;

		unsigned int GetEntityIndex(unsigned int entity) const;

		unsigned int GetEntityIndex(unsigned int entity, EntityInfo info) const;

		void GetEntities(Stream<unsigned int>& entities) const;

		void GetEntities(Stream<Stream<Substream>>& substreams, unsigned int** entity_buffers) const;

		void GetEntities(Stream<Stream<Sequence>>& sequences, unsigned int** entity_buffers) const;

		// the stream will store the first entity of the sequences that will be deallocated
		unsigned int GetRebalanceEntityCount(
			unsigned int chunk_index, 
			unsigned int& new_sequence_index, 
			Stream<unsigned int>& sequence_first
		) const;

		// the stream will store the first entity of the sequences that will be deallocated
		unsigned int GetRebalanceEntityCount(
			unsigned int chunk_index, 
			unsigned int count, 
			unsigned int& new_sequence_index,
			Stream<unsigned int>& sequence_first
		) const;

		// the stream will store the first entity of the sequences that will be deallocated
		unsigned int GetRebalanceEntityCount(
			unsigned int chunk_index, 
			unsigned int min_sequence_size, 
			unsigned int max_sequence_size,
			unsigned int& new_sequence_index,
			Stream<unsigned int>& sequence_first
		) const;

		// the stream will store the first entity of the sequences that will be deallocated
		unsigned int GetRebalanceEntityCount(
			unsigned int chunk_index,
			unsigned int min_sequence_size, 
			unsigned int max_sequence_size, 
			unsigned int count, 
			unsigned int& new_sequence_index,
			Stream<unsigned int>& sequence_first
		) const;

		// it will point to the same buffer as those inside tables
		void GetSequences(Stream<Stream<Sequence>>& sequences) const;

		// it will point to the same buffer as those inside tables
		void GetSequences(Stream<Stream<Sequence>>& sequences, unsigned int** entity_buffer) const;

		unsigned int GetSequenceCount() const;

		unsigned int GetSequenceCount(unsigned int chunk_index) const;

		// memory needed for a chunk
		size_t MemoryOf();

		// item count parameter value should be given from a call to GetRebalanceEntityCount; 
		void Rebalance(unsigned int new_first, unsigned int chunk_index, unsigned int entity_count);

		// item count parameter value should be given from a call to GetRebalanceEntityCount;
		void Rebalance(
			unsigned int new_first, 
			unsigned int chunk_index, 
			unsigned int entity_count, 
			LinearAllocator* temp_allocator
		);

		// item count parameter value should be given from a call to GetRebalanceEntityCount; 
		void Rebalance(unsigned int new_first, unsigned int chunk_index, unsigned int count, unsigned int entity_count);

		// item count parameter value should be given from a call to GetRebalanceEntityCount; 
		void Rebalance(
			unsigned int new_first, 
			unsigned int chunk_index, 
			unsigned int count, 
			unsigned int entity_count,
			LinearAllocator* temp_allocator
		);

		// item count parameter value should be given from a call to GetRebalanceEntityCount; 
		void Rebalance(
			unsigned int new_first, 
			unsigned int chunk_index,
			unsigned int min_sequence_size, 
			unsigned int max_sequence_size,
			unsigned int entity_count
		);

		// item count parameter value should be given from a call to GetRebalanceEntityCount; 
		void Rebalance(
			unsigned int new_first,
			unsigned int chunk_index,
			unsigned int min_sequence_size,
			unsigned int max_sequence_size,
			unsigned int entity_count,
			LinearAllocator* temp_allocator
		);

		// item count parameter value should be given from a call to GetRebalanceEntityCount; 
		void Rebalance(
			unsigned int new_first,
			unsigned int chunk_index, 
			unsigned int min_sequence_size,
			unsigned int max_sequence_size,
			unsigned int count,
			unsigned int entity_count
		);

		// item count parameter value should be given from a call to GetRebalanceEntityCount; 
		void Rebalance(
			unsigned int new_first,
			unsigned int chunk_index,
			unsigned int min_sequence_size,
			unsigned int max_sequence_size,
			unsigned int count,
			unsigned int entity_count,
			LinearAllocator* temp_allocator
		);

		// must search every chunk and every sequence in order to find the entity; DO NOT USE IN TIGHT LOOPS
		void RemoveEntities(const Stream<unsigned int>& entities);

		// must search every chunk and every sequence in order to find the entity; DO NOT USE IN TIGHT LOOPS
		void RemoveEntities(const Stream<unsigned int>& entities, Stream<unsigned int>& deleted_sequence_first);
		
		void RemoveEntities(const Stream<unsigned int>& entities, const Stream<EntityInfo>& infos);

		void RemoveEntities(const Stream<unsigned int>& entities, const Stream<EntityInfo>& infos, Stream<unsigned int>& deleted_sequence_first);

		// assumes that the entity value represents also the index in the info buffer
		void RemoveEntities(const Stream<unsigned int>& entities, const EntityInfo* infos, unsigned int buffer_initial_index);

		// assumes that the entity value represents also the index in the info buffer
		void RemoveEntities(
			const Stream<unsigned int>& entities, 
			const EntityInfo* infos, 
			unsigned int buffer_initial_index,
			Stream<unsigned int>& deleted_sequence_first
		);

		// the preferred way of removing entities since it will read the entity infos directly from the pool
		void RemoveEntities(const Stream<unsigned int>& entities, const EntityPool* pool);

		// the preferred way of removing entities since it will read the entity infos directly from the pool
		void RemoveEntities(const Stream<unsigned int>& entities, const EntityPool* pool, Stream<unsigned int>& deleted_sequence_first);

		// must search every chunk and every sequence in order to find the entity; DO NOT USE IN TIGHT LOOPS
		void UpdateComponent(unsigned int entity, unsigned char component, const void* data);

		// the preferred way of updating the component since it requires mostly copying the new data
		void UpdateComponent(unsigned int entity, unsigned char component, const void* data, EntityInfo info);

		// the preferred way of updating the component since it requires only copying the new data; last argument is dummy
		void UpdateComponent(unsigned int entity, unsigned char component_index, const void* data, EntityInfo info, bool is_component_index);

		// the preferred way of updating the component since it requires only copying the new data; last argument is dummy
		void UpdateComponent(unsigned int entity_index, unsigned char component_index, const void* data, EntityInfo info, unsigned int component_and_entity_index);

		// memory needed for a chunk
		static size_t MemoryOf(unsigned int chunk_size, unsigned int max_sequences, const Stream<ComponentInfo>& component_infos);

		static size_t MemoryOfInitialization(unsigned int component_count, unsigned int max_chunk_count);

	private:
		void Rebalance(unsigned int chunk_index, unsigned int buffer_offset, const Stream<Substream>& copy_order);

		Stream<ComponentInfo> m_component_infos;
		Stream<void*> m_buffers;
		SequentialTable* m_table;
		MemoryManager* m_memory_manager;
		unsigned int m_max_chunk_count;
		unsigned int m_chunk_size;
		unsigned int m_entity_count;
		unsigned int m_sequence_count;
	};

}

