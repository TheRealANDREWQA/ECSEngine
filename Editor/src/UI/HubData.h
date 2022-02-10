#pragma once
#include "../Project/ProjectFile.h"
#include "editorpch.h"


struct HubProject {
	const wchar_t* path;
	ProjectFile data;
	const char* error_message = nullptr;
};

struct HubData {
	ECSEngine::containers::CapacityStream<HubProject> projects;
	ECSEngine::Tools::UIReflectionDrawer* ui_reflection;
};