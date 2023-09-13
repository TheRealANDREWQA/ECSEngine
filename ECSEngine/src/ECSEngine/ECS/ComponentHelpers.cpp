#include "ecspch.h"
#include "ComponentHelpers.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	using namespace Reflection;

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeComponent(const ReflectionType* type)
	{
		return type->IsTag(ECS_COMPONENT_TAG);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeSharedComponent(const ReflectionType* type)
	{
		return type->IsTag(ECS_SHARED_COMPONENT_TAG);
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
		CapacityStream<unsigned int>* shared_indices
	) 
	{
		ECS_STACK_CAPACITY_STREAM(CapacityStream<unsigned int>, stream_of_indices, 2);
		stream_of_indices[0] = *unique_indices;
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

		reflection_manager->GetHierarchyTypes(query_options, hierarchy_index);

		unique_indices->size = stream_of_indices[0].size;
		shared_indices->size = stream_of_indices[1].size;
	}

	// ----------------------------------------------------------------------------------------------------------------------------
}