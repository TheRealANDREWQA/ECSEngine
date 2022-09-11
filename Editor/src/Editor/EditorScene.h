#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

namespace ECSEngine {
	struct EntityManager;
	struct AssetDatabaseReference;

	namespace Tools {
		struct ActionData;
	}
}

// Returns true if it could create the scene, else false.
// The path should contain the extension
bool CreateEmptyScene(
	const EditorState* editor_state,
	ECSEngine::Stream<wchar_t> path
);

// The path must be relative to the assets folder and with an extension
bool ExistScene(const EditorState* editor_state, ECSEngine::Stream<wchar_t> path);

// Loads only the entity manager + database reference. It does not load the referenced assets.
// If the entity manager deserialization succeeds and the asset database fails, then it will reset the entity manager
bool LoadEditorSceneCore(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename
);

// Writes a combined entity_manager + database reference in a single file
bool SaveEditorScene(
	const EditorState* editor_state,
	const ECSEngine::EntityManager* entity_manager,
	const ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename
);