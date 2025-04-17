#include "editorpch.h"
#include "EditorTextIDE.h"
#include "EditorState.h"

bool OpenSourceFileInIDE(const EditorState* editor_state, Stream<wchar_t> absolute_file_path, unsigned int file_line, bool wait_for_command) {
	return false;
	//ECS_FORMAT_TEMP_STRING_WIDE(command_line, L"/edit \"{#}\" /command \"Edit.GoTo {#}\"", absolute_file_path, file_line);
	//return OS::ShellOpenCommand(editor_state->settings.editing_ide_path, command_line);
	//Optional<OS::ProcessHandle> process_handle = OS::CreateProcessWithHandle(editor_state->settings.editing_ide_path, command_line, {}, true);
	//if (process_handle) {
	//	if (wait_for_command) {
	//		Optional<int> exit_code = process_handle.value.WaitAndGetExitCode();
	//		process_handle.value.Close();
	//		return exit_code.has_value && exit_code.value == 0;
	//	}
	//	else {
	//		process_handle.value.Close();
	//		return true;
	//	}
	//}
	//return false;
}