// ECS_REFLECT
#pragma once
#include "../Reflection/Reflection.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	constexpr size_t ECS_PLATFORM_WIN64_DX11 = 1 << 0;
	constexpr size_t ECS_PLATFORM_WIN64_DX11_STRING_INDEX = 0;
	constexpr size_t ECS_PLATFORM_ALL = ECS_PLATFORM_WIN64_DX11;

	extern ECSENGINE_API const char* ECS_PLATFORM_STRINGS[];

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void ConstructPointerFieldsForType(Stream<function::CopyPointer>& ECS_RESTRICT pointer_fields, ReflectionType type, const void* ECS_RESTRICT data);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Write(std::ofstream& ECS_RESTRICT stream, const void* ECS_RESTRICT data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Write(CapacityStream<void>& ECS_RESTRICT stream, const void* ECS_RESTRICT data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Write(std::ofstream& stream, Stream<void> data);

	// -----------------------------------------------------------------------------------------
	
	ECSENGINE_API void Write(CapacityStream<void>& stream, Stream<void> data);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Read(std::ifstream& ECS_RESTRICT stream, void* ECS_RESTRICT data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Read(std::istream& stream, CapacityStream<void>& data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Read(CapacityStream<void>& ECS_RESTRICT stream, void* ECS_RESTRICT data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Read(uintptr_t& ECS_RESTRICT stream, void* ECS_RESTRICT data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Read(uintptr_t& stream, CapacityStream<void>& data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Ignore(std::ifstream& stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Ignore(CapacityStream<void>& stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Ignore(uintptr_t& stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Serialize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name, 
		std::ofstream& ECS_RESTRICT stream, 
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API bool Serialize(
		ReflectionType type,
		std::ofstream& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API void Serialize(
		const ReflectionManager* ECS_RESTRICT reflection, 
		const char* ECS_RESTRICT type_name,
		CapacityStream<void>& ECS_RESTRICT stream, 
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API void Serialize(
		ReflectionType type,
		CapacityStream<void>& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API size_t SerializeSize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		const void* ECS_RESTRICT data
	);

	ECSENGINE_API size_t SerializeSize(ReflectionType type, const void* ECS_RESTRICT data);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation 
	// should be used for it, success_status should be initialized with true
	ECSENGINE_API size_t Deserialize(
		const ReflectionManager* ECS_RESTRICT reflection, 
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream, 
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation 
	// should be used for it, success_status should be initialized with true
	ECSENGINE_API size_t Deserialize(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation 
	// should be used for it, success_status should be initialized with true
	ECSENGINE_API size_t Deserialize(
		const ReflectionManager* ECS_RESTRICT reflection, 
		const char* ECS_RESTRICT type_name, 
		uintptr_t& ECS_RESTRICT ptr_to_read,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy
	);

	// Returns the count of bytes written in the memory pool; type_pointers_to_copy will be filled
	// if not nullptr with pointers to the memory pool data for each pointer field; stack allocation 
	// should be used for it, success_status should be initialized with true
	ECSENGINE_API size_t Deserialize(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT ptr_to_read,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy
	);

	ECSENGINE_API size_t DeserializeSize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT ptr_to_read,
		void* ECS_RESTRICT address
	);

	ECSENGINE_API size_t DeserializeSize(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT ptr_to_read,
		void* ECS_RESTRICT address
	);

	ECSENGINE_API size_t DeserializeSize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t ptr_to_read
	);

	ECSENGINE_API size_t DeserializeSize(
		ReflectionType type,
		uintptr_t ptr_to_read
	);

	// Only accounts for their value range to be compliant
	ECSENGINE_API bool ValidateEnums(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		const void* ECS_RESTRICT data
	);

	// Only accounts for their value range to be compliant
	ECSENGINE_API bool ValidateEnums(
		const ReflectionManager* ECS_RESTRICT reflection,
		ReflectionType type,
		const void* ECS_RESTRICT data
	);

}