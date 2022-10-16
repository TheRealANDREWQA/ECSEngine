#include "editorpch.h"
#include "AssetSettingHelper.h"
#include "../Editor/EditorState.h"

#include "../Assets/AssetManagement.h"
#include "../Editor/EditorPalette.h"
#include "../Project/ProjectFolders.h"

#define MAX_SETTINGS 8

#define LAZY_TIMER_RELOAD_MS 200

bool AssetSettingsHelper(UIDrawer* drawer, EditorState* editor_state, AssetSettingsHelperData* helper_data, Stream<wchar_t> file, ECS_ASSET_TYPE asset_type)
{
	// The initialization part
	bool initialize = helper_data->new_name.capacity == 0;
	if (initialize) {
		// Allocate a buffer for it
		const size_t ALLOCATION_SIZE = 256;
		void* allocation = editor_state->editor_allocator->Allocate(sizeof(char) * ALLOCATION_SIZE);
		helper_data->new_name.InitializeFromBuffer(allocation, 0, ALLOCATION_SIZE);
		helper_data->selected_setting = 0;
		helper_data->is_new_name = false;

		const size_t ALLOCATOR_CAPACITY = ECS_KB * 32;
		void* allocator_buffer = editor_state->editor_allocator->Allocate(ALLOCATOR_CAPACITY);
		helper_data->temp_allocator = LinearAllocator(allocator_buffer, ALLOCATOR_CAPACITY);
		helper_data->lazy_timer.DelayStart(LAZY_TIMER_RELOAD_MS * ECS_MB_10);
	}

	const char* COMBO_NAME = "Settings";

	ECS_STACK_CAPACITY_STREAM(unsigned int, handles, MAX_SETTINGS);
	// Display a selection box for the available settings
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&helper_data->temp_allocator);

	if (helper_data->lazy_timer.GetDuration_ms() > LAZY_TIMER_RELOAD_MS) {
		helper_data->temp_allocator.Clear();
		helper_data->current_names = GetAssetCorrespondingMetadata(editor_state, file, asset_type, allocator);
	}

	if (initialize) {
		if (helper_data->current_names.size > 0) {
			// Load the metadata - the first one
			bool success = editor_state->asset_database->ReadAssetFile(helper_data->current_names[0], file, helper_data->metadata, asset_type);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to read asset {#} metadata for file {#}, type {#}.", helper_data->current_names[0], file, ConvertAssetTypeString(asset_type));
				EditorSetConsoleError(error_message);
			}
		}
	}

	UIDrawerRowLayout row_layout = drawer->GenerateRowLayout();
	row_layout.SetHorizontalAlignment(ECS_UI_ALIGN_MIDDLE);
	row_layout.AddComboBox(helper_data->current_names, COMBO_NAME);
	row_layout.AddSquareLabel();
	row_layout.AddSquareLabel();

	UIDrawConfig config;
	size_t combo_configuration = UI_CONFIG_COMBO_BOX_CALLBACK;
	row_layout.GetTransform(config, combo_configuration);
	if (helper_data->selected_setting >= helper_data->current_names.size) {
		// Set it to 0 if for some reason it overpassed
		helper_data->selected_setting = 0;
	}

	struct ComboActionData {
		AssetSettingsHelperData* data;
		Stream<Stream<char>> names;
		Stream<wchar_t> file;
		EditorState* editor_state;
		ECS_ASSET_TYPE type;
	};

	auto combo_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		ComboActionData* data = (ComboActionData*)_data;
		bool success = data->editor_state->asset_database->ReadAssetFile(data->data->current_names[data->data->selected_setting], data->file, data->data->metadata, data->type);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to read asset {#} metadata for file {#}, type {#}.", data->data->current_names[data->data->selected_setting], data->file, ConvertAssetTypeString(data->type));
			EditorSetConsoleError(error_message);
		}
	};

	ComboActionData combo_action_data;
	combo_action_data.data = helper_data;
	combo_action_data.file = file;
	combo_action_data.type = asset_type;
	combo_action_data.editor_state = editor_state;

	UIConfigComboBoxCallback combo_callback;
	combo_callback.handler = { combo_action, &combo_action_data, sizeof(combo_action_data) };
	config.AddFlag(combo_callback);

	drawer->ComboBox(combo_configuration, config, COMBO_NAME, helper_data->current_names, helper_data->current_names.size, &helper_data->selected_setting);

	config.flag_count = 0;
	size_t button_configuration = UI_CONFIG_ACTIVE_STATE;
	row_layout.GetTransform(config, button_configuration);

	auto add_setting = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		AssetSettingsHelperData* data = (AssetSettingsHelperData*)_data;
		data->new_name.size = 0;
		data->is_new_name = true;
	};

	UIConfigActiveState active_state;
	active_state.state = helper_data->current_names.size < MAX_SETTINGS;
	config.AddFlag(active_state);
	drawer->SpriteButton(button_configuration, config, { add_setting, helper_data, 0 }, ECS_TOOLS_UI_TEXTURE_PLUS);

	config.flag_count = 0;
	button_configuration = UI_CONFIG_ACTIVE_STATE;
	row_layout.GetTransform(config, button_configuration);

	struct RemoveSettingData {
		ECS_ASSET_TYPE type;
		EditorState* editor_state;
		Stream<wchar_t> file;
		Stream<Stream<char>> settings;
		AssetSettingsHelperData* data;
	};

	active_state.state = helper_data->current_names.size > 1;
	config.AddFlag(active_state);

	RemoveSettingData remove_data;
	remove_data.editor_state = editor_state;
	remove_data.type = asset_type;
	remove_data.file = file;
	remove_data.settings.InitializeAndCopy(allocator, helper_data->current_names);
	remove_data.data = helper_data;

	auto remove_setting = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		RemoveSettingData* data = (RemoveSettingData*)_data;
		DeleteAssetSetting(data->editor_state, data->settings[data->data->selected_setting], data->file, data->type);
		data->data->selected_setting = 0;
		// Load the first metadata
		bool success = data->editor_state->asset_database->ReadAssetFile(data->settings[data->data->selected_setting], data->file, data->data->metadata, data->type);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to read asset {#} metadata for file {#}, type {#}.", data->settings[data->data->selected_setting], data->file, ConvertAssetTypeString(data->type));
			EditorSetConsoleError(error_message);
		}
	};

	drawer->SpriteButton(button_configuration, config, { remove_setting, &remove_data, sizeof(remove_data) }, ECS_TOOLS_UI_TEXTURE_X);
	drawer->NextRow();
	config.flag_count = 0;

	if (helper_data->is_new_name) {
		struct NewSettingInputCallbackData {
			AssetSettingsHelperData* data;
			EditorState* editor_state;
			Stream<Stream<char>> names;
			Stream<wchar_t> file;
			ECS_ASSET_TYPE type;
		};

		auto new_setting_input_callback = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			NewSettingInputCallbackData* data = (NewSettingInputCallbackData*)_data;
			if (keyboard->IsKeyDown(HID::Key::Enter)) {
				// Check to see if it already exists
				bool exists = function::FindString(data->data->new_name, data->names) != -1;
				if (!exists && data->data->new_name.size > 0) {
					bool success = CreateAssetSetting(data->editor_state, data->data->new_name, data->file, data->type);
					if (!success) {
						ECS_FORMAT_TEMP_STRING(error_message, "Failed to create metadata file for asset {#}, type {#}.", data->data->new_name, ConvertAssetTypeString(data->type));
						EditorSetConsoleError(error_message);
					}
					else {
						// Determine the index of the newly metadata
						size_t index = 0;
						for (; index < data->names.size; index++) {
							if (function::StringIsLess(data->data->new_name, data->names[index])) {
								break;
							}
						}
						data->data->selected_setting = index;

						// Set the default values for the metadata
						CreateDefaultAsset(data->data->metadata, data->data->new_name, data->file, data->type);
					}
				}
				data->data->is_new_name = false;
			}
		};

		size_t text_input_configuration = UI_CONFIG_ALIGN_ELEMENT | UI_CONFIG_TEXT_INPUT_CALLBACK;

		UIConfigAlignElement align_element;
		align_element.horizontal = ECS_UI_ALIGN_MIDDLE;
		config.AddFlag(align_element);

		//  Check to see if the name already exists. If it does make the text red
		bool name_exists = function::FindString(helper_data->new_name, helper_data->current_names) != -1;
		if (name_exists) {
			UIConfigTextParameters text_parameters = drawer->TextParameters();
			text_parameters.color = EDITOR_RED_COLOR;
			config.AddFlag(text_parameters);
			text_input_configuration |= UI_CONFIG_TEXT_PARAMETERS;
		}

		NewSettingInputCallbackData callback_data;
		callback_data.data = helper_data;
		callback_data.editor_state = editor_state;
		callback_data.file = file;
		callback_data.names = helper_data->current_names;
		callback_data.type = asset_type;
		UIConfigTextInputCallback callback;
		callback.handler = { new_setting_input_callback, &callback_data, sizeof(callback_data) };
		config.AddFlag(callback);

		drawer->TextInput(text_input_configuration, config, "Choose a name", &helper_data->new_name);
		drawer->NextRow(2.0f);
	}
	else {
		drawer->NextRow();
	}
	
	return helper_data->current_names.size > 0;
}

void AssetSettingsHelperDestroy(EditorState* editor_state, AssetSettingsHelperData* helper_data)
{
	// In case the initialization doesn't run, don't deallocate these
	if (editor_state->editor_allocator->Belongs(helper_data->new_name.buffer)) {
		editor_state->editor_allocator->Deallocate(helper_data->new_name.buffer);
	}
	if (editor_state->editor_allocator->Belongs(helper_data->temp_allocator.m_buffer)) {
		editor_state->editor_allocator->Deallocate(helper_data->temp_allocator.m_buffer);
	}
}

void AssetSettingsHelperChangedBaseAction(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	AssetSettingsHelperChangedBaseActionData* data = (AssetSettingsHelperChangedBaseActionData*)_data;
	unsigned int handle = data->editor_state->asset_database->FindAsset(data->helper_data->SelectedName(), data->file, data->asset_type);
	bool success = true;
	if (handle != -1) {
		success = data->editor_state->asset_database->UpdateAsset(handle, &data->helper_data->metadata, data->asset_type);
	}
	else {
		success = data->editor_state->asset_database->WriteAssetFile(data->helper_data->metadata, data->asset_type);
	}

	if (!success) {
		ECS_FORMAT_TEMP_STRING(error_message, "Failed to write new metadata values for mesh {#}, file {#}.", data->helper_data->SelectedName(), data->file);
		EditorSetConsoleWarn(error_message);
	}
}

size_t AssetSettingsHelperBaseConfiguration() {
	return UI_CONFIG_NAME_PADDING | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ELEMENT_NAME_FIRST;
}

void AssetSettingsHelperBaseConfig(UIDrawConfig* draw_config) {
	UIConfigNamePadding name_padding;
	name_padding.alignment = ECS_UI_ALIGN_LEFT;
	name_padding.total_length = 0.17f;

	draw_config->AddFlag(name_padding);

	UIConfigWindowDependentSize window_dependent_size;
	draw_config->AddFlag(window_dependent_size);
}
