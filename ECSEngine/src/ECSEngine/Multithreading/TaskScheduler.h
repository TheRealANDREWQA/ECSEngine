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

		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return elements.allocator;
		}

		void Add(const TaskSchedulerElement& element, bool copy_data = false);

		void Add(Stream<TaskSchedulerElement> stream, bool copy_data = false);

		void Copy(Stream<TaskSchedulerElement> stream, bool copy_data = false);

		void ClearFrame();

		unsigned int GetCurrentQueryIndex() const;

		const TaskSchedulerInfo* GetCurrentQueryInfo() const;

		// Advances the thread's query index to the next one
		void IncrementQueryIndex();

		// Needs to be called before starting the first frame
		// It records the static queries handles
		void InitializeSchedulerInfo(World* world);

		void Reset();

		void Remove(Stream<char> task_name);

		// Returns whether or not it succeded in solving the graph 
		// Most likely cause for unsuccessful solution is circular dependencies
		// Or missing tasks
		bool Solve(CapacityStream<char>* error_message = nullptr);

		// It will write a string version of the stored tasks inside and their order
		// Alongside any other useful information
		void StringifyScheduleOrder(AdditionStream<char> string) const;
		
		// Performs the initialization phase of the systems
		void RunInitializeTasks(World* world) const;

		// Copies all the tasks
		void SetTaskManagerTasks(TaskManager* task_manager) const;

		// It will set the wrapper for the task manager such that it will respect the scheduling
		static void SetTaskManagerWrapper(TaskManager* task_manager);

		static MemoryManager DefaultAllocator(GlobalMemoryManager* memory);

		ResizableStream<TaskSchedulerElement> elements;
		Stream<ECS_TASK_SCHEDULER_BARRIER_TYPE> task_barriers;
		// This is per query
		Stream<TaskSchedulerInfo> query_infos;
		// This is the current query index that was processed
		unsigned int query_index;
	};

}