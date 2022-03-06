#pragma once
#include "editorpch.h"

struct EditorState;
struct ProjectFile;

constexpr const wchar_t* PROJECT_ASSETS_RELATIVE_PATH = L"Assets";
constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH = L"Modules";
constexpr const wchar_t* PROJECT_DEBUG_RELATIVE_PATH = L"Debug";
constexpr const wchar_t* PROJECT_UI_RELATIVE_PATH = L"UI";
constexpr const wchar_t* PROJECT_METAFILES_RELATIVE_PATH = L"Metafiles";
constexpr const wchar_t* PROJECT_CONFIGURATION_RELATIVE_PATH = L"Configuration";
constexpr const wchar_t* PROJECT_BACKUP_RELATIVE_PATH = L".backup";

constexpr const wchar_t* PROJECT_CONFIGURATION_MODULES_RELATIVE_PATH = L"Configuration\\Modules";

constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH_DEBUG = L"Modules\\Debug";
constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH_RELEASE = L"Modules\\Release";
constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH_DISTRIBUTION = L"Modules\\Distribution";

constexpr const wchar_t PROJECT_EXTENSION[] = L".ecsproj";
constexpr const wchar_t* PROJECT_DIRECTORIES[] = {
	PROJECT_DEBUG_RELATIVE_PATH,
	PROJECT_UI_RELATIVE_PATH,
	PROJECT_ASSETS_RELATIVE_PATH,
	PROJECT_MODULES_RELATIVE_PATH,
	PROJECT_METAFILES_RELATIVE_PATH,
	PROJECT_MODULES_RELATIVE_PATH_DEBUG,
	PROJECT_MODULES_RELATIVE_PATH_RELEASE,
	PROJECT_MODULES_RELATIVE_PATH_DISTRIBUTION,
	PROJECT_CONFIGURATION_RELATIVE_PATH,
	PROJECT_CONFIGURATION_MODULES_RELATIVE_PATH,
	PROJECT_BACKUP_RELATIVE_PATH
};

constexpr const wchar_t* PROJECT_MODULE_RELATIVE_PATHS[] = {
	PROJECT_MODULES_RELATIVE_PATH_DEBUG,
	PROJECT_MODULES_RELATIVE_PATH_RELEASE,
	PROJECT_MODULES_RELATIVE_PATH_DISTRIBUTION
};

// It will make the concatenation between path and project name
void GetProjectFilePath(ECSEngine::CapacityStream<wchar_t>& characters, const ProjectFile* project_file);

void GetProjectDebugFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectUIFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectAssetsFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectMetafilesFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectConfigurationFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectSettingsPath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectConfigurationModuleFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

void GetProjectBackupFolder(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);