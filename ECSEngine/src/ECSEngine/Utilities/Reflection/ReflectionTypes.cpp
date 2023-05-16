#include "ecspch.h"
#include "ReflectionTypes.h"
#include "../../Utilities/Function.h"

namespace ECSEngine {

    namespace Reflection {

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t REFLECTION_BASIC_FIELD_TYPE_BYTE_SIZE[] = {
			sizeof(int8_t),
			sizeof(uint8_t),
			sizeof(int16_t),
			sizeof(uint16_t),
			sizeof(int32_t),
			sizeof(uint32_t),
			sizeof(int64_t),
			sizeof(uint64_t),
			sizeof(wchar_t),
			sizeof(float),
			sizeof(double),
			sizeof(bool),
			sizeof(unsigned char),
			sizeof(char2),
			sizeof(uchar2),
			sizeof(short2),
			sizeof(ushort2),
			sizeof(int2),
			sizeof(uint2),
			sizeof(long2),
			sizeof(ulong2),
			sizeof(char3),
			sizeof(uchar3),
			sizeof(short3),
			sizeof(ushort3),
			sizeof(int3),
			sizeof(uint3),
			sizeof(long3),
			sizeof(ulong3),
			sizeof(char4),
			sizeof(uchar4),
			sizeof(short4),
			sizeof(ushort4),
			sizeof(int4),
			sizeof(uint4),
			sizeof(long4),
			sizeof(ulong4),
			sizeof(float2),
			sizeof(float3),
			sizeof(float4),
			sizeof(double2),
			sizeof(double3),
			sizeof(double4),
			sizeof(bool2),
			sizeof(bool3),
			sizeof(bool4),
			0,
			0
		};

		size_t GetReflectionBasicFieldTypeByteSize(ReflectionBasicFieldType type)
		{
			return REFLECTION_BASIC_FIELD_TYPE_BYTE_SIZE[(unsigned int)type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

        bool ReflectionField::Has(Stream<char> string) const
        {
			if (tag.size > 0) {
				return function::FindFirstToken(tag, string).buffer != nullptr;
			}
			return false;
        }

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionField::Is(Stream<char> string) const
		{
			return function::CompareStrings(tag, string);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> ReflectionField::GetTag(Stream<char> string) const
		{
			Stream<char> token = function::FindFirstToken(tag, string);
			if (token.size == 0) {
				return token;
			}

			// Find the separation character
			Stream<char> separator = function::FindFirstCharacter(token, '~');
			if (separator.size == 0) {
				return token;
			}

			return { token.buffer, token.size - (separator.buffer - token.buffer) };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField ReflectionField::Copy(AllocatorPolymorphic allocator) const
		{
			ReflectionField field;

			field.name = function::StringCopy(allocator, name);
			field.definition = function::StringCopy(allocator, definition);
			if (field.tag.size > 0) {
				field.tag = function::StringCopy(allocator, tag);
			}
			else {
				field.tag = { nullptr, 0 };
			}
			field.info = info;

			return field;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField ReflectionField::CopyTo(uintptr_t& ptr) const {
			ReflectionField copy;

			copy.name.InitializeAndCopy(ptr, name);
			copy.definition.InitializeAndCopy(ptr, definition);
			copy.tag.InitializeAndCopy(ptr, tag);
			copy.info = info;

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionField::DeallocateSeparate(AllocatorPolymorphic allocator) const
		{
			Deallocate(allocator, name.buffer);
			Deallocate(allocator, definition.buffer);
			if (tag.size > 0) {
				Deallocate(allocator, tag.buffer);
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ReflectionField::CopySize() const {
			return name.CopySize() + definition.CopySize() + tag.CopySize();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionField::Compare(const ReflectionField& field) const
		{
			return function::CompareStrings(field.definition, definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEvaluation ReflectionEvaluation::CopyTo(uintptr_t& ptr) const {
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

		const char* ECS_BASIC_FIELD_TYPE_STRING[] = {
			STRING(int8_t),
			STRING(uint8_t),
			STRING(int16_t),
			STRING(uint16_t),
			STRING(int32_t),
			STRING(uint32_t),
			STRING(int64_t),
			STRING(uint64_t),
			STRING(wchar_t),
			STRING(float),
			STRING(double),
			STRING(bool),
			// enum
			"",
			STRING(char2),
			STRING(uchar2),
			STRING(short2),
			STRING(ushort2),
			STRING(int2),
			STRING(uint2),
			STRING(long2),
			STRING(ulong2),
			STRING(char3),
			STRING(uchar3),
			STRING(short3),
			STRING(ushort3),
			STRING(int3),
			STRING(uint3),
			STRING(long3),
			STRING(ulong3),
			STRING(char4),
			STRING(uchar4),
			STRING(short4),
			STRING(ushort4),
			STRING(int4),
			STRING(uint4),
			STRING(long4),
			STRING(ulong4),
			STRING(float2),
			STRING(float3),
			STRING(float4),
			STRING(double2),
			STRING(double3),
			STRING(double4),
			STRING(bool2),
			STRING(bool3),
			STRING(bool4),
			// User defined
			"",
			// Unknown
			""
		};

		static_assert(std::size(ECS_BASIC_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionBasicFieldType::COUNT);
		static_assert(std::size(ECS_STREAM_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionStreamFieldType::COUNT);
		static_assert(std::size(ECS_BASIC_FIELD_TYPE_STRING) == (unsigned int)ReflectionBasicFieldType::COUNT);

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetFieldTypeAlignment(ReflectionBasicFieldType field_type) {
			return ECS_BASIC_FIELD_TYPE_ALIGNMENT[(unsigned int)field_type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetFieldTypeAlignment(ReflectionStreamFieldType stream_type) {
			return ECS_STREAM_FIELD_TYPE_ALIGNMENT[(unsigned int)stream_type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetFieldTypeAlignment(const ReflectionFieldInfo* info)
		{
			return info->stream_type == ReflectionStreamFieldType::Basic || info->stream_type == ReflectionStreamFieldType::BasicTypeArray ?
				GetFieldTypeAlignment(info->basic_type) : GetFieldTypeAlignment(info->stream_type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> GetBasicFieldDefinition(ReflectionBasicFieldType basic_type)
		{
			return ECS_BASIC_FIELD_TYPE_STRING[(unsigned int)basic_type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		template<typename Type>
		ECS_INLINE void ConvertFromDouble(double* values, unsigned int count, void* converted_values) {
			Type* type_ptr = (Type*)converted_values;
			for (unsigned int index = 0; index < count; index++) {
				type_ptr[index] = (Type)values[index];
			}
		}

		void ConvertFromDouble4ToBasic(ReflectionBasicFieldType basic_type, double4 values, void* converted_values)
		{
			double* double_values = (double*)&values;

#define CASE(type) case ReflectionBasicFieldType::type
#define CASE4(type) CASE(type): CASE(type##2): CASE(type##3): CASE(type##4):
#define CASE4_INT32(type) CASE(type##32): CASE(type##2): CASE(type##3): CASE(type##4):
#define CASE4_INT_BASE(base_int, other_int) CASE(base_int): CASE(other_int##2): CASE(other_int##3): CASE(other_int##4):

			unsigned short basic_type_count = BasicTypeComponentCount(basic_type);
			switch (basic_type) {
				CASE4(Bool)
				{
					ConvertFromDouble<bool>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4(Float) 
				{
					ConvertFromDouble<float>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4(Double)
				{
					memcpy(converted_values, double_values, sizeof(double) * basic_type_count);
				}
				break;
				CASE4_INT32(Int) 
				{
					ConvertFromDouble<int>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4_INT32(UInt)
				{
					ConvertFromDouble<unsigned int>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4_INT_BASE(Int8, Char)
				{
					ConvertFromDouble<char>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4_INT_BASE(Int16, Short)
				{
					ConvertFromDouble<short>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4_INT_BASE(Int64, Long)
				{
					ConvertFromDouble<long long>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4_INT_BASE(UInt8, UChar)
				{
					ConvertFromDouble<unsigned char>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4_INT_BASE(UInt16, UShort)
				{
					ConvertFromDouble<unsigned short>(double_values, basic_type_count, converted_values);
				}
				break;
				CASE4_INT_BASE(UInt64, ULong)
				{
					ConvertFromDouble<unsigned long long>(double_values, basic_type_count, converted_values);
				}
				break;
				// Ignore all the other cases
			}

#undef CASE
#undef CASE4
#undef CASE4_INT32
#undef CASE4_INT_BASE
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegral(ReflectionBasicFieldType type)
		{
			// Use the negation - if it is different than all the other types
			return type != ReflectionBasicFieldType::Bool && type != ReflectionBasicFieldType::Bool2 && type != ReflectionBasicFieldType::Bool3 &&
				type != ReflectionBasicFieldType::Bool4 && type != ReflectionBasicFieldType::Double && type != ReflectionBasicFieldType::Double2 &&
				type != ReflectionBasicFieldType::Double3 && type != ReflectionBasicFieldType::Double4 && type != ReflectionBasicFieldType::Float &&
				type != ReflectionBasicFieldType::Float2 && type != ReflectionBasicFieldType::Float3 && type != ReflectionBasicFieldType::Float4 &&
				type != ReflectionBasicFieldType::Enum && type != ReflectionBasicFieldType::UserDefined && type != ReflectionBasicFieldType::Unknown &&
				type != ReflectionBasicFieldType::Wchar_t;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegralSingleComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Int8 || type == ReflectionBasicFieldType::UInt8 ||
				type == ReflectionBasicFieldType::Int16 || type == ReflectionBasicFieldType::UInt16 ||
				type == ReflectionBasicFieldType::Int32 || type == ReflectionBasicFieldType::UInt32 ||
				type == ReflectionBasicFieldType::Int64 || type == ReflectionBasicFieldType::UInt64;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegralMultiComponent(ReflectionBasicFieldType type)
		{
			return IsIntegral(type) && !IsIntegralSingleComponent(type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned char BasicTypeComponentCount(ReflectionBasicFieldType type)
		{
			if (type == ReflectionBasicFieldType::Bool || type == ReflectionBasicFieldType::Double || type == ReflectionBasicFieldType::Enum ||
				type == ReflectionBasicFieldType::Float || type == ReflectionBasicFieldType::Int8 || type == ReflectionBasicFieldType::UInt8 ||
				type == ReflectionBasicFieldType::Int16 || type == ReflectionBasicFieldType::UInt16 || type == ReflectionBasicFieldType::Int32 ||
				type == ReflectionBasicFieldType::UInt32 || type == ReflectionBasicFieldType::Int64 || type == ReflectionBasicFieldType::UInt64 ||
				type == ReflectionBasicFieldType::Wchar_t) {
				return 1;
			}
			else if (type == ReflectionBasicFieldType::Bool2 || type == ReflectionBasicFieldType::Double2 || type == ReflectionBasicFieldType::Float2 ||
				type == ReflectionBasicFieldType::Char2 || type == ReflectionBasicFieldType::UChar2 || type == ReflectionBasicFieldType::Short2 ||
				type == ReflectionBasicFieldType::UShort2 || type == ReflectionBasicFieldType::Int2 || type == ReflectionBasicFieldType::UInt2 ||
				type == ReflectionBasicFieldType::Long2 || type == ReflectionBasicFieldType::ULong2) {
				return 2;
			}
			else if (type == ReflectionBasicFieldType::Bool3 || type == ReflectionBasicFieldType::Double3 || type == ReflectionBasicFieldType::Float3 ||
				type == ReflectionBasicFieldType::Char3 || type == ReflectionBasicFieldType::UChar3 || type == ReflectionBasicFieldType::Short3 ||
				type == ReflectionBasicFieldType::UShort3 || type == ReflectionBasicFieldType::Int3 || type == ReflectionBasicFieldType::UInt3 ||
				type == ReflectionBasicFieldType::Long3 || type == ReflectionBasicFieldType::ULong3) {
				return 3;
			}
			else if (type == ReflectionBasicFieldType::Bool4 || type == ReflectionBasicFieldType::Double4 || type == ReflectionBasicFieldType::Float4 ||
				type == ReflectionBasicFieldType::Char4 || type == ReflectionBasicFieldType::UChar4 || type == ReflectionBasicFieldType::Short4 ||
				type == ReflectionBasicFieldType::UShort4 || type == ReflectionBasicFieldType::Int4 || type == ReflectionBasicFieldType::UInt4 ||
				type == ReflectionBasicFieldType::Long4 || type == ReflectionBasicFieldType::ULong4) {
				return 4;
			}
			else {
				return 0;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionBasicFieldType ReduceMultiComponentReflectionType(ReflectionBasicFieldType type) {
#define CASE234(string, value)	case ReflectionBasicFieldType::string##2: \
								case ReflectionBasicFieldType::string##3: \
								case ReflectionBasicFieldType::string##4: \
									return ReflectionBasicFieldType::value;

			switch (type) {
				CASE234(Bool, Bool);
				CASE234(Float, Float);
				CASE234(Double, Double);
				CASE234(Char, Int8);
				CASE234(UChar, UInt8);
				CASE234(Short, Int16);
				CASE234(UShort, UInt16);
				CASE234(Int, Int32);
				CASE234(UInt, UInt32);
				CASE234(Long, Int64);
				CASE234(ULong, UInt64);
			}

			return type;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionType::DeallocateCoallesced(AllocatorPolymorphic allocator) const
		{
			ECSEngine::Deallocate(allocator, name.buffer);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionType::Deallocate(AllocatorPolymorphic allocator) const
		{
			for (size_t index = 0; index < fields.size; index++) {
				DeallocateIfBelongs(allocator, fields[index].name.buffer);
				DeallocateIfBelongs(allocator, fields[index].definition.buffer);
				if (fields[index].tag.size > 0) {
					DeallocateIfBelongs(allocator, fields[index].tag.buffer);
				}
			}
			for (size_t index = 0; index < evaluations.size; index++) {
				DeallocateIfBelongs(allocator, evaluations[index].name.buffer);
			}
			
			if (tag.size > 0) {
				DeallocateIfBelongs(allocator, tag.buffer);
			}
			DeallocateIfBelongs(allocator, name.buffer);
			DeallocateIfBelongs(allocator, fields.buffer);
			DeallocateIfBelongs(allocator, evaluations.buffer);
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

		unsigned int ReflectionType::FindField(Stream<char> name) const
		{
			for (size_t index = 0; index < fields.size; index++) {
				if (function::CompareStrings(name, fields[index].name)) {
					return index;
				}
			}
			return -1;
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

		ReflectionType ReflectionType::Copy(AllocatorPolymorphic allocator) const
		{
			ReflectionType copy;

			copy.name.InitializeAndCopy(allocator, name);
			copy.fields.InitializeAndCopy(allocator, fields);
			copy.evaluations.InitializeAndCopy(allocator, evaluations);
			copy.tag.InitializeAndCopy(allocator, tag);
			copy.byte_size = byte_size;
			copy.alignment = alignment;
			copy.folder_hierarchy_index = folder_hierarchy_index;
			copy.is_blittable = is_blittable;
			copy.is_blittable_with_pointer = is_blittable_with_pointer;
			for (size_t index = 0; index < fields.size; index++) {
				copy.fields[index].name.InitializeAndCopy(allocator, fields[index].name);
				copy.fields[index].tag.InitializeAndCopy(allocator, fields[index].tag);
				copy.fields[index].definition.InitializeAndCopy(allocator, fields[index].definition);
			}

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType ReflectionType::CopyCoallesced(AllocatorPolymorphic allocator) const
		{
			size_t copy_size = CopySize();
			void* allocation = Allocate(allocator, copy_size);
			uintptr_t allocation_ptr = (uintptr_t)allocation;
			return CopyTo(allocation_ptr);
		}

		// ----------------------------------------------------------------------------------------------------------------------------


		ReflectionType ReflectionType::CopyTo(uintptr_t& ptr) const
		{
			ReflectionType copy;

			copy.name.InitializeAndCopy(ptr, name);
			copy.fields.InitializeFromBuffer(ptr, fields.size);
			for (size_t index = 0; index < fields.size; index++) {
				copy.fields[index].info = fields[index].info;
				copy.fields[index].name.InitializeAndCopy(ptr, fields[index].name);

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
			copy.is_blittable = is_blittable;
			copy.is_blittable_with_pointer = is_blittable_with_pointer;

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ReflectionType::CopySize() const
		{
			return name.CopySize() + tag.CopySize() + StreamCoallescedDeepCopySize(fields) + StreamCoallescedDeepCopySize(evaluations);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionEnum::Deallocate(AllocatorPolymorphic allocator) const
		{
			for (size_t index = 0; index < fields.size; index++) {
				DeallocateIfBelongs(allocator, fields[index].buffer);
			}
			DeallocateIfBelongs(allocator, fields.buffer);
			DeallocateIfBelongs(allocator, name.buffer);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum ReflectionEnum::CopyTo(uintptr_t& ptr) const
		{
			ReflectionEnum copy;

			copy.folder_hierarchy_index = folder_hierarchy_index;
			copy.name.InitializeAndCopy(ptr, name);
			copy.fields = StreamCoallescedDeepCopy(fields, ptr);

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionConstant ReflectionConstant::CopyTo(uintptr_t& ptr) const
		{
			ReflectionConstant constant;
			
			constant.name.InitializeAndCopy(ptr, name);
			constant.folder_hierarchy = folder_hierarchy;
			constant.value = value;

			return constant;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ReflectionConstant::CopySize() const
		{
			return name.MemoryOf(name.size);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}