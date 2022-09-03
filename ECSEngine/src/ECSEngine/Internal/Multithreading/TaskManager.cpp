#include "ecspch.h"
#include "TaskManager.h"
#include "../World.h"

// In microseconds
#define SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL (50'000)

// The allocator size for the data + task.name for dynamic tasks
// This is per thread
#define DYNAMIC_TASK_ALLOCATOR_SIZE (2'500)

#define STATIC_TASK_ALLOCATOR_SIZE (10'000)

namespace ECSEngine {

	inline AllocatorPolymorphic StaticTaskAllocator(TaskManager* manager) {
		return GetAllocatorPolymorphic(&manager->m_static_task_data_allocator);
	}

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
		task.function(thread_id, world, task.data);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_WRAPPER_TASK(ThreadWrapperCountTasks) {
		ThreadWrapperCountTasksData* wrapper_data = (ThreadWrapperCountTasksData*)_wrapper_data;
		wrapper_data->count.fetch_add(1);
		task.function(thread_id, world, task.data);
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
		*wrapper = new_custom_function;
		*wrapper_data = function::CopyNonZero(GetAllocatorPolymorphic(&task_manager->m_static_task_data_allocator), new_wrapper_data, new_wrapper_data_size);
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
		size_t max_dynamic_tasks
	) : m_last_thread_index(0), m_world(nullptr)
#ifdef ECS_TASK_MANAGER_WRAPPER 
		, m_static_wrapper(ThreadWrapperNone), m_static_wrapper_data(nullptr), m_static_wrapper_data_size(0)
		, m_dynamic_wrapper(ThreadWrapperNone), m_dynamic_wrapper_data(nullptr), m_dynamic_wrapper_data_size(0)
#endif
	{
		thread_linear_allocator_size = thread_linear_allocator_size == 0 ? ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE : thread_linear_allocator_size;
		max_dynamic_tasks = max_dynamic_tasks == 0 ? ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD : max_dynamic_tasks;

		size_t total_memory = 0;
		
		// dynamic queue; adding a cache line in order to prevent cache line bouncing when a thread is accessing its queue
		total_memory += (ThreadQueue::MemoryOf(max_dynamic_tasks) + ECS_CACHE_LINE_SIZE) * thread_count;
		
		// pointers to thread queues;
		total_memory += sizeof(ThreadQueue*) * thread_count;

		// condition variables
		total_memory += sizeof(ConditionVariable) * thread_count;

		// thread handles
		total_memory += sizeof(void*) * thread_count;

		// additional bytes for alignment
		total_memory += ECS_CACHE_LINE_SIZE * thread_count * 2;

		// The task allocator has a fixed size
		// +7 bytes to align the RingBuffer* for each thread
		total_memory += (DYNAMIC_TASK_ALLOCATOR_SIZE + sizeof(RingBuffer) + sizeof(RingBuffer*) + 7) * thread_count;

		total_memory += STATIC_TASK_ALLOCATOR_SIZE;

		void* allocation = memory->Allocate(total_memory, ECS_CACHE_LINE_SIZE);
		uintptr_t buffer_start = (uintptr_t)allocation;
		m_thread_queue = Stream<ThreadQueue*>(allocation, thread_count);
		buffer_start += sizeof(ThreadQueue*) * thread_count;
		
		// constructing thread queues and setting them in stream
		for (size_t index = 0; index < thread_count; index++) {
			// aligning the thread task stream that will be held in the thread's queue to a new cache line
			buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
			ThreadQueue temp_queue = ThreadQueue((void*)buffer_start, max_dynamic_tasks);
			buffer_start += ThreadQueue::MemoryOf(max_dynamic_tasks);

			// creating the queue and assigning it
			buffer_start = function::AlignPointer(buffer_start, alignof(ThreadQueue));
			ThreadQueue* buffer_reinterpretation = (ThreadQueue*)buffer_start;
			*buffer_reinterpretation = temp_queue;
			m_thread_queue[index] = buffer_reinterpretation;
			buffer_start += sizeof(ThreadQueue);
		}

		m_thread_task_index.store(0, ECS_RELEASE);

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

		m_dynamic_task_allocators = (RingBuffer**)buffer_start;
		buffer_start += sizeof(RingBuffer*) * thread_count;

		for (size_t index = 0; index < thread_count; index++) {
			m_dynamic_task_allocators[index] = (RingBuffer*)function::AlignPointer(buffer_start, alignof(RingBuffer));
			buffer_start += sizeof(RingBuffer);

			m_dynamic_task_allocators[index]->InitializeFromBuffer(buffer_start, DYNAMIC_TASK_ALLOCATOR_SIZE);
		}

		m_static_task_data_allocator = LinearAllocator((void*)buffer_start, STATIC_TASK_ALLOCATOR_SIZE);
		buffer_start += STATIC_TASK_ALLOCATOR_SIZE;

		m_tasks = ResizableStream<StaticThreadTask>(GetAllocatorPolymorphic(memory), 0);

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

		SetWaitType(ECS_TASK_MANAGER_WAIT_SLEEP);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddTask(ThreadTask task) {
		ECS_ASSERT(task.name != nullptr);

		task.data = function::CopyNonZero(StaticTaskAllocator(this), task.data, task.data_size);

#if 0
		if (task.name != nullptr) {
			task.name = function::StringCopy(TaskAllocator(this), task.name).buffer;
		}
#else
		task.name = function::StringCopy(StaticTaskAllocator(this), task.name).buffer;
#endif
		unsigned int index = m_tasks.ReserveNewElement();
		m_tasks[index].task = task;
		m_tasks[index].initialize_lock.unlock();

		m_tasks.size++;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddTasks(Stream<ThreadTask> tasks)
	{
		for (size_t index = 0; index < tasks.size; index++) {
			AddTask(tasks[index]);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::AddDynamicTask(ThreadTask task, bool can_be_stolen) {
		AddDynamicTaskWithAffinity(task, m_last_thread_index, can_be_stolen);
		unsigned int thread_index = m_last_thread_index;
		m_last_thread_index = m_last_thread_index == m_thread_queue.size - 1 ? (unsigned int)0 : m_last_thread_index + 1;
		return thread_index;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::AddDynamicTaskAndWake(ThreadTask task, bool can_be_stolen)
	{
		unsigned int thread_index = AddDynamicTask(task);
		WakeThread(thread_index);
		return thread_index;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskWithAffinity(ThreadTask task, unsigned int thread_id, bool can_be_stolen) {
		ECS_ASSERT(task.name != nullptr);
		
		// Make the name size a multiple of 8 such that the data is always aligned on an 8 byte boundary
		size_t name_size = strlen(task.name) + 1;

		size_t total_allocation_size = (name_size + 7) * sizeof(char) + task.data_size;

		char* name = (char*)m_dynamic_task_allocators[thread_id]->Allocate(total_allocation_size);
		memcpy(name, task.name, sizeof(char) * name_size);
		task.name = name;

		if (task.data_size > 0) {
			const void* old_data = task.data;
			task.data = (void*)function::AlignPointer((uintptr_t)function::OffsetPointer(name, name_size), 8);
			memcpy(task.data, old_data, task.data_size);
		}

		DynamicThreadTask dynamic_task;
		dynamic_task = { task, can_be_stolen };
		m_thread_queue[thread_id]->Push(dynamic_task);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskAndWakeWithAffinity(ThreadTask task, unsigned int thread_id, bool can_be_stolen) {
		AddDynamicTaskWithAffinity(task, thread_id, can_be_stolen);
		WakeThread(thread_id);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskGroup(ThreadFunction function, const char* function_name, Stream<void*> datas, size_t data_size, bool can_be_stolen)
	{
		unsigned int thread_count = GetThreadCount();
		unsigned int per_thread_count = datas.size / thread_count;
		unsigned int remainder = datas.size % thread_count;

		if (per_thread_count == 0) {
			for (unsigned int index = 0; index < remainder; index++) {
				AddDynamicTaskWithAffinity({ function, datas[index], data_size, function_name }, index, can_be_stolen);
			}
		}
		else {
			unsigned int task_index = 0;
			for (unsigned int index = 0; index < thread_count; index++) {
				for (unsigned int per_thread_index = 0; per_thread_index < per_thread_count; per_thread_index++) {
					AddDynamicTaskWithAffinity({ function, datas[task_index++], data_size, function_name }, index, can_be_stolen);
				}

				if (remainder > 0) {
					AddDynamicTaskWithAffinity({ function, datas[task_index++], data_size, function_name }, index, can_be_stolen);
				}
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskGroupAndWake(ThreadFunction function, const char* function_name, Stream<void*> datas, size_t data_size, bool can_be_stolen)
	{
		unsigned int thread_count = GetThreadCount();
		unsigned int per_thread_count = datas.size / thread_count;
		unsigned int remainder = datas.size % thread_count;

		if (per_thread_count == 0) {
			for (unsigned int index = 0; index < remainder; index++) {
				AddDynamicTaskAndWakeWithAffinity({ function, datas[index], data_size, function_name }, index, can_be_stolen);
			}
		}
		else {
			unsigned int task_index = 0;
			for (unsigned int index = 0; index < thread_count; index++) {
				for (unsigned int per_thread_index = 0; per_thread_index < per_thread_count; per_thread_index++) {
					AddDynamicTaskAndWakeWithAffinity({ function, datas[task_index++], data_size, function_name }, index, can_be_stolen);
				}

				if (remainder > 0) {
					AddDynamicTaskAndWakeWithAffinity({ function, datas[task_index++], data_size, function_name }, index, can_be_stolen);
				}
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void* TaskManager::AllocateTempBuffer(unsigned int thread_id, size_t size, size_t alignment)
	{
		return m_thread_linear_allocators[thread_id]->Allocate(size, alignment);
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

	void TaskManager::ClearThreadAllocators()
	{
		ClearTemporaryAllocators();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTemporaryAllocators()
	{
		for (size_t index = 0; index < GetThreadCount(); index++) {
			m_thread_linear_allocators[index]->Clear();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTaskIndex() {
		m_thread_task_index.store(0, ECS_RELEASE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTaskStream() {
		for (size_t index = 0; index < m_tasks.size; index++) {
			if (m_tasks[index].task.data_size > 0) {
				Deallocate(m_tasks.allocator, m_tasks[index].task.data);
			}
		}
		m_tasks.size = 0;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearFrame()
	{
		ClearThreadAllocators();
		ClearTaskIndex();
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

	void TaskManager::DecrementThreadTaskIndex()
	{
		m_thread_task_index.fetch_sub(1, ECS_ACQ_REL);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::DoFrame()
	{
		// Wake the threads
		WakeThreads();

		unsigned int thread_count = GetThreadCount();
		m_is_frame_done.Wait(thread_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ExecuteStaticTask(ThreadTask task, unsigned int thread_id)
	{
		m_static_wrapper(thread_id, m_world, task, m_static_wrapper_data);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ExecuteDynamicTask(ThreadTask task, unsigned int thread_id)
	{
		m_dynamic_wrapper(thread_id, m_world, task, m_dynamic_wrapper_data);
		// Release the dynamic task allocation
		size_t allocation_size = (strlen(task.name) + 8) * sizeof(char) + task.data_size;
		m_dynamic_task_allocators[thread_id]->Finish(task.name, allocation_size);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(FinishFrameTaskDynamic) {
		world->task_manager->m_is_frame_done.Notify();
	}

	ECS_THREAD_TASK(FinishFrameTask) {
		// Flush the entity manager as well
		world->entity_manager->EndFrame();
		world->task_manager->AddDynamicTaskGroup(WITH_NAME(FinishFrameTaskDynamic), { nullptr, 0 }, 0);
	}

	void TaskManager::FinishStaticTasks()
	{
		AddTask(ECS_THREAD_TASK_NAME(FinishFrameTask, nullptr, 0));
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::IncrementThreadTaskIndex() {
		m_thread_task_index.fetch_add(1, ECS_ACQ_REL);
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
		return m_tasks[task_index].task;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadTask* TaskManager::GetTaskPtr(unsigned int task_index) {
		ECS_ASSERT(task_index >= 0 && task_index < m_tasks.capacity);
		return &m_tasks[task_index].task;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadQueue* TaskManager::GetThreadQueue(unsigned int thread_id) const {
		return m_thread_queue[thread_id];
	}

	// ----------------------------------------------------------------------------------------------------------------------

	int TaskManager::GetThreadTaskIndex() const {
		return m_thread_task_index.load(ECS_ACQUIRE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic TaskManager::GetThreadTempAllocator(unsigned int thread_id) const
	{
		return GetAllocatorPolymorphic(m_thread_linear_allocators[thread_id]);
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
		m_tasks[index].task = task;
		m_tasks[index].initialize_lock.unlock();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetWorld(World* world)
	{
		m_world = world;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetThreadTaskIndex(int value) {
		m_thread_task_index.store(value, ECS_RELEASE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SleepThread(unsigned int thread_id) {
		m_sleep_wait[thread_id].Wait();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(SleepTask) {
		ConditionVariable* data = (ConditionVariable*)_data;
		data->Notify();
		world->task_manager->SleepThread(thread_id);
	}

	void TaskManager::SleepThreads(bool wait_until_all_sleep) {
		unsigned int thread_count = GetThreadCount();
		ConditionVariable* condition_variable = (ConditionVariable*)m_dynamic_task_allocators[0]->Allocate(sizeof(ConditionVariable));
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, task_data, thread_count);
		for (unsigned int thread_index = 0; thread_index < thread_count; thread_index++) {
			task_data[thread_index] = condition_variable;
		}
		task_data.size = thread_count;

		AddDynamicTaskGroup(WITH_NAME(SleepTask), task_data, 0, false);
		if (wait_until_all_sleep) {
			condition_variable->Wait(thread_count);
		}
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

	void TaskManager::SetWaitType(ECS_TASK_MANAGER_WAIT_TYPE wait_type)
	{
		m_wait_type = wait_type;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadTask TaskManager::StealTask(unsigned int thread_index)
	{
		// Try to go to the next index
		// Because going from a fixed index will cause multiple threads
		// trying to steal a job into waiting for each other instead of
		// exploring multiple queues
		DynamicThreadTask task;
		task.task.function = nullptr;

		unsigned int thread_count = GetThreadCount();
		thread_index = thread_index == thread_count ? 0 : thread_index + 1;
		for (unsigned int index = 0; index < thread_count - 1; index++) {
			ThreadQueue* current_queue = GetThreadQueue(thread_index);
			current_queue->Lock();
			if (current_queue->GetQueue()->Peek(task)) {
				if (task.can_be_stolen) {
					// Pop the task, release the lock and then return
					current_queue->PopNonAtomic(task);
					current_queue->Unlock();

					return task.task;
				}
			}
			current_queue->Unlock();
		}

		return task.task;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ChangeThreadPriorityTask) {
		int* priority = (int*)_data;
		BOOL success = SetThreadPriority(GetCurrentThread(), *priority);
		ECS_ASSERT(success, "Changing thread priority failed.");
	}

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
			AddDynamicTaskAndWakeWithAffinity(ECS_THREAD_TASK_NAME(ChangeThreadPriorityTask, &thread_priority, sizeof(thread_priority)), index, false);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(TerminateThreadTask) {
		ExitThread(0);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SpinThread(unsigned int thread_id)
	{
		ThreadQueue* thread_queue = GetThreadQueue(thread_id);
		thread_queue->SpinWaitElements();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::TerminateThread(unsigned int thread_id) {
		AddDynamicTaskAndWakeWithAffinity(ECS_THREAD_TASK_NAME(TerminateThreadTask, nullptr, 0), thread_id, false);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::TerminateThreads(bool wait_for_termination) {
		for (size_t index = 0; index < GetThreadCount(); index++) {
			TerminateThread(index);
		}

		if (wait_for_termination) {
			for (size_t index = 0; index < GetThreadCount(); index++) {
				// Use GetExitCodeThread
				DWORD result = WaitForSingleObject(m_thread_handles[index], 0);
				ECS_ASSERT(result != WAIT_FAILED && result != WAIT_ABANDONED);

				while (result == WAIT_TIMEOUT) {
					result = WaitForSingleObject(m_thread_handles[index], 0);
				}
			}
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

	ECS_THREAD_TASK(WaitThreadTask) {
		SpinLock* flag = (SpinLock*)_data;
		flag->unlock();
	}

	void TaskManager::WaitThread(unsigned int thread_id) {
		SpinLock* lock = (SpinLock*)m_dynamic_task_allocators[thread_id]->Allocate(sizeof(SpinLock));
		lock->lock();

		AddDynamicTaskWithAffinity(ECS_THREAD_TASK_NAME(WaitThreadTask, lock, 0), thread_id, false);
		lock->wait_locked();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(WaitThreadsTask) {
		Semaphore* semaphore = (Semaphore*)_data;
		semaphore->Enter();
	}

	void TaskManager::WaitThreads()
	{
		unsigned int thread_count = GetThreadCount();

		Semaphore* semaphore = (Semaphore*)m_dynamic_task_allocators[0]->Allocate(sizeof(Semaphore));
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, thread_data, thread_count);
		
		for (unsigned int index = 0; index < thread_count; index++) {
			thread_data[index] = semaphore;
		}
		thread_data.size = thread_count;

		AddDynamicTaskGroup(WITH_NAME(WaitThreadsTask), thread_data, 0, false);
		semaphore->SpinWait(thread_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ThreadProcedure(
		TaskManager* task_manager,
		unsigned int thread_id
	) {
		// keeping the pointer to its own dynamic task queue in order to avoid reloading the pointer
		// when calling the method
		ThreadQueue* thread_queue = task_manager->GetThreadQueue(thread_id);

		DynamicThreadTask thread_task;
		while (true) {
			if (thread_queue->Pop(thread_task)) {
#ifdef ECS_TASK_MANAGER_WRAPPER
				task_manager->ExecuteDynamicTask(thread_task.task, thread_id);
#else
				thread_task.task.function(thread_id, task_manager->m_world, thread_task.task.data);
#endif
			}
			else {
				auto go_to_sleep = [=]() {
					if (task_manager->m_wait_type == ECS_TASK_MANAGER_WAIT_SLEEP) {
						task_manager->SleepThread(thread_id);
					}
					else {
						task_manager->SpinThread(thread_id);
					}
				};

				// We failed to get a thread task from our queue.
				// If the stealing is enabled, try to do it
				if (function::HasFlag(task_manager->m_wait_type, ECS_TASK_MANAGER_WAIT_STEAL)) {
					thread_task.task = task_manager->StealTask(thread_id);
					// We managed to get a task
					if (thread_task.task.function != nullptr) {
						task_manager->ExecuteDynamicTask(thread_task.task, thread_id);
					}
					else {
						// There is nothing to be stolen - either try to get the next static task or go to sleep if overpassed
						int task_index = task_manager->GetThreadTaskIndex();
						if (task_index >= task_manager->m_tasks.size) {
							// Out of bounds - go to sleep
							go_to_sleep();
						}

						if (task_manager->m_tasks[task_index].initialize_lock.try_lock()) {
							// We managed to get the lock - we should do the initialization
							// We will anounce to the other threads that we finished the serial
							// part by setting the high bit
							task_manager->ExecuteStaticTask(task_manager->m_tasks[task_index].task, thread_id);
							// Increment the static index and then set the high bit
							task_manager->IncrementThreadTaskIndex();

							BitLock(task_manager->m_tasks[task_index].initialize_lock.value, 7);
						}
						else {
							// Spin wait while the serial part is being finished
							BitWaitLocked(task_manager->m_tasks[task_index].initialize_lock.value, 7);
						}
					}
				}
				else {
					go_to_sleep();
				}
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

}