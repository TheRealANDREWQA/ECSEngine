#include "ecspch.h"
#include "TaskManager.h"
#include "../ECS/World.h"
#include "ECSEngineConsole.h"


// In milliseconds
#define SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL 25

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

	// Returns the old value of the thread wrapper
	void TaskManagerChangeWrapperMode(
		TaskManager* task_manager,
		ThreadFunctionWrapperData new_wrapper,
		ThreadFunctionWrapperData* old_wrapper
	) {
		ECS_ASSERT(new_wrapper.data_size <= sizeof(task_manager->m_static_wrapper_data_storage));

		old_wrapper->function = new_wrapper.function;
		old_wrapper->data_size = new_wrapper.data_size;
		if (new_wrapper.data_size > 0) {
			memcpy(old_wrapper->data, new_wrapper.data, new_wrapper.data_size);
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
		GlobalMemoryManager* memory,
		size_t thread_linear_allocator_size,
		size_t max_dynamic_tasks
	) : m_last_thread_index(0), m_world(nullptr)
#ifdef ECS_TASK_MANAGER_WRAPPER 
		, m_static_wrapper({ ThreadWrapperNone, &m_static_wrapper_data_storage, 0 })
		, m_dynamic_wrapper({ ThreadWrapperNone, &m_dynamic_wrapper_data_storage, 0 })
#endif
	{
		thread_linear_allocator_size = thread_linear_allocator_size == 0 ? ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE : thread_linear_allocator_size;
		max_dynamic_tasks = max_dynamic_tasks == 0 ? ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD : max_dynamic_tasks;

		size_t total_memory = 0;
		
		// dynamic queue; adding a cache line in order to prevent cache line bouncing when a thread is accessing its queue
		// The thread queue buffer to be allocated on a separate cache line as well as the ThreadQueue itself
		total_memory += (ThreadQueue::MemoryOf(max_dynamic_tasks) + ECS_CACHE_LINE_SIZE * 2) * thread_count;
		
		// pointers to thread queues;
		total_memory += sizeof(ThreadQueue*) * thread_count;

		// condition variables
		total_memory += sizeof(ConditionVariable) * thread_count;

		// thread handles
		total_memory += sizeof(void*) * thread_count;

		// Atomic counters / 2 cache lines are needed but multiply by 2 again to account for the padding
		// necessary to align these fields
		total_memory += ECS_CACHE_LINE_SIZE * 2 * 2;

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
			// Align the thread queue buffer on a new cache line boundary
			buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
			ThreadQueue temp_queue = ThreadQueue((void*)buffer_start, max_dynamic_tasks);
			buffer_start += ThreadQueue::MemoryOf(max_dynamic_tasks);

			// Align the thread queue such that is on a separate cache line
			buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
			ThreadQueue* buffer_reinterpretation = (ThreadQueue*)buffer_start;
			*buffer_reinterpretation = temp_queue;
			m_thread_queue[index] = buffer_reinterpretation;
			buffer_start += sizeof(ThreadQueue);
		}

		buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
		m_thread_task_index = (std::atomic<unsigned int>*)buffer_start;
		buffer_start += sizeof(std::atomic<unsigned int>);
		buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
		m_last_thread_index = (std::atomic<unsigned int>*)buffer_start;
		buffer_start += sizeof(std::atomic<unsigned int>);
		buffer_start = function::AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);

		m_thread_task_index->store(0, ECS_RELAXED);
		m_last_thread_index->store(0, ECS_RELAXED);

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
			buffer_start = function::AlignPointer(buffer_start, alignof(RingBuffer));
			m_dynamic_task_allocators[index] = (RingBuffer*)buffer_start;
			buffer_start += sizeof(RingBuffer);

			m_dynamic_task_allocators[index]->InitializeFromBuffer(buffer_start, DYNAMIC_TASK_ALLOCATOR_SIZE);
			m_dynamic_task_allocators[index]->lock.unlock();
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
		ECS_ASSERT(task.name.size > 0);

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
		unsigned int thread_count = GetThreadCount();

		unsigned int last_thread_index = m_last_thread_index->fetch_add(1, ECS_RELAXED);
		last_thread_index = last_thread_index % thread_count;
		AddDynamicTaskWithAffinity(task, last_thread_index, can_be_stolen);
		return last_thread_index;
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
		size_t total_allocation_size = task.data_size + task.name.size;

		if (total_allocation_size > 0) {
			void* allocation = m_dynamic_task_allocators[thread_id]->Allocate(total_allocation_size, 8);
			uintptr_t allocation_ptr = (uintptr_t)allocation;

			if (task.data_size > 0) {
				void* old_data = task.data;
				task.data = (void*)allocation_ptr;
				allocation_ptr += task.data_size;
				memcpy(task.data, old_data, task.data_size);
			}

			if (task.name.size > 0) {
				task.name.InitializeAndCopy(allocation_ptr, task.name);
			}
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

	template<typename ExtractFunctor, typename Functor>
	void AddDynamicTaskGroupImplementation(unsigned int thread_count, unsigned int task_count, size_t data_size, ExtractFunctor&& extract_functor, Functor&& functor) {
		unsigned int per_thread_count = task_count / thread_count;
		unsigned int remainder = task_count % thread_count;

		if (per_thread_count == 0) {
			for (unsigned int index = 0; index < remainder; index++) {
				functor(extract_functor(index), index);
			}
		}
		else {
			unsigned int task_index = 0;
			for (unsigned int index = 0; index < thread_count; index++) {
				for (unsigned int per_thread_index = 0; per_thread_index < per_thread_count; per_thread_index++) {
					functor(extract_functor(task_index++), index);
				}

				if (remainder > 0) {
					functor(extract_functor(task_index++), index);
					remainder--;
				}
			}
		}
	}

	void TaskManager::AddDynamicTaskGroup(ThreadFunction function, const char* function_name, Stream<void*> data, size_t data_size, bool can_be_stolen)
	{
		AddDynamicTaskGroupImplementation(GetThreadCount(), data.size, data_size, [=](unsigned int index) { return data[index]; }, [=](void* data, unsigned int index) {
			AddDynamicTaskWithAffinity({ function, data, data_size, function_name }, index, can_be_stolen);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskGroup(
		ThreadFunction function, 
		const char* function_name, 
		void* data, 
		unsigned int task_count, 
		size_t data_size, 
		bool can_be_stolen
	)
	{
		AddDynamicTaskGroupImplementation(GetThreadCount(), task_count, data_size, [=](unsigned int index) { return data; }, [=](void* data, unsigned int index) {
			AddDynamicTaskWithAffinity({ function, data, data_size, function_name }, index, can_be_stolen);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskGroupAndWake(ThreadFunction function, const char* function_name, Stream<void*> data, size_t data_size, bool can_be_stolen)
	{
		AddDynamicTaskGroupImplementation(GetThreadCount(), data.size, data_size, [=](unsigned int index) { return data[index]; }, [=](void* data, unsigned int index) {
			AddDynamicTaskAndWakeWithAffinity({ function, data, data_size, function_name }, index, can_be_stolen);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddDynamicTaskGroupAndWake(
		ThreadFunction function, 
		const char* function_name, 
		void* data,
		unsigned int task_count, 
		size_t data_size, 
		bool can_be_stolen
	)
	{
		AddDynamicTaskGroupImplementation(GetThreadCount(), task_count, data_size, [=](unsigned int index) { return data; }, [=](void* data, unsigned int index) {
			AddDynamicTaskAndWakeWithAffinity({ function, data, data_size, function_name }, index, can_be_stolen);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void* TaskManager::AllocateTempBuffer(unsigned int thread_id, size_t size, size_t alignment)
	{
		return m_thread_linear_allocators[thread_id]->Allocate(size, alignment);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ChangeStaticWrapperMode(ThreadFunctionWrapperData wrapper_data)
	{
		TaskManagerChangeWrapperMode(this, wrapper_data, &m_static_wrapper);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ChangeDynamicWrapperMode(ThreadFunctionWrapperData wrapper_data) {
		TaskManagerChangeWrapperMode(this, wrapper_data, &m_dynamic_wrapper);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearThreadAllocators()
	{
		for (size_t index = 0; index < GetThreadCount(); index++) {
			m_thread_linear_allocators[index]->Clear();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTemporaryAllocators()
	{
		ClearThreadAllocators();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTaskIndex() {
		m_thread_task_index->store(0, ECS_RELEASE);
		// Make all initialize locks to 0 again
		for (unsigned int index = 0; index < m_tasks.size; index++) {
			m_tasks[index].initialize_lock.unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearTaskStream() {
		m_tasks.size = 0;
		m_static_task_data_allocator.Clear();
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
		m_thread_task_index->fetch_sub(1, ECS_ACQ_REL);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	std::atomic<size_t> THREAD_COUNTS_TEST[16] = { 0 };

	ECS_THREAD_TASK(EmptyTask) {
		THREAD_COUNTS_TEST[thread_id].fetch_add(1);
	}

	void TaskManager::DoFrame(bool wait_frame)
	{
		m_thread_task_index->store(0, ECS_RELAXED);

		// Reset the is_frame_done condition_variable
		if (wait_frame) {
			m_is_frame_done.Reset();
		}

		unsigned int thread_count = GetThreadCount();
		if (function::HasFlag(m_wait_type, ECS_TASK_MANAGER_WAIT_SLEEP)) {
			// Wake the threads
			WakeThreads(true);
		}
		else {
			// Push an empty task such that the threads will wake up and start doing the static tasks
			for (unsigned int index = 0; index < thread_count; index++) {
				AddDynamicTaskAndWakeWithAffinity(ECS_THREAD_TASK_NAME(EmptyTask, nullptr, 0), index, false);
			}
		}

		if (wait_frame) {
			m_is_frame_done.Wait(thread_count);
			ECS_ASSERT(m_is_frame_done.signal_count.load(ECS_RELAXED) == 0);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	bool TaskManager::ExecuteStaticTask(unsigned int thread_id)
	{
		unsigned int task_index = GetThreadTaskIndex();

		if (task_index >= m_tasks.size) {
			return false;
		}

		if (m_tasks[task_index].initialize_lock.try_lock()) {
			// We managed to get the lock - we should do the initialization
			// We will anounce to the other threads that we finished the serial
			// part by setting the high bit
			m_static_wrapper.function(thread_id, thread_id, m_world, m_tasks[task_index].task, m_static_wrapper.data);
			// Increment the static index and then set the high bit
			IncrementThreadTaskIndex();

			BitLockWithNotify(m_tasks[task_index].initialize_lock.value, 7);
		}
		else {
			// Wait while the serial part is being finished
			BitWaitUnlocked(m_tasks[task_index].initialize_lock.value, 7, true);
		}

		return true;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ExecuteDynamicTask(ThreadTask task, unsigned int thread_id, unsigned int task_thread_id)
	{
		m_dynamic_wrapper.function(thread_id, task_thread_id, m_world, task, m_dynamic_wrapper.data);

		size_t allocation_size = task.data_size + task.name.size * sizeof(char);

		if (task.data_size > 0) {
			m_dynamic_task_allocators[task_thread_id]->Finish(task.data, allocation_size);
		}
		else if (task.name.size > 0) {
			m_dynamic_task_allocators[task_thread_id]->Finish(task.name.buffer, allocation_size);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(FinishFrameTaskDynamic) {
		world->task_manager->m_is_frame_done.Notify();
		THREAD_COUNTS_TEST[thread_id].fetch_add(1);
	}

	ECS_THREAD_TASK(FinishFrameTask) {
		// Flush the entity manager as well
		world->entity_manager->EndFrame();

		unsigned int thread_count = world->task_manager->GetThreadCount();
		world->task_manager->AddDynamicTaskGroup(WITH_NAME(FinishFrameTaskDynamic), nullptr, thread_count, 0, false);
	}

	void TaskManager::FinishStaticTasks()
	{
		AddTask(ECS_THREAD_TASK_NAME(FinishFrameTask, nullptr, 0));
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::IncrementThreadTaskIndex() {
		m_thread_task_index->fetch_add(1, ECS_ACQ_REL);
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

	unsigned int TaskManager::GetThreadTaskIndex() const {
		return m_thread_task_index->load(ECS_ACQUIRE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic TaskManager::GetThreadTempAllocator(unsigned int thread_id) const
	{
		return GetAllocatorPolymorphic(m_thread_linear_allocators[thread_id]);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadFunctionWrapperData TaskManager::GetStaticThreadWrapper(CapacityStream<void>* data) const
	{
		ThreadFunctionWrapperData wrapper = m_static_wrapper;
		if (data != nullptr) {
			data->Add(Stream<void>(m_static_wrapper.data, m_static_wrapper.data_size));
			wrapper.data = data->buffer;
		}
		return wrapper;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadFunctionWrapperData TaskManager::GetDynamicThreadWrapper(CapacityStream<void>* data) const
	{
		ThreadFunctionWrapperData wrapper = m_dynamic_wrapper;
		if (data != nullptr) {
			data->Add(Stream<void>(m_dynamic_wrapper.data, m_dynamic_wrapper.data_size));
			wrapper.data = data->buffer;
		}
		return wrapper;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	bool TaskManager::IsSleeping(unsigned int thread_id) const
	{
		if (m_wait_type == ECS_TASK_MANAGER_WAIT_SLEEP) {
			return m_sleep_wait[thread_id].WaitingThreadCount() > 0;		
		}
		else {
			if (GetThreadQueue(thread_id)->GetSize() == 0) {
				// If the static index is out of bounds assume that the thread is spinning
				return GetThreadTaskIndex() < m_tasks.size;
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ReserveTasks(unsigned int count) {
		m_tasks.ReserveNewElements(count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::Reset()
	{
		ClearTemporaryAllocators();
		ResetStaticTasks();

		SleepThreads();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ResetStaticTasks()
	{
		ClearTaskStream();
		m_tasks.FreeBuffer();
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
		m_thread_task_index->store(value, ECS_RELEASE);
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
		condition_variable->Reset();

		AddDynamicTaskGroupAndWake(WITH_NAME(SleepTask), condition_variable, thread_count, 0, false);
		if (wait_until_all_sleep) {
			condition_variable->Wait(thread_count);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SleepUntilDynamicTasksFinish(size_t max_period) {
#ifdef ECS_TASK_MANAGER_WRAPPER
		ECS_ASSERT(m_dynamic_wrapper.function == ThreadWrapperCountTasks);

		Timer timer;
		timer.SetMarker();

		ThreadWrapperCountTasksData* data = (ThreadWrapperCountTasksData*)m_dynamic_wrapper.data;
		TickWait<'!'>(SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL, data->count, (unsigned int)0);
#endif
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetWaitType(ECS_TASK_MANAGER_WAIT_TYPE wait_type)
	{
		m_wait_type = wait_type;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ThreadTask TaskManager::StealTask(unsigned int& thread_index)
	{
		DynamicThreadTask task;
		task.task.function = nullptr;

		// Try to go to the next index
		// Because going from a fixed index will cause multiple threads
		// trying to steal a job into waiting for each other instead of exploring multiple queues
		// Use a different increment in order to avoid convoying - if threads
		// 4 and 5 would want to steal tasks and 5 gets stuck then 4 will fight for stealing tasks
		// with the 5th thread. If they have different increments this avoid this problem - especially in different
		// directions. With this method the thread 4 decrements and goes downwards meanwhile the thread 5 will go upwards
		bool direction = thread_index & 0x1;

		unsigned int thread_count = GetThreadCount();
		for (unsigned int index = 0; index < thread_count - 1; index++) {
			if (direction) {
				thread_index = thread_index == thread_count - 1 ? 0 : thread_index + 1;
			}
			else {
				thread_index = thread_index == 0 ? thread_count - 1 : thread_index - 1;
			}
			ThreadQueue* current_queue = GetThreadQueue(thread_index);
			if (current_queue->TryLock()) {
				if (current_queue->GetQueue()->Peek(task)) {
					if (task.can_be_stolen) {
						// Pop the task, release the lock and then return
						current_queue->PopNonAtomic(task);
						current_queue->Unlock();

						return task.task;
					}
					else {
						// Indicate that we still don't have a match
						task.task.function = nullptr;
					}
				}
				current_queue->Unlock();
			}
		}

		return task.task;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ChangeThreadPriorityTask) {
		OS::ECS_THREAD_PRIORITY priority = *(OS::ECS_THREAD_PRIORITY*)_data;
		OS::ChangeThreadPriority(priority);
	}

	void TaskManager::SetThreadPriorities(OS::ECS_THREAD_PRIORITY priority)
	{
		for (unsigned int index = 0; index < GetThreadCount(); index++) {			
			AddDynamicTaskAndWakeWithAffinity(ECS_THREAD_TASK_NAME(ChangeThreadPriorityTask, &priority, sizeof(priority)), index, false);
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

		// TODO: At the moment waiting is broken. GetExitCodeThread and WaitForSingleObject
		// are the functions to use but they keep looping indefinetely for some reason.
		// Disable this feature for the time being

		//if (wait_for_termination) {
		//	for (size_t index = 0; index < GetThreadCount(); index++) {
		//		// Use GetExitCodeThread
		//		DWORD exit_code = -1;
		//		BOOL status = GetExitCodeThread(m_thread_handles[index], &exit_code);
		//		while (!status) {
		//			status = GetExitCodeThread(m_thread_handles[index], &exit_code);
		//		}

		//		ECS_ASSERT(exit_code == 0);
		//	}
		//}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::WakeThread(unsigned int thread_id, bool force_wake) {
		if (force_wake) {
			m_sleep_wait[thread_id].ResetAndNotify();
		}
		else {
			m_sleep_wait[thread_id].Notify();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::WakeThreads(bool force_wake) {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			WakeThread(index, force_wake);
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
				task_manager->ExecuteDynamicTask(thread_task.task, thread_id, thread_id);
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

				auto go_to_sleep_dynamic_task_check = [&]() {
					// We need to recheck before going to sleep if there is something on the dynamic task queue
					bool was_executed = false;
					if (thread_queue->Pop(thread_task)) {
						was_executed = true;
						task_manager->ExecuteDynamicTask(thread_task.task, thread_id, thread_id);
					}
					if (!was_executed) {
						go_to_sleep();
					}
				};

				// We failed to get a thread task from our queue.
				// If the stealing is enabled, try to do it
				if (function::HasFlag(task_manager->m_wait_type, ECS_TASK_MANAGER_WAIT_STEAL)) {
					unsigned int stolen_thread_id = thread_id;
					thread_task.task = task_manager->StealTask(stolen_thread_id);
					// We managed to get a task
					if (thread_task.task.function != nullptr) {
						task_manager->ExecuteDynamicTask(thread_task.task, thread_id, stolen_thread_id);
					}
					else {
						// There is nothing to be stolen - either try to get the next static task or go to wait if overpassed
						if (!task_manager->ExecuteStaticTask(thread_id)) {
							go_to_sleep_dynamic_task_check();
						}
					}
				}
				else {
					if (!task_manager->ExecuteStaticTask(thread_id)) {
						go_to_sleep_dynamic_task_check();
					}
				}
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

}