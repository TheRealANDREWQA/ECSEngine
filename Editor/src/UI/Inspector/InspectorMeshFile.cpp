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
#include "InspectorMeshFile.h"

using namespace ECSEngine;
ECS_TOOLS;

#define MESH_PREVIEW_HEIGHT 0.4f
#define MESH_PREVIEW_ROTATION_DELTA_FACTOR 250.0f
#define MESH_PREVIEW_RADIUS_DELTA_FACTOR 0.00125f


struct InspectorDrawMeshFileData {
	Stream<wchar_t> path;
	GLTFThumbnail thumbnail;
	CoalescedMesh* mesh;
	float radius_delta;
	float2 rotation_delta;
	float window_thumbnail_percentage;

	MeshMetadata current_metadata;
	// This will be copied every frame from the current metadata
	// such that when 
	MeshMetadata previous_metadata;
	AssetSettingsHelperData helper_data;
};

void InspectorCleanMeshFile(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;

	// Unload the mesh and free the resources only if the mesh was previously loaded
	FileExplorerMeshThumbnail file_explorer_thumbnail;
	if (editor_state->file_explorer_data->mesh_thumbnails.TryGetValue(ResourceIdentifier(data->path), file_explorer_thumbnail)) {
		if (file_explorer_thumbnail.could_be_read) {
			editor_state->ui_resource_manager->UnloadCoalescedMeshImplementation(data->mesh, false);
			// Release the texture as well
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
	if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
		// Active the raw input
		mouse->EnableRawInput();
	}
	else if (mouse->IsDown(ECS_MOUSE_LEFT)) {
		data->rotation_delta.x += MESH_PREVIEW_ROTATION_DELTA_FACTOR * mouse_delta.y;
		data->rotation_delta.y += MESH_PREVIEW_ROTATION_DELTA_FACTOR * mouse_delta.x;
	}
	else if (mouse->IsReleased(ECS_MOUSE_LEFT)) {
		// Disable the raw input
		mouse->DisableRawInput();
	}
}

void InspectorMeshPreviewHoverable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

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
		InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_MESH);
	}

	if (data->current_metadata.file.size == 0) {
		// Retrieve the target file
		Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, data->path);
		ECS_ASSERT(relative_path.size > 0);
		data->current_metadata.file = relative_path;
	}

	InspectorDefaultFileInfo(editor_state, drawer, data->path);

	// Convert the absolute separator into relative
	ReplaceCharacter(data->current_metadata.file, ECS_OS_PATH_SEPARATOR, ECS_OS_PATH_SEPARATOR_REL);

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
		changed_base_data.target_database = editor_state->asset_database;

		UIConfigTextInputCallback float_input_callback;
		float_input_callback.handler = { AssetSettingsHelperChangedAction, &changed_base_data, sizeof(changed_base_data) };
		float_input_callback.trigger_only_on_release = true;

		config.AddFlag(float_input_callback);

		// Display the values
		drawer->FloatInput(base_configuration | UI_CONFIG_TEXT_INPUT_CALLBACK | UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_NUMBER_INPUT_RANGE,
			config, "Scale factor", &data->current_metadata.scale_factor, 1.0f, -100.0f, 100.0f);
		drawer->NextRow();

		config.flag_count--;

		const size_t CHECK_BOX_CONFIGURATION = base_configuration ^ UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_MAKE_SQUARE | UI_CONFIG_CHECK_BOX_CALLBACK;

		UIConfigCheckBoxCallback check_box_callback;
		check_box_callback.handler = float_input_callback.handler;
		config.AddFlag(check_box_callback);
		drawer->CheckBox(CHECK_BOX_CONFIGURATION, config, "Invert Z axis", &data->current_metadata.invert_z_axis);
		drawer->NextRow();

		drawer->CheckBox(CHECK_BOX_CONFIGURATION, config, "Origin To Object Center", &data->current_metadata.origin_to_object_center);
		drawer->NextRow();

		// This removed the check box callback
		config.flag_count--;

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
	ReplaceCharacter(data->current_metadata.file, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);

	auto calculate_texture_size_from_region = [=](uint2 window_size, float2 region_scale) {
		return uint2(
			(unsigned int)(window_size.x * region_scale.x),
			(unsigned int)(window_size.y * data->window_thumbnail_percentage)
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
			CoalescedMesh* mesh = editor_state->ui_resource_manager->LoadCoalescedMeshImplementation(data->path, false);
			data->mesh = mesh;
			data->thumbnail = GLTFGenerateThumbnail(graphics, texture_size, &mesh->mesh);
		}
		else {
			// Only update the thumbnail if there is something to update or if the region scale changed
			bool dimensions_changed = false;
			uint2 texture_size = GetTextureDimensions(data->thumbnail.texture.AsTexture2D());
			uint2 texture_size_from_region = calculate_texture_size_from_region(window_size, drawer->GetRegionScale());
			dimensions_changed = texture_size != texture_size_from_region;

			if (dimensions_changed) {
				// Release the texture and the view
				Texture2D texture = data->thumbnail.texture.AsTexture2D();
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
		thumbnail_transform.scale.y = data->window_thumbnail_percentage;
		thumbnail_transform.position.x = drawer->GetRegionPosition().x;
		thumbnail_transform.position.y = drawer->GetRegionPosition().y + drawer->GetRegionScale().y - data->window_thumbnail_percentage;
		// Add the offsets from the region offset
		thumbnail_transform.position += drawer->GetRegionRenderOffset();

		// Clamp the position on the y axis such that it is not less than the current position
		thumbnail_transform.position.y = ClampMin(thumbnail_transform.position.y, drawer->current_y);

		thumbnail_config.AddFlag(thumbnail_transform);

		drawer->SpriteRectangle(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, thumbnail_config, data->thumbnail.texture);
		// Only finalize the rectangle on the Y axis
		// Subtract the region maximum offset
		drawer->UpdateRenderBoundsRectangle(
			{ drawer->region_fit_space_horizontal_offset, thumbnail_transform.position.y - drawer->GetRegionRenderOffset().y },
			{ 0.0f, thumbnail_transform.scale.y + drawer->region_limit.y - drawer->region_position.y - drawer->region_scale.y }
		);

		// Add the clickable and hoverable handler on that rectangle aswell
		drawer->AddHoverable(0, thumbnail_transform.position, thumbnail_transform.scale, { InspectorMeshPreviewHoverable, data, 0 });
		drawer->AddClickable(0, thumbnail_transform.position, thumbnail_transform.scale, { InspectorMeshPreviewClickable, data, 0 });

		auto slide_preview = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;
			// The delta needs to be inverted
			data->window_thumbnail_percentage -= mouse_delta.y;
			data->window_thumbnail_percentage = ClampMin(data->window_thumbnail_percentage, 0.0f);

			UIDefaultHoverableData default_data;
			default_data.colors[0] = system->m_descriptors.color_theme.theme;
			default_data.percentages[0] = 1.25f;

			action_data->data = &default_data;
			DefaultHoverableAction(action_data);
		};

		const float SLIDE_PREVIEW_THICKNESS = 0.01f;
		
		float2 slide_preview_scale = { drawer->GetRegionScale().x, SLIDE_PREVIEW_THICKNESS };

		// Add a clickable in order to increase or decrease the preview
		float2 slide_preview_position = thumbnail_transform.position;
		slide_preview_position.y -= SLIDE_PREVIEW_THICKNESS * 0.5f;
		slide_preview_position.y -= drawer->GetRegionRenderOffset().y;

		drawer->AddDefaultHoverable(0, slide_preview_position, slide_preview_scale, drawer->color_theme.theme);
		drawer->AddClickable(0, slide_preview_position, slide_preview_scale, { slide_preview, data, 0 });
	}
}

void ChangeInspectorToMeshFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index, Stream<char> initial_name) {
	InspectorDrawMeshFileData data;
	data.mesh = nullptr;
	data.radius_delta = 0.0f;
	data.rotation_delta = { 0.0f, 0.0f };
	data.window_thumbnail_percentage = MESH_PREVIEW_HEIGHT;
	memset(&data.helper_data, 0, sizeof(data.helper_data));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	uint3 inspector_indices = ChangeInspectorDrawFunctionWithSearchEx(
		editor_state,
		inspector_index,
		{ InspectorDrawMeshFile, InspectorCleanMeshFile },
		&data,
		sizeof(data),
		-1,
		[=](void* inspector_data) {
			InspectorDrawMeshFileData* other_data = (InspectorDrawMeshFileData*)inspector_data;
			return other_data->path == path;
		}
	);

	if (inspector_indices.y != -1) {
		// Get the data and set the path
		InspectorDrawMeshFileData* draw_data = (InspectorDrawMeshFileData*)GetInspectorDrawFunctionData(editor_state, inspector_indices.y);
		draw_data->path = path.Copy(GetLastInspectorTargetAllocator(editor_state, inspector_indices.y));
		UpdateLastInspectorTargetData(editor_state, inspector_indices.y, draw_data);

		if (initial_name.size > 0) {
			struct InitializeData {
				Stream<char> name;
			};

			AllocatorPolymorphic initialize_allocator = GetLastInspectorTargetAllocator(editor_state, inspector_indices.y);
			InitializeData initialize_data;
			initialize_data.name = initial_name.Copy(initialize_allocator);
			
			auto initialize = [](EditorState* editor_state, void* data, void* _initialize_data, unsigned int inspector_index) {
				InitializeData* initialize_data = (InitializeData*)_initialize_data;
				ChangeInspectorMeshFileConfiguration(editor_state, inspector_index, initialize_data->name);
			};

			SetLastInspectorTargetInitialize(editor_state, inspector_indices.y, initialize, &initialize_data, sizeof(initialize_data));
		}
	}
	else {
		if (initial_name.size > 0 && inspector_indices.z != -1) {
			ChangeInspectorMeshFileConfiguration(editor_state, inspector_indices.z, initial_name);
		}
	}
}

void InspectorMeshFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_MESH_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawMeshFile, InspectorCleanMeshFile }, ASSET_MESH_EXTENSIONS[index]);
	}
}

void ChangeInspectorMeshFileConfiguration(EditorState* editor_state, unsigned int inspector_index, Stream<char> name)
{
	InspectorDrawMeshFileData* draw_data = (InspectorDrawMeshFileData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
	draw_data->helper_data.SetNewSetting(name);
}

InspectorAssetTarget InspectorDrawMeshTarget(const void* inspector_data)
{
	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)inspector_data;
	return { data->path, data->current_metadata.name };
}
