#include "ecspch.h"
#include "Serialization.h"
#include "../../Reflection/Reflection.h"
#include "../../Reflection/ReflectionStringFunctions.h"
#include "../SerializationHelpers.h"
#include "../../../Containers/Stacks.h"
#include "../../../Allocators/ResizableLinearAllocator.h"

#define DESERIALIZE_FIELD_TABLE_MAX_TYPES (32)

namespace ECSEngine {

	using namespace Reflection;

	// ------------------------------------------------------------------------------------------------------------------

	bool SerializeShouldOmitField(Stream<char> type, Stream<char> name, Stream<SerializeOmitField> omit_fields)
	{
		for (size_t index = 0; index < omit_fields.size; index++) {
			if (function::CompareStrings(omit_fields[index].type, type) && function::CompareStrings(omit_fields[index].name, name)) {
				return true;
			}
		}

		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool AssetSerializeOmitFieldsExist(const ReflectionManager* reflection_manager, Stream<SerializeOmitField> omit_fields)
	{
		for (size_t index = 0; index < omit_fields.size; index++) {
			ReflectionType type;
			if (reflection_manager->TryGetType(omit_fields[index].type, type)) {
				size_t subindex = 0;
				for (; subindex < type.fields.size; subindex++) {
					if (function::CompareStrings(type.fields[subindex].name, omit_fields[index].name)) {
						break;
					}
				}

				if (subindex == type.fields.size) {
					return false;
				}
			}
			else {
				return false;
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------

	void GetSerializeOmitFieldsFromExclude(
		const ReflectionManager* reflection_manager, 
		CapacityStream<SerializeOmitField>& omit_fields,
		Stream<char> type_name,
		Stream<Stream<char>> fields_to_keep
	)
	{
		const ReflectionType* type = reflection_manager->GetType(type_name);
		for (size_t index = 0; index < type->fields.size; index++) {
			unsigned int subindex = function::FindString(type->fields[index].name, fields_to_keep);
			if (subindex == -1) {
				omit_fields.Add({ type->name, type->fields[index].name });
			}
		}

		omit_fields.AssertCapacity();
	}

	// ------------------------------------------------------------------------------------------------------------------

	// It will write only the fields of this type, the user defined types are only specified by name
	// The custom fields should be filled in with the fields which have a custom serializer
	// The reflection manager is needed for blittable exceptions
	template<bool write_data>
	size_t WriteTypeTable(
		const ReflectionManager* reflection_manager, 
		const ReflectionType* type, 
		uintptr_t& stream, 
		Stream<SerializeOmitField> omit_fields
	) {
		size_t total_size = 0;

		// Write the name of the type first
		unsigned short type_name_size = (unsigned short)type->name.size;
		total_size += WriteWithSizeShort<write_data>(&stream, type->name.buffer, type_name_size);

		// Get the count of fields that want to be serialized
		size_t serializable_field_count = type->fields.size;

		for (size_t index = 0; index < type->fields.size; index++) {
			bool skip_serializable = type->fields[index].Has(STRING(ECS_SERIALIZATION_OMIT_FIELD));
			if (!skip_serializable && omit_fields.size > 0) {
				skip_serializable = SerializeShouldOmitField(type->name, type->fields[index].name, omit_fields);
			}
			serializable_field_count -= skip_serializable;
		}
		
		// Then write all its fields - and the count
		total_size += Write<write_data>(&stream, &serializable_field_count, sizeof(serializable_field_count));
		for (size_t index = 0; index < type->fields.size; index++) {
			bool skip_serializable = type->fields[index].Has(STRING(ECS_SERIALIZATION_OMIT_FIELD));

			if (!skip_serializable) {
				if (omit_fields.size > 0) {
					skip_serializable = SerializeShouldOmitField(type->name, type->fields[index].name, omit_fields);
				}
				
				if (!skip_serializable) {
					const ReflectionField* field = &type->fields[index];

					// Write the field name
					unsigned short field_name_size = (unsigned short)field->name.size;
					total_size += WriteWithSizeShort<write_data>(&stream, field->name.buffer, field_name_size);

					// Write the field type - the stream type, basic type, basic type count, stream byte size and the byte size all
					// need to be written or the custom serializer index with its version if a match was found
					unsigned int custom_serializer_index = -1;
					unsigned int custom_serializer_version = 0;
					bool blittable_user_defined = false;
					if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
						// Check blittable exceptions as well
						blittable_user_defined = reflection_manager->FindBlittableException(field->definition).x != -1;
						if (!blittable_user_defined) {
							blittable_user_defined = field->GetTag(STRING(ECS_GIVE_SIZE_REFLECTION)).size > 0;
							if (!blittable_user_defined) {
								custom_serializer_index = FindSerializeCustomType(field->definition);
							}
						}
					}

					total_size += Write<write_data>(&stream, &field->info.stream_type, sizeof(field->info.stream_type));
					total_size += Write<write_data>(&stream, &field->info.stream_byte_size, sizeof(field->info.stream_byte_size));
					total_size += Write<write_data>(&stream, &field->info.basic_type, sizeof(field->info.basic_type));
					total_size += Write<write_data>(&stream, &field->info.basic_type_count, sizeof(field->info.basic_type_count));
					total_size += Write<write_data>(&stream, &field->info.byte_size, sizeof(field->info.byte_size));
					total_size += Write<write_data>(&stream, &custom_serializer_index, sizeof(custom_serializer_index));
					total_size += Write<write_data>(&stream, &blittable_user_defined, sizeof(blittable_user_defined));
					total_size += Write<write_data>(&stream, &field->info.pointer_offset, sizeof(field->info.pointer_offset));

					// If user defined, write the definition aswell
					if (field->info.basic_type == ReflectionBasicFieldType::UserDefined || field->info.basic_type == ReflectionBasicFieldType::Enum) {
						Stream<char> definition_stream = field->definition;
						total_size += WriteWithSizeShort<write_data>(&stream, definition_stream.buffer, (unsigned short)definition_stream.size);
					}
				}
			}
		}

		if constexpr (write_data) {
			return 0;
		}
		else {
			return total_size;
		}
	}

	template<bool write_data>
	size_t WriteTypeTable(
		const ReflectionManager* reflection_manager, 
		const ReflectionType* type, 
		uintptr_t& stream,
		CapacityStream<Stream<char>>& deserialized_type_names,
		Stream<SerializeOmitField> omit_fields
	) {
		size_t write_total = 0;

		// Add it to the written types
		deserialized_type_names.Add(type->name);

		// Write the top most type first
		write_total += WriteTypeTable<write_data>(reflection_manager, type, stream, omit_fields);

		const size_t STACK_STORAGE = 128;
		Stream<char> _custom_dependent_types_storage[STACK_STORAGE];
		Queue<Stream<char>> custom_dependent_types(_custom_dependent_types_storage, STACK_STORAGE);

		// Now register for write all the nested types
		for (size_t index = 0; index < type->fields.size; index++) {
			const ReflectionField* field = &type->fields[index];
			bool skip_serializable = field->Has(STRING(ECS_SERIALIZATION_OMIT_FIELD));

			if (!skip_serializable && field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
				skip_serializable = SerializeShouldOmitField(type->name, field->name, omit_fields);

				if (!skip_serializable) {
					// Check to see if the type has its size given. If it does, treat it like a blittable type
					ulong2 blittable_size = GetReflectionTypeGivenFieldTag(field);
					if (blittable_size.x == -1) {
						blittable_size = reflection_manager->FindBlittableException(field->definition);
					}

					if (blittable_size.x == -1) {
						// Check that the type has not been already written
						bool is_missing = function::FindString(field->definition, deserialized_type_names) == -1;
						if (is_missing) {
							ReflectionType nested_type;
							// Can be a custom serializer type
							if (reflection_manager->TryGetType(field->definition, nested_type)) {
								custom_dependent_types.Push(field->definition);
							}
							else {
								unsigned int custom_serializer_index = FindSerializeCustomType(field->definition);
								if (custom_serializer_index != -1) {
									ECS_STACK_CAPACITY_STREAM(Stream<char>, current_custom_dependent_types, DESERIALIZE_FIELD_TABLE_MAX_TYPES / 2);

									ReflectionCustomTypeDependentTypesData dependent_data;
									dependent_data.definition = field->definition;
									dependent_data.dependent_types = current_custom_dependent_types;

									ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].container_type.dependent_types(&dependent_data);
									for (size_t subindex = 0; subindex < dependent_data.dependent_types.size; subindex++) {
										custom_dependent_types.Push(dependent_data.dependent_types[subindex]);
									}
								}
							}
						}
					}
				}
			}
		}

		Stream<char> current_dependent_type;
		while (custom_dependent_types.Pop(current_dependent_type)) {
			ReflectionType nested_type;
			if (reflection_manager->TryGetType(current_dependent_type, nested_type)) {
				bool is_missing = function::FindString(current_dependent_type, deserialized_type_names) == -1;
				if (is_missing) {
					write_total += WriteTypeTable<write_data>(reflection_manager, &nested_type, stream, deserialized_type_names, omit_fields);
				}
			}
			else {
				// Might be a nested custom serializer
				unsigned int nested_custom_serializer = FindSerializeCustomType(current_dependent_type);
				if (nested_custom_serializer != -1) {
					ECS_STACK_CAPACITY_STREAM(Stream<char>, current_custom_dependent_types, DESERIALIZE_FIELD_TABLE_MAX_TYPES / 2);

					ReflectionCustomTypeDependentTypesData dependent_data;
					dependent_data.definition = current_dependent_type;
					dependent_data.dependent_types = current_custom_dependent_types;

					ECS_SERIALIZE_CUSTOM_TYPES[nested_custom_serializer].container_type.dependent_types(&dependent_data);
					for (size_t index = 0; index < dependent_data.dependent_types.size; index++) {
						custom_dependent_types.Push(dependent_data.dependent_types[index]);
					}
				}
			}
		}

		if constexpr (write_data) {
			return 0;
		}
		else {
			return write_total;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	template<bool write_data>
	size_t WriteTypeTableWithSerializers(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		uintptr_t& stream,
		Stream<SerializeOmitField> omit_fields
	) {
		size_t write_total = 0;

		ECS_STACK_CAPACITY_STREAM(Stream<char>, deserialized_type_names, DESERIALIZE_FIELD_TABLE_MAX_TYPES);

		// Write the custom serializer versions
		unsigned int serializers_count = SerializeCustomTypeCount();
		write_total += Write<write_data>(&stream, &serializers_count, sizeof(serializers_count));

		for (size_t index = 0; index < serializers_count; index++) {
			write_total += WriteWithSizeShort<write_data>(&stream, ECS_SERIALIZE_CUSTOM_TYPES[index].name);
			write_total += Write<write_data>(&stream, &ECS_SERIALIZE_CUSTOM_TYPES[index].version, sizeof(unsigned int));
		}

		write_total += WriteTypeTable<write_data>(reflection_manager, type, stream, deserialized_type_names, omit_fields);

		return write_total;
	}

	// ------------------------------------------------------------------------------------------------------------------

	// The functor must take as parameter a uintptr_t& and write into it and return an ECS_SERIALIZE_CODE
	template<typename Functor>
	ECS_SERIALIZE_CODE SerializeFileImpl(size_t serialize_size, Stream<wchar_t> file, SerializeOptions* options, Functor&& functor) {
		AllocatorPolymorphic file_allocator = options == nullptr ? AllocatorPolymorphic{ nullptr } : options->allocator;

		ECS_SERIALIZE_CODE code = ECS_SERIALIZE_OK;
		bool serialize_status = SerializeWriteFile(file, file_allocator, serialize_size, true, [&](uintptr_t& buffer) {
			code = functor(buffer);
			});

		if (!serialize_status) {
			if (options != nullptr) {
				ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Could not open file {#} for serialization.", file);
			}
			return ECS_SERIALIZE_COULD_NOT_OPEN_OR_WRITE_FILE;
		}

		return code;
	}

	// ------------------------------------------------------------------------------------------------------------------

	// The functor must take as parameter a uintptr_t& and read from it and return an ECS_DESERIALIZE_CODE
	template<typename Functor>
	ECS_DESERIALIZE_CODE DeserializeFileImpl(Stream<wchar_t> file, DeserializeOptions* options, void** file_data, Functor&& functor) {
		ECS_DESERIALIZE_CODE code = ECS_DESERIALIZE_OK;

		AllocatorPolymorphic file_allocator = options != nullptr ? options->file_allocator : AllocatorPolymorphic{ nullptr };

		void* file_allocation = DeserializeReadBinaryFileAndKeepMemory(file, file_allocator, [&](uintptr_t& buffer) {
			code = functor(buffer);
			});

		// Could not open file
		if (file_allocation == nullptr) {
			code = ECS_DESERIALIZE_COULD_NOT_OPEN_OR_READ_FILE;
			if (options != nullptr) {
				ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Could not open file {#} for deserialization.", file);
			}
			if (file_data != nullptr) {
				*file_data = nullptr;
			}
		}
		// Deallocate the file data if the deserialization failed or if a  field allocator is specified
		else if (code != ECS_DESERIALIZE_OK || (options != nullptr && options->field_allocator.allocator != nullptr)) {
			DeallocateEx(file_allocator, file_allocation);
			if (file_data != nullptr) {
				*file_data = nullptr;
			}
		}
		else {
			if (file_data != nullptr) {
				*file_data = file_allocation;
			}
			else {
				DeallocateEx(file_allocator, file_allocation);
			}
		}

		return code;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool SerializeHasDependentTypesCustomSerializerRecurse(
		const ReflectionManager* reflection_manager, 
		Stream<char> definition, 
		bool& custom_serializer_success,
		Stream<SerializeOmitField> omit_fields
	) {
		// Get the list of dependent types
		ECS_STACK_CAPACITY_STREAM(Stream<char>, dependent_types, 16);

		unsigned int custom_serializer = FindSerializeCustomType(definition);

		if (custom_serializer != -1) {
			ReflectionCustomTypeDependentTypesData dependent_data;
			dependent_data.definition = definition;
			dependent_data.dependent_types = dependent_types;
			ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer].container_type.dependent_types(&dependent_data);

			for (size_t subindex = 0; subindex < dependent_data.dependent_types.size; subindex++) {
				ReflectionBasicFieldType basic_field_type;
				ReflectionStreamFieldType stream_field_type;

				ConvertStringToPrimitiveType(dependent_data.dependent_types[subindex], basic_field_type, stream_field_type);
				// It is not a basic type - should look for it as a dependency
				if (basic_field_type == ReflectionBasicFieldType::Unknown) {
					ReflectionType nested_type;
					// It is a reflected type - make sure its dependencies are met
					if (reflection_manager->TryGetType(dependent_data.dependent_types[subindex].buffer, nested_type)) {
						if (!SerializeHasDependentTypes(reflection_manager, &nested_type, omit_fields)) {
							custom_serializer_success = false;
							return true;
						}
					}
					else {
						// Might be a custom serializer type again
						SerializeHasDependentTypesCustomSerializerRecurse(reflection_manager, dependent_data.dependent_types[subindex], custom_serializer_success, omit_fields);
						if (!custom_serializer_success) {
							return true;
						}
					}
				}
				// If it is a basic type, then these should not count as dependencies
			}

			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool SerializeHasDependentTypes(const Reflection::ReflectionManager* reflection_manager, const Reflection::ReflectionType* type, Stream<SerializeOmitField> omit_fields)
	{
		bool custom_serializer_success = true;

		for (size_t index = 0; index < type->fields.size; index++) {
			bool skip_serializable = type->fields[index].Has(STRING(ECS_SERIALIZATION_OMIT_FIELD_REFLECT));
			if (!skip_serializable) {
				skip_serializable = SerializeShouldOmitField(type->name, type->fields[index].name, omit_fields);
				if (!skip_serializable) {
					skip_serializable = reflection_manager->FindBlittableException(type->fields[index].definition).x != -1;
				}
			}

			if (!skip_serializable && type->fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
				// Check to see if it has a custom serializer
				if (!SerializeHasDependentTypesCustomSerializerRecurse(reflection_manager, type->fields[index].definition, custom_serializer_success, omit_fields)) {
					// Check to see if it is a reflected type
					ReflectionType nested_type;
					if (reflection_manager->TryGetType(type->fields[index].definition, nested_type)) {
						if (!SerializeHasDependentTypes(reflection_manager, &nested_type, omit_fields)) {
							return false;
						}
					}
					else {
						return false;
					}
				}
				else if (!custom_serializer_success) {
					return false;
				}
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE Serialize(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		const void* data,
		Stream<wchar_t> file,
		SerializeOptions* options
	) {
		return SerializeFileImpl(SerializeSize(reflection_manager, type, data, options), file, options, [&](uintptr_t& buffer) {
			return Serialize(reflection_manager, type, data, buffer, options);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------

	template<bool write_data>
	ECS_SERIALIZE_CODE SerializeImplementation(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		const void* data,
		uintptr_t& stream,
		SerializeOptions* options,
		size_t* total_size
	) {
		bool has_options = options != nullptr;

		Stream<SerializeOmitField> omit_fields = has_options ? options->omit_fields : Stream<SerializeOmitField>(nullptr, 0);

		if (!has_options || (has_options && options->verify_dependent_types)) {
			// Check that the type has all its dependencies met
			bool has_dependencies = SerializeHasDependentTypes(reflection_manager, type, omit_fields);
			if (!has_dependencies) {
				if (has_options) {
					ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Could not serialize type {#}. Its nested user defined types are missing.", type->name);
				}
				return ECS_SERIALIZE_MISSING_DEPENDENT_TYPES;
			}
		}

		// Write the header if it has one
		if (has_options && options->header.size > 0) {
			*total_size += WriteWithSize<write_data>(&stream, options->header.buffer, options->header.size);
		}

		if (!has_options || (has_options && options->write_type_table)) {
			// The type table must be written
			*total_size += WriteTypeTableWithSerializers<write_data>(reflection_manager, type, stream, omit_fields);
		}

		SerializeOptions _nested_options;
		SerializeOptions* nested_options = nullptr;
		if (has_options) {
			nested_options = &_nested_options;
			nested_options->error_message = options->error_message;
			nested_options->verify_dependent_types = false;
			nested_options->write_type_table = false;
			nested_options->omit_fields = omit_fields;
		}

		for (size_t index = 0; index < type->fields.size; index++) {
			const ReflectionField* field = &type->fields[index];

			bool skip_serializable = field->Has(STRING(ECS_SERIALIZATION_OMIT_FIELD));

			if (!skip_serializable && omit_fields.size > 0) {
				skip_serializable = SerializeShouldOmitField(type->name, type->fields[index].name, omit_fields);
			}

			if (!skip_serializable) {
				// If a basic type, copy the data directly
				const void* field_data = function::OffsetPointer(data, field->info.pointer_offset);
				ReflectionStreamFieldType stream_type = field->info.stream_type;
				ReflectionBasicFieldType basic_type = field->info.basic_type;

				bool is_user_defined = basic_type == ReflectionBasicFieldType::UserDefined;
				ReflectionType nested_type;
				unsigned int custom_serializer_index = -1;

				Stream<char> definition_stream = field->definition;

				if (is_user_defined) {
					// Verify the given size fields
					ulong2 blittable_size = GetReflectionTypeGivenFieldTag(field);
					if (blittable_size.x == -1) {
						blittable_size = reflection_manager->FindBlittableException(definition_stream);
					}

					if (blittable_size.x != -1) {
						// Write the data as is and continue to the next field
						*total_size += Write<write_data>(&stream, field_data, blittable_size.x * field->info.basic_type_count);
						continue;
					}

					if (!reflection_manager->TryGetType(field->definition, nested_type)) {
						custom_serializer_index = FindSerializeCustomType(definition_stream);
					}
				}

				if (custom_serializer_index != -1) {
					SerializeCustomTypeWriteFunctionData custom_write_data;
					custom_write_data.data = field_data;
					custom_write_data.definition = definition_stream;
					custom_write_data.options = nested_options;
					custom_write_data.reflection_manager = reflection_manager;
					custom_write_data.stream = &stream;
					custom_write_data.write_data = write_data;

					*total_size += ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].write(&custom_write_data);
				}
				else {
					if (stream_type == ReflectionStreamFieldType::Basic && !is_user_defined) {
						*total_size += Write<write_data>(&stream, field_data, field->info.byte_size);
					}
					else {
						// User defined, call the serialization for it
						if (stream_type == ReflectionStreamFieldType::Basic && is_user_defined) {
							// No need to test the return code since it cannot fail if it gets to here
							SerializeImplementation<write_data>(reflection_manager, &nested_type, field_data, stream, nested_options, total_size);
						}
						// Determine if it is a pointer, basic array or stream
						// If pointer, only do it for 1 level of indirection and ASCII or wide char strings
						else if (stream_type == ReflectionStreamFieldType::Pointer) {
							if (GetReflectionFieldPointerIndirection(type->fields[index].info) == 1) {
								// Treat user defined pointers as pointers to a single entity
								if (is_user_defined) {
									// No need to test the return code since it cannot fail if it gets to here
									SerializeImplementation<write_data>(reflection_manager, &nested_type, *(void**)field_data, stream, nested_options, total_size);
								}
								else if (basic_type == ReflectionBasicFieldType::Int8) {
									const char* characters = *(const char**)field_data;
									size_t character_size = strlen(characters) + 1;
									*total_size += WriteWithSize<write_data>(&stream, characters, character_size);
								}
								else if (basic_type == ReflectionBasicFieldType::Wchar_t) {
									const wchar_t* characters = *(const wchar_t**)field_data;
									size_t character_size = wcslen(characters) + 1;
									*total_size += WriteWithSize<write_data>(&stream, characters, character_size * sizeof(wchar_t));
								}
								// Other type of pointers cannot be serialized - they must be turned into streams
								else {
									ECS_ASSERT(false, "Cannot serialize pointers of indirection 1 of types other than char or wchar_t.");
								}
							}
							// Other types of pointers cannot be serialized - multi level indirections are not allowed
							else {
								ECS_ASSERT(false, "Cannot serialize pointers with indirection greater than 1.");
							}
						}
						else if (stream_type == ReflectionStreamFieldType::BasicTypeArray) {
							// If it is a nested type, handle it separately
							if (is_user_defined) {
								size_t basic_count = field->info.basic_type_count;
								size_t element_byte_size = GetBasicTypeArrayElementSize(field->info);

								for (size_t index = 0; index < basic_count; index++) {
									// No need to test the return code since if it gets to here it cannot fail
									SerializeImplementation<write_data>(reflection_manager, &nested_type, function::OffsetPointer(field_data, index * element_byte_size), stream, nested_options, total_size);
								}
							}
							else {
								// Write the whole data at once
								*total_size += Write<write_data>(&stream, field_data, field->info.byte_size);
							}
						}
						// Streams should be handled by the custom serializer					
						else {
							// Check for streams. All, Stream, CapacityStream and ResizableStream can be aliased with a normal stream
							// Since we are interested in writing only the data along side the byte size
							Stream<void> field_stream = GetReflectionFieldStreamVoid(field->info, data);
							if (is_user_defined) {
								size_t type_byte_size = GetReflectionTypeByteSize(&nested_type);

								// Write the size first and then the user defined
								*total_size += Write<write_data>(&stream, &field_stream.size, sizeof(field_stream.size));
								for (size_t index = 0; index < field_stream.size; index++) {
									SerializeImplementation<write_data>(reflection_manager, &nested_type, function::OffsetPointer(field_data, index * type_byte_size), stream, nested_options, total_size);
								}
							}
							else {
								field_stream.size *= GetReflectionFieldStreamElementByteSize(field->info);
								*total_size += WriteWithSize<write_data>(&stream, field_stream.buffer, field_stream.size);
							}
						}
					}
				}
			}
		}

		return ECS_SERIALIZE_OK;
	}

	ECS_SERIALIZE_CODE Serialize(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		const void* data,
		uintptr_t& stream,
		SerializeOptions* options
	) {
		size_t dummy = 0;
		return SerializeImplementation<true>(reflection_manager, type, data, stream, options, &dummy);
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t SerializeSize(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		const void* data,
		SerializeOptions* options
	) {
		size_t total_size = 0;
		uintptr_t dummy_stream = 0;
		ECS_SERIALIZE_CODE code = SerializeImplementation<false>(reflection_manager, type, data, dummy_stream, options, &total_size);
		return code != ECS_SERIALIZE_OK ? -1 : total_size;
	}

	// ------------------------------------------------------------------------------------------------------------------

	void SerializeFieldTable(const Reflection::ReflectionManager* reflection_manager, const Reflection::ReflectionType* type, uintptr_t& stream, Stream<SerializeOmitField> omit_fields)
	{
		WriteTypeTableWithSerializers<true>(reflection_manager, type, stream, omit_fields);
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t SerializeFieldTableSize(const Reflection::ReflectionManager* reflection_manager, const Reflection::ReflectionType* type, Stream<SerializeOmitField> omit_fields)
	{
		uintptr_t dummy = 0;
		return WriteTypeTableWithSerializers<false>(reflection_manager, type, dummy, omit_fields);
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE Deserialize(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		void* address,
		Stream<wchar_t> file,
		DeserializeOptions* options,
		void** file_data
	) {
		return DeserializeFileImpl(file, options, file_data, [&](uintptr_t& buffer) {
			return Deserialize(reflection_manager, type, address, buffer, options);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------

	// Forward declaration
	void IgnoreType(
		uintptr_t& stream,
		DeserializeFieldTable deserialize_table,
		unsigned int type_index,
		const ReflectionManager* deserialized_manager,
		unsigned int count = 1
	);

	// Function to skip only a certain field of the type
	void IgnoreTypeField(
		uintptr_t& stream,
		DeserializeFieldTable deserialize_table,
		unsigned int type_index,
		unsigned int field_index,
		const ReflectionManager* deserialized_manager
	) {
		const DeserializeFieldInfo* info = &deserialize_table.types[type_index].fields[field_index];
		// If the field is not user defined, can handle it now
		if (info->basic_type != ReflectionBasicFieldType::UserDefined) {
			if (info->stream_type == ReflectionStreamFieldType::Basic || info->stream_type == ReflectionStreamFieldType::BasicTypeArray) {
				Ignore(&stream, info->byte_size);
			}
			else if (info->stream_type == ReflectionStreamFieldType::Pointer) {
				if (info->basic_type == ReflectionBasicFieldType::Int8) {
					IgnoreWithSize(&stream);
				}
				else if (info->basic_type == ReflectionBasicFieldType::Wchar_t) {
					IgnoreWithSize(&stream);
				}
			}
			else {
				// All the other stream types can be aliased
				IgnoreWithSize(&stream);
			}
		}
		else {
			// Verify blittable exception
			if (info->user_defined_as_blittable) {
				Ignore(&stream, info->byte_size);
			}
			else {
				Stream<char> field_definition = info->definition;

				unsigned int custom_serializer_index = info->custom_serializer_index;
				if (custom_serializer_index != -1) {
					// Search the type
					custom_serializer_index = FindSerializeCustomType(field_definition);
					ECS_ASSERT(custom_serializer_index != -1);

					DeserializeOptions options;
					options.read_type_table = false;
					options.field_table = &deserialize_table;

					// Use the read with the read data set to false.
					SerializeCustomTypeReadFunctionData read_data;
					read_data.data = nullptr;
					read_data.definition = field_definition;
					read_data.options = &options;
					read_data.read_data = false;
					read_data.was_allocated = false;
					read_data.reflection_manager = deserialized_manager;
					read_data.stream = &stream;
					read_data.version = deserialize_table.custom_serializers[custom_serializer_index];

					ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].read(&read_data);
				}
				else {
					unsigned int nested_type = deserialize_table.TypeIndex(field_definition);
					ECS_ASSERT(nested_type != -1);

					if (info->stream_type == ReflectionStreamFieldType::Basic) {
						IgnoreType(stream, deserialize_table, nested_type, deserialized_manager);
					}
					// Indirection 1
					else if (info->stream_type == ReflectionStreamFieldType::Pointer) {
						if (info->basic_type_count == 1) {
							IgnoreType(stream, deserialize_table, nested_type, deserialized_manager);
						}
						else {
							ECS_ASSERT(false, "Pointer Indirection greater than 1!");
						}
					}
					else if (info->stream_type == ReflectionStreamFieldType::BasicTypeArray) {
						IgnoreType(stream, deserialize_table, nested_type, deserialized_manager, info->basic_type_count);
					}
					// The stream types
					else {
						size_t stream_count = 0;
						Read<true>(&stream, &stream_count, sizeof(stream_count));
						IgnoreType(stream, deserialize_table, nested_type, deserialized_manager, stream_count);
					}
				}
			}
		}
	}

	// Function to skip a type, or a number of type instances
	void IgnoreType(
		uintptr_t& stream,
		DeserializeFieldTable deserialize_table,
		unsigned int type_index,
		const ReflectionManager* deserialized_manager,
		unsigned int count
	) {
		for (unsigned int counter = 0; counter < count; counter++) {
			for (unsigned int index = 0; index < deserialize_table.types[type_index].fields.size; index++) {
				IgnoreTypeField(stream, deserialize_table, type_index, index, deserialized_manager);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_DESERIALIZE_CODE DeserializeImplementation(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		void* address,
		uintptr_t& stream,
		DeserializeOptions* options,
		size_t* buffer_size
	) {
		bool has_options = options != nullptr;

		Stream<SerializeOmitField> omit_fields = has_options ? options->omit_fields : Stream<SerializeOmitField>(nullptr, 0);

		Stream<char> type_name = type->name;;

		bool has_backup_allocator = has_options && options->backup_allocator.allocator != nullptr;
		if (!has_options || (has_options && options->verify_dependent_types)) {
			// Verify that it has its dependencies met
			bool has_dependencies = SerializeHasDependentTypes(reflection_manager, type, omit_fields);
			if (!has_dependencies) {
				if (has_options) {
					ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Could not deserialize type {#} because it doesn't have its dependencies met.", type_name);
				}
				return ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES;
			}
		}

		if (has_options && options->header.buffer != nullptr) {
			// Read the header
			options->header.buffer = (void*)stream;
			Read<true>(&stream, options->header.buffer, options->header.size);

			if (options->validate_header != nullptr) {
				bool is_valid = options->validate_header(options->header, options->validate_header_data);
				if (!is_valid) {
					ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Could not deserialize type {#} because the header is not valid.", type_name);
					return ECS_DESERIALIZE_INVALID_HEADER;
				}
			}
		}

		// The type table now
		const size_t STACK_DESERIALIZE_TABLE_BUFFER_CAPACITY = ECS_KB * 8;
		void* deserialize_table_stack_allocation = ECS_STACK_ALLOC(STACK_DESERIALIZE_TABLE_BUFFER_CAPACITY);

		LinearAllocator table_linear_allocator(deserialize_table_stack_allocation, STACK_DESERIALIZE_TABLE_BUFFER_CAPACITY);
		DeserializeFieldTable deserialize_table;

		bool fail_if_mismatch = has_options && options->fail_if_field_mismatch;
		bool use_resizable_stream_allocator = has_options && options->use_resizable_stream_allocator;
		bool default_initialize_missing_fields = has_options && options->default_initialize_missing_fields;

		//bool use_deserialize_table = !has_options || (options->field_table != nullptr);
		// At the moment allow only table deserialization
		bool use_deserialize_table = true;
		unsigned int type_index = 0;
		size_t field_count = type->fields.size;

		auto read_type_table_from_file = [&]() {
			deserialize_table = DeserializeFieldTableFromData(stream, GetAllocatorPolymorphic(&table_linear_allocator));
			// Check to see if the table is valid
			if (deserialize_table.types.size == 0) {
				// The file was corrupted
				if (has_options) {
					ECS_FORMAT_ERROR_MESSAGE(options->error_message, "The field table has been corrupted when trying to deserialize type {#}."
						" The deserialization cannot continue.", type_name);
				}
				return ECS_DESERIALIZE_CORRUPTED_FILE;
			}
			unsigned int type_index = deserialize_table.TypeIndex(type_name);
			ECS_ASSERT(type_index != -1);
			field_count = deserialize_table.types[type_index].fields.size;
			return ECS_DESERIALIZE_OK;
		};
 
		AllocatorPolymorphic field_allocator = { nullptr };

		if (has_options) {		
			if (options->field_table != nullptr) {
				deserialize_table = *options->field_table;

				type_index = deserialize_table.TypeIndex(type_name);
				field_count = deserialize_table.types[type_index].fields.size;
			}
			else if (options->read_type_table) {
				ECS_DESERIALIZE_CODE code = read_type_table_from_file();
				if (code != ECS_DESERIALIZE_OK) {
					return code;
				}
			}

			field_allocator = options->field_allocator;
		}
		else {
			ECS_DESERIALIZE_CODE code = read_type_table_from_file();
			if (code != ECS_DESERIALIZE_OK) {
				return code;
			}
		}

		DeserializeOptions _nested_options;
		DeserializeOptions* nested_options = &_nested_options;
		nested_options->field_table = &deserialize_table;

		if (has_options) {
			nested_options = &_nested_options;
			nested_options->error_message = options->error_message;
			nested_options->fail_if_field_mismatch = options->fail_if_field_mismatch;
			nested_options->field_allocator = options->field_allocator;
			nested_options->backup_allocator = options->backup_allocator;
			nested_options->use_resizable_stream_allocator = options->use_resizable_stream_allocator;
			nested_options->read_type_table = false;
			nested_options->verify_dependent_types = false;
			nested_options->omit_fields = omit_fields;
		}

		auto deserialize_incompatible_basic = [](uintptr_t& data, void* field_data, const DeserializeFieldInfo& file_info, const ReflectionFieldInfo& type_info) {
			// Read the data from file into a double and then convert the double into the appropriate type
			unsigned int file_basic_component_count = BasicTypeComponentCount(file_info.basic_type);
			unsigned int type_basic_component_count = BasicTypeComponentCount(type_info.basic_type);

			ReflectionBasicFieldType file_single_type = ConvertBasicTypeMultiComponentToSingle(file_info.basic_type);
			ReflectionBasicFieldType type_single_type = ConvertBasicTypeMultiComponentToSingle(type_info.basic_type);

			double file_data[4];

			unsigned int iteration_count = function::ClampMin(file_basic_component_count, type_basic_component_count);
			unsigned int pointer_increment = type_info.byte_size / type_basic_component_count;
			unsigned int per_component_byte_size = file_info.byte_size / file_basic_component_count;
			for (unsigned int index = 0; index < iteration_count; index++) {
				void* type_data = function::OffsetPointer(field_data, pointer_increment * index);
				Read<true>(&data, &file_data, per_component_byte_size);

				ConvertReflectionBasicField(file_info.basic_type, type_info.basic_type, file_data, field_data);
			}
		};

		auto deserialize_with_table = [&]() {
			size_t temp_allocator_size = (has_options && options->deserialized_field_manager != nullptr) ? 0 : ECS_KB * 16;
			void* temp_allocator_buffer = ECS_STACK_ALLOC(temp_allocator_size);
			LinearAllocator temp_allocator(temp_allocator_buffer, temp_allocator_size);

			ReflectionManager _temporary_manager;
			ReflectionManager* deserialized_manager = has_options ? options->deserialized_field_manager : nullptr;
			if (deserialized_manager == nullptr) {
				// Create a temporary deserialized manager that can be used with the ignore calls
				_temporary_manager.type_definitions.Initialize(&temp_allocator, HashTableCapacityForElements(deserialize_table.types.size));
				deserialize_table.ToNormalReflection(&_temporary_manager, GetAllocatorPolymorphic(&temp_allocator));
				deserialized_manager = &_temporary_manager;
			}
			nested_options->deserialized_field_manager = deserialized_manager;

			if (default_initialize_missing_fields) {
				// For every field that is not found in the file
				for (size_t index = 0; index < type->fields.size; index++) {
					size_t subindex = 0;
					for (; subindex < field_count; subindex++) {
						if (deserialize_table.types[type_index].fields[subindex].name == type->fields[index].name) {
							break;
						}
					}

					if (subindex == field_count) {
						// Was not found - default initialize
						reflection_manager->SetInstanceFieldDefaultData(&type->fields[index], address);
					}
				}
			}
			
			// Iterate over the type stored inside the file
			// and for each field that is still valid read it
			for (size_t index = 0; index < field_count; index++) {
				// Search the field inside the type
				size_t subindex = 0;
				for (; subindex < type->fields.size; subindex++) {
					if (function::CompareStrings(type->fields[subindex].name, deserialize_table.types[type_index].fields[index].name)) {
						break;
					}
				}

				DeserializeFieldInfo file_field_info = deserialize_table.types[type_index].fields[index];
				Stream<char> field_definition = type->fields[subindex].definition;

				// The field doesn't exist in the current type
				if (subindex == type->fields.size) {
					// Just go to the next field
					// But first the data inside the stream must be skipped
					IgnoreTypeField(stream, deserialize_table, type_index, index, deserialized_manager);
					continue;
				}

				// Check to see if it is omitted
				if (SerializeShouldOmitField(type->name, type->fields[subindex].name, omit_fields)) {
					IgnoreTypeField(stream, deserialize_table, type_index, index, deserialized_manager);
					continue;
				}

				void* field_data = function::OffsetPointer(address, type->fields[subindex].info.pointer_offset);

				if (file_field_info.user_defined_as_blittable) {
					// See if the field has given size and it matches the byte size
					// If this is not a given size field or the byte size does not match then ignore it
					ulong2 field_given_size = GetReflectionTypeGivenFieldTag(&type->fields[subindex]);
					if (field_given_size.x == -1) {
						// Check blittable exception
						field_given_size = reflection_manager->FindBlittableException(field_definition);
					}

					// Read the type as a blittable value only if the byte sizes actually match
					if (file_field_info.byte_size == field_given_size.x) {
						Read<read_data>(&stream, field_data, file_field_info.byte_size);
					}
					else {
						// Ignore this field with the given byte size since the blittable size has changed
						// Or it is no longer a blittable type
						Ignore(&stream, file_field_info.byte_size);
					}
					continue;
				}

				// Verify the basic and the stream type
				ReflectionFieldInfo type_field_info = type->fields[subindex].info;

				// Both types are consistent, check for the same user defined type if there is one
				if (file_field_info.basic_type == type_field_info.basic_type && file_field_info.stream_type == type_field_info.stream_type) {
					if (file_field_info.basic_type == ReflectionBasicFieldType::UserDefined && file_field_info.custom_serializer_index == -1) {
						// Check for the type to be the same
						if (!function::CompareStrings(file_field_info.definition, field_definition)) {
							if (fail_if_mismatch) {
								if (has_options) {
									ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Deserialization for type {#} failed."
										" User defined field mismatch for {#}. In file found definition {#}, current type {#}.", 
										type_name,
										type->fields[subindex].name, 
										file_field_info.definition, 
										field_definition
									);
								}
								return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
							}

							// Ignore the data
							IgnoreTypeField(stream, deserialize_table, type_index, index, deserialized_manager);
							continue;
						}
						// Good to go, read the data using the nested type, or custom serializer
						else {
							const ReflectionType* nested_type = reflection_manager->GetType(field_definition);
							// Handle the stream cases
							switch (type_field_info.stream_type) {
							case ReflectionStreamFieldType::Basic:
							{
								ECS_DESERIALIZE_CODE code = DeserializeImplementation<read_data>(reflection_manager, nested_type, field_data, stream, nested_options, buffer_size);
								if (code != ECS_DESERIALIZE_OK) {
									return code;
								}
							}
							break;
							case ReflectionStreamFieldType::Pointer:
							{
								if (GetReflectionFieldPointerIndirection(type_field_info) == 1 && file_field_info.basic_type_count == 1) {
									ECS_DESERIALIZE_CODE code = DeserializeImplementation<read_data>(reflection_manager, nested_type, *(void**)field_data, stream, nested_options, buffer_size);
									if (code != ECS_DESERIALIZE_OK) {
										return code;
									}
								}
								else {
									if (fail_if_mismatch) {
										if (has_options) {
											ECS_FORMAT_ERROR_MESSAGE(
												options->error_message,
												"Could not deserialize type {#}, field {#} with nested type as {#}. "
												"The pointer level indirection is different.",
												type_name,
												type->fields[subindex].name,
												field_definition
											);
										}
										return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
									}
								}
							}
							break;
							case ReflectionStreamFieldType::BasicTypeArray:
							{
								// Choose the minimum between the two values
								unsigned short elements_to_read = function::ClampMax(type_field_info.basic_type_count, file_field_info.basic_type_count);
								unsigned short elements_to_ignore = file_field_info.basic_type_count - elements_to_read;

								size_t element_byte_size = GetBasicTypeArrayElementSize(type_field_info);
								for (unsigned short element_index = 0; element_index < elements_to_read; element_index++) {
									ECS_DESERIALIZE_CODE code = DeserializeImplementation<read_data>(
										reflection_manager,
										nested_type,
										function::OffsetPointer(field_data, element_byte_size * element_index),
										stream,
										nested_options,
										buffer_size
									);
									if (code != ECS_DESERIALIZE_OK) {
										return code;
									}
								}

								if (elements_to_ignore > 0) {
									unsigned int nested_type_table_index = deserialize_table.TypeIndex(file_field_info.definition);
									IgnoreType(stream, deserialize_table, nested_type_table_index, deserialized_manager, elements_to_ignore);
								}
							}
							break;
							case ReflectionStreamFieldType::Stream:
							case ReflectionStreamFieldType::CapacityStream:
							case ReflectionStreamFieldType::ResizableStream:
							{
								size_t element_count = 0;
								// This will read the byte size
								Read<true>(&stream, &element_count, sizeof(element_count));

								*buffer_size += element_count;

								// Must divide by the byte size of each element
								element_count /= file_field_info.stream_byte_size;

								size_t nested_type_byte_size = GetReflectionTypeByteSize(nested_type);
								size_t allocation_size = nested_type_byte_size * element_count;

								void* allocation = nullptr;
								if constexpr (read_data) {
									// Check resizable stream allocator
									if (type_field_info.stream_type == ReflectionStreamFieldType::ResizableStream && use_resizable_stream_allocator) {
										ResizableStream<void>* resizable_stream = (ResizableStream<void>*)field_data;
										allocation = Allocate(resizable_stream->allocator, allocation_size);

										resizable_stream->buffer = allocation;
										resizable_stream->capacity = element_count;
										resizable_stream->size = element_count;
									}
									else {
										AllocatorPolymorphic allocator_to_use = field_allocator.allocator != nullptr ? field_allocator : options->backup_allocator;
										allocation = Allocate(allocator_to_use, allocation_size);

										if (type_field_info.stream_type == ReflectionStreamFieldType::Stream) {
											Stream<void>* normal_stream = (Stream<void>*)field_data;
											normal_stream->buffer = allocation;
											normal_stream->size = element_count;
										}
										// The resizable stream and the capacity stream can be handle together
										else {
											CapacityStream<void>* capacity_stream = (CapacityStream<void>*)field_data;
											capacity_stream->buffer = allocation;
											capacity_stream->size = element_count;
											capacity_stream->capacity = element_count;
										}
									}
								}

								// Now deserialize each instance
								// If the read_data is false, then the offset here does nothing
								for (size_t element_index = 0; element_index < element_count; element_index++) {
									ECS_DESERIALIZE_CODE code = DeserializeImplementation<read_data>(
										reflection_manager,
										nested_type,
										function::OffsetPointer(allocation, element_index * nested_type_byte_size),
										stream,
										nested_options,
										buffer_size
										);
									if (code != ECS_DESERIALIZE_OK) {
										return code;
									}
								}
							}
							break;
							default:
								ECS_ASSERT(false, "No valid stream type when deserializing.");
							}
						}
					}
					else if (file_field_info.custom_serializer_index != -1) {
						// Verify that a serializer binds to it
						unsigned int current_custom_serializer_index = FindSerializeCustomType(field_definition);
						ECS_ASSERT(current_custom_serializer_index != -1, "No custom serializer was found when deserializing.");
					
						// Get the version
						SerializeCustomTypeReadFunctionData custom_data;
						custom_data.data = field_data;
						custom_data.definition = field_definition;
						custom_data.options = nested_options;
						custom_data.read_data = read_data;
						custom_data.was_allocated = false;
						custom_data.reflection_manager = reflection_manager;
						custom_data.stream = &stream;
						custom_data.version = deserialize_table.custom_serializers[current_custom_serializer_index];

						*buffer_size += ECS_SERIALIZE_CUSTOM_TYPES[current_custom_serializer_index].read(&custom_data);
					}
					// Good to go, read the data - no user defined type here
					else {
						AllocatorPolymorphic field_allocator = { nullptr };
						if (has_options) {
							field_allocator = options->GetFieldAllocator(type_field_info.stream_type, field_data);
						}

						ReadOrReferenceFundamentalType<read_data>(type_field_info, field_data, stream, file_field_info.basic_type_count, field_allocator);
					}
				}
				// Mismatch - try to solve it
				else {
					// If the stream type has changed, then keep in mind that
					bool is_stream_type_different = file_field_info.stream_type != type_field_info.stream_type;
					bool is_stream_type_basic_array = type_field_info.stream_type == ReflectionStreamFieldType::BasicTypeArray;

					// If the discrepency is the basic type only, try to solve it
					if (type_field_info.stream_type == ReflectionStreamFieldType::Basic && file_field_info.stream_type == ReflectionStreamFieldType::Basic) {
						if constexpr (read_data) {
							deserialize_incompatible_basic(stream, field_data, file_field_info, type_field_info);
						}
					}
					else {
						if (type_field_info.stream_type != ReflectionStreamFieldType::Basic) {
							if (file_field_info.stream_type == ReflectionStreamFieldType::Basic) {
								// Just skip the field - cannot deserialize basic types into stream types
								Ignore(&stream, file_field_info.byte_size);
							}
							else {
								// Stream type mismatch - this is fine, can deserialize any stream type into any other
								size_t pointer_data_byte_size = 0;
								Read<true>(&stream, &pointer_data_byte_size, sizeof(pointer_data_byte_size));
								
								size_t element_count = pointer_data_byte_size / file_field_info.stream_byte_size;
								if (type_field_info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
									size_t elements_to_read = function::ClampMin(element_count, (size_t)type_field_info.basic_type_count);
									if (type_field_info.basic_type != file_field_info.basic_type) {
										if constexpr (read_data) {
											for (size_t index = 0; index < elements_to_read; index++) {
												deserialize_incompatible_basic(stream, field_data, file_field_info, type_field_info);
											}
										}
										else {
											Ignore(&stream, pointer_data_byte_size);
										}
									}
									else {
										// Just read the data
										if constexpr (read_data) {
											Read<true>(&stream, field_data, elements_to_read * file_field_info.stream_byte_size);
										}
										else {
											Ignore(&stream, pointer_data_byte_size);
										}
									}

									// Ignore the rest elements
									Ignore(&stream, file_field_info.stream_byte_size * (element_count - elements_to_read));
								}
								else {
									// Normal streams

									// If basic mismatch aswell, need to check the allocator
									if constexpr (read_data) {
										void* allocation = nullptr;
										if (type_field_info.basic_type != file_field_info.basic_type) {
											if (has_options) {
												if (options->use_resizable_stream_allocator && type_field_info.stream_type == ReflectionStreamFieldType::ResizableStream) {
													ResizableStream<void>* field_stream = (ResizableStream<void>*)field_data;
													allocation = Allocate(field_stream->allocator, element_count * type_field_info.stream_byte_size);
												}
												else if (options->field_allocator.allocator != nullptr) {
													allocation = Allocate(options->field_allocator, element_count * type_field_info.stream_byte_size);
												}
											}
											else {
												allocation = Allocate(options->backup_allocator, element_count * type_field_info.stream_byte_size);
											}

											for (size_t index = 0; index < element_count; index++) {
												deserialize_incompatible_basic(stream, function::OffsetPointer(allocation, index), file_field_info, type_field_info);
											}
										}
										else {
											if (!has_options) {
												// Just reference the data
												ReferenceData<true>(&stream, &allocation, pointer_data_byte_size);
											}
											else {
												if (options->use_resizable_stream_allocator && type_field_info.stream_type == ReflectionStreamFieldType::ResizableStream) {
													ResizableStream<void>* field_stream = (ResizableStream<void>*)field_data;
													allocation = Allocate(field_stream->allocator, element_count * type_field_info.stream_byte_size);
												}
												else if (options->field_allocator.allocator != nullptr) {
													allocation = Allocate(options->field_allocator, element_count * type_field_info.stream_byte_size);
												}
												else {
													// Just reference the data
													ReferenceData<true>(&stream, &allocation, pointer_data_byte_size);
												}
											}
										}

										if (type_field_info.stream_type == ReflectionStreamFieldType::Stream) {
											Stream<void>* field_stream = (Stream<void>*)field_data;
											field_stream->buffer = allocation;
											field_stream->size = element_count;

											// Trim the \0 for strings if it is included
											if (type_field_info.basic_type == ReflectionBasicFieldType::Int8) {
												char* string = (char*)allocation;
												if (string[field_stream->size - 1] == '\0') {
													field_stream->size--;
												}
											}
											else if (type_field_info.basic_type == ReflectionBasicFieldType::Wchar_t) {
												wchar_t* string = (wchar_t*)allocation;
												if (string[field_stream->size - 1] == L'\0') {
													field_stream->size--;
												}
											}
										}
										// It is fine to alias the resizable with the capacity
										else if (type_field_info.stream_type == ReflectionStreamFieldType::CapacityStream ||
											type_field_info.stream_type == ReflectionStreamFieldType::ResizableStream) {
											CapacityStream<void>* field_stream = (CapacityStream<void>*)field_data;
											field_stream->buffer = allocation;
											field_stream->size = element_count;
											field_stream->capacity = element_count;

											// Trim the \0 for strings if it is included
											if (type_field_info.basic_type == ReflectionBasicFieldType::Int8) {
												char* string = (char*)allocation;
												if (string[field_stream->size - 1] == '\0') {
													field_stream->size--;
												}
											}
											else if (type_field_info.basic_type == ReflectionBasicFieldType::Wchar_t) {
												wchar_t* string = (wchar_t*)allocation;
												if (string[field_stream->size - 1] == L'\0') {
													field_stream->size--;
												}
											}
										}
										else if (type_field_info.stream_type == ReflectionStreamFieldType::Pointer) {
											if (GetReflectionFieldPointerIndirection(type_field_info) != 1) {
												ECS_ASSERT(false, "Cannot deserialize pointers of indirection greater than 1");

												if (has_options) {
													ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Cannot deserialize field {#} which is a "
														"pointer of indirection greater than 1.", type->fields[subindex].name);
												}
												return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
											}

											if (type_field_info.basic_type == ReflectionBasicFieldType::Int8) {
												char** string = (char**)field_data;
												*string = (char*)allocation;
											}
											else if (type_field_info.basic_type == ReflectionBasicFieldType::Wchar_t) {
												wchar_t** string = (wchar_t**)field_data;
												*string = (wchar_t*)allocation;
											}
											else {
												ECS_ASSERT(false, "Cannot deserialize pointers of indirection greater than 1");

												if (has_options) {
													ECS_FORMAT_ERROR_MESSAGE(options->error_message, "Cannot deserialize field {#} which is a "
														"pointer of indirection greater than 1.", type->fields[subindex].name);
												}
												return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
											}
										}
									}
									else {
										*buffer_size += element_count * (size_t)type_field_info.stream_byte_size;
									}
								}
							}
						}
						else {
							// Just ignore the data
							size_t pointer_data_byte_size = 0;
							Read<true>(&stream, &pointer_data_byte_size, sizeof(pointer_data_byte_size));

							Ignore(&stream, pointer_data_byte_size);
						}
					}
				}
			}

			return ECS_DESERIALIZE_OK;
		};

		auto deserialize_without_table = [&]() {
			ECS_ASSERT(false);
			return ECS_DESERIALIZE_OK;
		};

		// Handle separately the utilization of deserialize table and its absence
		ECS_DESERIALIZE_CODE return_code = ECS_DESERIALIZE_OK;
		if (use_deserialize_table) {
			return_code = deserialize_with_table();
		}
		// Use the type as is to deserialize
		else {
			return_code = deserialize_without_table();
		}

		return return_code;
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE Deserialize(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		void* address, 
		uintptr_t& stream,
		DeserializeOptions* options
	)
	{
		size_t buffer_size = 0;
		return DeserializeImplementation<true>(reflection_manager, type, address, stream, options, &buffer_size);
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSize(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		uintptr_t& data,
		DeserializeOptions* options,
		ECS_DESERIALIZE_CODE* code
	) {
		size_t buffer_size = 0;
		ECS_DESERIALIZE_CODE deserialize_code = DeserializeImplementation<false>(reflection_manager, type, nullptr, data, options, &buffer_size);
		if (deserialize_code != ECS_DESERIALIZE_OK) {
			buffer_size = -1;
		}

		if (code != nullptr) {
			*code = deserialize_code;
		}
		return buffer_size;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool ValidateDeserializeFieldInfo(const DeserializeFieldInfo& field_info) {
		if ((unsigned char)field_info.stream_type >= (unsigned char)ReflectionStreamFieldType::COUNT) {
			return false;
		}

		if ((unsigned char)field_info.basic_type >= (unsigned char)ReflectionBasicFieldType::COUNT) {
			return false;
		}

		if (field_info.basic_type_count == 0) {
			return false;
		}

		if (field_info.byte_size == 0) {
			return false;
		}

		if (IsStream(field_info.stream_type) && field_info.stream_byte_size == 0) {
			return false;
		}

		return true;
	}

	// One type read, doesn't go recursive
	// It returns -1 if the type has been corrupted (i.e. the stream type or basic type is out of bounds)
	// else 0 if the read_data is specified as true. Else returns the total read size
	template<bool read_data>
	size_t DeserializeFieldTableFromDataImplementation(
		uintptr_t& data,
		DeserializeFieldTable::Type* type,
		AllocatorPolymorphic allocator
	) {
		uintptr_t initial_data = data;

		unsigned short main_type_name_size = 0;
		void* main_type_name = nullptr;

		// Read the type name
		ReferenceDataWithSizeShort<true>(&data, &main_type_name, main_type_name_size);

		type->name = { main_type_name, main_type_name_size };
		// The field count
		size_t field_count = 0;
		Read<true>(&data, &field_count, sizeof(field_count));

		type->fields.size = field_count;
		type->fields.buffer = (DeserializeFieldInfo*)Allocate(allocator, sizeof(DeserializeFieldInfo) * field_count);

		// Read the fields now
		for (size_t index = 0; index < field_count; index++) {
			// The name first
			unsigned short field_name_size = 0;
			ReferenceDataWithSizeShort<true>(&data, (void**)&type->fields[index].name.buffer, field_name_size);
			type->fields[index].name.size = field_name_size;

			Read<true>(&data, &type->fields[index].stream_type, sizeof(type->fields[index].stream_type));
			Read<true>(&data, &type->fields[index].stream_byte_size, sizeof(type->fields[index].stream_byte_size));
			Read<true>(&data, &type->fields[index].basic_type, sizeof(type->fields[index].basic_type));
			Read<true>(&data, &type->fields[index].basic_type_count, sizeof(type->fields[index].basic_type_count));
			Read<true>(&data, &type->fields[index].byte_size, sizeof(type->fields[index].byte_size));
			Read<true>(&data, &type->fields[index].custom_serializer_index, sizeof(type->fields[index].custom_serializer_index));
			Read<true>(&data, &type->fields[index].user_defined_as_blittable, sizeof(type->fields[index].user_defined_as_blittable));
			Read<true>(&data, &type->fields[index].pointer_offset, sizeof(type->fields[index].pointer_offset));

			bool is_valid = ValidateDeserializeFieldInfo(type->fields[index]);
			if (!is_valid) {
				return -1;
			}

			// The definition
			if (type->fields[index].basic_type == ReflectionBasicFieldType::UserDefined || type->fields[index].basic_type == ReflectionBasicFieldType::Enum) {
				unsigned short definition_size = 0;
				ReferenceDataWithSizeShort<true>(&data, (void**)&type->fields[index].definition.buffer, definition_size);
				type->fields[index].definition.size = definition_size;
			}
			else {
				type->fields[index].definition = GetBasicFieldDefinition(type->fields[index].basic_type);
			}
		}

		if constexpr (read_data) {
			return 0;
		}
		else {
			return data - initial_data;
		}
	}

	// Goes recursive. The memory needs to be specified anyway, even if the read_data is false
	template<bool read_data>
	DeserializeFieldTable DeserializeFieldTableFromDataRecursive(
		uintptr_t& data, 
		AllocatorPolymorphic allocator
	)
	{
		DeserializeFieldTable field_table;

		size_t total_read_size = 0;

		// Allocate DESERIALIZE_FIELD_TABLE_MAX_TYPES and fail if there are more
		field_table.types.buffer = (DeserializeFieldTable::Type*)Allocate(allocator, sizeof(DeserializeFieldTable::Type) * DESERIALIZE_FIELD_TABLE_MAX_TYPES);

		unsigned int custom_serializer_count = 0;
		Read<true>(&data, &custom_serializer_count, sizeof(custom_serializer_count));

		unsigned int serializer_count = SerializeCustomTypeCount();

		// Allocate the versions of the custom serializers
		field_table.custom_serializers.buffer = (unsigned int*)Allocate(allocator, sizeof(unsigned int) * serializer_count, alignof(unsigned int));
		field_table.custom_serializers.size = serializer_count;

		struct ScopeDeallocator {
			void operator() () {
				if constexpr (!read_data) {
					// Need to deallocate these
					Deallocate(allocator, type_buffer);
					Deallocate(allocator, custom_serializer_buffer);
				}
			}

			void* type_buffer;
			void* custom_serializer_buffer;
			AllocatorPolymorphic allocator;
		};

		StackScope<ScopeDeallocator> scope_deallocator({ field_table.types.buffer, field_table.custom_serializers.buffer, allocator });
		
		ECS_STACK_CAPACITY_STREAM(char, serializer_name, 512);
		// We need to map the version now
		for (unsigned int index = 0; index < custom_serializer_count; index++) {
			// Firstly the name and then the version
			unsigned short serializer_name_size = 0;
			ReadWithSizeShort<read_data>(&data, serializer_name.buffer, serializer_name_size);
			serializer_name.size = serializer_name_size;

			unsigned int version = 0;
			Read<read_data>(&data, &version, sizeof(version));

			if constexpr (read_data) {
				unsigned int serializer_index = function::FindString(
					serializer_name.ToStream(),
					Stream<SerializeCustomType>(ECS_SERIALIZE_CUSTOM_TYPES, serializer_count),
					[](const SerializeCustomType& custom_type) {
						return custom_type.name;
					});

				if (serializer_index != -1) {
					field_table.custom_serializers[serializer_index] = version;
				}
			}
		}

		field_table.types.size = 1;
		size_t read_size = DeserializeFieldTableFromDataImplementation<read_data>(data, field_table.types.buffer, allocator);
		if (read_size == -1) {
			field_table.types.size = 0;
			return field_table;
		}

		total_read_size += read_size;

		// Now go recursive
		DeserializeFieldTable::Type* current_type = field_table.types.buffer;

		const size_t TO_BE_READ_USER_DEFINED_STORAGE_CAPACITY = 32;
		Stream<char> _to_be_read_user_defined_types_storage[TO_BE_READ_USER_DEFINED_STORAGE_CAPACITY];
		Queue<Stream<char>> to_be_read_user_defined_types(_to_be_read_user_defined_types_storage, TO_BE_READ_USER_DEFINED_STORAGE_CAPACITY);

		auto has_user_defined_fields = [&](size_t index) {
			bool is_user_defined = current_type->fields[index].basic_type == ReflectionBasicFieldType::UserDefined;
			if (is_user_defined && !current_type->fields[index].user_defined_as_blittable) {
				// Check to see if the type doesn't exist yet
				unsigned int custom_serializer_index = FindSerializeCustomType(current_type->fields[index].definition);

				if (custom_serializer_index == -1) {
					// It is a user defined type
					to_be_read_user_defined_types.Push(current_type->fields[index].definition);
				}
				else {
					// Get the list of the dependent types
					const size_t STACK_CAPACITY = 32;
					Stream<char> stack_memory[STACK_CAPACITY];

					Stack<Stream<char>> dependent_types(stack_memory, STACK_CAPACITY);
					dependent_types.Push(current_type->fields[index].definition);

					Stream<char> current_definition;
					while (dependent_types.Pop(current_definition)) {
						ECS_STACK_CAPACITY_STREAM(Stream<char>, nested_dependent_types, 32);

						custom_serializer_index = FindSerializeCustomType(current_definition);

						ReflectionCustomTypeDependentTypesData dependent_data;
						dependent_data.definition = current_definition;
						dependent_data.dependent_types = nested_dependent_types;
						ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].container_type.dependent_types(&dependent_data);

						ReflectionBasicFieldType basic_type;
						ReflectionStreamFieldType stream_type;
						for (size_t subindex = 0; subindex < dependent_data.dependent_types.size; subindex++) {
							ConvertStringToPrimitiveType(dependent_data.dependent_types[subindex], basic_type, stream_type);
							if (basic_type == ReflectionBasicFieldType::Unknown) {
								// Can be a user defined type or custom serializer again
								unsigned int nested_custom_serializer = FindSerializeCustomType(dependent_data.dependent_types[subindex]);
								if (nested_custom_serializer == -1) {
									// It is a user defined type
									to_be_read_user_defined_types.Push(dependent_data.dependent_types[subindex]);
								}
								else {
									dependent_types.Push(dependent_data.dependent_types[subindex]);
								}
							}
						}
					}
				}
			}
		};

		for (size_t index = 0; index < current_type->fields.size; index++) {
			has_user_defined_fields(index);
		}

		Stream<char> to_be_read_type_definition;
		while (to_be_read_user_defined_types.Pop(to_be_read_type_definition)) {
			current_type = field_table.types.buffer + field_table.types.size;
			// Check to see that the type was not read already - it can happen for example in struct { Stream<UserDefined>; Stream<UserDefined> }
			// and can't rule those out when walking down the fields cuz the types have not yet been read
			size_t index = 0;
			for (; index < field_table.types.size; index++) {
				if (function::CompareStrings(field_table.types[index].name, to_be_read_type_definition)) {
					break;
				}
			}

			// Only if the type was not yet read do it
			if (index == field_table.types.size) {
				size_t read_size = DeserializeFieldTableFromDataImplementation<read_data>(data, field_table.types.buffer + field_table.types.size, allocator);
				if (read_size == -1) {
					field_table.types.size = -1;
					return field_table;
				}

				total_read_size += read_size;

				field_table.types.size++;
				ECS_ASSERT(field_table.types.size <= DESERIALIZE_FIELD_TABLE_MAX_TYPES);

				for (size_t index = 0; index < current_type->fields.size; index++) {
					has_user_defined_fields(index);
				}
			}
		}

		if constexpr (!read_data) {
			field_table.types.size = total_read_size;
		}

		return field_table;
	}

	// ------------------------------------------------------------------------------------------------------------------

	DeserializeFieldTable DeserializeFieldTableFromData(uintptr_t& data, AllocatorPolymorphic allocator)
	{
		return DeserializeFieldTableFromDataRecursive<true>(data, allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t DeserializeFieldTableFromDataSize(uintptr_t data)
	{
		const size_t STACK_ALLOCATION_CAPACITY = ECS_KB * 8;

		void* stack_allocation = ECS_STACK_ALLOC(sizeof(char) * STACK_ALLOCATION_CAPACITY);
		LinearAllocator allocator(stack_allocation, STACK_ALLOCATION_CAPACITY);

		return DeserializeFieldTableFromDataRecursive<false>(data, GetAllocatorPolymorphic(&allocator)).types.size;
	}

	// ------------------------------------------------------------------------------------------------------------------

	void IgnoreDeserialize(uintptr_t& data)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);

		DeserializeFieldTable field_table;
		field_table = DeserializeFieldTableFromData(data, GetAllocatorPolymorphic(&stack_allocator));
		IgnoreDeserialize(data, field_table);

		stack_allocator.ClearBackup();
	}

	// ------------------------------------------------------------------------------------------------------------------

	void IgnoreDeserialize(
		uintptr_t& data, 
		DeserializeFieldTable field_table, 
		const ReflectionManager* deserialized_manager, 
		Stream<DeserializeTypeNameRemapping> name_remappings
	)
	{
		ReflectionManager temp_manager;
		size_t stack_allocation_size = deserialized_manager == nullptr ? ECS_KB * 32 : 0;
		void* stack_allocation = ECS_STACK_ALLOC(stack_allocation_size);
		LinearAllocator linear_allocator(stack_allocation, stack_allocation_size);
		if (deserialized_manager == nullptr) {
			deserialized_manager = &temp_manager;
			temp_manager.type_definitions.Initialize(&linear_allocator, 256);
			field_table.ToNormalReflection(&temp_manager, GetAllocatorPolymorphic(&linear_allocator));
		}

		// The type to be ignored is the first one from the field table
		IgnoreType(data, field_table, 0, deserialized_manager);
	}

#pragma region String versions

	// ------------------------------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE SerializeEx(const ReflectionManager* reflection_manager, Stream<char> string, const void* data, Stream<wchar_t> file, SerializeOptions* options)
	{
		return SerializeFileImpl(SerializeSizeEx(reflection_manager, string, data, options), file, options, [&](uintptr_t& buffer) {
			return SerializeEx(reflection_manager, string, data, buffer, options);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE SerializeEx(const ReflectionManager* reflection_manager, Stream<char> string, const void* data, uintptr_t& ptr, SerializeOptions* options)
	{
		ReflectionType reflection_type;
		if (reflection_manager->TryGetType(string, reflection_type)) {
			return Serialize(reflection_manager, &reflection_type, data, ptr, options);
		}
		else {
			// Try a custom type
			unsigned int custom_index = FindSerializeCustomType(string);
			ECS_ASSERT(custom_index != -1);

			SerializeCustomTypeWriteFunctionData write_data;
			write_data.reflection_manager = reflection_manager;
			write_data.data = data;
			write_data.definition = string;
			write_data.options = options;
			write_data.stream = &ptr;
			write_data.write_data = true;
			return ECS_SERIALIZE_CUSTOM_TYPES[custom_index].write(&write_data) == -1 ? ECS_SERIALIZE_CUSTOM_TYPE_FAILED : ECS_SERIALIZE_OK;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t SerializeSizeEx(const ReflectionManager* reflection_manager, Stream<char> string, const void* data, SerializeOptions* options)
	{
		ReflectionType reflection_type;
		if (reflection_manager->TryGetType(string, reflection_type)) {
			return SerializeSize(reflection_manager, &reflection_type, data, options);
		}
		else {
			// Try a custom type
			unsigned int custom_index = FindSerializeCustomType(string);
			ECS_ASSERT(custom_index != -1);

			SerializeCustomTypeWriteFunctionData write_data;
			write_data.reflection_manager = reflection_manager;
			write_data.data = data;
			write_data.definition = string;
			write_data.options = options;
			write_data.stream = nullptr;
			write_data.write_data = false;
			return ECS_SERIALIZE_CUSTOM_TYPES[custom_index].write(&write_data);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE DeserializeEx(
		const ReflectionManager* reflection_manager, 
		Stream<char> string, 
		void* address, 
		Stream<wchar_t> file, 
		DeserializeOptions* options, 
		void** file_data
	)
	{
		return DeserializeFileImpl(file, options, file_data, [&](uintptr_t& buffer) {
			return DeserializeEx(reflection_manager, string, address, buffer, options);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE DeserializeEx(const ReflectionManager* reflection_manager, Stream<char> string, void* address, uintptr_t& stream, DeserializeOptions* options)
	{
		ReflectionType reflection_type;
		if (reflection_manager->TryGetType(string, reflection_type)) {
			return Deserialize(reflection_manager, &reflection_type, address, stream, options);
		}
		else {
			unsigned int custom_index = FindSerializeCustomType(string);
			ECS_ASSERT(custom_index != -1);

			SerializeCustomTypeReadFunctionData read_data;
			read_data.data = address;
			read_data.definition = string;
			read_data.reflection_manager = reflection_manager;
			read_data.options = options;
			read_data.stream = &stream;
			read_data.read_data = true;
			read_data.version = ECS_SERIALIZE_CUSTOM_TYPES[custom_index].version;
			return ECS_SERIALIZE_CUSTOM_TYPES[custom_index].read(&read_data) == -1 ? ECS_DESERIALIZE_CORRUPTED_FILE : ECS_DESERIALIZE_OK;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t DeserializeSizeEx(const ReflectionManager* reflection_manager, Stream<char> string, uintptr_t& stream, DeserializeOptions* options)
	{
		Stream<char> definition = string;

		ReflectionType reflection_type;
		if (reflection_manager->TryGetType(definition, reflection_type)) {
			return DeserializeSize(reflection_manager, &reflection_type, stream, options);
		}
		else {
			unsigned int custom_index = FindSerializeCustomType(definition);
			ECS_ASSERT(custom_index != -1);

			SerializeCustomTypeReadFunctionData read_data;
			read_data.data = nullptr;
			read_data.definition = definition;
			read_data.reflection_manager = reflection_manager;
			read_data.options = options;
			read_data.stream = &stream;
			read_data.read_data = false;
			read_data.was_allocated = false;
			read_data.version = ECS_SERIALIZE_CUSTOM_TYPES[custom_index].version;
			return ECS_SERIALIZE_CUSTOM_TYPES[custom_index].read(&read_data) == -1 ? ECS_DESERIALIZE_CORRUPTED_FILE : ECS_DESERIALIZE_OK;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ------------------------------------------------------------------------------------------------------------------

	unsigned int DeserializeFieldTable::TypeIndex(Stream<char> type_name) const
	{
		for (unsigned int index = 0; index < types.size; index++) {
			if (function::CompareStrings(types[index].name, type_name)) {
				return index;
			}
		}

		return -1;
	}

	// ------------------------------------------------------------------------------------------------------------------

	unsigned int DeserializeFieldTable::FieldIndex(unsigned int type_index, Stream<char> field_name) const
	{
		for (unsigned int index = 0; index < types[type_index].fields.size; index++) {
			if (function::CompareStrings(types[type_index].fields[index].name, field_name)) {
				return index;
			}
		}

		return -1;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool DeserializeFieldTable::IsUnchanged(
		unsigned int type_index, 
		const ReflectionManager* reflection_manager, 
		const ReflectionType* type,
		Stream<DeserializeTypeNameRemapping> name_remappings
	) const
	{
		// If different field count, they are different
		if (type->fields.size != types[type_index].fields.size) {
			return false;
		}

		// Go through all fields. All recursive user defined types need to be unchanged for the whole struct to be unchanged
		for (size_t index = 0; index < type->fields.size; index++) {
			const ReflectionField* field = &type->fields[index];
			const DeserializeFieldInfo* deserialized_field = &types[type_index].fields[index];

			if (field->info.basic_type != deserialized_field->basic_type) {
				return false;
			}

			if (field->info.stream_type != deserialized_field->stream_type) {
				return false;
			}

			if (field->info.basic_type_count != deserialized_field->basic_type_count) {
				return false;
			}

			if (field->info.stream_byte_size != deserialized_field->stream_byte_size) {
				return false;
			}

			if (field->info.byte_size != deserialized_field->byte_size) {
				return false;
			}

			if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
				Stream<char> field_definition = field->definition;
				// If the user defined definition changed, then they are not equal
				if (!function::CompareStrings(deserialized_field->definition, field_definition)) {
					return false;
				}

				// Check blittable types
				size_t blittable_type_byte_size = reflection_manager->FindBlittableException(field_definition).x;
				if (blittable_type_byte_size != -1) {
					// It is blittable now and has same byte size
					if (!deserialized_field->user_defined_as_blittable || deserialized_field->byte_size != blittable_type_byte_size) {
						// Now it not anymore blittable - fail
						return false;
					}
				}
				else {
					// If not a custom serializer, check here in the field table that the type is unchanged
					unsigned int serializer_index = types[type_index].fields[index].custom_serializer_index;
					if (serializer_index != -1) {
						serializer_index = FindSerializeCustomType(field_definition);
						ECS_ASSERT(serializer_index != -1);

						// Check that both versions are the same
						if (custom_serializers[serializer_index] != ECS_SERIALIZE_CUSTOM_TYPES[serializer_index].version) {
							return false;
						}
					}
					else {
						// Check user defined type
						unsigned int nested_type_index = TypeIndex(field_definition);
						// The nested type should be found
						ECS_ASSERT(nested_type_index != -1);

						const ReflectionType* nested_type = reflection_manager->GetType(field_definition);
						if (!IsUnchanged(nested_type_index, reflection_manager, nested_type, name_remappings)) {
							return false;
						}
					}
				}
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------

	void DeserializeFieldTable::ToNormalReflection(
		ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator, 
		bool allocate_all,
		bool check_for_insertion,
		bool calculate_parameters
	) const
	{
		// Commit all types and then calculate the byte size and the alignment for them
		// + is_trivially copyable

		auto add_type = [=](size_t index) {
			ReflectionType type;
			type.name = types[index].name;
			if (allocate_all) {
				type.name = function::StringCopy(allocator, type.name);
			}
			type.fields.Initialize(allocator, types[index].fields.size);
			type.tag = { nullptr, 0 };
			type.evaluations = { nullptr, 0 };
			for (size_t field_index = 0; field_index < types[index].fields.size; field_index++) {
				type.fields[field_index].name = types[index].fields[field_index].name;
				type.fields[field_index].definition = types[index].fields[field_index].definition;
				type.fields[field_index].info.basic_type = types[index].fields[field_index].basic_type;
				type.fields[field_index].info.basic_type_count = types[index].fields[field_index].basic_type_count;
				type.fields[field_index].info.byte_size = types[index].fields[field_index].byte_size;
				type.fields[field_index].info.stream_type = types[index].fields[field_index].stream_type;
				type.fields[field_index].info.stream_byte_size = types[index].fields[field_index].stream_byte_size;
				type.fields[field_index].info.pointer_offset = types[index].fields[field_index].pointer_offset;
				type.fields[field_index].tag = { nullptr, 0 };

				if (allocate_all) {
					type.fields[field_index].name = function::StringCopy(allocator, type.fields[field_index].name);
					type.fields[field_index].definition = function::StringCopy(allocator, type.fields[field_index].definition);
				}
			}

			// Just copy it
			reflection_manager->AddType(&type);
		};

		for (size_t index = 0; index < types.size; index++) {
			if (check_for_insertion) {
				if (reflection_manager->type_definitions.Find(types[index].name) == -1) {
					add_type(index);
				}
			}
			else {
				add_type(index);
			}
		}

		if (calculate_parameters) {
			for (size_t index = 0; index < types.size; index++) {
				Reflection::ReflectionType* ptr = reflection_manager->type_definitions.GetValuePtr(types[index].name);
				ptr->byte_size = CalculateReflectionTypeByteSize(reflection_manager, ptr);
				ptr->alignment = CalculateReflectionTypeAlignment(reflection_manager, ptr);
				ptr->is_blittable = SearchIsBlittable(reflection_manager, ptr);
				ptr->is_blittable_with_pointer = SearchIsBlittableWithPointer(reflection_manager, ptr);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	void DeserializeFieldTable::ExtractTypesFromReflection(
		Reflection::ReflectionManager* reflection_manager, 
		CapacityStream<Reflection::ReflectionType*> reflection_types
	) const
	{
		for (size_t index = 0; index < types.size; index++) {
			reflection_types.AddAssert(reflection_manager->type_definitions.GetValuePtr(types[index].name));
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic DeserializeOptions::GetFieldAllocator(ReflectionStreamFieldType field_type, const void* data) const
	{
		if (field_type == ReflectionStreamFieldType::ResizableStream && use_resizable_stream_allocator) {
			return ((ResizableStream<void>*)data)->allocator;
		}
		return field_allocator;
	}

	// ------------------------------------------------------------------------------------------------------------------

}