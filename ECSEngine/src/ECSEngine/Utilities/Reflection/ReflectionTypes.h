#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../BasicTypes.h"

namespace ECSEngine {

	namespace Reflection {

#define ECS_REFLECTION_TYPE_TAG_DELIMITER_CHAR '~'
#define ECS_REFLECTION_TYPE_TAG_DELIMITER_STRING "~"

		struct ReflectionManager;

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

		struct ECSENGINE_API ReflectionFieldInfo {
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
			bool Has(Stream<char> string) const;

			bool Is(Stream<char> string) const;

			// Returns the tag isolated from others
			Stream<char> GetTag(Stream<char> string) const;

			// Each buffer will be allocated separately. To deallocate call DeallocateSeparate
			ReflectionField Copy(AllocatorPolymorphic allocator) const;

			ReflectionField CopyTo(uintptr_t& ptr) const;

			void DeallocateSeparate(AllocatorPolymorphic allocator) const;

			// Only the buffer size is needed
			size_t CopySize() const;

			// Returns true if they have the same representation
			bool Compare(const ReflectionField& field) const;

			Stream<char> name;
			Stream<char> definition;
			Stream<char> tag;
			ReflectionFieldInfo info;
		};

		struct ECSENGINE_API ReflectionEvaluation {
			ReflectionEvaluation CopyTo(uintptr_t& ptr) const;

			// Only the buffer size is needed
			size_t CopySize() const;

			Stream<char> name;
			double value;
		};

		struct ECSENGINE_API ReflectionType {
			void DeallocateCoallesced(AllocatorPolymorphic allocator) const;

			// It will try to deallocate anything that can be deallocated (when using
			// non coallesced allocations). It uses IfBelongs deallocations
			void Deallocate(AllocatorPolymorphic allocator) const;

			// If the tag is nullptr, it returns false. If it is set, it will check if the substring exists
			bool HasTag(Stream<char> string) const;

			// Does a CompareStrings, not a FindFirstToken
			bool IsTag(Stream<char> string) const;

			// In case the tag has multiple separated elements it will return the value separated
			Stream<char> GetTag(Stream<char> string, Stream<char> separator) const;

			unsigned int FindField(Stream<char> name) const;

			// Returns DBL_MAX if it doesn't exist
			double GetEvaluation(Stream<char> name) const;

			// Every buffer will be individually allocated
			ReflectionType Copy(AllocatorPolymorphic allocator) const;

			ReflectionType CopyCoallesced(AllocatorPolymorphic allocator) const;

			// Copies everything that needs to be copied into this buffer
			ReflectionType CopyTo(uintptr_t& ptr) const;

			size_t CopySize() const;

			Stream<char> name;
			Stream<char> tag;
			Stream<ReflectionField> fields;
			Stream<ReflectionEvaluation> evaluations;
			unsigned int folder_hierarchy_index;
			unsigned int byte_size;
			unsigned int alignment;
			bool is_blittable;
			bool is_blittable_with_pointer;
		};

		struct ECSENGINE_API ReflectionEnum {
			// Uses IfBelongs deallocations.
			void Deallocate(AllocatorPolymorphic allocator) const;

			// Copies everything that needs to be copied into this buffer
			ReflectionEnum CopyTo(uintptr_t& ptr) const;

			Stream<char> name;
			// These are the fields as they are parsed originally
			Stream<Stream<char>> original_fields;

			// These are the stylized fields - those that have no underscores
			// or all caps and without the COUNT label if it exists
			Stream<Stream<char>> fields;
			unsigned int folder_hierarchy_index;
		};

		struct ECSENGINE_API ReflectionConstant {
			ReflectionConstant CopyTo(uintptr_t& ptr) const;

			size_t CopySize() const;

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
			const ReflectionManager* reflection_manager;
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

		struct ReflectionCustomTypeIsBlittableData {
			Stream<char> definition;
			const ReflectionManager* reflection_manager;
		};

		typedef bool (*ReflectionCustomTypeIsBlittable)(ReflectionCustomTypeIsBlittableData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_IS_BLITTABLE_FUNCTION(name) bool ReflectionCustomTypeIsBlittable_##name(Reflection::ReflectionCustomTypeIsBlittableData* data)

		struct ReflectionCustomTypeCopyData {
			const Reflection::ReflectionManager* reflection_manager;
			Stream<char> definition;
			const void* source;
			void* destination;
			AllocatorPolymorphic allocator;
		};

		typedef void (*ReflectionCustomTypeCopy)(ReflectionCustomTypeCopyData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_COPY_FUNCTION(name) void ReflectionCustomTypeCopy_##name(Reflection::ReflectionCustomTypeCopyData* data)

		struct ReflectionCustomTypeCompareData {
			const Reflection::ReflectionManager* reflection_manager;
			Stream<char> definition;
			const void* first;
			const void* second;
		};

		typedef bool (*ReflectionCustomTypeCompare)(ReflectionCustomTypeCompareData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_COMPARE_FUNCTION(name) bool ReflectionCustomTypeCompare_##name(Reflection::ReflectionCustomTypeCompareData* data)

		struct ReflectionCustomType {
			ReflectionCustomTypeMatch match;
			ReflectionCustomTypeDependentTypes dependent_types;
			ReflectionCustomTypeByteSize byte_size;
			ReflectionCustomTypeIsBlittable is_blittable;
			ReflectionCustomTypeCopy copy;
			ReflectionCustomTypeCompare compare;
		};

		// Uses a jump table
		ECSENGINE_API size_t GetReflectionBasicFieldTypeByteSize(ReflectionBasicFieldType basic_type);

		// Works only for non user-defined_types
		ECSENGINE_API size_t GetFieldTypeAlignment(ReflectionBasicFieldType field_type);

		ECSENGINE_API size_t GetFieldTypeAlignment(ReflectionStreamFieldType stream_type);

		// Works only for non user-defined types
		ECSENGINE_API size_t GetFieldTypeAlignment(const ReflectionFieldInfo* info);

		// Returns a stable value
		ECSENGINE_API Stream<char> GetBasicFieldDefinition(ReflectionBasicFieldType basic_type);

		ECSENGINE_API void ConvertFromDouble4ToBasic(ReflectionBasicFieldType basic_type, double4 values, void* converted_values);

		// Checks for single, double, triple and quadruple component integers
		ECSENGINE_API bool IsIntegral(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralSingleComponent(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralMultiComponent(ReflectionBasicFieldType type);

		ECSENGINE_API unsigned char BasicTypeComponentCount(ReflectionBasicFieldType type);

		// If it is a 2, 3 or 4 type then it will return the basic one with a single component
		ECSENGINE_API ReflectionBasicFieldType ReduceMultiComponentReflectionType(ReflectionBasicFieldType type);

		inline bool IsBlittable(const ReflectionType* type) {
			return type->is_blittable;
		}

		inline bool IsBlittableWithPointer(const ReflectionType* type) {
			return type->is_blittable_with_pointer;
		}

		inline bool IsBoolBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool2 || type == ReflectionBasicFieldType::Bool3 || type == ReflectionBasicFieldType::Bool4;
		}

		inline bool IsBoolBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool || IsBoolBasicTypeMultiComponent(type);
		}

		// Checks for float2, float3, float4
		inline bool IsFloatBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Float2 || type == ReflectionBasicFieldType::Float3 || type == ReflectionBasicFieldType::Float4;
		}

		// Checks for float, float2, float3, float4
		inline bool IsFloatBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Float || IsFloatBasicTypeMultiComponent(type);
		}

		// Checks for double2, double3, double4
		inline bool IsDoubleBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Double2 || type == ReflectionBasicFieldType::Double3 || type == ReflectionBasicFieldType::Double4;
		}

		// Checks for double, double2, double3, double4
		inline bool IsDoubleBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Double || IsDoubleBasicTypeMultiComponent(type);
		}

		// Checks for float, float2, float3, float4, double, double2, double3, double4
		inline bool IsFloating(ReflectionBasicFieldType type) {
			return IsFloatBasicType(type) || IsDoubleBasicType(type);
		}

		inline bool IsUserDefined(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::UserDefined;
		}

		inline bool IsPointer(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::Pointer;
		}

		inline bool IsEnum(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Enum;
		}

		inline bool IsArray(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::BasicTypeArray;
		}

		inline bool IsStream(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::Stream || type == ReflectionStreamFieldType::CapacityStream
				|| type == ReflectionStreamFieldType::ResizableStream;
		}

#define ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(name) ECSENGINE_API bool ReflectionCustomTypeMatch_##name(Reflection::ReflectionCustomTypeMatchData* data); \
															ECSENGINE_API ulong2 ReflectionCustomTypeByteSize_##name(Reflection::ReflectionCustomTypeByteSizeData* data); \
															ECSENGINE_API void ReflectionCustomTypeDependentTypes_##name(Reflection::ReflectionCustomTypeDependentTypesData* data); \
															ECSENGINE_API bool ReflectionCustomTypeIsBlittable_##name(Reflection::ReflectionCustomTypeIsBlittableData* data); \
															ECSENGINE_API void ReflectionCustomTypeCopy_##name(Reflection::ReflectionCustomTypeCopyData* data); \
															ECSENGINE_API bool ReflectionCustomTypeCompare_##name(Reflection::ReflectionCustomTypeCompareData* data);

#define ECS_REFLECTION_CUSTOM_TYPE_STRUCT(name) { ReflectionCustomTypeMatch_##name, ReflectionCustomTypeDependentTypes_##name, ReflectionCustomTypeByteSize_##name, \
		ReflectionCustomTypeIsBlittable_##name, ReflectionCustomTypeCopy_##name, ReflectionCustomTypeCompare_##name }

	}

}