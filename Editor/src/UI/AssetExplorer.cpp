#include "editorpch.h"
#include "AssetExplorer.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorPalette.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../UI/HelperWindows.h"
#include "ECSEngineAssets.h"
#include "../Project/ProjectFolders.h"
#include "Inspector.h"

#define RETAINED_MODE_REFRESH_DURATION_MS 100

struct AssetExplorerData {
	EditorState* editor_state;
	bool asset_opened_headers[ECS_ASSET_TYPE_COUNT];
	bool resource_manager_opened_headers[(unsigned char)ResourceType::TypeCount];

	Timer retained_mode_timer;
};

void AssetExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	AssetExplorerData* data = (AssetExplorerData*)window_data;
	EditorState* editor_state = data->editor_state;

	ResourceManager* resource_manager = editor_state->RuntimeResourceManager();
	AssetDatabase* database = editor_state->asset_database;

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	auto iterate_asset_type = [&](ECS_ASSET_TYPE asset_type, auto stream) {
		const char* string = ConvertAssetTypeString(asset_type);
		drawer.CollapsingHeader(string, &data->asset_opened_headers[asset_type], [&]() {
			struct SelectData {
				EditorState* editor_state;
				const void* metadata;
				ECS_ASSET_TYPE asset_type;
			};

			auto select_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SelectData* data = (SelectData*)_data;
				ChangeInspectorToAsset(data->editor_state, data->metadata, data->asset_type);
			};

			for (size_t index = 0; index < stream.size; index++) {
				const void* asset = stream.buffer + index;
				Stream<char> asset_name = GetAssetName(asset, asset_type);

				SelectData select_data;
				select_data.asset_type = asset_type;
				select_data.editor_state = editor_state;
				select_data.metadata = &stream[index].value;

				ECS_STACK_CAPACITY_STREAM(char, reference_count_characters, 16);
				unsigned int current_asset_reference_count = stream[index].reference_count;
				ConvertIntToChars(reference_count_characters, current_asset_reference_count);

				UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
				row_layout.IndentRow(drawer.layout.node_indentation);
				row_layout.AddSquareLabel();
				row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
				row_layout.AddLabel(reference_count_characters);

				UIDrawConfig config;
				size_t configuration = 0;

				bool is_asset_loaded = IsAssetFromMetadataLoaded(editor_state->RuntimeResourceManager(), asset, asset_type, assets_folder, true);
				row_layout.GetTransform(config, configuration);

				const wchar_t* texture_to_use = nullptr;
				Color color_to_use;
				if (is_asset_loaded) {
					texture_to_use = ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK;
					color_to_use = EDITOR_GREEN_COLOR;
				}
				else {
					texture_to_use = ECS_TOOLS_UI_TEXTURE_X;
					color_to_use = EDITOR_RED_COLOR;
				}

				drawer.SpriteRectangle(configuration, config, texture_to_use, color_to_use);
				configuration = 0;
				config.flag_count = 0;

				row_layout.GetTransform(config, configuration);
					
				ECS_STACK_CAPACITY_STREAM(char, asset_string, 256);
				AssetToString(asset, asset_type, asset_string);

				ECS_STACK_CAPACITY_STREAM(char, long_asset_string, 256);
				AssetToString(asset, asset_type, long_asset_string, true);

				if (long_asset_string.size != asset_string.size) {
					UIConfigToolTip tooltip_config;
					tooltip_config.characters = long_asset_string;
					config.AddFlag(tooltip_config);
					configuration |= UI_CONFIG_TOOL_TIP;
				}
					
				bool is_asset_referenced = IsAssetReferencedInSandboxEntities(editor_state, asset, asset_type);
				UIConfigActiveState active_state;
				active_state.state = is_asset_referenced;
				config.AddFlag(active_state);
				configuration |= UI_CONFIG_ACTIVE_STATE;

				drawer.Button(configuration, config, asset_string, { select_action, &select_data, sizeof(select_data) });

				config.flag_count = 0;
				configuration = 0;
				row_layout.GetTransform(config, configuration);
				drawer.TextLabel(configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, config, reference_count_characters);

				drawer.NextRow();
			}
		});
	};

	drawer.Text("Assets");
	drawer.NextRow();

	// Display the metadata active in the database
	iterate_asset_type(ECS_ASSET_MESH, database->mesh_metadata.ToStream());
	iterate_asset_type(ECS_ASSET_TEXTURE, database->texture_metadata.ToStream());
	iterate_asset_type(ECS_ASSET_GPU_SAMPLER, database->gpu_sampler_metadata.ToStream());
	iterate_asset_type(ECS_ASSET_SHADER, database->shader_metadata.ToStream());
	iterate_asset_type(ECS_ASSET_MATERIAL, database->material_asset.ToStream());
	iterate_asset_type(ECS_ASSET_MISC, database->misc_asset.ToStream());

	drawer.NextRow();
	drawer.CrossLine();
	drawer.NextRow();

	drawer.Text("Resources in Resource Manager");
	drawer.NextRow();
	auto iterate_resource_type = [&](ResourceType resource_type) {
		const char* resource_string = ResourceTypeString(resource_type);

		drawer.CollapsingHeader(resource_string, data->resource_manager_opened_headers + (unsigned char)resource_type, [&]() {
			UIDrawConfig config;

			ResourceManager* resource_manager = editor_state->RuntimeResourceManager();
			resource_manager->ForEachResourceIdentifierNoSuffix(resource_type, [&](ResourceIdentifier identifier, unsigned short suffix_size) {
				Stream<void> suffix = { OffsetPointer(identifier.ptr, identifier.size), suffix_size };
				unsigned short reference_count = resource_manager->GetEntry(identifier, resource_type, suffix).reference_count;
				ECS_STACK_CAPACITY_STREAM(char, reference_count_chars, 16);
				ConvertIntToChars(reference_count_chars, reference_count);

				UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
				row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
				row_layout.AddLabel(reference_count_chars);
				
				config.flag_count = 0;
				size_t configuration = 0;
				row_layout.GetTransform(config, configuration);

				Stream<wchar_t> path = { identifier.ptr, identifier.size / sizeof(wchar_t) };
				drawer.TextLabelWide(configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, path);

				config.flag_count = 0;
				configuration = 0;
				row_layout.GetTransform(config, configuration);
				drawer.TextLabel(configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, reference_count_chars);

				drawer.NextRow();
			});
		});
	};

	for (unsigned int index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
		iterate_resource_type((ResourceType)index);
	}

}

static bool AssetExplorerRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	AssetExplorerData* explorer_data = (AssetExplorerData*)window_data;
	if (explorer_data->retained_mode_timer.GetDuration(ECS_TIMER_DURATION_MS) >= RETAINED_MODE_REFRESH_DURATION_MS) {
		explorer_data->retained_mode_timer.SetNewStart();
		return false;
	}
	return true;
}

void AssetExplorerSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int index = *(unsigned int*)stack_memory;

	AssetExplorerData* data = (AssetExplorerData*)OffsetPointer(stack_memory, sizeof(unsigned int));
	data->editor_state = editor_state;
	memset(data->asset_opened_headers, 0, sizeof(data->asset_opened_headers));
	memset(data->resource_manager_opened_headers, 0, sizeof(data->resource_manager_opened_headers));
	data->retained_mode_timer.SetUninitialized();

	descriptor.draw = AssetExplorerDraw;
	descriptor.retained_mode = AssetExplorerRetainedMode;

	descriptor.window_name = ASSET_EXPLORER_WINDOW_NAME;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
}

void CreateAssetExplorer(EditorState* editor_state) {
	CreateDockspaceFromWindow(ASSET_EXPLORER_WINDOW_NAME, editor_state, CreateAssetExplorerWindow);
}

void CreateAssetExplorerAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	CreateAssetExplorer(data);
}

unsigned int CreateAssetExplorerWindow(EditorState* editor_state) {
	return CreateDefaultWindow(ASSET_EXPLORER_WINDOW_NAME, editor_state, { 0.7f, 0.8f }, AssetExplorerSetDecriptor);
}