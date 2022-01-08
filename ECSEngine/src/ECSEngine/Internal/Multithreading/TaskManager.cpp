#include "ecspch.h"
#include "TaskManager.h"
#include "../World.h"

constexpr size_t SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL = 50;

const size_t THREAD_LOCAL_LINEAR_ALLOCATOR_SIZE = 1'000;
const size_t THREAD_LOCAL_MEMORY_MANAGER_SIZE = 1'000'000;

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------

	void ThreadWrapperNone(unsigned int thread_id, World* ECS_RESTRICT world, ThreadFunction function, void* ECS_RESTRICT data, void* ECS_RESTRICT wrapper_data) {
		function(thread_id, world, data);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ThreadWrapperCountTasks(unsigned int thread_id, World* ECS_RESTRICT _world, ThreadFunction function, void* ECS_RESTRICT data, void* ECS_RESTRICT _wrapper_data) {
		std::atomic<unsigned int>* wrapper_data = (std::atomic<unsigned int>*)_wrapper_data;
		wrapper_data->fetch_add(1);
		function(thread_id, _world, data);
		wrapper_data->fetch_sub(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManagerChangeWrapperMode(
		TaskManager* task_manager,
		TaskManagerWrapper new_wrapper_mode,
		void* ECS_RESTRICT new_wrapper_data,
		size_t new_wrapper_data_size,
		ThreadFunctionWrapper new_custom_function,
		TaskManagerWrapper* ECS_RESTRICT old_wrapper_mode,
		void** ECS_RESTRICT old_wrapper_data,
		size_t* ECS_RESTRICT old_wrapper_data_size,
		ThreadFunctionWrapper* ECS_RESTRICT old_wrapper
	) {
		*old_wrapper_mode = new_wrapper_mode;

		// Use the 0th thread memory manager to do the allocations
		if (*old_wrapper_data != nullptr && *old_wrapper_data_size > 0) {
			task_manager->m_thread_memory_managers[0]->Deallocate(*old_wrapper_data);
		}

		switch (new_wrapper_mode) {
		case TaskManagerWrapper::None:
			*old_wrapper_data = nullptr;
			*old_wrapper_data_size = 0;
			*old_wrapper = ThreadWrapperNone;
			break;
		case TaskManagerWrapper::CountTasks:
		{
			std::atomic<unsigned int>* count = (std::atomic<unsigned int>*)task_manager->m_thread_memory_managers[0]->Allocate(sizeof(std::atomic<unsigned int>));
			count->store(0);
			*old_wrapper_data = count;
			*old_wrapper_data_size = 1;
			*old_wrapper = ThreadWrapperCountTasks;
		}
		break;
		case TaskManagerWrapper::Custom:
			if (new_wrapper_data_size > 0) {
				*old_wrapper_data = function::Copy(task_manager->m_thread_memory_managers[0], new_wrapper_data, new_wrapper_data_size);
			}
			else {
				*old_wrapper_data = new_wrapper_data;
			}
			*old_wrapper_data_size = new_wrapper_data_size;
			*old_wrapper = new_custom_function;
			break;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	constexpr ThreadFunctionWrapper WRAPPERS[] = {
		ThreadWrapperNone,
		ThreadWrapperCountTasks
	};

	// ----------------------------------------------------------------------------------------------------------------------

	TaskManager::TaskManager(
		size_t thread_count,
		MemoryManager* memory_manager,
		size_t thread_linear_allocator_size,
		size_t thread_memory_manager_size,
		size_t max_dynamic_tasks
	) : m_last_thread_index(0), m_world(nullptr)
#ifdef ECS_TASK_MANAGER_WRAPPER 
		, m_static_wrapper_mode(TaskManagerWrapper::None), m_static_wrapper(ThreadWrapperNone), m_static_wrapper_data(nullptr), m_static_wrapper_data_size(0)
		, m_dynamic_wrapper_mode(TaskManagerWrapper::None), m_dynamic_wrapper(ThreadWrapperNone), m_dynamic_wrapper_data(nullptr), m_dynamic_wrapper_data_size(0)
#endif
	{
		void* task_allocation = memory_manager->Allocate(ECS_TASK_MANAGER_ALLOCATOR_CAPACITY);
		m_allocator = TaskAllocator(task_allocation, ECS_TASK_MANAGER_ALLOCATOR_CAPACITY);
		size_t total_memory = 0;
		
		// dynamic queue; adding a cache line in order to prevent cache line bouncing when a thread is accessing its queue
		total_memory += (ThreadQueue::MemoryOf(max_dynamic_tasks) + ECS_CACHE_LINE_SIZE) * thread_count;
		
		// pointers to thread queues;
		total_memory += sizeof(ThreadQueue*) * thread_count;

		// atomic indices; padded in order to prevent cache line bouncing
		total_memory += ECS_CACHE_LINE_SIZE * thread_count;

		// events
		total_memory += sizeof(ConditionVariable) * thread_count;
	
		// additional bytes for alignment
		total_memory += ECS_CACHE_LINE_SIZE * thread_count * 2;

		void* allocation = memory_manager->Allocate(total_memory, ECS_CACHE_LINE_SIZE);
		uintptr_t buffer_start = (uintptr_t)allocation;
		m_thread_queue = Stream<ThreadQueue*>(allocation, thread_count);
		buffer_start += sizeof(ThreadQueue*) * thread_count;
		
		// constructing thread queues and setting them in stream
		for (size_t index = 0; index < thread_count; index++) {
			// aligning the thread task stream that will be held in the thread's queue to a new cache line
			buffer_start = function::align_pointer(buffer_start, ECS_CACHE_LINE_SIZE);
			ThreadQueue temp_queue = ThreadQueue((void*)buffer_start, max_dynamic_tasks);
			buffer_start += sizeof(ThreadTask) * max_dynamic_tasks;

			// creating the queue and assigning it
			buffer_start = function::align_pointer(buffer_start, alignof(ThreadQueue));
			ThreadQueue* buffer_reinterpretation = (ThreadQueue*)buffer_start;
			*buffer_reinterpretation = temp_queue;
			m_thread_queue[index] = buffer_reinterpretation;
			buffer_start += sizeof(ThreadQueue);
		}

		// atomic indices
		buffer_start = function::align_pointer(buffer_start, alignof(std::atomic<unsigned int>*));
		m_thread_task_index = (std::atomic<int>**)buffer_start;
		buffer_start += sizeof(std::atomic<int>*) * thread_count;
		for (size_t index = 0; index < thread_count; index++) {
			buffer_start = function::align_pointer(buffer_start, ECS_CACHE_LINE_SIZE);
			m_thread_task_index[index] = (std::atomic<int>*)buffer_start;
			m_thread_task_index[index]->store(0, ECS_RELEASE);
			buffer_start += sizeof(std::atomic<int>);
		}

		// events
		buffer_start = function::align_pointer(buffer_start, alignof(ConditionVariable));
		m_events = (ConditionVariable*)buffer_start;
		for (size_t index = 0; index < thread_count; index++) {
			new (m_events + index) ConditionVariable();
			m_events[index].Reset();
		}

		m_tasks = ResizableStream<ThreadTask, MemoryManager>(memory_manager, 0);

		// Now the thread local allocators
		const size_t linear_allocator_cache_lines = (sizeof(LinearAllocator) - 1) / ECS_CACHE_LINE_SIZE;
		const size_t memory_manager_cache_lines = (sizeof(MemoryManager) - 1) / ECS_CACHE_LINE_SIZE;
		// Separate the linear allocator and the memory manager to different cache lines in order to prevent false sharing
		size_t thread_local_allocation_size = ((linear_allocator_cache_lines + memory_manager_cache_lines + 2) * 2 + sizeof(LinearAllocator*) + sizeof(MemoryManager*)) * thread_count;
		void* thread_local_allocation = memory_manager->Allocate(thread_local_allocation_size);
		uintptr_t thread_local_ptr = (uintptr_t)thread_local_allocation;

		// The linear allocators first
		m_thread_linear_allocators = (LinearAllocator**)thread_local_ptr;
		thread_local_ptr += sizeof(LinearAllocator*) * thread_count;
		for (size_t index = 0; index < thread_count; index++) {
			m_thread_linear_allocators[index] = (LinearAllocator*)thread_local_ptr;
			thread_local_ptr += (linear_allocator_cache_lines + 1) * ECS_CACHE_LINE_SIZE;
			*m_thread_linear_allocators[index] = LinearAllocator(memory_manager->Allocate(THREAD_LOCAL_LINEAR_ALLOCATOR_SIZE), THREAD_LOCAL_LINEAR_ALLOCATOR_SIZE);
		}

		m_thread_memory_managers = (MemoryManager**)thread_local_ptr;
		thread_local_ptr += sizeof(MemoryManager*) * thread_count;
		// Request the memory from the backup
		for (size_t index = 0; index < thread_count; index++) {
			m_thread_memory_managers[index] = (MemoryManager*)thread_local_ptr;
			thread_local_ptr += (memory_manager_cache_lines + 1) * ECS_CACHE_LINE_SIZE;
			*m_thread_memory_managers[index] = MemoryManager(THREAD_LOCAL_MEMORY_MANAGER_SIZE, 1024, THREAD_LOCAL_MEMORY_MANAGER_SIZE, memory_manager->m_backup);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddTask(ThreadTask task, size_t task_data_size) {
		if (task_data_size > 0) {
			void* allocation = m_tasks.allocator->Allocate(task_data_size);
			memcpy(allocation, task.data, task_data_size);
			task.data = allocation;
		}
#ifdef ECS_THREAD_TASK_NAME
		if (task.name != nullptr) {
			task.name = function::StringCopy(m_tasks.allocator, task.name).buffer;
		}
#endif
		m_tasks.Add(task);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::AddDynamicTask(ThreadTask task, size_t task_data_size) {
		AddDynamicTaskWithAffinity(task, m_last_thread_index, task_data_size);
		unsigned int thread_index = m_last_thread_index;
		m_last_thread_index = function::Select(m_last_thread_index == m_thread_queue.size - 1, (unsigned int)0, m_last_thread_index + 1);
		return thread_index;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::AddDynamicTaskAndWake(ThreadTask task, size_t task_data_size)
	{
		unsigned int thread_index = AddDynamicTask(task, task_data_size);
		WakeThread(thread_index);
		return thread_index;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskWithAffinity(ThreadTask task, unsigned int thread_id, size_t task_data_size) {
		if (task_data_size > 0) {
			void* allocation = m_allocator.Allocate(task_data_size);
			memcpy(allocation, task.data, task_data_size);
			task.data = allocation;
		}
#ifdef ECS_THREAD_TASK_NAME
		if (task.name != nullptr) {
			task.name = function::StringCopy(m_tasks.allocator, task.name).buffer;
		}
#endif
		m_thread_queue[thread_id]->Push(task);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskAndWakeWithAffinity(ThreadTask task, unsigned int thread_id, size_t task_data_size) {
		AddDynamicTaskWithAffinity(task, thread_id, task_data_size);
		WakeThread(thread_id);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ChangeStaticWrapperMode(TaskManagerWrapper wrapper_mode, void* wrapper_data, size_t wrapper_data_size, ThreadFunctionWrapper custom_function)
	{
#ifdef ECS_TASK_MANAGER_WRAPPER
		TaskManagerChangeWrapperMode(this, wrapper_mode, wrapper_data, wrapper_data_size, custom_function, &m_static_wrapper_mode, &m_static_wrapper_data, &m_static_wrapper_data_size, &m_static_wrapper);
#endif
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ChangeDynamicWrapperMode(TaskManagerWrapper wrapper_mode, void* wrapper_data, size_t wrapper_data_size, ThreadFunctionWrapper custom_function) {
#ifdef ECS_TASK_MANAGER_WRAPPER
		TaskManagerChangeWrapperMode(this, wrapper_mode, wrapper_data, wrapper_data_size, custom_function, &m_dynamic_wrapper_mode, &m_dynamic_wrapper_data, &m_dynamic_wrapper_data_size, &m_dynamic_wrapper);
#endif
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTaskIndex(unsigned int thread_id) {
		m_thread_task_index[thread_id]->store(0, ECS_RELEASE);
	}

	// ----------------------------------------------------------------------------------------------------------------------
	
	void TaskManager::ClearTaskIndices() {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			ClearTaskIndex(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTaskStream() {
		m_tasks.size = 0;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::CreateThreads() {
		size_t thread_count = m_thread_queue.size;
		for (size_t index = 0; index < thread_count; index++) {
			std::thread([=]() {
				ThreadProcedure(this, index);
			}).detach();
			//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::DecrementThreadTaskIndex(unsigned int thread_id)
	{
		m_thread_task_index[thread_id]->fetch_sub(1, ECS_ACQ_REL);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::IncrementThreadTaskIndex(unsigned int thread_id) {
		m_thread_task_index[thread_id]->fetch_add(1, ECS_ACQ_REL);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Timer* TaskManager::GetTimer() {
		return &m_timer;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::GetTaskCount() const {
		return m_tasks.size;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::GetThreadCount() const {
		return m_thread_queue.size;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadTask TaskManager::GetTask(unsigned int task_index) const {
		ECS_ASSERT(task_index >= 0 && task_index < m_tasks.capacity);
		return m_tasks[task_index];
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadTask* TaskManager::GetTaskPtr(unsigned int task_index) const {
		ECS_ASSERT(task_index >= 0 && task_index < m_tasks.capacity);
		return m_tasks.buffer + task_index;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadQueue* TaskManager::GetThreadQueue(unsigned int thread_id) const {
		return m_thread_queue[thread_id];
	}

	// ----------------------------------------------------------------------------------------------------------------------

	int TaskManager::GetThreadTaskIndex(unsigned int thread_id) const {
		return m_thread_task_index[thread_id]->load(ECS_ACQUIRE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ReserveTasks(unsigned int count) {
		m_tasks.ReserveNewElements(count);
	}

	// ----------------------------------------------------------------------------------------------------------------------
	
	void TaskManager::ResetDynamicQueue(unsigned int thread_id) {
		m_thread_queue[thread_id]->Reset();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ResetTaskAllocator()
	{
		m_allocator.Clear();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ResetDynamicQueues() {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			ResetDynamicQueue(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetTask(ThreadTask task, unsigned int index, size_t task_data_size) {
		ECS_ASSERT(m_tasks.size < m_tasks.capacity);
		if (task_data_size > 0) {
			void* allocation = m_tasks.allocator->Allocate(task_data_size);
			memcpy(allocation, task.data, task_data_size);
			task.data = allocation;
		}
		m_tasks[index] = task;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetWorld(World* world)
	{
		m_world = world;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetTimerMarker() {
		m_timer.SetMarker();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetThreadTaskIndex(unsigned int thread_id, int value) {
		m_thread_task_index[thread_id]->store(value, ECS_RELEASE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SleepThread(unsigned int thread_id) {
		m_events[thread_id].Wait();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SleepUntilDynamicTasksFinish(size_t max_period) {
#ifdef ECS_TASK_MANAGER_WRAPPER
		ECS_ASSERT(m_dynamic_wrapper_mode == TaskManagerWrapper::CountTasks)
		if (m_dynamic_wrapper_mode == TaskManagerWrapper::CountTasks) {
			Timer timer;
			timer.SetMarker();

			std::atomic<unsigned int>* count = (std::atomic<unsigned int>*)m_dynamic_wrapper_data;
			while (count->load(ECS_ACQUIRE) > 0) {
				if (timer.GetDuration_ms() > max_period) {
					return;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL));
			}
		}
#endif
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::TerminateThread(unsigned int thread_id) {
		SetThreadTaskIndex(thread_id, -1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::TerminateThreads() {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			TerminateThread(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::WakeThread(unsigned int thread_id) {
		m_events[thread_id].Notify();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::WakeThreads() {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			WakeThread(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ThreadProcedure(
		TaskManager* task_manager,
		unsigned int thread_id
	) {
		// keeping the pointer to its own dynamic task queue in order to avoid reloading the pointer
		// when calling the method
		ThreadQueue* thread_queue = task_manager->GetThreadQueue(thread_id);
		task_manager->SetThreadTaskIndex(thread_id, 0);

		// task index ECS_END_THREAD signals termination
		while (task_manager->GetThreadTaskIndex(thread_id) != ECS_END_THREAD) {
			ThreadTask task; 
			if (thread_queue->Pop(task)) {
				//__debugbreak();
#ifdef ECS_TASK_MANAGER_WRAPPER
				task_manager->m_dynamic_wrapper(thread_id, task_manager->m_world, task.function, task.data, task_manager->m_dynamic_wrapper_data);
#ifdef ECS_THREAD_TASK_NAME
				if (task.name != nullptr) {
					task_manager->m_tasks.allocator->Deallocate_ts(task.name);
				}
#endif
#else
				task.function(thread_id, task_manager->m_world, task.data);
#endif
			}
			else {
				task = task_manager->GetTask(task_manager->GetThreadTaskIndex(thread_id));
				task_manager->IncrementThreadTaskIndex(thread_id);
#ifdef ECS_TASK_MANAGER_WRAPPER
				task_manager->m_static_wrapper(thread_id, task_manager->m_world, task.function, task.data, task_manager->m_static_wrapper_data);
#else
				task.function(thread_id, task_manager->m_world, task.data);
#endif
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

}