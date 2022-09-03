#pragma once
#include "InternalStructures.h"

namespace ECSEngine {

	struct SerializeEntityManagerComponentData {
		const void* components;
		unsigned int count;
		void* buffer;
		size_t buffer_capacity;
		void* extra_data;
	};

	// This functor can be used to extract only some fields or write some others for serialization out of components
	// It receives the buffer capacity and must check that the write size is contained inside the buffer capacity. If it doesn't,
	// It must return how many bytes it wants to write and it will be recalled with a new buffer
	// Must return how many bytes were written. Can return -1 to signal an error
	// This function receives the component data for an entire base archetype
	typedef unsigned int (*SerializeEntityManagerComponent)(SerializeEntityManagerComponentData* data);

	struct SerializeEntityManagerHeaderComponentData {
		void* buffer;
		size_t buffer_capacity;
		void* extra_data;
	};

	// It shared the extra data with the extract function
	// Used to write a header in the file
	// Must return how many bytes were written. Can return -1 to signal an error
	typedef unsigned int (*SerializeEntityManagerHeaderComponent)(SerializeEntityManagerHeaderComponentData* data);

	struct DeserializeEntityManagerComponentData {
		const void* file_data;
		unsigned int count;
		unsigned char version;
		void* components;
		void* extra_data;
	};

	// This functor is used to reconstruct the data from the file. It must return true if the data is valid, else false
	// This function receives the component data for an entire base archetype
	typedef bool (*DeserializeEntityManagerComponent)(DeserializeEntityManagerComponentData* data);

	struct DeserializeEntityManagerHeaderComponentData {
		const void* file_data;
		unsigned int size;
		unsigned char version;
		void* extra_data;
	};
	
	// It shared the extra data with the extract function
	// Used to extract the component header
	// It must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerHeaderComponent)(DeserializeEntityManagerHeaderComponentData* data);

	struct SerializeEntityManagerSharedComponentData {
		SharedInstance instance;
		const void* component_data;
		void* buffer;
		size_t buffer_capacity;
		void* extra_data;
	};

	// This functor can be used to extract only some fields or write some other for serialization out of shared components
	// It receives the buffer capacity and must check that the write size is contained inside the buffer capacity. If it doesn't,
	// It must return how many bytes it wants to write and it will be recalled with a new buffer
	// Must returns how many bytes were written
	typedef unsigned int (*SerializeEntityManagerSharedComponent)(SerializeEntityManagerSharedComponentData* data);

	struct SerializeEntityManagerHeaderSharedComponentData {
		void* buffer;
		size_t buffer_capacity;
		void* extra_data;
	};

	// It shared the extra data with the extract function
	// Used to write a header in the file
	// Must return how many bytes were written.
	typedef unsigned int (*SerializeEntityManagerHeaderSharedComponent)(SerializeEntityManagerHeaderSharedComponentData* data);

	struct DeserializeEntityManagerSharedComponentData {
		SharedInstance instance;
		const void* file_data;
		unsigned int data_size;
		unsigned char version;
		void* component;
		void* extra_data;
	};

	// This functor is used to reconstruct the data from the file. It must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerSharedComponent)(DeserializeEntityManagerSharedComponentData* data);

	struct DeserializeEntityManagerHeaderSharedComponentData {
		const void* file_data;
		unsigned int data_size;
		unsigned char version;
		void* extra_data;
	};

	// It shared the extra data with the extract function
	// Used to extract the header from the file. Must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerHeaderSharedComponent)(DeserializeEntityManagerHeaderSharedComponentData* data);

	// If the name is specified, then it will match the component using the name instead of the index
	struct SerializeEntityManagerComponentInfo {
		SerializeEntityManagerComponent function;
		SerializeEntityManagerHeaderComponent header_function = nullptr;
		unsigned char version;
		void* extra_data;
		Stream<char> name = { nullptr, 0 };
	};

	// If the name is specified, then it will match the component using the name instead of the index
	struct SerializeEntityManagerSharedComponentInfo {
		SerializeEntityManagerSharedComponent function;
		SerializeEntityManagerHeaderSharedComponent header_function = nullptr;
		unsigned char version;
		void* extra_data;
		Stream<char> name = { nullptr, 0 };
	};

	// If the name is specified, then it will match the component using the name instead of the index
	struct DeserializeEntityManagerComponentInfo {
		DeserializeEntityManagerComponent function;
		DeserializeEntityManagerHeaderComponent header_function = nullptr;
		void* extra_data;
		Stream<char> name = { nullptr, 0 };
	};

	// If the name is specified, then it will match the component using the name instead of the index
	struct DeserializeEntityManagerSharedComponentInfo {
		DeserializeEntityManagerSharedComponent function;
		DeserializeEntityManagerHeaderSharedComponent header_function = nullptr;
		void* extra_data;
		Stream<char> name = { nullptr, 0 };
	};

	typedef HashTable<SerializeEntityManagerComponentInfo, Component, HashFunctionPowerOfTwo> SerializeEntityManagerComponentTable;

	typedef HashTable<SerializeEntityManagerSharedComponentInfo, Component, HashFunctionPowerOfTwo> SerializeEntityManagerSharedComponentTable;

	typedef HashTable<DeserializeEntityManagerComponentInfo, Component, HashFunctionPowerOfTwo> DeserializeEntityManagerComponentTable;

	typedef HashTable<DeserializeEntityManagerSharedComponentInfo, Component, HashFunctionPowerOfTwo> DeserializeEntityManagerSharedComponentTable;

	enum ECS_DESERIALIZE_ENTITY_MANAGER_STATUS {
		ECS_DESERIALIZE_ENTITY_MANAGER_OK = 0,
		ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_OPEN_FILE = 1 << 0,
		ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ = 1 << 1,
		ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID = 1 << 2,
		ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID = 1 << 3,
		ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING = 1 << 4
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_DESERIALIZE_ENTITY_MANAGER_STATUS);

}