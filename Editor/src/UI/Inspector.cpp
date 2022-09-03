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
#include "Sandbox.h"

//using namespace ECSEngine;
//ECS_TOOLS;

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

#define GRAPHICS_MODULE_TEXTURE ECS_TOOLS_UI_TEXTURE_LIGHT_BULB

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
	return TryGetInspectorTableFunction(editor_state, function, _identifier);
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
		if (editor_state->inspector_manager.data[current_inspector].target_sandbox == target_sandbox && !IsInspectorLocked(editor_state, current_inspector)) {
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
	drawer->TextLabel(UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_LABEL_TRANSPARENT, config, ascii_path.buffer + (folder_name.buffer - path.buffer));
	drawer->NextRow();

	UIDrawerSentenceFitSpaceToken fit_space_token;
	fit_space_token.token = '\\';
	config.AddFlag(fit_space_token);
	drawer->Sentence(UI_CONFIG_SENTENCE_FIT_SPACE | UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN, config, ascii_path.buffer);
	drawer->NextRow();
	drawer->CrossLine();
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorShowButton(UIDrawer* drawer, Stream<wchar_t> path) {
	drawer->Button("Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorOpenAndShowButton(UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button("Open", { OpenFileWithDefaultApplicationStreamAction, &path, sizeof(path) });

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer->GetLabelScale("Show");
	transform.position = drawer->GetAlignedToRight(transform.scale.x);
	config.AddFlag(transform);
	drawer->Button(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFileTimesInternal(UIDrawer* drawer, const char* creation_time, const char* write_time, const char* access_time, bool success) {
	UIDrawConfig config;
	// Display file times
	if (success) {
		drawer->Text("Creation time: ");
		drawer->Text(creation_time);
		drawer->NextRow();

		drawer->Text("Access time: ");
		drawer->Text(access_time);
		drawer->NextRow();

		drawer->Text("Last write time: ");
		drawer->Text(write_time);
		drawer->NextRow();
	}
	// Else error message
	else {
		drawer->Text("Could not retrieve directory times.");
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
	drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Nothing is selected.");
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

	Stream<wchar_t> stream_path = path;
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	struct OpenFolderData {
		EditorState* state;
		const wchar_t* path;
	};

	auto OpenFileExplorerFolder = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		OpenFolderData* data = (OpenFolderData*)_data;
		Stream<wchar_t> path = data->path;
		ChangeFileExplorerDirectory(data->state, path);
	};

	UIDrawConfig config;

	config.flag_count = 0;
	OpenFolderData open_data;
	open_data.path = path;
	open_data.state = editor_state;
	drawer->Button("Open", { OpenFileExplorerFolder, &open_data, sizeof(open_data) });

	UIConfigAbsoluteTransform absolute_transform;
	absolute_transform.scale = drawer->GetLabelScale("Show");
	absolute_transform.position = drawer->GetAlignedToRight(absolute_transform.scale.x);
	config.AddFlag(absolute_transform);
	drawer->Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Show", { LaunchFileExplorerAction, (void*)path, 0 });
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
	Stream<wchar_t> stream_path = path;
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
	Stream<wchar_t> stream_path = path;
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
	Stream<wchar_t> stream_path = path;
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
		EditorSetConsoleWarn("Could not save the new settings file.");
	}
}

#pragma endregion

void InspectorDrawModuleClean(EditorState* editor_state, void* _data) {
	EDITOR_STATE(editor_state);
	DrawModuleData* data = (DrawModuleData*)_data;
	
	// If there is currently a present reflection, release it
	if (data->settings_name.size > 0) {
		unsigned int module_index = GetModuleIndexFromName(editor_state, data->module_name);
		if (module_index != -1) {
			DeallocateInspectorSettingsHelper(editor_state, module_index, data->inspector_index);
		}
	}

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

	bool is_graphics_module = IsGraphicsModule(editor_state, module_index);
	const wchar_t* display_icon = is_graphics_module ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;
	InspectorIcon(drawer, display_icon, drawer->color_theme.theme);
	
	ECS_STACK_CAPACITY_STREAM(char, ascii_name, 256);
	function::ConvertWideCharsToASCII(data->module_name, ascii_name);
	if (editor_state->project_modules->buffer[module_index].is_graphics_module) {
		ascii_name.AddStream(" (Graphics Type)");
	}
	drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, ascii_name);
	drawer->NextRow();

	// Get all the components and the shared components
	ECS_STACK_CAPACITY_STREAM(unsigned int, reflected_components, 32);
	ECS_STACK_CAPACITY_STREAM(unsigned int, reflected_shared_components, 32);

	GetModuleReflectionAllComponentIndices(editor_state, module_index, &reflected_components, &reflected_shared_components);
	if (reflected_components.size == 0) {
		drawer->Text("The module does not have any components.");
		drawer->NextRow();
	}
	else {
		// Draw the components alongside their byte size and their ID
		ECS_STACK_CAPACITY_STREAM(char, label_list_characters, 512);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, labels, reflected_components.size);

		Stream<char> evaluation_name = "ID";

		for (unsigned int index = 0; index < reflected_components.size; index++) {
			labels[index].buffer = label_list_characters.buffer + label_list_characters.size;

			const Reflection::ReflectionType* type = editor_state->ModuleReflectionManager()->GetType(reflected_components[index]);
			size_t byte_size = Reflection::GetReflectionTypeByteSize(type);

			// The component might be lacking an ID
			double id_evaluation = type->GetEvaluation(evaluation_name);
			if (id_evaluation == DBL_MAX) {
				labels[index].size = function::FormatString(labels[index].buffer, "{#} (ID is missing, {#} byte size)", type->name, byte_size);
			}
			else {
				labels[index].size = function::FormatString(labels[index].buffer, "{#} ({#} ID, {#} byte size)", type->name, (unsigned short)id_evaluation, byte_size);
			}
			label_list_characters.size += labels[index].size;
		}

		labels.size = reflected_components.size;
		drawer->LabelList("Components:", labels);
	}

	drawer->CrossLine();

	if (reflected_shared_components.size == 0) {
		drawer->Text("The module does not have any shared components.");
		drawer->NextRow();
	}
	else {
		// Draw the components alongside their byte size and their ID
		ECS_STACK_CAPACITY_STREAM(char, label_list_characters, 512);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, labels, reflected_shared_components.size);

		Stream<char> evaluation_name = "ID";

		for (unsigned int index = 0; index < reflected_shared_components.size; index++) {
			labels[index].buffer = label_list_characters.buffer + label_list_characters.size;

			const Reflection::ReflectionType* type = editor_state->ModuleReflectionManager()->GetType(reflected_shared_components[index]);
			size_t byte_size = Reflection::GetReflectionTypeByteSize(type);

			// The component might be lacking an ID
			double id_evaluation = type->GetEvaluation(evaluation_name);
			if (id_evaluation == DBL_MAX) {
				labels[index].size = function::FormatString(labels[index].buffer, "{#} (ID is missing, {#} byte size)", type->name, byte_size);
			}
			else {
				labels[index].size = function::FormatString(labels[index].buffer, "{#} ({#} ID, {#} byte size)", type->name, (unsigned short)id_evaluation, byte_size);
			}
			label_list_characters.size += labels[index].size;
		}

		labels.size = reflected_components.size;
		drawer->LabelList("Shared Components:", labels);
	}

	drawer->CrossLine();

	// Display the reflected types for this module
	ECS_STACK_CAPACITY_STREAM(unsigned int, reflected_type_indices, 32);
	GetModuleSettingsUITypesIndices(editor_state, module_index, reflected_type_indices);

	if (reflected_type_indices.size > 0) {
		// Coallesce the string
		ECS_STACK_CAPACITY_STREAM(Stream<char>, type_names, 32);
		for (size_t index = 0; index < reflected_type_indices.size; index++) {
			UIReflectionType type = editor_state->module_reflection->GetType(reflected_type_indices[index]);
			type_names[index] = type.name;
		}
		type_names.size = reflected_type_indices.size;
		drawer->LabelList(UI_CONFIG_LABEL_TRANSPARENT, config, "Module settings types", type_names);
		drawer->NextRow();
	}
	// No settings for this module
	else {
		drawer->Text("The module does not have settings types.");
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

	drawer->Text("Project Settings ");

	// Display the current setting name - if it has one
	if (data->settings_name.size > 0) {
		GetProjectConfigurationModuleFolder(editor_state, setting_absolute_path);
		setting_absolute_path.Add(ECS_OS_PATH_SEPARATOR);
		setting_absolute_path.AddStreamSafe(data->settings_name);

		// Display the configuration's name
		ECS_STACK_CAPACITY_STREAM(char, ascii_setting_name, 256);
		function::ConvertWideCharsToASCII(data->settings_name, ascii_setting_name);

		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, ascii_setting_name);
	}
	else {
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "No module settings path is selected.");
	}
	drawer->NextRow();

	drawer->CrossLine();

	CreateSettingsCallbackData create_data = { editor_state, module_index };
	drawer->Button(
		UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE,
		config, 
		"Create new setting",
		{ create_new_setting, &create_data, sizeof(create_data) }
	);
	drawer->NextRow();

	// Enumerate all the other possible settings
	ECS_STACK_CAPACITY_STREAM(wchar_t, settings_folder, 512);
	GetModuleSettingsFolderPath(editor_state, module_index, settings_folder);

	Stream<wchar_t> settings_extensions[] = { MODULE_SETTINGS_EXTENSION };

	struct FunctorData {
		UIDrawer* drawer;
		EditorState* editor_state;
		LinearAllocator* temp_allocator;
		unsigned int module_index;
		unsigned int inspector_index;
		CapacityStream<wchar_t>* active_settings;
	};

	FunctorData functor_data = { drawer, editor_state, linear_allocator, module_index, data->inspector_index, &data->settings_name };

	if (create_settings_active_state.state) {
		drawer->Text("Available settings ");
		drawer->NextRow();

		ForEachFileInDirectoryWithExtension(settings_folder, { settings_extensions, std::size(settings_extensions) }, &functor_data,
			[](Stream<wchar_t> path, void* _data) {
				FunctorData* data = (FunctorData*)_data;

				Stream<wchar_t> stem = function::PathStem(path);

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

				SetNewSettingData callback_data = {
					data->editor_state,
					{name_allocation, stem.size},
					data->active_settings,
					data->module_index,
					data->inspector_index
				};

				float2 square_scale = data->drawer->GetSquareScale();

				UIConfigWindowDependentSize dependent_size;
				float total_scale = data->drawer->GetWindowScaleUntilBorder();
				float label_scale = total_scale - square_scale.x - data->drawer->layout.element_indentation;
				size_t label_configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_TRANSPARENT;
				if (is_active) {
					label_configuration = function::ClearFlag(label_configuration, UI_CONFIG_LABEL_TRANSPARENT);
				}
				dependent_size.scale_factor.x = data->drawer->GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, { label_scale, 0.0f }).x;
				config.AddFlag(dependent_size);

				data->drawer->ButtonWide(label_configuration, config, stem, { set_new_setting, &callback_data, sizeof(callback_data) });

				struct DeleteActionData {
					const wchar_t* filename;
					EditorState* editor_state;
					unsigned int module_index;
					unsigned int inspector_index;
					CapacityStream<wchar_t>* active_settings;
				};
				DeleteActionData action_data = { (wchar_t*)name_allocation, data->editor_state, data->module_index, data->inspector_index, data->active_settings };

				auto delete_action = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					DeleteActionData* data = (DeleteActionData*)_data;

					ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
					GetModuleSettingsFilePath(data->editor_state, data->module_index, data->filename, absolute_path);
					bool success = RemoveFile(absolute_path);
					if (!success) {
						ECS_FORMAT_TEMP_STRING(error_message, "Could not delete module settings file {#}.", absolute_path);
						EditorSetConsoleError(error_message);
					}
					else {
						// Check to see if that is the current setting
						if (data->active_settings->size > 0 && function::CompareStrings(data->filename, *data->active_settings)) {
							DeallocateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
							data->editor_state->editor_allocator->Deallocate(data->active_settings->buffer);
							*data->active_settings = { nullptr, 0, 0 };
						}
					}
				};

				data->drawer->SpriteButton(UI_CONFIG_LATE_DRAW | UI_CONFIG_MAKE_SQUARE, config, { delete_action, &action_data, sizeof(action_data) }, ECS_TOOLS_UI_TEXTURE_X, data->drawer->color_theme.text);
				data->drawer->NextRow();

				return true;
			});

		drawer->CrossLine();
	}

	// Present the data now - if a path is selected

	if (data->settings_name.size > 0) {
		drawer->Text("Settings Variables");
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

		auto array_add_element_change = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerArrayAddRemoveData* data = (UIDrawerArrayAddRemoveData*)_additional_data;
			// memset the elements to 0
			unsigned int new_elements = data->capacity_data->size - data->new_size;
			memset(function::OffsetPointer(data->capacity_data->buffer, data->element_byte_size * data->new_size), 0, data->element_byte_size * new_elements);

			SaveFileAfterChangeData* save_data = (SaveFileAfterChangeData*)_data;
			bool success = SaveInspectorSettingsHelper(save_data->editor_state, save_data->module_index, save_data->inspector_index);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(console_string, "An error occured when saving the module setting file for inspector {#} after value change. "
					"The modification has not been commited.", save_data->inspector_index);
				EditorSetConsoleError(console_string);
			}
		};

		ECS_STACK_CAPACITY_STREAM(unsigned int, instance_indices, 64);
		GetModuleSettingsUIInstancesIndices(editor_state, module_index, data->inspector_index, instance_indices);

		SaveFileAfterChangeData save_file_data = { editor_state, module_index, data->inspector_index };

		if (instance_indices.size > 0) {
			UIReflectionDrawConfig ui_reflection_configs[8];

			UIConfigTextInputCallback text_input_callback;
			text_input_callback.handler = { save_file_after_change, &save_file_data, sizeof(save_file_data) };

			UIConfigPathInputCallback path_input_callback;
			path_input_callback.callback = { save_file_after_change, &save_file_data, sizeof(save_file_data) };
	
			UIConfigArrayAddCallback array_add_callback;
			array_add_callback.handler = { array_add_element_change, &save_file_data, sizeof(save_file_data) };

			UIConfigArrayRemoveCallback array_remove_callback;
			array_remove_callback.handler = { save_file_after_change, &save_file_data, sizeof(save_file_data) };

			ui_reflection_configs[0].configurations = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			ui_reflection_configs[0].index[0] = UIReflectionIndex::Count;
			UIReflectionDrawConfigAddConfig(ui_reflection_configs, &array_add_callback);
			UIReflectionDrawConfigAddConfig(ui_reflection_configs, &array_remove_callback);

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
			dependent_size.scale_factor.x = 1.0f;
			config.AddFlag(dependent_size);
			for (size_t index = 0; index < instance_indices.size; index++) {
				// Display them as collapsing headers
				UIReflectionInstance* instance = editor_state->module_reflection->GetInstancePtr(instance_indices[index]);
				
				Stream<char> separator = function::FindFirstCharacter(instance->name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR);
				Stream<char> name = { instance->name.buffer, instance->name.size - separator.size };

				drawer->CollapsingHeader(name, collapsing_headers_state + index, [&]() {
					Stream<char> current_instance_name = instance->name;
					instance->name = name;
					editor_state->module_reflection->DrawInstance(
						instance,
						*drawer,
						config,
						0,
						{ ui_reflection_configs, std::size(ui_reflection_configs) }
					);
					// Restore the name
					instance->name = current_instance_name;
				});
			}
		}

		auto set_default_values = [](ActionData* action_data) {
			SaveFileAfterChangeData* data = (SaveFileAfterChangeData*)action_data->data;

			DrawModuleData* draw_data = (DrawModuleData*)data->editor_state->inspector_manager.data[data->inspector_index].draw_data;
			if (draw_data->reflected_settings.size > 0) {
				SetModuleDefaultSettings(data->editor_state, data->module_index, draw_data->reflected_settings);
				ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
				GetModuleSettingsFilePath(data->editor_state, data->module_index, draw_data->settings_name, absolute_path);

				bool success = SaveModuleSettings(data->editor_state, data->module_index, absolute_path, draw_data->reflected_settings);
				if (!success) {
					EditorSetConsoleError("An error has occured when trying to save default values for module settings file. The modification has not been commited.");
				}
			}
		};
		
		drawer->Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Default Values", { set_default_values, &save_file_data, sizeof(save_file_data) });
		config.flag_count = 0;
		drawer->NextRow();

		drawer->CrossLine();
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

struct DrawSandboxSettingsData {
	EditorState* editor_state;
	Stream<char> ui_reflection_instance_name;
	WorldDescriptor ui_descriptor;
	LinearAllocator runtime_settings_allocator;
	size_t last_write_descriptor;

	bool collapsing_module_state;
	bool collapsing_runtime_state;
};

void InspectorDrawSandboxSettingsClean(EditorState* editor_state, void* _data) {
	DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
	// Destroy the UI reflection
	editor_state->ui_reflection->DestroyInstance(data->ui_reflection_instance_name.buffer);

	// Deallocate the name
	editor_state->editor_allocator->Deallocate(data->ui_reflection_instance_name.buffer);

	// Deallocate the linear allocator
	editor_state->editor_allocator->Deallocate(data->runtime_settings_allocator.m_buffer);
}

struct DrawSandboxSelectionWindowData {
	EditorState* editor_state;
	unsigned int selection;
	unsigned int sandbox_index;

	EDITOR_MODULE_CONFIGURATION configuration;
};

void DrawSandboxSelectionWindow(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	DrawSandboxSelectionWindowData* data = (DrawSandboxSelectionWindowData*)window_data;
	EditorState* editor_state = data->editor_state;

	// Display the combo box for configuration selection
	drawer.ComboBox("Module configuration", { MODULE_CONFIGURATIONS, EDITOR_MODULE_CONFIGURATION_COUNT }, EDITOR_MODULE_CONFIGURATION_COUNT, (unsigned char*)&data->configuration);
	drawer.NextRow();

	UIDrawConfig config;

	float2 square_scale = drawer.GetSquareScale();
	float scale_until_border = drawer.GetWindowScaleUntilBorder();

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = drawer.GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, { scale_until_border - square_scale.x - drawer.layout.element_indentation, 0.0f }).x;
	config.AddFlag(dependent_size);

	const EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
	ProjectModules* project_modules = editor_state->project_modules;
	// Display all the modules now
	for (unsigned int index = 0; index < project_modules->size; index++) {
		bool is_graphics = project_modules->buffer[index].is_graphics_module;

		const wchar_t* texture_to_display = is_graphics ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;
		drawer.SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, texture_to_display, drawer.color_theme.theme);

		size_t configuration = UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_ACTIVE_STATE | UI_CONFIG_WINDOW_DEPENDENT_SIZE;
		// Look to see if it already belongs to the sandbox
		unsigned int subindex = 0;

		config.flag_count = 1;

		UIConfigActiveState active_state;
		active_state.state = true;
		for (; subindex < sandbox->modules_in_use.size; subindex++) {
			if (sandbox->modules_in_use[subindex].module_index == index) {
				active_state.state = false;
				UIConfigColor background_color;
				background_color.color = ECS_COLOR_GREEN;
				config.AddFlag(background_color);

				configuration = function::ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
				configuration |= UI_CONFIG_COLOR;
				break;
			}
		}

		config.AddFlag(active_state);

		struct SelectData {
			DrawSandboxSelectionWindowData* data;
			unsigned int index;
		};

		auto select_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SelectData* data = (SelectData*)_data;
			data->data->selection = data->index;
		};

		if (index == data->selection) {
			configuration = function::ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
		}

		SelectData select_data = { data, index };
		drawer.ButtonWide(configuration, config, project_modules->buffer[index].library_name, { select_action, &select_data, sizeof(select_data) });
		drawer.NextRow();
	}

	auto confirm_data = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		DrawSandboxSelectionWindowData* data = (DrawSandboxSelectionWindowData*)_data;
		AddSandboxModule(data->editor_state, data->sandbox_index, data->selection, data->configuration);

		// Write the sandbox file
		SaveEditorSandboxFile(data->editor_state);
		CloseXBorderClickableAction(action_data);
	};

	UIConfigActiveState active_state;
	active_state.state = data->selection != -1;

	// Now add and cancel buttons
	config.flag_count = 0;
	config.AddFlag(active_state);

	drawer.Button(UI_CONFIG_ALIGN_ELEMENT_BOTTOM | UI_CONFIG_ACTIVE_STATE, config, "Confirm", { confirm_data, data, 0, ECS_UI_DRAW_SYSTEM });
	drawer.Button(UI_CONFIG_ALIGN_ELEMENT_BOTTOM | UI_CONFIG_ALIGN_ELEMENT_RIGHT, config, "Cancel", { CloseXBorderClickableAction, nullptr, 0, ECS_UI_DRAW_SYSTEM });
}

struct CreateAddSandboxWindowData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

void CreateAddSandboxWindow(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateAddSandboxWindowData* data = (CreateAddSandboxWindowData*)_data;
	UIWindowDescriptor descriptor;

	DrawSandboxSelectionWindowData window_data;
	window_data.editor_state = data->editor_state;
	window_data.selection = -1;
	window_data.sandbox_index = data->sandbox_index;
	window_data.configuration = EDITOR_MODULE_CONFIGURATION_DEBUG;

	descriptor.window_name = "Select a Module";

	descriptor.draw = DrawSandboxSelectionWindow;
	descriptor.window_data = &window_data;
	descriptor.window_data_size = sizeof(window_data);

	descriptor.destroy_action = ReleaseLockedWindow;

	descriptor.initial_size_x = 0.7f;
	descriptor.initial_size_y = 0.6f;

	system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL);
}

void InspectorDrawSandboxSettings(EditorState* editor_state, void* _data, UIDrawer* drawer) {
	EDITOR_STATE(editor_state);

	DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
	unsigned int inspector_index = GetInspectorIndex(drawer->system->GetWindowName(drawer->window_index));

	unsigned int sandbox_index = GetInspectorTargetSandbox(editor_state, inspector_index);

	// Initialize the ui_reflection_instance if we haven't done so
	if (data->ui_reflection_instance_name.size == 0) {
		ECS_STACK_CAPACITY_STREAM(char, ui_reflection_name, 128);
		UIReflectionDrawer* ui_drawer = editor_state->ui_reflection;

		unsigned int current_index = 0;
		GetSandboxUIWindowName(current_index, ui_reflection_name);
		// Try the UI reflection instance indices until we get a free slot
		UIReflectionInstance* instance = ui_drawer->GetInstancePtr(ui_reflection_name.buffer);
		while (instance != nullptr) {
			// Try again with the current_index
			ui_reflection_name.size = 0;
			GetSandboxUIWindowName(current_index, ui_reflection_name);

			instance = ui_drawer->GetInstancePtr(ui_reflection_name.buffer);
			current_index++;
		}

		// Increase the index by 1 such that we include the '\0'
		ui_reflection_name.size++;
		void* allocation = editor_state->editor_allocator->Allocate(sizeof(char) * ui_reflection_name.size);
		ui_reflection_name.CopyTo(allocation);

		data->ui_reflection_instance_name = { allocation, ui_reflection_name.size - 1 };
		instance = ui_drawer->CreateInstance(data->ui_reflection_instance_name.buffer, STRING(WorldDescriptor));

		WorldDescriptor* sandbox_descriptor = GetSandboxWorldDescriptor(editor_state, sandbox_index);
		memcpy(&data->ui_descriptor, sandbox_descriptor, sizeof(data->ui_descriptor));

		// Bind the pointers
		ui_drawer->BindInstancePtrs(instance, &data->ui_descriptor);

		const size_t ALLOCATOR_SIZE = ECS_KB;
		data->runtime_settings_allocator = LinearAllocator(editor_allocator->Allocate(ALLOCATOR_SIZE), ALLOCATOR_SIZE);

		data->last_write_descriptor = 0;
		data->collapsing_runtime_state = false;
		data->collapsing_module_state = false;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG, drawer->color_theme.text, drawer->color_theme.theme);

	UIDrawConfig config;

	ECS_STACK_CAPACITY_STREAM(char, sandbox_window_name, 128);
	GetSandboxUIWindowName(sandbox_index, sandbox_window_name);
	sandbox_window_name.AddStream(" Settings");
	sandbox_window_name[sandbox_window_name.size] = '\0';
	drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, sandbox_window_name.buffer);
	drawer->NextRow();

	// Display a warning if there is no graphics module or there are multiple
	ECS_STACK_CAPACITY_STREAM(unsigned int, graphics_module_indices, 128);
	GetSandboxGraphicsModules(editor_state, sandbox_index, graphics_module_indices);
	if (graphics_module_indices.size == 0) {
		drawer->SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Warning: There is no graphics module assigned.");
		drawer->NextRow();
	}
	else if (graphics_module_indices.size > 1) {
		drawer->SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Warning: There are multiple graphics modules assigned.");
		drawer->NextRow();
	}

	drawer->CollapsingHeader("Modules", &data->collapsing_module_state, [&]() {
		// Display the count of modules in use
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

		ECS_STACK_CAPACITY_STREAM(char, in_use_stream, 256);
		in_use_stream.Copy("In use: ");
		function::ConvertIntToChars(in_use_stream, sandbox->modules_in_use.size);
		drawer->Text(in_use_stream);
		drawer->NextRow();

		UIDrawConfig module_config;
		UIConfigComboBoxPrefix combo_prefix;
		combo_prefix.prefix = "Configuration";
		module_config.AddFlag(combo_prefix);

		struct ComboBoxCallbackData {
			EditorState* editor_state;
		};

		auto combo_callback_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ComboBoxCallbackData* data = (ComboBoxCallbackData*)_data;
			SaveEditorSandboxFile(data->editor_state);
		};

		ComboBoxCallbackData callback_data = { editor_state };
		UIConfigComboBoxCallback combo_callback;
		combo_callback.handler = { combo_callback_action, &callback_data, sizeof(callback_data) };
		module_config.AddFlag(combo_callback);

		// Push the pattern for unique combo names
		drawer->PushIdentifierStackStringPattern();

		struct RemoveModuleActionData {
			EditorState* editor_state;
			unsigned int sandbox_index;
			unsigned int in_module_index;
		};

		auto remove_module_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RemoveModuleActionData* data = (RemoveModuleActionData*)_data;
			RemoveSandboxModuleInStream(data->editor_state, data->sandbox_index, data->in_module_index);

			// Save the sandbox file as well
			SaveEditorSandboxFile(data->editor_state);
		};

		struct ActivateDeactivateData {
			EditorState* editor_state;
			unsigned int sandbox_index;
			unsigned int in_stream_index;
		};

		auto activate_deactivate_button = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ActivateDeactivateData* data = (ActivateDeactivateData*)_data;
			if (IsSandboxModuleDeactivatedInStream(data->editor_state, data->sandbox_index, data->in_stream_index)) {
				ActivateSandboxModuleInStream(data->editor_state, data->sandbox_index, data->in_stream_index);
			}
			else {
				DeactivateSandboxModuleInStream(data->editor_state, data->sandbox_index, data->in_stream_index);
			}

			SaveEditorSandboxFile(data->editor_state);
		};

		ECS_STACK_CAPACITY_STREAM(unsigned int, module_display_order, 64);
		module_display_order.size = sandbox->modules_in_use.size;
		function::MakeSequence(module_display_order);

		for (unsigned int index = 0; index < graphics_module_indices.size; index++) {
			for (unsigned int subindex = 0; subindex < module_display_order.size; subindex++) {
				if (module_display_order[subindex] == graphics_module_indices[index]) {
					module_display_order.Swap(index, subindex);
				}
			}
		}

		// Draw the modules in use, first the graphics modules	
		for (unsigned int index = 0; index < module_display_order.size; index++) {
			unsigned int in_stream_index = module_display_order[index];

			// Draw the remove X button
			RemoveModuleActionData remove_data;
			remove_data.editor_state = editor_state;
			remove_data.in_module_index = index;
			remove_data.sandbox_index = sandbox_index;
			drawer->SpriteButton(UI_CONFIG_MAKE_SQUARE, module_config, { remove_module_action, &remove_data, sizeof(remove_data) }, ECS_TOOLS_UI_TEXTURE_X);

			bool is_graphics_module = IsGraphicsModule(editor_state, sandbox->modules_in_use[in_stream_index].module_index);
			const wchar_t* module_texture = is_graphics_module ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;

			const size_t CONFIGURATION = UI_CONFIG_MAKE_SQUARE;
			const wchar_t* status_texture = nullptr;
			Color status_color;

			const EditorModuleInfo* info = GetModuleInfo(editor_state, sandbox->modules_in_use[in_stream_index].module_index, sandbox->modules_in_use[in_stream_index].module_configuration);
			switch (info->load_status) {
			case EDITOR_MODULE_LOAD_GOOD:
				status_texture = ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK;
				status_color = ECS_COLOR_GREEN;
				break;
			case EDITOR_MODULE_LOAD_OUT_OF_DATE:
				status_texture = ECS_TOOLS_UI_TEXTURE_CLOCK;
				status_color = ECS_COLOR_YELLOW;
				break;
			case EDITOR_MODULE_LOAD_FAILED:
				status_texture = ECS_TOOLS_UI_TEXTURE_MINUS;
				status_color = ECS_COLOR_RED;
				break;
			default:
				ECS_ASSERT(false, "Invalid load status for module");
			}

			size_t label_configuration = UI_CONFIG_LABEL_TRANSPARENT;

			if (IsSandboxModuleDeactivatedInStream(editor_state, sandbox_index, index)) {
				status_color.alpha = 100;
				label_configuration |= UI_CONFIG_UNAVAILABLE_TEXT;
			}

			ActivateDeactivateData activate_deactivate_data = { editor_state, sandbox_index, index };
			drawer->SpriteButton(CONFIGURATION, module_config, { activate_deactivate_button, &activate_deactivate_data, sizeof(activate_deactivate_data) }, module_texture, status_color);
			drawer->SpriteRectangle(CONFIGURATION, module_config, status_texture, status_color);

			Stream<wchar_t> module_library = editor_state->project_modules->buffer[sandbox->modules_in_use[in_stream_index].module_index].library_name;
			drawer->TextLabelWide(label_configuration, module_config, module_library);

			float x_bound = drawer->GetCurrentPositionNonOffset().x;

			drawer->PushIdentifierStackRandom(index);

			drawer->ComboBox(
				UI_CONFIG_ALIGN_ELEMENT_RIGHT | UI_CONFIG_COMBO_BOX_NO_NAME,
				module_config,
				"configuration", 
				{ MODULE_CONFIGURATIONS, EDITOR_MODULE_CONFIGURATION_COUNT }, 
				EDITOR_MODULE_CONFIGURATION_COUNT,
				(unsigned char*)&sandbox->modules_in_use[in_stream_index].module_configuration
			);

			drawer->PopIdentifierStack();

			drawer->NextRow();
		}

		drawer->PopIdentifierStack();

		// The add button
		CreateAddSandboxWindowData create_data;
		create_data.editor_state = editor_state;
		create_data.sandbox_index = sandbox_index;
		drawer->Button("Add Module", { CreateAddSandboxWindow, &create_data, sizeof(create_data), ECS_UI_DRAW_SYSTEM });
	});

	drawer->CollapsingHeader("Runtime Settings", &data->collapsing_runtime_state, [&]() {
		// Display all the available settings
		ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, available_settings, 128);
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

		// Reload the values if the lazy evaluation has finished
		if (data->last_write_descriptor != sandbox->runtime_settings_last_write) {
			memcpy(&data->ui_descriptor, GetSandboxWorldDescriptor(editor_state, sandbox_index), sizeof(data->ui_descriptor));
			data->last_write_descriptor = sandbox->runtime_settings_last_write;
		}

		// Reset the linear allocator
		data->runtime_settings_allocator.Clear();
		GetSandboxAvailableRuntimeSettings(editor_state, available_settings, GetAllocatorPolymorphic(&data->runtime_settings_allocator));
		if (available_settings.size == 0) {
			drawer->Text("There are no available settings.");
			drawer->NextRow();
		}
		else {
			drawer->Text("Available runtime settings:");
			drawer->NextRow();

			struct SelectData {
				DrawSandboxSettingsData* draw_data;
				unsigned int sandbox_index;
				Stream<wchar_t> path;
			};

			auto select_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SelectData* data = (SelectData*)_data;
				if (ChangeSandboxRuntimeSettings(data->draw_data->editor_state, data->sandbox_index, data->path)) {
					const WorldDescriptor* sandbox_descriptor = GetSandboxWorldDescriptor(data->draw_data->editor_state, data->sandbox_index);
					memcpy(&data->draw_data->ui_descriptor, sandbox_descriptor, sizeof(*sandbox_descriptor));

					// Save the sandbox file as well
					SaveEditorSandboxFile(data->draw_data->editor_state);
				}
			};

			UIDrawConfig config;
			UIConfigWindowDependentSize dependent_size;
			config.AddFlag(dependent_size);

			const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			for (unsigned int index = 0; index < available_settings.size; index++) {
				SelectData select_data = { data, sandbox_index, available_settings[index] };

				size_t configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_TRANSPARENT;
				if (function::CompareStrings(available_settings[index], sandbox->runtime_settings)) {
					configuration = function::ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
				}

				drawer->ButtonWide(configuration, config, available_settings[index], { select_action, &select_data, sizeof(select_data) });
				drawer->NextRow();
			}
		}

		drawer->CrossLine();

		drawer->Text("Current Settings: ");
		if (sandbox->runtime_settings.size > 0) {
			drawer->TextWide(sandbox->runtime_settings);
		}
		else {
			drawer->Text("No settings selected (default values)");
		}

		drawer->NextRow();

		UIConfigWindowDependentSize dependent_size;
		config.AddFlag(dependent_size);

		editor_state->ui_reflection->DrawInstance(data->ui_reflection_instance_name.buffer, *drawer, config, UI_CONFIG_WINDOW_DEPENDENT_SIZE);

		struct SaveCallbackData {
			DrawSandboxSettingsData* data;
			unsigned int sandbox_index;
		};

		auto save_callback = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SaveCallbackData* data = (SaveCallbackData*)_data;
			if (ValidateWorldDescriptor(&data->data->ui_descriptor)) {
				SetSandboxRuntimeSettings(data->data->editor_state, data->sandbox_index, data->data->ui_descriptor);
				// If it errors it will report to the user.
				SaveSandboxRuntimeSettings(data->data->editor_state, data->sandbox_index);
			}
			else {
				EditorSetConsoleError("The current runtime setting values are not valid. Increase the global allocator size.");
				// Copy the old values
				memcpy(&data->data->ui_descriptor, GetSandboxWorldDescriptor(data->data->editor_state, data->sandbox_index), sizeof(data->data->ui_descriptor));
			}
		};

		SaveCallbackData save_data;
		save_data.data = data;
		save_data.sandbox_index = sandbox_index;
		drawer->Button("Save", { save_callback, &save_data, sizeof(save_data) });

		UIConfigAbsoluteTransform default_transform;
		default_transform.scale = drawer->GetLabelScale("Default");
		default_transform.position = drawer->GetAlignedToRight(default_transform.scale.x);

		auto default_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
			data->ui_descriptor = GetDefaultWorldDescriptor();
		};

		config.flag_count = 0;
		config.AddFlag(default_transform);
		drawer->Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Default", { default_action, data, 0 });

		drawer->NextRow();
		drawer->CrossLine();
	});
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
	Stream<char> window_name = drawer.system->GetWindowName(drawer.window_index);
	unsigned int inspector_index = GetInspectorIndex(window_name);

	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;

	if (initialize) {
		drawer.font.size *= REDUCE_FONT_SIZE;
		drawer.font.character_spacing *= REDUCE_FONT_SIZE;

		//InitializeInspectorInstance(editor_state, inspector_index);
		//ChangeInspectorToNothing(editor_state, inspector_index);
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

		const char* COMBO_PREFIX = "Sandbox: ";

		unsigned int sandbox_count = editor_state->sandboxes.size;

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, combo_labels, sandbox_count);
		// We need only to convert the sandbox indices. We will use a prefix to indicate that it is the sandbox index
		ECS_STACK_CAPACITY_STREAM(char, converted_labels, 128);
		for (unsigned int index = 0; index < sandbox_count; index++) {
			combo_labels[index].buffer = converted_labels.buffer + converted_labels.size;
			combo_labels[index].size = function::ConvertIntToChars(converted_labels, index);
			// The last character was a '\0' but it is not included in the size
			converted_labels.size++;
		}
		combo_labels.size = sandbox_count;

		// The final positions will be set inside the if
		UIConfigAbsoluteTransform combo_transform;
		combo_transform.position.y = drawer.GetCurrentPositionNonOffset().y;

		float square_scale = drawer.GetSquareScale(drawer.layout.default_element_y).x;
		UIConfigAbsoluteTransform cog_transform;

		if (sandbox_count > 0) {
			float combo_size = drawer.ComboBoxDefaultSize(combo_labels, { nullptr, 0 }, COMBO_PREFIX);

			combo_transform.position.x = drawer.GetAlignedToRight(combo_size).x;
			combo_transform.scale = { combo_size, drawer.GetElementDefaultScale().y };

			cog_transform.position.x = combo_transform.position.x - square_scale - drawer.layout.element_indentation;
			cog_transform.position.y = combo_transform.position.y;
			cog_transform.scale = { square_scale, drawer.layout.default_element_y };
		}

		if (data->draw_function == nullptr) {
			ChangeInspectorToNothing(editor_state, inspector_index);
		}

		data->draw_function(editor_state, data->draw_data, &drawer);

		// Draw now the combo for the sandbox
		// We draw later on rather then at that moment because it will interfere with what's on the first row
		if (sandbox_count > 0) {
			drawer.current_row_y_scale = zoomed_icon_size;

			// In order to detect the offset for the combo draw, look into the text stream and see when the text span on the first row
			UISpriteVertex* text_sprites = drawer.HandleTextSpriteBuffer(0);
			size_t text_sprite_count = *drawer.HandleTextSpriteCount(0);
			size_t last_index = 0;
			float row_y = text_sprites[0].position.y;
			float initial_text_position = text_sprites[0].position.x;

			while (last_index < text_sprite_count && text_sprites[last_index].position.y == row_y) {
				last_index += 6;
			}
			last_index -= 6;
			float2 span = GetTextSpan(Stream<UISpriteVertex>(text_sprites, last_index));

			float cog_limit = initial_text_position + span.x + drawer.layout.element_indentation;
			float offset = cog_limit - cog_transform.position.x;
			if (offset > 0.0f) {
				offset += drawer.region_render_offset.x;
				cog_transform.position.x += offset;
				combo_transform.position.x += offset;
			}

			// To the rightmost border draw the bound sandbox index
			config.flag_count = 0;
			config.AddFlag(combo_transform);

			UIConfigComboBoxPrefix combo_prefix;
			combo_prefix.prefix = COMBO_PREFIX;
			config.AddFlag(combo_prefix);

			// We need to notify with a callback that the target sandbox has changed such that the inspector is reset
			struct ComboCallbackData {
				EditorState* editor_state;
				unsigned int inspector_index;
			};

			auto combo_callback_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				ComboCallbackData* data = (ComboCallbackData*)_data;
				unsigned char new_sandbox = GetInspectorTargetSandbox(data->editor_state, data->inspector_index);
				SetInspectorTargetSandbox(data->editor_state, data->inspector_index, new_sandbox);
			};

			ComboCallbackData combo_callback_data = { editor_state, inspector_index };
			UIConfigComboBoxCallback combo_callback;
			combo_callback.handler = { combo_callback_action, &combo_callback_data, sizeof(combo_callback_data) };

			config.AddFlag(combo_callback);

			// Draw the checkbox
			// For the moment, cast the pointer to unsigned char even tho it is an unsigned int
			// It behaves correctly since if the value stays lower than UCHAR_MAX
			drawer.ComboBox(
				UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_COMBO_BOX_CALLBACK,
				config,
				"Sandbox combo",
				combo_labels,
				sandbox_count,
				(unsigned char*)&editor_state->inspector_manager.data[inspector_index].target_sandbox
			);

			config.flag_count = 0;
			config.AddFlag(cog_transform);

			struct OpenSandboxSettings {
				EditorState* editor_state;
				unsigned int inspector_index;
			};

			auto open_sandbox_settings = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;
				
				OpenSandboxSettings* data = (OpenSandboxSettings*)_data;
				ChangeInspectorToSandboxSettings(data->editor_state, data->inspector_index);
			};

			OpenSandboxSettings open_data;
			open_data = { editor_state, inspector_index };
			drawer.SpriteButton(
				UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y,
				config, 
				{ open_sandbox_settings, &open_data, sizeof(open_data) },
				ECS_TOOLS_UI_TEXTURE_COG
			);
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
	ECS_ASSERT(inspector_index < MAX_INSPECTOR_WINDOWS);

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
	ChangeInspectorToNothing(editor_state, inspector_index);
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
		inspector_data->draw_data = function::CopyNonZero(editor_allocator, data, data_size);
		inspector_data->draw_function = functions.draw_function;
		inspector_data->clean_function = functions.clean_function;
		inspector_data->data_size = data_size;

		// Also make this window the focused one
		ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
		GetInspectorName(inspector_index, window_name);
		editor_state->ui_system->SetActiveWindow(window_name);
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

void ChangeInspectorToSandboxSettings(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	DrawSandboxSettingsData data;
	data.editor_state = editor_state;
	data.ui_reflection_instance_name = { nullptr, 0 };
	unsigned int matched_inspector_index = ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawSandboxSettings, InspectorDrawSandboxSettingsClean },
		&data,
		sizeof(data),
		sandbox_index
	);

	if (matched_inspector_index == -1 && sandbox_index != -1) {
		// Create a new inspector instance and set it to the sandbox with the draw on the settings
		matched_inspector_index = CreateInspectorInstance(editor_state);
		CreateInspectorDockspace(editor_state, matched_inspector_index);
		SetInspectorTargetSandbox(editor_state, matched_inspector_index, sandbox_index);
		ChangeInspectorToSandboxSettings(editor_state, matched_inspector_index);
	}

	// Set the new active window to be this inspector
	ECS_STACK_CAPACITY_STREAM(char, inspector_window_name, 128);
	GetInspectorName(matched_inspector_index, inspector_window_name);
	editor_state->ui_system->SetActiveWindow(inspector_window_name);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int inspector_index)
{
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorTargetSandbox(const EditorState* editor_state, unsigned int inspector_index)
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

void SetInspectorTargetSandbox(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	editor_state->inspector_manager.data[inspector_index].target_sandbox = sandbox_index;
	// Reset the draw to nothing - some windows are dependent on the sandbox and changing it without anouncing
	// them can result in inconsistent results

	ChangeInspectorToNothing(editor_state, inspector_index);
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
		editor_state->ui_system->ChangeWindowNameFromIndex(INSPECTOR_WINDOW_NAME, swapped_inspector, inspector_index);
	}
	else if (editor_state->inspector_manager.data.size > 0) {
		editor_state->inspector_manager.data.size--;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorIndex(Stream<char> window_name) {
	size_t current_index = window_name.size - 1;

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
	inspector_name.AddStreamSafe(INSPECTOR_WINDOW_NAME);
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
		// A removal was done - allocate a new buffer and reroute inspectors on the sandboxes removed
		Stream<unsigned int> old_stream = editor_state->inspector_manager.round_robin_index;
		editor_state->inspector_manager.round_robin_index.Initialize(editor_state->editor_allocator, sandbox_count + 1);
		// The count for actions independent of sandbox must be moved separately
		memcpy(editor_state->inspector_manager.round_robin_index.buffer, old_stream.buffer, sizeof(unsigned int) * sandbox_count);

		editor_state->inspector_manager.round_robin_index[sandbox_count] = old_stream[old_stream.size];
		editor_state->editor_allocator->Deallocate(old_stream.buffer);
		
		// Fix any invalid round robin index
		for (unsigned int index = 0; index < sandbox_count; index++) {
			unsigned int target_inspectors_for_module = 0;
			for (unsigned int subindex = 0; subindex < editor_state->inspector_manager.data.size; subindex++) {
				target_inspectors_for_module += editor_state->inspector_manager.data[subindex].target_sandbox == index;
			}

			if (target_inspectors_for_module > 0) {
				editor_state->inspector_manager.round_robin_index[index] = editor_state->inspector_manager.round_robin_index[index] % target_inspectors_for_module;
			}
			else {
				editor_state->inspector_manager.round_robin_index[index] = 0;
			}
		}

		if (sandbox_count > 0) {
			editor_state->inspector_manager.round_robin_index[sandbox_count] = editor_state->inspector_manager.round_robin_index[sandbox_count] % sandbox_count;
		}
		else {
			editor_state->inspector_manager.round_robin_index[sandbox_count] = 0;
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

bool IsInspectorLocked(const EditorState* editor_state, unsigned int inspector_index)
{
	return function::HasFlag(editor_state->inspector_manager.data[inspector_index].flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------
