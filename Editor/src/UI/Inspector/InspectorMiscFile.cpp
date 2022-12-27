#include "editorpch.h"
#include "InspectorMiscFile.h"
#include "../Inspector.h"
#include "InspectorUtilities.h"
#include "../../Editor/EditorState.h"

#include "../../Assets/AssetManagement.h"
#include "../../Assets/AssetExtensions.h"
#include "../AssetIcons.h"

using namespace ECSEngine;
ECS_TOOLS;

struct InspectorDrawMiscFileData {
	MiscAsset asset;
};

void InspectorCleanMisc(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawMiscFileData* data = (InspectorDrawMiscFileData*)_data;
	//AssetSettingsHelperDestroy(editor_state, &data->helper_data);
}

void InspectorDrawMiscFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawMiscFileData* data = (InspectorDrawMiscFileData*)_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, path_storage, 512);
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	Stream<wchar_t> path = function::MountPathOnlyRel(data->asset.file, assets_folder, path);
	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ASSET_MISC_ICON, drawer->color_theme.text, drawer->color_theme.theme);

	InspectorIconNameAndPath(drawer, path);
	InspectorDrawFileTimes(drawer, path);
	InspectorOpenAndShowButton(drawer, path);
	drawer->CrossLine();

	// Draw the settings
	UIDrawConfig config;

	drawer->CrossLine();
}

void ChangeInspectorToMiscFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawMiscFileData data;
	memset(&data.asset, 0, sizeof(data.asset));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	inspector_index = ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawMiscFile, InspectorCleanMisc },
		&data,
		sizeof(data) + sizeof(wchar_t) * (path.size + 1)
	);

	if (inspector_index != -1) {
		// Get the data and set the path
		InspectorDrawMiscFileData* draw_data = (InspectorDrawMiscFileData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
		draw_data->asset.file = { function::OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->asset.file.Copy(path);
	}
}

void InspectorMiscFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_MISC_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawMiscFile, InspectorCleanMisc }, ASSET_MISC_EXTENSIONS[index]);
	}
}