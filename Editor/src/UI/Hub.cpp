#include "editorpch.h"
#include "Hub.h"
#include "HubData.h"
#include "..\Editor\EditorParameters.h"
#include "..\Editor\EditorState.h"
#include "..\Project\ProjectUITemplatePreview.h"
#include "..\Editor\EditorFile.h"
#include "..\Editor\EditorEvent.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr float INCREASE_FONT_SIZE_FACTOR = 1.3f;

void AddHubProject(EditorState* editor_state, const wchar_t* path)
{
	AddHubProject(editor_state, ToStream(path));
}

void AddHubProject(EditorState* editor_state, Stream<wchar_t> path) {
	EDITOR_STATE(editor_state);

	HubData* hub = editor_state->hub_data;
	ECS_ASSERT(hub->projects.size < hub->projects.capacity);

	hub->projects[hub->projects.size].error_message = nullptr;
	hub->projects[hub->projects.size].path = (wchar_t*)editor_allocator->Allocate(sizeof(wchar_t) * (path.size + 1), alignof(wchar_t));
	memcpy((void*)hub->projects[hub->projects.size].path, path.buffer, sizeof(wchar_t) * path.size);
	wchar_t* mutable_ptr = (wchar_t*)hub->projects[hub->projects.size].path;
	mutable_ptr[path.size] = L'\0';
	hub->projects.size++;

	if (!SaveEditorFile(editor_state)) {
		EditorSetConsoleError(ToStream("Error when saving editor file after project addition"));
	}
}

void DeallocateHubProjects(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	HubData* data = editor_state->hub_data;
	for (size_t index = 0; index < data->projects.size; index++) {
		if (data->projects[index].error_message != nullptr) {
			editor_allocator->Deallocate(data->projects[index].error_message);
			data->projects[index].error_message = nullptr;
		}
		/*else {
			const void* first_pointer = ui_reflection->reflection->GetTypeInstancePointer(STRING(ProjectFile), data->projects.buffer + index);
			editor_allocator->Deallocate(first_pointer);
		}*/
	}
}

void AddExistingProjectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ECS_TEMP_STRING(path, 256);
	char temp_characters[256];
	CapacityStream<char> error_message(temp_characters, 0, 256);
	OSFileExplorerGetFileData get_data;
	get_data.error_message = error_message;
	get_data.path = path;
	FileExplorerGetFile(&get_data);

	if (get_data.error_message.size > 0) {
		EditorSetConsoleError(get_data.error_message);
	}
	else {
		if (get_data.path.size > 0) {
			AddHubProject((EditorState*)_data, get_data.path);
		}
	}
}

void LoadHubProjects(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	HubData* hub_data = editor_state->hub_data;

	for (size_t index = 0; index < hub_data->projects.size; index++) {
		hub_data->projects[index].data.path = function::PathParent(ToStream(hub_data->projects[index].path));
		hub_data->projects[index].data.project_name = function::PathStem(ToStream(hub_data->projects[index].path));

		ProjectOperationData open_data;
		char error_message[256];
		open_data.error_message.InitializeFromBuffer(error_message, 0, 256);
		open_data.file_data = &hub_data->projects[index].data;
		open_data.editor_state = editor_state;

		bool success = OpenProjectFile(open_data);

		if (!success) {
			size_t error_message_size = strlen(error_message) + 1;
			void* allocation = editor_allocator->Allocate(error_message_size, alignof(char));
			hub_data->projects[index].error_message = (char*)allocation;
			memcpy(allocation, error_message, error_message_size);
		}
	}
}

void ReloadHubProjects(EditorState* editor_state) {
	ResetHubData(editor_state);
	LoadEditorFile(editor_state);
	LoadHubProjects(editor_state);
	SortHubProjects(editor_state);
}

void ReloadHubProjectsAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ReloadHubProjects((EditorState*)_data);
}

void ResetHubData(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	HubData* data = editor_state->hub_data;
	DeallocateHubProjects(editor_state);
	data->projects.size = 0;
}

void RemoveHubProject(EditorState* editor_state, const wchar_t* path)
{
	RemoveHubProject(editor_state, ToStream(path));
}

void RemoveHubProject(EditorState* editor_state, Stream<wchar_t> path)
{
	EDITOR_STATE(editor_state);

	HubData* hub_data = editor_state->hub_data;
	for (size_t index = 0; index < hub_data->projects.size; index++) {
		if (function::CompareStrings(path, ToStream(hub_data->projects[index].path))) {
			editor_allocator->Deallocate(hub_data->projects[index].path);
			if (hub_data->projects[index].error_message != nullptr) {
				editor_allocator->Deallocate(hub_data->projects[index].error_message);
			}
			hub_data->projects.Remove(index);
			task_manager->AddDynamicTaskAndWake(ECS_THREAD_TASK_NAME(SaveEditorFileThreadTask, editor_state, 0));
			return;
		}
	}
}

void SortHubProjects(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	HubData* data = editor_state->hub_data;

	struct SortElement {
		bool operator < (const SortElement& other) const {
			return file_time < other.file_time;
		}

		bool operator <= (const SortElement& other) const {
			return file_time <= other.file_time;
		}

		bool operator > (const SortElement& other) const {
			return file_time > other.file_time;
		}

		bool operator >= (const SortElement& other) const {
			return file_time >= other.file_time;
		}

		size_t file_time;
		HubProject project;
	};

	SortElement* sort_elements = (SortElement*)alloca(sizeof(SortElement) * data->projects.size);
	for (size_t index = 0; index < data->projects.size; index++) {
		sort_elements[index].file_time = ULLONG_MAX;
		sort_elements[index].project = data->projects[index];
	}
	for (size_t index = 0; index < data->projects.size; index++) {
		OS::GetRelativeFileTimes(data->projects[index].path, nullptr, nullptr, &sort_elements[index].file_time);
	}

	function::insertion_sort(sort_elements, data->projects.size);
	for (size_t index = 0; index < data->projects.size; index++) {
		data->projects[index] = sort_elements[index].project;
	}
}

void HubDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	HubData* data = editor_state->hub_data;

	UIFontDescriptor* font = drawer.GetFontDescriptor();
	font->size *= INCREASE_FONT_SIZE_FACTOR;
	font->character_spacing *= INCREASE_FONT_SIZE_FACTOR;

#pragma region General info

	drawer.DisableZoom();
	drawer.DisablePaddingForRenderSliders();
	UIDrawConfig config;
	UIConfigTextParameters ecs_text;
	ecs_text.size *= {1.75f, 1.75f};
	ecs_text.color = drawer.GetColorThemeDescriptor()->default_text;
	ecs_text.character_spacing *= 1.75f;

	char ecs_characters[128];
	memcpy(ecs_characters, "ECSEngine Version: ", 19);
	size_t version_size = strlen(VERSION_DESCRIPTION);
	memcpy(ecs_characters + 19, VERSION_DESCRIPTION, version_size);
	ecs_characters[19 + version_size] = '\0';
	config.AddFlag(ecs_text);

	drawer.Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_PARAMETERS, config, ecs_characters);

	UIConfigAbsoluteTransform transform;
	transform.scale = { drawer.region_limit.x - drawer.current_x, drawer.current_row_y_scale };
	transform.position = drawer.GetAlignedToRight(transform.scale.x);

	UIConfigTextAlignment alignment;
	alignment.horizontal = TextAlignment::Right;
	alignment.vertical = TextAlignment::Middle;
	config.AddFlag(transform);
	config.AddFlag(alignment);

	char user_characters[128];
	memcpy(user_characters, "User: ", 6);
	size_t user_size = strlen(USER);
	memcpy(user_characters + 6, USER, user_size);
	user_characters[6 + user_size] = '\0';
	drawer.TextLabel(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_ABSOLUTE_TRANSFORM
		| UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_TEXT_PARAMETERS, config, user_characters);

	drawer.NextRow();
	drawer.CrossLine();
	drawer.Text(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_PARAMETERS, config, "Projects");

	CreateProjectWizardData* wizard_data;
	char* save_project_error_message = nullptr;

	constexpr size_t error_message_size = 256;
	if (initialize) {
		wizard_data = drawer.GetMainAllocatorBufferAndStoreAsResource<CreateProjectWizardData>("Create project wizard data");
		wizard_data->project_data.error_message.InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(char) * error_message_size), 0, error_message_size);
		wizard_data->project_data.editor_state = editor_state;
		wizard_data->copy_project_name_to_source_dll = true;
		wizard_data->project_data.file_data = editor_state->project_file;

		save_project_error_message = (char*)drawer.GetMainAllocatorBufferAndStoreAsResource("Save project error message", error_message_size, alignof(char));
	}
	else {
		wizard_data = (CreateProjectWizardData*)drawer.GetResource("Create project wizard data");
		save_project_error_message = (char*)drawer.GetResource("Save project error message");
	}

	drawer.Button(UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE, config, "Create Project", { CreateProjectWizardAction, wizard_data, 0, UIDrawPhase::System });
	drawer.Button(UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE, config, "Add existing Project", { AddExistingProjectAction, editor_state, 0, UIDrawPhase::System });
	drawer.Button(UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE, config, "Reload Projects", { ReloadHubProjectsAction, editor_state, 0 });

	drawer.Button(UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE, config, "Default Project UI", { CreateProjectUITemplatePreviewAction, editor_state, 0, UIDrawPhase::System });

#pragma endregion

#pragma region Column headers
	drawer.NextRow(2.0f);

	float2 table_start_position = drawer.GetCurrentPosition();
	float end_table_position = drawer.region_limit.x;

#define FIRST_COLUMN_SCALE 0.5f
#define SECOND_COLUMN_SCALE 0.5f
#define THIRD_COLUMN_SCALE 0.4f
#define COLUMN_Y_SCALE 0.15f
#define REMOVE_BUTTON_SCALE 0.04f
#define REMOVE_BUTTON_PADDING 0.007f

	UIDrawConfig table_config;
	UIConfigAbsoluteTransform table_transform;
	drawer.CrossLine(UI_CONFIG_LATE_DRAW, table_config);

	UIConfigTextAlignment table_alignment;
	table_alignment.horizontal = TextAlignment::Middle;
	table_alignment.vertical = TextAlignment::Middle;
	table_transform.position = table_start_position;
	table_transform.scale = { FIRST_COLUMN_SCALE, COLUMN_Y_SCALE };

	UIConfigTextParameters table_text;
	table_text.size *= {1.3f, 1.3f};
	table_text.character_spacing *= 1.3f;
	table_text.color = drawer.GetColorThemeDescriptor()->default_text;

	table_config.AddFlag(table_alignment);
	table_config.AddFlag(table_text);
	table_config.AddFlag(table_transform);

	const size_t COLUMN_HEADER_CONFIGURATION = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_TEXT_PARAMETERS
		| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
	drawer.TextLabel(COLUMN_HEADER_CONFIGURATION, table_config, "Project Name");

	table_config.flag_count--;

	table_transform.position.x += table_transform.scale.x;
	table_transform.scale.x = SECOND_COLUMN_SCALE;
	table_config.AddFlag(table_transform);
	drawer.TextLabel(COLUMN_HEADER_CONFIGURATION, table_config, "Version");

	table_config.flag_count--;
	
	table_transform.position.x += table_transform.scale.x;
	table_transform.scale.x = THIRD_COLUMN_SCALE;
	table_config.AddFlag(table_transform);
	drawer.TextLabel(COLUMN_HEADER_CONFIGURATION, table_config, "Platform");

	table_config.flag_count--;

	table_transform.position.x += table_transform.scale.x;
	table_transform.scale.x = end_table_position - table_transform.position.x;
	table_config.AddFlag(table_transform);
	drawer.TextLabel(COLUMN_HEADER_CONFIGURATION, table_config, "Last Time Modified");
	drawer.NextRow(0.0f);
	drawer.CrossLine(UI_CONFIG_LATE_DRAW | UI_CONFIG_DO_NOT_ADVANCE, config);
#pragma endregion

#pragma region Rows

	font->size /= INCREASE_FONT_SIZE_FACTOR;
	font->character_spacing /= INCREASE_FONT_SIZE_FACTOR;

	const size_t ROW_LABEL_CONFIGURATION = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_ABSOLUTE_TRANSFORM
		| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;

	unsigned int invalid_indices_buffer[128];
	Stream<unsigned int> invalid_indices = Stream<unsigned int>(invalid_indices_buffer, 0);
	auto row_lambda = [&](unsigned int index) {
		float2 row_start_position = { table_start_position.x, drawer.GetCurrentPositionNonOffset().y };

		auto row_project_name = [&](UIDrawConfig& config, UIConfigAbsoluteTransform& transform, size_t index) {
			char temp_characters[256];
			function::ConvertWideCharsToASCII(data->projects[index].path, temp_characters, wcsnlen(data->projects[index].path, 256), 256);

			transform.position = row_start_position;
			transform.scale = { FIRST_COLUMN_SCALE, COLUMN_Y_SCALE };
			config.AddFlag(transform);

			UIConfigTextAlignment alignment;
			alignment.horizontal = TextAlignment::Left;
			alignment.vertical = TextAlignment::Top;
			config.AddFlag(alignment);

			char ascii_name[256];
			Stream<wchar_t> path_stem = function::PathStem(ToStream(data->projects[index].path));
			function::ConvertWideCharsToASCII(path_stem.buffer, ascii_name, path_stem.size, 256);
			drawer.TextLabel(ROW_LABEL_CONFIGURATION, config, ascii_name);

			alignment.vertical = TextAlignment::Bottom;
			config.flag_count--;
			config.AddFlag(alignment);

			drawer.TextLabel(ROW_LABEL_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT, config, temp_characters);

			config.flag_count -= 2;
		};

		auto row_platform_and_version = [&](UIDrawConfig& config, UIConfigAbsoluteTransform& transform, size_t index) {
			alignment.vertical = TextAlignment::Middle;
			alignment.horizontal = TextAlignment::Middle;
			config.AddFlag(alignment);

			transform.position.x += transform.scale.x;
			transform.scale.x = SECOND_COLUMN_SCALE;
			config.AddFlag(transform);

			drawer.TextLabel(ROW_LABEL_CONFIGURATION, config, data->projects[index].data.version_description);

			config.flag_count--;

			transform.position.x += transform.scale.x;
			transform.scale.x = THIRD_COLUMN_SCALE;
			config.AddFlag(transform);

			drawer.TextLabel(ROW_LABEL_CONFIGURATION, config, data->projects[index].data.platform_description);

			config.flag_count--;
		};

		auto row_cross_line = [&](UIDrawConfig& config, UIConfigAbsoluteTransform& transform) {
			transform.position.x = table_start_position.x;
			transform.position.y += transform.scale.y;
			transform.scale.x = end_table_position - table_start_position.x;
			config.AddFlag(transform);

			drawer.NextRow(0.0f);
			drawer.CrossLine(UI_CONFIG_LATE_DRAW | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, config);
			config.flag_count--;
		};

		if (data->projects[index].error_message == nullptr) {
			char last_write_time[128];
			last_write_time[0] = '\0';
			bool succeeded = OS::GetRelativeFileTimes(data->projects[index].path, nullptr, nullptr, last_write_time);

			if (succeeded) {
				UIDrawConfig config;
				UIConfigAbsoluteTransform transform;

				row_project_name(config, transform, index);
				row_platform_and_version(config, transform, index);

				transform.position.x += transform.scale.x;
				transform.scale.x = end_table_position - transform.position.x;
				config.AddFlag(transform);

				drawer.TextLabel(ROW_LABEL_CONFIGURATION, config, last_write_time);
				config.flag_count--;
				
				row_cross_line(config, transform);
			}
			else {
				UIDrawConfig config;
				UIConfigAbsoluteTransform transform;

				row_project_name(config, transform, index);
				row_platform_and_version(config, transform, index);

				transform.position.x += transform.scale.x;
				transform.scale.x = end_table_position - transform.position.x;
				config.AddFlag(transform);

				drawer.TextLabel(ROW_LABEL_CONFIGURATION | UI_CONFIG_UNAVAILABLE_TEXT, config, "Unable to read last write time");
				config.flag_count--;
				
				row_cross_line(config, transform);
			}

			config.flag_count = 0;
			transform.position = row_start_position;
			transform.scale = { end_table_position - row_start_position.x, COLUMN_Y_SCALE };
			config.AddFlag(transform);

			ProjectOperationData open_data;
			open_data.error_message.buffer = nullptr;
			open_data.file_data = &data->projects[index].data;
			open_data.editor_state = editor_state;
			drawer.Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
				| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, config, "", { OpenProjectAction, &open_data, sizeof(open_data) });
		}
		else {
			UIDrawConfig config;
			UIConfigAbsoluteTransform transform;

			row_project_name(config, transform, index);

			alignment.vertical = TextAlignment::Middle;
			alignment.horizontal = TextAlignment::Middle;
			config.AddFlag(alignment);

			transform.position.x += transform.scale.x;
			transform.scale.x = end_table_position - transform.position.x;
			config.AddFlag(transform);

			drawer.TextLabel(ROW_LABEL_CONFIGURATION, config, data->projects[index].error_message);
			config.flag_count--;
			
			row_cross_line(config, transform);
		}

		float2 cross_line_position = drawer.GetLastSolidColorRectanglePosition();
		float2 cross_line_scale = drawer.GetLastSolidColorRectangleScale();

		config.flag_count = 0;
		UIConfigAbsoluteTransform transform;
		transform.position.x = end_table_position - drawer.GetSquareScale(REMOVE_BUTTON_SCALE).x - REMOVE_BUTTON_PADDING;
		transform.position.y = AlignMiddle(cross_line_position.y, COLUMN_Y_SCALE, REMOVE_BUTTON_SCALE);
		transform.scale = drawer.GetSquareScale(REMOVE_BUTTON_SCALE);
		config.AddFlag(transform);

		struct RemoveProjectData {
			EditorState* editor_state;
			const wchar_t* project_file;
		};

		auto remove_project_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;
			
			RemoveProjectData* data = (RemoveProjectData*)_data;
			RemoveHubProject(data->editor_state, data->project_file);
		};

		RemoveProjectData remove_data = { editor_state, data->projects[index].path };
		drawer.SpriteButton(UI_CONFIG_ABSOLUTE_TRANSFORM, config, { remove_project_action, &remove_data, sizeof(remove_data) }, ECS_TOOLS_UI_TEXTURE_X, drawer.color_theme.default_text);
	};

	for (size_t index = 0; index < data->projects.size; index++) {
		row_lambda(index);
	}

	float current_y = drawer.GetCurrentPositionNonOffset().y;
	transform.position = table_start_position;
	transform.scale.y = current_y - table_start_position.y;
	transform.scale.x = 0.0f;
	config.flag_count = 0;

	config.AddFlag(transform);

	drawer.CrossLine(UI_CONFIG_VERTICAL | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_CROSS_LINE_DO_NOT_INFER | UI_CONFIG_LATE_DRAW, config);
	transform.position.x = end_table_position;
	config.flag_count = 0;
	config.AddFlag(transform);
	
	drawer.CrossLine(UI_CONFIG_VERTICAL | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_CROSS_LINE_DO_NOT_INFER | UI_CONFIG_LATE_DRAW, config);

#pragma endregion

}

void Hub(EditorState* editor_state) {
	EDITOR_STATE(editor_state);
	
	UIWindowDescriptor window_descriptor;
	window_descriptor.window_name = "Project";
	window_descriptor.draw = HubDraw;

	window_descriptor.initial_position_x = -1.0f;
	window_descriptor.initial_position_y = -1.0f;
	window_descriptor.initial_size_x = 2.0f;
	window_descriptor.initial_size_y = 2.0f;

	bool success = LoadEditorFile(editor_state);
	LoadHubProjects(editor_state);
	SortHubProjects(editor_state);

	window_descriptor.resource_count = 8192 * 4;
	window_descriptor.window_data = editor_state;
	window_descriptor.window_data_size = 0;
	ui_system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_BORDER_NOTHING | UI_DOCKSPACE_BACKGROUND);

	editor_state->editor_tick = EditorStateHubTick;

	if (!success) {
		CreateErrorMessageWindow(ui_system, "No editor file has been found or it has been corrupted.");
	}
}

void HubTick(EditorState* editor_state) {}
