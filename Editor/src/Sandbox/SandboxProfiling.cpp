#include "editorpch.h"
#include "SandboxProfiling.h"
#include "SandboxAccessor.h"
#include "SandboxFile.h"

// More than enough for normal use cases
#define PROFILING_ENTRIES 1024

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

void InvertSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->statistics_display.is_enabled = !sandbox->statistics_display.is_enabled;
	SaveEditorSandboxFile(editor_state);
}

void InitializeSandboxProfilers(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	AllocatorPolymorphic sandbox_allocator = GetAllocatorPolymorphic(sandbox->GlobalMemoryManager());

	WorldProfilingInitializeDescriptor descriptor;
	descriptor.allocator = sandbox_allocator;
	descriptor.entry_capacity = PROFILING_ENTRIES;
	descriptor.init_options = ECS_WORLD_PROFILING_ALL;
	descriptor.starting_options = ECS_WORLD_PROFILING_NONE;
	descriptor.task_manager = sandbox->sandbox_world.task_manager;
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
		sandbox->world_profiling.DisableOption(ECS_WORLD_PROFILING_CPU | ECS_WORLD_PROFILING_ALLOCATOR | ECS_WORLD_PROFILING_PHYSICAL_MEMORY);
	}
	else if (sandbox->cpu_statistics_type == EDITOR_SANDBOX_CPU_STATISTICS_BASIC) {
		sandbox->world_profiling.DisableOption(ECS_WORLD_PROFILING_ALLOCATOR);
		sandbox->world_profiling.EnableOption(ECS_WORLD_PROFILING_CPU | ECS_WORLD_PROFILING_PHYSICAL_MEMORY);
	}
	else if (sandbox->cpu_statistics_type == EDITOR_SANDBOX_CPU_STATISTICS_ADVANCED) {
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
