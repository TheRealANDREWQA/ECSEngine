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
#include "../../Containers/Deck.h"

#include "../../Allocators/StackAllocator.h"
#include "../../Allocators/MemoryArena.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Allocators/MemoryProtectedAllocator.h"
#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Allocators/MallocAllocator.h"
#include "../ReaderWriterInterface.h"

#include "SerializeIntVariableLength.h"

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

	bool SerializeCustomWriteHelper(SerializeCustomWriteHelperData* data) {
		WriteInstrument* write_instrument = data->write_data->write_instrument;

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

		bool success = true;
		if (data->definition_info.is_blittable) {
			if (!data->write_data->write_instrument->IsSizeDetermination()) {
				if (data->indices.buffer == nullptr) {
					if (element_stride == element_byte_size) {
						success &= write_instrument->Write(data->data_to_write.buffer, element_count * element_byte_size);
					}
					else {
						// Can't use a single write
						for (size_t index = 0; index < element_count; index++) {
							void* element = OffsetPointer(data->data_to_write.buffer, index * element_stride);
							success &= write_instrument->Write(element, element_byte_size);
						}
					}
				}
				else {
					for (size_t index = 0; index < element_count; index++) {
						void* element = OffsetPointer(data->data_to_write.buffer, data->indices[index] * element_stride);
						success &= write_instrument->Write(element, element_byte_size);
					}
				}
			}
			else {
				// Can use nullptr as source, since it is not actually reading anything
				success &= write_instrument->Write(nullptr, element_count * element_byte_size);
			}
		}
		else {
			// In this else, we know that the type is not blittable, so we must always call the appropriate serialize function

			if (data->definition_info.type != nullptr) {
				// It is a reflected type
				SerializeOptions options;
				if (data->write_data->options != nullptr) {
					options = *data->write_data->options;
				}
				options.write_type_table = false;
				options.verify_dependent_types = false;
				options.write_type_table_tags = false;

				iterate_elements([data, &options, &success](void* element) {
					success &= Serialize(data->write_data->reflection_manager, data->definition_info.type, element, data->write_data->write_instrument, &options) == ECS_SERIALIZE_OK;
				});
			}
			// If it has a custom serializer functor - use it
			else if (data->definition_info.custom_type_index != -1) {
				// We must change the definition used
				Stream<char> previous_definition = data->write_data->definition;
				data->write_data->definition = data->definition;

				iterate_elements([data, &success](void* element) {
					data->write_data->data = element;
					success &= ECS_SERIALIZE_CUSTOM_TYPES[data->definition_info.custom_type_index].write(data->write_data);
				});

				data->write_data->definition = previous_definition;
			}
			else {
				if (data->definition_info.field_stream_type != ReflectionStreamFieldType::Basic) {
					if (data->definition_info.field_stream_type == ReflectionStreamFieldType::Pointer) {
						// Determine if this is a reference pointer
						Stream<char> pointer_reference_key;
						Stream<char> pointer_reference_custom_element_name;
						if (GetReflectionPointerAsReferenceParams(data->tags, pointer_reference_key, pointer_reference_custom_element_name)) {
							// We have something to write only if the key is specified
							if (pointer_reference_key.size > 0) {
								if (!write_instrument->IsSizeDetermination()) {
									// Retrieve the token
									ECS_ASSERT(data->write_data->options != nullptr, "Serializing a reference pointer failed");
									iterate_elements([data, pointer_reference_key, pointer_reference_custom_element_name, write_instrument, &success](void* element) {
										ReflectionCustomTypeGetElementIndexOrToken token = data->write_data->options->passdown_info->GetPointerTargetToken(
											data->write_data->reflection_manager,
											pointer_reference_key,
											pointer_reference_custom_element_name,
											*(void**)element,
											true
										);
										ECS_ASSERT(token != -1);
										success &= write_instrument->Write(&token);
									});
								}
								else {
									// Can use nullptr as source, since it won't be reading anything
									success &= write_instrument->Write(nullptr, sizeof(ReflectionCustomTypeGetElementIndexOrToken) * element_count);
								}
							}
						}
						else {
							if (data->definition_info.field_basic_type == ReflectionBasicFieldType::UserDefined) {
								// Determine the definition without the asterisk
								Stream<char> pointee_definition = data->definition;
								while (pointee_definition.size > 0 && pointee_definition.Last() == '*') {
									pointee_definition.size--;
								}

								// Use the general serialize function - based on the definition
								SerializeOptions options;
								if (data->write_data->options != nullptr) {
									options = *data->write_data->options;
								}
								options.write_type_table = false;
								options.verify_dependent_types = false;
								options.write_type_table_tags = false;


								iterate_elements([data, pointee_definition, &options, &success, write_instrument](void* element) {
									success &= SerializeEx(data->write_data->reflection_manager, pointee_definition, *(void**)element, write_instrument, &options, data->tags);
								});
							}
							else {
								// The Int8 and Wchar_t are special cases, handle them as null terminated strings
								if (data->definition_info.field_basic_type == ReflectionBasicFieldType::Int8) {
									iterate_elements([data, write_instrument, &success](void* element) {
										const char* string = *(const char**)element;
										size_t string_length = strlen(string) + 1;
										success &= write_instrument->WriteWithSize<size_t>(Stream<void>(string, string_length * sizeof(char)));
									});
								}
								else if (data->definition_info.field_basic_type == ReflectionBasicFieldType::Wchar_t) {
									iterate_elements([data, write_instrument, &success](void* element) {
										const wchar_t* string = *(const wchar_t**)element;
										size_t string_length = wcslen(string) + 1;
										success &= write_instrument->WriteWithSize<size_t>(Stream<void>(string, string_length * sizeof(wchar_t)));
									});
								}
								else {
									// Get the byte size and write it directly
									size_t target_byte_size = GetReflectionBasicFieldTypeByteSize(data->definition_info.field_basic_type);
									if (!write_instrument->IsSizeDetermination()) {
										iterate_elements([data, target_byte_size, &success, write_instrument](void* element) {
											success &= write_instrument->Write(*(void**)element, target_byte_size);
										});
									}
									else {
										success &= write_instrument->Write(nullptr, element_count * target_byte_size);
									}
								}
							}
						}
					}
					else {
						// Serialize using known types - basic and stream based ones of primitive types
						// PERFORMANCE TODO: Can we replace the loops with memcpys?
						// If these are basic types, with no user defined ones and no nested streams,
						// We can memcpy directly
						ECS_ASSERT(data->definition_info.field_basic_type != ReflectionBasicFieldType::UserDefined);

						ReflectionField field = data->definition_info.GetBasicField();
						iterate_elements([&field, data, &success, write_instrument](void* element) {
							success &= WriteFundamentalType(field.info, element, write_instrument);
						});
					}
				}
				else {
					ECS_ASSERT(false, "Critical error in serialization helper write: unknown case (non blittable basic field)");
				}
			}
		}

		return success;
	}

	// -----------------------------------------------------------------------------------------

	bool DeserializeCustomReadHelper(DeserializeCustomReadHelperData* data) {
		bool success = true;

		ReadInstrument* read_instrument = data->read_data->read_instrument;
		AllocatorPolymorphic field_allocator = ECS_MALLOC_ALLOCATOR;

		if (data->override_allocator.allocator != nullptr) {
			field_allocator = data->override_allocator;
		}
		else if (data->read_data->options != nullptr && data->read_data->options->field_allocator.allocator != nullptr) {
			field_allocator = data->read_data->options->field_allocator;
		}

		// This will be needed for the IgnoreDeserialize calls, which take place only if ignore data is set to true
		DeserializeFieldTableOptions ignore_data_table_options;
		bool ignore_data = data->read_data->ShouldIgnoreData();
		if (ignore_data) {
			// Ensure that the field table and the deserialized field manager are specified, since they will be needed down the line
			ECS_ASSERT(data->read_data->options != nullptr && data->read_data->options->field_table != nullptr && 
				data->read_data->options->field_table->types.size > 0 && data->read_data->options->deserialized_field_manager);
			ignore_data_table_options.read_type_tags = data->read_data->options->read_type_table_tags;
		}
		
		AllocatorPolymorphic deallocate_allocator = field_allocator;
		auto deallocate_buffer = [&]() {
			Deallocate(deallocate_allocator, *data->allocated_buffer);
			*data->allocated_buffer = nullptr;
		};

		bool single_instance = data->elements_to_allocate == 0 && data->element_count == 1;
		size_t element_byte_size = data->definition_info.byte_size;

		// For the common path where there is no single instance, allocate the buffer now
		bool previous_was_allocated = data->read_data->was_allocated;
		if (!single_instance) {
			if (!ignore_data) {
				data->read_data->was_allocated = true;
				void* buffer = data->elements_to_allocate > 0 ?
					Allocate(field_allocator, data->elements_to_allocate * element_byte_size, data->definition_info.alignment) : nullptr;
				*data->allocated_buffer = buffer;
			}
		}
		// Keep a local stack value since for the ignore case we don't actually want to dereference the allocated buffer
		void* allocated_pointer = ignore_data ? nullptr : *data->allocated_buffer;

		// The functor will be called with a (void* element) as an argument
		// And it should return true if the iteration was successful, else false
		// Returns true if all iterations succeeded, else false
		auto iterate_elements = [&](auto has_indices, auto call_functor) -> bool {
			if (!single_instance) {
				for (size_t index = 0; index < data->element_count; index++) {
					size_t offset = index;
					if constexpr (has_indices) {
						offset = data->indices[index];
					}
					void* element = OffsetPointer(allocated_pointer, element_byte_size * index);
					if (!call_functor(element)) {
						return false;
					}
				}

				return true;
			}
			else {
				return call_functor(data->deserialize_target);
			}
		};

		if (data->definition_info.type != nullptr) {
			bool has_options = data->read_data->options != nullptr;

			DeserializeOptions options;
			if (has_options) {
				options = *data->read_data->options;
			}

			options.read_type_table = false;
			options.verify_dependent_types = false;
			options.header = {};
			options.validate_header = false;
			options.validate_header_data = nullptr;

			// The following code block will work for size determination as well

			// Allocate the data before
			if (!single_instance) {
				// Cannot use the general loop iteration because of a particularity inside
				// The is_unchanged branch
				auto loop = [&](auto use_indices) {
					bool success = true;

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
					if (field_table_index == -1) {
						// Fail if the type could not be found in the field table
						if (!ignore_data) {
							deallocate_buffer();
						}
						return false;
					}

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

						if (ignore_data) {
							// If we have to ignore the data, we can simply ignore everything at once
							success &= read_instrument->Ignore(data->element_count * file_blittable_size);
						}
						else {
							if constexpr (!use_indices) {
								if (is_unchanged) {
									// Can memcpy directly
									success &= read_instrument->Read(allocated_pointer, data->element_count * element_byte_size);
								}
								else {
									// Need to deserialize every element
									// But take into account the file deserialization byte size, such that we start
									// Each iteration at the correct offset. To do this, make an initial deserialization,
									// And see how many bytes were read using the deserialize call, the remaining bytes up until
									// The file_blittable_size need to be ignored
									if (data->element_count > 0) {
										size_t initial_instrument_offset = read_instrument->GetOffset();
										if (Deserialize(
											data->read_data->reflection_manager,
											data->definition_info.type,
											allocated_pointer,
											read_instrument,
											&options
										) != ECS_DESERIALIZE_OK) {
											deallocate_buffer();
											return false;
										}
										size_t per_iteration_skip_byte_count = read_instrument->GetOffset() - initial_instrument_offset;

										for (size_t index = 1; index < data->element_count; index++) {
											void* element = OffsetPointer(allocated_pointer, element_byte_size * index);
											ECS_DESERIALIZE_CODE code = Deserialize(
												data->read_data->reflection_manager,
												data->definition_info.type,
												element,
												read_instrument,
												&options
											);
											if (code != ECS_DESERIALIZE_OK) {
												deallocate_buffer();
												return false;
											}

											if (!read_instrument->Ignore(per_iteration_skip_byte_count)) {
												deallocate_buffer();
												return false;
											}
										}
									}
								}
							}
							else {
								if (is_unchanged) {
									// Can memcpy into each index, but cannot as a whole
									for (size_t index = 0; index < data->element_count; index++) {
										void* element = OffsetPointer(allocated_pointer, element_byte_size * data->indices[index]);
										success &= read_instrument->Read(element, file_blittable_size);
									}
								}
								else {
									// Take into account the file deserialization byte size, such that we start
									// Each iteration at the correct offset. To do this, make an initial deserialization,
									// And see how many bytes were read using the deserialize call, the remaining bytes up until
									// The file_blittable_size need to be ignored
									if (data->element_count > 0) {
										size_t initial_instrument_offset = read_instrument->GetOffset();
										if (Deserialize(
											data->read_data->reflection_manager,
											data->definition_info.type,
											OffsetPointer(allocated_pointer, element_byte_size * data->indices[0]),
											read_instrument,
											&options
										) != ECS_DESERIALIZE_OK) {
											deallocate_buffer();
											return false;
										}
										size_t per_iteration_skip_byte_count = read_instrument->GetOffset() - initial_instrument_offset;

										// Cannot memcpy directly
										for (size_t index = 1; index < data->element_count; index++) {
											void* element = OffsetPointer(allocated_pointer, element_byte_size * data->indices[index]);
											ECS_DESERIALIZE_CODE code = Deserialize(
												data->read_data->reflection_manager,
												data->definition_info.type,
												element,
												read_instrument,
												&options
											);
											if (code != ECS_DESERIALIZE_OK) {
												deallocate_buffer();
												return false;
											}
										}
									}
								}
							}
						}
					}
					else {
						if (ignore_data) {
							for (size_t index = 0; index < data->element_count; index++) {
								if (!IgnoreDeserialize(read_instrument, *data->read_data->options->field_table, field_table_index, &ignore_data_table_options, data->read_data->options->deserialized_field_manager)) {
									return false;
								}
							}
						}
						else {
							for (size_t index = 0; index < data->element_count; index++) {
								size_t offset = index;
								if constexpr (use_indices) {
									offset = data->indices[index];
								}
								void* element = OffsetPointer(allocated_pointer, element_byte_size * offset);
								ECS_DESERIALIZE_CODE code = Deserialize(data->read_data->reflection_manager, data->definition_info.type, element, read_instrument, &options);
								if (code != ECS_DESERIALIZE_OK) {
									deallocate_buffer();
									return false;
								}
							}
						}
					}

					return success;
				};
				// Hoist the if check outside the for
				if (data->indices.buffer == nullptr) {
					success &= loop(std::false_type{});
				}
				else {
					success &= loop(std::true_type{});
				}

				if (!success) {
					return false;
				}
			}
			else {
				if (ignore_data) {
					unsigned int field_table_index = data->read_data->options->field_table->TypeIndex(data->definition_info.type->name);
					if (field_table_index == -1) {
						// Fail if the type could not be found in the field table
						return false;
					}

					if (!IgnoreDeserialize(read_instrument, *data->read_data->options->field_table, field_table_index, &ignore_data_table_options, data->read_data->options->deserialized_field_manager)) {
						return false;
					}
				}
				else {
					ECS_DESERIALIZE_CODE code = Deserialize(data->read_data->reflection_manager, data->definition_info.type, data->deserialize_target, read_instrument, &options);
					if (code != ECS_DESERIALIZE_OK) {
						return false;
					}
				}
			}
		}
		else if (data->definition_info.custom_type_index != -1) {
			// We must change the definition of the read data
			Stream<char> previous_definition = data->read_data->definition;
			data->read_data->definition = data->definition;

			// The ignore case is handled already through data->read_data->ignore_data
			auto loop_iteration = [&](void* element) -> int {
				data->read_data->data = element;
				bool success = ECS_SERIALIZE_CUSTOM_TYPES[data->definition_info.custom_type_index].read(data->read_data);
				if (!success) {
					if (!ignore_data) {
						deallocate_buffer();
					}
					return false;
				}

				return true;
			};

			if (data->indices.buffer == nullptr) {
				success &= iterate_elements(std::false_type{}, loop_iteration);
			}
			else {
				success &= iterate_elements(std::true_type{}, loop_iteration);
			}

			data->read_data->definition = previous_definition;

			if (!success) {
				return false;
			}
		}
		else {
			if (data->definition_info.field_stream_type != ReflectionStreamFieldType::Basic) {
				if (data->definition_info.field_stream_type == ReflectionStreamFieldType::Pointer) {
					// Determine if this is a reference pointer
					Stream<char> pointer_reference_key;
					Stream<char> pointer_reference_custom_element_name;
					if (GetReflectionPointerAsReferenceParams(data->tags, pointer_reference_key, pointer_reference_custom_element_name)) {
						// There is something to deserialize only if the key is specified
						if (pointer_reference_key.size > 0) {
							if (!ignore_data) {
								ECS_ASSERT(data->read_data->options != nullptr && data->read_data->options->passdown_info != nullptr);
								auto loop_iteration = [&](void* element) -> bool {
									ReflectionCustomTypeGetElementIndexOrToken token;
									success &= read_instrument->Read(&token);
									void* referenced_value = data->read_data->options->passdown_info->RetrievePointerTargetValueFromToken(
										data->read_data->reflection_manager,
										pointer_reference_key, 
										pointer_reference_custom_element_name,
										token,
										true
									);
									*(void**)element = referenced_value;
									return true;
								};
								
								if (data->indices.size > 0) {
									iterate_elements(std::true_type{}, loop_iteration);
								}
								else {
									iterate_elements(std::false_type{}, loop_iteration);
								}
							}
							else {
								// Just advance the stream pointer, since we are not interested in the actual data yet
								success &= read_instrument->Ignore(sizeof(ReflectionCustomTypeGetElementIndexOrToken) * data->element_count);
							}
						}
					}
				}
				else {
					ReflectionFieldInfo field_info;
					field_info.basic_type = data->definition_info.field_basic_type;
					field_info.stream_type = data->definition_info.field_stream_type;
					field_info.stream_byte_size = data->definition_info.field_stream_byte_size;
					field_info.stream_alignment = data->definition_info.field_stream_alignment;

					AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR;
					bool has_options = data->read_data->options != nullptr;
					if (has_options) {
						allocator = data->read_data->options->field_allocator;
					}

					// We need to branch based on this option
					if (ignore_data) {
						// Thw following code block works for the size determination as well
						auto loop_iteration = [&](void* element) {
							if (data->definition_info.field_stream_type == ReflectionStreamFieldType::ResizableStream) {
								if (has_options && data->read_data->options->use_resizable_stream_allocator) {
									allocator = ((ResizableStream<void>*)element)->allocator;
								}
							}

							// The zero is for basic type arrays. Should not really happen
							return ReadOrReferenceFundamentalType<ReadOrReferenceFundamentalTypeFlag::IgnoreData>(field_info, element, read_instrument, 0, allocator, true);
						};

						// We don't care about the indices being specified or not in this case
						success &= iterate_elements(std::false_type{}, loop_iteration);
					}
					else {
						auto loop_iteration = [&](void* element) {
							if (data->definition_info.field_stream_type == ReflectionStreamFieldType::ResizableStream) {
								if (has_options && data->read_data->options->use_resizable_stream_allocator) {
									allocator = ((ResizableStream<void>*)element)->allocator;
								}
							}

							// The zero is for basic type arrays. Should not really happen
							return ReadOrReferenceFundamentalType<ReadOrReferenceFundamentalTypeFlag::ForceAllocation>(field_info, element, read_instrument, 0, allocator, true);
						};

						if (data->indices.buffer == nullptr) {
							success &= iterate_elements(std::false_type{}, loop_iteration);
						}
						else {
							success &= iterate_elements(std::true_type{}, loop_iteration);
						}
					}
				}
			}
			else {
				if (!ignore_data) {
					if (!single_instance) {
						// Use a fast path for this instead of the usual iterate elements
						size_t allocate_size = data->elements_to_allocate * element_byte_size;
						if (data->indices.buffer == nullptr) {
							success &= read_instrument->Read(allocated_pointer, allocate_size);
						}
						else {
							for (size_t index = 0; index < data->element_count; index++) {
								success &= read_instrument->Read(OffsetPointer(allocated_pointer, data->indices[index] * element_byte_size), element_byte_size);
							}
						}
					}
					else {
						success &= read_instrument->Read(data->deserialize_target, element_byte_size);
					}
				}
				else {
					size_t count = single_instance ? 1 : data->elements_to_allocate;
					success &= read_instrument->Ignore(count * element_byte_size);
				}
			}
		}

		data->read_data->was_allocated = previous_was_allocated;
		return success;
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
	
	// -----------------------------------------------------------------------------------------

#pragma region Custom serializers

#pragma region Stream

	// NOTE: The new implementation uses variable length integer enconding, which is the best overall
	// Tradeoff that we can make, which should help in reducing the byte size by quite a bit
	// For many small streams. This will make the serialization/deserialization a bit slower,
	// But it is probably worth the cost for extra smaller files
#define SERIALIZE_CUSTOM_STREAM_VERSION (0)

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(Stream) {
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

		WriteInstrument* write_instrument = data->write_instrument;

		bool success = true;
		success &= SerializeIntVariableLengthBool(write_instrument, buffer_count);;

		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
		ReflectionType reflection_type;
		reflection_type.name = { nullptr, 0 };
	
		SerializeCustomWriteHelperData helper_data;
		helper_data.Set(data, template_type, data->tags);
		helper_data.data_to_write = { buffer, buffer_count };
		return success && SerializeCustomWriteHelper(&helper_data);
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(Stream) {
		if (data->version != SERIALIZE_CUSTOM_STREAM_VERSION) {
			return false;
		}

		ReadInstrument* read_instrument = data->read_instrument;

		bool success = true;
		size_t buffer_count = 0;
		success &= DeserializeIntVariableLengthBool(read_instrument, buffer_count);
		// Early exit if we couldn't read the size
		if (!success) {
			return false;
		}

		if (!data->ShouldIgnoreData()) {
			if (data->definition.StartsWith(STRING(Stream))) {
				size_t* stream_size = (size_t*)OffsetPointer(data->data, sizeof(void*));
				*stream_size = buffer_count;
			}
			else if (data->definition.StartsWith(STRING(CapacityStream))) {
				unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
				*stream_size = buffer_count; // This is the size field
				stream_size[1] = buffer_count; // This is the capacity field
			}
			else if (data->definition.StartsWith(STRING(ResizableStream))) {
				unsigned int* stream_size = (unsigned int*)OffsetPointer(data->data, sizeof(void*));
				*stream_size = buffer_count; // This is the size field
				stream_size[1] = buffer_count; // This is the capacity field
			}
			else {
				ECS_ASSERT(false);
			}
		}

		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);

		DeserializeCustomReadHelperData helper_data;
		helper_data.Set(data, template_type, data->tags);
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
		WriteInstrument* write_instrument = data->write_instrument;
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
		
		bool success = true;
		success &= write_instrument->Write(&buffer_count);
		success &= write_instrument->Write(&buffer_capacity);
		success &= write_instrument->Write(&first_free);

		SerializeCustomWriteHelperData helper_data;
		helper_data.Set(data, template_type, data->tags);
		helper_data.data_to_write = { buffer, buffer_count };

		success &= SerializeCustomWriteHelper(&helper_data);

		//size_t template_type_byte_size = SerializeCustomTypeDeduceTypeHelper(template_type, data->reflection_manager, &reflection_type, custom_serializer_index, basic_type, stream_type);
		//data->definition = template_type;

		//// Now write the user defined data first - or basic data if it is
		//total_serialize_size += SerializeCustomWriteHelper(basic_type, stream_type, &reflection_type, custom_serializer_index, data, { buffer, buffer_count }, template_type_byte_size);

		// Write the indirection buffer after it because when reading the data the helper will allocate the data
		// such that the indirection buffer can be read directly into the allocation without temporaries
		success &= write_instrument->Write(set->indirection_buffer, sizeof(set->indirection_buffer[0]) * buffer_capacity);

		return success;
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(SparseSet) {
		if (data->version != SERIALIZE_SPARSE_SET_VERSION) {
			return false;
		}

		ReadInstrument* read_instrument = data->read_instrument;
		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);

		bool ignore_data = data->ShouldIgnoreData();
		// Determine if it is a trivial type - including streams
		SparseSet<char>* set = (SparseSet<char>*)data->data;

		unsigned int buffer_count = 0;
		unsigned int buffer_capacity = 0;
		unsigned int first_free = 0;

		bool success = true;
		bool* user_data = (bool*)ECS_SERIALIZE_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET].user_data;
		if (user_data != nullptr && *user_data) {
			size_t stream_size = 0;
			success &= read_instrument->Read(&stream_size);
			buffer_count = stream_size;
			buffer_capacity = buffer_count;
			first_free = 0;
		}
		else {
			success &= read_instrument->Read(&buffer_count);
			success &= read_instrument->Read(&buffer_capacity);
			success &= read_instrument->Read(&first_free);
		}
		// Early exit if we couldn't read the size/sizes
		if (!success) {
			return false;
		}

		void* allocated_buffer = nullptr;

		DeserializeCustomReadHelperData read_data;
		read_data.Set(data, template_type, data->tags);

		// We can allocate the buffer only once by using the elements_to_allocate variable
		size_t allocation_size = read_data.definition_info.byte_size * buffer_capacity + sizeof(uint2) * buffer_capacity;
		// If there is a remainder, allocate one more to compensate
		size_t elements_to_allocate = SlotsFor(allocation_size, read_data.definition_info.byte_size);

		read_data.allocated_buffer = &allocated_buffer;
		read_data.element_count = buffer_count;
		read_data.elements_to_allocate = elements_to_allocate;

		success &= DeserializeCustomReadHelper(&read_data);
		if (!success) {
			return false;
		}

		if (!ignore_data) {
			SparseSetInitializeUntyped(set, buffer_capacity, read_data.definition_info.byte_size, allocated_buffer);
		}

		if (user_data != nullptr && *user_data) {
			if (!ignore_data) {
				// No indirection buffer was written. In order to preserve the invariance we need to allocate the elements
				for (unsigned int index = 0; index < buffer_count; index++) {
					set->Allocate();
				}
			}
		}
		else {
			// Now read the indirection_buffer
			if (!ignore_data) {
				success &= read_instrument->Read(set->indirection_buffer, sizeof(set->indirection_buffer[0]) * buffer_capacity);

				set->size = buffer_count;
				set->first_empty_slot = first_free;
			}
			else {
				success &= read_instrument->Ignore(sizeof(set->indirection_buffer[0]) * buffer_capacity);
			}
		}

		return success;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

#pragma region Data Pointer

#define SERIALIZE_CUSTOM_DATA_POINTER_VERSION (0)

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(DataPointer) {
		const DataPointer* pointer = (const DataPointer*)data->data;
		return data->write_instrument->WriteWithSize<unsigned short>({ pointer->GetPointer(), pointer->GetData() });
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(DataPointer) {
		if (data->version != SERIALIZE_CUSTOM_DATA_POINTER_VERSION) {
			return false;
		}

		DataPointer* data_pointer = (DataPointer*)data->data;
		ReadInstrument* read_instrument = data->read_instrument;

		bool success = true;
		unsigned short byte_size = 0;
		success &= read_instrument->ReadAlways(&byte_size);
		// Early exit if we couldn't read the size
		if (!success) {
			return false;
		}

		if (!data->ShouldIgnoreData()) {
			void* pointer = nullptr;

			if (data->options->field_allocator.allocator != nullptr) {
				void* allocation = Allocate(data->options->field_allocator, byte_size);
				success &= read_instrument->Read(allocation, byte_size);
				pointer = allocation;
			}
			else {
				pointer = read_instrument->ReferenceData(byte_size);
				success &= pointer != nullptr;
			}

			data_pointer->SetPointer(pointer);
			data_pointer->SetData(byte_size);
		}
		else {
			success &= read_instrument->Ignore(byte_size);
		}
		return success;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

#pragma region Allocator

#define SERIALIZE_CUSTOM_ALLOCATOR_VERSION (0)

	// Returns true if it succeeded, else false
	static bool SerializeCreateBaseAllocatorInfo(SerializeCustomTypeWriteFunctionData* data, const CreateBaseAllocatorInfo& info) {
		bool success = true;

		WriteInstrument* write_instrument = data->write_instrument;

		success &= write_instrument->Write(&info.allocator_type);
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			success &= write_instrument->Write(&info.linear_capacity);
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			success &= write_instrument->Write(&info.stack_capacity);
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			success &= write_instrument->Write(&info.multipool_block_count);
			success &= write_instrument->Write(&info.multipool_capacity);
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			success &= write_instrument->Write(&info.arena_allocator_count);
			success &= write_instrument->Write(&info.arena_capacity);
			success &= write_instrument->Write(&info.arena_nested_type);
			if (info.arena_nested_type == ECS_ALLOCATOR_MULTIPOOL) {
				success &= write_instrument->Write(&info.arena_multipool_block_count);
			}
		}
		break;
		default:
			ECS_ASSERT(false);
		}

		return success;
	}

	// It will always read the bytes, it won't simply skip over them
	static bool DeserializeCreateBaseAllocatorInfo(ReadInstrument* read_instrument, CreateBaseAllocatorInfo& info) {
		bool success = true;

		success &= read_instrument->ReadAlways(&info.allocator_type);
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			success &= read_instrument->ReadAlways(&info.linear_capacity);
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			success &= read_instrument->ReadAlways(&info.stack_capacity);
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			success &= read_instrument->ReadAlways(&info.multipool_block_count);
			success &= read_instrument->ReadAlways(&info.multipool_capacity);
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			success &= read_instrument->ReadAlways(&info.arena_allocator_count);
			success &= read_instrument->ReadAlways(&info.arena_capacity);
			success &= read_instrument->ReadAlways(&info.arena_nested_type);
			if (info.arena_nested_type == ECS_ALLOCATOR_MULTIPOOL) {
				success &= read_instrument->ReadAlways(&info.arena_multipool_block_count);
			}
		}
		break;
		default:
			ECS_ASSERT(false);
		}

		return success;
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(Allocator) {
		bool success = true;

		// If this is a reference, don't write its contents
		if (FindFirstToken(data->tags, STRING(ECS_REFERENCE_ALLOCATOR)).size > 0) {
			return true;
		}

		WriteInstrument* write_instrument = data->write_instrument;
		// We must write as a first byte the type of allocator, such that if it changes, we can choose to abort or not the deserialization
		ECS_ALLOCATOR_TYPE allocator_type = AllocatorTypeFromString(data->definition);
		success &= write_instrument->Write(&allocator_type);

		// TODO: Determine if we need more information written, in order to restore fully the allocator
		// For the current usage, this is intended to write only the capacity of the allocator and if required,
		// Extra information like the base allocator types and sizes for a manager type allocator. It is not intended
		// To write an extensive allocator representation, including the allocations that were made, since that is not
		// Needed for the intended purpose. The intented purpose is for this to know how much to allocate when reconstructing
		// It, and then the fields will make allocations from it as needed.

		// This lambda will perform the appropriate serialization, based on the given type
		auto serialize = [data, &success, write_instrument](ECS_ALLOCATOR_TYPE allocator_type, const void* source) {
			switch (allocator_type) {
			case ECS_ALLOCATOR_LINEAR:
			{
				LinearAllocator* allocator = (LinearAllocator*)source;
				success &= write_instrument->Write(&allocator->m_capacity);
			}
			break;
			case ECS_ALLOCATOR_STACK:
			{
				StackAllocator* allocator = (StackAllocator*)source;
				success &= write_instrument->Write(&allocator->m_capacity);
			}
			break;
			case ECS_ALLOCATOR_MULTIPOOL:
			{
				MultipoolAllocator* allocator = (MultipoolAllocator*)source;
				size_t allocator_size = allocator->GetSize();
				unsigned int block_count = allocator->GetBlockCount();
				success &= write_instrument->Write(&allocator_size);
				success &= write_instrument->Write(&block_count);
			}
			break;
			case ECS_ALLOCATOR_ARENA:
			{
				MemoryArena* allocator = (MemoryArena*)source;
				CreateBaseAllocatorInfo base_allocator_info = allocator->GetInitialBaseAllocatorInfo();
				unsigned char allocator_count = allocator->m_allocator_count;
				success &= write_instrument->Write(&allocator_count);
				success &= SerializeCreateBaseAllocatorInfo(data, base_allocator_info);
			}
			break;
			case ECS_ALLOCATOR_MANAGER:
			{
				MemoryManager* allocator = (MemoryManager*)source;
				success &= SerializeCreateBaseAllocatorInfo(data, allocator->GetInitialAllocatorInfo());
				success &= SerializeCreateBaseAllocatorInfo(data, allocator->m_backup_info);
			}
			break;
			case ECS_ALLOCATOR_RESIZABLE_LINEAR:
			{
				ResizableLinearAllocator* allocator = (ResizableLinearAllocator*)source;
				success &= write_instrument->Write(&allocator->m_initial_capacity);
				success &= write_instrument->Write(&allocator->m_backup_size);
			}
			break;
			case ECS_ALLOCATOR_MEMORY_PROTECTED:
			{
				MemoryProtectedAllocator* allocator = (MemoryProtectedAllocator*)source;
				success &= write_instrument->Write(&allocator->chunk_size);
				success &= write_instrument->Write(&allocator->linear_allocators);
			}
			break;
			case ECS_ALLOCATOR_MALLOC:
			{
				// Nothing to write for malloc
			}
			break;
			case ECS_ALLOCATOR_INTERFACE:
			{
				ECS_ASSERT(false, "Unimplemented Interface allocator serialize code path");
			}
			break;
			default:
				ECS_ASSERT(false);
			}
		};

		if (allocator_type != ECS_ALLOCATOR_TYPE_COUNT) {
			serialize(allocator_type, data->data);
		}
		else {
			// It must be the polymorphic allocator
			AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)data->data;
			
			// Serialize the nested type once more. If the target allocator is nullptr, make the type as COUNT
			// Once more to signal that it is malloc
			ECS_ALLOCATOR_TYPE polymorphic_type = allocator->allocator->m_allocator_type;
			success &= write_instrument->Write(&polymorphic_type);
			serialize(polymorphic_type, allocator->allocator);
		}

		return success;
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(Allocator) {
		if (data->version != SERIALIZE_CUSTOM_ALLOCATOR_VERSION) {
			return false;
		}

		ReadInstrument* read_instrument = data->read_instrument;
		bool read_data = !data->ShouldIgnoreData();
		AllocatorPolymorphic field_allocator = GetDeserializeFieldAllocator(data, read_data);
		if (FindFirstToken(data->tags, STRING(ECS_REFERENCE_ALLOCATOR)).size > 0) {
			// As a safeguard, ensure that it is an allocator polymorphic
			ECS_ASSERT(data->definition == STRING(AllocatorPolymorphic), "Reference allocator is not an AllocatorPolymorphic!");
			if (read_data) {
				AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)data->data;
				*allocator = field_allocator;
			}
			return true;
		}

		bool success = true;
		// Read the first byte. If the type has changed, then abort this
		ECS_ALLOCATOR_TYPE allocator_type;
		success &= read_instrument->ReadAlways(&allocator_type);
		// Abort early on in case we couldn't read this important piece of data
		if (!success) {
			return false;
		}

		bool is_allocator_matched = allocator_type == AllocatorTypeFromString(data->definition);
		// When we have a mismatch, we must still advance through with the data pointer
		// When retrieving the field allocator, don't assert if we are interested in skipping only the section

		// This lambda will perform the appropriate deserialization, based on the given type.
		// Returns false if there is an error.
		auto deserialize = [read_data, read_instrument, &success, is_allocator_matched, field_allocator](ECS_ALLOCATOR_TYPE allocator_type, void* allocator_pointer) {
			switch (allocator_type) {
			case ECS_ALLOCATOR_LINEAR:
			{
				size_t capacity = 0;
				success &= read_instrument->Read(&capacity);
				if (is_allocator_matched) {
					// If it failed, don't continue, since the value is probably corrupted
					if (read_data && success) {
						LinearAllocator* allocator = (LinearAllocator*)allocator_pointer;
						new (allocator) LinearAllocator(Allocate(field_allocator, capacity), capacity);
					}
				}
			}
			break;
			case ECS_ALLOCATOR_STACK:
			{
				size_t capacity = 0;
				success &= read_instrument->Read(&capacity);
				if (is_allocator_matched) {
					if (read_data && success) {
						StackAllocator* allocator = (StackAllocator*)allocator_pointer;
						new (allocator) StackAllocator(Allocate(field_allocator, capacity), capacity);
					}
				}
			}
			break;
			case ECS_ALLOCATOR_MULTIPOOL:
			{
				size_t capacity = 0;
				unsigned int block_count = 0;
				success &= read_instrument->ReadAlways(&capacity);
				success &= read_instrument->ReadAlways(&block_count);

				size_t allocate_size = MultipoolAllocator::MemoryOf(block_count, capacity);
				if (is_allocator_matched) {
					if (read_data && success) {
						MultipoolAllocator* allocator = (MultipoolAllocator*)allocator_pointer;
						new (allocator) MultipoolAllocator(Allocate(field_allocator, allocate_size), capacity, block_count);
					}
				}
			}
			break;
			case ECS_ALLOCATOR_ARENA:
			{
				CreateBaseAllocatorInfo base_allocator_info;
				unsigned char allocator_count = 0;
				success &= read_instrument->ReadAlways(&allocator_count);
				success &= DeserializeCreateBaseAllocatorInfo(read_instrument, base_allocator_info);
				if (is_allocator_matched) {
					if (read_data && success) {
						MemoryArena* allocator = (MemoryArena*)allocator_pointer;
						new (allocator) MemoryArena(field_allocator, allocator_count, base_allocator_info);
					}
				}
			}
			break;
			case ECS_ALLOCATOR_MANAGER:
			{
				CreateBaseAllocatorInfo initial_allocator_info;
				CreateBaseAllocatorInfo backup_allocator_info;
				success &= DeserializeCreateBaseAllocatorInfo(read_instrument, initial_allocator_info);
				success &= DeserializeCreateBaseAllocatorInfo(read_instrument, backup_allocator_info);
				if (is_allocator_matched) {
					if (read_data && success) {
						MemoryManager* allocator = (MemoryManager*)allocator_pointer;
						new (allocator) MemoryManager(initial_allocator_info, backup_allocator_info, field_allocator);
					}
				}
			}
			break;
			case ECS_ALLOCATOR_RESIZABLE_LINEAR:
			{
				size_t initial_capacity = 0;
				size_t backup_capacity = 0;
				success &= read_instrument->ReadAlways(&initial_capacity);
				success &= read_instrument->ReadAlways(&backup_capacity);
				if (is_allocator_matched) {
					if (read_data && success) {
						ResizableLinearAllocator* allocator = (ResizableLinearAllocator*)allocator_pointer;
						new (allocator) ResizableLinearAllocator(initial_capacity, backup_capacity, field_allocator);
					}
				}
			}
			break;
			case ECS_ALLOCATOR_MEMORY_PROTECTED:
			{
				size_t chunk_size = 0;
				bool linear_allocators = false;
				success &= read_instrument->ReadAlways(&chunk_size);
				success &= read_instrument->ReadAlways(&linear_allocators);
				if (is_allocator_matched) {
					if (read_data && success) {
						MemoryProtectedAllocator* allocator = (MemoryProtectedAllocator*)allocator_pointer;
						new (allocator) MemoryProtectedAllocator(chunk_size, linear_allocators);
					}
				}
			}
			break;
			case ECS_ALLOCATOR_MALLOC:
			{
				// Nothing to be done for malloc, just to initialize the vtable
				if (read_data) {
					MallocAllocator* allocator = (MallocAllocator*)allocator_pointer;
					new (allocator) MallocAllocator();
				}
			}
			break;
			case ECS_ALLOCATOR_INTERFACE:
			{
				// This is an incorrect type, can't handle it
				return false;
			}
			break;
			default:
				return false;
			}

			return success;
		};

		if (allocator_type != ECS_ALLOCATOR_TYPE_COUNT) {
			if (!deserialize(allocator_type, data->data)) {
				return false;
			}
		}
		else {
			// It must be the polymorphic allocator
			AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)data->data;

			// Serialize the nested type once more. If the target allocator is nullptr, make the type as COUNT
			// Once more to signal that it is malloc
			ECS_ALLOCATOR_TYPE polymorphic_type = ECS_ALLOCATOR_TYPE_COUNT;
			success &= read_instrument->ReadAlways(&polymorphic_type);
			// Abort early on if we couldn't read this piece of data
			if (!success) {
				return false;
			}

			if (polymorphic_type != ECS_ALLOCATOR_TYPE_COUNT) {
				// Make this nullptr in order to not trigger a read when we are not supposed to
				if (!deserialize(polymorphic_type, read_data ? allocator->allocator : nullptr)) {
					return false;
				}
			}
		}

		return success;
	}

	// -----------------------------------------------------------------------------------------

#pragma endregion

#pragma region HashTable

#define SERIALIZE_CUSTOM_HASH_TABLE_VERSION (0)

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(HashTable) {
		bool success = true;
		WriteInstrument* write_instrument = data->write_instrument;

		HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);
		ReflectionDefinitionInfo value_definition_info = HashTableGetValueDefinitionInfo(data->reflection_manager, template_arguments);
		ReflectionDefinitionInfo identifier_definition_info = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_arguments);

		const HashTableDefault<char>* hash_table = (const HashTableDefault<char>*)data->data;
		// Write the count and capacity upfront
		success &= write_instrument->Write(&hash_table->m_count);
		success &= write_instrument->Write(&hash_table->m_capacity);

		// Write the metadata buffer as is directly to the stream
		success &= write_instrument->Write(hash_table->m_metadata, hash_table->GetExtendedCapacity() * sizeof(hash_table->m_metadata[0]));

		// We need to branch based on the SoA now
		if (template_arguments.is_soa) {
			// Handle the case when the value is empty, then we don't have to write anything
			if (value_definition_info.byte_size > 0) {
				ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
					!value_definition_info.is_blittable,
					data->tags,
					STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE),
					value_options
				);

				SerializeCustomWriteHelperData helper_data;
				helper_data.write_data = data;
				helper_data.definition = template_arguments.value_type;
				helper_data.tags = value_options;
				helper_data.definition_info = value_definition_info;

				hash_table->ForEachIndexConst([&](unsigned int index) {
					const void* element = OffsetPointer(hash_table->m_buffer, value_definition_info.byte_size * (size_t)index);
					if (value_definition_info.is_blittable) {
						success &= write_instrument->Write(element, value_definition_info.byte_size);
					}
					else {
						// Need to perform a full serialization
						helper_data.data_to_write = { element, 1 };
						success &= SerializeCustomWriteHelper(&helper_data);
					}
				});
			}

			ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
				!identifier_definition_info.is_blittable,
				data->tags,
				STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER),
				identifier_options
			);
			SerializeCustomWriteHelperData helper_data;
			helper_data.definition = template_arguments.identifier_type;
			helper_data.definition_info = identifier_definition_info;
			helper_data.write_data = data;
			helper_data.tags = identifier_options;

			// Handle the identifiers
			hash_table->ForEachIndexConst([&](unsigned int index) {
				const void* identifier = OffsetPointer(hash_table->m_identifiers, identifier_definition_info.byte_size * (size_t)index);
				if (identifier_definition_info.is_blittable) {
					success &= write_instrument->Write(identifier, identifier_definition_info.byte_size);
				}
				else {
					helper_data.data_to_write = { identifier, 1 };
					success &= SerializeCustomWriteHelper(&helper_data);
				}
			});
		}
		else {
			ulong2 pair_size = HashTableComputePairByteSizeAndAlignmentOffset(value_definition_info, identifier_definition_info);
			
			ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
				!value_definition_info.is_blittable,
				data->tags,
				STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE),
				value_options
			);

			SerializeCustomWriteHelperData value_helper_data;
			value_helper_data.definition = template_arguments.value_type;
			value_helper_data.definition_info = value_definition_info;
			value_helper_data.write_data = data;
			value_helper_data.tags = value_options;

			ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
				!identifier_definition_info.is_blittable,
				data->tags,
				STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER),
				identifier_options
			);

			SerializeCustomWriteHelperData identifier_helper_data;
			identifier_helper_data.definition = template_arguments.identifier_type;
			identifier_helper_data.definition_info = identifier_definition_info;
			identifier_helper_data.write_data = data;
			identifier_helper_data.tags = identifier_options;

			hash_table->ForEachIndexConst([&](unsigned int index) {
				const void* pair = OffsetPointer(hash_table->m_buffer, pair_size.x * (size_t)index);
				if (value_definition_info.is_blittable) {
					success &= write_instrument->Write(pair, value_definition_info.byte_size);
				}
				else {
					value_helper_data.data_to_write = { pair, 1 };
					success &= SerializeCustomWriteHelper(&value_helper_data);
				}

				const void* identifier = OffsetPointer(pair, value_definition_info.byte_size + pair_size.y);
				if (identifier_definition_info.is_blittable) {
					success &= write_instrument->Write(identifier, identifier_definition_info.byte_size);
				}
				else {
					identifier_helper_data.data_to_write = { identifier, 1 };
					success &= SerializeCustomWriteHelper(&identifier_helper_data);
				}
			});
		}

		return success;
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(HashTable) {
		if (data->version != SERIALIZE_CUSTOM_HASH_TABLE_VERSION) {
			return false;
		}

		bool success = true;
		ReadInstrument* read_instrument = data->read_instrument;

		HashTableDefault<char>* hash_table = (HashTableDefault<char>*)data->data;
		unsigned int count = 0;
		unsigned int capacity = 0;
		// Write the count and capacity upfront
		success &= read_instrument->ReadAlways(&count);
		success &= read_instrument->ReadAlways(&capacity);
		// Early exit if we couldn't read these important pieces of data
		if (!success) {
			return false;
		}

		bool read_data = !data->ShouldIgnoreData();
		if (read_data) {
			hash_table->m_count = count;
			hash_table->m_capacity = capacity;
		}

		// If the capacity is 0, we can early exit
		if (capacity == 0) {
			if (read_data) {
				hash_table->InitializeFromBuffer(nullptr, 0);
			}
			return true;
		}

		HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);
		ReflectionDefinitionInfo value_definition_info = HashTableGetValueDefinitionInfo(data->reflection_manager, template_arguments);
		ReflectionDefinitionInfo identifier_definition_info = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_arguments);

		if (read_data) {
			ECS_ASSERT(data->options != nullptr);
			// Determine the buffer that must be allocated. Use the backup allocator for the moment
			HashTableCustomTypeAllocateAndSetBuffers(hash_table, hash_table->m_capacity, data->options->field_allocator, template_arguments.is_soa, value_definition_info, identifier_definition_info);
		}

		// Read the metadata buffer directly from the stream
		success &= read_instrument->Read(hash_table->m_metadata, hash_table->GetExtendedCapacity() * sizeof(hash_table->m_metadata[0]));
		if (!success) {
			return false;
		}

		// Branch based on the SoA
		if (template_arguments.is_soa) {
			// Handle the case when the value is empty, then we don't have to write anything
			if (value_definition_info.byte_size > 0) {
				ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
					!value_definition_info.is_blittable,
					data->tags,
					STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE),
					value_options
				);

				DeserializeCustomReadHelperData helper_data;
				helper_data.definition = template_arguments.value_type;
				helper_data.definition_info = value_definition_info;
				helper_data.read_data = data;
				helper_data.elements_to_allocate = 0;
				helper_data.element_count = 1;
				helper_data.tags = value_options;

				hash_table->ForEachIndexConst([&](unsigned int index) {
					void* element = OffsetPointer(hash_table->m_buffer, value_definition_info.byte_size * (size_t)index);
					if (value_definition_info.is_blittable) {
						success &= read_instrument->Read(element, value_definition_info.byte_size);
					}
					else {
						// Need to perform a full serialization
						helper_data.deserialize_target = element;
						success &= DeserializeCustomReadHelper(&helper_data);
					}
				});
			}

			ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
				!identifier_definition_info.is_blittable,
				data->tags,
				STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER),
				identifier_options
			);
			DeserializeCustomReadHelperData helper_data;
			helper_data.definition = template_arguments.identifier_type;
			helper_data.definition_info = identifier_definition_info;
			helper_data.read_data = data;
			helper_data.elements_to_allocate = 0;
			helper_data.element_count = 1;
			helper_data.tags = identifier_options;

			// Handle the identifiers
			hash_table->ForEachIndexConst([&](unsigned int index) {
				void* identifier = OffsetPointer(hash_table->m_identifiers, identifier_definition_info.byte_size * (size_t)index);
				if (identifier_definition_info.is_blittable) {
					success &= read_instrument->Read(identifier, identifier_definition_info.byte_size);
				}
				else {
					helper_data.deserialize_target = identifier;
					success &= DeserializeCustomReadHelper(&helper_data);
				}
			});
		}
		else {
			ulong2 pair_size = HashTableComputePairByteSizeAndAlignmentOffset(value_definition_info, identifier_definition_info);

			ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
				!value_definition_info.is_blittable,
				data->tags,
				STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE),
				value_options
			);

			DeserializeCustomReadHelperData value_helper_data;
			value_helper_data.definition = template_arguments.value_type;
			value_helper_data.definition_info = value_definition_info;
			value_helper_data.read_data = data;
			value_helper_data.elements_to_allocate = 0;
			value_helper_data.element_count = 1;
			value_helper_data.tags = value_options;

			ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
				!identifier_definition_info.is_blittable,
				data->tags,
				STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER),
				identifier_options
			);

			DeserializeCustomReadHelperData identifier_helper_data;
			identifier_helper_data.definition = template_arguments.identifier_type;
			identifier_helper_data.definition_info = identifier_definition_info;
			identifier_helper_data.read_data = data;
			identifier_helper_data.elements_to_allocate = 0;
			identifier_helper_data.element_count = 1;
			identifier_helper_data.tags = identifier_options;

			hash_table->ForEachIndexConst([&](unsigned int index) {
				void* pair = OffsetPointer(hash_table->m_buffer, pair_size.x * (size_t)index);
				if (value_definition_info.is_blittable) {
					success &= read_instrument->Read(pair, value_definition_info.byte_size);
				}
				else {
					value_helper_data.deserialize_target = pair;
					success &= DeserializeCustomReadHelper(&value_helper_data);
				}

				void* identifier = OffsetPointer(pair, value_definition_info.byte_size + pair_size.y);
				if (identifier_definition_info.is_blittable) {
					success &= read_instrument->Read(identifier, identifier_definition_info.byte_size);
				}
				else {
					identifier_helper_data.deserialize_target = identifier;
					success &= DeserializeCustomReadHelper(&identifier_helper_data);
				}
			});
		}

		return success;
	}

#pragma endregion

#pragma region Deck

#define SERIALIZE_CUSTOM_DECK_VERSION (0)

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(Deck) {
		bool success = true;
		WriteInstrument* write_instrument = data->write_instrument;
		
		const DeckPowerOfTwo<char>* deck = (const DeckPowerOfTwo<char>*)data->data;

		// Write the element count first. If the count is 0, we can stop
		success &= write_instrument->Write(&deck->size);
		if (deck->size == 0) {
			return success;
		}

		// Write the other mandatory data
		success &= write_instrument->Write(&deck->chunk_size);
		success &= write_instrument->Write(&deck->power_of_two_exponent);
		
		// The rest of the draw we can forward to the custom stream serializer
		ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 2);
		ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);

		ECS_FORMAT_TEMP_STRING(buffers_definition, "ResizableStream<CapacityStream<{#}>>", template_arguments[0]);
		
		Stream<char> original_definition = data->definition;
		data->definition = buffers_definition;
		data->data = &deck->buffers;

		success &= SerializeCustomTypeWrite_Stream(data);

		data->definition = original_definition;
		data->data = deck;

		return success;
	}

	// -----------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(Deck) {
		if (data->version != SERIALIZE_CUSTOM_DECK_VERSION) {
			return false;
		}

		bool success = true;
		ReadInstrument* read_instrument = data->read_instrument;
		DeckPowerOfTwo<char>* deck = (DeckPowerOfTwo<char>*)data->data;

		decltype(deck->size) deck_size;
		success &= read_instrument->ReadAlways(&deck_size);
		if (!success) {
			return false;
		}

		if (deck_size == 0) {
			return true;
		}

		decltype(deck->chunk_size) chunk_size;
		decltype(deck->power_of_two_exponent) power_of_two_exponent;
		success &= read_instrument->ReadAlways(&chunk_size);
		success &= read_instrument->ReadAlways(&power_of_two_exponent);
		if (!success) {
			return false;
		}

		bool read_data = !data->ShouldIgnoreData();
		if (read_data) {
			deck->size = deck_size;
			deck->chunk_size = chunk_size;
			deck->power_of_two_exponent = power_of_two_exponent;
			deck->buffers.allocator = GetDeserializeFieldAllocator(data, false);
		}

		// The rest of the draw we can forward to the custom stream deserializer
		ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 2);
		ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);

		ECS_FORMAT_TEMP_STRING(buffers_definition, "ResizableStream<CapacityStream<T>>", template_arguments[0]);

		Stream<char> original_definition = data->definition;
		data->definition = buffers_definition;
		data->data = read_data ? &deck->buffers : data->data;

		size_t read_size = SerializeCustomTypeRead_Stream(data);

		data->definition = original_definition;
		data->data = read_data ? deck : data->data;;
		
		return read_size;
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
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(HashTable, SERIALIZE_CUSTOM_HASH_TABLE_VERSION),
		ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(Deck, SERIALIZE_CUSTOM_DECK_VERSION)
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


	void SerializeCustomWriteHelperData::Set(SerializeCustomTypeWriteFunctionData* _write_data, Stream<char> _definition, Stream<char> _tags)
	{
		write_data = _write_data;
		definition = _definition;
		tags = _tags;
		definition_info = SearchReflectionDefinitionInfo(write_data->reflection_manager, definition);
	}

	void DeserializeCustomReadHelperData::Set(SerializeCustomTypeReadFunctionData* _read_data, Stream<char> _definition, Stream<char> _tags)
	{
		read_data = _read_data;
		definition = _definition;
		tags = _tags;
		definition_info = SearchReflectionDefinitionInfo(read_data->reflection_manager, definition);
	}

	// -----------------------------------------------------------------------------------------

	bool WriteFundamentalType(const ReflectionFieldInfo& info, const void* data, WriteInstrument* write_instrument) {
		if (info.stream_type == ReflectionStreamFieldType::Basic) {
			return write_instrument->Write(data, info.byte_size);
		}
		else {
			if (info.stream_type == ReflectionStreamFieldType::Pointer) {
				unsigned int pointer_indirection = GetReflectionFieldPointerIndirection(info);
				if (pointer_indirection == 1) {
					if (info.basic_type == ReflectionBasicFieldType::Int8) {
						const char* characters = *(const char**)data;
						size_t character_count = strlen(characters) + 1;
						return write_instrument->WriteWithSizeVariableLength(Stream<char>(characters, character_count));
					}
					else if (info.basic_type == ReflectionBasicFieldType::Wchar_t) {
						const wchar_t* characters = *(const wchar_t**)data;
						size_t character_count = wcslen(characters) + 1;
						return write_instrument->WriteWithSizeVariableLength(Stream<wchar_t>(characters, character_count));
					}
					else {
						ECS_ASSERT(info.basic_type != ReflectionBasicFieldType::UserDefined, "WriteFundamentalType does not accept user defined pointers!");
						// Write the value that is there
						size_t write_size = GetReflectionBasicFieldTypeByteSize(info.basic_type);
						return write_instrument->Write(*(void**)data, write_size);
					}
				}
				else {
					ECS_ASSERT(false, "Failed to serialize basic pointer. Indirection is greater than 1.");
				}
			}
			else if (info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
				return write_instrument->Write(&info.basic_type_count) + write_instrument->Write(data, info.byte_size);
			}
			else if (IsStreamWithSoA(info.stream_type)) {
				// The SoA case can be handled here as well
				Stream<void> void_stream = GetReflectionFieldStreamVoid(info, data);
				size_t element_byte_size = GetReflectionFieldStreamElementByteSize(info);
				void_stream.size *= element_byte_size;
				bool write_success = write_instrument->WriteWithSizeVariableLength<void>(void_stream);

				// For strings, add a '\0' such that if the data is later on changed to a const char* or const wchar_t* it can still reference it
				if (info.basic_type == ReflectionBasicFieldType::Int8) {
					write_success &= write_instrument->Write("\0", sizeof(char));
				}
				else if (info.basic_type == ReflectionBasicFieldType::Wchar_t) {
					write_success &= write_instrument->Write(L"\0", sizeof(wchar_t));
				}

				return write_success;
			}
		}

		ECS_ASSERT(false, "An error has occured when trying to serialize fundamental type.");
		return false;
	}

	// -----------------------------------------------------------------------------------------

	template<ReadOrReferenceFundamentalTypeFlag flag>
	bool ReadOrReferenceFundamentalType(
		const ReflectionFieldInfo& info,
		void* data,
		ReadInstrument* read_instrument,
		unsigned short basic_type_array_count,
		AllocatorPolymorphic allocator,
		bool allocate_soa_pointer
	) {
		if (info.stream_type == ReflectionStreamFieldType::Basic) {
			return read_instrument->Read(data, info.byte_size);
		}
		else {
			bool should_read_data = !read_instrument->IsSizeDetermination() && (flag != ReadOrReferenceFundamentalTypeFlag::IgnoreData);
			constexpr bool force_allocation = flag == ReadOrReferenceFundamentalTypeFlag::ForceAllocation;

			if (info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
				unsigned short elements_to_read = ClampMax(info.basic_type_count, basic_type_array_count);
				if (!read_instrument->Read(data, elements_to_read * GetBasicTypeArrayElementSize(info))) {
					return false;
				}

				return read_instrument->Ignore((info.basic_type_count - elements_to_read) * GetBasicTypeArrayElementSize(info));
			}

			if (info.stream_type == ReflectionStreamFieldType::Pointer) {
				bool success = true;
				unsigned int pointer_indirection = GetReflectionFieldPointerIndirection(info);
				if (pointer_indirection == 1) {
					if (info.basic_type == ReflectionBasicFieldType::Int8 || info.basic_type == ReflectionBasicFieldType::Wchar_t) {
						size_t byte_size = 0;
						success &= DeserializeIntVariableLengthBool(read_instrument, byte_size);
						if (should_read_data) {
							size_t element_byte_size = info.basic_type == ReflectionBasicFieldType::Int8 ? sizeof(char) : sizeof(wchar_t);
							size_t allocate_size = byte_size + element_byte_size;
							if (allocator.allocator != nullptr) {
								if (byte_size > 0) {
									void* allocation = Allocate(allocator, allocate_size, info.stream_alignment);
									success &= read_instrument->ReadAlways(allocation, byte_size);

									void* null_terminator = OffsetPointer(allocation, byte_size);
									memset(null_terminator, 0, element_byte_size);

									void** pointer = (void**)data;
									*pointer = allocation;
								}
							}
							else {
								if constexpr (!force_allocation) {
									void* referenced_data = read_instrument->ReferenceData(byte_size);
									success &= referenced_data != nullptr;
									*(void**)data = referenced_data;
								}
								else {
									if (byte_size > 0) {
										void* allocation = Allocate(allocator, allocate_size, info.stream_alignment);
										success &= read_instrument->ReadAlways(allocation, byte_size);

										void* null_terminator = OffsetPointer(allocation, byte_size);
										memset(null_terminator, 0, element_byte_size);

										void** pointer = (void**)data;
										*pointer = allocation;
									}
								}
							}
						}
						else {
							success &= read_instrument->Ignore(byte_size);
						}
						return success;
					}
					else {
						size_t fundamental_byte_size = GetReflectionBasicFieldTypeByteSize(info.basic_type);
						// Allocate the pointer
						if (should_read_data) {
							void** pointer = (void**)data;
							if (allocator.allocator != nullptr) {
								*pointer = Allocate(allocator, fundamental_byte_size, GetReflectionFieldTypeAlignment(info.basic_type));
								success &= read_instrument->ReadAlways(*pointer, fundamental_byte_size);
							}
							else {
								if constexpr (!force_allocation) {
									*pointer = read_instrument->ReferenceData(fundamental_byte_size);
									success &= pointer != nullptr;
								}
								else {
									*pointer = Allocate(allocator, fundamental_byte_size, GetReflectionFieldTypeAlignment(info.basic_type));
									success &= read_instrument->Read(*pointer, fundamental_byte_size);
								}
							}
						}
						else {
							success &= read_instrument->Ignore(fundamental_byte_size);
						}
						return success;
					}
				}
				else {
					ECS_ASSERT(false, "Cannot deserialize pointer with indirection greater than 1.");
				}
			}
			else if (info.stream_type == ReflectionStreamFieldType::PointerSoA) {
				size_t byte_size = 0;
				bool success = true;

				success &= DeserializeIntVariableLengthBool(read_instrument, byte_size);
				if (should_read_data) {
					if (allocate_soa_pointer) {
						if (allocator.allocator != nullptr) {
							void** pointer = (void**)data;
							if (byte_size > 0) {
								void* allocation = Allocate(allocator, byte_size, info.stream_alignment);
								success &= read_instrument->ReadAlways(allocation, byte_size);

								*pointer = allocation;
							}
						}
						else {
							if constexpr (!force_allocation) {
								void* referenced_data = read_instrument->ReferenceData(byte_size);
								success &= referenced_data != nullptr;
								*(void**)data = referenced_data;
							}
							else {
								void** pointer = (void**)data;
								if (byte_size > 0) {
									void* allocation = Allocate(allocator, byte_size, info.stream_alignment);
									success &= read_instrument->ReadAlways(allocation, byte_size);

									*pointer = allocation;
								}
							}
						}
					}
					else {
						// Read the data into the buffer
						success &= read_instrument->Read(*(void**)data, byte_size);
					}
				}
				else {
					success &= read_instrument->Ignore(byte_size);
				}

				return success;
			}
			// We can put the PointerSoA in the same case as this
			else if (IsStream(info.stream_type)) {
				bool success = true;

				size_t byte_size = 0;
				success &= DeserializeIntVariableLengthBool(read_instrument, byte_size);

				bool update_stream_capacity = false;
				if (should_read_data) {
					if (allocator.allocator != nullptr) {
						void** pointer = (void**)data;
						if (byte_size > 0) {
							void* allocation = Allocate(allocator, byte_size, info.stream_alignment);
							success &= read_instrument->ReadAlways(allocation, byte_size);

							*pointer = allocation;
						}
						else {
							// This improves the readability
							*pointer = nullptr;
						}
						update_stream_capacity = true;
					}
					else {
						if constexpr (!force_allocation) {
							void* referenced_data = read_instrument->ReferenceData(byte_size);
							success &= referenced_data != nullptr;
							*(void**)data = referenced_data;
						}
						else {
							void** pointer = (void**)data;
							if (byte_size > 0) {
								void* allocation = Allocate(allocator, byte_size, info.stream_alignment);
								success &= read_instrument->ReadAlways(allocation, byte_size);

								*pointer = allocation;
							}
							else {
								// This improves the readability
								*pointer = nullptr;
							}
							update_stream_capacity = true;
						}
					}
				}
				else {
					success &= read_instrument->Ignore(byte_size);
				}

				if (should_read_data) {
					// If it is a string and ends with '\0', then eliminate it
					if (info.stream_type == ReflectionStreamFieldType::Stream) {
						Stream<void>* field_stream = (Stream<void>*)data;
						field_stream->size = byte_size / info.stream_byte_size;

						if (info.basic_type == ReflectionBasicFieldType::Int8) {
							char* characters = (char*)field_stream->buffer;
							if (field_stream->size > 0) {
								field_stream->size -= characters[field_stream->size - 1] == '\0';
							}
						}
						else if (info.basic_type == ReflectionBasicFieldType::Wchar_t) {
							wchar_t* characters = (wchar_t*)field_stream->buffer;
							if (field_stream->size > 0) {
								field_stream->size -= characters[field_stream->size - 1] == L'\0';
							}
						}
					}
					// They can be safely aliased
					else if (info.stream_type == ReflectionStreamFieldType::CapacityStream || info.stream_type == ReflectionStreamFieldType::ResizableStream) {
						CapacityStream<void>* field_stream = (CapacityStream<void>*)data;
						field_stream->size = (unsigned int)byte_size / info.stream_byte_size;

						if (update_stream_capacity) {
							field_stream->capacity = field_stream->size;
						}

						if (info.basic_type == ReflectionBasicFieldType::Int8) {
							char* characters = (char*)field_stream->buffer;
							if (field_stream->size > 0) {
								field_stream->size -= characters[field_stream->size - 1] == '\0';
							}
						}
						else if (info.basic_type == ReflectionBasicFieldType::Wchar_t) {
							wchar_t* characters = (wchar_t*)field_stream->buffer;
							if (field_stream->size > 0) {
								field_stream->size -= characters[field_stream->size - 1] == L'\0';
							}
						}
					}
				}

				return success;
			}
		}

		ECS_ASSERT(false, "Invalid code path - shouldn't be reached");
		return false;
	}

	template ECSENGINE_API bool ReadOrReferenceFundamentalType<ReadOrReferenceFundamentalTypeFlag::None>(const Reflection::ReflectionFieldInfo&, void*, ReadInstrument*, unsigned short, AllocatorPolymorphic, bool);
	template ECSENGINE_API bool ReadOrReferenceFundamentalType<ReadOrReferenceFundamentalTypeFlag::ForceAllocation>(const Reflection::ReflectionFieldInfo&, void*, ReadInstrument*, unsigned short, AllocatorPolymorphic, bool);
	template ECSENGINE_API bool ReadOrReferenceFundamentalType<ReadOrReferenceFundamentalTypeFlag::IgnoreData>(const Reflection::ReflectionFieldInfo&, void*, ReadInstrument*, unsigned short, AllocatorPolymorphic, bool);

	// -----------------------------------------------------------------------------------------

	bool SerializeCustomTypeReadFunctionData::ShouldIgnoreData() const
	{
		return ignore_data || read_instrument->IsSizeDetermination();
	}

	// -----------------------------------------------------------------------------------------
}