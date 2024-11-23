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
			new AllocatorCustomTypeInterface()
		};

		static_assert(ECS_COUNTOF(ECS_REFLECTION_CUSTOM_TYPES) == ECS_REFLECTION_CUSTOM_TYPE_COUNT, "ECS_REFLECTION_CUSTOM_TYPES is not in sync");

		// ----------------------------------------------------------------------------------------------------------------------------

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

			Stream<char> type = { opened_bracket.buffer + 1, PointerDifference(closed_bracket.buffer, opened_bracket.buffer) / sizeof(char) - 1 };
			return type;
		}

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

#pragma region Allocator

		// TODO: Finish this

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

	}

}