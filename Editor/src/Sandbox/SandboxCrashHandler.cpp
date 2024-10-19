#include "editorpch.h"
#include "SandboxCrashHandler.h"
#include "SandboxAccessor.h"
#include "SandboxModule.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorScene.h"
#include "ECSEngineRuntimeCrash.h"

// Use these to redirect the editor to the crashed folder location
// and open an inspector with the stack trace of the crash
#include "../UI/Inspector.h"
#include "../UI/FileExplorerData.h"

struct PostCrashCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

// This callback is used only for printing any extra information related to the crash
// In the console. The main thread will set the is_crashed flag for the sandbox
static void PostCrashCallback(WorldCrashHandlerPostCallbackFunctionData* function_data) {
	PostCrashCallbackData* data = (PostCrashCallbackData*)function_data->user_data;

	// Print a default error message in the console which indicates the sandbox index
	// Such that we don't have to indicate it in the next message
	ECS_FORMAT_TEMP_STRING(default_message, "The sandbox {#} crashed. {#}", data->sandbox_index, function_data->error_message);
	EditorSetConsoleError(default_message);
	if (!function_data->suspending_threads_success) {
		EditorSetConsoleError("Failed to suspend threads before crash. The stack trace might be inaccurate");
	}

	if (!function_data->resuming_threads_success) {
		EditorSetConsoleError("Failed to resume threads after crash. The editor might be hanged");
	}

	if (!function_data->crash_write_success) {
		EditorSetConsoleError("Failed to write complete crash information");
	}

	if (function_data->save_error_message.size > 0) {
		EditorSetConsoleError(function_data->save_error_message);
	}

	// Change the file explorer to the crash directory
	ChangeFileExplorerDirectory(data->editor_state, function_data->crash_directory);

	// Change an inspector to the stack trace
	ECS_STACK_CAPACITY_STREAM(wchar_t, stack_trace_file_storage, 512);
	Stream<wchar_t> stack_trace_file = RuntimeCrashPersistenceFilePath(function_data->crash_directory, &stack_trace_file_storage, ECS_RUNTIME_CRASH_FILE_STACK_TRACE);
	ChangeInspectorToFile(data->editor_state, stack_trace_file);
}

void SetSandboxCrashHandlerWrappers(EditorState* editor_state, unsigned int sandbox_index)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->sandbox_world.task_manager->SetThreadWorldCrashWrappers();
}

CrashHandler SandboxSetCrashHandler(EditorState* editor_state, unsigned int sandbox_index, AllocatorPolymorphic temporary_allocator)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	World* sandbox_world = &sandbox->sandbox_world;

	ECS_STACK_CAPACITY_STREAM(wchar_t, crash_folder, 512);
	GetProjectCrashFolder(editor_state, crash_folder);

	SetWorldCrashHandlerDescriptor descriptor;
	descriptor.asset_database = editor_state->asset_database;
	descriptor.crash_directory = crash_folder;
	descriptor.module_search_paths = {};
	descriptor.reflection_manager = editor_state->GlobalReflectionManager();
	descriptor.should_break = true;
	descriptor.world = sandbox_world;
	descriptor.world_descriptor = &sandbox->runtime_descriptor;

	PostCrashCallbackData post_callback_data = { editor_state, sandbox_index };
	descriptor.post_callback = { PostCrashCallback, &post_callback_data, sizeof(post_callback_data) };

	ResizableStream<SerializeEntityManagerComponentInfo> unique_infos(temporary_allocator, 32);
	ResizableStream<SerializeEntityManagerSharedComponentInfo> shared_infos(temporary_allocator, 32);
	ResizableStream<SerializeEntityManagerGlobalComponentInfo> global_infos(temporary_allocator, 32);

	CrashHandler previous_handler;
	bool success = GetEditorSceneSerializeOverrides(
		editor_state,
		sandbox_index,
		&unique_infos,
		&shared_infos,
		&global_infos,
		temporary_allocator
	);
	if (!success) {
		ECS_FORMAT_TEMP_STRING(message, "Failed to retrieve sandbox {#} component infos. The crash handler won't be set", sandbox_index);
		EditorSetConsoleError(message);
		previous_handler = EditorGetSimulationThreadsCrashHandler();
	}
	else {
		descriptor.unique_infos = unique_infos;
		descriptor.shared_infos = shared_infos;
		descriptor.global_infos = global_infos;
		descriptor.infos_are_stable = true;
		previous_handler = EditorSetSimulationThreadsCrashHandler(GetContinueWorldCrashHandler(&descriptor));
	}
	return previous_handler;
}

void SandboxRestorePreviousCrashHandler(CrashHandler handler)
{
	EditorSetSimulationThreadsCrashHandler(handler);
}

bool SandboxValidateStateAfterCrash(EditorState* editor_state)
{
	// At the moment, critical corruptions can be verified only on the graphics object
	Graphics* runtime_graphics = editor_state->RuntimeGraphics();
	return runtime_graphics->IsInternalStateValid();
}
