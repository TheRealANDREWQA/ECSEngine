// ECS_REFLECT
#pragma once
#include "ECSEngineInternal.h"

struct ECS_REFLECT ProjectFile {
	char version_description[32];
	char platform_description[32];
	size_t version;
	size_t platform;
	ECSEngine::containers::CapacityStream<wchar_t> project_name;
	ECSEngine::containers::CapacityStream<wchar_t> source_dll_name;
	ECSEngine::containers::CapacityStream<wchar_t> path;
};