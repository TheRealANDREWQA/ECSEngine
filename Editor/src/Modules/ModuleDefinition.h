// ECS_REFLECT
#pragma once
#include "ECSEngineMultithreading.h"
#include "ECSEngineModule.h"
#include "ECSEngineReflectionMacros.h"

extern ECSEngine::Stream<wchar_t> MODULE_ASSOCIATED_FILES[];

size_t MODULE_ASSOCIATED_FILES_SIZE();

#define MODULE_CONFIGURATION_DEBUG "Debug"
#define MODULE_CONFIGURATION_RELEASE "Release"
#define MODULE_CONFIGURATION_DISTRIBUTION "Distribution"

extern ECSEngine::Stream<char> MODULE_CONFIGURATIONS[];

#define MODULE_CONFIGURATION_DEBUG_WIDE L"Debug"
#define MODULE_CONFIGURATION_RELEASE_WIDE L"Release"
#define MODULE_CONFIGURATION_DISTRIBUTION_WIDE L"Distribution"

extern ECSEngine::Stream<wchar_t> MODULE_CONFIGURATIONS_WIDE[];

enum ECS_REFLECT EDITOR_MODULE_CONFIGURATION : unsigned char {
	EDITOR_MODULE_CONFIGURATION_DEBUG,
	EDITOR_MODULE_CONFIGURATION_RELEASE,
	EDITOR_MODULE_CONFIGURATION_DISTRIBUTION,
	EDITOR_MODULE_CONFIGURATION_COUNT
};

enum EDITOR_MODULE_LOAD_STATUS : unsigned char {
	EDITOR_MODULE_LOAD_FAILED,
	EDITOR_MODULE_LOAD_OUT_OF_DATE,
	EDITOR_MODULE_LOAD_GOOD
};

// This is a per Configuration basis
struct EditorModuleInfo {
	ECSEngine::AppliedModule ecs_module;

	EDITOR_MODULE_LOAD_STATUS load_status;
	size_t library_last_write_time;
};

struct EditorModuleReflectedSetting {
	void* data;
	ECSEngine::Stream<char> name;
};

struct EditorModule {
	ECSEngine::Stream<wchar_t> solution_path;
	ECSEngine::Stream<wchar_t> library_name;
	size_t solution_last_write_time;
	bool is_graphics_module;

	EditorModuleInfo infos[EDITOR_MODULE_CONFIGURATION_COUNT];
};

struct EditorLinkComponent {
	ECSEngine::Stream<char> name;
	ECSEngine::Stream<char> target;
};

typedef ECSEngine::ResizableStream<EditorModule> ProjectModules;