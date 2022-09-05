#include "ecspch.h"
#include "ReflectionTypes.h"
#include "../../Utilities/Function.h"

namespace ECSEngine {

    namespace Reflection {

		// ----------------------------------------------------------------------------------------------------------------------------

        bool ReflectionField::Skip(Stream<char> string) const
        {
			if (tag.size > 0) {
				return function::FindFirstToken(tag, string).buffer != nullptr;
			}
			return false;
        }

		bool ReflectionField::Is(Stream<char> string) const
		{
			return function::CompareStrings(tag, string);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField ReflectionField::Copy(uintptr_t& ptr) const {
			ReflectionField copy;

			copy.name.InitializeAndCopy(ptr, name);
			copy.definition.InitializeAndCopy(ptr, definition);
			copy.tag.InitializeAndCopy(ptr, tag);
			copy.info = info;

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ReflectionField::CopySize() const {
			return name.CopySize() + definition.CopySize() + tag.CopySize();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEvaluation ReflectionEvaluation::Copy(uintptr_t& ptr) const {
			ReflectionEvaluation copy;

			copy.name.InitializeAndCopy(ptr, name);
			copy.value = value;

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ReflectionEvaluation::CopySize() const {
			return name.CopySize();
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

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionType::HasTag(Stream<char> string) const
		{
			if (tag.size == 0) {
				return false;
			}
			return function::FindFirstToken(tag, string).buffer != nullptr;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionType::IsTag(Stream<char> string) const
		{
			return function::CompareStrings(string, tag);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		double ReflectionType::GetEvaluation(Stream<char> name) const
		{
			for (size_t index = 0; index < evaluations.size; index++) {
				if (function::CompareStrings(evaluations[index].name, name)) {
					return evaluations[index].value;
				}
			}

			return DBL_MAX;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType ReflectionType::Copy(uintptr_t& ptr) const
		{
			ReflectionType copy;

			copy.name.InitializeAndCopy(ptr, name);
			copy.fields.InitializeFromBuffer(ptr, fields.size);
			for (size_t index = 0; index < fields.size; index++) {
				copy.fields[index].info = fields[index].info;
				copy.fields[index].name.InitializeAndCopy(ptr, fields[index].name);

				if (fields[index].tag.size > 0) {
					copy.fields[index].tag.InitializeAndCopy(ptr, fields[index].tag);
				}
				else {
					copy.fields[index].tag = { nullptr, 0 };
				}

				ReflectionStreamFieldType stream_type = fields[index].info.stream_type;
				ReflectionBasicFieldType basic_type = fields[index].info.basic_type;
				if (basic_type == ReflectionBasicFieldType::UserDefined || stream_type == ReflectionStreamFieldType::Stream ||
					stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream ||
					basic_type == ReflectionBasicFieldType::Enum) 
				{
					copy.fields[index].definition.InitializeAndCopy(ptr, fields[index].definition);
				}
				else {
					copy.fields[index].definition = fields[index].definition;
				}

				// Verify field tag
				Stream<char> tag = fields[index].tag;
				if (tag.size > 0) {
					copy.fields[index].tag.InitializeAndCopy(ptr, tag);
				}
				else {
					copy.fields[index].tag = { nullptr, 0 };
				}
			}

			if (tag.size > 0) {
				copy.tag.InitializeAndCopy(ptr, tag);
			}
			else {
				copy.tag = { nullptr, 0 };
			}
			copy.byte_size = byte_size;
			copy.alignment = alignment;

			if (evaluations.size > 0) {
				copy.evaluations.InitializeFromBuffer(ptr, evaluations.size);
				for (size_t index = 0; index < evaluations.size; index++) {
					copy.evaluations[index].value = evaluations[index].value;
					copy.evaluations[index].name.InitializeAndCopy(ptr, evaluations[index].name);
				}
			}
			else {
				copy.evaluations = { nullptr, 0 };
			}
			copy.folder_hierarchy_index = folder_hierarchy_index;

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ReflectionType::CopySize() const
		{
			return name.CopySize() + tag.CopySize() + StreamDeepCopySize(fields) + StreamDeepCopySize(evaluations);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum ReflectionEnum::Copy(uintptr_t& ptr) const
		{
			ReflectionEnum copy;

			copy.folder_hierarchy_index = folder_hierarchy_index;
			copy.name.InitializeAndCopy(ptr, name);
			copy.fields = StreamDeepCopy(fields, ptr);

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
