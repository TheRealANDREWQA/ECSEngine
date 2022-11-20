#pragma once
#include "ECSEngineAssets.h"
#include "ECSEngineUI.h"

inline size_t AssetUIOverrideCount() {
	return ECSEngine::ECS_ASSET_METADATA_MACROS_SIZE();
}

struct EditorState;

// There must be GetAssetUIOverrideCount() overrides. The allocator needs to be a temporary allocator
void GetEntityComponentUIOverrides(EditorState* editor_state, ECSEngine::Tools::UIReflectionFieldOverride* overrides, ECSEngine::AllocatorPolymorphic allocator);

struct AssetOverrideSetSandboxIndexData {
	unsigned int sandbox_index;
};

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideSetSandboxIndex);

struct AssetOverrideBindCallbackData {
	ECSEngine::Tools::UIActionHandler handler;
};

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideBindCallback);

struct AssetOverrideSetAllData {
	AssetOverrideSetSandboxIndexData set_index;
	AssetOverrideBindCallbackData callback;
};

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideSetAll);

void AssetOverrideBindInstanceOverrides(
	ECSEngine::Tools::UIReflectionDrawer* drawer,
	ECSEngine::Tools::UIReflectionInstance* instance,
	unsigned int sandbox_index,
	ECSEngine::Tools::UIActionHandler modify_action_handler
);
