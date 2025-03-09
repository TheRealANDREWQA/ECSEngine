#pragma once
#include "InternalStructures.h"

namespace ECSEngine {

	struct WriteInstrument;
	struct ReadInstrument;

	struct SerializeEntityManagerComponentData {
		WriteInstrument* write_instrument;
		const void* components;
		void* extra_data;
		unsigned int count;
	};

	// This functor can be used to extract only some fields or write some others for serialization out of components
	// This function receives the component data for an entire base archetype. Should return true if it succeeded, else false
	typedef bool (*SerializeEntityManagerComponent)(SerializeEntityManagerComponentData* data);

	struct SerializeEntityManagerHeaderComponentData {
		WriteInstrument* write_instrument;
		void* extra_data;
	};

	// It shares the extra data with the extract function
	// Used to write a header in the file
	// Should return true if it succeeded, else false
	typedef bool (*SerializeEntityManagerHeaderComponent)(SerializeEntityManagerHeaderComponentData* data);

	struct DeserializeEntityManagerComponentData {
		ReadInstrument* read_instrument;
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
		ReadInstrument* read_instrument;
		unsigned int size;
		unsigned char version;
		void* extra_data;
	};
	
	// It shared the extra data with the extract function
	// Used to extract the component header
	// It must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerHeaderComponent)(DeserializeEntityManagerHeaderComponentData* data);

	struct SerializeEntityManagerSharedComponentData {
		WriteInstrument* write_instrument;
		SharedInstance instance;
		const void* component_data;
		void* extra_data;
	};

	// This functor can be used to extract only some fields or write some other for serialization out of shared components
	// Should return true if it succeeded, else false
	typedef bool (*SerializeEntityManagerSharedComponent)(SerializeEntityManagerSharedComponentData* data);

	struct SerializeEntityManagerHeaderSharedComponentData {
		WriteInstrument* write_instrument;
		void* extra_data;
	};

	// It shares the extra data with the extract function
	// Should return true if it succeeded, else false
	typedef bool (*SerializeEntityManagerHeaderSharedComponent)(SerializeEntityManagerHeaderSharedComponentData* data);

	struct DeserializeEntityManagerSharedComponentData {
		SharedInstance instance;
		ReadInstrument* read_instrument;
		unsigned int data_size;
		unsigned char version;
		void* component;
		void* extra_data;
		AllocatorPolymorphic component_allocator;
	};

	// This functor is used to reconstruct the data from the file. It must return true if the data is valid, else false
	typedef bool (*DeserializeEntityManagerSharedComponent)(DeserializeEntityManagerSharedComponentData* data);

	struct DeserializeEntityManagerHeaderSharedComponentData {
		ReadInstrument* read_instrument;
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
		ECS_INLINE size_t CopySize() const {
			return CopyableCopySize(component_functions.data) + CopyableCopySize(compare_entry.data);
		}

		ECS_INLINE DeserializeEntityManagerComponentFixup CopyTo(uintptr_t& ptr) const {
			DeserializeEntityManagerComponentFixup copy;
			memcpy(&copy, this, sizeof(copy));
			copy.component_functions.data = CopyableCopyTo(component_functions.data, ptr);
			copy.compare_entry.data = CopyableCopyTo(compare_entry.data, ptr);
			return copy;
		}

		ComponentFunctions component_functions = {};
		unsigned short component_byte_size = 0;

		// Only valid for shared components
		SharedComponentCompareEntry compare_entry = {};
	};

	// If the name is specified, then it will match the component using the name instead of the index
	struct DeserializeEntityManagerComponentInfo {
		// The copy functions are used for stream deep copy
		ECS_INLINE size_t CopySize() const {
			return name.CopySize() + component_fixup.CopySize();
		}

		DeserializeEntityManagerComponentInfo CopyTo(uintptr_t& ptr) {
			DeserializeEntityManagerComponentInfo info;

			memcpy(&info, this, sizeof(info));
			info.name.InitializeAndCopy(ptr, name);
			info.component_fixup = component_fixup.CopyTo(ptr);

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
			return name.CopySize() + component_fixup.CopySize();
		}

		DeserializeEntityManagerSharedComponentInfo CopyTo(uintptr_t& ptr) {
			DeserializeEntityManagerSharedComponentInfo info;

			memcpy(&info, this, sizeof(info));
			info.name.InitializeAndCopy(ptr, name);
			info.component_fixup = component_fixup.CopyTo(ptr);
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