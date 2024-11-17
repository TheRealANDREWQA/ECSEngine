#include "ecspch.h"
#include "TaskManager.h"
#include "../ECS/World.h"
#include "../Utilities/Crash.h"
#include "../Profiling/CPUFrameProfilerGlobal.h"

// In milliseconds
#define SLEEP_UNTIL_DYNAMIC_TASKS_FINISH_INTERVAL 25

// The allocator size for the data + task.name for dynamic tasks
// This is per thread
#define DYNAMIC_TASK_ALLOCATOR_SIZE (25'000)

#define STATIC_TASK_ALLOCATOR_SIZE (10'000)

#define RECOVERY_POINT_LONG_JUMP_SLEEP_VALUE 2

namespace ECSEngine {

	ECS_INLINE AllocatorPolymorphic StaticTaskAllocator(TaskManager* manager) {
		return &manager->m_static_task_data_allocator;
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
		total_memory += (ThreadQueue::MemoryOf(max_dynamic_tasks) + sizeof(CacheAligned<ThreadQueue>) + ECS_CACHE_LINE_SIZE) * thread_count;

		// condition variables
		total_memory += sizeof(CacheAligned<ConditionVariable>) * thread_count;

		// thread handles
		total_memory += sizeof(void*) * thread_count;

		// Atomic counters / 2 cache lines are needed but multiply by 2 again to account for the padding
		// necessary to align these fields
		total_memory += ECS_CACHE_LINE_SIZE * 2 * 2;

		// The task allocator has a fixed size
		// +7 bytes to align the RingBuffer* for each thread
		total_memory += (DYNAMIC_TASK_ALLOCATOR_SIZE + sizeof(CacheAligned<RingBuffer>)) * thread_count + ECS_CACHE_LINE_SIZE;

		total_memory += STATIC_TASK_ALLOCATOR_SIZE;

		// Align the recovery pojnts to another cache line
		total_memory += ECS_CACHE_LINE_SIZE + sizeof(jmp_buf) * thread_count;

		void* allocation = memory->Allocate(total_memory, ECS_CACHE_LINE_SIZE);
		uintptr_t buffer_start = (uintptr_t)allocation;
		m_thread_queue.InitializeFromBuffer(buffer_start, thread_count);
		
		// constructing thread queues and setting them in stream
		for (size_t index = 0; index < thread_count; index++) {
			// Align the thread queue buffer on a new cache line boundary
			buffer_start = AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
			m_thread_queue[index].value.InitializeFromBuffer(buffer_start, max_dynamic_tasks);
		}

		buffer_start = AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
		m_thread_task_index = (std::atomic<unsigned int>*)buffer_start;
		buffer_start += sizeof(std::atomic<unsigned int>);
		buffer_start = AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
		m_last_thread_index = (std::atomic<unsigned int>*)buffer_start;
		buffer_start += sizeof(std::atomic<unsigned int>);
		buffer_start = AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);

		m_thread_task_index->store(0, ECS_RELAXED);
		m_last_thread_index->store(0, ECS_RELAXED);

		// sleep variables
		buffer_start = AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
		m_sleep_wait = (CacheAligned<ConditionVariable>*)buffer_start;
		for (size_t index = 0; index < thread_count; index++) {
			m_sleep_wait[index].value.Reset();
		}
		buffer_start += sizeof(CacheAligned<ConditionVariable>) * thread_count;

		// thread handles
		buffer_start = AlignPointer(buffer_start, alignof(void*));
		m_thread_handles = (void**)buffer_start;
		buffer_start += sizeof(void*) * thread_count;

		m_dynamic_task_allocators = (CacheAligned<RingBuffer>*)buffer_start;
		buffer_start += sizeof(CacheAligned<RingBuffer>) * thread_count;

		for (size_t index = 0; index < thread_count; index++) {
			m_dynamic_task_allocators[index].value.InitializeFromBuffer(buffer_start, DYNAMIC_TASK_ALLOCATOR_SIZE);
			m_dynamic_task_allocators[index].value.lock.Clear();
		}

		m_static_task_data_allocator = LinearAllocator((void*)buffer_start, STATIC_TASK_ALLOCATOR_SIZE);
		buffer_start += STATIC_TASK_ALLOCATOR_SIZE;

		// At last the recovery points must be allocated
		m_threads_reset_point = (jmp_buf*)AlignPointer(buffer_start, ECS_CACHE_LINE_SIZE);
		buffer_start += sizeof(jmp_buf) * thread_count;

		m_tasks = ResizableStream<StaticTask>(memory, 0);

		// Now the thread local allocators
		m_thread_linear_allocators = (CacheAligned<LinearAllocator>*)memory->Allocate(
			(sizeof(CacheAligned<LinearAllocator>) + thread_linear_allocator_size) * thread_count, 
			ECS_CACHE_LINE_SIZE
		);

		void* thread_linear_allocators_buffer = OffsetPointer(m_thread_linear_allocators, sizeof(CacheAligned<LinearAllocator>) * thread_count);
		for (size_t index = 0; index < thread_count; index++) {
			m_thread_linear_allocators[index].value = LinearAllocator(thread_linear_allocators_buffer, thread_linear_allocator_size);
			thread_linear_allocators_buffer = OffsetPointer(thread_linear_allocators_buffer, thread_linear_allocator_size);
		}

		SetWaitType(ECS_TASK_MANAGER_WAIT_SLEEP);
		m_exception_handlers.Initialize(m_tasks.allocator, 0);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddTask(StaticThreadTask task) {
		ECS_ASSERT(task.task.name.size > 0, "Missing Static Task name");

		task.task.data = CopyNonZero(StaticTaskAllocator(this), task.task.data, task.task.data_size);

#if 0
		if (task.name != nullptr) {
			task.name = StringCopy(TaskAllocator(this), task.name).buffer;
		}
#else
		task.task.name = StringCopy(StaticTaskAllocator(this), task.task.name).buffer;
#endif
		unsigned int index = m_tasks.ReserveRange();
		m_tasks[index].task = task.task;
		m_tasks[index].barrier.ClearCount();
		m_tasks[index].barrier.SetTarget(task.barrier_task ? GetThreadCount() : 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::AddTasks(Stream<StaticThreadTask> tasks)
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
			void* allocation = m_dynamic_task_allocators[thread_id].value.Allocate(total_allocation_size, 8);
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
		m_thread_queue[thread_id].value.Push(dynamic_task);
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
		return m_thread_linear_allocators[thread_id].value.Allocate(size, alignment);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic TaskManager::AllocatorTemp(unsigned int thread_id) const
	{
		return &m_thread_linear_allocators[thread_id].value;
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

	struct ComposeWrapperData {
		ThreadFunctionWrapperData wrappers[2];
	};

	static ThreadFunctionWrapperData GetWrapperFromComposed(const ComposeWrapperData* wrapper_data, unsigned int index) {
		ThreadFunctionWrapperData maintain_wrapper;
		if (index == 0) {
			maintain_wrapper = wrapper_data->wrappers[0];
			if (wrapper_data->wrappers[0].data_size > 0) {
				maintain_wrapper.data = OffsetPointer(wrapper_data, sizeof(*wrapper_data));
			}
		}
		else {
			maintain_wrapper = wrapper_data->wrappers[1];
			if (wrapper_data->wrappers[1].data_size > 0) {
				maintain_wrapper.data = OffsetPointer(wrapper_data, sizeof(*wrapper_data) + wrapper_data->wrappers[1].data_size);
			}
		}
		return maintain_wrapper;
	}

	ECS_THREAD_WRAPPER_TASK(ComposeWrapper) {
		ComposeWrapperData* data = (ComposeWrapperData*)_wrapper_data;

		// To achieve composition, for the first wrapper give an impersonated
		// thread task that will call the second wrapper which in turn should call
		// the real function

		struct LocalWrapperData {
			ComposeWrapperData* data;
			unsigned int task_thread_id;
			ThreadTask task;
		};

		auto task_function_wrapper = [](unsigned int thread_id, World* world, void* _data) {
			LocalWrapperData* data = (LocalWrapperData*)_data;
			ComposeWrapperData* wrapper_data = data->data;

			void* current_wrapper_data = wrapper_data->wrappers[1].data;
			void* embedded_wrapper_data = OffsetPointer(wrapper_data, sizeof(*wrapper_data) + wrapper_data->wrappers[0].data_size);
			if (wrapper_data->wrappers[1].data_size > 0) {
				current_wrapper_data = embedded_wrapper_data;
			}
			wrapper_data->wrappers[1].function(thread_id, data->task_thread_id, world, data->task, current_wrapper_data);
		};

		void* embedded_wrapper_data = OffsetPointer(data, sizeof(*data));
		void* current_wrapper_data = data->wrappers[0].data;
		if (data->wrappers[0].data_size > 0) {
			current_wrapper_data = embedded_wrapper_data;
			embedded_wrapper_data = OffsetPointer(embedded_wrapper_data, data->wrappers[0].data_size);
		}

		LocalWrapperData local_wrapper_data = { data, task_thread_id, task };
		ThreadTask local_task = { task_function_wrapper, &local_wrapper_data, sizeof(local_wrapper_data), task.name };
		data->wrappers[0].function(thread_id, task_thread_id, world, local_task, current_wrapper_data);
	}

	static ThreadFunctionWrapperData CreateComposeWrapper(
		ThreadFunctionWrapperData existing_wrapper, 
		ThreadFunctionWrapperData addition_wrapper, 
		bool run_after_existing_one,
		CapacityStream<size_t> compose_wrapper_data_storage
	) {
		ComposeWrapperData* compose_data = (ComposeWrapperData*)compose_wrapper_data_storage.buffer;
		void* embedded_data = OffsetPointer(compose_data, sizeof(*compose_data));
		unsigned int compose_data_write_size = sizeof(*compose_data);

		ECS_ASSERT(compose_wrapper_data_storage.capacity >= compose_data_write_size + existing_wrapper.data_size + addition_wrapper.data_size);

		auto set_wrapper = [&](ThreadFunctionWrapperData wrapper, unsigned int write_index) {
			compose_data->wrappers[write_index] = wrapper;
			if (wrapper.data_size > 0) {
				memcpy(embedded_data, wrapper.data, wrapper.data_size);
				embedded_data = OffsetPointer(embedded_data, wrapper.data_size);
				compose_data_write_size += wrapper.data_size;
			}
		};

		if (run_after_existing_one) {
			set_wrapper(existing_wrapper, 0);
			set_wrapper(addition_wrapper, 1);
		}
		else {
			set_wrapper(addition_wrapper, 0);
			set_wrapper(existing_wrapper, 1);
		}

		return { ComposeWrapper, compose_data, compose_data_write_size };
	}

	void TaskManager::ComposeStaticWrapper(ThreadFunctionWrapperData addition_wrapper, bool run_after_existing_one)
	{
		ECS_STACK_CAPACITY_STREAM(size_t, compose_data_storage, 64);
		ThreadFunctionWrapperData composed_wrapper = CreateComposeWrapper(m_static_wrapper, addition_wrapper, run_after_existing_one, compose_data_storage);
		ChangeStaticWrapperMode(composed_wrapper);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ComposeDynamicWrapper(ThreadFunctionWrapperData addition_wrapper, bool run_after_existing_one)
	{
		ECS_STACK_CAPACITY_STREAM(size_t, compose_data_storage, 64);
		ThreadFunctionWrapperData composed_wrapper = CreateComposeWrapper(m_dynamic_wrapper, addition_wrapper, run_after_existing_one, compose_data_storage);
		ChangeDynamicWrapperMode(composed_wrapper);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_WRAPPER_TASK(FrameTimingWrapper) {
		Timer timer;
		CPUFrameProfilerPush(thread_id, task.name);
		task.function(thread_id, world, task.data);
		CPUFrameProfilerPop(thread_id, timer.GetDurationFloat(ECS_TIMER_DURATION_US));
	}

	void TaskManager::ComposeFrameTimingWrappers()
	{
		ThreadFunctionWrapperData frame_wrapper = { FrameTimingWrapper, nullptr, 0 };
		ComposeDynamicWrapper(frame_wrapper, false);
		ComposeStaticWrapper(frame_wrapper, false);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearThreadAllocators()
	{
		for (size_t index = 0; index < GetThreadCount(); index++) {
			ClearThreadAllocator(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ClearThreadAllocator(unsigned int thread_id)
	{
		m_thread_linear_allocators[thread_id].value.Clear();
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
			m_tasks[index].barrier.ClearCount();
			m_tasks[index].single_thread_finished.Clear();
			m_tasks[index].single_thread_lock.Clear();
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

			// Register the handle. We need to create a copy of the handle since
			// thread.detach() will make the handle invalid
			m_thread_handles[index] = OS::DuplicateThreadHandle(thread.native_handle());

			thread.detach();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::DestroyThreads()
	{
		for (unsigned int index = 0; index < GetThreadCount(); index++) {
			// Close our thread handle
			OS::CloseThreadHandle(m_thread_handles[index]);
			AddDynamicTaskAndWakeWithAffinity(ECS_THREAD_TASK_NAME(ExitThreadTask, nullptr, 0), index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::DecrementThreadTaskIndex()
	{
		m_thread_task_index->fetch_sub(1, ECS_ACQ_REL);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(EmptyTask) {}

	void TaskManager::DoFrame(bool wait_frame)
	{
		m_thread_task_index->store(0, ECS_RELAXED);

		// Reset the is_frame_done condition_variable
		if (wait_frame) {
			m_is_frame_done.Reset();
		}

		unsigned int thread_count = GetThreadCount();
		if (HasFlag(m_wait_type, ECS_TASK_MANAGER_WAIT_SLEEP)) {
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
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	bool TaskManager::ExecuteStaticTask(unsigned int thread_id)
	{
		unsigned int task_index = GetThreadTaskIndex();

		if (task_index >= m_tasks.size) {
			return false;
		}

		// Check to see if a crash has occured - we might endlessly come here over and
		// Over again since the static task counter won't be incremented and the barrier value
		// Will be correctly unlocked
		if (ECS_GLOBAL_CRASH_IN_PROGRESS.load(ECS_RELAXED) > 0) {
			// Notify the frame counter and go to the recovery point
			m_is_frame_done.Notify();
			ResetThreadToProcedure(thread_id, true);
		}

		// Try to acquire the single thread lock, 
		bool acquired_lock = m_tasks[task_index].single_thread_lock.TryLock();
		unsigned int target_value = m_tasks[task_index].barrier.Target();
		__try {
			if (acquired_lock) {
				// If this is a barrier, we need to wait for all threads to reach this task
				if (target_value > 1) {
					// We need to wait for the thread count - 1 since we don't increase the barrier count
					m_tasks[task_index].barrier.SpinWaitEx(target_value - 1, -1);
				}

				m_static_wrapper.function(thread_id, thread_id, m_world, m_tasks[task_index].task, m_static_wrapper.data);
				// Increment the static index and then set the high bit
				IncrementThreadTaskIndex();
				// The finished lock is done in the __finally block to ensure that
				// the threads are woken up in case we crash
			}
			else {
				if (target_value > 1) {
					m_tasks[task_index].barrier.EnterEx();
				}

				// Wait while the serial part is being finished
				m_tasks[task_index].single_thread_finished.WaitSignaled();
			}
		}
		__finally {
			if (acquired_lock) {
				// We need to use try lock here since the crashing thread might set this before us
				bool acquired_finished_lock = m_tasks[task_index].single_thread_finished.TryLock();
				if (acquired_finished_lock) {
					// Notify the waiting threads
					WakeByAddressAll(&m_tasks[task_index].single_thread_finished);
				}
			}
		}

		return true;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ExecuteDynamicTask(ThreadTask task, unsigned int thread_id, unsigned int task_thread_id)
	{
		m_dynamic_wrapper.function(thread_id, task_thread_id, m_world, task, m_dynamic_wrapper.data);

		size_t allocation_size = task.data_size + task.name.size * sizeof(char);

		if (task.data_size > 0) {
			m_dynamic_task_allocators[task_thread_id].value.Finish(task.data, allocation_size);
		}
		else if (task.name.size > 0) {
			m_dynamic_task_allocators[task_thread_id].value.Finish(task.name.buffer, allocation_size);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(FinishFrameTaskDynamic) {
		world->task_manager->m_is_frame_done.Notify();
	}

	ECS_THREAD_TASK(FinishFrameTask) {
		// Flush the entity manager as well
		world->entity_manager->EndFrame();
		unsigned int thread_count = world->task_manager->GetThreadCount();
		world->task_manager->AddDynamicTaskGroup(WITH_NAME(FinishFrameTaskDynamic), nullptr, thread_count, 0, false);
	}

	void TaskManager::FinishStaticTasks()
	{
		AddTask(ECS_STATIC_THREAD_TASK_NAME(FinishFrameTask, nullptr, 0, true));
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::FindTask(Stream<char> task_name) const
	{
		return m_tasks.Find(task_name, [](const StaticTask& task) {
			return task.task.name;
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int TaskManager::FindThreadID(size_t os_thread_id) const
	{
		unsigned int thread_count = GetThreadCount();
		for (unsigned int index = 0; index < thread_count; index++) {
			size_t current_os_thread_id = OS::GetThreadID(m_thread_handles[index]);
			if (current_os_thread_id == os_thread_id) {
				return index;
			}
		}
		return -1;
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
			return m_sleep_wait[thread_id].value.WaitingThreadCount() > 0;		
		}
		else {
			if (GetThreadQueue(thread_id)->GetSize() == 0) {
				// If the static index is out of bounds assume that the thread is spinning
				return GetThreadTaskIndex() >= m_tasks.size;
			}
			return false;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::PopExceptionHandler()
	{
		m_exception_handlers.size--;
	}

	void TaskManager::PopExceptionHandlerThreadSafe() {
		m_exception_handler_lock.Lock();
		PopExceptionHandler();
		m_exception_handler_lock.Unlock();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::PushExceptionHandler(TaskManagerExceptionHandler handler, void* data, size_t data_size)
	{
		ECS_ASSERT(data_size <= sizeof(ExceptionHandler::data));
		ExceptionHandler handler_element;
		handler_element.function = handler;
		if (data_size == 0) {
			handler_element.data_ptr = data;
			handler_element.is_data_ptr = true;
		}
		else {
			handler_element.is_data_ptr = false;
			memcpy(handler_element.data, data, data_size);
		}

		m_exception_handlers.Add(&handler_element);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::PushExceptionHandlerThreadSafe(TaskManagerExceptionHandler handler, void* data, size_t data_size) {
		m_exception_handler_lock.Lock();
		PushExceptionHandler(handler, data, data_size);
		m_exception_handler_lock.Unlock();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::RemoveTask(unsigned int task_index) {
		// The deallocation for the buffers at the moment is unnecessary since
		// The allocator is linear, but do it anyway in case in the future we change
		// The allocator
		if (m_tasks[task_index].task.data_size > 0) {
			Deallocate(StaticTaskAllocator(this), m_tasks[task_index].task.data);
		}
		m_tasks[task_index].task.name.Deallocate(StaticTaskAllocator(this));
		m_tasks.Remove(task_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ReserveTasks(unsigned int count) {
		m_tasks.Reserve(count);
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
		m_thread_queue[thread_id].value.Reset();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ResetDynamicQueues() {
		for (size_t index = 0; index < m_thread_queue.size; index++) {
			ResetDynamicQueue(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	bool TaskManager::ResumeThreads() const
	{
		unsigned int thread_count = GetThreadCount();
		size_t running_thread_id = OS::GetCurrentThreadID();
		bool managed_to_resume_threads = true;

		for (unsigned int index = 0; index < thread_count && managed_to_resume_threads; index++) {
			size_t os_thread_id = OS::GetThreadID(m_thread_handles[index]);
			if (os_thread_id != running_thread_id) {
				// We don't want to suspend ourselves in case the crashing thread produces the crash report
				bool success = OS::ResumeThread(m_thread_handles[index]);
				managed_to_resume_threads &= success;
			}
		}

		return managed_to_resume_threads;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ResetThreadToProcedure(unsigned int thread_id, bool sleep) const
	{
		longjmp(m_threads_reset_point[thread_id], sleep ? RECOVERY_POINT_LONG_JUMP_SLEEP_VALUE : RECOVERY_POINT_LONG_JUMP_SLEEP_VALUE - 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::ResetCurrentThreadToProcedure(bool sleep, bool abort_if_not_found) const
	{
		unsigned int thread_id = FindThreadID(OS::GetCurrentThreadID());
		if (thread_id == -1) {
			if (abort_if_not_found) {
				abort();
			}
		}
		else {
			ResetThreadToProcedure(thread_id, sleep);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::RemoveComposedDynamicWrapper(bool keep_first_wrapper)
	{
		ECS_ASSERT(m_dynamic_wrapper.function == ComposeWrapper);

		const ComposeWrapperData* wrapper_data = (const ComposeWrapperData*)m_dynamic_wrapper.data;
		ThreadFunctionWrapperData maintain_wrapper = GetWrapperFromComposed(wrapper_data, keep_first_wrapper ? 0 : 1);
		ChangeDynamicWrapperMode(maintain_wrapper);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::RemoveComposedStaticWrapper(bool keep_first_wrapper)
	{
		ECS_ASSERT(m_static_wrapper.function == ComposeWrapper);
		
		const ComposeWrapperData* wrapper_data = (const ComposeWrapperData*)m_static_wrapper.data;
		ThreadFunctionWrapperData maintain_wrapper = GetWrapperFromComposed(wrapper_data, keep_first_wrapper ? 0 : 1);
		ChangeStaticWrapperMode(maintain_wrapper);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SetTask(StaticThreadTask task, unsigned int index, size_t task_data_size) {
		ECS_ASSERT(m_tasks.size < m_tasks.capacity);
		if (task_data_size > 0) {
			void* allocation = Allocate(m_tasks.allocator, task_data_size);
			memcpy(allocation, task.task.data, task_data_size);
			task.task.data = allocation;
		}
		m_tasks[index].task = task.task;
		m_tasks[index].barrier.ClearCount();
		m_tasks[index].barrier.SetTarget(task.barrier_task ? GetThreadCount() : 1);
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
		m_sleep_wait[thread_id].value.Wait();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(SleepTask) {
		ConditionVariable* data = (ConditionVariable*)_data;
		data->Notify();
		world->task_manager->SleepThread(thread_id);
	}

	void TaskManager::SleepThreads(bool wait_until_all_sleep) {
		unsigned int thread_count = GetThreadCount();
		ConditionVariable* condition_variable = (ConditionVariable*)m_dynamic_task_allocators[0].value.Allocate(sizeof(ConditionVariable));
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

	static ECS_THREAD_TASK(TerminateThreadTask) {
		OS::ExitThread(0);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_WRAPPER_TASK(StaticCrashWrapper) {
		unsigned int crash_count = ECS_GLOBAL_CRASH_IN_PROGRESS.load(ECS_RELAXED);
		if (crash_count == 0) {
			task.function(thread_id, world, task.data);
		}
		else {
			// Go back to the recovery point and sleep afterwards
			// Before that tho we need to notify the main thread that
			// we acknowledged the crash
			world->task_manager->m_is_frame_done.Notify();
			world->task_manager->ResetThreadToProcedure(thread_id, true);
		}
	}

	static ECS_THREAD_WRAPPER_TASK(DynamicCrashWrapper) {
		// At the moment these 2 are the same
		StaticCrashWrapper(thread_id, task_thread_id, world, task, _wrapper_data);
	}

	void TaskManager::SetThreadWorldCrashWrappers()
	{
		// We want these to be run first
		ComposeStaticWrapper({ StaticCrashWrapper, nullptr, 0 }, false);
		ComposeDynamicWrapper({ DynamicCrashWrapper, nullptr, 0 }, false);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SignalStaticTasks()
	{
		unsigned int task_count = GetThreadTaskIndex();
		for (unsigned int index = task_count; index < m_tasks.size; index++) {
			// Try to acquire the lock and signal the finished single threaded lock,
			// if that failed we need to set the barrier count to target - 1 for those
			// tasks that have barriers in order to wake the thread executing the single
			// threaded part
			bool acquired_lock = m_tasks[index].single_thread_lock.TryLock();
			if (!acquired_lock) {
				unsigned int target_count = m_tasks[index].barrier.Target();
				if (target_count > 1) {
					unsigned int current_value = m_tasks[index].barrier.count.load(ECS_RELAXED);
					if (current_value < target_count - 1) {
						bool compare_success = m_tasks[index].barrier.count.compare_exchange_weak(current_value, target_count - 1);
						while (current_value < target_count - 1 && !compare_success) {
							compare_success = m_tasks[index].barrier.count.compare_exchange_weak(current_value, target_count - 1);
						}
						if (compare_success) {
							// Wake the threads waiting for this
							WakeByAddressAll(&m_tasks[index].barrier.count);
						}
					}
				}
			}

			bool acquired_finished_lock = m_tasks[index].single_thread_finished.TryLock();
			if (acquired_finished_lock) {
				// We also need to wake the threads waiting on this lock
				WakeByAddressAll(&m_tasks[index].single_thread_finished);
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void TaskManager::SpinThread(unsigned int thread_id)
	{
		ECS_ASSERT(false, "Spinning threads inside the task manager is currently broken since they are not notified with WakeByAddress that an element was added");
		//ThreadQueue* thread_queue = GetThreadQueue(thread_id);
		//thread_queue->SpinWaitElements();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	bool TaskManager::SuspendThreads() const
	{
		unsigned int thread_count = GetThreadCount();
		size_t running_thread_id = OS::GetCurrentThreadID();
		bool managed_to_suspend_threads = true;

		for (unsigned int index = 0; index < thread_count && managed_to_suspend_threads; index++) {
			size_t os_thread_id = OS::GetThreadID(m_thread_handles[index]);
			if (os_thread_id != running_thread_id) {
				// We don't want to suspend ourselves in case the crashing thread produces the crash report
				bool success = OS::SuspendThread(m_thread_handles[index]);
				managed_to_suspend_threads &= success;
			}
		}

		return managed_to_suspend_threads;
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
			m_sleep_wait[thread_id].value.ResetAndNotify();
		}
		else {
			m_sleep_wait[thread_id].value.Notify();
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
		flag->Unlock();
	}

	void TaskManager::WaitThread(unsigned int thread_id) {
		SpinLock* lock = (SpinLock*)m_dynamic_task_allocators[thread_id].value.Allocate(sizeof(SpinLock));
		lock->Lock();

		AddDynamicTaskWithAffinity(ECS_THREAD_TASK_NAME(WaitThreadTask, lock, 0), thread_id, false);
		lock->WaitLocked();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(WaitThreadsTask) {
		Semaphore* semaphore = (Semaphore*)_data;
		semaphore->Enter();
	}

	void TaskManager::WaitThreads()
	{
		unsigned int thread_count = GetThreadCount();

		const size_t STACK_CAPACITY = ECS_KB;

		ECS_STACK_CAPACITY_STREAM(unsigned int, awaken_threads, STACK_CAPACITY);
		// Record the awake threads - such that we do not push a task to a sleeping thread and wake it up
		// Since this is unnecessary
		for (unsigned int index = 0; index < thread_count; index++) {
			if (m_sleep_wait[index].value.WaitingThreadCount() == 0) {
				awaken_threads.AddAssert(index);
			}
		}
		if (awaken_threads.size == 0) {
			// All of them are sleeping - can return
			return;
		}

		Semaphore semaphore;
		semaphore.ClearCount();
		for (unsigned int index = 0; index < awaken_threads.size; index++) {
			// We must use add task and wake since the thread could have gone sleeping since
			// We last checked up until now
			AddDynamicTaskAndWakeWithAffinity(ECS_THREAD_TASK_NAME(WaitThreadsTask, &semaphore, 0), awaken_threads[index], false);
		}
		semaphore.SpinWait(awaken_threads.size);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	static int ThreadExceptionFilter(TaskManager* task_manager, unsigned int thread_id, EXCEPTION_POINTERS* exception) {
		TaskManagerExceptionHandlerData handler_data;
		handler_data.exception_information = OS::GetExceptionInformationFromNative(exception);
		handler_data.thread_id = thread_id;

		// The unhandled case means that we should crash in case it it a critical exception
		OS::ECS_OS_EXCEPTION_CONTINUE_STATUS continue_status = OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
		// Lock this such that new handlers cannot be added or removed
		task_manager->m_exception_handler_lock.Lock();
		for (int64_t index = (int64_t)task_manager->m_exception_handlers.size - 1; index >= 0; index--) {
			TaskManager::ExceptionHandler& current_handler = task_manager->m_exception_handlers[index];
			handler_data.user_data = current_handler.is_data_ptr ? current_handler.data_ptr : current_handler.data;
			continue_status = current_handler.function(&handler_data);
			if (continue_status != OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED) {
				break;
			}
		}
		task_manager->m_exception_handler_lock.Unlock();

		if (continue_status == OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED) {
			if (OS::IsExceptionCodeCritical(handler_data.exception_information.error_code)) {
				ECS_FORMAT_TEMP_STRING(error_message, "Thread {#} has hard crashed with an error of type ", thread_id);
				OS::ExceptionCodeToString(handler_data.exception_information.error_code, error_message);
				Crash(error_message);
				// We need to execute the crash block
				return GetExceptionNativeContinueCode(OS::ECS_OS_EXCEPTION_CONTINUE_RESOLVED);
			}
		}
		// We can reach this also from the unhandled case, and for both we want to continue the SEH search
		// Only for the resolved we want to continue the execution
		return GetExceptionNativeContinueCode(continue_status);
	}

	void ThreadProcedure(
		TaskManager* task_manager,
		unsigned int thread_id
	) {
		// Store the recovery point first
		if (setjmp(task_manager->m_threads_reset_point[thread_id]) == RECOVERY_POINT_LONG_JUMP_SLEEP_VALUE) {
			task_manager->SleepThread(thread_id);
		}

		// Keeping the pointer to its own dynamic task queue in order to avoid reloading the pointer
		// when calling the method
		ThreadQueue* thread_queue = task_manager->GetThreadQueue(thread_id);
		__try {
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
						if (HasFlag(task_manager->m_wait_type, ECS_TASK_MANAGER_WAIT_SLEEP)) {
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
					if (HasFlag(task_manager->m_wait_type, ECS_TASK_MANAGER_WAIT_STEAL)) {
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
		__except (ThreadExceptionFilter(task_manager, thread_id, GetExceptionInformation())) {
			task_manager->ResetThreadToProcedure(thread_id, true);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

}