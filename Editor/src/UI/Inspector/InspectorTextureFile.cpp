#include "editorpch.h"
#include "../Inspector.h"
#include "InspectorUtilities.h"
#include "../../Editor/EditorState.h"

#include "../AssetSettingHelper.h"
#include "InspectorTextureFile.h"
#include "../../Project/ProjectFolders.h"

#include "../../Assets/AssetExtensions.h"

using namespace ECSEngine;

// ----------------------------------------------------------------------------------------------------------------------------

struct InspectorDrawTextureData {
	ECS_INLINE unsigned int* Size() {
		return &path_size;
	}

	ECS_INLINE Stream<wchar_t> Path() {
		return GetCoalescedStreamFromType(this).As<wchar_t>();
	}

	unsigned int path_size;
	TextureMetadata current_metadata;
	AssetSettingsHelperData helper_data;
};

void InspectorCleanTexture(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawTextureData* data = (InspectorDrawTextureData*)_data;
	AssetSettingsHelperDestroy(editor_state, &data->helper_data);
}

void InspectorDrawTextureFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawTextureData* data = (InspectorDrawTextureData*)_data;

	Stream<wchar_t> path = data->Path();

	// Check to see if the file still exists - if it doesn't revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	InspectorIcon(drawer, path);
	InspectorDefaultFileInfo(editor_state, drawer, path);

	Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, path);
	ECS_ASSERT(relative_path.size > 0);

	// Change the relative path separator from absolute into relative
	ReplaceCharacter(relative_path, ECS_OS_PATH_SEPARATOR, ECS_OS_PATH_SEPARATOR_REL);
	
	data->current_metadata.file = relative_path;
	data->helper_data.metadata = &data->current_metadata;
	bool has_settings = AssetSettingsHelper(drawer, editor_state, &data->helper_data, ECS_ASSET_TEXTURE);

	if (has_settings) {
		data->current_metadata.name = data->helper_data.SelectedName();

		UIDrawConfig config;
		size_t base_configuration = AssetSettingsHelperBaseConfiguration();
		AssetSettingsHelperBaseConfig(&config);

		size_t base_config_size = config.flag_count;

		AssetSettingsHelperChangedActionData changed_base_data;
		changed_base_data.asset_type = ECS_ASSET_TEXTURE;
		changed_base_data.editor_state = editor_state;
		changed_base_data.helper_data = &data->helper_data;
		changed_base_data.target_database = editor_state->asset_database;

		UIConfigCheckBoxCallback check_box_callback;
		check_box_callback.handler = { AssetSettingsHelperChangedAction, &changed_base_data, sizeof(changed_base_data) };
		config.AddFlag(check_box_callback);

		drawer->CheckBox(base_configuration | UI_CONFIG_CHECK_BOX_CALLBACK, config, "sRGB (Color Texture)", &data->current_metadata.sRGB);
		drawer->NextRow();

		drawer->CheckBox(base_configuration | UI_CONFIG_CHECK_BOX_CALLBACK, config, "Generate Mip Maps", &data->current_metadata.generate_mip_maps);
		drawer->NextRow();
	
		config.flag_count = base_config_size;
		UIConfigComboBoxCallback combo_callback;
		combo_callback.handler = check_box_callback.handler;
		config.AddFlag(combo_callback);

		const Reflection::ReflectionEnum* compression_type = editor_state->ui_reflection->reflection->GetEnum(STRING(ECS_TEXTURE_COMPRESSION_EX));
		Stream<Stream<char>> compression_labels = compression_type->fields;
		drawer->ComboBox(base_configuration | UI_CONFIG_COMBO_BOX_CALLBACK, config, "Compression", compression_labels, compression_labels.size, (unsigned char*)&data->current_metadata.compression_type);
		drawer->NextRow();
		drawer->CrossLine();
	}

	// Convert back the relative path separator into absolute
	ReplaceCharacter(relative_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorTextureFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_TEXTURE_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawTextureFile, InspectorCleanTexture }, ASSET_TEXTURE_EXTENSIONS[index]);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToTextureFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index, Stream<char> initial_name)
{
	ECS_STACK_VOID_STREAM(stack_storage, ECS_KB);
	unsigned int write_size = 0;
	InspectorDrawTextureData* draw_data = CreateCoalescedStreamIntoType<InspectorDrawTextureData>(stack_storage, path, &write_size);
	memset(&draw_data->helper_data, 0, sizeof(draw_data->helper_data));

	uint3 indices = ChangeInspectorDrawFunctionWithSearchEx(
		editor_state,
		inspector_index,
		{ InspectorDrawTextureFile, InspectorCleanTexture },
		draw_data,
		write_size,
		-1,
		[=](void* inspector_data) {
			InspectorDrawTextureData* other_data = (InspectorDrawTextureData*)inspector_data;
			return other_data->Path() == path;
		}
	);
	
	if (initial_name.size > 0) {
		if (indices.z != -1) {
			ChangeInspectorTextureFileConfiguration(editor_state, indices.z, initial_name);
		}
		else if (indices.y != -1) {
			struct InitializeData {
				Stream<char> name;
			};

			AllocatorPolymorphic initialize_allocator = GetLastInspectorTargetInitializeAllocator(editor_state, inspector_index);
			InitializeData initialize_data = { initial_name.Copy(initialize_allocator) };
			auto initialize = [](EditorState* editor_state, void* data, void* _initialize_data, unsigned int inspector_index) {
				InitializeData* initialize_data = (InitializeData*)_initialize_data;
				ChangeInspectorTextureFileConfiguration(editor_state, inspector_index, initialize_data->name);
			};
			SetLastInspectorTargetInitialize(editor_state, indices.y, initialize, &initialize_data, sizeof(initialize_data));
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorTextureFileConfiguration(EditorState* editor_state, unsigned int inspector_index, Stream<char> name)
{
	InspectorDrawTextureData* draw_data = (InspectorDrawTextureData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
	draw_data->helper_data.SetNewSetting(name);
}

// ----------------------------------------------------------------------------------------------------------------------------

InspectorAssetTarget InspectorDrawTextureTarget(const void* inspector_data)
{
	InspectorDrawTextureData* data = (InspectorDrawTextureData*)inspector_data;
	return { data->Path(), data->current_metadata.name };
}

// ----------------------------------------------------------------------------------------------------------------------------