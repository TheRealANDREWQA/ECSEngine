#pragma once
#include "../Core.h"
#include "../Containers/HashTable.h"
#include "Multithreading\TaskSchedulerTypes.h"

namespace ECSEngine {

	struct ECSENGINE_API SystemManager {
		SystemManager() = default;
		SystemManager(GlobalMemoryManager* global_memory);

		//void AddQueryCache(const TaskComponentQuery* query);

		void BindSystemData(const char* system_name, const void* data, size_t data_size = 0);

		void BindTemporaryTable(const char* table_name, const void* data, size_t data_size = 0);

		void* GetSystemData(const char* system_name);

		void* GetTemporaryTable(const char* table_name);

		/*void UpdateQueryCache(
			ComponentSignature* unique_signatures, 
			ComponentSignature* shared_signatures,
			unsigned int* archetype_indices, 
			unsigned int new_archetype_count
		);*/

		void ResetTemporaryTable();

		AllocatorPolymorphic allocator;
		HashTableDefault<void*> system_data;
		HashTableDefault<void*> temporary_table;
	};

}