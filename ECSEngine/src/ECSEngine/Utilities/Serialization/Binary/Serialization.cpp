#include "ecspch.h"
#include "Serialization.h"
#include "../../Reflection/Reflection.h"
#include "../../Reflection/ReflectionStringFunctions.h"
#include "../../Reflection/ReflectionCustomTypes.h"
#include "../../Reflection/ReflectionAllocatorHandling.h"
#include "../SerializationHelpers.h"
#include "../../../Containers/Stacks.h"
#include "../../../Allocators/ResizableLinearAllocator.h"
#include "../../../Math/MathHelpers.h"
#include "SerializationMacro.h"
#include "../../ReaderWriterInterface.h"
#include "../../BufferedFileReaderWriter.h"

#define DESERIALIZE_FIELD_TABLE_MAX_TYPES (32)

#define SERIALIZE_FIELD_TABLE_VERSION 0
#define REFLECTION_MANAGER_SERIALIZE_VERSION 0

namespace ECSEngine {

	using namespace Reflection;

	// ------------------------------------------------------------------------------------------------------------------

	bool SerializeShouldOmitField(Stream<char> type, Stream<char> name, Stream<SerializeOmitField> omit_fields)
	{
		for (size_t index = 0; index < omit_fields.size; index++) {
			if (omit_fields[index].type == type && omit_fields[index].name == name) {
				return true;
			}
		}

		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool AssetSerializeOmitFieldsExist(const ReflectionManager* reflection_manager, Stream<SerializeOmitField> omit_fields)
	{
		for (size_t index = 0; index < omit_fields.size; index++) {
			const ReflectionType* type = reflection_manager->TryGetType(omit_fields[index].type);
			if (type != nullptr) {
				size_t subindex = 0;
				for (; subindex < type->fields.size; subindex++) {
					if (type->fields[subindex].name == omit_fields[index].name) {
						break;
					}
				}

				if (subindex == type->fields.size) {
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
			unsigned int subindex = FindString(type->fields[index].name, fields_to_keep);
			if (subindex == -1) {
				omit_fields.Add({ type->name, type->fields[index].name });
			}
		}

		omit_fields.AssertCapacity();
	}

	// ------------------------------------------------------------------------------------------------------------------

	// It will write only the fields of this type, the user defined types are only specified by name
	// The custom fields should be filled in with the fields which have a custom serializer
	// The reflection manager is needed for blittable exceptions. The write instrument is a template parameter
	// Such that we force the compiler to not use virtual calls for the common case of a uintptr_t instrument.
	// Returns true if it succeeded, else false.
	static bool WriteTypeTable(
		const ReflectionManager* reflection_manager, 
		const ReflectionType* type,
		WriteInstrument* write_instrument,
		Stream<SerializeOmitField> omit_fields,
		bool write_tags
	) {
		// Write the name of the type first
		if (!write_instrument->WriteWithSizeVariableLength(type->name)) {
			return false;
		}

		// Write the byte size, alignment, and is_blittable booleans here
		ECS_ASSERT(type->byte_size < UINT_MAX);
		ECS_ASSERT(type->alignment < UINT_MAX);
		unsigned int byte_size = type->byte_size;
		unsigned int alignment = type->alignment;
		bool write_success = write_instrument->Write(&byte_size);
		write_success &= write_instrument->Write(&alignment);
		write_success &= write_instrument->Write(&type->is_blittable);
		write_success &= write_instrument->Write(&type->is_blittable_with_pointer);
		if (!write_success) {
			return false;
		}

		if (write_tags) {
			if (!write_instrument->WriteWithSizeVariableLength(type->tag)) {
				return false;
			}
		}

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
		if (!write_instrument->Write(&serializable_field_count)) {
			return false;
		}
		for (size_t index = 0; index < type->fields.size; index++) {
			bool skip_serializable = type->fields[index].Has(STRING(ECS_SERIALIZATION_OMIT_FIELD));

			if (!skip_serializable) {
				// Gather multiple writes into the same success, such that we don't check on every single one of them
				write_success = true;

				if (omit_fields.size > 0) {
					skip_serializable = SerializeShouldOmitField(type->name, type->fields[index].name, omit_fields);
				}
				
				if (!skip_serializable) {
					const ReflectionField* field = &type->fields[index];

					// Write the field name
					write_success &= write_instrument->WriteWithSizeVariableLength(field->name);

					if (write_tags) {
						write_success &= write_instrument->WriteWithSizeVariableLength(field->tag);
					}

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

					DeserializeFieldInfoFlags flags;
					flags.user_defined_as_blittable = blittable_user_defined;
					write_success &= write_instrument->Write(&field->info.stream_type);
					write_success &= write_instrument->Write(&field->info.stream_byte_size);
					write_success &= write_instrument->Write(&field->info.stream_alignment);
					write_success &= write_instrument->Write(&field->info.basic_type);
					write_success &= write_instrument->Write(&field->info.basic_type_count);
					write_success &= write_instrument->Write(&field->info.byte_size);
					write_success &= write_instrument->Write(&custom_serializer_index);
					write_success &= write_instrument->Write(&flags);
					write_success &= write_instrument->Write(&field->info.pointer_offset);

					// If user defined, write the definition as well
					if (field->info.basic_type == ReflectionBasicFieldType::UserDefined || field->info.basic_type == ReflectionBasicFieldType::Enum) {
						Stream<char> definition_stream = field->definition;
						write_success &= write_instrument->WriteWithSizeVariableLength(definition_stream);
					}

					if (!write_success) {
						return false;
					}
				}
			}
		}

		return true;
	}

	// This function will write the current type, plus all the recursive type dependencies that it has
	static bool WriteTypeTable(
		const ReflectionManager* reflection_manager, 
		const ReflectionType* type, 
		WriteInstrument* write_instrument,
		CapacityStream<Stream<char>>& deserialized_type_names,
		Stream<SerializeOmitField> omit_fields,
		bool write_tags
	) {
		// Add it to the written types
		deserialized_type_names.Add(type->name);

		// Write the top most type first
		if (!WriteTypeTable(reflection_manager, type, write_instrument, omit_fields, write_tags)) {
			return false;
		}

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
						bool is_missing = FindString(field->definition, deserialized_type_names) == -1;
						if (is_missing) {
							// Can be a custom serializer type
							if (reflection_manager->TryGetType(field->definition) != nullptr) {
								custom_dependent_types.Push(field->definition);
							}
							else {
								unsigned int custom_serializer_index = FindSerializeCustomType(field->definition);
								if (custom_serializer_index != -1) {
									ECS_STACK_CAPACITY_STREAM(Stream<char>, current_custom_dependent_types, DESERIALIZE_FIELD_TABLE_MAX_TYPES / 2);

									ReflectionCustomTypeDependenciesData dependent_data;
									dependent_data.definition = field->definition;
									dependent_data.dependencies = current_custom_dependent_types;

									ECS_REFLECTION_CUSTOM_TYPES[custom_serializer_index]->GetDependencies(&dependent_data);
									for (size_t subindex = 0; subindex < dependent_data.dependencies.size; subindex++) {
										custom_dependent_types.Push(dependent_data.dependencies[subindex]);
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
			const ReflectionType* nested_type = reflection_manager->TryGetType(current_dependent_type);
			if (nested_type != nullptr) {
				bool is_missing = FindString(current_dependent_type, deserialized_type_names) == -1;
				if (is_missing) {
					if (!WriteTypeTable(reflection_manager, nested_type, write_instrument, deserialized_type_names, omit_fields, write_tags)) {
						return false;
					}
				}
			}
			else {
				// Might be a nested custom serializer
				unsigned int nested_custom_serializer = FindSerializeCustomType(current_dependent_type);
				if (nested_custom_serializer != -1) {
					ECS_STACK_CAPACITY_STREAM(Stream<char>, current_custom_dependent_types, DESERIALIZE_FIELD_TABLE_MAX_TYPES / 2);

					ReflectionCustomTypeDependenciesData dependent_data;
					dependent_data.definition = current_dependent_type;
					dependent_data.dependencies = current_custom_dependent_types;

					ECS_REFLECTION_CUSTOM_TYPES[nested_custom_serializer]->GetDependencies(&dependent_data);
					for (size_t index = 0; index < dependent_data.dependencies.size; index++) {
						custom_dependent_types.Push(dependent_data.dependencies[index]);
					}
				}
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------

	// Writes the necessary information about custom type interfaces, such that the user can version them
	static bool WriteTypeTableCustomInterfacesInfo(WriteInstrument* write_instrument) {
		bool write_success = true;

		// Write the version first
		unsigned int serialize_version = SERIALIZE_FIELD_TABLE_VERSION;
		write_success &= write_instrument->Write(&serialize_version);

		// Write the custom serializer versions
		unsigned int serializers_count = SerializeCustomTypeCount();
		write_success &= write_instrument->Write(&serializers_count);

		for (size_t index = 0; index < serializers_count; index++) {
			write_success &= write_instrument->WriteWithSizeVariableLength(ECS_SERIALIZE_CUSTOM_TYPES[index].name);
			write_success &= write_instrument->Write(&ECS_SERIALIZE_CUSTOM_TYPES[index].version);
		}

		return write_success;
	}

	// ------------------------------------------------------------------------------------------------------------------

	// Combines the custom type interfaces info with the recursive type write
	static bool WriteTypeTableWithSerializers(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		WriteInstrument* write_instrument,
		Stream<SerializeOmitField> omit_fields,
		bool write_tags
	) {
		bool write_success = true;

		write_success &= WriteTypeTableCustomInterfacesInfo(write_instrument);

		ECS_STACK_CAPACITY_STREAM(Stream<char>, deserialized_type_names, DESERIALIZE_FIELD_TABLE_MAX_TYPES);
		write_success &= WriteTypeTable(reflection_manager, type, write_instrument, deserialized_type_names, omit_fields, write_tags);

		return write_success;
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
			ReflectionCustomTypeDependenciesData dependent_data;
			dependent_data.definition = definition;
			dependent_data.dependencies = dependent_types;
			ECS_REFLECTION_CUSTOM_TYPES[custom_serializer]->GetDependencies(&dependent_data);

			for (size_t subindex = 0; subindex < dependent_data.dependencies.size; subindex++) {
				ReflectionBasicFieldType basic_field_type;
				ReflectionStreamFieldType stream_field_type;

				ConvertStringToPrimitiveType(dependent_data.dependencies[subindex], basic_field_type, stream_field_type);
				// It is not a basic type - should look for it as a dependency
				if (basic_field_type == ReflectionBasicFieldType::Unknown) {
					const ReflectionType* nested_type = reflection_manager->TryGetType(dependent_data.dependencies[subindex].buffer);
					// It is a reflected type - make sure its dependencies are met
					if (nested_type != nullptr) {
						if (!SerializeHasDependentTypes(reflection_manager, nested_type, omit_fields)) {
							custom_serializer_success = false;
							return true;
						}
					}
					else {
						// Might be a custom serializer type again
						SerializeHasDependentTypesCustomSerializerRecurse(reflection_manager, dependent_data.dependencies[subindex], custom_serializer_success, omit_fields);
						if (!custom_serializer_success) {
							return true;
						}
					}
				}
				// If it is a basic type, then these should not count as dependencies
			}

			return true;
		}
		// Handle the void case separately
		else if (definition == STRING(void)) {
			return true;
		}
		else {
			// Pointers can fall into this category
			Stream<char> no_pointer_definition = definition;
			while (no_pointer_definition.Last() == '*') {
				no_pointer_definition.size--;
			}
			if (no_pointer_definition.size != definition.size) {
				// Return the dependent types of the target of the pointee
				return SerializeHasDependentTypesCustomSerializerRecurse(reflection_manager, no_pointer_definition, custom_serializer_success, omit_fields);
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool SerializeHasDependentTypes(const Reflection::ReflectionManager* reflection_manager, const Reflection::ReflectionType* type, Stream<SerializeOmitField> omit_fields)
	{
		bool custom_serializer_success = true;

		for (size_t index = 0; index < type->fields.size; index++) {
			bool skip_serializable = type->fields[index].Has(STRING(ECS_SERIALIZATION_OMIT_FIELD));
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
					const ReflectionType* nested_type = reflection_manager->TryGetType(type->fields[index].definition);
					if (nested_type != nullptr) {
						if (!SerializeHasDependentTypes(reflection_manager, nested_type, omit_fields)) {
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
		WriteInstrument* write_instrument,
		SerializeOptions* options
	) {
		bool has_options = options != nullptr;

		bool write_tags = false;
		Stream<SerializeOmitField> omit_fields = Stream<SerializeOmitField>(nullptr, 0);
		if (has_options) {
			write_tags = options->write_type_table_tags;
			omit_fields = options->omit_fields;
		}

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
			if (!write_instrument->WriteWithSize<size_t>(options->header)) {
				return ECS_SERIALIZE_INSTRUMENT_FAILURE;
			}
		}

		if (!has_options || (has_options && options->write_type_table)) {
			// The type table must be written
			if (!WriteTypeTableWithSerializers(reflection_manager, type, write_instrument, omit_fields, write_tags)) {
				return ECS_SERIALIZE_INSTRUMENT_FAILURE;
			}
		}

		SerializeOptions _nested_options;
		SerializeOptions* nested_options = &_nested_options;
		if (has_options) {
			nested_options = &_nested_options;
			*nested_options = *options;
			nested_options->verify_dependent_types = false;
			nested_options->write_type_table = false;
			nested_options->header = {};
			nested_options->omit_fields = omit_fields;
		}

		// Use a small stack size in order to not blow the stack limit when nesting deep
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(passdown_allocator, ECS_KB * 4, ECS_MB * 16);
		ReflectionPassdownInfo passdown_info(&passdown_allocator);
		if (nested_options->passdown_info == nullptr) {
			nested_options->passdown_info = &passdown_info;
		}

		bool is_not_size_determination = !write_instrument->IsSizeDetermination();

		// Before writing the normal type fields, proceed by writing all the standalone type allocators
		// Such that they can be reconstructed before all the other fields
		for (size_t index = 0; index < type->misc_info.size; index++) {
			if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR) {
				// If it is a reference allocator, don't write its contents
				const ReflectionTypeMiscAllocator& allocator_field = type->misc_info[index].allocator_info;
				if (!HasFlag(allocator_field.modifier, ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER_REFERENCE)) {
					const ReflectionField& field = type->fields[allocator_field.field_index];

					SerializeCustomTypeWriteFunctionData allocator_write_data;
					// The function GetReflectionTypeAllocatorPointerAndDefinition requires a void* out parameter, use a stack variable for this
					void* write_data_pointer = nullptr;
					GetReflectionTypeAllocatorPointerAndDefinition(type, allocator_field.field_index, data, write_data_pointer, allocator_write_data.definition);

					allocator_write_data.data = write_data_pointer;
					allocator_write_data.tags = field.tag;
					allocator_write_data.options = nested_options;
					allocator_write_data.reflection_manager = reflection_manager;
					allocator_write_data.write_instrument = write_instrument;

					if (!ECS_SERIALIZE_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_ALLOCATOR].write(&allocator_write_data)) {
						return ECS_SERIALIZE_INSTRUMENT_FAILURE;
					}
				}
			}
		}

		for (size_t index = 0; index < type->fields.size; index++) {
			bool success = true;
			const ReflectionField* field = &type->fields[index];

			bool skip_serializable = field->Has(STRING(ECS_SERIALIZATION_OMIT_FIELD));
			// Consider the field skipped if it an allocator as well
			skip_serializable |= IsReflectionTypeFieldAllocatorFromMiscDirectOnly(type, index);

			if (!skip_serializable && omit_fields.size > 0) {
				skip_serializable = SerializeShouldOmitField(type->name, type->fields[index].name, omit_fields);
			}

			if (!skip_serializable) {
				// If a basic type, copy the data directly
				const void* field_data = OffsetPointer(data, field->info.pointer_offset);
				ReflectionStreamFieldType stream_type = field->info.stream_type;
				ReflectionBasicFieldType basic_type = field->info.basic_type;

				// If the field is a pointer reference key, add it now
				nested_options->passdown_info->AddPointerReferencesFromField(&type->fields[index], field_data, field_data);

				bool is_user_defined = basic_type == ReflectionBasicFieldType::UserDefined;
				const ReflectionType* nested_type = nullptr;
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
						success &= write_instrument->Write(field_data, blittable_size.x * field->info.basic_type_count);
						if (!success) {
							return ECS_SERIALIZE_INSTRUMENT_FAILURE;
						}
						continue;
					}

					nested_type = reflection_manager->TryGetType(field->definition);
					if (nested_type == nullptr) {
						custom_serializer_index = FindSerializeCustomType(definition_stream);
					}
				}

				if (custom_serializer_index != -1) {
					SerializeCustomTypeWriteFunctionData custom_write_data;
					custom_write_data.data = field_data;
					custom_write_data.definition = definition_stream;
					custom_write_data.options = nested_options;
					custom_write_data.reflection_manager = reflection_manager;
					custom_write_data.write_instrument = write_instrument;
					custom_write_data.tags = field->tag;

					success &= ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].write(&custom_write_data);
				}
				else {
					if (stream_type == ReflectionStreamFieldType::Basic && !is_user_defined) {
						success &= write_instrument->Write(field_data, field->info.byte_size);
					}
					else {
						// User defined, call the serialization for it
						if (stream_type == ReflectionStreamFieldType::Basic && is_user_defined) {
							// No need to test the return code since it cannot fail if it gets to here
							ECS_SERIALIZE_CODE serialize_code = Serialize(reflection_manager, nested_type, field_data, write_instrument, nested_options);
							if (serialize_code != ECS_SERIALIZE_OK) {
								return serialize_code;
							}
						}
						// Determine if it is a pointer, basic array or stream
						// If pointer, only do it for 1 level of indirection and ASCII or wide char strings
						else if (stream_type == ReflectionStreamFieldType::Pointer) {
							if (GetReflectionFieldPointerIndirection(type->fields[index].info) == 1) {
								// Determine if this is a reference pointer. If it is, handle it separately
								Stream<char> pointer_reference;
								Stream<char> pointer_reference_custom_element;
								if (GetReflectionPointerAsReferenceParams(type->fields[index].tag, pointer_reference, pointer_reference_custom_element)) {
									// If it is missing the pointer reference argument, skip it
									if (pointer_reference.size > 0) {
										size_t token = 0;
										if (is_not_size_determination) {
											token = nested_options->passdown_info->GetPointerTargetToken(reflection_manager, pointer_reference, pointer_reference_custom_element, *(void**)field_data, true);
											ECS_ASSERT(token != -1);
										}
										success &= write_instrument->Write(&token);
									}
								}
								else {
									// Treat user defined pointers as pointers to a single entity
									if (is_user_defined) {
										// No need to test the return code since it cannot fail if it gets to here
										ECS_SERIALIZE_CODE serialize_code = Serialize(reflection_manager, nested_type, *(void**)field_data, write_instrument, nested_options);
										if (serialize_code != ECS_SERIALIZE_OK) {
											return serialize_code;
										}
									}
									else {
										success &= WriteFundamentalType(type->fields[index].info, field_data, write_instrument);
									}
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

								for (size_t subindex = 0; subindex < basic_count; subindex++) {
									// No need to test the return code since if it gets to here it cannot fail
									ECS_SERIALIZE_CODE serialize_code = Serialize(reflection_manager, nested_type, OffsetPointer(field_data, subindex * element_byte_size), write_instrument, nested_options);
									if (serialize_code != ECS_SERIALIZE_OK) {
										return serialize_code;
									}
								}
							}
							else {
								// Write the whole data at once
								success &= write_instrument->Write(field_data, field->info.byte_size);
							}
						}
						// Streams of user defined types should be handled by the custom serializer					
						else {
							// Check for streams. All, Stream, CapacityStream, ResizableStream and PointerSoA can be aliased with a normal stream
							// Since we are interested in writing only the data along side the byte size
							Stream<void> field_stream = GetReflectionFieldStreamVoid(field->info, data);
							if (is_user_defined) {
								size_t type_byte_size = GetReflectionTypeByteSize(nested_type);

								// Write the size first and then the user defined
								success &= write_instrument->Write(&field_stream.size);
								for (size_t subindex = 0; subindex < field_stream.size; subindex++) {
									ECS_SERIALIZE_CODE serialize_code = Serialize(
										reflection_manager,
										nested_type,
										OffsetPointer(field_data, subindex * type_byte_size),
										write_instrument,
										nested_options
									);
									if (serialize_code != ECS_SERIALIZE_OK) {
										return serialize_code;
									}
								}
							}
							else {
								field_stream.size *= GetReflectionFieldStreamElementByteSize(field->info);
								success &= write_instrument->WriteWithSizeVariableLength(field_stream);
							}
						}
					}
				}
			}
		}

		return ECS_SERIALIZE_OK;
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE Serialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		const void* data,
		Stream<wchar_t> file,
		SerializeOptions* options
	) {
		ECS_STACK_VOID_STREAM(file_buffering, ECS_KB * 64);
		ECS_SERIALIZE_CODE result;
		OwningBufferedFileWriteInstrument::WriteTo(file, file_buffering, [&](WriteInstrument* write_instrument) {
			result = Serialize(reflection_manager, type, data, write_instrument, options);
			return result == ECS_SERIALIZE_OK;
		});
		return result;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool SerializeFieldTable(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		WriteInstrument* write_instrument,
		Stream<SerializeOmitField> omit_fields,
		bool write_tags
	)
	{
		return WriteTypeTableWithSerializers(reflection_manager, type, write_instrument, omit_fields, write_tags);
	}

	// ------------------------------------------------------------------------------------------------------------------

	// Forward declaration
	static bool IgnoreType(
		ReadInstrument* read_instrument,
		const DeserializeFieldTable& deserialize_table,
		unsigned int type_index,
		const ReflectionManager* deserialized_manager,
		unsigned int count = 1
	);

	// Function to skip only a certain field of the type
	static bool IgnoreTypeField(
		ReadInstrument* read_instrument,
		const DeserializeFieldTable& deserialize_table,
		unsigned int type_index,
		unsigned int field_index,
		const ReflectionManager* deserialized_manager
	) {
		bool success = true;

		const DeserializeFieldInfo* info = &deserialize_table.types[type_index].fields[field_index];
		// If the field is not user defined, can handle it now
		if (info->basic_type != ReflectionBasicFieldType::UserDefined) {
			if (info->stream_type == ReflectionStreamFieldType::Basic || info->stream_type == ReflectionStreamFieldType::BasicTypeArray) {
				success &= read_instrument->Ignore(info->byte_size);
			}
			else if (info->stream_type == ReflectionStreamFieldType::Pointer) {
				// Check to see if this pointer is a reference or not. If it is, treat it differently
				Stream<char> pointer_reference_key;
				Stream<char> pointer_reference_custom_element;
				if (GetReflectionPointerAsReferenceParams(info->tag, pointer_reference_key, pointer_reference_custom_element)) {
					// If the tag is empty, nothing was written
					if (pointer_reference_key.size > 0) {
						success &= read_instrument->Ignore(sizeof(ReflectionCustomTypeGetElementIndexOrToken));
					}
				}
				else {
					if (info->basic_type == ReflectionBasicFieldType::Int8) {
						success &= read_instrument->IgnoreWithSizeVariableLength<char>();
					}
					else if (info->basic_type == ReflectionBasicFieldType::Wchar_t) {
						success &= read_instrument->IgnoreWithSizeVariableLength<wchar_t>();
					}
					else {
						ECS_ASSERT(info->basic_type != ReflectionBasicFieldType::UserDefined, "Unhandled deserialize ignore case!");
						size_t byte_size = GetReflectionBasicFieldTypeByteSize(info->basic_type);
						success &= read_instrument->Ignore(byte_size);
					}
				}
			}
			else {
				// All the other stream types or pointerSoA can be aliased
				success &= read_instrument->IgnoreWithSizeVariableLength<void>();
			}
		}
		else {
			// Verify blittable exception
			if (info->flags.user_defined_as_blittable) {
				success &= read_instrument->Ignore(info->byte_size);
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
					options.deserialized_field_manager = deserialized_manager;

					// Use the read with the read data set to false.
					SerializeCustomTypeReadFunctionData read_data;
					read_data.data = nullptr;
					read_data.definition = field_definition;
					read_data.options = &options;
					read_data.was_allocated = false;
					read_data.reflection_manager = deserialized_manager;
					read_data.read_instrument = read_instrument;
					// Write the custom versions for all types
					deserialize_table.custom_serializers.CopyTo(read_data.custom_types_version);
					read_data.tags = info->tag;
					read_data.ignore_data = true;

					success &= ECS_SERIALIZE_CUSTOM_TYPES[custom_serializer_index].read(&read_data);
				}
				else {
					unsigned int nested_type = deserialize_table.TypeIndex(field_definition);
					ECS_ASSERT(nested_type != -1);

					if (info->stream_type == ReflectionStreamFieldType::Basic) {
						success &= IgnoreType(read_instrument, deserialize_table, nested_type, deserialized_manager);
					}
					// Indirection 1
					else if (info->stream_type == ReflectionStreamFieldType::Pointer) {
						if (info->basic_type_count == 1) {
							// Check to see if this pointer is a reference or not. If it is, treat it differently
							Stream<char> pointer_reference_key;
							Stream<char> pointer_reference_custom_element;
							if (GetReflectionPointerAsReferenceParams(info->tag, pointer_reference_key, pointer_reference_custom_element)) {
								// If the tag is empty, nothing was written
								if (pointer_reference_key.size > 0) {
									success &= read_instrument->Ignore(sizeof(ReflectionCustomTypeGetElementIndexOrToken));
								}
							}
							else {
								success &= IgnoreType(read_instrument, deserialize_table, nested_type, deserialized_manager);
							}
						}
						else {
							ECS_ASSERT(false, "Pointer Indirection greater than 1!");
						}
					}
					else if (info->stream_type == ReflectionStreamFieldType::BasicTypeArray) {
						success &= IgnoreType(read_instrument, deserialize_table, nested_type, deserialized_manager, info->basic_type_count);
					}
					// The stream types or PointerSoA
					else {
						size_t stream_count = 0;
						success &= read_instrument->ReadAlways(&stream_count);
						success &= IgnoreType(read_instrument, deserialize_table, nested_type, deserialized_manager, stream_count);
					}
				}
			}
		}

		return success;
	}

	// Function to skip a type, or a number of type instances
	static bool IgnoreType(
		ReadInstrument* read_instrument,
		const DeserializeFieldTable& deserialize_table,
		unsigned int type_index,
		const ReflectionManager* deserialized_manager,
		unsigned int count
	) {
		bool success = true;
		for (unsigned int counter = 0; counter < count; counter++) {
			for (unsigned int index = 0; index < deserialize_table.types[type_index].fields.size; index++) {
				success &= IgnoreTypeField(read_instrument, deserialize_table, type_index, index, deserialized_manager);
			}

			// Early exit if after an instance we failed to ignore it
			if (!success) {
				return success;
			}
		}

		return success;
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE Deserialize(
		const ReflectionManager* reflection_manager,
		const ReflectionType* type,
		void* address,
		ReadInstrument* read_instrument,
		DeserializeOptions* options
	)
	{
		bool has_options = options != nullptr;

		Stream<SerializeOmitField> omit_fields = has_options ? options->omit_fields : Stream<SerializeOmitField>(nullptr, 0);

		Stream<char> type_name = type->name;
		CapacityStream<char>* error_message = has_options ? options->error_message : nullptr;

		if (!has_options || (has_options && options->verify_dependent_types)) {
			// Verify that it has its dependencies met
			bool has_dependencies = SerializeHasDependentTypes(reflection_manager, type, omit_fields);
			if (!has_dependencies) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not deserialize type \"{#}\" because it doesn't have its dependencies met.", type_name);
				return ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES;
			}
		}

		if (has_options && options->header.size > 0 && options->header.buffer != nullptr) {
			// Read the header
			if (!read_instrument->ReadWithSize<size_t>(options->header.buffer, options->header.size, options->header.size)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not deserialize type \"{#}\" because the header could not be read.", type_name);
				return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
			}

			if (options->validate_header != nullptr) {
				bool is_valid = options->validate_header(options->header, options->validate_header_data);
				if (!is_valid) {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not deserialize type \"{#}\" because the header is not valid.", type_name);
					return ECS_DESERIALIZE_INVALID_HEADER;
				}
			}
		}

		// The type table now
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(table_linear_allocator, ECS_KB * 8, ECS_MB * 32);
		DeserializeFieldTable deserialize_table;

		bool fail_if_mismatch = has_options && options->fail_if_field_mismatch;
		bool use_resizable_stream_allocator = has_options && options->use_resizable_stream_allocator;
		bool default_initialize_missing_fields = has_options && options->default_initialize_missing_fields;
		bool initialize_type_allocators = has_options && options->initialize_type_allocators;
		bool use_field_allocators = has_options && options->use_type_field_allocators;
		bool is_not_size_determination = !read_instrument->IsSizeDetermination();

		//bool use_deserialize_table = !has_options || (options->field_table != nullptr);
		// At the moment allow only table deserialization
		bool use_deserialize_table = true;
		unsigned int type_index = 0;
		size_t field_count = type->fields.size;

		auto read_type_table_from_file = [&]() {
			DeserializeFieldTableOptions field_table_options;
			field_table_options.read_type_tags = has_options && options->read_type_table_tags;
			field_table_options.version = -1;

			deserialize_table = DeserializeFieldTableFromData(read_instrument, &table_linear_allocator, &field_table_options);
			// Check to see if the table is valid
			if (deserialize_table.types.size == 0) {
				// The file was corrupted
				ECS_FORMAT_ERROR_MESSAGE(error_message, "The field table has been corrupted when trying to deserialize type \"{#}\"."
					" The deserialization cannot continue.", type_name);
				return ECS_DESERIALIZE_CORRUPTED_FILE;
			}
			unsigned int type_index = deserialize_table.TypeIndex(type_name);
			ECS_ASSERT(type_index != -1);
			field_count = deserialize_table.types[type_index].fields.size;
			return ECS_DESERIALIZE_OK;
		};

		AllocatorPolymorphic initial_field_allocator = ECS_MALLOC_ALLOCATOR;

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

			initial_field_allocator = options->field_allocator;
		}
		else {
			ECS_DESERIALIZE_CODE code = read_type_table_from_file();
			if (code != ECS_DESERIALIZE_OK) {
				return code;
			}
		}

		DeserializeOptions _nested_options;
		if (has_options) {
			_nested_options = *options;
		}

		DeserializeOptions* nested_options = &_nested_options;
		nested_options->field_table = &deserialize_table;
		nested_options->version = -1;
		nested_options->read_type_table = false;
		nested_options->verify_dependent_types = false;
		nested_options->validate_header = false;
		nested_options->header = {};
		nested_options->validate_header_data = nullptr;

		// If the deserialized passdown info was not created previously, we must create it.
		// Use a small stack allocation in order to not blow through the stack limit
		// If we recurse a lot
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(passdown_allocator, ECS_KB * 4, ECS_MB * 16);
		ReflectionPassdownInfo passdown_info(&passdown_allocator);
		if (nested_options->passdown_info == nullptr) {
			nested_options->passdown_info = &passdown_info;
		}

		// If the field allocators are enabled, check the main type allocator and override the initial field allocator
		if (use_field_allocators) {
			initial_field_allocator = GetReflectionTypeOverallAllocator(type, address, initial_field_allocator);
		}

		// Returns true if it succeeded, else false
		auto deserialize_incompatible_basic = [read_instrument](void* field_data, const DeserializeFieldInfo& file_info, const ReflectionFieldInfo& type_info) -> bool {
			// Read the data from file into a double and then convert the double into the appropriate type
			unsigned int file_basic_component_count = BasicTypeComponentCount(file_info.basic_type);
			unsigned int type_basic_component_count = BasicTypeComponentCount(type_info.basic_type);
			unsigned int iteration_count = ClampMin(file_basic_component_count, type_basic_component_count);
			unsigned int per_component_byte_size = file_info.byte_size / file_basic_component_count;

			double file_data[4];

			if (!read_instrument->ReadAlways(file_data, per_component_byte_size * iteration_count)) {
				return false;
			}
			
			unsigned int pointer_increment = type_info.byte_size / type_basic_component_count;
			for (unsigned int index = 0; index < iteration_count; index++) {
				void* type_data = OffsetPointer(field_data, (size_t)pointer_increment * (size_t)index);
				ConvertReflectionBasicField(file_info.basic_type, type_info.basic_type, file_data, type_data);
			}
			return true;
		};

		auto deserialize_with_table = [&]() {
			size_t temp_allocator_size = (has_options && options->deserialized_field_manager != nullptr) ? 0 : ECS_KB * 16;
			void* temp_allocator_buffer = ECS_STACK_ALLOC(temp_allocator_size);
			LinearAllocator temp_allocator(temp_allocator_buffer, temp_allocator_size);

			ReflectionManager _temporary_manager;
			const ReflectionManager* deserialized_manager = has_options ? options->deserialized_field_manager : nullptr;
			if (deserialized_manager == nullptr) {
				// Create a temporary deserialized manager that can be used with the ignore calls
				_temporary_manager.type_definitions.Initialize(&temp_allocator, HashTableCapacityForElements(deserialize_table.types.size));
				deserialize_table.ToNormalReflection(&_temporary_manager, &temp_allocator);
				deserialized_manager = &_temporary_manager;
			}
			nested_options->deserialized_field_manager = deserialized_manager;

			const DeserializeFieldTable::Type& deserialized_type = deserialize_table.types[type_index];

			// Determine if we have a new allocator entry that did not exist in the file. In this case, fail, because we
			// Don't know how large this allocator should be, or how to handle it
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR) {
					if (deserialized_type.FindField(type->fields[type->misc_info[index].allocator_info.field_index].name) == -1) {
						// It doesn't exist, fail
						ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
							" Allocator field \"{#}\" was added, but the serialization does not have that field. ",
							type->name,
							type->fields[type->misc_info[index].allocator_info.field_index].name
						);
						return ECS_DESERIALIZE_NEW_ALLOCATOR_ENTRY;
					}
				}
			}

			if (default_initialize_missing_fields) {
				// For every field that is not found in the file
				for (size_t index = 0; index < type->fields.size; index++) {
					if (deserialized_type.FindField(type->fields[index].name) == -1) {
						// Was not found - default initialize
						reflection_manager->SetInstanceFieldDefaultData(&type->fields[index], address);
					}
				}
			}

			ECS_STACK_CAPACITY_STREAM(unsigned short, deserialize_type_fields_to_iterate, 256);
			ECS_ASSERT(deserialized_type.fields.size < deserialize_type_fields_to_iterate.capacity, "Deserialized type has too many fields for deserialization!");

			// Before continuing with the normal field read, we must read the allocators firsthand and create them.
			// Iterate over the fields from the serialized type, since that is the order they were written in.
			// But there is a catch. If an allocator has another allocator as a field allocator, we must initialize
			// The referenced field allocator before the main entry. In order to do this, gather the indices of the
			// Current type allocators, and initialize them in the increasing order of their indices.

			struct DeserializedAllocatorEntry {
				unsigned int current_type_field_index;
				unsigned int deserialized_type_field_index;
				unsigned int deserialized_type_allocator_count;
			};

			// Reuse this memory in order to not create more stack entries
			ECS_STACK_CAPACITY_STREAM(DeserializedAllocatorEntry, allocators_to_initialize_entries, 8);
			ECS_STACK_CAPACITY_STREAM(unsigned int, deserialized_type_allocator_indices, 8);

			unsigned int deserialized_type_allocator_count = 0;
			for (size_t index = 0; index < deserialized_type.fields.size; index++) {
				if (deserialized_type.fields[index].custom_serializer_index == ECS_REFLECTION_CUSTOM_TYPE_ALLOCATOR) {
					if (is_not_size_determination) {
						if (initialize_type_allocators) {
							// Check to see if the field still exists
							for (size_t current_type_field_index = 0; current_type_field_index < type->fields.size; current_type_field_index++) {
								if (type->fields[current_type_field_index].name == deserialized_type.fields[index].name) {
									if (IsReflectionTypeFieldAllocator(type, current_type_field_index)) {
										size_t field_allocator_index = GetReflectionTypeFieldAllocatorFromTagAsIndex(type, current_type_field_index);
										// If it doesn't have a reference, insert it at the front.
										if (field_allocator_index == -1) {
											allocators_to_initialize_entries.Insert(0, { (unsigned int)current_type_field_index, (unsigned int)index, deserialized_type_allocator_count });
										}
										else {
											// If it has a dependency, insert it after it, or if it wasn't added yet, at the end
											unsigned int existing_type_allocator_entry_index = allocators_to_initialize_entries.Find((unsigned int)field_allocator_index, [](const DeserializedAllocatorEntry& entry) {
												return entry.current_type_field_index;
												});
											if (existing_type_allocator_entry_index == -1) {
												allocators_to_initialize_entries.AddAssert({ (unsigned int)current_type_field_index, (unsigned int)index, deserialized_type_allocator_count });
											}
											else {
												allocators_to_initialize_entries.Insert(existing_type_allocator_entry_index + 1, { (unsigned int)current_type_field_index, (unsigned int)index, deserialized_type_allocator_count });
											}
										}
									}
									break;
								}
							}
							deserialized_type_allocator_count++;
						}
					}
					deserialized_type_allocator_indices.AddAssert((unsigned int)index);
				}
				else {
					// Add the field to be iterated later on
					deserialize_type_fields_to_iterate.Add((unsigned short)index);
				}
			}

			if (is_not_size_determination) {
				if (initialize_type_allocators) {
					// Create the allocators now
					for (size_t index = 0; index < allocators_to_initialize_entries.size; index++) {
						// Use a temporary to seek to that location
						size_t instrument_offset = read_instrument->GetOffset();
						for (unsigned int subindex = 0; subindex < allocators_to_initialize_entries[index].deserialized_type_allocator_count; subindex++) {
							IgnoreTypeField(read_instrument, deserialize_table, type_index, deserialized_type_allocator_indices[subindex], deserialized_manager);
						}

						// Once we reached the location, we can read the actual allocator entry
						// TODO: If the definition changed, abort? Currently, the custom serialize type does not do anything for this case

						unsigned int current_type_field_index = allocators_to_initialize_entries[index].current_type_field_index;
						AllocatorPolymorphic previous_field_allocator = nested_options->field_allocator;
						nested_options->field_allocator = GetReflectionTypeFieldAllocator(type, current_type_field_index, address, previous_field_allocator, use_field_allocators);

						SerializeCustomTypeReadFunctionData custom_read_data;
						GetReflectionTypeAllocatorPointerAndDefinition(type, current_type_field_index, address, custom_read_data.data, custom_read_data.definition);
						custom_read_data.options = nested_options;
						custom_read_data.reflection_manager = reflection_manager;
						custom_read_data.read_instrument = read_instrument;
						custom_read_data.tags = type->fields[current_type_field_index].tag;
						deserialize_table.custom_serializers.CopyTo(custom_read_data.custom_types_version);
						custom_read_data.was_allocated = false;

						if (!ECS_SERIALIZE_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_ALLOCATOR].read(&custom_read_data)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
								" Reading the allocator field \"{#}\" failed.",
								type->name,
								type->fields[current_type_field_index].name
							);
							return ECS_DESERIALIZE_CORRUPTED_FILE;
						}

						nested_options->field_allocator = previous_field_allocator;
						// Restore the read instrument to the initial offset
						if (!read_instrument->Seek(ECS_INSTRUMENT_SEEK_START, instrument_offset)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
								" Reading the allocator field \"{#}\" failed (seeking after deserialization failed).",
								type->name,
								type->fields[current_type_field_index].name
							);
							return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
						}
					}

					// For all type allocators that are references, initialize them now, if the initialize type allocators is specified
					for (size_t index = 0; index < type->misc_info.size; index++) {
						if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR) {
							if (HasFlag(type->misc_info[index].allocator_info.modifier, ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER_REFERENCE)) {
								AllocatorPolymorphic reference_allocator = GetReflectionTypeFieldAllocator(type, type->misc_info[index].allocator_info.field_index, address, initial_field_allocator);
								SetReflectionTypeFieldAllocatorReference(type, &type->misc_info[index].allocator_info, address, reference_allocator);
							}
						}
					}
				}
			}

			// Skip the allocator entries now, for the actual stream
			for (size_t index = 0; index < deserialized_type_allocator_indices.size; index++) {
				if (!IgnoreTypeField(read_instrument, deserialize_table, type_index, deserialized_type_allocator_indices[index], deserialized_manager)) {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
						" Skipping allocator fields failed.",
						type->name
					);
					return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
				}
			}

			// We need to keep track of the SoA streams that have been allocated such that when going
			// Through the fields such that only a single SoA allocation is made
			// These will point to the type's misc_info indices
			ECS_STACK_CAPACITY_STREAM(size_t, soa_initialized_indices, 64);
			// These are the field indices of the type that have been initialized
			// But data was not read into them. We need those such as, at the end,
			// We default initialize their data and not leave them hanging
			ECS_STACK_CAPACITY_STREAM(unsigned char, soa_initialized_but_not_read_indices, 64);

			// Iterate over the type stored inside the file
			// and for each field that is still valid read it
			for (size_t index = 0; index < deserialize_type_fields_to_iterate.size; index++) {
				size_t deserialize_field_index = deserialize_type_fields_to_iterate[index];

				// Search the field inside the type
				size_t subindex = 0;
				for (; subindex < type->fields.size; subindex++) {
					if (type->fields[subindex].name == deserialized_type.fields[deserialize_field_index].name) {
						break;
					}
				}

				const DeserializeFieldInfo& file_field_info = deserialized_type.fields[deserialize_field_index];

				// The field doesn't exist in the current type
				if (subindex == type->fields.size) {
					// Just go to the next field
					// But first the data inside the stream must be skipped
					if (!IgnoreTypeField(read_instrument, deserialize_table, type_index, deserialize_field_index, deserialized_manager)) {
						ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
							" Skipping field \"{#}\" failed.",
							type->name,
							deserialized_type.fields[deserialize_field_index].name
						);
						return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
					}
					continue;
				}

				// Check to see if it is omitted
				if (SerializeShouldOmitField(type->name, type->fields[subindex].name, omit_fields)) {
					if (!IgnoreTypeField(read_instrument, deserialize_table, type_index, deserialize_field_index, deserialized_manager)) {
						ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
							" Skipping omitted field \"{#}\" failed.",
							type->name,
							type->fields[subindex].name
						);
						return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
					}
					continue;
				}

				Stream<char> field_definition = type->fields[subindex].definition;
				void* field_data = OffsetPointer(address, type->fields[subindex].info.pointer_offset);

				// Modify the field allocator to the one specified by the tag, if enabled
				nested_options->field_allocator = GetReflectionTypeFieldAllocator(type, subindex, address, initial_field_allocator, use_field_allocators);

				if (file_field_info.flags.user_defined_as_blittable) {
					// See if the field has given size and it matches the byte size
					// If this is not a given size field or the byte size does not match then ignore it
					ulong2 field_given_size = GetReflectionTypeGivenFieldTag(&type->fields[subindex]);
					if (field_given_size.x == -1) {
						// Check blittable exception
						field_given_size = reflection_manager->FindBlittableException(field_definition);
					}

					// Read the type as a blittable value only if the byte sizes actually match
					bool success = true;
					if (file_field_info.byte_size == field_given_size.x) {
						success = read_instrument->Read(field_data, file_field_info.byte_size);
					}
					else {
						// Ignore this field with the given byte size since the blittable size has changed
						// Or it is no longer a blittable type
						success = read_instrument->Ignore(file_field_info.byte_size);
					}

					if (!success) {
						ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
							" User defined as blittable field \"{#}\" with name \"{#}\" could not be read/skipped.",
							type->name,
							field_definition,
							type->fields[subindex].name
						);
						return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
					}
					continue;
				}

				// If this field has pointer references, register them
				nested_options->passdown_info->AddPointerReferencesFromField(&type->fields[subindex], field_data, field_data);

				// Verify the basic and the stream type
				const ReflectionFieldInfo& type_field_info = type->fields[subindex].info;
				// Returns the allocation, if we are reading the data
				auto handle_soa_allocation = [&](size_t element_count) {
					void* allocation = nullptr;
					if (is_not_size_determination) {
						size_t soa_index = GetReflectionTypeSoaIndex(type, subindex);
						// Verify we need to make the allocation or not
						bool is_soa_initialized = soa_initialized_indices.Find(soa_index) != -1;
						if (!is_soa_initialized) {
							soa_initialized_indices.Add(soa_index);
							// Gather all the pointer byte sizes to make a single allocation
							const ReflectionTypeMiscSoa* soa = &type->misc_info[soa_index].soa;
							size_t per_element_size = 0;
							// At the same time, push the field indices into the initialized but not read data stream
							for (unsigned int soa_stream_index = 0; soa_stream_index < soa->parallel_stream_count; soa_stream_index++) {
								per_element_size += type->fields[soa->parallel_streams[soa_stream_index]].info.stream_byte_size;
								if (soa->parallel_streams[soa_stream_index] != subindex) {
									// Only if this index is different from ours
									soa_initialized_but_not_read_indices.AddAssert(soa->parallel_streams[soa_stream_index]);
								}
							}

							size_t allocation_size = per_element_size * element_count;
							// Don't forget to use the SoA specified allocator
							allocation = allocation_size == 0 ? nullptr : Allocate(GetReflectionTypeFieldAllocatorForSoa(type, soa, address, initial_field_allocator, use_field_allocators), allocation_size, type->fields[soa->parallel_streams[0]].info.stream_alignment);

							// Now write the corresponding pointers in the address
							for (unsigned int soa_stream_index = 0; soa_stream_index < soa->parallel_stream_count; soa_stream_index++) {
								void** soa_ptr = (void**)OffsetPointer(address, type->fields[soa->parallel_streams[soa_stream_index]].info.pointer_offset);
								*soa_ptr = allocation;
								allocation = OffsetPointer(allocation, type->fields[soa->parallel_streams[soa_stream_index]].info.stream_byte_size * element_count);
							}
						}
						else {
							// We need to remove ourselves from the initialized_but_not_read_indices
							unsigned int existing_index = soa_initialized_but_not_read_indices.Find(subindex);
							ECS_ASSERT(existing_index != -1, "Critical error during deserialization");
							soa_initialized_but_not_read_indices.RemoveSwapBack(existing_index);

							// For this case, we need to return the correct pointer
							allocation = *(void**)OffsetPointer(address, type->fields[subindex].info.pointer_offset);
						}
					}
					return allocation;
				};

				// Both types are consistent, check for the same user defined type if there is one
				if (file_field_info.basic_type == type_field_info.basic_type && file_field_info.stream_type == type_field_info.stream_type) {
					if (file_field_info.basic_type == ReflectionBasicFieldType::UserDefined && file_field_info.custom_serializer_index == -1) {
						// Check for the type to be the same
						if (file_field_info.definition != field_definition) {
							if (fail_if_mismatch) {
								ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
									" User defined field mismatch for \"{#}\". In file found definition \"{#}\", current type \"{#}\".",
									type_name,
									type->fields[subindex].name,
									file_field_info.definition,
									field_definition
								);
								return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
							}

							// Ignore the data
							if (!IgnoreTypeField(read_instrument, deserialize_table, type_index, deserialize_field_index, deserialized_manager)) {
								ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
									" Could not skip field \"{#}\".",
									type_name,
									type->fields[subindex].name
								);
								return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
							}
							continue;
						}
						// Good to go, read the data using the nested type, or custom serializer
						else {
							const ReflectionType* nested_type = reflection_manager->GetType(field_definition);
							// Handle the stream cases
							switch (type_field_info.stream_type) {
							case ReflectionStreamFieldType::Basic:
							{
								ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, nested_type, field_data, read_instrument, nested_options);
								if (code != ECS_DESERIALIZE_OK) {
									return code;
								}
							}
							break;
							case ReflectionStreamFieldType::Pointer:
							{
								if (GetReflectionFieldPointerIndirection(type_field_info) == 1) {
									ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, nested_type, *(void**)field_data, read_instrument, nested_options);
									if (code != ECS_DESERIALIZE_OK) {
										return code;
									}
								}
								else {
									if (fail_if_mismatch) {
										ECS_FORMAT_ERROR_MESSAGE(
											error_message,
											"Could not deserialize type \"{#}\", field \"{#}\" with nested type as \"{#}\". "
											"The pointer level indirection is different.",
											type_name,
											type->fields[subindex].name,
											field_definition
										);
										return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
									}
								}
							}
							break;
							case ReflectionStreamFieldType::PointerSoA:
							{
								// We need to read SoA data. We have to treat this more or less like a stream
								// There is a special case tho - we need to see if we need to make the allocation
								// Or it has been made already
								size_t element_count = 0;
								// This will read the byte size
								if (!read_instrument->ReadAlways(&element_count)) {
									ECS_FORMAT_ERROR_MESSAGE(
										error_message,
										"Deserialization for type \"{#}\" failed. Could not read pointer SoA size for field \"{#}\".",
										type_name,
										type->fields[subindex].name
									);
									return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
								}

								// Must divide by the byte size of each element
								element_count /= file_field_info.stream_byte_size;

								size_t nested_type_byte_size = GetReflectionTypeByteSize(nested_type);

								void* allocation = handle_soa_allocation(element_count);

								// Now deserialize each instance
								// If the read_data is false, then the offset here does nothing
								for (size_t element_index = 0; element_index < element_count; element_index++) {
									ECS_DESERIALIZE_CODE code = Deserialize(
										reflection_manager,
										nested_type,
										OffsetPointer(allocation, element_index * nested_type_byte_size),
										read_instrument,
										nested_options
									);
									if (code != ECS_DESERIALIZE_OK) {
										return code;
									}
								}
							}
							break;
							case ReflectionStreamFieldType::BasicTypeArray:
							{
								// Choose the minimum between the two values
								unsigned short elements_to_read = ClampMax(type_field_info.basic_type_count, file_field_info.basic_type_count);
								unsigned short elements_to_ignore = file_field_info.basic_type_count - elements_to_read;

								size_t element_byte_size = GetBasicTypeArrayElementSize(type_field_info);
								for (unsigned short element_index = 0; element_index < elements_to_read; element_index++) {
									ECS_DESERIALIZE_CODE code = Deserialize(
										reflection_manager,
										nested_type,
										OffsetPointer(field_data, element_byte_size * element_index),
										read_instrument,
										nested_options
									);
									if (code != ECS_DESERIALIZE_OK) {
										return code;
									}
								}

								if (elements_to_ignore > 0) {
									unsigned int nested_type_table_index = deserialize_table.TypeIndex(file_field_info.definition);
									if (!IgnoreType(read_instrument, deserialize_table, nested_type_table_index, deserialized_manager, elements_to_ignore)) {
										ECS_FORMAT_ERROR_MESSAGE(
											error_message,
											"Deserialization for type \"{#}\" failed. Could not ignore extra basic array entries for field \"{#}\".",
											type_name,
											type->fields[subindex].name
										);
										return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
									}
								}
							}
							break;
							case ReflectionStreamFieldType::Stream:
							case ReflectionStreamFieldType::CapacityStream:
							case ReflectionStreamFieldType::ResizableStream:
							{
								size_t element_count = 0;
								// This will read the byte size
								if (!read_instrument->ReadAlways(&element_count)) {
									ECS_FORMAT_ERROR_MESSAGE(
										error_message,
										"Deserialization for type \"{#}\" failed. Could not read stream size for field \"{#}\".",
										type_name,
										type->fields[subindex].name
									);
									return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
								}

								// Must divide by the byte size of each element
								element_count /= file_field_info.stream_byte_size;

								size_t nested_type_byte_size = GetReflectionTypeByteSize(nested_type);
								size_t allocation_size = nested_type_byte_size * element_count;

								void* allocation = nullptr;
								if (is_not_size_determination) {
									// Check resizable stream allocator
									if (type_field_info.stream_type == ReflectionStreamFieldType::ResizableStream && use_resizable_stream_allocator) {
										ResizableStream<void>* resizable_stream = (ResizableStream<void>*)field_data;
										allocation = Allocate(resizable_stream->allocator, allocation_size, type_field_info.stream_alignment);

										resizable_stream->buffer = allocation;
										resizable_stream->capacity = element_count;
										resizable_stream->size = element_count;
									}
									else {
										allocation = Allocate(nested_options->field_allocator, allocation_size, type_field_info.stream_alignment);

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
									ECS_DESERIALIZE_CODE code = Deserialize(
										reflection_manager,
										nested_type,
										OffsetPointer(allocation, element_index * nested_type_byte_size),
										read_instrument,
										nested_options
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
						// TODO: At the moment, we are refusing type changes
						// For custom serializers as it will complicate them a lot
						// There might be a need for this in the future tho
						if (field_definition != file_field_info.definition) {
							if (fail_if_mismatch) {
								ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type {#} failed."
									" User defined field mismatch for {#}. In file found definition {#}, current type {#}.",
									type_name,
									type->fields[subindex].name,
									file_field_info.definition,
									field_definition
								);
								return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
							}

							// Ignore the data
							if (!IgnoreTypeField(read_instrument, deserialize_table, type_index, deserialize_field_index, deserialized_manager)) {
								ECS_FORMAT_ERROR_MESSAGE(
									error_message,
									"Deserialization for type \"{#}\" failed. Could not skip custom type interface field \"{#}\" with file definition \"{#}\", current definition \"{#}\".",
									type_name,
									type->fields[subindex].name,
									file_field_info.definition,
									field_definition
								);
								return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
							}
							continue;
						}

						// Verify that a serializer binds to it
						unsigned int current_custom_serializer_index = FindSerializeCustomType(field_definition);
						ECS_ASSERT(current_custom_serializer_index != -1, "No custom serializer was found when deserializing.");

						// Get the version
						SerializeCustomTypeReadFunctionData custom_data;
						custom_data.data = field_data;
						custom_data.definition = field_definition;
						custom_data.options = nested_options;
						custom_data.was_allocated = false;
						custom_data.reflection_manager = reflection_manager;
						custom_data.read_instrument = read_instrument;
						custom_data.tags = type->fields[subindex].tag;
						deserialize_table.custom_serializers.CopyTo(custom_data.custom_types_version);

						if (!ECS_SERIALIZE_CUSTOM_TYPES[current_custom_serializer_index].read(&custom_data)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Deserialization for type \"{#}\" failed."
								" Reading custom serialization field \"{#}\" with definition \"{#}\" failed.",
								type->name,
								type->fields[subindex].name,
								field_definition
							);
							return ECS_DESERIALIZE_CORRUPTED_FILE;
						}
					}
					// Good to go, read the data - no user defined type here
					else {
						AllocatorPolymorphic field_allocator = nested_options->field_allocator;
						if (has_options) {
							field_allocator = options->GetFieldAllocator(type_field_info.stream_type, field_data, initial_field_allocator);
						}

						if (type_field_info.stream_type == ReflectionStreamFieldType::PointerSoA) {
							size_t read_instrument_offset = read_instrument->GetOffset();
							size_t byte_size;
							if (!DeserializeIntVariableLengthBool(read_instrument, byte_size)) {
								ECS_FORMAT_ERROR_MESSAGE(
									error_message,
									"Deserialization for type \"{#}\" failed. Could not read pointer SoA size for field \"{#}\".",
									type_name,
									type->fields[subindex].name
								);
								return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
							}
							// We don't want to advance this, as the read or reference fundamental type
							// Will need that value
							if (!read_instrument->Seek(ECS_INSTRUMENT_SEEK_START, read_instrument_offset)) {
								ECS_FORMAT_ERROR_MESSAGE(
									error_message,
									"Deserialization for type \"{#}\" failed. Could not seek back after pointer SoA size read for field \"{#}\".",
									type_name,
									type->fields[subindex].name
								);
								return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
							}

							size_t element_count = byte_size / file_field_info.stream_byte_size;
							handle_soa_allocation(element_count);
						}

						if (!ReadOrReferenceFundamentalType(type_field_info, field_data, read_instrument, file_field_info.basic_type_count, field_allocator, false)) {
							ECS_FORMAT_ERROR_MESSAGE(
								error_message,
								"Deserialization for type \"{#}\" failed. Could not read fundamental field \"{#}\".",
								type_name,
								type->fields[subindex].name
							);
							return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
						}
					}
				}
				// Mismatch - try to solve it
				else {
					// If the stream type has changed, then keep in mind that
					// If the discrepency is the basic type only, try to solve it
					if (type_field_info.stream_type == ReflectionStreamFieldType::Basic && file_field_info.stream_type == ReflectionStreamFieldType::Basic) {
						if (is_not_size_determination) {
							if (!deserialize_incompatible_basic(field_data, file_field_info, type_field_info)) {
								ECS_FORMAT_ERROR_MESSAGE(
									error_message,
									"Deserialization for type \"{#}\" failed. Could not read field \"{#}\" (mismatched basic field type).",
									type_name,
									type->fields[subindex].name
								);
								return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
							}
						}
						else {
							ECS_ASSERT(false, "Unimplemented code path");
						}
					}
					else {
						bool success = true;

						if (type_field_info.stream_type != ReflectionStreamFieldType::Basic) {
							if (file_field_info.stream_type == ReflectionStreamFieldType::Basic) {
								// Just skip the field - cannot deserialize basic types into stream types
								success &= read_instrument->Ignore(file_field_info.byte_size);
							}
							else {
								// Stream type mismatch - this is fine, can deserialize any stream type into any other
								if (file_field_info.stream_type == ReflectionStreamFieldType::Pointer) {
									// Too difficult to handle this case, at the moment, just leave it be
									// Ignore the data
									success &= read_instrument->Ignore(file_field_info.stream_byte_size);
								}
								else if (file_field_info.stream_type == ReflectionStreamFieldType::PointerSoA) {
									// Too difficult to handle this case, at the moment, just leave it be
									// Ignore the data
									size_t pointer_data_byte_size = 0;
									if (!DeserializeIntVariableLengthBool(read_instrument, pointer_data_byte_size)) {
										ECS_FORMAT_ERROR_MESSAGE(
											error_message,
											"Deserialization for type \"{#}\" failed. Could not read pointer SoA size for field \"{#}\" (mismatched field).",
											type_name,
											type->fields[subindex].name
										);
										return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
									}
									success &= read_instrument->Ignore(pointer_data_byte_size);
								}
								else {
									size_t pointer_data_byte_size = 0;
									if (file_field_info.stream_type != ReflectionStreamFieldType::BasicTypeArray) {
										if (!DeserializeIntVariableLengthBool(read_instrument, pointer_data_byte_size)) {
											ECS_FORMAT_ERROR_MESSAGE(
												error_message,
												"Deserialization for type \"{#}\" failed. Could not read stream size for field \"{#}\" (mismatched field).",
												type_name,
												type->fields[subindex].name
											);
											return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
										}
									}
									else {
										pointer_data_byte_size = file_field_info.byte_size;
									}
									size_t element_count = pointer_data_byte_size / file_field_info.stream_byte_size;

									if (type_field_info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
										size_t elements_to_read = ClampMin(element_count, (size_t)type_field_info.basic_type_count);
										if (type_field_info.basic_type != file_field_info.basic_type) {
											if (is_not_size_determination) {
												for (size_t element_index = 0; element_index < elements_to_read; element_index++) {
													success &= deserialize_incompatible_basic(field_data, file_field_info, type_field_info);
												}
											}
											else {
												success &= read_instrument->Ignore(pointer_data_byte_size);
											}
										}
										else {
											// Just read the data
											if (is_not_size_determination) {
												success &= read_instrument->Read(field_data, elements_to_read * file_field_info.stream_byte_size);
											}
											else {
												success &= read_instrument->Ignore(pointer_data_byte_size);
											}
										}

										// Ignore the rest elements
										success &= read_instrument->Ignore(file_field_info.stream_byte_size * (element_count - elements_to_read));
									}
									else {
										// Normal streams

										// If basic mismatch as well, need to check the allocator
										if (is_not_size_determination) {
											void* allocation = nullptr;
											if (type_field_info.basic_type != file_field_info.basic_type) {
												if (has_options) {
													if (options->use_resizable_stream_allocator && type_field_info.stream_type == ReflectionStreamFieldType::ResizableStream) {
														ResizableStream<void>* field_stream = (ResizableStream<void>*)field_data;
														allocation = Allocate(field_stream->allocator, element_count * type_field_info.stream_byte_size, type_field_info.stream_alignment);
													}
													else {
														ECS_ASSERT(nested_options->field_allocator.allocator != nullptr, "A deserialize field allocator must be specified when incompatible types are detected!");
														allocation = Allocate(nested_options->field_allocator, element_count * type_field_info.stream_byte_size, type_field_info.stream_alignment);
													}
												}
												else {
													ECS_ASSERT(nested_options->field_allocator.allocator != nullptr, "A deserialize field allocator must be specified when incompatible types are detected! (no options were specified)");
													allocation = Allocate(nested_options->field_allocator, element_count * type_field_info.stream_byte_size, type_field_info.stream_alignment);
												}

												for (size_t element_index = 0; element_index < element_count; element_index++) {
													success &= deserialize_incompatible_basic(OffsetPointer(allocation, element_index), file_field_info, type_field_info);
												}
											}
											else {
												if (!has_options) {
													// TODO: Use backup in case this fails?
													// Just reference the data
													allocation = read_instrument->ReferenceData(pointer_data_byte_size);
													success &= allocation != nullptr;
												}
												else {
													if (options->use_resizable_stream_allocator && type_field_info.stream_type == ReflectionStreamFieldType::ResizableStream) {
														ResizableStream<void>* field_stream = (ResizableStream<void>*)field_data;
														allocation = Allocate(field_stream->allocator, element_count * type_field_info.stream_byte_size, type_field_info.stream_alignment);
													}
													// Check the field allocator
													else if (nested_options->field_allocator.allocator != nullptr) {
														allocation = Allocate(nested_options->field_allocator, element_count * type_field_info.stream_byte_size, type_field_info.stream_alignment);
													}
													else {
														// TODO: Use backup in case this fails?
														// Just reference the data
														allocation = read_instrument->ReferenceData(pointer_data_byte_size);
														success &= allocation != nullptr;
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

													ECS_FORMAT_ERROR_MESSAGE(error_message, "Cannot deserialize field {#} which is a "
														"pointer of indirection greater than 1.", type->fields[subindex].name);
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

													ECS_FORMAT_ERROR_MESSAGE(error_message, "Cannot deserialize field {#} which is a "
														"pointer of indirection greater than 1.", type->fields[subindex].name);
													return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
												}
											}
											else if (type_field_info.stream_type == ReflectionStreamFieldType::PointerSoA) {
												// Assert at the moment. Here we should check to see if our
												// SoA was initialized or not and reject the allocation if we already
												// Were initialized
												ECS_ASSERT(false, "Deserialization failed because of a SoA pointer with mismatched type.");
												return ECS_DESERIALIZE_FIELD_TYPE_MISMATCH;
											}
										}
										else {
											success &= read_instrument->Ignore(element_count * (size_t)type_field_info.stream_byte_size);
										}
									}
								}
							}
						}
						else {
							// Just ignore the data
							if (file_field_info.stream_type == ReflectionStreamFieldType::Pointer) {
								success &= read_instrument->Ignore(file_field_info.stream_byte_size);
							}
							else if (file_field_info.stream_type == ReflectionStreamFieldType::PointerSoA) {
								size_t pointer_data_byte_size = 0;
								if (!DeserializeIntVariableLengthBool(read_instrument, pointer_data_byte_size)) {
									ECS_FORMAT_ERROR_MESSAGE(
										error_message,
										"Deserialization for type \"{#}\" failed. Could not read pointer SoA size for field \"{#}\" (mismatched field).",
										type_name,
										type->fields[subindex].name
									);
									return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
								}

								success &= read_instrument->Ignore(pointer_data_byte_size);
							}
							else {
								size_t pointer_data_byte_size = 0;
								if (!DeserializeIntVariableLengthBool(read_instrument, pointer_data_byte_size)) {
									ECS_FORMAT_ERROR_MESSAGE(
										error_message,
										"Deserialization for type \"{#}\" failed. Could not read stream size for field \"{#}\" (mismatched field).",
										type_name,
										type->fields[subindex].name
									);
									return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
								}

								success &= read_instrument->Ignore(pointer_data_byte_size);
							}
						}

						if (!success) {
							ECS_FORMAT_ERROR_MESSAGE(
								error_message,
								"Deserialization for type \"{#}\" failed. Could not read field \"{#}\" (mismatched field).",
								type_name,
								type->fields[subindex].name
							);
							return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
						}
					}
				}
			}

			// Before returning, we need to default initialize the SoA streams that have not been deserialized
			for (unsigned int index = 0; index < soa_initialized_but_not_read_indices.size; index++) {
				const ReflectionField* field = &type->fields[soa_initialized_but_not_read_indices[index]];
				void** pointer = (void**)OffsetPointer(address, field->info.pointer_offset);
				// If the type is blittable, just memset to 0. Else we need to default initialize every instance
				size_t pointer_size = GetReflectionFieldPointerSoASize(field->info, address);
				size_t element_byte_size = GetReflectionFieldStreamElementByteSize(field->info);
				// At the moment, only user defined types have a special default procedure.
				// All others are memset'ed. So test for that
				const ReflectionType* nested_reflection_type = reflection_manager->TryGetType(GetReflectionFieldPointerTarget(*field));
				if (nested_reflection_type != nullptr) {
					void* current_pointer = *pointer;
					for (size_t subindex = 0; subindex < pointer_size; subindex++) {
						reflection_manager->SetInstanceDefaultData(nested_reflection_type, current_pointer);
						current_pointer = OffsetPointer(current_pointer, element_byte_size);
					}
				}
				else {
					memset(*pointer, 0, pointer_size * element_byte_size);
				}
			}

			// There is one more thing to do. For SoA streams that have a capacity assigned, we need to change the capacity
			// To the size since that is the allocation capacity
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_SOA) {
					const ReflectionTypeMiscSoa* soa = &type->misc_info[index].soa;
					if (soa->capacity_field != UCHAR_MAX) {
						size_t soa_size = GetReflectionPointerSoASize(type, index, address);
						SetReflectionFieldSoaCapacityValue(type, soa, address, soa_size);
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
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* address,
		Stream<wchar_t> file,
		DeserializeOptions* options
	) {
		ECS_STACK_VOID_STREAM(file_buffering, ECS_KB * 64);
		Optional<OwningBufferedFileReadInstrument> read_instrument = OwningBufferedFileReadInstrument::Initialize(file, file_buffering);
		if (!read_instrument.has_value) {
			return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
		}

		return Deserialize(reflection_manager, type, address, &read_instrument.value, options);
	}

	// ------------------------------------------------------------------------------------------------------------------

	static bool ValidateDeserializeFieldInfo(const DeserializeFieldInfo& field_info) {
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

		if (IsStream(field_info.stream_type) && (field_info.stream_byte_size == 0 || field_info.stream_alignment == 0)) {
			return false;
		}

		return true;
	}

	// One type read, doesn't go recursive
	// Returns true if it succeeded, else false
	static bool DeserializeFieldTableFromDataImplementation(
		ReadInstrument* read_instrument,
		DeserializeFieldTable::Type* type,
		AllocatorPolymorphic allocator,
		const DeserializeFieldTableOptions* options
	) {
		// We are always reading with true here such that we can validate that the fields that are deserialized
		// Are valid, in case they are not we can return -1 to the caller even when read_data is false to let it know
		// That there is some corruption
		unsigned short main_type_name_size = 0;
		void* main_type_name = nullptr;

		// Read the type name
		if (!read_instrument->ReadOrReferenceDataWithSizeVariableLength(type->name, allocator)) {
			return false;
		}

		// Read the byte size, alignment and is_blittable flags
		unsigned int byte_size;
		unsigned int alignment;
		if (!read_instrument->Read(&byte_size)) {
			return false;
		}
		if (!read_instrument->Read(&alignment)) {
			return false;
		}
		type->byte_size = byte_size;
		type->alignment = alignment;
		
		if (!read_instrument->Read(&type->is_blittable)) {
			return false;
		}
		if (!read_instrument->Read(&type->is_blittable_with_pointer)) {
			return false;
		}
		
		if (options->read_type_tags) {
			if (!read_instrument->ReadOrReferenceDataWithSizeVariableLength(type->tag, allocator)) {
				return false;
			}
		}
		else {
			type->tag = { nullptr, 0 };
		}

		// The field count
		size_t field_count = 0;
		if (!read_instrument->Read(&field_count)) {
			return false;
		}

		type->fields.size = field_count;
		type->fields.buffer = (DeserializeFieldInfo*)Allocate(allocator, sizeof(DeserializeFieldInfo) * field_count);

		// Read the fields now
		for (size_t index = 0; index < field_count; index++) {
			bool read_success = true;

			// The name first
			read_success &= read_instrument->ReadOrReferenceDataWithSizeVariableLength(type->fields[index].name, allocator);

			// The tag now
			if (options->read_type_tags) {
				read_success &= read_instrument->ReadOrReferenceDataWithSizeVariableLength(type->fields[index].tag, allocator);
			}
			else {
				type->fields[index].tag = { nullptr, 0 };
			}

			read_success &= read_instrument->Read(&type->fields[index].stream_type);
			read_success &= read_instrument->Read(&type->fields[index].stream_byte_size);
			read_success &= read_instrument->Read(&type->fields[index].stream_alignment);
			read_success &= read_instrument->Read(&type->fields[index].basic_type);
			read_success &= read_instrument->Read(&type->fields[index].basic_type_count);
			read_success &= read_instrument->Read(&type->fields[index].byte_size);
			read_success &= read_instrument->Read(&type->fields[index].custom_serializer_index);
			read_success &= read_instrument->Read(&type->fields[index].flags);
			read_success &= read_instrument->Read(&type->fields[index].pointer_offset);

			if (!read_success) {
				return false;
			}

			bool is_valid = ValidateDeserializeFieldInfo(type->fields[index]);
			if (!is_valid) {
				return -1;
			}

			// The definition
			if (type->fields[index].basic_type == ReflectionBasicFieldType::UserDefined || type->fields[index].basic_type == ReflectionBasicFieldType::Enum) {
				if (!read_instrument->ReadOrReferenceDataWithSizeVariableLength(type->fields[index].definition, allocator)) {
					return false;
				}
			}
			else {
				type->fields[index].definition = GetBasicFieldDefinition(type->fields[index].basic_type);
			}
		}

		return true;
	}

	static bool DeserializeFieldTableCustomInterfacesInfo(ReadInstrument* read_instrument, AllocatorPolymorphic temporary_allocator, DeserializeFieldTable& field_table, const DeserializeFieldTableOptions* options) {
		// Firstly the serialize version
		if (!read_instrument->Read(&field_table.serialize_version)) {
			return false;
		}
		field_table.serialize_version = DeserializeFieldTableVersion(field_table.serialize_version, options);

		if (field_table.serialize_version != SERIALIZE_FIELD_TABLE_VERSION) {
			return false;
		}

		unsigned int custom_serializer_count = 0;
		if (!read_instrument->Read(&custom_serializer_count)) {
			return false;

		}
		if (custom_serializer_count == 0) {
			// This is indicative of an error
			return false;
		}

		unsigned int serializer_count = SerializeCustomTypeCount();

		// Allocate the versions of the custom serializers
		field_table.custom_serializers.buffer = (unsigned int*)Allocate(temporary_allocator, sizeof(unsigned int) * serializer_count, alignof(unsigned int));
		field_table.custom_serializers.size = serializer_count;

		ECS_STACK_CAPACITY_STREAM(char, serializer_name, 512);
		// We need to map the version now
		for (unsigned int index = 0; index < custom_serializer_count; index++) {
			serializer_name.size = 0;

			// Firstly the name and then the version
			if (!read_instrument->ReadWithSizeVariableLength(serializer_name)) {
				return false;
			}

			unsigned int version = 0;
			if (!read_instrument->Read(&version)) {
				return false;
			}

			unsigned int serializer_index = FindString(
				serializer_name.ToStream(),
				Stream<SerializeCustomType>(ECS_SERIALIZE_CUSTOM_TYPES, serializer_count),
				[](const SerializeCustomType& custom_type) {
					return custom_type.name;
				});

			if (serializer_index != -1) {
				field_table.custom_serializers[serializer_index] = version;
			}
		}

		return true;
	}


	// Goes recursive. The memory needs to be specified anyway, even if the read_data is false
	static DeserializeFieldTable DeserializeFieldTableFromDataRecursive(
		ReadInstrument* read_instrument, 
		AllocatorPolymorphic allocator,
		const DeserializeFieldTableOptions* options
	)
	{
		DeserializeFieldTable field_table;

		if (!DeserializeFieldTableCustomInterfacesInfo(read_instrument, allocator, field_table, options)) {
			field_table.types.size = 0;
			return field_table;
		}

		// Allocate DESERIALIZE_FIELD_TABLE_MAX_TYPES and fail if there are more
		field_table.types.buffer = (DeserializeFieldTable::Type*)Allocate(allocator, sizeof(DeserializeFieldTable::Type) * DESERIALIZE_FIELD_TABLE_MAX_TYPES);

		struct ScopeDeallocator {
			void operator() () {
				if (is_size_determination) {
					// Need to deallocate these
					Deallocate(allocator, type_buffer);
					Deallocate(allocator, custom_serializer_buffer);
				}
			}

			bool is_size_determination;
			void* type_buffer;
			void* custom_serializer_buffer;
			AllocatorPolymorphic allocator;
		};

		StackScope<ScopeDeallocator> scope_deallocator({ read_instrument->IsSizeDetermination(), field_table.types.buffer, field_table.custom_serializers.buffer, allocator });

		DeserializeFieldTableOptions temp_options;
		temp_options.version = field_table.serialize_version;
		temp_options.read_type_tags = DeserializeFieldTableReadTags(options);

		field_table.types.size = 1;
		if (!DeserializeFieldTableFromDataImplementation(read_instrument, field_table.types.buffer, allocator, &temp_options)) {
			field_table.types.size = 0;
			return field_table;
		}

		// Now go recursive
		DeserializeFieldTable::Type* current_type = field_table.types.buffer;

		const size_t TO_BE_READ_USER_DEFINED_STORAGE_CAPACITY = 32;
		Stream<char> _to_be_read_user_defined_types_storage[TO_BE_READ_USER_DEFINED_STORAGE_CAPACITY];
		// We need to use a stack. We must push the user defined types in the reversed order
		// I.e. iterate from the last field towards the first one for a type
		Stack<Stream<char>> to_be_read_user_defined_types(_to_be_read_user_defined_types_storage, TO_BE_READ_USER_DEFINED_STORAGE_CAPACITY);

		auto has_user_defined_fields = [&](size_t index) {
			bool is_user_defined = current_type->fields[index].basic_type == ReflectionBasicFieldType::UserDefined;
			if (is_user_defined && !current_type->fields[index].flags.user_defined_as_blittable) {
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

						ReflectionCustomTypeDependenciesData dependent_data;
						dependent_data.definition = current_definition;
						dependent_data.dependencies = nested_dependent_types;
						ECS_REFLECTION_CUSTOM_TYPES[custom_serializer_index]->GetDependencies(&dependent_data);

						ReflectionBasicFieldType basic_type;
						ReflectionStreamFieldType stream_type;
						for (size_t subindex = 0; subindex < dependent_data.dependencies.size; subindex++) {
							Stream<char> dependency = dependent_data.dependencies[subindex];
							ConvertStringToPrimitiveType(dependency, basic_type, stream_type);
							if (basic_type == ReflectionBasicFieldType::Unknown) {
								// Can be a user defined type or custom serializer again
								unsigned int nested_custom_serializer = FindSerializeCustomType(dependency);
								if (nested_custom_serializer == -1) {
									// It is a user defined type
									to_be_read_user_defined_types.Push(dependency);
								}
								else {
									dependent_types.Push(dependency);
								}
							}
						}
					}
				}
			}
		};

		for (int64_t index = (int64_t)current_type->fields.size - 1; index >= 0; index--) {
			has_user_defined_fields(index);
		}

		Stream<char> to_be_read_type_definition;
		while (to_be_read_user_defined_types.Pop(to_be_read_type_definition)) {
			current_type = field_table.types.buffer + field_table.types.size;
			// Check to see that the type was not read already - it can happen for example in struct { Stream<UserDefined>; Stream<UserDefined> }
			// and can't rule those out when walking down the fields cuz the types have not yet been read
			size_t index = 0;
			for (; index < field_table.types.size; index++) {
				if (field_table.types[index].name == to_be_read_type_definition) {
					break;
				}
			}

			// Only if the type was not yet read do it
			if (index == field_table.types.size) {
				if (!DeserializeFieldTableFromDataImplementation(read_instrument, field_table.types.buffer + field_table.types.size, allocator, &temp_options)) {
					field_table.types.size = 0;
					return field_table;
				}

				field_table.types.size++;
				ECS_ASSERT(field_table.types.size <= DESERIALIZE_FIELD_TABLE_MAX_TYPES);

				// Must iterate in reverse order
				for (int64_t field_index = (int64_t)current_type->fields.size - 1; field_index >= 0; field_index--) {
					has_user_defined_fields(field_index);
				}
			}
		}

		return field_table;
	}

	// ------------------------------------------------------------------------------------------------------------------

	DeserializeFieldTable DeserializeFieldTableFromData(ReadInstrument* read_instrument, AllocatorPolymorphic temporary_allocator, const DeserializeFieldTableOptions* options) {
		DeserializeFieldTableOptions default_options;
		if (options == nullptr) {
			options = &default_options;
		}

		return DeserializeFieldTableFromDataRecursive(read_instrument, temporary_allocator, options);
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool IgnoreDeserialize(ReadInstrument* read_instrument, const DeserializeFieldTableOptions* options)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);

		DeserializeFieldTable field_table;
		field_table = DeserializeFieldTableFromData(read_instrument, &stack_allocator);
		return IgnoreDeserialize(read_instrument, field_table, options);
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool IgnoreDeserialize(
		ReadInstrument* read_instrument,
		const DeserializeFieldTable& field_table,
		const DeserializeFieldTableOptions* options,
		const ReflectionManager* deserialized_manager,
		Stream<DeserializeTypeNameRemapping> name_remappings
	) {
		return IgnoreDeserialize(read_instrument, field_table, 0, options, deserialized_manager, name_remappings);
	}

	bool IgnoreDeserialize(
		ReadInstrument* read_instrument,
		const DeserializeFieldTable& field_table,
		unsigned int field_table_type_index,
		const DeserializeFieldTableOptions* options,
		const ReflectionManager* deserialized_manager,
		Stream<DeserializeTypeNameRemapping> name_remappings
	)
	{
		// TODO: At the moment, the name_remappings is not used. Should it be removed?

		DeserializeFieldTable versioned_table = field_table;
		versioned_table.serialize_version = DeserializeFieldTableVersion(field_table.serialize_version, options);

		ReflectionManager temp_manager;
		size_t stack_allocation_size = deserialized_manager == nullptr ? ECS_KB * 32 : 0;
		void* stack_allocation = ECS_STACK_ALLOC(stack_allocation_size);
		LinearAllocator linear_allocator(stack_allocation, stack_allocation_size);
		if (deserialized_manager == nullptr) {
			deserialized_manager = &temp_manager;
			temp_manager.type_definitions.Initialize(&linear_allocator, 32);
			versioned_table.ToNormalReflection(&temp_manager, &linear_allocator);
		}

		// The type to be ignored is the first one from the field table
		return IgnoreType(read_instrument, versioned_table, field_table_type_index, deserialized_manager);
	}

	// ------------------------------------------------------------------------------------------------------------------

	struct SerializeReflectionManagerHeader {
		unsigned char version;
		unsigned char reserved[7];
		// How many types are serialized
		size_t type_count;
	};

	bool SerializeReflectionManager(
		const ReflectionManager* reflection_manager,
		WriteInstrument* write_instrument,
		const SerializeReflectionManagerOptions* options
	) {
		SerializeReflectionManagerOptions default_options;
		if (options == nullptr) {
			options = &default_options;
		}

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
		// Record all the types that need to be written, including their dependencies
		ResizableStream<const ReflectionType*> types_to_write(&stack_allocator, 0);

		auto add_type_to_write = [&](const ReflectionType* type) {
			if (SearchBytes(types_to_write.ToStream(), type) == -1) {
				// The type wasn't already added, add it
				types_to_write.Add(type);

				if (!options->direct_types_only) {
					// Determine its dependencies
					ECS_STACK_CAPACITY_STREAM(Stream<char>, type_dependencies, 512);
					GetReflectionTypeDependentTypes(reflection_manager, type, type_dependencies);

					// Add the dependencies that don't exist already
					for (unsigned int index = 0; index < type_dependencies.size; index++) {
						const ReflectionType* dependency = reflection_manager->GetType(type_dependencies[index]);
						if (SearchBytes(types_to_write.ToStream(), dependency) == -1) {
							types_to_write.Add(dependency);
						}
					}
				}
			}
		};

		if (options->hierarchy_indices.size > 0) {
			reflection_manager->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
				if (SearchBytes(options->hierarchy_indices, type.folder_hierarchy_index) != -1) {
					add_type_to_write(&type);
				}
			});
		}
		else if (options->type_names.size > 0) {
			// We will need to assert that are all found
			for (size_t index = 0; index < options->type_names.size; index++) {
				add_type_to_write(reflection_manager->GetType(options->type_names[index]));
			}
		}
		else {
			// All types are to be written, simply add all pointers
			types_to_write.Reserve(reflection_manager->type_definitions.GetCount());
			reflection_manager->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
				types_to_write.Add(&type);
			});
		}

		// Write the header firstly
		SerializeReflectionManagerHeader header;
		ZeroOut(&header);
		header.version = REFLECTION_MANAGER_SERIALIZE_VERSION;
		header.type_count = types_to_write.size;

		if (!write_instrument->Write(&header)) {
			return false;
		}

		// We must write the custom interfaces info first
		if (!WriteTypeTableCustomInterfacesInfo(write_instrument)) {
			return false;
		}

		// Write each individual type, and then we are done
		for (size_t index = 0; index < types_to_write.size; index++) {
			// Always write tags
			if (!WriteTypeTable(reflection_manager, types_to_write[index], write_instrument, options->omit_fields, true)) {
				return false;
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool DeserializeReflectionManager(
		ReflectionManager* reflection_manager,
		ReadInstrument* read_instrument,
		AllocatorPolymorphic temporary_allocator,
		DeserializeFieldTable* field_table
	) {
		// Read the header
		SerializeReflectionManagerHeader header;
		if (!read_instrument->Read(&header)) {
			return false;
		}
		
		// Fail if the version are different
		if (header.version != REFLECTION_MANAGER_SERIALIZE_VERSION) {
			return false;
		}
		
		// Read the custom serializer section first. We will keep a DeserializeFieldTable separate to hold this information
		// Always read the type tags
		DeserializeFieldTableOptions deserialize_options;
		deserialize_options.read_type_tags = true;

		if (!DeserializeFieldTableCustomInterfacesInfo(read_instrument, temporary_allocator, *field_table, &deserialize_options)) {
			return false;
		}

		deserialize_options.version = field_table->serialize_version;
		field_table->types.Initialize(temporary_allocator, header.type_count);

		// Read each type one by one
		for (size_t index = 0; index < header.type_count; index++) {
			if (!DeserializeFieldTableFromDataImplementation(read_instrument, field_table->types.buffer + index, temporary_allocator, &deserialize_options)) {
				return false;
			}
		}

		// After all types have been successfully read, add them to the reflection manager. Don't use the allocate_all flag
		// Since the types that have been read will be valid outside the function while this is alive.
		field_table->ToNormalReflection(reflection_manager, temporary_allocator);

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------

#pragma region String versions

	// ------------------------------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE SerializeEx(const ReflectionManager* reflection_manager, Stream<char> definition, const void* data, WriteInstrument* write_instrument, SerializeOptions* options, Stream<char> tags)
	{
		const ReflectionType* reflection_type = reflection_manager->TryGetType(definition);
		if (reflection_type != nullptr) {
			return Serialize(reflection_manager, reflection_type, data, write_instrument, options);
		}
		else {
			// Try a custom type
			unsigned int custom_index = FindSerializeCustomType(definition);
			ECS_ASSERT(custom_index != -1);

			// Don't create a temporary passdown - that should be created already by an upper level
			SerializeCustomTypeWriteFunctionData write_data;
			write_data.reflection_manager = reflection_manager;
			write_data.data = data;
			write_data.definition = definition;
			write_data.options = options;
			write_data.write_instrument = write_instrument;
			write_data.tags = tags;
			return ECS_SERIALIZE_CUSTOM_TYPES[custom_index].write(&write_data) ? ECS_SERIALIZE_OK : ECS_SERIALIZE_CUSTOM_TYPE_FAILED;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE SerializeEx(
		const ReflectionManager* reflection_manager,
		Stream<char> definition,
		const void* address,
		Stream<wchar_t> file,
		SerializeOptions* options,
		Stream<char> tags
	) {
		ECS_STACK_VOID_STREAM(file_buffering, ECS_KB * 64);
		
		ECS_SERIALIZE_CODE code;
		OwningBufferedFileWriteInstrument::WriteTo(file, file_buffering, [&](WriteInstrument* write_instrument) {
			code = SerializeEx(reflection_manager, definition, address, write_instrument, options, tags);
			return code == ECS_SERIALIZE_OK;
		});

		return code;
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE DeserializeEx(const ReflectionManager* reflection_manager, Stream<char> definition, void* address, ReadInstrument* read_instrument, DeserializeOptions* options, Stream<char> tags)
	{
		const ReflectionType* reflection_type = reflection_manager->TryGetType(definition);
		if (reflection_type != nullptr) {
			return Deserialize(reflection_manager, reflection_type, address, read_instrument, options);
		}
		else {
			unsigned int custom_index = FindSerializeCustomType(definition);
			ECS_ASSERT(custom_index != -1);

			// Don't create an initial passdown information, that should be created before in the main deserialize function
			SerializeCustomTypeReadFunctionData read_data;
			read_data.data = address;
			read_data.definition = definition;
			read_data.reflection_manager = reflection_manager;
			read_data.options = options;
			read_data.read_instrument = read_instrument;
			for (size_t index = 0; index < ECS_REFLECTION_CUSTOM_TYPE_COUNT; index++) {
				read_data.custom_types_version[index] = ECS_SERIALIZE_CUSTOM_TYPES[index].version;
			}
			read_data.tags = tags;
			return ECS_SERIALIZE_CUSTOM_TYPES[custom_index].read(&read_data) ? ECS_DESERIALIZE_OK : ECS_DESERIALIZE_CORRUPTED_FILE;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE DeserializeEx(
		const ReflectionManager* reflection_manager, 
		Stream<char> definition, 
		void* address, 
		Stream<wchar_t> file,
		DeserializeOptions* options, 
		Stream<char> tags
	) {
		ECS_STACK_VOID_STREAM(file_buffering, ECS_KB * 64);
		Optional<OwningBufferedFileReadInstrument> read_instrument = OwningBufferedFileReadInstrument::Initialize(file, file_buffering);
		if (!read_instrument.has_value) {
			return ECS_DESERIALIZE_INSTRUMENT_FAILURE;
		}

		return DeserializeEx(reflection_manager, definition, address, &read_instrument.value, options, tags);
	}

	// ------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ------------------------------------------------------------------------------------------------------------------

	unsigned int DeserializeFieldTable::TypeIndex(Stream<char> type_name) const
	{
		for (unsigned int index = 0; index < types.size; index++) {
			if (types[index].name == type_name) {
				return index;
			}
		}

		return -1;
	}

	// ------------------------------------------------------------------------------------------------------------------

	unsigned int DeserializeFieldTable::FieldIndex(unsigned int type_index, Stream<char> field_name) const
	{
		for (unsigned int index = 0; index < types[type_index].fields.size; index++) {
			if (types[type_index].fields[index].name == field_name) {
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
		size_t omitted_count = 0;
		for (size_t index = 0; index < type->fields.size; index++) {
			if (type->fields[index].Has(STRING(ECS_SERIALIZATION_OMIT_FIELD))) {
				omitted_count++;
			}
		}

		// If different field count, they are different
		size_t field_count = types[type_index].fields.size;
		if (type->fields.size - omitted_count != field_count) {
			// Check to see if the give reflection type has omitted fields
			// It might be the case that

			return false;
		}

		// Go through all fields. All recursive user defined types need to be unchanged for the whole struct to be unchanged
		for (size_t index = 0; index < field_count; index++) {
			const DeserializeFieldInfo* deserialized_field = &types[type_index].fields[index];
			unsigned int field_index = type->FindField(deserialized_field->name);
			if (field_index == -1) {
				return false;
			}
			const ReflectionField* field = &type->fields[field_index];

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

			if (field->info.stream_alignment != deserialized_field->stream_alignment) {
				return false;
			}

			if (field->info.byte_size != deserialized_field->byte_size) {
				return false;
			}

			if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
				Stream<char> field_definition = field->definition;
				// If the user defined definition changed, then they are not equal
				if (deserialized_field->definition != field_definition) {
					return false;
				}

				// Check blittable types
				size_t blittable_type_byte_size = reflection_manager->FindBlittableException(field_definition).x;
				if (blittable_type_byte_size != -1) {
					// It is blittable now and has same byte size
					if (!deserialized_field->flags.user_defined_as_blittable || deserialized_field->byte_size != blittable_type_byte_size) {
						// Now it not anymore blittable - fail
						return false;
					}
				}
				else {
					// If not a custom serializer, check here in the field table that the type is unchanged
					unsigned int serializer_index = deserialized_field->custom_serializer_index;
					if (serializer_index != -1) {
						serializer_index = FindSerializeCustomType(field_definition);
						ECS_ASSERT(serializer_index != -1);

						// Check that both versions are the same
						if (custom_serializers[serializer_index] != ECS_SERIALIZE_CUSTOM_TYPES[serializer_index].version) {
							return false;
						}
					}
					else {
						// Check to see if the give size tag is specified
						ulong2 given_size = GetReflectionTypeGivenFieldTag(field);
						if (given_size.x != -1) {
							// For this type of field, there is an extreme case that needs to be considered 
							// When the field was changed, but it retained the same definition
							// And the same byte size. If we don't return false here, we can potentially
							// Have an incorrect return since the internal type might have changed its
							// Internal representation but kept its byte size

							// TODO: Decide if we can skip the return false here. That would mean a
							// Potentially faster load for deserializers, but it can result in missing
							// A change for this extreme edge case
							// For the time being, return false to be on the safer side
							return false;
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
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------

	bool DeserializeFieldTable::IsBlittable(unsigned int type_index) const {
		return types[type_index].is_blittable;
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t DeserializeFieldTable::TypeByteSize(unsigned int type_index) const {
		return types[type_index].byte_size;
	}

	// ------------------------------------------------------------------------------------------------------------------

	void DeserializeFieldTable::ToNormalReflection(
		ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator, 
		bool allocate_all,
		bool check_for_insertion
	) const
	{
		// Commit all types and then calculate the byte size and the alignment for them + is_blittable

		auto add_type = [=](size_t index) {
			ReflectionType type;
			type.name = types[index].name;
			if (allocate_all) {
				type.name = StringCopy(allocator, type.name);
			}
			type.fields.Initialize(allocator, types[index].fields.size);
			type.tag = { nullptr, 0 };
			type.evaluations = { nullptr, 0 };
			type.byte_size = types[index].byte_size;
			type.alignment = types[index].alignment;
			type.is_blittable = types[index].is_blittable;
			type.is_blittable_with_pointer = types[index].is_blittable_with_pointer;
			for (size_t field_index = 0; field_index < types[index].fields.size; field_index++) {
				type.fields[field_index].name = types[index].fields[field_index].name;
				type.fields[field_index].definition = types[index].fields[field_index].definition;
				type.fields[field_index].info.basic_type = types[index].fields[field_index].basic_type;
				type.fields[field_index].info.basic_type_count = types[index].fields[field_index].basic_type_count;
				type.fields[field_index].info.byte_size = types[index].fields[field_index].byte_size;
				type.fields[field_index].info.stream_type = types[index].fields[field_index].stream_type;
				type.fields[field_index].info.stream_byte_size = types[index].fields[field_index].stream_byte_size;
				type.fields[field_index].info.stream_alignment = types[index].fields[field_index].stream_alignment;
				type.fields[field_index].info.pointer_offset = types[index].fields[field_index].pointer_offset;
				type.fields[field_index].tag = types[index].fields[field_index].tag;

				if (allocate_all) {
					type.fields[field_index].name = StringCopy(allocator, type.fields[field_index].name);
					type.fields[field_index].definition = StringCopy(allocator, type.fields[field_index].definition);
					type.fields[field_index].tag = StringCopy(allocator, type.fields[field_index].tag);
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
	}

	// ------------------------------------------------------------------------------------------------------------------

	void DeserializeFieldTable::ExtractTypesFromReflection(
		ReflectionManager* reflection_manager, 
		CapacityStream<ReflectionType*> reflection_types
	) const
	{
		for (size_t index = 0; index < types.size; index++) {
			reflection_types.AddAssert(reflection_manager->type_definitions.GetValuePtr(types[index].name));
		}
	}

	// ------------------------------------------------------------------------------------------------------------------

	size_t DeserializeFieldTable::Type::FindField(Stream<char> name) const
	{
		for (size_t index = 0; index < fields.size; index++) {
			if (fields[index].name == name) {
				return index;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------------------

}