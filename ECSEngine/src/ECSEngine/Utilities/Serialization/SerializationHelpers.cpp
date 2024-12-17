#include "ecspch.h"
#include "SerializationHelpers.h"
#include "Binary/Serialization.h"
#include "../Reflection/ReflectionStringFunctions.h"
#include "../Reflection/Reflection.h"
#include "../Reflection/ReflectionMacros.h"
#include "../Reflection/ReflectionCustomTypes.h"
#include "../ReferenceCountSerialize.h"
#include "../../Resources/AssetMetadataSerialize.h"
#include "../../Containers/SparseSet.h"

#include "../../Allocators/StackAllocator.h"
#include "../../Allocators/MemoryArena.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Allocators/MemoryProtectedAllocator.h"
#include "../../Allocators/ResizableLinearAllocator.h"

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

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomWriteHelper(SerializeCustomWriteHelperData* data) {
		size_t serialize_size = 0;

		size_t element_count = data->indices.buffer == nullptr ? data->data_to_write.size : data->indices.size;
		size_t element_stride = data->element_stride == 0 ? data->definition_info.byte_size : data->element_stride;
		size_t element_byte_size = data->definition_info.byte_size;

		// This lambda helps in hoisting the has indices check outside the loop and calls the functor with a (void* element) parameter
		auto iterate_elements = [data, element_count, element_stride](auto call_functor) {
			if (data->indices.buffer != nullptr) {
				for (size_t index = 0; index < element_count; index++) {
					void* element = OffsetPointer(data->data_to_write.buffer, data->indices[index] * element_stride);
					call_functor(element);
				}
			}
			else {
				for (size_t index = 0; index < element_count; index++) {
					void* element = OffsetPointer(data->data_to_write.buffer, index * element_stride);
					call_functor(element);
				}
			}
		};

		if (data->definition_info.is_blittable) {
			if (data->write_data->write_data) {
				if (data->indices.buffer == nullptr) {
					if (element_stride == element_byte_size) {
						Write<true>(data->write_data->stream, data->data_to_write.buffer, element_count * element_byte_size);
					}
					else {
						// Can't use a single write
						for (size_t index = 0; index < element_count; index++) {
							void* element = OffsetPointer(data->data_to_write.buffer, index * element_stride);
							Write<true>(data->write_data->stream, element, element_byte_size);
						}
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = OffsetPointer(data->data_to_write.buffer, data->indices[index] * element_stride);
						Write<true>(data->write_data->stream, element, element_byte_size);
					}
				}
			}
			else {
				serialize_size += element_count * element_byte_size;
			}
		}
		else {
			// In this else, we know that the type is not blittable, so we must always call the appropriate serialize function

			if (data->definition_info.type != nullptr) {
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
					iterate_elements([data, &options](void* element) {
						Serialize(data->write_data->reflection_manager, data->definition_info.type, element, *data->write_data->stream, &options);
					});
				}
				else {
					iterate_elements([&serialize_size, data, &options](void* element) {
						serialize_size += SerializeSize(data->write_data->reflection_manager, data->definition_info.type, element, &options);
					});
				}
			}
			// If it has a custom serializer functor - use it
			else if (data->definition_info.custom_type_index != -1) {
				// We must change the definition used
				Stream<char> previous_definition = data->write_data->definition;
				data->write_data->definition = data->definition;

				iterate_elements([&serialize_size, data](void* element) {
					data->write_data->data = element;
					serialize_size += ECS_SERIALIZE_CUSTOM_TYPES[data->definition_info.custom_type_index].write(data->write_data);
				});

				data->write_data->definition = previous_definition;
			}
			else {
				if (data->definition_info.field_stream_type != ReflectionStreamFieldType::Basic) {
					// Serialize using known types - basic and stream based ones of primitive types
					// PERFORMANCE TODO: Can we replace the loops with memcpys?
					// If these are basic types, with no user defined ones and no nested streams,
					// We can memcpy directly

					ReflectionFieldInfo field_info;
					field_info.basic_type = data->definition_info.field_basic_type;
					field_info.stream_type = data->definition_info.field_stream_type;

					field_info.stream_byte_size = data->definition_info.field_stream_byte_size;
					size_t stream_offset = data->definition_info.field_stream_type == ReflectionStreamFieldType::ResizableStream ? sizeof(ResizableStream<void>) : sizeof(Stream<void>);

					if (data->write_data->write_data) {
						iterate_elements([&field_info, data](void* element) {
							WriteFundamentalType<true>(field_info, element, *data->write_data->stream);
						});
					}
					else {
						iterate_elements([&field_info, data, &serialize_size](void* element) {
							serialize_size += WriteFundamentalType<false>(field_info, element, *data->write_data->stream);
						});
					}
				}
				else {
					ECS_ASSERT(false, "Critical error in serialization helper write: unknown case (non blittable basic field)");
				}
			}
		}

		return serialize_size;
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
		size_t element_byte_size = data->definition_info.byte_size;

		if (data->definition_info.type != nullptr) {
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

					void* buffer = data->elements_to_allocate > 0 ?
						AllocateEx(backup_allocator, data->elements_to_allocate * element_byte_size, data->definition_info.alignment) : nullptr;
					*data->allocated_buffer = buffer;
					deserialize_size += data->elements_to_allocate * element_byte_size;

					auto loop = [&](auto use_indices) {
						// We need this separate case for the following reason
						// Take this structure for example
						// struct {
						//   unsigned int a;
						//	 unsigned short b;
						// }
						// It has a total byte size of 8, but if it were
						// To be serialized using Serialize, it will write
						// Only 6 bytes into the stream. If we are to use
						// the bulk blittable with byte size of 8, then if
						// We are to use the deserialize, it will be skipping
						// Only 6 bytes per entry, instead of the full 8. So
						// We need to account for this fact
						unsigned int field_table_index = data->read_data->options->field_table->TypeIndex(data->definition_info.type->name);
						ECS_ASSERT(field_table_index != -1, "Corrupt deserialization file");
						bool is_file_blittable = data->read_data->options->field_table->IsBlittable(field_table_index);
						if (is_file_blittable) {
							size_t file_blittable_size = data->read_data->options->field_table->TypeByteSize(field_table_index);
							// Check to see if the type has changed. It hasn't changed, we can memcpy directly
							// If we don't have any indices
							bool is_unchanged = data->read_data->options->field_table->IsUnchanged(
								field_table_index,
								data->read_data->reflection_manager,
								data->definition_info.type
							);

							if constexpr (!use_indices) {
								if (is_unchanged) {
									// Can memcpy directly
									Read<true>(data->read_data->stream, buffer, data->element_count * element_byte_size);
								}
								else {
									// Need to deserialize every element
									// But take into account the file deserialization byte size
									// For the ptr stream offset
									for (size_t index = 0; index < data->element_count; index++) {
										uintptr_t initial_ptr = *data->read_data->stream;
										void* element = OffsetPointer(buffer, element_byte_size * index);
										ECS_DESERIALIZE_CODE code = Deserialize(
											data->read_data->reflection_manager,
											data->definition_info.type,
											element,
											*data->read_data->stream,
											&options
										);
										if (code != ECS_DESERIALIZE_OK) {
											deallocate_buffer();
											return -1;
										}
										*data->read_data->stream = initial_ptr + file_blittable_size;
									}
								}
							}
							else {
								if (is_unchanged) {
									// Can memcpy into each index, but cannot as a whole
									for (size_t index = 0; index < data->element_count; index++) {
										void* element = OffsetPointer(buffer, element_byte_size * data->indices[index]);
										Read<true>(data->read_data->stream, element, file_blittable_size);
									}
								}
								else {
									// Cannot memcpy directly
									for (size_t index = 0; index < data->element_count; index++) {
										uintptr_t initial_ptr = *data->read_data->stream;
										void* element = OffsetPointer(buffer, element_byte_size * data->indices[index]);
										ECS_DESERIALIZE_CODE code = Deserialize(
											data->read_data->reflection_manager,
											data->definition_info.type,
											element,
											*data->read_data->stream,
											&options
										);
										if (code != ECS_DESERIALIZE_OK) {
											deallocate_buffer();
											return -1;
										}
										*data->read_data->stream = initial_ptr + file_blittable_size;
									}
								}
							}
						}
						else {
							for (size_t index = 0; index < data->element_count; index++) {
								size_t offset = index;
								if constexpr (use_indices) {
									offset = data->indices[index];
								}
								void* element = OffsetPointer(buffer, element_byte_size * offset);
								ECS_DESERIALIZE_CODE code = Deserialize(data->read_data->reflection_manager, data->definition_info.type, element, *data->read_data->stream, &options);
								if (code != ECS_DESERIALIZE_OK) {
									deallocate_buffer();
									return -1;
								}
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
						return_val = loop(std::true_type{});
					}

					data->read_data->was_allocated = previous_was_allocated;
					if (return_val == -1) {
						return -1;
					}
				}
				else {
					ECS_DESERIALIZE_CODE code = Deserialize(data->read_data->reflection_manager, data->definition_info.type, data->deserialize_target, *data->read_data->stream, &options);
					if (code != ECS_DESERIALIZE_OK) {
						return -1;
					}
				}
			}
			else {
				bool previous_was_allocated = data->read_data->was_allocated;
				size_t iterate_count = single_instance ? 1 : data->element_count;
				data->read_data->was_allocated = true;
				for (size_t index = 0; index < iterate_count; index++) {
					size_t byte_size = DeserializeSize(data->read_data->reflection_manager, data->definition_info.type, *data->read_data->stream, &options);
					if (byte_size == -1) {
						return -1;
					}
					deserialize_size += byte_size;
				}
				data->read_data->was_allocated = previous_was_allocated;
			}
		}
		else if (data->definition_info.custom_type_index != -1) {
			// We must change the definition of the read data
			Stream<char> previous_definition = data->read_data->definition;
			data->read_data->definition = data->definition;

			if (!single_instance) {
				bool previous_was_allocated = data->read_data->was_allocated;
				data->read_data->was_allocated = true;
				if (data->read_data->read_data) {
					size_t allocate_size = data->elements_to_allocate * element_byte_size;
					void* buffer = allocate_size > 0 ? AllocateEx(backup_allocator, allocate_size, data->definition_info.alignment) : nullptr;
					*data->allocated_buffer = buffer;
					deserialize_size += allocate_size;
				}

				auto loop = [&](auto use_indices) {
					for (size_t index = 0; index < data->element_count; index++) {
						size_t offset = index;
						if constexpr (use_indices) {
							offset = data->indices[index];
						}

						void* element = OffsetPointer(*data->allocated_buffer, element_byte_size * offset);
						data->read_data->data = element;
						size_t byte_size_or_success = ECS_SERIALIZE_CUSTOM_TYPES[data->definition_info.custom_type_index].read(data->read_data);
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
					// Don't forget to restore the previous definition
					data->read_data->definition = previous_definition;
					return -1;
				}
			}
			else {
				data->read_data->data = data->deserialize_target;
				deserialize_size += ECS_SERIALIZE_CUSTOM_TYPES[data->definition_info.custom_type_index].read(data->read_data);
			}

			data->read_data->definition = previous_definition;
		}
		else {
			if (data->definition_info.field_stream_type != ReflectionStreamFieldType::Basic) {
				ReflectionFieldInfo field_info;
				field_info.basic_type = data->definition_info.field_basic_type;
				field_info.stream_type = data->definition_info.field_stream_type;
				field_info.stream_byte_size = data->definition_info.field_stream_byte_size;
				field_info.stream_alignment = data->definition_info.field_stream_alignment;

				AllocatorPolymorphic allocator = { nullptr };
				bool has_options = data->read_data->options != nullptr;
				if (has_options) {
					allocator = data->read_data->options->field_allocator;
				}

				size_t stream_size = data->definition_info.field_stream_type == ReflectionStreamFieldType::ResizableStream ? sizeof(ResizableStream<void>) : sizeof(Stream<void>);

				if (data->read_data->read_data) {
					if (!single_instance) {
						// We don't need to modify the was_allocated field since we are reading fundamental types
						*data->allocated_buffer = data->elements_to_allocate > 0 ?
							AllocateEx(backup_allocator, data->elements_to_allocate * stream_size, field_info.stream_alignment) : nullptr;
						deserialize_size += data->elements_to_allocate * stream_size;

						auto loop = [&](auto use_indices) {
							for (size_t index = 0; index < data->element_count; index++) {
								size_t offset = index;
								if constexpr (use_indices) {
									offset = data->indices[index];
								}

								void* element = OffsetPointer(*data->allocated_buffer, index * stream_size);
								if (data->definition_info.field_stream_type == ReflectionStreamFieldType::ResizableStream) {
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
						size_t allocate_size = data->elements_to_allocate * element_byte_size;
						*data->allocated_buffer = allocate_size > 0 ? AllocateEx(backup_allocator, allocate_size, data->definition_info.alignment) : nullptr;

						if (data->indices.buffer == nullptr) {
							Read<true>(data->read_data->stream, *data->allocated_buffer, allocate_size);
						}
						else {
							for (size_t index = 0; index < data->element_count; index++) {
								Read<true>(data->read_data->stream, OffsetPointer(*data->allocated_buffer, data->indices[index] * element_byte_size), element_byte_size);
							}
						}
					}
					else {
						Read<true>(data->read_data->stream, data->deserialize_target, element_byte_size);
					}
				}
				else {
					size_t count = single_instance ? 1 : data->elements_to_allocate;
					deserialize_size += count * element_byte_size;
				}
			}
		}

		return deserialize_size;
	}

	// -----------------------------------------------------------------------------------------

	void SerializeCustomTypeCopyBlit(Reflection::ReflectionCustomTypeCopyData* data, size_t byte_size)
	{
		memcpy(data->destination, data->source, byte_size);
	}

	// -----------------------------------------------------------------------------------------

	// Returns the field allocator, if the options are specified, else nullptr. You can optionally assert that the options are always specified
	ECS_INLINE static AllocatorPolymorphic GetDeserializeFieldAllocator(const SerializeCustomTypeReadFunctionData* data, bool assert_it_exists) {
		if (data->options != nullptr) {
			return data->options->field_allocator;
		}
		ECS_ASSERT(!assert_it_exists, "Deserialize custom type missing field allocator");
		return { nullptr };
	}

	// Returns the backup allocator, if the options are specified, else nullptr. You can optionally assert that the options are always specified
	ECS_INLINE static AllocatorPolymorphic GetDeserializeBackupAllocator(const SerializeCustomTypeReadFunctionData* data, bool assert_it_exists) {
		if (data->options != nullptr) {
			return data->options->backup_allocator;
		}
		ECS_ASSERT(!assert_it_exists, "Deserialize custom type missing field allocator");
		return { nullptr };
	}
	
	// -----------------------------------------------------------------------------------------

#pragma region Custom serializers

#pragma region Stream

#define SERIALIZE_CUSTOM_STREAM_VERSION (0)

	size_t SerializeCustomTypeWrite_Stream(SerializeCustomTypeWriteFunctionData* data) {
		void* buffer = *(void**)data->data;
		size_t buffer_count = 0;

		if (data->definition.StartsWith(STRING(Stream))) {
			buffer_count = *(size_t*)OffsetPointer(data->data, sizeof(void*));
		}
		else if (data->definition.StartsWith(STRING(CapacityStream))) {
			buffer_count = *(unsigned int*)OffsetPointer(data->data, sizeof(void*));
		}
		else if (data->definition.StartsWith(STRING(ResizableStream))) {
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

		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
		ReflectionType reflection_type;
		reflection_type.name = { nullptr, 0 };
		unsigned int custom_serializer_index = 0;

		SerializeCustomWriteHelperData helper_data;
		helper_data.Set(data, template_type);
		helper_data.data_to_write = { buffer, buffer_count };
		return total_serialize_size + SerializeCustomWriteHelper(&helper_data);
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_Stream(SerializeCustomTypeReadFunctionData* data) {
		if (data->version != SERIALIZE_CUSTOM_STREAM_VERSION) {
			return -1;
		}

		size_t buffer_count = 0;
		Read<true>(data->stream, &buffer_count, sizeof(buffer_count));

		if (data->definition.StartsWith(STRING(Stream))) {
			if (data->read_data) {
				size_t* stream_size = (size_t*)OffsetPointer(data->data, sizeof(void*));
				*stream_size = buffer_count;
			}
		}
		else if (data->definition.StartsWith(STRING(CapacityStream))) {
			if (data->read_data) {
				unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
				*stream_size = buffer_count; // This is the size field
				stream_size[1] = buffer_count; // This is the capacity field
			}
		}
		else if (data->definition.StartsWith(STRING(ResizableStream))) {
			if (data->read_data) {
				unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
				*stream_size = buffer_count; // This is the size field
				stream_size[1] = buffer_count; // This is the capacity field
			}
		}
		else {
			ECS_ASSERT(false);
		}

		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);

		DeserializeCustomReadHelperData helper_data;
		helper_data.Set(data, template_type);
		helper_data.allocated_buffer = (void**)data->data;
		helper_data.element_count = buffer_count;
		helper_data.elements_to_allocate = buffer_count;
		return DeserializeCustomReadHelper(&helper_data);
	}

#pragma endregion

#pragma region SparseSet

#define SERIALIZE_SPARSE_SET_VERSION (0)

	// -----------------------------------------------------------------------------------------
	
	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(SparseSet) {
		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);

		bool* user_data = (bool*)ECS_SERIALIZE_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET].user_data;
		if (user_data != nullptr && *user_data) {
			// Serialize as a stream instead
			ECS_STACK_CAPACITY_STREAM(char, stream_name, 256);
			stream_name.CopyOther("Stream<");
			stream_name.AddStreamAssert(template_type);
			stream_name.AddAssert('>');

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

		SerializeCustomWriteHelperData helper_data;
		helper_data.Set(data, template_type);
		helper_data.data_to_write = { buffer, buffer_count };

		total_serialize_size += SerializeCustomWriteHelper(&helper_data);

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

		DeserializeCustomReadHelperData read_data;
		read_data.Set(data, template_type);

		// We can allocate the buffer only once by using the elements_to_allocate variable
		size_t allocation_size = read_data.definition_info.byte_size * buffer_capacity + sizeof(uint2) * buffer_capacity;
		// If there is a remainder, allocate one more to compensate
		size_t elements_to_allocate = SlotsFor(allocation_size, read_data.definition_info.byte_size);

		read_data.allocated_buffer = &allocated_buffer;
		read_data.element_count = buffer_count;
		read_data.elements_to_allocate = elements_to_allocate;

		size_t user_defined_size = DeserializeCustomReadHelper(&read_data);
		if (user_defined_size == -1) {
			return -1;
		}

		total_deserialize_size += user_defined_size;

		if (data->read_data) {
			SparseSetInitializeUntyped(set, buffer_capacity, read_data.definition_info.byte_size, allocated_buffer);
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

#pragma region Allocator

#define SERIALIZE_CUSTOM_ALLOCATOR_VERSION (0)

	// Returns the total amount of bytes written
	static size_t SerializeCreateBaseAllocatorInfo(SerializeCustomTypeWriteFunctionData* data, const CreateBaseAllocatorInfo& info) {
		size_t write_size = 0;

		write_size += data->Write(&info.allocator_type);
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			write_size += data->Write(&info.linear_capacity);
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			write_size += data->Write(&info.stack_capacity);
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			write_size += data->Write(&info.multipool_block_count);
			write_size += data->Write(&info.multipool_capacity);
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			write_size += data->Write(&info.arena_allocator_count);
			write_size += data->Write(&info.arena_capacity);
			write_size += data->Write(&info.arena_nested_type);
			if (info.arena_nested_type == ECS_ALLOCATOR_MULTIPOOL) {
				write_size += data->Write(&info.arena_multipool_block_count);
			}
		}
		break;
		default:
			ECS_ASSERT(false);
		}

		return write_size;
	}

	// Returns the total amount of bytes read
	static size_t DeserializeCreateBaseAllocatorInfo(SerializeCustomTypeReadFunctionData* data, CreateBaseAllocatorInfo& info) {
		size_t read_size = 0;

		read_size += data->Read(&info.allocator_type);
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			read_size += data->Read(&info.linear_capacity);
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			read_size += data->Read(&info.stack_capacity);
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			read_size += data->Read(&info.multipool_block_count);
			read_size += data->Read(&info.multipool_capacity);
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			read_size += data->Read(&info.arena_allocator_count);
			read_size += data->Read(&info.arena_capacity);
			read_size += data->Read(&info.arena_nested_type);
			if (info.arena_nested_type == ECS_ALLOCATOR_MULTIPOOL) {
				read_size += data->Read(&info.arena_multipool_block_count);
			}
		}
		break;
		default:
			ECS_ASSERT(false);
		}

		return read_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeWrite_Allocator(SerializeCustomTypeWriteFunctionData* data) {
		size_t write_size = 0;

		// We must write as a first byte the type of allocator, such that if it changes, we can choose to abort or not the deserialization
		ECS_ALLOCATOR_TYPE allocator_type = AllocatorTypeFromString(data->definition);
		write_size += WriteDeduce(data->stream, &allocator_type, data->write_data);

		// TODO: Determine if we need more information written, in order to restore fully the allocator
		// For the current usage, this is intended to write only the capacity of the allocator and if required,
		// Extra information like the base allocator types and sizes for a manager type allocator. It is not intended
		// To write an extensive allocator representation, including the allocations that were made, since that is not
		// Needed for the intended purpose. The intented purpose is for this to know how much to allocate when reconstructing
		// It, and then the fields will make allocations from it as needed.

		// This lambda will perform the appropriate serialization, based on the given type
		auto serialize = [data, &write_size](ECS_ALLOCATOR_TYPE allocator_type) {
			switch (allocator_type) {
			case ECS_ALLOCATOR_LINEAR:
			{
				LinearAllocator* allocator = (LinearAllocator*)data->data;
				write_size += data->Write(&allocator->m_capacity);
			}
			break;
			case ECS_ALLOCATOR_STACK:
			{
				StackAllocator* allocator = (StackAllocator*)data->data;
				write_size += data->Write(&allocator->m_capacity);
			}
			break;
			case ECS_ALLOCATOR_MULTIPOOL:
			{
				MultipoolAllocator* allocator = (MultipoolAllocator*)data->data;
				size_t allocator_size = allocator->GetSize();
				unsigned int block_count = allocator->GetBlockCount();
				write_size += data->Write(&allocator_size);
				write_size += data->Write(&block_count);
			}
			break;
			case ECS_ALLOCATOR_ARENA:
			{
				MemoryArena* allocator = (MemoryArena*)data->data;
				CreateBaseAllocatorInfo base_allocator_info = allocator->GetInitialBaseAllocatorInfo();
				unsigned char allocator_count = allocator->m_allocator_count;
				write_size += data->Write(&allocator_count);
				write_size += SerializeCreateBaseAllocatorInfo(data, base_allocator_info);
			}
			break;
			case ECS_ALLOCATOR_MANAGER:
			{
				MemoryManager* allocator = (MemoryManager*)data->data;
				write_size += SerializeCreateBaseAllocatorInfo(data, allocator->GetInitialAllocatorInfo());
				write_size += SerializeCreateBaseAllocatorInfo(data, allocator->m_backup_info);
			}
			break;
			case ECS_ALLOCATOR_RESIZABLE_LINEAR:
			{
				ResizableLinearAllocator* allocator = (ResizableLinearAllocator*)data->data;
				write_size += data->Write(&allocator->m_initial_capacity);
				write_size += data->Write(&allocator->m_backup_size);
			}
			break;
			case ECS_ALLOCATOR_MEMORY_PROTECTED:
			{
				MemoryProtectedAllocator* allocator = (MemoryProtectedAllocator*)data->data;
				write_size += data->Write(&allocator->chunk_size);
				write_size += data->Write(&allocator->linear_allocators);
			}
			break;
			default:
				ECS_ASSERT(false);
			}
		};

		if (allocator_type != ECS_ALLOCATOR_TYPE_COUNT) {
			serialize(allocator_type);
		}
		else {
			// It must be the polymorphic allocator
			AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)data->data;
			
			// Serialize the nested type once more. If the target allocator is nullptr, make the type as COUNT
			// Once more to signal that it is malloc
			ECS_ALLOCATOR_TYPE polymorphic_type = allocator->allocator == nullptr ? ECS_ALLOCATOR_TYPE_COUNT : allocator->allocator_type;
			write_size += data->Write(&polymorphic_type);
			if (polymorphic_type != ECS_ALLOCATOR_TYPE_COUNT) {
				serialize(polymorphic_type);
			}
		}

		return write_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_Allocator(SerializeCustomTypeReadFunctionData* data) {
		if (data->version != SERIALIZE_CUSTOM_ALLOCATOR_VERSION) {
			return -1;
		}

		size_t read_size = 0;

		// Read the first byte. If the type has changed, then abort this
		ECS_ALLOCATOR_TYPE allocator_type;
		Read<true>(data->stream, &allocator_type);
		if (!data->read_data) {
			read_size += sizeof(allocator_type);
		}

		bool is_allocator_matched = allocator_type == AllocatorTypeFromString(data->definition);
		// When we have a mismatch, we must still advance through with the data pointer
		// When retrieving the field allocator, don't assert if we are interested in skipping only the section
		AllocatorPolymorphic field_allocator = GetDeserializeFieldAllocator(data, data->read_data);

		// This lambda will perform the appropriate deserialization, based on the given type.
		// Returns false if there is an error.
		auto deserialize = [data, &read_size, is_allocator_matched, field_allocator](ECS_ALLOCATOR_TYPE allocator_type) {
			switch (allocator_type) {
			case ECS_ALLOCATOR_LINEAR:
			{
				size_t capacity = 0;
				read_size += data->Read(&capacity);
				if (data->read_data && is_allocator_matched) {
					LinearAllocator* allocator = (LinearAllocator*)data->data;
					*allocator = LinearAllocator(AllocateEx(field_allocator, capacity), capacity);
				}
			}
			break;
			case ECS_ALLOCATOR_STACK:
			{
				size_t capacity = 0;
				read_size += data->Read(&capacity);
				if (data->read_data && is_allocator_matched) {
					StackAllocator* allocator = (StackAllocator*)data->data;
					*allocator = StackAllocator(AllocateEx(field_allocator, capacity), capacity);
				}
			}
			break;
			case ECS_ALLOCATOR_MULTIPOOL:
			{
				size_t capacity = 0;
				unsigned int block_count = 0;
				read_size += data->Read(&capacity);
				read_size += data->Read(&block_count);
				if (data->read_data && is_allocator_matched) {
					MultipoolAllocator* allocator = (MultipoolAllocator*)data->data;
					*allocator = MultipoolAllocator(AllocateEx(field_allocator, MultipoolAllocator::MemoryOf(block_count, capacity)), capacity, block_count);
				}
			}
			break;
			case ECS_ALLOCATOR_ARENA:
			{
				CreateBaseAllocatorInfo base_allocator_info;
				unsigned char allocator_count = 0;
				read_size += data->Read(&allocator_count);
				read_size += DeserializeCreateBaseAllocatorInfo(data, base_allocator_info);
				if (data->read_data && is_allocator_matched) {
					MemoryArena* allocator = (MemoryArena*)data->data;
					*allocator = MemoryArena(field_allocator, allocator_count, base_allocator_info);
				}
			}
			break;
			case ECS_ALLOCATOR_MANAGER:
			{
				CreateBaseAllocatorInfo initial_allocator_info;
				CreateBaseAllocatorInfo backup_allocator_info;
				read_size += DeserializeCreateBaseAllocatorInfo(data, initial_allocator_info);
				read_size += DeserializeCreateBaseAllocatorInfo(data, backup_allocator_info);
				if (data->read_data && is_allocator_matched) {
					MemoryManager* allocator = (MemoryManager*)data->data;
					*allocator = MemoryManager(initial_allocator_info, backup_allocator_info, field_allocator);
				}
			}
			break;
			case ECS_ALLOCATOR_RESIZABLE_LINEAR:
			{
				size_t initial_capacity = 0;
				size_t backup_capacity = 0;
				read_size += data->Read(&initial_capacity);
				read_size += data->Read(&backup_capacity);
				if (data->read_data && is_allocator_matched) {
					ResizableLinearAllocator* allocator = (ResizableLinearAllocator*)data->data;
					*allocator = ResizableLinearAllocator(initial_capacity, backup_capacity, field_allocator);
				}
			}
			break;
			case ECS_ALLOCATOR_MEMORY_PROTECTED:
			{
				size_t chunk_size = 0;
				bool linear_allocators = false;
				read_size += ReadDeduce(data->stream, &chunk_size, data->read_data);
				read_size += ReadDeduce(data->stream, &linear_allocators, data->read_data);
				if (data->read_data && is_allocator_matched) {
					MemoryProtectedAllocator* allocator = (MemoryProtectedAllocator*)data->data;
					*allocator = MemoryProtectedAllocator(chunk_size, linear_allocators);
				}
			}
			break;
			default:
				return false;
			}

			return true;
		};

		if (allocator_type != ECS_ALLOCATOR_TYPE_COUNT) {
			if (!deserialize(allocator_type)) {
				return -1;
			}
		}
		else {
			// It must be the polymorphic allocator
			AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)data->data;

			// Serialize the nested type once more. If the target allocator is nullptr, make the type as COUNT
			// Once more to signal that it is malloc
			ECS_ALLOCATOR_TYPE polymorphic_type = ECS_ALLOCATOR_TYPE_COUNT;
			Read<true>(data->stream, &polymorphic_type);
			if (!data->read_data) {
				read_size += sizeof(polymorphic_type);
			}
			if (polymorphic_type != ECS_ALLOCATOR_TYPE_COUNT) {
				if (!deserialize(polymorphic_type)) {
					return -1;
				}
			}
		}

		return read_size;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

#pragma region HashTable

#define SERIALIZE_CUSTOM_HASH_TABLE_VERSION (0)

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeWrite_HashTable(SerializeCustomTypeWriteFunctionData* data) {
		size_t write_size = 0;

		HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);
		ReflectionDefinitionInfo value_definition_info = HashTableGetValueDefinitionInfo(data->reflection_manager, template_arguments);
		ReflectionDefinitionInfo identifier_definition_info = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_arguments);

		const HashTableDefault<char>* hash_table = (const HashTableDefault<char>*)data->data;
		// Write the count and capacity upfront
		write_size += data->Write(&hash_table->m_count);
		write_size += data->Write(&hash_table->m_capacity);

		// Write the metadata buffer as is directly to the stream
		write_size += data->Write(hash_table->m_metadata, hash_table->GetExtendedCapacity() * sizeof(hash_table->m_metadata[0]));

		// We need to branch based on the SoA now
		if (template_arguments.is_soa) {
			// Handle the case when the value is empty, then we don't have to write anything
			if (value_definition_info.byte_size > 0) {
				hash_table->ForEachIndexConst([&](unsigned int index) {
					if (value_definition_info.is_blittable) {
						write_size += data->Write(OffsetPointer(hash_table->m_buffer, value_definition_info.byte_size * (size_t)index), value_definition_info.byte_size);
					}
					else {
						// Need to perform a full serialization
					}
				});
			}
		}
		else {

		}

		return write_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeCustomTypeRead_HashTable(SerializeCustomTypeReadFunctionData* data) {
		//if (data->version != SERIALIZE_CUSTOM_STREAM_VERSION) {
		//	return -1;
		//}

		//unsigned int string_offset = 0;

		//size_t buffer_count = 0;

		//Read<true>(data->stream, &buffer_count, sizeof(buffer_count));

		//if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
		//	string_offset = sizeof("Stream<") - 1;
		//	if (data->read_data) {
		//		size_t* stream_size = (size_t*)OffsetPointer(data->data, sizeof(void*));
		//		*stream_size = buffer_count;
		//	}
		//}
		//else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
		//	string_offset = sizeof("CapacityStream<") - 1;
		//	if (data->read_data) {
		//		unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
		//		*stream_size = buffer_count; // This is the size field
		//		stream_size[1] = buffer_count; // This is the capacity field
		//	}
		//}
		//else if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
		//	string_offset = sizeof("ResizableStream<") - 1;
		//	if (data->read_data) {
		//		unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
		//		*stream_size = buffer_count; // This is the size field
		//		stream_size[1] = buffer_count; // This is the capacity field
		//	}
		//}
		//else {
		//	ECS_ASSERT(false);
		//}

		//// Determine if it is a trivial type - including streams
		//Stream<char> template_type = { data->definition.buffer + string_offset, data->definition.size - string_offset - 1 };

		//DeserializeCustomReadHelperData helper_data;
		//helper_data.allocated_buffer = (void**)data->data;
		//helper_data.data = data;
		//helper_data.definition = template_type;
		//helper_data.element_count = buffer_count;
		//helper_data.elements_to_allocate = buffer_count;
		//return DeserializeCustomReadHelperEx(&helper_data);
		return 0;
	}

#pragma endregion

	// -----------------------------------------------------------------------------------------

	// Must be kept in sync with ECS_REFLECTION_CUSTOM_TYPES
	SerializeCustomType ECS_SERIALIZE_CUSTOM_TYPES[] = {
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(Stream, SERIALIZE_CUSTOM_STREAM_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(ReferenceCounted, ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(SparseSet, SERIALIZE_SPARSE_SET_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(MaterialAsset, ECS_SERIALIZE_CUSTOM_TYPE_MATERIAL_ASSET_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(DataPointer, SERIALIZE_CUSTOM_DATA_POINTER_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(Allocator, SERIALIZE_CUSTOM_ALLOCATOR_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(HashTable, SERIALIZE_CUSTOM_HASH_TABLE_VERSION)
	};

	static_assert(ECS_COUNTOF(ECS_SERIALIZE_CUSTOM_TYPES) == ECS_REFLECTION_CUSTOM_TYPE_COUNT, "Serialize custom types must be maintained in sync with reflection custom types!");

#pragma endregion

	// -----------------------------------------------------------------------------------------

	unsigned int FindSerializeCustomType(Stream<char> definition)
	{
		ReflectionCustomTypeMatchData match_data = { definition };

		for (unsigned int index = 0; index < ECS_COUNTOF(ECS_SERIALIZE_CUSTOM_TYPES); index++) {
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
		return ECS_COUNTOF(ECS_SERIALIZE_CUSTOM_TYPES);
	}

	// -----------------------------------------------------------------------------------------


	void SerializeCustomWriteHelperData::Set(SerializeCustomTypeWriteFunctionData* _write_data, Stream<char> _definition)
	{
		write_data = _write_data;
		definition = _definition;
		definition_info = SearchReflectionDefinitionInfo(write_data->reflection_manager, definition);
	}

	void DeserializeCustomReadHelperData::Set(SerializeCustomTypeReadFunctionData* _read_data, Stream<char> _definition)
	{
		read_data = _read_data;
		definition = _definition;
		definition_info = SearchReflectionDefinitionInfo(read_data->reflection_manager, definition);
	}

	// -----------------------------------------------------------------------------------------

}