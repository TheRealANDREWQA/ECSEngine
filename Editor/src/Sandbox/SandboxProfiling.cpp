#include "editorpch.h"
#include "SandboxProfiling.h"
#include "SandboxAccessor.h"
#include "SandboxFile.h"

// More than enough for normal use cases
#define PROFILING_ENTRIES ECS_KB * 4

void ChangeSandboxCPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_CPU_STATISTICS_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->cpu_statistics_type = type;
	SaveEditorSandboxFile(editor_state);

	// We need to synchronize now
	SynchronizeSandboxProfilingWithStatisticTypes(editor_state, sandbox_index);
}

void ChangeSandboxGPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_GPU_STATISTICS_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->gpu_statistics_type = type;
	SaveEditorSandboxFile(editor_state);

	// We need to synchronize now
	SynchronizeSandboxProfilingWithStatisticTypes(editor_state, sandbox_index);
}

void ChangeSandboxStatisticDisplayForm(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY entry, 
	EDITOR_SANDBOX_STATISTIC_DISPLAY_FORM form
)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->statistics_display.display_form[entry] = form;
	SaveEditorSandboxFile(editor_state);
}

void ClearSandboxProfilers(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->world_profiling.Clear();
}

void DisableSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->statistics_display.is_enabled = false;
	SaveEditorSandboxFile(editor_state);
}

void EnableSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->statistics_display.is_enabled = true;
	SaveEditorSandboxFile(editor_state);
}

void EndSandboxFrameProfiling(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->world_profiling.EndFrame();
}

void EndSandboxSimulationProfiling(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->world_profiling.EndSimulation();
}

bool IsSandboxStatisticEnabled(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::ECS_WORLD_PROFILING_OPTIONS profiling_option)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	return sandbox->world_profiling.HasOption(profiling_option);
}

void InvertSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->statistics_display.is_enabled = !sandbox->statistics_display.is_enabled;
	SaveEditorSandboxFile(editor_state);
}

void InitializeSandboxProfilers(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	AllocatorPolymorphic sandbox_allocator = sandbox->GlobalMemoryManager();

	WorldProfilingInitializeDescriptor descriptor;
	descriptor.allocator = sandbox_allocator;
	descriptor.entry_capacity = PROFILING_ENTRIES;
	descriptor.init_options = ECS_WORLD_PROFILING_ALL;
	descriptor.starting_options = ECS_WORLD_PROFILING_NONE;
	descriptor.world = &sandbox->sandbox_world;

	sandbox->world_profiling.Initialize(&descriptor);
}

size_t ReserveSandboxProfilersAllocatorSize()
{
	return WorldProfiling::AllocatorSize(std::thread::hardware_concurrency(), PROFILING_ENTRIES);
}

void ResetSandboxStatistics(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	memset(&sandbox->statistics_display, 0, sizeof(sandbox->statistics_display));
	// Change the should_display to true for each value at first
	for (size_t index = 0; index < EDITOR_SANDBOX_STATISTIC_DISPLAY_COUNT; index++) {
		sandbox->statistics_display.should_display[index] = true;
	}
	sandbox->cpu_statistics_type = EDITOR_SANDBOX_CPU_STATISTICS_NONE;
	sandbox->gpu_statistics_type = EDITOR_SANDBOX_GPU_STATISTICS_NONE;

	SynchronizeSandboxProfilingWithStatisticTypes(editor_state, sandbox_index);
}

void StartSandboxFrameProfiling(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->world_profiling.StartFrame();
}

void StartSandboxSimulationProfiling(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->world_profiling.StartSimulation();
}

void SynchronizeSandboxProfilingWithStatisticTypes(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (sandbox->cpu_statistics_type == EDITOR_SANDBOX_CPU_STATISTICS_NONE) {
		if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			// We need to detach th physical memory
			if (sandbox->world_profiling.HasOption(ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
				sandbox->world_profiling.physical_memory_profiler.Detach();
			}
		}
		sandbox->world_profiling.DisableOption(ECS_WORLD_PROFILING_CPU | ECS_WORLD_PROFILING_ALLOCATOR | ECS_WORLD_PROFILING_PHYSICAL_MEMORY);
	}
	else if (sandbox->cpu_statistics_type == EDITOR_SANDBOX_CPU_STATISTICS_BASIC) {
		if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			// We need to detach th physical memory
			if (sandbox->world_profiling.HasOption(ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
				sandbox->world_profiling.physical_memory_profiler.Detach();
			}
		}
		sandbox->world_profiling.DisableOption(ECS_WORLD_PROFILING_ALLOCATOR | ECS_WORLD_PROFILING_PHYSICAL_MEMORY);
		sandbox->world_profiling.EnableOption(ECS_WORLD_PROFILING_CPU);
	}
	else if (sandbox->cpu_statistics_type == EDITOR_SANDBOX_CPU_STATISTICS_ADVANCED) {
		if (sandbox->run_state == EDITOR_SANDBOX_RUNNING) {
			// We need to detach th physical memory
			if (!sandbox->world_profiling.HasOption(ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
				sandbox->world_profiling.physical_memory_profiler.Reattach();
			}
		}
		sandbox->world_profiling.EnableOption(ECS_WORLD_PROFILING_CPU | ECS_WORLD_PROFILING_ALLOCATOR | ECS_WORLD_PROFILING_PHYSICAL_MEMORY);
	}
	else {
		ECS_ASSERT(false, "Invalid editor CPU statistic type");
	}

	if (sandbox->gpu_statistics_type == EDITOR_SANDBOX_GPU_STATISTICS_NONE) {

	}
	else if (sandbox->gpu_statistics_type == EDITOR_SANDBOX_GPU_STATISTICS_BASIC) {

	}
	else if (sandbox->gpu_statistics_type == EDITOR_SANDBOX_GPU_STATISTICS_ADVANCED) {

	}
	else {
		ECS_ASSERT(false, "Invalid editor GPU statistic type");
	}
}

OS::ECS_OS_EXCEPTION_CONTINUE_STATUS HandleAllSandboxPhysicalMemoryException(TaskManagerExceptionHandlerData* handler_data)
{
	EditorState* editor_state = (EditorState*)handler_data->user_data;
	if (handler_data->exception_information.error_code == OS::ECS_OS_EXCEPTION_PAGE_GUARD) {
		// Exclude temporary sandboxes
		unsigned int sandbox_count = GetSandboxCount(editor_state, true);
		unsigned int index = 0;
		for (; index < sandbox_count; index++) {
			EditorSandbox* sandbox = GetSandbox(editor_state, index);
			if (sandbox->world_profiling.HasOption(ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
				// We can use thread_id 0 here since we are on the main thread
				bool was_handled = sandbox->world_profiling.physical_memory_profiler.HandlePageGuardEnter(0, handler_data->exception_information.faulting_page);
				if (was_handled) {
					break;
				}
			}
		}

		if (index == sandbox_count) {
			// It doesn't belong to the physical memory profiler range
			return OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
		}
		else {
			// We can continue the execution
			return OS::ECS_OS_EXCEPTION_CONTINUE_RESOLVED;
		}
	}
	else {
		return OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
	}
}
