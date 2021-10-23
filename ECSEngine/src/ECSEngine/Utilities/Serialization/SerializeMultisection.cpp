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
		Stream<SerializeMultisectionData> multisections
	) {
		// Multi section count
		Write(stream, &multisections.size, sizeof(size_t));

		// For each multi section copy every stream associated with the byte size
		for (size_t index = 0; index < multisections.size; index++) {
			Write(stream, &multisections[index].data.size, sizeof(size_t));
			ECS_ASSERT(multisections[index].data.size > 0);
			for (size_t stream_index = 0; stream_index < multisections[index].data.size; stream_index++) {
				ECS_ASSERT(multisections[index].data[stream_index].size > 0);
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
		bool* ECS_RESTRICT success_status = nullptr
	) {
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
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	) {
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
		bool* ECS_RESTRICT success
	) {
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

				ECS_ASSERT(data_size > 0);
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

	bool SerializeMultisection(std::ofstream& stream, Stream<SerializeMultisectionData> data) {
		return SerializeMultisectionInternal(stream, data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	void SerializeMultisection(CapacityStream<void>& stream, Stream<SerializeMultisectionData> data) {
		SerializeMultisectionInternal(stream, data);
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
		std::ifstream& ECS_RESTRICT stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		bool* ECS_RESTRICT success_status
	) {
		return DeserializeMultisectionInternal(stream, data, memory_pool, success_status);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	size_t DeserializeMultisection(
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		return DeserializeMultisectionInternal(stream, data, faulty_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool
	) {
		return DeserializeMultisectionInternal(stream, data, memory_pool, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		return DeserializeMultisectionInternal(stream, data, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(std::ifstream& stream, CapacityStream<SerializeMultisectionData>& data, void* allocator, AllocatorType allocator_type, bool* success)
	{
		return DeserializeMultisectionInternal(stream, data, allocator, allocator_type, success);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(uintptr_t& stream, CapacityStream<SerializeMultisectionData>& data, void* allocator, AllocatorType allocator_type)
	{
		return DeserializeMultisectionInternal(stream, data, allocator, allocator_type, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionCount(uintptr_t stream)
	{
		return *(size_t*)stream;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionCount(std::ifstream& stream)
	{
		size_t count = 0;
		stream.read((char*)&count, sizeof(count));
		stream.seekg(-8, std::ios_base::cur);
		ECS_ASSERT(stream.good());
		return count;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionSize(uintptr_t stream) {
		size_t total_memory = 0;

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

}