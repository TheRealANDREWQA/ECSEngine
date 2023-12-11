#include "editorpch.h"
#include "InspectorAssetUtilities.h"
#include "../../Project/ProjectFolders.h"
#include "../../Editor/EditorState.h"
#include "../Inspector.h"
#include "../InspectorData.h"

// This is the copy index at which we stop
// If we keep finding existing copies
#define MAX_SEARCH_INDEX 100

// Returns empty if a copy name could not be found
static Stream<wchar_t> FindCopyName(Stream<wchar_t> path, CapacityStream<wchar_t>& storage) {
	Stream<wchar_t> extension = PathExtension(path);
	Stream<wchar_t> path_no_extension = PathNoExtension(path);
	storage.CopyOther(path_no_extension);
	storage.Add(L' ');
	storage.Add(L'(');
	unsigned int initial_storage_size = storage.size;
	for (size_t index = 0; index < MAX_SEARCH_INDEX; index++) {
		storage.size = initial_storage_size;
		ConvertIntToChars(storage, index);
		storage.Add(L')');
		storage.AddStreamAssert(extension);

		if (!ExistsFileOrFolder(storage)) {
			return storage;
		}
	}

	return {};
}

bool InspectorCopyCurrentAssetSetting(InspectorCopyCurrentAssetSettingData* data)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, copy_path_storage, 512);
	Stream<wchar_t> copy_path = FindCopyName(data->path, copy_path_storage);
	if (copy_path.size == 0) {
		return false;
	}

	// Try to copy the file to that name
	bool success = FileCopy(data->path, copy_path, false);
	if (!success) {
		return false;
	}

	// We need to copy the metadata file as well
	ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_file, 512);
	Stream<char> asset_name = GetAssetName(data->metadata, data->asset_type);
	Stream<wchar_t> asset_file = GetAssetFile(data->metadata, data->asset_type);
	data->editor_state->asset_database->FileLocationAsset(asset_name, asset_file, metadata_file, data->asset_type);

	// For all assets, the path represents their actual name
	// The target file is difference
	Stream<wchar_t> relative_copy_path = GetProjectAssetRelativePath(data->editor_state, copy_path);
	ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_copy_file, 512);
	ECS_STACK_CAPACITY_STREAM(char, ascii_path, 512);
	ConvertWideCharsToASCII(relative_copy_path, ascii_path);
	data->editor_state->asset_database->FileLocationAsset(ascii_path, asset_file, metadata_copy_file, data->asset_type);

	success = FileCopy(metadata_file, metadata_copy_file, false);
	if (success) {
		if (data->inspector_index != -1) {
			ChangeInspectorToAsset(data->editor_state, copy_path, {}, data->asset_type, data->inspector_index);
		}
	}
	return success;
}

void InspectorCopyCurrentAssetSettingAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	InspectorCopyCurrentAssetSettingData* data = (InspectorCopyCurrentAssetSettingData*)_data;
	bool success = InspectorCopyCurrentAssetSetting(data);
	if (!success) {
		Stream<wchar_t> relative_path = GetProjectAssetRelativePath(data->editor_state, data->path);
		ECS_FORMAT_TEMP_STRING(message, "Failed to create copy for asset {#}", relative_path);
		EditorSetConsoleError(message);
	}
}

void InspectorDrawCopyCurrentAssetSetting(UIDrawer* drawer, InspectorCopyCurrentAssetSettingData* data) {
	UIDrawConfig config;
	UIConfigAlignElement align_element;
	align_element.horizontal = ECS_UI_ALIGN_MIDDLE;
	config.AddFlag(align_element);

	drawer->Button(UI_CONFIG_ALIGN_ELEMENT, config, INSPECTOR_ASSET_COPY_BUTTON_LABEL, { InspectorCopyCurrentAssetSettingAction, data, sizeof(*data) });
	drawer->NextRow();
}