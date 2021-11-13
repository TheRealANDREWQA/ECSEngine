#include "ToolbarUI.h"
#include "..\Project\ProjectOperations.h"
#include "..\Editor\EditorState.h"
#include "..\Editor\EditorParameters.h"
#include "..\Project\ProjectUITemplate.h"

#if 1

#include "DirectoryExplorer.h"
#include "FileExplorer.h"
#include "MiscellaneousBar.h"
#include "Game.h"
#include "ModuleExplorer.h"
#include "Inspector.h"

#endif

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr size_t TOOLBAR_DATA_FILE_ROW_COUNT = 3;
constexpr size_t TOOLBAR_DATA_EDIT_ROW_COUNT = 4;
constexpr size_t TOOLBAR_DATA_LAYOUT_ROW_COUNT = 9;
constexpr size_t TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT = 4;

constexpr wchar_t* PATH = L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\Editor\\TEMPLATE.uit";

struct ToolbarData {
	UIActionHandler file_actions[TOOLBAR_DATA_FILE_ROW_COUNT];
	UIActionHandler window_actions[TOOLBAR_WINDOW_MENU_COUNT];
	UIActionHandler layout_actions[TOOLBAR_DATA_LAYOUT_ROW_COUNT];
	bool layout_has_submenu[TOOLBAR_DATA_LAYOUT_ROW_COUNT];
	bool layout_submenu_unavailables[TOOLBAR_DATA_LAYOUT_ROW_COUNT * 2];
	UIDrawerMenuState* layout_submenu_states;
	UIActionHandler* layout_submenu_handlers;
	EditorState* editor_state;
};

void DefaultUITemplate(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ECS_TEMP_STRING(template_path, 256);
	template_path.Copy(ToStream(EDITOR_DEFAULT_PROJECT_UI_TEMPLATE));
	template_path.AddStreamSafe(ToStream(PROJECT_UI_TEMPLATE_EXTENSION));
	template_path[template_path.size] = L'\0';
	if (ExistsFileOrFolder(template_path)) {
		LoadProjectUITemplateData data;
		data.editor_state = (EditorState*)action_data->data;
		data.ui_template.ui_file = template_path;

		action_data->data = &data;
		LoadProjectUITemplateAction(action_data);
	}
	else {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "Could not find default template {0}. It has been deleted.", template_path);
		error_message.AssertCapacity();
		CreateErrorMessageWindow(system, error_message);
	}
}

void LoadProjectUITemplateClickable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	LoadProjectUITemplateAction(action_data);
}

template<bool initialize>
void ToolbarDraw(void* window_data, void* drawer_descriptor) {
	UI_PREPARE_DRAWER(initialize);

	ToolbarData* data = (ToolbarData*)window_data;
	if constexpr (initialize) {
		// Default initialize all handlers with SkipAction, data size 0
		Stream<UIActionHandler> handlers(data,  TOOLBAR_DATA_FILE_ROW_COUNT + TOOLBAR_WINDOW_MENU_COUNT + TOOLBAR_DATA_LAYOUT_ROW_COUNT);
		for (size_t index = 0; index < handlers.size; index++) {
			handlers[index] = { SkipAction, nullptr, 0 };
		}

		EDITOR_STATE(data->editor_state);
		EditorState* editor_state = data->editor_state;
		ProjectFile* project_file = (ProjectFile*)editor_state->project_file;

#pragma region File

		data->file_actions[0].action = CreateProjectAction;
		data->file_actions[1].action = OpenProjectFileAction;
		data->file_actions[2].action = SaveProjectAction;

#pragma endregion

#pragma region Windows

		// The action data is the inject data + the window name
		data->window_actions[TOOLBAR_WINDOW_MENU_INJECT_WINDOW] = { CreateInjectValuesAction, &editor_state->inject_data, 0, UIDrawPhase::System };
		data->window_actions[TOOLBAR_WINDOW_MENU_GAME] = { CreateGameAction, editor_state, 0, UIDrawPhase::System };
		data->window_actions[TOOLBAR_WINDOW_MENU_CONSOLE] = { CreateConsoleAction, editor_state->console, 0, UIDrawPhase::System };
		data->window_actions[TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER] = { CreateDirectoryExplorerAction, editor_state, 0, UIDrawPhase::System };
		data->window_actions[TOOLBAR_WINDOW_MENU_FILE_EXPLORER] = { CreateFileExplorerAction, editor_state, 0, UIDrawPhase::System };
		data->window_actions[TOOLBAR_WINDOW_MENU_MODULE_EXPLORER] = { CreateModuleExplorerAction, editor_state, 0, UIDrawPhase::System };
		data->window_actions[TOOLBAR_WINDOW_MENU_INSPECTOR] = { CreateInspectorAction, editor_state, 0, UIDrawPhase::System };

#pragma endregion

#pragma region Layout

		size_t layout_handler_data_size = (sizeof(LoadProjectUITemplateData) + sizeof(SaveProjectUITemplateData)) * (TOOLBAR_DATA_LAYOUT_ROW_COUNT - 1);
		void* allocation = drawer.GetMainAllocatorBuffer(layout_handler_data_size);

		LoadProjectUITemplateData* layout_load_data = (LoadProjectUITemplateData*)allocation;
		SaveProjectUITemplateData* layout_save_data = (SaveProjectUITemplateData*)function::OffsetPointer(
			allocation, sizeof(LoadProjectUITemplateData) * (TOOLBAR_DATA_LAYOUT_ROW_COUNT - 1)
		);
		ECS_TEMP_STRING(system_string, 256);
		system_string.Copy(ToStream(EDITOR_SYSTEM_PROJECT_UI_TEMPLATE_PREFIX));
		system_string.Add(L' ');

		size_t prefix_size = system_string.size;
		system_string.size++;
		system_string.AddStreamSafe(ToStream(PROJECT_UI_TEMPLATE_EXTENSION));

		size_t string_total_size = system_string.size * TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT * sizeof(wchar_t);

		wchar_t __temp_project_memory[256];
		CapacityStream<wchar_t> project_string(__temp_project_memory, 0, 256);
		project_string.Copy(project_file->path);
		project_string.Add(ECS_OS_PATH_SEPARATOR);

		project_string.AddStream(ToStream(L"UI"));
		project_string.Add(ECS_OS_PATH_SEPARATOR);
		size_t project_number_index = project_string.size;
		project_string.size++;
		project_string.AddStreamSafe(ToStream(PROJECT_UI_TEMPLATE_EXTENSION));

		string_total_size += project_string.size * (TOOLBAR_DATA_LAYOUT_ROW_COUNT - TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT - 1) * sizeof(wchar_t);
		void* string_allocation = drawer.GetMainAllocatorBuffer(string_total_size);
		uintptr_t string_ptr = (uintptr_t)string_allocation;
		
		for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT; index++) {
			system_string[prefix_size] = index + L'1';
			layout_load_data[index].editor_state = data->editor_state;
			system_string.CopyTo((void*)string_ptr);
			layout_load_data[index].ui_template.ui_file.InitializeFromBuffer(string_ptr, system_string.size, system_string.size);
			layout_save_data[index].ui_template.ui_file = layout_load_data[index].ui_template.ui_file;
		}

		for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT - TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT - 1; index++) {
			project_string[project_number_index] = index + L'1';
			layout_load_data[index + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].editor_state = data->editor_state;
			project_string.CopyTo((void*)string_ptr);
			layout_load_data[index + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].ui_template.ui_file.InitializeFromBuffer(string_ptr, project_string.size, project_string.size);
			layout_save_data[index + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].ui_template.ui_file = layout_load_data[index + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].ui_template.ui_file;
		}

		// default template
		data->layout_actions[0] = { DefaultUITemplate, data->editor_state, 0, UIDrawPhase::System };

		size_t layout_memory_size = sizeof(UIDrawerMenuState) * TOOLBAR_DATA_LAYOUT_ROW_COUNT + sizeof(UIActionHandler) * TOOLBAR_DATA_LAYOUT_ROW_COUNT * 2;
		void* layout_submenu_allocation = drawer.GetMainAllocatorBuffer(layout_memory_size);

		uintptr_t layout_ptr = (uintptr_t)layout_submenu_allocation;
		data->layout_submenu_states = (UIDrawerMenuState*)layout_ptr;
		layout_ptr += sizeof(UIDrawerMenuState) * TOOLBAR_DATA_LAYOUT_ROW_COUNT;
		data->layout_submenu_handlers = (UIActionHandler*)layout_ptr;
		layout_ptr += sizeof(UIActionHandler) * TOOLBAR_DATA_LAYOUT_ROW_COUNT * 2;

		auto SavProjectUITemplateClickable = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SaveProjectUITemplateAction(action_data);
		};

		auto LoadProjectUITemplateClickable = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			LoadProjectUITemplateAction(action_data);
		};

		for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT; index++) {
			data->layout_submenu_handlers[index * 2] = { SavProjectUITemplateClickable, layout_save_data + index, 0, UIDrawPhase::System };
			data->layout_submenu_handlers[index * 2 + 1] = { LoadProjectUITemplateClickable, layout_load_data + index, 0, UIDrawPhase::System };
		}

		data->layout_has_submenu[0] = false;
		for (size_t index = 1; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT; index++) {
			data->layout_has_submenu[index] = true;
		}

		for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT; index++) {
			data->layout_submenu_unavailables[index * 2] = false;
			data->layout_submenu_states[index + 1].click_handlers = data->layout_submenu_handlers + index * 2;
			data->layout_submenu_states[index + 1].left_characters = "Save\nLoad";
			data->layout_submenu_states[index + 1].row_count = 2;
			data->layout_submenu_states[index + 1].submenu_index = 1;
			data->layout_submenu_states[index + 1].right_characters = nullptr;
			data->layout_submenu_states[index + 1].row_has_submenu = nullptr;
			data->layout_submenu_states[index + 1].separation_line_count = 0;
			data->layout_submenu_states[index + 1].submenues = nullptr;
			data->layout_submenu_states[index + 1].unavailables = data->layout_submenu_unavailables + index * 2;
		}

		for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT - TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT - 1; index++) {
			unsigned int offset = TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT * 2;
			unsigned int submenu_index = index + 1 + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT;
			data->layout_submenu_unavailables[index * 2 + offset] = false;
			data->layout_submenu_states[submenu_index].click_handlers = data->layout_submenu_handlers + index * 2 + offset;
			data->layout_submenu_states[submenu_index].left_characters = "Save\nLoad";
			data->layout_submenu_states[submenu_index].row_count = 2;
			data->layout_submenu_states[submenu_index].submenu_index = 1;
			data->layout_submenu_states[submenu_index].right_characters = nullptr;
			data->layout_submenu_states[submenu_index].row_has_submenu = nullptr;
			data->layout_submenu_states[submenu_index].separation_line_count = 0;
			data->layout_submenu_states[submenu_index].submenues = nullptr;
			data->layout_submenu_states[submenu_index].unavailables = data->layout_submenu_unavailables + index * 2 + offset;
		}

#pragma endregion

	}

	drawer.SetNextRowYOffset(0.0f);
	drawer.SetRowPadding(0.0f);
	drawer.DisablePaddingForRenderRegion();
	drawer.DisablePaddingForRenderSliders();
	drawer.DisableZoom();
	drawer.SetDrawMode(UIDrawerMode::Nothing);

	UILayoutDescriptor* layout = drawer.GetLayoutDescriptor();
	drawer.SetCurrentX(drawer.GetNextRowXPosition());
	drawer.SetCurrentY(-1.0f);
	constexpr size_t configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_BORDER;

	UIDrawConfig config;
	UIConfigWindowDependentSize transform;
	transform.type = WindowSizeTransformType::Vertical;
	transform.scale_factor.y = 1.0f;

	UIConfigBorder border;
	border.color = drawer.color_theme.borders;

	drawer.SolidColorRectangle<0>(drawer.region_position, drawer.region_scale, drawer.color_theme.theme);

	config.AddFlag(transform);
	config.AddFlag(border);

	UIDrawerMenuState current_state;
	current_state.left_characters = (char*)"New project\nOpen project\nSave project";
	current_state.right_characters = (char*)"Ctrl+N\nCtrl+O\nCtrl+S";
	current_state.click_handlers = data->file_actions;
	current_state.row_count = TOOLBAR_DATA_FILE_ROW_COUNT;
	current_state.submenu_index = 0;

	drawer.Menu<configuration>(config, "File", &current_state);

	current_state.right_characters = nullptr;
	current_state.separation_lines[0] = 1;
	current_state.separation_lines[1] = 4;
	current_state.separation_line_count = 2;
	current_state.left_characters = (char*)TOOLBAR_WINDOWS_MENU_CHAR_DESCRIPTION;
	current_state.click_handlers = data->window_actions;
	current_state.row_count = TOOLBAR_WINDOW_MENU_COUNT;

	drawer.Menu<configuration>(config, "Window", &current_state);

	char layout_state_characters[256];
	CapacityStream<char> layout_state_stream(layout_state_characters, 0, 256);
	layout_state_stream.Copy(ToStream("Default"));

	Stream<char> layout_system_string = ToStream("System ");
	for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT; index++) {
		layout_state_stream.Add('\n');
		layout_state_stream.AddStream(layout_system_string);
		layout_state_stream.Add(index + '1');
	}
	Stream<char> layout_project_string = ToStream("Project ");
	for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT - TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT - 1; index++) {
		layout_state_stream.Add('\n');
		layout_state_stream.AddStream(layout_project_string);
		layout_state_stream.Add(index + '1');
	}
	layout_state_stream.AddSafe('\0');

	current_state.left_characters = layout_state_characters;
	current_state.separation_lines[0] = 0;
	current_state.separation_lines[1] = 4;
	current_state.separation_line_count = 2;
	current_state.row_count = TOOLBAR_DATA_LAYOUT_ROW_COUNT;
	current_state.click_handlers = data->layout_actions;
	current_state.row_has_submenu = data->layout_has_submenu;
	current_state.submenues = data->layout_submenu_states;

	// Update the availability status - system
	for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT; index++) {
		SaveProjectUITemplateData* _template = (SaveProjectUITemplateData*)data->layout_submenu_states[index + 1].click_handlers[0].data;
		data->layout_submenu_states[index + 1].unavailables[1] = !ExistsFileOrFolder(_template->ui_template.ui_file);
	}
	// Update the availability status - project
	for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT - TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT - 1; index++) {
		SaveProjectUITemplateData* _template = (SaveProjectUITemplateData*)data->layout_submenu_states[index + 1 + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].click_handlers[0].data;
		data->layout_submenu_states[index + 1 + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].unavailables[1] = !ExistsFileOrFolder(_template->ui_template.ui_file);
	}

	drawer.Menu<configuration>(config, "Layout", &current_state);

	UIConfigRelativeTransform logo_transform;
	logo_transform.scale.x = 0.7f;
	float middle_position = drawer.GetAlignedToCenterX(logo_transform.scale.x * drawer.layout.default_element_x);
	logo_transform.offset.x = middle_position - drawer.current_x;
	config.flag_count = 0;
	config.AddFlag(logo_transform);
	config.AddFlag(border);
	drawer.SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(config, ECS_TOOLS_UI_TEXTURE_ECS_LOGO);
	drawer.SolidColorRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_BORDER>(config, drawer.color_theme.theme);

	UIConfigAbsoluteTransform project_transform;
	const ProjectFile* project_file = (const ProjectFile*)data->editor_state->project_file;
	ECS_TEMP_ASCII_STRING(project_name, 256);
	function::ConvertWideCharsToASCII(project_file->project_name, project_name);
	project_name[project_file->project_name.size] = '\0';
	project_transform.scale = drawer.GetLabelScale(project_name.buffer);
	project_transform.position.x = drawer.GetAlignedToRightOverLimit(project_transform.scale.x).x;
	project_transform.position.y = drawer.current_y;
	config.AddFlag(project_transform);
	drawer.TextLabel<UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_BORDER>(config, project_name.buffer);
}

void ToolbarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	descriptor.draw = ToolbarDraw<false>;
	descriptor.initialize = ToolbarDraw<true>;

	ToolbarData* data = (ToolbarData*)stack_memory;
	data->editor_state = editor_state;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
	descriptor.window_name = TOOLBAR_WINDOW_NAME;
}

void CreateToolbarUI(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor toolbar_window;
	toolbar_window.initial_position_x = -1.0f;
	toolbar_window.initial_position_y = -1.0f;
	toolbar_window.initial_size_x = 2.0f;
	toolbar_window.initial_size_y = TOOLBAR_SIZE_Y;

	size_t stack_memory[128];
	ToolbarSetDescriptor(toolbar_window, editor_state, stack_memory);

	ui_system->CreateWindowAndDockspace(toolbar_window, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_NO_DOCKING 
		| UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X | UI_DOCKSPACE_BORDER_FLAG_NO_TITLE);
}