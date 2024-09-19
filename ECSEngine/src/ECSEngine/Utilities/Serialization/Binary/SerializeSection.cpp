#include "ecspch.h"
#include "SerializeSection.h"
#include "../SerializationHelpers.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------

	bool SerializeSection(Stream<SerializeSectionData> data, Stream<wchar_t> file, AllocatorPolymorphic allocator, Stream<void> header) {
		size_t serialize_size = SerializeSectionSize(data);
		return SerializeWriteFile(file, allocator, serialize_size, true, [=](uintptr_t& buffer) {
			SerializeSection(data, buffer, header);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------

	void SerializeSection(Stream<SerializeSectionData> data, uintptr_t& stream, Stream<void> header) {
		bool success = true;

		Write<true>(&stream, &header.size, sizeof(header.size));
		if (header.buffer != nullptr && header.size > 0) {
			Write<true>(&stream, header.buffer, header.size);
		}

		// Write the section count
		Write<true>(&stream, &data.size, sizeof(size_t));

		// For each section write the name size followed by the null terminated name
		// Then write the byte count and the data
		for (size_t index = 0; index < data.size; index++) {
			unsigned short name_size = (unsigned short)strlen(data[index].name);
			Write<true>(&stream, &name_size, sizeof(unsigned short));
			Write<true>(&stream, data[index].name, (name_size + 1) * sizeof(char));
			Write<true>(&stream, &data[index].data.size, sizeof(size_t));
			Write<true>(&stream, data[index].data.buffer, data[index].data.size);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t SerializeSectionSize(Stream<SerializeSectionData> data) {
		// For the field count
		size_t total_size = sizeof(size_t);

		for (size_t index = 0; index < data.size; index++) {
			total_size += sizeof(unsigned short) + sizeof(char) * (strlen(data[index].name) + 1) + sizeof(size_t) + data[index].data.size;
		}

		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	void* DeserializeSection(CapacityStream<SerializeSectionData>& sections, Stream<wchar_t> file, AllocatorPolymorphic allocator, CapacityStream<void>* header)
	{
		return DeserializeReadBinaryFileAndKeepMemory(file, allocator, [&](uintptr_t& buffer) {
			DeserializeSection(sections, buffer, header);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------

	void* DeserializeSectionWithMatch(Stream<SerializeSectionData> sections, Stream<wchar_t> file, AllocatorPolymorphic allocator, CapacityStream<void>* header)
	{
		return DeserializeReadBinaryFileAndKeepMemory(file, allocator, [=](uintptr_t& buffer) {
			DeserializeSectionWithMatch(sections, buffer, header);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(CapacityStream<SerializeSectionData>& sections, uintptr_t& stream, CapacityStream<void>* header)
	{
		size_t header_size;
		Read<true>(&stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header->capacity >= header_size);
			Read<true>(&stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(&stream, header_size);
		}

		size_t section_count;
		Read<true>(&stream, &section_count, sizeof(size_t));
		if (section_count == 0) {
			return -1;
		}

		size_t total_size = 0;

		for (size_t index = 0; index < section_count; index++) {
			SerializeSectionData section_data;

			unsigned short name_size;
			Read<true>(&stream, &name_size, sizeof(unsigned short));
			section_data.name = (const char*)stream;
			Ignore(&stream, sizeof(char) * (name_size + 1));

			size_t data_size;
			Read<true>(&stream, &data_size, sizeof(size_t));
			section_data.data.buffer = (void*)stream;
			section_data.data.size = data_size;
			Ignore(&stream, data_size);

			sections.AddAssert(section_data);
		}

		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionWithMatch(Stream<SerializeSectionData> sections, uintptr_t& stream, CapacityStream<void>* header)
	{
		size_t header_size = 0;
		Read<true>(&stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header->capacity >= header_size);
			Read<true>(&stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(&stream, header_size);
		}

		size_t section_count;
		Read<true>(&stream, &section_count, sizeof(section_count));
		if (section_count == 0) {
			return -1;
		}

		size_t total_size = 0;
		ECS_STACK_CAPACITY_STREAM(char, temp_name, 512);

		// Prerecord section names
		unsigned short* name_sizes = (unsigned short*)ECS_STACK_ALLOC(sections.size * sizeof(unsigned short));
		for (size_t index = 0; index < sections.size; index++) {
			name_sizes[index] = (unsigned short)strlen(sections[index].name);
		}

		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read<true>(&stream, &name_size, sizeof(unsigned short));

			Read<true>(&stream, temp_name.buffer, (sizeof(char) + 1) * name_size);
			unsigned int section_index = -1;
			for (size_t subindex = 0; subindex < sections.size && section_index == -1; index++) {
				if (Stream<char>(temp_name.buffer, name_size) == Stream<char>(sections[subindex].name, name_sizes[subindex])) {
					section_index = subindex;
				}
			}

			size_t data_size;
			Read<true>(&stream, &data_size, sizeof(data_size));
			if (section_index != -1) {
				sections[section_index].data.buffer = (void*)stream;
				sections[section_index].data.size = data_size;
				total_size += data_size;
			}
			else {
				sections[section_index].data = { nullptr, 0 };
			}
			
			Ignore(&stream, data_size);
		}

		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionCount(uintptr_t stream, size_t header_size)
	{
		return *(size_t*)OffsetPointer((void*)stream, header_size);
	}

	// -------------------------------------------------------------------------------------------------------------------
	
	size_t DeserializeSectionSize(uintptr_t data, Stream<SerializeSectionData> serialize_data) {
		unsigned short* section_name_sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * serialize_data.size);
		for (size_t index = 0; index < serialize_data.size; index++) {
			section_name_sizes[index] = (unsigned short)strlen(serialize_data[index].name);
		}

		size_t _header_size = 0;
		Read<true>(&data, &_header_size, sizeof(_header_size));
		Ignore(&data, _header_size);

		size_t section_count;
		Read<true>(&data, &section_count, sizeof(size_t));

		size_t total_memory = 0;
		
		ECS_STACK_CAPACITY_STREAM(char, temp_name, 512);
		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read<true>(&data, &name_size, sizeof(unsigned short));
			Read<true>(&data, temp_name.buffer, (name_size + 1) * sizeof(char));

			unsigned int section_index = -1;
			for (size_t subindex = 0; subindex < serialize_data.size; subindex++) {
				if (Stream<char>(temp_name.buffer, name_size) == Stream<char>(serialize_data[subindex].name, section_name_sizes[subindex])) {
					section_index = subindex;
					break;
				}
			}

			bool should_add = section_index != -1;
			
			size_t data_size;
			Read<true>(&data, &data_size, sizeof(size_t));
			if (should_add) {
				total_memory += should_add * data_size;
				serialize_data[section_index].data.size = data_size;
			}
			Ignore(&data, data_size);
		}

		return total_memory;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionHeaderSize(uintptr_t data) {
		return *(size_t*)data;
	}

	// -------------------------------------------------------------------------------------------------------------------

	void DeserializeSectionHeader(uintptr_t data, CapacityStream<void>& header) {
		size_t header_size = 0;
		Read<true>(&data, &header_size, sizeof(header_size));
		ECS_ASSERT(header_size <= header.capacity);
		Read<true>(&data, header.buffer, header_size);
	}

	// -------------------------------------------------------------------------------------------------------------------

}