#include "ecspch.h"
#include "TextSerialize.h"
#include "../SerializationHelpers.h"
#include "../../Reflection/Reflection.h"
#include "../../Reflection/ReflectionStringFunctions.h"

namespace ECSEngine {

	using namespace Reflection;
	
	// ---------------------------------------------------------------------------------------------------------------------

	unsigned int TextSerializeConvertToFieldAPI(ReflectionType type, const void* data, TextSerializeField* serialize_fields) {
		unsigned int write_index = 0;
		for (size_t index = 0; index < type.fields.size; index++) {
			serialize_fields[write_index].basic_type = type.fields[index].info.basic_type;
			serialize_fields[write_index].name = type.fields[index].name;

			bool is_stream_type = (type.fields[index].info.stream_type != ReflectionStreamFieldType::Basic
				&& type.fields[index].info.stream_type != ReflectionStreamFieldType::Unknown);
			serialize_fields[write_index].character_stream = is_stream_type && type.fields[index].info.basic_type == ReflectionBasicFieldType::Int8;
			if (is_stream_type) {
				serialize_fields[write_index].is_stream = type.fields[index].info.basic_type != ReflectionBasicFieldType::Wchar_t && !serialize_fields[write_index].character_stream;
				serialize_fields[write_index].data.buffer = *(void**)OffsetPointer(data, type.fields[index].info.pointer_offset);

				// Only level 1 pointers should be accepted
				if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Pointer) {
					if (GetReflectionFieldPointerIndirection(type.fields[index].info) == 1) {
						// If only a pointer type - check for char and wchar_t, these are the only one for which we can serialize pointer only types
						// Else a stream must be used
						if (serialize_fields[write_index].character_stream) {
							serialize_fields[write_index].data.size = strlen((char*)serialize_fields[index].data.buffer);
						}
						else if (serialize_fields[write_index].basic_type == ReflectionBasicFieldType::Wchar_t) {
							serialize_fields[write_index].data.size = wcslen((wchar_t*)serialize_fields[index].data.buffer);
						}
						else {
							write_index--;
						}
					}
					// If it a multi level indirection, skip it
					else {
						write_index--;
					}
				}
				else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					serialize_fields[write_index].data.buffer = OffsetPointer(data, type.fields[index].info.pointer_offset);
					serialize_fields[write_index].data.size = type.fields[index].info.basic_type_count;
				}
				else {
					serialize_fields[write_index].data.size = GetReflectionFieldStreamSize(
						type.fields[index].info, 
						data
					);
				}
			}
			else {
				serialize_fields[write_index].is_stream = false;
				serialize_fields[write_index].data.buffer = OffsetPointer(data, type.fields[index].info.pointer_offset);
				serialize_fields[write_index].data.size = 1;
			}

			write_index++;
		}

		return write_index;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFromAPIToType(ReflectionType type, void* data, Stream<TextDeserializeField> deserialize_fields) {
		Stream<char>* field_names = (Stream<char>*)ECS_STACK_ALLOC(sizeof(Stream<char>) * type.fields.size);
		for (size_t index = 0; index < type.fields.size; index++) {
			field_names[index] = type.fields[index].name;
		}

		bool some_fields_missed = false;
		for (size_t index = 0; index < deserialize_fields.size; index++) {
			// Check to see if the field exists in the type
			unsigned int in_type_name_index = FindString(deserialize_fields[index].name, Stream<Stream<char>>(field_names, type.fields.size));
			if (in_type_name_index != -1) {
				// If the types mismatch, don't assign to it
				// Change the enum type to Int8
				deserialize_fields[index].basic_type = deserialize_fields[index].basic_type == ReflectionBasicFieldType::Enum ?
					ReflectionBasicFieldType::Int8 : deserialize_fields[index].basic_type;

				ReflectionFieldInfo info = type.fields[in_type_name_index].info;
				if (deserialize_fields[index].basic_type == info.basic_type) {
					// If there is pointer data (pointer_data.buffer != nullptr), make sure that the basic type is also a stream type
					if (deserialize_fields[index].pointer_data.buffer != nullptr && info.stream_type != ReflectionStreamFieldType::Basic) {
						// Only level 1 pointers should be accepted
						if (info.stream_type == ReflectionStreamFieldType::Pointer) {
							if (GetReflectionFieldPointerIndirection(info) == 1) {
								// If it is a pointer only, do the copy only if the basic type is an ASCII string or wide string
								if (deserialize_fields[index].basic_type == ReflectionBasicFieldType::Int8) {
									char** data_pointer = (char**)OffsetPointer(data, info.pointer_offset);
									*data_pointer = deserialize_fields[index].ascii_characters.buffer;
								}
								else if (deserialize_fields[index].basic_type == ReflectionBasicFieldType::Wchar_t) {
									wchar_t** data_pointer = (wchar_t**)OffsetPointer(data, info.pointer_offset);
									*data_pointer = deserialize_fields[index].wide_characters.buffer;
								}
								else {
									some_fields_missed = true;
								}
							}
							else {
								some_fields_missed = true;
							}
						}
						else {
							void* stream_data = deserialize_fields[index].pointer_data.buffer;
							size_t stream_size = deserialize_fields[index].pointer_data.size;
							if (ReflectionBasicFieldType::Int8 == deserialize_fields[index].basic_type) {
								stream_data = deserialize_fields[index].ascii_characters.buffer;
								stream_size = deserialize_fields[index].ascii_characters.size;
							}
							else if (ReflectionBasicFieldType::Wchar_t == deserialize_fields[index].basic_type) {
								stream_data = deserialize_fields[index].wide_characters.buffer;
								stream_size = deserialize_fields[index].wide_characters.size;
							}

							// If it is a basic array, copy directly into it only if the size allows for it
							// Else repoint the streams to those
							if (info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
								if (info.basic_type_count >= deserialize_fields[index].pointer_data.size) {
									memcpy(
										OffsetPointer(data, info.pointer_offset),
										stream_data,
										stream_size * GetReflectionBasicFieldTypeByteSize(deserialize_fields[index].basic_type)
									);
								}
								else {
									some_fields_missed = true;
								}
							}
							else {
								void* stream_offset = OffsetPointer(data, info.pointer_offset);
								if (info.stream_type == ReflectionStreamFieldType::Stream) {
									Stream<void>* stream = (Stream<void>*)stream_offset;
									stream->buffer = stream_data;
									stream->size = stream_size;
								}
								else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
									CapacityStream<void>* stream = (CapacityStream<void>*)stream_offset;
									stream->buffer = stream_data;
									stream->size = stream_size;
									stream->capacity = stream_size;
								}
								else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
									ResizableStream<char>* stream = (ResizableStream<char>*)stream_offset;
									stream->buffer = (char*)stream_data;
									stream->size = stream_size;
									stream->capacity = stream_size;
								}
								// Some error must have occured - incorrect stream type
								else {
									some_fields_missed = true;
								}
							}
						}
					}
					// Basic field type - blit the data
					else {
						const void* data_to_blit = &deserialize_fields[index].floating;
						size_t data_to_blit_size = info.byte_size;

						// For basic arrays for strings, blit them directly, else keep the stream
						if (info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
							if (info.basic_type == ReflectionBasicFieldType::Int8) {
								data_to_blit = deserialize_fields[index].ascii_characters.buffer;
								data_to_blit_size = deserialize_fields[index].ascii_characters.size;
							}
							else if (info.basic_type == ReflectionBasicFieldType::Wchar_t) {
								data_to_blit = deserialize_fields[index].wide_characters.buffer;
								data_to_blit_size = deserialize_fields[index].wide_characters.size;
							}
						}
						void* data_offset = OffsetPointer(data, info.pointer_offset);
						memcpy(data_offset, data_to_blit, data_to_blit_size);
					}
				}
				else {
					some_fields_missed = true;
				}
			}
		}

		return some_fields_missed ? ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS : ECS_TEXT_DESERIALIZE_OK;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	bool TextSerialize(ReflectionType type, const void* data, Stream<wchar_t> file, AllocatorPolymorphic allocator)
	{
		const size_t MAX_STACK_SERIALIZE_FIELDS = ECS_KB;
		ECS_ASSERT(type.fields.size < MAX_STACK_SERIALIZE_FIELDS);

		// Create a mapping for the serialize API
		TextSerializeField* serialize_fields = (TextSerializeField*)ECS_STACK_ALLOC(sizeof(TextSerializeField) * type.fields.size);
		unsigned int serialize_field_count = TextSerializeConvertToFieldAPI(type, data, serialize_fields);

		// Send the call to the API
		return TextSerializeFields({ serialize_fields, serialize_field_count }, file, allocator);
	}

	// ---------------------------------------------------------------------------------------------------------------------


	void TextSerialize(ReflectionType type, const void* data, uintptr_t& stream)
	{
		const size_t MAX_STACK_SERIALIZE_FIELDS = ECS_KB;
		ECS_ASSERT(type.fields.size < MAX_STACK_SERIALIZE_FIELDS);

		// Create a mapping for the serialize API
		TextSerializeField* serialize_fields = (TextSerializeField*)ECS_STACK_ALLOC(sizeof(TextSerializeField) * type.fields.size);
		unsigned int serialize_field_count = TextSerializeConvertToFieldAPI(type, data, serialize_fields);

		// Send the call to the API
		TextSerializeFields({ serialize_fields, serialize_field_count }, stream);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	size_t TextSerializeSize(ReflectionType type, const void* data)
	{
		const size_t MAX_STACK_SERIALIZE_FIELDS = ECS_KB;
		ECS_ASSERT(type.fields.size < MAX_STACK_SERIALIZE_FIELDS);

		// Create a mapping for the serialize API
		TextSerializeField* serialize_fields = (TextSerializeField*)ECS_STACK_ALLOC(sizeof(TextSerializeField) * type.fields.size);
		unsigned int serialize_field_count = TextSerializeConvertToFieldAPI(type, data, serialize_fields);

		return TextSerializeFieldsSize({ serialize_fields, serialize_field_count });
	}

	// ---------------------------------------------------------------------------------------------------------------------

	// Use the TextSerializeFields API
	ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		ReflectionType type,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator
	)
	{
		const size_t MAX_DESERIALIZE_FIELDS = 256;
		ECS_STACK_CAPACITY_STREAM(TextDeserializeField, deserialize_fields, MAX_DESERIALIZE_FIELDS);

		ECS_TEXT_DESERIALIZE_STATUS first_status = TextDeserializeFields(deserialize_fields, memory_pool, file, allocator);
		ECS_TEXT_DESERIALIZE_STATUS second_status = TextDeserializeFromAPIToType(type, address, deserialize_fields);
		return first_status | second_status;
	}

	// -----------------------------------------------------------------------------------------

	// Use the TextSerializeFields API
	ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		ReflectionType type,
		void* address,
		CapacityStream<void>& memory_pool,
		uintptr_t& stream
	) {
		const size_t MAX_DESERIALIZE_FIELDS = ECS_KB;
		ECS_STACK_CAPACITY_STREAM(TextDeserializeField, deserialize_fields, MAX_DESERIALIZE_FIELDS);

		ECS_TEXT_DESERIALIZE_STATUS first_status = TextDeserializeFields(deserialize_fields, memory_pool, stream);
		ECS_TEXT_DESERIALIZE_STATUS second_status = TextDeserializeFromAPIToType(type, address, deserialize_fields);
		return first_status | second_status;
	}

	// -----------------------------------------------------------------------------------------

	// Use the TextSerializeFields API
	ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		ReflectionType type,
		void* address,
		AllocatorPolymorphic pointer_allocator,
		Stream<wchar_t> file,
		AllocatorPolymorphic file_allocator
	) {
		const size_t MAX_DESERIALIZE_FIELDS = ECS_KB;
		ECS_STACK_CAPACITY_STREAM(TextDeserializeField, deserialize_fields, MAX_DESERIALIZE_FIELDS);

		ECS_TEXT_DESERIALIZE_STATUS first_status = TextDeserializeFields(deserialize_fields, pointer_allocator, file, file_allocator);
		ECS_TEXT_DESERIALIZE_STATUS second_status = TextDeserializeFromAPIToType(type, address, deserialize_fields);
		return first_status | second_status;
	}

	// -----------------------------------------------------------------------------------------

	// Use the TextSerializeFields API
	ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		ReflectionType type,
		void* address,
		AllocatorPolymorphic pointer_allocator,
		uintptr_t& stream
	) {
		const size_t MAX_DESERIALIZE_FIELDS = ECS_KB;
		ECS_STACK_CAPACITY_STREAM(TextDeserializeField, deserialize_fields, MAX_DESERIALIZE_FIELDS);

		ECS_TEXT_DESERIALIZE_STATUS first_status = TextDeserializeFields(deserialize_fields, pointer_allocator, stream);
		ECS_TEXT_DESERIALIZE_STATUS second_status = TextDeserializeFromAPIToType(type, address, deserialize_fields);
		return first_status | second_status;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	size_t TextDeserializeSize(
		uintptr_t stream
	) {
		return TextDeserializeFieldsSize(stream);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------------------------------------

}