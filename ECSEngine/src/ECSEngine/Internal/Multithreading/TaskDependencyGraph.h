#pragma once
#include "ThreadTask.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Containers/Stream.h"

ECS_CONTAINERS;

namespace ECSEngine {

	struct TaskGraphElement {
		ThreadTask task;
		Stream<char> task_name;
		Stream<Stream<char>> dependencies;
	};

	struct ECSENGINE_API TaskGraph {
		TaskGraph() : solved_task_count(0) {}
		TaskGraph(MemoryManager* allocator) : elements(allocator, 0), solved_task_count(0) {}

		TaskGraph(const TaskGraph& other) = default;
		TaskGraph& operator = (const TaskGraph & other) = default;

		// The task name and dependencies names will be copied
		void Add(TaskGraphElement element);

		void Add(Stream<TaskGraphElement> stream);

		void Copy(Stream<TaskGraphElement> stream);

		void Reset();

		void Remove(const char* task_name);

		void Remove(Stream<char> task_name);

		// Returns whether or not it succeded in solving the graph 
		// Most likely cause for unsuccessful solve is circular dependencies
		// Or missing tasks
		bool Solve();

		ResizableStream<TaskGraphElement, MemoryManager> elements;
		unsigned int solved_task_count;
	};

}