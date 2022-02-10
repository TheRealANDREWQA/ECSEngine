#pragma once
#include "../../Core.h"
#include "../../Containers/SequentialTable.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "../Multithreading/ThreadTask.h"

namespace ECSEngine {

	struct MemoryManager;
	struct TaskManager;

	constexpr size_t system_manager_systems_count = 128;

	ECS_CONTAINERS;

	ECSENGINE_API MemoryManager DefaultSystemManagerAllocator(GlobalMemoryManager* global_manager);

	struct ECSENGINE_API System {
		Stream<ThreadTask> tasks;
		const char* name;
	};

	// Handles activation, deactivation, addition and removal of systems;
	// Shared structures like partition trees or acceleration structures can be bound to systems
	// and easily referenced without coupling systems
	struct ECSENGINE_API SystemManager
	{
		using HashFunction = HashFunctionMultiplyString;
		struct SystemInternal {
			Stream<ThreadTask> tasks;
			void* data;
			unsigned int data_size;
			const char* name;
		};
	public:
		SystemManager();
		SystemManager(TaskManager* task_manager, MemoryManager* memory_manager, size_t system_count = system_manager_systems_count);

		void ActivateSystem(const char* name, const char* system_to_activate_after = nullptr);

		void ActivateSystemToLast(const char* name);

		void AddSystem(System system, const char* system_to_activate_after = nullptr);

		void AddSystemToLast(System system);

		// Data equal to 0 means just keep the pointer, do not copy
		void BindSystemResource(const char* system_name, void* data, size_t data_size = 0);

		void DeactivateSystem(const char* name);

		void* GetSystemResource(const char* system_name) const;

		void PushActiveSystem(const char* system_to_activate_after, SystemInternal system_internal);

		void PushActiveSystemToLast(SystemInternal system_internal);

		void RemoveSystem(const char* name);

	//private:
		TaskManager* m_task_manager;
		MemoryManager* m_memory;
		HashTable<SystemInternal, ResourceIdentifier, HashFunctionPowerOfTwo, HashFunction> m_systems;
		ResizableStream<Stream<const char>> m_deactivated_systems;
		ResizableStream<Stream<const char>> m_active_systems;
	};

}

