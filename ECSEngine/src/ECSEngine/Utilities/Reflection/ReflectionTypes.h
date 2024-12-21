#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../BasicTypes.h"
#include "../StringUtilities.h"
#include "../PointerUtilities.h"

namespace ECSEngine {

	namespace Reflection {

#define ECS_REFLECTION_TYPE_TAG_DELIMITER_CHAR '~'
#define ECS_REFLECTION_TYPE_TAG_DELIMITER_STRING "~"

		struct ReflectionManager;

		// IMPORTANT TODO: Should we create separate flags for Custom Reflection Type,
		// Blittable Exception? This would speed up some functions but it would require
		// A bit of work to make it happen

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
			PointerSoA,
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
			// The stream fields are valid only for stream_types different from basic
			unsigned char stream_alignment;
			unsigned short stream_byte_size;
			unsigned short basic_type_count;
			unsigned short byte_size;
			unsigned short pointer_offset;
			
			// Many reflection functions used ReflectionField/ReflectionFieldInfo as inputs
			// And having the SoA information per type would break them, requiring to be
			// Rewritten to accept the ReflectionType and the field index. In order to achieve
			// A reasonable compromise, we can have the size pointer offset and the basic type 
			// for the SoA pointer here directly, which would add another unsigned short and a byte, 
			// but it is not too much of a problem
			struct {
				unsigned short soa_size_pointer_offset = 0;
				ReflectionBasicFieldType soa_size_basic_type = ReflectionBasicFieldType::COUNT;
			};
		};

		struct ECSENGINE_API ReflectionField {
			// It returns true if the string appears in the tag, else returns false in both cases
			ECS_INLINE bool Has(Stream<char> string) const {
				if (tag.size > 0) {
					return FindFirstToken(tag, string).buffer != nullptr;
				}
				return false;
			}

			ECS_INLINE bool Is(Stream<char> string) const {
				return tag == string;
			}

			// Returns the tag isolated from others
			ECS_INLINE Stream<char> GetTag(Stream<char> string) const {
				return IsolateString(tag, string, ECS_REFLECTION_TYPE_TAG_DELIMITER_STRING);
			}

			// Each buffer will be allocated separately. To deallocate call DeallocateSeparate
			ReflectionField Copy(AllocatorPolymorphic allocator) const;

			ReflectionField CopyTo(uintptr_t& ptr) const;

			void DeallocateSeparate(AllocatorPolymorphic allocator) const;

			// Only the buffer size is needed
			ECS_INLINE size_t CopySize() const {
				return name.CopySize() + definition.CopySize() + tag.CopySize();
			}

			// Returns true if they have the same representation
			ECS_INLINE bool Compare(const ReflectionField& field) const {
				return field.definition == definition;
			}

			Stream<char> name;
			Stream<char> definition;
			Stream<char> tag;
			ReflectionFieldInfo info;
		};

		struct ECSENGINE_API ReflectionEvaluation {
			ReflectionEvaluation CopyTo(uintptr_t& ptr) const;

			// Only the buffer size is needed
			ECS_INLINE size_t CopySize() const {
				return name.CopySize();
			}

			Stream<char> name;
			double value;
		};

		enum ECS_REFLECTION_TYPE_MISC_INFO_TYPE : unsigned char {
			ECS_REFLECTION_TYPE_MISC_INFO_SOA,
			ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR,
			ECS_REFLECTION_TYPE_MISC_INFO_COUNT
		};

		struct ReflectionTypeMiscSoa {
			ECS_INLINE ReflectionTypeMiscSoa Copy(AllocatorPolymorphic allocator) const {
				ReflectionTypeMiscSoa copy = *this;
				copy.name = name.Copy(allocator);
				return copy;
			}

			ECS_INLINE ReflectionTypeMiscSoa CopyTo(uintptr_t& ptr) const {
				ReflectionTypeMiscSoa copy = *this;
				copy.name.InitializeAndCopy(ptr, name);
				return copy;
			}

			ECS_INLINE size_t CopySize() const {
				return name.CopySize();
			}

			ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) const {
				name.Deallocate(allocator);
			}

			ECS_INLINE bool HasCapacity() const {
				return capacity_field != UCHAR_MAX;
			}

			Stream<char> name;
			unsigned char size_field;
			unsigned char capacity_field;
			// This is an optional field, which describes an allocator to be used, if there is a preference
			unsigned char field_allocator_index;
			unsigned char parallel_stream_count;
			unsigned char parallel_streams[12];
		};

		enum ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER : unsigned char {
			ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER_NONE,
			// When set, it indicates that it should act as an override for the allocator to be used, lower
			// In priority than per field allocator
			ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER_MAIN,
			// When specified, it indicates that this field should not perform a full initialization, 
			// But only reference the allocator that is given
			ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER_REFERENCE
		};

		// This structure specifies a type's allocator, from which allocations can be made from
		// If the user specified that per type allocators or per field allocators are to be enabled (only the main has that property).
		struct ReflectionTypeMiscAllocator {
			ECS_INLINE ReflectionTypeMiscAllocator Copy(AllocatorPolymorphic allocator) const {
				return *this;
			}

			ECS_INLINE ReflectionTypeMiscAllocator CopyTo(uintptr_t& ptr) const {
				return *this;
			}

			ECS_INLINE size_t CopySize() const {
				// No buffer to copy
				return 0;
			}

			ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) const {
				// Nothing to deallocate
			}

			unsigned int field_index;
			ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER modifier;
		};

		struct ECSENGINE_API ReflectionTypeMiscInfo {
			ECS_INLINE ReflectionTypeMiscInfo() : type(ECS_REFLECTION_TYPE_MISC_INFO_COUNT) {}
			ECS_INLINE ReflectionTypeMiscInfo(const ReflectionTypeMiscSoa& soa) : type(ECS_REFLECTION_TYPE_MISC_INFO_SOA), soa(soa) {}
			ECS_INLINE ReflectionTypeMiscInfo(const ReflectionTypeMiscAllocator& allocator) : type(ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR), allocator_info(allocator) {}

			ReflectionTypeMiscInfo Copy(AllocatorPolymorphic allocator) const;

			ReflectionTypeMiscInfo CopyTo(uintptr_t& ptr) const;

			size_t CopySize() const;

			void Deallocate(AllocatorPolymorphic allocator) const;

			ECS_REFLECTION_TYPE_MISC_INFO_TYPE type;
			union {
				ReflectionTypeMiscSoa soa;
				ReflectionTypeMiscAllocator allocator_info;
			};
		};

		struct ECSENGINE_API ReflectionType {
			void DeallocateCoalesced(AllocatorPolymorphic allocator) const;

			// It will try to deallocate anything that can be deallocated (when using
			// non coalesced allocations). It uses IfBelongs deallocations
			void Deallocate(AllocatorPolymorphic allocator) const;

			// If the tag is nullptr, it returns false. If it is set, it will check if the substring exists
			ECS_INLINE bool HasTag(Stream<char> string) const {
				if (tag.size == 0) {
					return false;
				}
				return FindFirstToken(tag, string).buffer != nullptr;
			}

			// Does a CompareStrings, not a FindFirstToken
			ECS_INLINE bool IsTag(Stream<char> string) const {
				return string == tag;
			}

			// In case the tag has multiple separated elements it will return the value separated
			ECS_INLINE Stream<char> GetTag(Stream<char> string, Stream<char> separator) const {
				return IsolateString(tag, string, separator);
			}

			unsigned int FindField(Stream<char> name) const;

			ECS_INLINE void* GetField(const void* data, unsigned int field_index) const {
				return OffsetPointer(data, fields[field_index].info.pointer_offset);
			}

			// Returns DBL_MAX if it doesn't exist
			double GetEvaluation(Stream<char> name) const;

			// Every buffer will be individually allocated
			ReflectionType Copy(AllocatorPolymorphic allocator) const;

			ReflectionType CopyCoalesced(AllocatorPolymorphic allocator) const;

			// Copies everything that needs to be copied into this buffer
			ReflectionType CopyTo(uintptr_t& ptr) const;

			size_t CopySize() const;

			Stream<char> name;
			Stream<char> tag;
			Stream<ReflectionField> fields;
			Stream<ReflectionEvaluation> evaluations;
			Stream<ReflectionTypeMiscInfo> misc_info;
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

			ReflectionEnum Copy(AllocatorPolymorphic allocator) const;

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

		// This is used for replacing typedefs. The structure is the following: typedef definition name;
		struct ECSENGINE_API ReflectionTypedef {
			ReflectionTypedef CopyTo(uintptr_t& ptr) const;

			size_t CopySize() const;

			Stream<char> definition;
			unsigned int folder_hierarchy_index;
		};

		struct ReflectionManager;
		
		struct ReflectionCustomTypeMatchData {
			Stream<char> definition;
		};

		struct ReflectionCustomTypeByteSizeData {
			Stream<char> definition;
			const ReflectionManager* reflection_manager;
		};

		// No need to allocate the strings, they can be referenced inside the definition since it is stable
		struct ReflectionCustomTypeDependentTypesData {
			Stream<char> definition;
			CapacityStream<Stream<char>> dependent_types;
		};

		struct ReflectionCustomTypeIsBlittableData {
			Stream<char> definition;
			const ReflectionManager* reflection_manager;
		};

		struct ReflectionCustomTypeCopyData {
			const ReflectionManager* reflection_manager;
			Stream<char> definition;
			const void* source;
			void* destination;
			AllocatorPolymorphic allocator;
			bool deallocate_existing_data = false;
		};

		struct ReflectionCustomTypeCompareData {
			const ReflectionManager* reflection_manager;
			Stream<char> definition;
			const void* first;
			const void* second;
		};

		struct ReflectionCustomTypeDeallocateData {
			const ReflectionManager* reflection_manager;
			Stream<char> definition;
			void* source;
			AllocatorPolymorphic allocator;
			size_t element_count = 1;
			bool reset_buffers = true;
		};

		struct ReflectionCustomTypeInterface {
			virtual bool Match(ReflectionCustomTypeMatchData* data) = 0;

			// Return 0 if you cannot determine right now the byte size (e.g. you are template<typename T> struct { T data; ... })
			// The x component is the byte size, the y component is the alignment
			virtual ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) = 0;

			virtual void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) = 0;

			virtual bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) = 0;

			virtual void Copy(ReflectionCustomTypeCopyData* data) = 0;

			virtual bool Compare(ReflectionCustomTypeCompareData* data) = 0;

			virtual void Deallocate(ReflectionCustomTypeDeallocateData* data) = 0;
		};

		struct ReflectionDefinitionInfo {
			// Should be called only when is_basic_field is set to true
			ReflectionField GetBasicField() const {
				ReflectionField field;
				field.info.byte_size = byte_size;
				field.info.basic_type = field_basic_type;
				field.info.stream_type = field_stream_type;
				field.info.stream_alignment = field_stream_alignment;
				field.info.stream_byte_size = field_stream_byte_size;
				return field;
			}

			size_t byte_size;
			size_t alignment;
			// When it is not blittable, one of the reflection type, custom type or basic field must be filled in
			bool is_blittable;
			// When this is set, the basic field properties are filled in
			bool is_basic_field = false;
			// This gives the same information as custom type. The custom type field is provided for convenience,
			// While this can be used to address external structures that mirror the one from the reflection custom types
			unsigned int custom_type_index = -1;

			struct {
				// We cannot obtain SoA pointers from definitions, so no need to handle that case

				ReflectionBasicFieldType field_basic_type = ReflectionBasicFieldType::UserDefined;
				ReflectionStreamFieldType field_stream_type = ReflectionStreamFieldType::Basic;
				unsigned char field_stream_alignment = 0;
				unsigned short field_stream_byte_size = 0;
			};

			// These will be filled in case the definition is a reflection type or a custom type
			const ReflectionType* type = nullptr;
			ReflectionCustomTypeInterface* custom_type = nullptr;
		};

		// This structure can be used to reference fields from a type, including nested fields
		// Such that you can pinpoint the exact field
		struct ReflectionNestedFieldIndex {
			ECS_INLINE ReflectionNestedFieldIndex() {}
			ECS_INLINE ReflectionNestedFieldIndex(unsigned char field_index) {
				Add(field_index);
			}

			ECS_INLINE ReflectionNestedFieldIndex& Add(unsigned char field_index) {
				ECS_ASSERT(count < ECS_COUNTOF(indices));
				indices[count++] = field_index;
				return *this;
			}

			// Adds the values from other to this instance
			ECS_INLINE ReflectionNestedFieldIndex& Append(ReflectionNestedFieldIndex other) {
				ECS_ASSERT(count + other.count <= ECS_COUNTOF(indices));
				memcpy(indices + count, other.indices, other.count * sizeof(indices[0]));
				count += other.count;
				return *this;
			}

			unsigned char count = 0;
			unsigned char indices[7];
		};

		// The functor receives as parameters (Stream<char> field_name) which represents
		// The field name of a current type and must return unsigned int with the field
		// Index or -1 if the field was not found
		template<typename Functor>
		ReflectionNestedFieldIndex FindNestedFieldIndex(Stream<char> field_name, Functor&& functor) {
			ReflectionNestedFieldIndex field_index;

			ECS_STACK_CAPACITY_STREAM(Stream<char>, subfields, 32);
			// Split the field by dots
			SplitString(field_name, ".", &subfields);

			while (subfields.size > 1) {
				unsigned int current_field_index = functor(subfields[0]);
				if (current_field_index == -1) {
					// Make the count 0 to signal an error
					field_index.count = 0;
					return field_index;
				}

				field_index.Add(current_field_index);
				subfields.Advance();
			}

			unsigned int index = functor(field_name);
			if (index == -1) {
				field_index.count = 0;
				return field_index;
			}
			field_index.Add(index);
			return field_index;
		}

		// Uses a jump table
		ECSENGINE_API size_t GetReflectionBasicFieldTypeByteSize(ReflectionBasicFieldType basic_type);

		// Works only for non user-defined_types
		ECSENGINE_API size_t GetReflectionFieldTypeAlignment(ReflectionBasicFieldType field_type);

		ECSENGINE_API size_t GetReflectionFieldTypeAlignment(ReflectionStreamFieldType stream_type);

		// Works only for non user-defined types
		ECSENGINE_API size_t GetReflectionFieldTypeAlignment(const ReflectionFieldInfo* info);

		// Works only for non user-defined types - fundamental types
		ECSENGINE_API bool IsReflectionFieldTypeBlittable(const ReflectionFieldInfo* info);

		// Returns a stable value
		ECSENGINE_API Stream<char> GetBasicFieldDefinition(ReflectionBasicFieldType basic_type);

		// Fields that are not matched have DBL_MAX as value
		ECSENGINE_API double4 ConvertToDouble4FromBasic(ReflectionBasicFieldType basic_type, const void* values);

		ECSENGINE_API void ConvertFromDouble4ToBasic(ReflectionBasicFieldType basic_type, double4 values, void* converted_values);

		// Converts a single unsigned integer value to a size_t 
		ECSENGINE_API size_t ConvertToSizetFromBasic(ReflectionBasicFieldType basic_type, const void* value);

		// Converts a single size_t into an unsigned integer value
		ECSENGINE_API void ConvertFromSizetToBasic(ReflectionBasicFieldType basic_type, size_t value, void* pointer_value);

		// Checks for single, double, triple and quadruple component integers
		ECSENGINE_API bool IsIntegral(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralSingleComponent(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralMultiComponent(ReflectionBasicFieldType type);

		ECSENGINE_API unsigned char BasicTypeComponentCount(ReflectionBasicFieldType type);

		// If it is a 2, 3 or 4 type then it will return the basic one with a single component
		ECSENGINE_API ReflectionBasicFieldType ReduceMultiComponentReflectionType(ReflectionBasicFieldType type);

		ECSENGINE_API ECS_INT_TYPE BasicTypeToIntType(ReflectionBasicFieldType type);

		ECS_INLINE size_t GetReflectionTypeSoaAllocationAlignment(const ReflectionType* type, const ReflectionTypeMiscSoa* soa) {
			return type->fields[soa->parallel_streams[0]].info.stream_alignment;
		}

		ECS_INLINE bool IsBlittable(const ReflectionType* type) {
			return type->is_blittable;
		}

		ECS_INLINE bool IsBlittableWithPointer(const ReflectionType* type) {
			return type->is_blittable_with_pointer;
		}

		ECS_INLINE bool IsBoolBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool2 || type == ReflectionBasicFieldType::Bool3 || type == ReflectionBasicFieldType::Bool4;
		}

		ECS_INLINE bool IsBoolBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool || IsBoolBasicTypeMultiComponent(type);
		}

		// Checks for float2, float3, float4
		ECS_INLINE bool IsFloatBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Float2 || type == ReflectionBasicFieldType::Float3 || type == ReflectionBasicFieldType::Float4;
		}

		// Checks for float, float2, float3, float4
		ECS_INLINE bool IsFloatBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Float || IsFloatBasicTypeMultiComponent(type);
		}

		// Checks for double2, double3, double4
		ECS_INLINE bool IsDoubleBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Double2 || type == ReflectionBasicFieldType::Double3 || type == ReflectionBasicFieldType::Double4;
		}

		// Checks for double, double2, double3, double4
		ECS_INLINE bool IsDoubleBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Double || IsDoubleBasicTypeMultiComponent(type);
		}

		// Checks for float, float2, float3, float4, double, double2, double3, double4
		ECS_INLINE bool IsFloating(ReflectionBasicFieldType type) {
			return IsFloatBasicType(type) || IsDoubleBasicType(type);
		}

		ECS_INLINE bool IsUserDefined(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::UserDefined;
		}

		ECS_INLINE bool IsPointer(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::Pointer;
		}

		ECS_INLINE bool IsPointerWithSoA(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::Pointer || type == ReflectionStreamFieldType::PointerSoA;
		}

		ECS_INLINE bool IsEnum(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Enum;
		}

		ECS_INLINE bool IsArray(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::BasicTypeArray;
		}

		ECS_INLINE bool IsStream(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::Stream || type == ReflectionStreamFieldType::CapacityStream
				|| type == ReflectionStreamFieldType::ResizableStream;
		}

		ECS_INLINE bool IsStreamWithSoA(ReflectionStreamFieldType type) {
			return IsStream(type) || type == ReflectionStreamFieldType::PointerSoA;
		}

	}

}