#include "ecspch.h"
#include "SerializeMultisection.h"
#include "../../Function.h"
#include "../SerializationHelpers.h"
#include "../../../Containers/HashTable.h"

namespace ECSEngine {

	const size_t MAX_ACCELERATION_TABLE_MULTISECTIONS = ECS_KB * 16;

	using AccelerationTable = HashTableDefault<SerializeMultisectionData*>;

	// Add 32 to the multisection size so as to avoid breaking the load factor of the hash table for values like 29 -> 32
#define CREATE_ACCELERATION_TABLE(multisections) ECS_ASSERT(multisections.size < MAX_ACCELERATION_TABLE_MULTISECTIONS); \
									size_t table_capacity = function::PowerOfTwoGreater(multisections.size + 32); \
									size_t table_size = AccelerationTable::MemoryOf(table_capacity); \
									void* table_allocation = ECS_STACK_ALLOC(table_size); \
									AccelerationTable table(table_allocation, table_capacity); \
\
									for (size_t index = 0; index < multisections.size; index++) { \
										multisections[index].data = { nullptr, 0 }; \
										table.Insert(multisections.buffer + index, multisections[index].name); \
									}

	// ------------------------------------------------------------------------------------------------------------------------------

	void SerializeMultisection(
		Stream<SerializeMultisectionData> multisections,
		uintptr_t& stream,
		Stream<void> header
	) {
		Write<true>(&stream, &header.size, sizeof(header.size));
		if (header.buffer != nullptr && header.size > 0) {
			Write<true>(&stream, header.buffer, header.size);
		}

		// Multi section count
		Write<true>(&stream, &multisections.size, sizeof(multisections.size));

		// For each multisection copy every stream associated with the byte size and also the name if one exists
		for (size_t index = 0; index < multisections.size; index++) {
			// The name first - if it exists
			if (multisections[index].name == nullptr) {
				unsigned short zero = 0;
				Write<true>(&stream, &zero, sizeof(zero));
			}
			else {
				unsigned short name_size = (unsigned short)strlen(multisections[index].name);
				Write<true>(&stream, &name_size, sizeof(name_size));
				Write<true>(&stream, multisections[index].name, name_size + 1);
			}

			Write<true>(&stream, &multisections[index].data.size, sizeof(size_t));
			ECS_ASSERT(multisections[index].data.size > 0);
			for (size_t stream_index = 0; stream_index < multisections[index].data.size; stream_index++) {
				//ECS_ASSERT(multisections[index].data[stream_index].size > 0);
				Write<true>(&stream, &multisections[index].data[stream_index].size, sizeof(size_t));
				Write<true>(&stream, multisections[index].data[stream_index].buffer, multisections[index].data[stream_index].size);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	bool SerializeMultisection(Stream<SerializeMultisectionData> data, Stream<wchar_t> file, AllocatorPolymorphic allocator, Stream<void> header) {
		size_t serialize_data_size = SerializeMultisectionSize(data, header);

		return SerializeWriteFile(file, allocator, serialize_data_size, true, [=](uintptr_t& buffer) {
			SerializeMultisection(data, buffer, header);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t SerializeMultisectionSize(Stream<SerializeMultisectionData> data, Stream<void> header) {
		// The header size and the multisection count
		size_t total_memory = sizeof(size_t) * 2;
		if (header.size > 0) {
			total_memory += header.size;
		}

		for (size_t index = 0; index < data.size; index++) {
			// The name size must always be specified
			total_memory += sizeof(size_t) + sizeof(unsigned short);
			if (data[index].name != nullptr) {
				total_memory += strlen(data[index].name) + 1;
			}

			for (size_t stream_index = 0; stream_index < data[index].data.size; stream_index++) {
				total_memory += sizeof(size_t);
				total_memory += data[index].data[stream_index].size;
			}
		}

		return total_memory;
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	void* DeserializeMultisection(
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header
	)
	{
		return DeserializeReadBinaryFileAndKeepMemory(file, allocator, [&](uintptr_t& buffer) {
			DeserializeMultisection(data, memory_pool, buffer, header);
		});
	}

	// ---------------------------------------------------------------------------------------------------------------

	void* DeserializeMultisectionWithMatch(
		Stream<SerializeMultisectionData> data,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header
	) {
		return DeserializeReadBinaryFileAndKeepMemory(file, allocator, [&](uintptr_t& buffer) {
			DeserializeMultisectionWithMatch(data, memory_pool, buffer, header);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisection(
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		uintptr_t& stream,
		CapacityStream<void>* header
	) {
		size_t header_size = 0;
		Read<true>(&stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			if (header_size > header->capacity) {
				return -1;
			}
			Read<true>(&stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(&stream, header_size);
		}

		size_t multisection_count = 0;
		Read<true>(&stream, &multisection_count, sizeof(size_t));

		size_t total_memory = 0;
		for (size_t index = 0; index < multisection_count; index++) {
			const char* name = nullptr;
			// The name first
			unsigned short name_size = 0;
			Read<true>(&stream, &name_size, sizeof(name_size));
			if (name_size > 0) {
				name = (const char*)stream;
				Ignore(&stream, name_size + 1);
			}

			size_t data_stream_count = 0;
			Read<true>(&stream, &data_stream_count, sizeof(size_t));

			// If the data stream count is 0, just skip to the next field
			if (data_stream_count == 0) {
				continue;
			}
			
			Stream<void>* allocated_stream = (Stream<void>*)function::OffsetPointer(memory_pool);
			memory_pool.size += sizeof(Stream<void>) * data_stream_count;
			ECS_ASSERT(memory_pool.size <= memory_pool.capacity);

			for (size_t stream_index = 0; stream_index < data_stream_count; stream_index++) {
				size_t data_size = 0;
				Read<true>(&stream, &data_size, sizeof(data_size));
				if (data_size == 0) {
					allocated_stream[stream_index] = { nullptr, 0 };
				}
				else {
					allocated_stream[stream_index] = { (void*)stream, data_size };
					total_memory += data_size;
					Ignore(&stream, data_size);
				}
			}
		
			data.Add({ { allocated_stream, data_stream_count }, name });
		}

		data.AssertCapacity();
		return total_memory;
	}

	// ---------------------------------------------------------------------------------------------------------------

	size_t DeserializeMultisectionWithMatch(
		Stream<SerializeMultisectionData> data,
		CapacityStream<void>& memory_pool,
		uintptr_t& stream,
		CapacityStream<void>* header
	) {
		CREATE_ACCELERATION_TABLE(data);

		size_t header_size = 0;
		Read<true>(&stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			if (header_size > header->capacity) {
				return -1;
			}
			Read<true>(&stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(&stream, header_size);
		}

		size_t multisection_count = 0;
		Read<true>(&stream, &multisection_count, sizeof(size_t));

		size_t total_memory = 0;
		for (size_t index = 0; index < multisection_count; index++) {
			const char* name = nullptr;
			// The name first
			unsigned short name_size = 0;
			Read<true>(&stream, &name_size, sizeof(name_size));
			if (name_size > 0) {
				name = (const char*)stream;
				Ignore(&stream, name_size + 1);
			}

			size_t data_stream_count = 0;
			Read<true>(&stream, &data_stream_count, sizeof(size_t));

			// If the data stream count is 0, just skip to the next field
			if (data_stream_count == 0) {
				continue;
			}

			Stream<void>* allocated_stream = (Stream<void>*)function::OffsetPointer(memory_pool);
			memory_pool.size += sizeof(Stream<void>) * data_stream_count;

			for (size_t stream_index = 0; stream_index < data_stream_count; stream_index++) {
				size_t data_size = 0;
				Read<true>(&stream, &data_size, sizeof(data_size));
				if (data_size == 0) {
					allocated_stream[stream_index] = { nullptr, 0 };
				}
				else {
					allocated_stream[stream_index] = { (void*)stream, data_size };
					total_memory += data_size;
					Ignore(&stream, data_size);
				}
			}

			if (name != nullptr) {
				SerializeMultisectionData* section;
				if (table.TryGetValue(name, section)) {
					ECS_ASSERT(memory_pool.size <= memory_pool.capacity);
					section->data = { allocated_stream, data_stream_count };
				}
				else {
					memory_pool.size -= sizeof(Stream<void>) * data_stream_count;
				}
			}
			else {
				memory_pool.size -= sizeof(Stream<void>) * data_stream_count;
			}
		}

		return total_memory;
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
		Read<true>(&stream, &_header_size, sizeof(_header_size));
		Ignore(&stream, _header_size);

		size_t multisection_count = 0;
		Read<true>(&stream, &multisection_count, sizeof(size_t));
		for (size_t index = 0; index < multisection_count; index++) {
			unsigned short name_size = 0;
			Read<true>(&stream, &name_size, sizeof(name_size));
			Ignore(&stream, name_size);

			size_t stream_count = 0;
			Read<true>(&stream, &stream_count, sizeof(size_t));

			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read<true>(&stream, &data_size, sizeof(size_t));
				total_memory += data_size;
				Ignore(&stream, data_size);
			}
		}

		return total_memory;
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionPerSectionSize(uintptr_t stream, CapacityStream<SerializeMultisectionData>& multisections)
	{
		size_t _header_size = 0;
		Read<true>(&stream, &_header_size, sizeof(_header_size));
		Ignore(&stream, _header_size);

		size_t multisection_count = 0;
		Read<true>(&stream, &multisection_count, sizeof(size_t));

		for (size_t index = 0; index < multisection_count; index++) {
			const char* name = nullptr;
			unsigned short name_size = 0;
			Read<true>(&stream, &name_size, sizeof(name_size));
			if (name_size > 0) {
				name = (const char*)stream;
			}
			Ignore(&stream, name_size);

			size_t stream_count = 0;
			Read<true>(&stream, &stream_count, sizeof(size_t));

			if (stream_count == 0) {
				continue;
			}

			size_t multisection_size = 0;
			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read<true>(&stream, &data_size, sizeof(size_t));
				Ignore(&stream, data_size);
				multisection_size += data_size;
			}
			multisections[multisections.size].data.size = multisection_size;
			multisections.size++;
		}
		multisections.AssertCapacity();
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionPerSectionSizeWithMatch(uintptr_t stream, Stream<SerializeMultisectionData> multisections)
	{
		CREATE_ACCELERATION_TABLE(multisections);

		size_t _header_size = 0;
		Read<true>(&stream, &_header_size, sizeof(_header_size));
		Ignore(&stream, _header_size);

		size_t multisection_count = 0;
		Read<true>(&stream, &multisection_count, sizeof(size_t));

		for (size_t index = 0; index < multisection_count; index++) {
			const char* name = nullptr;
			unsigned short name_size = 0;
			Read<true>(&stream, &name_size, sizeof(name_size));
			if (name_size > 0) {
				name = (const char*)stream;
			}
			Ignore(&stream, name_size);

			size_t stream_count = 0;
			Read<true>(&stream, &stream_count, sizeof(size_t));

			if (stream_count == 0) {
				continue;
			}

			size_t multisection_size = 0;
			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read<true>(&stream, &data_size, sizeof(size_t));
				Ignore(&stream, data_size);
				multisection_size += data_size;
			}

			if (name != nullptr) {
				SerializeMultisectionData* section = nullptr;
				if (table.TryGetValue(name, section)) {
					section->data.size = multisection_size;
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionStreamCount(uintptr_t stream, CapacityStream<size_t>& multisection_stream_count)
	{
		size_t _header_size = 0;
		Read<true>(&stream, &_header_size, sizeof(_header_size));
		Ignore(&stream, _header_size);

		size_t multisection_count = 0;
		Read<true>(&stream, &multisection_count, sizeof(size_t));

		for (size_t index = 0; index < multisection_count; index++) {
			unsigned short name_size = 0;
			Read<true>(&stream, &name_size, sizeof(name_size));
			Ignore(&stream, name_size);

			size_t stream_count = 0;
			Read<true>(&stream, &stream_count, sizeof(size_t));

			multisection_stream_count.Add(stream_count);
			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read<true>(&stream, &data_size, sizeof(size_t));
				Ignore(&stream, data_size);
			}
		}

		multisection_stream_count.AssertCapacity();
	}

	// ---------------------------------------------------------------------------------------------------------------

	void DeserializeMultisectionStreamCountWithMatch(uintptr_t stream, Stream<SerializeMultisectionData> multisections) {
		CREATE_ACCELERATION_TABLE(multisections);

		size_t _header_size = 0;
		Read<true>(&stream, &_header_size, sizeof(_header_size));
		Ignore(&stream, _header_size);

		size_t multisection_count = 0;
		Read<true>(&stream, &multisection_count, sizeof(size_t));

		for (size_t index = 0; index < multisection_count; index++) {
			const char* name = nullptr;
			unsigned short name_size = 0;
			Read<true>(&stream, &name_size, sizeof(name_size));
			if (name_size > 0) {
				name = (const char*)stream;
			}
			Ignore(&stream, name_size);

			size_t stream_count = 0;
			Read<true>(&stream, &stream_count, sizeof(size_t));

			if (name != nullptr) {
				SerializeMultisectionData* section;
				if (table.TryGetValue(name, section)) {
					section->data.size = stream_count;
				}
			}
			
			for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
				size_t data_size = 0;
				Read<true>(&stream, &data_size, sizeof(size_t));
				Ignore(&stream, data_size);
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
		Read<true>(&stream, &header_size, sizeof(header_size));
		ECS_ASSERT(header_size <= header.capacity);
		Read<true>(&stream, header.buffer, header_size);
		header.size = header_size;
	}

	// ---------------------------------------------------------------------------------------------------------------

}