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

	void GetConstantBufferInjectTagFieldsFromType(const Reflection::ReflectionType* type, CapacityStream<unsigned int>* fields)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			function::ForEach<true>(INJECT_TAGS, [&](const char* inject_tag) {
				if (type->fields[index].Has(inject_tag)) {
					fields->AddAssert(index);
					return true;
				}
				return false;
			});
		}
	}

}
