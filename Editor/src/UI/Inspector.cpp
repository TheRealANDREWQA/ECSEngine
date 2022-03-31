#include "editorpch.h"
#include "Inspector.h"
#include "InspectorData.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorPalette.h"
#include "../HelperWindows.h"
#include "../OSFunctions.h"
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

using HashFunction = HashFunctionMultiplyString;

void InitializeInspector(EditorState* editor_state);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = InspectorWindowDraw;

	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
	descriptor.window_name = INSPECTOR_WINDOW_NAME;
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

	InspectorData* data = editor_state->inspector_data;
	return data->table.TryGetValue(identifier, function);
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, const wchar_t* _identifier) {
	return TryGetInspectorTableFunction(editor_state, function, ToStream(_identifier));
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconTexture(UIDrawer* drawer, ResourceView texture, Color color = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = icon_size / drawer->GetStaticElementDefaultScale();
	config.AddFlag(transform);

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM, config, texture, color);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIcon(UIDrawer* drawer, const wchar_t* path, Color color = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = icon_size / drawer->GetStaticElementDefaultScale();
	config.AddFlag(transform);

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM, config, path, color);
}

// ----------------------------------------------------------------------------------------------------------------------------

// A double sprite icon
void InspectorIconDouble(UIDrawer* drawer, const wchar_t* icon0, const wchar_t* icon1, Color icon_color0 = ECS_COLOR_WHITE, Color icon_color1 = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = icon_size / drawer->GetStaticElementDefaultScale();
	config.AddFlag(transform);

	drawer->SpriteRectangleDouble(UI_CONFIG_RELATIVE_TRANSFORM, config, icon0, icon1, icon_color0, icon_color1);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconNameAndPath(UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	Stream<wchar_t> folder_name = function::PathStem(path);
	ECS_TEMP_ASCII_STRING(ascii_path, 256);
	function::ConvertWideCharsToASCII(path.buffer, ascii_path.buffer, path.size + 1, 256);
	drawer->TextLabel(UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT, config, ascii_path.buffer + (folder_name.buffer - path.buffer));
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
	drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y, config, "Nothing is selected.");
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

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, drawer->color_theme.default_text);
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

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, icon_texture, drawer->color_theme.default_text, drawer->color_theme.theme);
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
			Graphics* graphics = editor_state->Graphics();
			Texture2D texture = GetResource(data->thumbnail.texture);
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
		InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_MESH, drawer->color_theme.default_text, drawer->color_theme.theme);
	}

	InspectorIconNameAndPath(drawer, data->path);

	InspectorDrawFileTimes(drawer, data->path);
	InspectorOpenAndShowButton(drawer, data->path);
	drawer->NextRow();

	auto calculate_texture_size_from_region = [](uint2 window_size, float2 region_scale) {
		return uint2(
			(unsigned int)(window_size.x * region_scale.x * 0.5f),
			(unsigned int)(window_size.y * MESH_PREVIEW_HEIGHT)
		);
	};

	// Only draw the preview if the mesh could actually be loaded
	if (mesh_thumbnail.could_be_read) {
		Graphics* graphics = editor_state->Graphics();
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
			uint2 texture_size = GetTextureDimensions(GetResource(data->thumbnail.texture));
			uint2 texture_size_from_region = calculate_texture_size_from_region(window_size, drawer->GetRegionScale());
			dimensions_changed = texture_size != texture_size_from_region;

			if (dimensions_changed) {
				// Release the texture and the view
				Texture2D texture = GetResource(data->thumbnail.texture);
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

struct CreateSettingsCallbackData {
	EditorState* editor_state;
	unsigned int module_index;
};

void CreateModuleSettingsCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateSettingsCallbackData* data = (CreateSettingsCallbackData*)_data;
	CapacityStream<char>* characters = (CapacityStream<char>*)_additional_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 512);
	function::ConvertASCIIToWide(wide_name, *characters);
	wide_name.AddStreamSafe(ToStream(L".config"));
	wide_name[wide_name.size] = L'\0';

	//// Save the current file and destroy the module settings
	//if (data->editor_state->project_modules->buffer[data->module_index].current_settings_path.buffer != nullptr) {
	//	bool save_success = SaveModuleSettings(data->editor_state, data->module_index);
	//	if (!save_success) {
	//		EditorSetConsoleWarn(ToStream("An error has occured when trying to save the module settings file before creating a new setting."));
	//	}
	//}

	DestroyModuleSettings(data->editor_state, data->module_index);
	// Assign the new path to the module
	ChangeModuleSettings(data->editor_state, wide_name, data->module_index);
	CreateModuleSettings(data->editor_state, data->module_index);

	// Write to a file the current values
	bool success = SaveModuleSettings(data->editor_state, data->module_index);
	if (!success) {
		EditorSetConsoleWarn(ToStream("Could not save the new settings file."));
	}
}

#define DRAW_MODULE_LINEAR_ALLOCATOR_CAPACITY ECS_KB

struct DrawModuleData {
	Stream<wchar_t> module_name;
	LinearAllocator* linear_allocator;
	bool* header_states;
	unsigned int window_index;
};

void InspectorDrawModuleClean(EditorState* editor_state, void* _data) {
	EDITOR_STATE(editor_state);
	DrawModuleData* data = (DrawModuleData*)_data;
	
	ui_system->RemoveWindowMemoryResource(data->window_index, data->linear_allocator->m_buffer);
	ui_system->RemoveWindowMemoryResource(data->window_index, data->linear_allocator);
	ui_system->RemoveWindowMemoryResource(data->window_index, data->header_states);

	ui_system->m_memory->Deallocate(data->linear_allocator->m_buffer);
	ui_system->m_memory->Deallocate(data->linear_allocator);
	ui_system->m_memory->Deallocate(data->header_states);

	editor_state->editor_allocator->Deallocate(data->module_name.buffer);
}

void InspectorDrawModule(EditorState* editor_state, void* _data, UIDrawer* drawer) {
	EDITOR_STATE(editor_state);

	DrawModuleData* data = (DrawModuleData*)_data;
	unsigned int module_index = ProjectModuleIndexFromName(editor_state, data->module_name);
	if (module_index == -1) {
		ChangeInspectorToNothing(editor_state);
		return;
	}
	
	// Display the current setting name
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
	}
	else {
		linear_allocator = data->linear_allocator;
		linear_allocator->Clear();

		collapsing_headers_state = data->header_states;
	}

	UIDrawConfig config;

	//ECS_STACK_CAPACITY_STREAM(wchar_t, setting_absolute_path, 512);
	//GetProjectConfigurationModuleFolder(editor_state, setting_absolute_path);
	//setting_absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	//setting_absolute_path.AddStreamSafe(editor_state->project_modules->buffer[module_index].current_settings_path);
	//setting_absolute_path[setting_absolute_path.size] = L'\0';

	//InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG, drawer->color_theme.default_text, drawer->color_theme.theme);
	//
	//if (editor_state->project_modules->buffer[module_index].current_settings_path.buffer != nullptr) {
	//	// Display the configuration's name
	//	ECS_STACK_CAPACITY_STREAM(char, ascii_setting_name, 256);
	//	function::ConvertWideCharsToASCII(editor_state->project_modules->buffer[module_index].current_settings_path, ascii_setting_name);

	//	drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y, config, "Current setting:");

	//	// Null terminate the string
	//	ascii_setting_name.AddSafe('\0');
	//	drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y, config, ascii_setting_name.buffer);
	//}
	//else {
	//	drawer->Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y, config, "No module settings path is selected.");
	//}
	drawer->NextRow();

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

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = 1.0f;
	config.AddFlag(dependent_size);
	CreateSettingsCallbackData create_data = { editor_state, module_index };
	drawer->Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Create new setting", { create_new_setting, &create_data, sizeof(create_data) });
	drawer->NextRow();
	drawer->CrossLine();

	// Enumerate all the other possible settings
	ECS_STACK_CAPACITY_STREAM(wchar_t, settings_folder, 512);
	GetProjectConfigurationModuleFolder(editor_state, settings_folder);

	const wchar_t* settings_extensions[] = { L".config" };

	struct FunctorData {
		UIDrawer* drawer;
		EditorState* editor_state;
		LinearAllocator* temp_allocator;
		unsigned int module_index;
	};

	FunctorData functor_data = { drawer, editor_state, linear_allocator, module_index };

	//ForEachFileInDirectoryWithExtension(settings_folder, { settings_extensions, std::size(settings_extensions) }, &functor_data,
	//	[](const wchar_t* path, void* _data) {
	//		FunctorData* data = (FunctorData*)_data;

	//		Stream<wchar_t> stream_path = ToStream(path);
	//		Stream<wchar_t> filename = function::PathFilename(stream_path);

	//		// Only display the settings for this module only - use the first part of the name to distinguish between modules
	//		const wchar_t* underscore = wcschr(filename.buffer, L'_');
	//		if (underscore == nullptr || !function::CompareStrings(data->editor_state->project_modules->buffer[data->module_index].library_name, Stream<wchar_t>(filename.buffer, underscore - filename.buffer))) {
	//			return true;
	//		}
	//		filename.buffer = (wchar_t*)underscore + 1;
	//		filename.size -= data->editor_state->project_modules->buffer[data->module_index].library_name.size + 1;

	//		bool is_active = function::CompareStrings(filename, data->editor_state->project_modules->buffer[data->module_index].current_settings_path);
	//		UIDrawConfig config;
	//		UIConfigCheckBoxCallback callback;

	//		struct SetNewSettingData {
	//			EditorState* editor_state;
	//			Stream<wchar_t> name;
	//			unsigned int module_index;
	//		};

	//		auto set_new_setting = [](ActionData* action_data) {
	//			UI_UNPACK_ACTION_DATA;

	//			SetNewSettingData* data = (SetNewSettingData*)_data;

	//			ECS_STACK_CAPACITY_STREAM(wchar_t, old_name, 512);
	//			old_name.Copy(data->editor_state->project_modules->buffer[data->module_index].current_settings_path);

	//			ChangeModuleSettings(data->editor_state, data->name, data->module_index);
	//			// Try to load the data
	//			bool success = LoadModuleSettings(data->editor_state, data->module_index);

	//			if (!success) {
	//				ChangeModuleSettings(data->editor_state, old_name, data->module_index);
	//				ECS_FORMAT_TEMP_STRING(error_message, "Could not switch module settings to {#} from {#}. The values could not be read from the file.", old_name, data->name);
	//				EditorSetConsoleError(error_message);
	//			}
	//		};

	//		void* name_allocation = data->temp_allocator->Allocate((filename.size + 1) * sizeof(wchar_t));
	//		memcpy(name_allocation, filename.buffer, sizeof(wchar_t) * (filename.size + 1));

	//		Stream<wchar_t> stem = function::PathStem(filename);
	//		ECS_STACK_CAPACITY_STREAM(char, ascii_name, 64);
	//		function::ConvertWideCharsToASCII(stem, ascii_name);

	//		SetNewSettingData callback_data = { data->editor_state, {name_allocation, filename.size}, data->module_index };
	//		callback.handler.action = set_new_setting;
	//		callback.handler.data = &callback_data;
	//		callback.handler.data_size = sizeof(callback_data);
	//		callback.disable_value_to_modify = true;
	//		config.AddFlag(callback);

	//		data->drawer->CheckBox(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_CHECK_BOX_CALLBACK, config, ascii_name.buffer, &is_active);

	//		if (!function::CompareStrings(filename, ToStream(MODULE_DEFAULT_SETTINGS_PATH))) {
	//			UIConfigAbsoluteTransform right_transform;
	//			right_transform.scale = data->drawer->GetSquareScale(data->drawer->layout.default_element_y * 0.75f);
	//			right_transform.position = data->drawer->GetAlignedToRight(right_transform.scale.x);
	//			config.AddFlag(right_transform);
	//			data->drawer->SpriteRectangle(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LATE_DRAW, config, ECS_TOOLS_UI_TEXTURE_X, data->drawer->color_theme.default_text);

	//			struct DeleteActionData {
	//				const wchar_t* filename;
	//				EditorState* editor_state;
	//				unsigned int module_index;
	//			};

	//			auto delete_action = [](ActionData* action_data) {
	//				UI_UNPACK_ACTION_DATA;

	//				DeleteActionData* data = (DeleteActionData*)_data;

	//				ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	//				GetProjectConfigurationModuleFolder(data->editor_state, absolute_path);
	//				absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	//				absolute_path.AddStreamSafe(ToStream(data->filename));
	//				absolute_path[absolute_path.size] = L'\0';
	//				bool success = RemoveFile(absolute_path);
	//				if (!success) {
	//					ECS_FORMAT_TEMP_STRING(error_message, "Could not delete module settings file {#}.", absolute_path);
	//					EditorSetConsoleError(error_message);
	//				}
	//				else {
	//					// Check to see if that is the current setting
	//					if (function::CompareStrings(ToStream(data->filename), data->editor_state->project_modules->buffer[data->module_index].current_settings_path)) {
	//						DestroyModuleSettings(data->editor_state, data->module_index);
	//						ChangeModuleSettings(data->editor_state, { nullptr, 0 }, data->module_index);
	//					}
	//				}
	//			};
	//			DeleteActionData action_data = { (wchar_t*)name_allocation, data->editor_state, data->module_index };
	//			data->drawer->AddDefaultClickableHoverable(
	//				{ right_transform.position - data->drawer->region_render_offset }, 
	//				right_transform.scale,
	//				{ delete_action, &action_data, sizeof(action_data) }
	//			);
	//		}

	//		data->drawer->NextRow();

	//		return true;
	//	});

	drawer->CrossLine();

	// Present the data now
	drawer->Text(UI_CONFIG_DO_NOT_CACHE, config, "Project Settings: ");
	drawer->NextRow();

	struct SaveFileAfterChangeData {
		EditorState* editor_state;
		unsigned int module_index;
	};

	auto save_file_after_change = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		SaveFileAfterChangeData* data = (SaveFileAfterChangeData*)_data;
		bool success = SaveModuleSettings(data->editor_state, data->module_index);
		if (!success) {
			EditorSetConsoleError(ToStream("An error occured when saving the module setting file after value change. The modification has not been commited."));
		}
	};

	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, instance_indices, editor_state->module_reflection->GetInstanceCount());
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->module_reflection->GetHierarchyInstances(hierarchy_index, instance_indices);

	SaveFileAfterChangeData save_file_data = { editor_state, module_index };

	if (instance_indices.size > 0) {
		UIReflectionDrawConfig ui_reflection_configs[8];

		UIConfigTextInputCallback text_input_callback;
		text_input_callback.handler = { save_file_after_change, &save_file_data, sizeof(save_file_data) };

		ui_reflection_configs[0].configurations = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
		ui_reflection_configs[0].index[0] = UIReflectionIndex::Count;

		ui_reflection_configs[1].configurations = UI_CONFIG_NUMBER_INPUT_DO_NOT_REDUCE_SCALE;
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

		ui_reflection_configs[6].index[0] = UIReflectionIndex::TextInput;
		UIReflectionDrawConfigAddConfig(ui_reflection_configs + 6, &text_input_callback);

		ui_reflection_configs[7].index[0] = UIReflectionIndex::ColorFloat;
		UIReflectionDrawConfigAddConfig(ui_reflection_configs + 7, &text_input_callback);
		UIReflectionDrawConfigAddConfig(ui_reflection_configs + 7, &color_input_callback);

		config.flag_count = 0;
		// The window dependendent size must be adjusted to account for the indentation
		dependent_size.scale_factor.x = drawer->GetWindowSizeFactors(
			WindowSizeTransformType::Horizontal, 
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

			drawer->CollapsingHeader(UI_CONFIG_DO_NOT_CACHE, header_config, instance->name, [&]() {
				editor_state->module_reflection->DrawInstance(
					instance,
					*drawer,
					config,
					{ ui_reflection_configs, std::size(ui_reflection_configs) }
				);
				});
		}
	}

	auto set_default_values = [](ActionData* action_data) {
		SaveFileAfterChangeData* data = (SaveFileAfterChangeData*)action_data->data;

		SetModuleDefaultSettings(data->editor_state, data->module_index);
		bool success = SaveModuleSettings(data->editor_state, data->module_index);
		if (!success) {
			EditorSetConsoleError(ToStream("An error has occured when trying to save default values for module settings file. The modification has not been commited."));
		}
	};
	
	drawer->Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Default Values", { set_default_values, &save_file_data, sizeof(save_file_data) });
	config.flag_count = 0;
}

// ----------------------------------------------------------------------------------------------------------------------------

struct RemoveModuleConfigurationGroupData {
	EditorState* editor_state;
	unsigned int group_index;
	unsigned int library_index;
};

void InspectorDeleteModuleConfigurationGroupAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RemoveModuleConfigurationGroupData* data = (RemoveModuleConfigurationGroupData*)_data;
	RemoveModuleConfigurationGroup(data->editor_state, data->group_index);

	if (data->editor_state->module_configuration_groups.size == 0) {
		DeleteModuleConfigurationGroupFile(data->editor_state);
	}
	else {
		bool success = SaveModuleConfigurationGroupFile(data->editor_state);
		if (!success) {
			EditorSetConsoleError(ToStream("An error occured during module configuration group file save after deleting a group."));
		}
	}
}

void InspectorDrawModuleConfigurationGroup(EditorState* editor_state, void* data, UIDrawer* drawer) {
	EDITOR_STATE(editor_state);
	bool* header_state = (bool*)data;
	const char* module_configuration_group_name = (const char*)function::OffsetPointer(data, 1);
	UIDrawConfig config;
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;

	// Test if the configuration group still exists - if it doesn't revert to draw nothing
	unsigned int group_index = GetModuleConfigurationGroupIndex(editor_state, ToStream(module_configuration_group_name));
	if (group_index == -1) {
		ChangeInspectorToNothing(editor_state);
		return;
	}
	const ModuleConfigurationGroup* group = editor_state->module_configuration_groups.buffer + group_index;

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG);
	// The group name
	drawer->TextLabel(UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT, config, module_configuration_group_name);

	// Specify that it is a module configuration group
	drawer->Indent(-2.0f);
	drawer->TextLabel(UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT, config, " - Module Configuration Group");
	drawer->NextRow();
	drawer->CrossLine();

	// Draw the configurations
	UIConfigCollapsingHeaderDoNotCache header_do_not_cache;
	header_do_not_cache.state = header_state;
	config.AddFlag(header_do_not_cache);
	Stream<const char*> combo_labels = Stream<const char*>(MODULE_CONFIGURATIONS, std::size(MODULE_CONFIGURATIONS));
	unsigned int combo_display_labels = 3;
	
	drawer->CollapsingHeader(UI_CONFIG_DO_NOT_CACHE, config, "Configurations", [&]() {
		ECS_TEMP_ASCII_STRING(ascii_name, 256);
		for (size_t index = 0; index < group->libraries.size; index++) {
			ascii_name.size = 0;
			function::ConvertWideCharsToASCII(group->libraries[index], ascii_name);

			UIConfigComboBoxCallback combo_callback;
			auto combo_callback_function = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				EditorState* editor_state = (EditorState*)_data;
				bool success = SaveModuleConfigurationGroupFile(editor_state);
				if (!success) {
					EditorSetConsoleError(ToStream("An error has occured when writing the module configuration group file after changing configuration."));
				}
			};
			combo_callback.handler = { combo_callback_function, editor_state, 0, UIDrawPhase::Normal };

			config.AddFlag(combo_callback);
			drawer->ComboBox(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_COMBO_BOX_CALLBACK,
				config, 
				ascii_name.buffer, 
				combo_labels,
				combo_display_labels,
				(unsigned char*)group->configurations + index
			);
			config.flag_count--;

			// Draw the remove button
			UIDrawConfig button_config;

			float window_x_scale = drawer->GetXScaleUntilBorder(drawer->current_x - drawer->region_render_offset.x);
			UIConfigRelativeTransform transform;
			transform.scale = { window_x_scale / drawer->layout.default_element_x, 1.0f };
			button_config.AddFlag(transform);

			// The remove action
			auto remove_callback = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				RemoveModuleConfigurationGroupData* data = (RemoveModuleConfigurationGroupData*)_data;
				ModuleConfigurationGroup* group = data->editor_state->module_configuration_groups.buffer + data->group_index;

				// If there are no libraries left, then ask for approval to destroy the group
				if (group->libraries.size == 1) {
					CreateConfirmWindow(system, ToStream("This action will delete the configuration group. Are you sure?"), { InspectorDeleteModuleConfigurationGroupAction, data, sizeof(*data) });
					return;
				}

				// No need to deallocate and create a new one; there is not a big memory footprint to warrant a deallocate and 
				// a new allocation
				for (size_t index = data->library_index; index <= group->libraries.size; index++) {
					group->configurations[index] = group->configurations[index + 1];
				}
				group->libraries.RemoveSwapBack(data->library_index);
				
				// Write the file
				bool success = SaveModuleConfigurationGroupFile(data->editor_state);
				if (!success) {
					EditorSetConsoleError(ToStream("An error occured during module configuration group file save after removing a module."));
				}
			};

			RemoveModuleConfigurationGroupData remove_data;
			remove_data.editor_state = editor_state;
			remove_data.group_index = group_index;
			remove_data.library_index = index;

			drawer->Button(
				UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_CACHE,
				button_config,
				"Remove",
				{ remove_callback, &remove_data, sizeof(remove_data), UIDrawPhase::System }
			);
			drawer->NextRow();
		}

	});

	drawer->NextRow(2.0f);

	// Draw the add button
	UIDrawConfig button_config;

	UIConfigWindowDependentSize size;
	button_config.AddFlag(size);

	UIConfigActiveState active_state;
	// Check that there is at least a module that can be added
	active_state.state = group->libraries.size < (modules->size - !HasGraphicsModule(editor_state));
	button_config.AddFlag(active_state);

	struct ButtonData {
		EditorState* editor_state;
		unsigned int index;
	};

	// The add sign
	auto add_callback = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		ButtonData* data = (ButtonData*)_data;
		CreateModuleConfigurationGroupAddWizard(data->editor_state, data->index);
	};

	ButtonData add_data;
	add_data.editor_state = editor_state;
	add_data.index = group_index;

	drawer->Button(
		UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE | UI_CONFIG_DO_NOT_CACHE,
		button_config,
		"Add",
		{ add_callback, &add_data, sizeof(add_data), UIDrawPhase::System }
	);
	drawer->NextRow();

	UIConfigComboBoxCallback fallback_callback;

	auto fallback_callback_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		EditorState* editor_state = (EditorState*)_data;
		bool success = SaveModuleConfigurationGroupFile(editor_state);
		if (!success) {
			EditorSetConsoleError(ToStream("An error has occured when writing the module configuration group file after changing the fallback configuration."));
		}
	};

	fallback_callback.handler = { fallback_callback_action, editor_state, 0, UIDrawPhase::Normal };

	config.AddFlag(fallback_callback);
	// Draw the fallback configuration for the other modules
	drawer->ComboBox(
		UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_COMBO_BOX_CALLBACK,
		config, 
		"Fallback configuration",
		combo_labels, 
		combo_display_labels, 
		(unsigned char*)group->configurations + group->libraries.size
	);
	config.flag_count--;
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorWindowDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	constexpr float PADLOCK_SIZE = 0.04f;

	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	InspectorData* data = editor_state->inspector_data;

	if (initialize) {
		// Might be called multiple times
		//if (data->table.m_capacity == 0) {
		InitializeInspector(editor_state);
		//}

		ChangeInspectorToFile(editor_state, ToStream(L"C:\\Users\\Andrei\\Test\\Assets\\Textures\\ErrorIcon.png"));
	}
	else {
		// The padlock
		UIDrawConfig config;
		UIConfigRelativeTransform transform;
		transform.scale.y = PADLOCK_SIZE / drawer.GetStaticElementDefaultScale().y;
		transform.offset.y = (ICON_SIZE - PADLOCK_SIZE) / 2;
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
		drawer.current_row_y_scale = ICON_SIZE;

		if (data->draw_function != nullptr) {
			data->draw_function(editor_state, data->draw_data, &drawer);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorWindow(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	size_t stack_memory[128];
	InspectorSetDescriptor(descriptor, editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
}

// ----------------------------------------------------------------------------------------------------------------------------

void CreateInspector(EditorState* editor_state) {
	CreateDockspaceFromWindow(INSPECTOR_WINDOW_NAME, editor_state, CreateInspectorWindow);
}

// ----------------------------------------------------------------------------------------------------------------------------

void CreateInspectorAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateInspector((EditorState*)_data);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorDrawFunction(EditorState* editor_state, InspectorFunctions functions, void* data, size_t data_size)
{
	EDITOR_STATE(editor_state);

	InspectorData* inspector_data = editor_state->inspector_data;
	// Only change the function if the inspector is unlocked
	if (!function::HasFlag(inspector_data->flags, INSPECTOR_FLAG_LOCKED)) {
		inspector_data->clean_function(editor_state, inspector_data->draw_data);

		if (inspector_data->data_size > 0) {
			editor_allocator->Deallocate(inspector_data->draw_data);
		}
		inspector_data->draw_data = function::AllocateMemory(editor_allocator, data, data_size);
		inspector_data->draw_function = functions.draw_function;
		inspector_data->clean_function = functions.clean_function;
		inspector_data->data_size = data_size;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void* GetInspectorDrawFunctionData(EditorState* editor_state) {
	return editor_state->inspector_data->draw_data;
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToNothing(EditorState* editor_state) {
	ChangeInspectorDrawFunction(editor_state, { InspectorDrawNothing, InspectorCleanNothing }, nullptr, 0);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToFolder(EditorState* editor_state, Stream<wchar_t> path) {
	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';

	ChangeInspectorDrawFunction(editor_state, { InspectorDrawFolderInfo, InspectorCleanNothing }, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToMeshFile(EditorState* editor_state, Stream<wchar_t> path) {
	InspectorDrawMeshFileData data;
	data.mesh = nullptr;
	data.radius_delta = 0.0f;
	data.rotation_delta = { 0.0f, 0.0f };

	// Allocate the data and embedd the path in it
	ChangeInspectorDrawFunction(editor_state, { InspectorDrawMeshFile, InspectorCleanMeshFile }, &data, sizeof(data) + sizeof(wchar_t) * (path.size + 1));

	// Get the data and set the path
	InspectorDrawMeshFileData* draw_data = (InspectorDrawMeshFileData*)GetInspectorDrawFunctionData(editor_state);
	draw_data->path = { function::OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
	draw_data->path.Copy(path);
	draw_data->path[draw_data->path.size] = L'\0';
}

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path) {
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
		ChangeInspectorDrawFunction(editor_state, functions, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
	}
	else {
		ChangeInspectorToMeshFile(editor_state, path);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index) {
	ECS_ASSERT(index < editor_state->project_modules->size);

	Stream<wchar_t> module_name = function::StringCopy(editor_state->editor_allocator, editor_state->project_modules->buffer[index].library_name);
	DrawModuleData draw_data;
	draw_data.linear_allocator = nullptr;
	draw_data.module_name = module_name;
	draw_data.window_index = 0;
	ChangeInspectorDrawFunction(
		editor_state,
		{ InspectorDrawModule, InspectorDrawModuleClean },
		&draw_data,
		sizeof(draw_data)
	);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToGraphicsModule(EditorState* editor_state) {
	ChangeInspectorToModule(editor_state, GRAPHICS_MODULE_INDEX);
	/*DrawModuleData draw_data;
	draw_data.linear_allocator = nullptr;
	draw_data.module_name = GRAPHICS_MODULE_INDEX;
	draw_data.window_index = 0;
	ChangeInspectorDrawFunction(
		editor_state,
		{ InspectorDrawModule, InspectorDrawModuleClean },
		&draw_data,
		sizeof(draw_data)
	);*/
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToModuleConfigurationGroup(EditorState* editor_state, unsigned int index) {
	ECS_ASSERT(index < editor_state->module_configuration_groups.size);

	// First char must be a bool for the header non cached state
	ECS_TEMP_ASCII_STRING(null_terminated_path, 256);
	null_terminated_path[0] = '\0';
	editor_state->module_configuration_groups[index].name.CopyTo(null_terminated_path.buffer + 1);
	// Null terminate the name
	null_terminated_path[editor_state->module_configuration_groups[index].name.size + 1] = '\0';
	ChangeInspectorDrawFunction(
		editor_state, 
		{ InspectorDrawModuleConfigurationGroup, InspectorCleanNothing },
		null_terminated_path.buffer, 
		editor_state->module_configuration_groups[index].name.size + 2
	);
}

// ----------------------------------------------------------------------------------------------------------------------------

void LockInspector(EditorState* editor_state)
{
	InspectorData* data = editor_state->inspector_data;
	data->flags = function::SetFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void UnlockInspector(EditorState* editor_state) 
{
	InspectorData* data = editor_state->inspector_data;
	data->flags = function::ClearFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspector(EditorState* editor_state)
{
	InspectorData* data = editor_state->inspector_data;
	data->draw_function = nullptr;
	data->clean_function = InspectorCleanNothing;
	data->data_size = 0;
	data->draw_data = nullptr;
	data->flags = 0;

	void* allocation = editor_state->editor_allocator->Allocate(InspectorTable::MemoryOf(POINTER_CAPACITY));
	data->table.InitializeFromBuffer(allocation, POINTER_CAPACITY);

	AddInspectorTableFunction(&data->table, { InspectorDrawTexture, InspectorCleanNothing }, L".png");
	AddInspectorTableFunction(&data->table, { InspectorDrawTexture, InspectorCleanNothing }, L".jpg");
	AddInspectorTableFunction(&data->table, { InspectorDrawTexture, InspectorCleanNothing }, L".bmp");
	AddInspectorTableFunction(&data->table, { InspectorDrawTexture, InspectorCleanNothing }, L".tiff");
	AddInspectorTableFunction(&data->table, { InspectorDrawTexture, InspectorCleanNothing }, L".tga");
	AddInspectorTableFunction(&data->table, { InspectorDrawTexture, InspectorCleanNothing }, L".hdr");
	AddInspectorTableFunction(&data->table, { InspectorDrawTextFile, InspectorCleanNothing }, L".txt");
	AddInspectorTableFunction(&data->table, { InspectorDrawTextFile, InspectorCleanNothing }, L".md");
	AddInspectorTableFunction(&data->table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".cpp");
	AddInspectorTableFunction(&data->table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".h");
	AddInspectorTableFunction(&data->table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".hpp");
	AddInspectorTableFunction(&data->table, { InspectorDrawCTextFile, InspectorCleanNothing }, L".c");
	AddInspectorTableFunction(&data->table, { InspectorDrawHlslTextFile, InspectorCleanNothing }, L".hlsl");
	AddInspectorTableFunction(&data->table, { InspectorDrawHlslTextFile, InspectorCleanNothing }, L".hlsli");
	AddInspectorTableFunction(&data->table, { InspectorDrawMeshFile, InspectorCleanMeshFile }, L".gltf");
	AddInspectorTableFunction(&data->table, { InspectorDrawMeshFile, InspectorCleanMeshFile }, L".glb");
}
