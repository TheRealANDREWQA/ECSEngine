#pragma once
#include "../Core.h"
#include "../Containers/HashTable.h"
#include "../Allocators/LinearAllocator.h"

namespace ECSEngine {

	struct GlobalMemoryManager;
	struct MemoryManager;

	struct SystemManagerSetting {
		Stream<char> name;
		void* data;
		size_t byte_size;
	};

	struct ECSENGINE_API SystemManager {
		SystemManager() = default;
		SystemManager(GlobalMemoryManager* global_memory);

		// Returns the pointer stored in the hash table
		void* BindData(Stream<char> identifier, const void* data, size_t data_size = 0);

		// Returns the pointer stored in the hash table
		void* BindTemporaryData(Stream<char> identifier, const void* data, size_t data_size = 0);

		// The names will be allocated separately
		void BindSystemSettings(Stream<char> system_name, Stream<SystemManagerSetting> settings);
		
		// Clears all the memory used and the hash tables (the memory is still being used
		void Clear();

		// Clears any temporary tables
		void ClearFrame();

		// Frees the memory of the allocators
		void FreeMemory();

		// It crashes if it doesn't exist
		void* GetData(Stream<char> identifier) const;

		// It crashes if it doesn't exist
		void* GetSystemSettings(Stream<char> system_name, Stream<char> setting_name) const;

		// It crashes if it doesn't exist
		void* GetTemporaryData(Stream<char> identifier) const;

		// It returns nullptr if it doesn't exist
		void* TryGetData(Stream<char> identifier) const;

		// It returns nullptr if it doesn't exist
		void* TryGetSystemSettings(Stream<char> system_name, Stream<char> settings_name) const;

		// It returns nullptr if it doesn't exist
		void* TryGetTemporaryData(Stream<char> identifier) const;

		void RemoveData(Stream<char> identifier);

		void RemoveSystemSetting(Stream<char> system_name);

		MemoryManager* allocator;
		LinearAllocator temporary_allocator;
		HashTableDefault<void*> data_table;
		HashTableDefault<void*> temporary_table;
		HashTableDefault<Stream<SystemManagerSetting>> system_settings;
	};

}