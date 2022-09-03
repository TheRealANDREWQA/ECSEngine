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

	ECS_SERIALIZE_CUSTOM_TYPE_IS_TRIVIALLY_COPYABLE_FUNCTION(ReferenceCounted) {
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);
		return IsTriviallyCopyable(data->reflection_manager, template_type);
	}

	// --------------------------------------------------------------------------------------
	
	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(ReferenceCounted) {
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);
		const Reflection::ReflectionType* reflection_type = data->reflection_manager->GetType(template_type);
		if (data->write_data) {
			Serialize(data->reflection_manager, reflection_type, data->data, *data->stream);
		}
		else {
			return SerializeSize(data->reflection_manager, reflection_type, data->data);
		}

		return 0;
	}

	// --------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(ReferenceCounted) {
		if (data->version != ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_VERSION) {
			return -1;
		}

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = Reflection::ReflectionCustomTypeGetTemplateArgument(data->definition);

		const Reflection::ReflectionType* reflection_type = data->reflection_manager->GetType(template_type);
		if (data->read_data) {
			DeserializeOptions options;
			ECS_DESERIALIZE_CODE code = Deserialize(data->reflection_manager, reflection_type, data->data, *data->stream);
			if (code != ECS_DESERIALIZE_OK) {
				return -1;
			}

			// Return 0 because no buffer we don't need to know the data size
			return 0;
		}
		else {
			size_t buffer_size = DeserializeSize(data->reflection_manager, reflection_type, *data->stream);
			return buffer_size;
		}

		return 0;
	}

	// --------------------------------------------------------------------------------------

}