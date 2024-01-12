#include "editorpch.h"
#include "ModuleExplorer.h"
#include "../Editor/EditorState.h"
#include "HelperWindows.h"
#include "../Modules/Module.h"
#include "../Editor/EditorPalette.h"
#include "../Modules/ModuleFile.h"
#include "Inspector.h"
#include "../Editor/EditorEvent.h"
#include "../Project/ProjectOperations.h"
#include "../Sandbox/SandboxModule.h"

using namespace ECSEngine;
ECS_TOOLS;

#define MODULE_EXPLORER_DATA_NAME "Module Explorer Data"

#define GRAPHICS_MODULE_TEXTURE ECS_TOOLS_UI_TEXTURE_LIGHT_BULB

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

constexpr const char* HEADER_SET_MODULE_CONFIGURATIONS = "Change All";

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
	EDITOR_MODULE_CONFIGURATION* configurations;
};

// --------------------------------------------------------------------------------------------------------

void AddModuleWizardDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	const float INPUT_X_FACTOR = 7.0f;
	const char* SOLUTION_PATH_NAME = "Solution Path";
	const char* SOLUTION_PATH_BUFFER_NAME = "Solution Path buffer";
	const char* LIBRARY_NAME = "Library Name";
	const char* LIBRARY_BUFFER_NAME = "Library Name buffer";
	const char* SOLUTION_PATH_WIDE_NAME = "Solution Path Wide";
	const char* FOLDER_DATA_NAME = "Folder data";
	const char* ADD_DATA_NAME = "Add data";

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

	if (initialize) {
		solution_path = (CapacityStream<char>*)CoalesceCapacityStreamWithData(
			drawer.GetMainAllocatorBufferAndStoreAsResource(SOLUTION_PATH_BUFFER_NAME, sizeof(CapacityStream<char>) + sizeof(char) * 256),
			0, 
			256
		);
		library_name = (CapacityStream<char>*)CoalesceCapacityStreamWithData(
			drawer.GetMainAllocatorBufferAndStoreAsResource((LIBRARY_BUFFER_NAME), sizeof(CapacityStream<char>) + sizeof(char) * 64),
			0, 
			64
		);
		solution_path_wide = (CapacityStream<wchar_t>*)CoalesceCapacityStreamWithData(
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
		folder_data->os_data.get_file_data.error_message.buffer = nullptr;
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

	UIDrawerTextInput* solution_input = drawer.TextInput(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_TEXT_INPUT_CALLBACK, config, SOLUTION_PATH_NAME, solution_path);
	folder_data->os_data.input = solution_input;

	auto folder_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		FolderActionData* data = (FolderActionData*)_data;
		action_data->data = &data->os_data;

		Stream<wchar_t> extensions[] = {
			L".vcxproj"
		};

		data->os_data.get_file_data.extensions = { extensions, std::size(extensions) };
		
		FileExplorerGetFileAction(action_data);
		if (data->os_data.get_file_data.path.size > 0) {
			Stream<wchar_t> stem = PathStem(data->os_data.get_file_data.path);

			ECS_STACK_CAPACITY_STREAM(char, ascii_stem, 256);
			ConvertWideCharsToASCII(stem, ascii_stem);
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

	drawer.SpriteButton(UI_CONFIG_MAKE_SQUARE, config, { folder_action, folder_data, 0 }, ECS_TOOLS_UI_TEXTURE_FOLDER);
	drawer.NextRow();

	config.flag_count = 1;
	UIDrawerTextInput* library_input = drawer.TextInput(UI_CONFIG_RELATIVE_TRANSFORM, config, LIBRARY_NAME, library_name);
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
			ECS_STACK_CAPACITY_STREAM(wchar_t, library_name, 256);
			ConvertASCIIToWide(library_name, *data->library_name);
			data->solution_path_wide->size = wcslen(data->solution_path_wide->buffer);

			bool success = AddModule(data->editor_state, *data->solution_path_wide, library_name, data->graphics_module);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, "Could not add new module with name {#} and path {#}.", *data->library_name, *data->solution_path_wide);
				EditorSetConsoleError(error_message);
			}

			if (success) {
				ThreadTask task = ECS_THREAD_TASK_NAME(SaveProjectModuleFileThreadTask, data->editor_state, 0);
				data->editor_state->task_manager->AddDynamicTaskAndWake(task);
			}
		}
		else {
			EditorSetConsoleError("Could not add new project module. Invalid data.");
		}

		DestroyCurrentActionWindow(action_data);
	};

	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Add", { add_project_action, add_data, 0, ECS_UI_DRAW_SYSTEM });

	config.flag_count = 0;
	add_transform.scale = drawer.GetLabelScale("Cancel");
	add_transform.position.x = drawer.GetAlignedToRight(add_transform.scale.x).x;
	config.AddFlag(add_transform);

	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Cancel", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
}

// --------------------------------------------------------------------------------------------------------

void CreateAddModuleWizard(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIWindowDescriptor descriptor;
	descriptor.window_data = action_data->data;
	descriptor.window_data_size = 0;
	descriptor.window_name = "Add Module Wizard";
	descriptor.draw = AddModuleWizardDraw;

	descriptor.initial_position_x = 0.0f;
	descriptor.initial_position_y = 0.0f;
	descriptor.initial_size_x = 100.0f;
	descriptor.initial_size_y = 100.0f;
	descriptor.destroy_action = ReleaseLockedWindow;
	
	system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_LOCK_WINDOW | UI_POP_UP_WINDOW_ALL);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = ModuleExplorerDraw;

	descriptor.window_name = MODULE_EXPLORER_WINDOW_NAME;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerConfirmRemoveModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	// Delete the module from the sandboxes
	RemoveSandboxModuleForced(data->editor_state, data->selected_module);
	RemoveModule(data->editor_state, data->selected_module);

	memmove(
		data->configurations + data->selected_module,
		data->configurations + data->selected_module + 1,
		(data->editor_state->project_modules->size - data->selected_module) * sizeof(EDITOR_MODULE_CONFIGURATION)
	);
	data->selected_module = -1;
	bool success = SaveModuleFile(data->editor_state);
	if (!success) {
		CreateErrorMessageWindow(system, "Could not save the module file after deletion of a module.");
	}
}

void ModuleExplorerRemoveModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	if (data->selected_module != -1) {
		// Detect if another module depends on this one
		ECS_STACK_CAPACITY_STREAM(unsigned int, dependent_modules, 512);
		GetModulesTypesDependentUpon(data->editor_state, data->selected_module, dependent_modules);
		if (dependent_modules.size == 0) {
			// Check to see if any sandboxes depend upon on it
			bool is_used = IsModuleInfoUsed(data->editor_state, data->selected_module, false, EDITOR_MODULE_CONFIGURATION_COUNT, &dependent_modules);
			if (is_used) {
				ECS_STACK_CAPACITY_STREAM(char, description, 1024);
				description.CopyOther("Are you sure you want to remove the selected module? It is being used by the following sandboxes: ");
				for (unsigned int index = 0; index < dependent_modules.size; index++) {
					ConvertIntToChars(description, dependent_modules[index]);
					if (index < dependent_modules.size - 1) {
						description.AddStream(", ");
					}
				}
				CreateConfirmWindow(system, description, { ModuleExplorerConfirmRemoveModule, data, 0 });
			}
			else {
				ModuleExplorerConfirmRemoveModule(action_data);
			}
		}
		else {
			// It has dependent modules - cannot delete the current one without deleting the dependent modules
			ECS_STACK_CAPACITY_STREAM(char, description, 1024);
			description.CopyOther("The selected module is being referenced by other modules. In order to remove this one you must remove all "
				"the others before hand. The dependent modules: ");
			for (unsigned int index = 0; index < dependent_modules.size; index++) {
				Stream<wchar_t> library_name = data->editor_state->project_modules->buffer[dependent_modules[index]].library_name;
				ConvertWideCharsToASCII(library_name, description);
				if (index < dependent_modules.size - 1) {
					description.AddStream(", ");
				}
			}
			CreateErrorMessageWindow(system, description);
		}
	}
}

// --------------------------------------------------------------------------------------------------------

struct ModuleExplorerRunModuleBuildCommandData {
	EditorState* editor_state;
	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION configuration;
};

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerPrintAllConsoleMessageAfterBuildCommand(EditorState* editor_state, EDITOR_LAUNCH_BUILD_COMMAND_STATUS* command_statuses) {
	ECS_STACK_CAPACITY_STREAM(char, console_message, 8192);

	const ProjectModules* modules = editor_state->project_modules;
	unsigned int counts_for_status_type[EDITOR_LAUNCH_BUILD_COMMAND_COUNT] = { 0 };
	for (unsigned int index = 0; index < modules->size; index++) {
		counts_for_status_type[command_statuses[index]]++;
	}

	auto add_same_type_module_status_to_listing = [&console_message, modules, command_statuses, counts_for_status_type](EDITOR_LAUNCH_BUILD_COMMAND_STATUS status_type) {
		for (unsigned int index = 0; index < modules->size; index++) {
			if (command_statuses[index]) {
				ConvertWideCharsToASCII(modules->buffer[index].library_name, console_message);
				console_message.Add(',');
				console_message.Add(' ');
			}
		}
	};

	auto append_string_message_for_type = [&](EDITOR_LAUNCH_BUILD_COMMAND_STATUS status_type, const char* message) {
		if (counts_for_status_type[status_type] > 0) {
			ECS_FORMAT_TEMP_STRING(append_message, message, counts_for_status_type[status_type]);
			console_message.AddStreamSafe(append_message);
			add_same_type_module_status_to_listing(status_type);

			EditorSetConsoleInfo(console_message);
		}
	};

	append_string_message_for_type(EDITOR_LAUNCH_BUILD_COMMAND_EXECUTING, "{#} modules have launched successfully their command. These are: ");
	console_message.size = 0;
	
	append_string_message_for_type(EDITOR_LAUNCH_BUILD_COMMAND_SKIPPED, "{#} modules have been skipped (they are up to date). These are: ");
	console_message.size = 0;

	append_string_message_for_type(EDITOR_LAUNCH_BUILD_COMMAND_ERROR_WHEN_LAUNCHING, "{#} modules have failed to launch their command line prompt. These are: ");
	console_message.size = 0;

	append_string_message_for_type(EDITOR_LAUNCH_BUILD_COMMAND_ALREADY_RUNNING, "{#} modules are already running. The current command will be ignored. These are: ");
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerBuildModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerRunModuleBuildCommandData* data = (ModuleExplorerRunModuleBuildCommandData*)_data;
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS command_status = BuildModule(data->editor_state, data->module_index, data->configuration, nullptr, false, true);
	PrintConsoleMessageForBuildCommand(data->editor_state, data->module_index, data->configuration, command_status);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerCleanModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerRunModuleBuildCommandData* data = (ModuleExplorerRunModuleBuildCommandData*)_data;
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS command_status = CleanModule(data->editor_state, data->module_index, data->configuration);
	PrintConsoleMessageForBuildCommand(data->editor_state, data->module_index, data->configuration, command_status);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerRebuildModule(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerRunModuleBuildCommandData* data = (ModuleExplorerRunModuleBuildCommandData*)_data;
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS command_status = RebuildModule(data->editor_state, data->module_index, data->configuration);
	PrintConsoleMessageForBuildCommand(data->editor_state, data->module_index, data->configuration, command_status);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerCommandAll(ActionData* action_data, void (*function)
	(
		EditorState*,
		Stream<unsigned int>,
		const EDITOR_MODULE_CONFIGURATION*,
		EDITOR_LAUNCH_BUILD_COMMAND_STATUS*,
		std::atomic<EDITOR_FINISH_BUILD_COMMAND_STATUS>*,
		bool
	)
) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;

	unsigned int module_count = data->editor_state->project_modules->size;
	unsigned int total_count = module_count * EDITOR_MODULE_CONFIGURATION_COUNT;
	EDITOR_LAUNCH_BUILD_COMMAND_STATUS* statuses = (EDITOR_LAUNCH_BUILD_COMMAND_STATUS*)ECS_STACK_ALLOC(
		sizeof(EDITOR_LAUNCH_BUILD_COMMAND_STATUS) * total_count
	);

	EDITOR_MODULE_CONFIGURATION* configurations = (EDITOR_MODULE_CONFIGURATION*)ECS_STACK_ALLOC(
		sizeof(EDITOR_MODULE_CONFIGURATION) * total_count
	);

	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, total_count);
	indices.size = total_count;
	for (unsigned int index = 0; index < module_count; index++) {
		unsigned int offset = index * EDITOR_MODULE_CONFIGURATION_COUNT;
		for (unsigned int subindex = 0; subindex < EDITOR_MODULE_CONFIGURATION_COUNT; subindex++) {
			indices[offset + subindex] = index;
			configurations[offset + subindex] = (EDITOR_MODULE_CONFIGURATION)subindex;
		}
	}

	function(data->editor_state, indices, configurations, statuses, nullptr, false);
	//ModuleExplorerPrintAllConsoleMessageAfterBuildCommand(data->editor_state, statuses);
}

void ModuleExplorerBuildAll(ActionData* action_data) {
	ModuleExplorerCommandAll(action_data, BuildModules);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerCleanAll(ActionData* action_data) {
	ModuleExplorerCommandAll(action_data, CleanModules);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerRebuildAll(ActionData* action_data) {
	ModuleExplorerCommandAll(action_data, RebuildModules);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerReset(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	ResetModules(data->editor_state);
	data->selected_module = -1;
	bool success = SaveModuleFile(data->editor_state);
	if (!success) {
		EditorSetConsoleError("Could not save module file after reset.");
	}
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerOpenBuildLog(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	ECS_STACK_CAPACITY_STREAM(wchar_t, log_path, 256);
	GetProjectDebugFolder(data, log_path);
	
	// ECSEngine Text file display
	/*TextFileDrawData window_data;
	window_data.next_row_y_offset = 0.015f;
	window_data.path = log_path.buffer;
	CreateTextFileWindow(window_data, system, BUILD_LOG_WINDOW_NAME);*/
	Stream<wchar_t> stream_log(log_path);
	action_data->data = &stream_log;
	OpenFileWithDefaultApplicationStreamAction(action_data);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerOpenModuleFolder(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	
	const ProjectFile* project_file = (const ProjectFile*)data->project_file;
	ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
	path.CopyOther(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(PROJECT_MODULES_RELATIVE_PATH);
	path.AddAssert(L'\\');
	OS::LaunchFileExplorerWithError(path, system);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerSetAllConfiguration(ActionData* action_data, EDITOR_MODULE_CONFIGURATION configuration) {
	UI_UNPACK_ACTION_DATA;

	ModuleExplorerData* data = (ModuleExplorerData*)_data;
	for (size_t index = 0; index < data->editor_state->project_modules->size; index++) {
		data->configurations[index] = configuration;
	}
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerSetAllDebug(ActionData* action_data) {
	ModuleExplorerSetAllConfiguration(action_data, EDITOR_MODULE_CONFIGURATION_DEBUG);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerSetAllRelease(ActionData* action_data) {
	ModuleExplorerSetAllConfiguration(action_data, EDITOR_MODULE_CONFIGURATION_RELEASE);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerSetAllDistribution(ActionData* action_data) {
	ModuleExplorerSetAllConfiguration(action_data, EDITOR_MODULE_CONFIGURATION_DISTRIBUTION);
}

// --------------------------------------------------------------------------------------------------------

void ModuleExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;

	ModuleExplorerData* explorer_data = nullptr;
	if (initialize) {
		explorer_data = (ModuleExplorerData*)drawer.GetMainAllocatorBufferAndStoreAsResource(
			MODULE_EXPLORER_DATA_NAME,
			sizeof(ModuleExplorerData)
		);
		explorer_data->selected_module = -1;
		explorer_data->editor_state = editor_state;

		const size_t ALLOCATE_COUNT = 32;
		explorer_data->configurations = (EDITOR_MODULE_CONFIGURATION*)drawer.GetMainAllocatorBuffer(
			sizeof(EDITOR_MODULE_CONFIGURATION) * ALLOCATE_COUNT
		);
		memset(explorer_data->configurations, 0, sizeof(EDITOR_MODULE_CONFIGURATION) * ALLOCATE_COUNT);
	}
	else {
		explorer_data = (ModuleExplorerData*)drawer.GetResource(MODULE_EXPLORER_DATA_NAME);
	}

	ProjectModules* project_modules = editor_state->project_modules;
	TaskSchedulerElement _elements[128];
	Stream<TaskSchedulerElement> elements(_elements, 0);

	UIDrawConfig config;

	ECS_STACK_CAPACITY_STREAM(char, error_message, 256);

#pragma region Header

	// Reduce the font size, so all the buttons fit better into a smaller space
	const size_t HEADER_BUTTON_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_BORDER | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE
		| UI_CONFIG_DO_CACHE;
	const size_t HEADER_BASE_CONFIGS = 1;

	UIConfigBorder border;
	border.color = drawer.color_theme.borders;
	border.is_interior = true;
	config.AddFlag(border);

	float next_row_padding = drawer.layout.next_row_padding;
	float row_y_offset = drawer.layout.next_row_y_offset;
	drawer.SetRowPadding(ECS_TOOLS_UI_ONE_PIXEL_X * 2.0f);
	drawer.SetNextRowYOffset(ECS_TOOLS_UI_ONE_PIXEL_Y);

	drawer.SetDrawMode(ECS_UI_DRAWER_NOTHING);
	UIConfigAbsoluteTransform header_transform;
	header_transform.position.x = drawer.region_position.x + drawer.region_render_offset.x;
	header_transform.position.y = drawer.current_y;
	header_transform.scale = drawer.GetLabelScale(HEADER_ADD_MODULE);
	config.AddFlag(header_transform);

	drawer.Button(HEADER_BUTTON_CONFIGURATION, config, HEADER_ADD_MODULE, {CreateAddModuleWizard, window_data, 0, ECS_UI_DRAW_SYSTEM});

	config.flag_count = HEADER_BASE_CONFIGS;
	bool is_any_module_info_locked = IsAnyModuleInfoLocked(editor_state);
	// Have one more condition for the remove active state
	// If there is a locked module info, don't allow removals
	// As to not perturb references to that module index
	UIConfigActiveState is_selected_state;
	is_selected_state.state = explorer_data->selected_module != -1 && !is_any_module_info_locked;
	config.AddFlag(is_selected_state);

	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_REMOVE_MODULE);
	config.AddFlag(header_transform);

	drawer.Button(HEADER_BUTTON_CONFIGURATION | UI_CONFIG_ACTIVE_STATE, config, HEADER_REMOVE_MODULE, {ModuleExplorerRemoveModule, explorer_data, 0});

	config.flag_count = HEADER_BASE_CONFIGS;
	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_SET_MODULE_CONFIGURATIONS);
	config.AddFlag(header_transform);

	UIDrawerMenuState menu_state;
	if (initialize) {
		UIActionHandler set_configuration_handlers[EDITOR_MODULE_CONFIGURATION_COUNT];
		set_configuration_handlers[0] = { ModuleExplorerSetAllDebug, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		set_configuration_handlers[1] = { ModuleExplorerSetAllRelease, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		set_configuration_handlers[2] = { ModuleExplorerSetAllDistribution, editor_state, 0, ECS_UI_DRAW_SYSTEM };

		ECS_STACK_CAPACITY_STREAM(char, set_module_configuration_menu_characters, 256);
		for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
			set_module_configuration_menu_characters.AddStream(MODULE_CONFIGURATIONS[index]);
			set_module_configuration_menu_characters.AddAssert('\n');
		}
		set_module_configuration_menu_characters[set_module_configuration_menu_characters.size - 1] = '\0';

		menu_state.click_handlers = set_configuration_handlers;
		menu_state.left_characters = set_module_configuration_menu_characters.buffer;
		menu_state.row_count = EDITOR_MODULE_CONFIGURATION_COUNT;
		menu_state.submenu_index = 0;
	}
	drawer.Menu(HEADER_BUTTON_CONFIGURATION | UI_CONFIG_MENU_COPY_STATES, config, HEADER_SET_MODULE_CONFIGURATIONS, &menu_state);

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_OPEN_MODULE_FOLDER);
	config.AddFlag(header_transform);

	drawer.Button(HEADER_BUTTON_CONFIGURATION, config, HEADER_OPEN_MODULE_FOLDER, { ModuleExplorerOpenModuleFolder, editor_state, 0, ECS_UI_DRAW_SYSTEM });
	config.flag_count--;

	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_OPEN_BUILD_LOG);
	config.AddFlag(header_transform);
	
	drawer.Button(HEADER_BUTTON_CONFIGURATION, config, HEADER_OPEN_BUILD_LOG, { ModuleExplorerOpenBuildLog, editor_state, 0, ECS_UI_DRAW_SYSTEM });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_BUILD_ALL);
	config.AddFlag(header_transform);

	drawer.Button(HEADER_BUTTON_CONFIGURATION, config, HEADER_BUILD_ALL, { ModuleExplorerBuildAll, explorer_data, 0 });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_CLEAN_ALL);
	config.AddFlag(header_transform);

	drawer.Button(HEADER_BUTTON_CONFIGURATION, config, HEADER_CLEAN_ALL, { ModuleExplorerCleanAll, explorer_data, 0 });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_REBUILD_ALL);
	config.AddFlag(header_transform);

	drawer.Button(HEADER_BUTTON_CONFIGURATION, config, HEADER_REBUILD_ALL, { ModuleExplorerRebuildAll, explorer_data, 0 });

	config.flag_count--;
	header_transform.position.x += header_transform.scale.x;
	header_transform.scale = drawer.GetLabelScale(HEADER_RESET_ALL);
	config.AddFlag(header_transform);

	// The reset should not be active while there is a locked module
	UIConfigActiveState reset_active_state;
	reset_active_state.state = !is_any_module_info_locked;
	config.AddFlag(reset_active_state);
	drawer.Button(HEADER_BUTTON_CONFIGURATION | UI_CONFIG_ACTIVE_STATE, config, HEADER_RESET_ALL, { ModuleExplorerReset, explorer_data, 0 });
	config.flag_count--;

	drawer.UpdateCurrentRowScale(header_transform.scale.y);
	drawer.SetRowPadding(next_row_padding);
	drawer.SetNextRowYOffset(row_y_offset);
	drawer.NextRow(0.25f);
	drawer.SetDrawMode(ECS_UI_DRAWER_INDENT);

#pragma endregion

#pragma region Module draw base

	config.flag_count = 0;

	float build_label_scale = drawer.GetLabelScale(HEADER_BUILD_MODULE).x;
	float clean_label_scale = drawer.GetLabelScale(HEADER_CLEAN_MODULE).x;
	float rebuild_label_scale = drawer.GetLabelScale(HEADER_REBUILD_MODULE).x;

	Stream<Stream<char>> combo_labels = Stream<Stream<char>>(MODULE_CONFIGURATIONS, EDITOR_MODULE_CONFIGURATION_COUNT);

	auto module_draw_base = [&](EDITOR_MODULE_LOAD_STATUS load_status, unsigned int index) {
		const size_t MODULE_SPRITE_CONFIGURATION = UI_CONFIG_MAKE_SQUARE;
		const size_t MAIN_BUTTON_CONFIGURATION = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X |
			UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_TEXT_PARAMETERS;
		const size_t SECONDARY_BUTTON_CONFIGURATION = UI_CONFIG_ACTIVE_STATE;
		const size_t COMBO_CONFIGURATION = UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX;

		Stream<wchar_t> module_name = project_modules->buffer[index].library_name;
		bool* is_graphics_module = &project_modules->buffer[index].is_graphics_module;

		config.flag_count = 0;
		UIConfigRelativeTransform transform;
		config.AddFlag(transform);

		struct ChangeGraphicsStatusData {
			EditorState* editor_state;
			bool* is_graphics_module;
		};

		auto change_graphics_status = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ChangeGraphicsStatusData* data = (ChangeGraphicsStatusData*)_data;
			bool is_set = *data->is_graphics_module;
			*data->is_graphics_module = !is_set;

			if (!SaveModuleFile(data->editor_state)) {
				EditorSetConsoleError("Could not save module file after changing module graphics status");
			}
		};

		ChangeGraphicsStatusData status_data = { editor_state, is_graphics_module };

		const wchar_t* module_texture = *is_graphics_module ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;
		const wchar_t* status_texture;
		Color status_color;

		switch (load_status) {
			case EDITOR_MODULE_LOAD_GOOD:
					status_color = EDITOR_GREEN_COLOR;
					status_texture = ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK;
					break;		
			case EDITOR_MODULE_LOAD_OUT_OF_DATE:
					status_color = EDITOR_YELLOW_COLOR;
					status_texture = ECS_TOOLS_UI_TEXTURE_CLOCK;
					break;
			case EDITOR_MODULE_LOAD_FAILED:
					status_color = EDITOR_RED_COLOR;
					status_texture = ECS_TOOLS_UI_TEXTURE_MINUS;
					break;
			default:
				ECS_ASSERT(false, "Invalid load status for a module.");
		}

		drawer.SpriteButton(MODULE_SPRITE_CONFIGURATION, config, { change_graphics_status, &status_data, sizeof(status_data) }, module_texture, status_color);
		drawer.Indent();
		drawer.SpriteRectangle(MODULE_SPRITE_CONFIGURATION, config, status_texture, status_color);
		drawer.Indent();

		ECS_STACK_CAPACITY_STREAM(char, ascii_module_name, 256);
		ConvertWideCharsToASCII(module_name, ascii_module_name);
		ascii_module_name[ascii_module_name.size] = '\0';

		struct SelectModuleData {
			ModuleExplorerData* explorer_data;
			unsigned int index;
		};

		auto SelectModule = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SelectModuleData* data = (SelectModuleData*)_data;
			data->explorer_data->selected_module = data->index;
			ChangeInspectorToModule(data->explorer_data->editor_state, data->index);
		};

		SelectModuleData select_data;
		select_data.index = index;
		select_data.explorer_data = explorer_data;

		config.flag_count = 0;
		
		float combo_scale = drawer.ComboBoxDefaultSize(combo_labels, { }, "Configuration: ");
		UIConfigWindowDependentSize main_button_size;
		float main_button_x_scale = drawer.GetXScaleUntilBorder(drawer.current_x) - build_label_scale - clean_label_scale - rebuild_label_scale - combo_scale
			- 4 * drawer.layout.element_indentation;
		main_button_size.scale_factor.x = drawer.GetWindowSizeFactors(main_button_size.type, { main_button_x_scale, 0.0f }).x;
		config.AddFlag(main_button_size);

		UIConfigTextAlignment alignment;
		alignment.horizontal = ECS_UI_ALIGN_LEFT;
		alignment.vertical = ECS_UI_ALIGN_MIDDLE;
		config.AddFlag(alignment);

		UIConfigTextParameters text_parameters;
		text_parameters.size = drawer.GetFontSize();
		text_parameters.character_spacing = drawer.font.character_spacing;
		text_parameters.color = MODULE_COLORS[explorer_data->configurations[index]];
		config.AddFlag(text_parameters);

		size_t SELECT_MODULE_CONFIGURATION = index == explorer_data->selected_module ? MAIN_BUTTON_CONFIGURATION : MAIN_BUTTON_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT;
		drawer.Button(SELECT_MODULE_CONFIGURATION, config, ascii_module_name.buffer, { SelectModule, &select_data, sizeof(select_data) });

		config.flag_count = 0;

		drawer.PushIdentifierStack(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
		drawer.PushIdentifierStackRandom(index);

		ModuleExplorerRunModuleBuildCommandData build_data;
		build_data.editor_state = editor_state;
		build_data.module_index = index;
		build_data.configuration = explorer_data->configurations[index];

		UIConfigActiveState active_state;
		active_state.state = !IsModuleInfoLocked(editor_state, index, explorer_data->configurations[index]);
		config.AddFlag(active_state);
		drawer.Button(SECONDARY_BUTTON_CONFIGURATION, config, HEADER_BUILD_MODULE, { ModuleExplorerBuildModule, &build_data, sizeof(build_data) });
		drawer.Button(SECONDARY_BUTTON_CONFIGURATION, config, HEADER_CLEAN_MODULE, { ModuleExplorerCleanModule, &build_data, sizeof(build_data) });
		drawer.Button(SECONDARY_BUTTON_CONFIGURATION,config, HEADER_REBUILD_MODULE, { ModuleExplorerRebuildModule, &build_data, sizeof(build_data) });
		config.flag_count--;

		UIConfigComboBoxPrefix prefix;
		prefix.prefix = "Configuration: ";
		config.AddFlag(prefix);

		unsigned char* configuration_ptr = (unsigned char*)&explorer_data->configurations[index];

		drawer.ComboBox(
			COMBO_CONFIGURATION,
			config,
			"Configuration Combo",
			combo_labels,
			combo_labels.size,
			configuration_ptr
		);
		
		drawer.PopIdentifierStack();
		drawer.PopIdentifierStack();

		drawer.NextRow();
		config.flag_count = 0;
	};

#pragma endregion

	for (size_t index = 0; index < project_modules->size; index++) {
		EditorModuleInfo* info = GetModuleInfo(editor_state, index, explorer_data->configurations[index]);
		module_draw_base(info->load_status, index);
	}

}

// --------------------------------------------------------------------------------------------------------

void CreateModuleExplorer(EditorState* editor_state) {
	CreateDockspaceFromWindow(MODULE_EXPLORER_WINDOW_NAME, editor_state, CreateModuleExplorerWindow);
}

// --------------------------------------------------------------------------------------------------------

void CreateModuleExplorerAction(ActionData* action_data) {
	CreateModuleExplorer((EditorState*)action_data->data);
}

// --------------------------------------------------------------------------------------------------------

unsigned int CreateModuleExplorerWindow(EditorState* editor_state) {
	return CreateDefaultWindow(MODULE_EXPLORER_WINDOW_NAME, editor_state, { 0.6f, 0.7f }, ModuleExplorerSetDescriptor);
}

// --------------------------------------------------------------------------------------------------------
