#include "editorpch.h"
#include "ProjectUITemplate.h"
#include "..\Editor\EditorState.h"
#include "ProjectOperations.h"

#include "..\UI\DirectoryExplorer.h"
#include "..\UI\FileExplorer.h"
#include "..\UI\DirectoryExplorer.h"
#include "..\UI\ToolbarUI.h"
#include "..\UI\MiscellaneousBar.h"
#include "..\UI\Game.h"
#include "..\UI\ModuleExplorer.h"
#include "..\UI\Inspector.h"
#include "..\UI\NotificationBar.h"

using namespace ECSEngine;
ECS_CONTAINERS;
ECS_TOOLS;

void CreateProjectDefaultUI(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	ui_system->Clear();

	CreateToolbarUI(editor_state);
	CreateMiscellaneousBar(editor_state);
	CreateNotificationBar(editor_state);
	UIDockspace* main_dockspace = CreateProjectBackgroundDockspace(ui_system);
	unsigned int viewport_window_index = CreateGameWindow(editor_state);
	ui_system->AddWindowToDockspaceRegion(viewport_window_index, main_dockspace, 0);
	CreateDirectoryExplorer(editor_state);
	CreateFileExplorer(editor_state);
}

UIDockspace* CreateProjectBackgroundDockspace(UISystem* system)
{
	float border_size = system->m_descriptors.dockspaces.border_size;
	float2 dockspace_position = { -1.0f, -1.0f + TOOLBAR_SIZE_Y + MISCELLANEOUS_BAR_SIZE_Y - border_size };
	float2 dockspace_scale = { 2.0f - border_size, 1.0f - dockspace_position.y - NOTIFICATION_BAR_WINDOW_SIZE - border_size };
	unsigned int dockspace_index = system->CreateFixedDockspace({ dockspace_position, dockspace_scale }, DockspaceType::FloatingVertical, 0, false, UI_DOCKSPACE_BACKGROUND);
	UIDockspace* dockspace = system->GetDockspace(dockspace_index, DockspaceType::FloatingVertical);

	system->RemoveWindowFromDockspaceRegion<false>(dockspace, DockspaceType::FloatingVertical, 0, 0);
	return dockspace;
}

bool OpenProjectUI(ProjectOperationData data)
{
	EDITOR_STATE(data.editor_state);

	ECS_TEMP_STRING(template_path, 256);
	GetProjectCurrentUI(template_path, data.file_data);
	return LoadProjectUITemplate(data.editor_state, { template_path }, data.error_message);
}

void OpenProjectUIAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;
	OpenProjectUI(*(ProjectOperationData*)_data);
}

bool SaveProjectUI(ProjectOperationData data)
{
	EDITOR_STATE(data.editor_state);

	ECS_TEMP_STRING(template_path, 256);
	GetProjectCurrentUI(template_path, data.file_data);
	return SaveProjectUITemplate(ui_system, { template_path }, data.error_message);
}

void SaveProjectUIAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;
	SaveProjectUI(*(ProjectOperationData*)_data);
}

bool SaveCurrentProjectUI(EditorState* editor_state) {
	return SaveCurrentProjectConverted(editor_state, SaveProjectUI);
}

void SaveCurrentProjectUIAction(ActionData* action_data) {
	SaveCurrentProjectUI((EditorState*)action_data->data);
}

void InjectWindowSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = InjectValuesWindowDraw;

	descriptor.destroy_action = InjectWindowDestroyAction;
	descriptor.window_data = &editor_state->inject_data;
	descriptor.window_data_size = sizeof(editor_state->inject_data);

	descriptor.window_name = editor_state->inject_window_name;
}

bool LoadProjectUITemplate(EditorState* editor_state, ProjectUITemplate _template, CapacityStream<char>& error_message)
{
	EDITOR_STATE(editor_state);

	Stream<char> _file_window_names[64];
	Stream<Stream<char>> file_window_names(_file_window_names, 0);
	bool success = ui_system->LoadUIFile(_template.ui_file, file_window_names);
	if (!success) {
		if (error_message.buffer != nullptr) {
			error_message.size = function::FormatString(error_message.buffer, "Error when loading Project UI template: {0} does not exist or it is corrupted!", _template.ui_file);
			error_message.AssertCapacity();
		}
		return false;
	}
	else {
		Stream<char> _window_names[] = {
			ToStream(editor_state->inject_window_name),
			ToStream(TOOLBAR_WINDOW_NAME),
			ToStream(MISCELLANEOUS_BAR_WINDOW_NAME),
			ToStream(NOTIFICATION_BAR_WINDOW_NAME),
			ToStream(CONSOLE_WINDOW_NAME),
			ToStream(DIRECTORY_EXPLORER_WINDOW_NAME),
			ToStream(FILE_EXPLORER_WINDOW_NAME),
			ToStream(GAME_WINDOW_NAME),
			ToStream(MODULE_EXPLORER_WINDOW_NAME),
			ToStream(INSPECTOR_WINDOW_NAME)
		};
		using SetDescriptorFunction = void (*)(UIWindowDescriptor&, EditorState*, void*);

		Stream<Stream<char>> window_names(_window_names, std::size(_window_names));
		SetDescriptorFunction set_functions[] = {
			InjectWindowSetDescriptor,
			ToolbarSetDescriptor,
			MiscellaneousBarSetDescriptor,
			NotificationBarSetDescriptor,
			ConsoleSetDescriptor,
			DirectoryExplorerSetDescriptor,
			FileExplorerSetDescriptor,
			GameSetDecriptor,
			ModuleExplorerSetDescriptor,
			InspectorSetDescriptor
		};

		for (size_t index = 0; index < file_window_names.size; index++) {
			UIWindowDescriptor descriptor;
			unsigned int in_index = function::FindString(file_window_names[index], window_names);

			size_t stack_memory[128];
			if (in_index != (unsigned int)-1) {
				set_functions[in_index](descriptor, editor_state, stack_memory);
				ui_system->RestoreWindow(window_names[in_index], descriptor);

				set_functions[in_index] = set_functions[window_names.size - 1];
				window_names.RemoveSwapBack(in_index);
			}
		}
		ui_system->RemoveUnrestoredWindows();
	}

	SaveProjectUIAutomaticallyData save_data;
	save_data.editor_state = editor_state;
	save_data.timer.SetMarker();
	ui_system->PushFrameHandler({ SaveProjectUIAutomatically, &save_data, sizeof(save_data) });
	return true;
}

void LoadProjectUITemplateSystemHandler(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ECS_TEMP_ASCII_STRING(error_message, 256);
	LoadProjectUITemplateData* data = (LoadProjectUITemplateData*)_data;
	bool success = LoadProjectUITemplate(data->editor_state, data->ui_template, error_message);

	if (!success) {
		CreateErrorMessageWindow(system, error_message);
	}
}

void LoadProjectUITemplateAction(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	size_t stack_memory[256];
	LoadProjectUITemplateData* data = (LoadProjectUITemplateData*)_data;
	ECS_ASSERT(data->ui_template.ui_file.size < 500);
	system->AddFrameHandler({ LoadProjectUITemplateSystemHandler, stack_memory, (unsigned int)(sizeof(LoadProjectUITemplateData) + sizeof(wchar_t) * data->ui_template.ui_file.size) });
	LoadProjectUITemplateData* handler_data = (LoadProjectUITemplateData*)system->GetFrameHandlerData(system->GetFrameHandlerCount() - 1);
	handler_data->editor_state = data->editor_state;
	handler_data->ui_template.ui_file.InitializeFromBuffer(function::OffsetPointer(handler_data, sizeof(LoadProjectUITemplateData)), data->ui_template.ui_file.size, data->ui_template.ui_file.size);
	memcpy(handler_data->ui_template.ui_file.buffer, data->ui_template.ui_file.buffer, sizeof(wchar_t) * data->ui_template.ui_file.size);
}

bool SaveProjectUITemplate(UISystem* system, ProjectUITemplate _template, CapacityStream<char>& error_message) {
	if (_template.ui_file.size < _template.ui_file.capacity) {
		_template.ui_file[_template.ui_file.size] = L'\0';
	}
	else {
		ECS_TEMP_STRING(temp_string, 256);
		temp_string.Copy(_template.ui_file);
		temp_string.AddSafe(L'\0');
		_template.ui_file.buffer = temp_string.buffer;
	}

	return system->WriteUIFile(_template.ui_file.buffer, error_message);
}

void SaveProjectUITemplateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SaveProjectUITemplateData* data = (SaveProjectUITemplateData*)_data;
	ECS_TEMP_ASCII_STRING(error_message, 256);
	bool success = SaveProjectUITemplate(system, data->ui_template, error_message);

	if (!success) {
		CreateErrorMessageWindow(system, error_message);
	}
}