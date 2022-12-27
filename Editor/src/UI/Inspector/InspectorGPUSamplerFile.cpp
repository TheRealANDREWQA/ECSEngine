#include "editorpch.h"
#include "InspectorMiscFile.h"
#include "../Inspector.h"
#include "InspectorUtilities.h"
#include "../../Editor/EditorState.h"

#include "../../Assets/AssetManagement.h"
#include "../../Assets/AssetExtensions.h"
#include "../AssetIcons.h"
#include "../AssetSettingHelper.h"

using namespace ECSEngine;
ECS_TOOLS;

#define ANISOTROPIC_LEVELS 5
#define ANISOTROPIC_CHAR_STORAGE 10

struct InspectorDrawGPUSamplerFileData {
	GPUSamplerMetadata sampler_metadata;
	Stream<wchar_t> path;
	unsigned char anisotropic_mapping[ANISOTROPIC_LEVELS];
	Stream<char> anisotropic_labels[ANISOTROPIC_LEVELS];
	char anisotropic_label_storage[ANISOTROPIC_CHAR_STORAGE];
};

void InspectorCleanGPUSampler(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawGPUSamplerFileData* data = (InspectorDrawGPUSamplerFileData*)_data;
	editor_state->editor_allocator->Deallocate(data->sampler_metadata.name.buffer);
}

void InspectorDrawGPUSamplerFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawGPUSamplerFileData* data = (InspectorDrawGPUSamplerFileData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	if (data->sampler_metadata.name.size == 0) {
		// Retrieve the name
		ECS_STACK_CAPACITY_STREAM(char, asset_name, 512);
		GetAssetNameFromThunkOrForwardingFile(editor_state, data->path, asset_name);
		data->sampler_metadata.name = function::StringCopy(editor_state->EditorAllocator(), asset_name);

		CapacityStream<char> anisotropic_chars(data->anisotropic_label_storage, 0, ANISOTROPIC_CHAR_STORAGE);

		// Initialize the anisotropic mapping
		unsigned char anisotropic_start = 1;
		for (size_t index = 0; index < ANISOTROPIC_LEVELS; index++) {
			data->anisotropic_mapping[index] = anisotropic_start;
			anisotropic_start *= 2;
			
			unsigned int anisotropic_offset = anisotropic_chars.size;
			size_t write_size = function::ConvertIntToChars(anisotropic_chars, data->anisotropic_mapping[index]);
			data->anisotropic_labels[index] = { anisotropic_chars.buffer + anisotropic_offset, write_size };
		}

		// Retrieve the data from the file, if any
		bool success = editor_state->asset_database->ReadGPUSamplerFile(data->sampler_metadata.name, &data->sampler_metadata);
		if (!success) {
			// Set the default for the metadata
			data->sampler_metadata.Default(data->sampler_metadata.name, { nullptr, 0 });
		}
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ASSET_GPU_SAMPLER_ICON, drawer->color_theme.text, drawer->color_theme.theme);

	InspectorIconNameAndPath(drawer, data->path);
	InspectorDrawFileTimes(drawer, data->path);
	InspectorOpenAndShowButton(drawer, data->path);
	drawer->CrossLine();

	// Draw the settings
	UIDrawConfig config;

	const Reflection::ReflectionEnum* address_enum = editor_state->ui_reflection->reflection->GetEnum(STRING(ECS_SAMPLER_ADDRESS_TYPE));
	const Reflection::ReflectionEnum* filter_enum = editor_state->ui_reflection->reflection->GetEnum(STRING(ECS_SAMPLER_FILTER_TYPE));

	AssetSettingsHelperChangedNoFileActionData callback_data;
	callback_data.editor_state = editor_state;
	callback_data.asset = &data->sampler_metadata;
	callback_data.asset_type = ECS_ASSET_GPU_SAMPLER;

	UIConfigComboBoxCallback combo_callback;
	combo_callback.handler = { AssetSettingsHelperChangedNoFileAction, &callback_data, sizeof(callback_data) };
	config.AddFlag(combo_callback);

	UIConfigWindowDependentSize window_dependent_size;
	config.AddFlag(window_dependent_size);

	UIConfigNamePadding name_padding;
	name_padding.total_length = ASSET_NAME_PADDING;
	config.AddFlag(name_padding);

	const size_t COMBO_BASE_CONFIGURATION = UI_CONFIG_COMBO_BOX_CALLBACK | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_NAME_PADDING | UI_CONFIG_ELEMENT_NAME_FIRST;

	Stream<Stream<char>> address_mode_labels = address_enum->fields;
	drawer->ComboBox(COMBO_BASE_CONFIGURATION, config, "Address Mode", address_mode_labels, address_mode_labels.size, (unsigned char*)&data->sampler_metadata.address_mode);
	drawer->NextRow();

	Stream<Stream<char>> filter_mode_labels = filter_enum->fields;
	drawer->ComboBox(COMBO_BASE_CONFIGURATION, config, "Filter Type", filter_mode_labels, filter_mode_labels.size, (unsigned char*)&data->sampler_metadata.filter_mode);
	drawer->NextRow();

	UIConfigActiveState active_state;
	active_state.state = data->sampler_metadata.filter_mode == ECS_SAMPLER_FILTER_ANISOTROPIC;
	config.AddFlag(active_state);

	UIConfigComboBoxMapping anisotropic_mapping;
	anisotropic_mapping.mappings = data->anisotropic_mapping;
	anisotropic_mapping.byte_size = sizeof(unsigned char);
	anisotropic_mapping.stable = true;
	config.AddFlag(anisotropic_mapping);
	
	drawer->ComboBox(
		COMBO_BASE_CONFIGURATION | UI_CONFIG_COMBO_BOX_MAPPING | UI_CONFIG_ACTIVE_STATE,
		config, 
		"Anisotropic level", 
		{ data->anisotropic_labels, ANISOTROPIC_LEVELS },
		ANISOTROPIC_LEVELS,
		&data->sampler_metadata.anisotropic_level
	);
	drawer->NextRow();

	drawer->CrossLine();
}

void ChangeInspectorToGPUSamplerFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawGPUSamplerFileData data;
	data.path = path;
	memset(&data.sampler_metadata, 0, sizeof(data.sampler_metadata));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	inspector_index = ChangeInspectorDrawFunctionWithSearch(
		editor_state,
		inspector_index,
		{ InspectorDrawGPUSamplerFile, InspectorCleanGPUSampler },
		&data,
		sizeof(data) + sizeof(wchar_t) * (path.size + 1),
		-1,
		[=](void* inspector_data) {
			InspectorDrawGPUSamplerFileData* other_data = (InspectorDrawGPUSamplerFileData*)inspector_data;
			return function::CompareStrings(other_data->path, path);
		}
	);

	if (inspector_index != -1) {
		// Get the data and set the path
		InspectorDrawGPUSamplerFileData* draw_data = (InspectorDrawGPUSamplerFileData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
		draw_data->path = { function::OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->path.Copy(path);
	}
}

void InspectorGPUSamplerFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_GPU_SAMPLER_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawGPUSamplerFile, InspectorCleanGPUSampler }, ASSET_GPU_SAMPLER_EXTENSIONS[index]);
	}
}