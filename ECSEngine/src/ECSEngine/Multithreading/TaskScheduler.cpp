#include "ecspch.h"
#include "TaskScheduler.h"
#include "TaskManager.h"
#include "../ECS/World.h"
#include "TaskStealing.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

#define DEFAULT_BATCH_SIZE (8)

	// ------------------------------------------------------------------------------------------------------------

	TaskScheduler::TaskScheduler(MemoryManager* allocator) : elements(GetAllocatorPolymorphic(allocator), 0), query_infos(nullptr, 0), query_index(0) {}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::Add(const TaskSchedulerElement& element, bool copy_data) {
		if (copy_data) {
			unsigned int index = elements.ReserveNewElement();
			elements.size++;
			TaskSchedulerElement* stream_element = &elements[index];
			*stream_element = element.Copy(elements.allocator);
		}
		else {
			elements.Add(&element);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::Add(Stream<TaskSchedulerElement> stream, bool copy_data) {
		for (size_t index = 0; index < stream.size; index++) {
			Add(stream[index], copy_data);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::Copy(Stream<TaskSchedulerElement> stream, bool copy_data) {
		Reset();
		Add(stream, copy_data);
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::ClearFrame()
	{
		query_index = 0;
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::InitializeSchedulerInfo(World* world)
	{
		query_infos.size = 0;
		for (size_t index = 0; index < elements.size; index++) {
			if (elements[index].component_query.IsValid()) {
				const TaskComponentQuery& current_query = elements[index].component_query;
				size_t exclude_count = current_query.exclude_component_count;
				size_t exclude_shared_count = current_query.exclude_shared_component_count;

				ComponentSignature unique = current_query.MandatoryUnique();
				ComponentSignature shared = current_query.MandatoryShared();

				if (exclude_count > 0 || exclude_shared_count > 0) {
					ComponentSignature unique_exclude = current_query.ExcludeSignature();
					ComponentSignature shared_exclude = current_query.ExcludeSharedSignature();

					ArchetypeQueryExclude query{ unique, shared, unique_exclude, shared_exclude };
					query_infos[query_infos.size++].query_handle = world->entity_manager->RegisterQuery(query);
				}
				else {
					if (current_query.component_count > 0 || current_query.shared_component_count > 0) {
						ArchetypeQuery query{ unique, shared };
						query_infos[query_infos.size++].query_handle = world->entity_manager->RegisterQuery(query);
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::Reset()
	{
		for (size_t index = 0; index < elements.size; index++) {
			// There is a single coalesced allocation
			DeallocateIfBelongs(elements.allocator, elements[index].GetAllocatedBuffer());
		}
		elements.FreeBuffer();

		task_barriers.Deallocate(elements.allocator);
		task_barriers.size = 0;

		query_infos.Deallocate(elements.allocator);
		query_infos.size = 0;
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::Remove(Stream<char> task_name) {
		for (size_t index = 0; index < elements.size; index++) {
			if (elements[index].task_name == task_name) {
				// The task has a single coalesced block
				DeallocateIfBelongs(elements.allocator, elements[index].GetAllocatedBuffer());
				elements.Remove(index);
				return;
			}
		}

		ECS_ASSERT_FORMAT(false, "Invalid task name {#} when trying to remove from TaskScheduler", task_name);
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int TaskScheduler::GetCurrentQueryIndex() const
	{
		return query_index > 0 ? query_index - 1 : 0;
	}

	// ------------------------------------------------------------------------------------------------------------

	const TaskSchedulerInfo* TaskScheduler::GetCurrentQueryInfo() const
	{
		unsigned int current_query_index = GetCurrentQueryIndex();
		ECS_CRASH_CONDITION_RETURN(current_query_index < query_infos.size, nullptr, "TaskScheduler: Trying to retrieve query index over the bound");
		return &query_infos[current_query_index];
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::IncrementQueryIndex()
	{
		query_index++;
	}

	// ------------------------------------------------------------------------------------------------------------

	struct QueryConflict {
		unsigned int query_index;
		unsigned int dependency_query_index;
	};

#pragma region Thread Wrapper

	struct StaticTaskSchedulerWrapperData {
		
	};

	ECS_THREAD_WRAPPER_TASK(StaticTaskSchedulerWrapper) {
		TaskScheduler* scheduler = world->task_scheduler;

		// The waiting was moved to the task manager - it now has a barrier

		// Can now call the thread function
		task.function(thread_id, world, task.data);
	}
	
	struct DynamicTaskSchedulerWrapperData {

	};

	ECS_THREAD_WRAPPER_TASK(DynamicTaskSchedulerWrapper) {
		task.function(thread_id, world, task.data);
	}

#pragma endregion

#pragma region Solve functions

	// It will swap all the task such that tasks that belong to the same group are contiguous
	// It will also record the number of tasks per group
	void SolveGroupParsing(TaskScheduler* scheduler, unsigned int* group_sizes, unsigned int* group_limits) {
		ResizableStream<TaskSchedulerElement>* elements = &scheduler->elements;

		for (size_t group_index = 0; group_index < ECS_THREAD_TASK_GROUP_COUNT; group_index++) {
			group_sizes[group_index] = 0;
			if (group_index > 0) {
				group_limits[group_index] = group_limits[group_index - 1];
			}
			else {
				group_limits[group_index] = 0;
			}

			for (size_t index = 0; index < elements->size; index++) {
				if (elements->buffer[index].task_group == (ECS_THREAD_TASK_GROUP)group_index) {
					elements->Swap(group_limits[group_index]++, index);
					group_sizes[group_index]++;
				}
			}
		}
	}

	// It will make sure that the task dependencies are fulfilled before doing the current task
	// It will also output into the waiting_barriers buffer the dependencies that require a wait - it will consider only
	// those that are adjacent. The waiting barrier stream must be of size elements.size
	bool SolveTaskDependencies(
		TaskScheduler* scheduler, 
		unsigned int* group_sizes,
		unsigned int* group_limits, 
		CapacityStream<ECS_TASK_SCHEDULER_BARRIER_TYPE>* waiting_barriers,
		CapacityStream<char>* error_message
	) {
		ResizableStream<TaskSchedulerElement>* elements = &scheduler->elements;

		ECS_STACK_CAPACITY_STREAM(size_t, dependency_counts, 512);

		for (size_t group_index = 0; group_index < ECS_THREAD_TASK_GROUP_COUNT; group_index++) {
			dependency_counts.size = group_sizes[group_index];

			unsigned int group_start = group_limits[group_index] - group_sizes[group_index];
			// Record the number of dependencies for each element
			// We will swap and reduce the count when one of the tasks has
			// its dependency met
			for (size_t index = 0; index < group_sizes[group_index]; index++) {
				dependency_counts[index] = elements->buffer[group_start + index].task_dependencies.size;
			}

			// Before start iterating, remove all the dependencies from the previous groups
			if (group_index > 0) {
				for (unsigned int index = group_start; index < group_limits[group_index]; index++) {
					for (unsigned int dependency_index = 0; dependency_index < elements->buffer[index].task_dependencies.size; dependency_index++) {
						for (unsigned int previous_index = 0; previous_index < group_start; previous_index++) {
							if (elements->buffer[index].task_dependencies[dependency_index].name == elements->buffer[previous_index].task_name) {
								elements->buffer[index].task_dependencies.RemoveSwapBack(dependency_index);
								break;
							} 
						}
					}
				}
			}

			size_t iteration = 0;
			unsigned int before_iteration_finished_tasks = 0;
			unsigned int finished_tasks = 0;
			// At each step we should be able to solve at least a task. If we cannot,
			// Then there is a cycle or wrong dependencies.
			for (; iteration < group_sizes[group_index]; iteration++) {
				before_iteration_finished_tasks = finished_tasks;
				for (unsigned int index = group_start + finished_tasks; index < group_limits[group_index]; index++) {
					// All of its dependencies are fulfilled
					if (elements->buffer[index].task_dependencies.size == 0) {
						Stream<char> task_name = elements->buffer[index].task_name;

						// Swap the element and its initial dependency count
						elements->Swap(group_start + finished_tasks, index);
						dependency_counts.Swap(finished_tasks, index - group_start);

						finished_tasks++;

						// Reduce the dependencies onto this task
						for (unsigned int subindex = group_start + finished_tasks; subindex < group_limits[group_index]; subindex++) {
							for (unsigned int dependency_index = 0; dependency_index < elements->buffer[subindex].task_dependencies.size; dependency_index++) {
								if (task_name == elements->buffer[subindex].task_dependencies[dependency_index].name) {
									elements->buffer[subindex].task_dependencies.RemoveSwapBack(dependency_index);
									break;
								}
							}
						}
					}
				}

				if (before_iteration_finished_tasks == finished_tasks) {
					// Could not solve a dependency with an iteration - fail

					// Go through the tasks and detect which one form a cycle
					if (error_message != nullptr) {
						error_message->AddStream("Conflicting pairs: ");
						for (unsigned int index = group_start + finished_tasks; index < group_limits[group_index]; index++) {
							Stream<char> task_name = elements->buffer[index].task_name;
							for (unsigned int subindex = index + 1; subindex < group_limits[group_index]; subindex++) {
								Stream<TaskDependency> subindex_dependencies = elements->buffer[subindex].task_dependencies;
								for (unsigned int dependency_index = 0; dependency_index < subindex_dependencies.size; dependency_index++) {
									if (task_name == subindex_dependencies[dependency_index].name) {
										// Check to see if the subindex also has as dependency the index
										Stream<char> subindex_name = elements->buffer[subindex].task_name;
										Stream<TaskDependency> index_dependencies = elements->buffer[index].task_dependencies;
										for (unsigned int second_dependency_index = 0; second_dependency_index < index_dependencies.size; second_dependency_index++) {
											if (index_dependencies[second_dependency_index].name == subindex_name) {
												// They are both dependencies, add to the dependency list
												error_message->Add('(');
												error_message->AddStream(task_name);
												error_message->Add(',');
												error_message->Add(' ');
												error_message->AddStream(subindex_name);
												error_message->Add(')');

												// Exit also the previous loop
												dependency_index = subindex_dependencies.size;
												break;
											}
										}
									}
								}
							}
						}
						error_message->AssertCapacity();
					}


					return false;
				}

				if (finished_tasks == group_sizes[group_index]) {
					break;
				}
			}

			// Bring the dependency counts back to their original size - because these are not necessarly
			// copied and instead referenced
			for (unsigned int index = group_start; index < group_limits[group_index]; index++) {
				elements->buffer[index].task_dependencies.size = dependency_counts[index - group_start];
			}
		}

		// Now fill in the waiting barriers indices
		for (unsigned int index = 1; index < elements->size; index++) {
			const TaskSchedulerElement* element = elements->buffer + index;
			ECS_TASK_SCHEDULER_BARRIER_TYPE barrier_type = ECS_TASK_SCHEDULER_BARRIER_NONE;
			if (elements->buffer[index - 1].IsTaskDependency(element)) {
				barrier_type = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
			}
			waiting_barriers->AddAssert(barrier_type);
		}

		return true;
	}

	// It reports which queries are conflicting
	void ComponentDependenciesConflicts(
		const TaskScheduler* scheduler,
		CapacityStream<QueryConflict>* conflicting_queries
	) {
		// Only do this for tasks which are at most MAX_DEPENDENCY_CHAIN tasks apart (A, B, C, D - for A only B and C will be considered)
		// those which are further apart assume that they won't conflict because a thread cannot remain stuck so far behind
		// (introduce a barrier to enforce this)
		const ResizableStream<TaskSchedulerElement>* elements = &scheduler->elements;

		for (unsigned int index = 1; index < elements->size; index++) {
			const TaskSchedulerElement* element = elements->buffer + index;

			const TaskSchedulerElement* other_element = elements->buffer + index - 1;
			bool conflicts = element->Conflicts(other_element);
			if (conflicts) {
				conflicting_queries->AddAssert({ index - 1, index });
			}
		}
	}

	// The main wait in which 
	void SelectThreadWrappers(TaskScheduler* scheduler, Stream<ECS_TASK_SCHEDULER_BARRIER_TYPE> waiting_barriers, Stream<QueryConflict> conflicting_queries) {
		ResizableStream<TaskSchedulerElement>* elements = &scheduler->elements;	
		Stream<TaskSchedulerInfo>* infos = &scheduler->query_infos;

		// Resize the scheduling data
		infos->Initialize(elements->allocator, elements->size);
		// Memset the infos to 0 - no barrier and fine locking wrapper
		memset(infos->buffer, 0, infos->MemoryOf(infos->size));

		scheduler->task_barriers.Initialize(elements->allocator, elements->size);
		// Copy the barriers
		scheduler->task_barriers.CopyOther(waiting_barriers);

		// Remaps the index from the task scheduler element to the packed conflicting_queries
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, query_index_remapping, elements->size);
		size_t current_query_index = 0;
		for (unsigned int index = 0; index < elements->size; index++) {
			if (elements->buffer[index].HasQuery()) {
				query_index_remapping[index] = current_query_index;
				current_query_index++;
			}
		}

		/*
			At the moment explore only just waiting on the conflicting queries.
			If the performance overhead is low (i.e. the tasks are well balanced)
			Then there is no need for more fancy methods
		*/

		for (size_t conflict_index = 0; conflict_index < conflicting_queries.size; conflict_index++) {
			if (conflicting_queries[conflict_index].query_index == conflicting_queries[conflict_index].dependency_query_index + 1) {
				// Put a wait for the query index
				infos->buffer[query_index_remapping[conflicting_queries[conflict_index].query_index]].barrier = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
			}
			else if (conflicting_queries[conflict_index].dependency_query_index == conflicting_queries[conflict_index].query_index + 1) {
				// Put a wait for the dependency query index
				infos->buffer[query_index_remapping[conflicting_queries[conflict_index].dependency_query_index]].barrier = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
			}
		}

		// Copy the read type alongside the batch size
		// If the batch size is 0, use a default.
		current_query_index = 0;
		for (size_t index = 0; index < elements->size; index++) {
			if (elements->buffer[index].HasQuery()) {
				infos->buffer[current_query_index].read_type = elements->buffer[index].component_query.read_type;
				unsigned short batch_size = elements->buffer[index].component_query.batch_size;
				infos->buffer[current_query_index++].batch_size = batch_size == 0 ? DEFAULT_BATCH_SIZE : batch_size;
			}
		}
	}

#pragma endregion

	bool TaskScheduler::Solve(CapacityStream<char>* error_message) {
		// For each group create an index mask for those tasks that are in the same group
		unsigned int group_sizes[ECS_THREAD_TASK_GROUP_COUNT];
		unsigned int group_limits[ECS_THREAD_TASK_GROUP_COUNT];
		SolveGroupParsing(this, group_sizes, group_limits);

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(ECS_TASK_SCHEDULER_BARRIER_TYPE, waiting_barriers, elements.size);
		bool success = SolveTaskDependencies(this, group_sizes, group_limits, &waiting_barriers, error_message);
		waiting_barriers.AssertCapacity();

		if (!success) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(QueryConflict, component_conflicts, 2048);
		ComponentDependenciesConflicts(this, &component_conflicts);
		component_conflicts.AssertCapacity();

		SelectThreadWrappers(this, waiting_barriers, component_conflicts);

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::RunInitializeTasks(World* world) const
	{
		for (unsigned int index = 0; index < elements.size; index++) {
			if (elements[index].initialize_task_function != nullptr) {
				elements[index].initialize_task_function(0, world, nullptr);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::SetTaskManagerTasks(TaskManager* task_manager) const
	{
		for (unsigned int index = 0; index < elements.size; index++) {
			task_manager->AddTask({ { elements[index].task_function, nullptr, 0, elements[index].task_name }, elements[index].barrier_task });
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::SetTaskManagerWrapper(TaskManager* task_manager)
	{
		task_manager->ChangeDynamicWrapperMode({ DynamicTaskSchedulerWrapper });
		task_manager->ChangeStaticWrapperMode({ StaticTaskSchedulerWrapper });
	}

	// ------------------------------------------------------------------------------------------------------------

	MemoryManager TaskScheduler::DefaultAllocator(GlobalMemoryManager* memory)
	{
		return MemoryManager(ECS_KB * 32, 1024, ECS_KB * 32, GetAllocatorPolymorphic(memory));
	}

	// ------------------------------------------------------------------------------------------------------------

}