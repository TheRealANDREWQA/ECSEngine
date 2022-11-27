#include "editorpch.h"
#include "AssetExplorer.h"
#include "..\Editor\EditorState.h"
#include "..\HelperWindows.h"
#include "ECSEngineAssets.h"

struct AssetExplorerData {
	EditorState* editor_state;
	bool opened_headers[ECS_ASSET_TYPE_COUNT];
	CapacityStream<char> selected_asset;
	ECS_ASSET_TYPE selected_asset_type;
};

void AssetExplorerDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	AssetExplorerData* data = (AssetExplorerData*)window_data;
	EditorState* editor_state = data->editor_state;

	ResourceManager* resource_manager = editor_state->RuntimeResourceManager();
	AssetDatabase* database = editor_state->asset_database;

	if (initialize) {
		size_t BUFFER_CAPACITY = 256;
		void* allocation = drawer.GetMainAllocatorBuffer(sizeof(char) * BUFFER_CAPACITY);
		data->selected_asset.InitializeFromBuffer(allocation, 0, BUFFER_CAPACITY);
		data->selected_asset_type = ECS_ASSET_TYPE_COUNT;
	}
	else {
		auto iterate = [&](ECS_ASSET_TYPE asset_type, auto stream) {
			const char* string = ConvertAssetTypeString(asset_type);
			drawer.CollapsingHeader(string, &data->opened_headers[asset_type], [&]() {
				struct SelectData {
					inline unsigned int* Size() {
						return &label_size;
					}

					AssetExplorerData* data;
					unsigned int label_size;
					ECS_ASSET_TYPE type;
				};

				auto select_action = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					SelectData* data = (SelectData*)_data;
					Stream<char> label = function::GetCoallescedStreamFromType(data).As<char>();
					data->data->selected_asset.Copy(label);
					data->data->selected_asset_type = data->type;
				};

				for (size_t index = 0; index < stream.size; index++) {
					const void* asset = stream.buffer + index;
					Stream<char> asset_name = GetAssetName(asset, asset_type);

					size_t select_storage[512];
					unsigned int write_size = 0;
					SelectData* select_data = function::CreateCoallescedStreamIntoType<SelectData>(select_storage, asset_name, &write_size);
					select_data->data = data;
					select_data->type = asset_type;

					UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
					row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });

					UIDrawConfig config;
					size_t configuration = 0;
					row_layout.GetTransform(config, configuration);

					ECS_STACK_CAPACITY_STREAM(char, asset_string, 256);
					AssetToString(asset, asset_type, asset_string);

					ECS_STACK_CAPACITY_STREAM(char, long_asset_string, 256);
					AssetToString(asset, asset_type, long_asset_string, true);

					if (long_asset_string.size != asset_string.size) {
						UIConfigToolTip tooltip_config;
						tooltip_config.characters = long_asset_string;
						config.AddFlag(tooltip_config);
						configuration |= UI_CONFIG_RECTANGLE_TOOL_TIP;
					}
					drawer.Button(configuration, config, asset_string, { select_action, select_data, write_size });
					drawer.NextRow();
				}
			});
		};

		drawer.Text("Assets");
		drawer.NextRow();

		// Display the metadata active in the database
		iterate(ECS_ASSET_MESH, database->mesh_metadata.ToStream());
		iterate(ECS_ASSET_TEXTURE, database->texture_metadata.ToStream());
		iterate(ECS_ASSET_GPU_SAMPLER, database->gpu_sampler_metadata.ToStream());
		iterate(ECS_ASSET_SHADER, database->shader_metadata.ToStream());
		iterate(ECS_ASSET_MATERIAL, database->material_asset.ToStream());
		iterate(ECS_ASSET_MISC, database->misc_asset.ToStream());
	}

}

void AssetExplorerSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int index = *(unsigned int*)stack_memory;

	AssetExplorerData* data = (AssetExplorerData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));
	data->editor_state = editor_state;
	data->selected_asset_type = ECS_ASSET_TYPE_COUNT;
	data->selected_asset = { nullptr, 0, 0 };
	memset(data->opened_headers, 0, sizeof(data->opened_headers));

	descriptor.draw = AssetExplorerDraw;

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