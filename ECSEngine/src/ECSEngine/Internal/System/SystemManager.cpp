#include "ecspch.h"
#include "SystemManager.h"
#include "../../Allocators/MemoryManager.h"
#include "../Multithreading/TaskManager.h"

namespace ECSEngine {

	MemoryManager DefaultSystemManagerAllocator(GlobalMemoryManager* global_manager)
	{
		return MemoryManager(10'000, 1024, 10'000, global_manager);
	}

	SystemManager::SystemManager() {}
	
	SystemManager::SystemManager(TaskManager* task_manager, MemoryManager* memory_manager, size_t system_count) 
	: m_task_manager(task_manager), m_memory(memory_manager) {
		m_systems.Initialize(memory_manager, system_count, 0);
		m_deactivated_systems = ResizableStream<Stream<const char>, MemoryManager>(memory_manager, 0);
		m_active_systems = ResizableStream<Stream<const char>, MemoryManager>(memory_manager, 0);
	}

	void SystemManager::ActivateSystem(const char* name, const char* system_to_activate_after) {
		for (size_t index = 0; index < m_deactivated_systems.size; index++) {
			if (strcmp(name, m_deactivated_systems[index].buffer) == 0) {
				size_t name_length = strlen(name);
				SystemInternal system_to_activate = m_systems.GetValue<true>(ResourceIdentifier(name, name_length));
				
				PushActiveSystem(system_to_activate_after, system_to_activate);
				m_deactivated_systems.RemoveSwapBack(index);
				return;
			}
		}
		ECS_ASSERT(false, "No system matches the given name");
	}

	void SystemManager::ActivateSystemToLast(const char* name) {
		for (size_t index = 0; index < m_deactivated_systems.size; index++) {
			if (strcmp(name, m_deactivated_systems[index].buffer) == 0) {
				size_t name_length = strlen(name);
				SystemInternal system_to_activate = m_systems.GetValue<true>(ResourceIdentifier(name, name_length));

				PushActiveSystemToLast(system_to_activate);
				m_deactivated_systems.RemoveSwapBack(index);
				return;
			}
		}
		ECS_ASSERT(false, "No system matches the given name");
	}

	void SystemManager::AddSystem(System system, const char* system_to_activate_after) {
		// initializing the thread task stream
		SystemInternal system_internal;
		system_internal.data = nullptr;
		system_internal.tasks.Initialize(m_memory, system.tasks.size);
		memcpy(system_internal.tasks.buffer, system.tasks.buffer, sizeof(ThreadTask) * system.tasks.size);

		// allocating memory for the name; it must be allocated in order for the hash table to have stable reference on the
		// ResourceIdentifier 
		size_t name_length = strlen(system.name);
		void* allocation = m_memory->Allocate(sizeof(char) * (name_length + 1), alignof(char));
		memcpy(allocation, system.name, name_length);
		char* temp_reinterpretation = (char*)allocation;
		temp_reinterpretation[name_length] = '\0';
		system_internal.name = (const char*)allocation;

		// inserting
		m_systems.Insert(system_internal, ResourceIdentifier(system_internal.name, name_length));
		PushActiveSystem(system_to_activate_after, system_internal);
	}

	void SystemManager::AddSystemToLast(System system) {
		// initializing the thread task stream
		SystemInternal system_internal;
		system_internal.data = nullptr;
		system_internal.tasks.Initialize(m_memory, system.tasks.size);
		memcpy(system_internal.tasks.buffer, system.tasks.buffer, sizeof(ThreadTask) * system.tasks.size);

		// allocating memory for the name; it must be allocated in order for the hash table to have stable reference on the
		// ResourceIdentifier 
		size_t name_length = strlen(system.name);
		void* allocation = m_memory->Allocate(sizeof(char) * (name_length + 1), alignof(char));
		memcpy(allocation, system.name, name_length);
		char* temp_reinterpretation = (char*)allocation;
		temp_reinterpretation[name_length] = '\0';
		system_internal.name = (const char*)allocation;

		// inserting
		m_systems.Insert(system_internal, ResourceIdentifier(system_internal.name, name_length));
		PushActiveSystemToLast(system_internal);
	}

	void SystemManager::BindSystemResource(const char* system_name, void* data, size_t data_size)
	{
		SystemInternal* system = m_systems.GetValuePtr<true>(ResourceIdentifier(system_name, strlen(system_name)));
		if (data_size > 0) {
			if (system->data_size > 0) {
				m_memory->Deallocate(system->data);
			}
			void* allocation = m_memory->Allocate(data_size);
			memcpy(allocation, data, data_size);
			system->data = allocation;
			system->data_size = data_size;
		}
		else {
			if (system->data_size > 0) {
				m_memory->Deallocate(system->data);
			}
			system->data = data;
			system->data_size = data_size;
		}
	}

	void SystemManager::DeactivateSystem(const char* name) {
		size_t previous_tasks = 0;
		for (size_t index = 0; index < m_active_systems.size; index++) {
			SystemInternal system = m_systems.GetValue<true>(ResourceIdentifier(m_active_systems[index].buffer, m_active_systems[index].size));
			previous_tasks += system.tasks.size;
			if (strcmp(name, m_active_systems[index].buffer) == 0) {
				for (size_t subindex = previous_tasks, end = m_task_manager->GetTaskCount(); subindex < end; subindex++) {
					m_task_manager->SetTask(m_task_manager->GetTask(subindex), subindex - system.tasks.size);
				}
				m_active_systems.Remove(index);
				return;
			}
		}

		ECS_ASSERT(false, "No system was found with the given name");
	}

	void* SystemManager::GetSystemResource(const char* system_name) const {
		return m_systems.GetValue<true>(ResourceIdentifier(system_name, strlen(system_name))).data;
	}

	void SystemManager::PushActiveSystem(const char* system_to_activate_after, SystemInternal system_internal) {
		unsigned int push_down_index = 0;
		if (system_to_activate_after != nullptr) {
			for (size_t index = 0; index < m_active_systems.size; index++) {
				if (strcmp(system_to_activate_after, m_active_systems[index].buffer) == 0) {
					push_down_index = index + 1;
					break;
				}
			}
		}
		m_active_systems.ReserveNewElements(1);
		m_active_systems.PushDownElements(push_down_index, 1);

		m_active_systems[push_down_index].buffer = system_internal.name;
		m_active_systems[push_down_index].size = strlen(system_internal.name);

		size_t previous_tasks = 0;
		for (int64_t index = 0; index < (int64_t)push_down_index - 1; index++) {
			previous_tasks += m_systems.GetValue<true>(ResourceIdentifier(m_active_systems[index].buffer, m_active_systems[index].size)).tasks.size;
		}
		
		m_task_manager->ReserveTasks(system_internal.tasks.size);
		for (int64_t index = m_task_manager->GetTaskCount() - 1; index >= (int64_t)previous_tasks; index--) {
			m_task_manager->SetTask(m_task_manager->GetTask(index), index + system_internal.tasks.size);
		}
		for (size_t index = previous_tasks, end = previous_tasks + system_internal.tasks.size; index < end; index++) {
			m_task_manager->SetTask(system_internal.tasks[index - previous_tasks], index);
		}
	}

	void SystemManager::PushActiveSystemToLast(SystemInternal system) {
		for (size_t subindex = 0; subindex < system.tasks.size; subindex++) {
			m_task_manager->AddTask(system.tasks[subindex]);
		}
		Stream<const char> active_system_name;
		active_system_name.buffer = system.name;
		active_system_name.size = strlen(system.name);
		m_active_systems.Add(active_system_name);
	}


	void SystemManager::RemoveSystem(const char* name) {

	}

}
