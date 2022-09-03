#include "ecspch.h"
#include "Reflection.h"
#include "ReflectionMacros.h"
#include "../File.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../ForEachFiles.h"
#include "ReflectionStringFunctions.h"
#include "../ReferenceCountSerialize.h"
#include "../../Containers/SparseSet.h"

#include "../../Internal/Resources/AssetMetadataSerialize.h"

namespace ECSEngine {

	namespace Reflection {

#pragma region Container types

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data)
		{
			const char* opened_bracket = strchr(data->definition.buffer, '<');
			ECS_ASSERT(opened_bracket != nullptr);

			const char* closed_bracket = strchr(opened_bracket, '>');
			ECS_ASSERT(closed_bracket != nullptr);

			data->dependent_types.AddSafe({ opened_bracket + 1, function::PointerDifference(closed_bracket, opened_bracket) - 1 });
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
			Stream<char> closed_bracket = function::FindFirstCharacter(definition, '>');
			ECS_ASSERT(opened_bracket.buffer != nullptr && closed_bracket.buffer != nullptr);

			Stream<char> type = { opened_bracket.buffer + 1, function::PointerDifference(closed_bracket.buffer, opened_bracket.buffer) - 1 };
			return type;
		}

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

		ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(Color) {
			return function::CompareStrings(("Color"), data->definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(Color) {
			return { sizeof(unsigned char) * 4, alignof(unsigned char) };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(Color) {}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(ColorFloat) {
			return function::CompareStrings(("ColorFloat"), data->definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(ColorFloat) {
			return { sizeof(float) * 4, alignof(float) };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(ColorFloat) {}

		// ----------------------------------------------------------------------------------------------------------------------------

		// TODO: move this to another file
		ReflectionCustomType ECS_REFLECTION_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_COUNT] = {
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(Stream),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(ReferenceCounted),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(SparseSet),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(Color),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(ColorFloat),
			ECS_REFLECTION_CUSTOM_TYPE_STRUCT(MaterialAsset)
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

		// Updates the type.tag
		void AddTypeTag(
			ReflectionManager* reflection,
			Stream<char> type_tag,
			ReflectionType& type
		) {
			size_t tag_index = 0;
			for (; tag_index < reflection->type_tags.size; tag_index++) {
				if (function::CompareStrings(reflection->type_tags[tag_index].tag_name, type_tag)) {
					reflection->type_tags[tag_index].type_names.Add(type.name);
					type.tag = reflection->type_tags[tag_index].tag_name;
					break;
				}
			}
			if (tag_index == reflection->type_tags.size) {
				Stream<char> type_tag_copy = function::StringCopy(reflection->folders.allocator, type_tag);
				tag_index = reflection->type_tags.Add({ type_tag_copy, ResizableStream<Stream<char>>(reflection->folders.allocator, 1) });
				reflection->type_tags[tag_index].type_names.Add(type.name);
				type.tag = type_tag_copy;
			}
		}

		void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* _data);
		void ReflectionManagerHasReflectStructuresThreadTask(unsigned int thread_id, World* world, void* _data);

		void ReflectionManagerStylizeEnum(ReflectionEnum enum_);

		// These are pending expressions to be evaluated
		struct ReflectionExpression {
			Stream<char> name;
			Stream<char> body;
		};

		struct ReflectionManagerParseStructuresThreadTaskData {
			CapacityStream<char> thread_memory;
			Stream<Stream<wchar_t>> paths;
			CapacityStream<ReflectionType> types;
			CapacityStream<ReflectionEnum> enums;
			CapacityStream<ReflectionConstant> constants;
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

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionManager::ReflectionManager(MemoryManager* allocator, size_t type_count, size_t enum_count) : folders(GetAllocatorPolymorphic(allocator), 0)
		{
			InitializeFieldTable();
			InitializeTypeTable(type_count);
			InitializeEnumTable(enum_count);

			type_tags.Initialize(GetAllocatorPolymorphic(allocator), 0);
			constants.Initialize(GetAllocatorPolymorphic(allocator), 0);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearTypeDefinitions()
		{
			type_definitions.ForEachConst([&](ReflectionType type, ResourceIdentifier identifier) {
				Deallocate(folders.allocator, type.name.buffer);
			});
			type_definitions.Clear();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::BindApprovedData(
			const ReflectionManagerParseStructuresThreadTaskData* data,
			unsigned int data_count,
			unsigned int folder_index
		)
		{
			size_t total_memory = 0;
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				total_memory += data[data_index].total_memory;
			}

			void* allocation = Allocate(folders.allocator, total_memory);
			uintptr_t ptr = (uintptr_t)allocation;

			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].types.size; index++) {
					const ReflectionType* data_type = &data[data_index].types[index];

					ReflectionType type = data_type->Copy(ptr);
					type.folder_hierarchy_index = folder_index;
					ResourceIdentifier identifier(type.name);
					// If the type already exists, fail
					if (type_definitions.Find(identifier) != -1) {
						FreeFolderHierarchy(folder_index);
						return false;
					}
					ECS_ASSERT(!type_definitions.Insert(type, identifier));
				}

				// TODO: Do we need a disable for the stylized labels?
				for (size_t index = 0; index < data[data_index].enums.size; index++) {
					const ReflectionEnum* data_enum = &data[data_index].enums[index];
					ReflectionEnum temp_copy = *data_enum;
					// Stylized the labels such that they don't appear excessively long
					ReflectionManagerStylizeEnum(temp_copy);

					ReflectionEnum enum_ = temp_copy.Copy(ptr);
					enum_.folder_hierarchy_index = folder_index;
					ResourceIdentifier identifier(enum_.name);
					if (enum_definitions.Find(identifier) != -1) {
						// If the enum already exists, fail
						FreeFolderHierarchy(folder_index);
						return false;
					}
					ECS_ASSERT(!enum_definitions.Insert(enum_, identifier));
				}

				// The constants now
				for (size_t constant_index = 0; constant_index < data[data_index].constants.size; constant_index++) {
					// Set the folder index before
					ReflectionConstant constant = data[data_index].constants[constant_index];
					constant.folder_hierarchy = folder_index;
					// Copy the name
					char* new_name = (char*)ptr;
					constant.name.CopyTo(ptr);
					constant.name.buffer = new_name;
					constants.Add(constant);
				}
			}

			folders[folder_index].allocated_buffer = allocation;

			// We need to keep the byte sizes from the ECS_SKIP_REFLECTION for all types inside the type definitions
			// And add them at the end

			// This is because if a type has multiple user defined types when recalculating the byte size
			// its going to be added multiple times
			unsigned int extended_capacity = type_definitions.GetExtendedCapacity();
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(size_t, type_byte_sizes, extended_capacity);

			// Go through all the types and calculate their byte size and alignment
			type_definitions.ForEachIndex([&](unsigned int index) {
				ReflectionType* type = type_definitions.GetValuePtrFromIndex(index);

				if (type->folder_hierarchy_index == folder_index) {
					// If it doesn't have any user defined fields, can calculate it
					size_t field_index = 0;
					for (; field_index < type->fields.size; field_index++) {
						if (type->fields[field_index].info.basic_type == ReflectionBasicFieldType::UserDefined
							&& type->fields[field_index].info.stream_type == ReflectionStreamFieldType::Basic ||
							type->fields[field_index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
							break;
						}
					}

					type_byte_sizes[index] = type->byte_size;

					if (field_index == type->fields.size) {
						type->byte_size = CalculateReflectionTypeByteSize(this, type);
						type->alignment = CalculateReflectionTypeAlignment(this, type);
					}
				}
				
				return false;
			});

			// If some fields depend directly on another type and when trying to get their byte size it fails,
			// then retry at the end
			ECS_STACK_CAPACITY_STREAM(uint3, retry_fields, 256);

			// Resolve references to the fields which reference other user defined types
			// Or custom containers
			ReflectionType* type = nullptr;

			auto update_type_field_user_defined = [&](Stream<char> definition, size_t data_index, size_t index, size_t field_index) {
				type = type_definitions.GetValuePtr(data[data_index].types[index].name);

				// If its a stream type, we need to update its stream_byte_size
				// Else we need to update the whole current structure
				ReflectionStreamFieldType stream_type = type->fields[field_index].info.stream_type;
				struct RestoreTemplateEnd {
					void operator() () {
						if (end != nullptr) {
							*end = previous_char;
						}
					}
					char* end;
					char previous_char;
				};

				StackScope<RestoreTemplateEnd> restore_template_end({ nullptr, '\0' });

				if (stream_type != ReflectionStreamFieldType::Basic) {
					// Extract the new definition
					if (IsStream(stream_type)) {
						Stream<char> template_start = function::FindFirstCharacter(definition, '<');
						Stream<char> template_end = function::FindCharacterReverse(definition, '>');
						ECS_ASSERT(template_start.buffer != nullptr);
						ECS_ASSERT(template_end.buffer != nullptr);

						restore_template_end.deallocator.end = template_end.buffer;
						restore_template_end.deallocator.previous_char = '>';

						template_end[0] = '\0';
						char* new_definition = (char*)function::SkipWhitespace(template_start.buffer + 1);
						definition.size = function::PointerDifference(function::SkipWhitespace(template_end.buffer - 1, -1), new_definition) + 1;
						definition.buffer = new_definition;
					}
				}

				unsigned int container_type_index = FindReflectionCustomType(definition);

				size_t byte_size = -1;
				size_t alignment = -1;
				if (container_type_index != -1) {
					// Get its byte size
					ReflectionCustomTypeByteSizeData byte_size_data;
					byte_size_data.definition = definition;
					byte_size_data.reflection_manager = this;
					ulong2 byte_size_alignment = ECS_REFLECTION_CUSTOM_TYPES[container_type_index].byte_size(&byte_size_data);
					byte_size = byte_size_alignment.x;
					alignment = byte_size_alignment.y;

					if (byte_size == 0) {
						retry_fields.AddSafe({ (unsigned int)data_index, (unsigned int)index, (unsigned int)field_index });
						return true;
					}
				}
				else {
					// Try to get the user defined type
					ReflectionType nested_type;
					if (TryGetType(definition, nested_type)) {
						byte_size = GetReflectionTypeByteSize(&nested_type);
						alignment = GetReflectionTypeAlignment(&nested_type);
					}
					else {
						// Look to see if it is an enum
						ReflectionEnum reflection_enum;
						if (TryGetEnum(definition, reflection_enum)) {
							byte_size = sizeof(unsigned char);
							alignment = alignof(unsigned char);

							// Update the field type to enum
							type->fields[field_index].info.basic_type = ReflectionBasicFieldType::Enum;
						}
					}
				}

				// It means it is not a container, nor a user defined which could be referenced
				if (byte_size == -1 || alignment == -1) {
					// Fail
					return false;
				}

				if (byte_size != 0) {
					// Only if it is a basic type we need to relocate all the other fields
					if (stream_type == ReflectionStreamFieldType::Basic) {
						size_t aligned_offset = function::AlignPointer(type->fields[field_index].info.pointer_offset, alignment);
						size_t alignment_difference = aligned_offset - type->fields[field_index].info.pointer_offset;

						type->fields[field_index].info.pointer_offset = aligned_offset;
						type->fields[field_index].info.byte_size = byte_size;
						for (size_t subindex = field_index + 1; subindex < type->fields.size; subindex++) {
							type->fields[subindex].info.pointer_offset += alignment_difference + byte_size;

							size_t current_alignment = GetFieldTypeAlignmentEx(this, type->fields[subindex]);
							aligned_offset = function::AlignPointer(type->fields[subindex].info.pointer_offset, current_alignment);

							alignment_difference += aligned_offset - type->fields[subindex].info.pointer_offset;
							type->fields[subindex].info.pointer_offset = aligned_offset;
						}

						// Recalculate the byte size and the alignment
						type->byte_size = CalculateReflectionTypeByteSize(this, type);
						type->alignment = CalculateReflectionTypeAlignment(this, type);
					}
					else {
						if (stream_type == ReflectionStreamFieldType::BasicTypeArray) {
							// Update the byte size
							type->fields[field_index].info.byte_size = type->fields[field_index].info.basic_type_count * byte_size;
						}
						else {
							// For pointer and streams we need to update the stream byte size
							type->fields[field_index].info.stream_byte_size = byte_size;
						}
					}
				}
				return true;
			};
			
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].types.size; index++) {
					for (size_t field_index = 0; field_index < data[data_index].types[index].fields.size; field_index++) {
						const ReflectionField* field = &data[data_index].types[index].fields[field_index];
						if (field->info.basic_type == ReflectionBasicFieldType::UserDefined && !field->Skip(STRING(ECS_SKIP_REFLECTION))) 
						{
							if (!update_type_field_user_defined(field->definition, data_index, index, field_index)) {
								// Fail
								FreeFolderHierarchy(folder_index);
								return false;
							}
						}
					}
				}
			}

			for (size_t retry_index = 0; retry_index < retry_fields.size; retry_index++) {
				size_t data_index = retry_fields[retry_index].x;
				size_t index = retry_fields[retry_index].y;
				size_t field_index = retry_fields[retry_index].z;

				if (!update_type_field_user_defined(data[data_index].types[index].fields[field_index].definition, data_index, index, field_index)) {
					// Fail
					FreeFolderHierarchy(folder_index);
					return false;
				}
			}

			// Add the bytes sizes previously recorded
			type_definitions.ForEachIndex([&](unsigned int index) {
				ReflectionType* type = type_definitions.GetValuePtrFromIndex(index);

				if (type->folder_hierarchy_index == folder_index) {
					type->byte_size += type_byte_sizes[index];
				}

				return false;
			});

			bool evaluate_success = true;
			// And evaluate the expressions. This must be done in the end such that all types are reflected when we come to evaluate
			for (unsigned int data_index = 0; data_index < data_count; data_index++) {
				data[data_index].expressions.ForEachConst<true>([&](Stream<ReflectionExpression> expressions, ResourceIdentifier identifier) {
					// Set the stream first
					ReflectionType* type_ptr = type_definitions.GetValuePtr(identifier);
					type_ptr->evaluations.InitializeFromBuffer(ptr, expressions.size);

					for (size_t expression_index = 0; expression_index < expressions.size; expression_index++) {
						double result = EvaluateExpression(expressions[expression_index].body);
						if (result == DBL_MAX) {
							// Fail
							FreeFolderHierarchy(folder_index);
							evaluate_success = false;
							return true;
						}

						type_ptr->evaluations[expression_index].name.InitializeAndCopy(ptr, expressions[expression_index].name);
						type_ptr->evaluations[expression_index].value = result;
					}
					return false;
				});
			}

			size_t difference = function::PointerDifference((void*)ptr, allocation);
			ECS_ASSERT(difference <= total_memory);

			return evaluate_success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearEnumDefinitions() {
			enum_definitions.ForEach([&](ReflectionEnum enum_, ResourceIdentifier identifier) {
				Deallocate(folders.allocator, enum_.name.buffer);
			});
			enum_definitions.Clear();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::CreateFolderHierarchy(Stream<wchar_t> root) {
			unsigned int index = folders.ReserveNewElement();
			folders[index] = { function::StringCopy(folders.allocator, root), nullptr };
			folders.size++;
			return index;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data)
		{
			free(data.thread_memory.buffer);
			Deallocate(folders.allocator, data.types.buffer);
			Deallocate(folders.allocator, data.enums.buffer);
			Deallocate(folders.allocator, data.paths.buffer);
			Deallocate(folders.allocator, data.constants.buffer);
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

		void ReflectionManager::FreeFolderHierarchy(unsigned int folder_index)
		{
			// The types associated
			type_definitions.ForEachIndex([&](unsigned int index) {
				const ReflectionType* type_ptr = type_definitions.GetValuePtrFromIndex(index);

				if (type_ptr->folder_hierarchy_index == folder_index) {
					// Look to see if it belongs to a tag
					for (size_t tag_index = 0; tag_index < type_tags.size; tag_index++) {
						for (int64_t subtag_index = 0; subtag_index < (int64_t)type_tags[tag_index].type_names.size; subtag_index++) {
							if (function::CompareStrings(type_tags[tag_index].type_names[subtag_index], type_ptr->name)) {
								type_tags[tag_index].type_names.RemoveSwapBack(subtag_index);
								subtag_index--;
							}
						}
					}

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

		void ReflectionManager::GetHierarchyTypes(unsigned int hierarchy_index, ReflectionManagerGetQuery options) const {
			if (options.tags.size == 0) {
				type_definitions.ForEachIndexConst([&](unsigned int index) {
					const ReflectionType* type = type_definitions.GetValuePtrFromIndex(index);
					if (type->folder_hierarchy_index == hierarchy_index) {
						options.indices->AddSafe(index);
					}
				});
			}
			else {
				type_definitions.ForEachIndexConst([&](unsigned int index) {
					const ReflectionType* type = type_definitions.GetValuePtrFromIndex(index);
					if (type->folder_hierarchy_index == hierarchy_index) {
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
								options.stream_indices[tag_index].AddSafe(index);
							}
							else {
								options.indices->AddSafe(index);
							}
						}
					}
				});
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

		void ReflectionManager::GetHierarchyComponentTypes(unsigned int hierarchy_index, CapacityStream<unsigned int>* component_indices, CapacityStream<unsigned int>* shared_indices) const
		{
			ECS_STACK_CAPACITY_STREAM(CapacityStream<unsigned int>, stream_of_indices, 2);
			stream_of_indices[0] = *component_indices;
			stream_of_indices[1] = *shared_indices;
			stream_of_indices.size = 2;

			Stream<char> tags[] = {
				ECS_COMPONENT_TAG,
				ECS_SHARED_COMPONENT_TAG
			};

			ReflectionManagerGetQuery query_options;
			query_options.tags = { tags, std::size(tags) };
			query_options.stream_indices = stream_of_indices;
			query_options.strict_compare = true;
			query_options.use_stream_indices = true;

			GetHierarchyTypes(hierarchy_index, query_options);

			component_indices->size = stream_of_indices[0].size;
			shared_indices->size = stream_of_indices[1].size;
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

		void ReflectionManager::GetAllFromTypeTag(Stream<char> tag, CapacityStream<unsigned int>& type_indices) const
		{
			for (size_t index = 0; index < type_tags.size; index++) {
				if (function::CompareStrings(tag, type_tags[index].tag_name)) {
					for (size_t subindex = 0; subindex < type_tags[index].type_names.size; subindex++) {
						unsigned int type_index = type_definitions.Find(type_tags[index].type_names[subindex]);
						ECS_ASSERT(type_index != -1);
						type_indices.Add(type_index);
					}
					break;
				}
			}
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

#define TYPE_234_INT(base, basic_reflect, extended_reflect) TYPE_234_SIGNED_INT(base, basic_reflect); TYPE_234_UNSIGNED_INT(base, basic_reflect)

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

			COMPLEX_TYPE(enum, ReflectionBasicFieldType::Enum, ReflectionStreamFieldType::Basic, sizeof(ReflectionBasicFieldType), 1);
			COMPLEX_TYPE(pointer, ReflectionBasicFieldType::UserDefined, ReflectionStreamFieldType::Pointer, sizeof(void*), 1);
			
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
			void* allocation = Allocate(folders.allocator, ReflectionTypeTable::MemoryOf(count));
			type_definitions.InitializeFromBuffer(allocation, count);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeEnumTable(size_t count)
		{
			void* allocation = Allocate(folders.allocator, ReflectionEnumTable::MemoryOf(count));
			enum_definitions.InitializeFromBuffer(allocation, count);
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

			data.types.Initialize(folders.allocator, 0, max_types);
			data.enums.Initialize(folders.allocator, 0, max_enums);
			data.paths.Initialize(folders.allocator, path_count);
			data.constants.Initialize(folders.allocator, 0, max_constants);
			data.expressions.Initialize(folders.allocator, max_expressions);

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

			return thread_data.success;
		}

		bool ReflectionManager::ProcessFolderHierarchy(unsigned int index, CapacityStream<char>* error_message) {
			AllocatorPolymorphic allocator = folders.allocator;
			Stream<wchar_t> c_file_extensions[] = {
				L".c",
				L".cpp",
				L".h",
				L".hpp"
			};

			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, files, ECS_KB);
			bool status = GetDirectoryFileWithExtensionRecursive(folders[index].root, allocator, files, { c_file_extensions, std::size(c_file_extensions) });
			if (!status) {
				return false;
			}

			return ProcessFolderHierarchyImplementation(this, index, files, error_message);
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

			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, files, ECS_KB);
			bool status = GetDirectoryFileWithExtensionRecursive(folders[folder_index].root, allocator, files, { c_file_extensions, std::size(c_file_extensions) });
			if (!status) {
				return false;
			}

			// Preparsing the files in order to have thread act only on files that actually need to process
			// Otherwise unequal distribution of files will result in poor multithreaded performance
			unsigned int files_count = files.size;

			// Process these files on separate threads only if their number is greater than thread count
			if (files_count < thread_count) {
				return ProcessFolderHierarchyImplementation(this, folder_index, files, error_message);
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
				reflect_thread_data[thread_index].files = files;

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
			for (size_t index = 0; index < files.size; index++) {
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

		void ReflectionManager::SetInstanceDefaultData(unsigned int index, void* data) const
		{
			const ReflectionType* type = GetType(index);
			size_t type_size = GetReflectionTypeByteSize(type);

			// Can't memset the data directly to 0 since for streams it might mess them up
			// For basic types do the memset to 0 and for streams just set the size to 0

			for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
				void* current_field = function::OffsetPointer(data, type->fields[field_index].info.pointer_offset);

				if (type->fields[field_index].info.has_default_value) {
					memcpy(
						current_field,
						&type->fields[field_index].info.default_bool,
						type->fields[field_index].info.byte_size
					);
				}
				else {
					if (IsStream(type->fields[field_index].info.stream_type)) {
						// Just set the size to 0
						if (type->fields[field_index].info.stream_type == ReflectionStreamFieldType::Stream) {
							Stream<void>* stream = (Stream<void>*)current_field;
							stream->size = 0;
						}
						else {
							CapacityStream<void>* stream = (CapacityStream<void>*)current_field;
							stream->size = 0;
						}
					}
					else {
						// Pointers and basic arrays will get memset'ed to 0. Should be fine
						memset(current_field, 0, type->fields[field_index].info.byte_size);
					}
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
						file_contents[bytes_read - 1] = '\0';
						
						data->thread_memory.size += bytes_read;
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

#pragma region Skip macro definitions
							// Skip macro definitions - that define tags
							const char* verify_define_char = file_contents + word_offset - 1;
							verify_define_char = function::SkipWhitespace(verify_define_char, -1);
							verify_define_char = function::SkipCodeIdentifier(verify_define_char, -1);
							// If it is a pound, it must be a definition - skip it
							if (*verify_define_char == '#') {
								continue;
							}
#pragma endregion

#pragma region Skip Comments
							// Skip comments that contain ECS_REFLECT
							const char* verify_comment_char = file_contents + word_offset - 1;
							verify_comment_char = function::SkipWhitespace(verify_comment_char, -1);

							Stream<char> comment_space = { file_contents, function::PointerDifference(verify_comment_char, file_contents) + 1 };
							const char* verify_comment_last_new_line = function::FindCharacterReverse(comment_space, '\n').buffer;
							if (verify_comment_last_new_line == nullptr) {
								WriteErrorMessage(data, "Last new line could not be found when checking ECS_REFLECT for comment", index);
								return;
							}
							const char* comment_char = function::FindCharacterReverse(comment_space, '/').buffer;
							if (comment_char != nullptr && comment_char > verify_comment_last_new_line) {
								// It might be a comment
								if (comment_char[-1] == '/') {
									// It is a comment - skip it
									continue;
								}
								else if (comment_char[1] == '*') {
									// It is a comment skip it
									continue;
								}
							}
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
								// In order to get the corrent closing paranthese for the current struct
								// we need to count how many opened parantheses we have and when we reached an equal count
								// then we found the struct's paranthese
								unsigned int opened_paranthese_count = 1;
								unsigned int closed_paranthese_count = 1;
								char* mutable_closing_paranthese = (char*)closing_parenthese;
								*mutable_closing_paranthese = '\0';

								const char* in_between_opened_paranthese = strchr(opening_parenthese + 1, '{');
								while (in_between_opened_paranthese != nullptr) {
									opened_paranthese_count++;
									in_between_opened_paranthese = strchr(in_between_opened_paranthese + 1, '{');
								}

								*mutable_closing_paranthese = '}';
								const char* previous_closed_paranthese = closing_parenthese;
								while (opened_paranthese_count > closed_paranthese_count) {
									// Go to the next closing paranthese
									closing_parenthese = strchr(closing_parenthese + 1, '}');
									if (closing_parenthese == nullptr) {
										// This is an error, unmatched parantheses
										WriteErrorMessage(data, "Finding struct or class closing brace failed. Faulty path: ", index);
										return;
									}
									closed_paranthese_count++;

									mutable_closing_paranthese = (char*)closing_parenthese;
									*mutable_closing_paranthese = '\0';

									// Count again the number of in_between_parantheses
									in_between_opened_paranthese = strchr(previous_closed_paranthese + 1, '{');
									while (in_between_opened_paranthese != nullptr) {
										opened_paranthese_count++;
										in_between_opened_paranthese = strchr(in_between_opened_paranthese + 1, '{');
									}

									*mutable_closing_paranthese = '}';
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

								AddTypeDefinition(data, opening_parenthese, closing_parenthese, space, index);
								if (data->success == false) {
									return;
								}
								else if (tag_name != nullptr) {
									const char* end_tag_name = function::SkipCodeIdentifier(tag_name);

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

		void ReflectionManagerStylizeEnum(ReflectionEnum enum_) {
			ECS_STACK_CAPACITY_STREAM(unsigned int, underscores, 128);
			function::FindToken(enum_.fields[0], '_', underscores);

			for (int64_t index = underscores.size - 1; index >= 0; index--) {
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

			// If the values are in caps, keep capitalized only the first letter
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
				// All are with caps. Capitalize them
				for (index = 0; index < enum_.fields.size; index++) {
					for (size_t subindex = 1; subindex < enum_.fields[index].size; subindex++) {
						function::Uncapitalize(enum_.fields[index].buffer + subindex);
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
			enum_definition.fields = Stream<Stream<char>>((void*)ptr, 0);

			size_t memory_size = sizeof(Stream<char>) * next_line_positions.size + alignof(ReflectionEnum);
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

				enum_definition.fields.Add(current_character + 1);
				data->total_memory += PtrDifference(current_character + 1, final_character);
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
				while (function_body_start != nullptr) {
					// Get the first character before the bracket. If it is ), then we have a function declaration
					const char* first_character_before = function_body_start - 1;
					while (first_character_before >= start && (*first_character_before == ' ' || *first_character_before == '\t' || *first_character_before == '\n')) {
						first_character_before--;
					}

					if (*first_character_before == ')') {
						// Remove all the semicolons in between the two declarations
						unsigned int opened_bracket_count = 0;
						const char* opened_bracket = function_body_start;
						const char* closed_bracket = strchr(opened_bracket + 1, '}');

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

						// Now remove the semicolons in between
						unsigned int body_start_offset = function_body_start - start;
						unsigned int body_end_offset = closed_bracket - start;
						for (unsigned int index = 0; index < semicolon_positions.size; index++) {
							if (body_start_offset < semicolon_positions[index] && semicolon_positions[index] < body_end_offset) {
								semicolon_positions.Remove(index);
								index--;
							}
						}

						function_body_start = strchr(closed_bracket + 1, '{');
					}
					else {
						// Error. Unidentified meaning of {
						WriteErrorMessage(data, "Unrecognized meaning of { inside type definition.", file_index);
						return false;
					}
				}

				// assigning the field stream
				uintptr_t ptr = (uintptr_t)data->thread_memory.buffer + data->thread_memory.size;
				uintptr_t ptr_before = ptr;
				ptr = function::AlignPointer(ptr, alignof(ReflectionField));
				type.fields = Stream<ReflectionField>((void*)ptr, 0);
				data->thread_memory.size += sizeof(ReflectionField) * semicolon_positions.size + ptr - ptr_before;
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
								bool succeded = AddTypeField(data, type, pointer_offset, field_start, field_end);

								if (!succeded) {
									WriteErrorMessage(data, "An error occured during field type determination.", file_index);
									return false;
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
			if (type.fields.size == 0) {
				WriteErrorMessage(data, "There were no fields to reflect.", file_index);
				return;
			}

			data->types.Add(type);
			data->total_memory += sizeof(ReflectionType);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool AddTypeField(
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
				return false;
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
					// _ when writing the tag
					
					// A special case to handle is the ECS_SKIP_REFLECTION
					if (memcmp(parsed_tag_character, STRING(ECS_SKIP_REFLECTION), sizeof(STRING(ECS_SKIP_REFLECTION)) - 1) == 0) {
						// It is a skip reflection tag - get the byte size, add it to the type's and advance
						Stream<char> argument_range = { parsed_tag_character, function::PointerDifference(next_line_character, parsed_tag_character) + 1 };
						int64_t byte_size = function::ConvertCharactersToInt(argument_range);
						type.byte_size += byte_size;
						return true;
					}

					char* ending_tag_character = (char*)function::SkipCodeIdentifier(parsed_tag_character);
					char* skipped_ending_tag_character = (char*)function::SkipWhitespace(ending_tag_character);

					// If there are more tags separated by spaces, then transform the spaces or the tabs into
					// _ when writing the tag
					while (skipped_ending_tag_character != next_line_character) {
						while (ending_tag_character != skipped_ending_tag_character) {
							*ending_tag_character = '_';
							ending_tag_character++;
						}

						ending_tag_character = (char*)function::SkipCodeIdentifier(skipped_ending_tag_character);
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

			unsigned short embedded_array_size = 0;
			// take into consideration embedded arrays
			if (*current_character == ']') {
				const char* closing_bracket = current_character;
				while (*current_character != '[') {
					current_character--;
				}
				embedded_array_size = function::ConvertCharactersToInt(Stream<char>((void*)(current_character + 1), PtrDifference(current_character + 1, closing_bracket)));
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
							return false;
						}
						// Use the parse function, any member from the union can be used
						bool parse_success = ParseReflectionBasicFieldType(
							info.basic_type, 
							Stream<char>(start_default_value, ending_default_value - start_default_value),
							&info.default_bool
						);
						info.has_default_value = parse_success;
					};
					// If it is an opening brace, it's ok.
					if (*default_value_parse == '{') {
						parse_default_value(default_value_parse, '}');
					}
					else if (function::IsCodeIdentifierCharacter(*default_value_parse)) {
						// Check to see that it is the constructor type - else it is the actual value
						const char* start_parse_value = default_value_parse;
						while (function::IsCodeIdentifierCharacter(*default_value_parse)) {
							*default_value_parse++;
						}

						// If it is an open paranthese, it is a constructor
						if (*default_value_parse == '(') {
							// Look for the closing paranthese
							parse_default_value(default_value_parse, ')');
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
			}
			return success;
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

				*first_star = '\0';
				unsigned short before_pointer_offset = pointer_offset;
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					first_star - 1,
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
					return true;
				}

				char* right_bracket = (char*)strchr(left_bracket, '>');
				if (right_bracket == nullptr) {
					ECS_FORMAT_TEMP_STRING(temp_message, "Incorrect {#}, missing >.", stream_name);
					WriteErrorMessage(data, temp_message.buffer, -1);
					return true;
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
					unsigned int component_count = BasicTypeComponentCount(field.info.basic_type);
					pointer_offset = function::AlignPointer(pointer_offset, field.info.byte_size / component_count);
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
							// Assert false or assume alignment of 8
							ECS_ASSERT(false);
							alignment = alignof(void*);
						}
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

		size_t SearchReflectionUserDefinedTypeByteSize(const ReflectionManager* reflection_manager, Stream<char> definition)
		{
			return SearchReflectionUserDefinedTypeByteSizeAlignment(reflection_manager, definition).x;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 SearchReflectionUserDefinedTypeByteSizeAlignment(const ReflectionManager* reflection_manager, Stream<char> definition)
		{
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

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, ReflectionField field)
		{
			if (field.info.basic_type != ReflectionBasicFieldType::UserDefined) {
				if (field.info.stream_type == ReflectionStreamFieldType::Basic || field.info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					return GetFieldTypeAlignment(field.info.basic_type);
				}
				else {
					return GetFieldTypeAlignment(field.info.stream_type);
				}
			}
			else {
				ReflectionType nested_type;
				if (reflection_manager->TryGetType(field.definition, nested_type)) {
					return GetReflectionTypeAlignment(&nested_type);
				}
				else {
					// Assume its a container type - which should always have an alignment of alignof(void*)
					return alignof(void*);
				}
			}
			
			// Should not reach here
			return 0;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegral(ReflectionBasicFieldType type)
		{
			// Use the negation - if it is different than all the other types
			return type != ReflectionBasicFieldType::Bool && type != ReflectionBasicFieldType::Bool2 && type != ReflectionBasicFieldType::Bool3 &&
				type != ReflectionBasicFieldType::Bool4 && type != ReflectionBasicFieldType::Double && type != ReflectionBasicFieldType::Double2 &&
				type != ReflectionBasicFieldType::Double3 && type != ReflectionBasicFieldType::Double4 && type != ReflectionBasicFieldType::Float &&
				type != ReflectionBasicFieldType::Float2 && type != ReflectionBasicFieldType::Float3 && type != ReflectionBasicFieldType::Float4 &&
				type != ReflectionBasicFieldType::Enum && type != ReflectionBasicFieldType::UserDefined && type != ReflectionBasicFieldType::Unknown &&
				type != ReflectionBasicFieldType::Wchar_t;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegralSingleComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Int8 || type == ReflectionBasicFieldType::UInt8 ||
				type == ReflectionBasicFieldType::Int16 || type == ReflectionBasicFieldType::UInt16 ||
				type == ReflectionBasicFieldType::Int32 || type == ReflectionBasicFieldType::UInt32 ||
				type == ReflectionBasicFieldType::Int64 || type == ReflectionBasicFieldType::UInt64;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegralMultiComponent(ReflectionBasicFieldType type)
		{
			return IsIntegral(type) && !IsIntegralSingleComponent(type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned char BasicTypeComponentCount(ReflectionBasicFieldType type)
		{
			if (type == ReflectionBasicFieldType::Bool || type == ReflectionBasicFieldType::Double || type == ReflectionBasicFieldType::Enum ||
				type == ReflectionBasicFieldType::Float || type == ReflectionBasicFieldType::Int8 || type == ReflectionBasicFieldType::UInt8 ||
				type == ReflectionBasicFieldType::Int16 || type == ReflectionBasicFieldType::UInt16 || type == ReflectionBasicFieldType::Int32 ||
				type == ReflectionBasicFieldType::UInt32 || type == ReflectionBasicFieldType::Int64 || type == ReflectionBasicFieldType::UInt64 ||
				type == ReflectionBasicFieldType::Wchar_t) {
				return 1;
			}
			else if (type == ReflectionBasicFieldType::Bool2 || type == ReflectionBasicFieldType::Double2 || type == ReflectionBasicFieldType::Float2 ||
				type == ReflectionBasicFieldType::Char2 || type == ReflectionBasicFieldType::UChar2 || type == ReflectionBasicFieldType::Short2 ||
				type == ReflectionBasicFieldType::UShort2 || type == ReflectionBasicFieldType::Int2 || type == ReflectionBasicFieldType::UInt2 ||
				type == ReflectionBasicFieldType::Long2 || type == ReflectionBasicFieldType::ULong2) {
				return 2;
			}
			else if (type == ReflectionBasicFieldType::Bool3 || type == ReflectionBasicFieldType::Double3 || type == ReflectionBasicFieldType::Float3 ||
				type == ReflectionBasicFieldType::Char3 || type == ReflectionBasicFieldType::UChar3 || type == ReflectionBasicFieldType::Short3 ||
				type == ReflectionBasicFieldType::UShort3 || type == ReflectionBasicFieldType::Int3 || type == ReflectionBasicFieldType::UInt3 ||
				type == ReflectionBasicFieldType::Long3 || type == ReflectionBasicFieldType::ULong3) {
				return 3;
			}
			else if (type == ReflectionBasicFieldType::Bool4 || type == ReflectionBasicFieldType::Double4 || type == ReflectionBasicFieldType::Float4 ||
				type == ReflectionBasicFieldType::Char4 || type == ReflectionBasicFieldType::UChar4 || type == ReflectionBasicFieldType::Short4 ||
				type == ReflectionBasicFieldType::UShort4 || type == ReflectionBasicFieldType::Int4 || type == ReflectionBasicFieldType::UInt4 ||
				type == ReflectionBasicFieldType::Long4 || type == ReflectionBasicFieldType::ULong4) {
				return 4;
			}
			else {
				return 0;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetBasicTypeArrayElementSize(const ReflectionFieldInfo& info)
		{
			return info.byte_size / info.basic_type_count;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* GetReflectionFieldStreamBuffer(const ReflectionFieldInfo& info, const void* data)
		{
			const void* stream_field = function::OffsetPointer(data, info.pointer_offset);
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

		size_t GetReflectionFieldStreamSize(const ReflectionFieldInfo& info, const void* data)
		{
			const void* stream_field = function::OffsetPointer(data, info.pointer_offset);

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

		Stream<void> GetReflectionFieldStreamVoid(const ReflectionFieldInfo& info, const void* data)
		{
			const void* stream_field = function::OffsetPointer(data, info.pointer_offset);
			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				return *stream;
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				return { stream->buffer, stream->size };
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<char>* stream = (ResizableStream<char>*)stream_field;
				return { stream->buffer, stream->size };
			}
			return { nullptr, 0 };
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

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
