#pragma once
#include "ECSEngineUI.h"
#include "ECSEngineAssets.h"

using namespace ECSEngine;

struct AssetSettingsHelperData {
	inline Stream<char> SelectedName() const {
		return current_names.size > 0 ? current_names[selected_setting] : Stream<char>(nullptr, 0);
	}

	unsigned char selected_setting;
	bool is_new_name;
	LinearAllocator temp_allocator;
	// The current name that is being selected
	Stream<Stream<char>> current_names;
	CapacityStream<char> new_name;
	Timer lazy_timer;
	void* metadata;
};

struct EditorState;

// Draws the necessary part of the selection of the current selected setting (where needed like mesh and texture)
bool AssetSettingsHelper(UIDrawer* drawer, EditorState* editor_state, AssetSettingsHelperData* helper_data, Stream<wchar_t> file, ECS_ASSET_TYPE asset_type);

void AssetSettingsHelperDestroy(EditorState* editor_state, AssetSettingsHelperData* helper_data);

struct AssetSettingsHelperChangedBaseActionData {
	EditorState* editor_state;
	Stream<wchar_t> file;
	AssetSettingsHelperData* helper_data;
	ECS_ASSET_TYPE asset_type;
};

void AssetSettingsHelperChangedBaseAction(ActionData* action_data);

size_t AssetSettingsHelperBaseConfiguration();

void AssetSettingsHelperBaseConfig(UIDrawConfig* draw_config);