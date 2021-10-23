#include "ecspch.h"
#include "SerializeSection.h"
#include "Serialization.h"
#include "../../Allocators/AllocatorPolymorphic.h"

ECS_CONTAINERS;

#define APPEND_STRING " ptrdata"
#define NAME_BUFFER_CAPACITY 4096

namespace ECSEngine {

	using namespace Reflection;

	// -------------------------------------------------------------------------------------------------------------------

	template<typename StreamType>
	bool SerializeInternal(
		StreamType& stream,
		Stream<SerializeSectionData> data,
		Stream<void> header
	) {
		Write(stream, &header.size, sizeof(header.size));
		if (header.buffer != nullptr && header.size > 0) {
			Write(stream, header);
		}

		// Write the section count
		Write(stream, &data.size, sizeof(size_t));

		// For each section write the name size followed by the name
		// Then write the byte count and the data
		for (size_t index = 0; index < data.size; index++) {
			unsigned short name_size = (unsigned short)strlen(data[index].name);
			Write(stream, &name_size, sizeof(unsigned short));
			Write(stream, data[index].name, name_size * sizeof(char));
			Write(stream, &data[index].data.size, sizeof(size_t));
			Write(stream, data[index].data.buffer, data[index].data.size);
		}

		if constexpr (std::is_same_v<StreamType, std::ofstream>) {
			if (!stream.good()) {
				return false;
			}
		}
		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------

	template<typename StreamType>
	size_t DeserializeInternal(
		StreamType& ECS_RESTRICT stream,
		Stream<SerializeSectionData> sections,
		CapacityStream<void>* header,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		size_t header_size;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header->capacity >= header_size);
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t section_count;
		Read(stream, &section_count, sizeof(size_t));
		if (section_count == 0) {
			if (faulty_index != nullptr) {
				*faulty_index = sections.size + 1;
			}
			return 0;
		}

		size_t total_size = 0;
		char temp_name[512];

		// Prerecord section names
		unsigned int* name_sizes = (unsigned int*)alloca(sections.size * sizeof(unsigned int));
		for (size_t index = 0; index < sections.size; index++) {
			name_sizes[index] = (unsigned int)strlen(sections[index].name);
		}

		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read(stream, &name_size, sizeof(unsigned short));

			Read(stream, temp_name, sizeof(char) * name_size);
			size_t data_size;
			Read(stream, &data_size, sizeof(size_t));

			bool has_been_found = false;
			for (size_t subindex = 0; subindex < sections.size; index++) {
				if (function::CompareStrings(Stream<char>(temp_name, name_size), Stream<char>(sections[subindex].name, name_sizes[subindex]))) {
					if (data_size > sections[subindex].data.size) {
						if (faulty_index != nullptr) {
							*faulty_index = subindex;
						}
						return total_size;
					}
					Read(stream, sections[subindex].data.buffer, data_size);
					sections[subindex].data.size = data_size;

					total_size += data_size;
					has_been_found = true;
					break;
				}
			}
			if (!has_been_found) {
				Ignore(stream, data_size);
			}
		}

		if constexpr (std::is_same_v<StreamType, std::ifstream>) {
			if (!stream.good()) {
				if (faulty_index != nullptr) {
					*faulty_index = sections.size;
				}
			}
		}
		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	template<typename StreamType>
	size_t DeserializeInternal(
		StreamType& ECS_RESTRICT stream,
		Stream<const char*> section_names,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header,
		bool* ECS_RESTRICT success_status
	) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header->capacity >= header_size);
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t section_count;
		Read(stream, &section_count, sizeof(size_t));
		if (section_count == 0) {
			if (success_status != nullptr) {
				*success_status = false;
			}
			return 0;
		}

		size_t total_size = 0;
		char temp_name[512];

		// Prerecord section names
		unsigned int* name_sizes = (unsigned int*)alloca(section_names.size * sizeof(unsigned int));
		for (size_t index = 0; index < section_names.size; index++) {
			name_sizes[index] = (unsigned int)strlen(section_names[index]);
		}

		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read(stream, &name_size, sizeof(unsigned short));

			Read(stream, temp_name, sizeof(char) * name_size);
			size_t data_size;
			Read(stream, &data_size, sizeof(size_t));

			bool has_been_found = false;
			for (size_t subindex = pointers.size; subindex < section_names.size; index++) {
				if (function::CompareStrings(Stream<char>(temp_name, name_size), Stream<char>(section_names[subindex], name_sizes[subindex]))) {				
					pointers[pointers.size].data = function::OffsetPointer(memory_pool);
					pointers[pointers.size].data_size = data_size;
					section_names.Swap(pointers.size, subindex);
					pointers.size++;
					Read(stream, memory_pool, data_size);
					total_size += data_size;
					has_been_found = true;
					break;
				}
			}
			if (!has_been_found) {
				Ignore(stream, data_size);
			}
		}

		if constexpr (std::is_same_v<StreamType, std::ifstream>) {
			if (!stream.good()) {
				if (success_status != nullptr) {
					*success_status = false;
				}
			}
		}
		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	template<typename StreamType>
	size_t DeserializeInternal(
		StreamType& ECS_RESTRICT stream,
		Stream<SerializeSectionData> sections,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type,
		CapacityStream<void>* header,
		bool* success
	) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		if (header != nullptr) {
			ECS_ASSERT(header->capacity >= header_size);
			Read(stream, header->buffer, header_size);
			header->size = header_size;
		}
		else {
			Ignore(stream, header_size);
		}

		size_t section_count;
		Read(stream, &section_count, sizeof(section_count));
		if (section_count == 0) {
			return 0;
		}

		size_t total_size = 0;
		char temp_name[512];

		// Prerecord section names
		unsigned int* name_sizes = (unsigned int*)alloca(sections.size * sizeof(unsigned int));
		for (size_t index = 0; index < sections.size; index++) {
			name_sizes[index] = (unsigned int)strlen(sections[index].name);
		}

		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read(stream, &name_size, sizeof(unsigned short));

			Read(stream, temp_name, sizeof(char) * name_size);
			size_t data_size;
			Read(stream, &data_size, sizeof(data_size));

			bool has_been_found = false;
			for (size_t subindex = 0; subindex < sections.size; index++) {
				if (function::CompareStrings(Stream<char>(temp_name, name_size), Stream<char>(sections[subindex].name, name_sizes[subindex]))) {
					sections[subindex].data.buffer = Allocate(allocator, allocator_type, data_size);
					Read(stream, sections[subindex].data.buffer, data_size);
					sections[subindex].data.size = data_size;

					total_size += data_size;
					has_been_found = true;
					break;
				}
			}
			if (!has_been_found) {
				Ignore(stream, data_size);
			}
		}

		if constexpr (std::is_same_v<StreamType, std::ifstream>) {
			if (success != nullptr) {
				*success = stream.good();
			}
		}
		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	void ConvertTypeToSectionData(
		ReflectionType type, 
		Stream<SerializeSectionData>& serialize_data,
		const void* ECS_RESTRICT data, 
		void* ECS_RESTRICT name_buffer
	) {
		uintptr_t name_ptr = (uintptr_t)name_buffer;

		auto allocate_name = [&name_ptr](const char* name) {
			size_t name_size = strlen(name) * sizeof(char);
			memcpy((void*)name_ptr, name, name_size);
			name_ptr += name_size;
			memcpy((void*)name_ptr, APPEND_STRING, std::size(APPEND_STRING) - 1);
			name_ptr += std::size(APPEND_STRING) - 1;
		};

		for (size_t index = 0; index = type.fields.size; index++) {
			SerializeSectionData type_data;
			type_data.data = { function::OffsetPointer(data, type.fields[index].info.pointer_offset), type.fields[index].info.byte_size };
			type_data.name = type.fields[index].name;
			serialize_data.Add(type_data);

			if (type.fields[index].info.extended_type == ReflectionExtendedFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				serialize_data[serialize_data.size].data.buffer = stream->buffer;
				serialize_data[serialize_data.size].name = (char*)name_ptr;
				allocate_name(type.fields[index].name);
				serialize_data[serialize_data.size++].data.size = type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.extended_type == ReflectionExtendedFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				serialize_data[serialize_data.size].data.buffer = stream->buffer;
				serialize_data[serialize_data.size].name = (char*)name_ptr;
				allocate_name(type.fields[index].name);
				serialize_data[serialize_data.size++].data.size = type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.extended_type == ReflectionExtendedFieldType::ResizableStream) {
				ResizableStream<void*, LinearAllocator>* stream = (ResizableStream<void*, LinearAllocator>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				serialize_data[serialize_data.size].data.buffer = stream->buffer;
				serialize_data[serialize_data.size].name = (char*)name_ptr;
				allocate_name(type.fields[index].name);
				serialize_data[serialize_data.size++].data.size = type.fields[index].info.additional_flags * stream->size;
			}
		}

		ECS_ASSERT(name_ptr - (uintptr_t)name_buffer < NAME_BUFFER_CAPACITY);
	}

	// -------------------------------------------------------------------------------------------------------------------

	void ConvertTypeToSectionNames(ReflectionType type, Stream<const char*>& section_names, void* ECS_RESTRICT name_buffer) {
		uintptr_t name_ptr = (uintptr_t)name_buffer;

		auto allocate_name = [&name_ptr](const char* name) {
			size_t name_size = strlen(name) * sizeof(char);
			memcpy((void*)name_ptr, name, name_size);
			name_ptr += name_size;
			memcpy((void*)name_ptr, APPEND_STRING, std::size(APPEND_STRING) - 1);
			name_ptr += std::size(APPEND_STRING) - 1;
		};

		for (size_t index = 0; index = type.fields.size; index++) {
			section_names.Add(type.fields[index].name);

			if (IsStream(type.fields[index].info.extended_type)) {
				section_names.Add((char*)name_ptr);
				allocate_name(type.fields[index].name);
			}
		}
		
		ECS_ASSERT(name_ptr - (uintptr_t)name_buffer < NAME_BUFFER_CAPACITY);
	}

	// -------------------------------------------------------------------------------------------------------------------

	void ResolveTypeFromSectionNames(
		ReflectionType type, 
		Stream<const char*> section_names,
		Stream<function::CopyPointer> pointers, 
		void* ECS_RESTRICT address
	) {
		// Can do a pointer check since it will point to the same buffer
		for (size_t index = 0; index < pointers.size; index++) {
			for (size_t subindex = 0; subindex < type.fields.size; subindex++) {
				if (section_names[index] == type.fields[subindex].name) {
					ECS_ASSERT(pointers[index].data_size == type.fields[subindex].info.byte_size);
					memcpy(function::OffsetPointer(address, type.fields[subindex].info.pointer_offset), pointers[index].data, pointers[index].data_size);
				}
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------

	bool SerializeSection(
		const ReflectionManager* reflection,
		const char* ECS_RESTRICT type_name,
		std::ofstream& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header
	) {
		ReflectionType type = reflection->GetType(type_name);
		return SerializeSection(type, stream, data, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	bool SerializeSection(
		ReflectionType type,
		std::ofstream& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header
	) {
		ECS_ASSERT(type.fields.size < 128);

		SerializeSectionData _serialize_data[256];
		Stream<SerializeSectionData> serialize_data(_serialize_data, 0);

		char name_buffer[NAME_BUFFER_CAPACITY];
		ConvertTypeToSectionData(type, serialize_data, data, name_buffer);

		return SerializeInternal(stream, serialize_data, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	void SerializeSection(
		const ReflectionManager* reflection,
		const char* ECS_RESTRICT type_name,
		CapacityStream<void>& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header
	) {
		ReflectionType type = reflection->GetType(type_name);
		SerializeSection(type, stream, data, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	void SerializeSection(
		ReflectionType type,
		CapacityStream<void>& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header
	) {
		ECS_ASSERT(type.fields.size < 128);

		SerializeSectionData _serialize_data[256];
		Stream<SerializeSectionData> serialize_data(_serialize_data, 0);

		char name_buffer[NAME_BUFFER_CAPACITY];
		ConvertTypeToSectionData(type, serialize_data, data, name_buffer);
		SerializeInternal(stream, serialize_data, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	bool SerializeSection(std::ofstream& stream, Stream<SerializeSectionData> data, Stream<void> header) {
		return SerializeInternal(stream, data, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	void SerializeSection(CapacityStream<void>& stream, Stream<SerializeSectionData> data, Stream<void> header) {
		SerializeInternal(stream, data, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t SerializeSectionSize(ReflectionType type, const void* data) {
		// For the field count
		size_t total_size = sizeof(size_t);

		for (size_t index = 0; index < type.fields.size; index++) {
			total_size += sizeof(unsigned short) + sizeof(char) * strlen(type.fields[index].name) + sizeof(size_t) + type.fields[index].info.byte_size;
			
			if (type.fields[index].info.extended_type == ReflectionExtendedFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				total_size += strlen(type.fields[index].name) + std::size(APPEND_STRING) - 1;
				total_size += type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.extended_type == ReflectionExtendedFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				total_size += strlen(type.fields[index].name) + std::size(APPEND_STRING) - 1;
				total_size += type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.extended_type == ReflectionExtendedFieldType::ResizableStream) {
				ResizableStream<void*, LinearAllocator>* stream = (ResizableStream<void*, LinearAllocator>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				total_size += strlen(type.fields[index].name) + std::size(APPEND_STRING) - 1;
				total_size += type.fields[index].info.additional_flags * stream->size;
			}
		}

		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t SerializeSectionSize(Stream<SerializeSectionData> data) {
		// For the field count
		size_t total_size = sizeof(size_t);

		for (size_t index = 0; index < data.size; index++) {
			total_size += sizeof(unsigned short) + sizeof(char) * strlen(data[index].name) + sizeof(size_t) + data[index].data.size;
		}

		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header,
		bool* ECS_RESTRICT success_status
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeSection(type, stream, address, memory_pool, pointers, header, success_status);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeSection(type, stream, address, header, faulty_index);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header,
		bool* ECS_RESTRICT success_status
	) {
		ECS_ASSERT(type.fields.size < 128);
		
		char name_buffer[NAME_BUFFER_CAPACITY];
		const char* _section_names[256];
		Stream<const char*> section_names(_section_names, 0);
		
		ConvertTypeToSectionNames(type, section_names, name_buffer);
		size_t pointer_data_size = DeserializeInternal(stream, section_names, memory_pool, pointers, header, success_status);
		
		ResolveTypeFromSectionNames(type, section_names, pointers, address);
		return pointer_data_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		ECS_ASSERT(type.fields.size < 128);

		char name_buffer[NAME_BUFFER_CAPACITY];
		SerializeSectionData _serialize_data[256];
		Stream<SerializeSectionData> serialize_data(_serialize_data, 0);
		
		ConvertTypeToSectionData(type, serialize_data, address, name_buffer);
		return DeserializeInternal(stream, serialize_data, header, faulty_index);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeSection(type, stream, address, memory_pool, pointers, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeSection(type, stream, address, header);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header
	) {
		ECS_ASSERT(type.fields.size < 128);

		char name_buffer[NAME_BUFFER_CAPACITY];
		const char* _section_names[256];
		Stream<const char*> section_names(_section_names, 0);
		
		ConvertTypeToSectionNames(type, section_names, name_buffer);
		size_t pointer_data_size = DeserializeInternal(stream, section_names, memory_pool, pointers, header, nullptr);

		ResolveTypeFromSectionNames(type, section_names, pointers, address);
		return pointer_data_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		ECS_ASSERT(type.fields.size < 128);

		char name_buffer[NAME_BUFFER_CAPACITY];
		SerializeSectionData _serialize_data[256];
		Stream<SerializeSectionData> serialize_data(_serialize_data, 0);

		ConvertTypeToSectionData(type, serialize_data, address, name_buffer);
		return DeserializeInternal(stream, serialize_data, header, faulty_index);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		std::ifstream& ECS_RESTRICT stream,
		Stream<const char*> section_names,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header,
		bool* ECS_RESTRICT success_status
	) {
		return DeserializeInternal(stream, section_names, memory_pool, pointers, header, success_status);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		std::ifstream& stream,
		Stream<SerializeSectionData> data,
		CapacityStream<void>* header,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		return DeserializeInternal(stream, data, header, faulty_index);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		uintptr_t& stream,
		Stream<const char*> section_names,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		CapacityStream<void>* header
	) {
		return DeserializeInternal(stream, section_names, memory_pool, pointers, header, nullptr);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		uintptr_t& stream,
		Stream<SerializeSectionData> data,
		CapacityStream<void>* header,
		unsigned int* ECS_RESTRICT faulty_index
	) {
		return DeserializeInternal(stream, data, header, faulty_index);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		uintptr_t& stream, 
		Stream<SerializeSectionData> data, 
		void* ECS_RESTRICT allocator, 
		AllocatorType allocator_type,
		CapacityStream<void>* header
	)
	{
		return DeserializeInternal(stream, data, allocator, allocator_type, header, nullptr);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSection(
		std::ifstream& stream, 
		Stream<SerializeSectionData> data, 
		void* ECS_RESTRICT allocator, 
		AllocatorType allocator_type,
		CapacityStream<void>* header, 
		bool* success
	)
	{
		return DeserializeInternal(stream, data, allocator, allocator_type, header, success);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionCount(uintptr_t stream, size_t header_size)
	{
		return *(size_t*)function::OffsetPointer((void*)stream, header_size);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionCount(std::ifstream& stream, size_t header_size)
	{
		size_t count = 0;
		Ignore(stream, header_size + sizeof(header_size));
		Read(stream, &count, sizeof(count));
		stream.seekg(0, std::ios_base::beg);
		ECS_ASSERT(stream.good());
		return count;
	}

	// -------------------------------------------------------------------------------------------------------------------
	
	size_t DeserializeSectionSize(
		uintptr_t data,
		Stream<SerializeSectionData> serialize_data,
		size_t* header_size
	) {
		unsigned short* section_name_sizes = (unsigned short*)alloca(sizeof(unsigned short) * serialize_data.size);
		for (size_t index = 0; index < serialize_data.size; index++) {
			section_name_sizes[index] = (unsigned short)strlen(serialize_data[index].name);
		}

		size_t _header_size = 0;
		Read(data, &_header_size, sizeof(_header_size));
		if (header_size != nullptr) {
			*header_size = _header_size;
		}
		Ignore(data, _header_size);

		size_t section_count;
		Read(data, &section_count, sizeof(size_t));

		size_t total_memory = 0;
		
		char temp_name[512];
		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read(data, &name_size, sizeof(unsigned short));
			Read(data, temp_name, name_size * sizeof(char));

			unsigned int section_index = -1;
			for (size_t subindex = 0; subindex < serialize_data.size; subindex++) {
				if (function::CompareStrings(Stream<char>(temp_name, name_size), Stream<char>(serialize_data[subindex].name, section_name_sizes[subindex]))) {
					section_index = subindex;
					break;
				}
			}

			bool should_add = section_index != -1;
			
			size_t data_size;
			Read(data, &data_size, sizeof(size_t));
			total_memory += should_add * data_size;
			data += data_size;
		}

		return total_memory;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionSize(
		uintptr_t data,
		Stream<const char*> section_names,
		size_t* header_size
	) {
		unsigned short* section_name_sizes = (unsigned short*)alloca(sizeof(unsigned short) * section_names.size);
		for (size_t index = 0; index < section_names.size; index++) {
			section_name_sizes[index] = (unsigned short)strlen(section_names[index]);
		}

		size_t _header_size = 0;
		Read(data, &_header_size, sizeof(_header_size));
		if (header_size != nullptr) {
			*header_size = _header_size;
		}
		Ignore(data, _header_size);

		size_t section_count;
		Read(data, &section_count, sizeof(size_t));

		size_t total_memory = 0;

		char temp_name[512];
		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read(data, &name_size, sizeof(unsigned short));
			Read(data, temp_name, name_size * sizeof(char));

			unsigned int section_index = -1;
			for (size_t subindex = 0; subindex < section_names.size; subindex++) {
				if (function::CompareStrings(Stream<char>(temp_name, name_size), Stream<char>(section_names[subindex], section_name_sizes[subindex]))) {
					section_index = subindex;
					break;
				}
			}

			bool should_add = section_index != -1;

			size_t data_size;
			Read(data, &data_size, sizeof(size_t));
			total_memory += should_add * data_size;
			data += data_size;
		}

		return total_memory;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionSize(
		uintptr_t data,
		ReflectionType type,
		size_t* header_size
	) {
		ECS_ASSERT(type.fields.size < 128);
		unsigned short field_name_sizes[128];
		for (size_t index = 0; index < type.fields.size; index++) {
			field_name_sizes[index] = (unsigned short)type.fields[index].name;
		}

		size_t _header_size = 0;
		Read(data, &_header_size, sizeof(_header_size));
		if (header_size != nullptr) {
			*header_size = _header_size;
		}
		Ignore(data, _header_size);

		size_t section_count;
		Read(data, &section_count, sizeof(size_t));

		size_t total_memory = 0;

		char temp_name[512];
		for (size_t index = 0; index < section_count; index++) {
			unsigned short name_size;
			Read(data, &name_size, sizeof(unsigned short));
			Read(data, temp_name, name_size * sizeof(char));

			unsigned int section_index = -1;
			for (size_t subindex = 0; subindex < type.fields.size; subindex++) {
				if (function::CompareStrings(Stream<char>(temp_name, name_size), Stream<char>(type.fields[subindex].name, field_name_sizes[subindex]))) {
					section_index = subindex;
					break;
				}
			}

			bool should_add = section_index != -1;

			size_t data_size;
			Read(data, &data_size, sizeof(size_t));
			total_memory += should_add * data_size;
			data += data_size;
		}

		return total_memory;

	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionHeaderSize(uintptr_t data) {
		return *(size_t*)data;
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSectionHeaderSize(std::ifstream& stream) {
		size_t count = 0;
		Read(stream, &count, sizeof(count));
		stream.seekg(0, std::ios_base::beg);
		ECS_ASSERT(stream.good());
		return count;
	}

	// -------------------------------------------------------------------------------------------------------------------

	void DeserializeSectionHeader(uintptr_t data, CapacityStream<void>& header) {
		size_t header_size = 0;
		Read(data, &header_size, sizeof(header_size));
		ECS_ASSERT(header_size <= header.capacity);
		Read(data, header, header_size);
	}

	// -------------------------------------------------------------------------------------------------------------------

	bool DeserializeSectionHeader(std::ifstream& stream, CapacityStream<void>& header) {
		size_t header_size = 0;
		Read(stream, &header_size, sizeof(header_size));
		ECS_ASSERT(header_size <= header.capacity);
		Read(stream, header, header_size);
		stream.seekg(0, std::ios_base::beg);
		return stream.good();
	}

	// -------------------------------------------------------------------------------------------------------------------

}