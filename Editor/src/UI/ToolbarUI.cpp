#include "editorpch.h"
#include "ToolbarUI.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorParameters.h"
#include "../Project/ProjectUITemplate.h"
#include "../Project/ProjectOperations.h"
#include "../Sandbox/SandboxAccessor.h"

#if 1

#include "DirectoryExplorer.h"
#include "FileExplorer.h"
#include "MiscellaneousBar.h"
#include "ModuleExplorer.h"
#include "SandboxExplorer.h"
#include "AssetExplorer.h"
#include "Inspector.h"
#include "Backups.h"
#include "EntitiesUI.h"
#include "Game.h"
#include "Scene.h"
#include "VisualizeTexture.h"
#include "ProjectSettingsWindow.h"

#endif

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr size_t TOOLBAR_DATA_FILE_ROW_COUNT = 3;
constexpr size_t TOOLBAR_DATA_EDIT_ROW_COUNT = 4;
constexpr size_t TOOLBAR_DATA_LAYOUT_ROW_COUNT = 9;
constexpr size_t TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT = 4;

#define RETAINED_MODE_CHECK_MS 2500

// ------------------------------------------------------------------------------------------------

struct ToolbarData {
	UIActionHandler file_actions[TOOLBAR_DATA_FILE_ROW_COUNT];
	UIActionHandler window_actions[TOOLBAR_WINDOW_MENU_COUNT];
	UIActionHandler layout_actions[TOOLBAR_DATA_LAYOUT_ROW_COUNT];

	// This is needed because the game UI can have multiple sub windows
	Stream<UIActionHandler> game_ui_handlers;
	Stream<UIActionHandler> scene_ui_handlers;
	bool window_has_submenu[TOOLBAR_WINDOW_MENU_COUNT];
	bool window_submenu_unavailable[TOOLBAR_WINDOW_MENU_COUNT];

	bool layout_has_submenu[TOOLBAR_DATA_LAYOUT_ROW_COUNT];
	bool layout_submenu_unavailables[TOOLBAR_DATA_LAYOUT_ROW_COUNT * 2];
	UIDrawerMenuState* layout_submenu_states;
	UIActionHandler* layout_submenu_handlers;
	EditorState* editor_state;

	// Used to determine if a redraw is necessary
	Timer retained_timer;
};

// ------------------------------------------------------------------------------------------------

void DefaultUITemplate(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ECS_STACK_CAPACITY_STREAM(wchar_t, template_path, 256);
	template_path.CopyOther(EDITOR_DEFAULT_PROJECT_UI_TEMPLATE);
	template_path.AddStreamSafe(PROJECT_UI_TEMPLATE_EXTENSION);
	template_path[template_path.size] = L'\0';
	if (ExistsFileOrFolder(template_path)) {
		LoadProjectUITemplateData data;
		data.editor_state = (EditorState*)action_data->data;
		data.ui_template.ui_file = template_path;

		action_data->data = &data;
		LoadProjectUITemplateAction(action_data);
	}
	else {
		ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
		FormatString(error_message, "Could not find default template {#}. It has been deleted.", template_path);
		CreateErrorMessageWindow(system, error_message);
	}
}

// ------------------------------------------------------------------------------------------------

void LoadProjectUITemplateClickable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	LoadProjectUITemplateAction(action_data);
}

// ------------------------------------------------------------------------------------------------

static bool ToolbarRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	ToolbarData* data = (ToolbarData*)window_data;
	bool redraw = false;

	if (data->retained_timer.GetDurationFloat(ECS_TIMER_DURATION_MS) >= RETAINED_MODE_CHECK_MS) {
		// Check the template statuses
		for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT; index++) {
			SaveProjectUITemplateData* _template = (SaveProjectUITemplateData*)data->layout_submenu_states[index + 1].click_handlers[0].data;
			bool current_status = !ExistsFileOrFolder(_template->ui_template.ui_file);
			redraw |= current_status != data->layout_submenu_states[index + 1].unavailables[1];
			data->layout_submenu_states[index + 1].unavailables[1] = current_status;
		}
		// Update the availability status - project
		for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT - TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT - 1; index++) {
			SaveProjectUITemplateData* _template = (SaveProjectUITemplateData*)data->layout_submenu_states[index + 1 + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].click_handlers[0].data;
			bool current_status = !ExistsFileOrFolder(_template->ui_template.ui_file);
			redraw |= current_status != data->layout_submenu_states[index + 1 + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].unavailables[1];
			data->layout_submenu_states[index + 1 + TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT].unavailables[1] = current_status;
		}

		data->retained_timer.SetNewStart();
	}
	redraw |= GetSandboxCount(data->editor_state, true) != data->game_ui_handlers.size;

	return !redraw;
}

void ToolbarDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	ToolbarData* data = (ToolbarData*)window_data;
	EditorState* editor_state = data->editor_state;

	if (initialize) {
		// Default initialize all handlers with SkipAction, data size 0
		Stream<UIActionHandler> handlers(data,  TOOLBAR_DATA_FILE_ROW_COUNT + TOOLBAR_WINDOW_MENU_COUNT + TOOLBAR_DATA_LAYOUT_ROW_COUNT);
		for (size_t index = 0; index < handlers.size; index++) {
			handlers[index] = { SkipAction, nullptr, 0 };
		}

		ProjectFile* project_file = editor_state->project_file;

#pragma region File

		data->file_actions[0].action = CreateProjectAction;
		data->file_actions[1].action = OpenProjectFileAction;
		data->file_actions[2].action = SaveProjectAction;

#pragma endregion

#pragma region Windows

		// The action data is the inject data + the window name
		data->window_actions[TOOLBAR_WINDOW_MENU_CONSOLE] = { CreateConsoleAction, GetConsole(), 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER] = { CreateDirectoryExplorerAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_FILE_EXPLORER] = { CreateFileExplorerAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_MODULE_EXPLORER] = { CreateModuleExplorerAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_SANDBOX_EXPLORER] = { CreateSandboxExplorerAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_ASSET_EXPLORER] = { CreateAssetExplorerAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_INSPECTOR] = { CreateInspectorAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_BACKUPS] = { CreateBackupsWindowAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_ENTITIES_UI] = { CreateEntitiesUIAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_VISUALIZE_TEXTURE] = { CreateVisualizeTextureUIWindowAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->window_actions[TOOLBAR_WINDOW_MENU_PROJECT_SETTINGS] = { CreateProjectSettingsWindowAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };

		data->game_ui_handlers.size = 0;
		
		// The sandbox UI needs to be handled separately
		for (size_t index = 0; index < TOOLBAR_WINDOW_MENU_COUNT; index++) {
			data->window_has_submenu[index] = false;
			data->window_submenu_unavailable[index] = false;
		}

		data->window_has_submenu[TOOLBAR_WINDOW_MENU_GAME_UI] = true;
		data->window_has_submenu[TOOLBAR_WINDOW_MENU_SCENE_UI] = true;

		data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_GAME_UI] = true;
		data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_SCENE_UI] = true;

#pragma endregion

#pragma region Layout

		size_t layout_handler_data_size = (sizeof(LoadProjectUITemplateData) + sizeof(SaveProjectUITemplateData)) * (TOOLBAR_DATA_LAYOUT_ROW_COUNT - 1);
		void* allocation = drawer.GetMainAllocatorBuffer(layout_handler_data_size);

		LoadProjectUITemplateData* layout_load_data = (LoadProjectUITemplateData*)allocation;
		SaveProjectUITemplateData* layout_save_data = (SaveProjectUITemplateData*)OffsetPointer(
			allocation, sizeof(LoadProjectUITemplateData) * (TOOLBAR_DATA_LAYOUT_ROW_COUNT - 1)
		);
		ECS_STACK_CAPACITY_STREAM(wchar_t, system_string, 256);
		system_string.CopyOther(EDITOR_SYSTEM_PROJECT_UI_TEMPLATE_PREFIX);
		system_string.AddAssert(L' ');

		size_t prefix_size = system_string.size;
		system_string.size++;
		system_string.AddStreamAssert(PROJECT_UI_TEMPLATE_EXTENSION);

		size_t string_total_size = system_string.size * TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT * sizeof(wchar_t);

		ECS_STACK_CAPACITY_STREAM(wchar_t, project_string, 512);
		project_string.CopyOther(project_file->path);
		project_string.AddAssert(ECS_OS_PATH_SEPARATOR);

		project_string.AddStreamAssert(L"UI");
		project_string.AddAssert(ECS_OS_PATH_SEPARATOR);
		size_t project_number_index = project_string.size;
		project_string.size++;
		project_string.AddStreamAssert(PROJECT_UI_TEMPLATE_EXTENSION);

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
		data->layout_actions[0] = { DefaultUITemplate, data->editor_state, 0, ECS_UI_DRAW_SYSTEM };

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
			data->layout_submenu_handlers[index * 2] = { SavProjectUITemplateClickable, layout_save_data + index, 0, ECS_UI_DRAW_SYSTEM };
			data->layout_submenu_handlers[index * 2 + 1] = { LoadProjectUITemplateClickable, layout_load_data + index, 0, ECS_UI_DRAW_SYSTEM };
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
			data->layout_submenu_states[index + 1].right_characters = { nullptr, 0 };
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
			data->layout_submenu_states[submenu_index].right_characters = { nullptr, 0 };
			data->layout_submenu_states[submenu_index].row_has_submenu = nullptr;
			data->layout_submenu_states[submenu_index].separation_line_count = 0;
			data->layout_submenu_states[submenu_index].submenues = nullptr;
			data->layout_submenu_states[submenu_index].unavailables = data->layout_submenu_unavailables + index * 2 + offset;
		}

#pragma endregion

	}

	// If the number of sandboxes has changed, modify the buffer
	size_t sandbox_count = GetSandboxCount(editor_state, true);
	if (sandbox_count != data->game_ui_handlers.size) {
		if (data->game_ui_handlers.size != 0) {
			// Deallocate if valid
			drawer.RemoveAllocation(data->game_ui_handlers.buffer);
		}

		void* allocation = drawer.GetMainAllocatorBuffer((sizeof(UIActionHandler) * 2 + sizeof(CreateGameUIActionData) + sizeof(CreateSceneUIWindowActionData)) 
			* GetSandboxCount(editor_state));
		data->game_ui_handlers.buffer = (UIActionHandler*)allocation;
		allocation = OffsetPointer(allocation, sizeof(UIActionHandler) * sandbox_count);

		data->scene_ui_handlers.buffer = (UIActionHandler*)allocation;
		allocation = OffsetPointer(allocation, sizeof(UIActionHandler) * sandbox_count);

		// Now initialize the action data
		CreateGameUIActionData* game_action_data = (CreateGameUIActionData*)allocation;
		allocation = OffsetPointer(allocation, sizeof(CreateGameUIActionData) * sandbox_count);
		CreateSceneUIWindowActionData* scene_action_data = (CreateSceneUIWindowActionData*)allocation;

		for (unsigned int index = 0; index < sandbox_count; index++) {
			game_action_data[index] = { editor_state, index };
			scene_action_data[index] = { editor_state, index };
			data->game_ui_handlers[index] = { CreateGameUIWindowAction, game_action_data + index, 0, ECS_UI_DRAW_SYSTEM };
			data->scene_ui_handlers[index] = { CreateSceneUIWindowAction, scene_action_data + index, 0, ECS_UI_DRAW_SYSTEM };
		}
		data->game_ui_handlers.size = sandbox_count;
		data->scene_ui_handlers.size = sandbox_count;

		if (data->game_ui_handlers.size == 0) {
			data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_GAME_UI] = true;
			data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_SCENE_UI] = true;
		}
	}

	drawer.SetNextRowYOffset(0.0f);
	drawer.SetRowPadding(0.0f);
	drawer.DisablePaddingForRenderRegion();
	drawer.DisablePaddingForRenderSliders();
	drawer.DisableZoom();
	drawer.SetDrawMode(ECS_UI_DRAWER_NOTHING);

	drawer.SetCurrentX(drawer.GetNextRowXPosition());
	drawer.SetCurrentY(-1.0f);
	constexpr size_t configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_BORDER | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X;

	UIDrawConfig config;
	UIConfigWindowDependentSize transform;
	transform.type = ECS_UI_WINDOW_DEPENDENT_VERTICAL;
	transform.scale_factor.y = 1.0f;

	UIConfigBorder border;
	border.color = drawer.color_theme.borders;

	drawer.SolidColorRectangle(0, drawer.region_position, drawer.region_scale, drawer.color_theme.theme);

	config.AddFlag(transform);
	config.AddFlag(border);

	UIDrawerMenuState current_state;
	current_state.left_characters = (char*)"New project\nOpen project\nSave project";
	current_state.right_characters = (char*)"Ctrl+N\nCtrl+O\nCtrl+S";
	current_state.click_handlers = data->file_actions;
	current_state.row_count = TOOLBAR_DATA_FILE_ROW_COUNT;
	current_state.submenu_index = 0;

	drawer.Menu(configuration | UI_CONFIG_DO_CACHE, config, "File", &current_state);

	current_state.right_characters = { nullptr, 0 };
	current_state.separation_lines[0] = TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER - 1;
	current_state.separation_lines[1] = TOOLBAR_WINDOW_MENU_ASSET_EXPLORER;
	current_state.separation_line_count = 2;
	current_state.left_characters = (char*)TOOLBAR_WINDOWS_MENU_CHAR_DESCRIPTION;
	current_state.click_handlers = data->window_actions;
	current_state.row_count = TOOLBAR_WINDOW_MENU_COUNT;

	UIDrawerMenuState toolbar_ui_state[TOOLBAR_WINDOW_MENU_COUNT];

	current_state.row_has_submenu = data->window_has_submenu;
	ECS_STACK_CAPACITY_STREAM(char, game_ui_characters, 512);
	ECS_STACK_CAPACITY_STREAM(char, scene_ui_characters, 512);

	current_state.unavailables = data->window_submenu_unavailable;
	current_state.submenues = toolbar_ui_state;

	if (data->game_ui_handlers.size > 0) {
		data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_GAME_UI] = false;
		data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_SCENE_UI] = false;
		
		ECS_STACK_CAPACITY_STREAM(char, current_window_name, 64);

		for (unsigned int index = 0; index < data->game_ui_handlers.size; index++) {
			GetGameUIWindowName(index, current_window_name);
			game_ui_characters.AddStreamSafe(current_window_name);
			game_ui_characters.Add('\n');

			current_window_name.size = 0;
			GetSceneUIWindowName(index, current_window_name);
			scene_ui_characters.AddStreamSafe(current_window_name);
			scene_ui_characters.Add('\n');
		}
		// Remove the last '\n' and replace it with '\0'
		game_ui_characters.size--;
		game_ui_characters[game_ui_characters.size] = '\0';

		scene_ui_characters.size--;
		scene_ui_characters[scene_ui_characters.size] = '\0';
		
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_GAME_UI].left_characters = game_ui_characters.buffer;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_GAME_UI].row_count = data->game_ui_handlers.size;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_GAME_UI].click_handlers = data->game_ui_handlers.buffer;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_GAME_UI].row_has_submenu = nullptr;

		toolbar_ui_state[TOOLBAR_WINDOW_MENU_SCENE_UI].left_characters = scene_ui_characters.buffer;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_SCENE_UI].row_count = data->scene_ui_handlers.size;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_SCENE_UI].click_handlers = data->scene_ui_handlers.buffer;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_SCENE_UI].row_has_submenu = nullptr;
	}
	else {
		data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_GAME_UI] = true;
		data->window_submenu_unavailable[TOOLBAR_WINDOW_MENU_SCENE_UI] = true;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_GAME_UI].row_count = 0;
		toolbar_ui_state[TOOLBAR_WINDOW_MENU_SCENE_UI].row_count = 0;
	}

	drawer.Menu(configuration, config, "Window", &current_state);

	ECS_STACK_CAPACITY_STREAM(char, layout_state_stream, 512);
	layout_state_stream.CopyOther("Default");

	Stream<char> layout_system_string = "System ";
	for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT; index++) {
		layout_state_stream.AddAssert('\n');
		layout_state_stream.AddStreamAssert(layout_system_string);
		layout_state_stream.AddAssert(index + '1');
	}
	Stream<char> layout_project_string = "Project ";
	for (size_t index = 0; index < TOOLBAR_DATA_LAYOUT_ROW_COUNT - TOOLBAR_DATA_LAYOUT_SYSTEM_COUNT - 1; index++) {
		layout_state_stream.AddAssert('\n');
		layout_state_stream.AddStreamAssert(layout_project_string);
		layout_state_stream.AddAssert(index + '1');
	}
	layout_state_stream.AddAssert('\0');

	current_state.left_characters = layout_state_stream.buffer;
	current_state.separation_lines[0] = 0;
	current_state.separation_lines[1] = 4;
	current_state.separation_line_count = 2;
	current_state.row_count = TOOLBAR_DATA_LAYOUT_ROW_COUNT;
	current_state.click_handlers = data->layout_actions;
	current_state.row_has_submenu = data->layout_has_submenu;
	current_state.submenues = data->layout_submenu_states;

	drawer.Menu(configuration | UI_CONFIG_DO_CACHE, config, "Layout", &current_state);

	UIConfigRelativeTransform logo_transform;
	logo_transform.scale.x = 0.7f;
	logo_transform.scale.y = drawer.GetRelativeTransformFactors(drawer.GetRegionScale()).y;
	float middle_position = drawer.GetAlignedToCenterX(logo_transform.scale.x * drawer.layout.default_element_x);
	logo_transform.offset.x = middle_position - drawer.current_x;
	config.flag_count = 0;
	config.AddFlag(logo_transform);
	config.AddFlag(border);
	drawer.SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, config, ECS_TOOLS_UI_TEXTURE_ECS_LOGO);
	drawer.SolidColorRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_BORDER, config, drawer.color_theme.theme);

	UIConfigAbsoluteTransform project_transform;
	const ProjectFile* project_file = (const ProjectFile*)data->editor_state->project_file;
	ECS_STACK_CAPACITY_STREAM(char, project_name, 256);
	ConvertWideCharsToASCII(project_file->project_name, project_name);
	project_transform.scale.x = drawer.GetLabelScale(project_name).x;
	project_transform.scale.y = drawer.GetRegionScale().y;
	project_transform.position.x = drawer.GetAlignedToRightOverLimit(project_transform.scale.x).x;
	project_transform.position.y = drawer.current_y;
	config.AddFlag(project_transform);
	drawer.TextLabel(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_BORDER | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, config, project_name);
}

// ------------------------------------------------------------------------------------------------

void ToolbarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
{
	descriptor.draw = ToolbarDraw;
	descriptor.retained_mode = ToolbarRetainedMode;

	ToolbarData* data = stack_memory->Reserve<ToolbarData>();
	data->editor_state = editor_state;
	data->retained_timer.SetUninitialized();
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
	descriptor.window_name = TOOLBAR_WINDOW_NAME;
}

// ------------------------------------------------------------------------------------------------

void CreateToolbarUI(EditorState* editor_state) {
	UIWindowDescriptor toolbar_window;
	toolbar_window.initial_position_x = -1.0f;
	toolbar_window.initial_position_y = -1.0f;
	toolbar_window.initial_size_x = 2.0f;
	toolbar_window.initial_size_y = TOOLBAR_SIZE_Y;

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB);
	ToolbarSetDescriptor(toolbar_window, editor_state, &stack_memory);

	editor_state->ui_system->CreateWindowAndDockspace(toolbar_window, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_NO_DOCKING 
		| UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X | UI_DOCKSPACE_BORDER_FLAG_NO_TITLE);
}

// ------------------------------------------------------------------------------------------------