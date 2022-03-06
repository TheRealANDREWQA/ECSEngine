#pragma once
#include "ThreadTask.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Containers/Stream.h"

namespace ECSEngine {

	struct TaskDependencyElement {
		ThreadTask task;
		Stream<Stream<char>> dependencies;
		ECS_THREAD_TASK_GROUP task_group;
	};

	struct ECSENGINE_API TaskDependencies {
		TaskDependencies() {}
		TaskDependencies(MemoryManager* allocator) : elements(GetAllocatorPolymorphic(allocator, AllocationType::MultiThreaded), 0) {}

		TaskDependencies(const TaskDependencies& other) = default;
		TaskDependencies& operator = (const TaskDependencies & other) = default;

		// The task name and dependencies names will be copied
		void Add(TaskDependencyElement element);

		void Add(Stream<TaskDependencyElement> stream);

		void Copy(Stream<TaskDependencyElement> stream);

		void Reset();

		void Remove(const char* task_name);

		void Remove(Stream<char> task_name);

		// Returns whether or not it succeded in solving the graph 
		// Most likely cause for unsuccessful solve is circular dependencies
		// Or missing tasks
		bool Solve();

		ResizableStream<TaskDependencyElement> elements;
	};

}