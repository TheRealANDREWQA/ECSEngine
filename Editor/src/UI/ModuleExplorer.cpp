#include "ModuleExplorer.h"
#include "..\Editor\EditorState.h"
#include "..\HelperWindows.h"
#include "..\Modules\Module.h"
#include "..\Editor\EditorPalette.h"
#include "..\OSFunctions.h"
#include "..\Modules\ModuleFile.h"
#include "Inspector.h"
#include "..\Editor\EditorEvent.h"
#include "..\Project\ProjectOperations.h"

using namespace ECSEngine;
ECS_CONTAINERS;
ECS_TOOLS;

#define MODULE_EXPLORER_DATA_NAME "Module Explorer Data"

constexpr const char* HEADER_ADD_MODULE = "Add";
constexpr const char* HEADER_REMOVE_MODULE = "Remove";
constexpr const char* HEADER_BUILD_MODULE = "Build";
constexpr const char* HEADER_CLEAN_MODULE = "Clean";
constexpr const char* HEADER_REBUILD_MODULE = "Rebuild";

constexpr const char* HEADER_OPEN_MODULE_FOLDER = "Open Module Folder";

constexpr const char* HEADER_OPEN_BUILD_LOG = "Open Log";
constexpr const char* HEADER_BUILD_ALL = "Build All";
constexpr const char* HEADER_CLEAN_ALL = "Clean All";
constexpr const char* HEADER_REBUILD_ALL = "Rebuild All";

constexpr const char* HEADER_RESET_ALL = "Reset";
constexpr float HEADER_ALL_OFFSET = 0.01f;

constexpr const char* BUILD_LOG_WINDOW_NAME = "Build Log";
constexpr size_t LAZY_SOLUTION_LAST_WRITE_TARGET_MILLISECONDS = 500;

#define MODULE_DEBUG_COLOR Color(155, 155, 155)
#define MODULE_RELEASE_COLOR DarkenColor(Color(255, 244, 125), 0.8f)
#define MODULE_DISTRIBUTION_COLOR Color(128, 237, 153)

const Color MODULE_COLORS[] = {
	MODULE_DEBUG_COLOR,
	MODULE_RELEASE_COLOR,
	MODULE_DISTRIBUTION_COLOR
};

struct ModuleExplorerData {
	EditorState* editor_state;
	unsigned int selected_module;
	bool display_locked_file_warning;
	Timer lazy_solution_last_write_timer;
};

template<bool initialize>
void AddModuleWizardDraw(void* window_data, void* drawer_descriptor) {
	UI_PREPARE_DRAWER(initialize);

	constexpr float INPUT_X_FACTOR = 7.0f;
	constexpr const char* SOLUTION_PATH_NAME = "Solution Path";
	constexpr const char* SOLUTION_PATH_BUFFER_NAME = "Solution Path buffer";
	constexpr const char* LIBRARY_NAME = "Library Name";
	constexpr const char* LIBRARY_BUFFER_NAME = "Library Name buffer";
	constexpr const char* SOLUTION_PATH_WIDE_NAME = "Solution Path Wide";
	constexpr const char* FOLDER_DATA_NAME = "Folder data";
	constexpr const char* ADD_DATA_NAME = "Add data";

	EditorState* editor_state = (EditorState*)window_data;

	CapacityStream<char>* solution_path = nullptr;
	CapacityStream<char>* library_name = nullptr;
	CapacityStream<wchar_t>* solution_path_wide = nullptr;

	struct FolderActionData {
		OSFileExplorerGetFileActionData os_data;
		UIDrawerTextInput* library_input;
		bool is_data_valid;
	};

	struct AddData {
		FolderActionData* folder_data;
		EditorState* editor_state;
		CapacityStream<wchar_t>* solution_path_wide;
		CapacityStream<char>* library_name;
		bool graphics_module;
	};

	FolderActionData* folder_data = nullptr;
	AddData* add_data = nullptr;

	if constexpr (initialize) {
		solution_path = (CapacityStream<char>*)function::CoallesceCapacityStreamWithData(
			drawer.GetMainAllocatorBufferAndStoreAsResource(SOLUTION_PATH_BUFFER_NAME, sizeof(CapacityStream<char>) + sizeof(char) * 256),
			0, 
			256
		);
		library_name = (CapacityStream<char>*)function::CoallesceCapacityStreamWithData(
			drawer.GetMainAllocatorBufferAndStoreAsResource((LIBRARY_BUFFER_NAME), sizeof(CapacityStream<char>) + sizeof(char) * 64),
			0, 
			64
		);
		solution_path_wide = (CapacityStream<wchar_t>*)function::CoallesceCapacityStreamWithData(
			drawer.GetMainAllocatorBufferAndStoreAsResource(SOLUTION_PATH_WIDE_NAME, sizeof(CapacityStream<wchar_t>) + sizeof(wchar_t) * 256),
			0,
			256
		);
		folder_data = (FolderActionData*)drawer.GetMainAllocatorBufferAndStoreAsResource(FOLDER_DATA_NAME, sizeof(FolderActionData));
		add_data = (AddData*)drawer.GetMainAllocatorBufferAndStoreAsResource(ADD_DATA_NAME, sizeof(FolderActionData));
		add_data->editor_state = editor_state;
		add_data->library_name = library_name;
		add_data->solution_path_wide = solution_path_wide;
		add_data->folder_data = folder_data;
		add_data->graphics_module = false;

		folder_data->os_data.get_file_data.path = *solution_path_wide;
		folder_data->os_data.get_file_data.filter = L".sln\0.vcxproj";
		folder_data->os_data.get_file_data.filter_count = 2;
		folder_data->os_data.get_file_data.error_message.buffer = nullptr;
		folder_data->os_data.get_file_data.initial_directory = nullptr;
		folder_data->os_data.get_file_data.hWnd = nullptr;
		folder_data->is_data_valid = false;
		folder_data->os_data.update_stream = nullptr;
	}
	else {
		solution_path = (CapacityStream<char>*)drawer.GetResource(SOLUTION_PATH_BUFFER_NAME);
		library_name = (CapacityStream<char>*)drawer.GetResource(LIBRARY_BUFFER_NAME);
		solution_path_wide = (CapacityStream<wchar_t>*)drawer.GetResource(SOLUTION_PATH_WIDE_NAME);
		folder_data = (FolderActionData*)drawer.GetResource(FOLDER_DATA_NAME);
		add_data = (AddData*)drawer.GetResource(ADD_DATA_NAME);
	}

	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	transform.scale.x *= INPUT_X_FACTOR;
	config.AddFlag(transform);

	UIConfigTextInputCallback name_callback;
	UIConvertASCIIToWideStreamData convert_data;
	convert_data.ascii = solution_path->buffer;
	convert_data.wide = solution_path_wide;
	name_callback.handler = { ConvertASCIIToWideStreamAction, &convert_data, sizeof(convert_data) };
	config.AddFlag(name_callback);

	UIDrawerTextInput* solution_input = drawer.TextInput<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_TEXT_INPUT_CALLBACK>(config, SOLUTION_PATH_NAME, solution_path);
	folder_data->os_data.input = solution_input;

	auto folder_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		FolderActionData* data = (FolderActionData*)_data;
		action_data->data = &data->os_data;
		FileExplorerGetFileAction(action_data);
		data->os_data.get_file_data.path.size = wcslen(data->os_data.get_file_data.path.buffer);
		if (data->os_data.get_file_data.path.size > 0) {
			Stream<wchar_t> stem = function::PathStem(data->os_data.get_file_data.path);

			ECS_TEMP_ASCII_STRING(ascii_stem, 256);
			function::ConvertWideCharsToASCII(stem, ascii_stem);
			data->library_input->DeleteAllCharacters();
			if (stem.size > 0) {
				data->library_input->InsertCharacters(ascii_stem.buffer, ascii_stem.size, 0, system);
				data->is_data_valid = true;
				return;
			}
		}
		else {
			data->library_input->DeleteAllCharacters();
		}
		data->is_data_valid = false;
	};

	drawer.SpriteButton<UI_CONFIG_MAKE_SQUARE>(config, { folder_action, folder_data, 0 }, ECS_TOOLS_UI_TEXTURE_FOLDER);
	drawer.NextRow();

	config.flag_count = 1;
	UIDrawerTextInput* library_input = drawer.TextInput<UI_CONFIG_RELATIVE_TRANSFORM>(config, LIBRARY_NAME, library_name);
	folder_data->library_input = library_input;
	drawer.NextRow();
	config.flag_count = 0;

	drawer.CheckBox("Graphics Module", &add_data->graphics_module);
	drawer.NextRow();

	UIConfigAbsoluteTransform add_transform;
	add_transform.scale.y = drawer.GetLabelScale("Add").y;
	add_transform.position = drawer.GetAlignedToBottom(add_transform.scale.y);
	config.AddFlag(add_transform);

	auto add_project_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		AddData* data = (AddData*)_data;
		if (data->folder_data->is_data_valid) {
			ECS_TEMP_STRING(library_name, 256);
			function::ConvertASCIIToWide(library_name, *data->library_name);
			library_name.size = data->library_name->size;
			data->solution_path_wide->size = wcslen(data->solution_path_wide->buffer);

			if (data->graphics_module) {
				SetProjectGraphicsModule(data->editor_state, *data->solution_path_wide, library_name, EditorModuleConfiguration::Debug);
			}
			else {
				AddProjectModule(data->editor_state, *data->solution_path_wide, library_name, EditorModuleConfiguration::Debug);
			}

			EDITOR_STATE(data->editor_state);

			ThreadTask task;
			ECS_SET_TASK_FUNCTION(task, SaveProjectModuleFileThreadTask);
			task.data = data->editor_state;
			task_manager->AddDynamicTaskAndWake(task);
		}
		else {
			EditorSetConsoleError(data->editor_state, ToStream("Could not add new project module. Invalid data."));
		}

		CloseXBorderClickableAction(action_data);
	};

	drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Add", { add_project_action, add_data, 0, UIDrawPhase::System });

	config.flag_count = 0;
	add_transform.scale = drawer.GetLabelScale("Cancel");
	add_transform.position.x = drawer.GetAlignedToRight(add_transform.scale.x).x;
	config.AddFlag(add_transform);

	drawer.Button<UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Cancel", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
}

void CreateAddModuleWizard(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIWindowDescriptor descriptor;
	descriptor.window_data = action_data->data;
	descriptor.window_data_size = 0;
	descriptor.window_name = "Add Module Wizard";
	descriptor.draw = AddModuleWizardDraw<false>;
	descriptor.initialize = AddModuleWizardDraw<true>;

	descriptor.initial_position_x = 0.0f;
	descriptor.initial_position_y = 0.0f;
	descriptor.initial_size_x = 100.0f;
	descriptor.initial_size_y = 100.0f;
	descriptor.destroy_action = ReleaseLockedWindow;
	
	system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_LOCK_WINDOW | UI_POP_UP_WINDOW_ALL);
}


void ModuleExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = ModuleExplorerDraw<false>;
	descriptor.initialize = ModuleExplorerDraw<true>;

	descriptor.window_name = MODULE_EXPLORER_WINDOW_NAME;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
}

void ModuleExplorerRemoveModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	if (data->selected_module != -1) {
		RemoveProjectModule(data->editor_state, data->selected_module);
		data->selected_module = -1;
		bool success = SaveModuleFile(data->editor_state);
		if (!success) {
			CreateErrorMessageWindow(system, "Could not save the module file after deletion of a module.");
		}
	}
}

void ModuleExplorerBuildModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	BuildProjectModule(data->editor_state, data->selected_module);
}

void ModuleExplorerCleanModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	CleanProjectModule(data->editor_state, data->selected_module);
}

void ModuleExplorerRebuildModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	RebuildProjectModule(data->editor_state, data->selected_module);
}

void ModuleExplorerBuildAll(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	BuildProjectModules(data);
}

void ModuleExplorerCleanAll(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	CleanProjectModules(data);
}

void ModuleExplorerRebuildAll(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	RebuildProjectModules(data);
}

void ModuleExplorerReset(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	ResetProjectModules(data->editor_state);
	data->selected_module = -1;
	bool success = SaveModuleFile(data->editor_state);
	if (!success) {
		EditorSetConsoleError(data->editor_state, ToStream("Could not save module file after reset."));
	}
}

void ModuleExplorerOpenBuildLog(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	wchar_t temp_memory[256];
	Stream<wchar_t> log_path(temp_memory, 0);
	GetProjectModuleBuildLogPath(data, log_path);
	
	// ECSEngine Text file display
	/*TextFileDrawData window_data;
	window_data.next_row_y_offset = 0.015f;
	window_data.path = log_path.buffer;
	CreateTextFileWindow(window_data, system, BUILD_LOG_WINDOW_NAME);*/
	action_data->data = &log_path;
	OpenFileWithDefaultApplicationStreamAction(action_data);
}

void ModuleExplorerOpenModuleFolder(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	
	const ProjectFile* project_file = (const ProjectFile*)data->project_file;
	ECS_TEMP_STRING(path, 256);
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(ToStream(PROJECT_MODULES_RELATIVE_PATH));
	path.AddSafe(L'\\');
	OS::LaunchFileExplorerWithError(path, system);
}

template<bool initialize>
void ModuleExplorerDraw(void* window_data, void* drawer_descriptor) {
	UI_PREPARE_DRAWER(initialize);

	EDITOR_STATE(window_data);
	EditorState* editor_state = (EditorState*)window_data;

	ModuleExplorerData* explorer_data = nullptr;
	if constexpr (initialize) {
		explorer_data = (ModuleExplorerData*)drawer.GetMainAllocatorBufferAndStoreAsResource(
			MODULE_EXPLORER_DATA_NAME,
			sizeof(ModuleExplorerData)
		);
		explorer_data->selected_module = -1;
		explorer_data->editor_state = editor_state;
		explorer_data->display_locked_file_warning = true;
		explorer_data->lazy_solution_last_write_timer.SetMarker();
	}
	else {
		explorer_data = (ModuleExplorerData*)drawer.GetResource(MODULE_EXPLORER_DATA_NAME);
	}

	ProjectModules* project_modules = (ProjectModules*)editor_state->project_modules;
	TaskDependencyElement _elements[128];
	Stream<TaskDependencyElement> elements(_elements, 0);

	UIDrawConfig config;

	ECS_TEMP_ASCII_STRING(error_message, 256);

#pragma region Header

	constexpr size_t HEADER_BUTTON_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_BORDER | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE;

	UIConfigBorder border;
	border.color = drawer.color_theme.borders;
	config.AddFlag(border);

	float next_row_padding = drawer.layout.next_row_padding;
	float row_y_offset = drawer.layout.next_row_y_offset;
	drawer.SetRowPadding(ECS_TOOLS_UI_ONE_PIXEL_X * 2.0f);
	drawer.SetNextRowYOffset(ECS_TOOLS_UI_ONE_PIXEL_Y);

	drawer.SetDrawMode(UIDrawerMode::Nothing);
	UIConfigAbsoluteTransform header_transform;
	header_transform.position.x = drawer.region_position.x + drawer.region_render_offset.x + border.thickness;
	header_transform.position.y = drawer.current_y;
	header_transform.scale = drawer.GetLabelScale(HEADER_ADD_MODULE);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION>(config, HEADER_ADD_MODULE, {CreateAddModuleWizard, window_data, 0, UIDrawPhase::System});

	config.flag_count = 1;
	UIConfigActiveState is_selected_state;
	if (explorer_data->selected_module != GRAPHICS_MODULE_INDEX) {
		is_selected_state.state = explorer_data->selected_module != -1;
	}
	else {
		is_selected_state.state = HasGraphicsModule(editor_state);
	}
	config.AddFlag(is_selected_state);

	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_REMOVE_MODULE);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION | UI_CONFIG_ACTIVE_STATE>(config, HEADER_REMOVE_MODULE, {ModuleExplorerRemoveModule, explorer_data, 0});

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_BUILD_MODULE);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION | UI_CONFIG_ACTIVE_STATE>(config, HEADER_BUILD_MODULE, { ModuleExplorerBuildModule, explorer_data, 0 });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_CLEAN_MODULE);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION | UI_CONFIG_ACTIVE_STATE>(config, HEADER_CLEAN_MODULE, { ModuleExplorerCleanModule, explorer_data, 0 });
	
	config.flag_count--;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_REBUILD_MODULE);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION | UI_CONFIG_ACTIVE_STATE>(config, HEADER_REBUILD_MODULE, { ModuleExplorerRebuildModule, explorer_data, 0 });

	config.flag_count--;

	size_t transform_flag_count = config.flag_count;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	config.AddFlag(header_transform);
	
	unsigned char default_value = 0;
	unsigned char* configuration_ptr = &default_value;
	if (explorer_data->selected_module != -1) {
		configuration_ptr = GetModuleConfigurationPtr(editor_state, explorer_data->selected_module);
	}

	float2 get_position = drawer.GetCurrentPosition();
	float2 get_scale = {0.0f, 0.0f};
	UIConfigGetTransform get_transform;
	get_transform.position = &get_position;
	get_transform.scale = &get_scale;
	config.AddFlag(get_transform);

	UIConfigComboBoxPrefix prefix;
	prefix.prefix = "Configuration: ";
	config.AddFlag(prefix);

	auto configuration_callback = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		ModuleExplorerData* data = (ModuleExplorerData*)_data;
		EDITOR_STATE(data->editor_state);
		// Save new module file
		bool success = SaveModuleFile(data->editor_state);

		// Delete associated files (.dll, .pdb, .lib, .exp) from the module folder
		const ProjectModules* modules = (const ProjectModules*)data->editor_state->project_modules;
		ECS_TEMP_STRING(path, 256);
		GetModulesFolder(data->editor_state, path);
		path.Add(ECS_OS_PATH_SEPARATOR);
		path.AddStream(modules->buffer[data->selected_module].library_name);
		size_t path_size = path.size;

		for (size_t index = 0; index < std::size(MODULE_ASSOCIATED_FILES); index++) {
			path.size = path_size;
			path.AddStreamSafe(ToStream(MODULE_ASSOCIATED_FILES[index]));
			if (ExistsFileOrFolder(path)) {
				OS::DeleteFileWithError(path, console);
			}
		}

		if (!success) {
			EditorSetConsoleError(data->editor_state, ToStream("Could not save module file after module configuration change."));
		}
	};

	UIConfigComboBoxCallback callback;
	callback.handler = { configuration_callback, explorer_data, 0 };
	config.AddFlag(callback);

	drawer.ComboBox<HEADER_BUTTON_CONFIGURATION | UI_CONFIG_ACTIVE_STATE | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX 
		| UI_CONFIG_GET_TRANSFORM | UI_CONFIG_COMBO_BOX_CALLBACK>(
		config,
		"Configuration Combo",
		Stream<const char*>(MODULE_CONFIGURATIONS, std::size(MODULE_CONFIGURATIONS)),
		std::size(MODULE_CONFIGURATIONS),
		configuration_ptr
	);

	config.flag_count = 1;
	header_transform.position.x = get_position.x + get_scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_OPEN_MODULE_FOLDER);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION>(config, HEADER_OPEN_MODULE_FOLDER, { ModuleExplorerOpenModuleFolder, editor_state, 0, UIDrawPhase::System });

	float min_position = header_transform.position.x + header_transform.scale.x + border.thickness + HEADER_ALL_OFFSET;
	
	float total_all_scale = drawer.GetLabelScale(HEADER_BUILD_ALL).x + drawer.GetLabelScale(HEADER_CLEAN_ALL).x + drawer.GetLabelScale(HEADER_REBUILD_ALL).x
		+ drawer.GetLabelScale(HEADER_RESET_ALL).x + drawer.GetLabelScale(HEADER_OPEN_BUILD_LOG).x + (std::size(MODULE_CONFIGURATIONS) + 2) * border.thickness;
	config.flag_count = 1;

	header_transform.position.x = drawer.GetAlignedToRightOverLimit(total_all_scale).x;
	header_transform.position.x = std::max(header_transform.position.x, min_position);
	header_transform.scale = drawer.GetLabelScale(HEADER_OPEN_BUILD_LOG);
	config.AddFlag(header_transform);
	
	drawer.Button<HEADER_BUTTON_CONFIGURATION>(config, HEADER_OPEN_BUILD_LOG, { ModuleExplorerOpenBuildLog, editor_state, 0, UIDrawPhase::System });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_BUILD_ALL);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION>(config, HEADER_BUILD_ALL, { ModuleExplorerBuildAll, editor_state, 0 });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_CLEAN_ALL);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION>(config, HEADER_CLEAN_ALL, { ModuleExplorerCleanAll, editor_state, 0 });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_REBUILD_ALL);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION>(config, HEADER_REBUILD_ALL, { ModuleExplorerRebuildAll, editor_state, 0 });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x + border.thickness;
	header_transform.scale = drawer.GetLabelScale(HEADER_RESET_ALL);
	config.AddFlag(header_transform);

	drawer.Button<HEADER_BUTTON_CONFIGURATION>(config, HEADER_RESET_ALL, { ModuleExplorerReset, explorer_data, 0 });

	drawer.UpdateCurrentRowScale(header_transform.scale.y);
	drawer.SetRowPadding(next_row_padding);
	drawer.SetNextRowYOffset(row_y_offset);
	drawer.NextRow();
	//drawer.SetDrawMode(UIDrawerMode::Indent);

#pragma endregion

#pragma region Module draw base

	// index GRAPHICS_MODULE_INDEX is used to signal the graphics module
	auto module_draw_base = [&](Stream<wchar_t> module_name, EditorModuleLoadStatus load_status, unsigned int index) {
		constexpr size_t MODULE_SPRITE_CONFIGURATION = UI_CONFIG_RELATIVE_TRANSFORM;
		constexpr size_t BUTTON_CONFIGURATION = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X |
			UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_TEXT_PARAMETERS;

		config.flag_count = 0;
		UIConfigRelativeTransform transform;
		const float SPRITE_SIZE = drawer.layout.default_element_y;
		transform.scale = drawer.GetSquareScale(SPRITE_SIZE) / drawer.GetStaticElementDefaultScale();
		config.AddFlag(transform);

		switch (load_status) {
			case EditorModuleLoadStatus::Good:
					drawer.SpriteRectangle<MODULE_SPRITE_CONFIGURATION>(config, ECS_TOOLS_UI_TEXTURE_MODULE, EDITOR_GREEN_COLOR);
					drawer.Indent();
					drawer.SpriteRectangle<MODULE_SPRITE_CONFIGURATION>(config, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, EDITOR_GREEN_COLOR);
					drawer.Indent();
					break;		
			case EditorModuleLoadStatus::OutOfDate:
					drawer.SpriteRectangle<MODULE_SPRITE_CONFIGURATION>(config, ECS_TOOLS_UI_TEXTURE_MODULE, EDITOR_YELLOW_COLOR);
					drawer.Indent();
					drawer.SpriteRectangle<MODULE_SPRITE_CONFIGURATION>(config, ECS_TOOLS_UI_TEXTURE_CLOCK, EDITOR_YELLOW_COLOR);
					drawer.Indent();
					break;
			case EditorModuleLoadStatus::Failed:
					drawer.SpriteRectangle<MODULE_SPRITE_CONFIGURATION>(config, ECS_TOOLS_UI_TEXTURE_MODULE, EDITOR_RED_COLOR);
					drawer.Indent();
					drawer.SpriteRectangle<MODULE_SPRITE_CONFIGURATION>(config, ECS_TOOLS_UI_TEXTURE_X, EDITOR_RED_COLOR);
					drawer.Indent();
					break;
		}

		ECS_TEMP_ASCII_STRING(ascii_module_name, 256);
		function::ConvertWideCharsToASCII(module_name, ascii_module_name);
		ascii_module_name[ascii_module_name.size] = '\0';

		struct SelectModuleData {
			ModuleExplorerData* explorer_data;
			unsigned int index;
		};

		auto SelectModule = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SelectModuleData* data = (SelectModuleData*)_data;
			data->explorer_data->selected_module = data->index;
			if (data->index == GRAPHICS_MODULE_INDEX) {
				ChangeInspectorToGraphicsModule(data->explorer_data->editor_state);
			}
			else {
				ChangeInspectorToModule(data->explorer_data->editor_state, data->index);
			}
		};

		SelectModuleData select_data;
		select_data.index = index;
		select_data.explorer_data = explorer_data;

		config.flag_count = 0;
		UIConfigWindowDependentSize window_size;
		window_size.type = WindowSizeTransformType::Horizontal;
		window_size.scale_factor = drawer.GetWindowSizeScaleUntilBorder();
		config.AddFlag(window_size);

		UIConfigTextAlignment alignment;
		alignment.horizontal = TextAlignment::Left;
		alignment.vertical = TextAlignment::Middle;
		config.AddFlag(alignment);

		UIConfigTextParameters text_parameters;
		text_parameters.size = drawer.GetFontSize();
		text_parameters.character_spacing = drawer.font.character_spacing;
		text_parameters.color = MODULE_COLORS[GetModuleConfigurationChar(editor_state, index)];
		config.AddFlag(text_parameters);

		if (index == explorer_data->selected_module) {
			drawer.Button<BUTTON_CONFIGURATION>(config, ascii_module_name.buffer, { SelectModule, &select_data, sizeof(select_data) });
		}
		else {
			drawer.Button<BUTTON_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT>(config, ascii_module_name.buffer, { SelectModule, &select_data, sizeof(select_data) });
		}
		drawer.NextRow();
		config.flag_count = 0;
	};

#pragma endregion

#pragma region Graphics Module

	UIConfigTextParameters header_text_parameters;
	header_text_parameters.size *= {1.25f, 1.25f};
	header_text_parameters.color = drawer.color_theme.default_text;
	header_text_parameters.character_spacing = drawer.font.character_spacing;
	UIDrawConfig header_config;
	header_config.AddFlag(header_text_parameters);
	drawer.Text<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_PARAMETERS>(header_config, "Graphics Module");

	drawer.NextRow(0.75f);
	drawer.CrossLine();
	drawer.NextRow(-0.25f);

	size_t lazy_solution_duration = explorer_data->lazy_solution_last_write_timer.GetDurationSinceMarker_ms();

	if (HasGraphicsModule(editor_state)) {
		if (UpdateProjectModuleLibraryLastWrite(editor_state, GRAPHICS_MODULE_INDEX)) {
			LoadEditorModule(editor_state, GRAPHICS_MODULE_INDEX);
		}
		if (lazy_solution_duration > LAZY_SOLUTION_LAST_WRITE_TARGET_MILLISECONDS) {
			bool is_updated = UpdateProjectModuleSolutionLastWrite(editor_state, GRAPHICS_MODULE_INDEX);
			if (is_updated && project_modules->buffer[GRAPHICS_MODULE_INDEX].load_status == EditorModuleLoadStatus::Good) {
				project_modules->buffer[GRAPHICS_MODULE_INDEX].load_status = EditorModuleLoadStatus::OutOfDate;
			}
		}
	}

	if (project_modules->buffer[GRAPHICS_MODULE_INDEX].library_name.size == 0 || project_modules->buffer[GRAPHICS_MODULE_INDEX].solution_path.size == 0) {
		module_draw_base(ToStream(L"No graphics module is selected."), EditorModuleLoadStatus::Failed, GRAPHICS_MODULE_INDEX);
	}
	else {
		module_draw_base(project_modules->buffer[GRAPHICS_MODULE_INDEX].library_name, project_modules->buffer[GRAPHICS_MODULE_INDEX].load_status, GRAPHICS_MODULE_INDEX);
	}

#pragma endregion

#pragma region Module info

	drawer.Text<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_PARAMETERS>(header_config, "Project Modules");
	drawer.NextRow(0.75f);
	drawer.CrossLine();
	drawer.NextRow(-0.25f);
	// Removing the header_text_parameters
	header_config.flag_count--;

	if (lazy_solution_duration > LAZY_SOLUTION_LAST_WRITE_TARGET_MILLISECONDS) {
		for (size_t index = GRAPHICS_MODULE_INDEX + 1; index < project_modules->size; index++) {
			if (UpdateProjectModuleLibraryLastWrite(editor_state, index)) {
				bool success = false;
				if (project_modules->buffer[index].library_last_write_time != 0) {
					success = HasModuleFunction(editor_state, index);
				}
				SetModuleLoadStatus(project_modules->buffer + index, success);
			}
			bool is_updated = UpdateProjectModuleSolutionLastWrite(editor_state, index);
			if (is_updated && project_modules->buffer[index].load_status == EditorModuleLoadStatus::Good) {
				project_modules->buffer[index].load_status = EditorModuleLoadStatus::OutOfDate;
			}
			module_draw_base(project_modules->buffer[index].library_name, project_modules->buffer[index].load_status, index);
		}
		explorer_data->lazy_solution_last_write_timer.SetMarker();
	}
	else {
		for (size_t index = GRAPHICS_MODULE_INDEX + 1; index < project_modules->size; index++) {
			if (UpdateProjectModuleLibraryLastWrite(editor_state, index)) {
				bool success = false;
				if (project_modules->buffer[index].library_last_write_time != 0) {
					success = HasModuleFunction(editor_state, index);
				}
				SetModuleLoadStatus(project_modules->buffer + index, success);
			}
			module_draw_base(project_modules->buffer[index].library_name, project_modules->buffer[index].load_status, index);
		}
	}

#pragma endregion

	if constexpr (!initialize) {
		// Inform the user when many .pdb.locked files gathered
		size_t locked_files_size = GetVisualStudioLockedFilesSize(editor_state);
		if (locked_files_size > ECS_GB && explorer_data->display_locked_file_warning) {
			ECS_FORMAT_TEMP_STRING(console_output, "Visual studio locked files size surpassed 1 GB (actual size is {0}). Consider deleting the files now"
				" if the debugger is not opened or by reopening the project.", locked_files_size);
			EditorSetConsoleWarn(editor_state, console_output);
		}
	}

}

void CreateModuleExplorer(EditorState* editor_state) {
	CreateDockspaceFromWindow(MODULE_EXPLORER_WINDOW_NAME, editor_state, CreateModuleExplorerWindow);
}

void CreateModuleExplorerAction(ActionData* action_data) {
	CreateModuleExplorer((EditorState*)action_data->data);
}

unsigned int CreateModuleExplorerWindow(EditorState* _editor_state) {
	EDITOR_STATE(_editor_state);

	constexpr float window_size_x = 0.7f;
	constexpr float window_size_y = 0.7f;
	float2 window_size = { window_size_x, window_size_y };

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, window_size.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, window_size.y);
	descriptor.initial_size_x = window_size.x;
	descriptor.initial_size_y = window_size.y;

	size_t stack_memory[128];
	ModuleExplorerSetDescriptor(descriptor, _editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
}