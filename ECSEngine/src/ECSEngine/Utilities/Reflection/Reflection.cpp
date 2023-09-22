#include "ecspch.h"
#include "Reflection.h"
#include "ReflectionMacros.h"
#include "../File.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../../Allocators/ResizableLinearAllocator.h"
#include "../ForEachFiles.h"
#include "ReflectionStringFunctions.h"
#include "../ReferenceCountSerialize.h"
#include "../../Containers/SparseSet.h"
#include "../../Resources/AssetMetadataSerialize.h"
#include "../../Math/MathTypeSizes.h"

namespace ECSEngine {

	namespace Reflection {

		// These are pending expressions to be evaluated
		struct ReflectionExpression {
			Stream<char> name;
			Stream<char> body;
		};

		struct ReflectionEmbeddedArraySize {
			Stream<char> reflection_type;
			Stream<char> field_name;
			Stream<char> body;
		};

		struct ReflectionManagerParseStructuresThreadTaskData {
			CapacityStream<char> thread_memory;
			Stream<Stream<wchar_t>> paths;
			CapacityStream<ReflectionType> types;
			CapacityStream<ReflectionEnum> enums;
			CapacityStream<ReflectionConstant> constants;
			CapacityStream<ReflectionEmbeddedArraySize> embedded_array_size;
			HashTableDefault<Stream<ReflectionExpression>> expressions;
			const ReflectionFieldTable* field_table;
			CapacityStream<char>* error_message;
			SpinLock error_message_lock;
			size_t total_memory;
			ConditionVariable* condition_variable;
			void* allocated_buffer;
			bool success;
		};

		struct ReflectionManagerHasReflectStructuresThreadTaskData {
			ReflectionManager* reflection_manager;
			Stream<Stream<wchar_t>> files;
			unsigned int folder_index;
			unsigned int starting_path_index;
			unsigned int ending_path_index;
			AtomicStream<unsigned int>* valid_paths;
			Semaphore* semaphore;
		};

#pragma region Reflection Type Tag Processing (For certain tags the source code to be parsed is modified such that certain effects can be made possible)

		Stream<char> ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_FUNCTIONS[] = {
			STRING(ID),
			STRING(IsShared),
			STRING(AllocatorSize)
		};

		struct ReflectionRuntimeComponentKnownType {
			Stream<char> string;
			size_t byte_size;
		};

		ReflectionRuntimeComponentKnownType ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE[] = {
			{ STRING(CoalescedMesh), 8 },
			{ STRING(ResourceView), 8 },
			{ STRING(SamplerState), 8 },
			{ STRING(VertexShader), 8 },
			{ STRING(PixelShader), 8 },
			{ STRING(ComputeShader), 8 },
			{ STRING(Material), 8 },
			{ STRING(Stream<void>), 8 }
		};

		enum ReflectionTypeTagHandlerForFunctionAppendPosition : unsigned char {
			REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_BEFORE_NAME,
			REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_BEFORE_BODY,
			REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_AFTER_BODY
		};

		struct ReflectionTypeTagHandlerForFunctionData {
			Stream<char> function_name;
			CapacityStream<char>* string_to_add;
		};

		typedef ReflectionTypeTagHandlerForFunctionAppendPosition (*ReflectionTypeTagHandlerForFunction)(ReflectionTypeTagHandlerForFunctionData data);

#define TYPE_TAG_HANDLER_FOR_FUNCTION(name) ReflectionTypeTagHandlerForFunctionAppendPosition name(ReflectionTypeTagHandlerForFunctionData data)

		struct ReflectionTypeTagHandlerForFieldData {
			Stream<char> field_name;
			Stream<char> definition;
			CapacityStream<char>* string_to_add;
		};

		enum ReflectionTypeTagHandlerForFieldAppendPosition : unsigned char {
			REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_BEFORE_TYPE,
			REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_AFTER_TYPE,
			REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_BEFORE_NAME,
			REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_AFTER_NAME
		};

		typedef ReflectionTypeTagHandlerForFieldAppendPosition (*ReflectionTypeTagHandlerForField)(ReflectionTypeTagHandlerForFieldData data);

#define TYPE_TAG_HANDLER_FOR_FIELD(name) ReflectionTypeTagHandlerForFieldAppendPosition name(ReflectionTypeTagHandlerForFieldData data)

		struct ReflectionTypeTagHandler {
			Stream<char> tag;
			ReflectionTypeTagHandlerForFunction function_functor;
			ReflectionTypeTagHandlerForField field_functor;
		};

		TYPE_TAG_HANDLER_FOR_FUNCTION(TypeTagComponent) {
			for (size_t index = 0; index < std::size(ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_FUNCTIONS); index++) {
				if (data.function_name == ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_FUNCTIONS[index]) {
					data.string_to_add->Copy(STRING(ECS_EVALUATE_FUNCTION_REFLECT) " ");
					break;
				}
			}
			return REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_BEFORE_NAME;
		}

		TYPE_TAG_HANDLER_FOR_FIELD(TypeTagComponent) {
			for (size_t index = 0; index < std::size(ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE); index++) {
				if (data.definition == ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE[index].string) {
					data.string_to_add->Copy(STRING(ECS_GIVE_SIZE_REFLECTION));
					data.string_to_add->Add('(');
					function::ConvertIntToChars(*data.string_to_add, ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE[index].byte_size);
					data.string_to_add->Add(')');
					break;
				}
			}
			return REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_AFTER_NAME;
		}

		ReflectionTypeTagHandler ECS_REFLECTION_TYPE_TAG_HANDLER[] = {
			{ ECS_COMPONENT_TAG, TypeTagComponent, TypeTagComponent },
			{ ECS_GLOBAL_COMPONENT_TAG, TypeTagComponent, TypeTagComponent }
		};

		// ----------------------------------------------------------------------------------------------------------------------------

		// The body must be the block enclosed by the pair of { and }
		// It returns the new region to be parsed, allocated from the thread memory. If an error occurs it returns { nullptr, 0 }
		Stream<char> ProcessTypeTagHandler(ReflectionManagerParseStructuresThreadTaskData* parse_data, size_t handler_index, Stream<char> type_body) {
			// Check for functions first - lines with a () and no semicolon
			char* region_start = parse_data->thread_memory.buffer + parse_data->thread_memory.size;

			*region_start = '{';
			parse_data->thread_memory.size++;
			const char* last_copied_value = type_body.buffer + 1;

			char FUNCTION_REPLACE_SEMICOLON_CHAR = '`';

			Stream<char> parenthese = function::FindFirstCharacter(type_body, '(');
			while (parenthese.size > 0) {
				Stream<char> previous_line_range = { last_copied_value, function::PointerDifference(parenthese.buffer, last_copied_value) };
				Stream<char> previous_new_line = function::FindCharacterReverse(previous_line_range, '\n');
				Stream<char> next_new_line = function::FindFirstCharacter(parenthese, '\n');

				if (previous_new_line.buffer != nullptr && next_new_line.buffer != nullptr) {
					Stream<char> semicolon_range = { previous_new_line.buffer, function::PointerDifference(next_new_line.buffer, previous_new_line.buffer) };
					Stream<char> semicolon = function::FindFirstCharacter(semicolon_range, ';');
					if (semicolon.buffer == nullptr) {
						// This is a function definition
						// Get the name
						const char* name_end = function::SkipWhitespace(parenthese.buffer - 1, -1);
						if (name_end <= previous_new_line.buffer) {
							// An error, this is thought to be a function but the name is malformed
							return {};
						}

						const char* name_start = function::SkipCodeIdentifier(name_end, -1) + 1;
						if (name_start > name_end) {
							// An error, this is thought to be a function but the name is malformed
							return {};
						}

						// Get the body of the function
						Stream<char> function_body_start = function::FindFirstCharacter(parenthese, '{');
						if (function_body_start.size == 0) {
							// An error, malformed function or something else
							return {};
						}

						const char* function_body_end = function::FindMatchingParenthesis(function_body_start.buffer + 1, function_body_start.buffer + function_body_start.size, '{', '}');
						if (function_body_end == nullptr) {
							// An error, malformed function or something else
							return {};
						}

						Stream<char> closing_parenthese = function::FindFirstCharacter(parenthese, ')');
						if (closing_parenthese.size == 0 || closing_parenthese.buffer > function_body_start.buffer) {
							// An error, malformed function or something else
							return {};
						}

						// Go and remove all semicolons inside the function body such that it won't interfere with the field determination
						Stream<char> current_function_body = { function_body_start.buffer, function::PointerDifference(function_body_end, function_body_start.buffer) + 1 };
						function::ReplaceCharacter(current_function_body, ';', FUNCTION_REPLACE_SEMICOLON_CHAR);

						ECS_STACK_CAPACITY_STREAM(char, string_to_add, ECS_KB);
						Stream<char> function_name = { name_start, function::PointerDifference(name_end, name_start) + 1 };
						ReflectionTypeTagHandlerForFunctionData type_tag_function;
						type_tag_function.function_name = function_name;
						type_tag_function.string_to_add = &string_to_add;
						auto tag_position = ECS_REFLECTION_TYPE_TAG_HANDLER[handler_index].function_functor(type_tag_function);

						char* copy_start = parse_data->thread_memory.buffer + parse_data->thread_memory.size;

						// Copy everything until the start of the previous line of the function
						Stream<char> before_function_copy = { last_copied_value, function::PointerDifference(previous_new_line.buffer, last_copied_value) };
						before_function_copy.CopyTo(copy_start);
						copy_start += before_function_copy.size;
						parse_data->thread_memory.size += before_function_copy.size;

						if (string_to_add.size > 0) {
							const char* prename_region_end = function::SkipWhitespace(previous_new_line.buffer + 1);
							Stream<char> prename_region = { last_copied_value, function::PointerDifference(prename_region_end, last_copied_value) };

							Stream<char> name_and_qualifiers = { prename_region_end, function::PointerDifference(closing_parenthese.buffer, prename_region_end) + 1 };

							auto copy_string = [&]() {
								*copy_start = ' ';
								copy_start++;

								string_to_add.CopyTo(copy_start);
								copy_start += string_to_add.size;

								*copy_start = ' ';
								copy_start++;
							};

							auto copy_prename = [&]() {
								prename_region.CopyTo(copy_start);
								copy_start += prename_region.size;
							};

							auto copy_name_and_qualifiers = [&]() {
								name_and_qualifiers.CopyTo(copy_start);
								copy_start += name_and_qualifiers.size;
							};

							auto copy_function_body = [&]() {
								current_function_body.CopyTo(copy_start);
								copy_start += current_function_body.size;
							};

							last_copied_value = function_body_end + 1;

							if (tag_position == REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_BEFORE_NAME) {
								// Copy the prename region
								copy_prename();
								
								copy_string();

								// Copy the name region
								copy_name_and_qualifiers();

								// Copy the current function body
								copy_function_body();
							}
							else if (tag_position == REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_BEFORE_BODY) {
								// Copy the prename region
								copy_prename();

								// Copy the name region
								copy_name_and_qualifiers();
								
								copy_string();

								// Copy the current function body
								copy_function_body();
							}
							else if (tag_position == REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_AFTER_BODY) {
								// Copy the prename region
								copy_prename();

								// Copy the name region
								copy_name_and_qualifiers();

								// Copy the current function body
								copy_function_body();
								
								copy_string();

								last_copied_value += string_to_add.size + 2;
							}
							else {
								ECS_ASSERT(false);
							}

							// 2 spaces to padd the string
							parse_data->thread_memory.size += prename_region.size + name_and_qualifiers.size + current_function_body.size + string_to_add.size + 2;
						}
						else {
							Stream<char> copy_range = { previous_new_line.buffer + 1, function::PointerDifference(function_body_end, previous_new_line.buffer + 1) + 1 };
							copy_range.CopyTo(copy_start);
							parse_data->thread_memory.size += copy_range.size;
							last_copied_value = function_body_end + 1;
						}
					}
				}

				Stream<char> search_next_function = { last_copied_value, function::PointerDifference(type_body.buffer + type_body.size, last_copied_value) };
				parenthese = function::FindFirstCharacter(search_next_function, '(');
			}

			// Now go through all lines that have semicolons remaining and see if they have fields
			ECS_STACK_CAPACITY_STREAM(unsigned int, semicolons, 512);
			function::FindToken(type_body, ';', semicolons);

			for (unsigned int index = 0; index < semicolons.size; index++) {
				// If the line is empty, skip it
				Stream<char> previous_range = { type_body.buffer, semicolons[index] };
				Stream<char> next_range = { type_body.buffer + semicolons[index], type_body.size - semicolons[index] };

				const char* previous_line = function::FindCharacterReverse(previous_range, '\n').buffer;
				const char* next_line = function::FindFirstCharacter(next_range, '\n').buffer;

				if (previous_line == nullptr || next_line == nullptr) {
					// Fail
					return {};
				}

				// Find the definition and the name
				const char* definition_start = function::SkipWhitespace(previous_line + 1);
				const char* initial_definition_start = definition_start;
				// If there are double colons for namespace separation, account for them
				Stream<char> definition_range = { definition_start, function::PointerDifference(next_line, definition_start) };
				Stream<char> double_colon = function::FindFirstToken(definition_range, "::");
				while (double_colon.size > 0) {
					double_colon.Advance(2);
					definition_start = function::SkipWhitespace(double_colon.buffer);
					double_colon = function::FindFirstToken(double_colon, "::");
				}

				const char* definition_end = function::SkipCodeIdentifier(definition_start);
				Stream<char> definition = { definition_start, function::PointerDifference(definition_end, definition_start) };

				// Skip pointer asterisks on both ends
				const char* name_start = function::SkipWhitespace(definition_end);
				name_start = function::SkipCharacter(name_start, '*');
				name_start = function::SkipWhitespace(name_start);
				name_start = function::SkipCharacter(name_start, '*');

				const char* name_end = function::SkipCodeIdentifier(name_start);
				Stream<char> name = { name_start, function::PointerDifference(name_end, name_start) };

				ECS_STACK_CAPACITY_STREAM(char, string_to_add, 512);
				ReflectionTypeTagHandlerForFieldData type_tag_field;
				type_tag_field.definition = definition;
				type_tag_field.field_name = name;
				type_tag_field.string_to_add = &string_to_add;
				auto position_to_add = ECS_REFLECTION_TYPE_TAG_HANDLER[handler_index].field_functor(type_tag_field);

				// Include the new lines
				Stream<char> before_type_range = { previous_line, function::PointerDifference(initial_definition_start, previous_line) };
				Stream<char> after_type_range = { initial_definition_start, function::PointerDifference(definition_end, initial_definition_start) };
				Stream<char> before_name_range = { definition_end, function::PointerDifference(name_start, definition_end) };
				Stream<char> after_name_range = { name_start, function::PointerDifference(next_line, name_start) + 1 };

				char* copy_pointer = parse_data->thread_memory.buffer + parse_data->thread_memory.size;

				auto copy_range = [&](Stream<char> range) {
					range.CopyTo(copy_pointer);
					copy_pointer += range.size;
				};

				auto copy_string = [&]() {
					// Add a space before and after
					*copy_pointer = ' ';
					copy_pointer++;

					string_to_add.CopyTo(copy_pointer);
					copy_pointer += string_to_add.size;

					*copy_pointer = ' ';
					copy_pointer++;
				};
				
				if (string_to_add.size > 0) {
					copy_range(before_type_range);

					if (position_to_add == REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_BEFORE_TYPE) {
						copy_string();
						copy_range(after_type_range);
						copy_range(before_name_range);
						copy_range(after_name_range);
					}
					else if (position_to_add == REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_AFTER_TYPE) {
						copy_range(after_type_range);
						copy_string();
						copy_range(before_name_range);
						copy_range(after_name_range);
					}
					else if (position_to_add == REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_BEFORE_NAME) {
						copy_range(after_type_range);
						copy_range(before_name_range);
						copy_string();
						copy_range(after_name_range);
					}
					else if (position_to_add == REFLECTION_TYPE_TAG_HANDLER_FOR_FIELD_APPEND_AFTER_NAME) {
						// This will add a new line before the tag itself and we want the new line to appear after the string
						after_name_range.size -= 1;

						copy_range(after_type_range);
						copy_range(before_name_range);
						copy_range(after_name_range);
						copy_string();

						*copy_pointer = '\n';

						// Restore the size because it will be added later on
						after_name_range.size += 1;
					}
					else {
						ECS_ASSERT(false);
					}

					// 2 values for the spaces for the string
					parse_data->thread_memory.size += before_type_range.size + after_type_range.size + before_name_range.size + after_name_range.size + string_to_add.size + 2;
				}
				else {
					Stream<char> copy_range = { previous_line, function::PointerDifference(next_line, previous_line) + 1 };
					copy_range.CopyTo(copy_pointer);
					parse_data->thread_memory.size += copy_range.size;
				}
			}

			char* last_parenthese = parse_data->thread_memory.buffer + parse_data->thread_memory.size;
			*last_parenthese = '}';
			parse_data->thread_memory.size++;
			parse_data->thread_memory.AssertCapacity();

			Stream<char> return_value = { region_start, function::PointerDifference(parse_data->thread_memory.buffer + parse_data->thread_memory.size, region_start) };
			// Replace any semicolons that were previously inside functions with their original semicolon
			function::ReplaceCharacter(return_value, FUNCTION_REPLACE_SEMICOLON_CHAR, ';');
			return return_value;
		}

#pragma endregion

#pragma region Container types

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data)
		{
			Stream<char> opened_bracket = function::FindFirstCharacter(data->definition, '<');
			ECS_ASSERT(opened_bracket.buffer != nullptr);

			Stream<char> closed_bracket = function::FindCharacterReverse(opened_bracket, '>');
			ECS_ASSERT(closed_bracket.buffer != nullptr);

			data->dependent_types.AddAssert({ opened_bracket.buffer + 1, function::PointerDifference(closed_bracket.buffer, opened_bracket.buffer) - 1 });
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionCustomTypeMatchTemplate(ReflectionCustomTypeMatchData* data, const char* string)
		{
			size_t string_size = strlen(string);

			if (data->definition.size < string_size) {
				return false;
			}

			if (data->definition[data->definition.size - 1] != '>') {
				return false;
			}

			if (memcmp(data->definition.buffer, string, string_size * sizeof(char)) == 0) {
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> ReflectionCustomTypeGetTemplateArgument(Stream<char> definition)
		{
			Stream<char> opened_bracket = function::FindFirstCharacter(definition, '<');
			Stream<char> closed_bracket = function::FindCharacterReverse(definition, '>');
			ECS_ASSERT(opened_bracket.buffer != nullptr && closed_bracket.buffer != nullptr);

			Stream<char> type = { opened_bracket.buffer + 1, function::PointerDifference(closed_bracket.buffer, opened_bracket.buffer) - 1 };
			return type;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma region Stream

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(Stream) {
			if (data->definition.size < sizeof("Stream<")) {
				return false;
			}

			if (data->definition[data->definition.size - 1] != '>') {
				return false;
			}

			if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
				return true;
			}

			if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
				return true;
			}

			if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
				return true;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(Stream) {
			if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
				return { sizeof(Stream<void>), alignof(Stream<void>) };
			}
			else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
				return { sizeof(CapacityStream<void>), alignof(CapacityStream<void>) };
			}
			else if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
				return { sizeof(ResizableStream<void>), alignof(ResizableStream<void>) };
			}

			ECS_ASSERT(false);
			return 0;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(Stream) {
			ReflectionCustomTypeDependentTypes_SingleTemplate(data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_IS_BLITTABLE_FUNCTION(Stream) {
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_COPY_FUNCTION(Stream) {
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			size_t element_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);
			bool blittable = Reflection::SearchIsBlittable(data->reflection_manager, data->definition);

			Stream<char>* source = (Stream<char>*)data->source;
			Stream<char>* destination = (Stream<char>*)data->destination;
			size_t allocation_size = element_size * source->size;
			void* allocation = nullptr;
			if (allocation_size > 0) {
				allocation = Allocate(data->allocator, allocation_size);
				memcpy(allocation, source->buffer, allocation_size);
			}

			destination->buffer = (char*)allocation;
			destination->size = source->size;

			if (!blittable) {
				size_t count = source->size;

				ReflectionType nested_type;
				if (data->reflection_manager->TryGetType(template_type, nested_type)) {
					for (size_t index = 0; index < count; index++) {
						CopyReflectionType(
							data->reflection_manager,
							&nested_type,
							function::OffsetPointer(source->buffer, index * element_size),
							function::OffsetPointer(allocation, element_size * index),
							data->allocator
						);
					}
				}
				else {
					unsigned int custom_type_index = FindReflectionCustomType(template_type);
					ECS_ASSERT(custom_type_index != -1);
					data->definition = template_type;

					for (size_t index = 0; index < count; index++) {
						data->source = function::OffsetPointer(source->buffer, index * element_size);
						data->destination = function::OffsetPointer(allocation, element_size * index);
						ECS_REFLECTION_CUSTOM_TYPES[custom_type_index].copy(data);
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_COMPARE_FUNCTION(Stream) {
			Stream<void> first;
			Stream<void> second;

			if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
				first = *(Stream<void>*)data->first;
				second = *(Stream<void>*)data->second;
			}
			// The resizable stream can be type punned with the capacity stream
			else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0 ||
					 memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") -1) == 0) {
				first = *(CapacityStream<void>*)data->first;
				second = *(CapacityStream<void>*)data->second;
			}
			else {
				ECS_ASSERT(false);
			}

			if (first.size != second.size) {
				return false;
			}
			
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			return CompareReflectionTypeInstances(data->reflection_manager, template_type, first.buffer, second.buffer, first.size);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region SparseSet

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(SparseSet) {
			return ReflectionCustomTypeMatchTemplate(data, "SparseSet") || ReflectionCustomTypeMatchTemplate(data, "ResizableSparseSet");
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(SparseSet) {
			if (memcmp(data->definition.buffer, "SparseSet<", sizeof("SparseSet<") - 1) == 0) {
				return { sizeof(SparseSet<char>), alignof(SparseSet<char>) };
			}
			else {
				return { sizeof(ResizableSparseSet<char>), alignof(ResizableSparseSet<char>) };
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(SparseSet) {
			ReflectionCustomTypeDependentTypes_SingleTemplate(data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_IS_BLITTABLE_FUNCTION(SparseSet) {
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_COPY_FUNCTION(SparseSet) {
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			size_t element_byte_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);

			bool blittable = SearchIsBlittable(data->reflection_manager, template_type);
			if (blittable) {
				// Can blit the data type
				SparseSetCopyTypeErased(data->source, data->destination, element_byte_size, data->allocator);
			}
			else {
				data->definition = template_type;
				struct CopyData {
					ReflectionCustomTypeCopyData* custom_data;
					const ReflectionType* type;
					unsigned int custom_type_index;
				};

				// Need to copy each and every element
				auto copy_function = [](const void* source, void* destination, AllocatorPolymorphic allocator, void* extra_data) {
					CopyData* data = (CopyData*)extra_data;
					if (data->type != nullptr) {
						CopyReflectionType(data->custom_data->reflection_manager, data->type, source, destination, allocator);
					}
					else {
						data->custom_data->source = source;
						data->custom_data->destination = destination;
						ECS_REFLECTION_CUSTOM_TYPES[data->custom_type_index].copy(data->custom_data);
					}
				};

				CopyData copy_data;
				copy_data.custom_data = data;
				copy_data.type = nullptr;

				ReflectionType nested_type;

				if (data->reflection_manager->TryGetType(template_type, nested_type)) {
					copy_data.type = &nested_type;
				}
				else {
					copy_data.custom_type_index = FindReflectionCustomType(template_type);
					ECS_ASSERT(copy_data.custom_type_index != -1);
				}

				SparseSetCopyTypeErasedFunction(data->source, data->destination, element_byte_size, data->allocator, copy_function, &copy_data);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_COMPARE_FUNCTION(SparseSet) {
			// This works for both resizable and non-resizable sparse sets since the resizable can be type punned
			// to a normal non-resizable set
			SparseSet<char>* first = (SparseSet<char>*)data->first;
			SparseSet<char>* second = (SparseSet<char>*)data->second;

			if (first->size != second->size || first->capacity != second->capacity) {
				return false;
			}

			if (first->first_empty_slot != second->first_empty_slot) {
				return false;
			}

			// Compare now the indirection buffers
			if (memcmp(first->indirection_buffer, second->indirection_buffer, sizeof(float2) * first->capacity) != 0) {
				return false;
			}

			// Compare the buffer data now
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			return CompareReflectionTypeInstances(data->reflection_manager, template_type, first->buffer, second->buffer, first->size);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region DataPointer

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(DataPointer) {
			return function::CompareStrings(STRING(DataPointer), data->definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(DataPointer) {
			return { sizeof(DataPointer), alignof(DataPointer) };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(DataPointer) {}

		// -----------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_IS_BLITTABLE_FUNCTION(DataPointer) {
			return false;
		}

		// -----------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_COPY_FUNCTION(DataPointer) {
			DataPointer* source = (DataPointer*)data->source;
			DataPointer* destination = (DataPointer*)data->destination;

			unsigned short source_size = source->GetData();
			void* allocation = nullptr;
			if (source_size > 0) {
				allocation = Allocate(data->allocator, source_size);
				memcpy(allocation, source->GetPointer(), source_size);
			}

			destination->SetData(source_size);
			destination->SetPointer(allocation);
		}

		// -----------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_COMPARE_FUNCTION(DataPointer) {
			const DataPointer* first = (const DataPointer*)data->first;
			const DataPointer* second = (const DataPointer*)data->second;

			unsigned short first_size = first->GetData();
			unsigned short second_size = second->GetData();
			if (first_size == second_size) {
				return memcmp(first->GetPointer(), second->GetPointer(), first_size) == 0;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ----------------------------------------------------------------------------------------------------------------------------

		// TODO: move this to another file
		ReflectionCustomType ECS_REFLECTION_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_COUNT] = {
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(Stream),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(ReferenceCounted),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(SparseSet),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(MaterialAsset),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(DataPointer)
		};

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int FindReflectionCustomType(Stream<char> definition)
		{
			ReflectionCustomTypeMatchData match_data = { definition };

			for (unsigned int index = 0; index < std::size(ECS_REFLECTION_CUSTOM_TYPES); index++) {
				if (ECS_REFLECTION_CUSTOM_TYPES[index].match(&match_data)) {
					return index;
				}
			}

			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* _data);
		void ReflectionManagerHasReflectStructuresThreadTask(unsigned int thread_id, World* world, void* _data);

		void ReflectionManagerStylizeEnum(ReflectionEnum& enum_);

		// If the path index is -1 it won't write it
		void WriteErrorMessage(ReflectionManagerParseStructuresThreadTaskData* data, const char* message, unsigned int path_index) {
			data->success = false;
			data->error_message_lock.lock();
			data->error_message->AddStream(message);
			if (path_index != -1) {
				function::ConvertWideCharsToASCII((data->paths[path_index]), *data->error_message);
			}
			data->error_message->Add('\n');
			data->error_message->AssertCapacity();
			data->error_message_lock.unlock();

			data->condition_variable->Notify();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionManager::ReflectionManager(AllocatorPolymorphic allocator, size_t type_count, size_t enum_count) : folders(allocator, 0)
		{
			InitializeFieldTable();
			InitializeTypeTable(type_count);
			InitializeEnumTable(enum_count);

			constants.Initialize(allocator, 0);
			blittable_types.Initialize(allocator, 0);

			AddKnownBlittableExceptions();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::GetKnownBlittableExceptions(CapacityStream<BlittableType>* blittable_types)
		{
			// Stream<void> is here for assets
			blittable_types->AddAssert({ STRING(Stream<void>), sizeof(Stream<void>), alignof(Stream<void>) });
			
			// Don't add an additional include just for the Color and ColorFloat types
			blittable_types->AddAssert({ STRING(Color), sizeof(unsigned char) * 4, alignof(unsigned char) });
			blittable_types->AddAssert({ STRING(ColorFloat), sizeof(float) * 4, alignof(float) });

			// Add the math types now
			// Don't include the headers just for the byte sizes - use the MathTypeSizes.h
			for (size_t index = 0; index < ECS_MATH_STRUCTURE_TYPE_COUNT; index++) {
				blittable_types->AddAssert({ ECS_MATH_STRUCTURE_TYPE_STRINGS[index], ECS_MATH_STRUCTURE_TYPE_BYTE_SIZES[index], MathStructureAlignment() });
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddKnownBlittableExceptions()
		{
			ECS_STACK_CAPACITY_STREAM(BlittableType, blittable_types, 64);
			GetKnownBlittableExceptions(&blittable_types);

			for (unsigned int index = 0; index < blittable_types.size; index++) {
				AddBlittableException(blittable_types[index].name, blittable_types[index].byte_size, blittable_types[index].alignment);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddBlittableException(Stream<char> definition, size_t byte_size, size_t alignment)
		{
			blittable_types.Add({ definition, byte_size, alignment });
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddType(const ReflectionType* type, AllocatorPolymorphic allocator, bool coallesced)
		{
			ReflectionType copied_type;
			const ReflectionType* final_type = type;
			if (allocator.allocator != nullptr) {
				copied_type = coallesced ? type->CopyCoalesced(allocator) : type->Copy(allocator);
				final_type = &copied_type;
			}
			InsertIntoDynamicTable(type_definitions, Allocator(), *final_type, ResourceIdentifier(final_type->name));
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddTypeToHierarchy(const ReflectionType* type, unsigned int folder_hierarchy, AllocatorPolymorphic allocator, bool coallesced)
		{
			InsertIntoDynamicTable(type_definitions, Allocator(), *type, ResourceIdentifier(type->name));
			folders[folder_hierarchy].added_types.Add({ type->name, allocator, coallesced });
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::BindApprovedData(
			ReflectionManagerParseStructuresThreadTaskData* data,
			unsigned int data_count,
			unsigned int folder_index
		)
		{
			size_t total_memory = 0;
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				total_memory += data[data_index].total_memory;
			}

			void* allocation = Allocate(folders.allocator, total_memory);
			folders[folder_index].allocated_buffer = allocation;

			uintptr_t ptr = (uintptr_t)allocation;

			// Bind all the new constants and add all enums before evaluating basic array sizes for reflection types
			// since they might refer them
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				// The constants now
				for (size_t constant_index = 0; constant_index < data[data_index].constants.size; constant_index++) {
					// Set the folder index before
					ReflectionConstant constant = data[data_index].constants[constant_index];
					constant.folder_hierarchy = folder_index;
					constants.Add(constant.CopyTo(ptr));
				}

				// TODO: Do we need a disable for the stylized labels?
				for (size_t index = 0; index < data[data_index].enums.size; index++) {
					const ReflectionEnum* data_enum = &data[data_index].enums[index];
					ReflectionEnum temp_copy = *data_enum;
					ECS_STACK_CAPACITY_STREAM(Stream<char>, temp_stylized_enum_fields, 512);
					temp_copy.fields.InitializeFromBuffer(temp_stylized_enum_fields.buffer, 0);
					temp_copy.fields.Copy(temp_copy.original_fields);
					// Stylized the labels such that they don't appear excessively long
					ReflectionManagerStylizeEnum(temp_copy);

					ReflectionEnum enum_ = temp_copy.CopyTo(ptr);
					enum_.folder_hierarchy_index = folder_index;
					ResourceIdentifier identifier(enum_.name);
					if (enum_definitions.Find(identifier) != -1) {
						// If the enum already exists, fail
						FreeFolderHierarchy(folder_index);
						return false;
					}
					ECS_ASSERT(!enum_definitions.Insert(enum_, identifier));
				}
			}

			// Evaluate constants in embedded array sizes first in order to update the fields
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].embedded_array_size.size; index++) {
					ReflectionEmbeddedArraySize embedded_size = data[data_index].embedded_array_size[index];
					double constant = GetConstant(embedded_size.body);
					if (constant == DBL_MAX) {
						// Check enum original fields - if we have a match then we can proceed
						enum_definitions.ForEachConst<true>([&](const ReflectionEnum& enum_, ResourceIdentifier identifier) {
							for (size_t field_index = 0; field_index < enum_.original_fields.size; field_index++) {
								if (enum_.original_fields[field_index] == embedded_size.body) {
									constant = (double)field_index;
									return true;
								}
							}
							return false;
						});

						if (constant == DBL_MAX) {
							// Fail
							FreeFolderHierarchy(folder_index);
							ECS_FORMAT_TEMP_STRING(
								error_message,
								"Failed to determine constant {#} for type {#}, field {#} when trying to determine embedded size.",
								embedded_size.body,
								embedded_size.reflection_type,
								embedded_size.field_name
							);
							WriteErrorMessage(data, error_message.buffer, -1);
							return false;
						}
					}

					unsigned short int_constant = (unsigned short)constant;

					unsigned int type_index = function::FindString(embedded_size.reflection_type, data[data_index].types.ToStream(), [](const ReflectionType& type) {
						return type.name;
					});
					
					if (type_index == -1) {
						// Fail
						FreeFolderHierarchy(folder_index);
						ECS_FORMAT_TEMP_STRING(
							error_message,
							"Embedded array size for field {#}, type {#} is missing its user determined/custom type.",
							embedded_size.field_name,
							embedded_size.reflection_type
						);
						WriteErrorMessage(data, error_message.buffer, -1);
						return false;
					}

					Stream<ReflectionField> fields = data[data_index].types[type_index].fields;
					size_t field_index = data[data_index].types[type_index].FindField(embedded_size.field_name);
					ReflectionField& current_field = fields[field_index];

					unsigned short new_byte_size = int_constant * current_field.info.byte_size;
					unsigned short difference = new_byte_size - current_field.info.byte_size;

					ReflectionType& reflection_type = data[data_index].types[type_index];
					reflection_type.byte_size += difference;

					current_field.info.basic_type_count = int_constant;
					current_field.info.byte_size = new_byte_size;
					current_field.info.stream_type = ReflectionStreamFieldType::BasicTypeArray;

					// Update the fields located after the current field
					for (size_t next_field_index = field_index + 1; next_field_index < fields.size; next_field_index++) {
						fields[next_field_index].info.pointer_offset += difference;
					}
				}
			}

			struct SkippedField {
				// If this value is 2 and the 2nd field is "name" then this field appears after "name"
				unsigned int field_after;
				unsigned int alignment;
				size_t byte_size;
				// This is used in case a skipped field has not yet been determined
				Stream<char> definition;
			};

			typedef HashTableDefault<Stream<SkippedField>> SkippedFieldsTable;
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);

			size_t total_type_count = 0;
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				total_type_count += data[data_index].types.size;
			}

			SkippedFieldsTable skipped_fields_table;
			skipped_fields_table.Initialize(&stack_allocator, function::PowerOfTwoGreater(total_type_count));

			ECS_STACK_CAPACITY_STREAM(SkippedField, current_skipped_fields, 32);

			// Because types can reference each other, keep a mask of the types that still need to be processed and
			// try iteratively to update types. If at a step not a single type can be updated then there is an error
			// X component is the data index, y component the type index inside that data
			Stream<ulong2> types_to_be_processed;
			types_to_be_processed.Initialize(&stack_allocator, total_type_count);
			types_to_be_processed.size = 0;
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].types.size; index++) {
					types_to_be_processed.Add({ data_index, index });

					current_skipped_fields.size = 0;

					ReflectionType* data_type = &data[data_index].types[index];
					for (size_t field_index = 0; field_index < data_type->fields.size; field_index++) {
						if (IsReflectionFieldSkipped(&data_type->fields[field_index])) {
							ulong2 byte_size_alignment = GetReflectionFieldSkipMacroByteSize(&data_type->fields[field_index]);

							if (byte_size_alignment.x == -1) {
								// Try to deduce the type
								byte_size_alignment = SearchReflectionUserDefinedTypeByteSizeAlignment(this, data_type->fields[field_index].definition);
								if (byte_size_alignment.x == -1) {
									// Fail if couldn't determine
									FreeFolderHierarchy(folder_index);
									return false;
								}
							}
							else {
								if (byte_size_alignment.y == -1) {
									// Assume that the alignment is that of the highest platform type
									byte_size_alignment.y = alignof(void*);
								}
							}

							// Convert the 0 values to -1 to indicate that the skipped field is not yet ready
							byte_size_alignment.x = byte_size_alignment.x == 0 ? -1 : byte_size_alignment.x;
							byte_size_alignment.y = byte_size_alignment.y == 0 ? -1 : byte_size_alignment.y;

							if (field_index == -1) {
								current_skipped_fields.AddAssert({ (unsigned int)-1, (unsigned int)byte_size_alignment.y, byte_size_alignment.x, data_type->fields[field_index].definition });
							}
							else {
								current_skipped_fields.AddAssert({ (unsigned int)field_index - 1, (unsigned int)byte_size_alignment.y, byte_size_alignment.x, data_type->fields[field_index].definition });
							}

							// Remove the field
							data_type->fields.Remove(field_index);
							field_index--;
						}
					}

					Stream<SkippedField> allocated_skipped_fields = { nullptr, 0 };
					if (current_skipped_fields.size > 0) {
						allocated_skipped_fields.InitializeAndCopy(GetAllocatorPolymorphic(&stack_allocator), current_skipped_fields);
					}
					skipped_fields_table.Insert(allocated_skipped_fields, data_type->name);
				}
			}

			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].types.size; index++) {
					const ReflectionType* data_type = &data[data_index].types[index];

					ReflectionType type = data_type->CopyTo(ptr);
					type.folder_hierarchy_index = folder_index;
					ResourceIdentifier identifier(type.name);
					// If the type already exists, fail
					if (type_definitions.Find(identifier) != -1) {
						FreeFolderHierarchy(folder_index);
						return false;
					}

					// For type fields which are user defined determined to be enums change their basic type
					// We need to do this after all enums have been commited
					for (size_t field_index = 0; field_index < type.fields.size; field_index++) {
						ReflectionField* field = &type.fields[field_index];
						if (field->info.basic_type == ReflectionBasicFieldType::UserDefined &&
							(field->info.stream_type == ReflectionStreamFieldType::Basic || field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray)) {
							ReflectionEnum enum_;
							if (TryGetEnum(field->definition, enum_)) {
								field->info.basic_type = ReflectionBasicFieldType::Enum;
								unsigned short new_byte_size = field->info.basic_type_count * sizeof(unsigned char);
								field->info.byte_size = new_byte_size;

								// Offset all fields that come after it
								for (size_t field_subindex = field_index + 1; field_subindex < type.fields.size; field_subindex++) {
									field->info.pointer_offset += new_byte_size;
								}
							}
						}
					}

					ECS_ASSERT(!type_definitions.Insert(type, identifier));
				}
			}

			auto _is_still_to_be_determined = [&](Stream<char> type, auto is_still_to_be_determined) {
				for (size_t index = 0; index < types_to_be_processed.size; index++) {
					if (data[types_to_be_processed[index].x].types[types_to_be_processed[index].y].name == type) {
						return true;
					}
				}

				ECS_STACK_CAPACITY_STREAM(Stream<char>, dependent_types, 128);
				GetReflectionFieldDependentTypes(type, dependent_types);
				// Check to see if it is a custom type that has dependencies on these types
				for (unsigned int dependent_index = 0; dependent_index < dependent_types.size; dependent_index++) {
					if (is_still_to_be_determined(dependent_types[dependent_index], is_still_to_be_determined)) {
						return true;
					}
				}
				return false;
			};

			auto is_still_to_be_determined = [&](Stream<char> type) {
				return _is_still_to_be_determined(type, _is_still_to_be_determined);
			};

			// Returns true if all skipped fields have had their byte size and alignment determined
			// It will also update the skipped fields which have previously had not had their byte size
			// assigned and now they have their type processed
			auto determine_skipped_fields = [&](Stream<SkippedField> skipped_fields) {
				for (size_t index = 0; index < skipped_fields.size; index++) {
					if (skipped_fields[index].byte_size == -1 || skipped_fields[index].alignment == -1) {
						ulong2 byte_size_alignment = SearchReflectionUserDefinedTypeByteSizeAlignment(this, skipped_fields[index].definition);
						skipped_fields[index].byte_size = byte_size_alignment.x;
						skipped_fields[index].alignment = byte_size_alignment.y;

						if (skipped_fields[index].byte_size == -1 || skipped_fields[index].alignment == -1) {
							return false;
						}
					}
				}
				return true;
			};

			auto align_type = [&](ReflectionType* type, Stream<SkippedField> skipped_fields) {
				if (type->fields.size > 0) {
					for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
						size_t field_offset = field_index == 0 ? 0 : 
							type->fields[field_index - 1].info.pointer_offset + type->fields[field_index - 1].info.byte_size;
						size_t search_index = field_index - 1;
						// For the first field we need to check separately if it has any previous fields
						for (size_t skipped_index = 0; skipped_index < skipped_fields.size; skipped_index++) {
							if (skipped_fields[skipped_index].field_after == search_index) {
								field_offset = function::AlignPointer(field_offset, skipped_fields[skipped_index].alignment);
								field_offset += skipped_fields[skipped_index].byte_size;
							}
						}

						// Align the field
						size_t current_field_alignment = GetFieldTypeAlignmentEx(this, type->fields[field_index]);
						type->fields[field_index].info.pointer_offset = function::AlignPointer(field_offset, current_field_alignment);
					}
				}
			};

			// It updates the byte size to reflect the skipped fields at the end and also update the alignment in case these
			// have a higher alignment. The cached parameters need to be calculated before hand.
			auto finalize_type_byte_size = [&](ReflectionType* type, Stream<SkippedField> skipped_fields) {
				// If it has skipped fields and those are at the end take them into account for byte size and alignment
				if (skipped_fields.size > 0) {
					size_t max_alignment = 0;

					size_t final_field_offset = type->fields[type->fields.size - 1].info.pointer_offset + type->fields[type->fields.size - 1].info.byte_size;

					for (size_t skipped_index = 0; skipped_index < skipped_fields.size; skipped_index++) {
						max_alignment = std::max(max_alignment, (size_t)skipped_fields[skipped_index].alignment);
						if (skipped_fields[skipped_index].field_after == type->fields.size - 1) {
							// Update the byte size total
							size_t aligned_offset = function::AlignPointer(final_field_offset, skipped_fields[skipped_index].alignment);
							type->byte_size = function::AlignPointer(
								aligned_offset + skipped_fields[skipped_index].byte_size, 
								std::max(max_alignment, (size_t)type->alignment)
							);

							final_field_offset = aligned_offset + skipped_fields[skipped_index].byte_size;
						}
					}

					type->alignment = std::max(type->alignment, (unsigned int)max_alignment);
				}
			};

			// Aligns the type, calculates the cached parameters ignoring the skipped fields and then takes into consideration these
			auto calculate_cached_parameters = [&](ReflectionType* type, Stream<SkippedField> skipped_fields) {
				align_type(type, skipped_fields);
				CalculateReflectionTypeCachedParameters(this, type);
				// Determine the stream byte size as well
				SetReflectionTypeFieldsStreamByteSize(this, type);
				finalize_type_byte_size(type, skipped_fields);
			};

			// Calculate the byte sizes of the types that can be calculated
			// Go through all the types and calculate their byte size

			size_t iteration_determined_types = -1;
			while (iteration_determined_types > 0) {
				iteration_determined_types = 0;

				for (size_t index = 0; index < types_to_be_processed.size; index++) {
					ReflectionType* type = GetType(data[types_to_be_processed[index].x].types[types_to_be_processed[index].y].name);

					Stream<SkippedField> skipped_fields = skipped_fields_table.GetValue(type->name);
					bool has_determined_skipped_fields = determine_skipped_fields(skipped_fields);

					if (has_determined_skipped_fields) {
						// If it doesn't have any user defined fields, can calculate it
						if (type->fields.size == 0) {
							// An omitted type
							size_t current_offset = 0;
							size_t max_alignment = 0;
							for (size_t field_index = 0; field_index < skipped_fields.size; field_index++) {
								current_offset = function::AlignPointer(current_offset, skipped_fields[field_index].alignment);
								current_offset += skipped_fields[field_index].byte_size;
								max_alignment = std::max(max_alignment, (size_t)skipped_fields[field_index].alignment);
							}

							type->byte_size = function::AlignPointer(current_offset, max_alignment);
							type->alignment = max_alignment;

							// Assume the type is blittable since we don't know its composition
							type->is_blittable = true;
							type->is_blittable_with_pointer = true;
							
							iteration_determined_types++;
							types_to_be_processed.RemoveSwapBack(index);
							index--;
						}
						else {
							size_t field_index = 0;
							for (; field_index < type->fields.size; field_index++) {
								if (type->fields[field_index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
									break;
								}
							}

							if (field_index == type->fields.size) {
								// No user defined type as basic stream type or basic type array
								// Can calculate the cached parameters
								calculate_cached_parameters(type, skipped_fields);

								iteration_determined_types++;
								types_to_be_processed.RemoveSwapBack(index);
								index--;
							}
							else {
								// If all fields can be determined, then continue (we shouldn't update them as we go
								// because we would need to keep track of those that have already been updated)
								bool are_all_user_defined_types_determined = true;
								for (size_t field_index = 0; field_index < type->fields.size && are_all_user_defined_types_determined; field_index++) {
									if (type->fields[field_index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
										if (is_still_to_be_determined(type->fields[field_index].definition)) {
											are_all_user_defined_types_determined = false;
										}
									}
								}

								if (are_all_user_defined_types_determined) {
									for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
										ReflectionField* field = &type->fields[field_index];
										Stream<char> field_definition = field->definition;
										if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
											ReflectionStreamFieldType stream_type = field->info.stream_type;

											size_t byte_size = GetReflectionTypeGivenFieldTag(field).x;
											if (byte_size == -1) {
												// Check blittable exception
												byte_size = FindBlittableException(field->definition).x;
											}

											if (byte_size == -1) {
												// If its a stream type, we need to update its stream_byte_size
												// Else we need to update the whole current structure
												if (stream_type != ReflectionStreamFieldType::Basic) {
													// Extract the new definition
													if (IsStream(stream_type)) {
														Stream<char> template_start = function::FindFirstCharacter(field_definition, '<');
														Stream<char> template_end = function::FindCharacterReverse(field_definition, '>');
														ECS_ASSERT(template_start.buffer != nullptr);
														ECS_ASSERT(template_end.buffer != nullptr);

														// If it is a void stream, don't do anything
														if (memcmp(template_start.buffer + 1, STRING(void), sizeof(STRING(void)) - 1) == 0) {													
															byte_size = sizeof(Stream<void>);
														}
														else {
															// This will get determined when using the container custom Stream
															/*char* new_definition = (char*)function::SkipWhitespace(template_start.buffer + 1);
															field->definition.size = function::PointerDifference(function::SkipWhitespace(template_end.buffer - 1, -1), new_definition) + 1;
															field->definition.buffer = new_definition;*/
														}
													}
													else if (stream_type == ReflectionStreamFieldType::Pointer) {
														byte_size = sizeof(void*);
													}
												}

												unsigned int container_type_index = FindReflectionCustomType(field_definition);

												if (container_type_index != -1) {
													// Get its byte size
													ReflectionCustomTypeByteSizeData byte_size_data;
													byte_size_data.definition = field_definition;
													byte_size_data.reflection_manager = this;
													ulong2 byte_size_alignment = ECS_REFLECTION_CUSTOM_TYPES[container_type_index].byte_size(&byte_size_data);
													byte_size = byte_size_alignment.x;
												}
												else {
													// Try to get the user defined type
													ReflectionType nested_type;
													if (TryGetType(field_definition, nested_type)) {
														byte_size = GetReflectionTypeByteSize(&nested_type);
													}
												}
											}

											// It means it is not a container nor a user defined which could be referenced
											if (byte_size == -1) {
												// Some error has happened
												// Fail
												FreeFolderHierarchy(folder_index);
												return false;
											}

											if (byte_size != 0) {
												if (stream_type == ReflectionStreamFieldType::Basic || stream_type == ReflectionStreamFieldType::BasicTypeArray) {
													if (stream_type == ReflectionStreamFieldType::BasicTypeArray) {
														// Update the byte size
														type->fields[field_index].info.byte_size = type->fields[field_index].info.basic_type_count * byte_size;
													}
													else {
														type->fields[field_index].info.byte_size = byte_size;
													}
												}
												else {
													// For pointer and streams we need to update the stream byte size
													type->fields[field_index].info.stream_byte_size = byte_size;
												}
											}
										}
									}

									// Now align the type and calculate the cached parameters
									calculate_cached_parameters(type, skipped_fields);

									iteration_determined_types++;
									types_to_be_processed.RemoveSwapBack(index);
									index--;
								}
							}
						}
					}
				}
			}

			bool evaluate_success = true;
			// And evaluate the expressions. This must be done in the end such that all types are reflected when we come to evaluate
			for (unsigned int data_index = 0; data_index < data_count; data_index++) {
				if (data[data_index].expressions.ForEachConst<true>([&](Stream<ReflectionExpression> expressions, ResourceIdentifier identifier) {
					// Set the stream first
					ReflectionType* type_ptr = type_definitions.GetValuePtr(identifier);
					type_ptr->evaluations.InitializeFromBuffer(ptr, expressions.size);

					for (size_t expression_index = 0; expression_index < expressions.size; expression_index++) {
						double result = EvaluateExpression(expressions[expression_index].body);
						if (result == DBL_MAX) {
							// Fail
							FreeFolderHierarchy(folder_index);
							evaluate_success = false;
							ECS_FORMAT_TEMP_STRING(error_message, "Failed to parse function {#} for type {#}.", expressions[expression_index].name, type_ptr->name);
							WriteErrorMessage(data, error_message.buffer, -1);

							// Return true to exit the loop
							return true;
						}

						type_ptr->evaluations[expression_index].name.InitializeAndCopy(ptr, expressions[expression_index].name);
						type_ptr->evaluations[expression_index].value = result;
					}
					return false;
				})) {
					return false;
				}
			}

			size_t difference = function::PointerDifference((void*)ptr, allocation);
			ECS_ASSERT(difference <= total_memory);

			return evaluate_success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearTypesFromAllocator(bool isolated_use, bool isolated_use_deallocate_types)
		{
			AllocatorPolymorphic allocator = folders.allocator;

			if (isolated_use) {
				// Now for the identifiers, if they have the ptr different from their reflection type name,
				// then deallocate it as well
				if (isolated_use_deallocate_types) {
					type_definitions.ForEachConst([allocator](const ReflectionType& type, ResourceIdentifier identifier) {
						if (type.name.buffer != identifier.ptr) {
							Deallocate(allocator, identifier.ptr);
						}
						type.Deallocate(allocator);
					});
				}
				type_definitions.Deallocate(allocator);
				type_definitions.InitializeFromBuffer(nullptr, 0);

				enum_definitions.ForEachConst([allocator](const ReflectionEnum& enum_, ResourceIdentifier identifier) {
					if (enum_.name.buffer != identifier.ptr) {
						Deallocate(allocator, identifier.ptr);
					}
					enum_.Deallocate(allocator);
				});
				enum_definitions.Deallocate(allocator);
				enum_definitions.InitializeFromBuffer(nullptr, 0);

				for (unsigned int index = 0; index < constants.size; index++) {
					DeallocateIfBelongs(allocator, constants[index].name.buffer);
				}
				constants.FreeBuffer();
			}
			else {
				for (unsigned int index = 0; index < folders.size; index++) {
					if (folders[index].allocated_buffer != nullptr) {
						Deallocate(allocator, folders[index].allocated_buffer);
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearFromAllocator(bool isolated_use, bool isolated_use_deallocate_types)
		{
			AllocatorPolymorphic allocator = folders.allocator;

			ClearTypesFromAllocator(isolated_use, isolated_use_deallocate_types);

			constants.FreeBuffer();
			field_table.Deallocate(allocator);
			blittable_types.FreeBuffer();
			folders.FreeBuffer();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::CreateFolderHierarchy(Stream<wchar_t> root) {
			unsigned int index = folders.ReserveNewElement();
			folders[index] = { function::StringCopy(folders.allocator, root), nullptr };
			folders[index].added_types.Initialize(Allocator(), 0);
			folders.size++;
			return index;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::CopyTypes(ReflectionManager* other) const
		{
			HashTableCopy<true, false>(type_definitions, other->type_definitions, other->Allocator(), [](const ReflectionType* type, ResourceIdentifier identifier) {
				return type->name;
			});
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data)
		{
			free(data.thread_memory.buffer);
			Deallocate(folders.allocator, data.types.buffer);
			Deallocate(folders.allocator, data.enums.buffer);
			Deallocate(folders.allocator, data.paths.buffer);
			Deallocate(folders.allocator, data.constants.buffer);
			Deallocate(folders.allocator, data.embedded_array_size.buffer);
			Deallocate(folders.allocator, data.expressions.GetAllocatedBuffer());
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		double ReflectionManager::EvaluateExpression(Stream<char> expression)
		{
			// Do a prepass and detect all the sizeof(), alignof() and ECS_CONSTANT_REFLECT's used by the expression
			// And replace them
			auto parse_sizeof_alignof_type = [expression](unsigned int offset) {
				const char* opened_parenthese = strchr(expression.buffer + offset, '(');
				char* closed_parenthese = (char*)strchr(opened_parenthese, ')');
				// Null terminate the parenthese
				*closed_parenthese = '\0';

				opened_parenthese = function::SkipWhitespace(opened_parenthese + 1);
				closed_parenthese = (char*)function::SkipWhitespace(closed_parenthese - 1, -1);

				return Stream<char>(opened_parenthese, function::PointerDifference(closed_parenthese, opened_parenthese) + 1);
			};

			auto replace_sizeof_alignof_with_value = [&expression](unsigned int offset, double value) {
				ECS_STACK_CAPACITY_STREAM(char, string_value, 64);
				function::ConvertDoubleToChars(string_value, value, 6);
				
				// If the value is negative, put it in parantheses
				int64_t initial_size = strlen(expression.buffer + offset) + 1;
				int64_t displacement = string_value.size - initial_size;
				if (value < 0.0) {
					displacement += 2;
					offset++;
				}

				expression.DisplaceElements(offset + initial_size, displacement);

				// Copy the value now
				string_value.CopyTo(expression.buffer + offset);
				expression.size += displacement;

				// Put the parenthesis if value < 0.0
				if (value < 0.0) {
					expression[offset - 1] = '(';
					expression[offset + string_value.size] = ')';
				}
			};

			// Returns DBL_MAX if an error has happened, else 0.0
			auto handle_sizeof_alignof = [&](unsigned int offset, bool sizeof_) {
				// Check that we have the byte size of that type
				Stream<char> name = parse_sizeof_alignof_type(offset);
				
				ReflectionType type;
				if (TryGetType(name.buffer, type)) {
					size_t byte_size_or_alignment = 0;
					if (sizeof_) {
						byte_size_or_alignment = GetReflectionTypeByteSize(&type);
					}
					else {
						byte_size_or_alignment = GetReflectionTypeAlignment(&type);
					}

					replace_sizeof_alignof_with_value(offset, byte_size_or_alignment);
				}
				else {
					// Try to see if it a container type
					unsigned int container_type = FindReflectionCustomType(name);
					if (container_type == -1) {
						// Try to see if it is an enum, if it is not then error
						ReflectionEnum enum_;
						if (TryGetEnum(name.buffer, enum_)) {
							// Same alignment and size
							replace_sizeof_alignof_with_value(offset, sizeof(unsigned char));
						}
						else {
							// Error
							return DBL_MAX;
						}
					}
					else {
						// Get the byte size
						ReflectionCustomTypeByteSizeData byte_size_data;
						byte_size_data.reflection_manager = this;
						byte_size_data.definition = name;
						ulong2 byte_size_alignment = ECS_REFLECTION_CUSTOM_TYPES[container_type].byte_size(&byte_size_data);
						// If byte size is 0, then fail

						if (sizeof_) {
							if (byte_size_alignment.x == 0) {
								return DBL_MAX;
							}
							replace_sizeof_alignof_with_value(offset, byte_size_alignment.x);
						}
						else {
							if (byte_size_alignment.y == 0) {
								return DBL_MAX;
							}
							replace_sizeof_alignof_with_value(offset, byte_size_alignment.y);
						}
					}
				}

				return 0.0;
			};
			
			// The sizeof first
			Stream<char> token = function::FindFirstToken(expression, "sizeof");
			while (token.buffer != nullptr) {
				unsigned int offset = token.buffer - expression.buffer;
				if (handle_sizeof_alignof(offset, true) == DBL_MAX) {
					return DBL_MAX;
				}

				token = function::FindFirstToken(expression, "sizeof");
			}

			// The alignof next
			token = function::FindFirstToken(expression, "alignof");
			while (token.buffer != nullptr) {
				unsigned int offset = token.buffer - expression.buffer;
				if (handle_sizeof_alignof(offset, false) == DBL_MAX) {
					return DBL_MAX;
				}

				token = function::FindFirstToken(expression, "alignof");
			}
			
			// Now the constants
			for (size_t constant_index = 0; constant_index < constants.size; constant_index++) {
				token = function::FindFirstToken(expression, constants[constant_index].name);
				while (token.buffer != nullptr) {
					unsigned int offset = token.buffer - expression.buffer;
					token[constants[constant_index].name.size] = '\0';
					replace_sizeof_alignof_with_value(offset, constants[constant_index].value);

					token = function::FindFirstToken(expression, constants[constant_index].name);
				}
			}

			// Return the evaluation
			return function::EvaluateExpression(expression);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 ReflectionManager::FindBlittableException(Stream<char> name) const
		{
			unsigned int index = function::FindString(name, blittable_types.ToStream(), [](BlittableType type) {
				return type.name;
			});
			return index != -1 ? ulong2(blittable_types[index].byte_size, blittable_types[index].alignment) : ulong2(-1, -1);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::FreeFolderHierarchy(unsigned int folder_index)
		{
			// Start by deallocating all added types
			for (unsigned int index = 0; index < folders[folder_index].added_types.size; index++) {
				AddedType added_type = folders[folder_index].added_types[index];

				const ReflectionType* type = GetType(added_type.type_name);
				if (added_type.coallesced_allocation) {
					type->DeallocateCoalesced(added_type.allocator);
				}
				else {
					type->Deallocate(added_type.allocator);
				}
			}

			// The types associated
			type_definitions.ForEachIndex([&](unsigned int index) {
				const ReflectionType* type_ptr = type_definitions.GetValuePtrFromIndex(index);

				if (type_ptr->folder_hierarchy_index == folder_index) {
					type_definitions.EraseFromIndex(index);
					return true;
				}

				return false;
			});

			// The enums associated
			enum_definitions.ForEachIndex([&](unsigned int index) {
				const ReflectionEnum* enum_ = enum_definitions.GetValuePtrFromIndex(index);
				if (enum_->folder_hierarchy_index == folder_index) {
					enum_definitions.EraseFromIndex(index);
					return true;
				}
				return false;
			});

			// The constants associated
			for (int64_t index = 0; index < (int64_t)constants.size; index++) {
				if (constants[index].folder_hierarchy == folder_index) {
					constants.RemoveSwapBack(index);
					index--;
				}
			}
			constants.Trim();

			if (folders[folder_index].allocated_buffer != nullptr) {
				Deallocate(folders.allocator, folders[folder_index].allocated_buffer);
				folders[folder_index].allocated_buffer = nullptr;
			}

			folders[folder_index].added_types.FreeBuffer();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType* ReflectionManager::GetType(Stream<char> name)
		{
			ResourceIdentifier identifier = ResourceIdentifier(name);
			return type_definitions.GetValuePtr(identifier);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType* ReflectionManager::GetType(unsigned int index)
		{
			return type_definitions.GetValuePtrFromIndex(index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		const ReflectionType* ReflectionManager::GetType(Stream<char> name) const
		{
			ResourceIdentifier identifier = ResourceIdentifier(name);
			return type_definitions.GetValuePtr(identifier);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		const ReflectionType* ReflectionManager::GetType(unsigned int index) const
		{
			return type_definitions.GetValuePtrFromIndex(index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum* ReflectionManager::GetEnum(Stream<char> name) {
			ResourceIdentifier identifier = ResourceIdentifier(name);
			return enum_definitions.GetValuePtr(identifier);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum* ReflectionManager::GetEnum(unsigned int index)
		{
			return enum_definitions.GetValuePtrFromIndex(index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		const ReflectionEnum* ReflectionManager::GetEnum(Stream<char> name) const {
			ResourceIdentifier identifier = ResourceIdentifier(name);
			return enum_definitions.GetValuePtr(identifier);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		const ReflectionEnum* ReflectionManager::GetEnum(unsigned int index) const
		{
			return enum_definitions.GetValuePtrFromIndex(index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		double ReflectionManager::GetConstant(Stream<char> name) const
		{
			unsigned int index = function::FindString(name, constants.ToStream(), [](ReflectionConstant constant) {
				return constant.name;
			});

			if (index != -1) {
				return constants[index].value;
			}
			return DBL_MAX;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::GetHierarchyIndex(Stream<wchar_t> hierarchy) const
		{
			for (unsigned int index = 0; index < folders.size; index++) {
				if (function::CompareStrings(folders[index].root, hierarchy)) {
					return index;
				}
			}
			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::GetHierarchyTypes(ReflectionManagerGetQuery options, unsigned int hierarchy_index) const {
			if (options.tags.size == 0) {
				auto loop = [&](auto check_index) {
					type_definitions.ForEachIndexConst([&](unsigned int index) {
						const ReflectionType* type = type_definitions.GetValuePtrFromIndex(index);
						if constexpr (check_index.value) {
							if (type->folder_hierarchy_index == hierarchy_index) {
								options.indices->AddAssert(index);
							}
						}
						else {
							options.indices->AddAssert(index);
						}
					});
				};

				if (hierarchy_index == -1) {
					loop(std::false_type{});
				}
				else {
					loop(std::true_type{});
				}
			}
			else {
				auto loop = [&](auto check_index) {
					type_definitions.ForEachIndexConst([&](unsigned int index) {
						const ReflectionType* type = type_definitions.GetValuePtrFromIndex(index);
						bool valid_index = true;
						if constexpr (check_index.value) {
							valid_index = type->folder_hierarchy_index == hierarchy_index;;
						}

						if (valid_index) {
							size_t tag_index = 0;
							for (; tag_index < options.tags.size; tag_index++) {
								if (options.strict_compare) {
									if (type->IsTag(options.tags[tag_index])) {
										break;
									}
								}
								else {
									if (type->HasTag(options.tags[tag_index])) {
										break;
									}
								}
							}

							if (tag_index < options.tags.size) {
								if (options.use_stream_indices) {
									options.stream_indices[tag_index].AddAssert(index);
								}
								else {
									options.indices->AddAssert(index);
								}
							}
						}
						});
				};

				if (hierarchy_index == -1) {
					loop(std::false_type{});
				}
				else {
					loop(std::true_type{});
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::GetHierarchyCount() const
		{
			return folders.size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::GetTypeCount() const
		{
			return type_definitions.GetCount();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* ReflectionManager::GetTypeInstancePointer(Stream<char> name, void* instance, unsigned int pointer_index) const
		{
			const ReflectionType* type = GetType(name);
			return GetTypeInstancePointer(type, instance, pointer_index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* ReflectionManager::GetTypeInstancePointer(const ReflectionType* type, void* instance, unsigned int pointer_index) const
		{
			size_t count = 0;
			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].info.stream_type == ReflectionStreamFieldType::Pointer || IsStream(type->fields[index].info.stream_type)) {
					if (count == pointer_index) {
						return (void*)((uintptr_t)instance + type->fields[index].info.pointer_offset);
					}
					count++;
				}
			}
			ECS_ASSERT(false);
			return nullptr;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> ReflectionManager::GetTypeTag(unsigned int type_index) const
		{
			const ReflectionType* type = GetType(type_index);
			return type->tag;
		}

		Stream<char> ReflectionManager::GetTypeTag(Stream<char> name) const
		{
			unsigned int type_index = type_definitions.Find(name);
			if (type_index != -1) {
				return GetTypeTag(type_index);
			}
			return { nullptr, 0 };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::HasTypeTag(unsigned int type_index, Stream<char> tag) const
		{
			return GetType(type_index)->HasTag(tag);
		}

		bool ReflectionManager::HasTypeTag(Stream<char> name, Stream<char> tag) const
		{
			return GetType(name)->HasTag(tag);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::HasValidDependencies(Stream<char> type_name) const
		{
			return HasValidDependencies(GetType(type_name));
		}

		bool ReflectionManager::HasValidDependencies(const ReflectionType* type) const
		{
			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					ReflectionType child_type;
					bool is_child_type = TryGetType(type->fields[index].definition, child_type);
					if (!is_child_type) {
						return false;
					}
					if (!HasValidDependencies(&child_type)) {
						return false;
					}
				}
			}

			return true;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::TryGetType(Stream<char> name, ReflectionType& type) const
		{
			ResourceIdentifier identifier(name);
			return type_definitions.TryGetValue(identifier, type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::TryGetEnum(Stream<char> name, ReflectionEnum& enum_) const {
			ResourceIdentifier identifier(name);
			return enum_definitions.TryGetValue(identifier, enum_);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InheritConstants(const ReflectionManager* other)
		{
			constants.ReserveNewElements(other->constants.size);
			Stream<ReflectionConstant> new_constants = StreamCoalescedDeepCopy(other->constants.ToStream(), folders.allocator);
			// Set the folder hierarchy index for these constants to -1 such that they don't get bound to any
			// folder index
			for (size_t index = 0; index < new_constants.size; index++) {
				new_constants[index].folder_hierarchy = -1;
			}
			constants.AddStream(new_constants);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeFieldTable()
		{
			constexpr size_t table_size = 256;
			void* allocation = Allocate(folders.allocator, ReflectionFieldTable::MemoryOf(table_size));
			field_table.InitializeFromBuffer(allocation, table_size);

			// Initialize all values, helped by macros
			ResourceIdentifier identifier;
#define BASIC_TYPE(type, basic_type, stream_type) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionFieldInfo(basic_type, stream_type, sizeof(type), 1), identifier);
#define INT_TYPE(type, val) BASIC_TYPE(type, ReflectionBasicFieldType::val, ReflectionStreamFieldType::Basic)
#define COMPLEX_TYPE(type, basic_type, stream_type, byte_size) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionFieldInfo(basic_type, stream_type, byte_size, 1), identifier);

#define TYPE_234(base, reflection_type) COMPLEX_TYPE(base##2, ReflectionBasicFieldType::reflection_type##2, ReflectionStreamFieldType::Basic, sizeof(base) * 2); \
COMPLEX_TYPE(base##3, ReflectionBasicFieldType::reflection_type##3, ReflectionStreamFieldType::Basic, sizeof(base) * 3); \
COMPLEX_TYPE(base##4, ReflectionBasicFieldType::reflection_type##4, ReflectionStreamFieldType::Basic, sizeof(base) * 4);

#define TYPE_234_SIGNED_INT(base, basic_reflect) COMPLEX_TYPE(base##2, ReflectionBasicFieldType::basic_reflect##2, ReflectionStreamFieldType::Basic, sizeof(base) * 2); \
COMPLEX_TYPE(base##3, ReflectionBasicFieldType::basic_reflect##3, ReflectionStreamFieldType::Basic, sizeof(base) * 3); \
COMPLEX_TYPE(base##4, ReflectionBasicFieldType::basic_reflect##4, ReflectionStreamFieldType::Basic, sizeof(base) * 4);

#define TYPE_234_UNSIGNED_INT(base, basic_reflect) COMPLEX_TYPE(u##base##2, ReflectionBasicFieldType::U##basic_reflect##2, ReflectionStreamFieldType::Basic, sizeof(base) * 2); \
COMPLEX_TYPE(u##base##3, ReflectionBasicFieldType::U##basic_reflect##3, ReflectionStreamFieldType::Basic, sizeof(base) * 3); \
COMPLEX_TYPE(u##base##4, ReflectionBasicFieldType::U##basic_reflect##4, ReflectionStreamFieldType::Basic, sizeof(base) * 4);

#define TYPE_234_INT(base, basic_reflect) TYPE_234_SIGNED_INT(base, basic_reflect); TYPE_234_UNSIGNED_INT(base, basic_reflect)

			INT_TYPE(int8_t, Int8);
			INT_TYPE(uint8_t, UInt8);
			INT_TYPE(int16_t, Int16);
			INT_TYPE(uint16_t, UInt16);
			INT_TYPE(int32_t, Int32);
			INT_TYPE(uint32_t, UInt32);
			INT_TYPE(int64_t, Int64);
			INT_TYPE(uint64_t, UInt64);
			INT_TYPE(bool, Bool);

			BASIC_TYPE(float, ReflectionBasicFieldType::Float, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(double, ReflectionBasicFieldType::Double, ReflectionStreamFieldType::Basic);

			// Basic type aliases
			BASIC_TYPE(char, ReflectionBasicFieldType::Int8, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned char, ReflectionBasicFieldType::UInt8, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(short, ReflectionBasicFieldType::Int16, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned short, ReflectionBasicFieldType::UInt16, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(int, ReflectionBasicFieldType::Int32, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned int, ReflectionBasicFieldType::UInt32, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(long long, ReflectionBasicFieldType::Int64, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned long long, ReflectionBasicFieldType::UInt64, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(size_t, ReflectionBasicFieldType::UInt64, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(wchar_t, ReflectionBasicFieldType::Wchar_t, ReflectionStreamFieldType::Basic);

			COMPLEX_TYPE(enum, ReflectionBasicFieldType::Enum, ReflectionStreamFieldType::Basic, sizeof(ReflectionBasicFieldType));
			COMPLEX_TYPE(pointer, ReflectionBasicFieldType::UserDefined, ReflectionStreamFieldType::Pointer, sizeof(void*));
			
			TYPE_234(bool, Bool);
			TYPE_234(float, Float);
			TYPE_234(double, Double);

			TYPE_234_INT(char, Char);
			TYPE_234_INT(short, Short);
			TYPE_234_INT(int, Int);
			TYPE_234_INT(long long, Long);

#undef INT_TYPE
#undef BASIC_TYPE
#undef COMPLEX_TYPE
#undef TYPE_234_INT
#undef TYPE_234_UNSIGNED_INT
#undef TYPE_234_SIGNED_INT
#undef TYPE_234
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeTypeTable(size_t count)
		{
			type_definitions.Initialize(folders.allocator, count);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeEnumTable(size_t count)
		{
			enum_definitions.Initialize(folders.allocator, count);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeParseThreadTaskData(
			size_t thread_memory,
			size_t path_count, 
			ReflectionManagerParseStructuresThreadTaskData& data, 
			CapacityStream<char>* error_message
		)
		{
			data.error_message = error_message;

			// Allocate memory for the type and enum stream; speculate 64 types and 32 enums
			const size_t max_types = 32;
			const size_t max_enums = 16;
			const size_t path_size = 128;
			const size_t max_constants = 32;
			const size_t max_expressions = 32;
			const size_t max_embedded_array_size = 32;

			data.types.Initialize(folders.allocator, 0, max_types);
			data.enums.Initialize(folders.allocator, 0, max_enums);
			data.paths.Initialize(folders.allocator, path_count);
			data.constants.Initialize(folders.allocator, 0, max_constants);
			data.expressions.Initialize(folders.allocator, max_expressions);
			data.embedded_array_size.Initialize(folders.allocator, 0, max_embedded_array_size);

			void* thread_allocation = malloc(thread_memory);
			data.thread_memory.InitializeFromBuffer(thread_allocation, 0, thread_memory);
			data.field_table = &field_table;
			data.success = true;
			data.total_memory = 0;
			data.error_message_lock.unlock();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(Stream<wchar_t> root, CapacityStream<char>* error_message) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return ProcessFolderHierarchy(hierarchy_index, error_message);
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(
			Stream<wchar_t> root, 
			TaskManager* task_manager, 
			CapacityStream<char>* error_message
		) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return ProcessFolderHierarchy(hierarchy_index, task_manager, error_message);
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		// It will deallocate the files
		bool ProcessFolderHierarchyImplementation(ReflectionManager* manager, unsigned int folder_index, CapacityStream<Stream<wchar_t>> files, CapacityStream<char>* error_message) {			
			ReflectionManagerParseStructuresThreadTaskData thread_data;

			constexpr size_t thread_memory = 5'000'000;
			// Paths that need to be searched will not be assigned here
			ECS_TEMP_ASCII_STRING(temp_error_message, 1024);
			if (error_message == nullptr) {
				error_message = &temp_error_message;
			}
			manager->InitializeParseThreadTaskData(thread_memory, files.size, thread_data, error_message);

			ConditionVariable condition_variable;
			thread_data.condition_variable = &condition_variable;
			// Assigning paths
			for (size_t path_index = 0; path_index < files.size; path_index++) {
				thread_data.paths[path_index] = files[path_index];
			}

			ReflectionManagerParseThreadTask(0, nullptr, &thread_data);
			bool success = thread_data.success;
			if (thread_data.success == true) {
				success = manager->BindApprovedData(&thread_data, 1, folder_index);
			}

			manager->DeallocateThreadTaskData(thread_data);
			for (size_t index = 0; index < files.size; index++) {
				Deallocate(manager->folders.allocator, files[index].buffer);
			}

			return success;
		}

		bool ReflectionManager::ProcessFolderHierarchy(unsigned int index, CapacityStream<char>* error_message) {
			AllocatorPolymorphic allocator = folders.allocator;
			Stream<wchar_t> c_file_extensions[] = {
				L".c",
				L".cpp",
				L".h",
				L".hpp"
			};

			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, files_storage, ECS_KB);
			AdditionStream<Stream<wchar_t>> files;
			files.is_capacity = true;
			files.capacity_stream = files_storage;
			bool status = GetDirectoryFilesWithExtensionRecursive(folders[index].root, allocator, files, { c_file_extensions, std::size(c_file_extensions) });
			if (!status) {
				return false;
			}

			return ProcessFolderHierarchyImplementation(this, index, files.capacity_stream, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(
			unsigned int folder_index, 
			TaskManager* task_manager, 
			CapacityStream<char>* error_message
		) {
			unsigned int thread_count = task_manager->GetThreadCount();

			AllocatorPolymorphic allocator = folders.allocator;
			Stream<wchar_t> c_file_extensions[] = {
				L".c",
				L".cpp",
				L".h",
				L".hpp"
			};

			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, files_storage, ECS_KB);
			AdditionStream<Stream<wchar_t>> files;
			files.is_capacity = true;
			files.capacity_stream = files_storage;
			bool status = GetDirectoryFilesWithExtensionRecursive(folders[folder_index].root, allocator, files, { c_file_extensions, std::size(c_file_extensions) });
			if (!status) {
				return false;
			}

			// Preparsing the files in order to have thread act only on files that actually need to process
			// Otherwise unequal distribution of files will result in poor multithreaded performance
			unsigned int files_count = files.Size();

			// Process these files on separate threads only if their number is greater than thread count
			if (files_count < thread_count) {
				return ProcessFolderHierarchyImplementation(this, folder_index, files.capacity_stream, error_message);
			}

			unsigned int* path_indices_buffer = (unsigned int*)Allocate(folders.allocator, sizeof(unsigned int) * files_count, alignof(unsigned int));
			AtomicStream<unsigned int> path_indices = AtomicStream<unsigned int>(path_indices_buffer, 0, files_count);

			// Calculate the thread start and thread end
			unsigned int reflect_per_thread_data = files_count / thread_count;
			unsigned int reflect_remainder_tasks = files_count % thread_count;

			// If there is more than a task per thread - launch all
			// If per thread data is 0, then launch as many threads as remainder
			unsigned int reflect_thread_count = reflect_per_thread_data == 0 ? reflect_remainder_tasks : thread_count;
			ReflectionManagerHasReflectStructuresThreadTaskData* reflect_thread_data = (ReflectionManagerHasReflectStructuresThreadTaskData*)ECS_STACK_ALLOC(
				sizeof(ReflectionManagerHasReflectStructuresThreadTaskData) * reflect_thread_count
			);
			Semaphore reflect_semaphore;
			reflect_semaphore.target.store(reflect_thread_count, ECS_RELAXED);

			unsigned int reflect_current_path_start = 0;
			for (size_t thread_index = 0; thread_index < reflect_thread_count; thread_index++) {
				bool has_remainder = reflect_remainder_tasks > 0;
				reflect_remainder_tasks -= has_remainder;
				unsigned int current_thread_start = reflect_current_path_start;
				unsigned int current_thread_end = reflect_current_path_start + reflect_per_thread_data + has_remainder;

				// Increase the total count of paths
				reflect_current_path_start += reflect_per_thread_data + has_remainder;

				reflect_thread_data[thread_index].ending_path_index = current_thread_end;
				reflect_thread_data[thread_index].starting_path_index = current_thread_start;
				reflect_thread_data[thread_index].folder_index = folder_index;
				reflect_thread_data[thread_index].reflection_manager = this;
				reflect_thread_data[thread_index].valid_paths = &path_indices;
				reflect_thread_data[thread_index].semaphore = &reflect_semaphore;
				reflect_thread_data[thread_index].files = files.capacity_stream;

				// Launch the task
				ThreadTask task = ECS_THREAD_TASK_NAME(ReflectionManagerHasReflectStructuresThreadTask, reflect_thread_data + thread_index, 0);
				task_manager->AddDynamicTaskAndWake(task);
			}

			reflect_semaphore.SpinWait();

			unsigned int parse_per_thread_paths = path_indices.size / thread_count;
			unsigned int parse_thread_paths_remainder = path_indices.size % thread_count;

			constexpr size_t thread_memory = 10'000'000;
			ConditionVariable condition_variable;

			ECS_TEMP_ASCII_STRING(temp_error_message, 1024);
			if (error_message == nullptr) {
				error_message = &temp_error_message;
			}

			unsigned int parse_thread_count = parse_per_thread_paths == 0 ? parse_thread_paths_remainder : thread_count;
			ReflectionManagerParseStructuresThreadTaskData* parse_thread_data = (ReflectionManagerParseStructuresThreadTaskData*)ECS_STACK_ALLOC(
				sizeof(ReflectionManagerParseStructuresThreadTaskData) * thread_count
			);

			unsigned int parse_thread_start = 0;
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				bool has_remainder = parse_thread_paths_remainder > 0;
				parse_thread_paths_remainder -= has_remainder;
				unsigned int thread_current_paths = parse_per_thread_paths + has_remainder;
				// initialize data with buffers
				InitializeParseThreadTaskData(thread_memory, thread_current_paths, parse_thread_data[thread_index], error_message);
				parse_thread_data[thread_index].condition_variable = &condition_variable;

				// Set thread paths
				for (size_t path_index = parse_thread_start; path_index < parse_thread_start + thread_current_paths; path_index++) {
					parse_thread_data[thread_index].paths[path_index - parse_thread_start] = files[path_indices[path_index]];
				}
				parse_thread_start += thread_current_paths;
			}

			condition_variable.Reset();

			// Push thread execution
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				ThreadTask task = ECS_THREAD_TASK_NAME(ReflectionManagerParseThreadTask, parse_thread_data + thread_index, 0);
				task_manager->AddDynamicTaskAndWake(task);
			}

			// Wait until threads are done
			condition_variable.Wait(parse_thread_count);

			bool success = true;
			
			size_t path_index = 0;
			// Check every thread for success and return the first error if there is such an error
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				if (parse_thread_data[thread_index].success == false) {
					success = false;
					break;
				}
				path_index += parse_thread_data[thread_index].paths.size;
			}

			if (success) {
				success = BindApprovedData(parse_thread_data, parse_thread_count, folder_index);
			}

			// Free the previously allocated memory
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				DeallocateThreadTaskData(parse_thread_data[thread_index]);
			}
			Deallocate(folders.allocator, path_indices_buffer);
			for (size_t index = 0; index < files.Size(); index++) {
				Deallocate(folders.allocator, files[index].buffer);
			}

			return success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::RemoveFolderHierarchy(unsigned int folder_index)
		{
			FreeFolderHierarchy(folder_index);
			folders.RemoveSwapBack(folder_index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::RemoveFolderHierarchy(Stream<wchar_t> root) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				RemoveFolderHierarchy(hierarchy_index);
				return;
			}
			// error if not found
			ECS_ASSERT(false);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::RemoveBlittableException(Stream<char> name)
		{
			unsigned int index = function::FindString(name, blittable_types.ToStream(), [](BlittableType type) {
				return type.name;
			});
			if (index != -1) {
				blittable_types.RemoveSwapBack(index);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(unsigned int folder_index, CapacityStream<char>* error_message)
		{
			FreeFolderHierarchy(folder_index);
			return ProcessFolderHierarchy(folder_index, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(unsigned int folder_index, TaskManager* task_manager, CapacityStream<char>* error_message)
		{
			FreeFolderHierarchy(folder_index);
			return ProcessFolderHierarchy(folder_index, task_manager, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(Stream<wchar_t> root, CapacityStream<char>* error_message) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return UpdateFolderHierarchy(hierarchy_index, error_message);
			}
			// Fail if it was not found previously
			ECS_ASSERT(false);
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(Stream<wchar_t> root, TaskManager* task_manager, CapacityStream<char>* error_message)
		{
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return UpdateFolderHierarchy(hierarchy_index, task_manager, error_message);
			}
			// Fail if it was not found previously
			ECS_ASSERT(false);
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::SetInstanceDefaultData(Stream<char> name, void* data) const
		{
			unsigned int type_index = type_definitions.Find(name);
			ECS_ASSERT(type_index != -1);
			SetInstanceDefaultData(type_index, data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::SetInstanceDefaultData(unsigned int index, void* data) const
		{
			const ReflectionType* type = GetType(index);
			SetInstanceDefaultData(type, data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::SetInstanceDefaultData(const ReflectionType* type, void* data) const
		{
			for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
				SetInstanceFieldDefaultData(&type->fields[field_index], data);
			}

			// Now go through all padding bytes and set them to 0
			for (size_t field_index = 0; field_index < type->fields.size - 1; field_index++) {
				size_t current_offset = type->fields[field_index].info.pointer_offset;
				size_t current_size = type->fields[field_index].info.byte_size;
				size_t next_offset = type->fields[field_index + 1].info.pointer_offset;
				size_t difference = next_offset - current_offset - current_size;
				if (difference > 0) {
					memset(function::OffsetPointer(data, current_offset + current_size), 0, difference);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::SetInstanceFieldDefaultData(const ReflectionField* field, void* data) const
		{
			void* current_field = function::OffsetPointer(data, field->info.pointer_offset);

			if (field->info.has_default_value) {
				memcpy(
					current_field,
					&field->info.default_bool,
					field->info.byte_size
				);
			}
			else {
				if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// Check to see if it exists
					ReflectionType nested_type;
					if (TryGetType(field->definition, nested_type)) {
						// Set the field to its default values
						SetInstanceDefaultData(field->definition, current_field);
					}
					else {
						// Might be a custom type. These should be fine with memsetting to 0
						memset(current_field, 0, field->info.byte_size);
					}
				}
				else {
					// Memsetting to 0 should be fine for all the other cases (streams, pointers,
					// basic type arrays, or enums)
					memset(current_field, 0, field->info.byte_size);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::SetTypeByteSize(ReflectionType* type, size_t byte_size)
		{
			type->byte_size = byte_size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::SetTypeAlignment(ReflectionType* type, size_t alignment)
		{
			type->alignment = alignment;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SetInstanceFieldDefaultData(const ReflectionField* field, void* data)
		{
			void* current_field = function::OffsetPointer(data, field->info.pointer_offset);

			if (field->info.has_default_value) {
				memcpy(
					current_field,
					&field->info.default_bool,
					field->info.byte_size
				);
			}
			else {
				ECS_ASSERT(field->info.basic_type != ReflectionBasicFieldType::UserDefined);
				// Memsetting to 0 should be fine for all the other cases (streams, pointers,
				// basic type arrays, or enums)
				memset(current_field, 0, field->info.byte_size);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsTypeCharacter(char character)
		{
			if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') 
				|| character == '_' || character == '<' || character == '>') {
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t PtrDifference(const void* start, const void* end)
		{
			return (uintptr_t)end - (uintptr_t)start;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManagerHasReflectStructuresThreadTask(unsigned int thread_id, World* world, void* _data) {
			ReflectionManagerHasReflectStructuresThreadTaskData* data = (ReflectionManagerHasReflectStructuresThreadTaskData*)_data;
			for (unsigned int path_index = data->starting_path_index; path_index < data->ending_path_index; path_index++) {
				bool has_reflect = HasReflectStructures(data->files[path_index]);
				if (has_reflect) {
					data->valid_paths->Add(path_index);
				}
			}

			data->semaphore->Enter();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* _data) {
			ReflectionManagerParseStructuresThreadTaskData* data = (ReflectionManagerParseStructuresThreadTaskData*)_data;

			ECS_STACK_CAPACITY_STREAM(unsigned int, words, 1024);

			size_t ecs_reflect_size = strlen(STRING(ECS_REFLECT));
			const size_t MAX_FIRST_LINE_CHARACTERS = 512;
			char first_line_characters[512];

			// search every path
			for (size_t index = 0; index < data->paths.size; index++) {
				ECS_FILE_HANDLE file = 0;
				// open the file from the beginning
				ECS_FILE_STATUS_FLAGS status = OpenFile(data->paths[index], &file, ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_READ_ONLY);

				// if access was granted
				if (status == ECS_FILE_STATUS_OK) {
					ScopedFile scoped_file({ file });

					const size_t MAX_FIRST_LINE_CHARACTERS = 512;
					// Assume that the first line is at most MAX_FIRST_LINE characters
					unsigned int first_line_count = ReadFromFile(file, { first_line_characters, MAX_FIRST_LINE_CHARACTERS });
					if (first_line_count == -1) {
						WriteErrorMessage(data, "Could not read first line. Faulty path: ", index);
						return;
					}
					first_line_characters[first_line_count - 1] = '\0';

					// read the first line in order to check if there is something to be reflected	
					// if there is something to be reflected
					const char* first_new_line = strchr(first_line_characters, '\n');
					const char* has_reflect = strstr(first_line_characters, STRING(ECS_REFLECT));
					if (first_new_line != nullptr && has_reflect != nullptr && has_reflect < first_new_line) {
						// reset words stream
						words.size = 0;

						size_t file_size = GetFileByteSize(file);
						if (file_size == 0) {
							WriteErrorMessage(data, "Could not deduce file size. Faulty path: ", index);
							return;
						}
						// Reset the file pointer and read again
						SetFileCursor(file, 0, ECS_FILE_SEEK_BEG);

						char* file_contents = data->thread_memory.buffer + data->thread_memory.size;
						unsigned int bytes_read = ReadFromFile(file, { file_contents, file_size });
						Stream<char> content = { file_contents, bytes_read };

						// Eliminate all comments
						content = function::RemoveSingleLineComment(content, ECS_C_FILE_SINGLE_LINE_COMMENT_TOKEN);
						content = function::RemoveMultiLineComments(content, ECS_C_FILE_MULTI_LINE_COMMENT_OPENED_TOKEN, ECS_C_FILE_MULTI_LINE_COMMENT_CLOSED_TOKEN);

						bytes_read = content.size;
						file_contents[bytes_read] = '\0';
						
						// Add one for the null terminator
						data->thread_memory.size += bytes_read + 1;
						// if there's not enough memory, fail
						if (data->thread_memory.size > data->thread_memory.capacity) {
							WriteErrorMessage(data, "Not enough memory to read file contents. Faulty path: ", index);
							return;
						}

						// Skip the first line - it contains the flag ECS_REFLECT
						const char* new_line = strchr(file_contents, '\n');
						if (new_line == nullptr) {
							WriteErrorMessage(data, "Could not skip ECS_REFLECT flag on first line. Faulty path: ", index);
							return;
						}
						file_contents = (char*)new_line + 1;

						Stream<char> file_stream = { file_contents, bytes_read };

						// Get the constants first
						ECS_STACK_CAPACITY_STREAM(unsigned int, constant_offsets, 64);
						function::FindToken(file_stream, STRING(ECS_CONSTANT_REFLECT), constant_offsets);

						for (size_t constant_index = 0; constant_index < constant_offsets.size; constant_index++) {
							// Look to see that it is a define
							Stream<char> search_space = { file_stream.buffer, constant_offsets[constant_index] };
							const char* last_line = function::FindCharacterReverse(search_space, '\n').buffer;
							if (last_line == nullptr) {
								// Fail if no new line before it
								WriteErrorMessage(data, "Could not find new line before ECS_CONSTANT_REFLECT. Faulty path: ", index);
								return;
							}

							last_line = function::SkipWhitespace(last_line + 1);
							// Only if this is a define continue
							if (memcmp(last_line, "#define", sizeof("#define") - 1) == 0) {
								// Get the macro name of the constant
								last_line += sizeof("#define");
								last_line = function::SkipWhitespace(last_line);

								const char* constant_name_start = last_line;
								const char* constant_name_end = function::SkipCodeIdentifier(constant_name_start);
								ReflectionConstant constant;
								constant.name = { constant_name_start, function::PointerDifference(constant_name_end, constant_name_start) };
								
								// Get the value now
								const char* ecs_constant_macro_start = function::SkipWhitespace(constant_name_end);
								if (ecs_constant_macro_start != file_stream.buffer + constant_offsets[constant_index]) {
									// Fail if the macro definition does not contain the constant reflect
									WriteErrorMessage(data, "ECS_CONSTANT_REFLECT definition fail. The macro could not be found after the macro name. Faulty path: ", index);
									return;
								}

								const char* opened_parenthese = strchr(ecs_constant_macro_start, '(');
								if (opened_parenthese == nullptr) {
									WriteErrorMessage(data, "ECS_CONSTANT_REFLECT is missing opened parenthese. Faulty path: ", index);
									return;
								}
								const char* closed_parenthese = strchr(opened_parenthese, ')');
								if (closed_parenthese == nullptr) {
									WriteErrorMessage(data, "ECS_CONSTANT_REFLECT is missing closed parenthese. Faulty path: ", index);
									return;
								}

								opened_parenthese = function::SkipWhitespace(opened_parenthese + 1);
								closed_parenthese = function::SkipWhitespace(closed_parenthese - 1, -1);

								Stream<char> value_characters(opened_parenthese, function::PointerDifference(closed_parenthese, opened_parenthese) + 1);
								// Parse the value now - as a double or a boolean
								char is_boolean = function::ConvertCharactersToBool(value_characters);
								if (is_boolean == -1) {
									// Not a boolean, parse as double
									constant.value = function::ConvertCharactersToDouble(value_characters);
								}
								else {
									constant.value = is_boolean;
								}

								data->constants.Add(constant);
								// Don't forget to update the total memory needed for the folder hierarchy
								data->total_memory += constant.name.size;
							}
							else {
								// Error, no define
								WriteErrorMessage(data, "Could not find #define before ECS_CONSTANT_REFLECT. Faulty path: ", index);
								return;
							}
						}

						// search for more ECS_REFLECTs 
						function::FindToken(file_stream, STRING(ECS_REFLECT), words);

						const char* tag_name = nullptr;

						size_t last_position = 0;
						for (size_t word_index = 0; word_index < words.size; word_index++) {
							tag_name = nullptr;

							unsigned int word_offset = words[word_index];

#pragma region Has macro definitions
							// Skip macro definitions - that define tags
							const char* verify_define_char = file_contents + word_offset - 1;
							verify_define_char = function::SkipWhitespace(verify_define_char, -1);
							verify_define_char = function::SkipCodeIdentifier(verify_define_char, -1);
							// If it is a pound, it must be a definition - skip it
							if (*verify_define_char == '#') {
								continue;
							}
#pragma endregion

#pragma region Has Comments
							// Skip comments that contain ECS_REFLECT
							//const char* verify_comment_char = file_contents + word_offset - 1;
							//verify_comment_char = function::SkipWhitespace(verify_comment_char, -1);

							//Stream<char> comment_space = { file_contents, function::PointerDifference(verify_comment_char, file_contents) + 1 };
							//const char* verify_comment_last_new_line = function::FindCharacterReverse(comment_space, '\n').buffer;
							//if (verify_comment_last_new_line == nullptr) {
							//	WriteErrorMessage(data, "Last new line could not be found when checking ECS_REFLECT for comment", index);
							//	return;
							//}
							//const char* comment_char = function::FindCharacterReverse(comment_space, '/').buffer;
							//if (comment_char != nullptr && comment_char > verify_comment_last_new_line) {
							//	// It might be a comment
							//	if (comment_char[-1] == '/') {
							//		// It is a comment - skip it
							//		continue;
							//	}
							//	else if (comment_char[1] == '*') {
							//		// It is a comment skip it
							//		continue;
							//	}
							//}
#pragma endregion

							const char* ecs_reflect_end_position = file_contents + word_offset + ecs_reflect_size;

							// get the type name
							const char* space = strchr(ecs_reflect_end_position, ' ');
							// The reflection is tagged, record it for later on
							if (space != ecs_reflect_end_position) {
								tag_name = ecs_reflect_end_position[0] == '_' ? ecs_reflect_end_position + 1 : ecs_reflect_end_position;
							}

							// if no space was found after the token, fail
							if (space == nullptr) {
								WriteErrorMessage(data, "Finding type leading space failed. Faulty path: ", index);
								return;
							}
							space++;

							const char* second_space = strchr(space, ' ');

							// if the second space was not found, fail
							if (second_space == nullptr) {
								WriteErrorMessage(data, "Finding type final space failed. Faulty path: ", index);
								return;
							}

							// null terminate 
							char* second_space_mutable = (char*)(second_space);
							*second_space_mutable = '\0';
							data->total_memory += PtrDifference(space, second_space_mutable) + 1;

							file_contents[word_offset] = '\0';
							// find the last new line character in order to speed up processing
							const char* last_new_line = strrchr(file_contents + last_position, '\n');

							// if it failed, set it to the start of the processing block
							last_new_line = last_new_line == nullptr ? file_contents + last_position : last_new_line;

							const char* opening_parenthese = strchr(second_space + 1, '{');
							// if no opening curly brace was found, fail
							if (opening_parenthese == nullptr) {
								WriteErrorMessage(data, "Finding opening curly brace failed. Faulty path: ", index);
								return;
							}

							const char* closing_parenthese = strchr(opening_parenthese + 1, '}');
							// if no closing curly brace was found, fail
							if (closing_parenthese == nullptr) {
								WriteErrorMessage(data, "Finding closing curly brace failed. Faulty path: ", index);
								return;
							}

							last_position = PtrDifference(file_contents, closing_parenthese + 1);

							// enum declaration
							const char* enum_ptr = strstr(last_new_line + 1, "enum");
							if (enum_ptr != nullptr) {
								AddEnumDefinition(data, opening_parenthese, closing_parenthese, space, index);
								if (data->success == false) {
									return;
								}
							}
							else {
								closing_parenthese = function::FindMatchingParenthesis(opening_parenthese + 1, file_contents + bytes_read, '{', '}');
								if (closing_parenthese == nullptr) {
									WriteErrorMessage(data, "Finding struct or class closing brace failed. Faulty path: ", index);
									return;
								}

								// Update the last position
								last_position = PtrDifference(file_contents, closing_parenthese + 1);

								// type definition
								const char* struct_ptr = strstr(last_new_line + 1, "struct");
								const char* class_ptr = strstr(last_new_line + 1, "class");

								// if none found, fail
								if (struct_ptr == nullptr && class_ptr == nullptr) {
									WriteErrorMessage(data, "Enum and type definition validation failed, didn't find neither", index);
									return;
								}

								// Check to see if it is a tagged type with a preparsing handler
								const char* type_opening_parenthese = opening_parenthese;
								const char* type_closing_parenthese = closing_parenthese;
								const char* type_name = space;

								if (tag_name != nullptr) {
									const char* end_tag_name = function::SkipCodeIdentifier(tag_name);
									Stream<char> current_tag = { tag_name, function::PointerDifference(end_tag_name, tag_name) };
									for (size_t handler_index = 0; handler_index < std::size(ECS_REFLECTION_TYPE_TAG_HANDLER); handler_index++) {
										if (ECS_REFLECTION_TYPE_TAG_HANDLER[handler_index].tag == current_tag) {
											Stream<char> new_range = ProcessTypeTagHandler(data, handler_index, Stream<char>(
												opening_parenthese,
												function::PointerDifference(closing_parenthese, opening_parenthese))
											);
											if (new_range.size == 0) {
												ECS_FORMAT_TEMP_STRING(message, "Failed to preparse the type {#} with the known tag {#}", type_name, current_tag);
												WriteErrorMessage(data, message.buffer, index);
												return;
											}
											type_opening_parenthese = new_range.buffer;
											type_closing_parenthese = new_range.buffer + new_range.size - 1;
											break;
										}
									}
								}

								AddTypeDefinition(data, type_opening_parenthese, type_closing_parenthese, type_name, index);
								if (data->success == false) {
									return;
								}
								else if (tag_name != nullptr) {
									const char* end_tag_name = function::SkipCodeIdentifier(tag_name);

									if (*end_tag_name == '(') {
										// variable type macro
										end_tag_name = function::SkipCodeIdentifier(end_tag_name + 1);
										if (*end_tag_name == ')') {
											end_tag_name++;
										}
										else {
											// Return, invalid variable type macro
											WriteErrorMessage(data, "Invalid type tag macro, no closing parenthese found. Path index: ", index);
											return;
										}
									}

									char* new_tag_name = data->thread_memory.buffer + data->thread_memory.size;
									size_t string_size = PtrDifference(tag_name, end_tag_name);
									data->thread_memory.size += string_size + 1;

									// Record its tag
									data->total_memory += (string_size + 1) * sizeof(char);
									memcpy(new_tag_name, tag_name, sizeof(char) * string_size);
									new_tag_name[string_size] = '\0';
									data->types[data->types.size - 1].tag = new_tag_name;
								}
								else {
									data->types[data->types.size - 1].tag = { nullptr, 0 };
								}
							}
						}
					}
				}
			}

			data->condition_variable->Notify();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionFieldInfo ExtendedTypeStringToFieldInfo(ReflectionManager* reflection, const char* extended_type_string)
		{
			ReflectionFieldInfo info;

			ResourceIdentifier identifier(extended_type_string);
			bool success = reflection->field_table.TryGetValue(identifier, info);
			ECS_ASSERT(success, "Invalid type_string");

			return info;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManagerStylizeEnum(ReflectionEnum& enum_) {
			ECS_STACK_CAPACITY_STREAM(unsigned int, underscores, 128);
			function::FindToken(enum_.fields[0], '_', underscores);

			for (int64_t index = (int64_t)underscores.size - 1; index >= 0; index--) {
				size_t is_the_same_count = 0;
				for (; is_the_same_count < enum_.fields.size; is_the_same_count++) {
					if (memcmp(enum_.fields[is_the_same_count].buffer, enum_.fields[0].buffer, underscores[index] - 1) != 0) {
						break;
					}
				}

				if (is_the_same_count == enum_.fields.size) {
					// This is a prefix - eliminate it
					for (size_t subindex = 0; subindex < enum_.fields.size; subindex++) {
						enum_.fields[subindex].buffer += underscores[index] + 1;
						enum_.fields[subindex].size -= underscores[index] + 1;
					}
					break;
				}
			}

			// If the last value is contains COUNT discard it
			if (function::FindFirstToken(enum_.fields[enum_.fields.size - 1], "COUNT").size > 0) {
				enum_.fields.size--;
			}

			// If the values are in caps, capitalize every word
			size_t index = 0;
			for (; index < enum_.fields.size; index++) {
				size_t subindex = 0;
				for (; subindex < enum_.fields[index].size; subindex++) {
					if (enum_.fields[index][subindex] >= 'a' && enum_.fields[index][subindex] <= 'z') {
						break;
					}
				}

				if (subindex != enum_.fields[index].size) {
					break;
				}
			}

			if (index == enum_.fields.size) {
				// All are with caps.
				// Get all the _ positions that are left. If it contains a digit (like in 3D)
				// keep the letters capitalized in that section
				for (index = 0; index < enum_.fields.size; index++) {
					ECS_STACK_CAPACITY_STREAM(unsigned int, current_underscores, 32);
					function::FindToken(enum_.fields[index], '_', current_underscores);

					unsigned int last_underscore = -1;
					ECS_STACK_CAPACITY_STREAM(Stream<char>, partitions, 32);
					for (size_t subindex = 0; subindex < current_underscores.size; subindex++) {
						// Replace the under score with a space
						enum_.fields[index][current_underscores[subindex]] = ' ';
						last_underscore++;
						partitions.Add({ enum_.fields[index].buffer + last_underscore, current_underscores[subindex] - last_underscore });
						last_underscore = current_underscores[subindex];
					}

					last_underscore++;
					partitions.Add({ enum_.fields[index].buffer + last_underscore, enum_.fields[index].size - last_underscore });

					for (size_t partition_index = 0; partition_index < partitions.size; partition_index++) {
						bool lacks_digits = true;
						for (size_t subindex = 0; lacks_digits && subindex < partitions[partition_index].size; subindex++) {
							if (function::IsNumberCharacter(partitions[partition_index][subindex])) {
								lacks_digits = false;
							}
						}

						if (lacks_digits) {
							for (size_t subindex = 1; subindex < partitions[partition_index].size; subindex++) {
								function::Uncapitalize(partitions[partition_index].buffer + subindex);
							}
						}
					}
				}
			}
		}

		void AddEnumDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			const char* opening_parenthese, 
			const char* closing_parenthese,
			const char* name,
			unsigned int index
		)
		{
			ReflectionEnum enum_definition;
			enum_definition.name = name;

			// find next line tokens and exclude the next after the opening paranthese and replace
			// the closing paranthese with \0 in order to stop searching there
			ECS_STACK_CAPACITY_STREAM(unsigned int, next_line_positions, 1024);

			const char* dummy_space = strchr(opening_parenthese, '\n') + 1;

			char* closing_paranthese_mutable = (char*)closing_parenthese;
			*closing_paranthese_mutable = '\0';
			function::FindToken({ dummy_space, function::PointerDifference(closing_parenthese, dummy_space) }, '\n', next_line_positions);

			// assign the subname stream and assert enough memory to hold the pointers
			uintptr_t ptr = (uintptr_t)data->thread_memory.buffer + data->thread_memory.size;
			uintptr_t before_ptr = ptr;
			ptr = function::AlignPointer(ptr, alignof(ReflectionEnum));
			enum_definition.original_fields = Stream<Stream<char>>((void*)ptr, 0);

			size_t memory_size = sizeof(Stream<char>) * next_line_positions.size + alignof(ReflectionEnum);
			// We need to double this since we are doing stylized fields and original fields
			memory_size *= 2;
			data->thread_memory.size += memory_size;
			data->total_memory += memory_size;
			if (data->thread_memory.size > data->thread_memory.capacity) {
				WriteErrorMessage(data, "Not enough memory to assign enum definition stream", index);
				return;
			}

			for (size_t next_line_index = 0; next_line_index < next_line_positions.size; next_line_index++) {
				const char* current_character = dummy_space + next_line_positions[next_line_index];
				while (!IsTypeCharacter(*current_character)) {
					current_character--;
				}

				char* final_character = (char*)(current_character + 1);
				// null terminate the type
				*final_character = '\0';
				while (IsTypeCharacter(*current_character)) {
					current_character--;
				}

				enum_definition.original_fields.Add(current_character + 1);
				// We multiply by 2 since we have original fields and stylized fields
				data->total_memory += PtrDifference(current_character + 1, final_character) * 2;
			}

			data->enums.Add(enum_definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void AddTypeDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			const char* opening_parenthese,
			const char* closing_parenthese, 
			const char* name,
			unsigned int file_index
		)
		{
			ReflectionType type;
			type.name = name;
			type.byte_size = 0;

			// find next line tokens and exclude the next after the opening paranthese and replace
			// the closing paranthese with \0 in order to stop searching there
			ECS_STACK_CAPACITY_STREAM(unsigned int, next_line_positions, 1024);

			// semicolon positions will help in getting the field name
			ECS_STACK_CAPACITY_STREAM(unsigned int, semicolon_positions, 512);

			opening_parenthese++;
			char* closing_paranthese_mutable = (char*)closing_parenthese;
			*closing_paranthese_mutable = '\0';

			// Check all expression evaluations
			Stream<char> type_body = { opening_parenthese, function::PointerDifference(closing_parenthese, opening_parenthese) };
			Stream<char> evaluate_expression_token = function::FindFirstToken(type_body, STRING(ECS_EVALUATE_FUNCTION_REFLECT));
			while (evaluate_expression_token.buffer != nullptr) {
				// Go down the line to get the name
				// Its right before the parenthesis
				const char* function_parenthese = function::FindFirstCharacter(evaluate_expression_token, '(').buffer;
				const char* end_function_name = function::SkipWhitespace(function_parenthese - 1, -1);
				const char* start_function_name = function::SkipCodeIdentifier(end_function_name, -1) + 1;

				ReflectionExpression expression;
				expression.name = { start_function_name, function::PointerDifference(end_function_name, start_function_name) + 1 };

				// Get the body now - look for the return
				const char* return_string = function::FindFirstToken(evaluate_expression_token, "return").buffer;
				ECS_ASSERT(return_string != nullptr);
				return_string += sizeof("return") - 1;

				// Everything until the semicolon is the body
				const char* semicolon = strchr(return_string, ';');
				ECS_ASSERT(semicolon != nullptr);

				// The removal of '\n' will be done when binding the data, for now just reference this part of the code
				expression.body = { return_string, function::PointerDifference(semicolon, return_string) };
				unsigned int type_table_index = data->expressions.Find(type.name);
				if (type_table_index == -1) {
					// Insert it
					ReflectionExpression* stable_expression = (ReflectionExpression*)function::OffsetPointer(data->thread_memory.buffer, data->thread_memory.size);
					*stable_expression = expression;
					data->thread_memory.size += sizeof(ReflectionExpression);

					data->expressions.Insert({ stable_expression, 1 }, type.name);
				}
				else {
					// Allocate a new buffer and update the expressions
					ReflectionExpression* expressions = (ReflectionExpression*)function::OffsetPointer(data->thread_memory.buffer, data->thread_memory.size);
					Stream<ReflectionExpression>* previous_expressions = data->expressions.GetValuePtrFromIndex(type_table_index);
					previous_expressions->CopyTo(expressions);

					data->thread_memory.size += sizeof(ReflectionExpression) * (previous_expressions->size + 1);
					expressions[previous_expressions->size] = expression;

					previous_expressions->buffer = expressions;
					previous_expressions->size++;
				}

				// Get the next expression
				Stream<char> remaining_part = { semicolon, function::PointerDifference(closing_parenthese, semicolon) };
				evaluate_expression_token = function::FindFirstToken(remaining_part, STRING(ECS_EVALUATE_FUNCTION_REFLECT));

				// Don't forget to update the total memory needed for the folder hierarchy
				// The slot in the stream + the string for the name (the body doesn't need to be copied since 
				// it will be evaluated and then discarded)
				data->total_memory += expression.name.size + sizeof(ReflectionEvaluation);
			}

			bool omitted_fields = false;

			// Returns false if an error has happened, else true.
			// The fields will be added directly into the type on the stack
			auto get_fields_from_section = [&](const char* start) {
				next_line_positions.size = 0;
				semicolon_positions.size = 0;

				size_t start_size = strlen(start);

				// Get all the new lines in between the two macros
				function::FindToken({ start, start_size }, '\n', next_line_positions);

				// Get all the semicolons
				function::FindToken({ start, start_size }, ';', semicolon_positions);

				// Process the inline function declarations such that they get rejected.
				// Do this by removing the semicolons inside them
				const char* function_body_start = strchr(start, '{');
				const char* closed_bracket = nullptr;


				while (function_body_start != nullptr) {
					// Get the first character before the bracket. If it is ), then we have a function declaration
					const char* first_character_before = function_body_start - 1;
					while (first_character_before >= start && (*first_character_before == ' ' || *first_character_before == '\t' || *first_character_before == '\n')) {
						first_character_before--;
					}

					if (first_character_before < start) {
						WriteErrorMessage(data, "Type contains brace tokens {} in an unexpected fashion. Faulty path: ", file_index);
						return false;
					}

					// Check for const placed before { and after ()
					if (*first_character_before == 't') {
						while (first_character_before >= start && function::IsCodeIdentifierCharacter(*first_character_before)) {
							first_character_before--;
						}
						first_character_before++;

						if (memcmp(first_character_before, STRING(const), sizeof(STRING(const)) - 1) == 0) {
							// We have a const modifier. Exclude it
							first_character_before = function::SkipWhitespace(first_character_before - 1, -1);
							if (first_character_before < start) {
								// An error has happened, fail
								WriteErrorMessage(data, "Possible inline function declaration with const contains unexpected {} tokens. Faulty path: ", file_index);
								return false;
							}
						}
					}

					if (*first_character_before == ')' || *first_character_before == 't') {
						// Remove all the semicolons in between the two declarations
						unsigned int opened_bracket_count = 0;
						const char* opened_bracket = function_body_start;
						closed_bracket = strchr(opened_bracket + 1, '}');

						if (closed_bracket == nullptr) {
							// Fail. Invalid match of brackets
							WriteErrorMessage(data, "Determining inline function declaration body failed. Invalid mismatch of brackets.", file_index);
							return false;
						}

						auto get_opened_bracket_count = [&opened_bracket_count](Stream<char> search_space) {
							Stream<char> new_opened_bracket = function::FindFirstCharacter(search_space, '{');
							while (new_opened_bracket.buffer != nullptr) {
								opened_bracket_count++;
								new_opened_bracket.buffer += 1;
								new_opened_bracket.size -= 1;
								new_opened_bracket = function::FindFirstCharacter(new_opened_bracket, '{');
							}
						};

						Stream<char> search_space = { opened_bracket + 1, function::PointerDifference(closed_bracket, opened_bracket) - 1 };
						get_opened_bracket_count(search_space);

						// Now search for more '}'
						while (opened_bracket_count > 0) {
							opened_bracket_count--;

							const char* old_closed_bracket = closed_bracket;
							opened_bracket = strchr(closed_bracket + 1, '{');
							closed_bracket = strchr(closed_bracket + 1, '}');
							if (closed_bracket == nullptr) {
								// Fail. Invalid match of brackets
								WriteErrorMessage(data, "Determining inline function declaration body failed. Invalid mismatch of brackets.", file_index);
								return false;
							}
							
							if (old_closed_bracket < opened_bracket && opened_bracket < closed_bracket) {
								search_space = { opened_bracket + 1, function::PointerDifference(closed_bracket, opened_bracket) - 1 };
								get_opened_bracket_count(search_space);
							}
						}

						// Now remove the semicolons and new lines in between
						unsigned int body_start_offset = function_body_start - start;
						unsigned int body_end_offset = closed_bracket - start;
						for (unsigned int index = 0; index < semicolon_positions.size; index++) {
							if (body_start_offset < semicolon_positions[index] && semicolon_positions[index] < body_end_offset) {
								semicolon_positions.Remove(index);
								index--;
							}
						}
						for (unsigned int index = 0; index < next_line_positions.size; index++) {
							if (body_start_offset < next_line_positions[index] && next_line_positions[index] < body_end_offset) {
								next_line_positions.Remove(index);
								index--;
							}
						}

						function_body_start = strchr(closed_bracket + 1, '{');
					}
					else {
						// It can be a pair of {} for default value declaration.
						// Look for an equals before. If it is found on the same line, then we have a default value
						const char* previous_equals = function_body_start;
						if (closed_bracket != nullptr) {
							previous_equals = closed_bracket + 1;
							while (previous_equals < function_body_start && *previous_equals != '=') {
								previous_equals++;
							}
						}

						if (previous_equals != function_body_start) {
							// Possible default value if it has a closing }
							closed_bracket = strchr(function_body_start + 1, '}');
							if (closed_bracket == nullptr) {
								// Error. Unidentified meaning of {
								WriteErrorMessage(data, "Unrecognized meaning of { inside type definition.", file_index);
								return false;
							}
							function_body_start = strchr(closed_bracket + 1, '{');
						}
						else {
							// Error. Unidentified meaning of {
							WriteErrorMessage(data, "Unrecognized meaning of { inside type definition.", file_index);
							return false;
						}
					}
				}

				// assigning the field stream
				uintptr_t ptr = (uintptr_t)data->thread_memory.buffer + data->thread_memory.size;
				uintptr_t ptr_before = ptr;
				ptr = function::AlignPointer(ptr, alignof(ReflectionField));
				type.fields = Stream<ReflectionField>((void*)ptr, 0);
				data->thread_memory.size += sizeof(ReflectionField) * semicolon_positions.size + alignof(ReflectionField);
				data->total_memory += sizeof(ReflectionField) * semicolon_positions.size + alignof(ReflectionField);
				if (data->thread_memory.size > data->thread_memory.capacity) {
					WriteErrorMessage(data, "Assigning type field stream failed, insufficient memory.", file_index);
					return false;
				}

				if (next_line_positions.size > 0 && semicolon_positions.size > 0) {
					// determining each field
					unsigned short pointer_offset = 0;
					unsigned int last_new_line = next_line_positions[0];

					unsigned int current_semicolon_index = 0;
					for (size_t index = 0; index < next_line_positions.size - 1 && current_semicolon_index < semicolon_positions.size; index++) {
						// Check to see if a semicolon is in between the two new lines - if it is, then a definition might exist
						// for a data member or for a member function
						while (current_semicolon_index < semicolon_positions.size && semicolon_positions[current_semicolon_index] < last_new_line) {
							current_semicolon_index++;
						}
						// Exit if we have no more semicolons that fit the blocks
						if (current_semicolon_index >= semicolon_positions.size) {
							break;
						}

						unsigned int current_new_line = next_line_positions[index + 1];
						if (semicolon_positions[current_semicolon_index] > last_new_line && semicolon_positions[current_semicolon_index] < current_new_line) {
							const char* field_start = start + next_line_positions[index];
							char* field_end = (char*)start + semicolon_positions[current_semicolon_index];

							field_end[0] = '\0';

							// Must check if it is a function definition - if it is then skip it
							const char* opened_paranthese = strchr(field_start, '(');
							field_end[0] = ';';

							if (opened_paranthese == nullptr) {
								ECS_REFLECTION_ADD_TYPE_FIELD_RESULT result = AddTypeField(data, type, pointer_offset, field_start, field_end);

								if (result == ECS_REFLECTION_ADD_TYPE_FIELD_FAILED) {
									WriteErrorMessage(data, "An error occured during field type determination.", file_index);
									return false;
								}
								else if (result == ECS_REFLECTION_ADD_TYPE_FIELD_OMITTED) {
									omitted_fields = true;
								}
							}
							current_semicolon_index++;
						}
						last_new_line = current_new_line;
					}
				}
				return true;
			};

			// Look for field_start and field_end macros
			const char* field_start_macro = function::FindFirstToken(type_body, STRING(ECS_FIELDS_START_REFLECT)).buffer;
			if (field_start_macro != nullptr) {
				while (field_start_macro != nullptr) {
					// Get the fields end macro if there is one
					char* field_end_macro = (char*)function::FindFirstToken(
						Stream<char>(field_start_macro, function::PointerDifference(closing_parenthese, field_start_macro)), 
						STRING(ECS_FIELDS_END_REFLECT)
					).buffer;
					if (field_end_macro != nullptr) {
						// Put a '\0' before the macro - it will stop all the searches there
						field_end_macro[-1] = '\0';
					}

					bool success = get_fields_from_section(field_start_macro);
					if (!success) {
						return;
					}

					// Advance to the next
					if (field_end_macro != nullptr) {
						field_start_macro = function::FindFirstToken(
							Stream<char>(field_end_macro, function::PointerDifference(closing_parenthese, field_end_macro)), 
							STRING(ECS_FIELDS_START_REFLECT)
						).buffer;
					}
					else {
						// There was no end macro specified, we searched everything
						field_start_macro = nullptr;
					}
				}
			}
			else {
				bool success = get_fields_from_section(opening_parenthese);
				if (!success) {
					return;
				}
			}

			// Fail if there were no fields to be reflected
			if (type.fields.size == 0 && !omitted_fields) {
				WriteErrorMessage(data, "There were no fields to reflect: ", file_index);
				return;
			}

			data->types.Add(type);
			data->total_memory += sizeof(ReflectionType);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_ADD_TYPE_FIELD_RESULT AddTypeField(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* last_line_character, 
			const char* semicolon_character
		)
		{
			// Check to see if the field has a tag - all tags must appear after the semicolon
			// Make the tag default to nullptr
			type.fields[type.fields.size].tag = { nullptr, 0 };

			const char* tag_character = semicolon_character + 1;
			const char* next_line_character = strchr(tag_character, '\n');
			if (next_line_character == nullptr) {
				return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
			}
			const char* parsed_tag_character = function::SkipWhitespace(tag_character);
			// Some field determination functions write the field without knowing the tag. When the function exits, set the tag accordingly to 
			// what was determined here
			const char* final_field_tag = nullptr;
			// If they are the same, no tag
			if (parsed_tag_character != next_line_character) {
				// Check comments at the end of the line
				if (parsed_tag_character[0] != '/' || (parsed_tag_character[1] != '/' && parsed_tag_character[1] != '*')) {
					// Look for tag - there can be more tags separated by a space - change the space into a
					// ~ when writing the tag
					
					// A special case to handle is the ECS_SKIP_REFLECTION
					if (memcmp(parsed_tag_character, STRING(ECS_SKIP_REFLECTION), sizeof(STRING(ECS_SKIP_REFLECTION)) - 1) == 0) {
						// If it doesn't have an argument, then try to deduce the byte size
						Stream<char> argument_range = { parsed_tag_character, function::PointerDifference(next_line_character, parsed_tag_character) / sizeof(char) + 1 };
						Stream<char> parenthese = function::FindFirstCharacter(argument_range, '(');
						if (parenthese.size == 0) {
							return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
						}

						Stream<char> end_parenthese = function::FindMatchingParenthesis(parenthese.AdvanceReturn(), '(', ')');
						if (end_parenthese.size == 0) {
							return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
						}

						// We will forward the deduction at the end.
						size_t field_index = type.fields.size;
						type.fields[field_index].tag = { parsed_tag_character, function::PointerDifference(end_parenthese.buffer, parsed_tag_character) / sizeof(char) + 1 };
						const char* definition_start = function::SkipWhitespaceEx(last_line_character);
						// Skip the namespace qualification
						const char* colons = strstr(definition_start, "::");
						while (colons != nullptr && colons < semicolon_character) {
							definition_start = function::SkipWhitespace(colons + 2);
							colons = strstr(definition_start, "::");
						}

						const char* definition_end = function::SkipCodeIdentifier(definition_start);
						// Include the pointer asterisk
						while (*definition_end == '*') {
							definition_end++;
						}
						type.fields[field_index].definition = { definition_start, function::PointerDifference(definition_end, definition_start) };
						type.fields.size++;
						return ECS_REFLECTION_ADD_TYPE_FIELD_OMITTED;
					}

					char* ending_tag_character = (char*)function::SkipCodeIdentifier(parsed_tag_character);
					char* skipped_ending_tag_character = (char*)function::SkipWhitespace(ending_tag_character);

					// If there are more tags separated by spaces, then transform the spaces or the tabs into
					// ~ when writing the tag
					while (skipped_ending_tag_character != next_line_character) {
						if (*skipped_ending_tag_character == '(') {
							// Parameter macro - find all opened pairs
							unsigned int opened_count = 1;
							const char* last_opened = skipped_ending_tag_character;
							const char* last_closed = skipped_ending_tag_character;
							while (opened_count > 0) {
								// Search for ')' and '('
								const char* opened = strchr(last_opened + 1, '(');
								if (opened != nullptr && opened < next_line_character) {
									last_opened = opened;
									opened_count++;
								}

								const char* closed = strchr(last_closed + 1, ')');
								if (closed == nullptr) {
									return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
								}

								if (closed < next_line_character) {
									opened_count--;
								}
								last_closed = closed;
							}
							skipped_ending_tag_character = (char*)last_closed + 1;
							ending_tag_character = (char*)function::SkipWhitespace(skipped_ending_tag_character);
						}

						while (ending_tag_character < skipped_ending_tag_character) {
							*ending_tag_character = ECS_REFLECTION_TYPE_TAG_DELIMITER_CHAR;
							ending_tag_character++;
						}

						if (*skipped_ending_tag_character != '\n') {
							ending_tag_character = (char*)function::SkipCodeIdentifier(skipped_ending_tag_character);
						}
						skipped_ending_tag_character = (char*)function::SkipWhitespace(ending_tag_character);
					}

					*ending_tag_character = '\0';

					type.fields[type.fields.size].tag = parsed_tag_character;
					data->total_memory += type.fields[type.fields.size].tag.size;

					final_field_tag = parsed_tag_character;
				}
			}

			// null terminate the semicolon;
			char* last_field_character = (char*)semicolon_character;
			*last_field_character = '\0';

			// check to see if the field has a default value by looking for =
			// If it does bump it back to the space before it		
			char* equal_character = (char*)strchr(last_line_character, '=');
			if (equal_character != nullptr) {
				semicolon_character = equal_character - 1;
				equal_character--;
				*equal_character = '\0';
			}

			const char* current_character = semicolon_character - 1;

			Stream<char> embedded_array_size_body = {};
			unsigned short embedded_array_size = 0;
			// take into consideration embedded arrays
			if (*current_character == ']') {
				const char* closing_bracket = current_character;
				while (*current_character != '[') {
					current_character--;
				}

				Stream<char> parse_characters = Stream<char>((void*)(current_character + 1), PtrDifference(current_character + 1, closing_bracket));
				parse_characters = function::SkipWhitespace(parse_characters);
				parse_characters = function::SkipWhitespace(parse_characters, -1);
				// Check to see if this is a constant or the number is specified as is
				if (function::IsIntegerNumber(parse_characters, true)) {
					embedded_array_size = function::ConvertCharactersToInt(parse_characters);
				}
				else {
					embedded_array_size_body = parse_characters;
				}
				char* mutable_ptr = (char*)current_character;
				*mutable_ptr = '\0';
				current_character--;
			}

			// getting the field name
			while (IsTypeCharacter(*current_character)) {
				current_character--;
			}
			current_character++;

			bool success = DeduceFieldType(data, type, pointer_offset, current_character, last_line_character, last_field_character - 1);
			if (success) {
				ReflectionFieldInfo& info = type.fields[type.fields.size - 1].info;
				// Set the default value to false initially
				info.has_default_value = false;

				if (embedded_array_size > 0) {
					info.stream_byte_size = info.byte_size;
					info.basic_type_count = embedded_array_size;
					pointer_offset -= info.byte_size;
					info.byte_size *= embedded_array_size;
					pointer_offset += info.byte_size;

					// change the extended type to array
					info.stream_type = ReflectionStreamFieldType::BasicTypeArray;
				}
				// Get the default value if possible
				if (equal_character != nullptr && info.stream_type == ReflectionStreamFieldType::Basic) {
					// it was decremented before to place a '\0'
					equal_character += 2;

					const char* default_value_parse = function::SkipWhitespace(equal_character);

					auto parse_default_value = [&](const char* default_value_parse, char value_to_stop) {
						// Continue until the closing brace
						const char* start_default_value = default_value_parse + 1;

						const char* ending_default_value = start_default_value;
						while (*ending_default_value != value_to_stop && *ending_default_value != '\0' && *ending_default_value != '\n') {
							ending_default_value++;
						}
						if (*ending_default_value == '\0' || *ending_default_value == '\n') {
							// Abort. Can't deduce default value
							return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
						}
						// Use the parse function, any member from the union can be used
						bool parse_success = ParseReflectionBasicFieldType(
							info.basic_type, 
							Stream<char>(start_default_value, ending_default_value - start_default_value),
							&info.default_bool
						);
						info.has_default_value = parse_success;
						return ECS_REFLECTION_ADD_TYPE_FIELD_SUCCESS;
					};
					// If it is an opening brace, it's ok.
					if (*default_value_parse == '{') {
						ECS_REFLECTION_ADD_TYPE_FIELD_RESULT result = parse_default_value(default_value_parse, '}');
						if (result == ECS_REFLECTION_ADD_TYPE_FIELD_FAILED) {
							return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
						}
					}
					else if (function::IsCodeIdentifierCharacter(*default_value_parse)) {
						// Check to see that it is the constructor type - else it is the actual value
						const char* start_parse_value = default_value_parse;
						while (function::IsCodeIdentifierCharacter(*default_value_parse)) {
							default_value_parse++;
						}

						// If it is an open paranthese, it is a constructor
						if (*default_value_parse == '(') {
							// Look for the closing paranthese
							ECS_REFLECTION_ADD_TYPE_FIELD_RESULT result = parse_default_value(default_value_parse, ')');
							if (result == ECS_REFLECTION_ADD_TYPE_FIELD_FAILED) {
								return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
							}
						}
						else {
							// If it is a space, tab or semicolon, it means that we skipped the value
							info.has_default_value = ParseReflectionBasicFieldType(
								info.basic_type, 
								Stream<char>(start_parse_value, default_value_parse - start_parse_value),
								&info.default_bool
							);
						}
					}
				}

				ReflectionField& field = type.fields[type.fields.size - 1];
				if (final_field_tag != nullptr) {
					field.tag = final_field_tag;
				}
				else {
					field.tag = { nullptr, 0 };
				}

				Stream<char> namespace_string = "ECSEngine::";

				// Need remove the namespace ECSEngine for template types and basic types
				if (memcmp(field.definition.buffer, namespace_string.buffer, namespace_string.size) == 0) {
					field.definition.buffer += namespace_string.size;
					field.definition.size -= namespace_string.size;

					// Don't forget to remove this from the total memory pool
					data->total_memory -= namespace_string.size;
				}
				Stream<char> ecsengine_namespace = function::FindFirstToken(field.definition, namespace_string);
				while (ecsengine_namespace.buffer != nullptr) {
					// Move back all the characters over the current characters
					char* after_characters = ecsengine_namespace.buffer + namespace_string.size;
					size_t after_character_count = strlen(after_characters);
					memcpy(ecsengine_namespace.buffer, after_characters, after_character_count * sizeof(char));
					ecsengine_namespace[after_character_count] = '\0';

					ecsengine_namespace = function::FindFirstToken(ecsengine_namespace, namespace_string);
					// Don't forget to remove this from the total memory pool
					data->total_memory -= namespace_string.size;
				}

				if (embedded_array_size_body.size > 0) {
					// Register the text for constant evaluation later on
					ReflectionEmbeddedArraySize embedded_size;
					embedded_size.field_name = field.name;
					embedded_size.reflection_type = type.name;
					embedded_size.body = embedded_array_size_body;

					data->embedded_array_size.AddAssert(embedded_size);
				}
			}
			return success ? ECS_REFLECTION_ADD_TYPE_FIELD_SUCCESS : ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DeduceFieldType(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* last_line_character,
			const char* last_valid_character
		)
		{
			bool success = DeduceFieldTypePointer(data, type, pointer_offset, field_name, last_line_character);
			if (data->error_message->size > 0) {
				return false;
			}
			else if (!success) {
				success = DeduceFieldTypeStream(data, type, pointer_offset, field_name, last_line_character);
				if (data->error_message->size > 0) {
					return false;
				}

				if (!success) {
					// if this is not a pointer type, extended type then
					ReflectionField field;
					DeduceFieldTypeExtended(
						data,
						pointer_offset,
						field_name - 2,
						field
					);

					field.name = field_name;
					field.tag = type.fields[type.fields.size].tag;
					data->total_memory += strlen(field_name) + 1;
					type.fields.Add(field);
					success = true;
				}
			}
			return success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DeduceFieldTypePointer(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* last_line_character
		)
		{
			// Test first pointer type
			const char* star_ptr = strchr(last_line_character + 1, '*');
			if (star_ptr != nullptr) {
				ReflectionField field;
				field.name = field_name;
				
				size_t pointer_level = 1;
				char* first_star = (char*)star_ptr;
				star_ptr++;
				while (*star_ptr == '*') {
					pointer_level++;
					star_ptr++;
				}

				unsigned short before_pointer_offset = pointer_offset;
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					function::SkipWhitespace(first_star - 1, -1),
					field
				);

				field.info.basic_type_count = pointer_level;
				field.info.stream_type = ReflectionStreamFieldType::Pointer;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.byte_size = sizeof(void*);
				
				pointer_offset = function::AlignPointer(before_pointer_offset, alignof(void*));
				field.info.pointer_offset = pointer_offset;
				pointer_offset += sizeof(void*);

				data->total_memory += field.name.size;
				type.fields.Add(field);
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		// The stream will store the contained element byte size inside the additional flags component
		bool DeduceFieldTypeStream(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset, 
			const char* field_name, 
			const char* new_line_character
		)
		{
			// Test each keyword
			
			// Start with resizable and capacity, because these contain the stream keyword
			// Must search new line character + 1 since it might be made \0 for tags
			const char* stream_ptr = strstr(new_line_character + 1, STRING(ResizableStream));

			auto parse_stream_type = [&](ReflectionStreamFieldType stream_type, size_t byte_size, const char* stream_name) {
				ReflectionField field;
				field.name = field_name;

				unsigned short before_pointer_offset = pointer_offset;
				char* left_bracket = (char*)strchr(stream_ptr, '<');
				if (left_bracket == nullptr) {
					ECS_FORMAT_TEMP_STRING(temp_message, "Incorrect {#}, missing <.", stream_name);
					WriteErrorMessage(data, temp_message.buffer, -1);
					return;
				}

				char* right_bracket = (char*)strchr(left_bracket, '>');
				if (right_bracket == nullptr) {
					ECS_FORMAT_TEMP_STRING(temp_message, "Incorrect {#}, missing >.", stream_name);
					WriteErrorMessage(data, temp_message.buffer, -1);
					return;
				}

				// if nested templates, we need to find the last '>'
				char* next_right_bracket = (char*)strchr(right_bracket + 1, '>');
				while (next_right_bracket != nullptr) {
					right_bracket = next_right_bracket;
					next_right_bracket = (char*)strchr(right_bracket + 1, '>');
				}

				// Make the left bracket \0 because otherwise it will continue to get parsed
				char left_bracket_before = left_bracket[-1];
				left_bracket[0] = '\0';
				left_bracket[-1] = '\0';
				*right_bracket = '\0';
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					right_bracket - 1,
					field
				);
				left_bracket[0] = '<';
				left_bracket[-1] = left_bracket_before;
				right_bracket[0] = '>';
				right_bracket[1] = '\0';

				// All streams have aligned 8, the highest
				pointer_offset = function::AlignPointer(before_pointer_offset, alignof(Stream<void>));
				field.info.pointer_offset = pointer_offset;
				field.info.stream_type = stream_type;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.byte_size = byte_size;
				field.definition = function::SkipWhitespace(new_line_character + 1);

				pointer_offset += byte_size;

				data->total_memory += field.name.size + 1;
				data->total_memory += field.definition.size + 1;
				type.fields.Add(field);
			};

			if (stream_ptr != nullptr) {
				parse_stream_type(ReflectionStreamFieldType::ResizableStream, sizeof(ResizableStream<void>), STRING(ResizableStream));
				return true;
			}

			// Must search new line character + 1 since it might be made \0 for tags
			stream_ptr = strstr(new_line_character + 1, STRING(CapacityStream));
			if (stream_ptr != nullptr) {
				parse_stream_type(ReflectionStreamFieldType::CapacityStream, sizeof(CapacityStream<void>), STRING(CapacityStream));
				return true;
			}

			// Must search new line character + 1 since it might be made \0 for tags
			stream_ptr = strstr(new_line_character + 1, STRING(Stream));
			if (stream_ptr != nullptr) {
				parse_stream_type(ReflectionStreamFieldType::Stream, sizeof(Stream<void>), STRING(Stream));
				return true;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void DeduceFieldTypeExtended(
			ReflectionManagerParseStructuresThreadTaskData* data,
			unsigned short& pointer_offset,
			const char* last_type_character, 
			ReflectionField& field
		)
		{
			char* current_character = (char*)last_type_character;
			if (*current_character == '>') {
				// Template type - remove
				unsigned int hey_there = 0;
			}

			current_character[1] = '\0';
			while (IsTypeCharacter(*current_character)) {
				current_character--;
			}
			char* first_space = current_character;
			current_character--;
			while (IsTypeCharacter(*current_character)) {
				current_character--;
			}

			auto processing = [&](const char* basic_type) {
				size_t type_size = strlen(basic_type);
				GetReflectionFieldInfo(data, basic_type, field);

				if (field.info.basic_type != ReflectionBasicFieldType::UserDefined && field.info.basic_type != ReflectionBasicFieldType::Unknown) {
					field.info.pointer_offset = pointer_offset;
					pointer_offset += field.info.byte_size;
				}
				else {
					field.info.pointer_offset = pointer_offset;
				}
			};

			// if there is no second word to process
			if (PtrDifference(current_character, first_space) == 1) {
				processing(first_space + 1);
			}
			// if there are two words
			else {
				// if there is a const, ignore it
				const char* const_ptr = strstr(current_character, "const");

				if (const_ptr != nullptr) {
					processing(first_space + 1);
				}
				// not a const type, possibly unsigned something, so process it normaly
				else {
					processing(current_character + 1);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionFieldInfo(ReflectionManagerParseStructuresThreadTaskData* data, Stream<char> basic_type, ReflectionField& field)
		{
			Stream<char> field_name = field.name;
			field = GetReflectionFieldInfo(data->field_table, basic_type);
			field.name = field_name;
			if (field.info.stream_type == ReflectionStreamFieldType::Unknown || field.info.basic_type == ReflectionBasicFieldType::UserDefined) {
				data->total_memory += basic_type.size;
				field.info.byte_size = 0;
				field.info.basic_type_count = 1;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField GetReflectionFieldInfo(const ReflectionFieldTable* reflection_field_table, Stream<char> basic_type)
		{
			ReflectionField field;

			ResourceIdentifier identifier = ResourceIdentifier(basic_type);

			bool success = reflection_field_table->TryGetValue(identifier, field.info);
			field.definition = basic_type;
			if (!success) {
				field.info.stream_type = ReflectionStreamFieldType::Basic;
				field.info.basic_type = ReflectionBasicFieldType::UserDefined;
			}
			else {
				// Set to global strings the definition in order to not occupy extra memory

#define CASE(type, string) case ReflectionBasicFieldType::type: field.definition = STRING(string); break;
#define CASE234(type, string) CASE(type##2, string##2); CASE(type##3, string##3); CASE(type##4, string##4);
#define CASE1234(type, string) CASE(type, string); CASE234(type, string);

				switch (field.info.basic_type) {
					CASE1234(Bool, bool);
					CASE234(Char, char);
					CASE234(UChar, uchar);
					CASE234(Short, short);
					CASE234(UShort, ushort);
					CASE234(Int, int);
					CASE234(UInt, uint);
					CASE234(Long, long);
					CASE234(ULong, ulong);
					CASE(Int8, int8_t);
					CASE(UInt8, uint8_t);
					CASE(Int16, int16_t);
					CASE(UInt16, uint16_t);
					CASE(Int32, int32_t);
					CASE(UInt32, uint32_t);
					CASE(Int64, int64_t);
					CASE(UInt64, uint64_t);
					CASE1234(Float, float);
					CASE1234(Double, double);
				}
			}

#undef CASE
#undef CASE234
#undef CASE1234

			return field;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField GetReflectionFieldInfo(const ReflectionManager* reflection, Stream<char> basic_type)
		{
			return GetReflectionFieldInfo(&reflection->field_table, basic_type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField GetReflectionFieldInfoStream(const ReflectionManager* reflection, Stream<char> basic_type, ReflectionStreamFieldType stream_type)
		{
			ReflectionField field;

			field = GetReflectionFieldInfo(reflection, basic_type);
			field.info.stream_byte_size = field.info.byte_size;
			field.info.stream_type = stream_type;
			if (stream_type == ReflectionStreamFieldType::Stream) {
				field.info.byte_size = sizeof(Stream<void>);
			}
			else if (stream_type == ReflectionStreamFieldType::CapacityStream) {
				field.info.byte_size = sizeof(CapacityStream<void>);
			}
			else if (stream_type == ReflectionStreamFieldType::ResizableStream) {
				field.info.byte_size = sizeof(ResizableStream<char>);
			}
			else if (stream_type == ReflectionStreamFieldType::Pointer) {
				field.info.byte_size = sizeof(void*);
			}

			return field;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool HasReflectStructures(Stream<wchar_t> path)
		{
			ECS_FILE_HANDLE file_handle = 0;

			const size_t max_line_characters = 4096;
			char line_characters[max_line_characters];
			// open the file from the beginning
			ECS_FILE_STATUS_FLAGS status_flags = OpenFile(path, &file_handle, ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_READ_ONLY);
			if (status_flags == ECS_FILE_STATUS_OK) {
				// read the first line in order to check if there is something to be reflected
				bool success = ReadFile(file_handle, { line_characters, max_line_characters });
				bool has_reflect = false;
				if (success) {
					has_reflect = strstr(line_characters, STRING(ECS_REFLECT)) != nullptr;
				}
				CloseFile(file_handle);
				return has_reflect;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t CalculateReflectionTypeByteSize(const ReflectionManager* reflection_manager, const ReflectionType* type)
		{
			size_t alignment = CalculateReflectionTypeAlignment(reflection_manager, type);

			size_t byte_size = type->fields[type->fields.size - 1].info.pointer_offset + type->fields[type->fields.size - 1].info.byte_size;
			size_t remainder = byte_size % alignment;

			return remainder == 0 ? byte_size : byte_size + alignment - remainder;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t CalculateReflectionTypeAlignment(const ReflectionManager* reflection_manager, const ReflectionType* type)
		{
			size_t alignment = 0;

			for (size_t index = 0; index < type->fields.size && alignment < alignof(void*); index++) {
				if (type->fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// Check blittable exceptions
					size_t blittable_exception = reflection_manager->FindBlittableException(type->fields[index].definition).y;
					if (blittable_exception == -1) {
						ReflectionType nested_type;
						if (reflection_manager->TryGetType(type->fields[index].definition, nested_type)) {
							alignment = std::max(alignment, CalculateReflectionTypeAlignment(reflection_manager, &nested_type));
						}
						else {
							Stream<char> definition = type->fields[index].definition;

							// The type is not found, look for container types
							unsigned int container_index = FindReflectionCustomType(definition);
							if (container_index != -1) {
								ReflectionCustomTypeByteSizeData byte_size_data;
								byte_size_data.reflection_manager = reflection_manager;
								byte_size_data.definition = definition;
								ulong2 byte_size_alignment = ECS_REFLECTION_CUSTOM_TYPES[container_index].byte_size(&byte_size_data);

								alignment = std::max(alignment, byte_size_alignment.y);
							}
							else {
								// Check for pointers
								if (type->fields[index].info.stream_type == ReflectionStreamFieldType::Pointer) {
									alignment = alignof(void*);
								}
								else {
									// Assert false or assume alignment of 8
									// Check for ECS_GIVE_SIZE_REFLECTION
									if (type->fields[index].Has(STRING(ECS_GIVE_SIZE_REFLECTION))) {
										alignment = GetReflectionTypeGivenFieldTag(&type->fields[index]).y;
									}
									else {
										ECS_ASSERT(false);
										alignment = alignof(void*);
									}
								}
							}
						}
					}
					else {
						alignment = std::max(alignment, blittable_exception);
					}
				}
				else {
					if (type->fields[index].info.stream_type == ReflectionStreamFieldType::Basic ||
						type->fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
						alignment = std::max(alignment, GetFieldTypeAlignment(type->fields[index].info.basic_type));
					}
					else {
						alignment = std::max(alignment, GetFieldTypeAlignment(type->fields[index].info.stream_type));
					}
				}
			}

			return alignment;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void CalculateReflectionTypeCachedParameters(const ReflectionManager* reflection_manager, ReflectionType* type)
		{
			type->byte_size = CalculateReflectionTypeByteSize(reflection_manager, type);
			type->alignment = CalculateReflectionTypeAlignment(reflection_manager, type);
			type->is_blittable = SearchIsBlittable(reflection_manager, type);
			type->is_blittable_with_pointer = SearchIsBlittableWithPointer(reflection_manager, type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionTypeByteSize(const ReflectionType* type)
		{
			return type->byte_size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionTypeAlignment(const ReflectionType* type)
		{
			return type->alignment;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t SearchReflectionUserDefinedTypeByteSize(
			const ReflectionManager* reflection_manager, 
			Stream<char> definition
		)
		{
			return SearchReflectionUserDefinedTypeByteSizeAlignment(reflection_manager, definition).x;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 SearchReflectionUserDefinedTypeByteSizeAlignment(
			const ReflectionManager* reflection_manager,
			Stream<char> definition
		)
		{
			// Check pointers
			if (function::FindFirstCharacter(definition, '*').size > 0) {
				return { sizeof(void*), alignof(void*) };
			}

			// Check stream types
			const char* stream_string = STRING(Stream);
			if (memcmp(definition.buffer, stream_string, strlen(stream_string)) == 0) {
				return { sizeof(Stream<void>), alignof(Stream<void>) };
			}

			stream_string = STRING(CapacityStream);
			if (memcmp(definition.buffer, stream_string, strlen(stream_string)) == 0) {
				return { sizeof(CapacityStream<void>), alignof(CapacityStream<void>) };
			}

			stream_string = STRING(ResizableStream);
			if (memcmp(definition.buffer, stream_string, strlen(stream_string)) == 0) {
				return { sizeof(ResizableStream<char>), alignof(ResizableStream<char>) };
			}

			ulong2 blittable_exception = reflection_manager->FindBlittableException(definition);
			if (blittable_exception.x != -1) {
				return blittable_exception;
			}

			// Check fundamental type
			ReflectionField field = GetReflectionFieldInfo(reflection_manager, definition);
			if (field.info.basic_type != ReflectionBasicFieldType::UserDefined && field.info.basic_type != ReflectionBasicFieldType::Unknown) {
				return { field.info.byte_size, GetFieldTypeAlignment(field.info.basic_type) };
			}

			ReflectionType type;
			ReflectionEnum enum_;
			unsigned int container_index = -1;

			SearchReflectionUserDefinedType(reflection_manager, definition, type, container_index, enum_);
			if (type.name.size > 0) {
				return { GetReflectionTypeByteSize(&type), GetReflectionTypeAlignment(&type) };
			}
			else if (container_index != -1) {
				ReflectionCustomTypeByteSizeData byte_size_data;
				byte_size_data.reflection_manager = reflection_manager;
				byte_size_data.definition = definition;
				return ECS_REFLECTION_CUSTOM_TYPES[container_index].byte_size(&byte_size_data);
			}
			else if (enum_.name.size > 0) {
				return { sizeof(unsigned char), alignof(unsigned char) };
			}

			// Signal an error
			return { (size_t)-1, (size_t)-1 };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t SearchReflectionUserDefinedTypeAlignment(const ReflectionManager* reflection_manager, Stream<char> definition)
		{
			ReflectionType type;
			ReflectionEnum enum_;
			unsigned int container_index = -1;

			SearchReflectionUserDefinedType(reflection_manager, definition, type, container_index, enum_);
			if (type.name.size > 0) {
				return GetReflectionTypeAlignment(&type);
			}
			else if (container_index != -1) {
				ReflectionCustomTypeByteSizeData byte_size_data;
				byte_size_data.reflection_manager = reflection_manager;
				byte_size_data.definition = definition;
				return ECS_REFLECTION_CUSTOM_TYPES[container_index].byte_size(&byte_size_data).y;
			}
			else if (enum_.name.size > 0) {
				return alignof(unsigned char);
			}
			else {
				ReflectionField field = GetReflectionFieldInfo(reflection_manager, definition);
				if (field.info.basic_type != ReflectionBasicFieldType::UserDefined && field.info.basic_type != ReflectionBasicFieldType::Unknown) {
					return GetFieldTypeAlignment(field.info.basic_type);
				}
			}

			// Signal an error
			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SearchReflectionUserDefinedType(const ReflectionManager* reflection_manager, Stream<char> definition, ReflectionType& type, unsigned int& container_index, ReflectionEnum& enum_)
		{
			if (reflection_manager->TryGetType(definition, type)) {
				container_index = -1;
				enum_.name = { nullptr, 0 };
				return;
			}
			else {
				container_index = FindReflectionCustomType(definition);
				if (container_index != -1) {
					type.name = { nullptr, 0 };
					enum_.name = { nullptr, 0 };
					return;
				}
				else {
					if (reflection_manager->TryGetEnum(definition, enum_)) {
						type.name = { nullptr, 0 };
						// Container index is already -1
						return;
					}
					else {
						// None matches
						type.name = { nullptr, 0 };
						enum_.name = { nullptr, 0 };
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------

		bool SearchIsBlittableImplementation(
			const Reflection::ReflectionManager* reflection_manager,
			const Reflection::ReflectionType* type,
			bool pointers_are_copyable
		) {
			for (size_t index = 0; index < type->fields.size; index++) {
				// Check exceptions
				ulong2 exception_index = reflection_manager->FindBlittableException(type->fields[index].definition);
				if (exception_index.x != -1) {
					// Go to the next iteration
					continue;
				}

				if (type->fields[index].info.stream_type != ReflectionStreamFieldType::Basic && type->fields[index].info.stream_type != ReflectionStreamFieldType::BasicTypeArray) {
					// Stream type or pointer type, not trivially copyable
					return pointers_are_copyable && type->fields[index].info.stream_type == ReflectionStreamFieldType::Pointer;
				}

				if (type->fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// Check it
					ReflectionType nested_type;
					if (reflection_manager->TryGetType(type->fields[index].definition, nested_type)) {
						if (!SearchIsBlittable(reflection_manager, &nested_type)) {
							return false;
						}
					}
					else {
						// Check custom type
						unsigned int custom_index = FindReflectionCustomType(type->fields[index].definition);
						if (custom_index != -1) {
							ReflectionCustomTypeIsBlittableData data;
							data.reflection_manager = reflection_manager;
							data.definition = type->fields[index].definition;
							if (!ECS_REFLECTION_CUSTOM_TYPES[custom_index].is_blittable(&data)) {
								return false;
							}
						}
						else {
							// Check for ECS_GIVE_SIZE_REFLECTION tag. If it present, assume it is trivially copyable
							// because we don't know its fields
							ECS_ASSERT(type->fields[index].Has(STRING(ECS_GIVE_SIZE_REFLECTION)));
						}
					}
				}
			}

			return true;
		}

		// -----------------------------------------------------------------------------------------

		bool SearchIsBlittable(
			const Reflection::ReflectionManager* reflection_manager,
			const Reflection::ReflectionType* type
		) {
			return SearchIsBlittableImplementation(reflection_manager, type, false);
		}

		// -----------------------------------------------------------------------------------------

		bool SearchIsBlittableImplementation(
			const Reflection::ReflectionManager* reflection_manager,
			Stream<char> definition,
			bool pointers_are_copyable
		) {
			ReflectionType type;
			if (reflection_manager->TryGetType(definition, type)) {
				// If we have the type, return its trivial status
				return SearchIsBlittable(reflection_manager, &type);
			}
			else {
				// Might be another container type
				unsigned int custom_index = FindReflectionCustomType(definition);
				if (custom_index != -1) {
					ReflectionCustomTypeIsBlittableData is_data;
					is_data.definition = definition;
					is_data.reflection_manager = reflection_manager;
					return ECS_REFLECTION_CUSTOM_TYPES[custom_index].is_blittable(&is_data);
				}
				else {
					// Try enum
					Reflection::ReflectionEnum enum_;
					if (reflection_manager->TryGetEnum(definition, enum_)) {
						return true;
					}
				}
			}

			// It is not a user defined, not a container type. Either error or fundamental type
			ReflectionBasicFieldType basic_type;
			ReflectionStreamFieldType stream_type;
			ConvertStringToPrimitiveType(definition, basic_type, stream_type);
			if (IsUserDefined(basic_type) || basic_type == ReflectionBasicFieldType::Unknown) {
				return false;
			}

			if (stream_type != ReflectionStreamFieldType::Basic && stream_type != ReflectionStreamFieldType::BasicTypeArray) {
				// Check blittable exceptions
				ulong2 exception_index = reflection_manager->FindBlittableException(definition);
				if (exception_index.x != -1) {
					return true;
				}

				// Pointer or stream type
				return pointers_are_copyable && stream_type == ReflectionStreamFieldType::Pointer;
			}
			return true;
		}

		// -----------------------------------------------------------------------------------------

		bool SearchIsBlittable(
			const Reflection::ReflectionManager* reflection_manager,
			Stream<char> definition
		)
		{
			return SearchIsBlittableImplementation(reflection_manager, definition, false);
		}

		// -----------------------------------------------------------------------------------------

		bool SearchIsBlittableWithPointer(const ReflectionManager* reflection_manager, const ReflectionType* type)
		{
			return SearchIsBlittableImplementation(reflection_manager, type, true);
		}

		// -----------------------------------------------------------------------------------------

		bool SearchIsBlittableWithPointer(const ReflectionManager* reflection_manager, Stream<char> definition)
		{
			return SearchIsBlittableImplementation(reflection_manager, definition, true);
		}

		// -----------------------------------------------------------------------------------------

		void SetReflectionTypeFieldsStreamByteSize(const ReflectionManager* reflection_manager, ReflectionType* reflection_type)
		{
			for (size_t index = 0; index < reflection_type->fields.size; index++) {
				if (IsStream(reflection_type->fields[index].info.stream_type)) {
					Stream<char> target_type = ReflectionCustomTypeGetTemplateArgument(reflection_type->fields[index].definition);
					// Void streams are a special case - consider the element byte size as 1 since it's a byte stream
					if (target_type == STRING(void)) {
						reflection_type->fields[index].info.stream_byte_size = 1;
					}
					else {
						size_t stream_byte_size = SearchReflectionUserDefinedTypeByteSize(reflection_manager, target_type);
						reflection_type->fields[index].info.stream_byte_size = stream_byte_size;
					}
				}
				else if (reflection_type->fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					reflection_type->fields[index].info.stream_byte_size = reflection_type->fields[index].info.byte_size / 
						reflection_type->fields[index].info.basic_type_count;
				}
			}
		}

		// -----------------------------------------------------------------------------------------

		void CopyReflectionType(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			const void* source,
			void* destination,
			AllocatorPolymorphic field_allocator
		)
		{
			for (size_t index = 0; index < type->fields.size; index++) {
				const void* source_field = function::OffsetPointer(source, type->fields[index].info.pointer_offset);
				void* destination_field = function::OffsetPointer(destination, type->fields[index].info.pointer_offset);

				if (type->fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// If the byte size is given, then do a blit copy
					if (type->fields[index].Has(STRING(ECS_GIVE_SIZE_REFLECTION))) {
						memcpy(destination_field, source_field, type->fields[index].info.byte_size);
					}
					else {
						CopyReflectionType(reflection_manager, type->fields[index].definition, source_field, destination_field, field_allocator);
					}
				}
				else {
					CopyReflectionFieldBasic(&type->fields[index].info, source_field, destination_field, field_allocator);
				}
			}
		}

		// -----------------------------------------------------------------------------------------

		void CopyReflectionType(
			const Reflection::ReflectionManager* reflection_manager,
			Stream<char> definition,
			const void* source,
			void* destination,
			AllocatorPolymorphic field_allocator
		)
		{
			// Check blittable exceptions
			ulong2 exception_index = reflection_manager->FindBlittableException(definition);
			if (exception_index.x != -1) {
				memcpy(destination, source, exception_index.x);
			}
			else {
				// Check fundamental type
				ReflectionField field = GetReflectionFieldInfo(reflection_manager, definition);
				if (field.info.basic_type != ReflectionBasicFieldType::UserDefined && field.info.basic_type != ReflectionBasicFieldType::Unknown) {
					CopyReflectionFieldBasic(&field.info, source, destination, field_allocator);
				}
				else {
					// Check blittable exception
					ReflectionType reflection_type;
					if (reflection_manager->TryGetType(definition, reflection_type)) {
						// Forward the call
						CopyReflectionType(reflection_manager, &reflection_type, source, destination, field_allocator);
					}
					else {
						// Check to see if it is a custom type
						unsigned int custom_index = FindReflectionCustomType(definition);
						if (custom_index != -1) {
							ReflectionCustomTypeIsBlittableData trivially_data;
							trivially_data.definition = definition;
							trivially_data.reflection_manager = reflection_manager;
							bool trivially_copyable = ECS_REFLECTION_CUSTOM_TYPES[custom_index].is_blittable(&trivially_data);
							if (trivially_copyable) {
								ReflectionCustomTypeByteSizeData byte_size_data;
								byte_size_data.definition = definition;
								byte_size_data.reflection_manager = reflection_manager;
								size_t byte_size = ECS_REFLECTION_CUSTOM_TYPES[custom_index].byte_size(&byte_size_data).x;

								memcpy(destination, source, byte_size);
							}
							else {
								ReflectionCustomTypeCopyData copy_data;
								copy_data.allocator = field_allocator;
								copy_data.definition = definition;
								copy_data.destination = destination;
								copy_data.source = source;
								copy_data.reflection_manager = reflection_manager;

								ECS_REFLECTION_CUSTOM_TYPES[custom_index].copy(&copy_data);
							}
						}
						else {
							// Try with an enum
							ReflectionEnum enum_;
							if (reflection_manager->TryGetEnum(definition, enum_)) {
								memcpy(destination, source, sizeof(unsigned char));
							}
							else {
								// Error - no type matched
								ECS_ASSERT(false, "Copy reflection type unexpected error.");
							}
						}
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, const ReflectionField& field)
		{
			// Check the give size macro
			if (field.info.basic_type != ReflectionBasicFieldType::UserDefined) {
				if (field.info.stream_type == ReflectionStreamFieldType::Basic || field.info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					return GetFieldTypeAlignment(field.info.basic_type);
				}
				else {
					return GetFieldTypeAlignment(field.info.stream_type);
				}
			}
			else {
				ulong2 given_size = GetReflectionTypeGivenFieldTag(&field);
				if (given_size.y != -1) {
					return given_size.y;
				}

				// Check blittable exception
				size_t blittable_exception = reflection_manager->FindBlittableException(field.definition).y;
				if (blittable_exception != -1) {
					return blittable_exception;
				}

				// Check for pointers
				if (field.info.stream_type == ReflectionStreamFieldType::Pointer) {
					return alignof(void*);
				}
				else {
					ReflectionType nested_type;
					if (reflection_manager->TryGetType(field.definition, nested_type)) {
						return GetReflectionTypeAlignment(&nested_type);
					}
					else {
						// Check for enums
						ReflectionEnum enum_;
						if (reflection_manager->TryGetEnum(field.definition, enum_)) {
							return alignof(unsigned char);
						}

						unsigned int custom_index = FindReflectionCustomType(field.definition);
						ECS_ASSERT(custom_index != -1);

						ReflectionCustomTypeByteSizeData byte_size_data;
						byte_size_data.definition = field.definition;
						byte_size_data.reflection_manager = reflection_manager;
						return ECS_REFLECTION_CUSTOM_TYPES[custom_index].byte_size(&byte_size_data).y;
					}
				}
			}
			
			// Should not reach here
			return 0;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void CopyReflectionFieldBasic(const ReflectionFieldInfo* info, const void* source, void* destination, AllocatorPolymorphic allocator)
		{
			ReflectionStreamFieldType stream_type = info->stream_type;
			if (stream_type == ReflectionStreamFieldType::Basic || stream_type == ReflectionStreamFieldType::BasicTypeArray) {
				memcpy(destination, source, info->byte_size);
			}
			else {
				// Here the source needs to be passed in, it will offset it manually
				if (stream_type == ReflectionStreamFieldType::Stream) {
					Stream<void>* source_stream = (Stream<void>*)source;
					Stream<void>* destination_stream = (Stream<void>*)destination;

					size_t copy_size = source_stream->size * GetReflectionFieldStreamElementByteSize(*info);
					void* allocation = nullptr;
					if (copy_size > 0) {
						allocation = Allocate(allocator, copy_size);
						memcpy(allocation, source_stream->buffer, copy_size);
					}

					destination_stream->buffer = allocation;
					destination_stream->size = source_stream->size;
				}
				else if (stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream) {
					CapacityStream<void>* source_stream = (CapacityStream<void>*)source;
					CapacityStream<void>* destination_stream = (CapacityStream<void>*)destination;

					size_t copy_size = source_stream->capacity * GetReflectionFieldStreamElementByteSize(*info);
					void* allocation = nullptr;
					if (copy_size > 0) {
						allocation = Allocate(allocator, copy_size);
						memcpy(allocation, source_stream->buffer, copy_size);
					}

					destination_stream->buffer = allocation;
					destination_stream->size = source_stream->size;
					destination_stream->capacity = source_stream->capacity;
				}
				else if (stream_type == ReflectionStreamFieldType::Pointer) {
					// Just copy the pointer
					const void** source_ptr = (const void**)source;
					const void** destination_ptr = (const void**)destination;
					*destination_ptr = *source_ptr;
				}
				else {
					ECS_ASSERT(false, "Unknown stream type");
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetBasicTypeArrayElementSize(const ReflectionFieldInfo& info)
		{
			return info.byte_size / info.basic_type_count;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* GetReflectionFieldStreamBuffer(const ReflectionFieldInfo& info, const void* data, bool offset_into_data)
		{
			const void* stream_field = offset_into_data ? function::OffsetPointer(data, info.pointer_offset) : data;
			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				return stream->buffer;
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				return stream->buffer;
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<char>* stream = (ResizableStream<char>*)stream_field;
				return stream->buffer;
			}
			return nullptr;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldStreamSize(const ReflectionFieldInfo& info, const void* data, bool offset_into_data)
		{
			const void* stream_field = offset_into_data ? function::OffsetPointer(data, info.pointer_offset) : data;

			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				return stream->size;
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				return stream->size;
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<char>* stream = (ResizableStream<char>*)stream_field;
				return stream->size;
			}
			return 0;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		template<bool basic_array>
		ResizableStream<void> GetReflectionFieldResizableStreamVoidImpl(const ReflectionFieldInfo& info, const void* data, bool offset_into_data) {
			ResizableStream<void> return_value;

			const void* stream_field = offset_into_data ? function::OffsetPointer(data, info.pointer_offset) : data;
			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				return_value.buffer = stream->buffer;
				return_value.size = stream->size;
				return_value.capacity = stream->size;
				return_value.allocator = { nullptr };
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				return_value.buffer = stream->buffer;
				return_value.size = stream->size;
				return_value.capacity = stream->capacity;
				return_value.allocator = { nullptr };
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<void>* stream = (ResizableStream<void>*)stream_field;
				return_value = *stream;
			}
			if constexpr (basic_array) {
				if (info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					return_value.buffer = (void*)stream_field;
					return_value.size = info.basic_type_count;
					return_value.capacity = info.basic_type_count;
					return_value.allocator = { nullptr };
				}
			}
			return return_value;
		}

		Stream<void> GetReflectionFieldStreamVoid(const ReflectionFieldInfo& info, const void* data, bool offset_into_data)
		{
			ResizableStream<void> stream = GetReflectionFieldResizableStreamVoid(info, data, offset_into_data);
			return { stream.buffer, stream.size };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ResizableStream<void> GetReflectionFieldResizableStreamVoid(const ReflectionFieldInfo& info, const void* data, bool offset_into_data)
		{
			return GetReflectionFieldResizableStreamVoidImpl<false>(info, data, offset_into_data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<void> GetReflectionFieldStreamVoidEx(const ReflectionFieldInfo& info, const void* data, bool offset_into_data)
		{
			ResizableStream<void> stream = GetReflectionFieldResizableStreamVoidEx(info, data, offset_into_data);
			return { stream.buffer, stream.size };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ResizableStream<void> GetReflectionFieldResizableStreamVoidEx(const ReflectionFieldInfo& info, const void* data, bool offset_into_data)
		{
			return GetReflectionFieldResizableStreamVoidImpl<true>(info, data, offset_into_data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldStreamElementByteSize(const ReflectionFieldInfo& info)
		{
			return info.stream_byte_size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned char GetReflectionFieldPointerIndirection(const ReflectionFieldInfo& info)
		{
			ECS_ASSERT(info.stream_type == ReflectionStreamFieldType::Pointer);
			return info.basic_type_count;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionBasicFieldType ConvertBasicTypeMultiComponentToSingle(ReflectionBasicFieldType type)
		{
			if (IsBoolBasicTypeMultiComponent(type)) {
				return ReflectionBasicFieldType::Bool;
			}
			else if (IsFloatBasicTypeMultiComponent(type)) {
				return ReflectionBasicFieldType::Float;
			}
			else if (IsDoubleBasicTypeMultiComponent(type)) {
				return ReflectionBasicFieldType::Double;
			}
			else if (IsIntegralMultiComponent(type)) {
				unsigned char type_char = (unsigned char)type;
				if ((unsigned char)ReflectionBasicFieldType::Char2 >= type_char && (unsigned char)ReflectionBasicFieldType::ULong2) {
					return (ReflectionBasicFieldType)((unsigned char)ReflectionBasicFieldType::Int8 + type_char - (unsigned char)ReflectionBasicFieldType::Char2);
				}
				else if ((unsigned char)ReflectionBasicFieldType::Char3 >= type_char && (unsigned char)ReflectionBasicFieldType::ULong3) {
					return (ReflectionBasicFieldType)((unsigned char)ReflectionBasicFieldType::Int8 + type_char - (unsigned char)ReflectionBasicFieldType::Char3);
				}
				else if ((unsigned char)ReflectionBasicFieldType::Char4 >= type_char && (unsigned char)ReflectionBasicFieldType::ULong4) {
					return (ReflectionBasicFieldType)((unsigned char)ReflectionBasicFieldType::Int8 + type_char - (unsigned char)ReflectionBasicFieldType::Char4);
				}
			}

			return ReflectionBasicFieldType::COUNT;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		template<bool basic_array>
		void SetReflectionFieldResizableStreamVoidImpl(const ReflectionFieldInfo& info, void* data, ResizableStream<void> stream_data, bool offset_into_data) {
			void* stream_field = offset_into_data ? function::OffsetPointer(data, info.pointer_offset) : data;
			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				*stream = { stream_data.buffer, stream_data.size };
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				*stream = { stream_data.buffer, stream_data.size, stream_data.capacity };
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<void>* stream = (ResizableStream<void>*)stream_field;
				*stream = stream_data;
			}
			if constexpr (basic_array) {
				if (info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					size_t element_byte_size = info.byte_size / info.basic_type_count;
					size_t elements_to_copy = std::min((size_t)stream_data.size, (size_t)info.basic_type_count);
					size_t copy_size = elements_to_copy * element_byte_size;
					memcpy(stream_field, stream_data.buffer, copy_size);
					if (elements_to_copy < (size_t)info.basic_type_count) {
						memset(function::OffsetPointer(stream_field, copy_size), 0, info.byte_size - copy_size);
					}
				}
			}
		}

		void SetReflectionFieldResizableStreamVoid(const ReflectionFieldInfo& info, void* data, ResizableStream<void> stream_data, bool offset_into_data)
		{
			SetReflectionFieldResizableStreamVoidImpl<false>(info, data, stream_data, offset_into_data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SetReflectionFieldResizableStreamVoidEx(const ReflectionFieldInfo& info, void* data, ResizableStream<void> stream_data, bool offset_into_data)
		{
			SetReflectionFieldResizableStreamVoidImpl<true>(info, data, stream_data, offset_into_data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CompareReflectionTypes(
			const ReflectionManager* first_reflection_manager, 
			const ReflectionManager* second_reflection_manager,
			const ReflectionType* first, 
			const ReflectionType* second
		)
		{
			if (first->fields.size != second->fields.size) {
				return false;
			}

			// If the basic or the stream type has changed or the definition
			for (size_t index = 0; index < first->fields.size; index++) {
				const ReflectionFieldInfo* first_info = &first->fields[index].info;
				const ReflectionFieldInfo* second_info = &second->fields[index].info;

				if (first_info->basic_type != second_info->basic_type) {
					return false;
				}

				if (first_info->stream_type != second_info->stream_type) {
					return false;
				}

				if (first_info->byte_size != second_info->byte_size) {
					return false;
				}

				// Now compare the definitions. If they changed, the types have changed
				if (!function::CompareStrings(first->fields[index].definition, second->fields[index].definition)) {
					return false;
				}

				// Now check the name - if they have different names and the name can be found on other spot,
				// Consider them to be different. Else, allow it to be the same to basically support renaming
				if (!function::CompareStrings(first->fields[index].name, second->fields[index].name)) {
					unsigned int second_field_index = second->FindField(first->fields[index].name);
					if (second_field_index != -1) {
						return false;
					}
				}

				// If a user defined type is here, check that it too didn't change
				// For custom types we shouldn't need to check (those are supposed to be fixed, like containers)
				if (first_info->basic_type == ReflectionBasicFieldType::UserDefined) {
					ReflectionType first_nested;
					ReflectionType second_nested;

					if (first_reflection_manager->TryGetType(first->fields[index].definition, first_nested)) {
						if (second_reflection_manager->TryGetType(first->fields[index].definition, second_nested)) {
							// Verify these as well
							if (!CompareReflectionTypes(first_reflection_manager, second_reflection_manager, &first_nested, &second_nested)) {
								return false;
							}
						}
						else {
							// The first one has it but the second one doesn't have it.
							// An error must be somewhere
							return false;
						}
					}
					else {
						// Assume that it is a container type. Only check that the second one doesn't have it either
						if (second_reflection_manager->TryGetType(first->fields[index].definition, second_nested)) {
							// The second one has it but the first one doesn't have it.
							// An error must be somewhere
							return false;
						}
						// Else both don't have it (most likely is a container type)
					}
				}
			}

			return true;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CompareReflectionTypesTags(const ReflectionType* first, const ReflectionType* second, bool type_tags, bool field_tags)
		{
			if (type_tags) {
				if (first->tag != second->tag) {
					return false;
				}
			}

			if (field_tags) {
				if (first->fields.size != second->fields.size) {
					return false;
				}

				for (size_t index = 0; index < first->fields.size; index++) {
					if (first->fields[index].tag != second->fields[index].tag) {
						return false;
					}
				}
			}

			return true;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CompareReflectionFieldInfoInstances(const ReflectionFieldInfo* info, const void* first, const void* second, bool offset_into_data)
		{
			if (info->basic_type != ReflectionBasicFieldType::UserDefined) {
				if (info->stream_type == ReflectionStreamFieldType::Basic || info->stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					if (offset_into_data) {
						first = function::OffsetPointer(first, info->pointer_offset);
						second = function::OffsetPointer(second, info->pointer_offset);
					}
					return memcmp(first, second, info->byte_size) == 0;
				}
				else {
					size_t basic_byte_size = GetReflectionBasicFieldTypeByteSize(info->basic_type);
					if (IsStream(info->stream_type)) {
						Stream<void> first_data = GetReflectionFieldStreamVoid(*info, first, offset_into_data);
						Stream<void> second_data = GetReflectionFieldStreamVoid(*info, second, offset_into_data);
						return first_data.size == second_data.size && memcmp(first_data.buffer, second_data.buffer, first_data.size * basic_byte_size) == 0;
					}
					else if (info->stream_type == ReflectionStreamFieldType::Pointer) {
						if (offset_into_data) {
							first = function::OffsetPointer(first, info->pointer_offset);
							second = function::OffsetPointer(second, info->pointer_offset);
						}

						unsigned char pointer_indirection = GetReflectionFieldPointerIndirection(*info);
						while (pointer_indirection > 0) {
							first = *(void**)first;
							second = *(void**)second;
							pointer_indirection--;
						}
						// Compare the pointer values
						return first == second || memcmp(first, second, basic_byte_size) == 0;
					}
					else {
						// Unknown type or invalid
						ECS_ASSERT(false, "Invalid reflection stream field type when comparing infos.");
						return false;
					}
				}
			}
			else {
				// User defined type - assert that it doesn't happen - it is not in the description of the function
				ECS_ASSERT(false, "User defined type when trying to compare 2 reflection field infos.");
				return false;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CompareReflectionFieldInstances(
			const ReflectionManager* reflection_manager, 
			const ReflectionField* field, 
			const void* first, 
			const void* second, 
			bool offset_into_data,
			const CompareReflectionTypeInstancesOptions* options
		)
		{
			if (offset_into_data) {
				first = function::OffsetPointer(first, field->info.pointer_offset);
				second = function::OffsetPointer(second, field->info.pointer_offset);
			}

			// Check to see if this is a blittable field according to the options
			if (options->blittable_types.size > 0) {
				unsigned int index = function::FindString(field->definition, options->blittable_types, [](CompareReflectionTypeInstanceBlittableType blittable_type) {
					return blittable_type.field_definition;
				});
				if (index != -1) {
					// Check to see if the stream type is matched
					if (field->info.stream_type == options->blittable_types[index].stream_type) {
						// Blittable type - use memcmp
						return memcmp(first, second, field->info.byte_size) == 0;
					}
				}
			}

			// Check if the field has given byte size
			ulong2 given_size = GetReflectionTypeGivenFieldTag(field);
			if (given_size.x != -1) {
				return memcmp(first, second, given_size.x) == 0;
			}

			if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
				size_t element_byte_size = field->info.byte_size;

				// The size is the actual count
				Stream<void> first_stream;
				Stream<void> second_stream;
				if (field->info.stream_type == ReflectionStreamFieldType::Basic) {
					first_stream = { first, 1 };
					second_stream = { second, 1 };
				}
				else if (field->info.stream_type == ReflectionStreamFieldType::Pointer) {
					unsigned char pointer_indirection = GetReflectionFieldPointerIndirection(field->info);
					while (pointer_indirection > 0) {
						first = *(void**)first;
						second = *(void**)second;
						pointer_indirection--;
					}
					first_stream = { first, 1 };
					second_stream = { second, 1 };
				}
				else if (field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray || IsStream(field->info.stream_type)) {
					first_stream = GetReflectionFieldStreamVoidEx(field->info, first, false);
					second_stream = GetReflectionFieldStreamVoidEx(field->info, second, false);
					element_byte_size = field->info.stream_byte_size;
				}
				else {
					// Invalid type
					ECS_ASSERT(false, "Invalid stream type when trying to compare 2 reflection type fields.");
				}

				if (first_stream.size != second_stream.size) {
					return false;
				}

				if (first_stream.buffer == second_stream.buffer) {
					return true;
				}

				return CompareReflectionTypeInstances(reflection_manager, field->definition, first_stream.buffer, second_stream.buffer, first_stream.size, options);
			}
			else {
				// Can forward to the field info variant
				return CompareReflectionFieldInfoInstances(&field->info, first, second, false);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* type, 
			const void* first, 
			const void* second,
			const CompareReflectionTypeInstancesOptions* options
		)
		{
			bool is_blittable = IsBlittable(type);
			if (is_blittable) {
				size_t byte_size = GetReflectionTypeByteSize(type);
				return memcmp(first, second, byte_size) == 0;
			}
			else {
				// Compare t

				// Not blittable
				// Has some pointer data that needs to be checked
				for (size_t index = 0; index < type->fields.size; index++) {
					if (!CompareReflectionFieldInstances(reflection_manager, type->fields.buffer + index, first, second, true, options)) {
						return false;
					}
				}
				return true;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager, 
			Stream<char> definition, 
			const void* first, 
			const void* second, 
			size_t count,
			const CompareReflectionTypeInstancesOptions* options
		)
		{
			// Try with a reflection type
			ReflectionType nested_type;
			if (reflection_manager->TryGetType(definition, nested_type)) {
				// Check to see if it is blittable
				bool is_blittable = IsBlittable(&nested_type);
				size_t nested_byte_size = GetReflectionTypeByteSize(&nested_type);

				if (is_blittable) {
					return memcmp(first, second, count * nested_byte_size) == 0;
				}
				else {
					const void* current_first = first;
					const void* current_second = second;
					for (size_t index = 0; index < count; index++) {
						if (!CompareReflectionTypeInstances(reflection_manager, &nested_type, current_first, current_second, options)) {
							return false;
						}
						current_first = function::OffsetPointer(current_first, nested_byte_size);
						current_second = function::OffsetPointer(current_second, nested_byte_size);
					}
					return true;
				}
			}
			else {
				// Might be a custom type
				unsigned int custom_index = FindReflectionCustomType(definition);
				if (custom_index != -1) {
					ReflectionCustomTypeByteSizeData byte_size_data;
					byte_size_data.definition = definition;
					byte_size_data.reflection_manager = reflection_manager;
					ulong2 byte_size = ECS_REFLECTION_CUSTOM_TYPES[custom_index].byte_size(&byte_size_data);

					ReflectionCustomTypeIsBlittableData is_blittable_data;
					is_blittable_data.definition = definition;
					is_blittable_data.reflection_manager = reflection_manager;
					bool is_blittable = ECS_REFLECTION_CUSTOM_TYPES[custom_index].is_blittable(&is_blittable_data);
					if (is_blittable) {
						return memcmp(first, second, count * byte_size.x) == 0;
					}
					else {
						ReflectionCustomTypeCompareData compare_data;
						compare_data.definition = definition;
						compare_data.second = second;
						compare_data.first = first;
						compare_data.reflection_manager = reflection_manager;
						for (size_t index = 0; index < count; index++) {
							if (!ECS_REFLECTION_CUSTOM_TYPES[custom_index].compare(&compare_data)) {
								return false;
							}
							compare_data.first = function::OffsetPointer(compare_data.first, byte_size.x);
							compare_data.second = function::OffsetPointer(compare_data.second, byte_size.x);
						}
						return true;
					}
				}
				else {
					// Not a custom type or user defined. Check blittable types
					ulong2 blittable_type = reflection_manager->FindBlittableException(definition);
					if (blittable_type.x != -1) {
						return memcmp(first, second, count * blittable_type.x) == 0;
					}
					else {
						// Unrecognized type - assert false
						ECS_ASSERT(false);
						return false;
					}
				}
			}

			// Shouldn't be reached
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void CopyReflectionTypeToNewVersion(
			const ReflectionManager* old_reflection_manager,
			const ReflectionManager* new_reflection_manager,
			const ReflectionType* old_type, 
			const ReflectionType* new_type, 
			const void* old_data, 
			void* new_data,
			AllocatorPolymorphic allocator,
			bool always_allocate_for_buffers
		)
		{
			if (always_allocate_for_buffers) {
				ECS_ASSERT(allocator.allocator != nullptr);
			}

			// Go through the new type and try to get the data
			for (size_t new_index = 0; new_index < new_type->fields.size; new_index++) {
				// Check the remapping tag
				Stream<char> mapping_tag = new_type->fields[new_index].GetTag(STRING(ECS_MAP_FIELD_REFLECTION));
				Stream<char> name_to_search = new_type->fields[new_index].name;
				if (mapping_tag.size > 0) {
					name_to_search = function::FindDelimitedString(mapping_tag, '(', ')', true);
				}
				
				unsigned int field_index = old_type->FindField(new_type->fields[new_index].name);
				if (field_index != -1) {
					ConvertReflectionFieldToOtherField(
						old_reflection_manager,
						new_reflection_manager,
						&old_type->fields[field_index],
						&new_type->fields[new_index],
						old_data,
						new_data,
						allocator,
						always_allocate_for_buffers
					);
				}
				else {
					// Use the default
					if (new_reflection_manager != nullptr) {
						new_reflection_manager->SetInstanceFieldDefaultData(&new_type->fields[new_index], new_data);	
					}
					else {
						SetInstanceFieldDefaultData(&new_type->fields[new_index], new_data);
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ConvertReflectionBasicField(
			ReflectionBasicFieldType first_type, 
			ReflectionBasicFieldType second_type, 
			const void* first_data, 
			void* second_data,
			size_t count
		)
		{
			unsigned char first_basic_count = BasicTypeComponentCount(first_type);
			unsigned char second_basic_count = BasicTypeComponentCount(second_type);

			double double_data[4] = { 0 };

			first_type = ReduceMultiComponentReflectionType(first_type);
			second_type = ReduceMultiComponentReflectionType(second_type);

			size_t first_byte_size = GetReflectionBasicFieldTypeByteSize(first_type);
			size_t second_byte_size = GetReflectionBasicFieldTypeByteSize(second_type);

#define CASE(reflection_type, normal_type) case ReflectionBasicFieldType::reflection_type: { \
				normal_type* typed_first_data = (normal_type*)first_data; \
				for (unsigned char subindex = 0; subindex < first_basic_count; subindex++) { \
					double_data[subindex] = *typed_first_data; \
					typed_first_data++;\
				} \
			} break;

#define SWITCH  \
			switch (first_type) {  \
				CASE(Bool, bool); \
				CASE(Enum, char); \
				CASE(Float, float); \
				CASE(Double, double); \
				CASE(Int8, int8_t); \
				CASE(UInt8, uint8_t); \
				CASE(Int16, int16_t); \
				CASE(UInt16, uint16_t); \
				CASE(Int32, int32_t); \
				CASE(UInt32, uint32_t); \
				CASE(Int64, int64_t); \
				CASE(UInt64, uint64_t); \
				CASE(Wchar_t, wchar_t); \
			}

			for (size_t index = 0; index < count; index++) {
				SWITCH;

#undef CASE

#define CASE(reflection_type, normal_type) case ReflectionBasicFieldType::reflection_type: { \
				normal_type* typed_second_data = (normal_type*)second_data; \
				for (unsigned char subindex = 0; subindex < second_basic_count; subindex++) { \
					*typed_second_data = (normal_type)double_data[subindex]; \
					typed_second_data++;\
				} \
			} break;

				SWITCH;

				first_data = function::OffsetPointer(first_data, first_byte_size);
				second_data = function::OffsetPointer(second_data, second_byte_size);
			}

#undef CASE
#undef SWITCH

		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ConvertReflectionFieldToOtherField(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionField* first_info, 
			const ReflectionField* second_info, 
			const void* first_data, 
			void* second_data,
			AllocatorPolymorphic allocator,
			bool always_allocate_for_buffers
		)
		{
			if (always_allocate_for_buffers) {
				ECS_ASSERT(allocator.allocator != nullptr);
			}

			// If they have the same type, go ahead and try to copy
			if (first_info->Compare(*second_info)) {
				// If not a user defined type, can copy it
				if (first_info->info.basic_type != ReflectionBasicFieldType::UserDefined) {
					if (first_info->info.stream_type != ReflectionStreamFieldType::BasicTypeArray) {
						if (always_allocate_for_buffers) {
							ResizableStream<void> previous_data = GetReflectionFieldResizableStreamVoid(first_info->info, first_data);
							size_t allocation_size = (size_t)second_info->info.stream_byte_size * previous_data.size;
							void* allocation = Allocate(allocator, allocation_size);
							memcpy(allocation, previous_data.buffer, allocation_size);

							previous_data.buffer = allocation;
							previous_data.capacity = previous_data.size;
							SetReflectionFieldResizableStreamVoid(second_info->info, second_data, previous_data);
						}
						else {
							memcpy(
								function::OffsetPointer(second_data, second_info->info.pointer_offset),
								function::OffsetPointer(first_data, first_info->info.pointer_offset),
								second_info->info.byte_size
							);
						}
					}
					else {
						unsigned short copy_size = std::min(first_info->info.byte_size, second_info->info.byte_size);
						memcpy(
							function::OffsetPointer(second_data, second_info->info.pointer_offset),
							function::OffsetPointer(first_data, first_info->info.pointer_offset),
							copy_size
						);
						if (copy_size != second_info->info.byte_size) {
							memset(function::OffsetPointer(second_data, copy_size), 0, second_info->info.byte_size - copy_size);
						}
					}
				}
				else {
					ECS_ASSERT(first_reflection_manager != nullptr && second_reflection_manager != nullptr);

					// Check blittable types
					ulong2 first_blittable = first_reflection_manager->FindBlittableException(first_info->definition);
					ulong2 second_blittable = second_reflection_manager->FindBlittableException(second_info->definition);
					if (first_blittable.x != -1 ||  second_blittable.x != -1) {
						if (first_blittable.x == second_blittable.x) {
							// Same definition and both are respectively blittable - blit
							memcpy(
								function::OffsetPointer(second_data, second_info->info.pointer_offset),
								function::OffsetPointer(first_data, first_info->info.pointer_offset),
								first_blittable.x
							);
						}
						else {
							// One is blittable, the other is not
							SetInstanceFieldDefaultData(second_info, second_data);
						}
					}
					else {
						// Call the functor again
						ReflectionType new_nested_type;
						ReflectionType old_nested_type;
						if (first_reflection_manager->TryGetType(first_info->definition, old_nested_type)) {
							if (second_reflection_manager->TryGetType(second_info->definition, new_nested_type)) {
								const void* old_data_field = function::OffsetPointer(first_data, first_info->info.pointer_offset);
								void* new_data_field = function::OffsetPointer(second_data, second_info->info.pointer_offset);
								if (first_info->info.stream_type == ReflectionStreamFieldType::Basic) {
									CopyReflectionTypeToNewVersion(
										first_reflection_manager,
										second_reflection_manager,
										&old_nested_type,
										&new_nested_type,
										old_data_field,
										new_data_field,
										allocator,
										always_allocate_for_buffers
									);
								}
								else {
									ResizableStream<void> old_data = GetReflectionFieldResizableStreamVoidEx(first_info->info, first_data);
									// Check to see if the type has changed. If it didn't, we can just reference it.
									// If it did, we would need to make a new allocation.
									if (CompareReflectionTypes(
										first_reflection_manager,
										second_reflection_manager,
										&old_nested_type,
										&new_nested_type
									)) {
										// Check to see if we need to allocate
										if (always_allocate_for_buffers) {
											size_t allocation_size = (size_t)second_info->info.stream_byte_size * old_data.size;
											void* allocation = Allocate(allocator, allocation_size);
											memcpy(allocation, old_data.buffer, allocation_size);
											old_data.buffer = allocation;
											old_data.capacity = old_data.size;
										}

										// Can set the data directly
										SetReflectionFieldResizableStreamVoidEx(second_info->info, second_data, old_data);
									}
									else {
										// We need to allocate
										if (allocator.allocator == nullptr) {
											// The behaviour is omitted
											second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
										}
										else {
											size_t old_type_byte_size = GetReflectionTypeByteSize(&old_nested_type);
											size_t new_type_byte_size = GetReflectionTypeByteSize(&new_nested_type);

											if (second_info->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
												unsigned int copy_count = std::min(old_data.size, (unsigned int)second_info->info.basic_type_count);
												void* current_data = function::OffsetPointer(second_data, second_info->info.pointer_offset);
												for (unsigned int index = 0; index < copy_count; index++) {
													CopyReflectionTypeToNewVersion(
														first_reflection_manager,
														second_reflection_manager,
														&old_nested_type,
														&new_nested_type,
														function::OffsetPointer(old_data.buffer, old_type_byte_size * index),
														function::OffsetPointer(current_data, new_type_byte_size * index),
														allocator,
														always_allocate_for_buffers
													);
												}
											}
											else {
												size_t allocation_size = (size_t)second_info->info.stream_byte_size * old_data.size;
												void* allocation = Allocate(allocator, allocation_size);
												for (size_t index = 0; index < old_data.size; index++) {
													CopyReflectionTypeToNewVersion(
														first_reflection_manager,
														second_reflection_manager,
														&old_nested_type,
														&new_nested_type,
														function::OffsetPointer(old_data.buffer, old_type_byte_size * index),
														function::OffsetPointer(allocation, new_type_byte_size * index),
														allocator,
														always_allocate_for_buffers
													);
												}
												old_data.buffer = allocation;
												old_data.capacity = old_data.size;
												SetReflectionFieldResizableStreamVoid(second_info->info, second_data, old_data);
											}
										}
									}
								}
							}
							else {
								// Custom type, enum, or error.
								// Call the default instance data
								second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
							}
						}
						else {
							// Check for custom type
							unsigned int custom_type_index = FindReflectionCustomType(second_info->definition);
							if (custom_type_index != -1) {
								// Get the dependent types. If any of these has changed, then revert to default instance field
								ECS_STACK_CAPACITY_STREAM(Stream<char>, dependent_types, 128);
								ReflectionCustomTypeDependentTypesData dependent_data;
								dependent_data.dependent_types = dependent_types;
								dependent_data.definition = second_info->definition;
								ECS_REFLECTION_CUSTOM_TYPES[custom_type_index].dependent_types(&dependent_data);

								unsigned int index = 0;
								for (; index < dependent_data.dependent_types.size; index++) {
									ReflectionType first_type;
									ReflectionType second_type;
									if (first_reflection_manager->TryGetType(dependent_data.dependent_types[index], first_type)) {
										if (second_reflection_manager->TryGetType(dependent_data.dependent_types[index], second_type)) {
											if (!CompareReflectionTypes(
												first_reflection_manager,
												second_reflection_manager,
												&first_type,
												&second_type
											)) {
												// Revert to default data
												break;
											}
										}
										else {
											// Revert to default data
											break;
										}
									}
									else {
										// Revert to default data
										break;
									}
								}

								if (index < dependent_data.dependent_types.size) {
									second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
								}
								else {
									// Nothing has changed. Can copy it
									memcpy(
										function::OffsetPointer(second_data, second_info->info.pointer_offset),
										function::OffsetPointer(first_data, first_info->info.pointer_offset),
										second_info->info.byte_size
									);
								}
							}
							else {
								// Enum or error.
								// Call the default instance data
								second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
							}
						}
					}
				}
			}
			else {
				// Try to copy into this newer version
				// If either one is a user defined type, then proceed with the default data
				if (second_info->info.basic_type == ReflectionBasicFieldType::UserDefined
					|| first_info->info.basic_type == ReflectionBasicFieldType::UserDefined) {
					if (second_reflection_manager != nullptr) {
						second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
					}
					else {
						SetInstanceFieldDefaultData(second_info, second_data);
					}
				}
				else {
					if (second_info->info.stream_type != ReflectionStreamFieldType::Basic && 
						first_info->info.stream_type != ReflectionStreamFieldType::Basic) {
						ResizableStream<void> field = GetReflectionFieldResizableStreamVoid(first_info->info, first_data);
						// If they have the same basic field type
						if (first_info->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
							if (second_info->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
								// Convert if different types, else can just copy
								if (first_info->info.basic_type == second_info->info.basic_type) {
									SetReflectionFieldResizableStreamVoidEx(second_info->info, second_data, field);
								}
								else {
									unsigned short copy_count = std::min(first_info->info.basic_type_count, second_info->info.basic_type_count);
									ConvertReflectionBasicField(
										first_info->info.basic_type,
										second_info->info.basic_type,
										function::OffsetPointer(first_data, first_info->info.pointer_offset),
										function::OffsetPointer(second_data, second_info->info.pointer_offset),
										copy_count
									);

									if (copy_count < second_info->info.basic_type_count) {
										unsigned int element_byte_size = (unsigned int)second_info->info.byte_size / (unsigned int)second_info->info.basic_type_count;
										unsigned int copy_byte_size = element_byte_size * copy_count;
										memset(function::OffsetPointer(second_data, second_info->info.pointer_offset + copy_byte_size), 0, second_info->info.byte_size - copy_byte_size);
									}
								}
							}
							else {
								// Need to allocate
								if (allocator.allocator == nullptr) {
									// Set the default
									if (second_reflection_manager != nullptr) {
										second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
									}
									else {
										SetInstanceFieldDefaultData(second_info, second_data);
									}
								}
								else {
									void* allocation = Allocate(allocator, field.size * (size_t)second_info->info.stream_byte_size);
									ConvertReflectionBasicField(
										first_info->info.basic_type,
										second_info->info.basic_type,
										field.buffer,
										allocation,
										field.size
									);
									field.buffer = allocation;
									field.capacity = field.size;
									SetReflectionFieldResizableStreamVoid(second_info->info, second_data, field);
								}
							}
						}
						else {
							if (first_info->info.basic_type == second_info->info.basic_type) {
								// Can just copy directly
								// Check to see if we need to allocate always
								if (always_allocate_for_buffers) {
									size_t allocation_size = (size_t)second_info->info.stream_byte_size * field.size;
									void* allocation = Allocate(allocator, allocation_size);
									memcpy(allocation, field.buffer, allocation_size);
									field.buffer = allocation;
									field.capacity = field.size;
								}

								SetReflectionFieldResizableStreamVoidEx(second_info->info, second_data, field);
							}
							else {
								// Need to convert
								if (second_info->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
									// No need to allocate, just to convert
									void* write_buffer = function::OffsetPointer(second_data, second_info->info.pointer_offset);
									unsigned int write_count = std::min(field.size, (unsigned int)second_info->info.basic_type_count);
									ConvertReflectionBasicField(
										first_info->info.basic_type,
										second_info->info.basic_type,
										field.buffer,
										write_buffer,
										write_count
									);

									if (write_count != (unsigned int)second_info->info.basic_type_count) {
										// Set the 0 the other fields
										unsigned int write_byte_size = write_count * (unsigned int)second_info->info.byte_size / (unsigned int)second_info->info.basic_type_count;
										memset(
											function::OffsetPointer(
												write_buffer,
												write_byte_size
											),
											0,
											(unsigned int)second_info->info.byte_size - write_byte_size
										);
									}
								}
								else {
									// Need to allocate
									if (allocator.allocator == nullptr) {
										if (second_reflection_manager != nullptr) {
											second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
										}
										else {
											SetInstanceFieldDefaultData(second_info, second_data);
										}
									}
									else {
										void* allocation = Allocate(allocator, field.size * second_info->info.stream_byte_size);
										ConvertReflectionBasicField(
											first_info->info.basic_type,
											second_info->info.basic_type,
											field.buffer,
											allocation,
											field.size
										);
										field.buffer = allocation;
										SetReflectionFieldResizableStreamVoid(second_info->info, second_data, field);
									}
								}
							}
						}
					}
					else {
						// If one is basic and the other one stream, just set the default for the field
						if (second_info->info.stream_type == ReflectionStreamFieldType::Basic && first_info->info.stream_type == ReflectionStreamFieldType::Basic) {
							// Convert the basic field type
							ConvertReflectionBasicField(
								first_info->info.basic_type, 
								second_info->info.basic_type,
								function::OffsetPointer(first_data, first_info->info.pointer_offset), 
								function::OffsetPointer(second_data, second_info->info.pointer_offset)
							);
						}
						else {
							if (second_reflection_manager != nullptr) {
								second_reflection_manager->SetInstanceFieldDefaultData(second_info, second_data);
							}
							else {
								SetInstanceFieldDefaultData(second_info, second_data);
							}
						}
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsReflectionFieldSkipped(const ReflectionField* field)
		{
			return field->Has(STRING(ECS_SKIP_REFLECTION));
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionTypesDifferentFields(
			const ReflectionType* first, 
			const ReflectionType* second, 
			CapacityStream<size_t>* first_surplus_fields, 
			CapacityStream<size_t>* second_surplus_fields
		)
		{
			ECS_ASSERT(first_surplus_fields != nullptr || second_surplus_fields != nullptr);

			auto loop = [](const ReflectionType* first, const ReflectionType* second, CapacityStream<size_t>* surplus) {
				for (size_t index = 0; index < first->fields.size; index++) {
					unsigned int field = second->FindField(first->fields[index].name);
					if (field == -1) {
						surplus->AddAssert(index);
					}
				}
			};

			if (first_surplus_fields != nullptr) {
				loop(first, second, first_surplus_fields);
			}

			if (second_surplus_fields != nullptr) {
				loop(second, first, second_surplus_fields);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ConstructReflectionTypeDependencyGraph(Stream<ReflectionType> types, CapacityStream<Stream<char>>& ordered_types, CapacityStream<uint2>& subgroups)
		{
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, valid_mask, types.size);
			valid_mask.size = types.size;
			function::MakeSequence(valid_mask);

			ECS_STACK_CAPACITY_STREAM(Stream<char>, current_dependencies, 32);

			unsigned int added_types = -1;
			// This will exit even when valid_mask has size 0
			while (added_types != 0) {
				added_types = 0;
				
				uint2 current_subgroup = { ordered_types.size, 0 };
				for (unsigned int index = 0; index < valid_mask.size; index++) {
					for (size_t field_index = 0; field_index < types.size; field_index++) {
						current_dependencies.size = 0;
						GetReflectionFieldDependentTypes(&types[index].fields[field_index], current_dependencies);

						unsigned int dependency_index = 0;
						for (; dependency_index < current_dependencies.size; dependency_index++) {
							// Check to see if this dependency is already met
							if (function::FindString(current_dependencies[dependency_index], ordered_types.ToStream()) == -1) {
								break;
							}
						}

						if (dependency_index == current_dependencies.size) {
							// All dependencies are met - can add it
							ordered_types.AddAssert(types[index].name);
							valid_mask.RemoveSwapBack(index);
							index--;
							added_types++;
							current_subgroup.y++;
						}
					}
				}
			}

			return added_types != 0;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DependsUpon(const ReflectionManager* reflection_manager, const ReflectionType* type, Stream<char> subtype)
		{
			for (size_t index = 0; index < type->fields.size; index++) {
				const Reflection::ReflectionField* field = &type->fields[index];
				if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
					if (field->info.stream_type == ReflectionStreamFieldType::Basic ||
						field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
						if (function::CompareStrings(field->definition, subtype)) {
							return true;
						}

						// Check blittable exception
						if (reflection_manager->FindBlittableException(field->definition).x != -1) {
							if (field->definition == subtype) {
								return true;
							}
						}
						else {
							// Check to see if it has the give size tag - if it has it ignore the field
							Stream<char> give_size_tag = field->GetTag(STRING(ECS_GIVE_SIZE_REFLECTION));
							if (give_size_tag.size == 0) {
								// Check to see if it is a nested type or a custom serializer
								ReflectionType nested_type;
								if (reflection_manager->TryGetType(field->definition, nested_type)) {
									if (DependsUpon(reflection_manager, &nested_type, subtype)) {
										return true;
									}
								}
								else {
									// check custom serializer
									unsigned int custom_serializer_index = FindReflectionCustomType(field->definition);
									ECS_ASSERT(custom_serializer_index != -1);

									ECS_STACK_CAPACITY_STREAM(Stream<char>, dependent_types, 64);
									ReflectionCustomTypeDependentTypesData dependent_data;
									dependent_data.definition = field->definition;
									dependent_data.dependent_types = dependent_types;
									ECS_REFLECTION_CUSTOM_TYPES[custom_serializer_index].dependent_types(&dependent_data);

									for (unsigned int subindex = 0; subindex < dependent_data.dependent_types.size; subindex++) {
										if (function::CompareStrings(subtype, dependent_data.dependent_types[subindex])) {
											return true;
										}
									}
								}
							}
						}
					}
					else {
						// Pointer or stream
						// Must contain in the definition the subtype
						if (function::FindFirstToken(type->fields[index].definition, subtype).size > 0) {
							return true;
						}
					}
				}
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 GetReflectionTypeGivenFieldTag(const ReflectionField* field)
		{
			Stream<char> tag = field->GetTag(STRING(ECS_GIVE_SIZE_REFLECTION));
			if (tag.size == 0) {
				return { (size_t)-1, (size_t)-1 };
			}

			Stream<char> comma = function::FindFirstCharacter(tag, ',');

			ulong2 result;
			result.y = 8; // assume alignment 8
			if (comma.size > 0) {
				// Parse the alignment
				result.y = function::ConvertCharactersToInt(comma);
				ECS_ASSERT(result.y == 1 || result.y == 2 || result.y == 4 || result.y == 8);
				tag = { tag.buffer, tag.size - comma.size };
			}
			
			result.x = function::ConvertCharactersToInt(tag);
			if (result.x < 8) {
				result.y = function::PowerOfTwoGreater(result.x) / 2;
			}
			return result;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 GetReflectionFieldSkipMacroByteSize(const ReflectionField* field)
		{
			Stream<char> tag = field->GetTag(STRING(ECS_SKIP_REFLECTION));
			Stream<char> opened_paranthese = function::FindFirstCharacter(tag, '(');
			Stream<char> closed_paranthese = function::FindMatchingParenthesis(opened_paranthese.buffer + 1, tag.buffer + tag.size, '(', ')');

			opened_paranthese.Advance();
			Stream<char> removed_whitespace = function::SkipWhitespace(opened_paranthese);
			if (removed_whitespace.buffer == closed_paranthese.buffer) {
				return { (size_t)-1, (size_t)-1 };
			}

			Stream<char> comma = function::FindFirstCharacter(opened_paranthese, ',');
			size_t byte_size = -1;
			size_t alignment = -1;

			if (comma.size == 0) {
				// No alignment specified
				byte_size = function::ConvertCharactersToInt(opened_paranthese);
			}
			else {
				// Has alignment specified
				byte_size = function::ConvertCharactersToInt(Stream<char>(opened_paranthese.buffer, comma.buffer - opened_paranthese.buffer));
				alignment = function::ConvertCharactersToInt(comma);
			}
			return { byte_size, alignment };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionFieldDependentTypes(const ReflectionField* field, CapacityStream<Stream<char>>& dependencies)
		{
			// Only user defined or custom types can have dependencies
			if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
				unsigned int custom_type = FindReflectionCustomType(field->definition);
				if (custom_type != -1) {
					ReflectionCustomTypeDependentTypesData dependent_types;
					dependent_types.definition = field->definition;
					dependent_types.dependent_types = dependencies;
					ECS_REFLECTION_CUSTOM_TYPES[custom_type].dependent_types(&dependent_types);
					dependencies.size += dependent_types.dependent_types.size;
				}
				else {
					dependencies.AddAssert(field->definition);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionFieldDependentTypes(Stream<char> definition, CapacityStream<Stream<char>>& dependencies)
		{
			unsigned int custom_type = FindReflectionCustomType(definition);
			if (custom_type != -1) {
				ReflectionCustomTypeDependentTypesData dependent_types;
				dependent_types.definition = definition;
				dependent_types.dependent_types = dependencies;
				ECS_REFLECTION_CUSTOM_TYPES[custom_type].dependent_types(&dependent_types);
				dependencies.size += dependent_types.dependent_types.size;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionDataPointerElementByteSize(const ReflectionManager* manager, Stream<char> tag)
		{
			tag = function::FindFirstCharacter(tag, '(');
			if (tag.size == 0) {
				return -1;
			}
			tag.buffer += 1;
			// The last character is a closed parenthese
			tag.size -= 2;

			size_t user_defined_type = SearchReflectionUserDefinedTypeByteSize(manager, tag);
			if (user_defined_type != -1) {
				return user_defined_type;
			}

			// Explicit number given
			return function::ConvertCharactersToInt(tag);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionTypeDependentTypes(const ReflectionManager* manager, const ReflectionType* type, CapacityStream<Stream<char>>& dependent_types)
		{
			for (size_t index = 0; index < type->fields.size; index++) {
				if (type->fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// Check to see if it has the size given
					if (GetReflectionTypeGivenFieldTag(&type->fields[index]).x == -1) {
						// Check blittable exception
						ulong2 blittable_index = manager->FindBlittableException(type->fields[index].definition);
						if (blittable_index.x == -1) {
							// Not a blittable type
							// Try nested type, then custom type
							ReflectionType nested_type;
							if (manager->TryGetType(type->fields[index].definition, nested_type)) {
								dependent_types.AddAssert(nested_type.name);
								// Retrieve its dependent types as well
								GetReflectionTypeDependentTypes(manager, &nested_type, dependent_types);
							}
							else {
								// Try custom type
								unsigned int custom_type_index = FindReflectionCustomType(type->fields[index].definition);
								if (custom_type_index != -1) {
									ReflectionCustomTypeDependentTypesData dependent_data;
									dependent_data.definition = type->fields[index].definition;
									dependent_data.dependent_types = dependent_types;
									ECS_REFLECTION_CUSTOM_TYPES[custom_type_index].dependent_types(&dependent_data);
									dependent_types.size = dependent_data.dependent_types.size;
								}
								else {
									// Possibly an error, could not identify what type this is
									ECS_ASSERT(false);
								}
							}
						}
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
