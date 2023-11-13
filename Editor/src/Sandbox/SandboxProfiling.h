#pragma once

struct EditorState;

enum EDITOR_SANDBOX_CPU_STATISTICS_TYPE : unsigned char;
enum EDITOR_SANDBOX_GPU_STATISTICS_TYPE : unsigned char;

void ChangeSandboxCPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_CPU_STATISTICS_TYPE type);

void ChangeSandboxGPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_GPU_STATISTICS_TYPE type);

void ClearSandboxCPUProfiler(EditorState* editor_state, unsigned int sandbox_index);

void ClearSandboxGPUProfiler(EditorState* editor_state, unsigned int sandbox_index);

void DisableSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index);

void EnableSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index);

void EndSandboxFrameProfiling(EditorState* editor_state, unsigned int sandbox_index);

void InvertSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index);

// This function should be called after the sandbox allocator and task manager were created
void InitializeSandboxCPUProfiler(EditorState* editor_state, unsigned int sandbox_index);

// This function should be called after the sandbox alloator and task manager were created
// TODO: When the ECSEngine GPUProfiler is implemented, update this
void InitializeSandboxGPUProfiler(EditorState* editor_state, unsigned int sandbox_index);

void ResetSandboxStatistics(EditorState* editor_state, unsigned int sandbox_index);

// Needs to be paired with a EndSandboxFrameProfiling call to correctly deduce the data
void StartSandboxFrameProfiling(EditorState* editor_state, unsigned int sandbox_index);