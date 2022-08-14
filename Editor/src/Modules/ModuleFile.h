#pragma once
#include "ECSEngineStream.h"

struct EditorState;

namespace ECSEngine {
	struct World;
}

// Uses the project file to determine the path
bool LoadModuleFile(EditorState* editor_state);

// Uses the project file to determine the path
bool SaveModuleFile(EditorState* editor_state);

void SaveProjectModuleFileThreadTask(unsigned int thread_id, ECSEngine::World* world, void* data);