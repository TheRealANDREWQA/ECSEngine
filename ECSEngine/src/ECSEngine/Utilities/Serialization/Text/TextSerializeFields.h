#pragma once
#include "../../../Core.h"
#include "../DeserializeTable.h"
#include "../../Reflection/ReflectionTypes.h"

namespace ECSEngine {

#define ECS_END_TEXT_SERIALIZE_STRING "END_SERIALIZE\n"

	// Data.size represents the element count for a stream type
	struct TextSerializeField {
		Stream<void> data;
		const char* name;
		Reflection::ReflectionBasicFieldType basic_type;
		bool is_stream = false;
		bool character_stream = false;
	};

	// Basic type for integer will always be 64 bit long (signed or unsigned)
	struct TextDeserializeField {
		TextDeserializeField() {}

		Stream<void> pointer_data;
		const char* name;
		Reflection::ReflectionBasicFieldType basic_type;
		// Represents the index of the field inside the file - if some fields are skipped this will reflect it
		// and decisions can be made based on it - if to abort the file deserialization, reset, etc...
		unsigned int in_file_field_index;
		// Embedd the value for normal fields directly into this type
		union {
			uint64_t unsigned_;
			int64_t signed_;
			double floating;
			ulong2 unsigned2;
			long2 signed2;
			double2 floating2;
			ulong3 unsigned3;
			long3 signed3;
			double3 floating3;
			ulong4 unsigned4;
			long4 signed4;
			double4 floating4;
			Stream<char> ascii_characters;
			Stream<wchar_t> wide_characters;
		};
	};

	// -----------------------------------------------------------------------------------------

	// It will serialize into a memory buffer and then commit to the file
	// Allocator nullptr means use malloc
	ECSENGINE_API bool TextSerializeFields(
		Stream<TextSerializeField> fields,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void TextSerializeFields(
		Stream<TextSerializeField> fields,
		uintptr_t& stream
	);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API size_t TextSerializeFieldsSize(Stream<TextSerializeField> fields);

	// -----------------------------------------------------------------------------------------
	
	enum ECS_TEXT_DESERIALIZE_STATUS : unsigned char {
		ECS_TEXT_DESERIALIZE_OK = 0,
		ECS_TEXT_DESERIALIZE_COULD_NOT_READ = 1 << 0,
		ECS_TEXT_DESERIALIZE_FIELD_DATA_MISSING = 1 << 1,
		ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS = 1 << 2
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_TEXT_DESERIALIZE_STATUS);

	// It will read the whole file into a temporary memory buffer and then deserialize from memory
	// Allocator nullptr means use malloc
	// Returns whether or not an error has occured during reading the file. If one field cannot be deserialized 
	// (i.e. a value is not specified, incorrect type of data), then that field will be omitted from the fields stream
	// Names will also be allocated from the pool
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// -----------------------------------------------------------------------------------------

	// It will read the whole file into a temporary memory buffer and then deserialize from memory
	// Allocator nullptr means use malloc
	// Returns whether or not an error has occured during reading the file. If one field cannot be deserialized 
	// (i.e. a value is not specified, incorrect type of data), then that field will be omitted from the fields stream
	// The names of the fields will also be allocated from the pointer allocator
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields,
		AllocatorPolymorphic pointer_allocator,
		Stream<wchar_t> file,
		AllocatorPolymorphic file_allocator = { nullptr }
	);

	// -----------------------------------------------------------------------------------------

	// If one field cannot be deserialized (i.e. a value is not specified, incorrect type of data), 
	// then that field will be omitted from the fields stream
	// The names will also be allocated from the pool
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields,
		CapacityStream<void>& memory_pool,
		uintptr_t& stream
	);

	// -----------------------------------------------------------------------------------------

	// If one field cannot be deserialized (i.e. a value is not specified, incorrect type of data), 
	// then that field will be omitted from the fields stream
	// Names will be allocated from inside the allocator separately from the pointer data
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserializeFields(
		CapacityStream<TextDeserializeField>& fields,
		AllocatorPolymorphic pointer_allocator,
		uintptr_t& stream
	);

	// -----------------------------------------------------------------------------------------

	// Total pointer data size inside the file
	ECSENGINE_API size_t TextDeserializeFieldsSize(
		uintptr_t stream
	);

	// -----------------------------------------------------------------------------------------

	// Returns the count of serialized fields
	ECSENGINE_API size_t TextDeserializeFieldsCount(
		uintptr_t stream
	);

	// -----------------------------------------------------------------------------------------
}