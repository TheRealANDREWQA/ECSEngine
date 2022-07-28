#pragma once
#include "ecspch.h"
#include "../../Core.h"
#include "../../Utilities/Timer.h"
#include "../../Containers/Stream.h"
#include "../../Containers/Queues.h"
#include "TaskAllocator.h"
#include "ThreadTask.h"
#include "../../Allocators/MemoryManager.h"

#ifndef ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
#define ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD 64
#endif

#ifndef ECS_TASK_MANAGER_ALLOCATOR_CAPACITY
#define ECS_TASK_MANAGER_ALLOCATOR_CAPACITY 10'000
#endif

#ifndef ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE
#define ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE 1'000
#endif

#ifndef ECS_TASK_MANAGER_THREAD_MEMORY_MANAGER_SIZE
#define ECS_TASK_MANAGER_THREAD_MEMORY_MANAGER_SIZE 1'000'000
#endif

#define ECS_END_THREAD -100000

#define ECS_SET_TASK_FUNCTION(task, _function) task.function = _function; task.name = STRING(_function);

namespace ECSEngine {

	struct LinearAllocator;
	struct ConditionVariable;

	using ThreadQueue = ThreadSafeQueue<ThreadTask>;

	// If partitioning a stream is desired with more explicit names that a uint2
	struct ThreadPartition {
		unsigned int offset;
		unsigned int size;
	};

	enum ECS_THREAD_PRIORITY : unsigned char {
		ECS_THREAD_PRIORITY_VERY_LOW,
		ECS_THREAD_PRIORITY_LOW,
		ECS_THREAD_PRIORITY_NORMAL,
		ECS_THREAD_PRIORITY_HIGH,
		ECS_THREAD_PRIORITY_VERY_HIGH
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

	struct ECSENGINE_API TaskManager
	{
	public:
		TaskManager(
			size_t thread_count, 
			GlobalMemoryManager* memory_manager,
			size_t thread_linear_allocator_size = ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE,
			size_t thread_memory_manager_size = ECS_TASK_MANAGER_THREAD_MEMORY_MANAGER_SIZE,
			size_t max_dynamic_tasks = ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
		);

		TaskManager(const TaskManager& other) = default;
		TaskManager& operator = (const TaskManager& other) = default;

		void AddTask(ThreadTask task);

		// it will add that task to the next thread after the last one used and returns the index of the 
		// thread that is executing the task
		unsigned int AddDynamicTask(ThreadTask task);

		// it will add that task to the next thread after the last one used and returns the index of the
		// thread that is executing the task
		unsigned int AddDynamicTaskAndWake(ThreadTask task);

		// does not affect last thread id used
		void AddDynamicTaskWithAffinity(ThreadTask task, unsigned int thread_id);

		// does not affect last thread id used
		void AddDynamicTaskAndWakeWithAffinity(ThreadTask task, unsigned int thread_id);

		void* AllocateTempBuffer(unsigned int thread_id, size_t size, size_t alignment = 8);

		void* AllocatePersistentBuffer(unsigned int thread_id, size_t size, size_t alignment = 8);

		void ChangeStaticWrapperMode(ThreadFunctionWrapper custom_function, void* wrapper_data = nullptr, size_t wrapper_data_size = 0);

		void ChangeDynamicWrapperMode(ThreadFunctionWrapper custom_function, void* wrapper_data = nullptr, size_t wrapper_data_size = 0);

		void ClearTaskStream();

		void ClearTaskIndices();

		void ClearTaskIndex(unsigned int thread_id);

		void CreateThreads();

		// It will call ExitThread to kill the thread. It does so by adding a dynamic task and waking the thread
		// This helps eliminate dangerous state changes that are not fully commited, like writing files, modifying global state,
		// locks or allocations
		void DestroyThreads();

		void DecrementThreadTaskIndex(unsigned int thread_id);

		void DeallocatePersistentBuffer(unsigned int thread_id, const void* data);

		void IncrementThreadTaskIndex(unsigned int thread_id);

		unsigned int GetTaskCount() const;

		unsigned int GetThreadCount() const;

		ThreadQueue* GetThreadQueue(unsigned int thread_id) const;

		ThreadTask GetTask(unsigned int task_index) const;

		ThreadTask* GetTaskPtr(unsigned int task_index) const;

		int GetThreadTaskIndex(unsigned int thread_index) const;

		AllocatorPolymorphic GetThreadTempAllocator(unsigned int thread_id) const;

		AllocatorPolymorphic GetThreadPersistentAllocator(unsigned int thread_id) const;

		void ReserveTasks(unsigned int count);

		void ResetDynamicQueues();

		void ResetDynamicQueue(unsigned int thread_id);

		// Sets a specific task, should only be used in initialization
		void SetTask(ThreadTask task, unsigned int index, size_t task_data_size = 0);

		void SetWorld(World* world);

		void SetThreadTaskIndex(unsigned int thread_index, int value);

		void SleepThread(unsigned int thread_id);

		// Max period is expressed as the amount of milliseconds that the thread waits at most
		void SleepUntilDynamicTasksFinish(size_t max_period = ULLONG_MAX);

		// Elevates or reduces the priority of the threads
		void SetThreadPriorities(ECS_THREAD_PRIORITY priority);

		void TerminateThread(unsigned int thread_id);

		void TerminateThreads();

		void WakeThread(unsigned int thread_id);

		void WakeThreads();

		// Waits for the thread with thread_index to get to the task index
		// It uses a spin wait
		void WaitThread(int task_index, unsigned int thread_index);

		// Wait for all threads to get to this task index
		// It uses a spin wait. The thread that calls this function
		// must be on this task index
		void WaitAllThreads(int task_index);

	//private:
		World* m_world;
		void** m_thread_handles;

		Stream<ThreadQueue*> m_thread_queue;
		ResizableStream<ThreadTask> m_tasks;
		std::atomic<int>** m_thread_task_index;

		ConditionVariable* m_sleep_wait;

		unsigned int m_last_thread_index;
		// These are pointer to pointers in order to have them on separated cache lines
		// to avoid false sharing
		LinearAllocator** m_thread_linear_allocators;
		MemoryManager** m_thread_memory_managers;


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

