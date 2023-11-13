#include "ecspch.h"
#include "CPUFrameProfiler.h"
#include "../Utilities/OSFunctions.h"
#include "../Multithreading/TaskManager.h"

namespace ECSEngine {

#define FRAME_THREAD_ROOT_CAPACITY 256
#define ROOT_STACK_CAPACITY 2

	void CPUFrameProfilerEntry::Initialize(
		AllocatorPolymorphic allocator, 
		Stream<char> _name, 
		unsigned int entry_capacity, 
		unsigned char _tag
	)
	{
		statistic.Initialize(allocator, entry_capacity);
		name = _name.Copy(allocator);
		tag = _tag;
	}

	// Considers the allocator of the thread to be created already
	static void CPUFrameProfilerThreadInitializeAfterAllocator(
		CPUFrameProfilerThread* thread,
		unsigned int entry_capacity,
		void* thread_handle
	) {
		AllocatorPolymorphic allocator_polymorphic = thread->Allocator();
		thread->roots.Initialize(allocator_polymorphic, FRAME_THREAD_ROOT_CAPACITY);

		for (unsigned int index = 0; index < thread->roots.size; index++) {
			ResizableStream<CPUFrameProfilerEntry> root(allocator_polymorphic, ROOT_STACK_CAPACITY);
			thread->roots.Add(root);
		}

		thread->root_index = -1;
		thread->child_index = 0;
		thread->previous_frame_user_time = 0;
		thread->previous_frame_kernel_time = 0;
		thread->frame_user_time.Initialize(allocator_polymorphic, entry_capacity);
		thread->frame_kernel_time.Initialize(allocator_polymorphic, entry_capacity);
		thread->thread_handle = thread_handle;
	}

	AllocatorPolymorphic CPUFrameProfilerThread::Allocator() const
	{
		return GetAllocatorPolymorphic(&allocator);
	}

	void CPUFrameProfilerThread::Clear()
	{
		// Clearing this is extremely easy - clear the allocator and re-initialize everything
		allocator.Clear();
		unsigned int entry_capacity = frame_kernel_time.entries.GetCapacity();
		CPUFrameProfilerThreadInitializeAfterAllocator(this, entry_capacity, thread_handle);
	}

	void CPUFrameProfilerThread::EndFrame()
	{
		root_index = -1;
		child_index = 0;

		// Retrieve the thread times
		ulong2 thread_times = OS::GetThreadTimes(thread_handle);
		if (thread_times.x == -1 && thread_times.x == -1) {
			// In case we failed, set these to the current values. They will skew the results
			// But the user at least knows that some error has happened
			thread_times.x = previous_frame_user_time;
			thread_times.y = previous_frame_kernel_time;
		}

		float2 times = OS::ThreadTimesDuration(thread_times, { previous_frame_user_time, previous_frame_kernel_time }, ECS_TIMER_DURATION_MS);
		frame_user_time.Add(times.x);
		frame_kernel_time.Add(times.y);

		previous_frame_user_time = thread_times.x;
		previous_frame_kernel_time = thread_times.y;
	}

	void CPUFrameProfilerThread::Push(Stream<char> name, unsigned int entry_capacity, unsigned char tag)
	{
		auto create_entry = [=]() {
			CPUFrameProfilerEntry entry;
			entry.Initialize(Allocator(), name, entry_capacity, tag);
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
					ResizableStream<CPUFrameProfilerEntry> new_root;
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

	void CPUFrameProfilerThread::Pop(float value)
	{
		child_index--;
		roots[root_index][child_index].Add(value);
	}

	void CPUFrameProfilerThread::Initialize(
		AllocatorPolymorphic backup_allocator, 
		size_t arena_capacity, 
		size_t arena_backup_capacity,
		unsigned int entry_capacity,
		void* _thread_handle
	)
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

		CPUFrameProfilerThreadInitializeAfterAllocator(this, entry_capacity, _thread_handle);
	}

	template<bool user_time, bool kernel_time>
	static unsigned char CalculateUsageImpl(const CPUFrameProfiler* profiler, unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) {
		if (thread_id == -1) {
			size_t usage = 0;
			for (size_t index = 0; index < profiler->threads.size; index++) {
				usage += (size_t)profiler->CalculateUsage(index, value_type);
			}
			return (unsigned char)((float)usage / (float)profiler->threads.size);
		}
		else {
			float thread_execution_value = 0.0f;
			if constexpr (user_time) {
				thread_execution_value += profiler->threads[thread_id].frame_user_time.GetValue(value_type);
			}
			if (constexpr (kernel_time)) {
				thread_execution_value += profiler->threads[thread_id].frame_kernel_time.GetValue(value_type);
			}
			float frame_value = profiler->GetSimulationFrameTime(value_type);
			return (unsigned char)(thread_execution_value / frame_value * 100.0f);
		}
	}

	unsigned char CPUFrameProfiler::CalculateUsage(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const
	{
		return CalculateUsageImpl<true, true>(this, thread_id, value_type);
	}

	unsigned char CPUFrameProfiler::CalculateUsageUser(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const
	{
		return CalculateUsageImpl<true, false>(this, thread_id, value_type);
	}

	unsigned char CPUFrameProfiler::CalculateUsageKernel(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const
	{
		return CalculateUsageImpl<false, true>(this, thread_id, value_type);
	}

	unsigned char CPUFrameProfiler::CalculateUserToOverall(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const
	{
		if (thread_id == -1) {
			size_t ratio = 0;
			for (size_t index = 0; index < threads.size; index++) {
				ratio += (size_t)CalculateUserToOverall(index, value_type);
			}
			return (unsigned char)((float)ratio / (float)threads.size);
		}
		else {
			float user_value = threads[thread_id].frame_user_time.GetValue(value_type);
			float kernel_value = threads[thread_id].frame_kernel_time.GetValue(value_type);
			return (unsigned char)(user_value / (user_value + kernel_value) * 100.0f);
		}
	}

	void CPUFrameProfiler::Clear()
	{
		// Clear every thread, unitialize the timer and clear the frame statistics
		for (size_t index = 0; index < threads.size; index++) {
			threads[index].Clear();
		}
		timer.SetUninitialized();
		simulation_frame_time.Clear();
		overall_frame_time.Clear();
	}

	void CPUFrameProfiler::EndFrame()
	{
		// Get the simulation frame time here
		float frame_time = timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
		simulation_frame_time.Add(frame_time);
		for (size_t index = 0; index < threads.size; index++) {
			threads[index].EndFrame();
		}
	}

	float CPUFrameProfiler::GetSimulationFrameTime(ECS_STATISTIC_VALUE_TYPE value_type) const
	{
		return simulation_frame_time.GetValue(value_type);
	}

	float CPUFrameProfiler::GetOverallFrameTime(ECS_STATISTIC_VALUE_TYPE value_type) const
	{
		return overall_frame_time.GetValue(value_type);
	}

	void CPUFrameProfiler::Push(unsigned int thread_id, Stream<char> name, unsigned char tag)
	{
		threads[thread_id].Push(name, entry_capacity, tag);
	}

	void CPUFrameProfiler::Pop(unsigned int thread_id, float value)
	{
		threads[thread_id].Pop(value);
	}

	void CPUFrameProfiler::Initialize(
		AllocatorPolymorphic allocator, 
		Stream<void*> thread_handles,
		unsigned int _entry_capacity,
		size_t thread_arena_capacity, 
		size_t thread_arena_backup_capacity
	)
	{
		entry_capacity = _entry_capacity;;

		threads.Initialize(allocator, thread_handles.size);
		for (unsigned int index = 0; index < thread_handles.size; index++) {
			threads[index].Initialize(allocator, thread_arena_capacity, thread_arena_backup_capacity, entry_capacity, thread_handles[index]);
		}

		simulation_frame_time.Initialize(allocator, entry_capacity);
		overall_frame_time.Initialize(allocator, entry_capacity);
		timer.SetUninitialized();
	}

	void CPUFrameProfiler::Initialize(
		AllocatorPolymorphic allocator, 
		const TaskManager* task_manager, 
		unsigned int entry_capacity, 
		size_t thread_arena_capacity, 
		size_t thread_arena_backup_capacity
	)
	{
		Stream<void*> thread_handles = { task_manager->m_thread_handles, task_manager->GetThreadCount() };
		Initialize(allocator, thread_handles, entry_capacity, thread_arena_capacity, thread_arena_backup_capacity);
	}

	void CPUFrameProfiler::StartFrame()
	{
		if (timer.IsUninitialized()) {
			timer.SetNewStart();
		}
		else {
			float overall_frame_duration = timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
			overall_frame_time.Add(overall_frame_duration);
			timer.SetNewStart();
		}
	}

	size_t CPUFrameProfilerAllocatorReserve(unsigned int thread_count, size_t thread_arena_capacity, size_t thread_arena_backup_capacity)
	{
		// Estimate that half of the threads would use a backup allocation
		return (size_t)thread_count * thread_arena_capacity + ((size_t)thread_count / 2) * thread_arena_backup_capacity;
	}

}