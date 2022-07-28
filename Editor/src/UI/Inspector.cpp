#include "editorpch.h"
#include "Inspector.h"
#include "InspectorData.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorPalette.h"
#include "../HelperWindows.h"
#include "../Editor/EditorEvent.h"
#include "FileExplorer.h"
#include "../Modules/Module.h"
#include "ECSEngineRendering.h"
#include "../Modules/ModuleSettings.h"
#include "../Project/ProjectFolders.h"

using namespace ECSEngine;
ECS_TOOLS;

constexpr float2 WINDOW_SIZE = float2(0.5f, 1.2f);

constexpr float ICON_SIZE = 0.07f;

constexpr size_t POINTER_CAPACITY = 32;
constexpr float2 TEXT_FILE_BORDER_OFFSET = { 0.005f, 0.005f };
constexpr float TEXT_FILE_ROW_OFFSET = 0.008f;
constexpr float C_FILE_ROW_OFFSET = 0.015f;

constexpr size_t INSPECTOR_FLAG_LOCKED = 1 << 0;

#define MESH_PREVIEW_HEIGHT 0.4f
#define MESH_PREVIEW_ROTATION_DELTA_FACTOR 125.0f
#define MESH_PREVIEW_RADIUS_DELTA_FACTOR 0.00125f

#define DRAW_MODULE_LINEAR_ALLOCATOR_CAPACITY ECS_KB
#define DRAW_MODULE_SETTINGS_ALLOCATOR_CAPACITY ECS_KB * 128
#define DRAW_MODULE_SETTINGS_ALLOCATOR_POOL_COUNT 1024
#define DRAW_MODULE_SETTINGS_ALLOCATOR_BACKUP_SIZE ECS_KB * 512

void InitializeInspectorTable(EditorState* editor_state);

void InitializeInspectorInstance(EditorState* editor_state, unsigned int index);

void InspectorDestroyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_additional_data;
	unsigned int* inspector_index = (unsigned int*)_data;

	DestroyInspectorInstance(editor_state, *inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {	
	unsigned int inspector_index = *(unsigned int*)stack_memory;

	descriptor.draw = InspectorWindowDraw;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;

	descriptor.destroy_action = InspectorDestroyCallback;
	descriptor.destroy_action_data = stack_memory;
	descriptor.destroy_action_data_size = sizeof(inspector_index);

	CapacityStream<char> inspector_name(function::OffsetPointer(stack_memory, sizeof(unsigned int)), 0, 128);
	GetInspectorName(inspector_index, inspector_name);
	descriptor.window_name = inspector_name.buffer;
}

// ----------------------------------------------------------------------------------------------------------------------------

void AddInspectorTableFunction(InspectorTable* table, InspectorFunctions function, const wchar_t* _identifier) {
	ResourceIdentifier identifier(_identifier);

	ECS_ASSERT(table->Find(identifier) == -1);
	ECS_ASSERT(!table->Insert(function, identifier));
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, Stream<wchar_t> _identifier) {
	ResourceIdentifier identifier(_identifier.buffer, _identifier.size * sizeof(wchar_t));

	return editor_state->inspector_manager.function_table.TryGetValue(identifier, function);
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, const wchar_t* _identifier) {
	return TryGetInspectorTableFunction(editor_state, function, ToStream(_identifier));
}

// ----------------------------------------------------------------------------------------------------------------------------

void GetInspectorsForModule(const EditorState* editor_state, unsigned int target_sandbox, CapacityStream<unsigned int>& inspector_indices) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].target_sandbox == target_sandbox) {
			inspector_indices.AddSafe(index);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetMatchingIndexFromRobin(EditorState* editor_state, unsigned int target_sandbox) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		unsigned int current_inspector = (editor_state->inspector_manager.round_robin_index[target_sandbox] + index) % editor_state->inspector_manager.data.size;
		if (editor_state->inspector_manager.data[current_inspector].target_sandbox == target_sandbox || !IsInspectorLocked(editor_state, current_inspector)) {
			editor_state->inspector_manager.round_robin_index[target_sandbox] = current_inspector + 1 == editor_state->inspector_manager.data.size ? 
				0 : current_inspector + 1;
			return current_inspector;
		}
	}

	return -1;
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetMatchingIndexFromRobin(EditorState* editor_state) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		unsigned int current_inspector = (editor_state->inspector_manager.round_robin_index[editor_state->sandboxes.size] + index) % editor_state->inspector_manager.data.size;
		if (!IsInspectorLocked(editor_state, current_inspector)) {
			editor_state->inspector_manager.round_robin_index[editor_state->sandboxes.size] = current_inspector + 1 == editor_state->inspector_manager.data.size ? 
				0 : current_inspector + 1;
			return current_inspector;
		}
	}

	return -1;
}

// Just calls the clean function 
void CleanInspectorData(EditorState* editor_state, unsigned int inspector_index) {
	editor_state->inspector_manager.data[inspector_index].clean_function(editor_state, editor_state->inspector_manager.data[inspector_index].draw_data);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconTexture(UIDrawer* drawer, ResourceView texture, Color color = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = drawer->GetRelativeTransformFactorsZoomed(icon_size);
	config.AddFlag(transform);

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM, config, texture, color);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIcon(UIDrawer* drawer, const wchar_t* path, Color color = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = drawer->GetRelativeTransformFactorsZoomed(icon_size);
	config.AddFlag(transform);

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM, config, path, color);
}

// ----------------------------------------------------------------------------------------------------------------------------

// A double sprite icon
void InspectorIconDouble(UIDrawer* drawer, const wchar_t* icon0, const wchar_t* icon1, Color icon_color0 = ECS_COLOR_WHITE, Color icon_color1 = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = drawer->GetRelativeTransformFactorsZoomed(icon_size);
	config.AddFlag(transform);

	drawer->SpriteRectangleDouble(UI_CONFIG_RELATIVE_TRANSFORM, config, icon0, icon1, icon_color0, icon_color1);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconNameAndPath(UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	Stream<wchar_t> folder_name = function::PathStem(path);
	ECS_TEMP_ASCII_STRING(ascii_path, 256);
	function::ConvertWideCharsToASCII(path.buffer, ascii_path.buffer, path.size + 1, 256);
	drawer->TextLabel(UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT, config, ascii_path.buffer + (folder_name.buffer - path.buffer));
	drawer->NextRow();

	UIDrawerSentenceFitSpaceToken fit_space_token;
	fit_space_token.token = '\\';
	config.AddFlag(fit_space_token);
	drawer->Sentence(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_SENTENCE_FIT_SPACE | UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN, config, ascii_path.buffer);
	drawer->NextRow();
	drawer->CrossLine();
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorShowButton(UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button(UI_CONFIG_DO_NOT_CACHE, config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorOpenAndShowButton(UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button(UI_CONFIG_DO_NOT_CACHE, config, "Open", { OpenFileWithDefaultApplicationStreamAction, &path, sizeof(path) });

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer->GetLabelScale("Show");
	transform.position = drawer->GetAlignedToRight(transform.scale.x);
	config.AddFlag(transform);
	drawer->Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFileTimesInternal(UIDrawer* drawer, const char* creation_time, const char* write_time, const char* access_time, bool success) {
	UIDrawConfig config;
	// Display file times
	if (success) {
		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Creation time: ");
		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, creation_time);
		drawer->NextRow();

		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Access time: ");
		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, access_time);
		drawer->NextRow();

		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Last write time: ");
		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, write_time);
		drawer->NextRow();
	}
	// Else error message
	else {
		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Could not retrieve directory times.");
		drawer->NextRow();
	}
}

void InspectorDrawFileTimes(UIDrawer* drawer, const wchar_t* path) {
	char creation_time[256];
	char write_time[256];
	char access_time[256];
	bool success = OS::GetFileTimes(path, creation_time, access_time, write_time);

	InspectorDrawFileTimesInternal(drawer, creation_time, write_time, access_time, success);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFileTimes(UIDrawer* drawer, Stream<wchar_t> path) {
	char creation_time[256];
	char write_time[256];
	char access_time[256];
	bool success = OS::GetFileTimes(path, creation_time, access_time, write_time);

	InspectorDrawFileTimesInternal(drawer, creation_time, write_time, access_time, success);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorCleanNothing(EditorState* editor_state, void* data) {}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawNothing(EditorState* editor_state, void* data, UIDrawer* drawer) {
	UIDrawConfig config;
	drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ALIGN_TO_ROW_Y, config, "Nothing is selected.");
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFolderInfo(EditorState* editor_state, void* data, UIDrawer* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Test to see if the folder still exists - if it does not revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FOLDER);

	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	struct OpenFolderData {
		EditorState* state;
		const wchar_t* path;
	};

	auto OpenFileExplorerFolder = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		OpenFolderData* data = (OpenFolderData*)_data;
		Stream<wchar_t> path = ToStream(data->path);
		ChangeFileExplorerDirectory(data->state, path);
	};

	UIDrawConfig config;

	config.flag_count = 0;
	OpenFolderData open_data;
	open_data.path = path;
	open_data.state = editor_state;
	drawer->Button(UI_CONFIG_DO_NOT_CACHE, config, "Open", { OpenFileExplorerFolder, &open_data, sizeof(open_data) });

	UIConfigAbsoluteTransform absolute_transform;
	absolute_transform.scale = drawer->GetLabelScale("Show");
	absolute_transform.position = drawer->GetAlignedToRight(absolute_transform.scale.x);
	config.AddFlag(absolute_transform);
	drawer->Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Show", { LaunchFileExplorerAction, (void*)path, 0 });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTexture(EditorState* editor_state, void* data, UIDrawer* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - if it doesn't revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIcon(drawer, path);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	InspectorOpenAndShowButton(drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawBlankFile(EditorState* editor_state, void* data, UIDrawer* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, drawer->color_theme.text);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);
	InspectorOpenAndShowButton(drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTextFileImplementation(EditorState* editor_state, void* data, UIDrawer* drawer, const wchar_t* icon_texture, float row_offset) {
	EDITOR_STATE(editor_state);
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, icon_texture, drawer->color_theme.text, drawer->color_theme.theme);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	DrawTextFileEx(drawer, path, TEXT_FILE_BORDER_OFFSET, TEXT_FILE_ROW_OFFSET);

	InspectorOpenAndShowButton(drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTextFile(EditorState* editor_state, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_TEXT, TEXT_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCTextFile(EditorState* editor_state, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_C, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCppTextFile(EditorState* editor_state, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_CPP, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawHlslTextFile(EditorState* editor_state, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_SHADER, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

struct InspectorDrawMeshFileData {
	Stream<wchar_t> path;
	GLTFThumbnail thumbnail;
	CoallescedMesh* mesh;
	float radius_delta;
	float2 rotation_delta;
};

void InspectorCleanMeshFile(EditorState* editor_state, void* _data) {
	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;

	// Unload the mesh and free the resources only if the mesh was previously loaded
	FileExplorerMeshThumbnail file_explorer_thumbnail;
	if (editor_state->file_explorer_data->mesh_thumbnails.TryGetValue(ResourceIdentifier(data->path.buffer, data->path.size), file_explorer_thumbnail)) {
		if (file_explorer_thumbnail.could_be_read) {
			editor_state->resource_manager->UnloadCoallescedMeshImplementation(data->mesh);
			// Release the texture aswell
			Graphics* graphics = editor_state->UIGraphics();
			Texture2D texture = data->thumbnail.texture.GetResource();
			graphics->FreeResource(texture);
			graphics->FreeResource(data->thumbnail.texture);
		}
	}
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

void InspectorDrawMeshFile(EditorState* editor_state, void* _data, UIDrawer* drawer) {
	InspectorDrawMeshFileData* data = (InspectorDrawMeshFileData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state);
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

	InspectorIconNameAndPath(drawer, data->path);

	InspectorDrawFileTimes(drawer, data->path);
	InspectorOpenAndShowButton(drawer, data->path);
	drawer->NextRow();

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
			CoallescedMesh* mesh = editor_state->resource_manager->LoadCoallescedMeshImplementation(data->path.buffer);
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

// ----------------------------------------------------------------------------------------------------------------------------

struct DrawModuleData {
	Stream<wchar_t> module_name;
	CapacityStream<wchar_t> settings_name;
	Stream<EditorModuleReflectedSetting> reflected_settings;
	MemoryManager* settings_allocator;
	LinearAllocator* linear_allocator;
	bool* header_states;
	unsigned int window_index;
	unsigned int inspector_index;
};

#pragma region Draw Module Helpers

// ----------------------------------------------------------------------------------------------------------------------------

void AllocateInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	ECS_STACK_CAPACITY_STREAM(EditorModuleReflectedSetting, settings, 64);
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(draw_data->settings_allocator);

	AllocateModuleSettings(editor_state, module_index, settings, allocator);
	draw_data->reflected_settings.InitializeAndCopy(allocator, settings);
}

void CreateInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	ECS_STACK_CAPACITY_STREAM(EditorModuleReflectedSetting, settings, 64);

	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(draw_data->settings_allocator);
	CreateModuleSettings(editor_state, module_index, settings, allocator, inspector_index);

	draw_data->reflected_settings.InitializeAndCopy(allocator, settings);
}

// ----------------------------------------------------------------------------------------------------------------------------

void DeallocateInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	DestroyModuleSettings(editor_state, module_index, draw_data->reflected_settings, inspector_index);
	draw_data->settings_allocator->Clear();
}

// ----------------------------------------------------------------------------------------------------------------------------

// Returns true if it succeded. Else false
bool LoadInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	if (draw_data->reflected_settings.size > 0) {
		DeallocateInspectorSettingsHelper(editor_state, module_index, inspector_index);
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetModuleSettingsFilePath(editor_state, module_index, draw_data->settings_name, absolute_path);
	
	CreateInspectorSettingsHelper(editor_state, module_index, inspector_index);
	bool success = LoadModuleSettings(
		editor_state, 
		module_index, 
		absolute_path, 
		draw_data->reflected_settings,
		GetAllocatorPolymorphic(draw_data->settings_allocator)
	);

	return success;
}

bool SaveInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetModuleSettingsFilePath(editor_state, module_index, draw_data->settings_name, absolute_path);

	bool success = SaveModuleSettings(
		editor_state,
		module_index,
		absolute_path,
		draw_data->reflected_settings
	);
	return success;
}

// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

struct CreateSettingsCallbackData {
	EditorState* editor_state;
	unsigned int module_index;
	unsigned int inspector_index;
};

void CreateModuleSettingsCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateSettingsCallbackData* data = (CreateSettingsCallbackData*)_data;
	CapacityStream<char>* characters = (CapacityStream<char>*)_additional_data;

	DrawModuleData* draw_data = (DrawModuleData*)data->editor_state->inspector_manager.data[data->inspector_index].draw_data;

	if (draw_data->reflected_settings.size > 0) {
		DeallocateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
		data->editor_state->editor_allocator->Deallocate(draw_data->settings_name.buffer);
	}
	CreateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);

	ECS_STACK_CAPACITY_STREAM(wchar_t, converted_name, 128);
	function::ConvertASCIIToWide(converted_name, *characters);

	draw_data->settings_name.InitializeAndCopy(GetAllocatorPolymorphic(data->editor_state->editor_allocator), converted_name);

	// Write to a file the current values
	bool success = SaveInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
	if (!success) {
		EditorSetConsoleWarn(ToStream("Could not save the new settings file."));
	}
}

void InspectorDrawModuleClean(EditorState* editor_state, void* _data) {
	EDITOR_STATE(editor_state);
	DrawModuleData* data = (DrawModuleData*)_data;
	
	if (data->linear_allocator != nullptr) {
		ui_system->RemoveWindowMemoryResource(data->window_index, data->linear_allocator->m_buffer);
		ui_system->RemoveWindowMemoryResource(data->window_index, data->linear_allocator);

		ui_system->m_memory->Deallocate(data->linear_allocator->m_buffer);
		ui_system->m_memory->Deallocate(data->linear_allocator);
	}

	if (data->header_states != nullptr) {
		ui_system->RemoveWindowMemoryResource(data->window_index, data->header_states);
		ui_system->m_memory->Deallocate(data->header_states);
	}

	if (data->settings_allocator != nullptr) {
		ui_system->RemoveWindowMemoryResource(data->window_index, data->settings_allocator);
		data->settings_allocator->Free();
	}

	editor_state->editor_allocator->Deallocate(data->module_name.buffer);
}

void InspectorDrawModule(EditorState* editor_state, void* _data, UIDrawer* drawer) {
	EDITOR_STATE(editor_state);

	DrawModuleData* data = (DrawModuleData*)_data;
	unsigned int module_index = GetModuleIndexFromName(editor_state, data->module_name);
	if (module_index == -1) {
		ChangeInspectorToNothing(editor_state);
		return;
	}
	
	LinearAllocator* linear_allocator = nullptr;
	bool* collapsing_headers_state = nullptr;
	if (data->linear_allocator == nullptr) {
		linear_allocator = (LinearAllocator*)drawer->GetMainAllocatorBuffer(sizeof(LinearAllocator));
		*linear_allocator = LinearAllocator(drawer->GetMainAllocatorBuffer(DRAW_MODULE_LINEAR_ALLOCATOR_CAPACITY), DRAW_MODULE_LINEAR_ALLOCATOR_CAPACITY);
		data->linear_allocator = linear_allocator;
		data->window_index = drawer->window_index;

		data->header_states = (bool*)drawer->GetMainAllocatorBuffer(sizeof(bool) * 32);
		memset(data->header_states, 0, sizeof(bool) * 32);
		collapsing_headers_state = data->header_states;

		data->settings_allocator = (MemoryManager*)drawer->GetMainAllocatorBuffer(sizeof(MemoryManager));
		*data->settings_allocator = MemoryManager(
			DRAW_MODULE_SETTINGS_ALLOCATOR_CAPACITY,
			DRAW_MODULE_SETTINGS_ALLOCATOR_POOL_COUNT,
			DRAW_MODULE_SETTINGS_ALLOCATOR_BACKUP_SIZE,
			editor_state->GlobalMemoryManager()
		);
	}
	else {
		linear_allocator = data->linear_allocator;
		linear_allocator->Clear();

		collapsing_headers_state = data->header_states;
	}

	UIDrawConfig config;

	ECS_STACK_CAPACITY_STREAM(wchar_t, setting_absolute_path, 512);

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG, drawer->color_theme.text, drawer->color_theme.theme);
	
	ECS_STACK_CAPACITY_STREAM(char, ascii_name, 256);
	function::ConvertWideCharsToASCII(data->module_name, ascii_name);
	ascii_name.Add('\0');
	drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ALIGN_TO_ROW_Y, config, ascii_name.buffer);
	drawer->NextRow();

	drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Project Settings ");

	// Display the current setting name - if it has one
	if (data->settings_name.size > 0) {
		GetProjectConfigurationModuleFolder(editor_state, setting_absolute_path);
		setting_absolute_path.Add(ECS_OS_PATH_SEPARATOR);
		setting_absolute_path.AddStreamSafe(data->settings_name);
		setting_absolute_path[setting_absolute_path.size] = L'\0';

		// Display the configuration's name
		ECS_STACK_CAPACITY_STREAM(char, ascii_setting_name, 256);
		function::ConvertWideCharsToASCII(data->settings_name, ascii_setting_name);

		// Null terminate the string
		ascii_setting_name.AddSafe('\0');
		drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ALIGN_TO_ROW_Y, config, ascii_setting_name.buffer);
	}
	else {
		drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ALIGN_TO_ROW_Y, config, "No module settings path is selected.");
	}
	drawer->NextRow();

	// Display the reflected types for this module
	ECS_STACK_CAPACITY_STREAM(unsigned int, reflected_type_indices, 32);
	GetModuleSettingsUITypesIndices(editor_state, module_index, reflected_type_indices);

	drawer->CrossLine();
	if (reflected_type_indices.size > 0) {
		// Coallesce the string
		ECS_STACK_CAPACITY_STREAM(const char*, type_names, 32);
		for (size_t index = 0; index < reflected_type_indices.size; index++) {
			UIReflectionType type = editor_state->module_reflection->GetType(reflected_type_indices[index]);
			type_names[index] = type.name;
		}
		type_names.size = reflected_type_indices.size;
		drawer->LabelList(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT, config, "Module settings types", type_names);
		drawer->NextRow();
	}
	// No settings for this module
	else {
		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "The module does not have settings types.");
		drawer->NextRow();
	}
	drawer->CrossLine();

	// Allow the user to create a different profile
	auto create_new_setting = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		CreateSettingsCallbackData* data = (CreateSettingsCallbackData*)_data;

		TextInputWizardData wizard_data;
		wizard_data.input_name = "Setting name";
		wizard_data.window_name = "Choose Setting Name";
		wizard_data.callback = CreateModuleSettingsCallback;
		wizard_data.callback_data = data;
		wizard_data.callback_data_size = sizeof(*data);

		CreateTextInputWizard(&wizard_data, system);
	};

	UIConfigActiveState create_settings_active_state;
	create_settings_active_state.state = reflected_type_indices.size != 0;

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = 1.0f;
	config.AddFlag(dependent_size);
	config.AddFlag(create_settings_active_state);

	CreateSettingsCallbackData create_data = { editor_state, module_index };
	drawer->Button(
		UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE,
		config, 
		"Create new setting",
		{ create_new_setting, &create_data, sizeof(create_data) }
	);
	drawer->NextRow();

	// Enumerate all the other possible settings
	ECS_STACK_CAPACITY_STREAM(wchar_t, settings_folder, 512);
	GetModuleSettingsFolderPath(editor_state, module_index, settings_folder);

	const wchar_t* settings_extensions[] = { MODULE_SETTINGS_EXTENSION };

	struct FunctorData {
		UIDrawer* drawer;
		EditorState* editor_state;
		LinearAllocator* temp_allocator;
		unsigned int module_index;
		unsigned int inspector_index;
		CapacityStream<wchar_t>* active_settings;
	};

	FunctorData functor_data = { drawer, editor_state, linear_allocator, module_index, data->inspector_index, &data->settings_name };

	drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Available settings ");
	drawer->NextRow();
	drawer->CrossLine();

	ForEachFileInDirectoryWithExtension(settings_folder, { settings_extensions, std::size(settings_extensions) }, &functor_data,
		[](const wchar_t* path, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			Stream<wchar_t> stream_path = ToStream(path);
			Stream<wchar_t> stem = function::PathStem(stream_path);

			bool is_active = data->active_settings != nullptr ? function::CompareStrings(stem, *data->active_settings) : false;
			UIDrawConfig config;
			UIConfigCheckBoxCallback callback;

			struct SetNewSettingData {
				EditorState* editor_state;
				Stream<wchar_t> name;
				CapacityStream<wchar_t>* active_settings;
				unsigned int module_index;
				unsigned int inspector_index;
			};

			auto set_new_setting = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SetNewSettingData* data = (SetNewSettingData*)_data;
				if (data->active_settings->size > 0) {
					data->editor_state->editor_allocator->Deallocate(data->active_settings->buffer);
				}
				data->active_settings->InitializeAndCopy(GetAllocatorPolymorphic(data->editor_state->editor_allocator), data->name);
				LoadInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
			};

			wchar_t* name_allocation = (wchar_t*)data->temp_allocator->Allocate((stem.size + 1) * sizeof(wchar_t));
			stem.CopyTo(name_allocation);
			name_allocation[stem.size] = L'\0';

			ECS_STACK_CAPACITY_STREAM(char, ascii_name, 64);
			function::ConvertWideCharsToASCII(stem, ascii_name);

			SetNewSettingData callback_data = { 
				data->editor_state, 
				{name_allocation, stem.size}, 
				data->active_settings,
				data->module_index,
				data->inspector_index
			};
			callback.handler.action = set_new_setting;
			callback.handler.data = &callback_data;
			callback.handler.data_size = sizeof(callback_data);
			callback.disable_value_to_modify = true;
			config.AddFlag(callback);

			data->drawer->CheckBox(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_CHECK_BOX_CALLBACK, config, ascii_name.buffer, &is_active);

			UIConfigAbsoluteTransform right_transform;
			right_transform.scale = data->drawer->GetSquareScale(data->drawer->layout.default_element_y * 0.75f);
			right_transform.position = data->drawer->GetAlignedToRight(right_transform.scale.x);
			config.AddFlag(right_transform);
			data->drawer->SpriteRectangle(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LATE_DRAW, config, ECS_TOOLS_UI_TEXTURE_X, data->drawer->color_theme.text);

			struct DeleteActionData {
				const wchar_t* filename;
				EditorState* editor_state;
				unsigned int module_index;
				unsigned int inspector_index;
				CapacityStream<wchar_t>* active_settings;
			};

			auto delete_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				DeleteActionData* data = (DeleteActionData*)_data;

				ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
				GetModuleSettingsFilePath(data->editor_state, data->module_index, ToStream(data->filename), absolute_path);
				bool success = RemoveFile(absolute_path);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(error_message, "Could not delete module settings file {#}.", absolute_path);
					EditorSetConsoleError(error_message);
				}
				else {
					// Check to see if that is the current setting
					if (data->active_settings->size > 0 && function::CompareStrings(ToStream(data->filename), *data->active_settings)) {
						DeallocateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
						data->editor_state->editor_allocator->Deallocate(data->active_settings->buffer);
						*data->active_settings = { nullptr, 0, 0 };
					}
				}
			};
			DeleteActionData action_data = { (wchar_t*)name_allocation, data->editor_state, data->module_index, data->inspector_index, data->active_settings };
			data->drawer->AddDefaultClickableHoverable(
				{ right_transform.position - data->drawer->region_render_offset }, 
				right_transform.scale,
				{ delete_action, &action_data, sizeof(action_data) }
			);

			data->drawer->NextRow();

			return true;
		});

	drawer->CrossLine();

	// Present the data now - if a path is selected

	if (data->settings_name.size > 0) {
		drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Settings Variables");
		drawer->NextRow();

		drawer->CrossLine();

		struct SaveFileAfterChangeData {
			EditorState* editor_state;
			unsigned int module_index;
			unsigned int inspector_index;
		};

		auto save_file_after_change = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SaveFileAfterChangeData* data = (SaveFileAfterChangeData*)_data;
			bool success = SaveInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(console_string, "An error occured when saving the module setting file for inspector {#} after value change. "
					"The modification has not been commited.", data->inspector_index);
				EditorSetConsoleError(console_string);
			}
		};

		ECS_STACK_CAPACITY_STREAM(unsigned int, instance_indices, 64);
		GetModuleSettingsUIInstancesIndices(editor_state, module_index, data->inspector_index, instance_indices);

		SaveFileAfterChangeData save_file_data = { editor_state, module_index };

		if (instance_indices.size > 0) {
			UIReflectionDrawConfig ui_reflection_configs[8];

			UIConfigTextInputCallback text_input_callback;
			text_input_callback.handler = { save_file_after_change, &save_file_data, sizeof(save_file_data) };

			UIConfigPathInputCallback path_input_callback;
			path_input_callback.callback = { save_file_after_change, &save_file_data, sizeof(save_file_data) };

			ui_reflection_configs[0].configurations = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			ui_reflection_configs[0].index[0] = UIReflectionIndex::Count;

			ui_reflection_configs[1].index[0] = UIReflectionIndex::IntegerInput;
			ui_reflection_configs[1].index[1] = UIReflectionIndex::DoubleInput;
			ui_reflection_configs[1].index[2] = UIReflectionIndex::FloatInput;
			ui_reflection_configs[1].index[3] = UIReflectionIndex::IntegerInputGroup;
			ui_reflection_configs[1].index[4] = UIReflectionIndex::FloatInputGroup;
			ui_reflection_configs[1].index[5] = UIReflectionIndex::DoubleInputGroup;
			ui_reflection_configs[1].index_count = 6;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 1, &text_input_callback);

			UIConfigCheckBoxCallback check_box_callback;
			check_box_callback.handler = { save_file_after_change, &save_file_data, sizeof(save_file_data) };
			ui_reflection_configs[2].index[0] = UIReflectionIndex::CheckBox;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 2, &check_box_callback);

			ui_reflection_configs[3].index[0] = UIReflectionIndex::TextInput;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 3, &text_input_callback);

			UIConfigColorInputCallback color_input_callback;
			color_input_callback.callback = { save_file_after_change, &save_file_data, sizeof(save_file_data) };
			ui_reflection_configs[4].index[0] = UIReflectionIndex::Color;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 4, &color_input_callback);

			UIConfigComboBoxCallback combo_box_callback;
			combo_box_callback.handler = { save_file_after_change, &save_file_data, sizeof(save_file_data) };
			ui_reflection_configs[5].index[0] = UIReflectionIndex::ComboBox;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 5, &combo_box_callback);

			ui_reflection_configs[6].index[0] = UIReflectionIndex::ColorFloat;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 6, &text_input_callback);
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 6, &color_input_callback);

			ui_reflection_configs[7].index[0] = UIReflectionIndex::DirectoryInput;
			ui_reflection_configs[7].index[1] = UIReflectionIndex::FileInput;
			ui_reflection_configs[7].index_count = 2;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs + 7, &path_input_callback);

			config.flag_count = 0;
			// The window dependendent size must be adjusted to account for the indentation
			dependent_size.scale_factor.x = drawer->GetWindowSizeFactors(	
				ECS_UI_WINDOW_DEPENDENT_HORIZONTAL,
				{ drawer->region_limit.x - drawer->region_fit_space_horizontal_offset - drawer->layout.node_indentation, 0.0f }
			).x;
			config.AddFlag(dependent_size);
			for (size_t index = 0; index < instance_indices.size; index++) {
				// Display them as collapsing headers
				UIConfigCollapsingHeaderDoNotCache collapsing_do_not_cache;
				collapsing_do_not_cache.state = collapsing_headers_state + index;
				UIDrawConfig header_config;
				header_config.AddFlag(collapsing_do_not_cache);

				UIReflectionInstance* instance = editor_state->module_reflection->GetInstancePtr(instance_indices[index]);
				
				// Must null terminate the suffix
				char* separator = (char*)strchr(instance->name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR);
				*separator = '\0';

				drawer->CollapsingHeader(UI_CONFIG_DO_NOT_CACHE, header_config, instance->name, [&]() {
					editor_state->module_reflection->DrawInstance(
						instance,
						*drawer,
						config,
						{ ui_reflection_configs, std::size(ui_reflection_configs) }
					);
				});
				*separator = ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR;
			}
		}

		auto set_default_values = [](ActionData* action_data) {
			SaveFileAfterChangeData* data = (SaveFileAfterChangeData*)action_data->data;

			DrawModuleData* draw_data = (DrawModuleData*)data->editor_state->inspector_manager.data[data->inspector_index].draw_data;
			if (draw_data->settings_name.size > 0) {
				DeallocateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
			}

			AllocateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
			SetModuleDefaultSettings(data->editor_state, data->module_index, draw_data->reflected_settings);
			ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
			GetModuleSettingsFilePath(data->editor_state, data->module_index, draw_data->settings_name, absolute_path);

			bool success = SaveModuleSettings(data->editor_state, data->module_index, absolute_path, draw_data->reflected_settings);
			if (!success) {
				EditorSetConsoleError(ToStream("An error has occured when trying to save default values for module settings file. The modification has not been commited."));
			}
		};
		
		drawer->Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Default Values", { set_default_values, &save_file_data, sizeof(save_file_data) });
		config.flag_count = 0;
		drawer->NextRow();

		drawer->CrossLine();
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorWindowDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	const float PADLOCK_SIZE = 0.04f;
	const float REDUCE_FONT_SIZE = 0.9f;

	UI_PREPARE_DRAWER(initialize);
	//drawer.DisablePaddingForRenderRegion();
	//drawer.DisablePaddingForRenderSliders();

	EditorState* editor_state = (EditorState*)window_data;

	// Determine the index from the name
	const char* window_name = drawer.system->GetWindowName(drawer.window_index);
	unsigned int inspector_index = GetInspectorIndex(window_name);

	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;

	if (initialize) {
		drawer.font.size *= REDUCE_FONT_SIZE;
		drawer.font.character_spacing *= REDUCE_FONT_SIZE;

		InitializeInspectorInstance(editor_state, inspector_index);
		ChangeInspectorToNothing(editor_state, inspector_index);
	}
	else {
		// The padlock
		float zoomed_icon_size = ICON_SIZE * drawer.zoom_ptr->y;
		float padlock_size = PADLOCK_SIZE * drawer.zoom_ptr->y;

		UIDrawConfig config;
		UIConfigRelativeTransform transform;
		transform.scale.y = drawer.GetRelativeTransformFactorsZoomed({ 0.0f, PADLOCK_SIZE }).y;
		transform.offset.y = (zoomed_icon_size - padlock_size) / 2;
		config.AddFlag(transform);

		drawer.SpriteTextureBool(
			UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_MAKE_SQUARE,
			config, 
			ECS_TOOLS_UI_TEXTURE_UNLOCKED_PADLOCK, 
			ECS_TOOLS_UI_TEXTURE_LOCKED_PADLOCK,
			&data->flags, 
			INSPECTOR_FLAG_LOCKED, 
			EDITOR_GREEN_COLOR
		);
		drawer.current_row_y_scale = zoomed_icon_size;

		if (data->draw_function != nullptr) {
			data->draw_function(editor_state, data->draw_data, &drawer);
		}

	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorWindow(EditorState* editor_state, unsigned int inspector_index) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	size_t stack_memory[128];
	unsigned int* stack_inspector_index = (unsigned int*)stack_memory;
	*stack_inspector_index = inspector_index;
	InspectorSetDescriptor(descriptor, editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorDockspace(EditorState* editor_state, unsigned int inspector_index) {
	unsigned int window_index = CreateInspectorWindow(editor_state, inspector_index);

	float2 window_position = editor_state->ui_system->GetWindowPosition(window_index);
	float2 window_scale = editor_state->ui_system->GetWindowScale(window_index);
	editor_state->ui_system->CreateDockspace({ window_position, window_scale }, DockspaceType::FloatingHorizontal, window_index, false);
	return window_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorInstance(EditorState* editor_state) {
	unsigned int inspector_index = editor_state->inspector_manager.data.ReserveNewElement();
	InitializeInspectorInstance(editor_state, inspector_index);
	editor_state->inspector_manager.data.size++;
	return inspector_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

void CreateInspectorAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	unsigned int inspector_index = CreateInspectorInstance(editor_state);
	CreateInspectorDockspace(editor_state, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

// Inspector index == -1 means find the first suitable inspector
// Sandbox index != -1 means find the first suitable inspector bound to sandbox index
// Returns the final inspector index
unsigned int ChangeInspectorDrawFunction(
	EditorState* editor_state, 
	unsigned int inspector_index, 
	InspectorFunctions functions,
	void* data, 
	size_t data_size,
	unsigned int sandbox_index = -1
)
{
	EDITOR_STATE(editor_state);

	if (sandbox_index != -1) {
		inspector_index = GetMatchingIndexFromRobin(editor_state, sandbox_index);
	}
	else if (inspector_index == -1) {
		inspector_index = GetMatchingIndexFromRobin(editor_state);
	}

	if (inspector_index != -1) {
		InspectorData* inspector_data = editor_state->inspector_manager.data.buffer + inspector_index;
		inspector_data->clean_function(editor_state, inspector_data->draw_data);

		if (inspector_data->data_size > 0) {
			editor_allocator->Deallocate(inspector_data->draw_data);
		}
		inspector_data->draw_data = function::AllocateMemory(editor_allocator, data, data_size);
		inspector_data->draw_function = functions.draw_function;
		inspector_data->clean_function = functions.clean_function;
		inspector_data->data_size = data_size;
	}

	return inspector_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

void* GetInspectorDrawFunctionData(EditorState* editor_state, unsigned int inspector_index) {
	return editor_state->inspector_manager.data[inspector_index].draw_data;
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToNothing(EditorState* editor_state, unsigned int inspector_index) {

	ChangeInspectorDrawFunction(editor_state, inspector_index, { InspectorDrawNothing, InspectorCleanNothing }, nullptr, 0);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToFolder(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';

	ChangeInspectorDrawFunction(
		editor_state, 
		inspector_index,
		{ InspectorDrawFolderInfo, InspectorCleanNothing }, 
		null_terminated_path.buffer,
		sizeof(wchar_t) * (path.size + 1)
	);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToMeshFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawMeshFileData data;
	data.mesh = nullptr;
	data.radius_delta = 0.0f;
	data.rotation_delta = { 0.0f, 0.0f };

	// Allocate the data and embedd the path in it
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

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorFunctions functions;
	Stream<wchar_t> extension = function::PathExtension(path);

	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';
	if (extension.size == 0 || !TryGetInspectorTableFunction(editor_state, functions, extension)) {
		functions.draw_function = InspectorDrawBlankFile;
		functions.clean_function = InspectorCleanNothing;
	}

	if (functions.draw_function != InspectorDrawMeshFile) {
		ChangeInspectorDrawFunction(editor_state, inspector_index, functions, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
	}
	else {
		ChangeInspectorToMeshFile(editor_state, path, inspector_index);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index, unsigned int inspector_index) {
	ECS_ASSERT(index < editor_state->project_modules->size);

	Stream<wchar_t> module_name = function::StringCopy(editor_state->EditorAllocator(), editor_state->project_modules->buffer[index].library_name);
	DrawModuleData draw_data;
	draw_data.linear_allocator = nullptr;
	draw_data.module_name = module_name;
	draw_data.window_index = 0;
	draw_data.settings_name = { nullptr, 0 };
	draw_data.header_states = nullptr;
	draw_data.settings_allocator = nullptr;
	draw_data.reflected_settings = { nullptr, 0 };

	if (inspector_index == -1) {
		inspector_index = GetMatchingIndexFromRobin(editor_state);
	}
	draw_data.inspector_index = inspector_index;

	ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawModule, InspectorDrawModuleClean },
		&draw_data,
		sizeof(draw_data)
	);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToGraphicsModule(EditorState* editor_state, unsigned int inspector_index) {
	ChangeInspectorToModule(editor_state, GRAPHICS_MODULE_INDEX);
	
	/*DrawModuleData draw_data;
	draw_data.linear_allocator = nullptr;
	draw_data.module_name = editor_state->project_modules->buffer[GRAPHICS_MODULE_INDEX].library_name;
	draw_data.window_index = 0;
	
	ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawModule, InspectorDrawModuleClean },
		&draw_data,
		sizeof(draw_data)
	);*/
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorTargetSandbox(EditorState* editor_state, unsigned int inspector_index)
{
	return editor_state->inspector_manager.data[inspector_index].target_sandbox;
}

// ----------------------------------------------------------------------------------------------------------------------------

void UpdateInspectorUIModuleSettings(EditorState* editor_state, unsigned int module_index)
{
	for (size_t index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].draw_function == InspectorDrawModule) {
			DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[index].draw_data;
			unsigned int current_module_index = GetModuleIndexFromName(editor_state, draw_data->module_name);
			if (current_module_index == module_index) {
				// Deallocate the current settings
				if (draw_data->settings_name.size > 0) {
					DeallocateInspectorSettingsHelper(editor_state, module_index, index);
					LoadInspectorSettingsHelper(editor_state, module_index, index);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void DestroyInspectorInstance(EditorState* editor_state, unsigned int inspector_index)
{
	CleanInspectorData(editor_state, inspector_index);
	// Deallocate the data if any is allocated
	if (editor_state->inspector_manager.data[inspector_index].data_size > 0) {
		editor_state->editor_allocator->Deallocate(editor_state->inspector_manager.data[inspector_index].draw_data);
		editor_state->inspector_manager.data[inspector_index].data_size = 0;
	}

	// If the inspector is not the last one and the size is greater than 1, swap it with the last one
	if (inspector_index != editor_state->inspector_manager.data.size - 1 && editor_state->inspector_manager.data.size > 0) {
		editor_state->inspector_manager.data.RemoveSwapBack(inspector_index);
		// Update the window name for the swapped inspector
		unsigned int swapped_inspector = editor_state->inspector_manager.data.size;
		editor_state->ui_system->ChangeWindowNameFromIndex(ToStream(INSPECTOR_WINDOW_NAME), swapped_inspector, inspector_index);
	}
	else if (editor_state->inspector_manager.data.size > 0) {
		editor_state->inspector_manager.data.size--;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorIndex(const char* window_name) {
	size_t name_size = strlen(window_name);
	size_t current_index = name_size - 1;

	unsigned int number = 0;
	while (function::IsNumberCharacter(window_name[current_index])) {
		number *= 10;
		number += window_name[current_index] - '0';
		current_index--;
	}

	return number;
}

// ----------------------------------------------------------------------------------------------------------------------------

void GetInspectorName(unsigned int inspector_index, CapacityStream<char>& inspector_name)
{
	inspector_name.AddStreamSafe(ToStream(INSPECTOR_WINDOW_NAME));
	function::ConvertIntToChars(inspector_name, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorManager(EditorState* editor_state)
{
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(editor_state->editor_allocator);
	editor_state->inspector_manager.data.Initialize(allocator, 1);
	editor_state->inspector_manager.round_robin_index.Initialize(allocator, 2);
	InitializeInspectorTable(editor_state);
}

// ----------------------------------------------------------------------------------------------------------------------------

void LockInspector(EditorState* editor_state, unsigned int inspector_index)
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;
	data->flags = function::SetFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void UnlockInspector(EditorState* editor_state, unsigned int inspector_index) 
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;
	data->flags = function::ClearFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorTable(EditorState* editor_state) {
	void* allocation = editor_state->editor_allocator->Allocate(InspectorTable::MemoryOf(POINTER_CAPACITY));
	editor_state->inspector_manager.function_table.InitializeFromBuffer(allocation, POINTER_CAPACITY);
	
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTexture, InspectorCleanNothing }, L".png");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTexture, InspectorCleanNothing }, L".jpg");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTexture, InspectorCleanNothing }, L".bmp");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTexture, InspectorCleanNothing }, L".tiff");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTexture, InspectorCleanNothing }, L".tga");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTexture, InspectorCleanNothing }, L".hdr");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTextFile, InspectorCleanNothing }, L".txt");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawTextFile, InspectorCleanNothing }, L".md");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".cpp");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".h");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".hpp");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawCTextFile, InspectorCleanNothing }, L".c");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawHlslTextFile, InspectorCleanNothing }, L".hlsl");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawHlslTextFile, InspectorCleanNothing }, L".hlsli");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawMeshFile, InspectorCleanMeshFile }, L".gltf");
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawMeshFile, InspectorCleanMeshFile }, L".glb");
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorInstance(EditorState* editor_state, unsigned int index)
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + index;
	data->draw_function = nullptr;
	data->clean_function = InspectorCleanNothing;
	data->data_size = 0;
	data->draw_data = nullptr;
	data->flags = 0;
	data->target_sandbox = 0;
	data->table = &editor_state->inspector_manager.function_table;
}

// ----------------------------------------------------------------------------------------------------------------------------

void FixInspectorSandboxReference(EditorState* editor_state, unsigned int old_sandbox_index, unsigned int new_sandbox_index) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].target_sandbox == old_sandbox_index) {
			editor_state->inspector_manager.data[index].target_sandbox = new_sandbox_index;
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void RegisterInspectorSandbox(EditorState* editor_state) {
	unsigned int sandbox_count = editor_state->sandboxes.size;
	if (sandbox_count > editor_state->inspector_manager.round_robin_index.size) {
		// An addition was done - just copy to a new buffer
		Stream<unsigned int> old_stream = editor_state->inspector_manager.round_robin_index;
		editor_state->inspector_manager.round_robin_index.Initialize(editor_state->editor_allocator, sandbox_count + 1);
		old_stream.CopyTo(editor_state->inspector_manager.round_robin_index.buffer);

		// Move the count, for actions independent of sandbox, positioned at old_stream.size to sandbox_count + 1
		editor_state->inspector_manager.round_robin_index[sandbox_count] = old_stream[old_stream.size];
		editor_state->editor_allocator->Deallocate(old_stream.buffer);
	}
	else if (sandbox_count < editor_state->inspector_manager.round_robin_index.size) {
		// A removal was done - allocate a new buffer and reroute inspectors on the 
		// sandboxes removed
		Stream<unsigned int> old_stream = editor_state->inspector_manager.round_robin_index;
		editor_state->inspector_manager.round_robin_index.Initialize(editor_state->editor_allocator, sandbox_count + 1);
		// The count for actions independent of sandbox must be moved separately
		memcpy(editor_state->inspector_manager.round_robin_index.buffer, old_stream.buffer, sizeof(unsigned int) * sandbox_count);

		editor_state->inspector_manager.round_robin_index[sandbox_count] = old_stream[old_stream.size];
		editor_state->editor_allocator->Deallocate(old_stream.buffer);

		for (unsigned int index = sandbox_count; index < old_stream.size; index++) {
			FixInspectorSandboxReference(editor_state, index, 0);
		}
		
		// Fix any invalid round robin index
		for (unsigned int index = 0; index < sandbox_count; index++) {
			unsigned int target_inspectors_for_module = 0;
			for (unsigned int subindex = 0; subindex < editor_state->inspector_manager.data.size; subindex++) {
				target_inspectors_for_module += editor_state->inspector_manager.data[subindex].target_sandbox == index;
			}

			editor_state->inspector_manager.round_robin_index[index] = editor_state->inspector_manager.round_robin_index[index] % target_inspectors_for_module;
		}
		editor_state->inspector_manager.round_robin_index[sandbox_count] = editor_state->inspector_manager.round_robin_index[sandbox_count] % sandbox_count;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

bool IsInspectorLocked(const EditorState* editor_state, unsigned int inspector_index)
{
	return function::HasFlag(editor_state->inspector_manager.data[inspector_index].flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------
