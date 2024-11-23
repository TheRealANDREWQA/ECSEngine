#include "ecspch.h"
#include "ReferenceCountSerialize.h"
#include "Reflection/Reflection.h"
#include "Reflection/ReflectionCustomTypes.h"
#include "Serialization/Binary/Serialization.h"
#include "Serialization/SerializationHelpers.h"

namespace ECSEngine {

	using namespace Reflection;

	bool ReferenceCountedCustomTypeInterface::Match(Reflection::ReflectionCustomTypeMatchData* data)
	{
		return Reflection::ReflectionCustomTypeMatchTemplate(data, "ReferenceCounted");
	}

	ulong2 ReferenceCountedCustomTypeInterface::GetByteSize(Reflection::ReflectionCustomTypeByteSizeData* data)
	{
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);

		ulong2 byte_size_alignment = Reflection::SearchReflectionUserDefinedTypeByteSizeAlignment(data->reflection_manager, template_type);
		if (byte_size_alignment.x == -1) {
			// Cannot determine right now - the type is not yet determined by the reflection manager
			return { 0, alignof(Stream<char>) };
		}

		// If the alignment is less than 4, that of the reference count, clamp it to it
		byte_size_alignment.y = max(alignof(unsigned int), byte_size_alignment.y);
		return { byte_size_alignment.x + byte_size_alignment.y, byte_size_alignment.y };
	}

	void ReferenceCountedCustomTypeInterface::GetDependentTypes(Reflection::ReflectionCustomTypeDependentTypesData* data)
	{
		ReflectionCustomTypeDependentTypes_SingleTemplate(data);
	}

	bool ReferenceCountedCustomTypeInterface::IsBlittable(Reflection::ReflectionCustomTypeIsBlittableData* data)
	{
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);
		return SearchIsBlittable(data->reflection_manager, template_type);
	}

	void ReferenceCountedCustomTypeInterface::Copy(Reflection::ReflectionCustomTypeCopyData* data)
	{
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);
		CopyReflectionDataOptions copy_options;
		copy_options.allocator = data->allocator;
		copy_options.always_allocate_for_buffers = true;
		copy_options.deallocate_existing_buffers = data->deallocate_existing_data;

		CopyReflectionTypeInstance(data->reflection_manager, template_type, data->source, data->destination, &copy_options);

		// Also copy the reference count
		size_t byte_size = Reflection::SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);
		ECS_ASSERT(byte_size != -1);

		unsigned int* source_value = (unsigned int*)OffsetPointer(data->source, byte_size);
		unsigned int* destination_value = (unsigned int*)OffsetPointer(data->destination, byte_size);
		*destination_value = *source_value;
	}

	bool ReferenceCountedCustomTypeInterface::Compare(Reflection::ReflectionCustomTypeCompareData* data)
	{
		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
		if (!Reflection::CompareReflectionTypeInstances(data->reflection_manager, template_type, data->first, data->second, 1)) {
			return false;
		}

		size_t byte_size = Reflection::SearchReflectionUserDefinedTypeByteSize(data->reflection_manager, template_type);
		ECS_ASSERT(byte_size != -1);
		// Now compare the reference count
		unsigned int* first_reference_count = (unsigned int*)OffsetPointer(data->first, byte_size);
		unsigned int* second_reference_count = (unsigned int*)OffsetPointer(data->second, byte_size);
		return *first_reference_count == *second_reference_count;
	}

	void ReferenceCountedCustomTypeInterface::Deallocate(Reflection::ReflectionCustomTypeDeallocateData* data) {
		Stream<char> template_type = ReflectionCustomTypeGetTemplateArgument(data->definition);
		DeallocateReflectionInstanceBuffers(data->reflection_manager, template_type, data->source, data->allocator, data->element_count, data->element_byte_size, data->reset_buffers);
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
		unsigned int* reference_count = (unsigned int*)OffsetPointer(data->data, result.byte_size);
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
		Read(data->stream, OffsetPointer(data->data, result.byte_size), sizeof(unsigned int), data->read_data);

		return buffer_size;
	}

	// --------------------------------------------------------------------------------------

}