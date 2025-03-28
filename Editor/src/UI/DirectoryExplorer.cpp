#include "editorpch.h"
#include "DirectoryExplorer.h"
#include "../Editor/EditorState.h"
#include "FileExplorerData.h"
#include "HelperWindows.h"

#include "ECSEngineOS.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr size_t LINEAR_ALLOCATOR_SIZE = 10'000;
constexpr size_t POINTER_CAPACITY = 256;
constexpr size_t RIGHT_CLICK_ROW_COUNT = 5;
constexpr const char* RIGHT_CLICK_LEFT_CHARACTERS = "Show in Explorer\nCreate\nDelete\nRename\nCopy Path";

constexpr size_t LAZY_EVALUATION_MILLISECONDS_TICK = 500;

constexpr const wchar_t* PROTECTED_FOLDERS[] = {
	L"Assets"
};

struct DirectoryExplorerData {
	CapacityStream<wchar_t>* current_path;
	LinearAllocator allocator;
	EditorState* editor_state;
	CapacityStream<UIActionHandler> right_click_menu_handlers;
	bool* right_click_menu_has_submenu;
	UIDrawerMenuState* right_click_submenu_states;
	UIDrawerFilesystemHierarchy* drawer_hierarchy;
	CapacityStream<Stream<char>> directories_ptrs;
	bool is_right_click_window_opened;
};

bool IsProtectedFolderSelected(DirectoryExplorerData* data) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, folder, 512);

	EditorState* editor_state = (EditorState*)data->editor_state;
	ProjectFile* project_file = editor_state->project_file;
	folder.CopyOther(project_file->path);
	folder.AddAssert(ECS_OS_PATH_SEPARATOR);
	size_t folder_base_size = folder.size;
	
	for (size_t index = 0; index < std::size(PROTECTED_FOLDERS); index++) {
		folder.size = folder_base_size;
		folder.AddStreamSafe(PROTECTED_FOLDERS[index]);

		bool is_the_same = folder == *data->current_path;
		if (is_the_same) {
			return true;
		}
	}
	return false;
}

void DirectoryExplorerHierarchySelectableCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	const char* label = (const char*)_additional_data;
	DirectoryExplorerData* data = (DirectoryExplorerData*)_data;
	EditorState* editor_state = (EditorState*)data->editor_state;

	Stream<char> label_stream(label, strlen(label));
	ProjectFile* project_file = editor_state->project_file;
	data->current_path->CopyOther(project_file->path);
	data->current_path->Add(ECS_OS_PATH_SEPARATOR);

	ECS_STACK_CAPACITY_STREAM(wchar_t, temp_characters, 256);
	ConvertASCIIToWide(temp_characters, label_stream);
	data->current_path->AddStreamAssert(temp_characters);
	ChangeFileExplorerDirectory(data->editor_state, *data->current_path);

}

void DirectoryExplorerRightClick(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	if (mouse->IsReleased(ECS_MOUSE_RIGHT)) {
		UIDrawerFilesystemHierarchyUserRightClickData* right_click_data = (UIDrawerFilesystemHierarchyUserRightClickData*)_data;
		DirectoryExplorerData* data = (DirectoryExplorerData*)right_click_data->data;

		UIDrawerMenuRightClickData menu_call_data;
		menu_call_data.name = "Menu";
		menu_call_data.window_index = window_index;
		menu_call_data.state.left_characters = (char*)RIGHT_CLICK_LEFT_CHARACTERS;
		menu_call_data.state.row_count = RIGHT_CLICK_ROW_COUNT;
		menu_call_data.state.row_has_submenu = nullptr;
		menu_call_data.state.submenues = nullptr;
		menu_call_data.state.submenu_index = 0;
		menu_call_data.state.unavailables = nullptr;
		menu_call_data.state.click_handlers = data->right_click_menu_handlers.buffer;

		action_data->data = &menu_call_data;
		RightClickMenu(action_data);
	}
}

void DirectoryExplorerCreateFolderCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ChooseDirectoryOrFileNameData* choose_data = (ChooseDirectoryOrFileNameData*)_additional_data;
	DirectoryExplorerData* data = (DirectoryExplorerData*)_data;

	data->current_path->Add(ECS_OS_PATH_SEPARATOR);
	ConvertASCIIToWide(*data->current_path, *choose_data->ascii);

	ECS_STACK_CAPACITY_STREAM(char, ascii, 512);
	ConvertWideCharsToASCII(*data->current_path, ascii);
	OS::CreateFolderWithError(*data->current_path, system);

	EditorState* editor_state = (EditorState*)data->editor_state;
	ProjectFile* project_file = editor_state->project_file;
	ascii.size = data->current_path->size - project_file->path.size - 1;
	ascii.buffer += project_file->path.size + 1;
	data->drawer_hierarchy->active_label.CopyOther(ascii);
}

void DirectoryExplorerCreateFolder(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	if (IsClickableTrigger(action_data)) {
		DirectoryExplorerData* data = (DirectoryExplorerData*)_data;
		ChooseDirectoryOrFileNameData choose_data;
		choose_data.callback.action = DirectoryExplorerCreateFolderCallback;
		choose_data.callback.data = data;
		choose_data.callback.data_size = 0;
		choose_data.callback.phase = ECS_UI_DRAW_NORMAL;

		CreateChooseDirectoryOrFileNameDockspace(system, choose_data);
	}
}

void DirectoryExplorerDeleteFolder(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	if (IsClickableTrigger(action_data)) {
		DirectoryExplorerData* data = (DirectoryExplorerData*)_data;
		if (!IsProtectedFolderSelected(data)) {
			OS::DeleteFolderWithError(*data->current_path, system);
			data->current_path->size = 0;
		}
		else {
			ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
			size_t parent_size = PathParentSize(*data->current_path);
			CapacityStream<wchar_t> folder_name(data->current_path->buffer + parent_size + 1, data->current_path->size - parent_size - 1, data->current_path->size - parent_size - 1 );
			FormatString(error_message, "Cannot delete {#} folder.", folder_name);
			CreateErrorMessageWindow(system, error_message);
		}
	}
}

void DirectoryExplorerRenameFolderCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ChooseDirectoryOrFileNameData* choose_data = (ChooseDirectoryOrFileNameData*)_additional_data;
	DirectoryExplorerData* data = (DirectoryExplorerData*)_data;

	system->m_memory->Deallocate(choose_data->wide);
	unsigned int directory_explorer_index = system->GetWindowFromName(DIRECTORY_EXPLORER_WINDOW_NAME);
	system->RemoveWindowMemoryResource(directory_explorer_index, choose_data->wide);

	OS::RenameFolderWithError(*data->current_path, *choose_data->wide, system);

	data->current_path->CopyOther(*data->current_path);
	data->current_path->AddStream(*choose_data->wide);
	data->current_path->buffer[data->current_path->size] = L'\0';
	data->current_path->AssertCapacity();

	ECS_STACK_CAPACITY_STREAM(char, ascii, 512);
	ConvertWideCharsToASCII(*data->current_path, ascii);

	EditorState* editor_state = (EditorState*)data->editor_state;
	ProjectFile* project_file = editor_state->project_file;
	ascii.size = data->current_path->size - project_file->path.size - 1;
	ascii.buffer += project_file->path.size + 1;
	data->drawer_hierarchy->active_label.CopyOther(ascii);
}

void DirectoryExplorerRenameFolder(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	if (IsClickableTrigger(action_data)) {
		DirectoryExplorerData* data = (DirectoryExplorerData*)_data;
		
		// If it is not the Assets folder, allow renaming
		if (!IsProtectedFolderSelected(data)) {

			ChooseDirectoryOrFileNameData choose_data;

			constexpr size_t max_character_count = 128;
			void* allocation = system->m_memory->Allocate(sizeof(CapacityStream<wchar_t>) + sizeof(wchar_t) * max_character_count);

			unsigned int directory_explorer_index = system->GetWindowFromName(DIRECTORY_EXPLORER_WINDOW_NAME);
			ECS_ASSERT(directory_explorer_index != -1);
			system->AddWindowMemoryResource(allocation, directory_explorer_index);
			uintptr_t ptr = (uintptr_t)allocation;
			choose_data.wide = (CapacityStream<wchar_t>*)ptr;
			ptr += sizeof(CapacityStream<wchar_t>);

			choose_data.wide->InitializeFromBuffer(ptr, 0, max_character_count);

			choose_data.callback.action = DirectoryExplorerRenameFolderCallback;
			choose_data.callback.data = data;
			choose_data.callback.data_size = 0;
			choose_data.callback.phase = ECS_UI_DRAW_PHASE::ECS_UI_DRAW_NORMAL;

			CreateChooseDirectoryOrFileNameDockspace(system, choose_data);
		}
		// else display error message: Cannot rename Assets folder
		else {
			CreateErrorMessageWindow(system, "Cannot rename Assets folder!");
		}
	}
}

void DirectoryExplorerLaunchFileExplorer(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	DirectoryExplorerData* data = (DirectoryExplorerData*)_data;

	Stream<wchar_t> path(*data->current_path);
	action_data->data = &path;
	LaunchFileExplorerStreamAction(action_data);
}

void DirectoryExplorerCopyPath(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	DirectoryExplorerData* data = (DirectoryExplorerData*)_data;

	Stream<wchar_t> path(*data->current_path);
	action_data->data = &path;
	CopyPathToClipboardStreamAction(action_data);
}

void DirectoryExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	DirectoryExplorerData* data = (DirectoryExplorerData*)window_data;

	const size_t HIERARCHY_CONFIGURATION = UI_CONFIG_FILESYSTEM_HIERARCHY_SPRITE_TEXTURE | UI_CONFIG_FILESYSTEM_HIERARCHY_SELECTABLE_CALLBACK
		| UI_CONFIG_FILESYSTEM_HIERARCHY_RIGHT_CLICK | UI_CONFIG_DO_CACHE;

	if (initialize) {
		drawer.layout.next_row_y_offset *= 0.5f;
		EditorState* editor_state = data->editor_state;
		
		AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();

		FileExplorerData* file_explorer_data = (FileExplorerData*)editor_state->file_explorer_data;
		data->current_path = &file_explorer_data->current_directory;
		data->directories_ptrs.Initialize(editor_allocator, 0, POINTER_CAPACITY);
		data->directories_ptrs.size = 0;

		data->allocator = LinearAllocator::InitializeFrom(editor_allocator, LINEAR_ALLOCATOR_SIZE);

		size_t total_right_click_menu_size = (sizeof(UIActionHandler) + sizeof(bool)) * RIGHT_CLICK_ROW_COUNT + sizeof(UIDrawerMenuState);
		void* right_click_allocation = Allocate(editor_allocator, total_right_click_menu_size);
		uintptr_t right_click_ptr = (uintptr_t)right_click_allocation;
		data->right_click_menu_handlers.InitializeFromBuffer(right_click_ptr, RIGHT_CLICK_ROW_COUNT, RIGHT_CLICK_ROW_COUNT);

		// Used for initialization during development stage; should be removed after finalization
		for (size_t index = 0; index < data->right_click_menu_handlers.size; index++) {
			data->right_click_menu_handlers[index] = { SkipAction, nullptr, 0 };
		}

		data->right_click_menu_handlers[0] = { DirectoryExplorerLaunchFileExplorer, data, 0 };
		data->right_click_menu_handlers[1] = { DirectoryExplorerCreateFolder, data, 0, ECS_UI_DRAW_SYSTEM };
		data->right_click_menu_handlers[2] = { DirectoryExplorerDeleteFolder, data, 0, ECS_UI_DRAW_SYSTEM };
		data->right_click_menu_handlers[3] = { DirectoryExplorerRenameFolder, data, 0, ECS_UI_DRAW_SYSTEM };
		data->right_click_menu_handlers[4] = { DirectoryExplorerCopyPath, data, 0 };

		data->right_click_menu_has_submenu = (bool*)right_click_ptr;
		right_click_ptr += sizeof(bool) * RIGHT_CLICK_ROW_COUNT;

		data->right_click_submenu_states = (UIDrawerMenuState*)right_click_ptr;
		data->right_click_submenu_states->click_handlers = nullptr;
		data->right_click_submenu_states->left_characters = "Script";
		data->right_click_submenu_states->row_count = 1;
		data->right_click_submenu_states->row_has_submenu = nullptr;
		data->right_click_submenu_states->submenues = nullptr;
		data->right_click_submenu_states->submenu_index = 1;
		data->right_click_submenu_states->unavailables = nullptr;
		
		UIDrawConfig config;

		UIConfigFilesystemHierarchySpriteTexture folder_texture;
		folder_texture.closed_texture = ECS_TOOLS_UI_TEXTURE_FOLDER;
		folder_texture.opened_texture = ECS_TOOLS_UI_TEXTURE_FOLDER;
		config.AddFlag(folder_texture);

		UIConfigFilesystemHierarchySelectableCallback selectable_callback;
		selectable_callback.callback = DirectoryExplorerHierarchySelectableCallback;
		selectable_callback.data = window_data;
		selectable_callback.data_size = 0;
		config.AddFlag(selectable_callback);

		UIConfigFilesystemHierarchyRightClick right_click;
		right_click.callback = DirectoryExplorerRightClick;
		right_click.data = data;
		right_click.data_size = 0;
		right_click.phase = ECS_UI_DRAW_SYSTEM;
		config.AddFlag(right_click);

		data->drawer_hierarchy = drawer.FilesystemHierarchy(HIERARCHY_CONFIGURATION, config, "Hierarchy", Stream<Stream<char>>(nullptr, 0));
		EditorStateLazyEvaluationTrigger(editor_state, EDITOR_LAZY_EVALUATION_DIRECTORY_EXPLORER);
	}

	EditorState* editor_state = data->editor_state;

	UIDrawConfig config;

	UIConfigFilesystemHierarchySpriteTexture folder_texture;
	folder_texture.closed_texture = ECS_TOOLS_UI_TEXTURE_FOLDER;
	folder_texture.opened_texture = ECS_TOOLS_UI_TEXTURE_FOLDER;
	config.AddFlag(folder_texture);

	UIConfigFilesystemHierarchySelectableCallback selectable_callback;
	selectable_callback.callback = DirectoryExplorerHierarchySelectableCallback;
	selectable_callback.data = window_data;
	selectable_callback.data_size = 0;
	config.AddFlag(selectable_callback);

	UIConfigFilesystemHierarchyRightClick right_click;
	right_click.callback = DirectoryExplorerRightClick;
	right_click.data = data;
	right_click.data_size = 0;
	right_click.phase = ECS_UI_DRAW_SYSTEM;
	config.AddFlag(right_click);

	if (!initialize) {
		data->drawer_hierarchy = drawer.FilesystemHierarchy(HIERARCHY_CONFIGURATION, config, "Hierarchy", data->directories_ptrs);
	}
}

void CreateDirectoryExplorer(EditorState* editor_state) {
	CreateDockspaceFromWindow(DIRECTORY_EXPLORER_WINDOW_NAME, editor_state, CreateDirectoryExplorerWindow);
}

void CreateDirectoryExplorerAction(ActionData* action_data) {
	CreateDirectoryExplorer((EditorState*)action_data->data);
}

void DirectoryExplorerDestroyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	DirectoryExplorerData* data = (DirectoryExplorerData*)_additional_data;
	Deallocate(data->editor_state->EditorAllocator(), data->right_click_menu_handlers.buffer);
}

void DirectoryExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
{
	descriptor.draw = DirectoryExplorerDraw;

	DirectoryExplorerData* data = stack_memory->Reserve<DirectoryExplorerData>();
	data->editor_state = editor_state;
	descriptor.window_name = DIRECTORY_EXPLORER_WINDOW_NAME;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);

	descriptor.destroy_action = DirectoryExplorerDestroyCallback;
}

unsigned int CreateDirectoryExplorerWindow(EditorState* editor_state) {
	constexpr float window_size_x = 0.8f;
	constexpr float window_size_y = 0.8f;
	float2 window_size = { window_size_x, window_size_y };

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, window_size.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, window_size.y);
	descriptor.initial_size_x = window_size.x;
	descriptor.initial_size_y = window_size.y;

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB);
	DirectoryExplorerSetDescriptor(descriptor, editor_state, &stack_memory);

	return editor_state->ui_system->Create_Window(descriptor);
}

void TickDirectoryExplorer(EditorState* editor_state)
{
	ProjectFile* project_file = editor_state->project_file;
	unsigned int window_index = editor_state->ui_system->GetWindowFromName(DIRECTORY_EXPLORER_WINDOW_NAME);
	if (window_index != -1) {
		DirectoryExplorerData* data = (DirectoryExplorerData*)editor_state->ui_system->GetWindowData(window_index);

		if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_DIRECTORY_EXPLORER, LAZY_EVALUATION_MILLISECONDS_TICK)) {
			data->allocator.Clear();
			data->directories_ptrs.size = 0;

			ECS_STACK_CAPACITY_STREAM(wchar_t, folder_path_wide_stream, 256);
			folder_path_wide_stream.AddStream(project_file->path);
			folder_path_wide_stream.Add(ECS_OS_PATH_SEPARATOR);
			size_t folder_path_wide_size_return = folder_path_wide_stream.size;

			for (size_t index = 0; index < std::size(PROTECTED_FOLDERS); index++) {
				folder_path_wide_stream.size = folder_path_wide_size_return;
				folder_path_wide_stream.AddStream(PROTECTED_FOLDERS[index]);
				folder_path_wide_stream[folder_path_wide_stream.size] = L'\0';

				Stream<char> folder_path_stream(data->allocator.Allocate(256 * sizeof(char), alignof(char)), folder_path_wide_stream.size);
				ConvertWideCharsToASCII(folder_path_wide_stream.buffer, folder_path_stream.buffer, folder_path_wide_stream.size + 1, 256);

				data->directories_ptrs.AddAssert(folder_path_stream.buffer + project_file->path.size + 1);

				struct ForEachData {
					DirectoryExplorerData* data;
					ProjectFile* project_file;
				};

				ForEachData for_each_data;
				for_each_data.data = data;
				for_each_data.project_file = project_file;

				ForEachDirectoryRecursive(folder_path_wide_stream, &for_each_data, [](Stream<wchar_t> path, void* _data) {
					ForEachData* for_each_data = (ForEachData*)_data;

					size_t current_path_size = path.size + 1;
					char* ascii_current_path = (char*)for_each_data->data->allocator.Allocate(sizeof(char) * current_path_size, alignof(char));
					ConvertWideCharsToASCII(path.buffer, ascii_current_path, current_path_size, current_path_size);
					for_each_data->data->directories_ptrs.AddAssert(ascii_current_path + for_each_data->project_file->path.size + 1);

					return true;
				}, true);

			}
		}

		// Copy the current directory if it changed from the directory selectable inside file explorer
		// And make the parent state true in order to have it appear here
		if (data->current_path->size > 0) {
			ECS_STACK_CAPACITY_STREAM(char, ascii_stream, 512);
			Path starting_path = *data->current_path;
			starting_path.buffer += project_file->path.size + 1;
			starting_path.size -= project_file->path.size + 1;
			ConvertWideCharsToASCII(starting_path, ascii_stream);

			data->drawer_hierarchy->active_label.CopyOther(ascii_stream);

			ASCIIPath parent_path = PathParent(ascii_stream);
			UIDrawerFilesystemHierarchyLabelData* label_data = data->drawer_hierarchy->label_states.TryGetValuePtr(parent_path);
			if (label_data != nullptr) {
				label_data->state = true;
			}
		}
	}
}
