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

	struct TaskSchedulerTransferStaticData {
		Stream<char> task_name;
		Stream<void> data;
	};

	struct TaskSchedulerSetManagerOptions {
		bool call_initialize_functions = true;
		bool preserve_data_flag = false;
		bool assert_exists_initialize_from_other_task = true;
		Stream<TaskSchedulerTransferStaticData> transfer_data = {};
	};

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

		// It will call the cleanup functions for the static tasks and it will fill in the task that want the data to be kept around
		// It will use the allocator given. You can optionally specify whether or not it should an entry for those tasks that do not
		// have a cleanup function
		void CallCleanupFunctions(
			World* world, 
			AllocatorPolymorphic allocator, 
			CapacityStream<TaskSchedulerTransferStaticData>* transfer_data, 
			bool add_transfer_data_for_all_entries = false
		);

		void Copy(Stream<TaskSchedulerElement> stream, bool copy_data = false);

		void ClearFrame();

		// Returns nullptr if it doesn't find it
		TaskSchedulerElement* FindElement(Stream<char> task_name);

		// Returns nullptr if it doesn't find it
		const TaskSchedulerElement* FindElement(Stream<char> task_name) const;

		unsigned int GetCurrentQueryIndex() const;

		const TaskSchedulerInfo* GetCurrentQueryInfo() const;

		// It will fill in an entry for each static task with its corresponding data
		// It will allocate from the given allocator
		void GetTransferStaticData(const TaskManager* task_manager, AllocatorPolymorphic allocator, CapacityStream<TaskSchedulerTransferStaticData>* transfer_data) const;

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

		// Copies all the tasks and optionally calls the initialize task function
		// You can give previous simulation transfer data in case you have it
		void SetTaskManagerTasks(
			TaskManager* task_manager, 
			const TaskSchedulerSetManagerOptions* options
		) const;

		// All the tasks which want to take the data from another task
		// Will do so from this task manager
		void SetTasksDataFromOther(TaskManager* task_manager, bool assert_if_not_set) const;

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