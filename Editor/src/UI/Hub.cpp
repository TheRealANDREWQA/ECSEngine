#include "editorpch.h"
#include "Hub.h"
#include "HubData.h"
#include "..\Editor\EditorParameters.h"
#include "..\Editor\EditorState.h"
#include "..\Project\ProjectUITemplatePreview.h"
#include "..\Editor\EditorFile.h"
#include "..\Editor\EditorEvent.h"

#include "..\UI\Backups.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr float INCREASE_FONT_SIZE_FACTOR = 1.3f;

void AddHubProject(EditorState* editor_state, Stream<wchar_t> path) {
	HubData* hub = editor_state->hub_data;
	ECS_ASSERT(hub->projects.size < hub->projects.capacity);

	unsigned int index = hub->projects.size;
	hub->projects[index].error_message = { nullptr, 0 };
	hub->projects[index].data.path = StringCopy(hub->allocator, path);

	// The project name will only reference the filename of the path
	Stream<wchar_t> project_name = PathFilename(hub->projects[hub->projects.size].data.path);
	hub->projects[index].data.project_name = project_name;
	hub->projects.size++;

	if (!SaveEditorFile(editor_state)) {
		EditorSetConsoleError("Error when saving editor file after project addition");
	}

	LoadHubProjectInfo(editor_state, index);
}

void DeallocateHubProject(EditorState* editor_state, unsigned int project_hub_index)
{
	AllocatorPolymorphic allocator = editor_state->hub_data->allocator;
	HubData* data = editor_state->hub_data;

	data->projects[project_hub_index].data.path.Deallocate(allocator);
	data->projects[project_hub_index].error_message.Deallocate(allocator);
}

void DeallocateHubProjects(EditorState* editor_state)
{
	HubData* data = editor_state->hub_data;
	for (unsigned int index = 0; index < data->projects.size; index++) {
		DeallocateHubProject(editor_state, index);
	}
}

void AddExistingProjectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
	ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
	OS::FileExplorerGetFileData get_data;
	get_data.error_message = error_message;
	get_data.path = path;
	Stream<wchar_t> extensions[] = {
		L".ecsproj"
	};
	get_data.extensions = { &extensions, std::size(extensions) };
	OS::FileExplorerGetFile(&get_data);

	if (get_data.error_message.size > 0) {
		EditorSetConsoleError(get_data.error_message);
	}
	else {
		if (get_data.path.size > 0) {
			AddHubProject((EditorState*)_data, PathParent(get_data.path));
		}
	}
}

bool LoadHubProjectInfo(EditorState* editor_state, unsigned int index) {
	AllocatorPolymorphic allocator = editor_state->hub_data->allocator;

	ProjectOperationData open_data;
	ECS_STACK_CAPACITY_STREAM(char, error_message_storage, 512);
	open_data.error_message = error_message_storage;
	open_data.file_data = &editor_state->hub_data->projects[index].data;
	open_data.editor_state = editor_state;

	bool success = OpenProjectFile(open_data, true);

	if (!success) {
		editor_state->hub_data->projects[index].error_message.Deallocate(allocator);
		editor_state->hub_data->projects[index].error_message.InitializeAndCopy(allocator, open_data.error_message);
	}

	return success;
}

void LoadHubProjectsInfo(EditorState* editor_state)
{
	HubData* hub_data = editor_state->hub_data;

	for (size_t index = 0; index < hub_data->projects.size; index++) {
		LoadHubProjectInfo(editor_state, index);
	}
}

void ReloadHubProjects(EditorState* editor_state) {
	ResetHubData(editor_state);
	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	bool success = LoadEditorFile(editor_state, &error_message);
	if (!success || error_message.size > 0) {
		error_message.Insert(0, "Failed to read editor settings. ");
		CreateErrorMessageWindow(editor_state->ui_system, error_message);
	}
	LoadHubProjectsInfo(editor_state);
	SortHubProjects(editor_state);
}

void ReloadHubProjectsAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ReloadHubProjects((EditorState*)_data);
}

void ResetHubData(EditorState* editor_state) {
	HubData* data = editor_state->hub_data;
	DeallocateHubProjects(editor_state);
	data->projects.size = 0;
}

void RemoveHubProject(EditorState* editor_state, Stream<wchar_t> path)
{
	TaskManager* task_manager = editor_state->task_manager;
	
	HubData* hub_data = editor_state->hub_data;
	for (size_t index = 0; index < hub_data->projects.size; index++) {
		if (path == hub_data->projects[index].data.path) {
			DeallocateHubProject(editor_state, index);
			hub_data->projects.Remove(index);
			task_manager->AddDynamicTaskAndWake(ECS_THREAD_TASK_NAME(SaveEditorFileThreadTask, editor_state, 0));
			return;
		}
	}
}

// The path is the parent directory
// Creates an error message window if it fails
bool RestoreHubProject(EditorState* editor_state, Stream<wchar_t> path) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, project_file_path, 512);
	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);

	ProjectFile project_file = GetProjectFileDefaultConfiguration();
	project_file.path = path;

	GetProjectFilePath(&project_file, project_file_path);
	ProjectOperationData operation_data;
	operation_data.editor_state = editor_state;
	operation_data.file_data = &project_file;
	operation_data.error_message = error_message;

	if (!SaveProjectFile(operation_data)) {
		CreateErrorMessageWindow(editor_state->ui_system, operation_data.error_message);
		return false;
	}
	else {
		AddHubProject(editor_state, project_file.path);
	}

	return true;
}

void RestoreHubProjectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	
	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	ECS_STACK_CAPACITY_STREAM(wchar_t, path, 512);

	// Get the folder
	OS::FileExplorerGetDirectoryData get_directory;
	get_directory.path = path;
	get_directory.error_message = error_message;

	if (OS::FileExplorerGetDirectory(&get_directory)) {
		RestoreHubProject(editor_state, get_directory.path);
	}
	else {
		CreateErrorMessageWindow(system, "Could not get retrieve project path.");
	}
}

static void AutoDetectCompilerAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	const size_t MAX_COMPILERS = 64;

	EditorState* editor_state = (EditorState*)_data;
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 8);
	ECS_STACK_CAPACITY_STREAM(CompilerVersion, versions, MAX_COMPILERS);
	AutoDetectCompilers(&stack_allocator, &versions);

	if (versions.size > 0) {
		if (versions.size == 1) {
			// Collocate the compiler path and editing ide path at the moment into a single call
			struct SelectData {
				EditorState* editor_state;
				Stream<wchar_t> compiler_path;
				Stream<wchar_t> editing_ide_path;
			};
			
			auto select_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SelectData* data = (SelectData*)_data;
				if (!ChangeCompilerVersionAndEditingIdeExecutablePath(data->editor_state, data->compiler_path, data->editing_ide_path)) {
					CreateErrorMessageWindow(system, "Failed to save editor file after changing compiler version (and editing IDE executable path)");
				}
				data->compiler_path.Deallocate(data->editor_state->EditorAllocator());
				data->editing_ide_path.Deallocate(data->editor_state->EditorAllocator());
			};

			ECS_FORMAT_TEMP_STRING(confirm_window_message, "The compiler that was auto detected is {#}. Select this as the compiler?", versions[0].aggregated);
			// Allocate the string with the editor allocator such that it is stable
			SelectData select_data = { editor_state, versions[0].compiler_path.Copy(editor_state->EditorAllocator()), versions[0].editing_ide_path.Copy(editor_state->EditorAllocator()) };
			CreateConfirmWindow(system, confirm_window_message, { select_action, &select_data, sizeof(select_data), ECS_UI_DRAW_SYSTEM });
		}
		else {
			// Can't be captured in the lambda body if using constexpr, make it a macro
			#define COLLOCATE_CHARACTER_SEPARATOR L'|'

			auto select_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				EditorState* editor_state = (EditorState*)_data;
				ChooseElementCallbackData* callback_data = (ChooseElementCallbackData*)_additional_data;
				// Split the additional data into the compiler path and editor IDE path
				Stream<wchar_t> collocated_path = callback_data->additional_data.As<wchar_t>();
				Stream<wchar_t> separator = FindFirstCharacter(collocated_path, COLLOCATE_CHARACTER_SEPARATOR);

				if (!ChangeCompilerVersionAndEditingIdeExecutablePath(editor_state, collocated_path.StartDifference(separator), separator.AdvanceReturn())) {
					CreateErrorMessageWindow(system, "Failed to save editor file after changing compiler version");
				}
			};

			ECS_STACK_CAPACITY_STREAM(Stream<char>, version_display_names, MAX_COMPILERS);
			// Collocate the compiler path and editor IDE executable path for now.
			// Write the compiler path followed by a | as separator from the IDE path.
			ECS_STACK_CAPACITY_STREAM(Stream<void>, compiler_and_ide_paths, MAX_COMPILERS);
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
			for (size_t index = 0; index < versions.size; index++) {
				version_display_names[index] = versions[index].aggregated;
				compiler_and_ide_paths[index].Initialize(&stack_allocator, versions[index].compiler_path.CopySize() + versions[index].editing_ide_path.CopySize() + sizeof(wchar_t));
				Stream<wchar_t> reinterpretation = compiler_and_ide_paths[index].As<wchar_t>();
				reinterpretation.size = 0;
				reinterpretation.AddStream(versions[index].compiler_path);
				reinterpretation.Add(COLLOCATE_CHARACTER_SEPARATOR);
				reinterpretation.AddStream(versions[index].editing_ide_path);
			}
			version_display_names.size = versions.size;
			compiler_and_ide_paths.size = versions.size;

			ChooseElementWindowData choose_data;
			choose_data.element_labels = version_display_names;
			choose_data.description = "Select the compiler version";
			choose_data.window_name = "Select Compiler";
			choose_data.select_handler = { select_action, editor_state, 0, ECS_UI_DRAW_SYSTEM };
			choose_data.additional_data = compiler_and_ide_paths;

			CreateChooseElementWindow(system, choose_data);

			#undef COLLOCATE_CHARACTER_SEPARATOR
		}
	}
	else {
		CreateErrorMessageWindow(system, "Failed to auto detect any compiler path");
	}
}

void SortHubProjects(EditorState* editor_state)
{
	HubData* data = editor_state->hub_data;

	struct SortElement {
		ECS_INLINE bool operator < (const SortElement& other) const {
			return file_time < other.file_time;
		}

		ECS_INLINE bool operator == (const SortElement& other) const {
			return file_time == other.file_time;
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
		OS::GetRelativeFileTimes(data->projects[index].data.path, nullptr, nullptr, &sort_elements[index].file_time);
	}

	InsertionSort(sort_elements, data->projects.size);
	for (size_t index = 0; index < data->projects.size; index++) {
		data->projects[index] = sort_elements[index].project;
	}
}

void HubDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
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
	UIConfigTextParameters ecs_text = drawer.TextParameters();
	ecs_text.size *= {1.75f, 1.75f};
	ecs_text.character_spacing *= 1.75f;

	ECS_STACK_CAPACITY_STREAM(char, ecs_characters, 128);
	ecs_characters.AddStreamAssert("ECSEngine Version: ");
	ecs_characters.AddStreamAssert(VERSION_DESCRIPTION);
	config.AddFlag(ecs_text);

	drawer.Text(UI_CONFIG_TEXT_PARAMETERS, config, ecs_characters);

	UIConfigAbsoluteTransform transform;
	transform.scale = { drawer.region_limit.x - drawer.current_x, drawer.current_row_y_scale };
	transform.position = drawer.GetAlignedToRight(transform.scale.x);

	UIConfigTextAlignment alignment;
	alignment.horizontal = ECS_UI_ALIGN_RIGHT;
	alignment.vertical = ECS_UI_ALIGN_MIDDLE;
	config.AddFlag(transform);
	config.AddFlag(alignment);

	ECS_STACK_CAPACITY_STREAM(char, user_characters, 128);
	user_characters.AddStreamAssert("User: ");
	user_characters.AddStreamAssert(USER);
	drawer.TextLabel(UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_ABSOLUTE_TRANSFORM
		| UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_TEXT_PARAMETERS, config, user_characters);

	drawer.NextRow();
	drawer.CrossLine();
	drawer.Text(UI_CONFIG_TEXT_PARAMETERS, config, "Projects");

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

	const size_t BUTTON_CONFIGURATION = UI_CONFIG_ALIGN_TO_ROW_Y;

	drawer.Button(BUTTON_CONFIGURATION, config, "Create Project", { CreateProjectWizardAction, wizard_data, 0, ECS_UI_DRAW_SYSTEM });
	drawer.Button(BUTTON_CONFIGURATION, config, "Add existing Project", { AddExistingProjectAction, editor_state, 0, ECS_UI_DRAW_SYSTEM });
	drawer.Button(BUTTON_CONFIGURATION, config, "Restore existing Project", { RestoreHubProjectAction, editor_state, 0, ECS_UI_DRAW_SYSTEM });
	drawer.Button(BUTTON_CONFIGURATION, config, "Reload Projects", { ReloadHubProjectsAction, editor_state, 0 });

	drawer.Button(BUTTON_CONFIGURATION, config, "Default Project UI", { CreateProjectUITemplatePreviewAction, editor_state, 0, ECS_UI_DRAW_SYSTEM });
	drawer.Button(BUTTON_CONFIGURATION, config, "Auto Detect Compiler Path", { AutoDetectCompilerAction, editor_state, 0, ECS_UI_DRAW_SYSTEM });

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
	table_alignment.horizontal = ECS_UI_ALIGN_MIDDLE;
	table_alignment.vertical = ECS_UI_ALIGN_MIDDLE;
	table_transform.position = table_start_position;
	table_transform.scale = { FIRST_COLUMN_SCALE, COLUMN_Y_SCALE };

	UIConfigTextParameters table_text = drawer.TextParameters();
	table_text.size *= {1.3f, 1.3f};
	table_text.character_spacing *= 1.3f;

	table_config.AddFlag(table_alignment);
	table_config.AddFlag(table_text);
	table_config.AddFlag(table_transform);

	const size_t COLUMN_HEADER_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_TEXT_PARAMETERS
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

	const size_t ROW_LABEL_CONFIGURATION = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_ABSOLUTE_TRANSFORM
		| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;

	auto row_lambda = [&](unsigned int index) {
		float2 row_start_position = { table_start_position.x, drawer.GetCurrentPositionNonOffset().y };

		auto row_project_name = [&](UIDrawConfig& config, UIConfigAbsoluteTransform& transform, size_t index) {
			ECS_STACK_CAPACITY_STREAM(char, temp_characters, 256);
			ConvertWideCharsToASCII(data->projects[index].data.path, temp_characters);

			transform.position = row_start_position;
			transform.scale = { FIRST_COLUMN_SCALE, COLUMN_Y_SCALE };
			config.AddFlag(transform);

			UIConfigTextAlignment alignment;
			alignment.horizontal = ECS_UI_ALIGN_LEFT;
			alignment.vertical = ECS_UI_ALIGN_TOP;
			config.AddFlag(alignment);

			ECS_STACK_CAPACITY_STREAM(char, ascii_name, 256);
			Stream<wchar_t> path_stem = PathStem(data->projects[index].data.path);
			ConvertWideCharsToASCII(path_stem, ascii_name);
			drawer.TextLabel(ROW_LABEL_CONFIGURATION, config, ascii_name);

			alignment.vertical = ECS_UI_ALIGN_BOTTOM;
			config.flag_count--;
			config.AddFlag(alignment);

			drawer.TextLabel(ROW_LABEL_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT, config, temp_characters);

			config.flag_count -= 2;
		};

		auto row_platform_and_version = [&](UIDrawConfig& config, UIConfigAbsoluteTransform& transform, size_t index) {
			alignment.vertical = ECS_UI_ALIGN_MIDDLE;
			alignment.horizontal = ECS_UI_ALIGN_MIDDLE;
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

		if (data->projects[index].error_message.size == 0) {
			ECS_STACK_CAPACITY_STREAM(char, last_write_time, 128);
			bool succeeded = OS::GetRelativeFileTimes(data->projects[index].data.path, nullptr, nullptr, &last_write_time);

			if (succeeded) {
				UIDrawConfig row_config;
				UIConfigAbsoluteTransform row_transform;

				row_project_name(row_config, row_transform, index);
				row_platform_and_version(row_config, row_transform, index);

				row_transform.position.x += row_transform.scale.x;
				row_transform.scale.x = end_table_position - row_transform.position.x;
				row_config.AddFlag(row_transform);

				drawer.TextLabel(ROW_LABEL_CONFIGURATION, row_config, last_write_time);
				row_config.flag_count--;
				
				row_cross_line(row_config, row_transform);
			}
			else {
				UIDrawConfig row_config;
				UIConfigAbsoluteTransform row_transform;

				row_project_name(row_config, row_transform, index);
				row_platform_and_version(row_config, row_transform, index);

				row_transform.position.x += transform.scale.x;
				row_transform.scale.x = end_table_position - row_transform.position.x;
				row_config.AddFlag(row_transform);

				drawer.TextLabel(ROW_LABEL_CONFIGURATION | UI_CONFIG_UNAVAILABLE_TEXT, row_config, "Unable to read last write time");
				row_config.flag_count--;
				
				row_cross_line(row_config, row_transform);
			}

			config.flag_count = 0;
			transform.position = row_start_position;
			transform.scale = { end_table_position - row_start_position.x, COLUMN_Y_SCALE };
			config.AddFlag(transform);

			ProjectOperationData open_data;
			open_data.error_message.buffer = nullptr;
			open_data.file_data = &data->projects[index].data;
			open_data.editor_state = editor_state;
			drawer.Button(
				UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, 
				config, 
				"", 
				{ OpenProjectAction, &open_data, sizeof(open_data) }
			);
		}
		else {
			UIDrawConfig row_config;
			UIConfigAbsoluteTransform row_transform;

			row_project_name(row_config, row_transform, index);

			alignment.vertical = ECS_UI_ALIGN_MIDDLE;
			alignment.horizontal = ECS_UI_ALIGN_MIDDLE;
			row_config.AddFlag(alignment);

			row_transform.position.x += row_transform.scale.x;
			row_transform.scale.x = end_table_position - row_transform.position.x;
			row_config.AddFlag(row_transform);

			drawer.TextLabel(ROW_LABEL_CONFIGURATION, row_config, data->projects[index].error_message);
			row_config.flag_count--;
			
			row_cross_line(row_config, row_transform);
		}

		float2 cross_line_position = drawer.GetLastSolidColorRectanglePosition();
		float2 cross_line_scale = drawer.GetLastSolidColorRectangleScale();

		config.flag_count = 0;
		UIConfigAbsoluteTransform x_transform;
		x_transform.position.x = end_table_position - drawer.GetSquareScale(REMOVE_BUTTON_SCALE).x - REMOVE_BUTTON_PADDING;
		x_transform.position.y = AlignMiddle(cross_line_position.y, COLUMN_Y_SCALE, REMOVE_BUTTON_SCALE);
		x_transform.scale = drawer.GetSquareScale(REMOVE_BUTTON_SCALE);
		config.AddFlag(x_transform);

		struct RemoveProjectData {
			EditorState* editor_state;
			Stream<wchar_t> project_file;
		};

		auto remove_project_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;
			
			RemoveProjectData* data = (RemoveProjectData*)_data;
			RemoveHubProject(data->editor_state, data->project_file);
		};

		RemoveProjectData remove_data = { editor_state, data->projects[index].data.path };
		drawer.SpriteButton(UI_CONFIG_ABSOLUTE_TRANSFORM, config, { remove_project_action, &remove_data, sizeof(remove_data) }, ECS_TOOLS_UI_TEXTURE_X, drawer.color_theme.text);
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
	UISystem* ui_system = editor_state->ui_system;

	UIWindowDescriptor window_descriptor;
	window_descriptor.window_name = "Project";
	window_descriptor.draw = HubDraw;

	window_descriptor.initial_position_x = -1.0f;
	window_descriptor.initial_position_y = -1.0f;
	window_descriptor.initial_size_x = 2.0f;
	window_descriptor.initial_size_y = 2.0f;

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	bool success = LoadEditorFile(editor_state, &error_message);
	LoadHubProjectsInfo(editor_state);
	SortHubProjects(editor_state);

	window_descriptor.resource_count = 8192 * 4;
	window_descriptor.window_data = editor_state;
	window_descriptor.window_data_size = 0;
	ui_system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_BORDER_NOTHING | UI_DOCKSPACE_BACKGROUND);

	editor_state->editor_tick = EditorStateHubTick;

	// Create this error message window after the hub window was created, such that it appears over the hub
	if (!success || error_message.size > 0) {
		CreateErrorMessageWindow(ui_system, error_message);
	}
}

void HubTick(EditorState* editor_state) {}

bool ValidateProjectPath(Stream<wchar_t> path)
{
	ProjectFile temp_project_file;
	temp_project_file.path = path;

	ECS_STACK_CAPACITY_STREAM(wchar_t, project_file_path, 512);
	GetProjectFilePath(&temp_project_file, project_file_path);

	return ExistsFileOrFolder(project_file_path);
}

