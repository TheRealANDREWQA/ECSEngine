#include "editorpch.h"
#include "Editor\EditorState.h"
#include "HelperWindows.h"

using namespace ECSEngine;
ECS_TOOLS;
ECS_CONTAINERS;

constexpr float2 CHOOSE_DIRECTORY_OR_FILE_NAME_SCALE = float2(0.6f, 0.25f);

constexpr const char* RENAME_FILE_WIZARD_NAME = "Rename File";
constexpr const char* RENAME_FILE_WIZARD_INPUT_NAME = "New File Name";

constexpr const char* RENAME_FOLDER_WIZARD_NAME = "Rename Folder";
constexpr const char* RENAME_FOLDER_WIZARD_INPUT_NAME = "New Folder Name";

void CreateDockspaceFromWindow(const char* window_name, EditorState* _editor_state, CreateWindowFunction function)
{
	EDITOR_STATE(_editor_state);

	unsigned int window_index = ui_system->GetWindowFromName(window_name);
	if (window_index == -1) {
		window_index = function(_editor_state);

		UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
		ui_system->CreateDockspace(window_transform, DockspaceType::FloatingVertical, window_index, false);
	}
	else {
		ui_system->SetActiveWindow(window_index);
	}
}

unsigned int CreateDefaultWindow(
	const char* window_name,
	EditorState* editor_state,
	float2 window_size,
	WindowDraw draw
)
{
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, window_size.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, window_size.y);
	descriptor.initial_size_x = window_size.x;
	descriptor.initial_size_y = window_size.y;

	descriptor.draw = draw;

	descriptor.window_name = window_name;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;

	return ui_system->Create_Window(descriptor);
}

void ChooseDirectoryOrFileNameButtonAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ChooseDirectoryOrFileNameData* data = (ChooseDirectoryOrFileNameData*)_data;
	if (data->callback.action != nullptr) {
		action_data->additional_data = data;
		action_data->data = data->callback.data;
		data->callback.action(action_data);
	}
	CloseXBorderClickableAction(action_data);
}

void ChooseDirectoryOrFileName(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	ChooseDirectoryOrFileNameData* data = (ChooseDirectoryOrFileNameData*)window_data;
	if (initialize) {
		if (data->ascii == nullptr) {
			data->ascii = drawer.GetMainAllocatorBuffer<CapacityStream<char>>();
			if (data->wide != nullptr) {
				void* allocation = drawer.GetMainAllocatorBuffer(sizeof(char) * data->wide->capacity, alignof(char));
				data->ascii->InitializeFromBuffer(allocation, data->wide->size, data->wide->capacity);
				if (data->wide->size > 0) {
					function::ConvertWideCharsToASCII(data->wide->buffer, data->ascii->buffer, data->wide->size, data->ascii->capacity);
				}
			}
			else {
				void* allocation = drawer.GetMainAllocatorBuffer(sizeof(char) * 256, alignof(char));
				data->ascii->InitializeFromBuffer(allocation, 0, 256);
			}
		}
		if (data->callback.data_size > 0) {
			void* allocation = drawer.GetMainAllocatorBuffer(data->callback.data_size);
			memcpy(allocation, data->callback.data, data->callback.data_size);
			data->callback.data = allocation;
		}
	}

	UIDrawConfig config;

	UIConfigTextInputCallback callback;

	if (data->wide != nullptr) {
		UIConvertASCIIToWideStreamData callback_data;
		callback_data.wide = data->wide;
		callback_data.ascii = data->ascii->buffer;
		callback.handler = { ConvertASCIIToWideStreamAction, &callback_data, sizeof(callback_data) };
	}
	else {
		callback.handler = { SkipAction, nullptr, 0 };
	}
	config.AddFlag(callback);

	UIConfigTextInputHint hint;
	hint.characters = "Enter a name";
	config.AddFlag(hint);

	UIConfigWindowDependentSize size;
	size.type = WindowSizeTransformType::Horizontal;
	size.scale_factor.x = 1.0f;
	config.AddFlag(size);

	drawer.TextInput(UI_CONFIG_TEXT_INPUT_CALLBACK | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_NO_NAME, 
		config, "Name", data->ascii);
	drawer.NextRow();

	config.flag_count = 0;
	UIConfigAbsoluteTransform absolute_transform;

	float2 label_size = drawer.GetLabelScale("Finish");
	absolute_transform.position = drawer.GetAlignedToBottom(label_size.y);
	absolute_transform.scale = label_size;

	config.AddFlag(absolute_transform);
	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Finish", { ChooseDirectoryOrFileNameButtonAction, data, 0, UIDrawPhase::System });
	config.flag_count--;
	

	absolute_transform.scale = drawer.GetLabelScale("Cancel");
	absolute_transform.position.x = drawer.GetAlignedToRight(absolute_transform.scale.x).x;
	config.AddFlag(absolute_transform);
	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM,config, "Cancel", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
}

unsigned int CreateChooseDirectoryOrFileNameDockspace(UISystem* system, ChooseDirectoryOrFileNameData data)
{
	UIWindowDescriptor descriptor;

	descriptor.initial_size_x = CHOOSE_DIRECTORY_OR_FILE_NAME_SCALE.x;
	descriptor.initial_size_y = CHOOSE_DIRECTORY_OR_FILE_NAME_SCALE.y;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, CHOOSE_DIRECTORY_OR_FILE_NAME_SCALE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, CHOOSE_DIRECTORY_OR_FILE_NAME_SCALE.y);

	descriptor.window_name = "Choose Name";
	descriptor.draw = ChooseDirectoryOrFileName;

	descriptor.window_data = &data;
	descriptor.window_data_size = sizeof(data);

	descriptor.destroy_action = ReleaseLockedWindow;

	return system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_LOCK_WINDOW);
}

void RenameFileWizardCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	const wchar_t* path = (const wchar_t*)_data;
	CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;

	ECS_TEMP_STRING(wide_name, 256);
	function::ConvertASCIIToWide(wide_name, *new_name);

	RenameFileActionData rename_data;
	rename_data.new_name = wide_name;
	rename_data.path = ToStream(path);
	action_data->data = &rename_data;
	RenameFileAction(action_data);
}

unsigned int CreateRenameFileWizard(Stream<wchar_t> path, UISystem* system)
{
	TextInputWizardData wizard_data;
	wizard_data.input_name = RENAME_FILE_WIZARD_INPUT_NAME;
	wizard_data.window_name = RENAME_FILE_WIZARD_NAME;
	wizard_data.callback = RenameFileWizardCallback;
	
	ECS_TEMP_STRING(null_terminated_path, 512);
	null_terminated_path.Copy(path);
	null_terminated_path[path.size] = L'\0';
	wizard_data.callback_data = null_terminated_path.buffer;
	wizard_data.callback_data_size = sizeof(wchar_t) * (path.size + 1);

	return CreateTextInputWizard(&wizard_data, system);
}

void CreateRenameFileWizardAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
	CreateRenameFileWizard(*data, system);
}

void RenameFolderWizardCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	const wchar_t* path = (const wchar_t*)_data;
	CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;

	ECS_TEMP_STRING(wide_name, 256);
	function::ConvertASCIIToWide(wide_name, *new_name);

	RenameFolderActionData rename_data;
	rename_data.new_name = wide_name;
	rename_data.path = ToStream(path);
	action_data->data = &rename_data;
	RenameFolderAction(action_data);
}

unsigned int CreateRenameFolderWizard(Stream<wchar_t> path, UISystem* system) {
	TextInputWizardData wizard_data;
	wizard_data.input_name = RENAME_FOLDER_WIZARD_INPUT_NAME;
	wizard_data.window_name = RENAME_FOLDER_WIZARD_NAME;
	wizard_data.callback = RenameFolderWizardCallback;

	ECS_TEMP_STRING(null_terminated_path, 512);
	null_terminated_path.Copy(path);
	null_terminated_path[path.size] = L'\0';
	wizard_data.callback_data = null_terminated_path.buffer;
	wizard_data.callback_data_size = sizeof(wchar_t) * (path.size + 1);

	return CreateTextInputWizard(&wizard_data, system);
}

void CreateRenameFolderWizardAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
	CreateRenameFolderWizard(*data, system);
}
