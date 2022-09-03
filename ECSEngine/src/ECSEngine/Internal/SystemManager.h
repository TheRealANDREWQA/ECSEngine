#pragma once
#include "../Core.h"
#include "../Containers/HashTable.h"

namespace ECSEngine {

	struct GlobalMemoryManager;
	struct MemoryManager;

	struct ECSENGINE_API SystemManager {
		struct SystemSettings {
			Stream<void*> data;
			Stream<char>* names;
		};

		SystemManager() = default;
		SystemManager(GlobalMemoryManager* global_memory);

		void BindSystemData(Stream<char> system_name, const void* data, size_t data_size = 0);

		void BindTemporaryTable(Stream<char> table_name, const void* data, size_t data_size = 0);

		// If the byte_sizes pointer is nullptr, it will only reference this data. The names will be allocated anyway
		void BindSystemSettings(Stream<char> system_name, Stream<void*> data, Stream<char>* names, size_t* byte_sizes = nullptr);
		
		// Clears any temporary tables
		void ClearFrame();

		// It crashes if it doesn't exist
		void* GetSystemData(Stream<char> system_name) const;

		// It crashes if it doesn't exist
		void* GetSystemSettings(Stream<char> system_name, Stream<char> setting_name) const;

		// It crashes if it doesn't exist
		void* GetTemporaryTable(Stream<char> table_name) const;

		MemoryManager* allocator;
		HashTableDefault<void*> system_data;
		HashTableDefault<void*> temporary_table;
		HashTableDefault<SystemSettings> system_settings;
	};

}