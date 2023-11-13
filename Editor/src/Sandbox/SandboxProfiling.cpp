#include "editorpch.h"
#include "SandboxProfiling.h"
#include "SandboxAccessor.h"
#include "SandboxFile.h"

// More than enough for normal use cases
#define STATISTICS_CAPACITY 1024

void ChangeSandboxCPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_CPU_STATISTICS_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->cpu_statistics_type = type;
	ClearSandboxCPUProfiler(editor_state, sandbox_index);
	SaveEditorSandboxFile(editor_state);
}

void ChangeSandboxGPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_GPU_STATISTICS_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->gpu_statistics_type = type;
	ClearSandboxGPUProfiler(editor_state, sandbox_index);
	SaveEditorSandboxFile(editor_state);
}

void ClearSandboxCPUProfiler(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->cpu_frame_profiler.Clear();
}

void ClearSandboxGPUProfiler(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
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
	if (sandbox->cpu_statistics_type != EDITOR_SANDBOX_CPU_STATISTICS_NONE) {
		sandbox->cpu_frame_profiler.EndFrame();
	}
	if (sandbox->gpu_statistics_type != EDITOR_SANDBOX_GPU_STATISTICS_NONE) {

	}
}

void InvertSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->statistics_display.is_enabled = !sandbox->statistics_display.is_enabled;
	SaveEditorSandboxFile(editor_state);
}

void InitializeSandboxCPUProfiler(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	AllocatorPolymorphic sandbox_allocator = GetAllocatorPolymorphic(sandbox->GlobalMemoryManager());
	sandbox->cpu_frame_profiler.Initialize(sandbox_allocator, sandbox->sandbox_world.task_manager, STATISTICS_CAPACITY);
}

void InitializeSandboxGPUProfiler(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	AllocatorPolymorphic sandbox_allocator = GetAllocatorPolymorphic(sandbox->GlobalMemoryManager());
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
}

void StartSandboxFrameProfiling(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (sandbox->cpu_statistics_type != EDITOR_SANDBOX_CPU_STATISTICS_NONE) {
		ChangeCPUFrameProfilerGlobal(&sandbox->cpu_frame_profiler);
		sandbox->cpu_frame_profiler.StartFrame();
	}
	if (sandbox->gpu_statistics_type != EDITOR_SANDBOX_GPU_STATISTICS_NONE) {

	}
}
