#pragma once
#include "../Reflection/Reflection.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	// Make pointer data size equal to 0 if it is not a pointer
	// Make data size equal to 0 if not using Reflection
	// Data buffer should be a pointer to the address that needs to be filled in
	// e.g. struct int { unsigned int x }; &x
	struct SerializeIndexPointer {
		Stream<void> data;
		Stream<void> pointer_data;
		const char* name;
	};

	ECSENGINE_API bool SerializeIndex(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ofstream& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API bool SerializeIndex(
		ReflectionType type,
		std::ofstream& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API void SerializeIndex(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		CapacityStream<void>& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API void SerializeIndex(
		ReflectionType type,
		CapacityStream<void>& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API bool SerializeIndex(std::ofstream& stream, Stream<SerializeIndexPointer> pointers);

	ECSENGINE_API void SerializeIndex(CapacityStream<void>& stream, Stream<SerializeIndexPointer> pointers);

	ECSENGINE_API size_t SerializeIndexSize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API size_t SerializeIndexSize(
		ReflectionType type,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API size_t SerializeIndexSize(Stream<SerializeIndexPointer> pointers);

	ECSENGINE_API size_t DeserializeIndex(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer> pointers,
		bool* ECS_RESTRICT success_status = nullptr
	);

	ECSENGINE_API size_t DeserializeIndex(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer> pointers,
		bool* ECS_RESTRICT success_status = nullptr
	);

	ECSENGINE_API size_t DeserializeIndex(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer> pointers
	);

	ECSENGINE_API size_t DeserializeIndex(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer> pointers
	);

	ECSENGINE_API bool DeserializeIndex(
		std::ifstream& stream,
		Stream<SerializeIndexPointer> pointers
	);

	ECSENGINE_API bool DeserializeIndex(
		std::ifstream& stream,
		Stream<SerializeIndexPointer> pointers,
		CapacityStream<void>& ECS_RESTRICT memory_pool
	);

	ECSENGINE_API void DeserializeIndex(
		uintptr_t& stream,
		Stream<SerializeIndexPointer> pointers
	);

	ECSENGINE_API void DeserializeIndex(
		uintptr_t& stream,
		Stream<SerializeIndexPointer> pointers,
		CapacityStream<void>& ECS_RESTRICT memory_pool
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeIndexSize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t data
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeIndexSize(
		ReflectionType type,
		uintptr_t data
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeIndexSize(
		Stream<SerializeIndexPointer> pointers,
		uintptr_t data
	);

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeIndexSize(
		Stream<const char*> fields,
		uintptr_t data
	);
}