#pragma once
#include "editorpch.h"

struct EditorState;
struct ProjectFile;

#define PROJECT_ASSETS_RELATIVE_PATH L"Assets"
#define PROJECT_MODULES_RELATIVE_PATH L"Modules"
#define PROJECT_DEBUG_RELATIVE_PATH L"Debug"
#define PROJECT_UI_RELATIVE_PATH L"UI"
#define PROJECT_METADATA_RELATIVE_PATH L"Metadata"
#define PROJECT_CONFIGURATION_RELATIVE_PATH L"Configuration"
#define PROJECT_BACKUP_RELATIVE_PATH L".backup"
#define PROJECT_CRASH_RELATIVE_PATH L"Assets\\Crash"
#define PROJECT_PREFABS_RELATIVE_PATH L"Assets\\Prefabs"

#define PROJECT_CONFIGURATION_MODULES_RELATIVE_PATH L"Configuration\\Modules"
#define PROJECT_CONFIGURATION_RUNTIME_RELATIVE_PATH L"Configuration\\Runtime"

#define PROJECT_MODULES_RELATIVE_PATH_DEBUG L"Modules\\Debug"
#define PROJECT_MODULES_RELATIVE_PATH_RELEASE L"Modules\\Release"
#define PROJECT_MODULES_RELATIVE_PATH_DISTRIBUTION L"Modules\\Distribution"

#define PROJECT_EXTENSION L".ecsproj"
#define PROJECT_RUNTIME_SETTINGS_FILE_EXTENSION L".config"
#define PROJECT_SANDBOX_FILE_EXTENSION L".ecss"
#define PROJECT_MODULES_FILE_EXTENSION L".ecsmodules"
#define PROJECT_SETTINGS_FILE_EXTENSION L".projsettings"

extern const wchar_t* PROJECT_DIRECTORIES[];

size_t PROJECT_DIRECTORIES_SIZE();

// It will make the concatenation between path and project name
void GetProjectFilePath(const ProjectFile* project_file, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectSettingsFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectModulesFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectSandboxFile(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectDebugFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectUIFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectAssetsFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectMetadataFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectConfigurationFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectModulesFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectConfigurationModuleFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectBackupFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectConfigurationRuntimeFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectCrashFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectPrefabFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectRootPath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

// Returns { nullptr, 0 } if the path is not relative to the assets folder
// It does not replace absolute path separators with relative separators
ECSEngine::Stream<wchar_t> GetProjectAssetRelativePath(const EditorState* editor_state, ECSEngine::Stream<wchar_t> path);

// This function is the same as GetProjectAssetRelativePath, with the difference that it will replace the absolute path separators
// Into relative ones, which requires a storage parameter to be passed in
ECSEngine::Stream<wchar_t> GetProjectAssetRelativePathWithSeparatorReplacement(const EditorState* editor_state, ECSEngine::Stream<wchar_t> path, ECSEngine::CapacityStream<wchar_t>& storage);

// Returns the path from the storage
ECSEngine::Stream<wchar_t> GetProjectPathFromAssetRelative(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& storage, ECSEngine::Stream<wchar_t> relative_path);