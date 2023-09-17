#include "ecspch.h"
#include "ComponentHelpers.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Allocators/ResizableLinearAllocator.h"

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

	ComponentBuffer GetReflectionTypeRuntimeBufferIndex(const ReflectionType* type, unsigned int buffer_index, unsigned int* field_index)
	{
		unsigned int index = 0;
		unsigned int current_buffer_index = 0;
		for (; index < type->fields.size && current_buffer_index <= buffer_index; index++) {
			if (IsStream(type->fields[index].info.stream_type) && type->fields[index].info.basic_type != ReflectionBasicFieldType::UserDefined) {
				current_buffer_index++;
			}
			else if (function::CompareStrings(type->fields[index].definition, STRING(DataPointer))) {
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
		if (function::CompareStrings(type->fields[index].definition, STRING(DataPointer))) {
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
			else if (function::CompareStrings(type->fields[index].definition, STRING(DataPointer))) {
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