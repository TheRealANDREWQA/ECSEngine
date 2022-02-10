#include "ecspch.h"
#include "Serialization.h"
#include "SerializeSection.h"
#include "../../Reflection/Reflection.h"
#include "../SerializationHelpers.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	// ------------------------------------------------------------------------------------------------------------------

	void ConvertTypeToSerializeSection(
		ReflectionType type,
		const void* data,
		Stream<SerializeSectionData>& sections
	) {
		for (size_t index = 0; index < type.fields.size; index++) {
			// If a basic type, copy the data directly
			const void* field_data = function::OffsetPointer(data, type.fields[index].info.pointer_offset);

			if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Basic) {
				sections.Add({ { field_data, type.fields[index].info.byte_size }, type.fields[index].name });
			}
			else {
				// Determine if it is a pointer, basic array or stream
				// If pointer, only do it for 1 level of indirection and ASCII or wide char strings
				if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Pointer) {
					if (GetReflectionFieldPointerIndirection(type.fields[index].info) == 1) {
						if (type.fields[index].info.basic_type == ReflectionBasicFieldType::Int8) {
							const char* characters = *(const char**)field_data;
							sections.Add({ { characters, strlen(characters) }, type.fields[index].name });
						}
						else if (type.fields[index].info.basic_type == ReflectionBasicFieldType::Wchar_t) {
							const wchar_t* characters = *(const wchar_t**)field_data;
							sections.Add({ { characters, wcslen(characters) }, type.fields[index].name });
						}
						// Other type of pointers cannot be serialized - they must be turned into streams
					}
					// Other types of pointers cannot be serialized - multi level indirections are not allowed
				}
				else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					// Set the data to the actual pointers
					// The byte size represents the actual byte size of the array
					sections.Add({ {field_data, type.fields[index].info.byte_size}, type.fields[index].name });
				}
				else {
					// Check for streams
					Stream<void> stream = GetReflectionFieldStreamVoid(type.fields[index].info, data);
					if (stream.buffer != nullptr) {
						// If it succeeded
						stream.size *= GetReflectionFieldStreamElementByteSize(type.fields[index].info);
						sections.Add({ stream, type.fields[index].name });
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	void ConvertSerializeSectionToType(
		ReflectionType type,
		Stream<SerializeSectionData> sections,
		void* address
	) {
		// The sections are ordered so as to match the fields inside the type
		for (size_t index = 0; index < sections.size; index++) {
			// If normal type, blit the data
			void* data = function::OffsetPointer(address, type.fields[index].info.pointer_offset);
			if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Basic) {
				sections[index].data.CopyTo(data);
			}
			else {
				// If pointer - only copy for level 1 indirection ASCII strings and wide strings
				if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Pointer) {
					if (GetReflectionFieldPointerIndirection(type.fields[index].info) == 1) {
						if (type.fields[index].info.basic_type == ReflectionBasicFieldType::Int8) {
							char** characters = (char**)data;
							// The pointer must only be redirected - not copied
							*characters = (char*)sections[index].data.buffer;
						}
						else if (type.fields[index].info.basic_type == ReflectionBasicFieldType::Wchar_t) {
							wchar_t** characters = (wchar_t**)data;
							*characters = (wchar_t*)sections[index].data.buffer;
						}
						// Else set to nullptr the value
						else {
							void** pointer = (void**)data;
							*pointer = nullptr;
						}
					}
					// Else set the pointer to nullptr
					else {
						void** pointer = (void**)data;
						*pointer = nullptr;
					}
				}
				else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					// The content can be copied because it is embedded into the type
					sections[index].data.CopyTo(data);
				}
				else {
					size_t element_size = GetReflectionFieldStreamElementByteSize(type.fields[index].info);
					size_t element_count = sections[index].data.size / element_size;

					// For stream types, only redirect them
					if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Stream) {
						Stream<void>* stream = (Stream<void>*)data;
						stream->buffer = sections[index].data.buffer;
						stream->size = element_count;
					}
					else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::CapacityStream) {
						CapacityStream<void>* stream = (CapacityStream<void>*)data;
						stream->buffer = sections[index].data.buffer;
						stream->size = element_count;
					}
					else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::ResizableStream) {
						ResizableStream<char>* stream = (ResizableStream<char>*)data;
						stream->buffer = (char*)sections[index].data.buffer;
						stream->size = element_count;
					}
					else {
						// An error must have happened - or erroneous field type - memset set it to 0
						memset(data, 0, type.fields[index].info.byte_size);
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool Serialize(
		ReflectionType type,
		const void* data,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator
	) {
		const size_t MAX_FIELDS = ECS_KB * 2;
		ECS_ASSERT(type.fields.size < MAX_FIELDS);

		SerializeSectionData* _sections = (SerializeSectionData*)ECS_STACK_ALLOC(sizeof(SerializeSectionData) * type.fields.size);
		Stream<SerializeSectionData> sections(_sections, 0);

		ConvertTypeToSerializeSection(type, data, sections);

		size_t serialize_size = SerializeSectionSize(sections);
		return SerializeWriteFile(file, allocator, serialize_size, true, [=](uintptr_t& buffer) {
			SerializeSection(sections, buffer);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------

	void Serialize(
		ReflectionType type,
		const void* data,
		uintptr_t& stream
	) {
		const size_t MAX_FIELDS = ECS_KB * 2;
		ECS_ASSERT(type.fields.size < MAX_FIELDS);

		SerializeSectionData* _sections = (SerializeSectionData*)ECS_STACK_ALLOC(sizeof(SerializeSectionData) * type.fields.size);
		Stream<SerializeSectionData> sections(_sections, 0);

		ConvertTypeToSerializeSection(type, data, sections);
		SerializeSection(sections, stream);
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t SerializeSize(
		ReflectionType type,
		const void* data
	) {
		const size_t MAX_FIELDS = ECS_KB * 2;
		ECS_ASSERT(type.fields.size < MAX_FIELDS);

		SerializeSectionData* _sections = (SerializeSectionData*)ECS_STACK_ALLOC(sizeof(SerializeSectionData) * type.fields.size);
		Stream<SerializeSectionData> sections(_sections, 0);

		ConvertTypeToSerializeSection(type, data, sections);
		return SerializeSectionSize(sections);
	}

	// ------------------------------------------------------------------------------------------------------------------

	void* Deserialize(
		ReflectionType type,
		void* address,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator
	) {
		return DeserializeReadBinaryFileAndKeepMemory(file, allocator, [&](uintptr_t& buffer) {
			return Deserialize(type, address, buffer);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t Deserialize(
		ReflectionType type,
		void* address,
		uintptr_t& stream
	) {
		const size_t MAX_FIELDS = ECS_KB * 2;
		ECS_ASSERT(type.fields.size < MAX_FIELDS);

		SerializeSectionData* _sections = (SerializeSectionData*)ECS_STACK_ALLOC(sizeof(SerializeSectionData) * type.fields.size);
		Stream<SerializeSectionData> sections(_sections, 0);
		for (size_t index = 0; index < type.fields.size; index++) {
			sections[index].name = type.fields[index].name;
		}

		DeserializeSectionWithMatch(sections, stream);

		ConvertSerializeSectionToType(type, sections, address);

		size_t pointer_data_count = 0;
		for (size_t index = 0; index < type.fields.size; index++) {
			// If it is a stream, count it for pointer data
			if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Stream || type.fields[index].info.stream_type == ReflectionStreamFieldType::CapacityStream || 
				type.fields[index].info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				pointer_data_count += GetReflectionFieldStreamElementByteSize(type.fields[index].info) * GetReflectionFieldStreamSize(type.fields[index].info, address);
			}
			// Also count the ASCII strings and wide characters
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Pointer && GetReflectionFieldPointerIndirection(type.fields[index].info) == 1) {
				if (type.fields[index].info.basic_type == ReflectionBasicFieldType::Int8) {
					// Count the null terminator
					pointer_data_count += (strlen(*(const char**)function::OffsetPointer(address, type.fields[index].info.pointer_offset)) + 1) * sizeof(char);
				}
				else if (type.fields[index].info.basic_type == ReflectionBasicFieldType::Wchar_t) {
					// Count the null terminator
					pointer_data_count += (wcslen(*(const wchar_t**)function::OffsetPointer(address, type.fields[index].info.pointer_offset)) + 1) * sizeof(wchar_t);
				}
			}
		}

		return pointer_data_count;
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSize(
		ReflectionType type,
		uintptr_t data
	) {
		const size_t MAX_FIELDS = ECS_KB * 2;
		ECS_ASSERT(type.fields.size < MAX_FIELDS);

		SerializeSectionData* _sections = (SerializeSectionData*)ECS_STACK_ALLOC(sizeof(SerializeSectionData) * type.fields.size);
		Stream<SerializeSectionData> sections(_sections, 0);
		for (size_t index = 0; index < type.fields.size; index++) {
			sections[index].name = type.fields[index].name;
		}

		return DeserializeSectionSize(data, sections);
	}

	// ------------------------------------------------------------------------------------------------------------------

}