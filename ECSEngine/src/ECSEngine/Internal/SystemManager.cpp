#include "ecspch.h"
#include "SystemManager.h"
#include "../Utilities/FunctionInterfaces.h"
#include "Multithreading/TaskManager.h"

#define MEMORY_MANAGER_SIZE 10'000
#define MEMORY_MANAGER_CHUNK_COUNT 256
#define MEMORY_MANAGER_NEW_ALLOCATION_SIZE 5'000

#define SYSTEM_DATA_COUNT 256
#define TEMPORARY_TABLE_COUNT 512

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------------------

	SystemManager::SystemManager(GlobalMemoryManager* global_memory)
	{
		MemoryManager* memory_manager = (MemoryManager*)global_memory->Allocate(sizeof(MemoryManager));
		*memory_manager = MemoryManager(MEMORY_MANAGER_SIZE, MEMORY_MANAGER_CHUNK_COUNT, MEMORY_MANAGER_NEW_ALLOCATION_SIZE, global_memory);

		size_t table_size = system_data.MemoryOf(SYSTEM_DATA_COUNT) + temporary_table.MemoryOf(TEMPORARY_TABLE_COUNT);
		void* allocation = global_memory->Allocate(table_size);
		uintptr_t ptr = (uintptr_t)allocation;
		system_data.InitializeFromBuffer(ptr, SYSTEM_DATA_COUNT);
		temporary_table.InitializeFromBuffer(ptr, TEMPORARY_TABLE_COUNT);

		task_dependencies = TaskDependencies(memory_manager);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::AddSystem(Stream<TaskDependencyElement> system_tasks)
	{
		task_dependencies.Add(system_tasks);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::BindSystemData(const char* system_name, const void* data, size_t data_size)
	{
		unsigned int index = system_data.Find(system_name);

		data = function::Copy(task_dependencies.elements.allocator, data, data_size);
		if (index == -1) {
			ECS_ASSERT(!system_data.Insert((void*)data, system_name));
		}
		else {
			void** ptr = system_data.GetValuePtrFromIndex(index);
			*ptr = (void*)data;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::BindTemporaryTable(const char* table_name, const void* data, size_t data_size)
	{
		unsigned int index = temporary_table.Find(table_name);

		data = function::Copy(task_dependencies.elements.allocator, data, data_size);
		if (index == -1) {
			ECS_ASSERT(!temporary_table.Insert((void*)data, table_name));
		}
		else {
			void** ptr = temporary_table.GetValuePtrFromIndex(index);
			*ptr = (void*)data;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::ClearDependencies()
	{
		task_dependencies.Reset();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetSystemData(const char* system_name)
	{
		void* data = nullptr;
		system_data.TryGetValue(system_name, data);
		return data;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetTemporaryTable(const char* table_name)
	{
		void* data = nullptr;
		temporary_table.TryGetValue(table_name, data);
		return data;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::ResetTemporaryTable()
	{
		temporary_table.Clear();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	bool SystemManager::SolveDependencies(TaskManager* task_manager)
	{
		bool success = task_dependencies.Solve();
		if (success) {
			for (size_t index = 0; index < task_dependencies.elements.size; index++) {
				task_manager->AddTask(task_dependencies.elements[index].task);
			}
			return true;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

}