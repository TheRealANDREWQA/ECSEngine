#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "Reflection/ReflectionTypes.h"
#include "Serialization/SerializationHelpers.h"

namespace ECSEngine {

	struct ReferenceCountedCustomTypeInterface : public Reflection::ReflectionCustomTypeInterface {
		bool Match(Reflection::ReflectionCustomTypeMatchData* data) override;

		ulong2 GetByteSize(Reflection::ReflectionCustomTypeByteSizeData* data) override;

		void GetDependentTypes(Reflection::ReflectionCustomTypeDependentTypesData* data) override;

		bool IsBlittable(Reflection::ReflectionCustomTypeIsBlittableData* data) override;

		void Copy(Reflection::ReflectionCustomTypeCopyData* data) override;

		bool Compare(Reflection::ReflectionCustomTypeCompareData* data) override;

		void Deallocate(Reflection::ReflectionCustomTypeDeallocateData* data) override;
	};

	ECS_SERIALIZE_CUSTOM_TYPE_FUNCTION_HEADER(ReferenceCounted);

#define ECS_SERIALIZE_CUSTOM_TYPE_REFERENCE_COUNTED_VERSION (0)

}