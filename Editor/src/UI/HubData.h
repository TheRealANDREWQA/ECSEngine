#pragma once
#include "../Project/ProjectFile.h"
#include "editorpch.h"

struct HubProject {
	ECSEngine::Stream<char> error_message = { nullptr, 0 };
	ProjectFile data;
};

struct HubData {
	ECSEngine::CapacityStream<HubProject> projects;
	ECSEngine::AllocatorPolymorphic allocator;
};