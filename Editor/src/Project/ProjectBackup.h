#pragma once

struct EditorState;

enum ProjectBackupFiles : unsigned char {
	PROJECT_BACKUP_UI,
	PROJECT_BACKUP_PROJECT,
	PROJECT_BACKUP_MODULES,
	PROJECT_BACKUP_SANDBOX_FILE,
	PROJECT_BACKUP_CONFIGURATION,
	PROJECT_BACKUP_ASSETS_METADATA,
	PROJECT_BACKUP_SCENE,
	PROJECT_BACKUP_COUNT
};

extern ECSEngine::Stream<char> PROJECT_BACKUP_FILE_NAMES[];

bool ProjectNeedsBackup(EditorState* editor_state);

void ResetProjectNeedsBackup(EditorState* editor_state);

// Delays the backup time by the given amount
void AddProjectNeedsBackupTime(EditorState* editor_state, size_t second_count);

// Returns whether or not it succeeded in copying the files from the backup folder into the current directory
// The folder path must be fully qualified (absolute).
bool LoadProjectBackup(const EditorState* editor_state, ECSEngine::Stream<wchar_t> folder);

// Returns whether or not it succeeded in copying the files from the backup folder into the current directory
// The folder path must be fully qualified (absolute). The file mask tells which parts to be recovered
bool LoadProjectBackup(const EditorState* editor_state, ECSEngine::Stream<wchar_t> folder, ECSEngine::Stream<ProjectBackupFiles> file_mask);

// Returns whether or not it succeeded in saving the project's file
bool SaveProjectBackup(const EditorState* editor_state);