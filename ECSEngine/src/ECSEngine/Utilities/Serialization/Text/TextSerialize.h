#pragma once
#include "../../../Core.h"
#include "../../Reflection/ReflectionTypes.h"
#include "TextSerializeFields.h"

namespace ECSEngine {
	
	// -----------------------------------------------------------------------------------------

	// It will serialize into a memory buffer and then commit to the file
	// SettingsAllocator nullptr means use malloc
	ECSENGINE_API bool TextSerialize(
		Reflection::ReflectionType type,
		const void* data,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void TextSerialize(
		Reflection::ReflectionType type,
		const void* data,
		uintptr_t& stream
	);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API size_t TextSerializeSize(Reflection::ReflectionType type, const void* data);

	// -----------------------------------------------------------------------------------------

	// If one field cannot be deserialized (i.e. a value is not specified, incorrect type of data), 
	// then that field will be omitted from the deserialization. It is recommended you set the values before hand
	// With sentinel values and you check if they changed - if they did not that means the field could not be deserialized
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		Reflection::ReflectionType type,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// -----------------------------------------------------------------------------------------

	// If one field cannot be deserialized (i.e. a value is not specified, incorrect type of data), 
	// then that field will be omitted from the deserialization. It is recommended you set the values before hand
	// With sentinel values and you check if they changed - if they did not that means the field could not be deserialized
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		Reflection::ReflectionType type,
		void* address,
		CapacityStream<void>& memory_pool,
		uintptr_t& ptr_to_read
	);

	// -----------------------------------------------------------------------------------------

	// If one field cannot be deserialized (i.e. a value is not specified, incorrect type of data), 
	// then that field will be omitted from the deserialization. It is recommended you set the values before hand
	// With sentinel values and you check if they changed - if they did not that means the field could not be deserialized
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		Reflection::ReflectionType type,
		void* address,
		AllocatorPolymorphic pointer_allocator,
		Stream<wchar_t> file,
		AllocatorPolymorphic file_allocator = { nullptr }
	);

	// -----------------------------------------------------------------------------------------

	// If one field cannot be deserialized (i.e. a value is not specified, incorrect type of data), 
	// then that field will be omitted from the deserialization. It is recommended you set the values before hand
	// With sentinel values and you check if they changed - if they did not that means the field could not be deserialized
	ECSENGINE_API ECS_TEXT_DESERIALIZE_STATUS TextDeserialize(
		Reflection::ReflectionType type,
		void* address,
		AllocatorPolymorphic pointer_allocator,
		uintptr_t& ptr_to_read
	);

	// -----------------------------------------------------------------------------------------

	// It returns the total amount of pointer data inside that file - it does not validate if that data
	// has a match inside the type
	ECSENGINE_API size_t TextDeserializeSize(
		uintptr_t ptr_to_read
	);

	// -----------------------------------------------------------------------------------------


}