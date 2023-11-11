#include "ecspch.h"
#include "FrameProfiler.h"

namespace ECSEngine {

#define FRAME_THREAD_ROOT_CAPACITY 256
#define ROOT_STACK_CAPACITY 2

	void FrameProfilerEntry::Initialize(
		AllocatorPolymorphic allocator, 
		Stream<char> _name, 
		unsigned int entry_capacity, 
		unsigned int min_values_capacity, 
		unsigned char _tag
	)
	{
		statistic.Initialize(allocator, entry_capacity, min_values_capacity);
		name = _name.Copy(allocator);
		tag = _tag;
	}

	AllocatorPolymorphic FrameProfilerThread::Allocator() const
	{
		return GetAllocatorPolymorphic(&allocator);
	}

	void FrameProfilerThread::Push(Stream<char> name, unsigned int entry_capacity, unsigned int entry_min_values_capacity, unsigned char tag)
	{
		auto create_entry = [=]() {
			FrameProfilerEntry entry;
			entry.Initialize(Allocator(), name, entry_capacity, entry_min_values_capacity, tag);
			return entry;
		};

		if (child_index == 0) {
			// There are no entries for this root, we need to advance
			root_index++;
			if (root_index >= roots.capacity) {
				// We need to resize the entries
				roots.Resize(roots.capacity * 1.25f + 1);
			}

			// Check to see if the name matches the current root
			// If it doesn't we need to insert a new root basically
			if (roots[root_index].size == 0) {
				// There is nothing present at this root, we can insert here
				roots[root_index].Add(create_entry());
			}
			else {
				Stream<char> current_root_name = roots[root_index][0].name;
				if (current_root_name != name) {
					// If the name didn't match, we need to insert at this index
					ResizableStream<FrameProfilerEntry> new_root;
					new_root.Initialize(Allocator(), ROOT_STACK_CAPACITY);
					new_root.Add(create_entry());
					roots.Insert(root_index, new_root);
				}
				else {
					// Nothing to do here since they match, we need to wait for Pop
					// To give us the update value
				}
			}
		}
		else {
			// Firstly check to see if the child is a new entry into the stack
			if (child_index > roots[root_index].size) {
				// A new entry
				roots[root_index].Add(create_entry());
			}
			else {
				// Verify to see if the name is matched
				Stream<char> current_entry_name = roots[root_index][child_index].name;
				if (current_entry_name != name) {
					// A new entry, must displace the elements
					roots[root_index].Insert(child_index, create_entry());
				}
				else {
					// Nothing to do here since they match, we need to wait for Pop
					// To give us the update value
				}
			}
		}

		child_index++;
	}

	void FrameProfilerThread::Pop(float value)
	{
		child_index--;
		roots[root_index][child_index].Add(value);
	}

	void FrameProfilerThread::Initialize(AllocatorPolymorphic backup_allocator, size_t arena_capacity, size_t arena_backup_capacity)
	{
		CreateBaseAllocatorInfo init_allocator_info;
		init_allocator_info.allocator_type = ECS_ALLOCATOR_ARENA;
		init_allocator_info.arena_allocator_count = 4;
		init_allocator_info.arena_multipool_block_count = ECS_KB * 2;
		init_allocator_info.arena_capacity = arena_capacity;
		init_allocator_info.arena_nested_type = ECS_ALLOCATOR_MULTIPOOL;

		CreateBaseAllocatorInfo backup_allocator_info = init_allocator_info;
		backup_allocator_info.arena_capacity = arena_backup_capacity;
		allocator = MemoryManager(init_allocator_info, backup_allocator_info, backup_allocator);

		AllocatorPolymorphic allocator_polymorphic = GetAllocatorPolymorphic(&allocator);
		roots.Initialize(allocator_polymorphic, FRAME_THREAD_ROOT_CAPACITY);

		for (unsigned int index = 0; index < roots.size; index++) {
			ResizableStream<FrameProfilerEntry> root(allocator_polymorphic, ROOT_STACK_CAPACITY);
			roots.Add(root);
		}

		root_index = -1;
		child_index = 0;
	}

	void FrameProfiler::Push(unsigned int thread_id, Stream<char> name, unsigned char tag)
	{
		threads[thread_id].Push(name, entry_capacity, entry_min_value_capacity, tag);
	}

	void FrameProfiler::Pop(unsigned int thread_id, float value)
	{
		threads[thread_id].Pop(value);
	}

	void FrameProfiler::Initialize(
		AllocatorPolymorphic allocator, 
		unsigned int thread_count,
		unsigned int _entry_capacity,
		unsigned int _entry_min_value_capacity,
		size_t thread_arena_capacity, 
		size_t thread_arena_backup_capacity
	)
	{
		entry_capacity = _entry_capacity;
		entry_min_value_capacity = _entry_min_value_capacity;

		threads.Initialize(allocator, thread_count);
		for (unsigned int index = 0; index < thread_count; index++) {
			threads[index].Initialize(allocator, thread_arena_capacity, thread_arena_backup_capacity);
		}
	}

	size_t FrameProfilerAllocatorReserve(unsigned int thread_count, size_t thread_arena_capacity, size_t thread_arena_backup_capacity)
	{
		// Estimate that half of the threads would use a backup allocation
		return (size_t)thread_count * thread_arena_capacity + ((size_t)thread_count / 2) * thread_arena_backup_capacity;
	}

}