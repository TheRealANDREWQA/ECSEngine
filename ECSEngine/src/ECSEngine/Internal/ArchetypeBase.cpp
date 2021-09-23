#include "ecspch.h"
#include "ArchetypeBase.h"
#include "../Utilities/Function.h"
#include "../Allocators/MemoryManager.h"

namespace ECSEngine {

	ArchetypeBase::ArchetypeBase(
		unsigned int max_chunk_count,
		unsigned int chunk_size,
		unsigned int max_sequences,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager,
		unsigned int initial_chunk_count
	)
		: m_sequence_count(max_sequences), m_chunk_size(chunk_size), m_memory_manager(memory_manager), m_entity_count(0),
		m_max_chunk_count(max_chunk_count), m_component_infos(_component_infos)
	{
		void* initial_allocation = memory_manager->Allocate(MemoryOfInitialization(_component_infos.size, max_chunk_count), ECS_CACHE_LINE_SIZE);

		// initializing buffers
		uintptr_t buffer = (uintptr_t)initial_allocation;
		buffer = function::align_pointer(buffer, alignof(void*));
		m_buffers.buffer = (void**)buffer;
		buffer += sizeof(void*) * max_chunk_count * _component_infos.size;

		// initializing table pointer
		buffer = function::align_pointer(buffer, alignof(SequentialTable));
		m_table = (SequentialTable*)buffer;

		// creating initial chunks
		for (size_t index = 0; index < initial_chunk_count; index++) {
			CreateChunk();
		}
	}

	ArchetypeBase::ArchetypeBase(
		void* initial_allocation, 
		unsigned int max_chunk_count,
		unsigned int chunk_size, 
		unsigned int max_sequences,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager,
		unsigned int initial_chunk_count
	)
		: m_sequence_count(max_sequences), m_chunk_size(chunk_size), m_memory_manager(memory_manager), m_entity_count(0),
		m_max_chunk_count(max_chunk_count), m_component_infos(_component_infos)
	{

		// initializing buffers
		uintptr_t buffer = (uintptr_t)initial_allocation;
		buffer = function::align_pointer(buffer, alignof(void*));
		m_buffers.buffer = (void**)buffer;
		buffer += sizeof(void*) * max_chunk_count * _component_infos.size;

		// initializing table pointer
		buffer = function::align_pointer(buffer, alignof(SequentialTable));
		m_table = (SequentialTable*)buffer;

		// creating initial chunks
		for (size_t index = 0; index < initial_chunk_count; index++) {
			CreateChunk();
		}
	}

	ArchetypeBase::ArchetypeBase(
		ArchetypeBaseDescriptor descriptor,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager
	) : ArchetypeBase(
		descriptor.max_chunk_count,
		descriptor.chunk_size,
		descriptor.max_sequence_count,
		_component_infos,
		memory_manager,
		descriptor.initial_chunk_count
	) {}

	ArchetypeBase::ArchetypeBase(
		void* initial_allocation,
		ArchetypeBaseDescriptor descriptor,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager
	) : ArchetypeBase(
		initial_allocation,
		descriptor.max_chunk_count,
		descriptor.chunk_size,
		descriptor.max_sequence_count,
		_component_infos,
		memory_manager,
		descriptor.initial_chunk_count
	) {}
	
	void ArchetypeBase::AddEntities(unsigned int first, unsigned int size) {
		unsigned int local_first = first, local_size = size;
		for (size_t index = 0; index < m_buffers.size; index++) {
			unsigned int buffer_offset = m_table[index].Insert(local_first, local_size);
			if (buffer_offset != 0xFFFFFFFF) {
				m_entity_count += local_size;
				return;
			}
			else {
				unsigned int buffer_offset, biggest_block_size;
				m_table[index].GetBiggestFreeSequenceAndAllocate(local_first, buffer_offset, biggest_block_size);
				m_entity_count += biggest_block_size;
				local_size -= biggest_block_size;
				local_first += biggest_block_size;
			}
		}
		while (true) {
			CreateChunk();
			unsigned int offset = m_table[m_buffers.size - 1].Insert(local_first, local_size);
			if (offset != 0xFFFFFFFF) {
				m_entity_count += local_size;
				return;
			}
			else {
				unsigned int local_buffer_offset, biggest_block_size;
				m_table[m_buffers.size - 1].GetBiggestFreeSequenceAndAllocate(local_first, local_buffer_offset, biggest_block_size);
				m_entity_count += biggest_block_size;
				local_size -= biggest_block_size;
				local_first += biggest_block_size;
			}
		}
	}

	void ArchetypeBase::AddEntities(unsigned int first, unsigned int size, Stream<unsigned int>& buffer_offset, unsigned int* sizes) {
		unsigned int local_first = first, local_size = size;
		for (size_t index = 0; index < m_buffers.size; index++) {
			unsigned int offset = m_table[index].Insert(local_first, local_size);
			if (offset != 0xFFFFFFFF) {
				m_entity_count += local_size;
				buffer_offset[index] = offset;
				buffer_offset.size++;
				sizes[index] = local_size;
				return;
			}
			else {
				unsigned int local_buffer_offset, biggest_block_size;
				m_table[index].GetBiggestFreeSequenceAndAllocate(local_first, local_buffer_offset, biggest_block_size);
				m_entity_count += biggest_block_size;
				local_size -= biggest_block_size;
				local_first += biggest_block_size;
				buffer_offset[index] = local_buffer_offset;
				buffer_offset.size++;
				sizes[index] = biggest_block_size;
			}
		}
		while (true) {
			CreateChunk();
			unsigned int offset = m_table[m_buffers.size - 1].Insert(local_first, local_size);
			if (offset != 0xFFFFFFFF) {
				m_entity_count += local_size;
				buffer_offset[m_buffers.size - 1] = offset;
				buffer_offset.size++;
				sizes[m_buffers.size - 1] = local_size;
				return;
			}
			else {
				unsigned int local_buffer_offset, biggest_block_size;
				m_table[m_buffers.size - 1].GetBiggestFreeSequenceAndAllocate(local_first, local_buffer_offset, biggest_block_size);
				m_entity_count += biggest_block_size;
				local_size -= biggest_block_size;
				local_first += biggest_block_size;
				buffer_offset[m_buffers.size - 1] = local_buffer_offset;
				buffer_offset.size++;
				sizes[m_buffers.size - 1] = biggest_block_size;
			}
		}
	}

	void ArchetypeBase::AddEntities(
		unsigned int first, 
		unsigned int size,
		Stream<unsigned int>& buffer_offset, 
		unsigned int* sizes, 
		unsigned int* sequence_index
	) {
		unsigned int local_first = first, local_size = size;
		for (size_t index = 0; index < m_buffers.size; index++) {
			unsigned int offset = m_table[index].Insert(local_first, local_size);
			if (offset != 0xFFFFFFFF) {
				m_entity_count += local_size;
				buffer_offset[index] = offset;
				buffer_offset.size++;
				sizes[index] = local_size;
				sequence_index[index] = m_table[index].GetSequenceCount() - 1;
				return;
			}
			else {
				unsigned int local_buffer_offset, biggest_block_size;
				m_table[index].GetBiggestFreeSequenceAndAllocate(local_first, local_buffer_offset, biggest_block_size);
				m_entity_count += biggest_block_size;
				local_size -= biggest_block_size;
				local_first += biggest_block_size;
				buffer_offset[index] = local_buffer_offset;
				buffer_offset.size++;
				sizes[index] = biggest_block_size;
				sequence_index[index] = m_table[index].GetSequenceCount() - 1;
			}
		}
		while (true) {
			CreateChunk();
			unsigned int offset = m_table[m_buffers.size - 1].Insert(local_first, local_size);
			if (offset != 0xFFFFFFFF) {
				m_entity_count += local_size;
				buffer_offset[m_buffers.size - 1] = offset;
				buffer_offset.size++;
				sizes[m_buffers.size - 1] = local_size;
				sequence_index[m_buffers.size - 1] = m_table[m_buffers.size - 1].GetSequenceCount() - 1;
				return;
			}
			else {
				unsigned int local_buffer_offset, biggest_block_size;
				m_table[m_buffers.size - 1].GetBiggestFreeSequenceAndAllocate(local_first, local_buffer_offset, biggest_block_size);
				m_entity_count += biggest_block_size;
				local_size -= biggest_block_size;
				local_first += biggest_block_size;
				buffer_offset[m_buffers.size - 1] = local_buffer_offset;
				buffer_offset.size++;
				sizes[m_buffers.size - 1] = biggest_block_size;
				sequence_index[m_buffers.size - 1] = m_table[m_buffers.size - 1].GetSequenceCount() - 1;
			}
		}
	}

	void ArchetypeBase::AddEntities(unsigned int first, unsigned int size, bool contiguous) {
		ECS_ASSERT(size <= m_chunk_size);

		for (size_t index = 0; index < m_buffers.size; index++) {
			unsigned int buffer_offset = m_table[index].Insert(first, size);
			if (buffer_offset != 0xFFFFFFFF) {
				m_entity_count += size;
				return;
			}
		}
		CreateChunk();
		m_table[m_buffers.size - 1].Insert(first, size);
		m_entity_count += size;
	}

	void ArchetypeBase::AddEntities(
		unsigned int first, 
		unsigned int size, 
		unsigned int& offset, 
		unsigned int& chunk_index,
		bool contiguous
	) {
		ECS_ASSERT(size <= m_chunk_size);

		for (size_t index = 0; index < m_buffers.size; index++) {
			offset = m_table[index].Insert(first, size);
			if (offset != 0xFFFFFFFF) {
				m_entity_count += size;
				chunk_index = index;
				return;
			}
		}
		CreateChunk();
		offset = m_table[m_buffers.size - 1].Insert(first, size);
		chunk_index = m_buffers.size - 1;
		m_entity_count += size;
	}

	void ArchetypeBase::AddEntities(
		unsigned int first,
		unsigned int size,
		unsigned int& offset,
		unsigned int& chunk_index,
		unsigned int& sequence_index,
		bool contiguous
	) {
		ECS_ASSERT(size <= m_chunk_size);

		for (size_t index = 0; index < m_buffers.size; index++) {
			offset = m_table[index].Insert(first, size);
			if (offset != 0xFFFFFFFF) {
				m_entity_count += size;
				chunk_index = index;
				sequence_index = m_table[index].GetSequenceCount() - 1;
				return;
			}
		}
		CreateChunk();
		offset = m_table[m_buffers.size - 1].Insert(first, size);
		chunk_index = m_buffers.size - 1;
		sequence_index = m_table[m_buffers.size - 1].GetSequenceCount() - 1;
		m_entity_count += size;
	}

	void ArchetypeBase::AddEntities(
		unsigned int first,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int source_count
	) {
		unsigned int buffer[256];
		unsigned int sizes[256];
		Stream<unsigned int> chunk_offsets(buffer, 0);
		AddEntities(first, size, chunk_offsets, sizes);

		for (size_t source_index = 0; source_index < source_count; source_index++) {
			unsigned int first_index = 0;
			unsigned int last_index = 0;
			for (size_t chunk_index = 0; chunk_index < chunk_offsets.size; chunk_index++) {
				last_index += sizes[chunk_index];
				CopyKernel(
					data[source_index], 
					component_order[source_index],
					chunk_index, 
					chunk_offsets[chunk_index], 
					first_index, 
					last_index, 
					size
				);
				first_index += sizes[chunk_index];
			}
		}
	}

	void ArchetypeBase::AddEntities_mt(
		unsigned int first_copy_index,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int chunk_offset,
		unsigned int chunk_index,
		unsigned int total_entity_count,
		const Stream<unsigned int>& source_indices
	) {
		for (size_t source_index = 0; source_index < source_indices.size; source_index++) {
			CopyKernel(
				data[source_indices[source_index]],
				component_order[source_indices[source_index]],
				chunk_index,
				chunk_offset,
				first_copy_index,
				first_copy_index + size,
				total_entity_count
			);
		}
	}

	void ArchetypeBase::AddEntities(
		unsigned int first,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int source_count,
		bool parsed_by_entity
	) {
		unsigned int buffer[256];
		unsigned int sizes[256];
		Stream<unsigned int> chunk_offsets(buffer, 0);
		AddEntities(first, size, chunk_offsets, sizes);

		for (size_t source_index = 0; source_index < source_count; source_index++) {
			unsigned int first_index = 0;
			unsigned int last_index = 0;
			for (size_t chunk_index = 0; chunk_index < chunk_offsets.size; chunk_index++) {
				last_index += sizes[chunk_index];
				CopyKernel(
					data[source_index], 
					component_order[source_index],
					chunk_index,
					chunk_offsets[chunk_index],
					first_index, 
					last_index, 
					true
				);
				first_index += sizes[chunk_index];
			}
		}
	}

	// parsed by entity
	void ArchetypeBase::AddEntities_mt(
		unsigned int first_copy_index,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int chunk_offset,
		unsigned int chunk_index,
		const Stream<unsigned int>& source_indices
	) {
		for (size_t source_index = 0; source_index < source_indices.size; source_index++) {
			CopyKernel(
				data[source_indices[source_index]],
				component_order[source_indices[source_index]],
				chunk_index,
				chunk_offset,
				first_copy_index,
				first_copy_index + size,
				true
			);
		}
	}

	void ArchetypeBase::AddEntitiesSequential(
		unsigned int first,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int source_count
	) {
		unsigned int buffer[256];
		unsigned int sizes[256];
		Stream<unsigned int> chunk_offsets(buffer, 0);
		AddEntities(first, size, chunk_offsets, sizes);

		for (size_t source_index = 0; source_index < source_count; source_index++) {
			unsigned int first_index = 0;
			unsigned int last_index = 0;
			for (size_t chunk_index = 0; chunk_index < chunk_offsets.size; chunk_index++) {
				last_index += sizes[chunk_index];
				CopyKernelSequential(
					data[source_index], 
					component_order[source_index], 
					chunk_index, 
					chunk_offsets[chunk_index], 
					first_index, 
					last_index
				);
				first_index += sizes[chunk_index];
			}
		}
	}

	void ArchetypeBase::AddEntitiesSequential_mt(
		unsigned int first_copy_index,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int chunk_offset,
		unsigned int chunk_index,
		const Stream<unsigned int>& source_indices
	) {
		for (size_t source_index = 0; source_index < source_indices.size; source_index++) {
			CopyKernelSequential(
				data[source_indices[source_index]],
				component_order[source_indices[source_index]],
				chunk_index,
				chunk_offset,
				first_copy_index,
				first_copy_index + size
			);
		}
	}

	void ArchetypeBase::AddEntitiesSingleChunk(
		unsigned int first,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int source_count
	) {
		unsigned int chunk_offset;
		unsigned int chunk_index;
		AddEntities(first, size, chunk_offset, chunk_index, true);

		for (size_t source_index = 0; source_index < source_count; source_index++) {
			CopyKernel(data[source_index], component_order[source_index], chunk_index, chunk_offset, 0, size, size);
		}
	}

	void ArchetypeBase::AddEntitiesSingleChunk_mt(
		unsigned int first_copy_index,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int chunk_offset,
		unsigned int chunk_index,
		unsigned int total_entity_count,
		const Stream<unsigned int>& source_indices
	) {
		for (size_t source_index = 0; source_index < source_indices.size; source_index++) {
			CopyKernel(
				data[source_indices[source_index]], 
				component_order[source_indices[source_index]], 
				chunk_index, 
				chunk_offset,
				first_copy_index, 
				first_copy_index + size, 
				total_entity_count
			);
		}
	}

	void ArchetypeBase::AddEntitiesSingleChunk(
		unsigned int first,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int source_count,
		bool parsed_by_entity
	) {
		unsigned int chunk_offset;
		unsigned int chunk_index;
		AddEntities(first, size, chunk_offset, chunk_index, true);

		for (size_t source_index = 0; source_index < source_count; source_index++) {
			CopyKernel(data[source_index], component_order[source_index], chunk_index, chunk_offset, 0, size, true);
		}
	}

	void ArchetypeBase::AddEntitiesSingleChunk_mt(
		unsigned int first_copy_index,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int chunk_offset,
		unsigned int chunk_index,
		const Stream<unsigned int>& source_indices
	) {
		for (size_t source_index = 0; source_index < source_indices[source_index]; source_index++) {
			CopyKernel(
				data[source_indices[source_index]], 
				component_order[source_indices[source_index]], 
				chunk_index, 
				chunk_offset, 
				first_copy_index, 
				first_copy_index + size, 
				true
			);
		}
	}

	void ArchetypeBase::AddEntitiesSequentialSingleChunk(
		unsigned int first,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int source_count
	) {
		unsigned int chunk_offset;
		unsigned int chunk_index;
		AddEntities(first, size, chunk_offset, chunk_index, true);

		for (size_t source_index = 0; source_index < source_count; source_index) {
			CopyKernelSequential(data[source_index], component_order[source_index], chunk_index, chunk_offset, 0, size);
		}
	}

	void ArchetypeBase::AddEntitiesSequentialSingleChunk_mt(
		unsigned int first_copy_index,
		unsigned int size,
		const unsigned char** component_order,
		const void*** data,
		unsigned int chunk_offset,
		unsigned int chunk_index,
		unsigned int total_entity_count,
		const Stream<unsigned int>& source_indices
	) {
		for (size_t source_index = 0; source_index < source_indices.size; source_index) {
			CopyKernelSequential(
				data[source_indices[source_index]], 
				component_order[source_indices[source_index]], 
				chunk_index, 
				chunk_offset, 
				first_copy_index, 
				first_copy_index + size
			);
		}
	}

	void ArchetypeBase::CopyKernel(
		const void** data,
		const unsigned char* component_order,
		unsigned int chunk_index,
		unsigned int chunk_offset,
		unsigned int first_copy_index,
		unsigned int last_copy_index,
		unsigned int data_size
	) {
		// checking to see if there are entities to copy to not waste time searching components in infos
		if (first_copy_index < last_copy_index) {
			// copying each component in its buffer 
			for (size_t index = 1; index <= component_order[0]; index++) {
				unsigned char component_index = FindComponentIndex(component_order[index]);
				for (size_t copy_index = first_copy_index; copy_index < last_copy_index; copy_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[chunk_index * m_component_infos.size + component_index] +
							(chunk_offset + copy_index - first_copy_index) * m_component_infos[component_index].size),
						data[copy_index + data_size * (index - 1)],
						m_component_infos[component_index].size
					);
				}
			}
		}
	}

	void ArchetypeBase::CopyKernel(
		const void** data,
		const unsigned char* component_order,
		unsigned int chunk_index,
		unsigned int chunk_offset,
		unsigned int first_copy_index,
		unsigned int last_copy_index,
		bool parsed_by_entities
	) {
		// checking to see if there are entities to copy to not waste time searching components in infos
		if (first_copy_index < last_copy_index) {
			// caching component indices in stack memory
			unsigned char component_indices[64];
			for (size_t index = 1; index <= component_order[0]; index++) {
				component_indices[index - 1] = FindComponentIndex(component_order[index]);
			}
			// iterating over entities and copying each component that it is owned by it
			for (size_t copy_index = first_copy_index; copy_index < last_copy_index; copy_index++) {
				for (size_t component_index = 0; component_index < component_order[0]; component_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[chunk_index * m_component_infos.size + component_indices[component_index]]
							+ (chunk_offset + copy_index - first_copy_index) * m_component_infos[component_indices[component_index]].size),
						data[copy_index * component_order[0] + component_index],
						m_component_infos[component_indices[component_index]].size
					);
				}
			}
		}
	}

	void ArchetypeBase::CopyKernelSequential(
		const void** data,
		const unsigned char* component_order,
		unsigned int chunk_index,
		unsigned int chunk_offset,
		unsigned int first_copy_index,
		unsigned int last_copy_index
	) {
		if (first_copy_index < last_copy_index) {
			for (size_t component_index = 1; component_index <= component_order[0]; component_index++) {
				unsigned char archetype_component_index = FindComponentIndex(component_order[component_index]);
				memcpy(
					(void*)((uintptr_t)m_buffers[archetype_component_index + chunk_index * m_component_infos.size] 
						+ (chunk_offset) * m_component_infos[archetype_component_index].size),
					(void*)((uintptr_t)data[component_index - 1] + first_copy_index * m_component_infos[archetype_component_index].size),
					m_component_infos[archetype_component_index].size * (last_copy_index - first_copy_index)
				);
			}
		}
	}

	void ArchetypeBase::CreateChunk() {
		ECS_ASSERT(m_buffers.size < m_max_chunk_count);

		// adding 8 bytes for table buffer alignment
		void* allocation = m_memory_manager->Allocate(MemoryOf(), ECS_CACHE_LINE_SIZE);
		uintptr_t buffer = (uintptr_t)allocation;
		for (size_t index = 0; index < m_component_infos.size; index++) {
			buffer = function::align_pointer(buffer, m_component_infos[index].alignment);
			m_buffers[m_buffers.size * m_component_infos.size + index] = (void*)buffer;
			buffer += m_component_infos[index].size * m_chunk_size;
		}

		// alignment to 8 for sequential table buffer
		buffer = function::align_pointer(buffer, 8);
		m_table[m_buffers.size] = SequentialTable((void*)buffer, m_chunk_size, m_sequence_count);

		m_buffers.size++;
	}

	void ArchetypeBase::CreateChunks(unsigned int count) {
		for (size_t index = 0; index < count; index++) {
			CreateChunk();
		}
	}

	void ArchetypeBase::Deallocate() {
		m_memory_manager->Deallocate(m_buffers.buffer);
	}

	void ArchetypeBase::DeleteSequence(unsigned int chunk_index, unsigned int sequence_index) {
		m_table[chunk_index].DeleteSequence(sequence_index, true);
	}

	void ArchetypeBase::DestroyChunk(unsigned int index) {
		ECS_ASSERT(index >= 0 && m_buffers.size > index);

		m_memory_manager->Deallocate(m_buffers[index * m_component_infos.size]);
		// if it not the last chunk, the last chunk will replace it
		if (index != m_buffers.size - 1) {
			unsigned int starting_buffer = index * m_component_infos.size;
			unsigned int end_buffer = m_component_infos.size * (m_buffers.size - 1);
			for (size_t buffer_increment = 0; buffer_increment < m_component_infos.size; buffer_increment++) {
				m_buffers[starting_buffer + buffer_increment] = m_buffers[end_buffer + buffer_increment];
			}
			m_table[index] = m_table[m_buffers.size - 1];
		}
		m_buffers.size--;
	}

	unsigned char ArchetypeBase::FindComponentIndex(unsigned char component) const {
		for (size_t index = 0; index < m_component_infos.size; index++) {
			if (m_component_infos[index].index == component) {
				return index;
			}
		}
		return unsigned char(0xFF);
	}

	void ArchetypeBase::FreeChunk(unsigned int chunk_index) {
		size_t sequence_count = m_table[chunk_index].GetSequenceCount();
		Sequence* sequence_buffer = m_table[chunk_index].GetSequenceBuffer();
		for (size_t index = 0; index < sequence_count; index++) {
			m_entity_count -= sequence_buffer[index].size;
		}
		m_table[chunk_index].Clear();
	}

	void ArchetypeBase::GetBuffers(Stream<void*>& buffers) const {
		buffers = m_buffers;
	}

	void ArchetypeBase::GetBuffers(Stream<void*>& buffers, const unsigned char* components) const {
		size_t buffer_index = 0;
		for (size_t component = 1; component <= components[0]; component++) {
			unsigned char component_index = FindComponentIndex(components[component]);
			for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
				buffers[buffer_index] = m_buffers[chunk_index * m_component_infos.size + component_index];
				buffer_index++;
			}
		}
		buffers.size = buffer_index;
	}

	unsigned int ArchetypeBase::GetChunkCount() const {
		return m_buffers.size;
	}

	void* ArchetypeBase::GetComponent(unsigned int entity, unsigned char component) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetComponent(entity, component, info);
	}

	void* ArchetypeBase::GetComponent(unsigned int entity, unsigned char component, EntityInfo info) const {
		unsigned char component_index = FindComponentIndex(component);
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		return (void*)((uintptr_t)m_buffers[component_index + info.chunk * m_component_infos.size] +
			entity_index * m_component_infos[component_index].size);
	}

	void* ArchetypeBase::GetComponent(unsigned int entity, unsigned char component_index, bool is_component_index) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetComponent(entity, component_index, info, is_component_index);
	}

	void* ArchetypeBase::GetComponent(unsigned int entity, unsigned char component_index, EntityInfo info, bool is_component_index) const {
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		return (void*)((uintptr_t)m_buffers[component_index + info.chunk * m_component_infos.size] +
			entity_index * m_component_infos[component_index].size);
	}

	void* ArchetypeBase::GetComponent(unsigned int entity_index, unsigned char component_index, EntityInfo info, unsigned int entity_index_and_component_index) const {
		return (void*)((uintptr_t)m_buffers[component_index + info.chunk * m_component_infos.size] +
			entity_index * m_component_infos[component_index].size);
	}

	void ArchetypeBase::GetComponent(unsigned int entity, void** data) const {
		EntityInfo info = GetEntityInfo(entity);
		GetComponent(entity, info, data);
	}
	
	void ArchetypeBase::GetComponent(unsigned int entity, const unsigned char* components, void** data) const {
		EntityInfo info = GetEntityInfo(entity);
		GetComponent(entity, components, info, data);
	}

	void ArchetypeBase::GetComponent(unsigned int entity, EntityInfo info, void** data) const {
		size_t offset = info.chunk * m_component_infos.size;
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		for (size_t index = 0; index < m_component_infos.size; index++) {
			data[index] = (void*)((uintptr_t)m_buffers[index + offset] + entity_index * m_component_infos[index].size);
		}
	}

	void ArchetypeBase::GetComponent(unsigned int entity, const unsigned char* components, EntityInfo info, void** data) const {
		size_t offset = info.chunk * m_component_infos.size;
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		for (size_t index = 1; index <= components[0]; index++) {
			unsigned char component_index = FindComponentIndex(components[index]);
			data[index - 1] = (void*)((uintptr_t)m_buffers[component_index + offset] + entity_index * m_component_infos[component_index].size);
		}
	}

	unsigned char ArchetypeBase::GetComponentCount() const {
		return m_component_infos.size;
	}

	EntityInfo ArchetypeBase::GetEntityInfo(unsigned int entity) const {
		EntityInfo info;
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			unsigned int entity_sequence = m_table[chunk_index].GetSequenceOf(entity);
			if (entity_sequence != 0xFFFFFFFF) {
				info.sequence = entity_sequence;
				info.chunk = chunk_index;
				return info;
			}
		}

		// error handling - Entity info in archetype failed
		ECS_ASSERT(false);
		return info;
	}

	unsigned int ArchetypeBase::GetEntityCount() const {
		return m_entity_count;
	}

	unsigned int ArchetypeBase::GetEntityIndex(unsigned int entity) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetEntityIndex(entity, info);
	}

	unsigned int ArchetypeBase::GetEntityIndex(unsigned int entity, EntityInfo info) const {
		return m_table[info.chunk].Find(entity, info.sequence);
	}

	void ArchetypeBase::GetComponentInfo(Stream<ComponentInfo>& info) const {
		info = m_component_infos;
	}

	void ArchetypeBase::GetComponentOffsets(const unsigned char* components, Stream<void*>& buffers, Stream<Stream<Sequence>>& sequences) const {
		buffers.size = 0;
		for (size_t component_index = 1; component_index <= components[0]; component_index++) {
			unsigned char archetype_component_index = FindComponentIndex(components[component_index]);
			for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
				buffers[buffers.size++] = m_buffers[chunk_index * m_component_infos.size + archetype_component_index];
				m_table[chunk_index].GetSequences(sequences[chunk_index]);
			}
		}
	}

	void ArchetypeBase::GetEntities(Stream<unsigned int>& entities) const {
		entities.size = 0;
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			m_table[chunk_index].GetKeys(entities);
		}
	}

	void ArchetypeBase::GetEntities(Stream<Stream<Substream>>& substreams, unsigned int** entity_buffers) const {
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			Sequence* sequence_buffer = m_table[chunk_index].GetSequenceBuffer();
			size_t sequence_count = m_table[chunk_index].GetSequenceCount();
			m_table[chunk_index].GetKeyBuffer(entity_buffers[chunk_index]);
			for (size_t sequence_index = 0; sequence_index < sequence_count; sequence_index++) {
				substreams[chunk_index][sequence_index].offset = sequence_buffer[sequence_index].buffer_start;
				substreams[chunk_index][sequence_index].size = sequence_buffer[sequence_index].size;
			}
			substreams[chunk_index].size = sequence_count;
		}
		substreams.size = m_buffers.size;
	}

	void ArchetypeBase::GetEntities(Stream<Stream<Sequence>>& sequences, unsigned int** entity_buffers) const {
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			m_table[chunk_index].GetKeyBuffer(entity_buffers[chunk_index]);
			m_table[chunk_index].GetSequences(sequences[chunk_index]);
		}
		sequences.size = m_buffers.size;
	}

	unsigned int ArchetypeBase::GetRebalanceEntityCount(
		unsigned int chunk_index, 
		unsigned int& sequence_index, 
		Stream<unsigned int>& sequence_first
	) const {
		return m_table[chunk_index].GetCoalesceSequencesItemCount(sequence_index, sequence_first);
	}

	unsigned int ArchetypeBase::GetRebalanceEntityCount(
		unsigned int chunk_index, 
		unsigned int count, 
		unsigned int& sequence_index,
		Stream<unsigned int>& sequence_first
	) const {
		return m_table[chunk_index].GetCoalesceSequencesItemCount(sequence_index, count, sequence_first);
	}

	unsigned int ArchetypeBase::GetRebalanceEntityCount(
		unsigned int chunk_index, 
		unsigned int min_sequence_size, 
		unsigned int max_sequence_size, 
		unsigned int& sequence_index,
		Stream<unsigned int>& sequence_first
	) const {
		return m_table[chunk_index].GetCoalesceSequencesItemCount(
			sequence_index,
			min_sequence_size,
			max_sequence_size, 
			sequence_first
		);
	}

	unsigned int ArchetypeBase::GetRebalanceEntityCount(
		unsigned int chunk_index, 
		unsigned int min_sequence_size, 
		unsigned int max_sequence_size, 
		unsigned int count, 
		unsigned int& sequence_index,
		Stream<unsigned int>& sequence_first
	) const {
		return m_table[chunk_index].GetCoalesceSequencesItemCount(
			sequence_index, 
			min_sequence_size, 
			max_sequence_size, 
			count, 
			sequence_first
		);
	}

	void ArchetypeBase::GetSequences(Stream<Stream<Sequence>>& sequences) const {
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			m_table[chunk_index].GetSequences(sequences[chunk_index]);
		}
		sequences.size = m_buffers.size;
	}

	void ArchetypeBase::GetSequences(Stream<Stream<Sequence>>& sequences, unsigned int** entity_buffers) const {
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			m_table[chunk_index].GetSequences(sequences[chunk_index]);
			m_table[chunk_index].GetKeyBuffer(entity_buffers[chunk_index]);
		}
		sequences.size = m_buffers.size;
	}

	unsigned int ArchetypeBase::GetSequenceCount() const {
		unsigned int total = 0;
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			total += m_table[chunk_index].GetSequenceCount();
		}
		return total;
	}

	unsigned int ArchetypeBase::GetSequenceCount(unsigned int chunk_index) const {
		return m_table[chunk_index].GetSequenceCount();
	}

	size_t ArchetypeBase::MemoryOf() {
		return MemoryOf(m_chunk_size, m_sequence_count, m_component_infos);
	}

	void ArchetypeBase::Rebalance(unsigned int chunk_index, unsigned int initial_offset, const Stream<Substream>& copy_order) {
		size_t offset = 0;
		for (size_t index = 0; index < copy_order.size; index++) {
			for (size_t buffer_index = chunk_index * m_component_infos.size; buffer_index < (chunk_index + 1) * m_component_infos.size; buffer_index++) {
				memcpy(
					(void*)((uintptr_t)m_buffers[buffer_index] + m_component_infos.size * (initial_offset + offset)),
					(void*)((uintptr_t)m_buffers[buffer_index] + m_component_infos.size * copy_order[index].offset),
					m_component_infos.size * copy_order[index].size
				);
				offset += copy_order[index].size;
			}
		}
	}

	void ArchetypeBase::Rebalance(unsigned int new_first, unsigned int chunk_index, unsigned int item_count) {
		Substream* copy_order_buffer = (Substream*)m_memory_manager->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first, 
			item_count, 
			copy_order
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
	}

	void ArchetypeBase::Rebalance(
		unsigned int new_first, 
		unsigned int chunk_index, 
		unsigned int item_count, 
		LinearAllocator* temp_allocator
	) {
		temp_allocator->SetMarker();
		Substream* copy_order_buffer = (Substream*)temp_allocator->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first,
			item_count,
			copy_order
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
		temp_allocator->Deallocate(nullptr);
	}

	void ArchetypeBase::Rebalance(unsigned int new_first, unsigned int chunk_index, unsigned int count, unsigned int item_count) {
		Substream* copy_order_buffer = (Substream*)m_memory_manager->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first,
			item_count,
			copy_order, 
			count
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
	}

	void ArchetypeBase::Rebalance(
		unsigned int new_first,
		unsigned int chunk_index,
		unsigned int count, 
		unsigned int item_count,
		LinearAllocator* temp_allocator
	) {
		temp_allocator->SetMarker();
		Substream* copy_order_buffer = (Substream*)temp_allocator->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first,
			item_count,
			copy_order,
			count
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
		temp_allocator->Deallocate(nullptr);
	}

	void ArchetypeBase::Rebalance(
		unsigned int new_first, 
		unsigned int chunk_index, 
		unsigned int min_sequence_size, 
		unsigned int max_sequence_size,
		unsigned int item_count
	) {
		Substream* copy_order_buffer = (Substream*)m_memory_manager->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first,
			item_count,
			copy_order,
			min_sequence_size,
			max_sequence_size
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
	}

	void ArchetypeBase::Rebalance(
		unsigned int new_first,
		unsigned int chunk_index,
		unsigned int min_sequence_size,
		unsigned int max_sequence_size,
		unsigned int item_count,
		LinearAllocator* temp_allocator
	) {
		temp_allocator->SetMarker();
		Substream* copy_order_buffer = (Substream*)temp_allocator->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first,
			item_count,
			copy_order,
			min_sequence_size,
			max_sequence_size
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
		temp_allocator->Deallocate(nullptr);
	}

	void ArchetypeBase::Rebalance(
		unsigned int new_first, 
		unsigned int chunk_index, 
		unsigned int min_sequence_size, 
		unsigned int max_sequence_size, 
		unsigned int count,
		unsigned int item_count
	) {
		Substream* copy_order_buffer = (Substream*)m_memory_manager->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first,
			item_count,
			copy_order,
			min_sequence_size,
			max_sequence_size,
			count
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
	}

	void ArchetypeBase::Rebalance(
		unsigned int new_first,
		unsigned int chunk_index,
		unsigned int min_sequence_size,
		unsigned int max_sequence_size,
		unsigned int count,
		unsigned int item_count,
		LinearAllocator* temp_allocator
	) {
		temp_allocator->SetMarker();
		Substream* copy_order_buffer = (Substream*)temp_allocator->Allocate(
			sizeof(Substream) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER * 2,
			alignof(Substream)
		);

		Stream<Substream> copy_order(copy_order_buffer, 0);
		Substream* sorted_sequences_buffer = copy_order_buffer + item_count;
		unsigned int buffer_offset = m_table[chunk_index].CoalesceSequences(
			sorted_sequences_buffer,
			new_first,
			item_count,
			copy_order,
			min_sequence_size,
			max_sequence_size,
			count
		);

		Rebalance(chunk_index, buffer_offset, copy_order);
		temp_allocator->Deallocate(nullptr);
	}

	void ArchetypeBase::RemoveEntities(const Stream<unsigned int>& entities) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			for (size_t table_index = 0; table_index < m_buffers.size; table_index++) {
				// entity becomes its index in buffer after delete call; last entity in sequence represents
				// the buffer offset of the last entity in that sequence
				unsigned int last_entity_in_sequence = m_table[table_index].DeleteItem(entity);

				// checking to see if they are different in order to avoid an unnecessary swap
				if (last_entity_in_sequence != 0xFFFFFFFF) {
					// not using CopyKernel because it would mean that temporary values must be written to an
					// external buffer which might be slow since it is memory speed bound
					for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
						memcpy(
							(void*)((uintptr_t)m_buffers[table_index * m_component_infos.size + component_index] +
								entity * m_component_infos[component_index].size),
							(void*)((uintptr_t)m_buffers[table_index * m_component_infos.size + component_index] +
								last_entity_in_sequence * m_component_infos[component_index].size),
							m_component_infos[component_index].size
						);
					}
					// decrementing one by one because if an entity is not contained in this archetype 
					// it must not decrement the total count
					m_entity_count--;
					break;
				}
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::RemoveEntities(const Stream<unsigned int>& entities, Stream<unsigned int>& deleted_sequence_first) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			for (size_t table_index = 0; table_index < m_buffers.size; table_index++) {
				// entity becomes its index in buffer after delete call; last entity in sequence represents
				// the buffer offset of the last entity in that sequence
				unsigned int last_entity_in_sequence = m_table[table_index].DeleteItem(entity, deleted_sequence_first);

				// checking to see if they are different in order to avoid an unnecessary swap
				if (last_entity_in_sequence != 0xFFFFFFFF) {
					// not using CopyKernel because it would mean that temporary values must be written to an
					// external buffer which might be slow since it is memory speed bound
					for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
						memcpy(
							(void*)((uintptr_t)m_buffers[table_index * m_component_infos.size + component_index] +
								entity * m_component_infos[component_index].size),
							(void*)((uintptr_t)m_buffers[table_index * m_component_infos.size + component_index] +
								last_entity_in_sequence * m_component_infos[component_index].size),
							m_component_infos[component_index].size
						);
					}
					// decrementing one by one because if an entity is not contained in this archetype 
					// it must not decrement the total count
					m_entity_count--;
					break;
				}
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::RemoveEntities(const Stream<unsigned int>& entities, const Stream<EntityInfo>& entity_infos) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			unsigned int last_entity_in_sequence = m_table[entity_infos[index].chunk].DeleteItem(entity, entity_infos[index].sequence);
			if (entity != last_entity_in_sequence) {
				for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[entity_infos[index].chunk * m_component_infos.size + component_index] +
							entity * m_component_infos[component_index].size),
						(void*)((uintptr_t)m_buffers[entity_infos[index].chunk * m_component_infos.size + component_index] +
							last_entity_in_sequence * m_component_infos[component_index].size),
						m_component_infos[component_index].size
					);
				}
				// decrementing one by one because if an entity is not contained in this archetype 
				// it must not decrement the total count
				m_entity_count--;
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::RemoveEntities(
		const Stream<unsigned int>& entities, 
		const Stream<EntityInfo>& entity_infos, 
		Stream<unsigned int>& deleted_sequence_first
	) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			unsigned int last_entity_in_sequence = m_table[entity_infos[index].chunk].DeleteItem(entity, entity_infos[index].sequence, deleted_sequence_first);
			if (entity != last_entity_in_sequence) {
				for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[entity_infos[index].chunk * m_component_infos.size + component_index] +
							entity * m_component_infos[component_index].size),
						(void*)((uintptr_t)m_buffers[entity_infos[index].chunk * m_component_infos.size + component_index] +
							last_entity_in_sequence * m_component_infos[component_index].size),
						m_component_infos[component_index].size
					);
				}
				// decrementing one by one because if an entity is not contained in this archetype 
				// it must not decrement the total count
				m_entity_count--;
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::RemoveEntities(const Stream<unsigned int>& entities, const EntityInfo* infos, unsigned int buffer_initial_index) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			unsigned int last_entity_in_sequence = m_table[infos[entities[index] - buffer_initial_index].chunk]
				.DeleteItem(entity, infos[entities[index] - buffer_initial_index].sequence);
			if (entity != last_entity_in_sequence) {
				for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[infos[entities[index] - buffer_initial_index].chunk * m_component_infos.size 
							+ component_index] + entity * m_component_infos[component_index].size),
						(void*)((uintptr_t)m_buffers[infos[entities[index] - buffer_initial_index].chunk * m_component_infos.size 
							+ component_index] + last_entity_in_sequence * m_component_infos[component_index].size),
						m_component_infos[component_index].size
					);
				}
				// decrementing one by one because if an entity is not contained in this archetype 
				// it must not decrement the total count
				m_entity_count--;
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::RemoveEntities(
		const Stream<unsigned int>& entities, 
		const EntityInfo* infos, 
		unsigned int buffer_initial_index,
		Stream<unsigned int>& deleted_sequence_first
	) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			unsigned int last_entity_in_sequence = m_table[infos[entities[index] - buffer_initial_index].chunk]
				.DeleteItem(entity, infos[entities[index] - buffer_initial_index].sequence, deleted_sequence_first);
			if (entity != last_entity_in_sequence) {
				for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[infos[entities[index] - buffer_initial_index].chunk * m_component_infos.size
							+ component_index] + entity * m_component_infos[component_index].size),
						(void*)((uintptr_t)m_buffers[infos[entities[index] - buffer_initial_index].chunk * m_component_infos.size
							+ component_index] + last_entity_in_sequence * m_component_infos[component_index].size),
						m_component_infos[component_index].size
					);
				}
				// decrementing one by one because if an entity is not contained in this archetype 
				// it must not decrement the total count
				m_entity_count--;
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::RemoveEntities(const Stream<unsigned int>& entities, const EntityPool* pool) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			EntityInfo entity_info = pool->GetEntityInfo(entity);
			unsigned int last_entity_in_sequence = m_table[entity_info.chunk].DeleteItem(entity, entity_info.sequence);
			if (entity != last_entity_in_sequence) {
				for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[entity_info.chunk * m_component_infos.size + component_index] 
							+ entity * m_component_infos[component_index].size),
						(void*)((uintptr_t)m_buffers[entity_info.chunk * m_component_infos.size + component_index] 
							+ last_entity_in_sequence * m_component_infos[component_index].size),
						m_component_infos[component_index].size
					);
				}
				// decrementing one by one because if an entity is not contained in this archetype 
				// it must not decrement the total count
				m_entity_count--;
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::RemoveEntities(
		const Stream<unsigned int>& entities, 
		const EntityPool* pool, 
		Stream<unsigned int>& deleted_sequence_first
	) {
		for (size_t index = 0; index < entities.size; index++) {
			unsigned int entity = entities[index];
			EntityInfo entity_info = pool->GetEntityInfo(entity);
			unsigned int last_entity_in_sequence = m_table[entity_info.chunk].DeleteItem(entity, entity_info.sequence, deleted_sequence_first);
			if (entity != last_entity_in_sequence) {
				for (size_t component_index = 0; component_index < m_component_infos.size; component_index++) {
					memcpy(
						(void*)((uintptr_t)m_buffers[entity_info.chunk * m_component_infos.size + component_index]
							+ entity * m_component_infos[component_index].size),
						(void*)((uintptr_t)m_buffers[entity_info.chunk * m_component_infos.size + component_index]
							+ last_entity_in_sequence * m_component_infos[component_index].size),
						m_component_infos[component_index].size
					);
				}
				// decrementing one by one because if an entity is not contained in this archetype 
				// it must not decrement the total count
				m_entity_count--;
			}
		}
#ifdef ECS_ARCHETYPE_DESTROY_CHUNK_WHEN_NO_SEQUENCE
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			if (m_table[chunk_index].GetSequenceCount() == 0) {
				DestroyChunk(chunk_index);
			}
		}
#endif
	}

	void ArchetypeBase::UpdateComponent(unsigned int entity, unsigned char component, const void* data) {
		EntityInfo info = GetEntityInfo(entity);
		UpdateComponent(entity, component, data, info);
	}

	void ArchetypeBase::UpdateComponent(unsigned int entity, unsigned char component, const void* data, EntityInfo info) {
		unsigned char component_index = FindComponentIndex(component);
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		memcpy(
			(void*)((uintptr_t)m_buffers[info.chunk * m_component_infos.size + component_index] +
				entity_index * m_component_infos[component_index].size),
			data,
			m_component_infos[component_index].size
		);
	}

	void ArchetypeBase::UpdateComponent(unsigned int entity, unsigned char component_index, const void* data, EntityInfo info, bool is_component_index) {
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		memcpy(
			(void*)((uintptr_t)m_buffers[info.chunk * m_component_infos.size + component_index] +
				entity_index * m_component_infos[component_index].size),
			data,
			m_component_infos[component_index].size
		);
	}

	void ArchetypeBase::UpdateComponent(unsigned int entity_index, unsigned char component_index, const void* data, EntityInfo info, unsigned int component_and_entity_index) {
		memcpy(
			(void*)((uintptr_t)m_buffers[info.chunk * m_component_infos.size + component_index] +
				entity_index * m_component_infos[component_index].size),
			data,
			m_component_infos[component_index].size
		);
	}

	size_t ArchetypeBase::MemoryOf(unsigned int chunk_size, unsigned int max_sequences, const Stream<ComponentInfo>& component_infos) {
		size_t buffer_memory = 0;
		for (size_t index = 0; index < component_infos.size; index++) {
			buffer_memory += chunk_size * component_infos[index].size + component_infos[index].alignment;
		}
		// extra bytes for alignment
		return buffer_memory + SequentialTable::MemoryOf(chunk_size, max_sequences) + 8;
	}

	size_t ArchetypeBase::MemoryOfInitialization(unsigned int component_count, unsigned int chunk_count) {
		return (sizeof(void*) * component_count + sizeof(SequentialTable)) * chunk_count + 8;
	}

}