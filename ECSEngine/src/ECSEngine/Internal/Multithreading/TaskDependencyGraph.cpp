#include "ecspch.h"
#include "TaskDependencyGraph.h"
#include "../../Utilities/Function.h"

ECS_CONTAINERS;

namespace ECSEngine {

	void TaskGraph::Add(TaskGraphElement element) {
		size_t total_size = element.task_name.size * sizeof(char);
		for (size_t index = 0; index < element.dependencies.size; index++) {
			total_size += element.dependencies[index].size;
		}

		void* allocation = elements.allocator->Allocate(total_size, alignof(char));
		uintptr_t ptr = (uintptr_t)allocation;
		element.task_name.CopyTo((void*)ptr);
		element.task_name.buffer = (char*)ptr;
		ptr += element.task_name.size * sizeof(char);
		for (size_t index = 0; index < element.dependencies.size; index++) {
			element.dependencies[index].CopyTo((void*)ptr);
			element.dependencies[index].buffer = (char*)ptr;
			ptr += element.dependencies[index].size;
		}

		elements.Add(element);
	}

	void TaskGraph::Add(Stream<TaskGraphElement> stream) {
		for (size_t index = 0; index < stream.size; index++) {
			Add(stream[index]);
		}
	}

	void TaskGraph::Copy(Stream<TaskGraphElement> stream) {
		elements.Copy(stream);
	}

	void TaskGraph::Reset()
	{
		for (size_t index = 0; index < elements.size; index++) {
			elements.allocator->Deallocate(elements[index].task_name.buffer);
		}
		elements.FreeBuffer();
	}

	void TaskGraph::Remove(const char* name) {
		Remove(ToStream(name));
	}

	void TaskGraph::Remove(Stream<char> task_name) {
		for (size_t index = 0; index < elements.size; index++) {
			if (function::CompareStrings(elements[index].task_name, task_name)) {
				elements.allocator->Deallocate(elements[index].task_name.buffer);
				elements.Remove(index);
				return;
			}
		}
	}

	bool TaskGraph::Solve() {
		// First pass - detect all tasks that do not have dependencies and place them
		// at the front
		unsigned int current_count = 0;
		for (size_t index = 0; index < elements.size; index++) {
			if (elements[index].dependencies.size == 0) {
				elements.Swap(current_count, index);
				current_count++;
			}
		}

		// Next passes - every task that has its dependencies met 
		size_t pass_count = 0;
		// Loop for at most 1'000 times in order to prevent infinite loops 
		while (pass_count < 1'000 && current_count < elements.size) {
			for (size_t index = current_count; index < elements.size; index++) {
				Stream<char> temp_dependencies[128];
				Stream<Stream<char>> current_dependencies(temp_dependencies, elements[index].dependencies.size);
				elements[index].dependencies.CopyTo(temp_dependencies);

				for (size_t subindex = 0; subindex < current_count && current_dependencies.size > 0; subindex++) {
					if (function::FindString(elements[index].task_name, current_dependencies) != -1) {
						current_dependencies.RemoveSwapBack(index);
					}
				}
				if (current_dependencies.size == 0) {
					elements.Swap(current_count, index);
					current_count++;
				}
			}
			pass_count++;
		}

		solved_task_count = current_count;
		if (pass_count < 1'000) {
			return true;
		}
		return false;
	}

}