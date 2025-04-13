#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

// Opens a source file in a default editing environment (Visual Studio for the moment), the file
// Being identified with the absolute path and for a specific line. You can optionally choose to wait
// For the command to finish, in which case it returns true if the entire command succeeded, including
// The showcase, else false. If not waiting for the command to finish, it returns true if the command
// Was launched else false
bool OpenSourceFileInIDE(const EditorState* editor_state, ECSEngine::Stream<wchar_t> absolute_file_path, unsigned int file_line, bool wait_for_command = false);