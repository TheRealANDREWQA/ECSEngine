#include "ecspch.h"
#include "TextSerializeFields.h"
#include "../SerializationHelpers.h"
#include "../../Reflection/ReflectionStringFunctions.h"

namespace ECSEngine {

	using namespace Reflection;

	// -----------------------------------------------------------------------------------------------------------------------------

	bool TextSerializeFields(Stream<TextSerializeField> fields, Stream<wchar_t> file, AllocatorPolymorphic allocator)
	{
		size_t serialize_size = TextSerializeFieldsSize(fields);
		return SerializeWriteFile(file, allocator, serialize_size, false, [fields](uintptr_t& stream) {
			TextSerializeFields(fields, stream);
		});
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void TextSerializeFields(Stream<TextSerializeField> fields, uintptr_t& stream)
	{
		for (size_t index = 0; index < fields.size; index++) {
			ReflectionBasicFieldType basic_type = fields[index].basic_type;
			ECS_ASSERT(basic_type != ReflectionBasicFieldType::UserDefined && basic_type != ReflectionBasicFieldType::Unknown && basic_type != ReflectionBasicFieldType::Enum);

			if (fields[index].character_stream) {
				basic_type = ReflectionBasicFieldType::Enum;
			}

			// Write the name of the field and then the actual type
			char* character_stream = (char*)stream;
			size_t name_size = strlen(fields[index].name);
			memcpy(character_stream, fields[index].name, name_size * sizeof(char));
			character_stream += name_size;
			stream += name_size;

			// The type string follows
			character_stream[0] = ' ';
			character_stream[1] = '(';
			// -1 size means enough space
			CapacityStream<char> type_string(character_stream + 2, 0, -1);
			GetReflectionBasicFieldTypeString(basic_type, &type_string);
			type_string[type_string.size] = ')';
			type_string[type_string.size + 1] = ' ';
			type_string[type_string.size + 2] = '=';
			type_string[type_string.size + 3] = ' ';

			character_stream = type_string.buffer + type_string.size + 4;
			stream = (uintptr_t)character_stream;

			if (fields[index].is_stream) {
				// Add a ['\n' on the first line
				character_stream[0] = '[';
				character_stream[1] = '\n';

				character_stream += 2;
				stream += 2;
				char* before_character_stream = character_stream;
				size_t byte_size = GetReflectionBasicFieldTypeByteSize(basic_type);
				for (size_t subindex = 0; subindex < fields[index].data.size; subindex++) {
					// Write a tab before
					character_stream[0] = '\t';
					character_stream++;
					// Capacity -1 means we have ignore capacity requirements
					CapacityStream<char> capacity_characters(character_stream, 0, -1);
					size_t write_count = ConvertReflectionBasicFieldTypeToString(
						basic_type, 
						function::OffsetPointer(fields[index].data.buffer, byte_size * subindex), 
						capacity_characters
					);

					character_stream += write_count;
					// Write a new line at the end
					character_stream[0] = '\n';
					character_stream++;
				}

				// End with another ]'\n' pair
				character_stream[0] = ']';
				character_stream[1] = '\n';
				stream += character_stream - before_character_stream + 2;
			}
			else {
				// Basic type - data and new line
				// Capacity size -1 means ignore capacity requirements
				CapacityStream<char> capacity_characters(character_stream, 0, -1);

				const void* convert_data = fields[index].character_stream || basic_type == ReflectionBasicFieldType::Wchar_t ? &fields[index].data : fields[index].data.buffer;
				size_t write_count = ConvertReflectionBasicFieldTypeToString(basic_type, convert_data, capacity_characters);
				capacity_characters.Add('\n');
				stream += write_count + 1;
			}
		}

		size_t end_serialize_size = strlen(ECS_END_TEXT_SERIALIZE_STRING);
		// Put the end serialize string
		memcpy((void*)stream, ECS_END_TEXT_SERIALIZE_STRING, end_serialize_size);
		stream += end_serialize_size;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	size_t TextSerializeFieldsSize(Stream<TextSerializeField> fields)
	{
		ECS_STACK_CAPACITY_STREAM(char, temporary_buffer, 512);

		// Output that string to the end of the serialization process so as to know when packing multiple files when to stop
		size_t total_size = strlen(ECS_END_TEXT_SERIALIZE_STRING);

		for (size_t index = 0; index < fields.size; index++) {
			ReflectionBasicFieldType basic_type = fields[index].basic_type;
			ECS_ASSERT(basic_type != ReflectionBasicFieldType::UserDefined && basic_type != ReflectionBasicFieldType::Unknown && basic_type != ReflectionBasicFieldType::Enum);
			basic_type = fields[index].character_stream ? ReflectionBasicFieldType::Enum : fields[index].basic_type;

			// Write the name of the field
			total_size += strlen(fields[index].name) * sizeof(char);

			// Next is the type enclosed in parantheses and 2 spaces - one before the type and another after it
			total_size += GetReflectionBasicFieldTypeStringSize(basic_type) + 4 * sizeof(char);

			// An equals and a space
			total_size += 2 * sizeof(char);
			
			if (fields[index].is_stream) {
				// Stream types start with a [ and end with ]
				// This must be taken into consideration
				total_size += sizeof(char) * 2; // ['\n' immediately after the type name

				size_t byte_size = GetReflectionBasicFieldTypeByteSize(basic_type);
				for (size_t subindex = 0; subindex < fields[index].data.size; subindex++) {
					// a '\t' and a '\n' are added for each data entry
					total_size += ConvertReflectionBasicFieldTypeToStringSize(basic_type, function::OffsetPointer(fields[index].data.buffer, byte_size * subindex)) + sizeof(char) * 2;
				}

				// A new line with a ]'\n' is added at the end
				total_size += sizeof(char) * 2;
			}
			else {
				// Basic type (not a stream)
				// A '\n' is added at the end
				const void* buffer_data = fields[index].character_stream || fields[index].basic_type == ReflectionBasicFieldType::Wchar_t ? 
					&fields[index].data : fields[index].data.buffer;
				total_size += ConvertReflectionBasicFieldTypeToStringSize(basic_type, buffer_data) + sizeof(char);
			}
		}

		return total_size;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file, 
		AllocatorPolymorphic allocator
	)
	{
		ECS_TEXT_DESERIALIZE_STATUS status = ECS_TEXT_DESERIALIZE_OK;

		size_t pointer_data_bytes = DeserializeReadFile(file, allocator, false, [&](uintptr_t& stream) {
			status = TextDeserializeFields(fields, memory_pool, stream);
			return 0;
		});

		return status;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields,
		AllocatorPolymorphic pointer_allocator,
		Stream<wchar_t> file,
		AllocatorPolymorphic file_allocator
	)
	{
		ECS_TEXT_DESERIALIZE_STATUS status = ECS_TEXT_DESERIALIZE_OK;

		size_t pointer_data_bytes = DeserializeReadFile(file, file_allocator, false, [&](uintptr_t& stream) {
			status = TextDeserializeFields(fields, pointer_allocator, stream);

			return 0;
		});

		return status;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// When using the memory_pool (CapacityStream<void>), for arrays the data can be put directly onto it
	// However when using allocations, those must be commited only once - they are gathered inside a stack buffer
	// The extract fields is used by the find size and find count in order to not create separate functions that do the validation
	// ExtractInfo is actually used only when extract fields is set
	// ExtractInfo receives 3 parameters - the byte size, the in_file_field_index and a boolean representing if the info is for an array entry or not
	template<bool pointer_data_allocation_available, bool extract_fields, typename Allocate, typename Deallocate, typename ExtractInfo>
	ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFieldsImplementation(
		CapacityStream<TextDeserializeField>& fields,
		uintptr_t& stream,
		Allocate&& allocate,
		Deallocate&& deallocate,
		ExtractInfo&& extract_info
	) {
		ECS_TEXT_DESERIALIZE_STATUS status = ECS_TEXT_DESERIALIZE_OK;

		char* characters = (char*)stream;

		// Check for '\0' that are embedded inside character streams - temporarly make them 1 and the record their position
		// in order to make them back to '\0' at the end

		unsigned int last_checked_modified_ascii_string_index = 0;
		ECS_STACK_CAPACITY_STREAM(unsigned int, modified_ascii_string_indices, 2048);
		char* null_character = strchr(characters, '\0');
		ECS_STACK_CAPACITY_STREAM(char, ascii_string_field, 64);
		GetReflectionBasicFieldTypeString(ReflectionBasicFieldType::Enum, &ascii_string_field);

		while (null_character != nullptr) {
			// Check to see that it corresponds to an ASCII string
			const char* last_new_line = null_character;

			const size_t MAX_SEARCH_DEPTH = 1024;
			size_t iteration = 0;
			while (iteration < MAX_SEARCH_DEPTH && last_new_line > characters && *last_new_line != '\n') {
				last_new_line--;
				iteration++;
			}
			if (*last_new_line != '\n' && last_new_line != characters) {
				// Error - an '\0' appeared outside a field
				return ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS;
			}

			// If the new line was found look, the ASCIIString
			bool field_has_ascii_string = strstr(last_new_line, ascii_string_field.buffer) != nullptr;
			if (!field_has_ascii_string) {
				// If there is not an ascii field, it might be the last '\0' which terminates the entire file
				// Look for another new line
				const char* second_last_new_line = last_new_line - 1;
				iteration = 0;
				while (second_last_new_line > characters && iteration < MAX_SEARCH_DEPTH && *second_last_new_line != '\n') {
					second_last_new_line--;
					iteration++;
				}
				// If we have failed to find another new line, it means the file is corrupted
				if (*second_last_new_line != '\n') {
					return ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS;
				}

				// If another new line is found, check to see that the end deserialize_string is here
				if (!function::CompareStrings(Stream<char>(second_last_new_line + 1, last_new_line - second_last_new_line), ToStream(ECS_END_TEXT_SERIALIZE_STRING))) {
					// If it's not, the file is corrupted
					return ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS;
				}
				else {
					// Exit the loop
					null_character = nullptr;
				}
			}
			else {
				// The field is ASCII, record all '\0' positions and make them 1
				iteration = 0;
				const char* new_line = null_character + 1;
				while (iteration < MAX_SEARCH_DEPTH && *new_line != '\n') {
					new_line++;
				}
				// Could not find another end line, fail
				if (*new_line != '\n') {
					return ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS;
				}

				for (char* character = null_character; character < new_line; character++) {
					if (*character == '\0') {
						modified_ascii_string_indices.AddSafe(character - characters);
						*character = 1;
					}
				}

				null_character = (char*)strchr(new_line + 1, '\0');
			}
		}

		// Check to see if the end serialize string exists - if it doesn't, fail
		const char* end_serialize_string = strstr(characters, ECS_END_TEXT_SERIALIZE_STRING);
		if (end_serialize_string == nullptr) {
			return ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS;
		}
		// If the new line is not before the string, fail
		if (end_serialize_string[-1] != '\n') {
			return ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS;
		}

		unsigned int in_file_field_index = 0;
		char* new_line = strchr(characters, '\n');
		auto next_iteration = [&](ECS_TEXT_DESERIALIZE_STATUS error, bool next_row) {
			status |= error;
			*new_line = '\n';
			characters = new_line + 1;
			new_line = strchr(characters, '\n');
			in_file_field_index += next_row;
		};

		// For arrays when using an allocator, the values will be temporarly be stored on the stack
		// Then a complete allocation will be used for it
		ECS_STACK_CAPACITY_STREAM(char, temporary_stack_values, ECS_KB * 128);
		ECS_STACK_CAPACITY_STREAM(Stream<void>, segmented_allocations, 128);

		while (new_line != nullptr) {
			// If the characters are equal to the end serialize string, we're done
			if (characters == end_serialize_string) {
				break;
			}

			// Look for an =. Fail if it doesn't exist
			// Make the new line temporarly '\0'
			*new_line = '\0';

			char* equals = strchr(characters, '=');
			if (equals == nullptr) {
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}

			// If it misses the pair of parantheses before the =, fail.
			*equals = '\0';
			char* open_paranthese = strchr(characters, '(');
			if (open_paranthese == nullptr) {
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}

			char* closed_paranthese = strchr(open_paranthese, ')');
			if (closed_paranthese == nullptr) {
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}

			// Assure that there is no other '(' or ')' until the =
			if (strchr(open_paranthese + 1, '(') != nullptr) {
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}

			if (strchr(closed_paranthese + 1, ')') != nullptr) {
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}

			*equals = '=';
			// Check that there are non whitespace characters after the =
			const char* right_hand_side = equals + 1;
			while (right_hand_side < new_line && (*right_hand_side == ' ' || *right_hand_side == '\t')) {
				right_hand_side++;
			}

			// Only whitespace characters on the right side of the =
			if (right_hand_side == new_line) {
				next_iteration(ECS_TEXT_DESERIALIZE_FIELD_DATA_MISSING, true);
				continue;
			}

			const char* current_character = characters;
			// Parse until the first type identifier characters
			while (current_character < new_line && !function::IsCodeIdentifierCharacter(*current_character)) {
				current_character++;
			}

			if (current_character == new_line) {
				// Skip the field - no correct identifier
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}

			// Read the identifier
			const char* identifier_start = current_character;
			while (current_character < new_line && function::IsCodeIdentifierCharacter(*current_character)) {
				current_character++;
			}

			if (current_character == new_line) {
				// Skip the field - only identifier on this line
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}

			size_t name_size = current_character - identifier_start;
			Stream<char> type_string(open_paranthese + 1, closed_paranthese - open_paranthese - 1);

			// Now get the basic type
			ReflectionBasicFieldType basic_type = ConvertStringToBasicFieldType(type_string);
			if (basic_type == ReflectionBasicFieldType::Unknown || basic_type == ReflectionBasicFieldType::UserDefined) {
				next_iteration(ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS, true);
				continue;
			}
			size_t byte_size = GetReflectionBasicFieldTypeByteSize(basic_type);

			// Check to see if it is an array or a single basic type
			const char* square_bracket = strchr(equals + 1, '[');
			if (square_bracket != nullptr) {
				// It is an array
				// Keep parsing every row
				next_iteration(ECS_TEXT_DESERIALIZE_OK, false);

				// Reset the stack buffers and the segmented allocations
				temporary_stack_values.size = 0;
				segmented_allocations.Reset();

				bool closed_bracket_found = false;
				const void* array_data = nullptr;
				size_t array_entries = 0;
				if constexpr (pointer_data_allocation_available) {
					// Get the pool starting position
					array_data = allocate(0);
				}
				while (new_line != nullptr) {
					// Make the new line a '\0'
					*new_line = '\0';

					// If the closed bracket is found or the end serialize string break
					if (strchr(characters, ']') != nullptr) {
						closed_bracket_found = true;
						// The next iteration will be executed once at the end of the loop
						//next_iteration(ECS_TEXT_DESERIALIZE_OK, false);
						break;
					}
					else if (characters == end_serialize_string) {
						break;
					}

					Stream<char> entry_characters = Stream<char>(characters, new_line - characters);
					if constexpr (!extract_fields) {
						// Get a new array entry
						if constexpr (pointer_data_allocation_available) {
							void* allocation = allocate(byte_size);
							bool success = ParseReflectionBasicFieldType(basic_type, entry_characters, allocation);
							if (!success) {
								// The first argument is the buffer towards the data - in this case we don't care about it
								deallocate(nullptr, byte_size);
							}
							else {
								// Increase the count of entries
								array_entries++;
							}
						}
						else {
							// Check to see if there is enough space on the stack buffer
							if (temporary_stack_values.size + byte_size <= temporary_stack_values.capacity) {
								void* allocation = temporary_stack_values.buffer + temporary_stack_values.size;
								bool success = ParseReflectionBasicFieldType(basic_type, entry_characters, allocation);
								if (success) {
									temporary_stack_values.size += byte_size;
									array_entries++;
								}
							}
							else {
								// Not enough space - commit the temporary values into an allocator buffer and remember to join them at the end
								void* allocation = allocate(temporary_stack_values.size);
								memcpy(allocation, temporary_stack_values.buffer, temporary_stack_values.size);
								// Add this allocation to the segmented ones
								segmented_allocations.AddSafe({ allocation, temporary_stack_values.size });
								temporary_stack_values.size = 0;

								// Use again the stack buffer
								allocation = temporary_stack_values.buffer;
								bool success = ParseReflectionBasicFieldType(basic_type, entry_characters, allocation);
								if (success) {
									temporary_stack_values.size += byte_size;
									array_entries++;
								}
							}
						}
					}
					else {
						char temp_memory[256];
						bool success = ParseReflectionBasicFieldType(basic_type, entry_characters, temp_memory);
						if (success) {
							extract_info(byte_size, in_file_field_index, true);
						}
					}

					next_iteration(ECS_TEXT_DESERIALIZE_OK, false);
				}

				if constexpr (!extract_fields) {
					if (!closed_bracket_found) {
						status |= ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS;
						// Deallocate all the segmented allocations
						for (size_t index = 0; index < segmented_allocations.size; index++) {
							// The second parameter is the number of bytes - we don't care here
							deallocate(segmented_allocations[index].buffer, 0);
						}
					}
					else {
						if constexpr (!pointer_data_allocation_available) {
							// If there are no segmented allocations, copy the data from the stack buffer
							if (segmented_allocations.size == 0) {
								array_data = allocate(temporary_stack_values.size);
								memcpy((void*)array_data, temporary_stack_values.buffer, temporary_stack_values.size);
							}
							else {
								// Make an aggregate total and copy each part
								size_t total_size = temporary_stack_values.size;
								for (size_t index = 0; index < segmented_allocations.size; index++) {
									total_size += segmented_allocations[index].size;
								}

								size_t copy_offset = 0;
								array_data = allocate(total_size);
								// Copy the values from the segmented allocations first
								for (size_t index = 0; index < segmented_allocations.size; index++) {
									memcpy(function::OffsetPointer(array_data, copy_offset), segmented_allocations[index].buffer, segmented_allocations[index].size);
									copy_offset += segmented_allocations[index].size;
									deallocate(segmented_allocations[index].buffer, 0);
								}

								// Copy the remaining stack buffer elements
								memcpy(function::OffsetPointer(array_data, copy_offset), temporary_stack_values.buffer, temporary_stack_values.size);
							}
						}

						// Add the entry to the deserialized fields
						// The name must also be allocated
						void* name_allocation = allocate(name_size + 1);
						memcpy(name_allocation, identifier_start, name_size);
						char* name_characters = (char*)name_allocation;
						name_characters[name_size] = '\0';

						TextDeserializeField field;
						field.basic_type = basic_type;
						field.name = (const char*)name_allocation;
						field.in_file_field_index = in_file_field_index;
						field.pointer_data = { array_data, array_entries };
						fields.AddSafe(&field);
					}
				}
			}
			// No pointer data - basic type serialization
			else {
				Stream<char> stream_characters(equals + 1, new_line - equals - 1);
				if constexpr (!extract_fields) {
					TextDeserializeField field;
					bool success = ParseReflectionBasicFieldType(basic_type, stream_characters, &field.floating);

					if (success) {
						// The name must be copied
						void* name_allocation = allocate(name_size + 1);
						memcpy(name_allocation, identifier_start, name_size);
						char* name_characters = (char*)name_allocation;
						name_characters[name_size] = '\0';

						field.name = name_characters;
						field.basic_type = basic_type;
						field.pointer_data = { nullptr, 0 };
						field.in_file_field_index = in_file_field_index;

						// If it is an ASCII string or wide character, copy these to a separate buffer
						if (basic_type == ReflectionBasicFieldType::Enum) {
							void* string_allocation = allocate(field.ascii_characters.size + 1);
							memcpy(string_allocation, field.ascii_characters.buffer, field.ascii_characters.size);
							char* char_string = (char*)string_allocation;

							// Now modify any bytes that were previously '\0' and modified into 1
							for (const char* character = field.ascii_characters.buffer; character < field.ascii_characters.buffer + field.ascii_characters.size; character++) {
								while ((char*)(stream + modified_ascii_string_indices[last_checked_modified_ascii_string_index]) < character
									&& last_checked_modified_ascii_string_index < modified_ascii_string_indices.size) {
									last_checked_modified_ascii_string_index++;
								}
								if (character == (char*)(stream + modified_ascii_string_indices[last_checked_modified_ascii_string_index])) {
									char_string[character - field.ascii_characters.buffer] = '\0';
								}
							}

							char_string[field.ascii_characters.size] = '\0';
							field.ascii_characters.buffer = (char*)string_allocation;
						}
						else if (basic_type == ReflectionBasicFieldType::Wchar_t) {
							void* string_allocation = allocate(sizeof(wchar_t) * (field.wide_characters.size + 1));
							memcpy(string_allocation, field.wide_characters.buffer, sizeof(wchar_t)* field.wide_characters.size);
							wchar_t* wide_string = (wchar_t*)string_allocation;
							wide_string[field.wide_characters.size] = L'\0';
							field.wide_characters.buffer = (wchar_t*)wide_string;
						}

						fields.AddSafe(&field);
					}
				}
				else {
					char temp_memory[256];
					bool success = ParseReflectionBasicFieldType(basic_type, stream_characters, temp_memory);
					if (success) {
						// ASCII string
						bool array_entry = false;
						if (basic_type == ReflectionBasicFieldType::Enum) {
							Stream<char>* string = (Stream<char>*)temp_memory;
							byte_size = (string->size + 1) * sizeof(char);
							array_entry = true;
						}
						else if (basic_type == ReflectionBasicFieldType::Wchar_t) {
							Stream<wchar_t>* string = (Stream<wchar_t>*)temp_memory;
							byte_size = (string->size + 1) * sizeof(wchar_t);
							array_entry = true;
						}
						extract_info(byte_size, in_file_field_index, array_entry);
					}
				}
			}

			next_iteration(ECS_TEXT_DESERIALIZE_OK, true);
		}

		// Advance the stream 
		stream = (uintptr_t)end_serialize_string + strlen(ECS_END_TEXT_SERIALIZE_STRING);

		return status;
	}

	ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields,
		CapacityStream<void>& memory_pool, 
		uintptr_t& stream
	)
	{
		return TextDeserializeFieldsImplementation<true, false>(fields, stream, [&](size_t byte_size) {
			void* allocation = function::OffsetPointer(memory_pool);
			memory_pool.size += byte_size;
			ECS_ASSERT(memory_pool.size <= memory_pool.capacity);
			return allocation;
		}, [&](void* allocation, size_t byte_size) {
				memory_pool.size -= byte_size;
		}, [](size_t byte_size, unsigned int in_file_field_index, bool array_entry) {});
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields, 
		AllocatorPolymorphic pointer_allocator,
		uintptr_t& stream
	)
	{
		return TextDeserializeFieldsImplementation<false, false>(fields, stream, [=](size_t byte_size) {
			return AllocateEx(pointer_allocator, byte_size);
		}, [=](void* buffer, size_t byte_size) {
				DeallocateEx(pointer_allocator, buffer);
		}, [](size_t byte_size, unsigned int in_file_field_index, bool array_entry) {});
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	size_t TextDeserializeFieldsSize(uintptr_t stream)
	{
		size_t total_memory = 0;

		CapacityStream<TextDeserializeField> dummy;
		TextDeserializeFieldsImplementation<false, true>(dummy, stream, [](size_t byte_size) {}, [](const void* buffer, size_t data_size) {},
			[&](size_t byte_size, unsigned int in_file_field_index, bool array_entry) {
				total_memory += byte_size * array_entry;
		});

		return total_memory;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	size_t TextDeserializeFieldsCount(uintptr_t stream)
	{
		size_t field_count = 0;

		CapacityStream<TextDeserializeField> dummy;
		TextDeserializeFieldsImplementation<false, true>(dummy, stream, [](size_t byte_size) {}, [](const void* buffer, size_t data_size) {}, 
			[&](size_t byte_size, unsigned int in_file_field_index, bool array_entry) {
				field_count = in_file_field_index;
		});

		return field_count;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}