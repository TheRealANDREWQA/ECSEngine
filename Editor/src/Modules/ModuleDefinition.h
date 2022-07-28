#pragma once
#include "ECSEngineMultithreading.h"
#include "ECSEngineModule.h"

constexpr const wchar_t* MODULE_ASSOCIATED_FILES[] = {
	L".dll",
	L".pdb",
	L".lib",
	L".exp"
};

constexpr const char* MODULE_CONFIGURATION_DEBUG = "Debug";
constexpr const char* MODULE_CONFIGURATION_RELEASE = "Release";
constexpr const char* MODULE_CONFIGURATION_DISTRIBUTION = "Distribution";

constexpr const char* MODULE_CONFIGURATIONS[] = {
	MODULE_CONFIGURATION_DEBUG,
	MODULE_CONFIGURATION_RELEASE,
	MODULE_CONFIGURATION_DISTRIBUTION
};

constexpr const wchar_t* MODULE_CONFIGURATION_DEBUG_WIDE = L"Debug";
constexpr const wchar_t* MODULE_CONFIGURATION_RELEASE_WIDE = L"Release";
constexpr const wchar_t* MODULE_CONFIGURATION_DISTRIBUTION_WIDE = L"Distribution";

constexpr const wchar_t* MODULE_CONFIGURATIONS_WIDE[] = {
	MODULE_CONFIGURATION_DEBUG_WIDE,
	MODULE_CONFIGURATION_RELEASE_WIDE,
	MODULE_CONFIGURATION_DISTRIBUTION_WIDE
};

enum EDITOR_MODULE_CONFIGURATION : unsigned char {
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
	ECSEngine::Module ecs_module;

	// The streams for the module
	ECSEngine::Stream<ECSEngine::TaskSchedulerElement> tasks;
	ECSEngine::Stream<ECSEngine::Tools::UIWindowDescriptor> ui_descriptors;
	ECSEngine::Stream<ECSEngine::ModuleBuildAssetType> build_asset_types;
	ECSEngine::Stream<ECSEngine::ModuleLinkComponentTarget> link_components;
	ECSEngine::ModuleSerializeComponentStreams serialize_streams;

	// The components of the module
	ECSEngine::Stream<ECSEngine::Stream<char>> component_names;
	ECSEngine::Stream<ECSEngine::Stream<char>> shared_component_names;

	EDITOR_MODULE_LOAD_STATUS load_status;
	size_t library_last_write_time;
};

struct EditorModuleReflectedSetting {
	void* data;
	const char* name;
};

struct EditorModule {
	ECSEngine::Stream<wchar_t> solution_path;
	ECSEngine::Stream<wchar_t> library_name;
	size_t solution_last_write_time;

	EditorModuleInfo infos[EDITOR_MODULE_CONFIGURATION_COUNT];
};

using ProjectModules = ECSEngine::ResizableStream<EditorModule>;