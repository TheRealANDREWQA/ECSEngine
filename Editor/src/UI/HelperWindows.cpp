#include "editorpch.h"
#include "Editor\EditorState.h"
#include "HelperWindows.h"
#include "TargetSandboxWindows.h"

using namespace ECSEngine;
ECS_TOOLS;

constexpr float2 CHOOSE_DIRECTORY_OR_FILE_NAME_SCALE = float2(0.6f, 0.25f);

constexpr const char* RENAME_FILE_WIZARD_NAME = "Rename File";
constexpr const char* RENAME_FILE_WIZARD_INPUT_NAME = "New File Name";

constexpr const char* RENAME_FOLDER_WIZARD_NAME = "Rename Folder";
constexpr const char* RENAME_FOLDER_WIZARD_INPUT_NAME = "New Folder Name";

void CreateDockspaceFromWindow(const char* window_name, EditorState* editor_state, CreateWindowFunction function)
{
	UISystem* ui_system = editor_state->ui_system;

	unsigned int window_index = ui_system->GetWindowFromName(window_name);
	if (window_index == -1) {
		window_index = function(editor_state);

		UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
		// Alternate the types such that they don't get overused
		DockspaceType dockspace_type = window_index % 2 == 0 ? DockspaceType::FloatingHorizontal : DockspaceType::FloatingVertical;
		ui_system->CreateDockspace(window_transform, dockspace_type, window_index, false);
	}
	else {
		ui_system->SetActiveWindow(window_index);
	}
}

void CreateDockspaceFromWindowWithIndex(const char* window_name, EditorState* editor_state, unsigned int index, CreateWindowFunctionWithIndex function)
{
	UISystem* ui_system = editor_state->ui_system;

	// Construct the name as well
	ECS_STACK_CAPACITY_STREAM(char, constructed_name, 128);
	constructed_name.CopyOther(window_name);
	ConvertIntToChars(constructed_name, index);

	unsigned int window_index = ui_system->GetWindowFromName(constructed_name);
	if (window_index == -1) {
		window_index = function(editor_state, index);

		UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
		// Alternate the types such that they don't get overused
		DockspaceType dockspace_type = window_index % 2 == 0 ? DockspaceType::FloatingHorizontal : DockspaceType::FloatingVertical;
		ui_system->CreateDockspace(window_transform, dockspace_type, window_index, false);
	}
	else {
		ui_system->SetActiveWindow(window_index);
	}
}

unsigned int CreateDefaultWindow(
	const char* window_name,
	EditorState* editor_state,
	float2 window_size,
	void (*set_descriptor)(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
)
{
	size_t stack_memory[512];

	UIWindowDescriptor descriptor;
	descriptor.Center(window_size);

	set_descriptor(descriptor, editor_state, stack_memory);

	return editor_state->ui_system->Create_Window(descriptor);
}

unsigned int CreateDefaultWindowWithIndex(
	const char* window_name,
	EditorState* editor_state,
	float2 window_size,
	unsigned int index,
	void (*set_descriptor)(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
) {
	size_t stack_memory[512];

	UIWindowDescriptor descriptor;
	descriptor.Center(window_size);

	unsigned int* stack_index = (unsigned int*)stack_memory;
	*stack_index = index;

	set_descriptor(descriptor, editor_state, stack_memory);

	return editor_state->ui_system->Create_Window(descriptor);
}

unsigned int UpdateUIWindowIndex(EditorState* editor_state, const char* base_name, unsigned int old_index, unsigned int new_index) {
	ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
	window_name.CopyOther(base_name);
	unsigned int base_name_size = window_name.size;
	ConvertIntToChars(window_name, old_index);

	unsigned int window_index = editor_state->ui_system->GetWindowFromName(window_name);
	if (window_index != -1) {
		window_name.size = base_name_size;
		ConvertIntToChars(window_name, new_index);
		editor_state->ui_system->SetWindowName(window_index, window_name);
	}
	return window_index;
}

unsigned int GetWindowNameIndex(Stream<char> name)
{
	return ConvertCharactersToInt(name);
}

unsigned int GetWindowNameIndex(const UIDrawer& drawer)
{
	return GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
}

void ChooseDirectoryOrFileNameButtonAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ChooseDirectoryOrFileNameData* data = (ChooseDirectoryOrFileNameData*)_data;
	if (data->callback.action != nullptr) {
		action_data->additional_data = data;
		action_data->data = data->callback.data;
		data->callback.action(action_data);
	}
	DestroyCurrentActionWindow(action_data);
}

void ChooseDirectoryOrFileName(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	ChooseDirectoryOrFileNameData* data = (ChooseDirectoryOrFileNameData*)window_data;
	if (initialize) {
		if (data->ascii == nullptr) {
			data->ascii = drawer.GetMainAllocatorBuffer<CapacityStream<char>>();
			if (data->wide != nullptr) {
				void* allocation = drawer.GetMainAllocatorBuffer(sizeof(char) * data->wide->capacity, alignof(char));
				data->ascii->InitializeFromBuffer(allocation, data->wide->size, data->wide->capacity);
				if (data->wide->size > 0) {
					ConvertWideCharsToASCII(data->wide->buffer, data->ascii->buffer, data->wide->size, data->ascii->capacity);
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
	size.type = ECS_UI_WINDOW_DEPENDENT_SIZE::ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
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
	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Finish", { ChooseDirectoryOrFileNameButtonAction, data, 0, ECS_UI_DRAW_SYSTEM });
	config.flag_count--;
	

	absolute_transform.scale = drawer.GetLabelScale("Cancel");
	absolute_transform.position.x = drawer.GetAlignedToRight(absolute_transform.scale.x).x;
	config.AddFlag(absolute_transform);
	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM,config, "Cancel", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
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

struct RenameData {
	Stream<wchar_t> GetPath() const {
		return { OffsetPointer(this, sizeof(*this)), path_size };
	}

	void* GetCallbackData() const {
		return callback.data_size > 0 ? OffsetPointer(this, sizeof(*this) + path_size * sizeof(wchar_t)) : callback.data;
	}

	void WritePath(Stream<wchar_t> path) {
		path.CopyTo(OffsetPointer(this, sizeof(*this)));
		path_size = path.size;
	}
	
	// Returns the total size needed for the copy
	unsigned int WriteCallback(UIActionHandler _callback) {
		callback = _callback;
		if (_callback.data_size > 0) {
			memcpy(OffsetPointer(this, sizeof(*this) + path_size * sizeof(wchar_t)), _callback.data, _callback.data_size);
		}
		return sizeof(*this) + path_size * sizeof(wchar_t) + _callback.data_size;
	}
	
	bool is_file;
	unsigned int path_size;
	UIActionHandler callback;
};

void RenameWizardCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RenameData* data = (RenameData*)_data;
	CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 256);
	ConvertASCIIToWide(wide_name, *new_name);

	Stream<wchar_t> path = data->GetPath();
	RenameWizardAdditionalData additional_data = { false, path, *new_name, wide_name };
	if (data->callback.action != nullptr) {
		action_data->data = data->GetCallbackData();
		action_data->additional_data = &additional_data;
		data->callback.action(action_data);
	}

	if (!additional_data.prevent_default_renaming) {
		if (data->is_file) {
			RenameFileActionData rename_data;
			rename_data.new_name = wide_name;
			rename_data.path = path;
			action_data->data = &rename_data;
			RenameFileAction(action_data);
		}
		else {
			RenameFolderActionData rename_data;
			rename_data.new_name = wide_name;
			rename_data.path = path;
			action_data->data = &rename_data;
			RenameFolderAction(action_data);
		}
	}
}

static unsigned int CreateRenameWizard(
	Stream<wchar_t> path,
	UISystem* system,
	UIActionHandler user_callback,
	const char* input_name,
	const char* window_name,
	bool is_file
) {
	size_t wizard_callback_data_storage[128];
	RenameData* rename_data = (RenameData*)wizard_callback_data_storage;
	rename_data->WritePath(path);
	unsigned int write_size = rename_data->WriteCallback(user_callback);
	ECS_ASSERT(write_size <= sizeof(wizard_callback_data_storage));
	rename_data->is_file = is_file;

	Stream<wchar_t> filename = PathFilename(path);
	ECS_STACK_CAPACITY_STREAM(char, ascii_filename, 512);
	ConvertWideCharsToASCII(filename, ascii_filename);

	TextInputWizardData wizard_data;
	wizard_data.input_name = input_name;
	wizard_data.window_name = window_name;
	wizard_data.callback = RenameWizardCallback;
	wizard_data.callback_data = rename_data;
	wizard_data.callback_data_size = write_size;
	wizard_data.SetInitialCharacters(ascii_filename);

	return CreateTextInputWizard(&wizard_data, system);
}

unsigned int CreateRenameFileWizard(Stream<wchar_t> path, UISystem* system, UIActionHandler user_callback)
{
	return CreateRenameWizard(path, system, user_callback, RENAME_FILE_WIZARD_INPUT_NAME, RENAME_FILE_WIZARD_NAME, true);
}

void RenameFolderWizardCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RenameData* data = (RenameData*)_data;
	CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 256);
	ConvertASCIIToWide(wide_name, *new_name);

	Stream<wchar_t> path = data->GetPath();
	RenameWizardAdditionalData additional_data = { false, path, *new_name, wide_name };
	if (data->callback.action != nullptr) {
		action_data->data = data->GetCallbackData();
		action_data->additional_data = &additional_data;
		data->callback.action(action_data);
	}

	if (!additional_data.prevent_default_renaming) {
		RenameFolderActionData rename_data;
		rename_data.new_name = wide_name;
		rename_data.path = path;
		action_data->data = &rename_data;
		RenameFolderAction(action_data);
	}
}

unsigned int CreateRenameFolderWizard(Stream<wchar_t> path, UISystem* system, UIActionHandler user_callback) {
	return CreateRenameWizard(path, system, user_callback, RENAME_FOLDER_WIZARD_INPUT_NAME, RENAME_FOLDER_WIZARD_NAME, false);
}

unsigned int GetActiveWindowSandbox(const EditorState* editor_state)
{
	unsigned int active_window = editor_state->ui_system->GetActiveWindow();
	
	unsigned int target_sandbox = SceneUITargetSandbox(editor_state, active_window);
	if (target_sandbox != -1) {
		return target_sandbox;
	}

	target_sandbox = GameUITargetSandbox(editor_state, active_window);
	if (target_sandbox != -1) {
		return target_sandbox;
	}

	target_sandbox = GetInspectorTargetSandboxFromUIWindow(editor_state, active_window);
	if (target_sandbox != -1) {
		return target_sandbox;
	}

	target_sandbox = EntitiesUITargetSandbox(editor_state, active_window);
	return target_sandbox;
}
