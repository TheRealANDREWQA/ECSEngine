#pragma once
#include "ECSEngineMultithreading.h"
#include "ECSEngineModule.h"

constexpr const wchar_t* MODULE_ASSOCIATED_FILES[] = {
	L".dll",
	L".pdb",
	L".lib",
	L".exp"
};

namespace ECSEngine {
	struct World;
}

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

enum class EditorModuleConfiguration : unsigned char {
	Debug,
	Release,
	Distribution,
	Count
};

enum class EditorModuleLoadStatus : unsigned char {
	Failed,
	OutOfDate,
	Good
};

struct EditorModule {
	ECSEngine::containers::Stream<wchar_t> solution_path;
	ECSEngine::containers::Stream<wchar_t> library_name;
	ECSEngine::Module ecs_module;
	EditorModuleConfiguration configuration;
	EditorModuleLoadStatus load_status;
	size_t solution_last_write_time;
	size_t library_last_write_time;
};