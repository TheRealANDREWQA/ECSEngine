#include "ecspch.h"
#include "TaskDependencies.h"
#include "../../Utilities/Function.h"

namespace ECSEngine {

	void TaskDependencies::Add(TaskDependencyElement element) {
		size_t name_size = strlen(element.task.name) + 1;
		size_t total_size = name_size * sizeof(char) + sizeof(Stream<char>) * element.dependencies.size;
		for (size_t index = 0; index < element.dependencies.size; index++) {
			total_size += element.dependencies[index].size + 1;
		}

		void* allocation = Allocate(elements.allocator, total_size, alignof(char));
		uintptr_t ptr = (uintptr_t)allocation;
		memcpy((void*)ptr, element.task.name, name_size);
		element.task.name = (char*)ptr;
		ptr += name_size * sizeof(char);

		Stream<char>* streams = (Stream<char>*)ptr;
		ptr += sizeof(Stream<char>) * element.dependencies.size;
		for (size_t index = 0; index < element.dependencies.size; index++) {
			element.dependencies[index].CopyTo((void*)ptr);
			element.dependencies[index].buffer = (char*)ptr;
			element.dependencies[index].buffer[element.dependencies[index].size] = '\0';
			ptr += element.dependencies[index].size + 1;
		}
		element.dependencies.CopyTo(streams);
		element.dependencies.buffer = streams;

		elements.Add(element);
	}

	void TaskDependencies::Add(Stream<TaskDependencyElement> stream) {
		for (size_t index = 0; index < stream.size; index++) {
			Add(stream[index]);
		}
	}

	void TaskDependencies::Copy(Stream<TaskDependencyElement> stream) {
		elements.Copy(stream);
	}

	void TaskDependencies::Reset()
	{
		for (size_t index = 0; index < elements.size; index++) {
			// There is a single coallesced allocation
			Deallocate(elements.allocator, elements[index].task.name);
		}
		elements.FreeBuffer();
	}

	void TaskDependencies::Remove(const char* name) {
		Remove(ToStream(name));
	}

	void TaskDependencies::Remove(Stream<char> task_name) {
		for (size_t index = 0; index < elements.size; index++) {
			if (function::CompareStrings(ToStream(elements[index].task.name), task_name)) {
				// The task has a single coallesced block
				Deallocate(elements.allocator, elements[index].task.name);
				elements.Remove(index);
				return;
			}
		}
	}

	bool TaskDependencies::Solve() {
		// For each group create an index mask for those tasks that are in the same group
		Stream<unsigned int> group_indices[ECS_THREAD_TASK_GROUP_COUNT];
		for (size_t index = 0; index < ECS_THREAD_TASK_GROUP_COUNT; index++) {
			group_indices[index].buffer = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * elements.size);
		}

		for (size_t index = 0; index < elements.size; index++) {
			group_indices[elements[index].task_group].Add(index);
		}

		size_t group_start = 0;
		// The algorithm is the same for all gruops
		for (size_t group_index = 0; group_index < ECS_THREAD_TASK_GROUP_COUNT; group_index++) {
			// First pass - detect all tasks that do not have dependencies and place them
			// at the front
			unsigned int current_count = 0;
			for (size_t index = 0; index < elements.size; index++) {
				size_t subindex = group_indices[group_index][index];
				if (elements[subindex].dependencies.size == 0) {
					elements.Swap(current_count, subindex);
					current_count++;
				}
			}

			// Next passes - every task that has its dependencies met 
			size_t pass_count = 0;
			// Loop for at most 100 times in order to prevent infinite loops 
			while (pass_count < 100 && current_count < group_indices[group_index].size) {
				for (size_t index = 0; index < group_indices[group_index].size; index++) {
					size_t subindex = group_indices[group_index][index];
					Stream<char> temp_dependencies[128];
					Stream<Stream<char>> current_dependencies(temp_dependencies, elements[subindex].dependencies.size);
					elements[subindex].dependencies.CopyTo(temp_dependencies);

					for (size_t dependency_index = 0; dependency_index < current_count && current_dependencies.size > 0; dependency_index++) {
						unsigned int found_index = function::FindString(ToStream(elements[group_start + dependency_index].task.name), current_dependencies);
						if (found_index != -1) {
							current_dependencies.RemoveSwapBack(found_index);
						}
					}
					if (current_dependencies.size == 0) {
						elements.Swap(current_count, subindex);
						current_count++;
					}
				}
				pass_count++;
			}

			group_start += current_count;
			if (pass_count >= 100) {
				return false;
			}
		}
	}

}