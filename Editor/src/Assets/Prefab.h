#pragma once
#include "ECSEngineEntities.h"

struct EditorState;

// Returns true if it succeeded, else false
bool ReadPrefabFile(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Entity entity, ECSEngine::Stream<wchar_t> path);

// Returns true if it succeeded, else false
bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Entity entity, ECSEngine::Stream<wchar_t> path);

// It will save the prefab in the current directory opened in the file explorer with the entity name
// Returns true if it succeeded, else false
bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Entity entity);