#pragma once
#include "../../Core.h"
#include "../InternalStructures.h"
#include "ThreadTask.h"

namespace ECSEngine {

	enum ECS_ACCESS_TYPE : unsigned char {
		ECS_READ,
		ECS_WRITE,
		ECS_READ_WRITE
	};

	// Returns true if there is a read-write conflict between the two. Else false
	ECSENGINE_API bool Conflicts(ECS_ACCESS_TYPE first, ECS_ACCESS_TYPE second);

	enum ECS_THREAD_TASK_GROUP : unsigned char {
		ECS_THREAD_TASK_INITIALIZE_EARLY,
		ECS_THREAD_TASK_INITIALIZE_LATE,
		ECS_THREAD_TASK_SIMULATE_EARLY,
		ECS_THREAD_TASK_SIMULATE_LATE,
		ECS_THREAD_TASK_FINALIZE_EARLY,
		ECS_THREAD_TASK_FINALIZE_LATE,
		ECS_THREAD_TASK_GROUP_COUNT
	};

	enum ECS_THREAD_TASK_READ_VISIBILITY_TYPE : unsigned char {
		ECS_THREAD_TASK_READ_LAZY,
		/*
			The system can read out of sync data. e.g. some deferred calls are pending
			but are not commited before the data is being read.
		*/
		ECS_THREAD_TASK_READ_LATEST_SELECTION,
		/*
			Forces the scheduler to commit all the pending modifications to the relevant
			components.
		*/
		ECS_THREAD_TASK_READ_LATEST_ALL,
		/*
			Forces the scheduler to commit all pending modifications.
		*/
	};

	enum ECS_THREAD_TASK_WRITE_VISIBILITY_TYPE : unsigned char {
		ECS_THREAD_TASK_WRITE_LAZY,
		/*
			This allows the scheduler to make a deferred write call if there is read dependency.
			The update will be commited at a later time
		*/
		ECS_THREAD_TASK_WRITE_MAKE_VISIBLE,
		/*
			This "makes" visible the write to the other tasks after it immediately.
			It might create a temporary buffer where the stores are written and the next
			task will read this buffer. The commit into the archetype will be done lazily.
		*/
		ECS_THREAD_TASK_WRITE_COMMIT
		/*
			The writes will be done directly into the entities' components.
			It might force a wait barrier in order for this to happen
		*/
	};

	// It describes how a task depends on the other one.
	// Example. A -> B, A is a dependency for B. At the moment this
	// dependency is at the task level (not query).
	struct TaskDependency {
		Stream<char> name;
	};

#define ECS_TASK_COMPONENT_QUERY_COUNT (6)
#define ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT (3)
	
	// Can store in itself at max ECS_TASK_COMPONENT_QUERY_COUNT
	struct ECSENGINE_API TaskComponentQuery {
		void AddComponent(Component component, ECS_ACCESS_TYPE access_type, LinearAllocator* temp_memory);
		
		void AddSharedComponent(Component component, ECS_ACCESS_TYPE access_type, LinearAllocator* temp_memory);

		void AddComponentExclude(Component component, LinearAllocator* temp_memory);

		void AddSharedComponentExclude(Component component, LinearAllocator* temp_memory);

		inline const Component* Components() const {
			return component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? components_ptr : components;
		}

		inline const Component* SharedComponents() const {
			return shared_component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? shared_components_ptr : shared_components;
		}

		inline const ECS_ACCESS_TYPE* ComponentAccess() const {
			return component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? component_access_ptr : component_access;
		}

		inline const ECS_ACCESS_TYPE* SharedComponentAccess() const {
			return shared_component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? shared_component_access_ptr : shared_component_access;
		}

		inline const Component* ExcludeComponents() const {
			return exclude_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT ? exclude_components_ptr : exclude_components;
		}

		inline const Component* ExcludeSharedComponents() const {
			return exclude_shared_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT ? exclude_shared_components_ptr : exclude_shared_components;
		}

		// Returns 0 if it doesn't need to have the buffers allocated
		size_t CopySize() const;

		// The allocator is needed in case there are some buffers stored
		// It might not allocate
		TaskComponentQuery Copy(AllocatorPolymorphic allocator) const;

		// It the case copy size returns more than 0
		TaskComponentQuery CopyTo(void* allocation) const;

		TaskComponentQuery CopyTo(uintptr_t& buffer) const;

		// In case everything is embedded
		TaskComponentQuery BitwiseCopy() const;

		void* GetAllocatedBuffer() const;

		// Determines whether or not there is a read-write conflict on the same component
		bool Conflicts(const TaskComponentQuery& other) const;

		union {
			struct {
				Component components[ECS_TASK_COMPONENT_QUERY_COUNT];
				Component shared_components[ECS_TASK_COMPONENT_QUERY_COUNT];
				ECS_ACCESS_TYPE component_access[ECS_TASK_COMPONENT_QUERY_COUNT];
				ECS_ACCESS_TYPE shared_component_access[ECS_TASK_COMPONENT_QUERY_COUNT];

				Component exclude_components[ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT];
				Component exclude_shared_components[ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT];
			};
			struct {
				Component* components_ptr;
				Component* shared_components_ptr;
				ECS_ACCESS_TYPE* component_access_ptr;
				ECS_ACCESS_TYPE* shared_component_access_ptr;

				Component* exclude_components_ptr;
				Component* exclude_shared_components_ptr;
			};
		};

		unsigned char component_count = 0;
		unsigned char exclude_component_count = 0;
		unsigned char shared_component_count = 0;
		unsigned char exclude_shared_component_count = 0;
		ECS_THREAD_TASK_READ_VISIBILITY_TYPE read_type;
		ECS_THREAD_TASK_WRITE_VISIBILITY_TYPE write_type;
		unsigned short batch_size;
	};

	// This is the building block that the dependency solver and the scheduler will
	// use in order to determine the order in which the tasks should run and how they
	// should synchronize.
	struct ECSENGINE_API TaskSchedulerElement {

		size_t CopySize() const;

		TaskSchedulerElement Copy(AllocatorPolymorphic allocator) const;

		TaskSchedulerElement CopyTo(void* allocation) const;

		TaskSchedulerElement CopyTo(uintptr_t& buffer) const;

		void* GetAllocatedBuffer() const;

		// Returns true if the current task is a dependency for other
		bool IsTaskDependency(const TaskSchedulerElement& other) const;

		// Fills in the indices of the queries which are conflicting between the two elements
		void Conflicts(const TaskSchedulerElement& other, CapacityStream<uint2>* conflicting_queries) const;

		ThreadTask task;
		Stream<TaskDependency> task_dependencies;
		Stream<TaskComponentQuery> component_queries;

		ECS_THREAD_TASK_GROUP task_group;
	};

}