// ECS_REFLECT
#pragma once
#include "ECSEngineContainers.h"

struct ECS_REFLECT ProjectFile {
	char version_description[32];
	char platform_description[32];
	size_t version;
	size_t platform;
	ECSEngine::CapacityStream<wchar_t> project_name; ECS_SKIP_REFLECTION()
	ECSEngine::CapacityStream<wchar_t> path; ECS_SKIP_REFLECTION()
};