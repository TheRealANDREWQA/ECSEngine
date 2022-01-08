#pragma once
#include "../Reflection/Reflection.h"
#include "../../Allocators/AllocatorTypes.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	// Serializes into a temporary memory buffer then commits to the file
	// Allocator nullptr means use malloc
	ECSENGINE_API bool SerializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// Serializes into a temporary memory buffer then commits to the file
	// Allocator nullptr means use malloc
	ECSENGINE_API bool SerializeIndex(
		ReflectionType type,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// Serializes into memory
	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data
	ECSENGINE_API void SerializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		const void* data
	);

	// Serializes into memory
	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data
	ECSENGINE_API void SerializeIndex(
		ReflectionType type,
		uintptr_t& stream,
		const void* data
	);

	ECSENGINE_API size_t SerializeIndexSize(
		const ReflectionManager* reflection,
		const char* type_name,
		const void* data
	);

	ECSENGINE_API size_t SerializeIndexSize(
		ReflectionType type,
		const void* data
	);

	// It reads the whole file into a temporary buffer and then deserializes from memory
	// Allocator nullptr means use malloc
	// It returns the amount of pointer data used from the memory pool - a size of -1 means failure
	ECSENGINE_API size_t DeserializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> pointers,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// It reads the whole file into a temporary buffer and then deserializes from memory
	// Allocator nullptr means use malloc
	// It returns the amount of pointer data used from the memory pool - a size of -1 means failure
	ECSENGINE_API size_t DeserializeIndex(
		ReflectionType type,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> pointers,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data 
	ECSENGINE_API size_t DeserializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> pointers
	);

	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data
	ECSENGINE_API size_t DeserializeIndex(
		ReflectionType type,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> pointers
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeIndexSize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t data
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeIndexSize(
		ReflectionType type,
		uintptr_t data
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeIndexSize(
		Stream<const char*> fields,
		uintptr_t data
	);
}