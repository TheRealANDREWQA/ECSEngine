#pragma once
#include "../Core.h"
#include "../Utilities/Timer.h"
#include "../Containers/Stream.h"
#include "../Containers/Queues.h"
#include "ThreadTask.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/LinearAllocator.h"
#include "RingBuffer.h"
#include "../OS/ExceptionHandling.h"
#include <setjmpex.h>
#include "../Utilities/BasicTypes.h"

#ifndef ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
#define ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD 128
#endif

#ifndef ECS_TASK_MANAGER_ALLOCATOR_CAPACITY
#define ECS_TASK_MANAGER_ALLOCATOR_CAPACITY 10'000
#endif

#ifndef ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE
#define ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE 10'000
#endif

#define ECS_SET_TASK_FUNCTION(task, _function) task.function = _function; task.name = STRING(_function);

namespace ECSEngine {

	struct ConditionVariable;

	struct DynamicThreadTask {
		ThreadTask task;
		bool can_be_stolen;
	};

	typedef ThreadSafeQueue<DynamicThreadTask> ThreadQueue;

	// This describes how the thread should behave when their dynamic queue is finished
	enum ECS_TASK_MANAGER_WAIT_TYPE : unsigned char {
		ECS_TASK_MANAGER_WAIT_SLEEP  = 1,
		ECS_TASK_MANAGER_WAIT_SPIN   = 2,
		/*
			This flag can be combined with the other two
			When the thread finishes its queue, it will try
			to steal a task and if there is nothing to be stolen,
			then it will go to the wait type specified
		*/
		ECS_TASK_MANAGER_WAIT_STEAL  = 4 
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_TASK_MANAGER_WAIT_TYPE);

	// If partitioning a stream is desired with more explicit names that a uint2
	struct ThreadPartition {
		unsigned int offset;
		unsigned int size;
	};

	// Will distribute the task evenly across the threads taking into account the remainder
	// The indices stream must have as its size the number of threads that want to be scheduled
	// It returns the number of threads that need to be launched
	ECSENGINE_API unsigned int ThreadPartitionStream(Stream<ThreadPartition> indices, unsigned int task_count);

	// Obviously, it doesn't require any data when setting this wrapper
	ECSENGINE_API ECS_THREAD_WRAPPER_TASK(ThreadWrapperNone);

	struct ThreadWrapperCountTasksData {
		std::atomic<unsigned int> count = 0;
	};
	
	// It requires a ThreadWrapperCountTasksData to be allocated
	ECSENGINE_API ECS_THREAD_WRAPPER_TASK(ThreadWrapperCountTasks);

	struct StaticThreadTask {
		ECS_INLINE StaticThreadTask() {}
		ECS_INLINE StaticThreadTask(ThreadTask _task, bool _barrier_task) : task(_task), barrier_task(_barrier_task) {}

		ThreadTask task;
		bool barrier_task;
	};

	struct TaskManagerExceptionHandlerData {
		OS::ExceptionInformation exception_information;
		unsigned int thread_id;
		void* user_data;
	};

	// This handler will be called to determine if the exception is to be handled
	// In case it is unhandled, any necessary pre crash preparation could be done inside it
	typedef OS::ECS_OS_EXCEPTION_CONTINUE_STATUS (*TaskManagerExceptionHandler)(TaskManagerExceptionHandlerData* function_data);

	// The static threads are executed by a single thread
	// The purpose of the static tasks is to dynamically detect how the data
	// should be partitioned and then push the dynamic tasks accordingly
	// The task manager can be used to steal tasks from other queues
	// if automatic task stealing is desired
	struct ECSENGINE_API TaskManager
	{
	public:
		TaskManager(
			size_t thread_count, 
			GlobalMemoryManager* memory_manager,
			size_t thread_linear_allocator_size = 0, // The default for this is ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE
			size_t max_dynamic_tasks = 0 // The default for this is ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
		);

		TaskManager(const TaskManager& other) = default;
		TaskManager& operator = (const TaskManager& other) = default;

		void AddTask(StaticThreadTask task);

		void AddTasks(Stream<StaticThreadTask> tasks);

		// it will add that task to the next thread after the last one used and returns the index of the 
		// thread that is executing the task
		unsigned int AddDynamicTask(ThreadTask task, bool can_be_stolen = true);

		// it will add that task to the next thread after the last one used and returns the index of the
		// thread that is executing the task
		unsigned int AddDynamicTaskAndWake(ThreadTask task, bool can_be_stolen = true);

		// does not affect last thread id used
		void AddDynamicTaskWithAffinity(ThreadTask task, unsigned int thread_id, bool can_be_stolen = true);

		// does not affect last thread id used
		void AddDynamicTaskAndWakeWithAffinity(ThreadTask task, unsigned int thread_id, bool can_be_stolen = true);

		// Distributes the tasks equally among the threads
		// The can be stolen flag will affect all thread functions
		void AddDynamicTaskGroup(ThreadFunction function, const char* function_name, Stream<void*> data, size_t data_size, bool can_be_stolen = true);

		// Distributes the tasks equally among the threads
		// The can be stolen flag will affect all thread functions
		void AddDynamicTaskGroup(ThreadFunction function, const char* function_name, void* data, unsigned int task_count, size_t data_size, bool can_be_stolen = true);

		// Distributes the tasks equally among the threads
		// The can be stolen flag will affect all thread functions
		// Wakes the threads after pushing the tasks
		void AddDynamicTaskGroupAndWake(ThreadFunction function, const char* function_name, Stream<void*> data, size_t data_size, bool can_be_stolen = true);

		// Distributes the tasks equally among the threads
		// The can be stolen flag will affect all thread functions
		// Wakes the threads after pushing the tasks
		void AddDynamicTaskGroupAndWake(ThreadFunction function, const char* function_name, void* data, unsigned int task_count, size_t data_size, bool can_be_stolen = true);

		// Distributes the tasks equally among the threads
		// The can be stolen flag will affect all thread tasks
		// The functor will be called for each iteration such that you can modify
		// The data pointer. The functor receives the index as parameter (size_t)
		template<typename Functor>
		void AddDynamicTaskGroup(
			ThreadFunction function, 
			const char* function_name, 
			size_t task_count, 
			void* data,
			size_t data_size, 
			bool can_be_stolen, 
			Functor&& functor
		) {
			size_t thread_count = GetThreadCount();
			// Division and modulo are one instruction on x86
			size_t per_thread_count = task_count / thread_count;
			size_t remainder = task_count % thread_count;

			size_t last_thread_index = (size_t)m_last_thread_index->fetch_add(remainder, ECS_RELAXED);
			last_thread_index = last_thread_index % thread_count;
			size_t total_index = 0;

			ThreadTask thread_task = { function, data, data_size, function_name };
			for (size_t index = 0; index < per_thread_count; index++) {
				for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
					functor(total_index);
					total_index++;
					AddDynamicTaskWithAffinity(thread_task, thread_id, can_be_stolen);
				}
			}

			for (size_t index = 0; index < remainder; index++) {
				functor(total_index);
				total_index++;
				AddDynamicTaskWithAffinity(thread_task, last_thread_index, can_be_stolen);

				last_thread_index = last_thread_index == thread_count - 1 ? 0 : last_thread_index + 1;
			}
		}

		// Distributes the tasks equally among the threads
		// The can be stolen flag will affect all thread tasks
		// The functor will be called for each iteration such that you can modify
		// The data pointer. The functor receives the index as parameter and the count
		// Of entries to be processed (both are size_t)
		template<typename Functor>
		void AddDynamicTaskParallelFor(
			ThreadFunction function,
			const char* function_name,
			size_t entry_count,
			size_t batch_count,
			void* data,
			size_t data_size,
			bool can_be_stolen,
			Functor&& functor
		) {
			size_t task_count = entry_count / batch_count;
			size_t task_remainder = entry_count % batch_count;
			bool has_remainder = task_remainder != 0;

			AddDynamicTaskGroup(function, function_name, task_count + has_remainder, data, data_size, can_be_stolen, [&](size_t index) {
				if (index == task_count) {
					functor(index, task_remainder);
				}
				else {
					functor(index, batch_count);
				}
			});
		}

		void* AllocateTempBuffer(unsigned int thread_id, size_t size, size_t alignment = alignof(void*));

		AllocatorPolymorphic AllocatorTemp(unsigned int thread_id) const;

		void ChangeStaticWrapperMode(ThreadFunctionWrapperData wrapper_data);

		void ChangeDynamicWrapperMode(ThreadFunctionWrapperData wrapper_data);

		// It will use a compose wrapper that will in turn call both sub wrappers to run
		void ComposeStaticWrapper(ThreadFunctionWrapperData addition_wrapper, bool run_after_existing_one = true);

		// It will use a compose wrapper that will in turn call both sub wrappers to run
		void ComposeDynamicWrapper(ThreadFunctionWrapperData addition_wrapper, bool run_after_existing_one = true);

		// It will compose wrappers for both static and dynamic tasks that will monitor how long each task takes
		void ComposeFrameTimingWrappers();

		void ClearThreadAllocators();

		void ClearThreadAllocator(unsigned int thread_id);

		void ClearTemporaryAllocators();

		void ClearTaskStream();

		void ClearTaskIndex();

		void CreateThreads();

		// Clears any temporary resources and returns the static task index 
		// to the beginning of the stream
		void ClearFrame();

		// It will call ExitThread to kill the thread. It does so by adding a dynamic task and waking the thread
		// This helps eliminate dangerous state changes that are not fully commited, like writing files, modifying global state,
		// locks or allocations
		void DestroyThreads();

		void DecrementThreadTaskIndex();

		// Can choose whether or not to block for the frame to finish
		void DoFrame(bool wait_frame = true);

		// Invokes the wrapper first. It will return true if it had a static task to run,
		// else it returns false
		bool ExecuteStaticTask(unsigned int thread_id);

		// Invokes the wrapper first. There are 2 indices because the task can be stolen. One
		// index indicates the current thread that will execute the dynamic task, the other index
		// will indicate from which queue the task was allocated
		void ExecuteDynamicTask(ThreadTask task, unsigned int thread_id, unsigned int task_thread_id);

		// Inserts a guard task at the end which will anounce the main thread
		// when all the tasks have been finished
		void FinishStaticTasks();

		// Returns -1 if it doesn't find the task
		unsigned int FindTask(Stream<char> task_name) const;

		// Returns the thread id (the index) given the OS thread id. Returns -1 if it doesn't find it
		unsigned int FindThreadID(size_t os_thread_id) const;

		ECS_INLINE void IncrementThreadTaskIndex() {
			m_thread_task_index->fetch_add(1, ECS_ACQ_REL);
		}

		ECS_INLINE unsigned int GetTaskCount() const {
			return m_tasks.size;
		}

		ECS_INLINE unsigned int GetThreadCount() const {
			return m_thread_queue.size;
		}

		ECS_INLINE ThreadQueue* GetThreadQueue(unsigned int thread_id) {
			return &m_thread_queue[thread_id].value;
		}

		ECS_INLINE const ThreadQueue* GetThreadQueue(unsigned int thread_id) const {
			return &m_thread_queue[thread_id].value;
		}

		ECS_INLINE ThreadTask GetTask(unsigned int task_index) const {
			ECS_ASSERT(task_index >= 0 && task_index < m_tasks.capacity);
			return m_tasks[task_index].task;
		}

		ECS_INLINE ThreadTask* GetTaskPtr(unsigned int task_index) {
			ECS_ASSERT(task_index >= 0 && task_index < m_tasks.capacity);
			return &m_tasks[task_index].task;
		}

		ECS_INLINE unsigned int GetThreadTaskIndex() const {
			return m_thread_task_index->load(ECS_ACQUIRE);
		}

		ECS_INLINE AllocatorPolymorphic GetThreadTempAllocator(unsigned int thread_id) {
			return &m_thread_linear_allocators[thread_id].value;
		}

		// Returns the current static thread wrapper. If the data pointer is specified, it will copy the thread
		// wrapper data into it (it must have a byte size of at least 64 bytes)
		ThreadFunctionWrapperData GetStaticThreadWrapper(CapacityStream<void>* data = nullptr) const;

		// Returns the current dynamic thread wrapper. If the data pointer is specified, it will copy the thread
		// wrapper data into it (it must have a byte size of at least 64 bytes)
		ThreadFunctionWrapperData GetDynamicThreadWrapper(CapacityStream<void>* data = nullptr) const;

		// Returns true if the thread is sleeping/spinning (ran out of tasks to perform) else false
		bool IsSleeping(unsigned int thread_id) const;

		void PopExceptionHandler();

		// This can be called from multiple threads
		void PopExceptionHandlerThreadSafe();

		void PushExceptionHandler(TaskManagerExceptionHandler handler, void* data, size_t data_size);

		// This can be called from multiple threads
		void PushExceptionHandlerThreadSafe(TaskManagerExceptionHandler handler, void* data, size_t data_size);

		// Removes (not with swap back) a static task
		void RemoveTask(unsigned int task_index);

		void ReserveTasks(unsigned int count);

		// Resets all the temporary allocators, frees the static tasks and puts the threads to sleep
		// If they are not already sleeping
		void Reset();

		void ResetStaticTasks();

		void ResetDynamicQueues();

		void ResetDynamicQueue(unsigned int thread_id);

		// Returns true if it managed to resume all threads, else false
		// You can call this function even if you are a thread belonging to this task manager
		// It will skip resuming in that case
		bool ResumeThreads() const;

		// Longjumps into the start of the thread procedure. You can choose to enter a sleep state after
		// Going back to the recovery point or continuing immediately
		void ResetThreadToProcedure(unsigned int thread_id, bool sleep) const;

		// It will deduce the thread id of the running thread and jump to the thread procedure
		// You can choose to abort if the thread id is not found or just skip the call
		void ResetCurrentThreadToProcedure(bool sleep, bool abort_if_not_found = true) const;

		// It will remove the last composed wrapper that was added, but you need
		// To specify which of the wrappers to keep
		void RemoveComposedDynamicWrapper(bool keep_first_wrapper);

		// It will remove the last composed wrapper that was added, but you need
		// To specify which of the wrappers to keep
		void RemoveComposedStaticWrapper(bool keep_first_wrapper);

		// Sets a specific task, should only be used in initialization
		void SetTask(StaticThreadTask task, unsigned int index, size_t task_data_size = 0);

		void SetWorld(World* world);

		void SetThreadTaskIndex(int value);

		void SleepThread(unsigned int thread_id);

		// This function puts all threads into sleep even when the wait type is set to
		// spin wait. You can choose to sleep until all threads have gone to sleep
		// or just push the dynamic tasks
		void SleepThreads(bool wait_until_all_sleep = true);

		// Max period is expressed as the amount of milliseconds that the thread waits at most
		void SleepUntilDynamicTasksFinish(size_t max_period = ULLONG_MAX);
		
		// Changes the way the threads wait for more tasks
		void SetWaitType(ECS_TASK_MANAGER_WAIT_TYPE wait_type);

		// It will go through the queues of the other threads and try to steal a task.
		// If there is no task to be stolen, it will return a task without a function pointer.
		// It also update the thread_id to reflect the index of the thread from which this task
		// came from
		ThreadTask StealTask(unsigned int& thread_id);

		// Elevates or reduces the priority of the threads
		void SetThreadPriorities(OS::ECS_THREAD_PRIORITY priority);

		// Adds wrappers for both dynamic tasks and static tasks such that after a crash
		// They won't execute those tasks
		void SetThreadWorldCrashWrappers();
		
		// It will go through all static tasks and "nullify" the barrier
		// Basically, it will set the thread target count to be the one
		// Necessary for the barrier to let the thread escape from it
		// (useful for example to make a thread stuck in the wait escape
		// during a crash)
		void SignalStaticTasks();

		// Spin waits until someone pushes something into the thread's queue
		void SpinThread(unsigned int thread_id);

		// Returns true if it managed to suspend all threads, else false
		// You can call this function even if you are a thread belonging to this task manager
		// It will skip suspending itself in that case
		bool SuspendThreads() const;

		void TerminateThread(unsigned int thread_id);

		void TerminateThreads(bool wait_for_termination = false);

		// If the waiting mode is set to sleeping and force wake is set to true
		// then it will wake the thread no matter how many times sleep thread has been called
		// (normally a matching number of times wake thread would have to be called)
		void WakeThread(unsigned int thread_id, bool force_wake = false);

		// If the waiting mode is set to sleeping and force wake is set to true
		// then it will wake the thread no matter how many times sleep thread has been called
		// (normally a matching number of times wake thread would have to be called)
		void WakeThreads(bool force_wake = false);

		// It waits for the thread with the given index to finish its thread queue
		// and any task stealing that it might do. It inserts a dynamic task that increments
		// an index when it before it goes to its wait state. The calling thread_id is needed
		// in order to allocate the flag
		void WaitThread(unsigned int thread_id);

		// Wait for all threads to finish their thread queues.
		// The thread id is used to make a temp allocation
		void WaitThreads();

	//private:
		World* m_world;
		void** m_thread_handles;

		Stream<CacheAligned<ThreadQueue>> m_thread_queue;

		// Make this structure occupy a cache line to avoid any false sharing possibility
		// There are not a whole lot of padding bytes added, so this is probably worth it
		struct StaticTask {
			ThreadTask task;
			Semaphore barrier;
			// The lock is used to acquire exclude access to the task to be executed
			SpinLock single_thread_lock;
			// The atomic flag is used indicate to the barrier threads when the main
			// Single thread part finished executing
			SpinLock single_thread_finished;
		private:
			char padding[ECS_CACHE_LINE_SIZE - sizeof(task) - sizeof(barrier) - sizeof(single_thread_lock) - sizeof(single_thread_finished)];
		};

		ResizableStream<StaticTask> m_tasks;
		// Allocated on a separate cache line in order to not affect other
		// fields when modifying this
		std::atomic<unsigned int>* m_thread_task_index;

		// Allocated on a separate cache line in order to not affect other
		// fields when modidying this
		// Used by the dynamic add to know where to insert the new task
		std::atomic<unsigned int>* m_last_thread_index;
		
		ECS_TASK_MANAGER_WAIT_TYPE m_wait_type;

		// When all the static tasks and their spawned dynamic tasks are finished, it will set this variable to 0
		ConditionVariable m_is_frame_done;

		CacheAligned<ConditionVariable>* m_sleep_wait;

		// These are pointer to pointers in order to have them on separated cache lines
		// to avoid false sharing
		CacheAligned<LinearAllocator>* m_thread_linear_allocators;

		// From this allocator the data for the tasks and the name of the task
		// are allocated. It is used like a ring buffer		
		CacheAligned<RingBuffer>* m_dynamic_task_allocators;

		// data for tasks + names for static tasks
		LinearAllocator m_static_task_data_allocator;

		// These will be filled with a stack environment of the thread procedure
		// Such that you can longjmp into the thread procedure
		jmp_buf* m_threads_reset_point;

		struct ExceptionHandler {
			TaskManagerExceptionHandler function;
			// Embed the data directly here to avoid another allocation
			union {
				size_t data[32];
				void* data_ptr;
			};
			bool is_data_ptr;
		};

		// This works like a stack. The last added handler has priority above all the others
		// And can handle the exception before those
		ResizableStream<ExceptionHandler> m_exception_handlers;
		SpinLock m_exception_handler_lock;

#ifdef ECS_TASK_MANAGER_WRAPPER
		// Embed the data directly here
		size_t m_static_wrapper_data_storage[32];
		size_t m_dynamic_wrapper_data_storage[32];
		ThreadFunctionWrapperData m_static_wrapper;
		ThreadFunctionWrapperData m_dynamic_wrapper;
#endif
	};

	 ECSENGINE_API void ThreadProcedure(
		TaskManager* task_manager,
		unsigned int thread_id
	);

}

