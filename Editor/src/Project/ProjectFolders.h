#pragma once

struct EditorState;
struct ProjectFile;

constexpr const wchar_t* PROJECT_ASSETS_RELATIVE_PATH = L"Assets";
constexpr const char* PROJECT_ASSETS_RELATIVE_PATH_ASCII = "Assets";
constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH = L"Modules";
constexpr const char* PROJECT_MODULES_RELATIVE_PATH_ASCII = "Modules";
constexpr const wchar_t* PROJECT_DEBUG_RELATIVE_PATH = L"Debug";
constexpr const char* PROJECT_DEBUG_RELATIVE_PATH_ASCII = "Debug";
constexpr const wchar_t* PROJECT_UI_RELATIVE_PATH = L"UI";
constexpr const char* PROJECT_UI_RELATIVE_PATH_ASCII = "UI";

constexpr const wchar_t* PROJECT_METAFILES_RELATIVE_PATH = L"Metafiles";
constexpr const char* PROJECT_METAFILES_RELATIVE_PATH_ASCII = "Metafiles";

constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH_DEBUG = L"Modules\\Debug";
constexpr const char* PROJECT_MODULES_RELATIVE_PATH_ASCII_DEBUG = "Modules\\Debug";

constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH_RELEASE = L"Modules\\Release";
constexpr const char* PROJECT_MODULES_RELATIVE_PATH_ASCII_RELEASE = "Modules\\Release";

constexpr const wchar_t* PROJECT_MODULES_RELATIVE_PATH_DISTRIBUTION = L"Modules\\Distribution";
constexpr const char* PROJECT_MODULES_RELATIVE_PATH_ASCII_DISTRIBUTION = "Modules\\Distribution";

constexpr const wchar_t PROJECT_EXTENSION[] = L".ecsproj";
constexpr const wchar_t* PROJECT_DIRECTORIES[] = {
	PROJECT_DEBUG_RELATIVE_PATH,
	PROJECT_UI_RELATIVE_PATH,
	PROJECT_ASSETS_RELATIVE_PATH,
	PROJECT_MODULES_RELATIVE_PATH,
	PROJECT_METAFILES_RELATIVE_PATH,
	PROJECT_MODULES_RELATIVE_PATH_DEBUG,
	PROJECT_MODULES_RELATIVE_PATH_RELEASE,
	PROJECT_MODULES_RELATIVE_PATH_DISTRIBUTION
};

constexpr const wchar_t* PROJECT_MODULE_RELATIVE_PATHS[] = {
	PROJECT_MODULES_RELATIVE_PATH_DEBUG,
	PROJECT_MODULES_RELATIVE_PATH_RELEASE,
	PROJECT_MODULES_RELATIVE_PATH_DISTRIBUTION
};

// It will make the concatenation between path and project name
void GetProjectFilePath(ECSEngine::containers::CapacityStream<wchar_t>& characters, const ProjectFile* project_file);

void GetProjectDebugFolder(const EditorState* editor_state, ECSEngine::containers::CapacityStream<wchar_t>& path);

void GetProjectAssetsFolder(const EditorState* editor_state, ECSEngine::containers::CapacityStream<wchar_t>& path);

void GetProjectMetafilesFolder(const EditorState* editor_state, ECSEngine::containers::CapacityStream<wchar_t>& path);