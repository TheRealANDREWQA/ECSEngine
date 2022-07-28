#include "ecspch.h"
#include "TaskSchedulerTypes.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddComponent(Component component, ECS_ACCESS_TYPE access_type, LinearAllocator* temp_memory)
	{
		if (component_count < ECS_TASK_COMPONENT_QUERY_COUNT) {
			components[component_count] = component;
			component_access[component_count++] = access_type;
		}
		else {
			// Allocate memory
			void* allocation = temp_memory->Allocate((sizeof(Component) + sizeof(ECS_ACCESS_TYPE)) * (component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;
			ECS_ACCESS_TYPE* new_access_ptr = (ECS_ACCESS_TYPE*)function::OffsetPointer(allocation, sizeof(Component) * (component_count + 1));

			const Component* current_components = Components();
			const ECS_ACCESS_TYPE* current_access = ComponentAccess();
			memcpy(new_components, current_components, sizeof(Component) * component_count);
			memcpy(new_access_ptr, current_access, sizeof(ECS_ACCESS_TYPE) * component_count);

			new_components[component_count] = component;
			new_access_ptr[component_count++] = access_type;

			components_ptr = new_components;
			component_access_ptr = new_access_ptr;
		}
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddSharedComponent(Component component, ECS_ACCESS_TYPE access_type, LinearAllocator* temp_memory)
	{
		if (shared_component_count < ECS_TASK_COMPONENT_QUERY_COUNT) {
			shared_components[shared_component_count] = component;
			shared_component_access[shared_component_count++] = access_type;
		}
		else {
			// Allocate memory
			void* allocation = temp_memory->Allocate((sizeof(Component) + sizeof(ECS_ACCESS_TYPE)) * (shared_component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;
			ECS_ACCESS_TYPE* new_access_ptr = (ECS_ACCESS_TYPE*)function::OffsetPointer(allocation, sizeof(Component) * (shared_component_count + 1));

			const Component* current_components = SharedComponents();
			const ECS_ACCESS_TYPE* current_access = SharedComponentAccess();
			memcpy(new_components, current_components, sizeof(Component) * shared_component_count);
			memcpy(new_access_ptr, current_access, sizeof(ECS_ACCESS_TYPE) * shared_component_count);

			new_components[shared_component_count] = component;
			new_access_ptr[shared_component_count++] = access_type;

			shared_components_ptr = new_components;
			shared_component_access_ptr = new_access_ptr;
		}
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddComponentExclude(Component component, LinearAllocator* temp_memory)
	{
		if (exclude_component_count < ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			exclude_components[exclude_component_count++] = component;
		}
		else {
			// Allocate memory
			void* allocation = temp_memory->Allocate((sizeof(Component)) * (exclude_component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;

			const Component* current_components = ExcludeComponents();
			memcpy(new_components, current_components, sizeof(Component) * exclude_component_count);
			new_components[exclude_component_count++] = component;
			exclude_components_ptr = new_components;
		}
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddSharedComponentExclude(Component component, LinearAllocator* temp_memory)
	{
		if (exclude_shared_component_count < ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			exclude_shared_components[exclude_shared_component_count++] = component;
		}
		else {
			// Allocate memory
			void* allocation = temp_memory->Allocate((sizeof(Component)) * (exclude_shared_component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;

			const Component* current_components = ExcludeComponents();
			memcpy(new_components, current_components, sizeof(Component) * exclude_shared_component_count);
			new_components[exclude_shared_component_count++] = component;
			exclude_shared_components_ptr = new_components;
		}
	}

	// --------------------------------------------------------------------------------------

	size_t TaskComponentQuery::CopySize() const
	{
		size_t total_size = 0;

		if (component_count > ECS_TASK_COMPONENT_QUERY_COUNT) {
			total_size += (sizeof(Component) + sizeof(ECS_ACCESS_TYPE)) * component_count;
		}

		if (shared_component_count > ECS_TASK_COMPONENT_QUERY_COUNT) {
			total_size += (sizeof(Component) + sizeof(ECS_ACCESS_TYPE)) * shared_component_count;
		}

		if (exclude_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			total_size += sizeof(Component) * exclude_component_count;
		}

		if (exclude_shared_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			total_size += sizeof(Component) * exclude_shared_component_count;
		}

		return total_size;
	}

	// --------------------------------------------------------------------------------------

	TaskComponentQuery TaskComponentQuery::Copy(AllocatorPolymorphic allocator) const
	{
		size_t total_size = CopySize();

		TaskComponentQuery copy;
		if (total_size > 0) {
			void* allocation = Allocate(allocator, total_size);
			copy = Copy(allocation);
		}
		else {
			copy = BitwiseCopy();
		}

		return copy;
	}

	// --------------------------------------------------------------------------------------

	TaskComponentQuery TaskComponentQuery::Copy(void* allocation) const
	{
		uintptr_t ptr = (uintptr_t)allocation;
		return Copy(ptr);
	}

	// --------------------------------------------------------------------------------------

	TaskComponentQuery TaskComponentQuery::Copy(uintptr_t& buffer) const
	{
		TaskComponentQuery copy;

		if (component_count > ECS_TASK_COMPONENT_QUERY_COUNT) {
			copy.components_ptr = (Component*)buffer;
			memcpy(copy.components_ptr, components_ptr, sizeof(Component) * component_count);
			buffer += sizeof(Component) * component_count;

			copy.component_access_ptr = (ECS_ACCESS_TYPE*)buffer;
			memcpy(copy.component_access_ptr, component_access_ptr, sizeof(ECS_ACCESS_TYPE) * component_count);
			buffer += sizeof(ECS_ACCESS_TYPE) * component_count;
		}
		else {
			memcpy(copy.components, components, sizeof(Component) * component_count);
			memcpy(copy.component_access, component_access, sizeof(ECS_ACCESS_TYPE) * component_count);
		}

		if (shared_component_count > ECS_TASK_COMPONENT_QUERY_COUNT) {
			copy.shared_components_ptr = (Component*)buffer;
			memcpy(copy.shared_components_ptr, shared_components_ptr, sizeof(Component) * shared_component_count);
			buffer += sizeof(Component) * shared_component_count;

			copy.shared_component_access_ptr = (ECS_ACCESS_TYPE*)buffer;
			memcpy(copy.shared_component_access_ptr, shared_component_access_ptr, sizeof(ECS_ACCESS_TYPE) * shared_component_count);
			buffer += sizeof(ECS_ACCESS_TYPE) * shared_component_count;
		}
		else {
			memcpy(copy.shared_components, shared_components, sizeof(Component) * shared_component_count);
			memcpy(copy.shared_component_access, shared_component_access, sizeof(ECS_ACCESS_TYPE) * shared_component_count);
		}

		if (exclude_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			copy.exclude_components_ptr = (Component*)buffer;
			memcpy(copy.exclude_components_ptr, exclude_components_ptr, sizeof(Component) * exclude_component_count);
			buffer += sizeof(Component) * exclude_component_count;
		}
		else {
			memcpy(copy.exclude_components, exclude_components, sizeof(Component) * exclude_component_count);
		}

		if (exclude_shared_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			copy.exclude_shared_components_ptr = (Component*)buffer;
			memcpy(copy.exclude_shared_components_ptr, exclude_shared_components_ptr, sizeof(Component) * exclude_shared_component_count);
			buffer += sizeof(Component) * exclude_shared_component_count;
		}
		else {
			memcpy(copy.exclude_shared_components, exclude_shared_components, sizeof(Component) * exclude_shared_component_count);
		}

		copy.component_count = component_count;
		copy.shared_component_count = shared_component_count;
		copy.exclude_component_count = exclude_component_count;
		copy.exclude_shared_component_count = exclude_shared_component_count;

		return copy;
	}

	// --------------------------------------------------------------------------------------

	TaskComponentQuery TaskComponentQuery::BitwiseCopy() const
	{
		TaskComponentQuery copy;
		memcpy(&copy, this, sizeof(copy));
		return copy;
	}

	// --------------------------------------------------------------------------------------

	void* TaskComponentQuery::GetAllocatedBuffer() const
	{
		if (component_count > ECS_TASK_COMPONENT_QUERY_COUNT) {
			return components_ptr;
		}

		if (shared_component_count > ECS_TASK_COMPONENT_QUERY_COUNT) {
			return shared_components_ptr;
		}

		if (exclude_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			return exclude_components_ptr;
		}

		if (exclude_shared_component_count > ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			return exclude_shared_components_ptr;
		}

		return nullptr;
	}

	// --------------------------------------------------------------------------------------

	bool TaskComponentQuery::Conflicts(const TaskComponentQuery& other) const
	{
		const Component* components = Components();
		const ECS_ACCESS_TYPE* components_access = ComponentAccess();
		const Component* shared_components = SharedComponents();
		const ECS_ACCESS_TYPE* shared_components_access = SharedComponentAccess();

		const Component* other_components = other.Components();
		const ECS_ACCESS_TYPE* other_components_access = other.ComponentAccess();
		const Component* other_shared_components = other.SharedComponents();
		const ECS_ACCESS_TYPE* other_shared_components_access = other.SharedComponentAccess();

		for (size_t index = 0; index < component_count; index++) {
			for (size_t other_index = 0; other_index < other.component_count; other_index++) {
				if (other_components[other_index] == components[index]) {
					if (ECSEngine::Conflicts(other_components_access[other_index], components_access[index])) {
						return true;
					}
					else {
						break;
					}
				}
			}
		}

		return false;
	}

	// --------------------------------------------------------------------------------------

	size_t TaskSchedulerElement::CopySize() const
	{
		size_t name_size = strlen(task.name) + 1;
		size_t total_size = name_size * sizeof(char) + sizeof(TaskDependency) * task_dependencies.size + sizeof(TaskComponentQuery) * component_queries.size;

		for (size_t index = 0; index < task_dependencies.size; index++) {
			total_size += task_dependencies[index].name.size;
		}

		for (size_t index = 0; index < component_queries.size; index++) {
			total_size += component_queries[index].CopySize();
		}

		return total_size;
	}

	// --------------------------------------------------------------------------------------

	TaskSchedulerElement TaskSchedulerElement::Copy(AllocatorPolymorphic allocator) const
	{
		void* allocation = Allocate(allocator, CopySize());
		return Copy(allocation);
	}

	// --------------------------------------------------------------------------------------

	TaskSchedulerElement TaskSchedulerElement::Copy(void* allocation) const
	{
		uintptr_t ptr = (uintptr_t)allocation;
		return Copy(ptr);
	}

	// --------------------------------------------------------------------------------------

	TaskSchedulerElement TaskSchedulerElement::Copy(uintptr_t& buffer) const
	{
		TaskSchedulerElement copy;

		memcpy(&copy, this, sizeof(copy));

		if (task.name != nullptr) {
			size_t name_size = strlen(task.name) + 1;
			memcpy((void*)buffer, task.name, name_size * sizeof(char));
			copy.task.name = (char*)buffer;
			buffer += name_size * sizeof(char);
		}

		copy.task_dependencies.InitializeAndCopy(buffer, task_dependencies);

		for (size_t index = 0; index < task_dependencies.size; index++) {
			copy.task_dependencies[index].name.InitializeAndCopy(buffer, task_dependencies[index].name);
		}

		copy.component_queries.InitializeAndCopy(buffer, component_queries);

		for (size_t index = 0; index < component_queries.size; index++) {
			size_t copy_size = component_queries[index].CopySize();
			if (copy_size > 0) {
				copy.component_queries[index] = component_queries[index].Copy((void*)buffer);
				buffer += copy_size;
			}
		}

		return copy;
	}

	// --------------------------------------------------------------------------------------

	void* TaskSchedulerElement::GetAllocatedBuffer() const
	{
		return nullptr;
	}

	// --------------------------------------------------------------------------------------

	bool TaskSchedulerElement::IsTaskDependency(const TaskSchedulerElement& other) const
	{
		Stream<char> current_name = ToStream(task.name);
		for (size_t index = 0; index < other.task_dependencies.size; index++) {
			if (function::CompareStrings(current_name, other.task_dependencies[index].name)) {
				return true;
			}
		}

		return false;
	}

	// --------------------------------------------------------------------------------------

	void TaskSchedulerElement::Conflicts(const TaskSchedulerElement& other, CapacityStream<uint2>* conflicting_queries) const
	{
		for (size_t index = 0; index < component_queries.size; index++) {
			for (size_t other_index = 0; other_index < other.component_queries.size; other_index++) {
				if (component_queries[index].Conflicts(other.component_queries[other_index])) {
					conflicting_queries->Add({ (unsigned int)index, (unsigned int)other_index });
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------

	bool Conflicts(ECS_ACCESS_TYPE first, ECS_ACCESS_TYPE second)
	{
		if ((first & ECS_READ) != 0 && (second & ECS_WRITE) != 0) {
			return true;
		}

		if ((first & ECS_WRITE) != 0 && (second & ECS_READ) != 0) {
			return true;
		}
		
		return false;
	}

	// --------------------------------------------------------------------------------------

}