#pragma once
#include "ECSEngineStream.h"

struct EditorState;

namespace ECSEngine {
	struct World;
}

constexpr const wchar_t* PROJECT_MODULE_EXTENSION = L".ecsmodules";

void GetProjectModuleFilePath(const EditorState* editor_state, ECSEngine::CapacityStream<wchar_t>& path);

// Uses the project file to determine the path
bool LoadModuleFile(EditorState* editor_state);

// Uses the project file to determine the path
bool SaveModuleFile(EditorState* editor_state);

void SaveProjectModuleFileThreadTask(unsigned int thread_id, ECSEngine::World* world, void* data);