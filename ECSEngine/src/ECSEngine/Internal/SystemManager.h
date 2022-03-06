#pragma once
#include "../Core.h"
#include "Multithreading/TaskDependencies.h"
#include "../Containers/HashTable.h"

namespace ECSEngine {

	struct TaskManager;

	struct ECSENGINE_API SystemManager {
		SystemManager() = default;
		SystemManager(GlobalMemoryManager* global_memory);

		void AddSystem(Stream<TaskDependencyElement> system_tasks);

		void BindSystemData(const char* system_name, const void* data, size_t data_size = 0);

		void BindTemporaryTable(const char* table_name, const void* data, size_t data_size = 0);

		void ClearDependencies();

		void* GetSystemData(const char* system_name);

		void* GetTemporaryTable(const char* table_name);

		void ResetTemporaryTable();

		// Returns whether or not it succeded in solving the graph
		bool SolveDependencies(TaskManager* task_manager);

		HashTableDefault<void*> system_data;
		HashTableDefault<void*> temporary_table;
		TaskDependencies task_dependencies;
	};

}