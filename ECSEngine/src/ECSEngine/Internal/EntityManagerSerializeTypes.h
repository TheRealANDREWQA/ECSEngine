#pragma once
#include "InternalStructures.h"

namespace ECSEngine {

	// This functor can be used to extract only some fields or write some others for serialization out of components
	// It receives the buffer capacity and must check that the write size is contained inside the buffer capacity. If it doesn't,
	// It must return how many bytes it wants to write and it will be recalled with a new buffer
	// Must return how many bytes were written
	typedef unsigned int (*SerializeEntityManagerExtractComponent)(const void* component, void* buffer, size_t buffer_capacity, void* extra_data);

	// This functor is used to reconstruct the data from the file. It must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerExtractComponent)(const void* file_data, unsigned int data_size, void* component, void* extra_data);

	// This functor can be used to extract only some fields or write some other for serialization out of shared components
	// It receives the buffer capacity and must check that the write size is contained inside the buffer capacity. If it doesn't,
	// It must return how many bytes it wants to write and it will be recalled with a new buffer
	// Must returns how many bytes were written
	typedef unsigned int (*SerializeEntityManagerExtractSharedComponent)(SharedInstance instance, const void* component_data, void* buffer, size_t buffer_capacity, void* extra_data);

	// This functor is used to reconstruct the data from the file. It must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerExtractSharedComponent)(SharedInstance instance, const void* file_data, unsigned int data_size, void* component, void* extra_data);

#define ECS_SERIALIZE_ENTITY_MANAGER_EXTRACT_COMPONENT(function_name) unsigned int function_name(const void* component, void* buffer, size_t buffer_capacity, void* extra_data)
#define ECS_SERIALIZE_ENTITY_MANAGER_EXTRACT_SHARED_COMPONENT(function_name) unsigned int function_name(SharedInstance instance, const void* component_data, void* buffer, size_t buffer_capacity, void* extra_data)

#define ECS_DESERIALIZE_ENTITY_MANAGER_EXTRACT_COMPONENT(function_name) void function_name(const void* file_data, unsigned int data_size, void* component, void* extra_data)
#define ECS_DESERIALIZE_ENTITY_MANAGER_EXTRACT_SHARED_COMPONENT(function_name) void function_name(SharedInstance instance, const void* file_data, unsigned int data_size, void* component, void* extra_data)

	typedef HashTable<SerializeEntityManagerExtractComponent, Component, HashFunctionPowerOfTwo> SerializeEntityManagerComponentTable;

	typedef HashTable<SerializeEntityManagerExtractSharedComponent, Component, HashFunctionPowerOfTwo> SerializeEntityManagerSharedComponentTable;

	typedef HashTable<DeserializeEntityManagerExtractComponent, Component, HashFunctionPowerOfTwo> DeserializeEntityManagerComponentTable;

	typedef HashTable<DeserializeEntityManagerExtractSharedComponent, Component, HashFunctionPowerOfTwo> DeserializeEntityManagerSharedComponentTable;

	enum ECS_DESERIALIZE_ENTITY_MANAGER_STATUS {
		ECS_DESERIALIZE_ENTITY_MANAGER_OK = 0,
		ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_OPEN_FILE = 1 << 0,
		ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ = 1 << 1,
		ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID = 1 << 2,
		ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID = 1 << 3
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_DESERIALIZE_ENTITY_MANAGER_STATUS)

}