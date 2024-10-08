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
#include "../FilePreprocessor.h"
#include "../StreamUtilities.h"
#include "../EvaluateExpression.h"

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
		
		// This structure differs in the sense that it
		// Contains the names of the fields, not the indices
		struct ReflectionTypeMiscNamedSoa {
			Stream<char> type_name;
			Stream<char> name;
			Stream<char> size_field;
			Stream<char> capacity_field;
			Stream<char> parallel_streams[_countof(ReflectionTypeMiscSoa::parallel_streams)];
			unsigned char parallel_stream_count;
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

		// All besides misc asset data are pointers
		ReflectionRuntimeComponentKnownType ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE[] = {
			{ STRING(CoalescedMesh), sizeof(void*) },
			{ STRING(ResourceView), sizeof(void*) },
			{ STRING(SamplerState), sizeof(void*) },
			{ STRING(VertexShader), sizeof(void*) },
			{ STRING(PixelShader), sizeof(void*) },
			{ STRING(ComputeShader), sizeof(void*) },
			{ STRING(Material), sizeof(void*) },
			{ STRING(MiscAssetData), sizeof(Stream<void>) }
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
			for (size_t index = 0; index < ECS_COUNTOF(ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_FUNCTIONS); index++) {
				if (data.function_name == ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_FUNCTIONS[index]) {
					data.string_to_add->CopyOther(STRING(ECS_EVALUATE_FUNCTION_REFLECT) " ");
					break;
				}
			}
			return REFLECTION_TYPE_TAG_HANDLER_FOR_FUNCTION_APPEND_BEFORE_NAME;
		}

		TYPE_TAG_HANDLER_FOR_FIELD(TypeTagComponent) {
			for (size_t index = 0; index < ECS_COUNTOF(ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE); index++) {
				if (data.definition == ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE[index].string) {
					data.string_to_add->CopyOther(STRING(ECS_GIVE_SIZE_REFLECTION));
					data.string_to_add->Add('(');
					ConvertIntToChars(*data.string_to_add, ECS_REFLECTION_RUNTIME_COMPONENT_KNOWN_TYPE[index].byte_size);
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

			Stream<char> parenthese = FindFirstCharacter(type_body, '(');
			while (parenthese.size > 0) {
				Stream<char> previous_line_range = { last_copied_value, PointerDifference(parenthese.buffer, last_copied_value) };
				Stream<char> previous_new_line = FindCharacterReverse(previous_line_range, '\n');
				Stream<char> next_new_line = FindFirstCharacter(parenthese, '\n');

				if (previous_new_line.buffer != nullptr && next_new_line.buffer != nullptr) {
					Stream<char> semicolon_range = { previous_new_line.buffer, PointerDifference(next_new_line.buffer, previous_new_line.buffer) };
					Stream<char> semicolon = FindFirstCharacter(semicolon_range, ';');
					if (semicolon.buffer == nullptr) {
						// This is a function definition
						// Get the name
						const char* name_end = SkipWhitespace(parenthese.buffer - 1, -1);
						if (name_end <= previous_new_line.buffer) {
							// An error, this is thought to be a function but the name is malformed
							return {};
						}

						const char* name_start = SkipCodeIdentifier(name_end, -1) + 1;
						if (name_start > name_end) {
							// An error, this is thought to be a function but the name is malformed
							return {};
						}

						// Get the body of the function
						Stream<char> function_body_start = FindFirstCharacter(parenthese, '{');
						if (function_body_start.size == 0) {
							// An error, malformed function or something else
							return {};
						}

						const char* function_body_end = FindMatchingParenthesis(function_body_start.buffer + 1, function_body_start.buffer + function_body_start.size, '{', '}');
						if (function_body_end == nullptr) {
							// An error, malformed function or something else
							return {};
						}

						Stream<char> closing_parenthese = FindFirstCharacter(parenthese, ')');
						if (closing_parenthese.size == 0 || closing_parenthese.buffer > function_body_start.buffer) {
							// An error, malformed function or something else
							return {};
						}

						// Go and remove all semicolons inside the function body such that it won't interfere with the field determination
						Stream<char> current_function_body = { function_body_start.buffer, PointerDifference(function_body_end, function_body_start.buffer) + 1 };
						ReplaceCharacter(current_function_body, ';', FUNCTION_REPLACE_SEMICOLON_CHAR);

						ECS_STACK_CAPACITY_STREAM(char, string_to_add, ECS_KB);
						Stream<char> function_name = { name_start, PointerDifference(name_end, name_start) + 1 };
						ReflectionTypeTagHandlerForFunctionData type_tag_function;
						type_tag_function.function_name = function_name;
						type_tag_function.string_to_add = &string_to_add;
						auto tag_position = ECS_REFLECTION_TYPE_TAG_HANDLER[handler_index].function_functor(type_tag_function);

						char* copy_start = parse_data->thread_memory.buffer + parse_data->thread_memory.size;

						// Copy everything until the start of the previous line of the function
						Stream<char> before_function_copy = { last_copied_value, PointerDifference(previous_new_line.buffer, last_copied_value) };
						before_function_copy.CopyTo(copy_start);
						copy_start += before_function_copy.size;
						parse_data->thread_memory.size += before_function_copy.size;

						if (string_to_add.size > 0) {
							const char* prename_region_end = SkipWhitespace(previous_new_line.buffer + 1);
							Stream<char> prename_region = { last_copied_value, PointerDifference(prename_region_end, last_copied_value) };

							Stream<char> name_and_qualifiers = { prename_region_end, PointerDifference(closing_parenthese.buffer, prename_region_end) + 1 };

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
							Stream<char> copy_range = { previous_new_line.buffer + 1, PointerDifference(function_body_end, previous_new_line.buffer + 1) + 1 };
							copy_range.CopyTo(copy_start);
							parse_data->thread_memory.size += copy_range.size;
							last_copied_value = function_body_end + 1;
						}
					}
					else {
						last_copied_value = next_new_line.buffer;
					}
				}

				Stream<char> search_next_function = { last_copied_value, PointerDifference(type_body.buffer + type_body.size, last_copied_value) };
				parenthese = FindFirstCharacter(search_next_function, '(');
			}

			// Now go through all lines that have semicolons remaining and see if they have fields
			ECS_STACK_ADDITION_STREAM(unsigned int, semicolons, 512);
			FindToken(type_body, ';', semicolons_addition);

			for (unsigned int index = 0; index < semicolons.size; index++) {
				// If the line is empty, skip it
				Stream<char> previous_range = { type_body.buffer, semicolons[index] };
				Stream<char> next_range = { type_body.buffer + semicolons[index], type_body.size - semicolons[index] };

				const char* previous_line = FindCharacterReverse(previous_range, '\n').buffer;
				const char* next_line = FindFirstCharacter(next_range, '\n').buffer;

				if (previous_line == nullptr || next_line == nullptr) {
					// Fail
					return {};
				}

				// Find the definition and the name
				const char* definition_start = SkipWhitespace(previous_line + 1);
				const char* initial_definition_start = definition_start;
				// If there are double colons for namespace separation, account for them
				Stream<char> definition_range = { definition_start, PointerDifference(next_line, definition_start) };
				Stream<char> double_colon = FindFirstToken(definition_range, "::");
				while (double_colon.size > 0) {
					double_colon.Advance(2);
					definition_start = SkipWhitespace(double_colon.buffer);
					double_colon = FindFirstToken(double_colon, "::");
				}

				const char* definition_end = SkipCodeIdentifier(definition_start);
				Stream<char> definition = { definition_start, PointerDifference(definition_end, definition_start) };

				// Skip pointer asterisks on both ends
				const char* name_start = SkipWhitespace(definition_end);
				name_start = SkipCharacter(name_start, '*');
				name_start = SkipWhitespace(name_start);
				name_start = SkipCharacter(name_start, '*');

				const char* name_end = SkipCodeIdentifier(name_start);
				Stream<char> name = { name_start, PointerDifference(name_end, name_start) };

				ECS_STACK_CAPACITY_STREAM(char, string_to_add, 512);
				ReflectionTypeTagHandlerForFieldData type_tag_field;
				type_tag_field.definition = definition;
				type_tag_field.field_name = name;
				type_tag_field.string_to_add = &string_to_add;
				auto position_to_add = ECS_REFLECTION_TYPE_TAG_HANDLER[handler_index].field_functor(type_tag_field);

				// Include the new lines
				Stream<char> before_type_range = { previous_line, PointerDifference(initial_definition_start, previous_line) };
				Stream<char> after_type_range = { initial_definition_start, PointerDifference(definition_end, initial_definition_start) };
				Stream<char> before_name_range = { definition_end, PointerDifference(name_start, definition_end) };
				Stream<char> after_name_range = { name_start, PointerDifference(next_line, name_start) + 1 };

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
					Stream<char> copy_range = { previous_line, PointerDifference(next_line, previous_line) + 1 };
					copy_range.CopyTo(copy_pointer);
					parse_data->thread_memory.size += copy_range.size;
				}
			}

			char* last_parenthese = parse_data->thread_memory.buffer + parse_data->thread_memory.size;
			*last_parenthese = '}';
			parse_data->thread_memory.size++;
			parse_data->thread_memory.AssertCapacity();

			Stream<char> return_value = { region_start, PointerDifference(parse_data->thread_memory.buffer + parse_data->thread_memory.size, region_start) };
			// Replace any semicolons that were previously inside functions with their original semicolon
			ReplaceCharacter(return_value, FUNCTION_REPLACE_SEMICOLON_CHAR, ';');
			return return_value;
		}

#pragma endregion

#pragma region Container types

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data)
		{
			Stream<char> opened_bracket = FindFirstCharacter(data->definition, '<');
			ECS_ASSERT(opened_bracket.buffer != nullptr);

			Stream<char> closed_bracket = FindCharacterReverse(opened_bracket, '>');
			ECS_ASSERT(closed_bracket.buffer != nullptr);

			// Check to see if it is a trivial type or not - do not report trivial types as dependent types
			Stream<char> type_name = { opened_bracket.buffer + 1, PointerDifference(closed_bracket.buffer, opened_bracket.buffer) - 1 };
			ReflectionBasicFieldType basic_field_type;
			ReflectionStreamFieldType stream_field_type;
ConvertStringToPrimitiveType(type_name, basic_field_type, stream_field_type);
if (basic_field_type == ReflectionBasicFieldType::Unknown || basic_field_type == ReflectionBasicFieldType::UserDefined
	|| stream_field_type == ReflectionStreamFieldType::Unknown) {
	// One more exception here, void, treat it as a special case
	if (type_name != "void") {
		data->dependent_types.AddAssert(type_name);
	}
}
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
			Stream<char> opened_bracket = FindFirstCharacter(definition, '<');
			Stream<char> closed_bracket = FindCharacterReverse(definition, '>');
			ECS_ASSERT(opened_bracket.buffer != nullptr && closed_bracket.buffer != nullptr);

			Stream<char> type = { opened_bracket.buffer + 1, PointerDifference(closed_bracket.buffer, opened_bracket.buffer) - 1 };
			return type;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma region Stream

		bool StreamCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
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

		ulong2 StreamCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
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

		void StreamCustomTypeInterface::GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) {
			ReflectionCustomTypeDependentTypes_SingleTemplate(data);
		}

		bool StreamCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			return false;
		}

		void StreamCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			size_t element_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);
			bool blittable = Reflection::SearchIsBlittable(data->reflection_manager, data->definition);

			ECS_INT_TYPE size_type = ECS_INT_TYPE_COUNT;
			if (data->definition.StartsWith("Stream<")) {
				size_type = ECS_INT64;
			}
			else if (data->definition.StartsWith("CapacityStream<") || data->definition.StartsWith("ResizableStream<")) {
				size_type = ECS_INT32;
			}
			else {
				ECS_ASSERT(false);
			}

			const void* source = *(const void**)data->source;
			void** destination = (void**)data->destination;
			size_t source_size = GetIntValueUnsigned(OffsetPointer(data->source, sizeof(void*)), size_type);

			size_t allocation_size = element_size * source_size;
			void* allocation = nullptr;
			if (allocation_size > 0) {
				allocation = Allocate(data->allocator, allocation_size);
				memcpy(allocation, source, allocation_size);
			}

			if (data->deallocate_existing_data) {
				if (*destination != nullptr) {
					ECSEngine::Deallocate(data->allocator, *destination);
				}
			}

			*destination = allocation;
			SetIntValueUnsigned(OffsetPointer(data->destination, sizeof(void*)), size_type, source_size);

			if (!blittable) {
				size_t count = source_size;
				ReflectionType nested_type;
				CopyReflectionDataOptions copy_options;
				copy_options.allocator = data->allocator;
				copy_options.always_allocate_for_buffers = true;
				copy_options.deallocate_existing_buffers = data->deallocate_existing_data;

				if (data->reflection_manager->TryGetType(template_type, nested_type)) {
					for (size_t index = 0; index < count; index++) {
						CopyReflectionTypeInstance(
							data->reflection_manager,
							&nested_type,
							OffsetPointer(source, index * element_size),
							OffsetPointer(allocation, element_size * index),
							&copy_options
						);
					}
				}
				else {
					unsigned int custom_type_index = FindReflectionCustomType(template_type);
					ECS_ASSERT(custom_type_index != -1);
					data->definition = template_type;

					for (size_t index = 0; index < count; index++) {
						data->source = OffsetPointer(source, index * element_size);
						data->destination = OffsetPointer(allocation, element_size * index);
						ECS_REFLECTION_CUSTOM_TYPES[custom_type_index]->Copy(data);
					}
				}
			}
		}

		bool StreamCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
			Stream<void> first;
			Stream<void> second;

			if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
				first = *(Stream<void>*)data->first;
				second = *(Stream<void>*)data->second;
			}
			// The resizable stream can be type punned with the capacity stream
			else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0 ||
				memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
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

		void StreamCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
			// Deallocate firstly the elements, if they require deallocation
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			// This is a special case. The given source and the given element count
			// Reflect the actual Stream<void> data for this instance.
			// So, we must not iterate
			Stream<void> stream_data = { data->source, data->element_count };

			if (stream_data.size > 0) {
				DeallocateReflectionInstanceBuffers(
					data->reflection_manager,
					template_type,
					stream_data.buffer,
					data->allocator,
					stream_data.size,
					data->element_byte_size,
					data->reset_buffers
				);
			}

			// The main function will call the deallocate for this type
			// Of streams as well. We cannot memset the correct pointer,
			// So leave it to that function to perform the deallocation
			// And the reset

			//if (stream_data.buffer != nullptr) {
			//	DeallocateEx(data->allocator, stream_data.buffer);
			//}
			//// For the resizable stream, we need to memset only the first 3 fields
			//static_assert(sizeof(Stream<void>) == sizeof(CapacityStream<void>));
			//if (data->reset_buffers) {
			//	memset(data->source, 0, sizeof(Stream<void>));
			//}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region SparseSet

		bool SparseSetCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			return ReflectionCustomTypeMatchTemplate(data, "SparseSet") || ReflectionCustomTypeMatchTemplate(data, "ResizableSparseSet");
		}

		ulong2 SparseSetCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			if (memcmp(data->definition.buffer, "SparseSet<", sizeof("SparseSet<") - 1) == 0) {
				return { sizeof(SparseSet<char>), alignof(SparseSet<char>) };
			}
			else {
				return { sizeof(ResizableSparseSet<char>), alignof(ResizableSparseSet<char>) };
			}
		}

		void SparseSetCustomTypeInterface::GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) {
			ReflectionCustomTypeDependentTypes_SingleTemplate(data);
		}

		bool SparseSetCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			return false;
		}

		void SparseSetCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			size_t element_byte_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);

			if (data->deallocate_existing_data) {
				SparseSetDeallocateUntyped(data->destination, data->allocator);
			}

			bool blittable = SearchIsBlittable(data->reflection_manager, template_type);
			if (blittable) {
				// Can blit the data type
				SparseSetCopyTypeErased(data->source, data->destination, element_byte_size, data->allocator);
			}
			else {
				data->definition = template_type;
				struct CopyData {
					CopyReflectionDataOptions* copy_type_options;
					ReflectionCustomTypeCopyData* custom_data;
					const ReflectionType* type;
					unsigned int custom_type_index;
				};

				// Need to copy each and every element
				auto copy_function = [](const void* source, void* destination, AllocatorPolymorphic allocator, void* extra_data) {
					CopyData* data = (CopyData*)extra_data;
					if (data->type != nullptr) {
						CopyReflectionTypeInstance(data->custom_data->reflection_manager, data->type, source, destination, data->copy_type_options);
					}
					else {
						data->custom_data->source = source;
						data->custom_data->destination = destination;
						ECS_REFLECTION_CUSTOM_TYPES[data->custom_type_index]->Copy(data->custom_data);
					}
				};

				CopyReflectionDataOptions copy_type_options;
				copy_type_options.allocator = data->allocator;
				copy_type_options.deallocate_existing_buffers = data->deallocate_existing_data;
				copy_type_options.always_allocate_for_buffers = true;
				CopyData copy_data;
				copy_data.custom_data = data;
				copy_data.type = nullptr;
				copy_data.copy_type_options = &copy_type_options;

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

		bool SparseSetCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
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

		void SparseSetCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
			if (data->definition.StartsWith("SparseSet<")) {
				alignas(void*) char set_memory[sizeof(SparseSet<char>)];
				for (size_t index = 0; index < data->element_count; index++) {
					void* current_source = OffsetPointer(data->source, index * data->element_byte_size);
					if (!data->reset_buffers) {
						memcpy(set_memory, current_source, sizeof(set_memory));
					}
					SparseSetDeallocateUntyped(current_source, data->allocator);
					if (!data->reset_buffers) {
						memcpy(current_source, set_memory, sizeof(set_memory));
					}
				}
			}
			else if (data->definition.StartsWith("ResizableSparseSet<")) {
				// It is fine to type pun to any type
				alignas(void*) char set_memory[sizeof(ResizableSparseSet<char>)];
				for (size_t index = 0; index < data->element_count; index++) {
					void* current_source = OffsetPointer(data->source, index * data->element_byte_size);
					ResizableSparseSet<char>* set = (ResizableSparseSet<char>*)current_source;
					if (!data->reset_buffers) {
						memcpy(set_memory, current_source, sizeof(set_memory));
					}
					set->FreeBuffer();
					if (!data->reset_buffers) {
						memcpy(current_source, set_memory, sizeof(set_memory));
					}
				}
			}
			else {
				ECS_ASSERT(false);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region DataPointer

		bool DataPointerCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			return STRING(DataPointer) == data->definition;
		}

		ulong2 DataPointerCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			return { sizeof(DataPointer), alignof(DataPointer) };
		}

		void DataPointerCustomTypeInterface::GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) {}

		bool DataPointerCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			return false;
		}

		void DataPointerCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			const DataPointer* source = (const DataPointer*)data->source;
			DataPointer* destination = (DataPointer*)data->destination;

			if (data->deallocate_existing_data) {
				destination->Deallocate(data->allocator);
			}

			unsigned short source_size = source->GetData();
			void* allocation = nullptr;
			if (source_size > 0) {
				allocation = Allocate(data->allocator, source_size);
				memcpy(allocation, source->GetPointer(), source_size);
			}

			destination->SetData(source_size);
			destination->SetPointer(allocation);
		}

		bool DataPointerCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
			const DataPointer* first = (const DataPointer*)data->first;
			const DataPointer* second = (const DataPointer*)data->second;

			unsigned short first_size = first->GetData();
			unsigned short second_size = second->GetData();
			if (first_size == second_size) {
				return memcmp(first->GetPointer(), second->GetPointer(), first_size) == 0;
			}
			return false;
		}

		void DataPointerCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
			for (size_t index = 0; index < data->element_count; index++) {
				void* current_source = OffsetPointer(data->source, index * data->element_byte_size);
				DataPointer* data_pointer = (DataPointer*)current_source;
				void* pointer = data_pointer->GetPointer();
				if (pointer != nullptr) {
					DeallocateEx(data->allocator, pointer);
				}
				if (data->reset_buffers) {
					data_pointer = nullptr;
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ----------------------------------------------------------------------------------------------------------------------------

		// TODO: move this to another file
		// Must be kept in sync with the ECS_SERIALIZE_CUSTOM_TYPES
		ReflectionCustomTypeInterface* ECS_REFLECTION_CUSTOM_TYPES[] = {
			new StreamCustomTypeInterface(),
			new ReferenceCountedCustomTypeInterface(),
			new SparseSetCustomTypeInterface(),
			new MaterialAssetCustomTypeInterface(),
			new DataPointerCustomTypeInterface()
		};

		static_assert(ECS_COUNTOF(ECS_REFLECTION_CUSTOM_TYPES) == ECS_REFLECTION_CUSTOM_TYPE_COUNT, "ECS_REFLECTION_CUSTOM_TYPES is not in sync");

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int FindReflectionCustomType(Stream<char> definition)
		{
			ReflectionCustomTypeMatchData match_data = { definition };

			for (unsigned int index = 0; index < ECS_COUNTOF(ECS_REFLECTION_CUSTOM_TYPES); index++) {
				if (ECS_REFLECTION_CUSTOM_TYPES[index]->Match(&match_data)) {
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
			data->error_message_lock.Lock();
			data->error_message->AddStream(message);
			if (path_index != -1) {
				ConvertWideCharsToASCII((data->paths[path_index]), *data->error_message);
			}
			data->error_message->Add('\n');
			data->error_message->AssertCapacity();
			data->error_message_lock.Unlock();

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

		void ReflectionManager::GetKnownBlittableExceptions(CapacityStream<BlittableType>* blittable_types, AllocatorPolymorphic allocator)
		{
			void* color_default = Allocate(allocator, sizeof(unsigned char) * 4);
			memset(color_default, UCHAR_MAX, sizeof(unsigned char) * 4);
			// Don't add an additional include just for the Color and ColorFloat types
			blittable_types->AddAssert({ STRING(Color), sizeof(unsigned char) * 4, alignof(unsigned char), color_default });

			float* float_color_default = (float*)Allocate(allocator, sizeof(float) * 4);
			for (unsigned int index = 0; index < 4; index++) {
				float_color_default[index] = 1.0f;
			}
			blittable_types->AddAssert({ STRING(ColorFloat), sizeof(float) * 4, alignof(float), float_color_default });

			// Add the math types now
			// Don't include the headers just for the byte sizes - use the MathTypeSizes.h
			for (size_t index = 0; index < ECS_MATH_STRUCTURE_TYPE_COUNT; index++) {
				MathStructureInfo structure_info = ECS_MATH_STRUCTURE_TYPE_INFOS[index];
				blittable_types->AddAssert({ structure_info.name, structure_info.byte_size, structure_info.alignment, nullptr });
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddKnownBlittableExceptions()
		{
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
			ECS_STACK_CAPACITY_STREAM(BlittableType, blittable_types, 64);
			GetKnownBlittableExceptions(&blittable_types, &stack_allocator);

			for (unsigned int index = 0; index < blittable_types.size; index++) {
				AddBlittableException(blittable_types[index].name, blittable_types[index].byte_size, blittable_types[index].alignment, blittable_types[index].default_data);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddBlittableException(Stream<char> definition, size_t byte_size, size_t alignment, const void* default_data)
		{
			void* allocated_data = Allocate(Allocator(), byte_size);
			if (default_data) {
				memcpy(allocated_data, default_data, byte_size);
			}
			else {
				memset(allocated_data, 0, byte_size);
			}
			blittable_types.Add({ definition, byte_size, alignment, allocated_data });
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddEnum(const ReflectionEnum* enum_, AllocatorPolymorphic allocator)
		{
			ReflectionEnum copied_enum = *enum_;
			if (allocator.allocator != nullptr) {
				copied_enum = enum_->Copy(allocator);
			}
			copied_enum.folder_hierarchy_index = -1;
			enum_definitions.InsertDynamic(Allocator(), copied_enum, ResourceIdentifier(copied_enum.name));
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddEnumsFrom(const ReflectionManager* other)
		{
			other->enum_definitions.ForEachConst([&](const ReflectionEnum& enum_, ResourceIdentifier identifier) {
				AddEnum(&enum_, Allocator());
			});
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddType(const ReflectionType* type, AllocatorPolymorphic allocator, bool coalesced)
		{
			ReflectionType copied_type = *type;
			if (allocator.allocator != nullptr) {
				copied_type = coalesced ? type->CopyCoalesced(allocator) : type->Copy(allocator);
			}
			copied_type.folder_hierarchy_index = -1;
			type_definitions.InsertDynamic(Allocator(), copied_type, ResourceIdentifier(copied_type.name));
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddTypesFrom(const ReflectionManager* other)
		{
			other->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
				AddType(&type, Allocator());
			});
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::AddTypeToHierarchy(const ReflectionType* type, unsigned int folder_hierarchy, AllocatorPolymorphic allocator, bool coalesced)
		{
			type_definitions.InsertDynamic(Allocator(), *type, ResourceIdentifier(type->name));
			folders[folder_hierarchy].added_types.Add({ type->name, allocator, coalesced });
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
					temp_copy.fields.CopyOther(temp_copy.original_fields);
					// Stylized the labels such that they don't appear excessively long
					ReflectionManagerStylizeEnum(temp_copy);

					ReflectionEnum enum_ = temp_copy.CopyTo(ptr);
					enum_.folder_hierarchy_index = folder_index;
					ResourceIdentifier identifier(enum_.name);
					if (enum_definitions.Find(identifier) != -1) {
						// If the enum already exists, fail
						ECS_FORMAT_TEMP_STRING(message, "Enum {#} was already reflect - conflict detected", enum_.name);
						WriteErrorMessage(data, message.buffer, -1);
						FreeFolderHierarchy(folder_index);
						return false;
					}
					enum_definitions.Insert(enum_, identifier);
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

					unsigned int type_index = FindString(embedded_size.reflection_type, data[data_index].types.ToStream(), [](const ReflectionType& type) {
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
			skipped_fields_table.Initialize(&stack_allocator, PowerOfTwoGreater(total_type_count));

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
									ECS_FORMAT_TEMP_STRING(message, "Failed to determine byte size and alignment for | {#} {#}; |, type {#}",
										data_type->fields[field_index].definition, data_type->fields[field_index].name, data_type->name);
									WriteErrorMessage(data, message.buffer, -1);
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
						else if (data_type->fields[field_index].definition.size == 0) {
							// Remove those unneeded fields
							data_type->fields.Remove(field_index);
							field_index--;
						}
					}

					Stream<SkippedField> allocated_skipped_fields = { nullptr, 0 };
					if (current_skipped_fields.size > 0) {
						allocated_skipped_fields.InitializeAndCopy(&stack_allocator, current_skipped_fields);
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
						ECS_FORMAT_TEMP_STRING(message, "The reflection type {#} was already reflected - a conflict was detected", type.name);
						WriteErrorMessage(data, message.buffer, -1);
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
					
					type_definitions.Insert(type, identifier);
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
								field_offset = AlignPointer(field_offset, skipped_fields[skipped_index].alignment);
								field_offset += skipped_fields[skipped_index].byte_size;
							}
						}

						// Align the field
						size_t current_field_alignment = GetFieldTypeAlignmentEx(this, type->fields[field_index]);
						type->fields[field_index].info.pointer_offset = AlignPointer(field_offset, current_field_alignment);
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
						max_alignment = max(max_alignment, (size_t)skipped_fields[skipped_index].alignment);
						if (skipped_fields[skipped_index].field_after == type->fields.size - 1) {
							// Update the byte size total
							size_t aligned_offset = AlignPointer(final_field_offset, skipped_fields[skipped_index].alignment);
							type->byte_size = AlignPointer(
								aligned_offset + skipped_fields[skipped_index].byte_size, 
								max(max_alignment, (size_t)type->alignment)
							);

							final_field_offset = aligned_offset + skipped_fields[skipped_index].byte_size;
						}
					}

					type->alignment = max(type->alignment, (unsigned int)max_alignment);
				}
			};

			// Aligns the type, calculates the cached parameters ignoring the skipped fields and then takes into consideration these
			auto calculate_cached_parameters = [&](ReflectionType* type, Stream<SkippedField> skipped_fields) {
				align_type(type, skipped_fields);
				CalculateReflectionTypeCachedParameters(this, type);
				// Determine the stream byte size as well
				SetReflectionTypeFieldsStreamByteSizeAlignment(this, type);
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
								current_offset = AlignPointer(current_offset, skipped_fields[field_index].alignment);
								current_offset += skipped_fields[field_index].byte_size;
								max_alignment = max(max_alignment, (size_t)skipped_fields[field_index].alignment);
							}

							type->byte_size = AlignPointer(current_offset, max_alignment);
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

											ulong2 byte_size_alignment = GetReflectionTypeGivenFieldTag(field);
											if (byte_size_alignment.x == -1 || byte_size_alignment.y == -1) {
												// Check blittable exception
												byte_size_alignment = FindBlittableException(field->definition);
											}

											if (byte_size_alignment.x == -1 || byte_size_alignment.y == -1) {
												// If its a stream type, we need to update its stream_byte_size
												// Else we need to update the whole current structure
												if (stream_type != ReflectionStreamFieldType::Basic) {
													// Extract the new definition
													if (IsStream(stream_type)) {
														Stream<char> template_start = FindFirstCharacter(field_definition, '<');
														Stream<char> template_end = FindCharacterReverse(field_definition, '>');
														ECS_ASSERT(template_start.buffer != nullptr);
														ECS_ASSERT(template_end.buffer != nullptr);

														// If it is a void stream, don't do anything
														if (memcmp(template_start.buffer + 1, STRING(void), sizeof(STRING(void)) - 1) == 0) {													
															byte_size_alignment = { sizeof(Stream<void>), alignof(Stream<void>) };
														}
														else {
															// This will get determined when using the container custom Stream
															/*char* new_definition = (char*)SkipWhitespace(template_start.buffer + 1);
															field->definition.size = PointerDifference(SkipWhitespace(template_end.buffer - 1, -1), new_definition) + 1;
															field->definition.buffer = new_definition;*/
														}
													}
													else if (IsPointerWithSoA(stream_type)) {
														byte_size_alignment = { sizeof(void*), alignof(void*) };
													}
												}

												unsigned int container_type_index = FindReflectionCustomType(field_definition);

												if (container_type_index != -1) {
													// Get its byte size
													ReflectionCustomTypeByteSizeData byte_size_data;
													byte_size_data.definition = field_definition;
													byte_size_data.reflection_manager = this;
													ulong2 custom_byte_size_alignment = ECS_REFLECTION_CUSTOM_TYPES[container_type_index]->GetByteSize(&byte_size_data);
													byte_size_alignment = custom_byte_size_alignment;
												}
												else {
													// Try to get the user defined type
													ReflectionType nested_type;
													if (TryGetType(field_definition, nested_type)) {
														byte_size_alignment = { GetReflectionTypeByteSize(&nested_type), GetReflectionTypeAlignment(&nested_type) };
													}
												}
											}

											// It means it is not a container nor a user defined which could be referenced
											if (byte_size_alignment.x == -1 || byte_size_alignment.y == -1) {
												// There is a use case where there was nothing to be reflected on a row
												// And the field was omitted but it is empty - in that case just set the byte size to 0
												if (field->definition.size == 0 || field->name.size == 0) {
													byte_size_alignment = { 0, 0 };
												}
												else {
													// Some error has happened
													// Fail
													ECS_FORMAT_TEMP_STRING(message, "Cannot determine user defined type for field | {#} {#}; | for type {#}", field->definition, field->name, type->name);
													WriteErrorMessage(data, message.buffer, -1);
													FreeFolderHierarchy(folder_index);
													return false;
												}
											}

											if (byte_size_alignment.x != 0 && byte_size_alignment.x != 0) {
												if (stream_type == ReflectionStreamFieldType::Basic || stream_type == ReflectionStreamFieldType::BasicTypeArray) {
													if (stream_type == ReflectionStreamFieldType::BasicTypeArray) {
														// Update the byte size
														type->fields[field_index].info.byte_size = type->fields[field_index].info.basic_type_count * byte_size_alignment.x;
														type->fields[field_index].info.stream_alignment = byte_size_alignment.y;
														type->fields[field_index].info.stream_byte_size = byte_size_alignment.x;
													}
													else {
														type->fields[field_index].info.byte_size = byte_size_alignment.x;
													}
												}
												else {
													// For pointer and streams we need to update the stream byte size
													type->fields[field_index].info.stream_byte_size = byte_size_alignment.x;
													type->fields[field_index].info.stream_alignment = byte_size_alignment.y;
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

			size_t difference = PointerDifference((void*)ptr, allocation);
			ECS_ASSERT(difference <= total_memory);

			return evaluate_success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::BlittableExceptionIndex(Stream<char> name) const
		{
			return FindString(name, blittable_types.ToStream(), [](BlittableType type) {
				return type.name;
			});
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
			
			for (unsigned int index = 0; index < blittable_types.size; index++) {
				Deallocate(allocator, blittable_types[index].default_data);
			}

			constants.FreeBuffer();
			field_table.Deallocate(allocator);
			blittable_types.FreeBuffer();
			folders.FreeBuffer();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::CreateFolderHierarchy(Stream<wchar_t> root) {
			unsigned int index = folders.ReserveRange();
			folders[index] = { StringCopy(folders.allocator, root), nullptr };
			folders[index].added_types.Initialize(Allocator(), 0);
			return index;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::CreateStaticFrom(const ReflectionManager* other, AllocatorPolymorphic allocator)
		{
			type_definitions.InitializeSmallFixed(allocator, other->type_definitions.GetCount());
			other->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
				AddType(&type, allocator);
			});
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::CopyEnums(ReflectionManager* other) const
		{
			HashTableCopy<true, false>(enum_definitions, other->enum_definitions, other->Allocator(), [](const ReflectionEnum* enum_, ResourceIdentifier identifier) {
				return enum_->name;
			});
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::CopyTypes(ReflectionManager* other) const
		{
			HashTableCopy<true, false>(type_definitions, other->type_definitions, other->Allocator(), [](const ReflectionType* type, ResourceIdentifier identifier) {
				return type->name;
			});
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::DeallocateStatic(AllocatorPolymorphic allocator) {
			type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
				type.DeallocateCoalesced(allocator);
			});
			type_definitions.Deallocate(allocator);
			type_definitions.Reset();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data)
		{
			Free(data.thread_memory.buffer);
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

				opened_parenthese = SkipWhitespace(opened_parenthese + 1);
				closed_parenthese = (char*)SkipWhitespace(closed_parenthese - 1, -1);

				return Stream<char>(opened_parenthese, PointerDifference(closed_parenthese, opened_parenthese) + 1);
			};

			auto replace_sizeof_alignof_with_value = [&expression](unsigned int offset, double value) {
				ECS_STACK_CAPACITY_STREAM(char, string_value, 64);
				ConvertDoubleToChars(string_value, value, 6);
				
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
						ulong2 byte_size_alignment = ECS_REFLECTION_CUSTOM_TYPES[container_type]->GetByteSize(&byte_size_data);
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
			Stream<char> token = FindFirstToken(expression, "sizeof");
			while (token.buffer != nullptr) {
				unsigned int offset = token.buffer - expression.buffer;
				if (handle_sizeof_alignof(offset, true) == DBL_MAX) {
					return DBL_MAX;
				}

				token = FindFirstToken(expression, "sizeof");
			}

			// The alignof next
			token = FindFirstToken(expression, "alignof");
			while (token.buffer != nullptr) {
				unsigned int offset = token.buffer - expression.buffer;
				if (handle_sizeof_alignof(offset, false) == DBL_MAX) {
					return DBL_MAX;
				}

				token = FindFirstToken(expression, "alignof");
			}
			
			// Now the constants
			for (size_t constant_index = 0; constant_index < constants.size; constant_index++) {
				token = FindFirstToken(expression, constants[constant_index].name);
				while (token.buffer != nullptr) {
					unsigned int offset = token.buffer - expression.buffer;
					token[constants[constant_index].name.size] = '\0';
					replace_sizeof_alignof_with_value(offset, constants[constant_index].value);

					token = FindFirstToken(expression, constants[constant_index].name);
				}
			}

			// Return the evaluation
			return ECSEngine::EvaluateExpression(expression);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 ReflectionManager::FindBlittableException(Stream<char> name) const
		{
			unsigned int index = BlittableExceptionIndex(name);
			return index != -1 ? ulong2(blittable_types[index].byte_size, blittable_types[index].alignment) : ulong2(-1, -1);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::FreeFolderHierarchy(unsigned int folder_index)
		{
			// Start by deallocating all added types
			for (unsigned int index = 0; index < folders[folder_index].added_types.size; index++) {
				AddedType added_type = folders[folder_index].added_types[index];

				const ReflectionType* type = GetType(added_type.type_name);
				if (added_type.coalesced_allocation) {
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
			unsigned int index = FindString(name, constants.ToStream(), [](ReflectionConstant constant) {
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
				if (folders[index].root == hierarchy) {
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
				if (IsPointerWithSoA(type->fields[index].info.stream_type) || IsStream(type->fields[index].info.stream_type)) {
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
			unsigned int write_index = constants.ReserveRange(other->constants.size);
			Stream<ReflectionConstant> new_constants = StreamCoalescedDeepCopy(other->constants.ToStream(), folders.allocator);
			// Set the folder hierarchy index for these constants to -1 such that they don't get bound to any
			// folder index
			for (size_t index = 0; index < new_constants.size; index++) {
				new_constants[index].folder_hierarchy = -1;
				constants[write_index + index] = new_constants[index];
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeFieldTable()
		{
			constexpr size_t table_size = 256;
			void* allocation = Allocate(folders.allocator, ReflectionFieldTable::MemoryOf(table_size));
			field_table.InitializeFromBuffer(allocation, table_size);

			ReflectionFieldInfo field_info;
			memset(&field_info, 0, sizeof(field_info));
			field_info.basic_type_count = 1;

			// Initialize all values, helped by macros
			ResourceIdentifier identifier;
#define BASIC_TYPE(type, _basic_type, _stream_type) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); \
			field_info.basic_type = _basic_type; \
			field_info.stream_type = _stream_type; \
			field_info.byte_size = sizeof(type); \
			field_table.Insert(field_info, identifier);
#define INT_TYPE(type, val) BASIC_TYPE(type, ReflectionBasicFieldType::val, ReflectionStreamFieldType::Basic)
#define COMPLEX_TYPE(type, _basic_type, _stream_type, _byte_size) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); \
			field_info.basic_type = _basic_type; \
			field_info.stream_type = _stream_type; \
			field_info.byte_size = _byte_size; \
			field_table.Insert(field_info, identifier);

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

			// Allocate memory for the type and enum stream; speculate some reasonable numbers
			const size_t max_types = 64;
			const size_t max_enums = 16;
			const size_t path_size = 128;
			const size_t max_constants = 64;
			const size_t max_expressions = 64;
			const size_t max_embedded_array_size = 64;

			data.types.Initialize(folders.allocator, 0, max_types);
			data.enums.Initialize(folders.allocator, 0, max_enums);
			data.paths.Initialize(folders.allocator, path_count);
			data.constants.Initialize(folders.allocator, 0, max_constants);
			data.expressions.Initialize(folders.allocator, max_expressions);
			data.embedded_array_size.Initialize(folders.allocator, 0, max_embedded_array_size);

			void* thread_allocation = Malloc(thread_memory);
			data.thread_memory.InitializeFromBuffer(thread_allocation, 0, thread_memory);
			data.field_table = &field_table;
			data.success = true;
			data.total_memory = 0;
			data.error_message_lock.Clear();
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
			ECS_STACK_CAPACITY_STREAM(char, temp_error_message, 1024);
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

			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, files_storage, ECS_KB);
			AdditionStream<Stream<wchar_t>> files = &files_storage;
			bool status = GetDirectoryFilesWithExtensionRecursive(folders[index].root, allocator, files, GetCppSourceFilesExtensions());
			if (!status) {
				return false;
			}

			return ProcessFolderHierarchyImplementation(this, index, *files.capacity_stream, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(
			unsigned int folder_index, 
			TaskManager* task_manager, 
			CapacityStream<char>* error_message
		) {
			unsigned int thread_count = task_manager->GetThreadCount();

			AllocatorPolymorphic allocator = folders.allocator;

			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, files_storage, ECS_KB);
			AdditionStream<Stream<wchar_t>> files = &files_storage;
			bool status = GetDirectoryFilesWithExtensionRecursive(folders[folder_index].root, allocator, files, GetCppSourceFilesExtensions());
			if (!status) {
				return false;
			}

			// Preparsing the files in order to have thread act only on files that actually need to process
			// Otherwise unequal distribution of files will result in poor multithreaded performance
			unsigned int files_count = files.Size();

			// Process these files on separate threads only if their number is greater than thread count
			if (files_count < thread_count) {
				return ProcessFolderHierarchyImplementation(this, folder_index, *files.capacity_stream, error_message);
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
				reflect_thread_data[thread_index].files = *files.capacity_stream;

				// Launch the task
				ThreadTask task = ECS_THREAD_TASK_NAME(ReflectionManagerHasReflectStructuresThreadTask, reflect_thread_data + thread_index, 0);
				task_manager->AddDynamicTaskAndWake(task);
			}

			reflect_semaphore.SpinWait();

			unsigned int parse_per_thread_paths = path_indices.size / thread_count;
			unsigned int parse_thread_paths_remainder = path_indices.size % thread_count;

			constexpr size_t thread_memory = 10'000'000;
			ConditionVariable condition_variable;

			ECS_STACK_CAPACITY_STREAM(char, temp_error_message, 1024);
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
			unsigned int index = FindString(name, blittable_types.ToStream(), [](BlittableType type) {
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
			// Take into consideration empty types, just to make sure
			if (type->fields.size > 0) {
				for (size_t field_index = 0; field_index < type->fields.size - 1; field_index++) {
					size_t current_offset = type->fields[field_index].info.pointer_offset;
					size_t current_size = type->fields[field_index].info.byte_size;
					size_t next_offset = type->fields[field_index + 1].info.pointer_offset;
					size_t difference = next_offset - current_offset - current_size;
					if (difference > 0) {
						memset(OffsetPointer(data, current_offset + current_size), 0, difference);
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::SetInstanceFieldDefaultData(const ReflectionField* field, void* data, bool offset_data) const
		{
			void* current_field = data;
			if (offset_data) {
				current_field = OffsetPointer(data, field->info.pointer_offset);
			}

			if (field->info.has_default_value) {
				memcpy(
					current_field,
					&field->info.default_bool,
					field->info.byte_size
				);
			}
			else {
				if (field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// Check blittable exceptions
					unsigned int blittable_index = BlittableExceptionIndex(field->definition);
					if (blittable_index == -1) {
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
						// Use the blittable exception default value
						memcpy(current_field, blittable_types[blittable_index].default_data, field->info.byte_size);
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

		void SetInstanceFieldDefaultData(const ReflectionField* field, void* data, bool offset_data)
		{
			void* current_field = data;
			if (offset_data) {
				current_field = OffsetPointer(data, field->info.pointer_offset);
			}

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

			ECS_STACK_CAPACITY_STREAM(unsigned int, words, ECS_KB);
			AdditionStream<unsigned int> words_addition = &words;

			size_t ecs_reflect_size = strlen(STRING(ECS_REFLECT));
			ECS_STACK_CAPACITY_STREAM(char, first_line_characters, 512);

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
					unsigned int first_line_count = ReadFromFile(file, { first_line_characters.buffer, first_line_characters.capacity - 1 });
					if (first_line_count == -1) {
						WriteErrorMessage(data, "Could not read first line. Faulty path: ", index);
						return;
					}
					first_line_characters[first_line_count] = '\0';
					first_line_characters.size = first_line_count;

					// read the first line in order to check if there is something to be reflected	
					// if there is something to be reflected
					Stream<char> first_new_line = FindFirstCharacter(first_line_characters, '\n');
					Stream<char> has_reflect = FindFirstToken(first_line_characters, STRING(ECS_REFLECT));
					if (first_new_line.size > 0 && has_reflect.size > 0 && has_reflect.buffer < first_new_line.buffer) {
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
						content = RemoveSingleLineComment(content, ECS_C_FILE_SINGLE_LINE_COMMENT_TOKEN);
						content = RemoveMultiLineComments(content, ECS_C_FILE_MULTI_LINE_COMMENT_OPENED_TOKEN, ECS_C_FILE_MULTI_LINE_COMMENT_CLOSED_TOKEN);

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
						ECS_STACK_ADDITION_STREAM(unsigned int, constant_offsets, 64);
						FindToken(file_stream, STRING(ECS_CONSTANT_REFLECT), constant_offsets_addition);

						for (size_t constant_index = 0; constant_index < constant_offsets.size; constant_index++) {
							// Look to see that it is a define
							Stream<char> search_space = { file_stream.buffer, constant_offsets[constant_index] };
							const char* last_line = FindCharacterReverse(search_space, '\n').buffer;
							if (last_line == nullptr) {
								// Fail if no new line before it
								WriteErrorMessage(data, "Could not find new line before ECS_CONSTANT_REFLECT. Faulty path: ", index);
								return;
							}

							last_line = SkipWhitespace(last_line + 1);
							// Only if this is a define continue
							if (memcmp(last_line, "#define", sizeof("#define") - 1) == 0) {
								// Get the macro name of the constant
								last_line += sizeof("#define");
								last_line = SkipWhitespace(last_line);

								const char* constant_name_start = last_line;
								const char* constant_name_end = SkipCodeIdentifier(constant_name_start);
								ReflectionConstant constant;
								constant.name = { constant_name_start, PointerDifference(constant_name_end, constant_name_start) };
								
								// Get the value now
								const char* ecs_constant_macro_start = SkipWhitespace(constant_name_end);
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

								opened_parenthese = SkipWhitespace(opened_parenthese + 1);
								closed_parenthese = SkipWhitespace(closed_parenthese - 1, -1);

								Stream<char> value_characters(opened_parenthese, PointerDifference(closed_parenthese, opened_parenthese) + 1);
								// Parse the value now - as a double or a boolean
								char is_boolean = ConvertCharactersToBool(value_characters);
								if (is_boolean == -1) {
									// Not a boolean, parse as double
									constant.value = ConvertCharactersToDouble(value_characters);
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
						FindToken(file_stream, STRING(ECS_REFLECT), words_addition);

						const char* tag_name = nullptr;

						size_t last_position = 0;
						for (size_t word_index = 0; word_index < words.size; word_index++) {
							tag_name = nullptr;

							unsigned int word_offset = words[word_index];

#pragma region Has macro definitions
							// Skip macro definitions - that define tags
							const char* verify_define_char = file_contents + word_offset - 1;
							verify_define_char = SkipWhitespace(verify_define_char, -1);
							verify_define_char = SkipCodeIdentifier(verify_define_char, -1);
							// If it is a pound, it must be a definition - skip it
							if (*verify_define_char == '#') {
								continue;
							}
#pragma endregion

#pragma region Has Comments
							// Skip comments that contain ECS_REFLECT
							//const char* verify_comment_char = file_contents + word_offset - 1;
							//verify_comment_char = SkipWhitespace(verify_comment_char, -1);

							//Stream<char> comment_space = { file_contents, PointerDifference(verify_comment_char, file_contents) + 1 };
							//const char* verify_comment_last_new_line = FindCharacterReverse(comment_space, '\n').buffer;
							//if (verify_comment_last_new_line == nullptr) {
							//	WriteErrorMessage(data, "Last new line could not be found when checking ECS_REFLECT for comment", index);
							//	return;
							//}
							//const char* comment_char = FindCharacterReverse(comment_space, '/').buffer;
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
								closing_parenthese = FindMatchingParenthesis(opening_parenthese + 1, file_contents + bytes_read, '{', '}');
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
									const char* end_tag_name = SkipCodeIdentifier(tag_name);
									Stream<char> current_tag = { tag_name, PointerDifference(end_tag_name, tag_name) };
									for (size_t handler_index = 0; handler_index < ECS_COUNTOF(ECS_REFLECTION_TYPE_TAG_HANDLER); handler_index++) {
										if (ECS_REFLECTION_TYPE_TAG_HANDLER[handler_index].tag == current_tag) {
											Stream<char> new_range = ProcessTypeTagHandler(data, handler_index, Stream<char>(
												opening_parenthese,
												PointerDifference(closing_parenthese, opening_parenthese))
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
									const char* end_tag_name = SkipCodeIdentifier(tag_name);

									if (*end_tag_name == '(') {
										// variable type macro
										end_tag_name = SkipCodeIdentifier(end_tag_name + 1);
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
			ECS_STACK_ADDITION_STREAM(unsigned int, underscores, 128);
			FindToken(enum_.fields[0], '_', underscores_addition);

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
			if (FindFirstToken(enum_.fields[enum_.fields.size - 1], "COUNT").size > 0) {
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
					ECS_STACK_ADDITION_STREAM(unsigned int, current_underscores, 32);
					FindToken(enum_.fields[index], '_', current_underscores_addition);

					unsigned int last_underscore = -1;
					ECS_STACK_ADDITION_STREAM(Stream<char>, partitions, 32);
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
							if (IsNumberCharacter(partitions[partition_index][subindex])) {
								lacks_digits = false;
							}
						}

						if (lacks_digits) {
							for (size_t subindex = 1; subindex < partitions[partition_index].size; subindex++) {
								Uncapitalize(partitions[partition_index].buffer + subindex);
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
			ECS_STACK_ADDITION_STREAM(unsigned int, next_line_positions, 1024);

			const char* dummy_space = strchr(opening_parenthese, '\n') + 1;

			char* closing_paranthese_mutable = (char*)closing_parenthese;
			*closing_paranthese_mutable = '\0';
			FindToken({ dummy_space, PointerDifference(closing_parenthese, dummy_space) }, '\n', next_line_positions_addition);

			// assign the subname stream and assert enough memory to hold the pointers
			uintptr_t ptr = (uintptr_t)data->thread_memory.buffer + data->thread_memory.size;
			uintptr_t before_ptr = ptr;
			ptr = AlignPointer(ptr, alignof(ReflectionEnum));
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

			ECS_STACK_CAPACITY_STREAM(ReflectionTypeMiscNamedSoa, type_named_soa, 32);

			// find next line tokens and exclude the next after the opening paranthese and replace
			// the closing paranthese with \0 in order to stop searching there
			ECS_STACK_ADDITION_STREAM(unsigned int, next_line_positions, 1024);

			// semicolon positions will help in getting the field name
			ECS_STACK_ADDITION_STREAM(unsigned int, semicolon_positions, 512);

			opening_parenthese++;
			char* closing_paranthese_mutable = (char*)closing_parenthese;
			*closing_paranthese_mutable = '\0';

			// Check all expression evaluations
			Stream<char> type_body = { opening_parenthese, PointerDifference(closing_parenthese, opening_parenthese) };
			Stream<char> evaluate_expression_token = FindFirstToken(type_body, STRING(ECS_EVALUATE_FUNCTION_REFLECT));
			while (evaluate_expression_token.buffer != nullptr) {
				// Go down the line to get the name
				// Its right before the parenthesis
				const char* function_parenthese = FindFirstCharacter(evaluate_expression_token, '(').buffer;
				const char* end_function_name = SkipWhitespace(function_parenthese - 1, -1);
				const char* start_function_name = SkipCodeIdentifier(end_function_name, -1) + 1;

				ReflectionExpression expression;
				expression.name = { start_function_name, PointerDifference(end_function_name, start_function_name) + 1 };

				// Get the body now - look for the return
				const char* return_string = FindFirstToken(evaluate_expression_token, "return").buffer;
				ECS_ASSERT(return_string != nullptr);
				return_string += sizeof("return") - 1;

				// Everything until the semicolon is the body
				const char* semicolon = strchr(return_string, ';');
				ECS_ASSERT(semicolon != nullptr);

				// The removal of '\n' will be done when binding the data, for now just reference this part of the code
				expression.body = { return_string, PointerDifference(semicolon, return_string) };
				unsigned int type_table_index = data->expressions.Find(type.name);
				if (type_table_index == -1) {
					// Insert it
					ReflectionExpression* stable_expression = (ReflectionExpression*)OffsetPointer(data->thread_memory.buffer, data->thread_memory.size);
					*stable_expression = expression;
					data->thread_memory.size += sizeof(ReflectionExpression);

					data->expressions.Insert({ stable_expression, 1 }, type.name);
				}
				else {
					// Allocate a new buffer and update the expressions
					ReflectionExpression* expressions = (ReflectionExpression*)OffsetPointer(data->thread_memory.buffer, data->thread_memory.size);
					Stream<ReflectionExpression>* previous_expressions = data->expressions.GetValuePtrFromIndex(type_table_index);
					previous_expressions->CopyTo(expressions);

					data->thread_memory.size += sizeof(ReflectionExpression) * (previous_expressions->size + 1);
					expressions[previous_expressions->size] = expression;

					previous_expressions->buffer = expressions;
					previous_expressions->size++;
				}

				// Get the next expression
				Stream<char> remaining_part = { semicolon, PointerDifference(closing_parenthese, semicolon) };
				evaluate_expression_token = FindFirstToken(remaining_part, STRING(ECS_EVALUATE_FUNCTION_REFLECT));

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
				FindToken({ start, start_size }, '\n', next_line_positions_addition);

				// Get all the semicolons
				FindToken({ start, start_size }, ';', semicolon_positions_addition);

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
						while (first_character_before >= start && IsCodeIdentifierCharacter(*first_character_before)) {
							first_character_before--;
						}
						first_character_before++;

						if (memcmp(first_character_before, STRING(const), sizeof(STRING(const)) - 1) == 0) {
							// We have a const modifier. Exclude it
							first_character_before = SkipWhitespace(first_character_before - 1, -1);
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
							Stream<char> new_opened_bracket = FindFirstCharacter(search_space, '{');
							while (new_opened_bracket.buffer != nullptr) {
								opened_bracket_count++;
								new_opened_bracket.buffer += 1;
								new_opened_bracket.size -= 1;
								new_opened_bracket = FindFirstCharacter(new_opened_bracket, '{');
							}
						};

						Stream<char> search_space = { opened_bracket + 1, PointerDifference(closed_bracket, opened_bracket) - 1 };
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
								search_space = { opened_bracket + 1, PointerDifference(closed_bracket, opened_bracket) - 1 };
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
				ptr = AlignPointer(ptr, alignof(ReflectionField));
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
							// Also, some misc macros could fit in here
							const char* opened_paranthese = strchr(field_start, '(');
							field_end[0] = ';';

							if (opened_paranthese == nullptr) {
								// Now check to see if this line is a typedef. In such cases, simply ignore this line
								bool is_typedef = false;
								const char* first_line_character = SkipWhitespaceEx(field_start);
								if (first_line_character != nullptr) {
									const char* first_word_end = SkipCodeIdentifier(first_line_character);
									Stream<char> first_word = { first_line_character, PointerDifference(first_word_end, first_line_character) / sizeof(char) };
									is_typedef = first_word == "typedef";
								}

								if (!is_typedef) {
									// Check to see if this line contains only const and possibly a semicolon
									// If this is all that there is on the line, consider this to be a const from a method
									bool is_const_method_line = false;
									Stream<char> line = { field_start, PointerDifference(field_end, field_start) / sizeof(*field_start) };
									Stream<char> const_line_start = FindFirstToken(line, "const");
									if (const_line_start.size > 0) {
										const char* const_start = const_line_start.buffer;
										const_start--;
										while (const_start > field_start && (*const_start == '(' || *const_start == ')' || *const_start == '{' ||
											*const_start == '}' || IsWhitespaceEx(*const_start))) {
											const_start--;
										}

										if (const_start == field_start) {
											const char* const_end = const_line_start.buffer + strlen("const");
											const_end++;
											while (const_end < field_end && (*const_end == '(' || *const_end == ')' || *const_end == '{' ||
												*const_end == '}' || IsWhitespaceEx(*const_end))) {
												const_end++;
											}

											// Greater or equal in case const is at the very end of the line, before the semicolon
											is_const_method_line = const_end >= field_end;
										}
									}

									if (!is_const_method_line) {
										ECS_REFLECTION_ADD_TYPE_FIELD_RESULT result = AddTypeField(data, type, pointer_offset, field_start, field_end);

										if (result == ECS_REFLECTION_ADD_TYPE_FIELD_FAILED) {
											WriteErrorMessage(data, "An error occured during field type determination.", file_index);
											return false;
										}
										else if (result == ECS_REFLECTION_ADD_TYPE_FIELD_OMITTED) {
											omitted_fields = true;
										}
									}
								}
							}
							else {
								// Check to see if this is a misc macro
								Stream<char> misc_macro_parse_range = { field_start + 1, PointerDifference(field_end, field_start + 1) / sizeof(char) };
								Stream<char> misc_macro = SkipWhitespaceEx(misc_macro_parse_range);
								if (misc_macro.size > 0) {
									// Check to see if this is a misc macro
									if (misc_macro.StartsWith(STRING(ECS_SOA_REFLECT))) {
										// If this macro is properly written, accept it
										// It needs to have at least 5 arguments - a name,
										// a size field and a capacity field and at least 2
										// parallel streams
										Stream<char> argument_string = GetStringParameter(misc_macro);
										if (argument_string.size > 0) {
											ECS_STACK_CAPACITY_STREAM(Stream<char>, soa_parameters, 32);
											SplitString(argument_string, ',', &soa_parameters);
											if (soa_parameters.size >= 5) {
												// Eliminate leading or trailing whitespace
												for (unsigned int soa_index = 0; soa_index < soa_parameters.size; soa_index++) {
													soa_parameters[soa_index] = SkipWhitespace(soa_parameters[soa_index]);
													soa_parameters[soa_index] = SkipWhitespace(soa_parameters[soa_index], -1);
												}
												
												if (soa_parameters.size <= ECS_COUNTOF(ReflectionTypeMiscNamedSoa::parallel_streams)) {
													// Add a new entry. We don't need to allocate anything since the
													// Names are all stable
													ReflectionTypeMiscNamedSoa named_soa;
													named_soa.type_name = type.name;
													named_soa.name = soa_parameters[0];
													named_soa.size_field = soa_parameters[1];
													named_soa.capacity_field = soa_parameters[2];
													named_soa.parallel_stream_count = soa_parameters.size - 3;
													memcpy(named_soa.parallel_streams, soa_parameters.buffer + 3, soa_parameters.MemoryOf(named_soa.parallel_stream_count));
													type_named_soa.AddAssert(&named_soa);
												}
												else {
													WriteErrorMessage(data, "Invalid SoA type specification.", file_index);
													return false;
												}
											}
											else {
												WriteErrorMessage(data, "Invalid SoA type specification.", file_index);
												return false;
											}
										}
										else {
											WriteErrorMessage(data, "Invalid SoA type specification.", file_index);
											return false;
										}
									}
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
			const char* field_start_macro = FindFirstToken(type_body, STRING(ECS_FIELDS_START_REFLECT)).buffer;
			if (field_start_macro != nullptr) {
				while (field_start_macro != nullptr) {
					// Get the fields end macro if there is one
					char* field_end_macro = (char*)FindFirstToken(
						Stream<char>(field_start_macro, PointerDifference(closing_parenthese, field_start_macro)), 
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
						field_start_macro = FindFirstToken(
							Stream<char>(field_end_macro, PointerDifference(closing_parenthese, field_end_macro)), 
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

			// We can validate the SoA fields here, such that we don't continue in case there
			// Is a mismatch between the fields
			// Allocate memory for the type misc info
			type.misc_info.InitializeFromBuffer(OffsetPointer(data->thread_memory), type_named_soa.size);
			data->thread_memory.size += type_named_soa.MemoryOf(type_named_soa.size);
			data->total_memory += type_named_soa.MemoryOf(type_named_soa.size);
			for (unsigned int index = 0; index < type_named_soa.size; index++) {
				auto output_error = [&](Stream<char> field_type, Stream<char> field_name) {
					ECS_FORMAT_TEMP_STRING(message, "Invalid SoA specification for type {#}: {#} {#} doesn't exist. ", type.name, field_type, field_name);
					WriteErrorMessage(data, message.buffer, file_index);
				};
				auto output_type_error = [&](Stream<char> field_name) {
					ECS_FORMAT_TEMP_STRING(message, "Invalid SoA specification for type {#}: field {#} invalid type {#} for size/capacity. ", type.name, field_name);
					WriteErrorMessage(data, message.buffer, file_index);
				};

				const ReflectionTypeMiscNamedSoa* named_soa = &type_named_soa[index];

				// Build the entry along the way
				ReflectionTypeMiscInfo* misc_soa = &type.misc_info[index];
				misc_soa->type = ECS_REFLECTION_TYPE_MISC_INFO_SOA;
				misc_soa->soa.name = named_soa->name;
				misc_soa->soa.parallel_stream_count = named_soa->parallel_stream_count;

				unsigned int size_field_index = type.FindField(named_soa->size_field);
				if (size_field_index == -1) {
					output_error("size", named_soa->size_field);
					return;
				}
				ECS_ASSERT(size_field_index <= UCHAR_MAX);
				if (!IsIntegralSingleComponent(type.fields[size_field_index].info.basic_type)) {
					output_type_error(named_soa->size_field);
					return;
				}
				misc_soa->soa.size_field = size_field_index;

				unsigned int capacity_field_index = -1;
				if (named_soa->capacity_field != "\"\"") {
					capacity_field_index = type.FindField(named_soa->capacity_field);
					if (capacity_field_index == -1) {
						output_error("capacity", named_soa->capacity_field);
						return;
					}
					ECS_ASSERT(capacity_field_index <= UCHAR_MAX);
					if (!IsIntegralSingleComponent(type.fields[capacity_field_index].info.basic_type)) {
						output_type_error(named_soa->capacity_field);
					}
				}
				misc_soa->soa.capacity_field = capacity_field_index;
				for (unsigned char subindex = 0; subindex < named_soa->parallel_stream_count; subindex++) {
					unsigned int current_field_index = type.FindField(named_soa->parallel_streams[subindex]);
					if (current_field_index == -1) {
						output_error("stream", named_soa->parallel_streams[subindex]);
						return;
					}
					ECS_ASSERT(current_field_index <= UCHAR_MAX);
					// Verify that this is a pointer field
					if (type.fields[current_field_index].info.stream_type != ReflectionStreamFieldType::Pointer) {
						ECS_FORMAT_TEMP_STRING(message, "Invalid SoA specification for type {#}: field {#} is not a pointer. ", type.name, type.fields[current_field_index].name);
						WriteErrorMessage(data, message.buffer, file_index);
						return;
					}
					// Change the pointer type to PointerSoA
					type.fields[current_field_index].info.stream_type = ReflectionStreamFieldType::PointerSoA;
					misc_soa->soa.parallel_streams[subindex] = current_field_index;

				}

				// We need to add to the total memory the misc copy size
				data->total_memory += misc_soa->CopySize();
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
			const char* parsed_tag_character = SkipWhitespace(tag_character);
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
						Stream<char> argument_range = { parsed_tag_character, PointerDifference(next_line_character, parsed_tag_character) / sizeof(char) + 1 };
						Stream<char> parenthese = FindFirstCharacter(argument_range, '(');
						if (parenthese.size == 0) {
							return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
						}

						Stream<char> end_parenthese = FindMatchingParenthesis(parenthese.AdvanceReturn(), '(', ')');
						if (end_parenthese.size == 0) {
							return ECS_REFLECTION_ADD_TYPE_FIELD_FAILED;
						}

						// We will forward the deduction at the end.
						size_t field_index = type.fields.size;
						type.fields[field_index].tag = { parsed_tag_character, PointerDifference(end_parenthese.buffer, parsed_tag_character) / sizeof(char) + 1 };
						const char* definition_start = SkipWhitespaceEx(last_line_character);
						// Skip the namespace qualification
						const char* colons = strstr(definition_start, "::");
						while (colons != nullptr && colons < semicolon_character) {
							definition_start = SkipWhitespace(colons + 2);
							colons = strstr(definition_start, "::");
						}

						// We need to take into account types that contain multiple words
						// like unsigned int. So determine the name and go reverse
						const char* equals = strchr(last_line_character, '=');
						const char* name_end = equals != nullptr && equals < semicolon_character ? equals : semicolon_character;
						while (!IsCodeIdentifierCharacter(*name_end)) {
							name_end--;
						}
						const char* name_start = name_end;
						while (IsCodeIdentifierCharacter(*name_start)) {
							name_start--;
						}

						const char* definition_end = name_start;
						// We need to have the pointer * included as well
						while (IsWhitespace(*definition_end)) {
							definition_end--;
						}

						type.fields[field_index].definition = { definition_start, PointerDifference(definition_end, definition_start) + 1 };
						type.fields.size++;
						return ECS_REFLECTION_ADD_TYPE_FIELD_OMITTED;
					}

					char* ending_tag_character = (char*)SkipCodeIdentifier(parsed_tag_character);
					char* skipped_ending_tag_character = (char*)SkipWhitespace(ending_tag_character);

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
							ending_tag_character = (char*)SkipWhitespace(skipped_ending_tag_character);
						}

						while (ending_tag_character < skipped_ending_tag_character) {
							*ending_tag_character = ECS_REFLECTION_TYPE_TAG_DELIMITER_CHAR;
							ending_tag_character++;
						}

						if (*skipped_ending_tag_character != '\n') {
							ending_tag_character = (char*)SkipCodeIdentifier(skipped_ending_tag_character);
						}
						skipped_ending_tag_character = (char*)SkipWhitespace(ending_tag_character);
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
				parse_characters = SkipWhitespace(parse_characters);
				parse_characters = SkipWhitespace(parse_characters, -1);
				// Check to see if this is a constant or the number is specified as is
				if (IsIntegerNumber(parse_characters, true)) {
					embedded_array_size = ConvertCharactersToInt(parse_characters);
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
				if (type.fields[type.fields.size - 1].definition.size == 0 || type.fields[type.fields.size - 1].name.size == 0) {
					// There was nothing to be reflected - just mark it as omitted
					return ECS_REFLECTION_ADD_TYPE_FIELD_OMITTED;
				}

				ReflectionFieldInfo& info = type.fields[type.fields.size - 1].info;
				// Set the default value to false initially
				info.has_default_value = false;

				if (embedded_array_size > 0) {
					info.stream_byte_size = info.byte_size;
					info.stream_alignment = GetReflectionFieldTypeAlignment(info.basic_type);
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

					const char* default_value_parse = SkipWhitespace(equal_character);

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
						CapacityStream<void> default_value = { &info.default_bool, 0, sizeof(info.default_double4) };
						bool parse_success = ParseReflectionBasicFieldType(
							info.basic_type, 
							Stream<char>(start_default_value, ending_default_value - start_default_value),
							&default_value
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
					else if (IsCodeIdentifierCharacter(*default_value_parse)) {
						// Check to see that it is the constructor type - else it is the actual value
						const char* start_parse_value = default_value_parse;
						while (IsCodeIdentifierCharacter(*default_value_parse)) {
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
							CapacityStream<void> default_value = { &info.default_bool, 0, sizeof(info.default_double4) };
							info.has_default_value = ParseReflectionBasicFieldType(
								info.basic_type, 
								Stream<char>(start_parse_value, default_value_parse - start_parse_value),
								&default_value
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
				Stream<char> ecsengine_namespace = FindFirstToken(field.definition, namespace_string);
				while (ecsengine_namespace.buffer != nullptr) {
					// Move back all the characters over the current characters
					char* after_characters = ecsengine_namespace.buffer + namespace_string.size;
					size_t after_character_count = strlen(after_characters);
					memcpy(ecsengine_namespace.buffer, after_characters, after_character_count * sizeof(char));
					ecsengine_namespace[after_character_count] = '\0';

					ecsengine_namespace = FindFirstToken(ecsengine_namespace, namespace_string);
					// Don't forget to remove this from the total memory pool
					data->total_memory -= namespace_string.size;
					field.definition.size -= namespace_string.size;
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
					SkipWhitespace(first_star - 1, -1),
					field
				);

				field.info.basic_type_count = pointer_level;
				field.info.stream_type = ReflectionStreamFieldType::Pointer;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.stream_alignment = field.info.byte_size;
				field.info.byte_size = sizeof(void*);
				
				pointer_offset = AlignPointer(before_pointer_offset, alignof(void*));
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
				pointer_offset = AlignPointer(before_pointer_offset, alignof(Stream<void>));
				field.info.pointer_offset = pointer_offset;
				field.info.stream_type = stream_type;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.stream_alignment = field.info.byte_size;
				field.info.byte_size = byte_size;
				field.definition = SkipWhitespace(new_line_character + 1);

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
			field.info.stream_alignment = field.info.byte_size;
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
			else if (IsPointerWithSoA(stream_type)) {
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
				bool success = ReadFile(file_handle, { line_characters, max_line_characters - 1 });
				bool has_reflect = false;
				if (success) {
					line_characters[max_line_characters - 1] = '\0';
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
							alignment = max(alignment, CalculateReflectionTypeAlignment(reflection_manager, &nested_type));
						}
						else {
							Stream<char> definition = type->fields[index].definition;

							// The type is not found, look for container types
							unsigned int container_index = FindReflectionCustomType(definition);
							if (container_index != -1) {
								ReflectionCustomTypeByteSizeData byte_size_data;
								byte_size_data.reflection_manager = reflection_manager;
								byte_size_data.definition = definition;
								ulong2 byte_size_alignment = ECS_REFLECTION_CUSTOM_TYPES[container_index]->GetByteSize(&byte_size_data);

								alignment = max(alignment, byte_size_alignment.y);
							}
							else {
								// Check for pointers
								if (IsPointerWithSoA(type->fields[index].info.stream_type)) {
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
						alignment = max(alignment, blittable_exception);
					}
				}
				else {
					if (type->fields[index].info.stream_type == ReflectionStreamFieldType::Basic ||
						type->fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
						alignment = max(alignment, GetReflectionFieldTypeAlignment(type->fields[index].info.basic_type));
					}
					else {
						alignment = max(alignment, GetReflectionFieldTypeAlignment(type->fields[index].info.stream_type));
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

		size_t GetReflectionTypeSoaIndex(const ReflectionType* type, Stream<char> field) {
			unsigned int field_index = type->FindField(field);
			ECS_ASSERT(field_index != -1, "Invalid field for SoA index");
			return GetReflectionTypeSoaIndex(type, field_index);
		}

		size_t GetReflectionTypeSoaIndex(const ReflectionType* type, unsigned int field_index) {
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_SOA) {
					const ReflectionTypeMiscSoa* soa = &type->misc_info[index].soa;
					if (soa->size_field == field_index || soa->capacity_field == field_index) {
						return index;
					}

					for (unsigned char subindex = 0; subindex < soa->parallel_stream_count; subindex++) {
						if (soa->parallel_streams[subindex] == field_index) {
							return index;
						}
					}
				}
			}
			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<void> GetReflectionTypeSoaStream(const ReflectionType* type, const void* data, size_t soa_index) {
			unsigned int field_index = type->misc_info[soa_index].soa.parallel_streams[0];
			void** buffer = (void**)OffsetPointer(data, type->fields[field_index].info.pointer_offset);
			size_t buffer_size = GetReflectionPointerSoASize(type, soa_index, data);
			return { *buffer, buffer_size };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<void> GetReflectionTypeSoaStream(const ReflectionType* type, const void* data, unsigned int field_index, bool* is_present)
		{
			*is_present = false;
			size_t soa_index = GetReflectionTypeSoaIndex(type, field_index);
			if (soa_index != -1) {
				unsigned int size_field = type->misc_info[soa_index].soa.size_field;
				if (size_field != field_index || type->misc_info[soa_index].soa.capacity_field != field_index) {
					*is_present = true;
					return GetReflectionTypeSoaStream(type, data, soa_index);
				}
			}
			return {};
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldSoaCapacityValue(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, const void* data) {
			if (soa->capacity_field != UCHAR_MAX) {
				return GetIntValueUnsigned(
					OffsetPointer(data, type->fields[soa->capacity_field].info.pointer_offset), 
					BasicTypeToIntType(type->fields[soa->capacity_field].info.basic_type)
				);
			}
			else {
				return -1;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SetReflectionFieldSoaCapacityValue(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, void* data, size_t value) {
			if (soa->capacity_field != UCHAR_MAX) {
				SetIntValueUnsigned(
					OffsetPointer(data, type->fields[soa->capacity_field].info.pointer_offset),
					BasicTypeToIntType(type->fields[soa->capacity_field].info.basic_type),
					value
				);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SetReflectionFieldSoaStream(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, void* data, ResizableStream<void> stream_data) {
			// TODO: If the soa is enhanced to have the allocator offset, we need to assign that as well
			ECS_ASSERT(soa->size_field != UCHAR_MAX);
			SetReflectionPointerSoASize(type, soa, data, stream_data.size);
			// Use the maximum between the 2 in case the capacity is not set
			unsigned int stream_capacity = max(stream_data.capacity, stream_data.size);
			SetReflectionFieldSoaCapacityValue(type, soa, data, stream_capacity);
			
			void* current_pointer_data = stream_data.buffer;
			for (unsigned int index = 0; index < soa->parallel_stream_count; index++) {
				unsigned int field_index = soa->parallel_streams[index];
				void** buffer_ptr = (void**)OffsetPointer(data, type->fields[field_index].info.pointer_offset);
				*buffer_ptr = current_pointer_data;
				current_pointer_data = OffsetPointer(current_pointer_data, (size_t)stream_capacity * (size_t)type->fields[field_index].info.stream_byte_size);
			}
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
			if (FindFirstCharacter(definition, '*').size > 0) {
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
				return { field.info.byte_size, GetReflectionFieldTypeAlignment(field.info.basic_type) };
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
				return ECS_REFLECTION_CUSTOM_TYPES[container_index]->GetByteSize(&byte_size_data);
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
				return ECS_REFLECTION_CUSTOM_TYPES[container_index]->GetByteSize(&byte_size_data).y;
			}
			else if (enum_.name.size > 0) {
				return alignof(unsigned char);
			}
			else {
				ReflectionField field = GetReflectionFieldInfo(reflection_manager, definition);
				if (field.info.basic_type != ReflectionBasicFieldType::UserDefined && field.info.basic_type != ReflectionBasicFieldType::Unknown) {
					return GetReflectionFieldTypeAlignment(field.info.basic_type);
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
			// If it has a SoA specification, then it is not blittable
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_SOA) {
					return false;
				}
			}

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
							if (!ECS_REFLECTION_CUSTOM_TYPES[custom_index]->IsBlittable(&data)) {
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
					return ECS_REFLECTION_CUSTOM_TYPES[custom_index]->IsBlittable(&is_data);
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

		void SetReflectionTypeFieldsStreamByteSizeAlignment(const ReflectionManager* reflection_manager, ReflectionType* reflection_type)
		{
			for (size_t index = 0; index < reflection_type->fields.size; index++) {
				if (IsStream(reflection_type->fields[index].info.stream_type)) {
					Stream<char> target_type = ReflectionCustomTypeGetTemplateArgument(reflection_type->fields[index].definition);
					// Void streams are a special case - consider the element byte size as 1 since it's a byte stream
					if (target_type == STRING(void)) {
						reflection_type->fields[index].info.stream_byte_size = 1;
						reflection_type->fields[index].info.stream_alignment = 1;
					}
					else {
						ulong2 stream_byte_size_alignment = SearchReflectionUserDefinedTypeByteSizeAlignment(reflection_manager, target_type);
						reflection_type->fields[index].info.stream_byte_size = stream_byte_size_alignment.x;
						reflection_type->fields[index].info.stream_alignment = stream_byte_size_alignment.y;
					}
				}
				else if (reflection_type->fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					reflection_type->fields[index].info.stream_byte_size = reflection_type->fields[index].info.byte_size / 
						reflection_type->fields[index].info.basic_type_count;
					reflection_type->fields[index].info.stream_alignment = SearchReflectionUserDefinedTypeAlignment(
						reflection_manager, 
						reflection_type->fields[index].definition
					);
				}
			}

			// Include SoA pointers here as well. Set the stream byte size and the
			// Pointer and basic type for the size
			for (size_t index = 0; index < reflection_type->misc_info.size; index++) {
				if (reflection_type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_SOA) {
					const ReflectionTypeMiscSoa* soa = &reflection_type->misc_info[index].soa;
					unsigned int size_field = soa->size_field;
					for (unsigned char soa_index = 0; soa_index < soa->parallel_stream_count; soa_index++) {
						Stream<char> target_type = GetReflectionFieldPointerTarget(reflection_type->fields[index]);
						ulong2 stream_byte_size_alignment = SearchReflectionUserDefinedTypeByteSizeAlignment(reflection_manager, target_type);
						reflection_type->fields[soa->parallel_streams[soa_index]].info.stream_byte_size = stream_byte_size_alignment.x;
						reflection_type->fields[soa->parallel_streams[soa_index]].info.stream_alignment = stream_byte_size_alignment.y;
						reflection_type->fields[soa->parallel_streams[soa_index]].info.soa_size_basic_type = reflection_type->fields[size_field].info.basic_type;
						reflection_type->fields[soa->parallel_streams[soa_index]].info.soa_size_pointer_offset = reflection_type->fields[size_field].info.pointer_offset;
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void CopyReflectionTypeBlittableFields(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* type, 
			const void* source, 
			void* destination
		)
		{
			for (size_t index = 0; index < type->fields.size; index++) {
				if (SearchIsBlittable(reflection_manager, type->fields[index].definition)) {
					// We can copy the field
					memcpy(type->GetField(destination, index), type->GetField(source, index), type->fields[index].info.byte_size);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, const ReflectionField& field)
		{
			// Check the give size macro
			if (field.info.basic_type != ReflectionBasicFieldType::UserDefined) {
				if (field.info.stream_type == ReflectionStreamFieldType::Basic || field.info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					return GetReflectionFieldTypeAlignment(field.info.basic_type);
				}
				else {
					return GetReflectionFieldTypeAlignment(field.info.stream_type);
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
				if (IsPointerWithSoA(field.info.stream_type)) {
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
						return ECS_REFLECTION_CUSTOM_TYPES[custom_index]->GetByteSize(&byte_size_data).y;
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
						allocation = Allocate(allocator, copy_size, info->stream_alignment);
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
						allocation = Allocate(allocator, copy_size, info->stream_alignment);
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
				else if (stream_type == ReflectionStreamFieldType::PointerSoA) {
					const void** source_ptr = (const void**)source;
					void** destination_ptr = (void**)destination;

					// In order to correctly offset into the size ptr, we need to offset back
					// Towards the start of the data
					size_t source_ptr_size = GetReflectionFieldPointerSoASize(*info, OffsetPointer(source, -(int64_t)info->pointer_offset));

					size_t copy_size = source_ptr_size * GetReflectionFieldStreamElementByteSize(*info);
					void* allocation = nullptr;
					if (copy_size > 0) {
						allocation = Allocate(allocator, copy_size, info->stream_alignment);
						memcpy(allocation, *source_ptr, copy_size);
					}

					*destination_ptr = allocation;
					SetReflectionFieldPointerSoASize(*info, OffsetPointer(destination, -(int64_t)info->pointer_offset), source_ptr_size);
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
			const void* stream_field = offset_into_data ? OffsetPointer(data, info.pointer_offset) : data;
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
			const void* stream_field = offset_into_data ? OffsetPointer(data, info.pointer_offset) : data;

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

			const void* stream_field = offset_into_data ? OffsetPointer(data, info.pointer_offset) : data;
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
			else if (info.stream_type == ReflectionStreamFieldType::PointerSoA) {
				return_value.buffer = *(void**)stream_field;
				const void* original_data = OffsetPointer(stream_field, -(int64_t)info.pointer_offset);
				return_value.size = GetReflectionFieldPointerSoASize(info, original_data);
				// TODO: Insert the offset for the capacity field into the info?
				// At the moment, this does not return the appropriate capacity value
				return_value.capacity = return_value.size;
				return_value.allocator = { nullptr };
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
			ECS_ASSERT(IsPointerWithSoA(info.stream_type));
			return info.basic_type_count;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> GetReflectionFieldPointerTarget(const ReflectionField& field)
		{
			return field.definition;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldPointerSoASize(const ReflectionFieldInfo& info, const void* data) {
			const void* size_ptr = OffsetPointer(data, info.soa_size_pointer_offset);
			return ConvertToSizetFromBasic(info.soa_size_basic_type, size_ptr);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionPointerSoASize(const ReflectionType* type, size_t soa_index, const void* data) {
			const ReflectionFieldInfo* info = &type->fields[type->misc_info[soa_index].soa.size_field].info;
			const void* size_ptr = OffsetPointer(data, info->pointer_offset);
			return ConvertToSizetFromBasic(info->basic_type, size_ptr);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionPointerSoAPerElementSize(const ReflectionType* type, size_t soa_index)
		{
			size_t per_element_size = 0;
			const ReflectionTypeMiscSoa* soa = &type->misc_info[soa_index].soa;
			for (unsigned int index = 0; index < soa->parallel_stream_count; index++) {
				per_element_size += GetReflectionFieldStreamElementByteSize(type->fields[soa->parallel_streams[index]].info);
			}
			return per_element_size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void MergeReflectionPointerSoAAllocations(const ReflectionType* type, size_t soa_index, void* data, AllocatorPolymorphic allocator) {
			const ReflectionTypeMiscSoa* soa = &type->misc_info[soa_index].soa;
			size_t per_element_size = GetReflectionPointerSoAPerElementSize(type, soa_index);
			size_t soa_size = GetReflectionPointerSoASize(type, soa_index, data);
			size_t total_allocation_size = per_element_size * soa_size;
			void* allocation = AllocateEx(allocator, total_allocation_size, GetReflectionTypeSoaAllocationAlignment(type, soa));
			
			for (unsigned int index = 0; index < soa->parallel_stream_count; index++) {
				const ReflectionFieldInfo* current_field = &type->fields[soa->parallel_streams[index]].info;
				void** current_ptr = (void**)OffsetPointer(data, current_field->pointer_offset);
				// Copy the data from the current ptr to the allocation and deallocate the current ptr afterwards
				size_t current_copy_size = soa_size * GetReflectionFieldStreamElementByteSize(*current_field);
				memcpy(allocation, *current_ptr, current_copy_size);
				DeallocateEx(allocator, *current_ptr);
				*current_ptr = allocation;
				allocation = OffsetPointer(allocation, current_copy_size);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void MergeReflectionPointerSoAAllocationsForType(const ReflectionType* type, void* data, AllocatorPolymorphic allocator) {
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_SOA) {
					MergeReflectionPointerSoAAllocations(type, index, data, allocator);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsReflectionPointerSoAAllocationHolder(const ReflectionType* type, unsigned int field_index) {
			size_t soa_index = GetReflectionTypeSoaIndex(type, field_index);
			ECS_ASSERT_FORMAT(soa_index != -1, "Trying to reference reflection type's {#} field {#} as PointerSoA, but it is not found as one",
				type->name, type->fields[field_index].name);
			return field_index == type->misc_info[soa_index].soa.parallel_streams[0];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DeallocateReflectionPointerSoAAllocation(const ReflectionType* type, unsigned int field_index, void* data, AllocatorPolymorphic allocator) {
			if (IsReflectionPointerSoAAllocationHolder(type, field_index)) {
				void* pointer = *(void**)OffsetPointer(data, type->fields[field_index].info.pointer_offset);
				DeallocateEx(allocator, pointer);
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SetReflectionFieldPointerSoASize(const ReflectionFieldInfo& info, void* data, size_t value) {
			ECS_ASSERT(info.stream_type == ReflectionStreamFieldType::PointerSoA, 
				"Invalid stream field type for SetReflectionFieldPointerSoASize. It must be a PointerSoA!");
			void* size_ptr = OffsetPointer(data, info.soa_size_pointer_offset);
			ConvertFromSizetToBasic(info.soa_size_basic_type, value, size_ptr);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SetReflectionPointerSoASize(const ReflectionType* type, size_t soa_index, void* data, size_t value) {
			SetReflectionPointerSoASize(type, &type->misc_info[soa_index].soa, data, value);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void SetReflectionPointerSoASize(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, void* data, size_t value) {
			const ReflectionFieldInfo* info = &type->fields[soa->size_field].info;
			void* size_ptr = OffsetPointer(data, info->pointer_offset);
			ConvertFromSizetToBasic(info->basic_type, value, size_ptr);
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
			void* stream_field = offset_into_data ? OffsetPointer(data, info.pointer_offset) : data;
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
			else if (info.stream_type == ReflectionStreamFieldType::PointerSoA) {
				void** pointer = (void**)stream_field;
				*pointer = stream_data.buffer;
				SetReflectionFieldPointerSoASize(info, OffsetPointer(stream_field, -(int64_t)info.pointer_offset), stream_data.size);
				// TODO: If the info is enhanced with the soa capacity offset, we need to set that here as well
			}
			if constexpr (basic_array) {
				if (info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					size_t element_byte_size = info.byte_size / info.basic_type_count;
					size_t elements_to_copy = min((size_t)stream_data.size, (size_t)info.basic_type_count);
					size_t copy_size = elements_to_copy * element_byte_size;
					memcpy(stream_field, stream_data.buffer, copy_size);
					if (elements_to_copy < (size_t)info.basic_type_count) {
						memset(OffsetPointer(stream_field, copy_size), 0, info.byte_size - copy_size);
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
				if (first->fields[index].definition != second->fields[index].definition) {
					return false;
				}

				// Now check the name - if they have different names and the name can be found on other spot,
				// Consider them to be different. Else, allow it to be the same to basically support renaming
				if (first->fields[index].name != second->fields[index].name) {
					unsigned int second_field_index = second->FindField(first->fields[index].name);
					if (second_field_index != -1) {
						return false;
					}
				}

				// If a user defined type is here, check that it too didn't change
				// For custom types we shouldn't need to check (those are supposed to be fixed, like containers)
				if (first_info->basic_type == ReflectionBasicFieldType::UserDefined) {
					// Make sure it is not a blittable type
					if (first_reflection_manager->FindBlittableException(first->fields[index].definition).x == -1) {
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
						first = OffsetPointer(first, info->pointer_offset);
						second = OffsetPointer(second, info->pointer_offset);
					}
					return memcmp(first, second, info->byte_size) == 0;
				}
				else {
					size_t basic_byte_size = GetReflectionBasicFieldTypeByteSize(info->basic_type);
					// The SoA pointer is handled here
					if (IsStreamWithSoA(info->stream_type)) {
						Stream<void> first_data = GetReflectionFieldStreamVoid(*info, first, offset_into_data);
						Stream<void> second_data = GetReflectionFieldStreamVoid(*info, second, offset_into_data);
						return first_data.size == second_data.size && memcmp(first_data.buffer, second_data.buffer, first_data.size * basic_byte_size) == 0;
					}
					else if (info->stream_type == ReflectionStreamFieldType::Pointer) {
						if (offset_into_data) {
							first = OffsetPointer(first, info->pointer_offset);
							second = OffsetPointer(second, info->pointer_offset);
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
			CompareReflectionTypeInstancesOptions default_options;
			if (options == nullptr) {
				options = &default_options;
			}

			if (offset_into_data) {
				first = OffsetPointer(first, field->info.pointer_offset);
				second = OffsetPointer(second, field->info.pointer_offset);
			}

			// Check to see if this is a blittable field according to the options
			if (options->blittable_types.size > 0) {
				unsigned int index = FindString(field->definition, options->blittable_types, [](CompareReflectionTypeInstanceBlittableType blittable_type) {
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
				// The SoA pointer is handled here
				else if (field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray || IsStreamWithSoA(field->info.stream_type)) {
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
						current_first = OffsetPointer(current_first, nested_byte_size);
						current_second = OffsetPointer(current_second, nested_byte_size);
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
					ulong2 byte_size = ECS_REFLECTION_CUSTOM_TYPES[custom_index]->GetByteSize(&byte_size_data);

					ReflectionCustomTypeIsBlittableData is_blittable_data;
					is_blittable_data.definition = definition;
					is_blittable_data.reflection_manager = reflection_manager;
					bool is_blittable = ECS_REFLECTION_CUSTOM_TYPES[custom_index]->IsBlittable(&is_blittable_data);
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
							if (!ECS_REFLECTION_CUSTOM_TYPES[custom_index]->Compare(&compare_data)) {
								return false;
							}
							compare_data.first = OffsetPointer(compare_data.first, byte_size.x);
							compare_data.second = OffsetPointer(compare_data.second, byte_size.x);
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
			const CopyReflectionDataOptions* options
		)
		{
			if (options->always_allocate_for_buffers || options->deallocate_existing_buffers) {
				ECS_ASSERT(options->allocator.allocator != nullptr);
			}

			CopyReflectionDataOptions copy_with_offset_options = *options;
			copy_with_offset_options.offset_pointer_data_from_field = true;

			// Go through the new type and try to get the data
			for (size_t new_index = 0; new_index < new_type->fields.size; new_index++) {
				// Check the remapping tag
				Stream<char> mapping_tag = new_type->fields[new_index].GetTag(STRING(ECS_MAP_FIELD_REFLECTION));
				Stream<char> name_to_search = new_type->fields[new_index].name;
				if (mapping_tag.size > 0) {
					name_to_search = FindDelimitedString(mapping_tag, '(', ')', true);
				}
				
				unsigned int field_index = old_type->FindField(name_to_search);
				if (field_index != -1) {
					ConvertReflectionFieldToOtherField(
						old_reflection_manager,
						new_reflection_manager,
						old_type,
						new_type,
						field_index,
						new_index,
						old_data,
						new_data,
						&copy_with_offset_options
					);

					// If there are padding bytes between this field and the next one, and they match in length, make sure to copy
					// That as well otherwise comparison checks will fail because of them
					unsigned short current_next_field_offset = 0;
					unsigned short old_next_field_offset = 0;
					if (new_index < new_type->fields.size - 1) {
						current_next_field_offset = new_type->fields[new_index + 1].info.pointer_offset;
					}
					else {
						current_next_field_offset = new_type->byte_size;
					}

					if (field_index < old_type->fields.size - 1) {
						old_next_field_offset = old_type->fields[field_index + 1].info.pointer_offset;
					}
					else {
						old_next_field_offset = old_type->byte_size;
					}

					unsigned short current_padding_bytes = current_next_field_offset - new_type->fields[new_index].info.pointer_offset - 
						new_type->fields[new_index].info.byte_size;
					if (options->set_padding_bytes_to_zero) {
						if (current_padding_bytes > 0) {
							memset(OffsetPointer(new_type->GetField(new_data, new_index), new_type->fields[new_index].info.byte_size), 0, current_padding_bytes);
						}
					}
					else {
						unsigned short old_padding_bytes = old_next_field_offset - old_type->fields[field_index].info.pointer_offset -
							old_type->fields[field_index].info.byte_size;
						if (current_padding_bytes == old_padding_bytes && current_padding_bytes > 0) {
							memcpy(
								OffsetPointer(new_type->GetField(new_data, new_index), new_type->fields[new_index].info.byte_size),
								OffsetPointer(old_type->GetField(old_data, field_index), old_type->fields[field_index].info.byte_size),
								current_padding_bytes
							);
						}
					}
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

			if (options->always_allocate_for_buffers) {
				// We need to merge the SoA buffers
				// At the moment, all SoA streams do not have an allocator for themselves
				MergeReflectionPointerSoAAllocationsForType(new_type, new_data, options->allocator);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void CopyReflectionTypeInstance(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* type, 
			const void* source_data, 
			void* destination_data, 
			const CopyReflectionDataOptions* options
		)
		{
			if (options->always_allocate_for_buffers || options->deallocate_existing_buffers) {
				ECS_ASSERT(options->allocator.allocator != nullptr);
			}

			CopyReflectionDataOptions copy_with_offset_options = *options;
			copy_with_offset_options.offset_pointer_data_from_field = true;

			// Go through the new type and try to get the data
			for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
				CopyReflectionFieldInstance(
					reflection_manager,
					type,
					field_index,
					source_data,
					destination_data,
					&copy_with_offset_options
				);

				// If there are padding bytes between this field and the next one, and they match in length, make sure to copy
				// That as well otherwise comparison checks will fail because of them
				unsigned short next_field_offset = 0;
				if (field_index < type->fields.size - 1) {
					next_field_offset = type->fields[field_index + 1].info.pointer_offset;
				}
				else {
					next_field_offset = type->byte_size;
				}

				unsigned short current_padding_bytes = next_field_offset - type->fields[field_index].info.pointer_offset -
					type->fields[field_index].info.byte_size;
				if (current_padding_bytes > 0) {
					if (options->set_padding_bytes_to_zero) {
						memset(OffsetPointer(type->GetField(destination_data, field_index), type->fields[field_index].info.byte_size), 0, current_padding_bytes);
					}
					else {
						memcpy(
							OffsetPointer(type->GetField(destination_data, field_index), type->fields[field_index].info.byte_size),
							OffsetPointer(type->GetField(source_data, field_index), type->fields[field_index].info.byte_size),
							current_padding_bytes
						);
					}
				}
			}

			// At the end, if the buffer allocation is enabled, we need to coalesce any SoA buffers
			if (options->always_allocate_for_buffers) {
				MergeReflectionPointerSoAAllocationsForType(type, destination_data, options->allocator);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void CopyReflectionTypeInstance(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options
		) {
			// Start with blittable types
			ulong2 exception_index = reflection_manager->FindBlittableException(definition);
			if (exception_index.x != -1) {
				memcpy(destination, source, exception_index.x);
			}
			else {
				// Check fundamental type
				ReflectionField field = GetReflectionFieldInfo(reflection_manager, definition);
				if (field.info.basic_type != ReflectionBasicFieldType::UserDefined && field.info.basic_type != ReflectionBasicFieldType::Unknown) {
					CopyReflectionFieldBasic(&field.info, source, destination, options->allocator);
				}
				else {
					// Check blittable exception
					ReflectionType reflection_type;
					if (reflection_manager->TryGetType(definition, reflection_type)) {
						// Forward the call
						CopyReflectionTypeInstance(reflection_manager, &reflection_type, source, destination, options);
					}
					else {
						// Check to see if it is a custom type
						unsigned int custom_index = FindReflectionCustomType(definition);
						if (custom_index != -1) {
							ReflectionCustomTypeIsBlittableData trivially_data;
							trivially_data.definition = definition;
							trivially_data.reflection_manager = reflection_manager;
							bool trivially_copyable = ECS_REFLECTION_CUSTOM_TYPES[custom_index]->IsBlittable(&trivially_data);
							if (trivially_copyable) {
								ReflectionCustomTypeByteSizeData byte_size_data;
								byte_size_data.definition = definition;
								byte_size_data.reflection_manager = reflection_manager;
								size_t byte_size = ECS_REFLECTION_CUSTOM_TYPES[custom_index]->GetByteSize(&byte_size_data).x;
								memcpy(destination, source, byte_size);
							}
							else {
								ReflectionCustomTypeCopyData copy_data;
								copy_data.allocator = options->allocator;
								copy_data.definition = definition;
								copy_data.destination = destination;
								copy_data.source = source;
								copy_data.reflection_manager = reflection_manager;

								ECS_REFLECTION_CUSTOM_TYPES[custom_index]->Copy(&copy_data);
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

		bool CopyIntoReflectionFieldValue(const ReflectionType* type, Stream<char> field_name, void* instance, const void* data)
		{
			unsigned int field_index = type->FindField(field_name);
			if (field_index != -1) {
				void* field_data = type->GetField(instance, field_index);
				memcpy(field_data, data, type->fields[field_index].info.byte_size);
				return true;
			}
			return false;
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

				first_data = OffsetPointer(first_data, first_byte_size);
				second_data = OffsetPointer(second_data, second_byte_size);
			}

#undef CASE
#undef SWITCH

		}

		// ----------------------------------------------------------------------------------------------------------------------------

		// The signature of the function was changed to take types and field indices instead
		// Of ReflectionField directly in order to accomodate the PointerSoA which has some special
		// Treatment

		void ConvertReflectionFieldToOtherField(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionType* source_type, 
			const ReflectionType* destination_type,
			unsigned int source_field_index,
			unsigned int destination_field_index,
			const void* source_data, 
			void* destination_data,
			const CopyReflectionDataOptions* options
		)
		{
			const ReflectionField* source_field = &source_type->fields[source_field_index];
			const ReflectionField* destination_field = &destination_type->fields[destination_field_index];

			if (options->always_allocate_for_buffers || options->deallocate_existing_buffers) {
				ECS_ASSERT(options->allocator.allocator != nullptr);
			}

			if (options->offset_pointer_data_from_field) {
				source_data = OffsetPointer(source_data, source_field->info.pointer_offset);
				destination_data = OffsetPointer(destination_data, destination_field->info.pointer_offset);
			}

			ResizableStream<void> initial_destination_stream_data;
			if (options->deallocate_existing_buffers && IsStreamWithSoA(destination_field->info.stream_type)) {
				initial_destination_stream_data = GetReflectionFieldResizableStreamVoid(destination_field->info, destination_data, false);
			}

			// If they have the same type, go ahead and try to copy
			if (source_field->Compare(*destination_field)) {
				// If not a user defined type, can copy it
				if (source_field->info.basic_type != ReflectionBasicFieldType::UserDefined) {
					if (source_field->info.stream_type != ReflectionStreamFieldType::BasicTypeArray) {
						if (options->always_allocate_for_buffers && IsStreamWithSoA(source_field->info.stream_type)) {
							// Include SoA pointers as well. Let the main caller merge the SoA pointers
							ResizableStream<void> previous_data = GetReflectionFieldResizableStreamVoid(source_field->info, source_data, false);
							size_t allocation_size = (size_t)destination_field->info.stream_byte_size * previous_data.size;
							void* allocation = Allocate(options->allocator, allocation_size, destination_field->info.stream_alignment);
							memcpy(allocation, previous_data.buffer, allocation_size);

							previous_data.buffer = allocation;
							previous_data.capacity = previous_data.size;
							SetReflectionFieldResizableStreamVoid(destination_field->info, destination_data, previous_data, false);
						}
						else {
							memcpy(
								destination_data,
								source_data,
								destination_field->info.byte_size
							);
						}
					}
					else {
						unsigned short copy_size = min(source_field->info.byte_size, destination_field->info.byte_size);
						memcpy(
							destination_data,
							source_data,
							copy_size
						);
						if (copy_size != destination_field->info.byte_size) {
							memset(OffsetPointer(destination_data, copy_size), 0, destination_field->info.byte_size - copy_size);
						}
					}
				}
				else {
					ECS_ASSERT(first_reflection_manager != nullptr && second_reflection_manager != nullptr);

					// Check blittable types
					ulong2 first_blittable = first_reflection_manager->FindBlittableException(source_field->definition);
					ulong2 second_blittable = second_reflection_manager->FindBlittableException(destination_field->definition);
					if (first_blittable.x != -1 ||  second_blittable.x != -1) {
						if (first_blittable.x == second_blittable.x) {
							// Same definition and both are respectively blittable - blit
							memcpy(
								destination_data,
								source_data,
								first_blittable.x
							);
						}
						else {
							// One is blittable, the other is not
							SetInstanceFieldDefaultData(destination_field, destination_data, false);
						}
					}
					else {
						// Call the functor again
						ReflectionType new_nested_type;
						ReflectionType old_nested_type;
						if (first_reflection_manager->TryGetType(source_field->definition, old_nested_type)) {
							if (second_reflection_manager->TryGetType(destination_field->definition, new_nested_type)) {
								if (source_field->info.stream_type == ReflectionStreamFieldType::Basic || source_field->info.stream_type == ReflectionStreamFieldType::Pointer) {
									if (source_field->info.stream_type == ReflectionStreamFieldType::Pointer) {
										source_data = *(void**)source_data;
										destination_data = *(void**)destination_data;
									}
									CopyReflectionTypeToNewVersion(
										first_reflection_manager,
										second_reflection_manager,
										&old_nested_type,
										&new_nested_type,
										source_data,
										destination_data,
										options
									);
								}
								else {
									// The SoA pointers are handled here as well
									ResizableStream<void> old_data = GetReflectionFieldResizableStreamVoidEx(source_field->info, source_data, false);
									// Check to see if the type has changed. If it didn't, we can just reference it.
									// If it did, we would need to make a new allocation.
									if (CompareReflectionTypes(
										first_reflection_manager,
										second_reflection_manager,
										&old_nested_type,
										&new_nested_type
									)) {
										// Check to see if we need to allocate
										if (options->always_allocate_for_buffers) {
											size_t allocation_size = GetReflectionFieldStreamElementByteSize(destination_field->info) * old_data.size;
											void* allocation = Allocate(options->allocator, allocation_size, destination_field->info.stream_alignment);
											memcpy(allocation, old_data.buffer, allocation_size);
											old_data.buffer = allocation;
											old_data.capacity = old_data.size;
										}

										// Can set the data directly
										SetReflectionFieldResizableStreamVoidEx(destination_field->info, destination_data, old_data, false);
									}
									else {
										// We need to allocate
										if (options->allocator.allocator == nullptr) {
											// The behaviour is omitted
											second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data);
										}
										else {
											size_t old_type_byte_size = GetReflectionTypeByteSize(&old_nested_type);
											size_t new_type_byte_size = GetReflectionTypeByteSize(&new_nested_type);

											if (destination_field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
												unsigned int copy_count = min(old_data.size, (unsigned int)destination_field->info.basic_type_count);
												void* current_data = destination_data;
												for (unsigned int index = 0; index < copy_count; index++) {
													CopyReflectionTypeToNewVersion(
														first_reflection_manager,
														second_reflection_manager,
														&old_nested_type,
														&new_nested_type,
														OffsetPointer(old_data.buffer, old_type_byte_size * index),
														OffsetPointer(current_data, new_type_byte_size * index),
														options
													);
												}
											}
											else {
												// SoA pointers are handled as well
												size_t allocation_size = GetReflectionFieldStreamElementByteSize(destination_field->info) * old_data.size;
												void* allocation = Allocate(options->allocator, allocation_size, destination_field->info.stream_alignment);
												for (size_t index = 0; index < old_data.size; index++) {
													CopyReflectionTypeToNewVersion(
														first_reflection_manager,
														second_reflection_manager,
														&old_nested_type,
														&new_nested_type,
														OffsetPointer(old_data.buffer, old_type_byte_size * index),
														OffsetPointer(allocation, new_type_byte_size * index),
														options
													);
												}
												old_data.buffer = allocation;
												old_data.capacity = old_data.size;
												SetReflectionFieldResizableStreamVoid(destination_field->info, destination_data, old_data, false);
											}
										}
									}
								}
							}
							else {
								// Custom type, enum, or error.
								// Call the default instance data
								second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data, false);
							}
						}
						else {
							// Check for custom type
							unsigned int custom_type_index = FindReflectionCustomType(destination_field->definition);
							if (custom_type_index != -1) {
								// Get the dependent types. If any of these has changed, then revert to default instance field
								ECS_STACK_CAPACITY_STREAM(Stream<char>, dependent_types, 128);
								ReflectionCustomTypeDependentTypesData dependent_data;
								dependent_data.dependent_types = dependent_types;
								dependent_data.definition = destination_field->definition;
								ECS_REFLECTION_CUSTOM_TYPES[custom_type_index]->GetDependentTypes(&dependent_data);

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
									second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data, false);
								}
								else {
									// Nothing has changed. Can copy it
									if (options->always_allocate_for_buffers) {
										ECS_ASSERT(false, "Allocating custom types is not supported from old to new version");
										//ReflectionCustomTypeCopyData copy_data;
										//copy_data.reflection_manager = 
									}
									else {
										memcpy(
											destination_data,
											source_data,
											destination_field->info.byte_size
										);
									}
								}
							}
							else {
								// Enum or error.
								// Call the default instance data
								second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data, false);
							}
						}
					}
				}
			}
			else {
				// Try to copy into this newer version
				// If either one is a user defined type, then proceed with the default data
				if (destination_field->info.basic_type == ReflectionBasicFieldType::UserDefined
					|| source_field->info.basic_type == ReflectionBasicFieldType::UserDefined) {
					if (second_reflection_manager != nullptr) {
						second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data, false);
					}
					else {
						SetInstanceFieldDefaultData(destination_field, destination_data, false);
					}
				}
				else {
					if (destination_field->info.stream_type != ReflectionStreamFieldType::Basic && 
						source_field->info.stream_type != ReflectionStreamFieldType::Basic) {
						// SoA pointers are handled here as well
						ResizableStream<void> field = GetReflectionFieldResizableStreamVoid(source_field->info, source_data, false);

						// If they have the same basic field type
						if (source_field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
							if (destination_field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
								// Convert if different types, else can just copy
								if (source_field->info.basic_type == destination_field->info.basic_type) {
									SetReflectionFieldResizableStreamVoidEx(destination_field->info, destination_data, field, false);
								}
								else {
									unsigned short copy_count = min(source_field->info.basic_type_count, destination_field->info.basic_type_count);
									ConvertReflectionBasicField(
										source_field->info.basic_type,
										destination_field->info.basic_type,
										source_data,
										destination_data,
										copy_count
									);

									if (copy_count < destination_field->info.basic_type_count) {
										unsigned int element_byte_size = (unsigned int)destination_field->info.byte_size / (unsigned int)destination_field->info.basic_type_count;
										unsigned int copy_byte_size = element_byte_size * copy_count;
										memset(OffsetPointer(destination_data, copy_byte_size), 0, destination_field->info.byte_size - copy_byte_size);
									}
								}
							}
							else {
								// Need to allocate
								if (options->allocator.allocator == nullptr) {
									// Set the default
									if (second_reflection_manager != nullptr) {
										second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data, false);
									}
									else {
										SetInstanceFieldDefaultData(destination_field, destination_data, false);
									}
								}
								else {
									void* allocation = Allocate(
										options->allocator, 
										field.size * GetReflectionFieldStreamElementByteSize(destination_field->info),
										destination_field->info.stream_alignment
									);
									ConvertReflectionBasicField(
										source_field->info.basic_type,
										destination_field->info.basic_type,
										field.buffer,
										allocation,
										field.size
									);
									field.buffer = allocation;
									field.capacity = field.size;
									SetReflectionFieldResizableStreamVoid(destination_field->info, destination_data, field, false);
								}
							}
						}
						else {
							if (source_field->info.basic_type == destination_field->info.basic_type) {
								// Can just copy directly
								// Check to see if we need to allocate always
								if (options->always_allocate_for_buffers) {
									size_t allocation_size = GetReflectionFieldStreamElementByteSize(destination_field->info) * field.size;
									void* allocation = Allocate(options->allocator, allocation_size, destination_field->info.stream_alignment);
									memcpy(allocation, field.buffer, allocation_size);
									field.buffer = allocation;
									field.capacity = field.size;
								}

								SetReflectionFieldResizableStreamVoidEx(destination_field->info, destination_data, field, false);
							}
							else {
								// Need to convert
								if (destination_field->info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
									// No need to allocate, just to convert
									void* write_buffer = destination_data;
									unsigned int write_count = min(field.size, (unsigned int)destination_field->info.basic_type_count);
									ConvertReflectionBasicField(
										source_field->info.basic_type,
										destination_field->info.basic_type,
										field.buffer,
										write_buffer,
										write_count
									);

									if (write_count != (unsigned int)destination_field->info.basic_type_count) {
										// Set the 0 the other fields
										unsigned int write_byte_size = write_count * (unsigned int)destination_field->info.byte_size / (unsigned int)destination_field->info.basic_type_count;
										memset(
											OffsetPointer(
												write_buffer,
												write_byte_size
											),
											0,
											(unsigned int)destination_field->info.byte_size - write_byte_size
										);
									}
								}
								else {
									// Need to allocate
									if (options->allocator.allocator == nullptr) {
										if (second_reflection_manager != nullptr) {
											second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data, false);
										}
										else {
											SetInstanceFieldDefaultData(destination_field, destination_data, false);
										}
									}
									else {
										void* allocation = Allocate(
											options->allocator, 
											field.size * GetReflectionFieldStreamElementByteSize(destination_field->info),
											destination_field->info.stream_alignment
										);
										ConvertReflectionBasicField(
											source_field->info.basic_type,
											destination_field->info.basic_type,
											field.buffer,
											allocation,
											field.size
										);
										field.buffer = allocation;
										SetReflectionFieldResizableStreamVoid(destination_field->info, destination_data, field, false);
									}
								}
							}
						}
					}
					else {
						// If one is basic and the other one stream, just set the default for the field
						if (destination_field->info.stream_type == ReflectionStreamFieldType::Basic && source_field->info.stream_type == ReflectionStreamFieldType::Basic) {
							// Convert the basic field type
							ConvertReflectionBasicField(
								source_field->info.basic_type, 
								destination_field->info.basic_type,
								source_data, 
								destination_data
							);
						}
						else {
							if (second_reflection_manager != nullptr) {
								second_reflection_manager->SetInstanceFieldDefaultData(destination_field, destination_data, false);
							}
							else {
								SetInstanceFieldDefaultData(destination_field, destination_data, false);
							}
						}
					}
				}
			}

			if (options->deallocate_existing_buffers && IsStreamWithSoA(destination_field->info.stream_type)) {
				if (initial_destination_stream_data.allocator.allocator == nullptr) {
					initial_destination_stream_data.allocator = options->allocator;
				}
				if (destination_field->info.stream_type == ReflectionStreamFieldType::PointerSoA) {
					// We need to check if this is the first pointer field and deallocate only
					// Then, since we have coalesced allocations
					if (IsReflectionPointerSoAAllocationHolder(destination_type, destination_field_index)) {
						// Only in this case we need to deallocate
						initial_destination_stream_data.FreeBuffer();
					}
				}
				else {
					initial_destination_stream_data.FreeBuffer();
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void CopyReflectionFieldInstance(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			unsigned int field_index,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options
		)
		{
			const ReflectionField* field = &type->fields[field_index];

			if (options->always_allocate_for_buffers || options->deallocate_existing_buffers) {
				ECS_ASSERT(options->allocator.allocator != nullptr);
			}

			if (options->offset_pointer_data_from_field) {
				source = OffsetPointer(source, field->info.pointer_offset);
				destination = OffsetPointer(destination, field->info.pointer_offset);
			}

			if (options->deallocate_existing_buffers && IsStreamWithSoA(field->info.stream_type)) {
				ResizableStream<void> destination_stream_data = GetReflectionFieldResizableStreamVoid(field->info, destination, false);
				if (destination_stream_data.allocator.allocator == nullptr) {
					destination_stream_data.allocator = options->allocator;
				}
				// Here, we have the special pointer SoA case where we need to deallocate only the first pointer
				if (field->info.stream_type == ReflectionStreamFieldType::PointerSoA) {
					if (IsReflectionPointerSoAAllocationHolder(type, field_index)) {
						destination_stream_data.FreeBuffer();
					}
				}
				else {
					destination_stream_data.FreeBuffer();
				}
			}

			// If not a user defined type, can copy it
			if (field->info.basic_type != ReflectionBasicFieldType::UserDefined) {
				if (field->info.stream_type != ReflectionStreamFieldType::BasicTypeArray) {
					if (options->always_allocate_for_buffers && IsStreamWithSoA(field->info.stream_type)) {
						// Here we include the SoA pointer as well. We let the user take care of the
						// Merging of the SoA pointers
						ResizableStream<void> previous_data = GetReflectionFieldResizableStreamVoid(field->info, source, false);
						size_t allocation_size = GetReflectionFieldStreamElementByteSize(field->info) * previous_data.size;
						void* allocation = Allocate(options->allocator, allocation_size, field->info.stream_alignment);
						memcpy(allocation, previous_data.buffer, allocation_size);

						previous_data.buffer = allocation;
						previous_data.capacity = previous_data.size;
						SetReflectionFieldResizableStreamVoid(field->info, destination, previous_data, false);
					}
					else {
						memcpy(
							destination,
							source,
							field->info.byte_size
						);
					}
				}
				else {
					unsigned short copy_size = field->info.byte_size;
					memcpy(
						destination,
						source,
						copy_size
					);
				}
			}
			else {
				ECS_ASSERT(reflection_manager != nullptr);

				// Check blittable types
				ulong2 blittable = reflection_manager->FindBlittableException(field->definition);
				if (blittable.x != -1) {
					memcpy(
						destination,
						source,
						blittable.x
					);
				}
				else {
					// Call the functor again
					ReflectionType nested_type;
					if (reflection_manager->TryGetType(field->definition, nested_type)) {
						if (field->info.stream_type == ReflectionStreamFieldType::Basic) {
							CopyReflectionTypeInstance(
								reflection_manager,
								&nested_type,
								source,
								destination,
								options
							);
						}
						else {
							ResizableStream<void> source_data = GetReflectionFieldResizableStreamVoidEx(field->info, source, false);
							// Check to see if we need to allocate
							if (options->always_allocate_for_buffers) {
								size_t allocation_size = GetReflectionFieldStreamElementByteSize(field->info) * source_data.size;
								void* allocation = Allocate(options->allocator, allocation_size, field->info.stream_alignment);
								memcpy(allocation, source_data.buffer, allocation_size);
								source_data.buffer = allocation;
								source_data.capacity = source_data.size;
							}

							// Can set the data directly
							SetReflectionFieldResizableStreamVoidEx(field->info, destination, source_data, false);
						}
					}
					else {
						// Check for custom type
						unsigned int custom_type_index = FindReflectionCustomType(field->definition);
						if (custom_type_index != -1) {
							// Copy it
							if (options->always_allocate_for_buffers) {
								unsigned int custom_index = FindReflectionCustomType(field->definition);
								if (custom_index != -1) {
									ReflectionCustomTypeIsBlittableData trivially_data;
									trivially_data.definition = field->definition;
									trivially_data.reflection_manager = reflection_manager;
									bool is_blittable = ECS_REFLECTION_CUSTOM_TYPES[custom_index]->IsBlittable(&trivially_data);
									if (is_blittable) {
										ReflectionCustomTypeByteSizeData byte_size_data;
										byte_size_data.definition = field->definition;
										byte_size_data.reflection_manager = reflection_manager;
										size_t byte_size = ECS_REFLECTION_CUSTOM_TYPES[custom_index]->GetByteSize(&byte_size_data).x;

										memcpy(destination, source, byte_size);
									}
									else {
										ReflectionCustomTypeCopyData copy_data;
										copy_data.allocator = options->allocator;
										copy_data.definition = field->definition;
										copy_data.destination = destination;
										copy_data.source = source;
										copy_data.reflection_manager = reflection_manager;
										copy_data.deallocate_existing_data = options->deallocate_existing_buffers;

										ECS_REFLECTION_CUSTOM_TYPES[custom_index]->Copy(&copy_data);
									}
								}
							}
							else {
								memcpy(
									destination,
									source,
									field->info.byte_size
								);
							}
						}
						else {
							// Check the pointer case - pointer to user defined types
							// Gets here
							if (IsPointerWithSoA(field->info.stream_type)) {
								// Can mempcy the pointer
								memcpy(
									destination,
									source,
									field->info.byte_size
								);
							}
							else {
								// Enum or error.
								// Call the default instance data
								reflection_manager->SetInstanceFieldDefaultData(field, destination, false);
							}
						}
					}
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void DeallocateReflectionInstanceBuffers(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			void* source,
			AllocatorPolymorphic allocator,
			size_t element_count,
			size_t element_byte_size,
			bool reset_buffers
		) {
			ReflectionType user_type;
			if (reflection_manager->TryGetType(definition, user_type)) {
				DeallocateReflectionTypeInstanceBuffers(reflection_manager, &user_type, source, allocator, element_count, element_byte_size, reset_buffers);
			}
			else {
				unsigned int custom_index = FindReflectionCustomType(definition);
				if (custom_index != -1) {
					ReflectionCustomTypeDeallocateData deallocate_data;
					deallocate_data.definition = definition;
					deallocate_data.allocator = allocator;
					deallocate_data.reflection_manager = reflection_manager;
					deallocate_data.reset_buffers = reset_buffers;
					deallocate_data.source = source;
					deallocate_data.element_count = element_count;
					deallocate_data.element_byte_size = element_byte_size;
					ECS_REFLECTION_CUSTOM_TYPES[custom_index]->Deallocate(&deallocate_data);
				}
				else {
					// Blittable type/exception or enum or error
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void DeallocateReflectionTypeInstanceBuffers(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			void* source,
			AllocatorPolymorphic allocator,
			size_t element_count,
			size_t element_byte_size,
			bool reset_buffers
		) {
			// We can early exit in case this is a blittable type
			if (IsBlittable(type)) {
				return;
			}

			// Iterate over the fields in order to reduce the branching required
			for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
				if (type->fields[field_index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					// Pre-cache the reflection type of the nested type or the custom reflection index
					// If neither of these is set, then we can simply skip the step
					
					// The functor will be called with (Stream<void> user_defined_stream, size_t current_element_byte_size)
					// As arguments
					auto deallocate_buffers = [&](auto functor) {
						for (size_t index = 0; index < element_count; index++) {
							void* current_source = OffsetPointer(source, index * element_byte_size);

							Stream<void> user_defined_stream;
							size_t current_element_byte_size = 0;
							if (type->fields[field_index].info.stream_type == ReflectionStreamFieldType::Basic) {
								user_defined_stream = { OffsetPointer(current_source, type->fields[field_index].info.pointer_offset), 1 };
							}
							else {
								current_element_byte_size = GetReflectionFieldStreamElementByteSize(type->fields[field_index].info);
								user_defined_stream = GetReflectionFieldStreamVoidEx(type->fields[field_index].info, current_source);
							}

							// The size of the stream is the element count
							functor(user_defined_stream, current_element_byte_size);
						}
					};

					ReflectionType nested_type;
					if (reflection_manager->TryGetType(type->fields[field_index].definition, nested_type)) {
						deallocate_buffers([&](Stream<void> user_defined_stream, size_t current_element_byte_size) {
							DeallocateReflectionTypeInstanceBuffers(
								reflection_manager,
								&nested_type,
								user_defined_stream.buffer,
								allocator,
								user_defined_stream.size,
								current_element_byte_size,
								reset_buffers
							);
						});
					}
					unsigned int nested_custom_index = FindReflectionCustomType(type->fields[field_index].definition);
					if (nested_custom_index != -1) {
						deallocate_buffers([&](Stream<void> user_defined_stream, size_t current_element_byte_size) {
							ReflectionCustomTypeDeallocateData deallocate_data;
							deallocate_data.allocator = allocator;
							deallocate_data.definition = type->fields[field_index].definition;
							deallocate_data.element_byte_size = current_element_byte_size;
							deallocate_data.element_count = user_defined_stream.size;
							deallocate_data.reflection_manager = reflection_manager;
							deallocate_data.reset_buffers = reset_buffers;
							deallocate_data.source = user_defined_stream.buffer;
							ECS_REFLECTION_CUSTOM_TYPES[nested_custom_index]->Deallocate(&deallocate_data);
						});
					}
				}

				// The SoA needs to be handled separately
				if (IsStream(type->fields[field_index].info.stream_type)) {
					for (size_t index = 0; index < element_count; index++) {
						void* current_source = OffsetPointer(source, index * element_byte_size);
						ResizableStream<void> buffer_data = GetReflectionFieldResizableStreamVoid(type->fields[field_index].info, current_source);
						if (type->fields[field_index].info.stream_type != ReflectionStreamFieldType::ResizableStream) {
							buffer_data.allocator = allocator;
						}
						buffer_data.FreeBuffer();
						if (reset_buffers) {
							SetReflectionFieldResizableStreamVoid(type->fields[field_index].info, current_source, buffer_data);
						}
					}
				}
			}

			for (size_t misc_index = 0; misc_index < type->misc_info.size; misc_index++) {
				if (type->misc_info[misc_index].type == ECS_REFLECTION_TYPE_MISC_INFO_SOA) {
					for (size_t index = 0; index < element_count; index++) {
						void* current_source = OffsetPointer(source, index * element_byte_size);
						Stream<void> buffer_data = GetReflectionTypeSoaStream(type, current_source, misc_index);
						buffer_data.Deallocate(allocator);
						if (reset_buffers) {
							ResizableStream<void> empty_data;
							SetReflectionFieldSoaStream(type, &type->misc_info[misc_index].soa, current_source, empty_data);
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

		ReflectionTypeFieldDeep ConvertReflectionNestedFieldIndexToDeep(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* type, 
			ReflectionNestedFieldIndex field_index
		)
		{
			ReflectionTypeFieldDeep deep_field;

			deep_field.type = type;
			if (field_index.count > 0) {
				for (unsigned char index = 0; index < field_index.count - 1; index++) {
					deep_field.type = reflection_manager->GetType(deep_field.type->fields[field_index.indices[index]].definition);
				}
				deep_field.type_offset_from_original = deep_field.type->fields[field_index.indices[field_index.count - 1]].info.pointer_offset;
				deep_field.field_index = field_index.indices[field_index.count - 1];
			}
			else {
				deep_field.field_index = -1;
				deep_field.type_offset_from_original = 0;
			}

			return deep_field;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CompareReflectionTypeBufferData(
			ReflectionTypeFieldDeep deep_field,
			const void* target,
			CapacityStream<void> compare_data
		) {
			const ReflectionField& field = deep_field.type->fields[deep_field.field_index];
			if (field.info.stream_type == ReflectionStreamFieldType::PointerSoA) {
				const void* pointer_start = OffsetPointer(target, deep_field.type_offset_from_original);
				size_t soa_index = GetReflectionTypeSoaIndex(deep_field.type, deep_field.field_index);
				const ReflectionTypeMiscSoa& soa = deep_field.type->misc_info[soa_index].soa;
				
				unsigned short initial_offset = deep_field.type_offset_from_original;
				short size_offset_direction = (short)deep_field.type->fields[soa.size_field].info.pointer_offset -
					(short)field.info.pointer_offset;
				size_t soa_size = ConvertToSizetFromBasic(deep_field.type->fields[soa.size_field].info.basic_type, OffsetPointer(pointer_start, size_offset_direction));
				if (soa_size != compare_data.size) {
					return false;
				}

				size_t soa_capacity = soa_size;
				if (soa.HasCapacity()) {
					short capacity_offset_direction = (short)deep_field.type->fields[soa.capacity_field].info.pointer_offset -
						(short)field.info.pointer_offset;
					soa_capacity = ConvertToSizetFromBasic(deep_field.type->fields[soa.capacity_field].info.basic_type, OffsetPointer(pointer_start, capacity_offset_direction));
				}

				const void* current_soa_data = *(const void**)pointer_start;
				for (unsigned char index = 0; index < soa.parallel_stream_count; index++) {
					size_t current_element_byte_size = GetReflectionFieldStreamElementByteSize(deep_field.type->fields[soa.parallel_streams[index]].info);
					if (memcmp(current_soa_data, compare_data.buffer, current_element_byte_size * soa_size) != 0) {
						return false;
					}
					current_soa_data = OffsetPointer(current_soa_data, current_element_byte_size * soa_capacity);
					compare_data.buffer = OffsetPointer(compare_data.buffer, current_element_byte_size * compare_data.capacity);
				}

				return true;
			}
			else {
				if (IsStream(field.info.stream_type) || field.info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					Stream<void> data = GetReflectionFieldStreamVoid(field.info, OffsetPointer(target, deep_field.type_offset_from_original), false);
					return data.Compare(compare_data);
				}
				else {
					// Error. No buffer
					ECS_ASSERT(false, "No buffer when trying to compare buffer data of a reflection type instance");
				}
			}

			return false;
		}
		
		// ----------------------------------------------------------------------------------------------------------------------------

		void SetReflectionTypeInstanceBuffer(
			ReflectionTypeFieldDeep deep_field,
			void* target,
			CapacityStream<void> buffer_data,
			const SetReflectionTypeInstanceBufferOptions* options
		) {
			SetReflectionTypeInstanceBufferOptions default_options;
			default_options.deallocate_existing = false;
			if (options == nullptr) {
				options = &default_options;
			}

			const ReflectionType* type = deep_field.type;
			const ReflectionField& field = type->fields[deep_field.field_index];
			void* offsetted_target = OffsetPointer(target, deep_field.type_offset_from_original);
			ResizableStream<void> current_data = GetReflectionFieldResizableStreamVoid(field.info, offsetted_target, false);
			
			bool perform_set = true;
			if (options->checked_copy) {
				perform_set = !CompareReflectionTypeBufferData(deep_field, target, buffer_data);
			}

			if (perform_set) {
				if (options->deallocate_existing) {
					// Only for resizable streams use the free buffer
					// For all the other streams, deallocate using the allocator from the options
					if (field.info.stream_type == ReflectionStreamFieldType::ResizableStream) {
						current_data.FreeBuffer();
					}
					else {
						// We don't have to set this to nullptr since
						// We are overriding this field anyway
						DeallocateEx(options->allocator, current_data.buffer);
						// Reset the buffer - needed for the ResizeNoCopy call
						// Later on to not think that we have data
						current_data.buffer = nullptr;
						current_data.size = 0;
						current_data.capacity = 0;
					}
				}

				if (current_data.allocator.allocator == nullptr) {
					current_data.allocator = options->allocator;
				}
				size_t per_element_byte_size = 0;
				size_t soa_index = -1;
				if (field.info.stream_type == ReflectionStreamFieldType::PointerSoA) {
					soa_index = GetReflectionTypeSoaIndex(deep_field.type, deep_field.field_index);
					per_element_byte_size = GetReflectionPointerSoAPerElementSize(deep_field.type, soa_index);
				}
				else {
					per_element_byte_size = GetReflectionFieldStreamElementByteSize(field.info);
				}
				current_data.ResizeNoCopy(per_element_byte_size * buffer_data.capacity);
				current_data.CopyOther(buffer_data);
				if (field.info.stream_type == ReflectionStreamFieldType::PointerSoA) {
					// We need to offset back to the beginning of the nested type
					SetReflectionFieldSoaStream(
						deep_field.type,
						&deep_field.type->misc_info[soa_index].soa,
						OffsetPointer(target, deep_field.NestedTypeOffset()),
						current_data
					);
				}
				else {
					SetReflectionFieldResizableStreamVoid(
						field.info,
						offsetted_target,
						current_data,
						false
					);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionTypeFieldDeep FindReflectionTypeFieldDeep(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* reflection_type, 
			Stream<char> field
		)
		{
			ECS_STACK_CAPACITY_STREAM(Stream<char>, subfields, 32);
			// Split the field by dots
			SplitString(field, ".", &subfields);

			ReflectionTypeFieldDeep deep_field;
			deep_field.type = nullptr;
			deep_field.field_index = -1;
			deep_field.type_offset_from_original = 0;

			if (subfields.size == 1) {
				// Normal find
				deep_field.type = reflection_type;
				deep_field.field_index = reflection_type->FindField(field);
				if (deep_field.field_index != -1) {
					deep_field.type_offset_from_original = reflection_type->fields[deep_field.field_index].info.pointer_offset;
				}
				return deep_field;
			}
			else {
				while (subfields.size > 1) {
					unsigned int current_field_index = reflection_type->FindField(subfields[0]);
					if (current_field_index == -1) {
						return deep_field;
					}

					deep_field.type_offset_from_original += reflection_type->fields[current_field_index].info.pointer_offset;
					Stream<char> nested_type_definition = reflection_type->fields[current_field_index].definition;
					reflection_type = reflection_manager->GetType(nested_type_definition);
					subfields.Advance();
				}

				deep_field.type = reflection_type;
				deep_field.field_index = reflection_type->FindField(subfields[0]);
				return deep_field;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ConstructReflectionTypeDependencyGraph(Stream<ReflectionType> types, CapacityStream<Stream<char>>& ordered_types, CapacityStream<uint2>& subgroups)
		{
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, valid_mask, types.size);
			valid_mask.size = types.size;
			MakeSequence(valid_mask);

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
							if (FindString(current_dependencies[dependency_index], ordered_types.ToStream()) == -1) {
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
						if (field->definition == subtype) {
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
									ECS_REFLECTION_CUSTOM_TYPES[custom_serializer_index]->GetDependentTypes(&dependent_data);

									for (unsigned int subindex = 0; subindex < dependent_data.dependent_types.size; subindex++) {
										if (subtype == dependent_data.dependent_types[subindex]) {
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
						if (FindFirstToken(type->fields[index].definition, subtype).size > 0) {
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

			Stream<char> comma = FindFirstCharacter(tag, ',');

			ulong2 result;
			result.y = 8; // assume alignment 8
			if (comma.size > 0) {
				// Parse the alignment
				result.y = ConvertCharactersToInt(comma);
				ECS_ASSERT(result.y == 1 || result.y == 2 || result.y == 4 || result.y == 8);
				tag = { tag.buffer, tag.size - comma.size };
			}
			
			result.x = ConvertCharactersToInt(tag);
			if (result.x < 8) {
				result.y = PowerOfTwoGreater(result.x) / 2;
			}
			return result;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 GetReflectionFieldSkipMacroByteSize(const ReflectionField* field)
		{
			Stream<char> tag = field->GetTag(STRING(ECS_SKIP_REFLECTION));
			Stream<char> opened_paranthese = FindFirstCharacter(tag, '(');
			Stream<char> closed_paranthese = FindMatchingParenthesis(opened_paranthese.buffer + 1, tag.buffer + tag.size, '(', ')');

			opened_paranthese.Advance();
			Stream<char> removed_whitespace = SkipWhitespace(opened_paranthese);
			if (removed_whitespace.buffer == closed_paranthese.buffer) {
				return { (size_t)-1, (size_t)-1 };
			}

			Stream<char> comma = FindFirstCharacter(opened_paranthese, ',');
			size_t byte_size = -1;
			size_t alignment = -1;

			if (comma.size == 0) {
				// No alignment specified
				byte_size = ConvertCharactersToInt(opened_paranthese);
			}
			else {
				// Has alignment specified
				byte_size = ConvertCharactersToInt(Stream<char>(opened_paranthese.buffer, comma.buffer - opened_paranthese.buffer));
				alignment = ConvertCharactersToInt(comma);
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
					ECS_REFLECTION_CUSTOM_TYPES[custom_type]->GetDependentTypes(&dependent_types);
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
				ECS_REFLECTION_CUSTOM_TYPES[custom_type]->GetDependentTypes(&dependent_types);
				dependencies.size += dependent_types.dependent_types.size;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionDataPointerElementByteSize(const ReflectionManager* manager, Stream<char> tag)
		{
			tag = FindFirstCharacter(tag, '(');
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
			return ConvertCharactersToInt(tag);
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
									ECS_REFLECTION_CUSTOM_TYPES[custom_type_index]->GetDependentTypes(&dependent_data);
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

		static void TypeChangeAddIndex(ReflectionTypeChange* entry, unsigned int index) {
			ECS_ASSERT(entry->indices_count < ECS_COUNTOF(entry->indices));
			entry->indices[entry->indices_count++] = index;
		}

		static void TypeChangeAddIndices(ReflectionTypeChange* entry, const ReflectionTypeChange* parent) {
			entry->indices_count = parent->indices_count;
			memcpy(entry->indices, parent->indices, sizeof(parent->indices[0]) * parent->indices_count);
		}

		static void TypeChangeAddIndexWithParent(ReflectionTypeChange* entry, const ReflectionTypeChange* parent, unsigned int index) {
			TypeChangeAddIndices(entry, parent);
			TypeChangeAddIndex(entry, index);
		}

		// The current level is used to write all the previous indices in the current entry
		static void DetermineReflectionTypeChangeSetImpl(
			const ReflectionManager* previous_reflection_manager,
			const ReflectionManager* new_reflection_manager,
			const ReflectionType* previous_type,
			const ReflectionType* new_type,
			AdditionStream<ReflectionTypeChange> changes,
			ReflectionTypeChange* current_level
		) {
			// Determine the additions
			for (size_t index = 0; index < new_type->fields.size; index++) {
				if (previous_type->FindField(new_type->fields[index].name) == -1) {
					ReflectionTypeChange* change = changes.Reserve();
					change->change_type = ECS_REFLECTION_TYPE_CHANGE_ADD;
					TypeChangeAddIndexWithParent(change, current_level, index);
				}
			}

			// Determine the removals
			for (size_t index = 0; index < previous_type->fields.size; index++) {
				if (new_type->FindField(previous_type->fields[index].name) == -1) {
					ReflectionTypeChange* change = changes.Reserve();
					change->change_type = ECS_REFLECTION_TYPE_CHANGE_REMOVE;
					TypeChangeAddIndexWithParent(change, current_level, index);
				}
			}

			// Determine the updates
			for (size_t index = 0; index < new_type->fields.size; index++) {
				unsigned int previous_index = previous_type->FindField(previous_type->fields[index].name);
				if (previous_index != -1) {
					if (previous_type->fields[previous_index].definition != new_type->fields[index].definition) {
						ReflectionTypeChange* change = changes.Reserve();
						change->change_type = ECS_REFLECTION_TYPE_CHANGE_UPDATE;
						TypeChangeAddIndexWithParent(change, current_level, index);
					}
					else {
						// They have the same definition, check for nested type changes
						if (previous_type->fields[previous_index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
							// Check to see if we have a nested type or not
							// Must be able to get it from both reflection managers
							ReflectionType previous_nested_type;
							if (previous_reflection_manager->TryGetType(previous_type->fields[previous_index].definition, previous_nested_type)) {
								ReflectionType new_nested_type;
								if (new_reflection_manager->TryGetType(new_type->fields[index].definition, new_nested_type)) {
									TypeChangeAddIndex(current_level, index);
									DetermineReflectionTypeChangeSet(
										previous_reflection_manager,
										new_reflection_manager,
										&previous_nested_type,
										&new_nested_type,
										changes
									);
									current_level->indices_count--;
								}
							}
						}
					}
				}
			}
		}

		void DetermineReflectionTypeChangeSet(
			const ReflectionManager* previous_reflection_manager, 
			const ReflectionManager* new_reflection_manager, 
			const ReflectionType* previous_type, 
			const ReflectionType* new_type, 
			AdditionStream<ReflectionTypeChange> changes
		)
		{
			ReflectionTypeChange change_level;
			change_level.indices_count = 0;
			DetermineReflectionTypeChangeSetImpl(
				previous_reflection_manager,
				new_reflection_manager,
				previous_type,
				new_type,
				changes,
				&change_level
			);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		static void DetermineReflectionTypeInstanceUpdatesImpl(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			const void* first_data,
			const void* second_data,
			AdditionStream<ReflectionTypeChange> updates,
			ReflectionTypeChange* current_level
		) {
			for (size_t index = 0; index < type->fields.size; index++) {
				if (!CompareReflectionFieldInstances(reflection_manager, &type->fields[index], first_data, second_data)) {
					if (type->fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
						// If it is a user defined type and we can retrieve the nested type,
						// We need to determine the update for each of its fields
						ReflectionType nested_type;
						if (reflection_manager->TryGetType(type->fields[index].definition, nested_type)) {
							TypeChangeAddIndex(current_level, index);
							DetermineReflectionTypeInstanceUpdatesImpl(reflection_manager, &nested_type, first_data, second_data, updates, current_level);
							current_level->indices_count--;
						}
						else {
							ReflectionTypeChange* update = updates.Reserve();
							update->change_type = ECS_REFLECTION_TYPE_CHANGE_UPDATE;
							TypeChangeAddIndexWithParent(update, current_level, index);
						}
					}
					else {
						ReflectionTypeChange* update = updates.Reserve();
						update->change_type = ECS_REFLECTION_TYPE_CHANGE_UPDATE;
						TypeChangeAddIndexWithParent(update, current_level, index);
					}
				}
			}
		}

		void DetermineReflectionTypeInstanceUpdates(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* type, 
			const void* first_data, 
			const void* second_data, 
			AdditionStream<ReflectionTypeChange> updates
		)
		{
			ReflectionTypeChange level;
			DetermineReflectionTypeInstanceUpdatesImpl(
				reflection_manager,
				type,
				first_data,
				second_data,
				updates,
				&level
			);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void UpdateReflectionTypeInstanceFromChanges(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* type, 
			void* destination_data, 
			const void* source_data, 
			Stream<ReflectionTypeChange> changes,
			AllocatorPolymorphic allocator
		)
		{
			UpdateReflectionTypeInstancesFromChanges(reflection_manager, type, { &destination_data, 1 }, source_data, changes, allocator);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void UpdateReflectionTypeInstancesFromChanges(
			const ReflectionManager* reflection_manager, 
			const ReflectionType* type, 
			Stream<void*> destinations, 
			const void* source_data, 
			Stream<ReflectionTypeChange> changes, 
			AllocatorPolymorphic allocator
		)
		{
			ECS_STACK_CAPACITY_STREAM(ReflectionType, current_types, 512);
			ECS_STACK_CAPACITY_STREAM(unsigned int, data_offsets, 512);
			current_types.Add(*type);
			data_offsets.Add((unsigned int)0);
			for (size_t index = 0; index < changes.size; index++) {
				if (changes[index].indices_count > current_types.size) {
					for (unsigned int subindex = current_types.size; subindex < changes[index].indices_count - 1; subindex++) {
						data_offsets.Add(data_offsets[data_offsets.size - 1] + current_types[current_types.size - 1].fields[changes[index].indices[subindex]].info.pointer_offset);
						const ReflectionType* nested_type = reflection_manager->GetType(current_types[subindex].fields[changes[index].indices[subindex]].definition);
						current_types.AddAssert(*nested_type);
					}
				}
				if (changes[index].indices_count < current_types.size) {
					current_types.size = changes[index].indices_count;
					data_offsets.size = changes[index].indices_count;
				}
				unsigned int current_field_index = changes[index].indices[changes[index].indices_count - 1];
				unsigned int current_data_offset = data_offsets[data_offsets.size - 1] + current_field_index;
				const void* current_source_data = OffsetPointer(source_data, current_data_offset);
				CopyReflectionDataOptions copy_options;
				if (allocator.allocator != nullptr) {
					copy_options.allocator = allocator;
					copy_options.always_allocate_for_buffers = true;
					copy_options.deallocate_existing_buffers = true;
				}
				for (size_t destination_index = 0; destination_index < destinations.size; destination_index++) {
					void* current_destination_data = OffsetPointer(destinations[destination_index], current_data_offset);
					CopyReflectionFieldInstance(
						reflection_manager, 
						&current_types[current_types.size - 1], 
						current_field_index, 
						current_source_data, 
						current_destination_data, 
						&copy_options
					);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
