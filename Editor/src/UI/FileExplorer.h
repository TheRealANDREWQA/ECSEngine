#pragma once
#include "ECSEngineUI.h"

#define FILE_EXPLORER_WINDOW_NAME "File Explorer"

struct EditorState;

// Stack memory size should be at least 512
void FileExplorerSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void FileExplorerDraw(void* window_data, void* drawer_descriptor, bool initialize);

void CreateFileExplorer(EditorState* editor_state);

unsigned int CreateFileExplorerWindow(EditorState* editor_state);

void CreateFileExplorerAction(ECSEngine::Tools::ActionData* action_data);

// Index is used to reset the shift indices, can be omitted by external setters
void ChangeFileExplorerDirectory(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int index = -1);

// Index is used to reset the shift indices, can be omitted by external setters
void ChangeFileExplorerFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int index = -1);

void FileExplorerTick(EditorState* editor_state);