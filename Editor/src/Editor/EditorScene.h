#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

namespace ECSEngine {
	struct EntityManager;
	struct AssetDatabaseReference;
	struct AssetDatabaseReferencePointerRemap;
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
bool LoadEditorSceneCore(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename,
	ECSEngine::CapacityStream<ECSEngine::AssetDatabaseReferencePointerRemap>* pointer_remap = nullptr
);

// It does not unload or clear the scene before doing this. 
// Loads only the entity manager + database reference. It does not load the referenced assets. (It will remap
// the asset handles tho)
bool LoadEditorSceneCore(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<wchar_t> filename
);

// Writes a combined entity_manager + database reference in a single file
// The reason the entity manager is not const is because it will remove any empty
// archetypes as well
bool SaveEditorScene(
	const EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	const ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename
);

// Saves the normal editor scene
bool SaveEditorScene(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<wchar_t> filename
);

// Saves the runtime scene instead of the normal editor scene
bool SaveEditorSceneRuntime(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<wchar_t> filename
);