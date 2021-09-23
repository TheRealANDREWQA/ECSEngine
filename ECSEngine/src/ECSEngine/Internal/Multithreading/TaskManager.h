#pragma once
#include "ecspch.h"
#include "../../Core.h"
#include "../../Utilities/Timer.h"
#include "../../Containers/Stream.h"
#include "../../Containers/Queues.h"
#include "../../Containers/StaticLockFreeStream.h"
#include "TaskAllocator.h"
#include "ThreadTask.h"

#ifndef ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
#define ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD 64
#endif

#ifndef ECS_TASK_MANAGER_ALLOCATOR_CAPACITY
#define ECS_TASK_MANAGER_ALLOCATOR_CAPACITY 10'000
#endif

#define ECS_END_THREAD -100000

#define ECS_SET_TASK_FUNCTION(task, _function) task.function = _function; task.name = STRING(_function);

namespace ECSEngine {

	struct LinearAllocator;
	struct MemoryManager;
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
			size_t max_dynamic_tasks = ECS_MAXIMUM_TASK_MANAGER_TASKS_PER_THREAD
		);

		static void ThreadProcedure(
			TaskManager* task_manager, 
			LinearAllocator* temp_thread_allocator, 
			MemoryManager* thread_allocator, 
			unsigned int thread_id
		);

		void AddTask(ThreadTask task, size_t task_data_size = 0);

		// it will add that task to the next thread after the last one used and returns the index of the 
		// thread that is executing the task
		unsigned int AddDynamicTask(ThreadTask task, size_t task_data_size = 0);

		// it will add that task to the next thread after the last one used and returns the index of the
		// thread that is executing the task
		unsigned int AddDynamicTaskAndWake(ThreadTask task, size_t task_data_size = 0);

		// does not affect last thread id used
		void AddDynamicTask(ThreadTask task, unsigned int thread_id, size_t task_data_size = 0);

		// does not affect last thread id used
		void AddDynamicTaskAndWake(ThreadTask task, unsigned int thread_id, size_t task_data_size = 0);

		void ChangeStaticWrapperMode(TaskManagerWrapper wrapper_mode, void* wrapper_data = nullptr, size_t wrapper_data_size = 0, ThreadFunctionWrapper custom_function = nullptr);

		void ChangeDynamicWrapperMode(TaskManagerWrapper wrapper_mode, void* wrapper_data = nullptr, size_t wrapper_data_size = 0, ThreadFunctionWrapper custom_function = nullptr);

		void ClearTaskStream();

		void ClearTaskIndices();

		void ClearTaskIndex(unsigned int thread_id);

		void CreateThreads(LinearAllocator** temp_allocators, MemoryManager** memory);

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

}

