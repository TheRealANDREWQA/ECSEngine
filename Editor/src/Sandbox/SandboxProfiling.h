#pragma once

struct EditorState;

enum EDITOR_SANDBOX_CPU_STATISTICS_TYPE : unsigned char;
enum EDITOR_SANDBOX_GPU_STATISTICS_TYPE : unsigned char;

void ChangeSandboxCPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_CPU_STATISTICS_TYPE type);

void ChangeSandboxGPUStatisticsType(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_GPU_STATISTICS_TYPE type);

void ClearSandboxProfilers(EditorState* editor_state, unsigned int sandbox_index);

void DisableSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index);

void EnableSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index);

void EndSandboxFrameProfiling(EditorState* editor_state, unsigned int sandbox_index);

void EndSandboxSimulationProfiling(EditorState* editor_state, unsigned int sandbox_index);

void InvertSandboxStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index);

// This function should be called after the sandbox allocator and task manager were created
void InitializeSandboxProfilers(EditorState* editor_state, unsigned int sandbox_index);

size_t ReserveSandboxProfilersAllocatorSize();

void ResetSandboxStatistics(EditorState* editor_state, unsigned int sandbox_index);

// Needs to be paired with a EndSandboxFrameProfiling call to correctly deduce the data
void StartSandboxFrameProfiling(EditorState* editor_state, unsigned int sandbox_index);

void StartSandboxSimulationProfiling(EditorState* editor_state, unsigned int sandbox_index);

// This function will make sure that the underlying engine profiling structure
// Is synchronized with the flags from the editor. Useful after deserialization
// Where we set the statistics types directly and with this function we can synchronize them
void SynchronizeSandboxProfilingWithStatisticTypes(EditorState* editor_state, unsigned int sandbox_index);