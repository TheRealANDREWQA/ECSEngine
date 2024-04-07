#pragma once
#include "../Core.h"
#include "../ECS/InternalStructures.h"
#include "ThreadTask.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

	enum ECS_ACCESS_TYPE : unsigned char {
		ECS_READ,
		ECS_WRITE,
		ECS_READ_WRITE,
		ECS_ACCESS_TYPE_COUNT
	};

	// Returns true if there is a read-write conflict between the two. Else false
	ECSENGINE_API bool Conflicts(ECS_ACCESS_TYPE first, ECS_ACCESS_TYPE second);

	// Most tasks should fall in the mid subphase of the groups
	// The early and late can be used to easily schedule an action
	// Before or after another mid action
	enum ECS_THREAD_TASK_GROUP : unsigned char {
		ECS_THREAD_TASK_INITIALIZE_EARLY,
		ECS_THREAD_TASK_INITIALIZE_MID,
		ECS_THREAD_TASK_INITIALIZE_LATE,
		ECS_THREAD_TASK_SIMULATE_EARLY,
		ECS_THREAD_TASK_SIMULATE_MID,
		ECS_THREAD_TASK_SIMULATE_LATE,
		ECS_THREAD_TASK_FINALIZE_EARLY,
		ECS_THREAD_TASK_FINALIZE_MID,
		ECS_THREAD_TASK_FINALIZE_LATE,
		ECS_THREAD_TASK_GROUP_COUNT
	};

	ECSENGINE_API void TaskGroupToString(ECS_THREAD_TASK_GROUP group, CapacityStream<char>& error_message);

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
		ECS_INLINE TaskDependency() : name() {}
		ECS_INLINE TaskDependency(Stream<char> _name) : name(_name) {}

		ECS_INLINE size_t CopySize() const {
			return name.CopySize();
		}

		ECS_INLINE TaskDependency CopyTo(uintptr_t& ptr) const {
			return name.CopyTo(ptr);
		}

		ECS_INLINE void ToString(CapacityStream<char>& string) const {
			string.AddStreamAssert(name);
		}

		Stream<char> name;
	};

#define ECS_TASK_COMPONENT_QUERY_COUNT (6)
#define ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT (3)
	
	// Can store in itself at max ECS_TASK_COMPONENT_QUERY_COUNT for normal components
	// and at max ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT for exclude components
	struct ECSENGINE_API TaskComponentQuery {
		ECS_INLINE TaskComponentQuery() {}

		void AddComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory);
		
		void AddSharedComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory);

		void AddComponentExclude(Component component, AllocatorPolymorphic temp_memory);

		void AddSharedComponentExclude(Component component, AllocatorPolymorphic temp_memory);

		// The optional components must be added after all unique ones have been added
		void AddOptionalComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory);

		// The optional components must be added after all shared ones have been added
		void AddOptionalSharedComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory);

		ECS_INLINE const Component* Components() const {
			return component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? components_ptr : components;
		}

		ECS_INLINE const Component* SharedComponents() const {
			return shared_component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? shared_components_ptr : shared_components;
		}

		ECS_INLINE const ECS_ACCESS_TYPE* ComponentAccess() const {
			return component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? component_access_ptr : component_access;
		}

		ECS_INLINE const ECS_ACCESS_TYPE* SharedComponentAccess() const {
			return shared_component_count > ECS_TASK_COMPONENT_QUERY_COUNT ? shared_component_access_ptr : shared_component_access;
		}

		ECS_INLINE const Component* ExcludeComponents() const {
			return exclude_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT ? exclude_components_ptr : exclude_components;
		}

		ECS_INLINE const Component* ExcludeSharedComponents() const {
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

		ECS_INLINE bool IsValid() const {
			return component_count > 0 || shared_component_count > 0;
		}

		ECS_INLINE ComponentSignature AggregateUnique() const {
			return ComponentSignature((Component*)Components(), component_count);
		}

		ECS_INLINE ComponentSignature AggregateShared() const {
			return ComponentSignature((Component*)SharedComponents(), shared_component_count);
		}

		ECS_INLINE ComponentSignature MandatoryUnique() const {
			return ComponentSignature((Component*)Components(), component_count - optional_component_count);
		}

		ECS_INLINE ComponentSignature MandatoryShared() const {
			return ComponentSignature((Component*)SharedComponents(), shared_component_count - optional_shared_component_count);
		}

		ECS_INLINE ComponentSignature OptionalUnique() const {
			return ComponentSignature((Component*)Components() + component_count - optional_component_count, optional_component_count);
		}

		ECS_INLINE ComponentSignature OptionalShared() const {
			return ComponentSignature((Component*)SharedComponents() + shared_component_count - optional_shared_component_count, optional_shared_component_count);
		}

		ECS_INLINE ComponentSignature ExcludeSignature() const {
			return ComponentSignature((Component*)ExcludeComponents(), exclude_component_count);
		}

		ECS_INLINE ComponentSignature ExcludeSharedSignature() const {
			return ComponentSignature((Component*)ExcludeSharedComponents(), exclude_shared_component_count);
		}

		// In case everything is embedded
		TaskComponentQuery BitwiseCopy() const;

		// Useful with CopyTo to determine which was the initial allocation
		void* GetAllocatedBuffer() const;

		// Determines whether or not there is a read-write conflict on the same component
		bool Conflicts(const TaskComponentQuery* other) const;

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
		unsigned char optional_component_count = 0;
		unsigned char optional_shared_component_count = 0;
		ECS_THREAD_TASK_READ_VISIBILITY_TYPE read_type = ECS_THREAD_TASK_READ_LAZY;
		ECS_THREAD_TASK_WRITE_VISIBILITY_TYPE write_type = ECS_THREAD_TASK_WRITE_LAZY;
		unsigned short batch_size = 0;
	};

	// This function is necessary only for the case that the module is recompiled during
	// Runtime and in order to not have leaks, this function will be called to perform
	// The necessary cleanup. If it returns true, it is considered that the data was deallocated else,
	// It assumes that the data is to be transfered for when the simulation is unpaused, so it will be
	// Provided once again to the 
	typedef bool (*StaticThreadTaskInitializeCleanup)(void* data, World* world);

	// This information is received by the initialize task function
	// It can allocate a structure from here and have it be passed to
	// The normal function every time it is called. You ensure the given
	// data is large enough to store your frame data. Another field that
	// is given is the previous simulation data. This is useful for the
	// Case that a module is recompiled and you want to transfer the previous
	// Data to the new recompiled module. In this case, you can perform any
	// Modifications you like to it, and also you need to deallocate it if
	// you don't plan on using it otherwise it will be leaked
	struct StaticThreadTaskInitializeInfo {
		ECS_INLINE void* Allocate(size_t size) {
			ECS_CRASH_CONDITION_RETURN(frame_data->size + size <= frame_data->capacity, nullptr, "Task initialize data not enough space");
			void* data = OffsetPointer(frame_data->buffer, frame_data->size);
			frame_data->size += size;
			return data;
		}

		template<typename T>
		ECS_INLINE T* Allocate() {
			return (T*)Allocate(sizeof(T));
		}

		Stream<void> previous_data;
		CapacityStream<void>* frame_data;
		// This is the data that was passed in when setting up the task element
		const void* initialize_data;
	};

	typedef void (*StaticThreadTaskInitializeFunction)(World* world, StaticThreadTaskInitializeInfo* data);

	// This is the building block that the dependency solver and the scheduler will
	// use in order to determine the order in which the tasks should run and how they
	// should synchronize.
	struct ECSENGINE_API TaskSchedulerElement {

		bool Conflicts(const TaskSchedulerElement* other) const;

		size_t CopySize() const;

		TaskSchedulerElement Copy(AllocatorPolymorphic allocator) const;

		TaskSchedulerElement CopyTo(void* allocation) const;

		TaskSchedulerElement CopyTo(uintptr_t& buffer) const;

		void* GetAllocatedBuffer() const;

		// Returns true if the current task is a dependency for other
		bool IsTaskDependency(const TaskSchedulerElement* other) const;

		ECS_INLINE bool HasQuery() const {
			return component_query.IsValid();
		}

		ECS_INLINE void SetInitializeData(const void* data, size_t data_size) {
			ECS_ASSERT(data_size <= sizeof(initialize_task_function_data));
			memcpy(initialize_task_function_data, data, data_size);
		}

		ThreadFunction task_function;
		Stream<char> task_name;
		// If you want to inherit the data of another task, you can do that here
		// It will be bound at initialization time and given as parameter to the
		// Task function. You need to set this as the name of the task that you want
		// To inherit
		Stream<char> initialize_data_task_name = {};
		StaticThreadTaskInitializeFunction initialize_task_function = nullptr;
		// This is some small embedded data that you can pass to the initialize function
		size_t initialize_task_function_data[6];
		StaticThreadTaskInitializeCleanup cleanup_function = nullptr;
		TaskComponentQuery component_query = {};
		Stream<TaskDependency> task_dependencies = {};

		ECS_THREAD_TASK_GROUP task_group = ECS_THREAD_TASK_GROUP_COUNT;
		bool barrier_task = false;
		// If this flag is set to true, it will auto inherit the previous data
		// Even when the initialize task function is set. If you want to transfer
		// the data, you need to set this flag to false
		bool preserve_data = true;
	};

#define ECS_SET_SCHEDULE_TASK_FUNCTION(element, function)	element.task_name = STRING(function); \
															element.task_function = function;

}