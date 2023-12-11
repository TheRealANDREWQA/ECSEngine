#pragma once
#include "ECSEngineAssets.h"
#include "ECSEngineUI.h"

struct EditorState;
ECS_TOOLS;

#define INSPECTOR_ASSET_COPY_BUTTON_LABEL "Copy"

namespace ECSEngine {
	struct AssetDatabase;
}

struct InspectorCopyCurrentAssetSettingData {
	EditorState* editor_state;
	ECSEngine::Stream<wchar_t> path;
	const void* metadata;
	ECSEngine::ECS_ASSET_TYPE asset_type;
	// We need this pointer separately since some assets might use temporary databases
	const ECSEngine::AssetDatabase* database;
	unsigned int inspector_index = -1;
};

// Returns true if it succeeded, else false.
// If the inspector index is different from -1, it will change the inspector
// To target this newly created file
bool InspectorCopyCurrentAssetSetting(InspectorCopyCurrentAssetSettingData* data);

void InspectorCopyCurrentAssetSettingAction(ActionData* action_data);

void InspectorDrawCopyCurrentAssetSetting(UIDrawer* drawer, InspectorCopyCurrentAssetSettingData* data);