#pragma once
#include "ECSEngineAssets.h"
#include "ECSEngineUI.h"

#define ASSET_UI_OVERRIDE_COUNT ECS_ASSET_TYPE_COUNT

struct EditorState;

// There must be ASSET_UI_OVERRIDE_COUNT overrides. The allocator needs to be a temporary allocator
void GetEntityComponentUIOverrides(EditorState* editor_state, ECSEngine::Tools::UIReflectionFieldOverride* overrides, ECSEngine::AllocatorPolymorphic allocator);

struct AssetOverrideSetSandboxIndexData {
	unsigned int sandbox_index;
};

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideSetSandboxIndex);