#include "editorpch.h"
#include "SandboxProfiling.h"
#include "SandboxAccessor.h"

void ResetSandboxSystemStatisticsDisplay(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	memset(&sandbox->system_statistics_display, 0, sizeof(sandbox->system_statistics_display));
}

void ResetSandboxSystemStatistics(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	memset(&sandbox->system_statistics, 0, sizeof(sandbox->system_statistics));
}