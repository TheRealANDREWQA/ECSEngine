#include "ecspch.h"
#include "AssetDatabaseSerializeFunctions.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../Utilities/Serialization/Binary/Serialization.h"
#include "../../Utilities/Serialization/SerializationHelpers.h"
#include "AssetDatabase.h"
#include "AssetMetadata.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	bool ReflectionContainerTypeMatch_ReferenceCountedAsset(Reflection::ReflectionContainerTypeMatchData* data)
	{
		return Reflection::ReflectionContainerTypeMatchTemplate(data, "ReferenceCountedAsset");
	}

	// --------------------------------------------------------------------------------------

	ulong2 ReflectionContainerTypeByteSize_ReferenceCountedAsset(Reflection::ReflectionContainerTypeByteSizeData* data)
	{
		Stream<char> template_type = Reflection::ReflectionContainerTypeGetTemplateArgument(data->definition);
		char previous_char = template_type[template_type.size];
		template_type[template_type.size] = '\0';

		Reflection::ReflectionType type;
		if (data->reflection_manager->TryGetType(template_type.buffer, type)) {
			// It is with 8 bytes bigger
			size_t byte_size = Reflection::GetReflectionTypeByteSize(&type) + 8;
			template_type[template_type.size] = previous_char;
			
			return { byte_size, alignof(Stream<char>) };
		}
		
		// Cannot determine right now - the type is not yet determined by the reflection manager
		template_type[template_type.size] = previous_char;
		return { 0, alignof(Stream<char>) };
	}

	// --------------------------------------------------------------------------------------

	void ReflectionContainerTypeDependentTypes_ReferenceCountedAsset(Reflection::ReflectionContainerTypeDependentTypesData* data)
	{
		const char* opened_bracket = strchr(data->definition.buffer, '<');
		const char* closed_bracket = strchr(data->definition.buffer, '>');
		ECS_ASSERT(opened_bracket != nullptr && closed_bracket != nullptr);

		Stream<char> type = { opened_bracket + 1, function::PointerDifference(closed_bracket, opened_bracket) - 1 };
	}

	// --------------------------------------------------------------------------------------

	size_t SerializeCustomTypeWriteFunction_ReferenceCountedAsset(SerializeCustomTypeWriteFunctionData* data)
	{
		// Determine if it is a trivial type - including streams
		Stream<char> template_type = Reflection::ReflectionContainerTypeGetTemplateArgument(data->definition);
		char previous_char = template_type[template_type.size];
		template_type[template_type.size] = '\0';

		const Reflection::ReflectionType* reflection_type = data->reflection_manager->GetType(template_type.buffer);
		template_type[template_type.size] = previous_char;

		if (data->write_data) {
			Serialize(data->reflection_manager, reflection_type, data->data, *data->stream);
		}
		else {
			return SerializeSize(data->reflection_manager, reflection_type, data->data);
		}

		return 0;
	}

	// --------------------------------------------------------------------------------------

	size_t SerializeCustomTypeReadFunction_ReferenceCountedAsset(SerializeCustomTypeReadFunctionData* data)
	{
		if (data->version != ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_ASSET_VERSION) {
			return -1;
		}

		Reflection::ReflectionBasicFieldType basic_type;
		Reflection::ReflectionStreamFieldType stream_type;
		unsigned int string_offset = 0;

		// Determine if it is a trivial type - including streams
		Stream<char> template_type = Reflection::ReflectionContainerTypeGetTemplateArgument(data->definition);
		char previous_char = template_type[template_type.size];
		template_type[template_type.size] = '\0';

		const Reflection::ReflectionType* reflection_type = data->reflection_manager->GetType(template_type.buffer);
		template_type[template_type.size] = previous_char;

		if (data->read_data) {
			ECS_DESERIALIZE_CODE code = Deserialize(data->reflection_manager, reflection_type, data->data, *data->stream);
			if (code != ECS_DESERIALIZE_OK) {
				return -1;
			}

			// Return 0 because no buffer data is needed
			return 0;
		}
		else {
			size_t byte_size = DeserializeSize(data->reflection_manager, reflection_type, *data->stream);
			if (byte_size == -1) {
				return -1;
			}

			// Return 0 because no buffer data is needed
			return 0;
		}

		// Return 0 because no buffer data is needed
		return 0;
	}

	// --------------------------------------------------------------------------------------

}