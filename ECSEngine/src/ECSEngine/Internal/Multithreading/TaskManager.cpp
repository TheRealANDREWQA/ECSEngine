#include "ecspch.h"
#include "TaskManager.h"
#include "../World.h"

// In microseconds
constexpr size_t SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL = 50'000; // 50 ms

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int ThreadPartitionStream(Stream<ThreadPartition> indices, unsigned int task_count) {
		unsigned int per_thread_count = task_count / indices.size;
		unsigned int remainder = task_count % indices.size;

		unsigned int start = 0;
		unsigned int initial_remainder = remainder;
		for (unsigned int index = 0; index < indices.size; index++) {
			indices[index].offset = start;
			indices[index].size = per_thread_count + (remainder > 0);
			remainder = remainder > 0 ? remainder - 1 : 0;

			start += indices[index].size;
		}

		return per_thread_count > 0 ? indices.size : initial_remainder;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_WRAPPER_TASK(ThreadWrapperNone) {
		function(thread_id, world, _data);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_WRAPPER_TASK(ThreadWrapperCountTasks) {
		ThreadWrapperCountTasksData* wrapper_data = (ThreadWrapperCountTasksData*)_wrapper_data;
		wrapper_data->count.fetch_add(1);
		function(thread_id, world, _data);
		wrapper_data->count.fetch_sub(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ExitThreadTask) {
		ExitThread(0);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManagerChangeWrapperMode(
		TaskManager* task_manager,
		ThreadFunctionWrapper new_custom_function,
		void* new_wrapper_data,
		size_t new_wrapper_data_size,
		ThreadFunctionWrapper* wrapper,
		void** wrapper_data,
		size_t* wrapper_data_size
	) {
		if (*wrapper_data_size > 0) {
			task_manager->m_thread_memory_managers[0]->Deallocate(*wrapper_data);
		}

		*wrapper = new_custom_function;
		*wrapper_data = function::CopyNonZero(GetAllocatorPolymorphic(task_manager->m_thread_memory_managers[0]), new_wrapper_data, new_wrapper_data_size);
		*wrapper_data_size = new_wrapper_data_size;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	constexpr ThreadFunctionWrapper WRAPPERS[] = {
		ThreadWrapperNone,
		ThreadWrapperCountTasks
	};

	// ----------------------------------------------------------------------------------------------------------------------

	TaskManager::TaskManager(
		size_t thread_count,
		GlobalMemoryManager* memory,
		size_t thread_linear_allocator_size,
		size_t thread_memory_manager_size,
		size_t max_dynamic_tasks
	) : m_last_thread_index(0), m_world(nullptr)
#ifdef ECS_TASK_MANAGER_WRAPPER 
		, m_static_wrapper(ThreadWrapperNone), m_static_wrapper_data(nullptr), m_static_wrapper_data_size(0)
		, m_dynamic_wrapper(ThreadWrapperNone), m_dynamic_wrapper_data(nullptr), m_dynamic_wrapper_data_size(0)
#endif
	{
		size_t total_memory = 0;
		
		// dynamic queue; adding a cache line in order to prevent cache line bouncing when a thread is accessing its queue
		total_memory += (ThreadQueue::MemoryOf(max_dynamic_tasks) + ECS_CACHE_LINE_SIZE) * thread_count;
		
		// pointers to thread queues;
		total_memory += sizeof(ThreadQueue*) * thread_count;

		// atomic indices; padded in order to prevent cache line bouncing
		total_memory += ECS_CACHE_LINE_SIZE * thread_count;

		// condition variables
		total_memory += sizeof(ConditionVariable) * thread_count;

		// thread handles
		total_memory += sizeof(void*) * thread_count;
	
		// additional bytes for alignment
		total_memory += ECS_CACHE_LINE_SIZE * thread_count * 2;

		void* allocation = memory->Allocate(total_memory, ECS_CACHE_LINE_SIZE);
		uintptr_t buffer_start = (uintptr_t)allocation;
		m_thread_queue = Stream<ThreadQueue*>(allocation, thread_count);
		buffer_start += sizeof(ThreadQueue*) * thread_count;
		
		// constructing thread queues and setting them in stream
		for (size_t index = 0; index < thread_count; index++) {
			// aligning the thread task stream that will be held in the thread's queue to a new cache line
			buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
			ThreadQueue temp_queue = ThreadQueue((void*)buffer_start, max_dynamic_tasks);
			buffer_start += sizeof(ThreadTask) * max_dynamic_tasks;

			// creating the queue and assigning it
			buffer_start = function::AlignPointer(buffer_start, alignof(ThreadQueue));
			ThreadQueue* buffer_reinterpretation = (ThreadQueue*)buffer_start;
			*buffer_reinterpretation = temp_queue;
			m_thread_queue[index] = buffer_reinterpretation;
			buffer_start += sizeof(ThreadQueue);
		}

		// atomic indices
		buffer_start = function::AlignPointer(buffer_start, alignof(std::atomic<unsigned int>*));
		m_thread_task_index = (std::atomic<int>**)buffer_start;
		buffer_start += sizeof(std::atomic<int>*) * thread_count;
		for (size_t index = 0; index < thread_count; index++) {
			buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
			m_thread_task_index[index] = (std::atomic<int>*)buffer_start;
			m_thread_task_index[index]->store(0, ECS_RELEASE);
			buffer_start += sizeof(std::atomic<int>);
		}

		// sleep variables
		buffer_start = function::AlignPointer(buffer_start, alignof(ConditionVariable));
		m_sleep_wait = (ConditionVariable*)buffer_start;
		for (size_t index = 0; index < thread_count; index++) {
			new (m_sleep_wait + index) ConditionVariable();
			m_sleep_wait[index].Reset();
		}
		buffer_start += sizeof(ConditionVariable) * thread_count;

		// thread handles
		buffer_start = function::AlignPointer(buffer_start, alignof(void*));
		m_thread_handles = (void**)buffer_start;
		buffer_start += sizeof(void*) * thread_count;

		m_tasks = ResizableStream<ThreadTask>(GetAllocatorPolymorphic(memory), 0);

		// Now the thread local allocators
		const size_t linear_allocator_cache_lines = (sizeof(LinearAllocator) - 1) / ECS_CACHE_LINE_SIZE;
		const size_t memory_manager_cache_lines = (sizeof(MemoryManager) - 1) / ECS_CACHE_LINE_SIZE;
		// Separate the linear allocator and the memory manager to different cache lines in order to prevent false sharing
		size_t thread_local_allocation_size = ((linear_allocator_cache_lines + memory_manager_cache_lines + 2) * 2 * ECS_CACHE_LINE_SIZE + sizeof(LinearAllocator*) + sizeof(MemoryManager*)) * thread_count;
		void* thread_local_allocation = memory->Allocate(thread_local_allocation_size);
		uintptr_t thread_local_ptr = (uintptr_t)thread_local_allocation;

		size_t linear_allocator_total_size = thread_linear_allocator_size * thread_count;
		void* linear_allocation = memory->Allocate(linear_allocator_total_size);

		// The linear allocators first
		m_thread_linear_allocators = (LinearAllocator**)thread_local_ptr;
		thread_local_ptr += sizeof(LinearAllocator*) * thread_count;
		for (size_t index = 0; index < thread_count; index++) {
			m_thread_linear_allocators[index] = (LinearAllocator*)thread_local_ptr;
			thread_local_ptr += (linear_allocator_cache_lines + 1) * ECS_CACHE_LINE_SIZE;
			*m_thread_linear_allocators[index] = LinearAllocator(linear_allocation, thread_linear_allocator_size);

			linear_allocation = function::OffsetPointer(linear_allocation, thread_linear_allocator_size);
		}

		m_thread_memory_managers = (MemoryManager**)thread_local_ptr;
		thread_local_ptr += sizeof(MemoryManager*) * thread_count;
		// Request the memory from the backup
		for (size_t index = 0; index < thread_count; index++) {
			m_thread_memory_managers[index] = (MemoryManager*)thread_local_ptr;
			thread_local_ptr += (memory_manager_cache_lines + 1) * ECS_CACHE_LINE_SIZE;
			*m_thread_memory_managers[index] = MemoryManager(thread_memory_manager_size, 1024, thread_memory_manager_size, memory);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddTask(ThreadTask task) {
		ECS_ASSERT(task.name != nullptr);

		if (task.data_size > 0) {
			void* allocation = Allocate(m_tasks.allocator, task.data_size);
			memcpy(allocation, task.data, task.data_size);
			task.data = allocation;
		}

#if 0
		if (task.name != nullptr) {
			task.name = function::StringCopy(m_tasks.allocator, task.name).buffer;
		}
#else
		task.name = function::StringCopy(m_tasks.allocator, task.name).buffer;
#endif
		m_tasks.Add(task);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::AddDynamicTask(ThreadTask task) {
		AddDynamicTaskWithAffinity(task, m_last_thread_index);
		unsigned int thread_index = m_last_thread_index;
		m_last_thread_index = function::Select(m_last_thread_index == m_thread_queue.size - 1, (unsigned int)0, m_last_thread_index + 1);
		return thread_index;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::AddDynamicTaskAndWake(ThreadTask task)
	{
		unsigned int thread_index = AddDynamicTask(task);
		WakeThread(thread_index);
		return thread_index;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskWithAffinity(ThreadTask task, unsigned int thread_id) {
		ECS_ASSERT(task.name != nullptr);

		if (task.data_size > 0) {
			void* allocation = m_thread_memory_managers[thread_id]->Allocate(task.data_size);
			memcpy(allocation, task.data, task.data_size);
			task.data = allocation;
		}

#if 0
		if (task.name != nullptr) {
			task.name = function::StringCopy(m_tasks.allocator, task.name).buffer;
		}
#else
		task.name = function::StringCopy(m_tasks.allocator, task.name).buffer;
#endif
		m_thread_queue[thread_id]->Push(task);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskAndWakeWithAffinity(ThreadTask task, unsigned int thread_id) {
		AddDynamicTaskWithAffinity(task, thread_id);
		WakeThread(thread_id);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void* TaskManager::AllocateTempBuffer(unsigned int thread_id, size_t size, size_t alignment)
	{
		return m_thread_linear_allocators[thread_id]->Allocate(size, alignment);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void* TaskManager::AllocatePersistentBuffer(unsigned int thread_id, size_t size, size_t alignment)
	{
		return m_thread_memory_managers[thread_id]->Allocate(size, alignment);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ChangeStaticWrapperMode(ThreadFunctionWrapper custom_function, void* wrapper_data, size_t wrapper_data_size)
	{
		TaskManagerChangeWrapperMode(this, custom_function, wrapper_data, wrapper_data_size, &m_static_wrapper, &m_static_wrapper_data, &m_static_wrapper_data_size);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ChangeDynamicWrapperMode(ThreadFunctionWrapper custom_function, void* wrapper_data, size_t wrapper_data_size) {
		TaskManagerChangeWrapperMode(this, custom_function, wrapper_data, wrapper_data_size, &m_dynamic_wrapper, &m_dynamic_wrapper_data, &m_dynamic_wrapper_data_size);
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
		for (size_t index = 0; index < m_tasks.size; index++) {
			if (m_tasks[index].data_size > 0) {
				Deallocate(m_tasks.allocator, m_tasks[index].data);
			}
		}
		m_tasks.size = 0;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::CreateThreads() {
		size_t thread_count = m_thread_queue.size;
		for (size_t index = 0; index < thread_count; index++) {
			auto thread = std::thread([=]() {
				ThreadProcedure(this, index);
			});

			// Register the handle
			m_thread_handles[index] = thread.native_handle();

			thread.detach();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::DestroyThreads()
	{
		for (unsigned int index = 0; index < GetThreadCount(); index++) {
			AddDynamicTaskAndWakeWithAffinity(ECS_THREAD_TASK_NAME(ExitThreadTask, nullptr, 0), index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::DecrementThreadTaskIndex(unsigned int thread_id)
	{
		m_thread_task_index[thread_id]->fetch_sub(1, ECS_ACQ_REL);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::DeallocatePersistentBuffer(unsigned int thread_id, const void* data)
	{
		m_thread_memory_managers[thread_id]->Deallocate(data);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::IncrementThreadTaskIndex(unsigned int thread_id) {
		m_thread_task_index[thread_id]->fetch_add(1, ECS_ACQ_REL);
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

	AllocatorPolymorphic TaskManager::GetThreadTempAllocator(unsigned int thread_id) const
	{
		return GetAllocatorPolymorphic(m_thread_linear_allocators[thread_id]);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic TaskManager::GetThreadPersistentAllocator(unsigned int thread_id) const
	{
		return GetAllocatorPolymorphic(m_thread_memory_managers[thread_id]);
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

	void TaskManager::ResetDynamicQueues() {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			ResetDynamicQueue(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetTask(ThreadTask task, unsigned int index, size_t task_data_size) {
		ECS_ASSERT(m_tasks.size < m_tasks.capacity);
		if (task_data_size > 0) {
			void* allocation = Allocate(m_tasks.allocator, task_data_size);
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

	void TaskManager::SetThreadTaskIndex(unsigned int thread_id, int value) {
		m_thread_task_index[thread_id]->store(value, ECS_RELEASE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SleepThread(unsigned int thread_id) {
		m_sleep_wait[thread_id].Wait();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SleepUntilDynamicTasksFinish(size_t max_period) {
#ifdef ECS_TASK_MANAGER_WRAPPER
		ECS_ASSERT(m_dynamic_wrapper == ThreadWrapperCountTasks);

		Timer timer;
		timer.SetMarker();

		ThreadWrapperCountTasksData* data = (ThreadWrapperCountTasksData*)m_dynamic_wrapper_data;
		TickWait<'!'>(SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL, data->count, (unsigned int)0);
#endif
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetThreadPriorities(ECS_THREAD_PRIORITY priority)
	{
		int thread_priority = 0;
		switch (priority) {
		case ECS_THREAD_PRIORITY_VERY_LOW:
			thread_priority = THREAD_PRIORITY_LOWEST;
			break;
		case ECS_THREAD_PRIORITY_LOW:
			thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
			break;
		case ECS_THREAD_PRIORITY_NORMAL:
			thread_priority = THREAD_PRIORITY_NORMAL;
			break;
		case ECS_THREAD_PRIORITY_HIGH:
			thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
			break; 
		case ECS_THREAD_PRIORITY_VERY_HIGH:
			thread_priority = THREAD_PRIORITY_HIGHEST;
			break;
		}

		for (unsigned int index = 0; index < GetThreadCount(); index++) {
			BOOL success = SetThreadPriority(m_thread_handles[index], thread_priority);
			if (!success) {
				DWORD error = GetLastError();
				__debugbreak();
			}
		}
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
		m_sleep_wait[thread_id].Notify();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::WakeThreads() {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			WakeThread(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::WaitThread(int task_index, unsigned int thread_index)
	{
		int thread_task_index = GetThreadTaskIndex(thread_index);
		if (thread_task_index != task_index) {
			SpinWait<'!'>(*m_thread_task_index[thread_index], task_index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::WaitAllThreads(int task_index)
	{
		unsigned int thread_count = GetThreadCount();
		for (unsigned int index = 0; index < thread_count; index++) {
			WaitThread(task_index, index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	MemoryManager DefaultTaskManagerAllocator(GlobalMemoryManager* global_memory)
	{
		return MemoryManager(500'000, 256, 500'000, global_memory);
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
#ifdef ECS_TASK_MANAGER_WRAPPER
				task_manager->m_dynamic_wrapper(thread_id, task_manager->m_world, task.function, task.data, task_manager->m_dynamic_wrapper_data);
				if (task.name != nullptr) {
					DeallocateTs(task_manager->m_tasks.allocator.allocator, task_manager->m_tasks.allocator.allocator_type, task.name);
				}

				// Deallocate the task data if allocated from the thread allocator
				if (task.data_size > 0) {
					task_manager->m_thread_memory_managers[thread_id]->Deallocate(task.data);
				}
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