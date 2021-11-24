#include "ecspch.h"
#include "SerializeMultisection.h"
#include "Serialization.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../Function.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------------------------

	template<typename StreamType>
	bool SerializeMultisectionInternal(
		StreamType& stream,
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

		if constexpr (std::is_same_v<StreamType, std::ofstream>) {
			return stream.good();
		}

		return true;

	}

	// ------------------------------------------------------------------------------------------------------------------------------

	template<typename StreamType>
	size_t DeserializeMultisectionInternal(
		StreamType& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		CapacityStream<void>* header,
		bool* ECS_RESTRICT success_status = nullptr
	) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header_size <= header->capacity);
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		size_t initial_memory_pool_size = memory_pool.size;
		for (size_t index = 0; index < multisection_count; index++) {
			size_t data_stream_count = 0;
			Read(stream, &data_stream_count, sizeof(size_t));
			ECS_ASSERT(data_stream_count > 0);
			ECS_ASSERT(data[data.size].data.size <= data_stream_count);

			for (size_t stream_index = 0; stream_index < data[index].data.size; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				ECS_ASSERT(data_size > 0);
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

		if constexpr (std::is_same_v<StreamType, std::ifstream>) {
			if (success_status != nullptr) {
				*success_status = stream.good();
			}
		}

		return memory_pool.size - initial_memory_pool_size;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	template<typename StreamType>
	size_t DeserializeMultisectionInternal(
		StreamType& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>* header,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header_size <= header->capacity);
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		size_t memory_size = 0;
		for (size_t index = 0; index < multisection_count; index++) {
			size_t data_stream_count = 0;
			Read(stream, &data_stream_count, sizeof(size_t));
			ECS_ASSERT(data_stream_count > 0);
			ECS_ASSERT(data[data.size].data.size <= data_stream_count);

			for (size_t stream_index = 0; stream_index < data[index].data.size; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));

				ECS_ASSERT(data_size > 0);
				if (data_size > data[data.size].data[stream_index].size) {
					if (faulty_index != nullptr) {
						*faulty_index = data.size;
					}
					return memory_size;
				}
				Read(stream, data[data.size].data[stream_index].buffer, data_size);
				data[data.size].data[stream_index].size = data_size;
				memory_size += data_size;
			}
			data.size++;
			for (size_t stream_index = data[index].data.size; stream_index < data_stream_count; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));
				Ignore(stream, data_size);
			}
		}

		data.AssertCapacity();

		if constexpr (std::is_same_v<StreamType, std::ifstream>) {
			if (faulty_index != nullptr) {
				if (!stream.good()) {
					*faulty_index = data.size;
				}
			}
		}

		return memory_size;
	}

	template<typename StreamType>
	size_t DeserializeMultisectionInternal(
		StreamType& ECS_RESTRICT stream,
		CapacityStream<SerializeMultisectionData>& ECS_RESTRICT data,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type,
		CapacityStream<void>* header,
		bool* ECS_RESTRICT success
	) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header_size <= header->capacity);
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t multisection_count = 0;
		Read(stream, &multisection_count, sizeof(size_t));

		size_t memory_size = 0;
		for (size_t index = 0; index < multisection_count; index++) {
			size_t data_stream_count = 0;
			Read(stream, &data_stream_count, sizeof(size_t));
			ECS_ASSERT(data_stream_count > 0 && data_stream_count == data[index].data.size);
			for (size_t stream_index = 0; stream_index < data[index].data.size; stream_index++) {
				size_t data_size = 0;
				Read(stream, &data_size, sizeof(size_t));

				//ECS_ASSERT(data_size > 0);
				data[data.size].data[stream_index].buffer = Allocate(allocator, allocator_type, data_size);
				Read(stream, data[data.size].data[stream_index].buffer, data_size);
				data[data.size].data[stream_index].size = data_size;
				memory_size += data_size;
			}
			data.size++;
		}

		data.AssertCapacity();
		if constexpr (std::is_same_v<StreamType, std::ifstream>) {
			if (success != nullptr) {
				*success = stream.good();
			}
		}
		return memory_size;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	bool SerializeMultisection(std::ofstream& stream, Stream<SerializeMultisectionData> data, Stream<void> header) {
		return SerializeMultisectionInternal(stream, data, header);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	void SerializeMultisection(CapacityStream<void>& stream, Stream<SerializeMultisectionData> data, Stream<void> header) {
		SerializeMultisectionInternal(stream, data, header);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t SerializeMultisectionSize(Stream<SerializeMultisectionData> data) {
		size_t total_memory = sizeof(size_t);

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
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		CapacityStream<void>* ECS_RESTRICT header,
		bool* success_status
	) {
		return DeserializeMultisectionInternal(stream, data, memory_pool, header, success_status);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	size_t DeserializeMultisection(
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>* header,
		unsigned int* faulty_index
	) {
		return DeserializeMultisectionInternal(stream, data, header, faulty_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		CapacityStream<void>* ECS_RESTRICT header
	) {
		return DeserializeMultisectionInternal(stream, data, memory_pool, header, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>* header,
		unsigned int* faulty_index
	) {
		return DeserializeMultisectionInternal(stream, data, header, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		std::ifstream& stream, 
		CapacityStream<SerializeMultisectionData>& data,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type,
		CapacityStream<void>* header,
		bool* success
	)
	{
		return DeserializeMultisectionInternal(stream, data, allocator, allocator_type, header, success);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		uintptr_t& stream, 
		CapacityStream<SerializeMultisectionData>& data,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type, 
		CapacityStream<void>* header
	)
	{
		return DeserializeMultisectionInternal(stream, data, allocator, allocator_type, header, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionCount(uintptr_t stream, size_t header_size)
	{
		return *(size_t*)function::OffsetPointer((void*)stream, header_size + sizeof(header_size));
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionCount(std::ifstream& stream, size_t header_size)
	{
		size_t count = 0;
		Ignore(stream, header_size + sizeof(header_size));
		stream.read((char*)&count, sizeof(count));
		stream.seekg(0, std::ios_base::beg);
		ECS_ASSERT(stream.good());
		return count;
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

	void DeserializeMultisectionStreamCount(std::ifstream& stream, CapacityStream<size_t>& multisection_stream_count)
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

		stream.seekg(std::ios::beg);
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

	void DeserializeMultisectionStreamSizes(std::ifstream& stream, CapacityStream<Stream<size_t>>& multisection_sizes)
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

		stream.seekg(std::ios::beg);
	}

	// ---------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionHeaderSize(uintptr_t stream) {
		return *(size_t*)stream;
	}

	// ---------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionHeaderSize(std::ifstream& stream) {
		size_t count = 0;
		stream.read((char*)&count, sizeof(count));
		stream.seekg(0, std::ios_base::beg);
		ECS_ASSERT(stream.good());
		return count;
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionHeader(uintptr_t stream, CapacityStream<void>& header) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		Read(stream, header, header_size);
	}

	// ---------------------------------------------------------------------------------------------------------------

	bool DeserializeMultisectionHeader(std::ifstream& stream, CapacityStream<void>& header) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		Read(stream, header, header_size);
		stream.seekg(0, std::ios_base::beg);
		return stream.good();
	}

	// ---------------------------------------------------------------------------------------------------------------

}