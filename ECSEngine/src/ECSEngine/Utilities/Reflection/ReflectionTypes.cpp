#include "ecspch.h"
#include "ReflectionTypes.h"

namespace ECSEngine {

    namespace Reflection {

        bool ReflectionField::Skip(const char* string) const
        {
			if (tag != nullptr) {
				const char* string_to_search = string == nullptr ? STRING(ECS_OMIT_FIELD_REFLECT) : string;

				return strstr(tag, string_to_search) != nullptr;
			}
			return false;
        }

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned char ECS_BASIC_FIELD_TYPE_ALIGNMENT[] = {
			alignof(int8_t),
			alignof(uint8_t),
			alignof(int16_t),
			alignof(uint16_t),
			alignof(int32_t),
			alignof(uint32_t),
			alignof(int64_t),
			alignof(uint64_t),
			alignof(wchar_t),
			alignof(float),
			alignof(double),
			alignof(bool),
			// enum
			alignof(unsigned char),
			alignof(char2),
			alignof(uchar2),
			alignof(short2),
			alignof(ushort2),
			alignof(int2),
			alignof(uint2),
			alignof(long2),
			alignof(ulong2),
			alignof(char3),
			alignof(uchar3),
			alignof(short3),
			alignof(ushort3),
			alignof(int3),
			alignof(uint3),
			alignof(long3),
			alignof(ulong3),
			alignof(char4),
			alignof(uchar4),
			alignof(short4),
			alignof(ushort4),
			alignof(int4),
			alignof(uint4),
			alignof(long4),
			alignof(ulong4),
			alignof(float2),
			alignof(float3),
			alignof(float4),
			alignof(double2),
			alignof(double3),
			alignof(double4),
			alignof(bool2),
			alignof(bool3),
			alignof(bool4),
			// User defined
			alignof(void*),
			// Unknown
			alignof(void*)
		};

		unsigned char ECS_STREAM_FIELD_TYPE_ALIGNMENT[] = {
			// Basic - should not be accessed
			alignof(void*),
			alignof(void*),
			// BasicTypeArray - should not be accessed
			alignof(void*),
			alignof(Stream<void>),
			alignof(CapacityStream<void>),
			alignof(ResizableStream<char>),
			// Unknown
			8
		};

		static_assert(std::size(ECS_BASIC_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionBasicFieldType::COUNT);
		static_assert(std::size(ECS_STREAM_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionStreamFieldType::COUNT);

		size_t GetFieldTypeAlignment(ReflectionBasicFieldType field_type) {
			return ECS_BASIC_FIELD_TYPE_ALIGNMENT[(unsigned int)field_type];
		}

		size_t GetFieldTypeAlignment(ReflectionStreamFieldType stream_type) {
			return ECS_STREAM_FIELD_TYPE_ALIGNMENT[(unsigned int)stream_type];
		}

		bool ReflectionType::HasTag(const char* string) const
		{
			if (tag == nullptr) {
				return false;
			}
			return strstr(tag, string);
		}

	}

}
