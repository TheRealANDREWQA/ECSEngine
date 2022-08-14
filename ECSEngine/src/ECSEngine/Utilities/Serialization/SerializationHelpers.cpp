#include "ecspch.h"
#include "SerializationHelpers.h"
#include "Binary/Serialization.h"
#include "../Reflection/ReflectionStringFunctions.h"
#include "../Reflection/Reflection.h"
#include "../../Internal/Resources/AssetDatabaseSerializeFunctions.h"
#include "../../Containers/SparseSet.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------

	bool IgnoreFile(ECS_FILE_HANDLE file, size_t byte_size)
	{
		return SetFileCursor(file, byte_size, ECS_FILE_SEEK_CURRENT) != -1;
	}

	// -----------------------------------------------------------------------------------------

	bool Ignore(CapacityStream<void>& stream, size_t byte_size)
	{
		stream.size += byte_size;
		return stream.size < stream.capacity;
	}

	// -----------------------------------------------------------------------------------------

	bool Ignore(uintptr_t* stream, size_t byte_size)
	{
		*stream += byte_size;
		return true;
	}

	// -----------------------------------------------------------------------------------------

	bool IgnoreWithSize(uintptr_t* stream)
	{
		size_t size = 0;
		Read<true>(stream, &size, sizeof(size));
		Ignore(stream, size);
		return true;
	}

	// -----------------------------------------------------------------------------------------

	bool IgnoreWithSizeShort(uintptr_t* stream)
	{
		unsigned short size = 0;
		Read<true>(stream, &size, sizeof(size));
		Ignore(stream, size);
		return true;
	}

	// -----------------------------------------------------------------------------------------

	using namespace Reflection;

	size_t SerializeCustomTypeBasicTypeHelper(
		Stream<char>& template_type,
		const ReflectionManager* reflection_manager,
		ReflectionType* type,
		unsigned int& custom_serializer_index,
		ReflectionBasicFieldType& basic_type,
		ReflectionStreamFieldType& stream_type
	) {
		size_t min_size = sizeof("Stream<");
		unsigned int string_offset = 0;

		size_t current_type_byte_size = 0;
		stream_type = ReflectionStreamFieldType::Basic;

		if (template_type.size < min_size) {
			string_offset = 0;
		}
		else if (memcmp(template_type.buffer, "Stream<", sizeof("Stream<")) == 0) {
			string_offset = sizeof("Stream<") - 1;
			current_type_byte_size = sizeof(Stream<void>);

			stream_type = ReflectionStreamFieldType::Stream;
		}
		else if (memcmp(template_type.buffer, "CapacityStream<", sizeof("CapacityStream<")) == 0) {
			string_offset = sizeof("CapacityStream<") - 1;
			current_type_byte_size = sizeof(CapacityStream<void>);

			stream_type = ReflectionStreamFieldType::CapacityStream;
		}
		else if (memcmp(template_type.buffer, "ResizableStream<", sizeof("ResizableStream<")) == 0) {
			string_offset = sizeof("ResizableStream<") - 1;
			current_type_byte_size = sizeof(ResizableStream<void>);

			stream_type = ReflectionStreamFieldType::ResizableStream;
		}
		//else {
		//	// Verify pointers 
		//	Stream<char> asterisk = function::FindFirstCharacter(template_type, '*');
		//	if (asterisk.buffer != nullptr) {
		//		stream_type = ReflectionStreamFieldType::Pointer;
		//	}
		//}
		ReflectionContainerTypeByteSizeData byte_data;
		byte_data.definition = template_type;

		template_type = { template_type.buffer + string_offset, template_type.size - string_offset - (string_offset > 0) };
		basic_type = ConvertStringToBasicFieldType(template_type);

		type->name = { nullptr, 0 };
		custom_serializer_index = -1;

		// It is not a trivial type - try with a reflected type or with a type from the custom serializers
		if (basic_type == ReflectionBasicFieldType::Unknown) {
			// It is not a reflected type
			if (!reflection_manager->TryGetType(template_type, *type)) {
				// Custom serializer
				custom_serializer_index = FindSerializeCustomType(template_type);

				// This should not fail
				ECS_ASSERT(custom_serializer_index != -1, "Failed to serialize custom stream");
				current_type_byte_size = ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].container_type.byte_size(&byte_data).x;
			}
			else {
				basic_type = ReflectionBasicFieldType::UserDefined;
				current_type_byte_size = GetReflectionTypeByteSize(type);
			}
		}
		else {
			current_type_byte_size = GetReflectionBasicFieldTypeByteSize(basic_type);
		}

		return current_type_byte_size;
	}

	size_t SerializeCustomWriteHelper(
		ReflectionBasicFieldType basic_type,
		ReflectionStreamFieldType stream_type,
		const ReflectionType* reflection_type,
		unsigned int custom_serializer_index,
		SerializeCustomTypeWriteFunctionData* write_data,
		Stream<void> data_to_write,
		size_t element_byte_size,
		Stream<size_t> indices
	) {
		size_t serialize_size = 0;

		size_t element_count = indices.buffer == nullptr ? data_to_write.size : indices.size;

		if (reflection_type->name.size > 0) {
			// It is a reflected type
			SerializeOptions options;
			options.write_type_table = false;
			options.error_message = write_data->options != nullptr ? write_data->options->error_message : nullptr;
			options.verify_dependent_types = false;

			if (write_data->write_data) {
				// Hoist the indices check outside the for
				if (indices.buffer == nullptr) {
					for (size_t index = 0; index < element_count; index++) {
						void* element = function::OffsetPointer(data_to_write.buffer, index * element_byte_size);
						// Use the serialize function now
						Serialize(write_data->reflection_manager, reflection_type, element, *write_data->stream, &options);
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = function::OffsetPointer(data_to_write.buffer, indices[index] * element_byte_size);
						Serialize(write_data->reflection_manager, reflection_type, element, *write_data->stream, &options);
					}
				}
			}
			else {
				// Hoist the indices check outside the for
				if (indices.buffer == nullptr) {
					for (size_t index = 0; index < element_count; index++) {
						void* element = function::OffsetPointer(data_to_write.buffer, index * element_byte_size);
						serialize_size += SerializeSize(write_data->reflection_manager, reflection_type, element, &options);
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = function::OffsetPointer(data_to_write.buffer, indices[index] * element_byte_size);
						serialize_size += SerializeSize(write_data->reflection_manager, reflection_type, element, &options);
					}
				}
			}
		}
		// It has a custom serializer functor - use it
		else if (custom_serializer_index != -1) {
			// Hoist the indices check outside the for
			if (indices.buffer == nullptr) {
				for (size_t index = 0; index < element_count; index++) {
					void* element = function::OffsetPointer(data_to_write.buffer, index * element_byte_size);
					write_data->data = element;
					serialize_size += ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].write(write_data);
				}
			}
			else {
				for (size_t index = 0; index < element_count; index++) {
					void* element = function::OffsetPointer(data_to_write.buffer, indices[index] * element_byte_size);
					write_data->data = element;
					serialize_size += ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].write(write_data);
				}
			}
		}
		else {
			if (stream_type != ReflectionStreamFieldType::Basic) {
				// Serialize using known types - basic and stream based ones of primitive types
				ReflectionFieldInfo field_info;
				field_info.basic_type = basic_type;
				field_info.stream_type = stream_type;

				field_info.stream_byte_size = element_byte_size;
				size_t stream_offset = stream_type == ReflectionStreamFieldType::ResizableStream ? sizeof(ResizableStream<void>) : sizeof(Stream<void>);

				if (write_data->write_data) {
					// Hoist the indices check outside the for
					if (indices.buffer == nullptr) {
						for (size_t index = 0; index < element_count; index++) {
							void* element = function::OffsetPointer(
								data_to_write.buffer,
								index * stream_offset
							);
							WriteFundamentalType<true>(field_info, element, *write_data->stream);
						}
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							void* element = function::OffsetPointer(
								data_to_write.buffer,
								indices[index] * stream_offset
							);
							WriteFundamentalType<true>(field_info, element, *write_data->stream);
						}
					}
				}
				else {
					// Hoist the indices check outside the for
					if (indices.buffer == nullptr) {
						for (size_t index = 0; index < element_count; index++) {
							void* element = function::OffsetPointer(
								data_to_write.buffer,
								index * stream_offset
							);
							serialize_size += WriteFundamentalType<false>(field_info, element, *write_data->stream);
						}
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							void* element = function::OffsetPointer(
								data_to_write.buffer,
								indices[index] * stream_offset
							);
							serialize_size += WriteFundamentalType<false>(field_info, element, *write_data->stream);
						}
					}
				}
			}
			else {
				if (write_data->write_data) {
					if (indices.buffer == nullptr) {
						Write<true>(write_data->stream, data_to_write.buffer, element_count * element_byte_size);
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							void* element = function::OffsetPointer(data_to_write.buffer, indices[index] * element_byte_size);
							Write<true>(write_data->stream, element, element_byte_size);
						}
					}
				}
				else {
					serialize_size += element_count * element_byte_size;
				}
			}
		}

		return serialize_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeCustomReadHelper(
		ReflectionBasicFieldType basic_type,
		ReflectionStreamFieldType stream_type,
		const ReflectionType* reflection_type,
		unsigned int custom_serializer_index,
		SerializeCustomTypeReadFunctionData* read_data,
		size_t element_count,
		size_t elements_to_allocate,
		size_t element_byte_size,
		void** allocated_buffer,
		AllocatorPolymorphic override_allocator,
		Stream<size_t> indices
	) {
		size_t deserialize_size = 0;

		AllocatorPolymorphic backup_allocator = { nullptr };

		if (override_allocator.allocator != nullptr) {
			backup_allocator = override_allocator;
		}
		else if (read_data->options != nullptr) {
			backup_allocator = read_data->options->backup_allocator;
		}

		AllocatorPolymorphic deallocate_allocator = backup_allocator;
		auto deallocate_buffer = [&]() {
			DeallocateEx(deallocate_allocator, *allocated_buffer);
			*allocated_buffer = nullptr;
		};

		if (reflection_type->name.size > 0) {
			bool has_options = read_data->options != nullptr;

			DeserializeOptions options;
			options.read_type_table = false;
			options.verify_dependent_types = false;

			if (has_options) {
				options.backup_allocator = read_data->options->backup_allocator;
				options.error_message = read_data->options->error_message;
				options.field_allocator = read_data->options->field_allocator;
				options.use_resizable_stream_allocator = read_data->options->use_resizable_stream_allocator;
				options.fail_if_field_mismatch = read_data->options->fail_if_field_mismatch;
				options.field_table = read_data->options->field_table;
			}

			if (read_data->read_data) {
				// Allocate the data before
				void* buffer = AllocateEx(backup_allocator, elements_to_allocate * element_byte_size);
				*allocated_buffer = buffer;
				deserialize_size += elements_to_allocate * element_byte_size;

				// Hoist the if check outside the for
				if (indices.buffer == nullptr) {
					for (size_t index = 0; index < element_count; index++) {
						void* element = function::OffsetPointer(buffer, element_byte_size * index);
						ECS_DESERIALIZE_CODE code = Deserialize(read_data->reflection_manager, reflection_type, element, *read_data->stream, &options);
						if (code != ECS_DESERIALIZE_OK) {
							deallocate_buffer();
							return -1;
						}
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = function::OffsetPointer(buffer, element_byte_size * indices[index]);
						ECS_DESERIALIZE_CODE code = Deserialize(read_data->reflection_manager, reflection_type, element, *read_data->stream, &options);
						if (code != ECS_DESERIALIZE_OK) {
							deallocate_buffer();
							return -1;
						}
					}
				}
			}
			else {
				for (size_t index = 0; index < element_count; index++) {
					size_t byte_size = DeserializeSize(read_data->reflection_manager, reflection_type, *read_data->stream, &options);
					if (byte_size == -1) {
						return -1;
					}
					deserialize_size += byte_size;
				}
			}
		}
		else if (custom_serializer_index != -1) {
			if (read_data->read_data) {
				void* buffer = AllocateEx(backup_allocator, elements_to_allocate * element_byte_size);
				*allocated_buffer = buffer;
				deserialize_size += elements_to_allocate * element_byte_size;
			}

			if (indices.buffer == nullptr) {
				for (size_t index = 0; index < element_count; index++) {
					void* element = function::OffsetPointer(*allocated_buffer, element_byte_size * index);
					read_data->data = element;
					size_t byte_size_or_success = ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].read(read_data);
					if (byte_size_or_success == -1) {
						if (read_data->read_data) {
							deallocate_buffer();
							return -1;
						}
					}

					deserialize_size += byte_size_or_success;
				}
			}
			else {
				for (size_t index = 0; index < element_count; index++) {
					void* element = function::OffsetPointer(*allocated_buffer, element_byte_size * indices[index]);
					read_data->data = element;
					size_t byte_size_or_success = ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].read(read_data);
					if (byte_size_or_success == -1) {
						if (read_data->read_data) {
							deallocate_buffer();
							return -1;
						}
					}

					deserialize_size += byte_size_or_success;
				}
			}
		}
		else {
			if (stream_type != ReflectionStreamFieldType::Basic) {
				ReflectionFieldInfo field_info;
				field_info.basic_type = basic_type;
				field_info.stream_type = stream_type;
				field_info.stream_byte_size = element_byte_size;

				AllocatorPolymorphic allocator = { nullptr };
				bool has_options = read_data->options != nullptr;
				if (has_options) {
					allocator = read_data->options->field_allocator;
				}

				size_t stream_size = stream_type == ReflectionStreamFieldType::ResizableStream ? sizeof(ResizableStream<void>) : sizeof(Stream<void>);

				if (read_data->read_data) {
					*allocated_buffer = AllocateEx(backup_allocator, elements_to_allocate * stream_size);
					deserialize_size += elements_to_allocate * stream_size;

					if (indices.buffer == nullptr) {
						for (size_t index = 0; index < element_count; index++) {
							void* element = function::OffsetPointer(*allocated_buffer, index * stream_size);
							if (stream_type == ReflectionStreamFieldType::ResizableStream) {
								if (has_options && read_data->options->use_resizable_stream_allocator) {
									allocator = ((ResizableStream<void>*)element)->allocator;
								}
							}

							// The zero is for basic type arrays. Should not really happen
							ReadOrReferenceFundamentalType<true, true>(field_info, element, *read_data->stream, 0, allocator);
						}
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							void* element = function::OffsetPointer(*allocated_buffer, indices[index] * stream_size);
							if (stream_type == ReflectionStreamFieldType::ResizableStream) {
								if (has_options && read_data->options->use_resizable_stream_allocator) {
									allocator = ((ResizableStream<void>*)element)->allocator;
								}
							}

							// The zero is for basic type arrays. Should not really happen
							ReadOrReferenceFundamentalType<true, true>(field_info, element, *read_data->stream, 0, allocator);
						}
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = function::OffsetPointer(*allocated_buffer, index * stream_size);

						// The zero is for basic type arrays. Should not really happen
						deserialize_size += ReadOrReferenceFundamentalType<false>(field_info, element, *read_data->stream, 0, allocator);
					}
				}
			}
			else {
				if (read_data->read_data) {
					*allocated_buffer = AllocateEx(backup_allocator, elements_to_allocate * element_byte_size);

					if (indices.buffer == nullptr) {
						Read<true>(read_data->stream, *allocated_buffer, element_count * element_byte_size);
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							Read<true>(read_data->stream, function::OffsetPointer(*allocated_buffer, indices[index]), element_byte_size);
						}
					}
				}
				else {
					deserialize_size += elements_to_allocate * element_byte_size;
				}
			}
		}

		return deserialize_size;
	}
	
	// -----------------------------------------------------------------------------------------

#pragma region Custom serializers

#pragma region Stream

#define SERIALIZE_CUSTOM_STREAM_VERSION (0)

	size_t SerializeCustomTypeWrite_Stream(SerializeCustomTypeWriteFunctionData* data) {
		ReflectionBasicFieldType basic_type;
		ReflectionStreamFieldType stream_type;
		unsigned int string_offset = 0;

		void* buffer = *(void**)data->data;
		size_t buffer_count = 0;

		if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
			string_offset = sizeof("Stream<") - 1;
			buffer_count = *(size_t*)function::OffsetPointer(data->data, sizeof(void*));
		}
		else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
			string_offset = sizeof("CapacityStream<") - 1;
			buffer_count = *(unsigned int*)function::OffsetPointer(data->data, sizeof(void*));
		}
		else if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
			string_offset = sizeof("ResizableStream<") - 1;
			buffer_count = *(unsigned int*)function::OffsetPointer(data->data, sizeof(void*));
		}
		else {
			ECS_ASSERT(false);
		}

		size_t total_serialize_size = 0;
		if (data->write_data) {
			Write<true>(data->stream, &buffer_count, sizeof(buffer_count));
		}
		else {
			total_serialize_size += Write<false>(data->stream, &buffer_count, sizeof(buffer_count));
		}

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = { data->definition.buffer + string_offset, data->definition.size - string_offset - 1 };
		ReflectionType reflection_type;
		reflection_type.name = { nullptr, 0 };
		unsigned int custom_serializer_index = 0;

		size_t template_type_byte_size = SerializeCustomTypeBasicTypeHelper(template_type, data->reflection_manager, &reflection_type, custom_serializer_index, basic_type, stream_type);
		data->definition = template_type;

		total_serialize_size = SerializeCustomWriteHelper(basic_type, stream_type, &reflection_type, custom_serializer_index, data, { buffer, buffer_count }, template_type_byte_size);
		return total_serialize_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_Stream(SerializeCustomTypeReadFunctionData* data) {
		if (data->version != SERIALIZE_CUSTOM_STREAM_VERSION) {
			return -1;
		}

		ReflectionBasicFieldType basic_type;
		ReflectionStreamFieldType stream_type;
		unsigned int string_offset = 0;

		size_t buffer_count = 0;

		Read<true>(data->stream, &buffer_count, sizeof(buffer_count));

		if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
			string_offset = sizeof("Stream<") - 1;
			size_t* stream_size = (size_t*)function::OffsetPointer(data->data, sizeof(void*));
			*stream_size = buffer_count;
		}
		else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
			string_offset = sizeof("CapacityStream<") - 1;
			unsigned int* stream_size = (unsigned int*)function::OffsetPointer(data->data, sizeof(void*));
			*stream_size = buffer_count;
		}
		else if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
			string_offset = sizeof("ResizableStream<") - 1;
			unsigned int* stream_size = (unsigned int*)function::OffsetPointer(data->data, sizeof(void*));
			*stream_size = buffer_count;
		}
		else {
			ECS_ASSERT(false);
		}

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = { data->definition.buffer + string_offset, data->definition.size - string_offset - 1 };
		ReflectionType reflection_type;
		reflection_type.name = { nullptr, 0 };
		unsigned int custom_serializer_index = 0;

		size_t template_type_byte_size = SerializeCustomTypeBasicTypeHelper(template_type, data->reflection_manager, &reflection_type, custom_serializer_index, basic_type, stream_type);
		data->definition = template_type;

		// It will correctly handle the failure case
		size_t total_deserialize_size = DeserializeCustomReadHelper(
			basic_type,
			stream_type,
			&reflection_type,
			custom_serializer_index,
			data,
			buffer_count,
			buffer_count,
			template_type_byte_size,
			(void**)data->data
		);

		return total_deserialize_size;
	}

#pragma endregion

#pragma region SparseSet

#define SERIALIZE_SPARSE_SET_VERSION (0)

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeWrite_SparseSet(SerializeCustomTypeWriteFunctionData* data) {
		ReflectionBasicFieldType basic_type;
		ReflectionStreamFieldType stream_type;
		unsigned int string_offset = 0;		

		if (memcmp(data->definition.buffer, "SparseSet<", sizeof("SparseSet<") - 1) == 0) {
			string_offset = sizeof("SparseSet<") - 1;
		}
		else if (memcmp(data->definition.buffer, "ResizableSparseSet<", sizeof("ResizableSparseSet<") - 1) == 0) {
			string_offset = sizeof("ResizableSparseSet<") - 1;
		}
		else {
			ECS_ASSERT(false);
		}
		SparseSet<char>* set = (SparseSet<char>*)data->data;
		unsigned int buffer_count = set->size;
		unsigned int buffer_capacity = set->capacity;
		unsigned int first_free = set->first_empty_slot;
		void* buffer = set->buffer;

		size_t total_serialize_size = 0;
		if (data->write_data) {
			Write<true>(data->stream, &buffer_count, sizeof(buffer_count));
			Write<true>(data->stream, &buffer_capacity, sizeof(buffer_capacity));
			Write<true>(data->stream, &first_free, sizeof(first_free));
		}
		else {
			total_serialize_size += Write<false>(data->stream, &buffer_count, sizeof(buffer_count));
			total_serialize_size += Write<false>(data->stream, &buffer_capacity, sizeof(buffer_capacity));
			total_serialize_size += Write<false>(data->stream, &first_free, sizeof(first_free));
		}

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = { data->definition.buffer + string_offset, data->definition.size - string_offset - 1 };
		ReflectionType reflection_type;
		reflection_type.name = { nullptr, 0 };
		unsigned int custom_serializer_index = 0;

		size_t template_type_byte_size = SerializeCustomTypeBasicTypeHelper(template_type, data->reflection_manager, &reflection_type, custom_serializer_index, basic_type, stream_type);
		data->definition = template_type;

		// Now write the user defined data first - or basic data if it is
		total_serialize_size += SerializeCustomWriteHelper(basic_type, stream_type, &reflection_type, custom_serializer_index, data, { buffer, buffer_count }, template_type_byte_size);
		
		// Write the indirection buffer after it because when reading the data the helper will allocate the data
		// such that the indirection buffer can be read directly into the allocation without temporaries
		if (data->write_data) {
			Write<true>(data->stream, set->indirection_buffer, sizeof(uint2) * buffer_capacity);
		}
		else {
			total_serialize_size += Write<false>(data->stream, set->indirection_buffer, sizeof(uint2) * buffer_capacity);
		}
		
		return total_serialize_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_SparseSet(SerializeCustomTypeReadFunctionData* data) {
		if (data->version != SERIALIZE_SPARSE_SET_VERSION) {
			return -1;
		}

		ReflectionBasicFieldType basic_type;
		ReflectionStreamFieldType stream_type;
		unsigned int string_offset = 0;

		size_t total_deserialize_size = 0;

		unsigned int buffer_count = 0;
		unsigned int buffer_capacity = 0;
		unsigned int first_free = 0;

		Read<true>(data->stream, &buffer_count, sizeof(buffer_count));
		Read<true>(data->stream, &buffer_capacity, sizeof(buffer_capacity));
		Read<true>(data->stream, &first_free, sizeof(first_free));

		if (memcmp(data->definition.buffer, "SparseSet<", sizeof("SparseSet<") - 1) == 0) {
			string_offset = sizeof("Stream<") - 1;
		}
		else if (memcmp(data->definition.buffer, "ResizableSparseSet<", sizeof("ResizableSparseSet<") - 1) == 0) {
			string_offset = sizeof("ResizableSparseSet<") - 1;
		}
		else {
			ECS_ASSERT(false);
		}

		SparseSet<char>* set = (SparseSet<char>*)data->data;

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = { data->definition.buffer + string_offset, data->definition.size - string_offset - 1 };
		ReflectionType reflection_type;
		reflection_type.name = { nullptr, 0 };
		unsigned int custom_serializer_index = 0;

		size_t template_type_byte_size = SerializeCustomTypeBasicTypeHelper(template_type, data->reflection_manager, &reflection_type, custom_serializer_index, basic_type, stream_type);
		data->definition = template_type;

		size_t allocation_size = set->MemoryOf(buffer_capacity);
		// If there is a remainder, allocate one more to compensate
		size_t elements_to_allocate = allocation_size / template_type_byte_size + ((allocation_size % template_type_byte_size) != 0);

		void* allocated_buffer = nullptr;

		size_t user_defined_size = DeserializeCustomReadHelper(
			basic_type,
			stream_type,
			&reflection_type,
			custom_serializer_index,
			data,
			buffer_count,
			elements_to_allocate,
			template_type_byte_size,
			&allocated_buffer
		);
		if (user_defined_size == -1) {
			return -1;
		}

		total_deserialize_size += user_defined_size;

		set->InitializeFromBuffer(allocated_buffer, buffer_capacity);

		// Now read the indirection_buffer
		if (data->read_data) {
			Read<true>(data->stream, set->indirection_buffer, sizeof(uint2) * buffer_capacity);
		}
		else {
			total_deserialize_size += Read<false>(data->stream, set->indirection_buffer, sizeof(uint2) * buffer_capacity);
		}

		set->size = buffer_count;
		set->first_empty_slot = first_free;

		return total_deserialize_size;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color

#define SERIALIZE_CUSTOM_COLOR_VERSION (0)

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeWrite_Color(SerializeCustomTypeWriteFunctionData* data) {
		size_t write_size = 0;
		if (data->write_data) {
			Write<true>(data->stream, data->data, sizeof(char) * 4);
		}
		else {
			write_size += Write<false>(data->stream, data->data, sizeof(char) * 4);
		}
		return write_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_Color(SerializeCustomTypeReadFunctionData* data) {
		if (data->version != SERIALIZE_CUSTOM_COLOR_VERSION) {
			return -1;
		}

		if (data->read_data) {
			Read<true>(data->stream, data->data, sizeof(char) * 4);
		}
		else {
			Read<false>(data->stream, data->data, sizeof(char) * 4);
		}
		return 0;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Float

#define SERIALIZE_CUSTOM_COLOR_FLOAT_VERSION (0)

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeWrite_ColorFloat(SerializeCustomTypeWriteFunctionData* data) {
		size_t write_size = 0;
		if (data->write_data) {
			Write<true>(data->stream, data->data, sizeof(float) * 4);
		}
		else {
			write_size += Write<false>(data->stream, data->data, sizeof(float) * 4);
		}
		return write_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_ColorFloat(SerializeCustomTypeReadFunctionData* data) {
		if (data->version != SERIALIZE_CUSTOM_COLOR_FLOAT_VERSION) {
			return -1;
		}

		if (data->read_data) {
			Read<true>(data->stream, data->data, sizeof(float) * 4);
		}
		else {
			Read<false>(data->stream, data->data, sizeof(float) * 4);
		}
		return 0;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

	// -----------------------------------------------------------------------------------------

	SerializeCustomType ECS_SERIALIZE_CUSTOM_TYPES[] = {
		{
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(Streams),
			SerializeCustomTypeWrite_Stream,
			SerializeCustomTypeRead_Stream,
			SERIALIZE_CUSTOM_STREAM_VERSION
		}, 
		{
			ECS_REFLECTION_CONTAINER_TYPE_REFERENCE_COUNTED_ASSET,
			SerializeCustomTypeWriteFunction_ReferenceCountedAsset,
			SerializeCustomTypeReadFunction_ReferenceCountedAsset,
			ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_ASSET_VERSION
		}, 
		{
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(SparseSet),
			SerializeCustomTypeWrite_SparseSet,
			SerializeCustomTypeRead_SparseSet,
			SERIALIZE_SPARSE_SET_VERSION
		},
		{
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(Color),
			SerializeCustomTypeWrite_Color,
			SerializeCustomTypeRead_Color,
			SERIALIZE_CUSTOM_COLOR_VERSION
		},
		{
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(ColorFloat),
			SerializeCustomTypeWrite_ColorFloat,
			SerializeCustomTypeRead_ColorFloat,
			SERIALIZE_CUSTOM_COLOR_FLOAT_VERSION
		}
	};

#pragma endregion

	// -----------------------------------------------------------------------------------------

	unsigned int FindSerializeCustomType(Stream<char> definition)
	{
		ReflectionContainerTypeMatchData match_data = { definition };

		for (unsigned int index = 0; index < std::size(ECS_SERIALIZE_CUSTOM_TYPES); index++) {
			if (ECS_SERIALIZE_CUSTOM_TYPES[index].container_type.match(&match_data)) {
				return index;
			}
		}

		return -1;
	}

	// -----------------------------------------------------------------------------------------

	unsigned int SerializeCustomTypeCount()
	{
		return std::size(ECS_SERIALIZE_CUSTOM_TYPES);
	}

	// -----------------------------------------------------------------------------------------


}