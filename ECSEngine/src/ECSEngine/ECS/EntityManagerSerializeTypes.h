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
		AllocatorPolymorphic component_allocator;
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
		AllocatorPolymorphic component_allocator;
	};

	// This functor is used to reconstruct the data from the file. It must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerSharedComponent)(DeserializeEntityManagerSharedComponentData* data);

	struct DeserializeEntityManagerHeaderSharedComponentData {
		const void* file_data;
		unsigned int size;
		unsigned char version;
		void* extra_data;
	};

	// It shared the extra data with the extract function
	// Used to extract the header from the file. Must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerHeaderSharedComponent)(DeserializeEntityManagerHeaderSharedComponentData* data);

	// At the moment, these can be handled like unique components with a count of 1
	typedef SerializeEntityManagerComponentData SerializeEntityManagerGlobalComponentData;
	typedef SerializeEntityManagerComponent SerializeEntityManagerGlobalComponent;
	typedef SerializeEntityManagerHeaderComponentData SerializeEntityManagerHeaderGlobalComponentData;
	typedef SerializeEntityManagerHeaderComponent SerializeEntityManagerHeaderGlobalComponent;
	typedef DeserializeEntityManagerComponentData DeserializeEntityManagerGlobalComponentData;
	typedef DeserializeEntityManagerComponent DeserializeEntityManagerGlobalComponent;
	typedef DeserializeEntityManagerHeaderComponentData DeserializeEntityManagerHeaderGlobalComponentData;
	typedef DeserializeEntityManagerHeaderComponent DeserializeEntityManagerHeaderGlobalComponent;

	// If the name is specified, then it will match the component using the name instead of the index
	struct SerializeEntityManagerComponentInfo {
		// The copy functions are used for stream deep copy
		ECS_INLINE size_t CopySize() const {
			return name.MemoryOf(name.size);
		}

		SerializeEntityManagerComponentInfo CopyTo(uintptr_t& ptr) {
			SerializeEntityManagerComponentInfo info;

			memcpy(&info, this, sizeof(info));
			info.name.InitializeAndCopy(ptr, name);

			return info;
		}

		SerializeEntityManagerComponent function;
		SerializeEntityManagerHeaderComponent header_function = nullptr;
		unsigned char version;
		void* extra_data;
		Stream<char> name = { nullptr, 0 };
	};

	// If the name is specified, then it will match the component using the name instead of the index
	struct SerializeEntityManagerSharedComponentInfo {
		// The copy functions are used for stream deep copy
		ECS_INLINE size_t CopySize() const {
			return name.MemoryOf(name.size);
		}

		SerializeEntityManagerSharedComponentInfo CopyTo(uintptr_t& ptr) {
			SerializeEntityManagerSharedComponentInfo info;

			memcpy(&info, this, sizeof(info));
			info.name.InitializeAndCopy(ptr, name);

			return info;
		}

		SerializeEntityManagerSharedComponent function;
		SerializeEntityManagerHeaderSharedComponent header_function = nullptr;
		unsigned char version;
		void* extra_data;
		Stream<char> name = { nullptr, 0 };
	};

	struct DeserializeEntityManagerComponentFixup {
		ComponentFunctions component_functions = {};
		unsigned short component_byte_size = 0;

		// Only valid for shared components
		SharedComponentCompareFunction compare_function = nullptr;
		Stream<void> compare_function_data = {};
	};

	// If the name is specified, then it will match the component using the name instead of the index
	struct DeserializeEntityManagerComponentInfo {
		// The copy functions are used for stream deep copy
		ECS_INLINE size_t CopySize() const {
			return name.CopySize() + component_fixup.component_functions.data.CopySize();
		}

		DeserializeEntityManagerComponentInfo CopyTo(uintptr_t& ptr) {
			DeserializeEntityManagerComponentInfo info;

			memcpy(&info, this, sizeof(info));
			info.name.InitializeAndCopy(ptr, name);
			info.component_fixup.component_functions.data.InitializeFromBuffer(ptr, component_fixup.component_functions.data.size);
			info.component_fixup.component_functions.data.CopyOther(component_fixup.component_functions.data.buffer, component_fixup.component_functions.data.size);

			return info;
		}

		DeserializeEntityManagerComponent function;
		DeserializeEntityManagerHeaderComponent header_function = nullptr;
		void* extra_data;	
		DeserializeEntityManagerComponentFixup component_fixup;
		Stream<char> name = { nullptr, 0 };
	};

	// If the name is specified, then it will match the component using the name instead of the index
	struct DeserializeEntityManagerSharedComponentInfo {
		// The copy functions are used for stream deep copy
		ECS_INLINE size_t CopySize() const {
			return name.CopySize() + component_fixup.component_functions.data.CopySize();
		}

		DeserializeEntityManagerSharedComponentInfo CopyTo(uintptr_t& ptr) {
			DeserializeEntityManagerSharedComponentInfo info;

			memcpy(&info, this, sizeof(info));
			info.name.InitializeAndCopy(ptr, name);
			info.component_fixup.component_functions.data.InitializeFromBuffer(ptr, component_fixup.component_functions.data.size);
			info.component_fixup.component_functions.data.CopyOther(component_fixup.component_functions.data.buffer, component_fixup.component_functions.data.size);

			return info;
		}

		DeserializeEntityManagerSharedComponent function;
		DeserializeEntityManagerHeaderSharedComponent header_function = nullptr;
		void* extra_data;
		DeserializeEntityManagerComponentFixup component_fixup;
		Stream<char> name = { nullptr, 0 };
	};

	typedef SerializeEntityManagerComponentInfo SerializeEntityManagerGlobalComponentInfo;
	typedef DeserializeEntityManagerComponentInfo DeserializeEntityManagerGlobalComponentInfo;

	typedef HashTable<SerializeEntityManagerComponentInfo, Component, HashFunctionPowerOfTwo> SerializeEntityManagerComponentTable;

	typedef HashTable<SerializeEntityManagerSharedComponentInfo, Component, HashFunctionPowerOfTwo> SerializeEntityManagerSharedComponentTable;

	typedef HashTable<SerializeEntityManagerGlobalComponentInfo, Component, HashFunctionPowerOfTwo> SerializeEntityManagerGlobalComponentTable;

	typedef HashTable<DeserializeEntityManagerComponentInfo, Component, HashFunctionPowerOfTwo> DeserializeEntityManagerComponentTable;

	typedef HashTable<DeserializeEntityManagerSharedComponentInfo, Component, HashFunctionPowerOfTwo> DeserializeEntityManagerSharedComponentTable;

	typedef HashTable<DeserializeEntityManagerGlobalComponentInfo, Component, HashFunctionPowerOfTwo> DeserializeEntityManagerGlobalComponentTable;

	enum ECS_DESERIALIZE_ENTITY_MANAGER_STATUS {
		ECS_DESERIALIZE_ENTITY_MANAGER_OK,
		ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_OPEN_FILE,
		ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ,
		ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID,
		ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID,
		ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING,
		ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_FIXUP_IS_MISSING,
		ECS_DESERIALIZE_ENTITY_MANAGER_TOO_MUCH_DATA,
		ECS_DESERIALIZE_ENTITY_MANAGER_FILE_IS_CORRUPT
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_DESERIALIZE_ENTITY_MANAGER_STATUS);

}