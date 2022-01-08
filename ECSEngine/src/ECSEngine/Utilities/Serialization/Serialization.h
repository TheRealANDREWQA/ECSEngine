#pragma once
#include "../Reflection/Reflection.h"
#include "../File.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	constexpr size_t ECS_PLATFORM_WIN64_DX11 = 1 << 0;
	constexpr size_t ECS_PLATFORM_WIN64_DX11_STRING_INDEX = 0;
	constexpr size_t ECS_PLATFORM_ALL = ECS_PLATFORM_WIN64_DX11;

	extern ECSENGINE_API const char* ECS_PLATFORM_STRINGS[];

	// -----------------------------------------------------------------------------------------
	
	// It will serialize into a memory buffer and then commit to the file
	// Allocator nullptr means use malloc
	ECSENGINE_API bool Serialize(
		const ReflectionManager* reflection,
		const char* type_name, 
		Stream<wchar_t> file, 
		const void* data,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// It will serialize into a memory buffer and then commit to the file
	// Allocator nullptr means use malloc
	ECSENGINE_API bool Serialize(
		ReflectionType type,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator = {nullptr}
	);

	ECSENGINE_API void Serialize(
		const ReflectionManager* reflection, 
		const char* type_name,
		uintptr_t& stream, 
		const void* data
	);

	ECSENGINE_API void Serialize(
		ReflectionType type,
		uintptr_t& stream,
		const void* data
	);

	ECSENGINE_API size_t SerializeSize(
		const ReflectionManager* reflection,
		const char* type_name,
		const void* data
	);

	ECSENGINE_API size_t SerializeSize(ReflectionType type, const void* data);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation should be used for it
	// It will read the whole file into a temporary memory buffer and then deserialize from memory
	// Allocator nullptr means use malloc
	// A value of -1 as return means failure to open and read the file
	ECSENGINE_API size_t Deserialize(
		const ReflectionManager* reflection, 
		const char* type_name,
		Stream<wchar_t> file, 
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation should be used for it
	// It will read the whole file into a temporary memory buffer and then deserialize from memory
	// Allocator nullptr means use malloc
	// A value of -1 as return means failure to open and read the file
	ECSENGINE_API size_t Deserialize(
		ReflectionType type,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation should be used for it
	// A value of -1 as return means failure to open and read the file
	ECSENGINE_API size_t Deserialize(
		const ReflectionManager* reflection, 
		const char* type_name, 
		uintptr_t& ptr_to_read,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy
	);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation should be used for it
	// A value of -1 as return means failure to open and read the file
	ECSENGINE_API size_t Deserialize(
		ReflectionType type,
		uintptr_t& ptr_to_read,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy
	);

	ECSENGINE_API size_t DeserializeSize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t ptr_to_read,
		void* address
	);

	ECSENGINE_API size_t DeserializeSize(
		ReflectionType type,
		uintptr_t ptr_to_read,
		void* address
	);

	ECSENGINE_API size_t DeserializeSize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t ptr_to_read
	);

	ECSENGINE_API size_t DeserializeSize(
		ReflectionType type,
		uintptr_t ptr_to_read
	);

	// Only accounts for their value range to be compliant
	ECSENGINE_API bool ValidateEnums(
		const ReflectionManager* reflection,
		const char* type_name,
		const void* data
	);

	// Only accounts for their value range to be compliant
	ECSENGINE_API bool ValidateEnums(
		const ReflectionManager* reflection,
		ReflectionType type,
		const void* data
	);

}