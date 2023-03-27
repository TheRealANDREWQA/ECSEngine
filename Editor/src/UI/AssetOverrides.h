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

struct AssetOverrideCallbackVerifyData {
	ECSEngine::Stream<char> name;
	ECSEngine::Stream<wchar_t> file;
	EditorState* editor_state;
	bool prevent_registering;
};

struct AssetOverrideCallbackAdditionalInfo {
	unsigned int handle;
	ECSEngine::ECS_ASSET_TYPE type;
	bool success;
	bool is_selection;

	// This is used only for deselection
	// This will be the asset with all of its values
	// before removal - since the asset can be removed from
	// the database
	void* previous_asset;
};

// Can optionally mark the callback as a verify callback that will receive
// in the additional data field a AssetOverrideCallbackVerifyData* which
// it can use to prevent the registering/unregistering part of the.
// If the normal callback is called, then it receives in the additional_data field
// a structure of type
// For deselection, if you wish the callback to be called after the handle has been made -1
// but the handle will be the value before being deallocated
// then clear callback_before_handle_update to false
struct AssetOverrideBindCallbackData {
	ECSEngine::Tools::UIActionHandler handler;
	bool verify_handler = false;
	bool callback_before_handle_update = true;
};

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideBindCallback);

struct AssetOverrideBindNewDatabaseData {
	ECSEngine::AssetDatabase* database;
};

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideBindNewDatabase);

struct AssetOverrideSetAllData {
	AssetOverrideSetSandboxIndexData set_index;
	AssetOverrideBindCallbackData callback;
	AssetOverrideBindNewDatabaseData new_database = { nullptr };
};

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideSetAll);

void AssetOverrideBindInstanceOverrides(
	ECSEngine::Tools::UIReflectionDrawer* drawer,
	ECSEngine::Tools::UIReflectionInstance* instance,
	unsigned int sandbox_index,
	ECSEngine::Tools::UIActionHandler modify_action_handler
);