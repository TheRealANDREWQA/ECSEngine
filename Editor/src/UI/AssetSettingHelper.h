#pragma once
#include "ECSEngineUI.h"
#include "ECSEngineAssets.h"

using namespace ECSEngine;

#define ASSET_NAME_PADDING 0.2f

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
bool AssetSettingsHelper(UIDrawer* drawer, EditorState* editor_state, AssetSettingsHelperData* helper_data, ECS_ASSET_TYPE asset_type);

void AssetSettingsHelperDestroy(EditorState* editor_state, AssetSettingsHelperData* helper_data);

struct AssetSettingsHelperChangedActionData {
	EditorState* editor_state;
	AssetDatabase* target_database;
	AssetSettingsHelperData* helper_data;
	ECS_ASSET_TYPE asset_type;
};

void AssetSettingsHelperChangedAction(ActionData* action_data);

// If the thunk file path is specified, then it will also update the thunk file path
// with the new file path
struct AssetSettingsHelperChangedWithFileActionData {
	EditorState* editor_state;
	AssetDatabase* target_database;
	Stream<wchar_t> previous_target_file;
	Stream<char> previous_name;
	ECS_ASSET_TYPE asset_type;
	const void* asset;
	Stream<wchar_t> thunk_file_path = { nullptr, 0 };
};

void AssetSettingsHelperChangedWithFileAction(ActionData* action_data);

struct AssetSettingsHelperChangedNoFileActionData {
	EditorState* editor_state;
	AssetDatabase* target_database;
	ECS_ASSET_TYPE asset_type;
	const void* asset;
};

void AssetSettingsHelperChangedNoFileAction(ActionData* action_data);

size_t AssetSettingsHelperBaseConfiguration();

void AssetSettingsHelperBaseConfig(UIDrawConfig* draw_config);

// Sets the pointer of the metadata to the one found in the asset database, if the asset is loaded
void AssetSettingsExtractPointerFromMainDatabase(const EditorState* editor_state, void* metadata, ECS_ASSET_TYPE type);

void AssetSettingsIsReferencedUIStatus(Tools::UIDrawer* drawer, const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);