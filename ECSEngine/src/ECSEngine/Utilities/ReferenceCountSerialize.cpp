#include "ecspch.h"
#include "ReferenceCountSerialize.h"
#include "Reflection/Reflection.h"
#include "Serialization/Binary/Serialization.h"
#include "Serialization/SerializationHelpers.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(ReferenceCounted) {
		return Reflection::ReflectionCustomTypeMatchTemplate(data, "ReferenceCounted");
	}

	// --------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(ReferenceCounted) {
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);

		ulong2 byte_size_alignment = Reflection::SearchReflectionUserDefinedTypeByteSizeAlignment(data->reflection_manager, template_type);
		if (byte_size_alignment.x == -1) {
			// Cannot determine right now - the type is not yet determined by the reflection manager
			return { 0, alignof(Stream<char>) };
		}

		// If the alignment is less than 4, that of the reference count, clamp it to it
		byte_size_alignment.y = std::max(alignof(unsigned int), byte_size_alignment.y);
		return { byte_size_alignment.x + byte_size_alignment.y, byte_size_alignment.y };
	}

	// --------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(ReferenceCounted) {
		ReflectionCustomTypeDependentTypes_SingleTemplate(data);
	}

	// --------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_IS_BLITTABLE_FUNCTION(ReferenceCounted) {
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);
		return SearchIsBlittable(data->reflection_manager, template_type);
	}

	// --------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_COPY_FUNCTION(ReferenceCounted) {
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);
		CopyReflectionType(data->reflection_manager, template_type, data->source, data->destination, data->allocator);

		// Also copy the reference count
		size_t byte_size = Reflection::SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);
		ECS_ASSERT(byte_size != -1);

		unsigned int* source_value = (unsigned int*)function::OffsetPointer(data->source, byte_size);
		unsigned int* destination_value = (unsigned int*)function::OffsetPointer(data->destination, byte_size);
		*destination_value = *source_value;
	}

	// --------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_COMPARE_FUNCTION(ReferenceCounted) {
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);
		if (!Reflection::CompareReflectionTypeInstances(data->reflection_manager, template_type, data->first, data->second, 1)) {
			return false;
		}

		size_t byte_size = Reflection::SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);
		ECS_ASSERT(byte_size != -1);
		// Now compare the reference count
		unsigned int* first_reference_count = (unsigned int*)function::OffsetPointer(data->first, byte_size);
		unsigned int* second_reference_count = (unsigned int*)function::OffsetPointer(data->second, byte_size);
		return *first_reference_count == *second_reference_count;
	}

	// --------------------------------------------------------------------------------------
	
	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(ReferenceCounted) {
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);

		SerializeCustomWriteHelperExData ex_data;
		ex_data.data_to_write = { data->data, 1 };
		ex_data.template_type = template_type;
		ex_data.write_data = data;
		size_t written_size = SerializeCustomWriteHelperEx(&ex_data);

		// We need the helper to tell us the byte size of the original type
		// In order to write the reference count
		SerializeCustomTypeDeduceTypeHelperData helper_data;
		helper_data.reflection_manager = data->reflection_manager;
		helper_data.template_type = &template_type;

		SerializeCustomTypeDeduceTypeHelperResult result = SerializeCustomTypeDeduceTypeHelper(&helper_data);
		unsigned int* reference_count = (unsigned int*)function::OffsetPointer(data->data, result.byte_size);
		return written_size + Write(data->stream, reference_count, sizeof(unsigned int), data->write_data);
	}

	// --------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(ReferenceCounted) {
		if (data->version != ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_VERSION) {
			return -1;
		}

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);

		DeserializeCustomReadHelperExData ex_data;
		ex_data.data = data;
		ex_data.definition = template_type;
		ex_data.deserialize_target = data->data;
		ex_data.elements_to_allocate = 0;
		ex_data.element_count = 1;
		size_t buffer_size = DeserializeCustomReadHelperEx(&ex_data);

		// We need the byte size of the template type in order to read the reference count
		SerializeCustomTypeDeduceTypeHelperData helper_data;
		helper_data.reflection_manager = data->reflection_manager;
		helper_data.template_type = &template_type;

		SerializeCustomTypeDeduceTypeHelperResult result = SerializeCustomTypeDeduceTypeHelper(&helper_data);
		Read(data->stream, function::OffsetPointer(data->data, result.byte_size), sizeof(unsigned int), data->read_data);

		return buffer_size;
	}

	// --------------------------------------------------------------------------------------

}