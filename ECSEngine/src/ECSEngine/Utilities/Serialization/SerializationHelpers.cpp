#include "ecspch.h"
#include "SerializationHelpers.h"
#include "Binary/Serialization.h"
#include "../Reflection/ReflectionStringFunctions.h"
#include "../Reflection/Reflection.h"
#include "../Reflection/ReflectionMacros.h"
#include "../ReferenceCountSerialize.h"
#include "../../Resources/AssetMetadataSerialize.h"
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

	size_t IgnoreWithSize(uintptr_t* stream)
	{
		size_t size = 0;
		Read<true>(stream, &size, sizeof(size));
		Ignore(stream, size);
		return size;
	}

	// -----------------------------------------------------------------------------------------

	size_t IgnoreWithSizeShort(uintptr_t* stream)
	{
		unsigned short size = 0;
		Read<true>(stream, &size, sizeof(size));
		Ignore(stream, size);
		return size;
	}

	// -----------------------------------------------------------------------------------------

	using namespace Reflection;

	SerializeCustomTypeDeduceTypeHelperResult SerializeCustomTypeDeduceTypeHelper(SerializeCustomTypeDeduceTypeHelperData* data) {
		SerializeCustomTypeDeduceTypeHelperResult result;

		size_t min_size = sizeof("Stream<");
		unsigned int string_offset = 0;

		size_t current_type_byte_size = 0;
		result.stream_type = ReflectionStreamFieldType::Basic;

		auto compare = [&](auto string, size_t byte_size, ReflectionStreamFieldType stream_type) {
			if (memcmp(data->template_type->buffer, string, sizeof(string)) == 0) {
				string_offset = sizeof(string) - 1;
				current_type_byte_size = byte_size;
				result.stream_type = stream_type;
				return true;
			}
			return false;
		};

		if (data->template_type->size < min_size) {
			string_offset = 0;
		}
		else if (!compare("Stream<", sizeof(Stream<void>), ReflectionStreamFieldType::Stream)) {
			if (!compare("CapacityStream<", sizeof(CapacityStream<void>), ReflectionStreamFieldType::CapacityStream)) {
				compare("ResizableStream<", sizeof(ResizableStream<void>), ReflectionStreamFieldType::ResizableStream);
			}
		}
		//else {
		//	// Verify pointers 
		//	Stream<char> asterisk = FindFirstCharacter(template_type, '*');
		//	if (asterisk.buffer != nullptr) {
		//		stream_type = ReflectionStreamFieldType::Pointer;
		//	}
		//}

		ReflectionCustomTypeByteSizeData byte_data;
		byte_data.definition = *data->template_type;
		byte_data.reflection_manager = data->reflection_manager;

		*data->template_type = { data->template_type->buffer + string_offset, data->template_type->size - string_offset - (string_offset > 0) };
		result.basic_type = ConvertStringToBasicFieldType(*data->template_type);

		result.type.name = { nullptr, 0 };
		result.custom_serializer_index = -1;

		// It is not a trivial type - try with a reflected type or with a type from the custom serializers
		if (result.basic_type == ReflectionBasicFieldType::Unknown) {
			// It is not a reflected type
			if (!data->reflection_manager->TryGetType(*data->template_type, result.type)) {
				// Custom serializer
				result.custom_serializer_index = FindSerializeCustomType(*data->template_type);

				// This should not fail
				ECS_ASSERT(result.custom_serializer_index != -1, "Failed to serialize custom stream");
				current_type_byte_size = ECS_REFLECTION_CUSTOM_TYPES[result.custom_serializer_index]->GetByteSize(&byte_data).x;
			}
			else {
				result.basic_type = ReflectionBasicFieldType::UserDefined;
				current_type_byte_size = GetReflectionTypeByteSize(&result.type);
			}
		}
		else {
			current_type_byte_size = GetReflectionBasicFieldTypeByteSize(result.basic_type);
		}

		result.byte_size = current_type_byte_size;
		return result;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomWriteHelper(SerializeCustomWriteHelperData* data) {
		size_t serialize_size = 0;

		size_t element_count = data->indices.buffer == nullptr ? data->data_to_write.size : data->indices.size;

		if (data->reflection_type->name.size > 0) {
			// It is a reflected type
			SerializeOptions options;
			options.write_type_table = false;
			options.verify_dependent_types = false;
			options.write_type_table_tags = false;

			if (data->write_data->options != nullptr) {
				options.error_message = data->write_data->options->error_message;
				options.omit_fields = data->write_data->options->omit_fields;
				options.write_type_table_tags = data->write_data->options->write_type_table_tags;
			}

			if (data->write_data->write_data) {
				// Hoist the indices check outside the for
				if (data->indices.buffer == nullptr) {
					for (size_t index = 0; index < element_count; index++) {
						void* element = OffsetPointer(data->data_to_write.buffer, index * data->element_byte_size);
						// Use the serialize function now
						Serialize(data->write_data->reflection_manager, data->reflection_type, element, *data->write_data->stream, &options);
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = OffsetPointer(data->data_to_write.buffer, data->indices[index] * data->element_byte_size);
						Serialize(data->write_data->reflection_manager, data->reflection_type, element, *data->write_data->stream, &options);
					}
				}
			}
			else {
				// Hoist the indices check outside the for
				if (data->indices.buffer == nullptr) {
					for (size_t index = 0; index < element_count; index++) {
						void* element = OffsetPointer(data->data_to_write.buffer, index * data->element_byte_size);
						serialize_size += SerializeSize(data->write_data->reflection_manager, data->reflection_type, element, &options);
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = OffsetPointer(data->data_to_write.buffer, data->indices[index] * data->element_byte_size);
						serialize_size += SerializeSize(data->write_data->reflection_manager, data->reflection_type, element, &options);
					}
				}
			}
		}
		// It has a custom serializer functor - use it
		else if (data->custom_serializer_index != -1) {
			// Hoist the indices check outside the for
			if (data->indices.buffer == nullptr) {
				for (size_t index = 0; index < element_count; index++) {
					void* element = OffsetPointer(data->data_to_write.buffer, index * data->element_byte_size);
					data->write_data->data = element;
					serialize_size += ECS_SERIALIZE_CUSTOM_TYPES[data->custom_serializer_index].write(data->write_data);
				}
			}
			else {
				for (size_t index = 0; index < element_count; index++) {
					void* element = OffsetPointer(data->data_to_write.buffer, data->indices[index] * data->element_byte_size);
					data->write_data->data = element;
					serialize_size += ECS_SERIALIZE_CUSTOM_TYPES[data->custom_serializer_index].write(data->write_data);
				}
			}
		}
		else {
			if (data->stream_type != ReflectionStreamFieldType::Basic) {
				// Serialize using known types - basic and stream based ones of primitive types
				ReflectionFieldInfo field_info;
				field_info.basic_type = data->basic_type;
				field_info.stream_type = data->stream_type;

				field_info.stream_byte_size = data->element_byte_size;
				size_t stream_offset = data->stream_type == ReflectionStreamFieldType::ResizableStream ? sizeof(ResizableStream<void>) : sizeof(Stream<void>);

				if (data->write_data->write_data) {
					// Hoist the indices check outside the for
					if (data->indices.buffer == nullptr) {
						for (size_t index = 0; index < element_count; index++) {
							void* element = OffsetPointer(
								data->data_to_write.buffer,
								index * stream_offset
							);
							WriteFundamentalType<true>(field_info, element, *data->write_data->stream);
						}
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							void* element = OffsetPointer(
								data->data_to_write.buffer,
								data->indices[index] * stream_offset
							);
							WriteFundamentalType<true>(field_info, element, *data->write_data->stream);
						}
					}
				}
				else {
					// Hoist the indices check outside the for
					if (data->indices.buffer == nullptr) {
						for (size_t index = 0; index < element_count; index++) {
							void* element = OffsetPointer(
								data->data_to_write.buffer,
								index * stream_offset
							);
							serialize_size += WriteFundamentalType<false>(field_info, element, *data->write_data->stream);
						}
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							void* element = OffsetPointer(
								data->data_to_write.buffer,
								data->indices[index] * stream_offset
							);
							serialize_size += WriteFundamentalType<false>(field_info, element, *data->write_data->stream);
						}
					}
				}
			}
			else {
				if (data->write_data->write_data) {
					if (data->indices.buffer == nullptr) {
						Write<true>(data->write_data->stream, data->data_to_write.buffer, element_count * data->element_byte_size);
					}
					else {
						for (size_t index = 0; index < element_count; index++) {
							void* element = OffsetPointer(data->data_to_write.buffer, data->indices[index] * data->element_byte_size);
							Write<true>(data->write_data->stream, element, data->element_byte_size);
						}
					}
				}
				else {
					serialize_size += element_count * data->element_byte_size;
				}
			}
		}

		return serialize_size;
	}

	// -----------------------------------------------------------------------------------------

	SerializeCustomWriteHelperData FillSerializeCustomWriteHelper(const SerializeCustomTypeDeduceTypeHelperResult* result)
	{
		SerializeCustomWriteHelperData value;

		value.basic_type = result->basic_type;
		value.custom_serializer_index = result->custom_serializer_index;
		value.element_byte_size = result->byte_size;
		value.reflection_type = &result->type;
		value.stream_type = result->stream_type;
		
		return value;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomWriteHelperEx(SerializeCustomWriteHelperExData* data)
	{
		SerializeCustomTypeDeduceTypeHelperData helper_data;
		helper_data.reflection_manager = data->write_data->reflection_manager;
		helper_data.template_type = &data->template_type;

		SerializeCustomTypeDeduceTypeHelperResult result = SerializeCustomTypeDeduceTypeHelper(&helper_data);

		SerializeCustomWriteHelperData write_data = FillSerializeCustomWriteHelper(&result);
		write_data.data_to_write = data->data_to_write;
		write_data.element_byte_size = data->element_byte_size == -1 ? result.byte_size : data->element_byte_size;
		write_data.write_data = data->write_data;

		Stream<char> previous_definition = data->write_data->definition;
		data->write_data->definition = data->template_type;
		size_t return_result = SerializeCustomWriteHelper(&write_data);
		data->write_data->definition = previous_definition;
		return return_result;
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeCustomReadHelper(DeserializeCustomReadHelperData* data) {
		size_t deserialize_size = 0;

		AllocatorPolymorphic backup_allocator = { nullptr };

		if (data->override_allocator.allocator != nullptr) {
			backup_allocator = data->override_allocator;
		}
		else if (data->read_data->options != nullptr) {
			backup_allocator = data->read_data->options->backup_allocator;
		}

		AllocatorPolymorphic deallocate_allocator = backup_allocator;
		auto deallocate_buffer = [&]() {
			DeallocateEx(deallocate_allocator, *data->allocated_buffer);
			*data->allocated_buffer = nullptr;
		};

		bool single_instance = data->elements_to_allocate == 0 && data->element_count == 1;

		if (data->reflection_type->name.size > 0) {
			bool has_options = data->read_data->options != nullptr;

			DeserializeOptions options;
			options.read_type_table = false;
			options.verify_dependent_types = false;

			if (has_options) {
				options.backup_allocator = data->read_data->options->backup_allocator;
				options.error_message = data->read_data->options->error_message;
				options.field_allocator = data->read_data->options->field_allocator;
				options.use_resizable_stream_allocator = data->read_data->options->use_resizable_stream_allocator;
				options.fail_if_field_mismatch = data->read_data->options->fail_if_field_mismatch;
				options.field_table = data->read_data->options->field_table;
				options.omit_fields = data->read_data->options->omit_fields;
				options.deserialized_field_manager = data->read_data->options->deserialized_field_manager;
				options.version = data->read_data->options->version;
			}

			if (data->read_data->read_data) {
				// Allocate the data before
				if (!single_instance) {
					bool previous_was_allocated = data->read_data->was_allocated;
					data->read_data->was_allocated = true;

					void* buffer = data->elements_to_allocate > 0 ? AllocateEx(backup_allocator, data->elements_to_allocate * data->element_byte_size) : nullptr;
					*data->allocated_buffer = buffer;
					deserialize_size += data->elements_to_allocate * data->element_byte_size;

					auto loop = [&](auto use_indices) {
						for (size_t index = 0; index < data->element_count; index++) {
							size_t offset = index;
							if constexpr (use_indices) {
								offset = data->indices[index];
							}
							void* element = OffsetPointer(buffer, data->element_byte_size * offset);
							ECS_DESERIALIZE_CODE code = Deserialize(data->read_data->reflection_manager, data->reflection_type, element, *data->read_data->stream, &options);
							if (code != ECS_DESERIALIZE_OK) {
								deallocate_buffer();
								return -1;
							}
						}
						return 0;
					};

					int return_val;
					// Hoist the if check outside the for
					if (data->indices.buffer == nullptr) {
						return_val = loop(std::false_type{});
					}
					else {
						return_val =loop(std::true_type{});
					}

					data->read_data->was_allocated = previous_was_allocated;
					if (return_val == -1) {
						return -1;
					}
				}
				else {
					ECS_DESERIALIZE_CODE code = Deserialize(data->read_data->reflection_manager, data->reflection_type, data->deserialize_target, *data->read_data->stream, &options);
					if (code != ECS_DESERIALIZE_OK) {
						return -1;
					}
				}
			}
			else {
				bool previous_was_allocated = data->read_data->was_allocated;
				data->read_data->was_allocated = true;
				size_t iterate_count = single_instance ? 1 : data->element_count;
				for (size_t index = 0; index < iterate_count; index++) {
					size_t byte_size = DeserializeSize(data->read_data->reflection_manager, data->reflection_type, *data->read_data->stream, &options);
					if (byte_size == -1) {
						return -1;
					}
					deserialize_size += byte_size;
				}
				data->read_data->was_allocated = previous_was_allocated;
			}
		}
		else if (data->custom_serializer_index != -1) {
			if (!single_instance) {
				bool previous_was_allocated = data->read_data->was_allocated;
				data->read_data->was_allocated = true;
				if (data->read_data->read_data) {
					void* buffer = AllocateEx(backup_allocator, data->elements_to_allocate * data->element_byte_size);
					*data->allocated_buffer = buffer;
					deserialize_size += data->elements_to_allocate * data->element_byte_size;
				}

				auto loop = [&](auto use_indices) {
					for (size_t index = 0; index < data->element_count; index++) {
						size_t offset = index;
						if constexpr (use_indices) {
							offset = data->indices[index];
						}

						void* element = OffsetPointer(*data->allocated_buffer, data->element_byte_size * offset);
						data->read_data->data = element;
						size_t byte_size_or_success = ECS_SERIALIZE_CUSTOM_TYPES[data->custom_serializer_index].read(data->read_data);
						if (byte_size_or_success == -1) {
							if (data->read_data->read_data) {
								deallocate_buffer();
								return -1;
							}
						}

						deserialize_size += byte_size_or_success;
					}
					return 0;
				};

				int return_val;
				if (data->indices.buffer == nullptr) {
					return_val = loop(std::false_type{});
				}
				else {
					return_val = loop(std::true_type{});
				}

				data->read_data->was_allocated = previous_was_allocated;
				if (return_val == -1) {
					return -1;
				}
			}
			else {
				data->read_data->data = data->deserialize_target;
				deserialize_size += ECS_SERIALIZE_CUSTOM_TYPES[data->custom_serializer_index].read(data->read_data);
			}
		}
		else {
			if (data->stream_type != ReflectionStreamFieldType::Basic) {
				ReflectionFieldInfo field_info;
				field_info.basic_type = data->basic_type;
				field_info.stream_type = data->stream_type;
				field_info.stream_byte_size = data->element_byte_size;

				AllocatorPolymorphic allocator = { nullptr };
				bool has_options = data->read_data->options != nullptr;
				if (has_options) {
					allocator = data->read_data->options->field_allocator;
				}

				size_t stream_size = data->stream_type == ReflectionStreamFieldType::ResizableStream ? sizeof(ResizableStream<void>) : sizeof(Stream<void>);

				if (data->read_data->read_data) {
					if (!single_instance) {
						// We don't need to modify the was_allocated field since we are reading fundamental types
						*data->allocated_buffer = data->elements_to_allocate > 0 ? AllocateEx(backup_allocator, data->elements_to_allocate * stream_size) : nullptr;
						deserialize_size += data->elements_to_allocate * stream_size;

						auto loop = [&](auto use_indices) {
							for (size_t index = 0; index < data->element_count; index++) {
								size_t offset = index;
								if constexpr (use_indices) {
									offset = data->indices[index];
								}

								void* element = OffsetPointer(*data->allocated_buffer, index * stream_size);
								if (data->stream_type == ReflectionStreamFieldType::ResizableStream) {
									if (has_options && data->read_data->options->use_resizable_stream_allocator) {
										allocator = ((ResizableStream<void>*)element)->allocator;
									}
								}

								// The zero is for basic type arrays. Should not really happen
								ReadOrReferenceFundamentalType<true, true>(field_info, element, *data->read_data->stream, 0, allocator, true);
							}
						};

						if (data->indices.buffer == nullptr) {
							loop(std::false_type{});
						}
						else {
							loop(std::true_type{});
						}
					}
				}
				else {
					size_t iterate_count = single_instance ? 1 : data->element_count;
					for (size_t index = 0; index < iterate_count; index++) {
						void* element = OffsetPointer(*data->allocated_buffer, index * stream_size);

						// The zero is for basic type arrays. Should not really happen
						deserialize_size += ReadOrReferenceFundamentalType<false>(field_info, element, *data->read_data->stream, 0, allocator, true);
					}
				}
			}
			else {
				if (data->read_data->read_data) {
					if (!single_instance) {
						// No need to change the was_allocated field since we are reading blittable types
						*data->allocated_buffer = AllocateEx(backup_allocator, data->elements_to_allocate * data->element_byte_size);

						if (data->indices.buffer == nullptr) {
							Read<true>(data->read_data->stream, *data->allocated_buffer, data->element_count * data->element_byte_size);
						}
						else {
							for (size_t index = 0; index < data->element_count; index++) {
								Read<true>(data->read_data->stream, OffsetPointer(*data->allocated_buffer, data->indices[index] * data->element_byte_size), data->element_byte_size);
							}
						}
					}
					else {
						Read<true>(data->read_data->stream, data->deserialize_target, data->element_byte_size);
					}
				}
				else {
					size_t count = single_instance ? 1 : data->elements_to_allocate;
					deserialize_size += count * data->element_byte_size;
				}
			}
		}

		return deserialize_size;
	}

	// -----------------------------------------------------------------------------------------

	DeserializeCustomReadHelperData FillDeserializeCustomReadHelper(const SerializeCustomTypeDeduceTypeHelperResult* result)
	{
		DeserializeCustomReadHelperData value;

		value.basic_type = result->basic_type;
		value.custom_serializer_index = result->custom_serializer_index;
		value.element_byte_size = result->byte_size;
		value.reflection_type = &result->type;
		value.stream_type = result->stream_type;

		return value;
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeCustomReadHelperEx(DeserializeCustomReadHelperExData* data)
	{
		SerializeCustomTypeDeduceTypeHelperData helper_data;
		helper_data.reflection_manager = data->data->reflection_manager;
		helper_data.template_type = &data->definition;

		SerializeCustomTypeDeduceTypeHelperResult result = SerializeCustomTypeDeduceTypeHelper(&helper_data);
		
		Stream<char> previous_definition = data->data->definition;
		data->data->definition = data->definition;

		// Copy the results into this structure
		DeserializeCustomReadHelperData deserialize_data = FillDeserializeCustomReadHelper(&result);
		deserialize_data.elements_to_allocate = data->elements_to_allocate;
		deserialize_data.element_count = data->element_count;
		deserialize_data.read_data = data->data;
		deserialize_data.allocated_buffer = data->allocated_buffer;

		size_t return_result = DeserializeCustomReadHelper(&deserialize_data);
		// Restore the previous definition
		data->data->definition = previous_definition;
		return return_result;
	}

	// -----------------------------------------------------------------------------------------

	void SerializeCustomTypeCopyBlit(Reflection::ReflectionCustomTypeCopyData* data, size_t byte_size)
	{
		memcpy(data->destination, data->source, byte_size);
	}
	
	// -----------------------------------------------------------------------------------------

#pragma region Custom serializers

#pragma region Stream

#define SERIALIZE_CUSTOM_STREAM_VERSION (0)

	size_t SerializeCustomTypeWrite_Stream(SerializeCustomTypeWriteFunctionData* data) {
		unsigned int string_offset = 0;

		void* buffer = *(void**)data->data;
		size_t buffer_count = 0;

		if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
			string_offset = sizeof("Stream<") - 1;
			buffer_count = *(size_t*)OffsetPointer(data->data, sizeof(void*));
		}
		else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
			string_offset = sizeof("CapacityStream<") - 1;
			buffer_count = *(unsigned int*)OffsetPointer(data->data, sizeof(void*));
		}
		else if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
			string_offset = sizeof("ResizableStream<") - 1;
			buffer_count = *(unsigned int*)OffsetPointer(data->data, sizeof(void*));
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

		// Leave the byte size to -1 in order to use the return from the deduce helper
		SerializeCustomWriteHelperExData helper_data;
		helper_data.data_to_write = { buffer, buffer_count };
		helper_data.template_type = template_type;
		helper_data.write_data = data;
		return total_serialize_size + SerializeCustomWriteHelperEx(&helper_data);
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_Stream(SerializeCustomTypeReadFunctionData* data) {
		if (data->version != SERIALIZE_CUSTOM_STREAM_VERSION) {
			return -1;
		}

		unsigned int string_offset = 0;

		size_t buffer_count = 0;

		Read<true>(data->stream, &buffer_count, sizeof(buffer_count));

		if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
			string_offset = sizeof("Stream<") - 1;
			size_t* stream_size = (size_t*)OffsetPointer(data->data, sizeof(void*));
			*stream_size = buffer_count;
		}
		else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
			string_offset = sizeof("CapacityStream<") - 1;
			unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
			*stream_size = buffer_count;
		}
		else if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
			string_offset = sizeof("ResizableStream<") - 1;
			unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
			*stream_size = buffer_count;
		}
		else {
			ECS_ASSERT(false);
		}

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = { data->definition.buffer + string_offset, data->definition.size - string_offset - 1 };

		DeserializeCustomReadHelperExData helper_data;
		helper_data.allocated_buffer = (void**)data->data;
		helper_data.data = data;
		helper_data.definition = template_type;
		helper_data.element_count = buffer_count;
		helper_data.elements_to_allocate = buffer_count;
		return DeserializeCustomReadHelperEx(&helper_data);
	}

#pragma endregion

#pragma region SparseSet

#define SERIALIZE_SPARSE_SET_VERSION (0)

	// -----------------------------------------------------------------------------------------
	
	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(SparseSet) {
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

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = { data->definition.buffer + string_offset, data->definition.size - string_offset - 1 };

		bool* user_data = (bool*)ECS_SERIALIZE_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET].user_data;
		if (user_data != nullptr && *user_data) {
			// Serialize as a stream instead
			ECS_STACK_CAPACITY_STREAM(char, stream_name, 256);
			stream_name.CopyOther("Stream<");
			stream_name.AddStream(template_type);
			stream_name.Add('>');

			// It doesn't matter the type, it will always give the buffer and the size
			SparseSet<char>* set = (SparseSet<char>*)data->data;
			Stream<char> stream = set->ToStream();
			data->data = &stream;

			data->definition = stream_name;
			return SerializeCustomTypeWrite_Stream(data);
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

		SerializeCustomWriteHelperExData helper_data;
		helper_data.data_to_write = { buffer, buffer_count };
		helper_data.write_data = data;
		helper_data.template_type = template_type;

		total_serialize_size += SerializeCustomWriteHelperEx(&helper_data);

		//size_t template_type_byte_size = SerializeCustomTypeDeduceTypeHelper(template_type, data->reflection_manager, &reflection_type, custom_serializer_index, basic_type, stream_type);
		//data->definition = template_type;

		//// Now write the user defined data first - or basic data if it is
		//total_serialize_size += SerializeCustomWriteHelper(basic_type, stream_type, &reflection_type, custom_serializer_index, data, { buffer, buffer_count }, template_type_byte_size);

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

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(SparseSet) {
		if (data->version != SERIALIZE_SPARSE_SET_VERSION) {
			return -1;
		}

		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);

		// Determine if it is a trivial type - including streams
		SparseSet<char>* set = (SparseSet<char>*)data->data;

		unsigned int buffer_count = 0;
		unsigned int buffer_capacity = 0;
		unsigned int first_free = 0;

		bool* user_data = (bool*)ECS_SERIALIZE_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET].user_data;
		if (user_data != nullptr && *user_data) {
			size_t stream_size = 0;
			Read<true>(data->stream, &stream_size, sizeof(stream_size));
			buffer_count = stream_size;
			buffer_capacity = buffer_count;
			first_free = 0;
		}
		else {
			Read<true>(data->stream, &buffer_count, sizeof(buffer_count));
			Read<true>(data->stream, &buffer_capacity, sizeof(buffer_capacity));
			Read<true>(data->stream, &first_free, sizeof(first_free));
		}

		size_t total_deserialize_size = 0;

		void* allocated_buffer = nullptr;

		SerializeCustomTypeDeduceTypeHelperData deduce_data;
		deduce_data.reflection_manager = data->reflection_manager;
		deduce_data.template_type = &template_type;

		SerializeCustomTypeDeduceTypeHelperResult result = SerializeCustomTypeDeduceTypeHelper(&deduce_data);
		data->definition = template_type;

		// We can allocate the buffer only once by using the elements_to_allocate variable
		size_t allocation_size = result.byte_size * buffer_capacity + sizeof(uint2) * buffer_capacity;
		// If there is a remainder, allocate one more to compensate
		size_t elements_to_allocate = allocation_size / result.byte_size + ((allocation_size % result.byte_size) != 0);

		DeserializeCustomReadHelperData read_data = FillDeserializeCustomReadHelper(&result);
		read_data.allocated_buffer = &allocated_buffer;
		read_data.read_data = data;
		read_data.element_count = buffer_count;
		read_data.elements_to_allocate = elements_to_allocate;

		size_t user_defined_size = DeserializeCustomReadHelper(&read_data);
		if (user_defined_size == -1) {
			return -1;
		}

		total_deserialize_size += user_defined_size;

		if (data->read_data) {
			SparseSetInitializeUntyped(set, buffer_capacity, result.byte_size, allocated_buffer);
		}

		if (user_data != nullptr && *user_data) {
			if (data->read_data) {
				// No indirection buffer was written. In order to preserve the invariance we need to allocate the elements
				for (unsigned int index = 0; index < buffer_count; index++) {
					set->Allocate();
				}
			}
		}
		else {
			// Now read the indirection_buffer
			if (data->read_data) {
				Read<true>(data->stream, set->indirection_buffer, sizeof(uint2) * buffer_capacity);

				set->size = buffer_count;
				set->first_empty_slot = first_free;
			}
			else {
				total_deserialize_size += Read<false>(data->stream, set->indirection_buffer, sizeof(uint2) * buffer_capacity);
			}
		}

		return total_deserialize_size;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

#pragma region Data Pointer

#define SERIALIZE_CUSTOM_DATA_POINTER_VERSION (0)

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(DataPointer) {
		size_t write_size = 0;
		const DataPointer* pointer = (const DataPointer*)data->data;
		unsigned short byte_size = pointer->GetData();
		const void* ptr = pointer->GetPointer();

		return WriteWithSizeShort(data->stream, ptr, byte_size, data->write_data);
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(DataPointer) {
		if (data->version != SERIALIZE_CUSTOM_DATA_POINTER_VERSION) {
			return -1;
		}

		DataPointer* data_pointer = (DataPointer*)data->data;

		unsigned short byte_size = 0;
		Read<true>(data->stream, &byte_size, sizeof(byte_size));

		if (data->read_data) {
			void* pointer = nullptr;

			if (data->options->field_allocator.allocator != nullptr) {
				void* allocation = AllocateEx(data->options->field_allocator, byte_size);
				Read<true>(data->stream, allocation, byte_size);
				pointer = allocation;
			}
			else {
				ReferenceData<true>(data->stream, &pointer, byte_size);
			}

			data_pointer->SetPointer(pointer);
			data_pointer->SetData(byte_size);
		}
		else {
			Read<false>(data->stream, nullptr, byte_size);
		}
		return byte_size;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

	// -----------------------------------------------------------------------------------------

	// Must be kept in sync with ECS_REFLECTION_CUSTOM_TYPES
	SerializeCustomType ECS_SERIALIZE_CUSTOM_TYPES[] = {
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(Stream, SERIALIZE_CUSTOM_STREAM_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(ReferenceCounted, ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(SparseSet, SERIALIZE_SPARSE_SET_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(MaterialAsset, ECS_SERIALIZE_CUSTOM_TYPE_MATERIAL_ASSET_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(DataPointer, SERIALIZE_CUSTOM_DATA_POINTER_VERSION)
	};

#pragma endregion

	// -----------------------------------------------------------------------------------------

	unsigned int FindSerializeCustomType(Stream<char> definition)
	{
		ReflectionCustomTypeMatchData match_data = { definition };

		for (unsigned int index = 0; index < std::size(ECS_SERIALIZE_CUSTOM_TYPES); index++) {
			if (ECS_REFLECTION_CUSTOM_TYPES[index]->Match(&match_data)) {
				return index;
			}
		}

		return -1;
	}

	// -----------------------------------------------------------------------------------------

	void SetSerializeCustomTypeUserData(unsigned int index, void* buffer)
	{
		ECS_SERIALIZE_CUSTOM_TYPES[index].user_data = buffer;
	}

	// -----------------------------------------------------------------------------------------

	void ClearSerializeCustomTypeUserData(unsigned int index)
	{
		ECS_SERIALIZE_CUSTOM_TYPES[index].user_data = nullptr;
	}

	// -----------------------------------------------------------------------------------------

	bool SetSerializeCustomTypeSwitch(unsigned int index, unsigned char switch_index, bool new_status) 
	{
		ECS_ASSERT(switch_index < ECS_SERIALIZE_CUSTOM_TYPE_SWITCH_CAPACITY);
		bool old_status = ECS_SERIALIZE_CUSTOM_TYPES[index].switches[switch_index];
		ECS_SERIALIZE_CUSTOM_TYPES[index].switches[switch_index] = new_status;
		return old_status;
	}

	// -----------------------------------------------------------------------------------------

	void SetSerializeCustomSparsetSet()
	{
		static bool true_ = true;
		SetSerializeCustomTypeUserData(FindSerializeCustomType("SparseSet<char>"), &true_);
	}

	// -----------------------------------------------------------------------------------------

	unsigned int SerializeCustomTypeCount()
	{
		return std::size(ECS_SERIALIZE_CUSTOM_TYPES);
	}

	// -----------------------------------------------------------------------------------------


}