#pragma once
#include "../Reflection/Reflection.h"
#include "../../Allocators/AllocatorTypes.h"

ECS_CONTAINERS;


namespace ECSEngine {

	using namespace Reflection;

	struct SerializeSectionData {
		Stream<void> data;
		const char* name;
	};

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ofstream& stream,
		const void* ECS_RESTRICT data
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeSection(
		ReflectionType type,
		std::ofstream& stream,
		const void* ECS_RESTRICT data
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		CapacityStream<void>& stream,
		const void* ECS_RESTRICT data
	);

	// -------------------------------------------------------------------------------------------------------------------
	
	ECSENGINE_API void SerializeSection(
		ReflectionType type,
		CapacityStream<void>& stream,
		const void* ECS_RESTRICT data
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeSection(std::ofstream& stream, Stream<SerializeSectionData> data);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeSection(CapacityStream<void>& stream, Stream<SerializeSectionData> data);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeSectionSize(ReflectionType type, const void* data);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeSectionSize(Stream<SerializeSectionData> data);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, success_status must be true before calling this
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, success_status must be true before calling this
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, success_status must be true before calling this
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, preinitialize this to a specified value
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		std::ifstream& ECS_RESTRICT stream,
		Stream<const char*> section_names,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		std::ifstream& stream,
		Stream<SerializeSectionData> data,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<const char*> section_names,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<SerializeSectionData> data,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<SerializeSectionData> data,
		void* allocator,
		AllocatorType allocator_type
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSection(
		std::ifstream& stream,
		Stream<SerializeSectionData> data,
		void* allocator,
		AllocatorType allocator_type, 
		bool* success = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionCount(uintptr_t stream);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionCount(std::ifstream& stream);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionSize(
		uintptr_t data,
		Stream<SerializeSectionData> serialize_data
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionSize(
		uintptr_t data,
		Stream<const char*> section_names
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionSize(
		uintptr_t data,
		ReflectionType type
	);

}