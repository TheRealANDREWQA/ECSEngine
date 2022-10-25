#include "editorpch.h"
#include "../Inspector.h"
#include "ECSEngineUI.h"
#include "ECSEngineRendering.h"
#include "../FileExplorerData.h"
#include "Editor/EditorState.h"
#include "InspectorUtilities.h"

#include "../../Project/ProjectFolders.h"
#include "../AssetSettingHelper.h"

#include "../../Assets/AssetExtensions.h"

using namespace ECSEngine;
ECS_TOOLS;

#define MESH_PREVIEW_HEIGHT 0.4f
#define MESH_PREVIEW_ROTATION_DELTA_FACTOR 125.0f
#define MESH_PREVIEW_RADIUS_DELTA_FACTOR 0.00125f


struct InspectorDrawMeshFileData {
	Stream<wchar_t> path;
	GLTFThumbnail thumbnail;
	CoallescedMesh* mesh;
	float radius_delta;
	float2 rotation_delta;

	MeshMetadata current_metadata;
	AssetSettingsHelperData helper_data;
};

void InspectorCleanMeshFile(EditorState* editor_state, void* _data) {
	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;

	// Unload the mesh and free the resources only if the mesh was previously loaded
	FileExplorerMeshThumbnail file_explorer_thumbnail;
	if (editor_state->file_explorer_data->mesh_thumbnails.TryGetValue(ResourceIdentifier(data->path.buffer, data->path.size), file_explorer_thumbnail)) {
		if (file_explorer_thumbnail.could_be_read) {
			editor_state->ui_resource_manager->UnloadCoallescedMeshImplementation(data->mesh);
			// Release the texture aswell
			Graphics* graphics = editor_state->UIGraphics();
			Texture2D texture = data->thumbnail.texture.GetResource();
			graphics->FreeResource(texture);
			graphics->FreeResource(data->thumbnail.texture);
		}
	}

	AssetSettingsHelperDestroy(editor_state, &data->helper_data);
}

void InspectorMeshPreviewClickable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;
	if (mouse_tracker->LeftButton() == MBPRESSED) {
		// Active the raw input
		mouse->EnableRawInput();
	}
	else if (mouse_tracker->LeftButton() == MBHELD) {
		data->rotation_delta.x += MESH_PREVIEW_ROTATION_DELTA_FACTOR * mouse_delta.y;
		data->rotation_delta.y += MESH_PREVIEW_ROTATION_DELTA_FACTOR * mouse_delta.x;
	}
	else if (mouse_tracker->LeftButton() == MBRELEASED) {
		// Disable the raw input
		mouse->DisableRawInput();
	}
}

void InspectorMeshPreviewHoverable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;
	data->radius_delta -= mouse->GetScrollDelta() * MESH_PREVIEW_RADIUS_DELTA_FACTOR;
	PinWindowVerticalSliderPosition(system, window_index);
}

void InspectorDrawMeshFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	// If the thumbnail was not generated, use the generic mesh icon
	FileExplorerMeshThumbnail mesh_thumbnail;
	// Initialize this to false in order to avoid doing the preview if the resource cannot be located
	mesh_thumbnail.could_be_read = false;
	if (editor_state->file_explorer_data->mesh_thumbnails.TryGetValue(ResourceIdentifier(data->path.buffer, data->path.size * sizeof(wchar_t)), mesh_thumbnail)
		&& mesh_thumbnail.could_be_read) {
		// Use the thumbnail for the icon draw
		InspectorIconTexture(drawer, mesh_thumbnail.texture);
	}
	else {
		InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_MESH, drawer->color_theme.text, drawer->color_theme.theme);
	}

	if (data->current_metadata.file.size == 0) {
		// Retrieve the target file
		Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, data->path);
		ECS_ASSERT(relative_path.size > 0);
		data->current_metadata.file = relative_path;
	}

	InspectorIconNameAndPath(drawer, data->path);
	InspectorDrawFileTimes(drawer, data->path);
	InspectorOpenAndShowButton(drawer, data->path);
	drawer->CrossLine();

	// Convert the absolute separator into relative
	function::ReplaceCharacter(data->current_metadata.file, ECS_OS_PATH_SEPARATOR, ECS_OS_PATH_SEPARATOR_REL);

	data->helper_data.metadata = &data->current_metadata;
	data->current_metadata.name = data->helper_data.SelectedName();
	bool has_path = AssetSettingsHelper(drawer, editor_state, &data->helper_data, ECS_ASSET_MESH);

	// Draw the settings, if there is one
	if (has_path) {
		UIDrawConfig config;
		AssetSettingsHelperBaseConfig(&config);
		size_t base_configuration = AssetSettingsHelperBaseConfiguration();

		AssetSettingsHelperChangedActionData changed_base_data;
		changed_base_data.editor_state = editor_state;
		changed_base_data.asset_type = ECS_ASSET_MESH;
		changed_base_data.helper_data = &data->helper_data;

		UIConfigTextInputCallback float_input_callback;
		float_input_callback.handler = { AssetSettingsHelperChangedAction, &changed_base_data, sizeof(changed_base_data) };
		float_input_callback.trigger_only_on_release = true;

		config.AddFlag(float_input_callback);

		// Display the values
		drawer->FloatInput(base_configuration | UI_CONFIG_TEXT_INPUT_CALLBACK | UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_NUMBER_INPUT_RANGE,
			config, "Scale factor", &data->current_metadata.scale_factor, 1.0f, -100.0f, 100.0f);
		drawer->NextRow();

		config.flag_count = 2;
		drawer->CheckBox(base_configuration ^ UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_MAKE_SQUARE, config, "Invert Z axis", &data->current_metadata.invert_z_axis);
		drawer->NextRow();

		config.flag_count = 2;
		const Reflection::ReflectionEnum* optimize_enum = editor_state->ui_reflection->reflection->GetEnum(STRING(ECS_ASSET_MESH_OPTIMIZE_LEVEL));
		Stream<Stream<char>> optimize_labels = optimize_enum->fields;

		UIConfigComboBoxCallback combo_callback;
		combo_callback.handler = float_input_callback.handler;
		config.AddFlag(combo_callback);

		drawer->ComboBox(
			base_configuration | UI_CONFIG_COMBO_BOX_CALLBACK,
			config,
			"Optimization level",
			optimize_labels,
			optimize_labels.size,
			(unsigned char*)&data->current_metadata.optimize_level
		);
		drawer->NextRow();

		drawer->CrossLine();
	}

	// Convert back to absolute separator
	function::ReplaceCharacter(data->current_metadata.file, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);

	auto calculate_texture_size_from_region = [](uint2 window_size, float2 region_scale) {
		return uint2(
			(unsigned int)(window_size.x * region_scale.x),
			(unsigned int)(window_size.y * MESH_PREVIEW_HEIGHT)
		);
	};

	// Only draw the preview if the mesh could actually be loaded
	if (mesh_thumbnail.could_be_read) {
		Graphics* graphics = editor_state->UIGraphics();
		uint2 window_size = graphics->GetWindowSize();
		// Draw the thumbnail preview
		if (data->mesh == nullptr) {
			// The texture size is the width of the dockspace region with the height ratio taken into account
			uint2 texture_size = calculate_texture_size_from_region(window_size, drawer->GetRegionScale());

			// Load the mesh
			CoallescedMesh* mesh = editor_state->ui_resource_manager->LoadCoallescedMeshImplementation(data->path.buffer);
			data->mesh = mesh;
			data->thumbnail = GLTFGenerateThumbnail(graphics, texture_size, &mesh->mesh);
		}
		else {
			// Only update the thumbnail if there is something to update or if the region scale changed
			bool dimensions_changed = false;
			uint2 texture_size = GetTextureDimensions(data->thumbnail.texture.GetResource());
			uint2 texture_size_from_region = calculate_texture_size_from_region(window_size, drawer->GetRegionScale());
			dimensions_changed = texture_size != texture_size_from_region;

			if (dimensions_changed) {
				// Release the texture and the view
				Texture2D texture = data->thumbnail.texture.GetResource();
				graphics->FreeResource(texture);
				graphics->FreeResource(data->thumbnail.texture);

				GLTFThumbnailInfo old_info = data->thumbnail.info;

				data->thumbnail = GLTFGenerateThumbnail(
					graphics,
					texture_size_from_region,
					&data->mesh->mesh
				);

				data->thumbnail.info = old_info;

				// Update the thumbnail to the new location
				GLTFUpdateThumbnail(graphics, &data->mesh->mesh, data->thumbnail, data->radius_delta, { 0.0f, 0.0f, 0.0f }, { data->rotation_delta.x, data->rotation_delta.y, 0.0f });

				data->radius_delta = 0.0f;
				data->rotation_delta = { 0.0f, 0.0f };
			}
			else {
				bool update_thumbnail = data->radius_delta != 0.0f || data->rotation_delta.x != 0.0f || data->rotation_delta.y != 0.0f;
				if (update_thumbnail) {
					GLTFUpdateThumbnail(
						graphics,
						&data->mesh->mesh,
						data->thumbnail,
						data->radius_delta,
						{ 0.0f, 0.0f, 0.0f },
						{ data->rotation_delta.x, data->rotation_delta.y, 0.0f }
					);

					data->rotation_delta = { 0.0f, 0.0f };
					data->radius_delta = 0.0f;
				}
			}
		}

		// Draw the texture now
		UIDrawConfig thumbnail_config;

		UIConfigAbsoluteTransform thumbnail_transform;
		thumbnail_transform.scale.x = drawer->GetRegionScale().x;
		thumbnail_transform.scale.y = MESH_PREVIEW_HEIGHT;
		thumbnail_transform.position.x = drawer->GetRegionPosition().x;
		thumbnail_transform.position.y = drawer->GetRegionPosition().y + drawer->GetRegionScale().y - MESH_PREVIEW_HEIGHT;
		// Add the offsets from the region offset
		thumbnail_transform.position += drawer->GetRegionRenderOffset();

		// Clamp the position on the y axis such that it is not less than the current position
		thumbnail_transform.position.y = function::ClampMin(thumbnail_transform.position.y, drawer->current_y);

		thumbnail_config.AddFlag(thumbnail_transform);

		drawer->SpriteRectangle(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, thumbnail_config, data->thumbnail.texture);
		// Only finalize the rectangle on the Y axis
		// Subtract the region maximum offset
		drawer->UpdateRenderBoundsRectangle(
			{ drawer->region_fit_space_horizontal_offset, thumbnail_transform.position.y - drawer->GetRegionRenderOffset().y },
			{ 0.0f, thumbnail_transform.scale.y + drawer->region_limit.y - drawer->region_position.y - drawer->region_scale.y }
		);

		// Add the clickable and hoverable handler on that rectangle aswell
		drawer->AddHoverable(thumbnail_transform.position, thumbnail_transform.scale, { InspectorMeshPreviewHoverable, data, 0 });
		drawer->AddClickable(thumbnail_transform.position, thumbnail_transform.scale, { InspectorMeshPreviewClickable, data, 0 });
	}
}

void ChangeInspectorToMeshFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawMeshFileData data;
	data.mesh = nullptr;
	data.radius_delta = 0.0f;
	data.rotation_delta = { 0.0f, 0.0f };
	memset(&data.helper_data, 0, sizeof(data.helper_data));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	inspector_index = ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawMeshFile, InspectorCleanMeshFile },
		&data,
		sizeof(data) + sizeof(wchar_t) * (path.size + 1)
	);

	if (inspector_index != -1) {
		// Get the data and set the path
		InspectorDrawMeshFileData* draw_data = (InspectorDrawMeshFileData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
		draw_data->path = { function::OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->path.Copy(path);
		draw_data->path[draw_data->path.size] = L'\0';
	}
}

void InspectorMeshFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_MESH_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawMeshFile, InspectorCleanMeshFile }, ASSET_MESH_EXTENSIONS[index]);
	}
}
