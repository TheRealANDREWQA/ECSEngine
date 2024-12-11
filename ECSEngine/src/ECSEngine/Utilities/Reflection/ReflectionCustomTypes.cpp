#include "ecspch.h"
#include "ReflectionCustomTypes.h"
#include "ReflectionStringFunctions.h"
#include "Reflection.h"
#include "../../Resources/AssetMetadataSerialize.h"
#include "../ReferenceCountSerialize.h"
#include "../../Containers/SparseSet.h"

#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Allocators/StackAllocator.h"
#include "../../Allocators/MultipoolAllocator.h"
#include "../../Allocators/MemoryProtectedAllocator.h"

namespace ECSEngine {

	namespace Reflection {

		// ----------------------------------------------------------------------------------------------------------------------------

		// Must be kept in sync with the ECS_SERIALIZE_CUSTOM_TYPES
		ReflectionCustomTypeInterface* ECS_REFLECTION_CUSTOM_TYPES[] = {
			new StreamCustomTypeInterface(),
			new ReferenceCountedCustomTypeInterface(),
			new SparseSetCustomTypeInterface(),
			new MaterialAssetCustomTypeInterface(),
			new DataPointerCustomTypeInterface(),
			new AllocatorCustomTypeInterface(),
			new HashTableCustomTypeInterface()
		};

		static_assert(ECS_COUNTOF(ECS_REFLECTION_CUSTOM_TYPES) == ECS_REFLECTION_CUSTOM_TYPE_COUNT, "ECS_REFLECTION_CUSTOM_TYPES is not in sync");

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeAddDependentType(ReflectionCustomTypeDependentTypesData* data, Stream<char> definition) {
			// Check to see if it is a trivial type or not - do not report trivial types as dependent types
			ReflectionBasicFieldType basic_field_type;
			ReflectionStreamFieldType stream_field_type;
			ConvertStringToPrimitiveType(definition, basic_field_type, stream_field_type);
			if (basic_field_type == ReflectionBasicFieldType::Unknown || basic_field_type == ReflectionBasicFieldType::UserDefined
				|| stream_field_type == ReflectionStreamFieldType::Unknown) {
				// One more exception here, void, treat it as a special case
				if (definition != "void") {
					data->dependent_types.AddAssert(definition);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data)
		{
			Stream<char> opened_bracket = FindFirstCharacter(data->definition, '<');
			ECS_ASSERT(opened_bracket.buffer != nullptr);

			Stream<char> closed_bracket = FindCharacterReverse(opened_bracket, '>');
			ECS_ASSERT(closed_bracket.buffer != nullptr);

			Stream<char> type_name = { opened_bracket.buffer + 1, PointerDifference(closed_bracket.buffer, opened_bracket.buffer) - 1 };
			ReflectionCustomTypeAddDependentType(data, type_name);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeDependentTypes_MultiTemplate(ReflectionCustomTypeDependentTypesData* data) {
			ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 16);
			ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);
			for (unsigned int index = 0; index < template_arguments.size; index++) {
				ReflectionCustomTypeAddDependentType(data, template_arguments[index]);
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

			Stream<char> type = { opened_bracket.buffer + 1, PointerDifference(closed_bracket.buffer, opened_bracket.buffer) / sizeof(char) - 1 };
			return type;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeGetTemplateArguments(Stream<char> definition, CapacityStream<Stream<char>>& arguments) {
			Stream<char> opened_bracket = FindFirstCharacter(definition, '<');
			Stream<char> closed_bracket = FindCharacterReverse(definition, '>');
			ECS_ASSERT(opened_bracket.buffer != nullptr && closed_bracket.buffer != nullptr);

			const char* parse_character = opened_bracket.buffer + 1;
			
			// Retrieves a single template parameter, starting from the location of parse character
			auto parse_template_type = [closed_bracket, parse_character]() {
				// Go through the characters in between the brackets and keep track of how many templates we encountered.
				// Stop when we get to a comma and the template counter is 0, or we got to the closed bracket
				size_t template_count = 0;
				Stream<char> type = { parse_character, PointerElementDifference(parse_character, closed_bracket.buffer) };

				const char* iterate_character = parse_character;
				while (iterate_character < closed_bracket.buffer) {
					if (*iterate_character == '<') {
						template_count++;
					}
					else if (*iterate_character == '>') {
						template_count--;
					}
					else if (*iterate_character == ',' && template_count == 0) {
						type.size = iterate_character - type.buffer;
						break;
					}

					iterate_character++;
				}

				type = SkipWhitespace(type);
				type = SkipWhitespace(type, -1);
				return type;
			};

			while (parse_character < closed_bracket.buffer) {
				arguments.AddAssert(parse_template_type());
			}
		}

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

		ReflectionCustomTypeInterface* GetReflectionCustomType(Stream<char> definition) {
			unsigned int index = FindReflectionCustomType(definition);
			return GetReflectionCustomType(index);
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
			ReflectionDefinitionInfo element_info = SearchReflectionDefinitionInfo(data->reflection_manager, template_type);

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

			size_t allocation_size = element_info.byte_size * source_size;
			void* allocation = nullptr;
			if (allocation_size > 0) {
				allocation = Allocate(data->allocator, allocation_size);
				memcpy(allocation, source, allocation_size);
			}

			if (data->deallocate_existing_data) {
				size_t destination_size = GetIntValueUnsigned(OffsetPointer(data->destination, sizeof(void*)), size_type);
				if (*destination != nullptr) {
					if (destination_size > 0) {
						DeallocateReflectionInstanceBuffers(data->reflection_manager, template_type, *destination, data->allocator, destination_size, element_info.byte_size, false);
					}
					ECSEngine::DeallocateEx(data->allocator, *destination);
				}
			}

			*destination = allocation;
			SetIntValueUnsigned(OffsetPointer(data->destination, sizeof(void*)), size_type, source_size);

			if (!element_info.is_blittable) {
				size_t count = source_size;
				ReflectionType nested_type;
				CopyReflectionDataOptions copy_options;
				copy_options.allocator = data->allocator;
				copy_options.always_allocate_for_buffers = true;
				copy_options.deallocate_existing_buffers = false;

				if (data->reflection_manager->TryGetType(template_type, nested_type)) {
					for (size_t index = 0; index < count; index++) {
						CopyReflectionTypeInstance(
							data->reflection_manager,
							&nested_type,
							OffsetPointer(source, index * element_info.byte_size),
							OffsetPointer(allocation, element_info.byte_size * index),
							&copy_options
						);
					}
				}
				else {
					unsigned int custom_type_index = FindReflectionCustomType(template_type);
					ECS_ASSERT(custom_type_index != -1);
					data->definition = template_type;

					for (size_t index = 0; index < count; index++) {
						data->source = OffsetPointer(source, index * element_info.byte_size);
						data->destination = OffsetPointer(allocation, element_info.byte_size * index);
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
			// This is a special case. The given source and the given element count
			// Reflect the actual Stream<void> data for this instance.
			// So, we must not iterate
			Stream<void> stream_data = { data->source, data->element_count };

			if (stream_data.size > 0) {
				// Deallocate firstly the elements, if they require deallocation
				Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);

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
				// TODO: Deallocate elements individually
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

#pragma region Allocator

		// ----------------------------------------------------------------------------------------------------------------------------

		bool AllocatorCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			return AllocatorTypeFromString(data->definition) != ECS_ALLOCATOR_TYPE_COUNT;
		}

		ulong2 AllocatorCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			ECS_ALLOCATOR_TYPE type = AllocatorTypeFromString(data->definition);

#define MACRO(allocator) sizeof(allocator),

			// We only need the byte size, the alignment is going to be the maximum for all types
			static size_t byte_sizes[] = {
				ECS_EXPAND_ALLOCATOR_MACRO(MACRO)
			};

#undef MACRO

			static_assert(ECS_COUNTOF(byte_sizes) == ECS_ALLOCATOR_TYPE_COUNT);
			return { byte_sizes[type], alignof(void*) };
		}

		// No dependent types
		void AllocatorCustomTypeInterface::GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) {}

		bool AllocatorCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			// These types are not blittable, only a few fields are needed from them
			return false;
		}

		void AllocatorCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
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

		bool AllocatorCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
			const DataPointer* first = (const DataPointer*)data->first;
			const DataPointer* second = (const DataPointer*)data->second;

			unsigned short first_size = first->GetData();
			unsigned short second_size = second->GetData();
			if (first_size == second_size) {
				return memcmp(first->GetPointer(), second->GetPointer(), first_size) == 0;
			}
			return false;
		}

		void AllocatorCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
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

#pragma region Hash table

		// ----------------------------------------------------------------------------------------------------------------------------

		// Describes the type of hash table definition
		enum HASH_TABLE_TYPE : unsigned char {
			HASH_TABLE_NORMAL,
			HASH_TABLE_DEFAULT,
			HASH_TABLE_EMPTY
		};

		static Stream<char> HashTableExtractValueType(Stream<char> definition) {
			Stream<char> opened_bracket = FindFirstCharacter(definition, '<');
			Stream<char> closed_bracket = FindMatchingParenthesis(opened_bracket.AdvanceReturn(), '<', '>');

			// Go through the characters in between the brackets and keep track of how many templates we encountered.
			// Stop when we get to a comma and the template counter is 0, or we got to the closed bracket
			const char* iterate_character = opened_bracket.buffer + 1;
			size_t template_count = 0;
			Stream<char> value_type = opened_bracket.AdvanceReturn().StartDifference(closed_bracket);
			while (iterate_character < closed_bracket.buffer) {
				if (*iterate_character == '<') {
					template_count++;
				}
				else if (*iterate_character == '>') {
					template_count--;
				}
				else if (*iterate_character == ',' && template_count == 0) {
					value_type.size = iterate_character - value_type.buffer;
					break;
				}
				
				iterate_character++;
			}

			value_type = SkipWhitespace(value_type);
			value_type = SkipWhitespace(value_type, -1);
			return value_type;
		}

		struct HashTableTemplateArguments {
			Stream<char> value_type;
			Stream<char> identifier_type;
			Stream<char> hash_function_type;
			bool is_soa = false;
			HASH_TABLE_TYPE table_type;
		};

		static HashTableTemplateArguments HashTableExtractTemplateArguments(Stream<char> definition) {
			ECS_STACK_CAPACITY_STREAM(Stream<char>, string_template_arguments, 8);
			ReflectionCustomTypeGetTemplateArguments(definition, string_template_arguments);

			HashTableTemplateArguments template_arguments;

			// Each hash table type must be handled separately
			if (definition.StartsWith("HashTable<")) {
				template_arguments.table_type = HASH_TABLE_NORMAL;

				// There have to be at least 3 template arguments
				template_arguments.value_type = string_template_arguments[0];
				template_arguments.identifier_type = string_template_arguments[1];
				template_arguments.hash_function_type = string_template_arguments[2];
				
				// We don't care about the object hasher, it doesn't internally occupy space.
				// Check the soa parameter tho, it is optional
				if (string_template_arguments.size == 5) {
					// It has the SoA parameter
					template_arguments.is_soa = ConvertCharactersToBool(definition);
				}
			}
			else if (definition.StartsWith("HashTableDefault<")) {
				template_arguments.table_type = HASH_TABLE_DEFAULT;

				// Only the value type is needed, the other entries are known
				template_arguments.value_type = string_template_arguments[0];
				template_arguments.identifier_type = STRING(ResourceIdentifier);
				template_arguments.hash_function_type = STRING(HashFunctionPowerOfTwo);
			}
			else if (definition.StartsWith("HashTableEmpty<")) {
				template_arguments.table_type = HASH_TABLE_EMPTY;

				// 2 mandatory arguments, and one optional that is the object hasher
				template_arguments.value_type = STRING(HashTableEmpty);
				template_arguments.identifier_type = string_template_arguments[0];
				template_arguments.hash_function_type = string_template_arguments[1];
				template_arguments.is_soa = false;
			}

			return template_arguments;
		}

		bool HashTableCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			if (data->definition.size < sizeof("HashTable<")) {
				return false;
			}

			if (data->definition[data->definition.size - 1] != '>') {
				return false;
			}

			if (data->definition.StartsWith("HashTable<") || data->definition.StartsWith("HashTableDefault<") || data->definition.StartsWith("HashTableEmpty")) {
				return true;
			}

			return false;
		}

		ulong2 HashTableCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			// All hash tables have the same byte size and alignment. Technically, the size can depend on the hasher
			// Being used, but the ones we provide are at max 8 bytes long, which is the limit of the alignment
			HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);
			
			static Stream<char> known_hash_functions[] = {
				STRING(HashFunctionPowerOfTwo),
				STRING(HashFunctionPrimeNumber), 
				STRING(HashFunctionFibonacci),
				STRING(HashFunctionFolding),
				STRING(HashFunctionXORFibonacci)
			};

			// Assert that the hash function is known, otherwise we don't know how much space it takes
			ECS_ASSERT(FindString(template_arguments.hash_function_type, { known_hash_functions, ECS_COUNTOF(known_hash_functions) }) != -1, "The hash table reflection can't cope with custom hash functions.");
			return { sizeof(HashTableDefault<char>), alignof(HashTableDefault<char>) };
		}

		void HashTableCustomTypeInterface::GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) {
			// Can't use ReflectionCustomTypeDependentTypes_MultiTemplate because we need to handle the special
			// Case of ResourceIdentifier being used
			ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 16);
			ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);
			for (unsigned int index = 0; index < template_arguments.size; index++) {
				if (template_arguments[index] != STRING(ResourceIdentifier)) {
					ReflectionCustomTypeAddDependentType(data, template_arguments[index]);
				}
			}
		}

		bool HashTableCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			// Never blittable
			return false;
		}

		void HashTableCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			HashTableTemplateArguments template_parameters = HashTableExtractTemplateArguments(data->definition);

			// We need to have separate branches for SoA
			ReflectionDefinitionInfo value_info;
			if (template_parameters.table_type == HASH_TABLE_EMPTY) {
				value_info.alignment = 0;
				value_info.byte_size = 0;
				value_info.is_blittable = true;
			}
			else {
				value_info = SearchReflectionDefinitionInfo(data->reflection_manager, data->definition);
			}
			
			// Use a special case for handling the ResourceIdentifier
			ReflectionDefinitionInfo identifier_info;
			if (template_parameters.table_type == HASH_TABLE_DEFAULT || template_parameters.identifier_type == STRING(ResourceIdentifier)) {
				identifier_info.byte_size = sizeof(ResourceIdentifier);
				identifier_info.alignment = alignof(ResourceIdentifier);
				identifier_info.is_blittable = false;
				// Change the template parameter string type into Stream<void> as well, in order to have generic handling for this parameter later on
				template_parameters.identifier_type = STRING(Stream<void>);
			}
			else {
				identifier_info = SearchReflectionDefinitionInfo(data->reflection_manager, data->definition);
			}

			// Cast the two table sources to a random table specialization, to have easy access to the fields
			HashTableDefault<char>* destination = (HashTableDefault<char>*)data->destination;
			const HashTableDefault<char>* source = (const HashTableDefault<char>*)data->source;

			if (data->deallocate_existing_data) {
				if (destination->GetCapacity() > 0) {
					ECSEngine::DeallocateEx(data->allocator, (void*)destination->GetAllocatedBuffer());
				}
			}

			unsigned int source_extended_capacity = source->GetExtendedCapacity();
			if (template_parameters.is_soa) {
				// Compute the total byte size for the destination. We must use the extended capacity
				size_t total_allocation_size = (value_info.byte_size + identifier_info.byte_size + sizeof(unsigned char)) * source_extended_capacity;
				void* allocation = AllocateEx(data->allocator, total_allocation_size);
				
				// The metadata buffer is set last
				ECS_ASSERT(source->m_metadata > (void*)source->m_identifiers && source->m_identifiers > source->m_buffer, "Hash table reflection copy must be changed!");
				destination->m_buffer = (char*)allocation;
				destination->m_identifiers = (ResourceIdentifier*)OffsetPointer(destination->m_buffer, value_info.byte_size * source_extended_capacity);
				destination->m_metadata = (unsigned char*)OffsetPointer(destination->m_identifiers, identifier_info.byte_size * source_extended_capacity);
			}
			else {

			}

			destination->m_capacity = source->m_capacity;
			destination->m_count = source->m_count;
			// Copy the hasher as well, it has to be at max a void*
			memcpy(&destination->m_function, &source->m_function, sizeof(void*));

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

			/*const void* source = *(const void**)data->source;
			void** destination = (void**)data->destination;
			size_t source_size = GetIntValueUnsigned(OffsetPointer(data->source, sizeof(void*)), size_type);

			size_t allocation_size = element_size * source_size;
			void* allocation = nullptr;
			if (allocation_size > 0) {
				allocation = Allocate(data->allocator, allocation_size);
				memcpy(allocation, source, allocation_size);
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
			}*/
		}

		bool HashTableCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
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

		void HashTableCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
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

	}

}