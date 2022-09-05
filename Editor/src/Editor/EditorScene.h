#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

namespace ECSEngine {
	struct EntityManager;
	struct AssetDatabaseReference;
}

// Writes a combined entity_manager + database reference in a single file
bool SaveEditorScene(
	const EditorState* editor_state,
	const ECSEngine::EntityManager* entity_manager,
	const ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename
);

// Loads only the entity manager + database reference. It does not load the referenced assets.
// If the entity manager deserialization succeeds and the asset database fails, then it will reset the entity manager
bool LoadEditorSceneCore(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename
);