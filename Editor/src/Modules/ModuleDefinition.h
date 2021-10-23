#pragma once
#include "ECSEngineMultithreading.h"

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

enum class ModuleConfiguration : unsigned char {
	Debug,
	Release,
	Distribution,
	Count
};

enum class ModuleLoadStatus : unsigned char {
	Failed,
	OutOfDate,
	Good
};

struct Module {
	ECSEngine::containers::Stream<wchar_t> solution_path;
	ECSEngine::containers::Stream<wchar_t> library_name;
	ECSEngine::containers::Stream<ECSEngine::TaskGraphElement> tasks;
	ModuleConfiguration configuration;
	ModuleLoadStatus load_status;
	size_t solution_last_write_time;
	size_t library_last_write_time;
};