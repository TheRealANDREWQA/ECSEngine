#include "ecspch.h"
#include "TaskSchedulerTypes.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	// The last flag indicates whether or not to check that the optional count is zero
	static void AddComponentImpl(TaskComponentQuery* query, Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory, bool check_optional) {
		if (check_optional) {
			ECS_ASSERT(query->optional_component_count == 0, "Trying to add TaskComponentQuery unique component after optional one");
		}

		if (query->component_count < ECS_TASK_COMPONENT_QUERY_COUNT) {
			query->components[query->component_count] = component;
			query->component_access[query->component_count++] = access_type;
		}
		else {
			// Allocate memory
			void* allocation = Allocate(temp_memory, (sizeof(Component) + sizeof(ECS_ACCESS_TYPE)) * (query->component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;
			ECS_ACCESS_TYPE* new_access_ptr = (ECS_ACCESS_TYPE*)OffsetPointer(allocation, sizeof(Component) * (query->component_count + 1));

			const Component* current_components = query->Components();
			const ECS_ACCESS_TYPE* current_access = query->ComponentAccess();
			memcpy(new_components, current_components, sizeof(Component) * query->component_count);
			memcpy(new_access_ptr, current_access, sizeof(ECS_ACCESS_TYPE) * query->component_count);

			new_components[query->component_count] = component;
			new_access_ptr[query->component_count++] = access_type;

			query->components_ptr = new_components;
			query->component_access_ptr = new_access_ptr;
		}
	}

	static void AddSharedComponentImpl(TaskComponentQuery* query, Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory, bool check_optional) {
		if (check_optional) {
			ECS_ASSERT(query->optional_shared_component_count == 0, "Trying to add TaskComponentQuery shared component after optional one");
		}
		
		if (query->shared_component_count < ECS_TASK_COMPONENT_QUERY_COUNT) {
			query->shared_components[query->shared_component_count] = component;
			query->shared_component_access[query->shared_component_count++] = access_type;
		}
		else {
			// Allocate memory
			void* allocation = Allocate(temp_memory, (sizeof(Component) + sizeof(ECS_ACCESS_TYPE)) * (query->shared_component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;
			ECS_ACCESS_TYPE* new_access_ptr = (ECS_ACCESS_TYPE*)OffsetPointer(allocation, sizeof(Component) * (query->shared_component_count + 1));

			const Component* current_components = query->SharedComponents();
			const ECS_ACCESS_TYPE* current_access = query->SharedComponentAccess();
			memcpy(new_components, current_components, sizeof(Component) * query->shared_component_count);
			memcpy(new_access_ptr, current_access, sizeof(ECS_ACCESS_TYPE) * query->shared_component_count);

			new_components[query->shared_component_count] = component;
			new_access_ptr[query->shared_component_count++] = access_type;

			query->shared_components_ptr = new_components;
			query->shared_component_access_ptr = new_access_ptr;
		}
	}
	
	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory)
	{
		AddComponentImpl(this, component, access_type, temp_memory, true);
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddSharedComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory)
	{
		AddSharedComponentImpl(this, component, access_type, temp_memory, true);
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddComponentExclude(Component component, AllocatorPolymorphic temp_memory)
	{
		if (exclude_component_count < ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			exclude_components[exclude_component_count++] = component;
		}
		else {
			// Allocate memory
			void* allocation = Allocate(temp_memory, (sizeof(Component)) * (exclude_component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;

			const Component* current_components = ExcludeComponents();
			memcpy(new_components, current_components, sizeof(Component) * exclude_component_count);
			new_components[exclude_component_count++] = component;
			exclude_components_ptr = new_components;
		}
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddSharedComponentExclude(Component component, AllocatorPolymorphic temp_memory)
	{
		if (exclude_shared_component_count < ECS_TASK_COMPONENT_QUERY_EXCLUDE_COUNT) {
			exclude_shared_components[exclude_shared_component_count++] = component;
		}
		else {
			// Allocate memory
			void* allocation = Allocate(temp_memory, (sizeof(Component)) * (exclude_shared_component_count + 1), alignof(Component));
			Component* new_components = (Component*)allocation;

			const Component* current_components = ExcludeComponents();
			memcpy(new_components, current_components, sizeof(Component) * exclude_shared_component_count);
			new_components[exclude_shared_component_count++] = component;
			exclude_shared_components_ptr = new_components;
		}
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddOptionalComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory)
	{
		AddComponentImpl(this, component, access_type, temp_memory, false);
		optional_component_count++;
	}

	// --------------------------------------------------------------------------------------

	void TaskComponentQuery::AddOptionalSharedComponent(Component component, ECS_ACCESS_TYPE access_type, AllocatorPolymorphic temp_memory)
	{
		AddSharedComponentImpl(this, component, access_type, temp_memory, false);
		optional_shared_component_count++;
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
			copy = CopyTo(allocation);
		}
		else {
			copy = BitwiseCopy();
		}

		return copy;
	}

	// --------------------------------------------------------------------------------------

	TaskComponentQuery TaskComponentQuery::CopyTo(void* allocation) const
	{
		uintptr_t ptr = (uintptr_t)allocation;
		return CopyTo(ptr);
	}

	// --------------------------------------------------------------------------------------

	TaskComponentQuery TaskComponentQuery::CopyTo(uintptr_t& buffer) const
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

	bool TaskComponentQuery::Conflicts(const TaskComponentQuery* other) const
	{
		const Component* components = Components();
		const ECS_ACCESS_TYPE* components_access = ComponentAccess();
		const Component* shared_components = SharedComponents();
		const ECS_ACCESS_TYPE* shared_components_access = SharedComponentAccess();

		const Component* other_components = other->Components();
		const ECS_ACCESS_TYPE* other_components_access = other->ComponentAccess();
		const Component* other_shared_components = other->SharedComponents();
		const ECS_ACCESS_TYPE* other_shared_components_access = other->SharedComponentAccess();

		for (size_t index = 0; index < component_count; index++) {
			for (size_t other_index = 0; other_index < other->component_count; other_index++) {
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

		for (size_t index = 0; index < shared_component_count; index++) {
			for (size_t other_index = 0; other_index < other->shared_component_count; other_index++) {
				if (other_shared_components[other_index] == shared_components[index]) {
					if (ECSEngine::Conflicts(other_shared_components_access[other_index], shared_components_access[index])) {
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

	bool TaskSchedulerElement::Conflicts(const TaskSchedulerElement* other) const
	{
		if (component_query.IsValid() && other->component_query.IsValid()) {
			return component_query.Conflicts(&other->component_query);
		}
		return false;
	}

	// --------------------------------------------------------------------------------------

	size_t TaskSchedulerElement::CopySize() const
	{
		size_t total_size = task_name.size * sizeof(char) + sizeof(TaskDependency) * task_dependencies.size + sizeof(TaskComponentQuery);

		for (size_t index = 0; index < task_dependencies.size; index++) {
			total_size += task_dependencies[index].name.size;
		}

		return total_size;
	}

	// --------------------------------------------------------------------------------------

	TaskSchedulerElement TaskSchedulerElement::Copy(AllocatorPolymorphic allocator) const
	{
		void* allocation = Allocate(allocator, CopySize());
		return CopyTo(allocation);
	}

	// --------------------------------------------------------------------------------------

	TaskSchedulerElement TaskSchedulerElement::CopyTo(void* allocation) const
	{
		uintptr_t ptr = (uintptr_t)allocation;
		return CopyTo(ptr);
	}

	// --------------------------------------------------------------------------------------

	TaskSchedulerElement TaskSchedulerElement::CopyTo(uintptr_t& buffer) const
	{
		TaskSchedulerElement copy;

		memcpy(&copy, this, sizeof(copy));

		copy.task_dependencies.InitializeAndCopy(buffer, task_dependencies);

		for (size_t index = 0; index < task_dependencies.size; index++) {
			copy.task_dependencies[index].name.InitializeAndCopy(buffer, task_dependencies[index].name);
		}

		task_name.CopyTo(buffer);

		return copy;
	}

	// --------------------------------------------------------------------------------------

	void* TaskSchedulerElement::GetAllocatedBuffer() const
	{
		return task_dependencies.buffer;
	}

	// --------------------------------------------------------------------------------------

	bool TaskSchedulerElement::IsTaskDependency(const TaskSchedulerElement* other) const
	{
		Stream<char> current_name = task_name;
		for (size_t index = 0; index < other->task_dependencies.size; index++) {
			if (current_name == other->task_dependencies[index].name) {
				return true;
			}
		}

		return false;
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

	void TaskGroupToString(ECS_THREAD_TASK_GROUP group, CapacityStream<char>& error_message)
	{
		static Stream<char> GROUP_STRINGS[] = {
			"Initialize Early",
			"Initialize Mid",
			"Initialize Late",
			"Simulate Early",
			"Simulate Mid",
			"Simulate Late",
			"Finalize Early",
			"Finalize Mid",
			"Finalize Late"
		};
		static_assert(ECS_COUNTOF(GROUP_STRINGS) == ECS_THREAD_TASK_GROUP_COUNT, "The TASK_GROUP enum needs to be matched with its string representation");
		
		error_message.AddStreamAssert(GROUP_STRINGS[group]);
	}

	// --------------------------------------------------------------------------------------

}