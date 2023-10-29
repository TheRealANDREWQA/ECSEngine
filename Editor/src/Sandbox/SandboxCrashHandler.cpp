#include "editorpch.h"
#include "SandboxCrashHandler.h"
#include "SandboxAccessor.h"
#include "SandboxModule.h"
#include "../Project/ProjectFolders.h"

CrashHandler SandboxSetCrashHandler(EditorState* editor_state, unsigned int sandbox_index)
{
	CrashHandler current_crash_handler = ECS_GLOBAL_CRASH_HANDLER;

	World* sandbox_world = &GetSandbox(editor_state, sandbox_index)->sandbox_world;
	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, module_paths, 1);
	ECS_STACK_CAPACITY_STREAM(wchar_t, modules_folder, 512);
	GetProjectModulesFilePath(editor_state, modules_folder);
	module_paths.Add(modules_folder);
	SetWorldCrashHandler(sandbox_world, module_paths, true);

	return current_crash_handler;
}

void SandboxRestorePreviousCrashHandler(CrashHandler handler)
{
	SetCrashHandler(handler.function, handler.data);
}
