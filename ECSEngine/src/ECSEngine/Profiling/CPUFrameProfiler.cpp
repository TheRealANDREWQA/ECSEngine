#include "ecspch.h"
#include "CPUFrameProfiler.h"
#include "../OS/Thread.h"
#include "../OS/Misc.h"
#include "../Multithreading/TaskManager.h"
#include "../Allocators/ResizableLinearAllocator.h"

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
		thread->start_frame_cycle_count = 0;
		thread->frame_utilization.Initialize(allocator_polymorphic, entry_capacity);
		thread->start_frame_cycle_count = 0;
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
		unsigned int entry_capacity = frame_utilization.entries.GetCapacity();
		CPUFrameProfilerThreadInitializeAfterAllocator(this, entry_capacity, thread_handle);
	}

	void CPUFrameProfilerThread::EndFrame(size_t frame_cycle_delta)
	{
		root_index = -1;
		child_index = 0;
		size_t thread_cycle_stamp = OS::QueryThreadCycles(thread_handle);
		size_t thread_cycle_delta = thread_cycle_stamp - start_frame_cycle_count;
		unsigned char utilization_percentage = (unsigned char)((double)thread_cycle_delta / (double)frame_cycle_delta * 100.0);
		frame_utilization.Add(utilization_percentage);
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

	void CPUFrameProfilerThread::StartFrame()
	{
		start_frame_cycle_count = OS::QueryThreadCycles(thread_handle);
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
		size_t frame_cycle_stamp = OS::Rdtsc();
		size_t frame_cycle_delta = frame_cycle_stamp - start_frame_cycle_count;
			
		// Get the simulation frame time here
		float frame_time = timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
		simulation_frame_time.Add(frame_time);
		for (size_t index = 0; index < threads.size; index++) {
			threads[index].EndFrame(frame_cycle_delta);
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

	unsigned char CPUFrameProfiler::GetCPUUsage(unsigned int thread_id, ECS_STATISTIC_VALUE_TYPE value_type) const
	{
		if (thread_id == -1) {
			size_t usage = 0;
			for (size_t index = 0; index < threads.size; index++) {
				usage += (size_t)GetCPUUsage(index, value_type);
			}
			return (unsigned char)((float)usage / (float)threads.size);
		}
		else {
			return threads[thread_id].frame_utilization.GetValue(value_type);
		}
	}

	void CPUFrameProfiler::Push(unsigned int thread_id, Stream<char> name, unsigned char tag)
	{
		threads[thread_id].Push(name, entry_capacity, tag);
	}

	void CPUFrameProfiler::Pop(unsigned int thread_id, float value)
	{
		threads[thread_id].Pop(value);
	}

	size_t CPUFrameProfiler::ReduceCPUUsageToSamplesToGraph(unsigned int thread_id, Stream<float2> samples, double spike_threshold, unsigned int sample_offset) const {
		if (thread_id == -1) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
			Stream<unsigned char> usages;
			usages.Initialize(GetAllocatorPolymorphic(&stack_allocator), entry_capacity);
			usages.size = 0;
			unsigned int entry_count = threads[0].frame_utilization.entries.GetSize();
			for (unsigned int index = 0; index < entry_count; index++) {
				float current_usage_average = 0.0f;
				for (size_t thread_index = 0; thread_index < threads.size; thread_index++) {
					current_usage_average += (float)threads[thread_index].frame_utilization.entries.PeekByIndex(index);
				}
				current_usage_average /= (float)threads.size;
				usages.Add((unsigned char)current_usage_average);
			}

			return ReduceSamplesToGraph<unsigned char>(samples, usages, spike_threshold, sample_offset);
		}
		else {
			return threads[thread_id].frame_utilization.ReduceSamplesToGraph(samples, spike_threshold, sample_offset);
		}
	}

	size_t CPUFrameProfiler::ReduceSimulationFrameTimeToSamplesToGraph(Stream<float2> samples, double spike_threshold, unsigned int sample_offset) const {
		return simulation_frame_time.ReduceSamplesToGraph(samples, spike_threshold, sample_offset);
	}

	size_t CPUFrameProfiler::ReduceOverallFrameTimeToSamplesToGraph(Stream<float2> samples, double spike_threshold, unsigned int sample_offset) const {
		return overall_frame_time.ReduceSamplesToGraph(samples, spike_threshold, sample_offset);
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

		static_assert((sizeof(CPUFrameProfilerThread) % ECS_CACHE_LINE_SIZE) == 0);

		// Align these to a cache line boundary such that they don't false share
		void* threads_allocation = Allocate(allocator, threads.MemoryOf(thread_handles.size), ECS_CACHE_LINE_SIZE);
		threads.InitializeFromBuffer(threads_allocation, thread_handles.size);
		for (unsigned int index = 0; index < thread_handles.size; index++) {
			threads[index].Initialize(allocator, thread_arena_capacity, thread_arena_backup_capacity, entry_capacity, thread_handles[index]);
		}

		simulation_frame_time.Initialize(allocator, entry_capacity);
		overall_frame_time.Initialize(allocator, entry_capacity);
		timer.SetUninitialized();
		start_frame_cycle_count = 0;
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
		// Also set the thread timestamps for the simulation start
		for (size_t index = 0; index < threads.size; index++) {
			threads[index].StartFrame();
		}
		start_frame_cycle_count = OS::Rdtsc();
	}

	size_t CPUFrameProfilerAllocatorReserve(unsigned int thread_count, size_t thread_arena_capacity, size_t thread_arena_backup_capacity)
	{
		// Estimate that half of the threads would use a backup allocation
		return (size_t)thread_count * thread_arena_capacity + ((size_t)thread_count / 2) * thread_arena_backup_capacity;
	}

}