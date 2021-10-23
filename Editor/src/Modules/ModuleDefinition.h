#pragma once
#include "ECSEngineMultithreading.h"

#define MODULE_FUNCTION_NAME "ModuleFunction"
#define MODULE_DRAW_SCENE_NAME "ModuleDrawScene"
constexpr const wchar_t* MODULE_EXTENSION = L".dll";

constexpr const wchar_t* MODULE_ASSOCIATED_FILES[] = {
	L".dll",
	L".pdb",
	L".lib",
	L".exp"
};

namespace ECSEngine {
	struct World;
}

using ModuleFunction = void (*)(ECSEngine::World* world, ECSEngine::containers::Stream<ECSEngine::TaskGraphElement>& module_stream);
using ModuleDrawScene = void (*)(ECSEngine::World* world);

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

struct Module {
	ECSEngine::containers::Stream<wchar_t> solution_path;
	ECSEngine::containers::Stream<wchar_t> library_name;
	ModuleConfiguration configuration;
	size_t last_write_time;
};