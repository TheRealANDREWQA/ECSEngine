#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Utilities/Timer.h"
#include "../Containers/Stream.h"
#include "../Containers/Queues.h"
#include "ThreadTask.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/LinearAllocator.h"
#include "RingBuffer.h"
#include "../Utilities/OSFunctions.h"

#ifndef ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
#define ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD 64
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
		ECS_TASK_MANAGER_WAIT_SLEEP  = 0,
		ECS_TASK_MANAGER_WAIT_SPIN   = 1,
		/*
			This flag can be combined with the other two
			When the thread finishes its queue, it will try
			to steal a task and if there is nothing to be stolen,
			then it will go to the wait type specified
		*/
		ECS_TASK_MANAGER_WAIT_STEAL  = 2 
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

		void AddTask(ThreadTask task);

		void AddTasks(Stream<ThreadTask> tasks);

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

		void* AllocateTempBuffer(unsigned int thread_id, size_t size, size_t alignment = 8);

		void ChangeStaticWrapperMode(ThreadFunctionWrapper custom_function, void* wrapper_data = nullptr, size_t wrapper_data_size = 0);

		void ChangeDynamicWrapperMode(ThreadFunctionWrapper custom_function, void* wrapper_data = nullptr, size_t wrapper_data_size = 0);

		void ClearThreadAllocators();

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

		void IncrementThreadTaskIndex();

		unsigned int GetTaskCount() const;

		unsigned int GetThreadCount() const;

		ThreadQueue* GetThreadQueue(unsigned int thread_id) const;

		ThreadTask GetTask(unsigned int task_index) const;

		ThreadTask* GetTaskPtr(unsigned int task_index);

		unsigned int GetThreadTaskIndex() const;

		AllocatorPolymorphic GetThreadTempAllocator(unsigned int thread_id) const;

		// Returns true if the thread is sleeping/spinning (ran out of tasks to perform) else false
		bool IsSleeping(unsigned int thread_id) const;

		void ReserveTasks(unsigned int count);

		// Resets all the temporary allocators, frees the static tasks and puts the threads to sleep
		// If they are not already sleeping
		void Reset();

		void ResetStaticTasks();

		void ResetDynamicQueues();

		void ResetDynamicQueue(unsigned int thread_id);

		// Sets a specific task, should only be used in initialization
		void SetTask(ThreadTask task, unsigned int index, size_t task_data_size = 0);

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

		// Spin waits until someone pushes something into the thread's queue
		void SpinThread(unsigned int thread_id);

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

		Stream<ThreadQueue*> m_thread_queue;

		struct StaticThreadTask {
			ThreadTask task;
			SpinLock initialize_lock;
		};

		ResizableStream<StaticThreadTask> m_tasks;
		std::atomic<unsigned int> m_thread_task_index;
		// Used by the dynamic add to know where to insert the new task
		unsigned int m_last_thread_index;
		
		ECS_TASK_MANAGER_WAIT_TYPE m_wait_type;

		// When all the static tasks and their spawned dynamic tasks are finished, it will set this variable to 0
		ConditionVariable m_is_frame_done;

		ConditionVariable* m_sleep_wait;

		// These are pointer to pointers in order to have them on separated cache lines
		// to avoid false sharing
		LinearAllocator** m_thread_linear_allocators;

		// From this allocator the data for the tasks and the name of the task
		// are allocated. It is used like a ring buffer		
		RingBuffer** m_dynamic_task_allocators;

		// data for tasks + names for static tasks
		LinearAllocator m_static_task_data_allocator;


#ifdef ECS_TASK_MANAGER_WRAPPER
		ThreadFunctionWrapper m_static_wrapper;
		void* m_static_wrapper_data;
		size_t m_static_wrapper_data_size;
		ThreadFunctionWrapper m_dynamic_wrapper;
		void* m_dynamic_wrapper_data;
		size_t m_dynamic_wrapper_data_size;
#endif
	};

	 ECSENGINE_API void ThreadProcedure(
		TaskManager* task_manager,
		unsigned int thread_id
	);

}

