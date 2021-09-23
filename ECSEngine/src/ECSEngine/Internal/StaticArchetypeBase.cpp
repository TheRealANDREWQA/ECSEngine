#include "ecspch.h"
#include "StaticArchetypeBase.h"
#include "../Utilities/Function.h"
#include "../Allocators/MemoryManager.h"

namespace ECSEngine {

	StaticArchetypeBase::StaticArchetypeBase(
		unsigned int max_chunk_count,
		unsigned int chunk_size,
		unsigned int max_sequences,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager,
		unsigned int initial_chunk_count
	) : m_max_chunk_count(max_chunk_count), m_chunk_size(chunk_size), m_sequence_count(max_sequences),
		m_entity_count(0), m_memory_manager(memory_manager), m_component_infos(_component_infos)
	{
		void* initial_allocation = memory_manager->Allocate(MemoryOfInitialization(_component_infos.size, max_chunk_count), ECS_CACHE_LINE_SIZE);

		// initializing buffers
		uintptr_t buffer = (uintptr_t)initial_allocation;
		buffer = function::align_pointer(buffer, alignof(void*));
		m_buffers.buffer = (void**)buffer;
		buffer += sizeof(void*) * max_chunk_count * _component_infos.size;

		// initializing table pointer
		buffer = function::align_pointer(buffer, alignof(StaticSequentialTable));
		m_table = (StaticSequentialTable*)buffer;

		// creating initial chunks
		for (size_t index = 0; index < initial_chunk_count; index++) {
			CreateChunk();
		}
	}

	StaticArchetypeBase::StaticArchetypeBase(
		void* initial_allocation,
		unsigned int max_chunk_count,
		unsigned int chunk_size,
		unsigned int max_sequences,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager,
		unsigned int initial_chunk_count
	) : m_max_chunk_count(max_chunk_count), m_chunk_size(chunk_size), m_sequence_count(max_sequences),
		m_entity_count(0), m_memory_manager(memory_manager)
	{
		m_component_infos.buffer = (ComponentInfo*)initial_allocation;
		m_component_infos.size = _component_infos.size;
		memcpy(m_component_infos.buffer, _component_infos.buffer, sizeof(ComponentInfo) * _component_infos.size);

		// initializing buffers
		uintptr_t buffer = (uintptr_t)initial_allocation;
		buffer += sizeof(ComponentInfo) * _component_infos.size;

		buffer = function::align_pointer(buffer, alignof(void*));
		m_buffers.buffer = (void**)buffer;
		buffer += sizeof(void*) * max_chunk_count * _component_infos.size;

		// initializing table pointer
		buffer = function::align_pointer(buffer, alignof(StaticSequentialTable));
		m_table = (StaticSequentialTable*)buffer;

		// creating initial chunks
		for (size_t index = 0; index < initial_chunk_count; index++) {
			CreateChunk();
		}
	}

	StaticArchetypeBase::StaticArchetypeBase(
		ArchetypeBaseDescriptor descriptor,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager
	) {
		StaticArchetypeBase(
			descriptor.max_chunk_count, 
			descriptor.chunk_size,
			descriptor.max_sequence_count, 
			_component_infos, 
			memory_manager, 
			descriptor.initial_chunk_count
		);
	}

	StaticArchetypeBase::StaticArchetypeBase(
		void* initial_allocation,
		ArchetypeBaseDescriptor descriptor,
		const Stream<ComponentInfo>& _component_infos,
		MemoryManager* memory_manager
	) {
		StaticArchetypeBase(
			initial_allocation,
			descriptor.max_chunk_count,
			descriptor.chunk_size,
			descriptor.max_sequence_count,
			_component_infos,
			memory_manager,
			descriptor.initial_chunk_count
		);
	}

	void StaticArchetypeBase::AddEntities(unsigned int first, unsigned int size) {
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

	void StaticArchetypeBase::AddEntities(unsigned int first, unsigned int size, Stream<unsigned int>& buffer_offset, unsigned int* sizes) {
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

	void StaticArchetypeBase::AddEntities(
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

	void StaticArchetypeBase::AddEntities(unsigned int first, unsigned int size, bool contiguous) {
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

	void StaticArchetypeBase::AddEntities(
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

	void StaticArchetypeBase::AddEntities(
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
				sequence_index = index;
				return;
			}
		}
		CreateChunk();
		offset = m_table[m_buffers.size - 1].Insert(first, size);
		chunk_index = m_buffers.size - 1;
		sequence_index = m_table[m_buffers.size - 1].GetSequenceCount() - 1;
		m_entity_count += size;
	}

	void StaticArchetypeBase::AddEntities(
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

	void StaticArchetypeBase::AddEntities_mt(
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

	void StaticArchetypeBase::AddEntities(
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
	void StaticArchetypeBase::AddEntities_mt(
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

	void StaticArchetypeBase::AddEntitiesSequential(
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

	void StaticArchetypeBase::AddEntitiesSequential_mt(
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

	void StaticArchetypeBase::AddEntitiesSingleChunk(
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

	void StaticArchetypeBase::AddEntitiesSingleChunk_mt(
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

	void StaticArchetypeBase::AddEntitiesSingleChunk(
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

	void StaticArchetypeBase::AddEntitiesSingleChunk_mt(
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

	void StaticArchetypeBase::AddEntitiesSequentialSingleChunk(
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

	void StaticArchetypeBase::AddEntitiesSequentialSingleChunk_mt(
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

	void StaticArchetypeBase::CopyKernel(
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

	void StaticArchetypeBase::CopyKernel(
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

	void StaticArchetypeBase::CopyKernelSequential(
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
						+ (chunk_offset)*m_component_infos[archetype_component_index].size),
					(void*)((uintptr_t)data[component_index - 1] + first_copy_index * m_component_infos[archetype_component_index].size),
					m_component_infos[archetype_component_index].size * (last_copy_index - first_copy_index)
				);
			}
		}
	}

	void StaticArchetypeBase::CreateChunk() {
		ECS_ASSERT(m_buffers.size < m_max_chunk_count);

		// adding 8 bytes for table buffer alignment
		void* allocation = m_memory_manager->Allocate(MemoryOf(), ECS_CACHE_LINE_SIZE);
		uintptr_t buffer = (uintptr_t)allocation;
		size_t offset = m_buffers.size * m_component_infos.size;
		for (size_t index = 0; index < m_component_infos.size; index++) {
			buffer = function::align_pointer(buffer, m_component_infos[index].alignment);
			m_buffers[offset + index] = (void*)buffer;
			buffer += m_component_infos[index].size * m_chunk_size;
		}

		// alignment to 8 for sequential table buffer
		buffer = function::align_pointer(buffer, 8);
		m_table[m_buffers.size] = StaticSequentialTable((void*)buffer, m_chunk_size, m_sequence_count);

		m_buffers.size++;
	}

	void StaticArchetypeBase::CreateChunks(unsigned int count) {
		for (size_t index = 0; index < count; index++) {
			CreateChunk();
		}
	}

	void StaticArchetypeBase::Deallocate() {
		m_memory_manager->Deallocate(m_buffers.buffer);
	}

	void StaticArchetypeBase::DeleteSequence(unsigned int chunk_index, unsigned int sequence_index) {
		m_table[chunk_index].DeleteSequence(sequence_index, true);
	}

	void StaticArchetypeBase::DestroyChunk(unsigned int index) {
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

	unsigned char StaticArchetypeBase::FindComponentIndex(unsigned char component) const {
		for (size_t index = 0; index < m_component_infos.size; index++) {
			if (m_component_infos[index].index == component) {
				return index;
			}
		}
		return unsigned char(0xFF);
	}

	void StaticArchetypeBase::FreeChunk(unsigned int chunk_index) {
		size_t sequence_count = m_table[chunk_index].GetSequenceCount();
		Sequence* sequence_buffer = m_table[chunk_index].GetSequenceBuffer();
		for (size_t index = 0; index < sequence_count; index++) {
			m_entity_count -= sequence_buffer[index].size;
		}
		m_table[chunk_index].Clear();
	}

	void StaticArchetypeBase::GetBuffers(Stream<void*>& buffers) const {
		buffers = m_buffers;
	}

	void StaticArchetypeBase::GetBuffers(Stream<void*>& buffers, const unsigned char* components) const {
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

	unsigned int StaticArchetypeBase::GetChunkCount() const {
		return m_buffers.size;
	}

	void* StaticArchetypeBase::GetComponent(unsigned int entity, unsigned char component) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetComponent(entity, component, info);
	}

	void* StaticArchetypeBase::GetComponent(unsigned int entity, unsigned char component, EntityInfo info) const {
		unsigned char component_index = FindComponentIndex(component);
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		return (void*)((uintptr_t)m_buffers[component_index + info.chunk * m_component_infos.size] +
			entity_index * m_component_infos[component_index].size);
	}

	void* StaticArchetypeBase::GetComponent(unsigned int entity, unsigned char component_index, bool is_component_index) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetComponent(entity, component_index, info, is_component_index);
	}

	void* StaticArchetypeBase::GetComponent(unsigned int entity, unsigned char component_index, EntityInfo info, bool is_component_index) const {
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		return (void*)((uintptr_t)m_buffers[component_index + info.chunk * m_component_infos.size] +
			entity_index * m_component_infos[component_index].size);
	}

	void* StaticArchetypeBase::GetComponent(unsigned int entity_index, unsigned char component_index, EntityInfo info, unsigned int entity_index_and_component_index) const {
		return (void*)((uintptr_t)m_buffers[component_index + info.chunk * m_component_infos.size] +
			entity_index * m_component_infos[component_index].size);
	}

	void StaticArchetypeBase::GetComponent(unsigned int entity, void** data) const {
		EntityInfo info = GetEntityInfo(entity);
		GetComponent(entity, info, data);
	}

	void StaticArchetypeBase::GetComponent(unsigned int entity, const unsigned char* components, void** data) const {
		EntityInfo info = GetEntityInfo(entity);
		GetComponent(entity, components, info, data);
	}

	void StaticArchetypeBase::GetComponent(unsigned int entity, EntityInfo info, void** data) const {
		size_t offset = info.chunk * m_component_infos.size;
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		for (size_t index = 0; index < m_component_infos.size; index++) {
			data[index] = (void*)((uintptr_t)m_buffers[index + offset] + entity_index * m_component_infos[index].size);
		}
	}

	void StaticArchetypeBase::GetComponent(unsigned int entity, const unsigned char* components, EntityInfo info, void** data) const {
		size_t offset = info.chunk * m_component_infos.size;
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		for (size_t index = 1; index <= components[0]; index++) {
			unsigned char component_index = FindComponentIndex(components[index]);
			data[index - 1] = (void*)((uintptr_t)m_buffers[component_index + offset] + entity_index * m_component_infos[component_index].size);
		}
	}

	unsigned char StaticArchetypeBase::GetComponentCount() const {
		return m_component_infos.size;
	}

	void StaticArchetypeBase::GetComponentInfo(Stream<ComponentInfo>& infos) const {
		infos = m_component_infos;
	}

	void StaticArchetypeBase::GetComponentOffsets(const unsigned char* components, Stream<void*>& buffers, Stream<Stream<Sequence>>& sequences) const {
		buffers.size = 0;
		for (size_t component_index = 1; component_index <= components[0]; component_index++) {
			unsigned char archetype_component_index = FindComponentIndex(components[component_index]);
			for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
				buffers[buffers.size++] = m_buffers[chunk_index * m_component_infos.size + archetype_component_index];
				m_table[chunk_index].GetSequences(sequences[chunk_index]);
			}
		}
	}

	unsigned int StaticArchetypeBase::GetEntityIndex(unsigned int entity) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetEntityIndex(entity, info);
	}

	unsigned int StaticArchetypeBase::GetEntityIndex(unsigned int entity, EntityInfo info) const {
		return m_table[info.chunk].Find(entity, info.sequence);
	}

	EntityInfo StaticArchetypeBase::GetEntityInfo(unsigned int entity) const {
		EntityInfo info;
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			unsigned int entity_sequence = m_table[chunk_index].GetSequenceOf(entity);
			if (entity_sequence != 0xFFFFFFFF) {
				info.sequence = entity_sequence;
				info.chunk = chunk_index;
				return info;
			}
		}

		// error handling - Retrieving entity info failed
		ECS_ASSERT(false, "Retrieving entity info failed!");
		return info;
	}

	unsigned int StaticArchetypeBase::GetEntityCount() const {
		return m_entity_count;
	}

	void StaticArchetypeBase::GetEntities(Stream<Stream<Sequence>>& sequences) const {
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			m_table[chunk_index].GetSequences(sequences[chunk_index]);
		}
		sequences.size = m_buffers.size;
	}

	void StaticArchetypeBase::GetSequences(Stream<Stream<Sequence>>& sequences) const {
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			m_table[chunk_index].GetSequences(sequences[chunk_index]);
		}
		sequences.size = m_buffers.size;
	}

	unsigned int StaticArchetypeBase::GetSequenceCount() const {
		unsigned int total = 0;
		for (size_t chunk_index = 0; chunk_index < m_buffers.size; chunk_index++) {
			total += m_table[chunk_index].GetSequenceCount();
		}
		return total;
	}

	unsigned int StaticArchetypeBase::GetSequenceCount(unsigned int chunk_index) const {
		return m_table[chunk_index].GetSequenceCount();
	}

	size_t StaticArchetypeBase::MemoryOf() {
		return MemoryOf(m_chunk_size, m_sequence_count, m_component_infos);
	}

	void StaticArchetypeBase::UpdateComponent(unsigned int entity, unsigned char component, const void* data) {
		EntityInfo info = GetEntityInfo(entity);
		UpdateComponent(entity, component, data, info);
	}

	void StaticArchetypeBase::UpdateComponent(unsigned int entity, unsigned char component, const void* data, EntityInfo info) {
		unsigned char component_index = FindComponentIndex(component);
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		memcpy(
			(void*)((uintptr_t)m_buffers[info.chunk * m_component_infos.size + component_index] +
				entity_index * m_component_infos[component_index].size),
			data,
			m_component_infos[component_index].size
		);
	}

	void StaticArchetypeBase::UpdateComponent(unsigned int entity, unsigned char component_index, const void* data, EntityInfo info, bool is_component_index) {
		unsigned int entity_index = m_table[info.chunk].Find(entity, info.sequence);
		memcpy(
			(void*)((uintptr_t)m_buffers[info.chunk * m_component_infos.size + component_index] +
				entity_index * m_component_infos[component_index].size),
			data,
			m_component_infos[component_index].size
		);
	}

	void StaticArchetypeBase::UpdateComponent(unsigned int entity_index, unsigned char component_index, const void* data, EntityInfo info, unsigned int component_and_entity_index) {
		memcpy(
			(void*)((uintptr_t)m_buffers[info.chunk * m_component_infos.size + component_index] +
				entity_index * m_component_infos[component_index].size),
			data,
			m_component_infos[component_index].size
		);
	}

	size_t StaticArchetypeBase::MemoryOf(unsigned int chunk_size, unsigned int max_sequences, const Stream<ComponentInfo>& component_infos) {
		size_t buffer_memory = 0;
		for (size_t index = 0; index < component_infos.size; index++) {
			buffer_memory += chunk_size * component_infos[index].size + component_infos[index].alignment;
		}
		// extra bytes for alignment
		return buffer_memory + StaticSequentialTable::MemoryOf(max_sequences) + 8;
	}

	size_t StaticArchetypeBase::MemoryOfInitialization(unsigned int component_count, unsigned int chunk_count) {
		return (sizeof(void*) * component_count + sizeof(StaticSequentialTable)) * chunk_count + 8;
	}
}
