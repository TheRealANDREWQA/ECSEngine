#pragma once
#include "ECSEngineEntities.h"

struct EditorState;

// It will add an event to load the assets that the prefab has introduced
void LoadPrefabAssets(EditorState* editor_state, unsigned int sandbox_index);

// The entity manager needs to be empty. It will create a single entity inside it
// If it succeeds
// Returns true if it succeeded, else false
bool ReadPrefabFile(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::EntityManager* entity_manager, ECSEngine::Stream<wchar_t> path);

// Uses the entity information from the active entity manager
// Returns true if it succeeded, else false
bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Entity entity, ECSEngine::Stream<wchar_t> path);

// It will save the prefab in the current directory opened in the file explorer with the entity name
// Uses the entity information from the active entity manager
// Returns true if it succeeded, else false
bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Entity entity);