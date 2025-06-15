#include "ecspch.h"
#include "ReflectionCustomTypes.h"
#include "ReflectionStringFunctions.h"
#include "Reflection.h"
#include "../../Resources/AssetMetadataSerialize.h"
#include "../ReferenceCountSerialize.h"
#include "../../Containers/SparseSet.h"
#include "../../Containers/Deck.h"

#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Allocators/StackAllocator.h"
#include "../../Allocators/MultipoolAllocator.h"
#include "../../Allocators/MemoryProtectedAllocator.h"
#include "../../Allocators/MallocAllocator.h"

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
			new HashTableCustomTypeInterface(),
			new DeckCustomTypeInterface()
		};

		static_assert(ECS_COUNTOF(ECS_REFLECTION_CUSTOM_TYPES) == ECS_REFLECTION_CUSTOM_TYPE_COUNT, "ECS_REFLECTION_CUSTOM_TYPES is not in sync");

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeAddDependentType(ReflectionCustomTypeDependenciesData* data, Stream<char> definition) {
			// If the definition starts with a const, discard it
			if (definition.StartsWith("const ")) {
				definition.Advance(strlen("const "));
			}

			// Check to see if it is a trivial type or not - do not report trivial types as dependent types
			ReflectionBasicFieldType basic_field_type;
			ReflectionStreamFieldType stream_field_type;
			ConvertStringToPrimitiveType(definition, basic_field_type, stream_field_type);
			if (basic_field_type == ReflectionBasicFieldType::Unknown || basic_field_type == ReflectionBasicFieldType::UserDefined
				|| stream_field_type == ReflectionStreamFieldType::Unknown) {
				if (stream_field_type == ReflectionStreamFieldType::Pointer) {
					// If this is a Pointer, then try again for the target
					Stream<char> pointee_target = definition;
					while (pointee_target.size > 0 && pointee_target.Last() == '*') {
						pointee_target.size--;
					}
					ReflectionCustomTypeAddDependentType(data, pointee_target);
				}
				// One more exception here, void, treat it as a special case
				else if (definition != "void") {
					data->dependencies.AddAssert(definition);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependenciesData* data)
		{
			Stream<char> opened_bracket = FindFirstCharacter(data->definition, '<');
			ECS_ASSERT(opened_bracket.buffer != nullptr);

			Stream<char> closed_bracket = FindCharacterReverse(opened_bracket, '>');
			ECS_ASSERT(closed_bracket.buffer != nullptr);

			Stream<char> type_name = { opened_bracket.buffer + 1, PointerDifference(closed_bracket.buffer, opened_bracket.buffer) - 1 };
			ReflectionCustomTypeAddDependentType(data, type_name);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionCustomTypeDependentTypes_MultiTemplate(ReflectionCustomTypeDependenciesData* data) {
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
			
			Stream<char> template_range = opened_bracket.AdvanceReturn().StartDifference(closed_bracket);
			unsigned int initial_argument_size = arguments.size;
			SplitStringWithParameterList(template_range, ',', '<', '>', &arguments);

			// Trim whitespaces from the arguments
			for (unsigned int index = initial_argument_size; index < arguments.size; index++) {
				arguments[index] = TrimWhitespace(arguments[index]);
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

		void StreamCustomTypeInterface::GetDependencies(ReflectionCustomTypeDependenciesData* data) {
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

			// If the option to overwrite resizable allocators is enabled, do that now
			if (data->options.overwrite_resizable_allocators) {
				if (data->definition.StartsWith("ResizableStream")) {
					ResizableStream<void>* stream = (ResizableStream<void>*)data->destination;
					stream->allocator = data->allocator;
				}
			}

			const void* source = *(const void**)data->source;
			void** destination = (void**)data->destination;
			size_t source_size = GetIntValueUnsigned(OffsetPointer(data->source, sizeof(void*)), size_type);

			size_t allocation_size = element_info.byte_size * source_size;
			void* allocation = nullptr;
			if (allocation_size > 0) {
				allocation = Allocate(data->allocator, allocation_size);
			}

			if (data->options.deallocate_existing_data) {
				size_t destination_size = GetIntValueUnsigned(OffsetPointer(data->destination, sizeof(void*)), size_type);
				if (*destination != nullptr) {
					for (size_t index = 0; index < destination_size; index++) {
						DeallocateReflectionInstanceBuffers(data->reflection_manager, template_type, element_info, OffsetPointer(*destination, index * element_info.byte_size), data->allocator, false);
					}
					ECSEngine::Deallocate(data->allocator, *destination);
				}
			}

			*destination = allocation;
			SetIntValueUnsigned(OffsetPointer(data->destination, sizeof(void*)), size_type, source_size);
			if (size_type == ECS_INT32) {
				// It is a capacity or resizable stream, set its capacity to this value as well
				SetIntValueUnsigned(OffsetPointer(data->destination, sizeof(void*) + GetIntTypeByteSize(size_type)), size_type, source_size);
			}

			CopyReflectionDataOptions copy_options = data;
			CopyReflectionTypeInstance(data->reflection_manager, template_type, element_info, source, allocation, source_size, &copy_options, data->tags);
		}

		bool StreamCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
			Stream<void> first;
			Stream<void> second;

			if (data->definition.StartsWith("Stream<")) {
				first = *(Stream<void>*)data->first;
				second = *(Stream<void>*)data->second;
			}
			// The resizable stream can be type punned with the capacity stream
			else if (data->definition.StartsWith("CapacityStream<") || data->definition.StartsWith("ResizableStream<")) {
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
					stream_data.size
				);
			}

			// The main function will call the deallocate for this type
			// Of streams as well. We cannot memset the correct pointer,
			// So leave it to that function to perform the deallocation
			// And the reset

			//if (stream_data.buffer != nullptr) {
			//	Deallocate(data->allocator, stream_data.buffer);
			//}
			//// For the resizable stream, we need to memset only the first 3 fields
			//static_assert(sizeof(Stream<void>) == sizeof(CapacityStream<void>));
			//if (data->reset_buffers) {
			//	memset(data->source, 0, sizeof(Stream<void>));
			//}
		}

		size_t StreamCustomTypeInterface::GetElementCount(ReflectionCustomTypeGetElementCountData* data) {
			// We don't care about the element type name, we have a single type
			if (data->definition.StartsWith(STRING(Stream))) {
				const Stream<void>* stream = (const Stream<void>*)data->source;
				return stream->size;
			}
			else {
				// Can type pun the capacity and resizable streams
				const CapacityStream<void>* stream = (const CapacityStream<void>*)data->source;
				return stream->size;
			}
		}

		// For the stream type it doesn't matter if it is a token or not

		void* StreamCustomTypeInterface::GetElement(ReflectionCustomTypeGetElementData* data) {
			if (!data->has_cache) {
				data->has_cache = true;
				data->element_byte_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, ReflectionCustomTypeGetTemplateArgument(data->definition));
			}

			void* buffer = *(void**)data->source;
			return OffsetPointer(buffer, data->element_byte_size * data->index_or_token);
		}

		size_t StreamCustomTypeInterface::FindElement(ReflectionCustomTypeFindElementData* data) {
			if (!data->has_cache) {
				data->has_cache = true;
				data->element_byte_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, ReflectionCustomTypeGetTemplateArgument(data->definition));
			}

			Stream<void> stream;
			if (data->definition.StartsWith(STRING(Stream))) {
				stream = *(Stream<void>*)data->source;
			}
			else {
				// Capacity or resizable, can alias them
				stream = *(CapacityStream<void>*)data->source;
			}

			return SearchBytesEx(stream.buffer, stream.size, data->element, data->element_byte_size);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region SparseSet

		// This function takes into account whether or not the elements are blittable. If they are not blittable, it makes sure they are deallocated individually
		static void SparseSetDeallocateElements(
			const ReflectionManager* reflection_manager,
			void* sparse_set, 
			AllocatorPolymorphic set_allocator, 
			AllocatorPolymorphic element_allocator,
			Stream<char> template_type, 
			const ReflectionDefinitionInfo& element_definition_info
		) {
			if (!element_definition_info.is_blittable) {
				struct DeallocateData {
					const ReflectionManager* reflection_manager;
					AllocatorPolymorphic allocator;
					const ReflectionDefinitionInfo* definition_info;
					Stream<char> template_type;
				};

				DeallocateData deallocate_data = { reflection_manager, element_allocator, &element_definition_info, template_type };
				SparseSetDeallocateUntypedElements(sparse_set, element_definition_info.byte_size, [](void* element, void* extra_data) {
					DeallocateData* data = (DeallocateData*)extra_data;
					DeallocateReflectionInstanceBuffers(data->reflection_manager, data->template_type, *data->definition_info, element, data->allocator);
				}, &deallocate_data);
			}

			SparseSetDeallocateUntyped(sparse_set, set_allocator);
		}

		bool SparseSetCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			return ReflectionCustomTypeMatchTemplate(data, "SparseSet<") || ReflectionCustomTypeMatchTemplate(data, "ResizableSparseSet<");
		}

		ulong2 SparseSetCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			if (data->definition.StartsWith("SparseSet<")) {
				return { sizeof(SparseSet<char>), alignof(SparseSet<char>) };
			}
			else {
				return { sizeof(ResizableSparseSet<char>), alignof(ResizableSparseSet<char>) };
			}
		}

		void SparseSetCustomTypeInterface::GetDependencies(ReflectionCustomTypeDependenciesData* data) {
			ReflectionCustomTypeDependentTypes_SingleTemplate(data);
		}

		bool SparseSetCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			return false;
		}

		void SparseSetCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			ReflectionDefinitionInfo definition_info = SearchReflectionDefinitionInfo(data->reflection_manager, template_type);

			AllocatorPolymorphic set_allocator = data->allocator;
			if (data->definition.StartsWith("Resizable")) {
				ResizableSparseSet<char>* sparse_set = (ResizableSparseSet<char>*)data->destination;
				// Check for the override option
				if (data->options.overwrite_resizable_allocators) {
					sparse_set->allocator = data->allocator;
				}
				set_allocator = sparse_set->allocator;
			}

			if (data->options.deallocate_existing_data) {
				SparseSetDeallocateElements(
					data->reflection_manager,
					data->destination,
					set_allocator,
					data->allocator,
					template_type,
					definition_info
				);
			}

			if (definition_info.is_blittable) {
				// Can blit the data type
				SparseSetCopyTypeErased(data->source, data->destination, definition_info.byte_size, set_allocator);
			}
			else {
				data->definition = template_type;
				struct CopyData {
					const CopyReflectionDataOptions* copy_type_options;
					const ReflectionCustomTypeCopyData* custom_data;
					const ReflectionDefinitionInfo* definition_info;
					Stream<char> template_type;
				};

				// Need to copy each and every element
				auto copy_function = [](const void* source, void* destination, void* extra_data) {
					CopyData* data = (CopyData*)extra_data;
					CopyReflectionTypeInstance(data->custom_data->reflection_manager, data->template_type, *data->definition_info, source, destination, data->copy_type_options);
				};

				CopyReflectionDataOptions copy_type_options = data;
				
				CopyData copy_data;
				copy_data.custom_data = data;
				copy_data.copy_type_options = &copy_type_options;
				copy_data.template_type = template_type;
				copy_data.definition_info = &definition_info;

				SparseSetCopyTypeErasedFunction(data->source, data->destination, definition_info.byte_size, set_allocator, copy_function, &copy_data);
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
			Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
			ReflectionDefinitionInfo element_definition_info = SearchReflectionDefinitionInfo(data->reflection_manager, template_type);


			// It is fine to type pun to any type, since the basic layout is the same
			if (data->definition.StartsWith("SparseSet<")) {
				for (size_t index = 0; index < data->element_count; index++) {
					void* current_source = OffsetPointer(data->source, index * sizeof(SparseSet<char>));
					SparseSetDeallocateElements(data->reflection_manager, current_source, data->allocator, data->allocator, template_type, element_definition_info);
				}
			}
			else if (data->definition.StartsWith("ResizableSparseSet<")) {
				for (size_t index = 0; index < data->element_count; index++) {
					void* current_source = OffsetPointer(data->source, index * sizeof(ResizableSparseSet<char>));
					ResizableSparseSet<char>* set = (ResizableSparseSet<char>*)current_source;
					// We can use the same deallocate elements function, since the first field of the resizable is a normal set,
					// That is the actual field that needs to change, the allocator doesn't. Use the resizable allocator to deallocate the elements
					// TODO: At the moment, we are always using the sparse set allocator to allocate
					SparseSetDeallocateElements(data->reflection_manager, current_source, set->allocator, data->allocator, template_type, element_definition_info);
				}
			}
			else {
				ECS_ASSERT(false);
			}
		}

		size_t SparseSetCustomTypeInterface::GetElementCount(ReflectionCustomTypeGetElementCountData* data) {
			// We don't care about the element type name, we have a single type
			// We can type pun both the normal sparse set and the resizable one
			const SparseSet<char>* set = (const SparseSet<char>*)data->source;
			return set->size;
		}

		// For the sparse set, it doesn't matter if it is a token or not

		void* SparseSetCustomTypeInterface::GetElement(ReflectionCustomTypeGetElementData* data) {
			if (!data->has_cache) {
				data->has_cache = true;
				data->element_byte_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, ReflectionCustomTypeGetTemplateArgument(data->definition));
			}

			void* buffer = *(void**)data->source;
			return OffsetPointer(buffer, data->element_byte_size * data->index_or_token);
		}

		size_t SparseSetCustomTypeInterface::FindElement(ReflectionCustomTypeFindElementData* data) {
			if (!data->has_cache) {
				data->has_cache = true;
				data->element_byte_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, ReflectionCustomTypeGetTemplateArgument(data->definition));
			}

			// Can type pun both resizable and non resizable streams
			const SparseSet<char>* sparse_set = (const SparseSet<char>*)data->source;
			return SearchBytesEx(sparse_set->buffer, sparse_set->size, data->element, data->element_byte_size);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region DataPointer

		// TODO: At the moment, we can't detect types that are copyable/deallocatable themselves
		// With the current data pointer definition. We require a tag to specify the pointee. Add it when needed

		bool DataPointerCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			return STRING(DataPointer) == data->definition;
		}

		ulong2 DataPointerCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			return { sizeof(DataPointer), alignof(DataPointer) };
		}

		void DataPointerCustomTypeInterface::GetDependencies(ReflectionCustomTypeDependenciesData* data) {}

		bool DataPointerCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			return false;
		}

		void DataPointerCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			const DataPointer* source = (const DataPointer*)data->source;
			DataPointer* destination = (DataPointer*)data->destination;

			if (data->options.deallocate_existing_data) {
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
				void* current_source = OffsetPointer(data->source, index * sizeof(DataPointer));
				DataPointer* data_pointer = (DataPointer*)current_source;
				void* pointer = data_pointer->GetPointer();
				if (pointer != nullptr) {
					ECSEngine::Deallocate(data->allocator, pointer);
				}

				// Make this pointer empty
				data_pointer = nullptr;
			}
		}

		// The data pointer doesn't have to implement the Element family of functions

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Allocator

		// ----------------------------------------------------------------------------------------------------------------------------

		// PERFORMANCE TODO: All allocators inherit from AllocatorBase, and that structure contains already a ECS_ALLOCATOR_TYPE
		// Inside it. Should we use it to make the determination of the allocator type faster (although that doesn't help with AllocatorPolymorphic)?

		// Returns the allocator type, taking into account allocator polymorphic. It can return ECS_ALLOCATOR_TYPE_COUNT
		// If the allocator type is not identified
		static ECS_ALLOCATOR_TYPE AllocatorTypeFromStringWithPolymorphic(const void* allocator, Stream<char> definition) {
			if (definition == STRING(AllocatorPolymorphic)) {
				const AllocatorPolymorphic* allocator_polymorphic = (const AllocatorPolymorphic*)allocator;
				return allocator_polymorphic->allocator->m_allocator_type;
			}
			else {
				return AllocatorTypeFromString(definition);
			}
		}

		bool AllocatorCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			return AllocatorTypeFromString(data->definition) != ECS_ALLOCATOR_TYPE_COUNT || data->definition == STRING(AllocatorPolymorphic);
		}

		ulong2 AllocatorCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			ECS_ALLOCATOR_TYPE type = AllocatorTypeFromString(data->definition);

#define MACRO(allocator) sizeof(allocator),

			// We only need the byte size, the alignment is going to be the maximum for all types
			// For the interface type, we can't handle it
			static size_t byte_sizes[] = {
				ECS_EXPAND_ALLOCATOR_MACRO(MACRO)
				0,
				sizeof(AllocatorPolymorphic)
			};

#undef MACRO

			static_assert(ECS_COUNTOF(byte_sizes) == ECS_ALLOCATOR_TYPE_COUNT + 1);
			ECS_ASSERT(type != ECS_ALLOCATOR_INTERFACE);
			return { byte_sizes[type], alignof(void*) };
		}

		// No dependent types
		void AllocatorCustomTypeInterface::GetDependencies(ReflectionCustomTypeDependenciesData* data) {}

		bool AllocatorCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			// These types are not blittable, only a few fields are needed from them
			return false;
		}

		void AllocatorCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			// If the initialize type allocators option is enabled, resize this allocator according to the source
			auto initialize_implementation = [data](ECS_ALLOCATOR_TYPE allocator_type, void* destination_untyped, const void* source_untyped) {
				switch (allocator_type) {
				case ECS_ALLOCATOR_LINEAR:
				{
					LinearAllocator* destination = (LinearAllocator*)destination_untyped;
					const LinearAllocator* source = (const LinearAllocator*)source_untyped;
					new (destination) LinearAllocator(Allocate(data->allocator, source->m_capacity), source->m_capacity);
				}
				break;
				case ECS_ALLOCATOR_STACK:
				{
					StackAllocator* destination = (StackAllocator*)destination_untyped;
					const StackAllocator* source = (const StackAllocator*)source_untyped;
					new (destination) StackAllocator(Allocate(data->allocator, source->m_capacity), source->m_capacity);
				}
				break;
				case ECS_ALLOCATOR_MULTIPOOL:
				{
					MultipoolAllocator* destination = (MultipoolAllocator*)destination_untyped;
					const MultipoolAllocator* source = (const MultipoolAllocator*)source_untyped;
					new (destination) MultipoolAllocator(Allocate(data->allocator, MultipoolAllocator::MemoryOf(source->GetBlockCount(), source->GetSize())), source->GetSize(), source->GetBlockCount());
				}
				break;
				case ECS_ALLOCATOR_ARENA:
				{
					MemoryArena* destination = (MemoryArena*)destination_untyped;
					const MemoryArena* source = (const MemoryArena*)source_untyped;
					new (destination) MemoryArena(data->allocator, source->m_allocator_count, source->GetInitialBaseAllocatorInfo());
				}
				break;
				case ECS_ALLOCATOR_MANAGER:
				{
					MemoryManager* destination = (MemoryManager*)destination_untyped;
					const MemoryManager* source = (const MemoryManager*)source_untyped;
					new (destination) MemoryManager(source->GetInitialAllocatorInfo(), source->m_backup_info, data->allocator);
				}
				break;
				case ECS_ALLOCATOR_RESIZABLE_LINEAR:
				{
					ResizableLinearAllocator* destination = (ResizableLinearAllocator*)destination_untyped;
					const ResizableLinearAllocator* source = (const ResizableLinearAllocator*)source_untyped;
					new (destination) ResizableLinearAllocator(source->m_initial_capacity, source->m_backup_size, data->allocator);
				}
				break;
				case ECS_ALLOCATOR_MEMORY_PROTECTED:
				{
					MemoryProtectedAllocator* destination = (MemoryProtectedAllocator*)destination_untyped;
					const MemoryProtectedAllocator* source = (const MemoryProtectedAllocator*)source_untyped;
					new (destination) MemoryProtectedAllocator(source->chunk_size, source->linear_allocators);
				}
				break;
				case ECS_ALLOCATOR_MALLOC:
				{
					// Just initialize the vtable for the destination
					MallocAllocator* destination = (MallocAllocator*)destination_untyped;
					new (destination) MallocAllocator();
				}
				break;
				case ECS_ALLOCATOR_INTERFACE:
				{
					ECS_ASSERT(false, "Unimplemented Interface allocator copy code path");
				}
				break;
				default:
					ECS_ASSERT(false);
				}
			};

			if (data->options.initialize_type_allocators) {
				ECS_ALLOCATOR_TYPE allocator_type = AllocatorTypeFromString(data->definition);
				if (allocator_type == ECS_ALLOCATOR_TYPE_COUNT) {
					ECS_ASSERT(data->definition == STRING(AllocatorPolymorphic));
					// Allocate an instance for the pointer
					AllocatorPolymorphic* destination = (AllocatorPolymorphic*)data->destination;
					const AllocatorPolymorphic* source = (const AllocatorPolymorphic*)data->source;
					*destination = *source;
					destination->allocator = (AllocatorBase*)Allocate(data->allocator, AllocatorStructureByteSize(source->allocator->m_allocator_type));
					initialize_implementation(source->allocator->m_allocator_type, destination->allocator, source->allocator);
				}
				else {
					initialize_implementation(allocator_type, data->destination, data->source);
				}
			}
		}

		bool AllocatorCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
			// Comparing allocators doesn't make sense, always return true such that the
			// Comparison is not disturbed by this entry.
			return true;
		}

		void AllocatorCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
			// TODO: Determine if this appropriate or not
			ECS_ALLOCATOR_TYPE allocator_type = AllocatorTypeFromStringWithPolymorphic(data->source, data->definition);
			ECS_ASSERT(allocator_type != ECS_ALLOCATOR_TYPE_COUNT, "Unknown reflection custom allocator!");
			FreeAllocatorFrom({ (AllocatorBase*)data->source, ECS_ALLOCATION_SINGLE }, data->allocator);
		}

		// The allocator doesn't need to implement the Element family of functions

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Hash table

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> HashTableExtractValueType(Stream<char> definition) {
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

			value_type = TrimWhitespace(value_type);
			return value_type;
		}

		HashTableTemplateArguments HashTableExtractTemplateArguments(Stream<char> definition) {
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

		// Returns a definition info for the value template type, while taking into account the special case of HashTableEmpty
		ReflectionDefinitionInfo HashTableGetValueDefinitionInfo(const ReflectionManager* reflection_manager, const HashTableTemplateArguments& template_arguments) {
			ReflectionDefinitionInfo value_info;
			if (template_arguments.table_type == HASH_TABLE_EMPTY) {
				value_info.alignment = 0;
				value_info.byte_size = 0;
				value_info.is_blittable = true;
			}
			else {
				value_info = SearchReflectionDefinitionInfo(reflection_manager, template_arguments.value_type);
				ECS_ASSERT(value_info.byte_size != -1, "Invalid reflection hash table value type");
			}
			return value_info;
		}

		// Returns a definition info for the identifier template type, while taking into account the special case of ResourceIdentifier
		ReflectionDefinitionInfo HashTableGetIdentifierDefinitionInfo(const ReflectionManager* reflection_manager, HashTableTemplateArguments& template_arguments) {
			ReflectionDefinitionInfo identifier_info;
			if (template_arguments.table_type == HASH_TABLE_DEFAULT || template_arguments.identifier_type == STRING(ResourceIdentifier)) {
				identifier_info.byte_size = sizeof(ResourceIdentifier);
				identifier_info.alignment = alignof(ResourceIdentifier);
				identifier_info.is_blittable = false;
				// Change the template parameter string type into Stream<char> as well, in order to have generic handling for this parameter later on
				// We have to set the custom type as well
				identifier_info.custom_type = ECS_REFLECTION_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_STREAM];
				template_arguments.identifier_type = STRING(Stream<char>);
			}
			else {
				identifier_info = SearchReflectionDefinitionInfo(reflection_manager, template_arguments.identifier_type);
				ECS_ASSERT(identifier_info.byte_size != -1, "Invalid reflection hash table identifier type");
			}
			return identifier_info;
		}

		// Returns in the x component the byte size of the pair, while in the y component the offset that must be added to the value byte size
		// In order to correctly address the identifier inside the pair
		ulong2 HashTableComputePairByteSizeAndAlignmentOffset(const ReflectionDefinitionInfo& value_definition_info, const ReflectionDefinitionInfo& identifier_definition_info) {
			size_t pair_size = value_definition_info.byte_size + identifier_definition_info.byte_size;
			size_t identifier_pair_offset = 0;
			// Align the end of the normal identifier, i.e. at the end of the value, and see if we need to adjust the offset
			size_t alignment_identifier_start = AlignPointer(value_definition_info.byte_size, identifier_definition_info.alignment);
			if (alignment_identifier_start != value_definition_info.byte_size) {
				identifier_pair_offset = alignment_identifier_start - value_definition_info.byte_size;
				pair_size += identifier_pair_offset;
			}

			return { pair_size, identifier_pair_offset };
		}

		size_t HashTableCustomTypeAllocateAndSetBuffers(
			void* hash_table, 
			size_t capacity, 
			AllocatorPolymorphic allocator, 
			bool is_soa, 
			const ReflectionDefinitionInfo& value_definition_info, 
			const ReflectionDefinitionInfo& identifier_definition_info
		) {
			HashTableDefault<char>* typed_table = (HashTableDefault<char>*)hash_table;
			// Set the typed table capacity to the one given such that we can ask for the extended capacity. Restore the previous capacity
			unsigned int previous_table_capacity = typed_table->GetCapacity();
			typed_table->m_capacity = capacity;
			size_t extended_capacity = typed_table->GetExtendedCapacity();
			typed_table->m_capacity = previous_table_capacity;

			if (capacity == 0) {
				// Set all buffers to nullptr
				typed_table->m_buffer = nullptr;
				typed_table->m_identifiers = nullptr;
				typed_table->m_metadata = nullptr;
				return 0;
			}
			else {
				if (is_soa) {
					// Compute the total byte size for the destination. We must use the extended capacity
					size_t total_allocation_size = (value_definition_info.byte_size + identifier_definition_info.byte_size + sizeof(unsigned char)) * extended_capacity;
					void* allocation = Allocate(allocator, total_allocation_size);

					// The metadata buffer is set last
					typed_table->m_buffer = (decltype(typed_table->m_buffer))allocation;
					typed_table->m_identifiers = (ResourceIdentifier*)OffsetPointer(typed_table->m_buffer, value_definition_info.byte_size * extended_capacity);
					typed_table->m_metadata = (unsigned char*)OffsetPointer(typed_table->m_identifiers, identifier_definition_info.byte_size * extended_capacity);
					return total_allocation_size;
				}
				else {
					ulong2 pair_size = HashTableComputePairByteSizeAndAlignmentOffset(value_definition_info, identifier_definition_info);
					size_t total_allocation_size = (pair_size.x + sizeof(unsigned char)) * extended_capacity;
					void* allocation = Allocate(allocator, total_allocation_size);

					// The metadata buffer is set last
					typed_table->m_buffer = (decltype(typed_table->m_buffer))allocation;
					typed_table->m_identifiers = nullptr;
					typed_table->m_metadata = (unsigned char*)OffsetPointer(typed_table->m_buffer, pair_size.x * extended_capacity);
					return total_allocation_size;
				}
			}
		}

		static void HashTableDeallocateWithElements(
			const ReflectionManager* reflection_manager,
			void* hash_table,
			AllocatorPolymorphic allocator,
			const HashTableTemplateArguments& template_arguments,
			const ReflectionDefinitionInfo& value_definition_info,
			const ReflectionDefinitionInfo& identifier_definition_info
		) {
			HashTableDefault<char>* destination = (HashTableDefault<char>*)hash_table;

			// If both types are blittable, we don't have to do anything
			if ((!value_definition_info.is_blittable || !identifier_definition_info.is_blittable) && destination->GetCapacity() > 0) {
				// We need separate branches for SoA and non SoA in order to be more performant
				// We can call the for each index function for both cases, but branch out for more performance
				if (template_arguments.is_soa) {
					void* value_type = destination->m_buffer;
					void* identifier_type = destination->m_identifiers;
					destination->ForEachIndex([&](unsigned int table_index) {
						if (!value_definition_info.is_blittable) {
							DeallocateReflectionInstanceBuffers(
								reflection_manager, 
								template_arguments.value_type, 
								value_definition_info, 
								OffsetPointer(value_type, table_index * value_definition_info.byte_size), 
								allocator
							);
						}
						if (!identifier_definition_info.is_blittable) {
							DeallocateReflectionInstanceBuffers(
								reflection_manager, 
								template_arguments.identifier_type, 
								identifier_definition_info, 
								OffsetPointer(identifier_type, table_index * identifier_definition_info.byte_size), 
								allocator
							);
						}
						return false;
					});
				}
				else {
					// This is a pair, but we must take into account any extra padding that happens due to alignment
					ulong2 pair_info = HashTableComputePairByteSizeAndAlignmentOffset(value_definition_info, identifier_definition_info);

					void* pair_buffer = destination->m_buffer;
					destination->ForEachIndex([&](unsigned int table_index) {
						void* current_pair = OffsetPointer(pair_buffer, pair_info.x * (size_t)table_index);
						if (!value_definition_info.is_blittable) {
							// The value starts directly at the current pair
							DeallocateReflectionInstanceBuffers(reflection_manager, template_arguments.value_type, value_definition_info, current_pair, allocator);
						}
						if (!identifier_definition_info.is_blittable) {
							DeallocateReflectionInstanceBuffers(
								reflection_manager, 
								template_arguments.identifier_type, 
								identifier_definition_info, 
								OffsetPointer(current_pair, value_definition_info.byte_size + pair_info.y),
								allocator
							);
						}
						return false;
					});
				}
			}

			// We can call deallocate no matter what the hash table type is. The deallocate will reset the buffers as we
			destination->Deallocate(allocator);
		}

		bool HashTableCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			if (data->definition.size < sizeof("HashTable<")) {
				return false;
			}

			if (data->definition[data->definition.size - 1] != '>') {
				return false;
			}

			if (data->definition.StartsWith("HashTable<") || data->definition.StartsWith("HashTableDefault<") || data->definition.StartsWith("HashTableEmpty<")) {
				return true;
			}

			return false;
		}

		ulong2 HashTableCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			// All hash tables have the same byte size and alignment. Technically, the size can depend on the hasher
			// Being used, but the ones we provide are at max 8 bytes long, which is the limit of the alignment
			HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);
			return { sizeof(HashTableDefault<char>), alignof(HashTableDefault<char>) };
		}

		void HashTableCustomTypeInterface::GetDependencies(ReflectionCustomTypeDependenciesData* data) {
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

			// Cast the two table sources to a random table specialization, to have easy access to the fields
			HashTableDefault<char>* destination = (HashTableDefault<char>*)data->destination;
			const HashTableDefault<char>* source = (const HashTableDefault<char>*)data->source;

			ReflectionDefinitionInfo value_definition_info = HashTableGetValueDefinitionInfo(data->reflection_manager, template_parameters);
			ReflectionDefinitionInfo identifier_definition_info = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_parameters);

			if (data->options.deallocate_existing_data) {
				HashTableDeallocateWithElements(data->reflection_manager, destination, data->allocator, template_parameters, value_definition_info, identifier_definition_info);
			}

			// We need to have separate branches for SoA
			size_t source_extended_capacity = source->GetExtendedCapacity();
			HashTableCustomTypeAllocateAndSetBuffers(destination, source->GetCapacity(), data->allocator, template_parameters.is_soa, value_definition_info, identifier_definition_info);

			destination->m_capacity = source->m_capacity;
			destination->m_count = source->m_count;
			// Copy the hasher as well, it has to be at max a void*
			memcpy(&destination->m_function, &source->m_function, sizeof(void*));

			// Copy the metadata buffer
			memcpy(destination->m_metadata, source->m_metadata, sizeof(source->m_metadata[0]) * source_extended_capacity);

			// If we need to call the copy function, these are the options
			CopyReflectionDataOptions copy_options = data;

			// Need to branch out again by SoA
			if (template_parameters.is_soa) {
				if (value_definition_info.is_blittable) {
					// Can directly memcpy this buffer
					memcpy(destination->m_buffer, source->m_buffer, value_definition_info.byte_size * source_extended_capacity);
				}
				else {
					// Determine the custom element options for this value type
					ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK(data->tags, STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE), options_storage);

					// Need to iterate and call the copy function. As tags, pass the element options
					source->ForEachIndexConst([&](unsigned int index) {
						CopyReflectionTypeInstance(
							data->reflection_manager,
							template_parameters.value_type,
							value_definition_info,
							OffsetPointer(source->m_buffer, (size_t)index * value_definition_info.byte_size),
							OffsetPointer(destination->m_buffer, (size_t)index * value_definition_info.byte_size),
							&copy_options,
							options_storage
						);
					});
				}

				if (identifier_definition_info.is_blittable) {
					// Can directly memcpy this buffer
					memcpy(destination->m_identifiers, source->m_identifiers, identifier_definition_info.byte_size * source_extended_capacity);
				}
				else {
					// Determine the custom element options for the identifier type;
					ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK(data->tags, STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER), options_storage);

					source->ForEachIndexConst([&](unsigned int index) {
						CopyReflectionTypeInstance(
							data->reflection_manager,
							template_parameters.identifier_type,
							identifier_definition_info,
							OffsetPointer(source->m_identifiers, (size_t)index * identifier_definition_info.byte_size),
							OffsetPointer(destination->m_identifiers, (size_t)index * identifier_definition_info.byte_size),
							&copy_options,
							options_storage
						);
					});
				}
			}
			else {
				ulong2 pair_size = HashTableComputePairByteSizeAndAlignmentOffset(value_definition_info, identifier_definition_info);
				if (value_definition_info.is_blittable && identifier_definition_info.is_blittable) {
					// Can memcpy directly
					memcpy(destination->m_buffer, source->m_buffer, pair_size.x * source_extended_capacity);
				}
				else {
					// Retrieve the tag options for each individual entry
					ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
						!value_definition_info.is_blittable, 
						data->tags, 
						STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE), 
						value_options_storage
					);

					ECS_GET_REFLECTION_CUSTOM_TYPE_ELEMENT_OPTIONS_STACK_CONDITIONAL(
						!identifier_definition_info.is_blittable,
						data->tags, 
						STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER),
						identifier_options_storage
					);

					// Go through each entry and call the copy individually
					// The copy function has a fast path for the is blittable, so no need to add a pre-check
					source->ForEachIndexConst([&](unsigned int index) {
						const void* source_element = OffsetPointer(source->m_buffer, (size_t)index * pair_size.x);
						void* destination_element = OffsetPointer(destination->m_buffer, (size_t)index * pair_size.x);
						CopyReflectionTypeInstance(data->reflection_manager, template_parameters.value_type, value_definition_info, source_element, destination_element, &copy_options, value_options_storage);

						const void* source_identifier = OffsetPointer(source_element, value_definition_info.byte_size + pair_size.y);
						void* destination_identifier = OffsetPointer(destination_element, value_definition_info.byte_size + pair_size.y);
						CopyReflectionTypeInstance(
							data->reflection_manager,
							template_parameters.identifier_type,
							identifier_definition_info,
							source_identifier,
							destination_identifier,
							&copy_options,
							identifier_options_storage
						);
					});
				}
			}
		}

		bool HashTableCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
			// Type pun the elements such that we can access the fields easily
			const HashTableDefault<char>* first = (const HashTableDefault<char>*)data->first;
			const HashTableDefault<char>* second = (const HashTableDefault<char>*)data->second;

			// If the counts are different, early exit
			if (first->GetCount() != second->GetCount()) {
				return false;
			}

			HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);

			ReflectionDefinitionInfo value_definition_info = HashTableGetValueDefinitionInfo(data->reflection_manager, template_arguments);
			ReflectionDefinitionInfo identifier_definition_info = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_arguments);

			// In order to determine if they are the same, we can't use a simple memcmp.
			// For each entry in the first, find its identifier. When it is found, compare
			// Its value, if they are not the same, early exit

			size_t second_extended_capacity = second->GetExtendedCapacity();
			// If the identifier is not blittable or the hash table is not soa, create an array with the indices that have not been matched.
			// Once we match an index, remove it, to not spend more time checking it again
			CapacityStream<unsigned int> identifier_iteration_order;
			if (!identifier_definition_info.is_blittable || !template_arguments.is_soa) {
				void* buffer_allocation = nullptr;
				if (second->GetCount() < ECS_KB * 32) {
					// Can use a stack allocation to store these indices
					buffer_allocation = ECS_STACK_ALLOC(sizeof(unsigned int) * second->GetCount());
				}
				else {
					// Call malloc, not the best case, profile if we should do something about it
					buffer_allocation = Malloc(sizeof(unsigned int) * second->GetCount());
				}

				identifier_iteration_order.InitializeFromBuffer(buffer_allocation, 0, second->GetCount());
				second->GetElementIndices(identifier_iteration_order);
			}

			// We need to branch out based on soa
			if (template_arguments.is_soa) {
				auto compare_entry = [&](unsigned int index, auto is_blittable) {
					const void* first_identifier = OffsetPointer(first->m_identifiers, (size_t)index * identifier_definition_info.byte_size);
					size_t second_identifier_index = -1;
					if constexpr (is_blittable) {
						// Can use a simple search bytes. If we hit a case where the entry that matches does not exist, i.e. the slot is empty, continue the search
						second_identifier_index = SearchBytesEx(second->m_identifiers, second_extended_capacity, first_identifier, identifier_definition_info.byte_size);
						while (second_identifier_index != -1 && !second->IsItemAt(second_identifier_index)) {
							size_t new_offset = SearchBytesEx(
								OffsetPointer(second->m_identifiers, identifier_definition_info.byte_size * (second_identifier_index + 1)),
								second_extended_capacity - second_identifier_index - 1,
								first_identifier,
								identifier_definition_info.byte_size
							);
							second_identifier_index = new_offset == -1 ? -1 : second_identifier_index + new_offset + 1;
						}
					}
					else {
						// We must compare manually
						for (unsigned int subindex = 0; subindex < identifier_iteration_order.size; subindex++) {
							if (CompareReflectionTypeInstances(
								data->reflection_manager,
								template_arguments.identifier_type,
								identifier_definition_info,
								first_identifier,
								OffsetPointer(second->m_identifiers, (size_t)identifier_iteration_order[subindex] * identifier_definition_info.byte_size)
							)) {
								second_identifier_index = identifier_iteration_order[subindex];
								// Remove the entry
								identifier_iteration_order.RemoveSwapBack(subindex);
								break;
							}
						}
					}

					if (second_identifier_index == -1) {
						return true;
					}

					// Check the value at that index
					const void* first_value = OffsetPointer(first->m_buffer, (size_t)index * value_definition_info.byte_size);
					const void* second_value = OffsetPointer(second->m_buffer, (size_t)second_identifier_index * value_definition_info.byte_size);
					if (value_definition_info.is_blittable) {
						// Use memcmp
						if (memcmp(first_value, second_value, value_definition_info.byte_size) != 0) {
							return true;
						}
					}
					else {
						// Use the compare function
						if (!CompareReflectionTypeInstances(data->reflection_manager, template_arguments.value_type, value_definition_info, first_value, second_value)) {
							return true;
						}
					}
					return false;
				};

				// Hoist this check outside the iteration
				if (identifier_definition_info.is_blittable) {
					if (first->ForEachIndexConst<true>([&](unsigned int index) {
						return compare_entry(index, std::true_type{});
					})) {
						return false;
					}
				}
				else {
					if (first->ForEachIndexConst<true>([&](unsigned int index) {
						return compare_entry(index, std::false_type{});
					})) {
						return false;
					}
				}
			}
			else {
				ulong2 pair_info = HashTableComputePairByteSizeAndAlignmentOffset(value_definition_info, identifier_definition_info);
				// No need to branch out based on the blittability of the identifier, we are always using indices
				if (first->ForEachIndexConst<true>([&](unsigned int index) {
					const void* first_value = OffsetPointer(first->m_buffer, pair_info.x * (size_t)index);
					const void* first_identifier = OffsetPointer(first_value, value_definition_info.byte_size + pair_info.y);

					const void* second_value = nullptr;
					for (unsigned int subindex = 0; subindex < identifier_iteration_order.size; subindex++) {
						const void* current_second_value = OffsetPointer(second->m_buffer, pair_info.x * (size_t)identifier_iteration_order[subindex]);
						const void* current_second_identifier = OffsetPointer(current_second_value, value_definition_info.byte_size + pair_info.y);
						if (CompareReflectionTypeInstances(data->reflection_manager, template_arguments.identifier_type, identifier_definition_info, first_identifier, current_second_identifier)) {
							second_value = current_second_value;
							identifier_iteration_order.RemoveSwapBack(subindex);
							break;
						}
					}

					if (second_value == nullptr) {
						return true;
					}

					return !CompareReflectionTypeInstances(data->reflection_manager, template_arguments.value_type, value_definition_info, first_value, second_value);
				})) {
					return false;
				}
			}

			return true;
		}

		void HashTableCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
			HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);
			ReflectionDefinitionInfo value_info = HashTableGetValueDefinitionInfo(data->reflection_manager, template_arguments);
			ReflectionDefinitionInfo identifier_info = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_arguments);

			// All hash tables should have the same byte size - if the hash function has at max 8 bytes
			size_t hash_table_byte_size = sizeof(HashTableDefault<char>);
			for (size_t index = 0; index < data->element_count; index++) {
				void* current_source = OffsetPointer(data->source, index * hash_table_byte_size);
				HashTableDeallocateWithElements(data->reflection_manager, current_source, data->allocator, template_arguments, value_info, identifier_info);
			}
		}

		size_t HashTableCustomTypeInterface::GetElementCount(ReflectionCustomTypeGetElementCountData* data) {
			// Can type pun to any type
			const HashTableDefault<char>* table = (const HashTableDefault<char>*)data->source;
			return table->GetCount();
		}

		struct HashTableGetElementCacheInfo {
			bool is_soa;
			size_t pair_byte_size;
			size_t pair_identifier_offset;

			// These 2 fields are used to speed up the get index search, especially if linearly iterating
			size_t last_user_index;
			size_t last_table_index;
		};

		static_assert(sizeof(ECS_REFLECTION_CUSTOM_TYPE_GET_ELEMENT_CACHE_SIZE) <= ECS_REFLECTION_CUSTOM_TYPE_GET_ELEMENT_CACHE_SIZE);

		static void HashTableInitializeGetElementCacheInfo(ReflectionCustomTypeGetElementDataBase* data) {
			if (!data->has_cache) {
				data->has_cache = true;
				HashTableTemplateArguments template_arguments = HashTableExtractTemplateArguments(data->definition);
				HashTableGetElementCacheInfo* cache_info = (HashTableGetElementCacheInfo*)data->element_cache_data;
				cache_info->is_soa = template_arguments.is_soa;
				cache_info->last_table_index = 0;
				cache_info->last_user_index = 0;

				// If the element name type is not specified, error, since it's better for the user to be explicit about this
				//ECS_ASSERT(data->element_name_type.size > 0, "Hash table reflection custom type unspecified element name type for GetElement call!");

				if (data->element_name_type == STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER)) {
					data->element_name_index = ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER;
				}
				else {
					data->element_name_index = ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE;
				}

				if (cache_info->is_soa) {
					cache_info->pair_identifier_offset = 0;
					cache_info->pair_byte_size = 0;

					if (data->element_name_index == ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE) {
						ReflectionDefinitionInfo value_definition = HashTableGetValueDefinitionInfo(data->reflection_manager, template_arguments);
						data->element_byte_size = value_definition.byte_size;
					}
					else {
						ReflectionDefinitionInfo identifier_definition = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_arguments);
						data->element_byte_size = identifier_definition.byte_size;
					}
				}
				else {
					ReflectionDefinitionInfo value_definition = HashTableGetValueDefinitionInfo(data->reflection_manager, template_arguments);
					ReflectionDefinitionInfo identifier_definition = HashTableGetIdentifierDefinitionInfo(data->reflection_manager, template_arguments);
					ulong2 pair_info = HashTableComputePairByteSizeAndAlignmentOffset(value_definition, identifier_definition);
					cache_info->pair_identifier_offset = value_definition.byte_size + pair_info.y;
					cache_info->pair_byte_size = pair_info.x;
					if (data->element_name_index == ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE) {
						data->element_byte_size = value_definition.byte_size;
					}
					else {
						data->element_byte_size = identifier_definition.byte_size;
					}
				}

				
			}
		}

		void* HashTableCustomTypeInterface::GetElement(ReflectionCustomTypeGetElementData* data) {
			HashTableInitializeGetElementCacheInfo(data);

			HashTableGetElementCacheInfo* cache_info = (HashTableGetElementCacheInfo*)data->element_cache_data;
			HashTableDefault<char>* table = (HashTableDefault<char>*)data->source;

			// Find the table index that corresponds to the user index
			size_t table_index = 0;
			size_t extended_capacity = table->GetExtendedCapacity();
			if (data->is_token) {
				table_index = data->index_or_token;
			}
			else {
				table_index = cache_info->last_user_index > data->index_or_token ? 0 : cache_info->last_table_index;
				size_t remaining_elements = cache_info->last_user_index > data->index_or_token ? data->index_or_token : data->index_or_token - cache_info->last_user_index;
				// We exit the while from a break
				while (table_index < extended_capacity) {
					if (table->IsItemAt(table_index)) {
						remaining_elements--;
						if (remaining_elements == 0) {
							break;
						}
					}
					table_index++;
				}
			}

			if (table_index >= extended_capacity) {
				// The element is out of bounds
				return nullptr;
			}

			cache_info->last_table_index = table_index;
			cache_info->last_user_index = data->index_or_token;

			if (cache_info->is_soa) {
				if (data->element_name_index == ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE) {
					return OffsetPointer(table->m_buffer, data->element_byte_size * table_index);
				}
				else {
					return OffsetPointer(table->m_identifiers, data->element_byte_size * table_index);
				}
			}
			else {
				void* pair_start = OffsetPointer(table->m_buffer, table_index * cache_info->pair_byte_size);
				if (data->element_name_index == ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE) {
					return pair_start;
				}
				else {
					return OffsetPointer(pair_start, cache_info->pair_identifier_offset);
				}
			}
		}

		ReflectionCustomTypeGetElementIndexOrToken HashTableCustomTypeInterface::FindElement(ReflectionCustomTypeFindElementData* data) {
			HashTableInitializeGetElementCacheInfo(data);

			HashTableGetElementCacheInfo* cache_info = (HashTableGetElementCacheInfo*)data->element_cache_data;
			HashTableDefault<char>* table = (HashTableDefault<char>*)data->source;

			size_t table_index = -1;
			if (cache_info->is_soa) {
				if (data->element_name_index == ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE) {
					table_index = SearchBytesEx(table->m_buffer, table->GetExtendedCapacity(), data->element, data->element_byte_size);
				}
				else {
					table_index = SearchBytesEx(table->m_identifiers, table->GetExtendedCapacity(), data->element, data->element_byte_size);
				}
			}
			else {
				// Can't use the clasical search bytes
				for (size_t index = 0; index < (size_t)table->GetExtendedCapacity(); index++) {
					if (table->IsItemAt(index)) {
						const void* pair = OffsetPointer(table->m_buffer, cache_info->pair_byte_size * index);
						const void* pointer = pair;
						if (data->element_name_index == ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER) {
							pointer = OffsetPointer(pair, cache_info->pair_identifier_offset);
						}

						if (memcmp(data->element, pointer, data->element_byte_size) == 0) {
							table_index = index;
							break;
						}
					}
				}
			}

			if (table_index == -1) {
				return -1;
			}

			if (data->is_token) {
				// Can return the value as is
				return table_index;
			}
			else {
				// Determine how many other entries are before it. We can accelerate this in the future with SIMD, but we need to have a use case for it
				size_t previous_entry_count = 0;
				for (size_t index = 0; index < table_index; index++) {
					if (table->IsItemAt(index)) {
						previous_entry_count++;
					}
				}

				return previous_entry_count;
			}
		}

		bool HashTableCustomTypeInterface::ValidateTags(ReflectionCustomTypeValidateTagData* data) {
			// Validate that if the pointer reference tag is specified, then the custom element options for it are specified as well,
			// Since we want the user to be explicit about which element type it should target. The target element should be correct as well
			Stream<char> pointer_key;
			Stream<char> custom_element_type;
			if (GetReflectionPointerReferenceKeyParams(data->tag, pointer_key, custom_element_type)) {
				if (pointer_key.size > 0) {
					bool success = custom_element_type == STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE) || custom_element_type == STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER);
					if (!success) {
						if (custom_element_type.size == 0) {
							ECS_FORMAT_ERROR_MESSAGE(data->error_message, "Hash table pointer reference was specified, but the element for which it applies it was not");
						}
						else {
							ECS_FORMAT_ERROR_MESSAGE(data->error_message, "Hash table pointer reference was specified, but the element for which it applies it is not correct");
						}
					}
					return success;
				}
			}

			return true;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Deck

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DeckCustomTypeInterface::Match(ReflectionCustomTypeMatchData* data) {
			return data->definition.StartsWith("Deck<") || data->definition.StartsWith("DeckPowerOfTwo<") || data->definition.StartsWith("DeckModulo<");
		}

		ulong2 DeckCustomTypeInterface::GetByteSize(ReflectionCustomTypeByteSizeData* data) {
			// All decks have the same byte size and alignment, regardless of the argument
			return { sizeof(DeckPowerOfTwo<char>), alignof(DeckPowerOfTwo<char>) };
		}

		void DeckCustomTypeInterface::GetDependencies(ReflectionCustomTypeDependenciesData* data) {
			// This will work for the generalization, but as well as the specialization
			ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 2);
			ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);
			ReflectionCustomTypeAddDependentType(data, template_arguments[0]);
		}

		bool DeckCustomTypeInterface::IsBlittable(ReflectionCustomTypeIsBlittableData* data) {
			return false;
		}

		void DeckCustomTypeInterface::Copy(ReflectionCustomTypeCopyData* data) {
			// Type cast to a specific type to have access to the fields of the structures
			const DeckPowerOfTwo<char>* source = (const DeckPowerOfTwo<char>*)data->source;
			DeckPowerOfTwo<char>* destination = (DeckPowerOfTwo<char>*)data->destination;

			// We don't care about the accesor template argument, just about the first one
			ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 2);
			ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);

			// If the overwrite resizable allocator is specified, do it before the deallocate
			if (data->options.overwrite_resizable_allocators) {
				destination->buffers.allocator = data->allocator;
			}

			if (data->options.deallocate_existing_data) {
				destination->Deallocate();
			}

			ReflectionDefinitionInfo element_info = SearchReflectionDefinitionInfo(data->reflection_manager, template_arguments[0]);

			// Copy the entire fields, and then overwrite the buffers
			memcpy(destination, source, sizeof(*source));
			destination->buffers.Initialize(data->allocator, source->buffers.size);
			destination->buffers.size = source->buffers.size;
			
			for (size_t index = 0; index < source->buffers.size; index++) {
				destination->buffers[index].buffer = (char*)Allocate(data->allocator, element_info.byte_size * source->buffers[index].capacity, element_info.alignment);

				CopyReflectionDataOptions copy_options = data;
				CopyReflectionTypeInstance(data->reflection_manager, template_arguments[0], element_info, source->buffers[index].buffer, destination->buffers[index].buffer, source->buffers[index].size, &copy_options, data->tags);

				destination->buffers[index].size = source->buffers[index].size;
				destination->buffers[index].capacity = source->buffers[index].capacity;
			}
		}

		bool DeckCustomTypeInterface::Compare(ReflectionCustomTypeCompareData* data) {
			// Type cast to known types to access the fields easier
			const DeckPowerOfTwo<char>* first = (const DeckPowerOfTwo<char>*)data->first;
			const DeckPowerOfTwo<char>* second = (const DeckPowerOfTwo<char>*)data->second;

			// Early exit if the obvious parameters are different
			if (first->chunk_size != second->chunk_size || first->power_of_two_exponent != second->power_of_two_exponent|| first->size != second->size
				|| first->buffers.size != second->buffers.size) {
				return false;
			}

			// Do a prepass and if the individual counts of the substreams is different, fail
			for (size_t index = 0; index < first->buffers.size; index++) {
				if (first->buffers[index].size != second->buffers[index].size) {
					return false;
				}
			}

			// We don't care about the accesor template argument, just about the first one
			ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 2);
			ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);

			ReflectionDefinitionInfo element_info = SearchReflectionDefinitionInfo(data->reflection_manager, template_arguments[0]);
			for (size_t index = 0; index < first->buffers.size; index++) {
				if (!CompareReflectionTypeInstances(data->reflection_manager, template_arguments[0], element_info, first->buffers[index].buffer, second->buffers[index].buffer, first->buffers[index].size)) {
					return false;
				}
			}

			return true;
		}

		void DeckCustomTypeInterface::Deallocate(ReflectionCustomTypeDeallocateData* data) {
			ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 2);
			ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);
			ReflectionDefinitionInfo definition_info = SearchReflectionDefinitionInfo(data->reflection_manager, template_arguments[0]);

			DeckPowerOfTwo<char>* decks = (DeckPowerOfTwo<char>*)data->source;
			for (size_t index = 0; index < data->element_count; index++) {
				AllocatorPolymorphic initial_deck_allocator = decks[index].buffers.allocator;

				// Iterate over the chunks only if the elements themselves are not blittable
				if (!definition_info.is_blittable) {
					for (size_t subindex = 0; subindex < decks[index].buffers.size; subindex++) {
						DeallocateReflectionInstanceBuffers(data->reflection_manager, template_arguments[0], definition_info, decks[index].buffers[subindex].buffer, data->allocator, decks[index].buffers[subindex].size);
					}
				}

				// TODO: Should we override the allocator or leave it like it is?
				decks[index].buffers.allocator = data->allocator;
				decks[index].Deallocate();

				// Restore the previous allocator afterwards
				decks[index].buffers.allocator = initial_deck_allocator;
			}
		}

		size_t DeckCustomTypeInterface::GetElementCount(ReflectionCustomTypeGetElementCountData* data) {
			const DeckPowerOfTwo<char>* deck = (const DeckPowerOfTwo<char>*)data->source;
			return deck->size;
		}

		static void DeckUpdateCache(ReflectionCustomTypeGetElementDataBase* data) {
			if (!data->has_cache) {
				ECS_STACK_CAPACITY_STREAM(Stream<char>, template_arguments, 2);
				ReflectionCustomTypeGetTemplateArguments(data->definition, template_arguments);

				data->has_cache = true;
				data->element_byte_size = SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_arguments[0]);
			}
		}

		void* DeckCustomTypeInterface::GetElement(ReflectionCustomTypeGetElementData* data) {
			const DeckPowerOfTwo<char>* deck = (const DeckPowerOfTwo<char>*)data->source;
			DeckUpdateCache(data);

			// We can index the element directly, using modulo. Indices or tokens are the same
			size_t chunk_index = data->index_or_token / deck->chunk_size;
			size_t in_chunk_index = data->index_or_token % deck->chunk_size;
			return OffsetPointer(deck->buffers[chunk_index].buffer, in_chunk_index * data->element_byte_size);
		}

		ReflectionCustomTypeGetElementIndexOrToken DeckCustomTypeInterface::FindElement(ReflectionCustomTypeFindElementData* data) {
			const DeckPowerOfTwo<char>* deck = (const DeckPowerOfTwo<char>*)data->source;
			DeckUpdateCache(data);

			for (size_t index = 0; index < deck->buffers.size; index++) {
				size_t current_deck_index = SearchBytesEx(deck->buffers[index].buffer, deck->buffers[index].size, data->element, data->element_byte_size);
				if (current_deck_index != -1) {
					return current_deck_index + deck->chunk_size * index;
				}
			}

			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

	}

}