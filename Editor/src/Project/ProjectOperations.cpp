#include "editorpch.h"
#include "ProjectOperations.h"
#include "..\Editor\EditorParameters.h"
#include "..\UI\ToolbarUI.h"
#include "..\UI\MiscellaneousBar.h"
#include "..\UI\Game.h"
#include "..\UI\DirectoryExplorer.h"
#include "..\UI\FileExplorer.h"
#include "..\Editor\EditorState.h"
#include "..\UI\Hub.h"
#include "..\Editor\EditorEvent.h"
#include "..\UI\ModuleExplorer.h"
#include "ProjectUITemplate.h"
#include "..\Modules\ModuleFile.h"
#include "..\Modules\Module.h"
#include "..\Modules\ModuleConfigurationGroup.h"

using namespace ECSEngine;
ECS_TOOLS;
ECS_CONTAINERS;

constexpr float2 CREATE_PROJECT_WIZARD_SCALE = float2(0.75f, 0.65f);
constexpr size_t SAVE_PROJECT_AUTOMATICALLY_TICK = 1000;

struct SaveCurrentProjectConfirmationData {
	EditorState* editor_state;
	UIActionHandler handler;
};

// -------------------------------------------------------------------------------------------------------------------

void CreateProjectMisc(ProjectOperationData* data) {
	EDITOR_STATE(data->editor_state);

	ECS_TEMP_STRING(new_console_dump_stream, 512);

	new_console_dump_stream.AddStreamSafe(data->file_data->path);
	new_console_dump_stream.AddStreamSafe(ToStream(CONSOLE_RELATIVE_DUMP_PATH));
	console->ChangeDumpPath(new_console_dump_stream);

	wchar_t hub_characters[256];
	CapacityStream<wchar_t> hub_path(hub_characters, 0, 256);
	hub_path.Copy(data->file_data->path);
	hub_path.Add(ECS_OS_PATH_SEPARATOR);
	hub_path.AddStream(data->file_data->project_name);
	hub_path.AddStreamSafe(ToStream(PROJECT_EXTENSION));
	AddHubProject(data->editor_state, hub_path);

	data->editor_state->editor_tick = EditorStateProjectTick;

	SaveProjectUIAutomaticallyData save_automatically_data;
	save_automatically_data.editor_state = data->editor_state;;
	save_automatically_data.timer = Timer();
	save_automatically_data.timer.SetMarker();
	//ui_system->PushFrameHandler({ SaveProjectUIAutomatically, &save_automatically_data, sizeof(save_automatically_data) });
}

// -------------------------------------------------------------------------------------------------------------------

void CreateProjectAuxiliaryDirectories(ProjectOperationData* data) {
	EDITOR_STATE(data->editor_state);

	wchar_t temp_characters[512];
	CapacityStream<wchar_t> new_directory_path(temp_characters, 0, 512);

	new_directory_path.Copy(data->file_data->path);
	new_directory_path.AddSafe(ECS_OS_PATH_SEPARATOR);

	size_t predirectory_size = new_directory_path.size;
	for (size_t index = 0; index < std::size(PROJECT_DIRECTORIES); index++) {
		new_directory_path.AddStreamSafe(ToStream(PROJECT_DIRECTORIES[index]));

		bool success = CreateFolder(new_directory_path);
		if (!success) {
			if (!ExistsFileOrFolder(new_directory_path)) {
				if (data->error_message.buffer != nullptr) {
					data->error_message.size = function::FormatString(data->error_message.buffer, "Creating project auxilary directory {0} failed!", new_directory_path);
					data->error_message.AssertCapacity();
				}

				CreateErrorMessageWindow(ui_system, data->error_message);
				return;
			}
			else {
				ECS_TEMP_ASCII_STRING(description, 256);
				description.size = function::FormatString(description.buffer, "Folder {0} already exists. Do you want to keep it or clean it?", PROJECT_DIRECTORIES[index]);
				description.AssertCapacity();
				ChooseOptionWindowData choose_data;

				const char* button_names[2] = { "Keep", "Clean" };
				choose_data.button_names = button_names;
				choose_data.description = description;

				UIActionHandler handlers[2] = { {SkipAction, nullptr, 0}, {DeleteFolderContentsAction, &new_directory_path, sizeof(new_directory_path)} };
				choose_data.handlers = { handlers, 2 };

				unsigned int window_index = CreateChooseOptionWindow(ui_system, choose_data);

				auto destroy_action = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					ProjectOperationData* data = (ProjectOperationData*)_data;
					ReleaseLockedWindow(action_data);
					CreateProjectAuxiliaryDirectories(data);
				};

				UIActionHandler destroy_handler;
				destroy_handler.action = destroy_action;
				destroy_handler.data = data;
				destroy_handler.data_size = sizeof(*data);
				ui_system->SetWindowDestroyAction(window_index, destroy_handler);

				return;
			}
		}
		new_directory_path.size = predirectory_size;
	}

	ECS_TEMP_STRING(default_template, 256);
	default_template.Copy(ToStream(EDITOR_DEFAULT_PROJECT_UI_TEMPLATE));
	default_template.AddStreamSafe(ToStream(PROJECT_UI_TEMPLATE_EXTENSION));
	ProjectOperationData temp_data = *data;
	// data will be invalidated by the template load since it will release all the memory
	// that the windows hold so a copy of the data must be done before entering
	if (ExistsFileOrFolder(default_template)) {
		CapacityStream<char> error_message = { nullptr, 0, 0 };
		LoadProjectUITemplate(data->editor_state, { default_template }, error_message);
	}
	else {
		CreateProjectDefaultUI(data->editor_state);
	}
	CreateProjectMisc(&temp_data);
}

// -------------------------------------------------------------------------------------------------------------------

void CreateProject(ProjectOperationData* data)
{
	EDITOR_STATE(data->editor_state);

	if (ExistsProjectInFolder(data->file_data)) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.size = function::FormatString(error_message.buffer, "A project in {0} already exists. Do you want to overwrite it?", data->file_data->path);
		if (data->error_message.buffer != nullptr) {
			data->error_message.Copy(error_message);
		}

		struct DeleteData {
			ProjectOperationData* data;
			Stream<const wchar_t*> project_directories;
		};

		auto delete_project = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			DeleteData* data = (DeleteData*)_data;
			ECS_TEMP_STRING(temp_string, 256);

			const wchar_t* extension_[1] = { PROJECT_EXTENSION };
			Stream<const wchar_t*> extension(extension_, 1);

			bool success = true;

			struct ForEachData {
				bool* success;
				EditorState* editor_state;
			};

			auto functor = [](const wchar_t* path, void* _data) {
				ForEachData* data = (ForEachData*)_data;
				Stream<wchar_t> ecs_path = ToStream(path);
				*data->success &= RemoveFile(ecs_path);
				RemoveHubProject(data->editor_state, ecs_path);

				return true;
			};

			ForEachData for_each_data = { &success, data->data->editor_state };
			ForEachFileInDirectoryWithExtension(data->data->file_data->path, extension, &for_each_data, functor);

			GetProjectFilePath(temp_string, data->data->file_data);
			wchar_t temp_project_name[256];
			temp_string.CopyTo(temp_project_name);
			Path project_path(temp_project_name, temp_string.size);

			if (ExistsFileOrFolder(temp_string)) {
				success &= RemoveFile(temp_string);
			}
			for (size_t index = 0; index < data->project_directories.size; index++) {
				temp_string.size = data->data->file_data->path.size + 1;
				temp_string.AddStreamSafe(ToStream(data->project_directories[index]));
				if (ExistsFileOrFolder(temp_string)) {
					success &= RemoveFolder(temp_string);
				}
			}

			if (!success) {
				ECS_TEMP_ASCII_STRING(error_message, 256);
				error_message.size = function::FormatString(error_message.buffer, "Overwriting project {0} failed.", project_path);
				error_message.AssertCapacity();
				CreateErrorMessageWindow(system, error_message);
			}

			CreateProject(data->data);
		};

		DeleteData delete_data;
		delete_data.data = data;
		delete_data.project_directories = Stream<const wchar_t*>(PROJECT_DIRECTORIES, std::size(PROJECT_DIRECTORIES));

		UIActionHandler handler;
		handler.action = delete_project;
		handler.data = &delete_data;
		handler.data_size = sizeof(delete_data);
		handler.phase = UIDrawPhase::System;
		
		CreateConfirmWindow(ui_system, error_message, handler);
		return;
	}

	bool success = SaveProjectFile(*data);
	if (!success) {
		if (data->error_message.buffer != nullptr) {
			CreateErrorMessageWindow(ui_system, data->error_message);
		}
		else {
			ECS_TEMP_ASCII_STRING(error_message, 256);
			error_message.size = function::FormatString(error_message.buffer, "Error when creating project {0}.", data->file_data->path);
			error_message.AssertCapacity();
			CreateErrorMessageWindow(ui_system, error_message);
		}
		return;
	}

	CreateProjectAuxiliaryDirectories(data);
}

// -------------------------------------------------------------------------------------------------------------------

void CreateProjectAction(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	ProjectOperationData* data = (ProjectOperationData*)_data;
	CreateProject(data);
}

// -------------------------------------------------------------------------------------------------------------------

bool CheckProjectDirectoryIntegrity(const ProjectFile* project) {
	Stream<wchar_t> required_folders_buffer[std::size(PROJECT_DIRECTORIES)];
	Stream<Stream<wchar_t>> required_folders(required_folders_buffer, std::size(PROJECT_DIRECTORIES));
	for (size_t index = 0; index < required_folders.size; index++) {
		required_folders[index] = ToStream(PROJECT_DIRECTORIES[index]);
	}

	auto functor = [](const wchar_t* path, void* data) {
		Stream<Stream<wchar_t>>* required_folders = (Stream<Stream<wchar_t>>*)data;
		unsigned int index = function::FindString(ToStream(path), *required_folders);
		if (index != -1) {
			required_folders->RemoveSwapBack(index);
		}
		return required_folders->size > 0;
	};

	ForEachDirectory(project->path, &required_folders, functor);
	return required_folders.size > 0;
}

// -------------------------------------------------------------------------------------------------------------------

bool CheckProjectIntegrity(const ProjectFile* project)
{
	return CheckProjectDirectoryIntegrity(project);
}

// -------------------------------------------------------------------------------------------------------------------

void DeallocateCurrentProject(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	editor_state->project_file = nullptr;
}

// -------------------------------------------------------------------------------------------------------------------

bool ExistsProjectInFolder(const ProjectFile* project_file) {
	return IsFileWithExtension(project_file->path, ToStream(PROJECT_EXTENSION));
}
// -------------------------------------------------------------------------------------------------------------------

void GetProjectCurrentUI(wchar_t* characters, const ProjectFile* project_file, size_t max_character_count) {
	GetProjectCurrentUI(CapacityStream<wchar_t>(characters, 0, max_character_count), project_file);
}

// -------------------------------------------------------------------------------------------------------------------

void GetProjectCurrentUI(CapacityStream<wchar_t>& characters, const ProjectFile* project_file) {
	characters.Copy(project_file->path);
	characters.Add(ECS_OS_PATH_SEPARATOR);
	characters.AddStreamSafe(ToStream(PROJECT_CURRENT_UI_TEMPLATE));
}

// -------------------------------------------------------------------------------------------------------------------

void CreateProjectWizardAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateProjectWizardData* wizard_data = (CreateProjectWizardData*)_data;
	CreateProjectWizard(system, wizard_data);
}

// -------------------------------------------------------------------------------------------------------------------

void CreateProjectDefaultValues(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ProjectFile* data = (ProjectFile*)_data;
}

// -------------------------------------------------------------------------------------------------------------------

void CreateProjectWizardDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	CreateProjectWizardData* data = (CreateProjectWizardData*)window_data;
	EDITOR_STATE(data->project_data.editor_state);
	EditorState* editor_state = data->project_data.editor_state;
	ProjectFile* file_data = editor_state->project_file;

	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	transform.scale.x *= 5.0f;
	config.AddFlag(transform);

	CapacityStream<char>* project_name_stream;
	CapacityStream<char>* directory_stream;
	CapacityStream<char>* resource_folder_stream;
	CapacityStream<char>* source_dll_name_stream;
	const wchar_t* project_name_wide;
	OSFileExplorerGetDirectoryActionData* get_directory_data;
	
	if (initialize) {
		constexpr size_t project_name_stream_capacity = 128;
		constexpr size_t directory_stream_capacity = 256;
		constexpr size_t source_dll_directory_capacity = 64;
		constexpr size_t get_directory_path_capacity = 256;
		constexpr size_t project_data_error_message_capacity = 256;

		project_name_stream = drawer.GetMainAllocatorBufferAndStoreAsResource<CapacityStream<char>>("Name input stream buffer");
		directory_stream = drawer.GetMainAllocatorBufferAndStoreAsResource<CapacityStream<char>>("Directory input stream buffer");
		resource_folder_stream = drawer.GetMainAllocatorBufferAndStoreAsResource<CapacityStream<char>>("Resource folder stream");
		get_directory_data = drawer.GetMainAllocatorBufferAndStoreAsResource<OSFileExplorerGetDirectoryActionData>("Directory data");
		source_dll_name_stream = drawer.GetMainAllocatorBufferAndStoreAsResource<CapacityStream<char>>("Source dll stream");

		memcpy(&data->project_data.file_data->version_description, VERSION_DESCRIPTION, std::size(VERSION_DESCRIPTION));
		memcpy(&data->project_data.file_data->platform_description, ECS_PLATFORM_STRINGS[ECS_PLATFORM_WIN64_DX11_STRING_INDEX], strlen(ECS_PLATFORM_STRINGS[ECS_PLATFORM_WIN64_DX11_STRING_INDEX]) + 1);
		data->project_data.file_data->version = VERSION_INDEX;
		data->project_data.file_data->platform = ECS_PLATFORM_WIN64_DX11;

		project_name_stream->InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(char) * project_name_stream_capacity), 0, project_name_stream_capacity - 1);
		directory_stream->InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(char) * directory_stream_capacity), 0, directory_stream_capacity - 1);
		source_dll_name_stream->InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(char) * source_dll_directory_capacity), 0, source_dll_directory_capacity - 1);

		get_directory_data->get_directory_data.path.InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(wchar_t) * get_directory_path_capacity), 0, get_directory_path_capacity);
		get_directory_data->get_directory_data.error_message.InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(char) * get_directory_path_capacity), 0, get_directory_path_capacity);
		get_directory_data->update_stream = &data->project_data.file_data->path;

		data->project_data.error_message.InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(char) * project_data_error_message_capacity), 0, project_data_error_message_capacity);
		memset((void*)file_data->project_name.buffer, 0, sizeof(wchar_t) * file_data->project_name.capacity);
		memset((void*)data->project_data.error_message.buffer, 0, sizeof(char) * project_data_error_message_capacity);
		memset(project_name_stream->buffer, 0, sizeof(char) * project_name_stream_capacity);
		memset(directory_stream->buffer, 0, sizeof(char) * directory_stream_capacity);
		memset(source_dll_name_stream->buffer, 0, sizeof(char) * source_dll_directory_capacity);
	}
	else {
		project_name_stream = (CapacityStream<char>*)drawer.GetResource("Name input stream buffer");
		directory_stream = (CapacityStream<char>*)drawer.GetResource("Directory input stream buffer");
		resource_folder_stream = (CapacityStream<char>*)drawer.GetResource("Resource folder stream");
		source_dll_name_stream = (CapacityStream<char>*)drawer.GetResource("Source dll stream");

		get_directory_data = (OSFileExplorerGetDirectoryActionData*)drawer.GetResource("Directory data");
	}

	UIConfigTextInputCallback name_callback;
	UIConvertASCIIToWideStreamData convert_data;
	convert_data.ascii = project_name_stream->buffer;
	convert_data.wide = &file_data->project_name;
	name_callback.handler = { ConvertASCIIToWideStreamAction, &convert_data, sizeof(convert_data) };

	config.AddFlag(name_callback);

	UIDrawerTextInput* project_input = drawer.TextInput(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_TEXT_INPUT_CALLBACK, config, "Project name", project_name_stream);
	drawer.NextRow();
	config.flag_count--;
	
	UIDrawerTextInput* input = drawer.TextInput(UI_CONFIG_RELATIVE_TRANSFORM, config, "Directory", directory_stream);
	get_directory_data->input = input;
	drawer.SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_FOLDER);

	float2 folder_position = drawer.GetLastSpriteRectanglePosition();
	float2 folder_scale = drawer.GetLastSpriteRectangleScale();
	drawer.AddDefaultClickableHoverable(folder_position, folder_scale, { FileExplorerGetDirectoryAction, get_directory_data, 0 });

	drawer.NextRow();

	convert_data.ascii = source_dll_name_stream->buffer;
	convert_data.wide = &file_data->source_dll_name;
	config.AddFlag(name_callback);
	UIDrawerTextInput* source_dll_input = drawer.TextInput(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_TEXT_INPUT_CALLBACK, config, "Source dll name", source_dll_name_stream);
	config.flag_count--;
	
	if (!initialize) {
		float2 last_position = drawer.GetLastSolidColorRectanglePosition();
		float2 last_scale = drawer.GetLastSolidColorRectangleScale();;

		if (drawer.HasClicked(last_position, last_scale)) {
			data->copy_project_name_to_source_dll = false;
		}
		if (source_dll_input->text->size == 0 && project_input->text->size == 0) {
			data->copy_project_name_to_source_dll = true;
		}

		if (data->copy_project_name_to_source_dll) {
			source_dll_input->DeleteAllCharacters();
			source_dll_input->InsertCharacters(project_input->text->buffer, project_input->text->size, 0, drawer.system);
			ECS_ASSERT(convert_data.wide->capacity >= project_input->text->size);
			function::ConvertASCIIToWide(convert_data.wide->buffer, project_input->text->buffer, project_input->text->size);
			convert_data.wide->size = project_input->text->size;
		}
	}

	drawer.NextRow();
	
	config.flag_count = 0;
	UIConfigAbsoluteTransform bottom_button_transform;
	bottom_button_transform.position = drawer.GetAlignedToBottom(drawer.GetElementDefaultScale().y);
	bottom_button_transform.scale = drawer.GetElementDefaultScale();
	config.AddFlag(bottom_button_transform);
	drawer.Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, "Create", { CreateProjectAction, &data->project_data, sizeof(data->project_data), UIDrawPhase::System });

	config.flag_count = 0;
	bottom_button_transform.position.x = drawer.GetAlignedToRight(drawer.GetElementDefaultScale().x).x;
	config.AddFlag(bottom_button_transform);

	drawer.Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X, config, "Cancel", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
}

// -------------------------------------------------------------------------------------------------------------------

void CreateProjectWizard(UISystem* system, CreateProjectWizardData* wizard_data) {
	EDITOR_STATE(wizard_data->project_data.editor_state);

	// launch wizard window
	UIWindowDescriptor window_descriptor;
	window_descriptor.draw = CreateProjectWizardDraw;

	window_descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, CREATE_PROJECT_WIZARD_SCALE.x);
	window_descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, CREATE_PROJECT_WIZARD_SCALE.y);
	window_descriptor.initial_size_x = CREATE_PROJECT_WIZARD_SCALE.x;
	window_descriptor.initial_size_y = CREATE_PROJECT_WIZARD_SCALE.y;

	window_descriptor.window_name = "Create Project Wizard";
	window_descriptor.window_data = wizard_data;
	window_descriptor.window_data_size = sizeof(CreateProjectWizardData);

	window_descriptor.destroy_action = ReleaseLockedWindow;

	unsigned int window_index = system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW 
		| UI_DOCKSPACE_LOCK_WINDOW);	
	//system->PopUpSystemHandler(window_index, "Create Project Wizard", false);
}

// -------------------------------------------------------------------------------------------------------------------

bool OpenProjectFile(ProjectOperationData data) {
	EDITOR_STATE(data.editor_state);

	ECS_TEMP_STRING(project_path, 256);
	GetProjectFilePath(project_path, data.file_data);

	const size_t TEMP_MEMORY_SIZE = 1024;
	size_t temp_memory[TEMP_MEMORY_SIZE];

	ProjectFile* file_data = data.file_data;
	CapacityStream<void> memory_pool = CapacityStream<void>(temp_memory, 0, sizeof(size_t) * TEMP_MEMORY_SIZE);
	
	// There are currently only 3 stream fields
	const size_t MAX_STREAM_FIELDS = 8;
	const char** _field_names_deserialized = (const char**)ECS_STACK_ALLOC(sizeof(const char*) * MAX_STREAM_FIELDS);
	CapacityStream<const char*> field_names_deserialized(_field_names_deserialized, 0, MAX_STREAM_FIELDS);

#define RETURN_ERROR_MESSAGE(string, ...) if (data.error_message.buffer != nullptr) { data.error_message.size = function::FormatString(data.error_message.buffer, string, __VA_ARGS__); data.error_message.AssertCapacity(); }\
											return false;

	// Set the file data to 0. If a field could not be deserialized, it will be 0
	memset(file_data, 0, sizeof(*file_data));
	bool success = true;
	ECS_TEXT_DESERIALIZE_STATUS status = TextDeserialize(ui_reflection->reflection->GetType(STRING(ProjectFile)), file_data, memory_pool, project_path);
	if (status == ECS_TEXT_DESERIALIZE_COULD_NOT_READ) {
		RETURN_ERROR_MESSAGE("Opening project file {0} failed; data could not be read (byte size might be too small)", project_path);
	}

	// If the version is different, fail
	if (file_data->version < COMPATIBLE_PROJECT_FILE_VERSION_INDEX) {
		RETURN_ERROR_MESSAGE("Opening project file {0} failed; version is {1} while compatible versions are above {2}.", project_path, file_data->version, COMPATIBLE_PROJECT_FILE_VERSION_INDEX);
	}

	if (status == ECS_TEXT_DESERIALIZE_FIELD_DATA_MISSING || status == ECS_TEXT_DESERIALIZE_FAILED_TO_READ_SOME_FIELDS) {
		// If the version description could not be deserialized, fail
		if (file_data->version_description[0] == 0) {
			RETURN_ERROR_MESSAGE("Opening project file {0} failed; version description is missing.", project_path);
		}
		if (file_data->platform_description[0] == 0) {
			RETURN_ERROR_MESSAGE("Opening project file {0} failed; platform description is missing.", project_path);
		}
		if (file_data->source_dll_name.buffer == nullptr) {
			RETURN_ERROR_MESSAGE("Opening project file {0} failed; source dll name is missing.", project_path);
		}
		if (file_data->project_name.buffer == nullptr) {
			RETURN_ERROR_MESSAGE("Opening project file {0} failed; project name is missing.", project_path);
		}
		if (file_data->path.buffer == nullptr) {
			RETURN_ERROR_MESSAGE("Opening project file {0} failed; the project's path is missing.", project_path);
		}
	}

	if (!function::HasFlag(file_data->platform, ECS_PLATFORM_WIN64_DX11)) {
		RETURN_ERROR_MESSAGE("Opening project file {0} failed, compatible platform {1}, actual platform {2}", project_path, ECS_PLATFORM_WIN64_DX11, file_data->platform);
	}

	// A coallesced allocation is used to copy the source dll name, the path and the name
	void* allocation = editor_allocator->Allocate(sizeof(wchar_t) * (file_data->path.size + file_data->source_dll_name.size + file_data->project_name.size + 3));
	wchar_t* strings = (wchar_t*)allocation;
	file_data->path.CopyTo(strings);
	file_data->path.buffer = strings;
	strings += file_data->path.size;
	strings[0] = L'\0';

	file_data->source_dll_name.CopyTo(strings);
	file_data->source_dll_name.buffer = strings;
	strings += file_data->source_dll_name.size;
	strings[0] = L'\0';

	file_data->project_name.CopyTo(strings);
	file_data->project_name.buffer = strings;
	strings += file_data->project_name.size;
	strings[0] = L'\0';

	return true;

#undef RETURN_ERROR_MESSAGE
}

// -------------------------------------------------------------------------------------------------------------------

void OpenProjectFileAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	OpenProjectFile(*(ProjectOperationData*)action_data->data);
}

// -------------------------------------------------------------------------------------------------------------------

bool OpenProject(ProjectOperationData data)
{
	EDITOR_STATE(data.editor_state);

	function::SetFlagAtomic(data.editor_state->flags, EDITOR_STATE_DO_NOT_ADD_TASKS);

	// Repair missing folders, if any
	RepairProjectAuxiliaryDirectories(data);

	if (data.editor_state->project_file != nullptr) {
		DeallocateCurrentProject(data.editor_state);
	}

	ECS_TEMP_STRING(ui_template_stream, 256);
	ui_template_stream.Copy(data.file_data->path);
	ui_template_stream.Add(ECS_OS_PATH_SEPARATOR);
	ui_template_stream.AddStreamSafe(ToStream(PROJECT_CURRENT_UI_TEMPLATE));

	ProjectUITemplate ui_template;
	ui_template.ui_file = ui_template_stream;

	CapacityStream<char> error_message = { nullptr, 0, 0 };
	data.editor_state->project_file = data.file_data;

	wchar_t console_dump_path_characters[256];
	Stream<wchar_t> console_dump_path(console_dump_path_characters, 0);
	console_dump_path.Copy(data.file_data->path);
	console_dump_path.AddStream(ToStream(CONSOLE_RELATIVE_DUMP_PATH));
	ECS_ASSERT(console_dump_path.size < 256);
	console->ChangeDumpPath(console_dump_path);

	if (ExistsFileOrFolder(ui_template_stream)) {
		bool success = LoadProjectUITemplate(data.editor_state, ui_template, error_message);
		if (!success) {
			return false;
		}
	}
	else {
		ECS_TEMP_STRING(default_template_path, 256);
		default_template_path.Copy(ToStream(EDITOR_DEFAULT_PROJECT_UI_TEMPLATE));
		default_template_path.AddStreamSafe(ToStream(PROJECT_UI_TEMPLATE_EXTENSION));

		if (ExistsFileOrFolder(default_template_path)) {
			bool success = LoadProjectUITemplate(data.editor_state, { default_template_path }, error_message);
			if (!success) {
				return false;
			}
		}
		else {
			CreateProjectDefaultUI(data.editor_state);
			ECS_TEMP_ASCII_STRING(error_message, 256);
			bool success = SaveProjectUITemplate(ui_system, ui_template, error_message);
			if (!success) {
				return false;
			}
		}
	}

	wchar_t _temp_chars[256];
	CapacityStream<wchar_t> module_path(_temp_chars, 0, 256);
	GetProjectModuleFilePath(data.editor_state, module_path);
	if (ExistsFileOrFolder(module_path)) {
		bool success = LoadModuleFile(data.editor_state);
		if (!success) {
			EditorSetConsoleError(data.editor_state, ToStream("An error occured during loading module file. No modules have been imported"));
		}
	}
	else {
		ResetProjectModules(data.editor_state);
		ResetProjectGraphicsModule(data.editor_state);
	}
	// Load the module configurations groups
	CapacityStream<wchar_t> module_configuration_group_path(_temp_chars, 0, 256);
	GetModuleConfigurationGroupFilePath(data.editor_state, module_configuration_group_path);
	if (ExistsFileOrFolder(module_configuration_group_path)) {
		bool success = LoadModuleConfigurationGroupFile(data.editor_state);
		if (!success) {
			EditorSetConsoleError(data.editor_state, ToStream("An error occured during module configuration group file load. No groups have been imported"));
		}
	}
	else {
		// Else reset the groups
		ResetModuleConfigurationGroups(data.editor_state);
	}

	// Delete all the auxiliary build files .build, .clean, .rebuild
	DeleteProjectModuleFlagFiles(data.editor_state);

	SaveProjectUIAutomaticallyData save_automatically_data;
	save_automatically_data.editor_state = data.editor_state;
	save_automatically_data.timer = Timer();
	save_automatically_data.timer.SetMarker();
	ui_system->PushFrameHandler({ SaveProjectUIAutomatically, &save_automatically_data, sizeof(save_automatically_data) });

	struct RemoveEditorStateDoNotAddTasksData {
		EditorState* editor_state;
		unsigned int frame_count;
	};

	RemoveEditorStateDoNotAddTasksData remove_data = { data.editor_state, 10 };

	// Push a system handler that will remove the EDITOR_STATE_DO_NOT_ADD_TASKS flag after 1 frame
	auto remove_editor_state_do_not_add_tasks = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		RemoveEditorStateDoNotAddTasksData* data = (RemoveEditorStateDoNotAddTasksData*)_data;
		if (data->frame_count == 0) {
			function::ClearFlagAtomic(data->editor_state->flags, EDITOR_STATE_DO_NOT_ADD_TASKS);
			system->PopFrameHandler();
		}
		else {
			data->frame_count--;
		}
	};
	ui_system->PushFrameHandler({ remove_editor_state_do_not_add_tasks, &remove_data, sizeof(remove_data) });

	data.editor_state->editor_tick = EditorStateProjectTick;

	return true;
}

// -------------------------------------------------------------------------------------------------------------------

void OpenProjectSystemHandlerAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ProjectOperationData* data = (ProjectOperationData*)_data;
	bool success = OpenProject(*data);
	if (!success) {
		system->PopFrameHandler();
		EditorSetConsoleError(data->editor_state, ToStream("Could not open project."));
	}
}

// -------------------------------------------------------------------------------------------------------------------

void OpenProjectAction(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	system->PushFrameHandler({ OpenProjectSystemHandlerAction, action_data->data, sizeof(ProjectOperationData) });
}

// -------------------------------------------------------------------------------------------------------------------

void RepairProjectAuxiliaryDirectories(ProjectOperationData data)
{
	ECS_TEMP_STRING(project_path, 256);
	project_path.Copy(data.file_data->path);
	project_path.Add(ECS_OS_PATH_SEPARATOR);
	
	unsigned int path_base_size = project_path.size;
	for (size_t index = 0; index < std::size(PROJECT_DIRECTORIES); index++) {
		project_path.AddStreamSafe(ToStream(PROJECT_DIRECTORIES[index]));
		if (!ExistsFileOrFolder(project_path)) {
			CreateFolder(project_path);
		}
		project_path.size = path_base_size;
	}
}

// -------------------------------------------------------------------------------------------------------------------

bool SaveProjectFile(ProjectOperationData data) {
	EDITOR_STATE(data.editor_state);

	wchar_t temp_characters[256];
	CapacityStream<wchar_t> project_path = CapacityStream<wchar_t>(temp_characters, 0, 256);
	GetProjectFilePath(project_path, data.file_data);

	bool success = TextSerialize(ui_reflection->reflection->GetType(STRING(ProjectFile)), data.file_data, project_path);
	
	if (!success) {
		if (data.error_message.buffer != nullptr) {
			data.error_message.size = function::FormatString(data.error_message.buffer, "Saving project file failed; writing to {0}", project_path.buffer);
			data.error_message.AssertCapacity();
		}
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------------------------------------------

void SaveProjectFileAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ProjectOperationData* data = (ProjectOperationData*)_data;
	bool success = SaveProjectFile(*data);

	if (!success) {
		if (data->error_message.buffer != nullptr) {
			CreateErrorMessageWindow(system, data->error_message);
		}
		else {
			ECS_TEMP_ASCII_STRING(error_message, 256);
			wchar_t project_name_characters[256];
			CapacityStream<wchar_t> project_name(project_name_characters, 0, 256);
			GetProjectFilePath(project_name, data->file_data);

			error_message.size = function::FormatString(error_message.buffer, "Saving project file {0} failed.", project_name);
			error_message.AssertCapacity();

			CreateErrorMessageWindow(system, error_message);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------

bool SaveProjectContents(ProjectOperationData data) {
	return true;
}

// -------------------------------------------------------------------------------------------------------------------

void SaveProjectContents(ActionData* action_data) {
	SaveProjectContents(*(ProjectOperationData*)action_data->data);
}

// -------------------------------------------------------------------------------------------------------------------

bool SaveProject(ProjectOperationData data)
{
	bool success = SaveProjectFile(data);
	if (!success) {
		return false;
	}

	success = SaveProjectUI(data);
	if (!success) {
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------------------------------------------

void SaveProjectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ProjectOperationData* data = (ProjectOperationData*)_data;
	bool success = SaveProject(*data);

	if (!success) {
		if (data->error_message.buffer != nullptr) {
			CreateErrorMessageWindow(system, data->error_message);
		}
		else {
			ECS_TEMP_ASCII_STRING(error_message, 256);
			wchar_t project_name_characters[256];
			CapacityStream<wchar_t> project_name(project_name_characters, 0, 256);
			GetProjectFilePath(project_name, data->file_data);
			
			error_message.size = function::FormatString(error_message.buffer, "Saving project {0} failed.", project_name);
			error_message.AssertCapacity();
			CreateErrorMessageWindow(system, error_message);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------

bool SaveCurrentProjectConverted(EditorState* _editor_state, bool (*function)(ProjectOperationData)) {
	EDITOR_STATE(_editor_state);

	ProjectOperationData data;
	data.editor_state = _editor_state;
	data.file_data = data.editor_state->project_file;
	data.error_message.buffer = nullptr;

	return function(data);
}

// -------------------------------------------------------------------------------------------------------------------

bool SaveCurrentProjectFile(EditorState* _editor_state)
{
	return SaveCurrentProjectConverted(_editor_state, SaveProjectFile);
}

// -------------------------------------------------------------------------------------------------------------------

void SaveCurrentProjectFileAction(ActionData* action_data) {
	SaveCurrentProjectFile((EditorState*)action_data->data);
}

// -------------------------------------------------------------------------------------------------------------------

bool SaveCurrentProject(EditorState* _editor_state) {
	return SaveCurrentProjectConverted(_editor_state, SaveProject);
}

// -------------------------------------------------------------------------------------------------------------------

void SaveCurrentProjectAction(ActionData* action_data) {
	SaveCurrentProject((EditorState*)action_data->data);
}

// -------------------------------------------------------------------------------------------------------------------

bool SaveCurrentProjectContents(EditorState* _editor_state) {
	return SaveCurrentProjectConverted(_editor_state, SaveProjectContents);
}

// -------------------------------------------------------------------------------------------------------------------

void SaveCurrentProjectContentsAction(ActionData* action_data) {
	SaveCurrentProjectContents((EditorState*)action_data->data);
}

// -------------------------------------------------------------------------------------------------------------------

void SaveCurrentProjectConfirmation(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SaveCurrentProjectConfirmationData* data = (SaveCurrentProjectConfirmationData*)_data;
	SaveCurrentProject(data->editor_state);
	action_data->data = data->handler.data;
	data->handler.action(action_data);
}

// -------------------------------------------------------------------------------------------------------------------

void SaveCurrentProjectWithConfirmation(EditorState* editor_state, Stream<char> description, UIActionHandler callback)
{
	EDITOR_STATE(editor_state);

	const char* button_names[] = { "Save", "Don't Save" };
	UIActionHandler handlers[2];
	ChooseOptionWindowData choose_data;
	choose_data.button_names = button_names;
	choose_data.description = description;
	choose_data.handlers.buffer = handlers;
	choose_data.handlers.size = 2;

	SaveCurrentProjectConfirmationData handler_data;
	handler_data.editor_state = editor_state;
	handler_data.handler = callback;
	handlers[0] = { SaveCurrentProjectAction, &handler_data, sizeof(handler_data) };
	handlers[1] = callback;

	CreateChooseOptionWindow(ui_system, choose_data);
}

// -------------------------------------------------------------------------------------------------------------------

void ConsoleSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = ConsoleWindowDraw;

	EDITOR_STATE(editor_state);

	ConsoleWindowData* window_data = (ConsoleWindowData*)stack_memory;
	CreateConsoleWindowData(*window_data, (Console*)editor_state->console);

	descriptor.window_data = window_data;
	descriptor.window_data_size = sizeof(*window_data);
	descriptor.window_name = CONSOLE_WINDOW_NAME;
}

// -------------------------------------------------------------------------------------------------------------------

void SaveProjectThreadTask(unsigned int thread_index, World* world, void* data) {
	EDITOR_STATE(data);
	EditorState* editor_state = (EditorState*)data;
	ProjectFile* project_file = editor_state->project_file;

	ECS_TEMP_STRING(template_path, 256);
	template_path.Copy(project_file->path);
	template_path.Add(ECS_OS_PATH_SEPARATOR);
	template_path.AddStreamSafe(ToStream(PROJECT_CURRENT_UI_TEMPLATE));

	CapacityStream<char> error_message = { nullptr, 0, 0 };
	bool success = SaveProjectUITemplate(ui_system, { template_path }, error_message);
	if (!success) {
		Console* console = (Console*)editor_state->console;
		console->Error("Automatic project UI save failed.");
	}
}

// -------------------------------------------------------------------------------------------------------------------

void SaveProjectUIAutomatically(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	SaveProjectUIAutomaticallyData* data = (SaveProjectUIAutomaticallyData*)_data;
	if (data->timer.GetDurationSinceMarker_ms() > SAVE_PROJECT_AUTOMATICALLY_TICK) {
		EDITOR_STATE(data->editor_state);
		ThreadTask task = { SaveProjectThreadTask, data->editor_state, 0 };
		task_manager->AddDynamicTaskAndWake(task);
		data->timer.SetMarker();
	}
}