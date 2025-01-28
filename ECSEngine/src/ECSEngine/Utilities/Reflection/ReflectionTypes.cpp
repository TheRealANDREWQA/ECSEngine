#include "ecspch.h"
#include "ReflectionTypes.h"
#include "Reflection.h"
#include "../StringUtilities.h"
#include "../Tokenize.h"

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

		ReflectionField ReflectionField::Copy(AllocatorPolymorphic allocator) const
		{
			ReflectionField field;

			field.name = StringCopy(allocator, name);
			field.definition = StringCopy(allocator, definition);
			if (field.tag.size > 0) {
				field.tag = StringCopy(allocator, tag);
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

		ReflectionEvaluation ReflectionEvaluation::CopyTo(uintptr_t& ptr) const {
			ReflectionEvaluation copy;

			copy.name.InitializeAndCopy(ptr, name);
			copy.value = value;

			return copy;
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
			// PointerSoA,
			alignof(void*),
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

		static_assert(ECS_COUNTOF(ECS_BASIC_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionBasicFieldType::COUNT);
		static_assert(ECS_COUNTOF(ECS_STREAM_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionStreamFieldType::COUNT);
		static_assert(ECS_COUNTOF(ECS_BASIC_FIELD_TYPE_STRING) == (unsigned int)ReflectionBasicFieldType::COUNT);

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldTypeAlignment(ReflectionBasicFieldType field_type) {
			return ECS_BASIC_FIELD_TYPE_ALIGNMENT[(unsigned int)field_type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldTypeAlignment(ReflectionStreamFieldType stream_type) {
			return ECS_STREAM_FIELD_TYPE_ALIGNMENT[(unsigned int)stream_type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldTypeAlignment(const ReflectionFieldInfo* info)
		{
			return info->stream_type == ReflectionStreamFieldType::Basic || info->stream_type == ReflectionStreamFieldType::BasicTypeArray ?
				GetReflectionFieldTypeAlignment(info->basic_type) : GetReflectionFieldTypeAlignment(info->stream_type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsReflectionFieldTypeBlittable(const ReflectionFieldInfo* info) {
			return info->stream_type == ReflectionStreamFieldType::Basic || info->stream_type == ReflectionStreamFieldType::BasicTypeArray;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> GetBasicFieldDefinition(ReflectionBasicFieldType basic_type)
		{
			return ECS_BASIC_FIELD_TYPE_STRING[(unsigned int)basic_type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#define CASE(type) case ReflectionBasicFieldType::type
#define CASE4(type) CASE(type): CASE(type##2): CASE(type##3): CASE(type##4):
#define CASE4_INT32(type) CASE(type##32): CASE(type##2): CASE(type##3): CASE(type##4):
#define CASE4_INT_BASE(base_int, other_int) CASE(base_int): CASE(other_int##2): CASE(other_int##3): CASE(other_int##4):

		template<typename Type>
		ECS_INLINE void ConvertToDouble(double4* double_values, unsigned char count, const void* values) {
			const Type* typed_values = (const Type*)values;
			double* double_values_ptr = (double*)double_values;
			for (unsigned char index = 0; index < count; index++) {
				double_values_ptr[index] = (double)typed_values[index];
			}
		}

		double4 ConvertToDouble4FromBasic(ReflectionBasicFieldType basic_type, const void* values)
		{
			double4 value = double4::Splat(DBL_MAX);
			unsigned char basic_type_count = BasicTypeComponentCount(basic_type);

			switch (basic_type) {
			CASE4(Bool)
			{
				ConvertToDouble<bool>(&value, basic_type_count, values);
			}
			break;
			CASE4(Float) 
			{
				ConvertToDouble<float>(&value, basic_type_count, values);
			}
			break;
			CASE4(Double) 
			{
				ConvertToDouble<double>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT32(Int)
			{
				ConvertToDouble<int>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT32(UInt)
			{
				ConvertToDouble<unsigned int>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT_BASE(Int8, Char)
			{
				ConvertToDouble<char>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT_BASE(Int16, Short)
			{
				ConvertToDouble<short>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT_BASE(Int64, Long)
			{
				ConvertToDouble<long long>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT_BASE(UInt8, UChar)
			{
				ConvertToDouble<unsigned char>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT_BASE(UInt16, UShort)
			{
				ConvertToDouble<unsigned short>(&value, basic_type_count, values);
			}
			break;
			CASE4_INT_BASE(UInt64, ULong)
			{
				ConvertToDouble<unsigned long long>(&value, basic_type_count, values);
			}
			break;
			// Ignore all the other cases
			}

			return value;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		template<typename Type>
		ECS_INLINE void ConvertFromDouble(const double* values, unsigned char count, void* converted_values) {
			Type* type_ptr = (Type*)converted_values;
			for (unsigned char index = 0; index < count; index++) {
				type_ptr[index] = (Type)values[index];
			}
		}

		void ConvertFromDouble4ToBasic(ReflectionBasicFieldType basic_type, double4 values, void* converted_values)
		{
			const double* double_values = (double*)&values;

			unsigned char basic_type_count = BasicTypeComponentCount(basic_type);
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
		}

#undef CASE
#undef CASE4
#undef CASE4_INT32
#undef CASE4_INT_BASE

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ConvertToSizetFromBasic(ReflectionBasicFieldType basic_type, const void* value) {
			switch (basic_type) {
			case ReflectionBasicFieldType::UInt8:
			{
				return *(unsigned char*)value;
			}
			case ReflectionBasicFieldType::UInt16:
			{
				return *(unsigned short*)value;
			}
			case ReflectionBasicFieldType::UInt32:
			{
				return *(unsigned int*)value;
			}
			case ReflectionBasicFieldType::UInt64:
			{
				return *(size_t*)value;
			}
			default:
				ECS_ASSERT(false, "Invalid reflection basic type to convert to size_t");
			}

			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ConvertFromSizetToBasic(ReflectionBasicFieldType basic_type, size_t value, void* pointer_value) {
			switch (basic_type) {
			case ReflectionBasicFieldType::UInt8:
			{
				unsigned char* ptr = (unsigned char*)pointer_value;
				*ptr = value;
			}
			break;
			case ReflectionBasicFieldType::UInt16:
			{
				unsigned short* ptr = (unsigned short*)pointer_value;
				*ptr = value;
			}
			break;
			case ReflectionBasicFieldType::UInt32:
			{
				unsigned int* ptr = (unsigned int*)pointer_value;
				*ptr = value;
			}
			break;
			case ReflectionBasicFieldType::UInt64:
			{
				size_t* ptr = (size_t*)pointer_value;
				*ptr = value;
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid reflection basic type to write a size_t");
			}
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

		ECS_INT_TYPE BasicTypeToIntType(ReflectionBasicFieldType type)
		{
			switch (type) {
			case ReflectionBasicFieldType::Int8:
			case ReflectionBasicFieldType::UInt8:
				return ECS_INT8;
			case ReflectionBasicFieldType::Int16:
			case ReflectionBasicFieldType::UInt16:
				return ECS_INT16;
			case ReflectionBasicFieldType::Int32:
			case ReflectionBasicFieldType::UInt32:
				return ECS_INT32;
			case ReflectionBasicFieldType::Int64:
			case ReflectionBasicFieldType::UInt64:
				return ECS_INT64;
			}

			return ECS_INT_TYPE_COUNT;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionCustomTypeElementOptions(
			Stream<char> tag, 
			Stream<char> element_name, 
			CapacityStream<Stream<char>>& options, 
			CapacityStream<char>& stack_memory
		) {
			// These custom
			struct CustomElementMacro {
				Stream<char> macro;
				size_t field_count;
			};

			static CustomElementMacro TAGS_WITH_CUSTOM_ELEMENTS[] = {
				{ STRING(ECS_POINTER_AS_REFERENCE), 2 },
				{ STRING(ECS_POINTER_KEY_REFERENCE_TARGET), 2 }
			};

			for (size_t index = 0; index < ECS_COUNTOF(TAGS_WITH_CUSTOM_ELEMENTS); index++) {
				Stream<char> current_tag = GetReflectionFieldSeparatedTag(tag, TAGS_WITH_CUSTOM_ELEMENTS[index].macro);
				while (current_tag.size > 0) {
					Stream<char> current_tag_parameters = GetStringParameter(current_tag);
					ECS_STACK_CAPACITY_STREAM(Stream<char>, current_tag_splits, 8);
					SplitString(current_tag_parameters, ',', &current_tag_splits);
					size_t split_count = TAGS_WITH_CUSTOM_ELEMENTS[index].field_count;
					if (current_tag_splits.size == split_count) {
						if (current_tag_splits[split_count - 1].StartsWith(TAGS_WITH_CUSTOM_ELEMENTS[index].macro)) {
							// This macro matched
							unsigned int initial_stack_memory_size = stack_memory.size;
							stack_memory.AddStreamAssert(TAGS_WITH_CUSTOM_ELEMENTS[index].macro);
							stack_memory.AddAssert('(');
							for (size_t split_index = 0; split_index < split_count - 1; split_index++) {
								stack_memory.AddStreamAssert(current_tag_splits[split_index]);
								stack_memory.AddAssert(',');
							}
							// Remove the last comma, by reducing the size
							stack_memory.size--;
							stack_memory.AddAssert(')');
							options.AddAssert(stack_memory.SliceAt(initial_stack_memory_size));
							// Add a tag separator as well
							stack_memory.AddAssert(ECS_REFLECTION_TYPE_TAG_DELIMITER_CHAR);
						}
					}

					// Advance to the next tag
					current_tag = GetReflectionFieldSeparatedTag(tag.SliceAt(current_tag.buffer - tag.buffer + current_tag.size), TAGS_WITH_CUSTOM_ELEMENTS[index].macro);
				}
			}

			// Remove the last delimiter character, if there is one
			if (stack_memory.Last() == ECS_REFLECTION_TYPE_TAG_DELIMITER_CHAR) {
				stack_memory.size--;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool GetReflectionPointerAsReferenceParams(Stream<char> field_tag, Stream<char>& key, Stream<char>& custom_element_type) {
			Stream<char> isolated_tag = GetReflectionFieldSeparatedTag(field_tag, STRING(ECS_POINTER_AS_REFERENCE));
			if (isolated_tag.size > 0) {
				Stream<char> parameters = GetStringParameter(isolated_tag);
				if (parameters.size > 0) {
					Stream<char> comma = FindFirstCharacter(parameters, ',');
					if (comma.size > 0) {
						key = TrimWhitespaceEx(parameters.StartDifference(comma));

						custom_element_type = SkipWhitespaceEx(comma.AdvanceReturn());
						if (custom_element_type.StartsWith(STRING(ECS_CUSTOM_TYPE_ELEMENT))) {
							// Eliminate the overall parenthesis
							custom_element_type.size--;
							custom_element_type = GetStringParameter(custom_element_type);
						}
						else {
							custom_element_type = {};
						}
					}
					else {
						key = TrimWhitespaceEx(parameters);
					}
				}
				else {
					key = {};
					custom_element_type = {};
				}
				
				return true;
			}
			return false;
		}

		bool GetReflectionPointerReferenceKeyParams(Stream<char> field_tag, Stream<char>& key, Stream<char>& custom_element_type) {
			// At the moment, this function is identical to that of the pointer as reference,
			// Except for the macro string
			Stream<char> isolated_tag = GetReflectionFieldSeparatedTag(field_tag, STRING(ECS_POINTER_KEY_REFERENCE_TARGET));
			if (isolated_tag.size > 0) {
				Stream<char> parameters = GetStringParameter(isolated_tag);
				if (parameters.size > 0) {
					Stream<char> comma = FindFirstCharacter(parameters, ',');
					if (comma.size > 0) {
						key = TrimWhitespaceEx(parameters.StartDifference(comma));

						custom_element_type = SkipWhitespaceEx(comma.AdvanceReturn());
						if (custom_element_type.StartsWith(STRING(ECS_CUSTOM_TYPE_ELEMENT))) {
							// Eliminate the overall parenthesis
							custom_element_type.size--;
							custom_element_type = GetStringParameter(custom_element_type);
						}
						else {
							custom_element_type = {};
						}
					}
					else {
						key = TrimWhitespaceEx(parameters);
					}
				}
				else {
					key = {};
					custom_element_type = {};
				}

				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		int AdjustReflectionFieldBasicTypeArrayCount(Stream<ReflectionField> fields, unsigned int field_index, unsigned int new_count) {
			int new_byte_size = (int)(new_count * (unsigned int)fields[field_index].info.byte_size);
			int difference = new_byte_size - (int)fields[field_index].info.byte_size;

			fields[field_index].info.basic_type_count = new_count;
			fields[field_index].info.byte_size = new_byte_size;
			fields[field_index].info.stream_type = ReflectionStreamFieldType::BasicTypeArray;

			// Update the fields located after the current field
			for (size_t next_field_index = field_index + 1; next_field_index < fields.size; next_field_index++) {
				// Use this assignment to handle the int to unsigned short conversion properly
				fields[next_field_index].info.pointer_offset = (unsigned short)((int)fields[next_field_index].info.pointer_offset + difference);
			}
			return difference;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionTypeMiscInfo ReflectionTypeMiscInfo::Copy(AllocatorPolymorphic allocator) const {
			switch (type) {
			case ECS_REFLECTION_TYPE_MISC_INFO_SOA:
				return soa.Copy(allocator);
			case ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR:
				return allocator_info.Copy(allocator);
			default:
				ECS_ASSERT(false, "Unhandled/invalid reflection type misc info type");
			}
			return {};
		}

		ReflectionTypeMiscInfo ReflectionTypeMiscInfo::CopyTo(uintptr_t& ptr) const {
			switch (type) {
			case ECS_REFLECTION_TYPE_MISC_INFO_SOA:
				return soa.CopyTo(ptr);
			case ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR:
				return allocator_info.CopyTo(ptr);
			default:
				ECS_ASSERT(false, "Unhandled/invalid reflection type misc info type");
			}
			return {};
		}

		size_t ReflectionTypeMiscInfo::CopySize() const {
			switch (type) {
			case ECS_REFLECTION_TYPE_MISC_INFO_SOA:
				return soa.CopySize();
			case ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR:
				return allocator_info.CopySize();
			default:
				ECS_ASSERT(false, "Unhandled/invalid reflection type misc info type");
			}
			return {};
		}

		void ReflectionTypeMiscInfo::Deallocate(AllocatorPolymorphic allocator) const {
			switch (type) {
			case ECS_REFLECTION_TYPE_MISC_INFO_SOA:
			{
				soa.Deallocate(allocator);
			}
			break;
			case ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR:
			{
				allocator_info.Deallocate(allocator);
			}
			break;
			default:
				ECS_ASSERT(false, "Unhandled/invalid reflection type misc info type");
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionType::DeallocateCoalesced(AllocatorPolymorphic allocator) const
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
			for (size_t index = 0; index < misc_info.size; index++) {
				misc_info[index].Deallocate(allocator);
			}
			
			if (tag.size > 0) {
				DeallocateIfBelongs(allocator, tag.buffer);
			}
			DeallocateIfBelongs(allocator, name.buffer);
			DeallocateIfBelongs(allocator, fields.buffer);
			DeallocateIfBelongs(allocator, evaluations.buffer);
			DeallocateIfBelongs(allocator, misc_info.buffer);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionType::FindField(Stream<char> field_name) const
		{
			for (size_t index = 0; index < fields.size; index++) {
				if (field_name == fields[index].name) {
					return index;
				}
			}
			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		double ReflectionType::GetEvaluation(Stream<char> evaluation_name) const
		{
			for (size_t index = 0; index < evaluations.size; index++) {
				if (evaluations[index].name == evaluation_name) {
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
			copy.fields.Initialize(allocator, fields.size);
			copy.evaluations.InitializeAndCopy(allocator, evaluations);
			copy.tag.InitializeAndCopy(allocator, tag);
			copy.misc_info.Initialize(allocator, misc_info.size);
			copy.byte_size = byte_size;
			copy.alignment = alignment;
			copy.folder_hierarchy_index = folder_hierarchy_index;
			copy.is_blittable = is_blittable;
			copy.is_blittable_with_pointer = is_blittable_with_pointer;
			for (size_t index = 0; index < fields.size; index++) {
				copy.fields[index].name.InitializeAndCopy(allocator, fields[index].name);
				copy.fields[index].tag.InitializeAndCopy(allocator, fields[index].tag);
				copy.fields[index].definition.InitializeAndCopy(allocator, fields[index].definition);
				copy.fields[index].info = fields[index].info;
			}
			for (size_t index = 0; index < misc_info.size; index++) {
				copy.misc_info[index] = misc_info[index].Copy(allocator);
			}

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType ReflectionType::CopyCoalesced(AllocatorPolymorphic allocator) const
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
				Stream<char> field_tag = fields[index].tag;
				if (field_tag.size > 0) {
					copy.fields[index].tag.InitializeAndCopy(ptr, field_tag);
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

			if (misc_info.size > 0) {
				copy.misc_info.InitializeFromBuffer(ptr, misc_info.size);
				for (size_t index = 0; index < misc_info.size; index++) {
					copy.misc_info[index] = misc_info[index].CopyTo(ptr);
				}
			}
			else {
				copy.misc_info = {};
			}

			copy.folder_hierarchy_index = folder_hierarchy_index;
			copy.is_blittable = is_blittable;
			copy.is_blittable_with_pointer = is_blittable_with_pointer;

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ReflectionType::CopySize() const
		{
			return name.CopySize() + tag.CopySize() + StreamCoalescedDeepCopySize(fields) + StreamCoalescedDeepCopySize(evaluations)
				+ StreamCoalescedDeepCopySize(misc_info);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionEnum::Deallocate(AllocatorPolymorphic allocator) const
		{
			for (size_t index = 0; index < original_fields.size; index++) {
				DeallocateIfBelongs(allocator, original_fields[index].buffer);
			}
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
			copy.fields = StreamCoalescedDeepCopy(fields, ptr);
			copy.original_fields = StreamCoalescedDeepCopy(original_fields, ptr);

			return copy;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum ReflectionEnum::Copy(AllocatorPolymorphic allocator) const
		{
			ReflectionEnum copy;

			copy.folder_hierarchy_index = folder_hierarchy_index;
			copy.name.InitializeAndCopy(allocator, name);
			copy.fields = StreamCoalescedDeepCopy(fields, allocator);
			copy.original_fields = StreamCoalescedDeepCopy(original_fields, allocator);

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
			return name.CopySize();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionTypedef ReflectionTypedef::CopyTo(uintptr_t& ptr) const {			
			ReflectionTypedef type_definition;
			type_definition.definition = definition.CopyTo(ptr);
			type_definition.folder_hierarchy_index = folder_hierarchy_index;
			return type_definition;
		}

		size_t ReflectionTypedef::CopySize() const {
			return definition.CopySize();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionTypeTemplate::Argument ReflectionTypeTemplate::Argument::CopyTo(uintptr_t& ptr) const
		{
			Argument copy;
			copy.template_parameter_index = template_parameter_index;
			copy.has_default_value = has_default_value;
			copy.type = type;
			if (copy.type == ArgumentType::Integer) {
				copy.default_integer_value = default_integer_value;
			}
			else {
				copy.type_name.InitializeAndCopy(ptr, type_name);
				if (has_default_value) {
					copy.default_type_name.InitializeAndCopy(ptr, default_type_name);
				}
			}
			return copy;
		}

		size_t ReflectionTypeTemplate::Argument::CopySize() const
		{
			size_t copy_size = 0;
			if (type == ArgumentType::Type) {
				copy_size += type_name.CopySize();
				if (has_default_value) {
					copy_size += default_type_name.CopySize();
				}
			}
			return copy_size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionTypeTemplate ReflectionTypeTemplate::CopyTo(uintptr_t& ptr) const {
			ReflectionTypeTemplate copy = *this;
			copy.arguments = StreamCoalescedDeepCopy(arguments, ptr);
			copy.base_type = base_type.CopyTo(ptr);
			copy.embedded_array_sizes = embedded_array_sizes.CopyTo(ptr);
			return copy;
		}

		size_t ReflectionTypeTemplate::CopySize() const {
			return StreamCoalescedDeepCopySize(arguments) + base_type.CopySize() + embedded_array_sizes.CopySize();
		}

		unsigned int ReflectionTypeTemplate::GetMandatoryParameterCount() const {
			return mandatory_parameter_count;
		}

		ReflectionTypeTemplate::MatchStatus ReflectionTypeTemplate::DoesMatch(Stream<char> template_parameters, CapacityStream<Stream<char>>* matched_arguments, bool matched_arguments_include_default_parameters) const {
			if (template_parameters.size < 2) {
				return MatchStatus::Failure;
			}
			
			if (template_parameters[0] == '<') {
				template_parameters.Advance();
			}

			if (template_parameters.Last() == '>') {
				template_parameters.size--;
			}

			ECS_STACK_CAPACITY_STREAM(Stream<char>, stack_matched_arguments, 32);
			SplitStringWithParameterList(template_parameters, ',', '<', '>', &stack_matched_arguments);

			unsigned int mandatory_argument_count = GetMandatoryParameterCount();
			if (stack_matched_arguments.size < mandatory_argument_count) {
				return MatchStatus::Failure;
			}

			// Skip the whitespaces for the arguments
			for (unsigned int index = 0; index < stack_matched_arguments.size; index++) {
				stack_matched_arguments[index] = TrimWhitespace(stack_matched_arguments[index]);
			}

			for (unsigned int index = 0; index < arguments.size; index++) {
				// If the argument is specified
				if (arguments[index].template_parameter_index < stack_matched_arguments.size) {
					if (arguments[index].type == ArgumentType::Integer) {
						// If the template parameter is not an integer, fail
						if (!IsIntegerNumber(stack_matched_arguments[index])) {
							return MatchStatus::IncorrectParameter;
						}
					}
					else if (arguments[index].type == ArgumentType::Type) {
						// If it is a number, fail
						if (IsIntegerNumber(stack_matched_arguments[index])) {
							return MatchStatus::IncorrectParameter;
						}
					}
					else {
						ECS_ASSERT(false);
					}

					// The argument is valid, continue
					if (matched_arguments != nullptr) {
						matched_arguments->AddAssert(stack_matched_arguments[index]);
					}
				}
				else if (matched_arguments_include_default_parameters) {
					if (matched_arguments != nullptr) {
						// Add the default value
						if (arguments[index].type == ArgumentType::Integer) {
							matched_arguments->AddAssert({ arguments[index].default_integer_string_characters, (size_t)arguments[index].default_integer_string_length });
						}
						else if (arguments[index].type == ArgumentType::Type) {
							matched_arguments->AddAssert(arguments[index].default_type_name);
						}
						else {
							ECS_ASSERT(false);
						}
					}
				}
			}

			return MatchStatus::Success;
		}

		void ReflectionTypeTemplate::Finalize(AllocatorPolymorphic allocator, Stream<ReflectionEmbeddedArraySize>& template_embedded_array_sizes) {
			// There are 3 things that must be performed - compute the mandatory argument count,
			// Convert the default integers into strings and match the embedded array sizes with
			// Their template arguments

			// Mandatory parameter count loop
			for (size_t index = 0; index < arguments.size; index++) {
				if (arguments[index].has_default_value) {
					mandatory_parameter_count = index;
					break;
				}
			}

			// Integer default strings
			for (size_t index = 0; index < arguments.size; index++) {
				if (arguments[index].has_default_value && arguments[index].type == ArgumentType::Integer) {
					CapacityStream<char> default_string = { arguments[index].default_integer_string_characters, 0, ECS_COUNTOF(arguments[index].default_integer_string_characters) };
					ConvertIntToChars(default_string, arguments[index].default_integer_value);
					arguments[index].default_integer_string_length = default_string.size;
				}
			}

			auto embedded_array_size_projection = [](const Argument& argument) {
				// We don't want to match non integer fields
				return argument.type == ArgumentType::Integer ? argument.type_name : Stream<char>();
			};

			// Embedded array sizes matching
			size_t total_matched_embedded_array_size = 0;
			for (size_t index = 0; index < template_embedded_array_sizes.size; index++) {
				unsigned int matched_argument_index = arguments.Find(template_embedded_array_sizes[index].body, embedded_array_size_projection);
				if (matched_argument_index != -1) {
					total_matched_embedded_array_size++;
				}
			}

			if (total_matched_embedded_array_size > 0) {
				embedded_array_sizes.Initialize(allocator, total_matched_embedded_array_size);
				embedded_array_sizes.size = 0;
				for (size_t index = 0; index < template_embedded_array_sizes.size; index++) {
					unsigned int matched_argument_index = arguments.Find(template_embedded_array_sizes[index].body, embedded_array_size_projection);
					if (matched_argument_index != -1) {
						embedded_array_sizes.Add({ matched_argument_index, base_type.FindField(template_embedded_array_sizes[index].field_name) });
						template_embedded_array_sizes.RemoveSwapBack(index);
						index--;
					}
				}
			}
		}

		ReflectionType ReflectionTypeTemplate::Instantiate(const ReflectionManager* reflection_manager, AllocatorPolymorphic allocator, Stream<Stream<char>> template_arguments) const {
			ReflectionType instance;

			// We can perform a copy right away
			instance = base_type.Copy(allocator);
			
			// Start matching the template arguments and replace them as needed
			// Accumulate all template type replacements such that we can perform the
			// Replacement all at once
			ECS_STACK_CAPACITY_STREAM(ReplaceOccurrence<char>, type_template_replacements, 64);

			for (size_t index = 0; index < arguments.size; index++) {
				if (arguments[index].type == ArgumentType::Integer) {
					int64_t integer_value = 0;
					if (arguments[index].template_parameter_index < template_arguments.size) {
						integer_value = ConvertCharactersToInt(template_arguments[arguments[index].template_parameter_index]);
					}
					else {
						// Use the default - it must be defaulted
						ECS_ASSERT(arguments[index].has_default_value, "Trying to instantiate a reflection template without full arguments");
						integer_value = arguments[index].default_integer_value;
					}

					// At the moment, the only thing to do with the integer template parameters is to replace them in basic type arrays
					for (size_t subindex = 0; subindex < embedded_array_sizes.size; subindex++) {
						if ((size_t)embedded_array_sizes[subindex].x == index) {
							instance.byte_size += AdjustReflectionFieldBasicTypeArrayCount(instance.fields, embedded_array_sizes[subindex].y, integer_value);
						}
					}
				}
				else if (arguments[index].type == ArgumentType::Type) {
					Stream<char> type_name_replacement;
					if (arguments[index].template_parameter_index < template_arguments.size) {
						type_name_replacement = template_arguments[arguments[index].template_parameter_index];
					}
					else {
						// Use the default - it must be defaulted
						ECS_ASSERT(arguments[index].has_default_value, "Trying to instantiate a reflection template without full arguments");
						type_name_replacement = arguments[index].default_type_name;
					}
					type_template_replacements.AddAssert({ arguments[index].type_name, type_name_replacement });
				}
				else {
					ECS_ASSERT(false);
				}
			}

			// Perform the final replacement
			for (size_t index = 0; index < instance.fields.size; index++) {
				// Do the replacement only if the base type is UserDefined
				if (instance.fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					Stream<char> previous_definition = instance.fields[index].definition;
					instance.fields[index].definition = ReplaceTokensWithDelimiters(previous_definition, type_template_replacements, GetCppTypeTokenSeparators(), allocator);
					// Deallocate the previous definition
					previous_definition.Deallocate(allocator);

					// Now determine if the type is a basic type - and update the field info accordingly
					// Be careful to pointers, eliminate the asterisks
					Stream<char> definition_without_special_characters = instance.fields[index].definition;
					while (definition_without_special_characters.Last() == '*') {
						definition_without_special_characters.size--;
					}

					ReflectionField basic_field = GetReflectionFieldInfo(reflection_manager, definition_without_special_characters);
					if (basic_field.info.basic_type != ReflectionBasicFieldType::UserDefined) {
						unsigned int byte_size_difference = basic_field.info.byte_size - instance.fields[index].info.byte_size;
						// Update the byte size and basic type of the field
						instance.fields[index].info.byte_size = basic_field.info.byte_size;
						instance.fields[index].info.basic_type = basic_field.info.basic_type;

						// Update the pointer offsets of all next fields
						for (size_t subindex = index + 1; subindex < instance.fields.size; subindex++) {
							instance.fields[subindex].info.pointer_offset += byte_size_difference;
						}
					}
				}
			}

			ECS_STACK_CAPACITY_STREAM(char, instance_name, 512);
			instance_name.CopyOther(base_type.name);
			instance_name.AddAssert('<');
			for (size_t index = 0; index < template_arguments.size; index++) {
				instance_name.AddStreamAssert(template_arguments[index]);
				instance_name.AddAssert(',');
				instance_name.AddAssert(' ');
			}
			// Remove the last ', '
			instance_name.size -= 2;
			instance_name.AddAssert('>');
			// Deallocate the type name, and replace it
			instance.name.Deallocate(allocator);
			instance.name = instance_name.Copy(allocator);

			return instance;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
