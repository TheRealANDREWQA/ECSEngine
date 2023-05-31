#include "ecspch.h"
#include "CBufferTags.h"
#include "../Utilities/Reflection/ReflectionTypes.h"
#include "../Utilities/Function.h"
#include "../Utilities/FunctionInterfaces.h"

const char* INJECT_TAGS[] = {
	STRING(ECS_INJECT_CAMERA_POSITION),
	STRING(ECS_INJECT_MVP_MATRIX),
	STRING(ECS_INJECT_OBJECT_MATRIX)
};

namespace ECSEngine {

	void GetConstantBufferInjectTagFieldsFromTypeTag(const Reflection::ReflectionType* type, CapacityStream<unsigned int>* fields)
	{
		if (type->tag.size > 0) {
			function::ForEach(INJECT_TAGS, [&](const char* tag) {
				Stream<char> existing_tag = type->GetTag(tag, ECS_SHADER_REFLECTION_CONSTANT_BUFFER_TAG_DELIMITER);
				if (existing_tag.size > 0) {
					Stream<char> parentheses = function::GetStringParameter(existing_tag);
					unsigned short byte_offset = (unsigned short)function::ConvertCharactersToInt(parentheses);
					for (size_t index = 0; index < type->fields.size; index++) {
						if (type->fields[index].info.pointer_offset == byte_offset) {
							fields->AddAssert(index);
						}
					}
				}
			});
		}
	}

}
