#pragma once
#include "ECSEngineContainers.h"

struct EditorState;

namespace ECSEngine {
	struct EntityManager;
	struct AssetDatabaseReference;
}

bool SaveEditorScene(
	const EditorState* editor_state,
	const ECSEngine::EntityManager* entity_manager,
	const ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename
);

bool LoadEditorScene(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename
);