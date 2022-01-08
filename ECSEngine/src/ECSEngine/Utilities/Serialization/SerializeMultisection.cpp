#include "ecspch.h"
#include "SerializeMultisection.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../Function.h"
#include "SerializationHelpers.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------------------------

	void SerializeMultisection(
		uintptr_t& stream,
		Stream<SerializeMultisectionData> multisections,
		Stream<void> header
	) {
		Write(stream, &header.size, sizeof(header.size));
		if (header.buffer != nullptr && header.size > 0) {
			Write(stream, header);
		}

		// Multi section count
		Write(stream, &multisections.size, sizeof(multisections.size));

		// For each multi section copy every stream associated with the byte size
		for (size_t index = 0; index < multisections.size; index++) {
			Write(stream, &multisections[index].data.size, sizeof(size_t));
			ECS_ASSERT(multisections[index].data.size > 0);
			for (size_t stream_index = 0; stream_index < multisections[index].data.size; stream_index++) {
				//ECS_ASSERT(multisections[index].data[stream_index].size > 0);
				Write(stream, &multisections[index].data[stream_index].size, sizeof(size_t));
				Write(stream, multisections[index].data[stream_index].buffer, multisections[index].data[stream_index].size);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	bool SerializeMultisection(Stream<wchar_t> file, Stream<SerializeMultisectionData> data, AllocatorPolymorphic allocator, Stream<void> header) {
		size_t serialize_data_size = SerializeMultisectionSize(data, header);

		return SerializeWriteFile(file, allocator, serialize_data_size, [=](uintptr_t& buffer) {
			SerializeMultisection(buffer, data, header);
		});
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t SerializeMultisectionSize(Stream<SerializeMultisectionData> data, Stream<void> header) {
		size_t total_memory = sizeof(size_t);
		if (header.size > 0) {
			total_memory += sizeof(size_t) + header.size;
		}

		for (size_t index = 0; index < data.size; index++) {
			total_memory += sizeof(size_t);

			for (size_t stream_index = 0; stream_index < data[index].data.size; stream_index++) {
				total_memory += sizeof(size_t);
				total_memory += data[index].data[stream_index].size;
			}
		}

		return total_memory;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	size_t DeserializeMultisection(
		Stream<wchar_t> file,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header
	) {
		return DeserializeReadFile(file, allocator, [&](uintptr_t& buffer) {
			return DeserializeMultisection(buffer, data, memory_pool, header);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		CapacityStream<void>* header
	) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			if (header_size > header->capacity) {
				return -1;
			}
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		size_t initial_memory_pool_size = memory_pool.size;
		bool success = true;
		for (size_t index = 0; index < multisection_count && success; index++) {
			size_t data_stream_count = 0;
			Read(stream, &data_stream_count, sizeof(size_t));
			ECS_ASSERT(data_stream_count > 0);
			if (data_stream_count == 0) {
				success = false;
			}
			ECS_ASSERT(data[data.size].data.size <= data_stream_count);
			if (data[data.size].data.size > data_stream_count) {
				success = false;
			}

			for (size_t stream_index = 0; stream_index < data[index].data.size; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				ECS_ASSERT(data_size > 0);
				if (data_size == 0) {
					success = false;
				}
				data[data.size].data[stream_index].buffer = function::OffsetPointer(memory_pool);
				data[data.size].data[stream_index].size = data_size;
				Read(stream, memory_pool, data_size);
			}
			data.size++;
			for (size_t stream_index = data[index].data.size; stream_index < data_stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				Ignore(stream, data_size);
			}
		}

		data.AssertCapacity();
		return success ? memory_pool.size - initial_memory_pool_size : -1;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		Stream<wchar_t> file, 
		CapacityStream<SerializeMultisectionData>& data,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header
	)
	{
		return DeserializeReadFile(file, allocator, [&](uintptr_t& buffer) {
			return DeserializeMultisection(buffer, data, header);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		Stream<wchar_t> file,
		CapacityStream<SerializeMultisectionData>& data, 
		AllocatorPolymorphic file_allocator, 
		AllocatorPolymorphic buffer_allocator, 
		CapacityStream<void>* header
	)
	{
		return DeserializeReadFile(file, file_allocator, [&](uintptr_t& buffer) {
			return DeserializeMultisection(buffer, data, buffer_allocator, header);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>* header
	)
	{
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			if (header_size > header->capacity) {
				return -1;
			}
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		size_t memory_size = 0;
		size_t index = 0;
		bool success = true;
		for (; index < multisection_count && success; index++) {
			size_t data_stream_count = 0;
			Read(stream, &data_stream_count, sizeof(size_t));
			ECS_ASSERT(data_stream_count > 0 && data_stream_count == data[index].data.size);
			if (data_stream_count == 0 || data_stream_count != data[index].data.size) {
				success = false;
			}
			for (size_t stream_index = 0; stream_index < data[index].data.size && success; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				if (data_size <= data[data.size].data[stream_index].size) {
					Read(stream, data[data.size].data[stream_index].buffer, data_size);
				}
				else {
					success = false;
				}
				data[data.size].data[stream_index].size = data_size;
				memory_size += data_size;
			}
			data.size++;
		}

		data.AssertCapacity();
		return success ? memory_size : -1;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		uintptr_t& stream, 
		CapacityStream<SerializeMultisectionData>& data,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header
	)
	{
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			if (header_size > header->capacity) {
				return -1;
			}
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		size_t memory_size = 0;
		size_t index = 0;
		bool success = true;

		for (; index < multisection_count && success; index++) {
			size_t data_stream_count = 0;
			Read(stream, &data_stream_count, sizeof(size_t));
			ECS_ASSERT(data_stream_count > 0 && data_stream_count == data[index].data.size);
			if (data_stream_count == 0 || data_stream_count != data[index].data.size) {
				success = false;
				// Deallocate all the current allocations
				for (size_t subindex = 0; subindex < index; subindex++) {
					for (size_t stream_index = 0; stream_index < data[subindex].data.size; stream_index++) {
						DeallocateEx(allocator, data[subindex].data[stream_index].buffer);
					}
				}
			}
			for (size_t stream_index = 0; stream_index < data[index].data.size && success; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));

				data[data.size].data[stream_index].buffer = AllocateEx(allocator, data_size);
				Read(stream, data[data.size].data[stream_index].buffer, data_size);
				data[data.size].data[stream_index].size = data_size;
				memory_size += data_size;
			}
			data.size++;
		}

		data.AssertCapacity();
		return success ? memory_size : -1;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionCount(uintptr_t stream, size_t header_size)
	{
		return *(size_t*)function::OffsetPointer((void*)stream, header_size + sizeof(header_size));
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionSize(uintptr_t stream) {
		size_t total_memory = 0;

		size_t _header_size = 0;
		Read(stream, &_header_size, sizeof(_header_size));
		Ignore(stream, _header_size);

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));
		for (size_t index = 0; index < multisection_count; index++) {
			size_t stream_count = 0;
			Read(stream, &stream_count, sizeof(size_t));

			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				total_memory += data_size;
				Ignore(stream, data_size);
			}
		}

		return total_memory;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionSize(uintptr_t stream, Stream<size_t> multisection_data_stream_count) {
		size_t total_memory = 0;

		size_t _header_size = 0;
		Read(stream, &_header_size, sizeof(_header_size));
		Ignore(stream, _header_size);

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));
		for (size_t index = 0; index < multisection_count; index++) {
			size_t stream_count = 0;
			Read(stream, &stream_count, sizeof(size_t));

			ECS_ASSERT(stream_count >= multisection_data_stream_count[index]);
			for (size_t stream_index = 0; stream_index < multisection_data_stream_count[index]; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				total_memory += data_size;
				Ignore(stream, data_size);
			}
			for (size_t stream_index = multisection_data_stream_count[index]; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				Ignore(stream, data_size);
			}
		}

		return total_memory;
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionPerSectionSize(uintptr_t stream, CapacityStream<size_t>& sizes)
	{
		size_t _header_size = 0;
		Read(stream, &_header_size, sizeof(_header_size));
		Ignore(stream, _header_size);

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));
		ECS_ASSERT(multisection_count <= sizes.capacity);

		for (size_t index = 0; index < multisection_count; index++) {
			size_t stream_count = 0;
			Read(stream, &stream_count, sizeof(size_t));

			sizes[index] = 0;
			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				Ignore(stream, data_size);
				sizes[index] += data_size;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionPerSectionSize(uintptr_t stream, CapacityStream<size_t>& sizes, Stream<size_t> multisection_data_stream_count)
	{
		size_t _header_size = 0;
		Read(stream, &_header_size, sizeof(_header_size));
		Ignore(stream, _header_size);

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));
		ECS_ASSERT(multisection_count <= sizes.capacity);

		for (size_t index = 0; index < multisection_count; index++) {
			size_t stream_count = 0;
			Read(stream, &stream_count, sizeof(size_t));

			sizes[index] = 0;
			for (size_t stream_index = 0; stream_index < multisection_data_stream_count[index]; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				sizes[index] += data_size;
				Ignore(stream, data_size);
			}
			for (size_t stream_index = multisection_data_stream_count[index]; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				Ignore(stream, data_size);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionStreamCount(uintptr_t stream, CapacityStream<size_t>& multisection_stream_count)
	{
		size_t _header_size = 0;
		Read(stream, &_header_size, sizeof(_header_size));
		Ignore(stream, _header_size);

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		ECS_ASSERT(multisection_stream_count.capacity >= multisection_count);
		multisection_stream_count.size = multisection_count;

		for (size_t index = 0; index < multisection_count; index++) {
			size_t stream_count = 0;
			Read(stream, &stream_count, sizeof(size_t));

			multisection_stream_count[index] = stream_count;
			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				Ignore(stream, data_size);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionStreamSizes(uintptr_t stream, CapacityStream<Stream<size_t>>& multisection_sizes)
	{
		size_t _header_size = 0;
		Read(stream, &_header_size, sizeof(_header_size));
		Ignore(stream, _header_size);

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		ECS_ASSERT(multisection_sizes.capacity >= multisection_count);
		multisection_sizes.size = multisection_count;

		for (size_t index = 0; index < multisection_count; index++) {
			size_t stream_count = 0;
			Read(stream, &stream_count, sizeof(size_t));

			ECS_ASSERT(stream_count >= multisection_sizes[index].size);
			for (size_t stream_index = 0; stream_index < multisection_sizes[index].size; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				multisection_sizes[index][stream_index] = data_size;
				Ignore(stream, data_size);
			}
			for (size_t stream_index = multisection_sizes[index].size; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				Ignore(stream, data_size);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionHeaderSize(uintptr_t stream) {
		return *(size_t*)stream;
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionHeader(uintptr_t stream, CapacityStream<void>& header) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		Read(stream, header, header_size);
	}

	// ---------------------------------------------------------------------------------------------------------------

}