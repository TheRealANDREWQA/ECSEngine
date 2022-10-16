#include "editorpch.h"
#include "AssetOverrides.h"
#include "ECSEngineUI.h"
#include "../Editor/EditorState.h"
#include "../Assets/AssetManagement.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Project/ProjectFolders.h"

using namespace ECSEngine;

#define LAZY_EVALUATION_MILLISECONDS 250

struct GlobalOverrideData {
	EditorState* editor_state;
};

void ConvertMetadataNameAndFileToSelection(Stream<char> name, Stream<wchar_t> file, CapacityStream<char>& characters) {
	ECS_FORMAT_STRING(characters, "{#} ({#})", file, name);
}

struct BaseDrawData {
	EditorState* editor_state;
	CapacityStream<char> filter;
	ResizableLinearAllocator resizable_allocator;
	Timer timer;
	Stream<Stream<wchar_t>> paths;
	unsigned int sandbox_index;
	unsigned int* handle;
};

struct MeshDrawData {
	BaseDrawData base_data;
};

void FilterBar(UIDrawer* drawer, BaseDrawData* base_data) {
	if (drawer->initializer) {
		const size_t capacity = 256;
		base_data->filter.buffer = (char*)drawer->GetMainAllocatorBuffer(sizeof(char) * capacity);
		base_data->filter.capacity = capacity;
		base_data->filter.size = 0;
	}

	drawer->SetCurrentPositionToHeader();

	UIDrawConfig config;
	UIConfigWindowDependentSize size;
	size.scale_factor.x = drawer->GetWindowSizeScaleUntilBorder(true).x;
	size.offset = drawer->region_render_offset;
	config.AddFlag(size);

	UIConfigTextInputHint hint;
	hint.characters = "Search";
	config.AddFlag(hint);

	drawer->TextInput(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_DO_NOT_ADVANCE, config, "Filter", &base_data->filter);
	drawer->OffsetY(drawer->GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, size.scale_factor).y);	
	drawer->SetCurrentX(drawer->GetNextRowXPosition());
}

void InitializeBaseData(BaseDrawData* base_data) {
	const size_t initial_allocation_size = ECS_KB * 64;
	const size_t backup_allocation_size = ECS_KB * 256;
	base_data->resizable_allocator = ResizableLinearAllocator(
		base_data->editor_state->editor_allocator->Allocate(initial_allocation_size), 
		initial_allocation_size, 
		backup_allocation_size,
		base_data->editor_state->EditorAllocator()
	);
	// Make sure to trigger the retrieval immediately afterwards
	base_data->timer.DelayStart(LAZY_EVALUATION_MILLISECONDS);
}

void LazyRetrievalOfPaths(BaseDrawData* base_data, ECS_ASSET_TYPE type) {
	if (base_data->timer.GetDuration_ms() > LAZY_EVALUATION_MILLISECONDS) {
		base_data->resizable_allocator.Clear();
		base_data->paths = base_data->editor_state->asset_database->GetMetadatasForType(type, GetAllocatorPolymorphic(&base_data->resizable_allocator));
		base_data->timer.SetNewStart();
	}
}

struct SelectActionData {
	unsigned int Write(Stream<wchar_t> path) {
		memcpy(function::OffsetPointer(this, sizeof(*this)), path.buffer, path.size * sizeof(wchar_t));
		selection_size = path.size;
		return sizeof(*this) + sizeof(wchar_t) * path.size;
	}

	Stream<wchar_t> GetSelection() const {
		return { function::OffsetPointer(this, sizeof(*this)), selection_size };
	}

	EditorState* editor_state;
	unsigned int* handle;
	ECS_ASSET_TYPE type;

	unsigned int selection_size;
	unsigned int sandbox_index;
};

void SelectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectActionData* data = (SelectActionData*)_data;
	Stream<wchar_t> selection = data->GetSelection();

	ECS_STACK_CAPACITY_STREAM(char, name, 256);
	ECS_STACK_CAPACITY_STREAM(wchar_t, file, 256);
	AssetDatabase::ExtractNameFromFile(selection, name);
	AssetDatabase::ExtractFileFromFile(selection, file);

	// If now it is loaded, launch the resource manager load task as well
	RegisterSandboxAsset(data->editor_state, data->sandbox_index, name, file, data->type, data->handle);

	system->PushDestroyWindowHandler(system->GetWindowIndexFromBorder(dockspace, border_index));
}

// THe index inside the path stream
void CreateSelectActionData(SelectActionData* select_data, BaseDrawData* base_data, ECS_ASSET_TYPE asset_type) {
	select_data->editor_state = base_data->editor_state;
	select_data->handle = base_data->handle;
	select_data->sandbox_index = base_data->sandbox_index;
	select_data->type = asset_type;
}

void MeshDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	MeshDrawData* data = (MeshDrawData*)window_data;

	FilterBar(&drawer, &data->base_data);
	if (initialize) {
		InitializeBaseData(&data->base_data);
	}

	LazyRetrievalOfPaths(&data->base_data, ECS_ASSET_MESH);

	drawer.SetDrawMode(ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE, 2, 0.005f);

	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	transform.scale.y = 2.0f;
	config.AddFlag(transform);

	const size_t BASE_CONFIGURATION = UI_CONFIG_MAKE_SQUARE | UI_CONFIG_RELATIVE_TRANSFORM;

	size_t _select_storage[256];
	SelectActionData* select_data = (SelectActionData*)_select_storage;
	CreateSelectActionData(select_data, &data->base_data, ECS_ASSET_MESH);

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectAssetsFolder(data->base_data.editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	unsigned int base_size = absolute_path.size;

	for (size_t index = 0; index < data->base_data.paths.size; index++) {
		absolute_path.size = base_size;
		unsigned int write_size = select_data->Write(data->base_data.paths[index]);

		AssetDatabase::ExtractFileFromFile(data->base_data.paths[index], absolute_path);
		ResourceView thumbnail_view = FileExplorerGetMeshThumbnail(data->base_data.editor_state, absolute_path);
		if (thumbnail_view.view != nullptr) {
			drawer.SpriteButton(BASE_CONFIGURATION, config, { SelectAction, select_data, write_size }, thumbnail_view);
		}
		else {
			// Draw a file texture instead
			drawer.SpriteButton(BASE_CONFIGURATION, config, { SelectAction, select_data, write_size }, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, drawer.color_theme.text);
		}
	}
}

struct TextureOverrideData {
	EditorState* editor_state;
};

void TextureOverrideData(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);
}

struct AcquireCallbackData {
	EditorState* editor_state;
	ECS_ASSET_TYPE asset_type;
	unsigned int handle;
	CapacityStream<char>* selection;
};

void AcquireCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	AcquireCallbackData* data = (AcquireCallbackData*)_data;
	bool success = CreateAsset(data->editor_state, data->handle, data->asset_type);

	if (success) {
		Stream<char> asset_name = data->editor_state->asset_database->GetAssetName(data->handle, data->asset_type);
		Stream<wchar_t> asset_file = data->editor_state->asset_database->GetAssetPath(data->handle, data->asset_type);

		Stream<char> final_selection = asset_name;
		ECS_STACK_CAPACITY_STREAM(char, converted_name, 256);

		if (asset_file.size > 0) {
			ConvertMetadataNameAndFileToSelection(asset_name, asset_file, converted_name);
			final_selection = converted_name;

		}
		data->selection->Copy(final_selection);
	}
	else {
		EditorSetConsoleError("Failed to acquire the drag.");
	}
}

#pragma region Drawers

struct OverrideBaseData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	CapacityStream<char> selection;
};

struct OverrideMeshData {
	OverrideBaseData base_data;
};

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideMeshHandle) {
	OverrideMeshData* data = (OverrideMeshData*)extra_data;
	AssetDatabase* database = data->base_data.editor_state->asset_database;
	
	unsigned int* handle = (unsigned int*)field_data;

	MeshDrawData draw_data;
	draw_data.base_data.editor_state = data->base_data.editor_state;
	draw_data.base_data.handle = handle;
	draw_data.base_data.sandbox_index = data->base_data.sandbox_index;

	UIWindowDescriptor window_descriptor;
	window_descriptor.draw = MeshDraw;
	window_descriptor.window_data = &draw_data;
	window_descriptor.window_data_size = sizeof(draw_data);
	window_descriptor.window_name.size = 0;
	window_descriptor.initial_size_x = 0.4f;
	window_descriptor.initial_size_y = 0.7f;

	// Update the selection if the handle has changed
	ECS_STACK_CAPACITY_STREAM(char, composite_string, 512);
	unsigned int handle_value = *handle;
	if (handle_value != -1) {
		Stream<char> name = data->base_data.editor_state->asset_database->GetAssetName(handle_value, ECS_ASSET_MESH);
		Stream<wchar_t> file = data->base_data.editor_state->asset_database->GetAssetPath(handle_value, ECS_ASSET_MESH);

		if (file.size > 0) {
			// Name and file. Put the file first and the name in paranthesis
			ECS_FORMAT_STRING(composite_string, "{#} {{#})", file, name);
		}
		else {
			composite_string = name;
		}
	}
	data->base_data.selection.Copy(composite_string);

	// Draw the field as a selection input
	drawer->SelectionInput(configuration, *config, field_name, &data->base_data.selection, ECS_TOOLS_UI_TEXTURE_FOLDER, &window_descriptor);
}

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideTextureHandle) {

}

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideGPUSamplerHandle) {
	
}

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideShaderHandle) {

}

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideMaterialHandle) {

}

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideMiscHandle) {

}

#pragma endregion

#pragma region Initializers 

ECS_UI_REFLECTION_INSTANCE_INITIALIZE_OVERRIDE(AssetInitialize) {
	// Set the editor state and selection buffer
	OverrideBaseData* data = (OverrideBaseData*)_data;
	GlobalOverrideData* global_data = (GlobalOverrideData*)_global_data;

	const size_t capacity = 256;

	data->selection.buffer = (char*)drawer->allocator->Allocate(sizeof(char) * capacity, alignof(char));
	data->selection.size = 0;
	data->selection.capacity = capacity;

	data->editor_state = global_data->editor_state;
}

#pragma endregion

#pragma region Deallocators

ECS_UI_REFLECTION_INSTANCE_DEALLOCATE_OVERRIDE(AssetDeallocate) {
	// Every asset has a OverrideBaseData as the first field
	OverrideBaseData* data = (OverrideBaseData*)_data;
	Deallocate(allocator, data->selection.buffer);
}

#pragma endregion

#pragma region Modifiers

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideSetSandboxIndex) {
	OverrideBaseData* data = (OverrideBaseData*)_data;
	AssetOverrideSetSandboxIndexData* modify_data = (AssetOverrideSetSandboxIndexData*)user_data;
	data->sandbox_index = modify_data->sandbox_index;
}

#pragma endregion

void SetUIOverrideBase(EditorState* editor_state, UIReflectionFieldOverride* override, AllocatorPolymorphic allocator) {
	override->initialize_function = AssetInitialize;
	override->deallocate_function = AssetDeallocate;
	GlobalOverrideData* override_data = (GlobalOverrideData*)Allocate(allocator, sizeof(GlobalOverrideData));
	override_data->editor_state = editor_state;
	override->global_data = override_data;
	override->global_data_size = sizeof(GlobalOverrideData);
}

void GetEntityComponentUIOverrides(EditorState* editor_state, UIReflectionFieldOverride* overrides, AllocatorPolymorphic allocator) {
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		SetUIOverrideBase(editor_state, overrides + index, allocator);
	}

	overrides[ECS_ASSET_MESH].draw_function = OverrideMeshHandle;
	overrides[ECS_ASSET_MESH].tag = STRING(ECS_MESH_HANDLE);
	overrides[ECS_ASSET_MESH].draw_data_size = sizeof(OverrideMeshData);

	overrides[ECS_ASSET_TEXTURE].draw_function = OverrideMeshHandle;
	overrides[ECS_ASSET_TEXTURE].tag = STRING(ECS_MESH_HANDLE);
	overrides[ECS_ASSET_TEXTURE].draw_data_size = sizeof(OverrideMeshData);

	overrides[ECS_ASSET_GPU_SAMPLER].draw_function = OverrideMeshHandle;
	overrides[ECS_ASSET_GPU_SAMPLER].tag = STRING(ECS_MESH_HANDLE);
	overrides[ECS_ASSET_GPU_SAMPLER].draw_data_size = sizeof(OverrideMeshData);

	overrides[ECS_ASSET_SHADER].draw_function = OverrideMeshHandle;
	overrides[ECS_ASSET_SHADER].tag = STRING(ECS_MESH_HANDLE);
	overrides[ECS_ASSET_SHADER].draw_data_size = sizeof(OverrideMeshData);

	overrides[ECS_ASSET_MATERIAL].draw_function = OverrideMeshHandle;
	overrides[ECS_ASSET_MATERIAL].tag = STRING(ECS_MESH_HANDLE);
	overrides[ECS_ASSET_MATERIAL].draw_data_size = sizeof(OverrideMeshData);

	overrides[ECS_ASSET_MISC].draw_function = OverrideMeshHandle;
	overrides[ECS_ASSET_MISC].tag = STRING(ECS_MESH_HANDLE);
	overrides[ECS_ASSET_MISC].draw_data_size = sizeof(OverrideMeshData);
}