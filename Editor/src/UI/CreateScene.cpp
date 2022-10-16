#include "editorpch.h"
#include "CreateScene.h"

#include "../Editor/EditorState.h"
#include "../Editor/EditorScene.h"
#include "../Project/ProjectFolders.h"

#define SAVE_SCENE_POP_UP_NAME "Save Scene"

using namespace ECSEngine;
ECS_TOOLS;

// ----------------------------------------------------------------------------------------------

struct SaveScenePopUpDrawData {
	EditorState* editor_state;
	UIActionHandler continue_handler;

	unsigned int sandbox_indices[16];
	unsigned int sandbox_index_count;

	// Mask that tells which sandbox scene should be saved
	bool save[16];
};

void SaveSceneAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SaveScenePopUpDrawData* data = (SaveScenePopUpDrawData*)_data;

	SaveScenePopUpResult results;
	results.count = data->sandbox_index_count;
	for (unsigned int index = 0; index < data->sandbox_index_count; index++) {
		if (data->save[index]) {
			unsigned int sandbox_index = data->sandbox_indices[index];
			bool success = SaveSandboxScene(data->editor_state, sandbox_index);
			results.statuses[index] = success ? SAVE_SCENE_POP_UP_SUCCESSFUL : SAVE_SCENE_POP_UP_FAILED;
		}
		results.sandbox_indices[index] = data->sandbox_indices[index];
	}
	results.cancel_call = false;
	action_data->additional_data = &results;

	action_data->data = data->continue_handler.data_size == 0 ? data->continue_handler.data : function::OffsetPointer(data, sizeof(*data));
	data->continue_handler.action(action_data);

	DestroyCurrentActionWindow(action_data);
}

// ----------------------------------------------------------------------------------------------

void DoNotSaveSceneAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SaveScenePopUpDrawData* data = (SaveScenePopUpDrawData*)_data;

	SaveScenePopUpResult results;
	memset(results.statuses, SAVE_SCENE_POP_UP_ABORTED, sizeof(SAVE_SCENE_POP_UP_STATUS) * data->sandbox_index_count);
	results.count = data->sandbox_index_count;
	for (unsigned int index = 0; index < data->sandbox_index_count; index++) {
		results.sandbox_indices[index] = data->sandbox_indices[index];
	}
	results.cancel_call = false;
	action_data->additional_data = &results;
	
	action_data->data = data->continue_handler.data_size == 0 ? data->continue_handler.data : function::OffsetPointer(data, sizeof(*data));
	data->continue_handler.action(action_data);

	DestroyCurrentActionWindow(action_data);
}

void CancelSceneAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SaveScenePopUpDrawData* data = (SaveScenePopUpDrawData*)_data;

	SaveScenePopUpResult results;
	results.cancel_call = true;
	action_data->additional_data = &results;

	action_data->data = data->continue_handler.data_size == 0 ? data->continue_handler.data : function::OffsetPointer(data, sizeof(*data));
	data->continue_handler.action(action_data);

	DestroyCurrentActionWindow(action_data);
}

// ----------------------------------------------------------------------------------------------

void SaveScenePopUpDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SaveScenePopUpDrawData* data = (SaveScenePopUpDrawData*)window_data;

	ECS_STACK_CAPACITY_STREAM(char, description, 512);

	UIDrawConfig config;

	if (initialize) {
		memset(data->save, 1, sizeof(bool) * data->sandbox_index_count);
	}

	if (data->sandbox_index_count == 1) {
		EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_indices[0]);
		ECS_FORMAT_STRING(description, "Scene {#} used in sandbox {#} is not saved. Do you want to save it?", sandbox->scene_path, data->sandbox_indices[0]);

		drawer.Text(description);
		drawer.NextRow();
	}
	else {
		drawer.Text("There are multiple scenes not saved. Do you want to save them?");
		drawer.NextRow();

		drawer.PushIdentifierStackStringPattern();
		for (unsigned int index = 0; index < data->sandbox_index_count; index++) {
			drawer.PushIdentifierStackRandom(index);

			UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
			row_layout.AddSquareLabel();
			row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });

			UIDrawConfig config;
			size_t check_box_configuration = UI_CONFIG_CHECK_BOX_NO_NAME;
			row_layout.GetTransform(config, check_box_configuration);

			drawer.CheckBox(check_box_configuration, config, "Checkbox", data->save + index);

			config.flag_count = 0;
			size_t label_configuration = UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_LABEL_TRANSPARENT;
			row_layout.GetTransform(config, label_configuration);
			
			EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_indices[index]);
			Stream<wchar_t> path_stem = function::PathStem(sandbox->scene_path);
			drawer.TextLabelWide(label_configuration, config, path_stem);

			drawer.NextRow();

			drawer.PopIdentifierStack();
		}
		drawer.PopIdentifierStack();
	}
	
	drawer.NextRow(2.0f);

	UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();

	row_layout.SetHorizontalAlignment(ECS_UI_ALIGN_MIDDLE);
	row_layout.SetVerticalAlignment(ECS_UI_ALIGN_BOTTOM);
	row_layout.AddLabel("Save");
	row_layout.AddLabel("Don't save");
	row_layout.AddLabel("Cancel");

	size_t button_configuration = 0;
	config.flag_count = 0;
	row_layout.GetTransform(config, button_configuration);

	drawer.Button(button_configuration, config, "Save", { SaveSceneAction, data, 0, ECS_UI_DRAW_SYSTEM });

	button_configuration = 0;
	config.flag_count = 0;
	row_layout.GetTransform(config, button_configuration);

	drawer.Button(button_configuration, config, "Don't save", { DoNotSaveSceneAction, data, 0, ECS_UI_DRAW_SYSTEM });

	button_configuration = 0;
	config.flag_count = 0;
	row_layout.GetTransform(config, button_configuration);
	drawer.Button(button_configuration, config, "Cancel", { CancelSceneAction, data, 0, ECS_UI_DRAW_SYSTEM });
}

void CreateSaveScenePopUp(EditorState* editor_state, Stream<unsigned int> sandbox_indices, UIActionHandler continue_handler)
{
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, initial_indices, sandbox_indices.size);
	initial_indices.Copy(sandbox_indices);

	// Go through the sandboxes and verify it they are dirty. If they are not dirty, then skip them
	for (int32_t index = 0; index < (int32_t)sandbox_indices.size; index++) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_indices[index]);
		if (!sandbox->is_scene_dirty) {
			sandbox_indices.RemoveSwapBack(index);
			index--;
		}
	}

	if (sandbox_indices.size > 0) {
		UIWindowDescriptor descriptor;

		descriptor.window_name = "Save Scene";

		size_t _draw_data[128];
		SaveScenePopUpDrawData* draw_data = (SaveScenePopUpDrawData*)_draw_data;
		draw_data->editor_state = editor_state;
		draw_data->sandbox_index_count = sandbox_indices.size;
		sandbox_indices.CopyTo(draw_data->sandbox_indices);
		draw_data->continue_handler = continue_handler;

		descriptor.draw = SaveScenePopUpDraw;
		descriptor.window_data = draw_data;
		descriptor.window_data_size = sizeof(*draw_data);

		if (continue_handler.data_size > 0) {
			memcpy(function::OffsetPointer(draw_data, sizeof(*draw_data)), continue_handler.data, continue_handler.data_size);
			descriptor.window_data_size += continue_handler.data_size;
		}

		descriptor.destroy_action = ReleaseLockedWindow;
		editor_state->ui_system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X);
	}
	else {
		// Call the continue_handler with all successful
		SaveScenePopUpResult result;
		result.cancel_call = false;
		result.count = initial_indices.size;
		memcpy(result.sandbox_indices, initial_indices.buffer, initial_indices.MemoryOf(initial_indices.size));
		memset(result.statuses, SAVE_SCENE_POP_UP_SUCCESSFUL, sizeof(SAVE_SCENE_POP_UP_STATUS) * initial_indices.size);

		// It shouldn't need the window index
		ActionData action_data = editor_state->ui_system->GetFilledActionData(0);
		action_data.data = continue_handler.data;
		action_data.additional_data = &result;

		continue_handler.action(&action_data);
	}
}

// ----------------------------------------------------------------------------------------------

void CreateEmptySceneActualCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	CapacityStream<char>* name = (CapacityStream<char>*)_additional_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	Stream<wchar_t> current_folder = editor_state->file_explorer_data->current_directory;
	current_folder = function::PathRelativeToAbsolute(current_folder, assets_folder);
	ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 512);
	wide_name.Copy(current_folder);
	function::ConvertASCIIToWide(wide_name, *name);
	wide_name.AddStreamSafe(EDITOR_SCENE_EXTENSION);

	if (ExistScene(editor_state, wide_name)) {
		CreateErrorMessageWindow(system, "The scene already exists");
	}
	else {
		if (!CreateEmptyScene(editor_state, wide_name)) {
			EditorSetConsoleError("Could not create new scene.");
		}
	}
}

// ----------------------------------------------------------------------------------------------

// This spawns a window that asks the user for the name
void CreateEmptySceneAction(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;

	TextInputWizardData wizard_data;
	wizard_data.callback = CreateEmptySceneActualCallback;
	wizard_data.callback_data = editor_state;
	wizard_data.callback_data_size = 0;
	wizard_data.input_name = "Scene name";
	wizard_data.window_name = "Choose a name";

	CreateTextInputWizard(&wizard_data, system);
}

// ----------------------------------------------------------------------------------------------

void SaveSandboxSceneHandler(ActionData*);

struct SaveSandboxSceneHandlerData {
	EditorState* editor_state;
};

// ----------------------------------------------------------------------------------------------

void ChangeSandboxSceneAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ChangeSandboxSceneActionData* data = (ChangeSandboxSceneActionData*)_data;

	EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_index);
	if (sandbox->is_scene_dirty) {
		// Spawn a window to ask the user if he wants to save the current scene before loading the new one
		// only if it is dirty.
		SaveSandboxSceneHandlerData handler_data;
		handler_data.editor_state = data->editor_state;
		CreateSaveScenePopUp(data->editor_state, { &data->sandbox_index, 1 }, { SaveSandboxSceneHandler, &handler_data, sizeof(handler_data) });
	}
	else {
		ECS_STACK_CAPACITY_STREAM(wchar_t, new_scene_path, 512);
		ECS_STACK_CAPACITY_STREAM(char, error_message, 512);

		OS::FileExplorerGetFileData get_file_data;
		Stream<wchar_t> extensions[] = {
			EDITOR_SCENE_EXTENSION
		};
		get_file_data.extensions = { extensions, std::size(extensions) };
		get_file_data.path = new_scene_path;
		get_file_data.error_message = error_message;

		bool success = OS::FileExplorerGetFile(&get_file_data);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(console_message, "Failed to get new scene path. Reason: {#}.", get_file_data.error_message);
			EditorSetConsoleError(console_message);
		}
		else {
			ECS_STACK_CAPACITY_STREAM(wchar_t, assets_directory, 512);
			GetProjectAssetsFolder(data->editor_state, assets_directory);

			// Can change the path now
			Stream<wchar_t> relative_path = function::PathRelativeToFilename(get_file_data.path, assets_directory);
			success = ChangeSandboxScenePath(data->editor_state, data->sandbox_index, relative_path);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(console_message, "Failed to change path to {#} for sandbox {#}. The scene is invalid or an internal error happened.", relative_path, data->sandbox_index);
				EditorSetConsoleError(console_message);
			}
			else {
				// Update the sandbox file
				// Already prints an error message
				SaveEditorSandboxFile(data->editor_state);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------

void SaveSandboxSceneHandler(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SaveSandboxSceneHandlerData* data = (SaveSandboxSceneHandlerData*)_data;
	SaveScenePopUpResult* statuses = (SaveScenePopUpResult*)_additional_data;

	auto call_change = [action_data, data, statuses](unsigned int index) {
		ChangeSandboxSceneActionData change_data;
		change_data.editor_state = data->editor_state;
		change_data.sandbox_index = statuses->sandbox_indices[index];

		action_data->data = &change_data;
		ChangeSandboxSceneAction(action_data);
	};

	if (!statuses->cancel_call) {
		for (unsigned int index = 0; index < statuses->count; index++) {
			EditorSandbox* sandbox = GetSandbox(data->editor_state, statuses->sandbox_indices[index]);

			if (statuses->statuses[index] == SAVE_SCENE_POP_UP_SUCCESSFUL) {
				call_change(index);
			}
			else if (statuses->statuses[index] == SAVE_SCENE_POP_UP_FAILED) {
				ECS_FORMAT_TEMP_STRING(console_message, "Failed to save scene {#} for sandbox {#}.", sandbox->scene_path, statuses->sandbox_indices[index]);
				EditorSetConsoleError(console_message);
			}
			else {
				// Aborted the save
				// Trick the change function by setting the is dirty flag to false such that it proceeds to change the path
				sandbox->is_scene_dirty = false;
				call_change(index);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------