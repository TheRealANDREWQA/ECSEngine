#include "ecspch.h"
#include "TaskScheduler.h"
#include "../../Utilities/Function.h"
#include "TaskManager.h"
#include "../World.h"
#include "TaskStealing.h"

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

	void TaskScheduler::InitializeSchedulerInfo(World* world)
	{
		size_t current_query_index = 0;

		for (size_t index = 0; index < elements.size; index++) {
			for (size_t query_index = 0; query_index < elements[index].component_queries.size; query_index++) {
				const TaskComponentQuery& current_query = elements[index].component_queries[query_index];
				size_t exclude_count = current_query.exclude_component_count;
				size_t exclude_shared_count = current_query.exclude_shared_component_count;

				ComponentSignature unique((Component*)current_query.Components(), current_query.component_count);
				ComponentSignature shared((Component*)current_query.SharedComponents(), current_query.shared_component_count);

				if (exclude_count > 0 || exclude_shared_count > 0) {
					ComponentSignature unique_exclude((Component*)current_query.ExcludeComponents(), current_query.exclude_component_count);
					ComponentSignature shared_exclude((Component*)current_query.ExcludeSharedComponents(), current_query.exclude_shared_component_count);

					ArchetypeQueryExclude query(unique, shared, unique_exclude, shared_exclude);
					query_infos[current_query_index++].query_handle = world->entity_manager->RegisterQuery(query);
				}
				else {
					ArchetypeQuery query(unique, shared);
					query_infos[current_query_index++].query_handle = world->entity_manager->RegisterQuery(query);
				}
			}
		}

		ECS_ASSERT(current_query_index == query_infos.size);
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::Reset()
	{
		for (size_t index = 0; index < elements.size; index++) {
			// There is a single coallesced allocation
			Deallocate(elements.allocator, elements[index].task.name);
		}
		elements.FreeBuffer();

		if (task_barriers.size > 0) {
			Deallocate(elements.allocator, task_barriers.buffer);
		}

		if (query_infos.size > 0) {
			Deallocate(elements.allocator, query_infos.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::Remove(Stream<char> task_name) {
		for (size_t index = 0; index < elements.size; index++) {
			if (function::CompareStrings(elements[index].task.name, task_name)) {
				// The task has a single coallesced block
				DeallocateIfBelongs(elements.allocator, elements[index].task.name);
				elements.Remove(index);
				return;
			}
		}

		ECS_ASSERT(false);
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int TaskScheduler::GetCurrentQueryIndex() const
	{
		return query_index;
	}

	// ------------------------------------------------------------------------------------------------------------

	const TaskSchedulerInfo* TaskScheduler::GetCurrentQueryInfo() const
	{
		return &query_infos[GetCurrentQueryIndex()];
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::IncrementQueryIndex()
	{
		query_index++;
	}

	// ------------------------------------------------------------------------------------------------------------

	struct QueryConflictIndices {
		unsigned char query_index;
		unsigned char dependency_query_index;
	};

	enum QUERY_CONFLICT_INDEX : unsigned char {
		QUERY_CONFLICT_PREVIOUS,
		QUERY_CONFLICT_INTRA_TASK,
		QUERY_CONFLICT_INDEX_COUNT
	};

	// One for the previous and one for the intra task dependencies
	struct QueryConflict {
		Stream<QueryConflictIndices> indices[QUERY_CONFLICT_INDEX_COUNT];
	};

#pragma region Thread Wrapper

	struct StaticTaskSchedulerWrapperData {
		
	};

	ECS_THREAD_WRAPPER_TASK(StaticTaskSchedulerWrapper) {
		TaskScheduler* scheduler = world->task_scheduler;
		
		int task_index = world->task_manager->GetThreadTaskIndex();
		if (scheduler->task_barriers[task_index] == ECS_TASK_SCHEDULER_BARRIER_PRESENT) {
			world->task_manager->WaitThreads();
		}

		// Can now call the thread function
		task.function(thread_id, world, task.data);
	}
	
	struct DynamicTaskSchedulerWrapperData {

	};

	ECS_THREAD_WRAPPER_TASK(DynamicTaskSchedulerWrapper) {

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
		Stream<ECS_TASK_SCHEDULER_BARRIER_TYPE> waiting_barriers,
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
							if (
								function::CompareStrings(
									elements->buffer[index].task_dependencies[dependency_index].name, 
									elements->buffer[previous_index].task.name
								)
							)
							{
								elements->buffer[index].task_dependencies.RemoveSwapBack(dependency_index);
								break;
							} 
						}
					}
				}
			}

			size_t iteration = 0;
			unsigned int finished_tasks = 0;
			// At each step we should be able to solve at least a task. If we cannot,
			// Then there is a cycle or wrong dependencies.
			for (; iteration < group_sizes[group_index]; iteration++) {
				for (unsigned int index = group_start; index < group_limits[group_index]; index++) {
					// All of its dependencies are fulfilled
					if (elements->buffer[index].task_dependencies.size == 0) {
						Stream<char> task_name = elements->buffer[index].task.name;

						// Swap the element and its initial dependency count
						elements->Swap(group_start + finished_tasks, index);
						dependency_counts.Swap(finished_tasks, index - group_start);

						finished_tasks++;

						// Reduce the dependencies onto this task
						for (unsigned int subindex = group_start + finished_tasks; subindex < group_limits[group_index]; subindex++) {
							for (unsigned int dependency_index = 0; dependency_index < elements->buffer[subindex].task_dependencies.size; dependency_index++) {
								if (function::CompareStrings(task_name, elements->buffer[subindex].task_dependencies[dependency_index].name)) {
									elements->buffer[subindex].task_dependencies.RemoveSwapBack(dependency_index);
									break;
								}
							}
						}
					}
				}

				if (finished_tasks == group_sizes[group_index]) {
					break;
				}
			}

			// Bring the dependency counts back to their original size - because these are not necessarly
			// copied and instead referenced
			for (unsigned int index = group_start; index < group_limits[group_index]; index++) {
				elements->buffer[index].task_dependencies.size = dependency_counts[index];
			}

			if (iteration == group_sizes[group_index]) {
				// Go through the tasks and detect which one form a cycle
				if (error_message != nullptr) {
					error_message->AddStream("Conflicting pairs: ");
					for (unsigned int index = group_start + finished_tasks; index < group_limits[group_index]; index++) {
						Stream<char> task_name = elements->buffer[index].task.name;
						for (unsigned int subindex = index + 1; subindex < group_limits[group_index]; subindex++) {
							Stream<TaskDependency> subindex_dependencies = elements->buffer[subindex].task_dependencies;
							for (unsigned int dependency_index = 0; dependency_index < subindex_dependencies.size; dependency_index++) {
								if (function::CompareStrings(task_name, subindex_dependencies[dependency_index].name)) {
									// Check to see if the subindex also has as dependency the index
									Stream<char> subindex_name = elements->buffer[subindex].task.name;
									Stream<TaskDependency> index_dependencies = elements->buffer[index].task_dependencies;
									for (unsigned int second_dependency_index = 0; second_dependency_index < index_dependencies.size; second_dependency_index++) {
										if (function::CompareStrings(index_dependencies[second_dependency_index].name, subindex_name)) {
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
		}

		// Now fill in the waiting barriers indices
		for (unsigned int index = 1; index < elements->size; index++) {
			const TaskSchedulerElement* element = elements->buffer + index;
			if (elements->buffer[index - 1].IsTaskDependency(*element)) {
				waiting_barriers[index] = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
			}
			else {
				waiting_barriers[index] = ECS_TASK_SCHEDULER_BARRIER_NONE;
			}
		}

		return true;
	}

	// It reports which queries are conflicting
	void ComponentDependenciesConflicts(
		const TaskScheduler* scheduler,
		Stream<QueryConflict> conflicting_queries,
		CapacityStream<QueryConflictIndices>* conflicting_indices
	) {
		// Only do this for tasks which are at most MAX_DEPENDENCY_CHAIN tasks apart (A, B, C, D - for A only B and C will be considered)
		// those which are further apart assume that they won't conflict because a thread cannot remain stuck so far behind
		// (introduce a barrier to enforce this)
		const ResizableStream<TaskSchedulerElement>* elements = &scheduler->elements;

		ECS_STACK_CAPACITY_STREAM(uint2, current_conflict, 128);
		auto resolve_conflict = [&](size_t index, size_t dependency_index) {
			current_conflict.AssertCapacity();

			if (current_conflict.size > 0) {
				conflicting_queries[index].indices[dependency_index].InitializeFromBuffer(
					conflicting_indices->buffer + conflicting_indices->size,
					current_conflict.size
				);
				conflicting_indices->size += current_conflict.size;

				for (size_t conflict_index = 0; conflict_index < current_conflict.size; conflict_index++) {
					conflicting_queries[index].indices[dependency_index][conflict_index] = {
						(unsigned char)current_conflict[conflict_index].x,
						(unsigned char)current_conflict[conflict_index].y
					};
				}
			}
			else {
				conflicting_queries[index].indices[dependency_index] = { nullptr, 0 };
			}
		};

		auto detect_intra_task_conflicts = [&](const TaskSchedulerElement* element) {
			// The intra task dependencies must be handled separately
			for (size_t query_index = 0; query_index < element->component_queries.size; query_index++) {
				for (size_t dependency_index = query_index + 1; dependency_index < element->component_queries.size; dependency_index++) {
					if (element->component_queries[query_index].Conflicts(element->component_queries[dependency_index])) {
						current_conflict.Add({ (unsigned int)query_index, (unsigned int)dependency_index });
					}
				}
			}
		};

		detect_intra_task_conflicts(elements->buffer);
		resolve_conflict(0, QUERY_CONFLICT_INTRA_TASK);

		// The element 0 must be handled separately
		for (size_t index = 1; index < elements->size; index++) {
			const TaskSchedulerElement* element = elements->buffer + index;

			const TaskSchedulerElement* other_element = elements->buffer + index - 1;
			current_conflict.size = 0;
			element->Conflicts(*other_element, &current_conflict);
			resolve_conflict(index, QUERY_CONFLICT_PREVIOUS);

			current_conflict.size = 0;
			detect_intra_task_conflicts(element);
			resolve_conflict(index, QUERY_CONFLICT_INTRA_TASK);
		}
	}

	// The main wait in which 
	void SelectThreadWrappers(TaskScheduler* scheduler, Stream<ECS_TASK_SCHEDULER_BARRIER_TYPE> waiting_barriers, Stream<QueryConflict> conflicting_queries) {
		ResizableStream<TaskSchedulerElement>* elements = &scheduler->elements;	
		Stream<TaskSchedulerInfo>* infos = &scheduler->query_infos;
		
		// Each query will be expanded into its own data
		size_t total_query_count = 0;
		for (size_t index = 0; index < elements->size; index++) {
			total_query_count += elements->buffer[index].component_queries.size;
		}

		// Resize the scheduling data
		infos->Initialize(elements->allocator, total_query_count);
		// Memset the infos to 0 - no barrier and fine locking wrapper
		memset(infos->buffer, 0, sizeof(TaskSchedulerInfo) * total_query_count);

		scheduler->task_barriers.Initialize(elements->allocator, elements->size);
		// Copy the barriers
		scheduler->task_barriers.Copy(waiting_barriers);

		/*
			At the moment explore only just waiting on the conflicting queries.
			If the performance overhead is low (i.e. the tasks are well balanced)
			Then there is no need for more fancy methods
		*/

		size_t current_query_index = 0;
		Stream<QueryConflictIndices> conflicts = conflicting_queries[0].indices[QUERY_CONFLICT_INTRA_TASK];
		for (size_t conflict_index = 0; conflict_index < conflicts.size; conflict_index++) {
			if (conflicts[conflict_index].query_index == conflicts[conflict_index].dependency_query_index + 1) {
				// Put a wait for the query index
				infos->buffer[conflicts[conflict_index].query_index].barrier = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
			}
			else if (conflicts[conflict_index].dependency_query_index == conflicts[conflict_index].query_index + 1) {
				// Put a wait for the dependency query index
				infos->buffer[conflicts[conflict_index].dependency_query_index].barrier = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
			}
		}

		current_query_index = elements->buffer[0].component_queries.size;
		for (size_t index = 1; index < elements->size; index++) {
			conflicts = conflicting_queries[index].indices[QUERY_CONFLICT_PREVIOUS];
			for (size_t conflict_index = 0; conflict_index < conflicts.size; conflict_index++) {
				// If it is the last query of the previous task and this is the first query of the current task
				if (conflicts[conflict_index].dependency_query_index == elements->buffer[index - 1].component_queries.size
					&& conflicts[conflict_index].query_index == 0) {
					infos->buffer[current_query_index].barrier = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
				}
			}

			// For intra task dependencies check again if the conflicts are on adjacent queries
			conflicts = conflicting_queries[index].indices[QUERY_CONFLICT_INTRA_TASK];
			for (size_t conflict_index = 0; conflict_index < conflicts.size; conflict_index++) {
				if (conflicts[conflict_index].query_index == conflicts[conflict_index].dependency_query_index + 1) {
					infos->buffer[conflicts[conflict_index].query_index + current_query_index].barrier = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
				}
				else if (conflicts[conflict_index].dependency_query_index == conflicts[conflict_index].query_index + 1) {
					infos->buffer[conflicts[conflict_index].dependency_query_index + current_query_index].barrier = ECS_TASK_SCHEDULER_BARRIER_PRESENT;
				}
			}
		}

		// Copy the read visibility and the batch size
		// If the batch size is 0, use a default.
		current_query_index = 0;
		for (size_t index = 0; index < elements->size; index++) {
			for (size_t query_index = 0; query_index < elements->buffer[index].component_queries.size; query_index++) {
				infos->buffer[current_query_index].read_type = elements->buffer[index].component_queries[query_index].read_type;
				unsigned short batch_size = elements->buffer[index].component_queries[query_index].batch_size;
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
		bool success = SolveTaskDependencies(this, group_sizes, group_limits, waiting_barriers, error_message);
		waiting_barriers.AssertCapacity();

		if (!success) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(QueryConflict, component_conflicts, elements.size);
		ECS_STACK_CAPACITY_STREAM(QueryConflictIndices, component_conflicts_allocation, 2048);
		ComponentDependenciesConflicts(this, component_conflicts, &component_conflicts_allocation);
		component_conflicts_allocation.AssertCapacity();

		SelectThreadWrappers(this, waiting_barriers, component_conflicts);

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::SetTaskManagerTasks(TaskManager* task_manager) const
	{
		for (size_t index = 0; index < elements.size; index++) {
			task_manager->AddTask(elements[index].task);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void TaskScheduler::SetTaskManagerWrapper(TaskManager* task_manager)
	{
		task_manager->ChangeDynamicWrapperMode(DynamicTaskSchedulerWrapper);
		task_manager->ChangeStaticWrapperMode(StaticTaskSchedulerWrapper);
	}

	// ------------------------------------------------------------------------------------------------------------

	MemoryManager TaskScheduler::DefaultAllocator(GlobalMemoryManager* memory)
	{
		return MemoryManager(ECS_KB * 32, 1024, ECS_KB * 32, memory);
	}

	// ------------------------------------------------------------------------------------------------------------

}