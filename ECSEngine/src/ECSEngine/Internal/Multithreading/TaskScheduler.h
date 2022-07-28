#pragma once
#include "ThreadTask.h"
#include "TaskSchedulerTypes.h"

namespace ECSEngine {

	struct TaskManager;

	enum ECS_TASK_SCHEDULER_BARRIER_TYPE : unsigned char {
		ECS_TASK_SCHEDULER_BARRIER_NONE,
		ECS_TASK_SCHEDULER_BARRIER_PRESENT
	};

	enum ECS_TASK_SCHEDULER_WRAPPER_TYPE : unsigned char {
		ECS_TASK_SCHEDULER_WRAPPER_FINE_LOCKING,
		ECS_TASK_SCHEDULER_WRAPPER_DEFERRED_WRITE
	};

	struct TaskSchedulerInfo {
		ECS_TASK_SCHEDULER_BARRIER_TYPE barrier;
		ECS_TASK_SCHEDULER_WRAPPER_TYPE wrapper_type;
		ECS_THREAD_TASK_READ_VISIBILITY_TYPE read_type;
		SpinLock initialize_lock;
		unsigned short batch_size;

		unsigned int query_handle;


		union {
			// The fine locking wrapper data
			struct {

			};
			// The deferred write data
			struct {

			};
		};
	};

	struct World;

	struct ECSENGINE_API TaskScheduler {
		TaskScheduler() = default;
		TaskScheduler(MemoryManager* allocator);

		TaskScheduler(const TaskScheduler& other) = default;
		TaskScheduler& operator = (const TaskScheduler& other) = default;

		void Add(const TaskSchedulerElement& element, bool copy_data = false);

		void Add(Stream<TaskSchedulerElement> stream, bool copy_data = false);

		void Copy(Stream<TaskSchedulerElement> stream, bool copy_data = false);

		unsigned int GetCurrentQueryIndex(unsigned int thread_index) const;

		// Returns the indices of the archetypes that match the thread's current query
		Stream<unsigned short> GetQueryArchetypes(unsigned int thread_index, World* world) const;

		// Advances the thread's query index to the next one
		void IncrementQueryIndex(unsigned int thread_index);

		// Needs to be called before starting the first frame
		// It records the static queries handles
		void InitializeSchedulerInfo(World* world);

		void Reset();

		void Remove(const char* task_name);

		void Remove(Stream<char> task_name);

		// Returns whether or not it succeded in solving the graph 
		// Most likely cause for unsuccessful solution is circular dependencies
		// Or missing tasks
		bool Solve();

		// It is needed to initialize per thread information
		void SetThreadCount(unsigned int thread_count);

		// It will set the wrapper for the task manager such that it will respect the scheduling
		static void SetTaskManagerWrapper(TaskManager* task_manager);

		ResizableStream<TaskSchedulerElement> elements;
		Stream<ECS_TASK_SCHEDULER_BARRIER_TYPE> task_barriers;
		// This is per query
		Stream<TaskSchedulerInfo> query_infos;
		// Per thread
		Stream<std::atomic<unsigned int>> thread_query_indices;
	};

}