#pragma once
#include "../Core.h"
#include "../Containers/HashTable.h"
#include "../Allocators/LinearAllocator.h"
#include "../Containers/DataPointer.h"
#include "../Allocators/MemoryManager.h"

namespace ECSEngine {

	struct SystemManagerSetting {
		Stream<char> name;
		void* data;
		size_t byte_size;
	};

	struct ECSENGINE_API SystemManager {
		SystemManager() = default;
		SystemManager(GlobalMemoryManager* global_memory);

		ECS_INLINE AllocatorPolymorphic Allocator() {
			return allocator;
		}

		ECS_INLINE AllocatorPolymorphic TemporaryAllocator() {
			return &temporary_allocator;
		}

		// Returns the pointer stored in the hash table
		// If the data_size is 0, when removing, if the data was allocated using
		// the allocator from here it will deallocate that automatically
		void* BindData(Stream<char> identifier, const void* data, size_t data_size = 0);

		// Returns the pointer of the allocated data that you must fill in
		void* BindDataNoCopy(Stream<char> identifier, size_t data_size);

		// Returns the pointer stored in the hash table
		void* BindTemporaryData(Stream<char> identifier, const void* data, size_t data_size = 0);

		void BindSystemSettings(Stream<char> system_name, Stream<SystemManagerSetting> settings);
		
		// Clears all the memory used and the hash tables (the memory is still being used)
		void Clear();

		// Clears any temporary tables
		void ClearFrame();

		// Clears all the memory used by the system settings
		void ClearSystemSettings();

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

		// Remaps a pointer stored directly in the data table with another value, when the identifier
		// Is not known
		void RemapData(void* old_pointer, void* new_pointer);

		// If the data_size was 0 when the data was bound and if the data was allocated using
		// the allocator from here, then it will deallocate that automatically
		void RemoveData(Stream<char> identifier);

		void RemoveSystemSetting(Stream<char> system_name);

		MemoryManager* allocator;
		LinearAllocator temporary_allocator;
		HashTableDefault<DataPointer> data_table;
		HashTableDefault<void*> temporary_table;
		HashTableDefault<Stream<SystemManagerSetting>> system_settings;
	};

}