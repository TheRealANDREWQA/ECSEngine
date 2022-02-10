#pragma once
#include "../../Reflection/ReflectionTypes.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	// Serializes into a temporary memory buffer, then commits to the file
	// Allocator nullptr means use malloc
	ECSENGINE_API bool Serialize(
		Reflection::ReflectionType type,
		const void* data,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// Serializes into memory
	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data
	ECSENGINE_API void Serialize(
		Reflection::ReflectionType type,
		const void* data,
		uintptr_t& stream
	);

	ECSENGINE_API size_t SerializeSize(
		Reflection::ReflectionType type,
		const void* data
	);

	// It reads the whole file into a temporary buffer and then deserializes from memory
	// The return value is a pointer containing the file data. It must be deallocated manually if not nullptr!!
	// The pointer data will point to the values inside the stream - are not stable
	ECSENGINE_API void* Deserialize(
		Reflection::ReflectionType type,
		void* address,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data. The pointer data will point to the values inside the stream - are not stable
	ECSENGINE_API size_t Deserialize(
		Reflection::ReflectionType type,
		void* address,
		uintptr_t& stream
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSize(
		Reflection::ReflectionType type,
		uintptr_t data
	);

}