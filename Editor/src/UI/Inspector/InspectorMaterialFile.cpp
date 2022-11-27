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

struct InspectorDrawMaterialFileData {
	MaterialAsset material_asset;
	Stream<wchar_t> path;
};

void InspectorCleanMaterial(EditorState* editor_state, void* _data) {
	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;
	//AssetSettingsHelperDestroy(editor_state, &data->helper_data);
}

void InspectorDrawMaterialFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ASSET_MATERIAL_ICON, drawer->color_theme.text, drawer->color_theme.theme);

	InspectorIconNameAndPath(drawer, data->path);
	InspectorDrawFileTimes(drawer, data->path);
	InspectorOpenAndShowButton(drawer, data->path);
	drawer->CrossLine();

	// Draw the settings
	UIDrawConfig config;

	drawer->CrossLine();
}

void ChangeInspectorToMaterialFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawMaterialFileData data;
	data.path = path;
	memset(&data.material_asset, 0, sizeof(data.material_asset));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	inspector_index = ChangeInspectorDrawFunctionWithSearch(
		editor_state,
		inspector_index,
		{ InspectorDrawMaterialFile, InspectorCleanMaterial },
		&data,
		sizeof(data) + sizeof(wchar_t) * (path.size + 1),
		-1,
		[=](void* inspector_data) {
			InspectorDrawMaterialFileData* other_data = (InspectorDrawMaterialFileData*)inspector_data;
			return function::CompareStrings(other_data->path, path);
		}
	);

	if (inspector_index != -1) {
		// Get the data and set the path
		InspectorDrawMaterialFileData* draw_data = (InspectorDrawMaterialFileData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
		draw_data->path = { function::OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->path.Copy(path);
	}
}

void InspectorMaterialFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_MATERIAL_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawMaterialFile, InspectorCleanMaterial }, ASSET_MATERIAL_EXTENSIONS[index]);
	}
}