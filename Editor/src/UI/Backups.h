#pragma once
#include "ECSEngineUI.h"

struct EditorState;

#define BACKUPS_WINDOW_NAME "Backups"

struct EditorState;

void BackupsWindowSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void BackupsDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// Creates the dockspace and the window if it doesn't exist, else it will set it as the active window
void CreateBackupsWindow(EditorState* editor_state);

void CreateBackupsWindowAction(ECSEngine::Tools::ActionData* action_data);

// This is a dialog that ask which parts of the backup to be restored
void CreateRestoreBackupWindow(EditorState* editor_state, ECSEngine::Stream<wchar_t> folder);

struct CreateRestoreBackupWindowActionData {
	EditorState* editor_state;
	ECSEngine::Stream<wchar_t> folder;
};

// Must have as parameter a CreateRestoreBackupWindowActionData*
void CreateRestoreBackupWindowAction(ECSEngine::Tools::ActionData* action_data);