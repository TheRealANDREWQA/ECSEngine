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

	ECS_CONTAINERS;
	using ThreadQueue = ThreadSafeQueue<ThreadTask>;

	enum class TaskManagerWrapper : unsigned char {
		None,
		CountTasks,
		Custom
	};

	struct ECSENGINE_API TaskManager
	{
	public:
		TaskManager(
			size_t thread_count, 
			MemoryManager* memory_manager,
			size_t thread_linear_allocator_size = ECS_TASK_MANAGER_THREAD_LINEAR_ALLOCATOR_SIZE,
			size_t thread_memory_manager_size = ECS_TASK_MANAGER_THREAD_MEMORY_MANAGER_SIZE,
			size_t max_dynamic_tasks = ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
		);

		TaskManager(const TaskManager& other) = default;
		TaskManager& operator = (const TaskManager& other) = default;

		void AddTask(ThreadTask task, size_t task_data_size = 0);

		// it will add that task to the next thread after the last one used and returns the index of the 
		// thread that is executing the task
		unsigned int AddDynamicTask(ThreadTask task, size_t task_data_size = 0);

		// it will add that task to the next thread after the last one used and returns the index of the
		// thread that is executing the task
		unsigned int AddDynamicTaskAndWake(ThreadTask task, size_t task_data_size = 0);

		// does not affect last thread id used
		void AddDynamicTaskWithAffinity(ThreadTask task, unsigned int thread_id, size_t task_data_size = 0);

		// does not affect last thread id used
		void AddDynamicTaskAndWakeWithAffinity(ThreadTask task, unsigned int thread_id, size_t task_data_size = 0);

		void ChangeStaticWrapperMode(TaskManagerWrapper wrapper_mode, void* wrapper_data = nullptr, size_t wrapper_data_size = 0, ThreadFunctionWrapper custom_function = nullptr);

		void ChangeDynamicWrapperMode(TaskManagerWrapper wrapper_mode, void* wrapper_data = nullptr, size_t wrapper_data_size = 0, ThreadFunctionWrapper custom_function = nullptr);

		void ClearTaskStream();

		void ClearTaskIndices();

		void ClearTaskIndex(unsigned int thread_id);

		void CreateThreads();

		void DecrementThreadTaskIndex(unsigned int thread_id);

		void IncrementThreadTaskIndex(unsigned int thread_id);

		Timer* GetTimer();

		unsigned int GetTaskCount() const;

		unsigned int GetThreadCount() const;

		ThreadQueue* GetThreadQueue(unsigned int thread_id) const;

		ThreadTask GetTask(unsigned int task_index) const;

		ThreadTask* GetTaskPtr(unsigned int task_index) const;

		int GetThreadTaskIndex(unsigned int thread_index) const;

		void ReserveTasks(unsigned int count);

		void ResetDynamicQueues();

		void ResetDynamicQueue(unsigned int thread_id);

		void ResetTaskAllocator();

		// Sets a specific task, should only be used in initialization
		void SetTask(ThreadTask task, unsigned int index, size_t task_data_size = 0);

		void SetWorld(World* world);

		void SetTimerMarker();

		void SetThreadTaskIndex(unsigned int thread_index, int value);

		void SleepThread(unsigned int thread_id);

		// Max period is expressed as the amount of milliseconds that the thread waits at most
		void SleepUntilDynamicTasksFinish(size_t max_period = ULLONG_MAX);

		void TerminateThread(unsigned int thread_id);

		void TerminateThreads();

		void WakeThread(unsigned int thread_id);

		void WakeThreads();

	//private:
		World* m_world;
		Stream<ThreadQueue*> m_thread_queue;
		ResizableStream<ThreadTask, MemoryManager> m_tasks;
		std::atomic<int>** m_thread_task_index;
		ConditionVariable* m_events;
		Timer m_timer;
		TaskAllocator m_allocator;
		unsigned int m_last_thread_index;
		LinearAllocator** m_thread_linear_allocators;
		MemoryManager** m_thread_memory_managers;
#ifdef ECS_TASK_MANAGER_WRAPPER
		TaskManagerWrapper m_static_wrapper_mode;
		ThreadFunctionWrapper m_static_wrapper;
		void* m_static_wrapper_data;
		size_t m_static_wrapper_data_size;
		TaskManagerWrapper m_dynamic_wrapper_mode;
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

