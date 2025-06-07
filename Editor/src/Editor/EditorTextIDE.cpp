#include "editorpch.h"
#include "EditorTextIDE.h"
#include "EditorState.h"

#define OPEN_VISUAL_STUDIO_EXECUTABLE_NAME L"VisualStudioFileOpenTool.exe"
#define OPEN_VISUAL_STUDIO_EXECUTABLE_PATH L"Resources/OpenInIDE/" OPEN_VISUAL_STUDIO_EXECUTABLE_NAME

bool OpenSourceFileInIDE(const EditorState* editor_state, Stream<wchar_t> absolute_file_path, unsigned int file_line, bool wait_for_command) {
	// At the moment, we are using a special executable for Visual Studio because the devenv command does not work
	// As needed - it only opens the file, without going to the specified file.

	// At the moment, assume Visual Studio version 2022 - this is what the 18 signifies.
	ECS_FORMAT_TEMP_STRING_WIDE(command_line, L"{#} 18 \"{#}\" {#}", OPEN_VISUAL_STUDIO_EXECUTABLE_NAME, absolute_file_path, file_line);
	OS::CreateProcessOptions create_options;
	create_options.show_cmd_window = false;
	Optional<OS::ProcessHandle> process_handle = OS::CreateProcessWithHandle(OPEN_VISUAL_STUDIO_EXECUTABLE_PATH, command_line, create_options);
	if (process_handle) {
		if (wait_for_command) {
			Optional<int> exit_code = process_handle.value.WaitAndGetExitCode();
			process_handle.value.Close();
			return exit_code.has_value && exit_code.value == 0;
		}
		else {
			process_handle.value.Close();
			return true;
		}
	}
	return false;
}