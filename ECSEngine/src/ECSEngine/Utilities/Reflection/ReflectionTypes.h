#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../BasicTypes.h"

namespace ECSEngine {

	namespace Reflection {

		enum class ReflectionBasicFieldType : unsigned char {
			Int8,
			UInt8,
			Int16,
			UInt16,
			Int32,
			UInt32,
			Int64,
			UInt64,
			Wchar_t,
			Float,
			Double,
			Bool,
			Enum,
			Char2,
			UChar2,
			Short2,
			UShort2,
			Int2,
			UInt2,
			Long2,
			ULong2,
			Char3,
			UChar3,
			Short3,
			UShort3,
			Int3,
			UInt3,
			Long3,
			ULong3,
			Char4,
			UChar4,
			Short4,
			UShort4,
			Int4,
			UInt4,
			Long4,
			ULong4,
			Float2,
			Float3,
			Float4,
			Double2,
			Double3,
			Double4,
			Bool2,
			Bool3,
			Bool4,
			UserDefined,
			Unknown,
			COUNT
		};

		// The extended type indicates whether or not this is a stream/pointer/embedded array
		enum class ReflectionStreamFieldType : unsigned char {
			Basic,
			Pointer,
			BasicTypeArray,
			Stream,
			CapacityStream,
			ResizableStream,
			Unknown,
			COUNT
		};

		struct ReflectionFieldInfo {
			ReflectionFieldInfo() {}
			ReflectionFieldInfo(ReflectionBasicFieldType _basic_type, ReflectionStreamFieldType _extended_type, unsigned short _byte_size, unsigned short _basic_type_count)
				: stream_type(_extended_type), basic_type(_basic_type), byte_size(_byte_size), basic_type_count(_basic_type_count) {}

			ReflectionFieldInfo(const ReflectionFieldInfo& other) = default;
			ReflectionFieldInfo& operator = (const ReflectionFieldInfo& other) = default;

			union {
				char default_char;
				unsigned char default_uchar;
				short default_short;
				unsigned short default_ushort;
				int default_int;
				unsigned int default_uint;
				long long default_long;
				unsigned long long default_ulong;
				float default_float;
				double default_double;
				bool default_bool;
				char2 default_char2;
				uchar2 default_uchar2;
				short2 default_short2;
				ushort2 default_ushort2;
				int2 default_int2;
				uint2 default_uint2;
				long2 default_long2;
				ulong2 default_ulong2;
				float2 default_float2;
				double2 default_double2;
				bool2 default_bool2;
				char3 default_char3;
				uchar3 default_uchar3;
				short3 default_short3;
				ushort3 default_ushort3;
				int3 default_int3;
				uint3 default_uint3;
				long3 default_long3;
				ulong3 default_ulong3;
				float3 default_float3;
				double3 default_double3;
				bool3 default_bool3;
				char4 default_char4;
				uchar4 default_uchar4;
				short4 default_short4;
				ushort4 default_ushort4;
				int4 default_int4;
				uint4 default_uint4;
				long4 default_long4;
				ulong4 default_ulong4;
				float4 default_float4;
				double4 default_double4;
				bool4 default_bool4;
			};
			bool has_default_value;
			ReflectionStreamFieldType stream_type;
			ReflectionBasicFieldType basic_type;
			unsigned short basic_type_count;
			unsigned short stream_byte_size;
			unsigned short byte_size;
			unsigned short pointer_offset;
		};

		struct ECSENGINE_API ReflectionField {
			// It returns true if the string appears in the tag, else returns false in both cases
			bool Skip(Stream<char> string) const;

			bool Is(Stream<char> string) const;

			Stream<char> name;
			Stream<char> definition;
			Stream<char> tag;
			ReflectionFieldInfo info;
		};

		struct ECSENGINE_API ReflectionEvaluation {
			Stream<char> name;
			double value;
		};

		struct ECSENGINE_API ReflectionType {
			// If the tag is nullptr, it returns false. If it is set, it will check if the substring exists
			bool HasTag(Stream<char> string) const;

			// Does a CompareStrings, not a FindFirstToken
			bool IsTag(Stream<char> string) const;

			// Returns DBL_MAX if it doesn't exist
			double GetEvaluation(Stream<char> name) const;

			// Copies everything that needs to be copied into this buffer
			ReflectionType Copy(uintptr_t& ptr) const;

			Stream<char> name;
			Stream<char> tag;
			Stream<ReflectionField> fields;
			Stream<ReflectionEvaluation> evaluations;
			unsigned int folder_hierarchy_index;
			unsigned int byte_size;
			unsigned int alignment;
		};

		struct ReflectionEnum {
			// Copies everything that needs to be copied into this buffer
			ReflectionEnum Copy(uintptr_t& ptr) const;

			Stream<char> name;
			Stream<Stream<char>> fields;
			unsigned int folder_hierarchy_index;
		};

		struct ReflectionConstant {
			Stream<char> name;
			double value;
			unsigned int folder_hierarchy;
		};

		struct ReflectionCustomTypeMatchData {
			Stream<char> definition;
		};

		typedef bool (*ReflectionCustomTypeMatch)(ReflectionCustomTypeMatchData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(name) bool ReflectionCustomTypeMatch_##name(Reflection::ReflectionCustomTypeMatchData* data)

		struct ReflectionManager;

		struct ReflectionCustomTypeByteSizeData {
			Stream<char> definition;
			const Reflection::ReflectionManager* reflection_manager;
		};

		// Return 0 if you cannot determine right now the byte size (e.g. you are template<typename T> struct { T data; ... })
		// The x component is the byte size, the y component is the alignment
		typedef ulong2 (*ReflectionCustomTypeByteSize)(ReflectionCustomTypeByteSizeData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(name) ulong2 ReflectionCustomTypeByteSize_##name(Reflection::ReflectionCustomTypeByteSizeData* data)

		// No need to allocate the strings, they can be referenced inside the definition since it is stable
		struct ReflectionCustomTypeDependentTypesData {
			Stream<char> definition;
			CapacityStream<Stream<char>> dependent_types;
		};

		typedef void (*ReflectionCustomTypeDependentTypes)(ReflectionCustomTypeDependentTypesData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(name) void ReflectionCustomTypeDependentTypes_##name(Reflection::ReflectionCustomTypeDependentTypesData* data)

		struct ReflectionCustomType {
			ReflectionCustomTypeMatch match;
			ReflectionCustomTypeDependentTypes dependent_types;
			ReflectionCustomTypeByteSize byte_size;
		};

		// Works only for non user-defined_types
		ECSENGINE_API size_t GetFieldTypeAlignment(ReflectionBasicFieldType field_type);

		ECSENGINE_API size_t GetFieldTypeAlignment(ReflectionStreamFieldType stream_type);

#define ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(name) ECSENGINE_API bool ReflectionCustomTypeMatch_##name(Reflection::ReflectionCustomTypeMatchData* data); \
															ECSENGINE_API ulong2 ReflectionCustomTypeByteSize_##name(Reflection::ReflectionCustomTypeByteSizeData* data); \
															ECSENGINE_API void ReflectionCustomTypeDependentTypes_##name(Reflection::ReflectionCustomTypeDependentTypesData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_STRUCT(name) { ReflectionCustomTypeMatch_##name, ReflectionCustomTypeDependentTypes_##name, ReflectionCustomTypeByteSize_##name }

	}

}