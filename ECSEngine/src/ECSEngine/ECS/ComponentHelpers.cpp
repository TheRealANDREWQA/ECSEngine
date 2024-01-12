#include "ecspch.h"
#include "ComponentHelpers.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Allocators/ResizableLinearAllocator.h"

#define MAX_COMPONENT_BUFFERS 16
#define MAX_COMPONENT_BLITTABLE_FIELDS 16

namespace ECSEngine {

	using namespace Reflection;

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeComponent(const ReflectionType* type)
	{
		bool is_tag = type->IsTag(ECS_COMPONENT_TAG);
		if (is_tag) {
			double evaluation = type->GetEvaluation(ECS_COMPONENT_IS_SHARED_FUNCTION);
			return evaluation != DBL_MAX ? evaluation == 0.0 : false;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeSharedComponent(const ReflectionType* type)
	{
		bool is_tag = type->IsTag(ECS_COMPONENT_TAG);
		if (is_tag) {
			double evaluation = type->GetEvaluation(ECS_COMPONENT_IS_SHARED_FUNCTION);
			return evaluation != DBL_MAX ? evaluation == 1.0 : false;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeMaybeComponent(const Reflection::ReflectionType* type)
	{
		bool is_tag = type->IsTag(ECS_COMPONENT_TAG);
		if (is_tag && type->GetEvaluation(ECS_COMPONENT_IS_SHARED_FUNCTION) == DBL_MAX) {
			return true;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeGlobalComponent(const Reflection::ReflectionType* type)
	{
		return type->IsTag(ECS_GLOBAL_COMPONENT_TAG);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeLinkComponent(const ReflectionType* type)
	{
		return type->HasTag(ECS_LINK_COMPONENT_TAG);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeComponentType(const ReflectionType* type, ECS_COMPONENT_TYPE component_type)
	{
		if (component_type == ECS_COMPONENT_UNIQUE) {
			return IsReflectionTypeComponent(type);
		}
		else if (component_type == ECS_COMPONENT_SHARED) {
			return IsReflectionTypeSharedComponent(type);
		}
		else if (component_type == ECS_COMPONENT_GLOBAL) {
			return IsReflectionTypeGlobalComponent(type);
		}
		else {
			return IsReflectionTypeComponent(type) || IsReflectionTypeSharedComponent(type) || IsReflectionTypeGlobalComponent(type);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	ComponentBuffer GetReflectionTypeRuntimeBufferIndex(const ReflectionType* type, unsigned int buffer_index, unsigned int* field_index)
	{
		unsigned int index = 0;
		unsigned int current_buffer_index = 0;
		for (; index < type->fields.size && current_buffer_index <= buffer_index; index++) {
			if (IsStream(type->fields[index].info.stream_type) && type->fields[index].info.basic_type != ReflectionBasicFieldType::UserDefined) {
				current_buffer_index++;
			}
			else if (type->fields[index].definition == STRING(DataPointer)) {
				current_buffer_index++;
			}
		}

		if (current_buffer_index <= buffer_index) {
			// Invalid buffer index
			if (field_index != nullptr) {
				*field_index = -1;
			}
			return {};
		}

		ComponentBuffer component_buffer;
		// TODO: Add a reflection field tag such that it can specify the stream byte size
		if (type->fields[index].definition == STRING(DataPointer)) {
			component_buffer.element_byte_size = 1;
			component_buffer.is_data_pointer = true;
			component_buffer.pointer_offset = type->fields[index].info.pointer_offset;
			component_buffer.size_offset = 0;
		}
		else {
			component_buffer.element_byte_size = type->fields[index].info.stream_byte_size;
			component_buffer.is_data_pointer = false;
			component_buffer.pointer_offset = type->fields[index].info.pointer_offset + offsetof(Stream<void>, buffer);
			component_buffer.size_offset = type->fields[index].info.pointer_offset + offsetof(Stream<void>, size);
		}
		if (field_index != nullptr) {
			*field_index = index;
		}
		return component_buffer;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	void GetReflectionTypeRuntimeBuffers(const ReflectionType* type, CapacityStream<ComponentBuffer>& component_buffers)
	{
		// Get the buffers and the data_pointers
		for (size_t index = 0; index < type->fields.size; index++) {
			if (IsStream(type->fields[index].info.stream_type) && type->fields[index].info.basic_type != ReflectionBasicFieldType::UserDefined) {
				ComponentBuffer component_buffer;
				component_buffer.element_byte_size = type->fields[index].info.stream_byte_size;
				component_buffer.is_data_pointer = false;
				component_buffer.pointer_offset = type->fields[index].info.pointer_offset + offsetof(Stream<void>, buffer);
				component_buffer.size_offset = type->fields[index].info.pointer_offset + offsetof(Stream<void>, size);
				component_buffers.Add(component_buffer);
			}
			else if (type->fields[index].definition == STRING(DataPointer)) {
				// TODO: Add a reflection field tag such that it can specify the stream byte size
				ComponentBuffer component_buffer;
				component_buffer.element_byte_size = 1;
				component_buffer.is_data_pointer = true;
				component_buffer.pointer_offset = type->fields[index].info.pointer_offset;
				component_buffer.size_offset = 0;
				component_buffers.Add(component_buffer);
			}
		}
		component_buffers.AssertCapacity();
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	struct RuntimeComponentCopyDeallocateData {
		ComponentBuffer component_buffers[MAX_COMPONENT_BUFFERS];
		unsigned char count;
	};

	static void ReflectionTypeRuntimeComponentCopy(ComponentCopyFunctionData* copy_data) {
		RuntimeComponentCopyDeallocateData* data = (RuntimeComponentCopyDeallocateData*)copy_data->function_data;
		if (copy_data->deallocate_previous) {
			for (unsigned char index = 0; index < data->count; index++) {
				ComponentBufferReallocate(data->component_buffers[index], copy_data->allocator, copy_data->source, copy_data->destination);
			}
		}
		else {
			for (unsigned char index = 0; index < data->count; index++) {
				ComponentBufferCopy(data->component_buffers[index], copy_data->allocator, copy_data->source, copy_data->destination);
			}
		}
	}

	static void ReflectionTypeRuntimeComponentDeallocate(ComponentDeallocateFunctionData* deallocate_data) {
		RuntimeComponentCopyDeallocateData* data = (RuntimeComponentCopyDeallocateData*)deallocate_data->function_data;
		for (unsigned char index = 0; index < data->count; index++) {
			ComponentBufferDeallocate(data->component_buffers[index], deallocate_data->allocator, deallocate_data->data);
			// Set the stream or data pointer to empty for good measure - even tho it is not entirely necessary
			ComponentBufferSetStream(data->component_buffers[index], deallocate_data->data, {});
		}
	}

	ComponentFunctions GetReflectionTypeRuntimeComponentFunctions(const Reflection::ReflectionType* type, CapacityStream<void>* stack_memory)
	{
		ComponentFunctions component_functions;
		ECS_STACK_CAPACITY_STREAM(ComponentBuffer, component_buffers, MAX_COMPONENT_BUFFERS);
		GetReflectionTypeRuntimeBuffers(type, component_buffers);
		if (component_buffers.size > 0) {
			component_functions.copy_function = ReflectionTypeRuntimeComponentCopy;
			component_functions.deallocate_function = ReflectionTypeRuntimeComponentDeallocate;
			component_functions.allocator_size = GetReflectionComponentAllocatorSize(type);

			RuntimeComponentCopyDeallocateData* function_data = (RuntimeComponentCopyDeallocateData*)OffsetPointer(*stack_memory);
			ECS_ASSERT(stack_memory->capacity - stack_memory->size >= sizeof(*function_data), "Insufficient reflection type runtime component functions stack memory");
			stack_memory->size += sizeof(*function_data);
			function_data->count = component_buffers.size;
			component_buffers.CopyTo(function_data->component_buffers);
			component_functions.data = { function_data, sizeof(*function_data) };
		}

		return component_functions;
	}

	ComponentFunctions GetReflectionTypeRuntimeComponentFunctions(const Reflection::ReflectionType* type, AllocatorPolymorphic allocator)
	{
		ECS_STACK_VOID_STREAM(stack_memory, 1024);
		ComponentFunctions result = GetReflectionTypeRuntimeComponentFunctions(type, &stack_memory);
		if (result.data.size > 0) {
			result.data = result.data.Copy(allocator);
		}
		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	struct ReflectionTypeRuntimeCompareData {
		// The x contains the pointers offset, the y the field byte size
		ushort2 blittable_fields[MAX_COMPONENT_BLITTABLE_FIELDS];
		ComponentBuffer component_buffers[MAX_COMPONENT_BUFFERS];
		unsigned char blittable_field_count;
		unsigned char component_buffer_count;
	};

	static bool ReflectionTypeRuntimeCompare(SharedComponentCompareFunctionData* data) {
		const ReflectionTypeRuntimeCompareData* function_data = (const ReflectionTypeRuntimeCompareData*)data->function_data;
		
		for (unsigned char index = 0; index < function_data->blittable_field_count; index++) {
			const void* first_field = OffsetPointer(data->first, function_data->blittable_fields[index].x);
			const void* second_field = OffsetPointer(data->second, function_data->blittable_fields[index].x);
			if (memcmp(first_field, second_field, function_data->blittable_fields[index].y) != 0) {
				return false;
			}
		}
		for (unsigned char index = 0; index < function_data->component_buffer_count; index++) {
			Stream<void> first_stream = ComponentBufferGetStream(function_data->component_buffers[index], data->first);
			Stream<void> second_stream = ComponentBufferGetStream(function_data->component_buffers[index], data->second);
			if (first_stream.size != second_stream.size || memcmp(first_stream.buffer, second_stream.buffer, first_stream.size) != 0) {
				return false;
			}
		}

		return true;
	}

	SharedComponentCompareEntry GetReflectionTypeRuntimeCompareEntry(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type, 
		CapacityStream<void>* stack_memory
	)
	{
		SharedComponentCompareEntry entry;

		if (!IsBlittableWithPointer(type)) {
			entry.function = ReflectionTypeRuntimeCompare;
			ECS_ASSERT(stack_memory->size + sizeof(ReflectionTypeRuntimeCompareData) <= stack_memory->capacity);
			ReflectionTypeRuntimeCompareData* data = (ReflectionTypeRuntimeCompareData*)OffsetPointer(*stack_memory);
			data->blittable_field_count = 0;
			data->component_buffer_count = 0;

			for (size_t index = 0; index < type->fields.size; index++) {
				if (SearchIsBlittableWithPointer(reflection_manager, type->fields[index].definition)) {
					data->blittable_fields[data->blittable_field_count++] = { type->fields[index].info.pointer_offset, type->fields[index].info.byte_size };
				}
			}

			ECS_STACK_CAPACITY_STREAM(ComponentBuffer, runtime_buffers, MAX_COMPONENT_BUFFERS);
			GetReflectionTypeRuntimeBuffers(type, runtime_buffers);
			runtime_buffers.AssertCapacity();
			data->component_buffer_count = runtime_buffers.size;
			runtime_buffers.CopyTo(data->component_buffers);
			entry.data = data;
		}

		return entry;
	}

	SharedComponentCompareEntry GetReflectionTypeRuntimeCompareEntry(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type, 
		AllocatorPolymorphic allocator
	)
	{
		ECS_STACK_VOID_STREAM(stack_memory, 256);
		SharedComponentCompareEntry entry = GetReflectionTypeRuntimeCompareEntry(reflection_manager, type, &stack_memory);
		if (entry.data.size > 0) {
			entry.data = entry.data.Copy(allocator);
		}
		return entry;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	Component GetReflectionTypeComponent(const Reflection::ReflectionType* type)
	{
		double evaluation = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
		if (evaluation == DBL_MAX) {
			return Component{ -1 };
		}
		return Component{ (short)evaluation };
	}

	void GetHierarchyComponentTypes(
		const Reflection::ReflectionManager* reflection_manager, 
		unsigned int hierarchy_index, 
		CapacityStream<unsigned int>* unique_indices, 
		CapacityStream<unsigned int>* shared_indices,
		CapacityStream<unsigned int>* global_indices
	) 
	{
		ECS_STACK_CAPACITY_STREAM(unsigned, all_indices, ECS_KB * 16);
		Stream<char> tags[] = {
			ECS_COMPONENT_TAG,
			ECS_GLOBAL_COMPONENT_TAG
		};

		ReflectionManagerGetQuery query_options;
		query_options.tags = { tags, std::size(tags) };
		query_options.indices = &all_indices;
		query_options.strict_compare = true;

		reflection_manager->GetHierarchyTypes(query_options, hierarchy_index);

		for (unsigned int index = 0; index < all_indices.size; index++) {
			const Reflection::ReflectionType* reflection_type = reflection_manager->GetType(all_indices[index]);
			ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(reflection_type);

			if (component_type == ECS_COMPONENT_UNIQUE) {
				unique_indices->AddAssert(all_indices[index]);
			}
			else if (component_type == ECS_COMPONENT_SHARED) {
				shared_indices->AddAssert(all_indices[index]);
			}
			else if (component_type == ECS_COMPONENT_GLOBAL) {
				global_indices->AddAssert(all_indices[index]);
			}
			else {
				// It might be a maybe component
				ECS_ASSERT(IsReflectionTypeMaybeComponent(reflection_type));
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	size_t GetReflectionComponentAllocatorSize(const Reflection::ReflectionType* type)
	{
		double evaluation = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
		return evaluation == DBL_MAX ? 0 : (size_t)evaluation;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	ECS_COMPONENT_TYPE GetReflectionTypeComponentType(const Reflection::ReflectionType* type)
	{
		if (IsReflectionTypeComponent(type)) {
			return ECS_COMPONENT_UNIQUE;
		}
		else if (IsReflectionTypeSharedComponent(type)) {
			return ECS_COMPONENT_SHARED;
		}
		else if (IsReflectionTypeGlobalComponent(type)) {
			return ECS_COMPONENT_GLOBAL;
		}
		return ECS_COMPONENT_TYPE_COUNT;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool HasReflectionTypeComponentBuffers(const Reflection::ReflectionType* type)
	{
		return !IsBlittableWithPointer(type);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT ValidateReflectionTypeComponent(const Reflection::ReflectionType* type)
	{
		ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(type);
		if (component_type != ECS_COMPONENT_TYPE_COUNT) {
			// Verify that it has the ID function
			if (type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) == DBL_MAX) {
				return ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION;
			}

			// Now check the buffers
			bool has_buffers = HasReflectionTypeComponentBuffers(type);
			size_t allocator_size = GetReflectionComponentAllocatorSize(type);
			if (allocator_size == 0 && has_buffers) {
				return ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ALLOCATOR_SIZE_FUNCTION;
			}

			if (allocator_size != 0 && !has_buffers) {
				return ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NO_BUFFERS_BUT_ALLOCATOR_SIZE_FUNCTION;
			}
		}

		// Now check if it has the ECS_REFLECT_COMPONENT tag and is missing the is_shared function
		if (component_type == ECS_COMPONENT_TYPE_COUNT) {
			if (IsReflectionTypeMaybeComponent(type)) {
				return ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION;
			}
			else {
				return ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NOT_A_COMPONENT;
			}
		}

		return ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_VALID;
	}
	
	// ----------------------------------------------------------------------------------------------------------------------------

}