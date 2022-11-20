#include "editorpch.h"
#include "AssetTick.h"
#include "../Editor/EditorState.h"
#include "AssetManagement.h"

using namespace ECSEngine;

void TickAsset(EditorState* editor_state) {
	// Every second tick this
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_METADATA_FOR_ASSETS, 1'000)) {
		CreateAssetDefaultSetting(editor_state);
		DeleteMissingAssetSettings(editor_state);
	}
}