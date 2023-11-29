#pragma once
#include "ECSEngineEntities.h"

namespace ECSEngine {
	struct AssetDatabaseReference;
}

struct EditorState;

#define PREFAB_FILE_EXTENSION L".prefab"

// You can optionally retrieve the created entity by giving a pointer to it
// Returns true if it succeeded, else false
bool AddPrefabToSandbox(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> path, ECSEngine::Entity* created_entity = nullptr);

// It will add an event to load the assets that the prefab has introduced
void LoadPrefabAssets(EditorState* editor_state, unsigned int sandbox_index);

// The entity manager needs to be empty. It will create a single entity inside it
// If it succeeds. You can optionally retrieve it by giving a pointer to an entity
// Returns true if it succeeded, else false
bool ReadPrefabFile(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::AssetDatabaseReference* database_reference,
	ECSEngine::Stream<wchar_t> path,
	ECSEngine::Entity* created_entity = nullptr
);

// The entity manager needs to be empty. It will create a single entity inside it
// If it succeeds. You can optionally retrieve it by giving a pointer to an entity
// Returns true if it succeeded, else false
bool ReadPrefabFile(
	EditorState* editor_state, 
	unsigned int sandbox_index,
	ECSEngine::Stream<wchar_t> path, 
	ECSEngine::Entity* created_entity = nullptr
);

// Uses the entity information from the active entity manager
// Returns true if it succeeded, else false
bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Entity entity, ECSEngine::Stream<wchar_t> path);

// It will save the prefab in the current directory opened in the file explorer with the entity name
// Uses the entity information from the active entity manager
// Returns true if it succeeded, else false
bool SavePrefabFile(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Entity entity);