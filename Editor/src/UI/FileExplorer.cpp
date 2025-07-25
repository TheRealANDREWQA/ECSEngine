#include "editorpch.h"
#include "ECSEngineRendering.h"
#include "ECSEngineRenderingCompression.h"
#include "ECSEngineAssets.h"
#include "ECSEngineDebugDrawer.h"
#include "FileExplorer.h"
#include "FileExplorerData.h"
#include "../Editor/EditorState.h"
#include "HelperWindows.h"
#include "PrefabSceneDrag.h"
#include "../Project/ProjectOperations.h"
#include "../Project/ProjectRenameFile.h"
#include "Inspector.h"
#include "../Editor/EditorPalette.h"
#include "../Editor/EditorEvent.h"
#include "DragTargets.h"

#include "CreateScene.h"
#include "../Assets/AssetManagement.h"
#include "../Assets/AssetExtensions.h"
#include "AssetIcons.h"
#include "../Sandbox/SandboxRecordingFileExtension.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr float2 WINDOW_SIZE = float2(1.0f, 0.6f);
constexpr size_t DOUBLE_CLICK_DURATION = 350;
constexpr size_t ALLOCATOR_CAPACITY = 10'000;
constexpr float THUMBNAIL_SIZE = 0.115f;
constexpr float THUMBNAIL_TO_LABEL_SPACING = 0.014f;
constexpr float TOOLTIP_OFFSET = 0.02f;
constexpr size_t FILE_FUNCTORS_CAPACITY = 32;

constexpr size_t FILE_EXPLORER_RESET_SELECTED_FREE_LIMIT = 64;
constexpr size_t FILE_EXPLORER_RESET_COPIED_FREE_LIMIT = 64;

constexpr size_t FILE_RIGHT_CLICK_ROW_COUNT = 7;
char* FILE_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path\nCut\nCopy";

constexpr size_t FOLDER_RIGHT_CLICK_ROW_COUNT = 7;
char* FOLDER_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path\nCut\nCopy";

constexpr size_t MESH_FILE_RIGHT_CLICK_ROW_COUNT = 8;
char* MESH_FILE_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path\nCut\nCopy\nExport Materials";
bool MESH_FILE_HAS_SUBMENUES[MESH_FILE_RIGHT_CLICK_ROW_COUNT] = {
	false,
	false,
	false,
	false,
	false,
	false,
	false,
	true
};

constexpr size_t MESH_EXPORT_MATERIALS_CLICK_ROW_COUNT = 4;
char* MESH_EXPORT_MATERIALS_CLICK_CHARACTERS = "All Here\nAll To Folder\nSelection Here\nSelection To Folder";

constexpr size_t FILE_EXPLORER_DESELECTION_MENU_ROW_COUNT = 3;
char* FILE_EXPLORER_DESELECTION_MENU_CHARACTERS = "Create\nPaste\nReset Copied Files";
bool FILE_EXPLORER_DESELECTION_HAS_SUBMENUES[FILE_EXPLORER_DESELECTION_MENU_ROW_COUNT] = {
	true,
	false,
	false
};

constexpr size_t FILE_EXPLORER_DESELECTION_MENU_CREATE_ROW_COUNT = 6;
char* FILE_EXPLORER_DESELECTION_MENU_CREATE_CHARACTERS = "Folder\nScene\nSampler\nShader\nMaterial\nMisc";

constexpr size_t FILE_EXPLORER_CURRENT_DIRECTORY_CAPACITY = 256;
constexpr size_t FILE_EXPLORER_CURRENT_SELECTED_CAPACITY = 16;
constexpr size_t FILE_EXPLORER_COPIED_FILE_CAPACITY = 16;
constexpr size_t FILE_EXPLORER_FILTER_CAPACITY = 256;
constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_INITIAL_COUNT = 16;
constexpr size_t FILE_EXPLORER_STAGING_PRELOAD_TEXTURE_COUNT = 512;
constexpr size_t FILE_EXPLORER_MESH_THUMBNAILS_INITIAL_SIZE = 128;
constexpr size_t FILE_EXPLORER_RETAINED_MODE_ALLOCATOR_CAPACITY = ECS_KB * 64;

constexpr uint2 FILE_EXPLORER_MESH_THUMBNAIL_TEXTURE_SIZE = { 256, 256 };

constexpr unsigned int FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT = 1 << 0;
constexpr unsigned int FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES = 1 << 1;
constexpr unsigned int FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES = 1 << 2;
constexpr unsigned int FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES = 1 << 3;

constexpr unsigned int FILE_EXPLORER_FLAGS_PRELOAD_STARTED = 1 << 1;
constexpr unsigned int FILE_EXPLORER_FLAGS_PRELOAD_ENDED = 1 << 2;
constexpr unsigned int FILE_EXPLORER_FLAGS_PRELOAD_LAUNCHED_THREADS = 1 << 3;

constexpr int COLOR_CUT_ALPHA = 90;

constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_ALLOCATOR_SIZE_PER_THREAD = ECS_MB * 200;
constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_FALLBACK_SIZE = ECS_MB * 400;

constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_LAZY_EVALUATION = 1'500;
constexpr size_t FILE_EXPLORER_MESH_THUMBNAIL_LAZY_EVALUATION = 500;

constexpr size_t FILE_EXPLORER_RETAINED_MODE_FILE_RECHECK_LAZY_EVALUATION = 250;
constexpr size_t FILE_EXPLORER_DISPLAYED_ITEMS_ALLOCATOR_CAPACITY = ECS_KB * 32;

constexpr size_t FILE_EXPLORER_TEMPORARY_ALLOCATOR_CAPACITY = ECS_KB * 64;
constexpr size_t FILE_EXPLORER_ALLOCATOR_CAPACITY = ECS_KB * 400;

#define MAX_MESH_THUMBNAILS_PER_FRAME 2

enum FILE_RIGHT_CLICK_INDEX {
	FILE_RIGHT_CLICK_OPEN,
	FILE_RIGHT_CLICK_SHOW_IN_EXPLORER,
	FILE_RIGHT_CLICK_DELETE,
	FILE_RIGHT_CLICK_RENAME,
	FILE_RIGHT_CLICK_COPY_PATH,
	FILE_RIGHT_CLICK_CUT_SELECTION,
	FILE_RIGHT_CLICK_COPY_SELECTION
};

enum FOLDER_RIGHT_CLICK_INDEX {
	FOLDER_RIGHT_CLICK_OPEN,
	FOLDER_RIGHT_CLICK_SHOW_IN_EXPLORER,
	FOLDER_RIGHT_CLICK_DELETE,
	FOLDER_RIGHT_CLICK_RENAME,
	FOLDER_RIGHT_CLICK_COPY_PATH,
	FOLDER_RIGHT_CLICK_CUT_SELECTION,
	FOLDER_RIGHT_CLICK_COPY_SELECTION
};

enum DESELECTION_MENU_INDEX {
	DESELECTION_MENU_CREATE,
	DESELECTION_MENU_PASTE,
	DESELECTION_MENU_RESET_COPIED_FILES
};

enum DESELECTION_MENU_CREATE_INDEX {
	DESELECTION_MENU_CREATE_FOLDER,
	DESELECTION_MENU_CREATE_SCENE,
	DESELECTION_MENU_CREATE_SAMPLER,
	DESELECTION_MENU_CREATE_SHADER,
	DESELECTION_MENU_CREATE_MATERIAL,
	DESELECTION_MENU_CREATE_MISC
};

enum MESH_EXPORT_MATERIALS_INDEX : unsigned char {
	MESH_EXPORT_MATERIALS_ALL_HERE,
	MESH_EXPORT_MATERIALS_ALL_TO_FOLDER,
	MESH_EXPORT_MATERIALS_SELECTION_HERE,
	MESH_EXPORT_MATERIALS_SELECTION_TO_FOLDER
};

static void FileExplorerResetSelectedFiles(FileExplorerData* data) {
	// Deallocate every string stored
	for (size_t index = 0; index < data->selected_files.size; index++) {
		Deallocate(data->selected_files.allocator, data->selected_files[index].buffer);
	}

	// If it is a long list, deallocate the buffer
	if (data->selected_files.size > FILE_EXPLORER_RESET_SELECTED_FREE_LIMIT) {
		data->selected_files.FreeBuffer();
	}
	data->selected_files.size = 0;
	// We need to redraw if the selection is reset - even when new values are being
	// Set afterwards
	data->should_redraw = true;
}

static void FileExplorerResetSelectedFiles(EditorState* editor_state) {
	FileExplorerData* data = editor_state->file_explorer_data;
	FileExplorerResetSelectedFiles(data);
}

static void FileExplorerResetCopiedFiles(FileExplorerData* data) {
	// Cut/Copy will make a coalesced allocation
	if (data->copied_files.size > 0) {
		Deallocate(data->selected_files.allocator, data->copied_files.buffer);
		data->copied_files.size = 0;
	}
	data->flags = ClearFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);
}

static void FileExplorerAllocateSelectedFile(FileExplorerData* data, Stream<wchar_t> path) {
	Stream<wchar_t> new_path = StringCopy(data->selected_files.allocator, path);
	data->selected_files.Add(new_path);
}

static void FileExplorerSetShiftIndices(FileExplorerData* explorer_data, unsigned int index) {
	explorer_data->starting_shift_index = index;
	explorer_data->ending_shift_index = index;
	// Also redraw the explorer now
	explorer_data->should_redraw = true;
}

static void FileExplorerSetShiftIndices(EditorState* editor_state, unsigned int index) {
	FileExplorerSetShiftIndices(editor_state->file_explorer_data, index);
}

void ChangeFileExplorerDirectory(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	FileExplorerData* data = editor_state->file_explorer_data;
	data->current_directory.CopyOther(path);
	data->current_directory[path.size] = L'\0';

	FileExplorerResetSelectedFiles(data);
	FileExplorerSetShiftIndices(data, index);
	data->selected_files.size = 0;

	// We need to redraw the explorer now
	data->should_redraw = true;
}

void ChangeFileExplorerFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	FileExplorerData* data = editor_state->file_explorer_data;
	// If the path's parent is different from the current path, we need to change the directory as well
	Stream<wchar_t> path_parent = PathParent(path);
	if (path_parent != data->current_directory) {
		ChangeFileExplorerDirectory(editor_state, path_parent);
	}
	FileExplorerResetSelectedFiles(data);
	FileExplorerAllocateSelectedFile(data, path);

	FileExplorerSetShiftIndices(data, index);
	data->right_click_stream = data->selected_files[0];
	// We need to redraw the explorer now
	data->should_redraw = true;
}

struct SelectableData {
	ECS_INLINE bool IsTheSameData(const SelectableData* other) const {
		return (other != nullptr) && selection == other->selection;
	}

	EditorState* editor_state;
	unsigned int index;
	Path selection;
	Timer timer;
};

static void FileExplorerSetNewDirectory(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	// This function will also redraw the explorer
	ChangeFileExplorerDirectory(editor_state, path, index);
}

static void FileExplorerSetNewFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	// This function will also redraw the explorer
	ChangeFileExplorerFile(editor_state, path, index);
	ChangeInspectorToFile(editor_state, path);
}

// Returns -1 when it doesn't exist, else the index where it is located
static unsigned int FileExplorerHasSelectedFile(EditorState* editor_state, Stream<wchar_t> path) {
	FileExplorerData* data = editor_state->file_explorer_data;
	return FindString(path, Stream<Stream<wchar_t>>(data->selected_files.buffer, data->selected_files.size));
}

static void FileExplorerHandleControlPath(EditorState* editor_state, Stream<wchar_t> path) {
	// Check to see if the path already exists; if it is there, remove it else add it
	FileExplorerData* data = editor_state->file_explorer_data;
	unsigned int index = FileExplorerHasSelectedFile(editor_state, path);
	if (index != -1) {
		Deallocate(data->selected_files.allocator, data->selected_files[index].buffer);
		data->selected_files.RemoveSwapBack(index);
	}
	else {
		FileExplorerAllocateSelectedFile(data, path);
	}
	// We need to redraw the explorer as well
	data->should_redraw = true;
}

static void FileExplorerHandleShiftSelection(EditorState* editor_state, unsigned int index) {
	FileExplorerData* explorer_data = editor_state->file_explorer_data;
	if (explorer_data->ending_shift_index < index || explorer_data->ending_shift_index == -1) {
		explorer_data->ending_shift_index = index;
	}
	if (explorer_data->starting_shift_index > index || explorer_data->starting_shift_index == -1) {
		explorer_data->starting_shift_index = index;
	}
	explorer_data->flags = SetFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES);
	// We need to redraw the explorer as well
	explorer_data->should_redraw = true;
}

static void FileExplorerSelectableBase(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* data = (SelectableData*)_data;
	EditorState* editor_state = data->editor_state;
	FileExplorerData* explorer_data = editor_state->file_explorer_data;

	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
			if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
				FileExplorerHandleControlPath(editor_state, data->selection);
			}
			else if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
				FileExplorerHandleShiftSelection(editor_state, data->index);
			}
			else {
				// Check to see if the file is already selected - if it is, then do nothing in order for 
				// the drag to work correctly - we still have to notify the inspectors
				Stream<Stream<wchar_t>> selected_files(explorer_data->selected_files.buffer, explorer_data->selected_files.size);
				if (FindString(data->selection, selected_files) == -1) {
					FileExplorerSetNewFile(editor_state, data->selection, data->index);
				}
				else {
					ChangeInspectorToFile(editor_state, data->selection);
				}
			}
		}
	}
}

static void FileExplorerDirectorySelectable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* additional_data = (SelectableData*)_additional_data;
	SelectableData* data = (SelectableData*)_data;

	system->m_focused_window_data.clean_up_call_general = true;
	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
			if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				if (additional_data->timer.GetDurationSinceMarker(ECS_TIMER_DURATION_MS) < DOUBLE_CLICK_DURATION && keyboard->IsUp(ECS_KEY_LEFT_CTRL)) {
					FileExplorerSetNewDirectory(data->editor_state, data->selection, data->index);
				}
			}
			data->timer.SetMarker();
			if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
				FileExplorerHandleControlPath(data->editor_state, data->selection);
			}
			else if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
				FileExplorerHandleShiftSelection(data->editor_state, data->index);
			}
			else {
				FileExplorerData* explorer_data = data->editor_state->file_explorer_data;
				// Check to see if the directory is already selected - if it is, then do nothing in order for 
				// the drag to work correctly
				Stream<Stream<wchar_t>> selected_files = explorer_data->selected_files.ToStream();
				if (FindString(data->selection, selected_files) == -1) {
					FileExplorerResetSelectedFiles(explorer_data);
					FileExplorerAllocateSelectedFile(explorer_data, data->selection);
					FileExplorerSetShiftIndices(explorer_data, data->index);
				}
			}
		}
	}
}

static void FileExplorerChangeDirectoryFromFile(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	FileExplorerData* explorer_data = data->file_explorer_data;
	FileExplorerSetNewDirectory(data, explorer_data->selected_files[0], -1);
}

static void RenameFileCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	RenameWizardAdditionalData* additional_data = (RenameWizardAdditionalData*)_additional_data;
	additional_data->prevent_default_renaming = true;
	bool success = ProjectRenameFile(editor_state, additional_data->path, additional_data->wide_new_name);
	if (!success) {
		ECS_FORMAT_TEMP_STRING(message, "Failed to rename file {#} to {#}", additional_data->path, additional_data->wide_new_name);
		EditorSetConsoleError(message);
	}
}

static void FileExplorerMenuRenameCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	Stream<wchar_t> path = editor_state->file_explorer_data->right_click_stream;
	bool is_file = IsFile(path);
	UIActionHandler callback = { RenameFileCallback, editor_state, 0 };
	if (is_file) {
		CreateRenameFileWizard(path, system, callback);
	}
	else {
		CreateRenameFolderWizard(path, system, callback);
	}
}

static void FileExplorerLabelRenameCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* data = (SelectableData*)_data;
	SelectableData* additional_data = (SelectableData*)_additional_data;
	system->SetCleanupGeneralHandler();

	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
			if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				if (additional_data->timer.GetDurationSinceMarker(ECS_TIMER_DURATION_MS) < DOUBLE_CLICK_DURATION) {
					// We can use this implementation
					action_data->data = data->editor_state;
					FileExplorerMenuRenameCallback(action_data);
				}
			}
			data->timer.SetMarker();
			FileExplorerSelectableBase(action_data);
		}
	}
}

static void FileExplorerPasteElements(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	FileExplorerData* data = editor_state->file_explorer_data;

	unsigned int* _invalid_files = (unsigned int*)ECS_MALLOCA(sizeof(unsigned int) * data->copied_files.size);
	Stream<unsigned int> invalid_files(_invalid_files, 0);

	// For each path, copy it to the current directory or cut it
	if (HasFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT)) {
		for (size_t index = 0; index < data->copied_files.size; index++) {
			bool is_directory = IsFolder(data->copied_files[index]);

			bool current_success;
			if (is_directory) {
				current_success = FolderCut(data->copied_files[index], data->current_directory);
			}
			else {
				current_success = FileCut(data->copied_files[index], data->current_directory, true);
			}

			if (!current_success) {
				invalid_files.Add(index);
			}
		}
	}
	else {
		for (size_t index = 0; index < data->copied_files.size; index++) {
			bool is_directory = IsFolder(data->copied_files[index]);

			bool current_success;
			if (is_directory) {
				current_success = FolderCopy(data->copied_files[index], data->current_directory);
			}
			else {
				current_success = FileCopy(data->copied_files[index], data->current_directory, true, true);
			}

			if (!current_success) {
				invalid_files.Add(index);
			}
		}
	}

	// If some files failed
	if (invalid_files.size > 0) {
		constexpr const char* BASE_ERROR_MESSAGE = "One or more files/folders could not be copied. These are: ";

		size_t total_buffer_size = strlen(BASE_ERROR_MESSAGE);
		for (size_t index = 0; index < invalid_files.size; index++) {
			total_buffer_size += data->copied_files[invalid_files[index]].size + 1;
		}
		total_buffer_size += 8;

		char* _error_message = (char*)ECS_MALLOCA(sizeof(char) * total_buffer_size);
		CapacityStream<char> error_message(_error_message, 0, total_buffer_size);
		error_message.CopyOther(BASE_ERROR_MESSAGE);

		for (size_t index = 0; index < invalid_files.size; index++) {
			error_message.Add('\n');
			error_message.Add('\t');
			ConvertWideCharsToASCII(data->copied_files[invalid_files[index]], error_message);
		}

		EditorSetConsoleError(error_message);
		ECS_FREEA(_error_message);
		// Also reset the copied files
		FileExplorerResetCopiedFiles(data);
	}
	data->flags = ClearFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);

	ECS_FREEA(invalid_files.buffer);
	// We need to redraw the file explorer now
	data->should_redraw = true;
}

static void FileExplorerCopySelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;
	size_t total_size = 0;

	for (size_t index = 0; index < data->selected_files.size; index++) {
		total_size += data->selected_files[index].size * sizeof(wchar_t);
	}

	FileExplorerResetCopiedFiles(data);

	void* allocation = Allocate(data->selected_files.allocator, total_size + sizeof(Stream<wchar_t>) * data->selected_files.size);
	uintptr_t buffer = (uintptr_t)allocation;

	data->copied_files.InitializeFromBuffer(buffer, data->selected_files.size);
	for (size_t index = 0; index < data->copied_files.size; index++) {
		data->copied_files[index].InitializeFromBuffer(buffer, data->selected_files[index].size);
		data->copied_files[index].CopyOther(data->selected_files[index]);
	}

	data->flags = ClearFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);
	// The redraw flag doesn't need to be set since this action doesn't change the visuals
}

static void FileExplorerCutSelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;
	FileExplorerCopySelection(action_data);
	data->flags = SetFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);
	// We need to redraw the explorer now
	data->should_redraw = true;
}

static void FileExplorerDeleteSelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;

	unsigned int* _invalid_files = (unsigned int*)ECS_MALLOCA(data->selected_files.size * sizeof(unsigned int));
	unsigned int* _valid_copy_files = (unsigned int*)ECS_MALLOCA(data->copied_files.size * sizeof(unsigned int));
	Stream<unsigned int> invalid_files(_invalid_files, 0);
	Stream<unsigned int> valid_copy_files(_valid_copy_files, data->copied_files.size);

	MakeSequence(valid_copy_files);

	for (size_t index = 0; index < data->selected_files.size; index++) {
		bool success;
		if (IsFolder(data->selected_files[index])) {
			success = RemoveFolder(data->selected_files[index]);
		}
		else {
			success = RemoveFile(data->selected_files[index]);
			// If this is a thunk/forwarding asset file, remove the metadata as well
		}
		if (!success) {
			invalid_files.Add(index);
		}
		// Check to see if this file is also in the copy stream to remove it
		else {
			unsigned int copy_index = FindString(data->selected_files[index], data->copied_files);
			if (copy_index != -1) {
				for (size_t valid_index = 0; valid_index < valid_copy_files.size; valid_index++) {
					if (valid_copy_files[valid_index] == copy_index) {
						valid_copy_files.RemoveSwapBack(valid_index);
						// Exit the loop
						break;
					}
				}
			}
		}
	}
	if (invalid_files.size > 0) {
		constexpr const char* ERROR_MESSAGE = "One or more files/folders could not be deleted. These are: ";
		size_t total_size = strlen(ERROR_MESSAGE);
		for (size_t index = 0; index < invalid_files.size; index++) {
			total_size += data->selected_files[invalid_files[index]].size + 1;
		}
		// Safety padding
		char* temp_allocation = (char*)ECS_MALLOCA(sizeof(char) * total_size + 8);
		CapacityStream<char> error_message(temp_allocation, 0, total_size + 8);

		error_message.CopyOther(ERROR_MESSAGE);
		for (size_t index = 0; index < invalid_files.size; index++) {
			error_message.Add('\n');
			ConvertWideCharsToASCII(data->selected_files[invalid_files[index]], error_message);
		}

		CreateErrorMessageWindow(system, error_message);
		ECS_FREEA(temp_allocation);
	}

	if (valid_copy_files.size < data->copied_files.size) {
		size_t total_size = 0;
		for (size_t index = 0; index < valid_copy_files.size; index++) {
			total_size += data->copied_files[valid_copy_files[index]].size;
		}

		void* allocation = Allocate(data->selected_files.allocator, sizeof(wchar_t) * total_size + sizeof(Stream<wchar_t>) * valid_copy_files.size, alignof(wchar_t));
		uintptr_t buffer = (uintptr_t)allocation;
		Stream<Stream<wchar_t>> new_copy_files;
		new_copy_files.InitializeFromBuffer(buffer, valid_copy_files.size);
		for (size_t index = 0; index < new_copy_files.size; index++) {
			new_copy_files[index].InitializeFromBuffer(buffer, data->copied_files[valid_copy_files[index]].size);
			new_copy_files[index].CopyOther(data->copied_files[valid_copy_files[index]]);
		}
		FileExplorerResetCopiedFiles(data);
		data->copied_files = new_copy_files;
	}

	ECS_FREEA(_valid_copy_files);
	ECS_FREEA(_invalid_files);
	FileExplorerResetSelectedFiles(data);
	// We need to redraw the file explorer now
	data->should_redraw = true;
}

typedef void (*FileExplorerSelectFromIndex)(FileExplorerData* data, unsigned int index, Stream<wchar_t> path);

static void FileExplorerSelectFromIndexNothing(FileExplorerData* data, unsigned int index, Stream<wchar_t> path) {}

static void FileExplorerSelectFromIndexShift(FileExplorerData* data, unsigned int index, Stream<wchar_t> path) {
	if (data->starting_shift_index <= index && index <= data->ending_shift_index) {
		unsigned int selected_index = data->selected_files.ReserveRange();
		void* new_allocation = Allocate(data->selected_files.allocator, data->selected_files[selected_index].MemoryOf(path.size));
		data->selected_files[selected_index].InitializeFromBuffer(new_allocation, path.size);
		data->selected_files[selected_index].CopyOther(path);
	}
}

struct ForEachData {
	EditorState* editor_state;
	UIDrawer* drawer;
	UIDrawConfig* config;
	size_t draw_configuration;
	unsigned int element_count;
	CapacityStream<wchar_t>* mouse_element_path;
	FileExplorerSelectFromIndex select_function;
};

struct FileFunctorData {
	ForEachData* for_each_data;
	unsigned char color_alpha;
};

ECS_INLINE static bool FileExplorerIsElementSelected(FileExplorerData* data, Path path) {
	return FindString(path, Stream<Stream<wchar_t>>(data->selected_files.buffer, data->selected_files.size)) != (unsigned int)-1;
}

ECS_INLINE static bool FileExplorerIsElementCut(FileExplorerData* data, Path path) {
	return HasFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT) && FindString(path, data->copied_files) != (unsigned int)-1;
}

static void FileExplorerLabelDraw(UIDrawer* drawer, UIDrawConfig* config, SelectableData* _data, bool is_selected, bool is_folder) {
	FileExplorerData* data = _data->editor_state->file_explorer_data;
	Path current_path = _data->selection;

	size_t extension_size = PathExtensionSize(current_path) * (!is_folder);
	Path path_filename = PathFilename(current_path);

	char* allocation = (char*)data->temporary_allocator.Allocate((path_filename.size + 1) * sizeof(char), alignof(char));
	CapacityStream<char> ascii_stream(allocation, 0, 512);
	ConvertWideCharsToASCII(path_filename, ascii_stream);
	
	// Draw the stem
	path_filename.size -= extension_size;

	constexpr size_t LABEL_CONFIGURATION = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
		| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_PARAMETERS;

	float2 current_position = drawer->GetCurrentPosition();
	float label_horizontal_scale = drawer->GetSquareScaleScaled(THUMBNAIL_SIZE).x;

	UIConfigWindowDependentSize transform;	
	transform.type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
	transform.scale_factor = drawer->GetWindowSizeFactors(transform.type, { label_horizontal_scale, drawer->layout.default_element_y });
	config->AddFlag(transform);
	UIConfigTextParameters text_parameters = drawer->TextParameters();
	text_parameters.size *= 0.8f;
	config->AddFlag(text_parameters);

	drawer->element_descriptor.label_padd.x *= 0.5f;

	if (is_selected) {
		UIConfigBorder border;
		border.thickness = drawer->GetDefaultBorderThickness();
		border.color = EDITOR_GREEN_COLOR;
		border.draw_phase = ECS_UI_DRAW_NORMAL;
		config->AddFlag(border);

		drawer->TextLabel(LABEL_CONFIGURATION | UI_CONFIG_BORDER, *config, ascii_stream);
		config->flag_count--;
	}
	else {
		drawer->TextLabel(LABEL_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT, *config, ascii_stream);
	}

	drawer->element_descriptor.label_padd.x *= 2.0f;

	config->flag_count -= 2;

	UIConfigRectangleGeneral general_action;
	general_action.handler = { FileExplorerLabelRenameCallback, _data, sizeof(*_data), ECS_UI_DRAW_SYSTEM };
	config->AddFlag(general_action);

	drawer->Rectangle(
		UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_RECTANGLE_GENERAL_ACTION | UI_CONFIG_DO_NOT_FIT_SPACE,
		*config, 
		drawer->GetCurrentPosition(), 
		{ label_horizontal_scale, drawer->layout.default_element_y }
	);

	config->flag_count--;

	ascii_stream[path_filename.size] = extension_size > 0 ? '.' : '\0';
	ascii_stream.size = path_filename.size + extension_size;

	float2 font_size = drawer->GetFontSize();
	UITextTooltipHoverableData tooltip_data;
	tooltip_data.characters = ascii_stream;
	tooltip_data.base.offset_scale.y = true;
	tooltip_data.base.offset.y = TOOLTIP_OFFSET;
	tooltip_data.base.center_horizontal_x = true;
	tooltip_data.base.font_size = font_size;
	
	drawer->AddTextTooltipHoverable(0, current_position, { label_horizontal_scale, drawer->layout.default_element_y }, &tooltip_data);
}

struct PathButtonData {
	FileExplorerData* data;
	size_t size;
};

static void FileExplorerPathButtonAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	PathButtonData* data = (PathButtonData*)_data;
	data->data->current_directory.size = data->size;
	FileExplorerResetSelectedFiles(data->data);
}

#pragma region File functors

#define EXPAND_ACTION UI_UNPACK_ACTION_DATA; \
\
FileFunctorData* functor_data = (FileFunctorData*)_additional_data; \
SelectableData* data = (SelectableData*)_data; \
\
UIDrawer* drawer = functor_data->for_each_data->drawer; \
UIDrawConfig* config = functor_data->for_each_data->config; \
Color white_color = ECS_COLOR_WHITE; \
white_color.alpha = functor_data->color_alpha; \
size_t configuration = functor_data->for_each_data->draw_configuration;

static void FileTextureDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle(configuration, *config, data->selection, white_color);
}

static void FileBlankDraw(ActionData* action_data) {
	EXPAND_ACTION;

	Color sprite_color = drawer->color_theme.text;
	sprite_color.alpha = white_color.alpha;
	drawer->SpriteRectangle(configuration, *config, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, sprite_color);
}

static void FileOverlayDraw(ActionData* action_data, const wchar_t* overlay_texture) {
	EXPAND_ACTION;

	Color base_color = drawer->color_theme.theme;
	Color overlay_color = drawer->color_theme.text;
	base_color.alpha = white_color.alpha;
	overlay_color.alpha = white_color.alpha;
	drawer->SpriteRectangleDouble(
		configuration,
		*config,
		ECS_TOOLS_UI_TEXTURE_FILE_BLANK,
		overlay_texture,
		overlay_color,
		base_color
	);
}

static void FileCDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_C);
}

static void FileCppDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_CPP);
}

static void FileConfigDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG);
}

static void FileTextDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_TEXT);
}

static void FileShaderDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_SHADER);
}

static void FileEditorDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_EDITOR);
}

static void FileMeshDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_MESH);
}

static void FileSceneDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_SCENE);
}

static void FileAssetShaderDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ASSET_SHADER_ICON);
}

static void FileAssetGPUSamplerDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ASSET_GPU_SAMPLER_ICON);
}

static void FileAssetMiscDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ASSET_MISC_ICON);
}

static void FileMaterialDraw(ActionData* action_data) {
	EXPAND_ACTION;

	Color base_color = LightenColorClamp(drawer->color_theme.theme, 1.5f);
	Color overlay_color = drawer->color_theme.text;
	base_color.alpha = white_color.alpha;
	overlay_color.alpha = white_color.alpha;
	drawer->SpriteRectangleDouble(
		configuration,
		*config,
		ECS_TOOLS_UI_TEXTURE_FILE_BLANK,
		ASSET_MATERIAL_ICON,
		overlay_color,
		base_color
	);
}

static void FileMeshThumbnailDraw(ActionData* action_data) {
	EXPAND_ACTION;

	FileExplorerData* explorer_data = data->editor_state->file_explorer_data;
	FileExplorerMeshThumbnail thumbnail = explorer_data->mesh_thumbnails.GetValue(data->selection);

	drawer->SpriteRectangle(configuration, *config, thumbnail.texture, white_color);
}

static void FileInputRecordingFileDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_KEYBOARD_SQUARE);
}

static void FileStateRecordingFileDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_REPLAY);
}

#undef EXPAND_ACTION

#pragma endregion

#pragma region Filter

static void FilterNothing(ActionData* action_data) {}

static void FilterActive(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	bool* is_valid = (bool*)_additional_data;
	SelectableData* data = (SelectableData*)_data;

	Path filename = PathFilename(data->selection);

	ECS_STACK_CAPACITY_STREAM(char, ascii_path, 512);
	ConvertWideCharsToASCII(filename, ascii_path);
	ascii_path.AddAssert('\0');
	ascii_path.size--;

	FileExplorerData* file_explorer_data = data->editor_state->file_explorer_data;
	*is_valid = strstr(ascii_path.buffer, file_explorer_data->filter_stream.buffer) != nullptr;
}

static Action FILTER_FUNCTORS[] = { FilterNothing, FilterActive };

#pragma endregion

#pragma region Mesh Export Click Actions

struct MonitorMeshExportAllEventData {
	GLTFData gltf_data;
	GLTFExportTexturesOptions export_options;
};

// This is used to wait for the export to finish and report to the user any errors that may have happened
EDITOR_EVENT(MonitorMeshExportAllEvent) {
	MonitorMeshExportAllEventData* data = (MonitorMeshExportAllEventData*)_data;

	if (!data->export_options.semaphore->CheckCount(0)) {
		return true;
	}

	// Deallocate everything from the multithreaded allocator - the base of the allocation
	// Is the atomic stream itself
	if (data->export_options.failure_string->size > 0) {
		data->export_options.failure_string->SpinWaitWrites();
		EditorSetConsoleError(data->export_options.failure_string->ToStream());
	}
	GLTFExportTexturesOptionsDeallocateAll(&data->export_options, editor_state->MultithreadedEditorAllocator());

	// We also need to free the gltf data
	FreeGLTFFile(data->gltf_data);
	return false;
}

struct MeshExportAllTaskData {
	EditorState* editor_state;
	Stream<wchar_t> mesh_path;
	bool search_to_folder;
};

// For export all to here/folder we need to add a background task to take care of this since
// Exporting can take quite a bit of time
ECS_THREAD_TASK(MeshExportAllTask) {
	MeshExportAllTaskData* data = (MeshExportAllTaskData*)_data;

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	GLTFData gltf_data = LoadGLTFFile(data->mesh_path, ECS_MALLOC_ALLOCATOR, &error_message);
	if (gltf_data.data == nullptr) {
		// Failed, print the error message
		EditorSetConsoleError(error_message);
	}
	else {
		ECS_STACK_CAPACITY_STREAM(wchar_t, write_directory_storage, 512);
		Stream<wchar_t> write_directory = PathParentBoth(data->mesh_path);
		if (data->search_to_folder) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder_path, 512);
			GetProjectAssetsFolder(data->editor_state, assets_folder_path);
			if (!GLTFExportTexturesToFolderSearchRecursive(write_directory, assets_folder_path, write_directory_storage)) {
				ECS_FORMAT_TEMP_STRING(message, "Failed to export textures: the known export folders Textures/Materials are missing");
				EditorSetConsoleError(message);
			}
			write_directory = write_directory_storage;
		}

		size_t ERROR_STRING_CAPACITY = ECS_KB;
		GLTFExportTexturesOptions export_options;
		export_options.task_manager = data->editor_state->task_manager;
		export_options.use_standard_texture_names = true;
		GLTFExportTexturesOptionsAllocateAll(&export_options, ERROR_STRING_CAPACITY, data->editor_state->MultithreadedEditorAllocator());
		
		bool has_textures = GLTFExportTexturesDirectly(gltf_data, write_directory, &export_options);

		if (!has_textures) {
			EditorSetConsoleInfo("The selected mesh file has no textures embedded in it");
		}

		// Add the monitor event - we need to add this regardless whether or not there are textures
		// Since the deallocation are done there
		MonitorMeshExportAllEventData monitor_data;
		monitor_data.gltf_data = gltf_data;
		monitor_data.export_options = export_options;
		if (has_textures) {
			EditorAddEvent(data->editor_state, MonitorMeshExportAllEvent, &monitor_data, sizeof(monitor_data));
		}
		else {
			MonitorMeshExportAllEvent(data->editor_state, &monitor_data);
		}
	}
	data->editor_state->multithreaded_editor_allocator->DeallocateTs(data->mesh_path.buffer);
}

struct MeshExportAllActionData {
	EditorState* editor_state;
	const FileExplorerData* explorer_data;
};

template<bool search_folder>
void MeshExportAllAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	MeshExportAllActionData* data = (MeshExportAllActionData*)_data;
	MeshExportAllTaskData background_data;
	background_data.editor_state = data->editor_state;
	background_data.mesh_path.InitializeAndCopy(data->editor_state->MultithreadedEditorAllocator(), data->explorer_data->right_click_stream);
	background_data.search_to_folder = search_folder;

	EditorStateAddBackgroundTask(data->editor_state, ECS_THREAD_TASK_NAME(MeshExportAllTask, &background_data, sizeof(background_data)));
}

struct MeshExportSelectionDrawWindowData {
	GLTFData gltf_data;
	Stream<wchar_t> mesh_path;
	bool search_to_folder;
	EditorState* editor_state;

	// These do not need to be provided
	bool is_create_folder;

	// These are allocated from the window
	bool* is_selected;
	CapacityStream<PBRMaterialMapping> export_textures;
	Stream<Stream<char>> ascii_texture_names;
};

void MeshExportSelectionDrawWindow(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	MeshExportSelectionDrawWindowData* data = (MeshExportSelectionDrawWindowData*)window_data;

	const size_t EXPORT_TEXTURE_CAPACITY = 512;
	if (initialize) {
		void* allocation = drawer.GetMainAllocatorBuffer(sizeof(GLTFExportTexture) * EXPORT_TEXTURE_CAPACITY);
		data->export_textures.InitializeFromBuffer(allocation, 0, EXPORT_TEXTURE_CAPACITY);
		GLTFGetExportTexturesNames(data->gltf_data, &data->export_textures, data->editor_state->EditorAllocator());

		data->is_selected = (bool*)drawer.GetMainAllocatorBuffer(sizeof(bool) * data->export_textures.size);
		memset(data->is_selected, true, sizeof(bool) * data->export_textures.size);

		size_t ascii_allocation_size = sizeof(Stream<char>) * data->export_textures.size;
		for (unsigned int index = 0; index < data->export_textures.size; index++) {
			ascii_allocation_size += sizeof(char) * (data->export_textures[index].texture.size + 1);
		}
		void* ascii_allocation = drawer.GetMainAllocatorBuffer(ascii_allocation_size);
		uintptr_t ascii_allocation_ptr = (uintptr_t)ascii_allocation;
		data->ascii_texture_names.InitializeFromBuffer(ascii_allocation_ptr, data->export_textures.size);
		for (unsigned int index = 0; index < data->export_textures.size; index++) {
			size_t texture_size = data->export_textures[index].texture.size;
			data->ascii_texture_names[index].InitializeFromBuffer(ascii_allocation_ptr, texture_size);
			ConvertWideCharsToASCII(data->export_textures[index].texture.buffer, data->ascii_texture_names[index].buffer, texture_size, texture_size + 1);
		}
		data->is_create_folder = true;
	}
	else {
		// Draw Select All and Deselect All buttons at the top

		auto select_all_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;
			MeshExportSelectionDrawWindowData* data = (MeshExportSelectionDrawWindowData*)_data;
			memset(data->is_selected, true, sizeof(bool) * data->export_textures.size);
		};

		auto deselect_all_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;
			MeshExportSelectionDrawWindowData* data = (MeshExportSelectionDrawWindowData*)_data;
			memset(data->is_selected, false, sizeof(bool) * data->export_textures.size);
		};

		UIDrawConfig config;
		// Present this only if we search to folder
		if (data->search_to_folder) {
			drawer.Sentence(UI_CONFIG_SENTENCE_FIT_SPACE, config, "It will search for a folder named Materials/Textures in the current mesh directory to export the textures to."
				" If it doesn't exist, it will either search in parent directories, or create a new folder called Textures where the textures will be written to, depending on the `Create Folder` checkbox.");
			drawer.NextRow();

			drawer.CheckBox("Create Folder", &data->is_create_folder);
			drawer.NextRow();
		}

		drawer.Button("Select All", { select_all_action, data, 0 });
		drawer.Button("Deselect All", { deselect_all_action, data, 0 });

		drawer.NextRow();

		for (unsigned int index = 0; index < data->export_textures.size; index++) {
			drawer.CheckBox(data->ascii_texture_names[index], data->is_selected + index);
			drawer.NextRow();
		}
		
		auto export_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;
			MeshExportSelectionDrawWindowData* window_data = (MeshExportSelectionDrawWindowData*)_data;

			ECS_STACK_CAPACITY_STREAM(wchar_t, write_directory_storage, 512);
			Stream<wchar_t> write_directory = PathParentBoth(window_data->mesh_path);
			if (window_data->search_to_folder) {
				ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
				GetProjectAssetsFolder(window_data->editor_state, assets_folder);
				if (window_data->is_create_folder) {
					// If the option to create a folder is enabled, try to match a current level directory,
					// Else, create one
					if (!GLTFExportTexturesToFolderSearch(write_directory, write_directory_storage)) {
						write_directory_storage.CopyOther(write_directory);
						write_directory_storage.AddAssert(ECS_OS_PATH_SEPARATOR);
						write_directory_storage.AddStreamAssert(L"Textures");
						if (!CreateFolder(write_directory_storage)) {
							ECS_FORMAT_TEMP_STRING(message, "Failed to create folder {#} to export the textures to", write_directory_storage);
							EditorSetConsoleError(message);
							return;
						}
					}
					write_directory = write_directory_storage;
				}
				else {
					if (!GLTFExportTexturesToFolderSearchRecursive(write_directory, assets_folder, write_directory_storage)) {
						ECS_FORMAT_TEMP_STRING(message, "Failed to export textures: the known export folders Textures/Materials are missing");
						EditorSetConsoleError(message);
						return;
					}
				}
				write_directory = write_directory_storage;
			}

			GLTFExportTexturesOptions options;
			options.task_manager = window_data->editor_state->task_manager;
			// Use the multithreaded editor allocator such that we conform to the monitor event
			GLTFExportTexturesOptionsAllocateAll(&options, ECS_KB, window_data->editor_state->MultithreadedEditorAllocator());

			// Remove those who are deselected
			unsigned int remove_count = 0;
			for (unsigned int index = 0; index < window_data->export_textures.size; index++) {
				if (!window_data->is_selected[index]) {
					window_data->export_textures.RemoveSwapBack(index);
					// Also swap in place that texture's selected status
					window_data->is_selected[index] = window_data->is_selected[window_data->export_textures.size];
					index--;
				}
			}

			options.textures_to_write = window_data->export_textures;
			options.use_standard_texture_names = true;
			GLTFExportTexturesDirectly(window_data->gltf_data, write_directory, &options);

			// We don't need to handle the case where there is nothing written

			// Add the monitor event to inform the user
			MonitorMeshExportAllEventData monitor_data;
			monitor_data.export_options = options;
			monitor_data.gltf_data = window_data->gltf_data;
			EditorAddEvent(window_data->editor_state, MonitorMeshExportAllEvent, &monitor_data, sizeof(monitor_data));

			// Disable the freeing of the gltf data
			window_data->gltf_data.data = nullptr;
		};

		// Export and Cancel buttons
		UIDrawerOKCancelRow(drawer, "Export", "Cancel", { export_action, window_data, 0 }, {});
	}
}

void MeshExportSelectionDestroyWindow(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	MeshExportSelectionDrawWindowData* window_data = (MeshExportSelectionDrawWindowData*)_additional_data;

	// It can be disabled by the Export action such that the monitor event is the one deallocating the gltf data
	if (window_data->gltf_data.data != nullptr) {
		FreeGLTFFile(window_data->gltf_data);
	}
	window_data->editor_state->multithreaded_editor_allocator->DeallocateTs(window_data->mesh_path.buffer);
	for (unsigned int index = 0; index < window_data->export_textures.size; index++) {
		window_data->export_textures[index].texture.Deallocate(window_data->editor_state->EditorAllocator());
	}
}

struct MeshExportSelectionEventData {
	GLTFData gltf_data;
	Stream<wchar_t> mesh_path;
	bool search_to_folder;
};

EDITOR_EVENT(MeshExportSelectionEvent) {
	MeshExportSelectionEventData* data = (MeshExportSelectionEventData*)_data;

	UIWindowDescriptor window_descriptor;

	MeshExportSelectionDrawWindowData draw_data;
	draw_data.gltf_data = data->gltf_data;
	draw_data.mesh_path = data->mesh_path;
	draw_data.search_to_folder = data->search_to_folder;
	draw_data.editor_state = editor_state;

	window_descriptor.draw = MeshExportSelectionDrawWindow;
	window_descriptor.window_data = &draw_data;
	window_descriptor.window_data_size = sizeof(draw_data);

	window_descriptor.initial_size_x = 0.7f;
	window_descriptor.initial_size_y = 0.7f;
	window_descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, window_descriptor.initial_size_x);
	window_descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, window_descriptor.initial_size_y);

	window_descriptor.window_name = "Mesh Export Textures";
	window_descriptor.destroy_action = MeshExportSelectionDestroyWindow;

	editor_state->ui_system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_FIT_TO_VIEW | UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER | UI_DOCKSPACE_NO_DOCKING);
	return false;
}

struct MeshExportSelectionTaskData {
	EditorState* editor_state;
	Stream<wchar_t> mesh_path;
	bool search_to_folder;
};

ECS_THREAD_TASK(MeshExportSelectionTask) {
	MeshExportSelectionTaskData* data = (MeshExportSelectionTaskData*)_data;

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	GLTFData gltf_data = LoadGLTFFile(data->mesh_path, ECS_MALLOC_ALLOCATOR, &error_message);
	if (gltf_data.data == nullptr) {
		// Failed, print the error message
		EditorSetConsoleError(error_message);
		data->editor_state->multithreaded_editor_allocator->DeallocateTs(data->mesh_path.buffer);
	}
	else {
		// Determine if there are textures at all in this file
		// If not, don't bother to add the event
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		ECS_STACK_CAPACITY_STREAM(PBRMaterialMapping, textures, ECS_KB);
		GLTFGetExportTexturesNames(gltf_data, &textures, &stack_allocator);

		if (textures.size > 0) {
			MeshExportSelectionEventData event_data;
			event_data.mesh_path = data->mesh_path;
			event_data.search_to_folder = data->search_to_folder;
			event_data.gltf_data = gltf_data;
			EditorAddEvent(data->editor_state, MeshExportSelectionEvent, &event_data, sizeof(event_data));
		}
		else {
			EditorSetConsoleInfo("The selected mesh file has no textures embedded in it");
		}
	}
}

struct MeshExportSelectionActionData {
	EditorState* editor_state;
	const FileExplorerData* explorer_data;
};

template<bool search_folder>
void MeshExportSelectionAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	MeshExportSelectionActionData* data = (MeshExportSelectionActionData*)_data;
	MeshExportSelectionTaskData background_data;
	background_data.editor_state = data->editor_state;
	background_data.mesh_path.InitializeAndCopy(data->editor_state->MultithreadedEditorAllocator(), data->explorer_data->right_click_stream);
	background_data.search_to_folder = search_folder;

	EditorStateAddBackgroundTask(data->editor_state, ECS_THREAD_TASK_NAME(MeshExportSelectionTask, &background_data, sizeof(background_data)));
}

#pragma endregion

struct FileExplorerDragData {
	EditorState* editor_state;
	// Used to know whether or not the prefab was initialized such that we don't call it multiple times
	bool prefab_initiated;
};

static void FileExplorerDrag(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerDragData* data = (FileExplorerDragData*)_data;
	EditorState* editor_state = data->editor_state;
	FileExplorerData* explorer_data = editor_state->file_explorer_data;
	if (mouse->IsDown(ECS_MOUSE_LEFT)) {
		// Display the hover only if the mouse exited the action box
		if (!IsPointInRectangle(mouse_position, position, scale)) {
			if (!IsPrefabFileSelected(editor_state)) {
				Color transparent_color = ECS_COLOR_WHITE;
				transparent_color.alpha = 100;
				Color theme_color = system->m_windows[window_index].descriptors->color_theme.theme;
				theme_color.alpha = 100;

				float2 hover_scale = system->GetSquareScale(THUMBNAIL_SIZE) * system->m_windows[window_index].zoom;
				float2 hover_position = { mouse_position.x - hover_scale.x * 0.5f, mouse_position.y - hover_scale.y + THUMBNAIL_TO_LABEL_SPACING };

				// The theme transparent hover
				system->SetSprite(
					dockspace,
					border_index,
					ECS_TOOLS_UI_TEXTURE_MASK,
					hover_position,
					hover_scale,
					buffers,
					theme_color,
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					ECS_UI_DRAW_SYSTEM
				);

				// The directory or last file type used
				Stream<wchar_t> last_file = explorer_data->selected_files[explorer_data->selected_files.size - 1];
				Stream<wchar_t> last_type_extension = PathExtension(last_file);
				const wchar_t* texture = ECS_TOOLS_UI_TEXTURE_FOLDER;

				// If it has an extension, check to see existing files
				bool draw_texture = true;
				if (last_type_extension.size != 0) {
					Action action;
					ResourceIdentifier identifier(last_type_extension.buffer, last_type_extension.size * sizeof(wchar_t));
					bool success = explorer_data->file_functors.TryGetValue(identifier, action);
					if (success) {
						// The mesh is a special case
						if (action == FileMeshDraw) {
							// If it is a mesh draw, check to see if the thumbnail was generated
							FileExplorerMeshThumbnail thumbnail;
							if (explorer_data->mesh_thumbnails.TryGetValue(last_file, thumbnail)) {
								if (thumbnail.could_be_read) {
									// Set the thumbnail texture
									system->SetSprite(
										dockspace,
										border_index,
										thumbnail.texture,
										hover_position,
										hover_scale,
										buffers,
										transparent_color,
										{ 0.0f, 0.0f },
										{ 1.0f, 1.0f },
										ECS_UI_DRAW_SYSTEM
									);
									draw_texture = false;
								}
								else {
									// The last file texture
									texture = ECS_TOOLS_UI_TEXTURE_FILE_MESH;
								}
							}
							else {
								// The last file texture
								texture = ECS_TOOLS_UI_TEXTURE_FILE_MESH;
							}
						}
						else {
							// Emulate a drawer at the current location
							UIDrawer drawer(system->m_descriptors.color_theme, system->m_descriptors.font, system->m_descriptors.window_layout, system->m_descriptors.element_descriptor, false, true);
							drawer.SetCurrentX(hover_position.x);
							drawer.SetCurrentY(hover_position.y);
							drawer.buffers = buffers;
							drawer.system_buffers = buffers;
							drawer.system = system;
							drawer.min_region_render_limit = { -FLT_MAX, -FLT_MAX };
							drawer.max_region_render_limit = { FLT_MAX, FLT_MAX };

							UIDrawConfig config;
							UIConfigRelativeTransform transform;
							transform.scale = drawer.GetRelativeTransformFactors(hover_scale);
							config.AddFlag(transform);

							SelectableData selectable_data;
							selectable_data.editor_state = editor_state;
							selectable_data.index = -1;
							selectable_data.selection = last_file;

							FileFunctorData file_functor_data;
							ForEachData for_each_data;
							for_each_data.editor_state = editor_state;
							for_each_data.drawer = &drawer;
							for_each_data.config = &config;
							for_each_data.element_count = 0;
							for_each_data.mouse_element_path = nullptr;
							for_each_data.select_function = nullptr;
							for_each_data.draw_configuration = UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_SYSTEM_DRAW;

							file_functor_data.for_each_data = &for_each_data;
							file_functor_data.color_alpha = UINT8_MAX;
							ActionData functor_action_data;
							functor_action_data.additional_data = &file_functor_data;
							functor_action_data.data = &selectable_data;
							functor_action_data.system = system;
							action(&functor_action_data);

							// Set this such that we avoid the destructor to draw the resize bars
							drawer.initializer = true;
							draw_texture = false;
						}
					}
					else {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_BLANK;
					}
				}

				if (draw_texture) {
					system->SetSprite(
						dockspace,
						border_index,
						texture,
						hover_position,
						hover_scale,
						buffers,
						transparent_color,
						{ 0.0f, 0.0f },
						{ 1.0f, 1.0f },
						ECS_UI_DRAW_SYSTEM
					);
				}

				explorer_data->flags = SetFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES);
			}
			else if (!data->prefab_initiated) {
				PrefabStartDrag(editor_state);
				data->prefab_initiated = true;
			}
		}

		system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
	}
	else if (mouse->IsReleased(ECS_MOUSE_LEFT)) {
		if (HasFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES)) {
			explorer_data->flags = SetFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES);
			explorer_data->flags = ClearFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES);
		}
		if (data->prefab_initiated) {
			PrefabEndDrag(editor_state);
		}
	}

}

#pragma region File Explorer Commit Overwrite

constexpr const char* FILE_EXPLORER_SELECT_OVERWRITE_FILES_WINDOW_NAME = "Overwrite files";

struct FileExplorerSelectOverwriteFilesData {
	FileExplorerData* explorer_data;
	Stream<unsigned int> overwrite_files;
	Stream<wchar_t> destination;
};

void FileExplorerSelectOverwriteFilesDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	constexpr const char* DATA_NAME = "Data";
	constexpr const char* STATE_NAME = "State";

	UI_PREPARE_DRAWER(initialize);

	struct InternalData {
		FileExplorerSelectOverwriteFilesData* data;
		Stream<const char*>* files;
		bool* states;
	};

	FileExplorerSelectOverwriteFilesData* data = (FileExplorerSelectOverwriteFilesData*)window_data;

	InternalData internal_data;
	internal_data.data = data;
	if (initialize) {
		// Make a single coalesced allocation
		size_t total_memory = sizeof(Stream<const char*>);
		for (size_t index = 0; index < data->overwrite_files.size; index++) {
			total_memory += sizeof(char) * (data->explorer_data->selected_files[data->overwrite_files[index]].size + 1);
		}
		// The pointer memory
		total_memory += sizeof(const char*) * data->overwrite_files.size;
		
		// For the overwrite file indices
		total_memory += sizeof(unsigned int) * data->overwrite_files.size;
		// The destination string memory
		total_memory += sizeof(wchar_t) * data->destination.size;

		void* allocation = drawer.GetMainAllocatorBufferAndStoreAsResource(DATA_NAME, total_memory);
		uintptr_t ptr = (uintptr_t)allocation;
		internal_data.files = (Stream<const char*>*)allocation;
		ptr += sizeof(*internal_data.files);

		internal_data.files->buffer = (const char**)ptr;
		internal_data.files->size = data->overwrite_files.size;
		ptr += sizeof(const char*) * internal_data.files->size;

		for (size_t index = 0; index < data->overwrite_files.size; index++) {
			Stream<wchar_t> path = data->explorer_data->selected_files[data->overwrite_files[index]];
			ConvertWideCharsToASCII(
				path.buffer,
				(char*)ptr,
				path.size,
				path.size + 2
			);
			char* char_ptr = (char*)ptr;
			internal_data.files->buffer[index] = char_ptr;

			char_ptr[path.size] = '\0';
			ptr += path.size + 1;
		}

		// The overwrite indices memory - no need to store the resource - just repoint the data overwrite buffer
		memcpy((void*)ptr, data->overwrite_files.buffer, sizeof(unsigned int) * data->overwrite_files.size);
		data->overwrite_files.buffer = (unsigned int*)ptr;
		ptr += sizeof(unsigned int) * data->overwrite_files.size;

		// The destination string memory - no need to store the resource - just repoint the data destination buffer
		wchar_t* destination_string = (wchar_t*)ptr;
		memcpy(destination_string, data->destination.buffer, sizeof(wchar_t) * data->destination.size);
		data->destination.buffer = destination_string;

		// The state memory
		internal_data.states = (bool*)drawer.GetMainAllocatorBufferAndStoreAsResource(STATE_NAME, sizeof(bool) * internal_data.files->size);
		memset(internal_data.states, 1, sizeof(bool) * internal_data.files->size);
	}
	else {
		internal_data.files = (Stream<const char*>*)drawer.GetResource(DATA_NAME);
		internal_data.states = (bool*)drawer.GetResource(STATE_NAME);
	}

	const char* OVERWRITE_TEXT = "Overwrite";
	const char* CANCEL_TEXT = "Cancel";
	const char* DESELECT_ALL = "Deselect All";
	const char* SELECT_ALL = "Select All";

	// Draw the selection buttons - Select all, Deselect all
	auto select_all_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		InternalData* data = (InternalData*)_data;
		memset(data->states, 1, sizeof(bool) * data->data->overwrite_files.size);
	};

	auto deselect_all_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		InternalData* data = (InternalData*)_data;
		memset(data->states, 0, sizeof(bool) * data->data->overwrite_files.size);
	};

	// Display the main message
	drawer.Text("One or more files already exists in the destination. Are you sure you want to overwrite them?");
	drawer.NextRow();

	drawer.Button(SELECT_ALL, { select_all_action, &internal_data, sizeof(internal_data) });
	drawer.Button(DESELECT_ALL, { deselect_all_action, &internal_data, sizeof(internal_data) });

	UIDrawConfig config;
	drawer.Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Select the files you want to overwrite: ");
	drawer.NextRow();
	
	// Draw the checkboxes
	for (size_t index = 0; index < internal_data.files->size; index++) {
		drawer.CheckBox(internal_data.files->buffer[index], internal_data.states + index);
		drawer.NextRow();
	}

	// Draw the buttons - Overwrite and Cancel
	UIConfigAbsoluteTransform transform;

	// Overwrite button
	transform.scale = drawer.GetLabelScale("Overwrite");
	transform.position = drawer.GetAlignedToBottom(transform.scale.y);
	config.AddFlag(transform);

	auto overwrite_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		InternalData* data = (InternalData*)_data;
		ECS_STACK_CAPACITY_STREAM(char, error_message, 1024);
		error_message.CopyOther("One or more files could not be copied. These are:");
		unsigned int error_files_count = 0;
		for (size_t index = 0; index < data->data->overwrite_files.size; index++) {
			if (data->states[index]) {
				Stream<wchar_t> path_to_copy = data->data->explorer_data->selected_files[data->data->overwrite_files[index]];
				bool code = FileCut(path_to_copy, data->data->destination, true);
				if (!code) {
					error_message.Add('\n');
					error_message.Add('\t');
					ConvertWideCharsToASCII(path_to_copy, error_message);
					error_files_count++;
				}
			}
		}

		// Release the selected files
		FileExplorerResetSelectedFiles(data->data->explorer_data);

		if (error_files_count > 0) {
			CreateErrorMessageWindow(system, error_message);
		}

		DestroyCurrentActionWindow(action_data);
	};

	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, OVERWRITE_TEXT, { overwrite_action, &internal_data, sizeof(internal_data), ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM });
	config.flag_count--;
	
	// Aligned to the right
	transform.scale = drawer.GetLabelScale(CANCEL_TEXT);
	transform.position.x = drawer.GetAlignedToRight(transform.scale.x).x;

	auto cancel_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		FileExplorerSelectOverwriteFilesData* data = (FileExplorerSelectOverwriteFilesData*)_data;
		FileExplorerResetSelectedFiles(data->explorer_data);
		DestroyCurrentActionWindow(action_data);
	};
	config.AddFlag(transform);

	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, CANCEL_TEXT, { cancel_action, data, 0, ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM });
}

unsigned int CreateFileExplorerSelectOverwriteFiles(UISystem* system, FileExplorerSelectOverwriteFilesData data) {
	constexpr float2 OVERWITE_WINDOW_SIZE = { 0.5f, 1.0f };

	UIWindowDescriptor descriptor;

	descriptor.draw = FileExplorerSelectOverwriteFilesDraw;

	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, OVERWITE_WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, OVERWITE_WINDOW_SIZE.y);
	descriptor.initial_size_x = OVERWITE_WINDOW_SIZE.x;
	descriptor.initial_size_y = OVERWITE_WINDOW_SIZE.y;

	descriptor.window_data = &data;
	descriptor.window_data_size = sizeof(data);
	descriptor.window_name = FILE_EXPLORER_SELECT_OVERWRITE_FILES_WINDOW_NAME;

	descriptor.destroy_action = ReleaseLockedWindow;

	return system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_LOCK_WINDOW | UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_ALL);
}

#pragma endregion

#pragma region Preload textures

struct FileExplorerPreloadTextureThreadTaskData {
	EditorState* editor_state;
	Semaphore* semaphore;
	Stream<FileExplorerPreloadTexture> preload_textures;
};

ECS_THREAD_TASK(FileExplorerPreloadTextureThreadTask) {
	FileExplorerPreloadTextureThreadTaskData* data = (FileExplorerPreloadTextureThreadTaskData*)_data;
	
	UISystem* ui_system = data->editor_state->ui_system;
	ResourceManager* resource_manager = data->editor_state->UIResourceManager();
	FileExplorerData* explorer_data = data->editor_state->file_explorer_data;

	// Create a temporary global allocator
	GlobalMemoryManager allocator;
	CreateGlobalMemoryManager(&allocator, FILE_EXPLORER_PRELOAD_TEXTURE_ALLOCATOR_SIZE_PER_THREAD, 128, FILE_EXPLORER_PRELOAD_TEXTURE_FALLBACK_SIZE);

	// Load the texture normally - don't generate mip maps as that will require the immediate context
	ResourceManagerTextureDesc descriptor;
	AllocatorPolymorphic current_allocator = &allocator;
	descriptor.allocator = current_allocator;

	constexpr size_t TEXTURE_WIDTH = 256;

	for (size_t index = 0; index < data->preload_textures.size; index++) {
		// Load the file into a buffer
		Stream<void> file_contents = ReadWholeFileBinary(data->preload_textures[index].path, current_allocator);
		// Only if it succeeded
		if (file_contents.buffer != nullptr) {
			DecodedTexture decoded_texture = DecodeTexture(file_contents, data->preload_textures[index].path.buffer, current_allocator, ECS_DECODE_TEXTURE_HDR_TONEMAP);
			// Deallocate the previous contents
			allocator.Deallocate(file_contents.buffer);

			// Resize the texture
			Stream<void> resized_texture = ResizeTexture(
				decoded_texture.data.buffer,
				decoded_texture.width,
				decoded_texture.height,
				decoded_texture.format,
				TEXTURE_WIDTH,
				TEXTURE_WIDTH,
				current_allocator
			);

			// Release the old texture
			allocator.Deallocate(decoded_texture.data.buffer);

			// If this is a single channel texture, transform it into a 4 channel wide
			bool was_widened = false;
			if (decoded_texture.format == ECS_GRAPHICS_FORMAT_R8_UNORM) {
				decoded_texture.format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
				CompressTextureDescriptor compress_descriptor;
				compress_descriptor.allocator = current_allocator;

				Stream<Stream<void>> converted_texture = ConvertSingleChannelTextureToGrayscale({ &resized_texture, 1 }, TEXTURE_WIDTH, TEXTURE_WIDTH, current_allocator);

				// Deallocate the resized data
				allocator.Deallocate(resized_texture.buffer);
				
				// Change the resized texture buffer to this new one
				resized_texture = converted_texture[0];

				// The widened data will "leak" - cannot be reused inside the allocator but it will be released at the end
				// when the whole allocator is released
				was_widened = true;
			}

			auto create_texture = [&](Stream<void> mip_data, ECS_GRAPHICS_FORMAT format, void* deallocate_buffer) {
				// Create a D3D11 texture from the resized texture
				Texture2DDescriptor texture_descriptor;
				texture_descriptor.mip_data = { &mip_data, 1 };
				texture_descriptor.usage = ECS_GRAPHICS_USAGE_IMMUTABLE;
				texture_descriptor.size = { (unsigned int)TEXTURE_WIDTH, (unsigned int)TEXTURE_WIDTH };
				texture_descriptor.format = format;
				texture_descriptor.mip_levels = 1;
				Texture2D texture = ui_system->m_graphics->CreateTexture(&texture_descriptor);
				if (texture.tex != nullptr) {
					data->preload_textures[index].texture = ui_system->m_graphics->CreateTextureShaderViewResource(texture);
					data->preload_textures[index].last_write_time = OS::GetFileLastWrite(data->preload_textures[index].path.buffer);
				}

				allocator.Deallocate(deallocate_buffer);
			};

			// Determine if the texture can be compressed
			ECS_TEXTURE_COMPRESSION_EX compression_type = GetTextureCompressionType(decoded_texture.format);
			if (IsTextureCompressionTypeValid(compression_type)) {
				// Apply compression - only if not HDR compression
				if (compression_type != ECS_TEXTURE_COMPRESSION_EX_HDR) {
					CompressTextureDescriptor compress_descriptor;
					compress_descriptor.allocator = current_allocator;

					Stream<void> compressed_texture;
					if (CompressTexture({ &resized_texture, 1 }, &compressed_texture, TEXTURE_WIDTH, TEXTURE_WIDTH, compression_type, compress_descriptor)) {
						// Release the resized texture - only if it wasn't widened
						if (!was_widened) {
							allocator.Deallocate(resized_texture.buffer);
						}

						create_texture(compressed_texture, GetCompressedRenderFormat(compression_type), compressed_texture.buffer);
					}
				}
				else {
					create_texture(resized_texture, decoded_texture.format, resized_texture.buffer);
				}
			}
			else {
				create_texture(resized_texture, decoded_texture.format, resized_texture.buffer);
			}
		}
	}

	// Enter the semaphore
	unsigned int count = data->semaphore->Enter();
	allocator.Free();

	// The last thread deallocates the shared resources and set the preload end flag
	if (count == data->semaphore->target - 1) {
		Deallocate(data->editor_state->MultithreadedEditorAllocator(), data->semaphore);

		// Signal that processing ended
		explorer_data->preload_flags = SetFlag(explorer_data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_ENDED);
	}
}

void FileExplorerCommitStagingPreloadTextures(EditorState* editor_state) {
	UISystem* ui_system = editor_state->ui_system;
	FileExplorerData* data = editor_state->file_explorer_data;
	ResourceManager* resource_manager = ui_system->m_resource_manager;

	for (size_t index = 0; index < data->staging_preloaded_textures.size; index++) {
		if (data->staging_preloaded_textures[index].texture.view != nullptr) {
			// We need to redraw the explorer if it enters once in this loop
			data->should_redraw = true;

			data->preloaded_textures.Add(data->staging_preloaded_textures[index]);

			// Now change the resource manager bindings
			ResourceIdentifier identifier(data->staging_preloaded_textures[index].path);

			// If it doesn't exist
			if (!resource_manager->Exists(identifier, ResourceType::Texture)) {
				resource_manager->AddResource(
					identifier,
					ResourceType::Texture,
					data->staging_preloaded_textures[index].texture.view,
					false,
					data->staging_preloaded_textures[index].last_write_time
				);
			}
			else {
				resource_manager->RebindResource(identifier, ResourceType::Texture, data->staging_preloaded_textures[index].texture.view, false);
				resource_manager->RemoveReferenceCountForResource(identifier, ResourceType::Texture);
			}
		}
	}

	// Clear the buffer
	data->staging_preloaded_textures.Clear();

	// The flags need to be reset
	data->preload_flags = 0;
}

void FileExplorerRegisterPreloadTextures(EditorState* editor_state) {
	UISystem* ui_system = editor_state->ui_system;
	FileExplorerData* data = editor_state->file_explorer_data;
	ResourceManager* resource_manager = ui_system->m_resource_manager;

	bool* was_verified = (bool*)ECS_STACK_ALLOC(sizeof(bool) * data->preloaded_textures.size);
	memset(was_verified, 0, sizeof(bool) * data->preloaded_textures.size);

	constexpr size_t MAX_PRELOADS = 256;
	FileExplorerPreloadTexture* _new_preloads = (FileExplorerPreloadTexture*)ECS_STACK_ALLOC(sizeof(FileExplorerPreloadTexture) * MAX_PRELOADS);
	CapacityStream<FileExplorerPreloadTexture> new_preloads(_new_preloads, 0, MAX_PRELOADS);

	struct FunctorData {
		EditorState* editor_state;
		FileExplorerData* explorer_data;
		CapacityStream<FileExplorerPreloadTexture>* new_preloads;
		bool* was_verified;
	};

	FunctorData functor_data = { editor_state, data, &new_preloads, was_verified };

	auto functor = [](Stream<wchar_t> path, void* _data) {
		FunctorData* data = (FunctorData*)_data;

		uint2 texture_dimensions = GetTextureDimensions(path);

		// If it is big enough
		unsigned int total_pixel_count = texture_dimensions.x * texture_dimensions.y;
		if (total_pixel_count > 512 * 512) {
			// Check to see if this texture already exists
			Stream<wchar_t> stream_path = path;
			size_t file_last_write = OS::GetFileLastWrite(path);
			bool is_alive = false;
			for (unsigned int index = 0; index < data->explorer_data->preloaded_textures.size; index++) {
				if (stream_path == data->explorer_data->preloaded_textures[index].path) {
					// Only keep this texture alive if it wasn't changed since last time
					data->was_verified[index] = file_last_write <= data->explorer_data->preloaded_textures[index].last_write_time;
					is_alive = data->was_verified[index];
					// exit the loop
					index = data->explorer_data->preloaded_textures.size;
				}
			}

			// Add this texture to further processing only if it should not be kept alive and there is enough space in the new preloads stream
			if (!is_alive && data->new_preloads->size < data->new_preloads->capacity) {
				if (data->explorer_data->staging_preloaded_textures.size < data->explorer_data->staging_preloaded_textures.capacity) {
					FileExplorerPreloadTexture preload_texture;
					preload_texture.last_write_time = file_last_write;
					preload_texture.path = StringCopy(data->editor_state->EditorAllocator(), stream_path);
					preload_texture.texture = nullptr;
					data->explorer_data->staging_preloaded_textures.Add(preload_texture);
				}
			}
		}
		return true;
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, assests_folder, 256);
	GetProjectAssetsFolder(editor_state, assests_folder);
	Stream<wchar_t> extensions[] = {
		L".jpg",
		L".png",
		L".tiff",
		L".bmp",
		L".hdr",
		L".tga"
	};
	ForEachFileInDirectoryRecursiveWithExtension(assests_folder, { extensions, std::size(extensions) }, &functor_data, functor);

	// Evict all the textures that must not be kept alive
	for (int64_t index = 0; index < data->preloaded_textures.size; index++) {
		if (!was_verified[index]) {
			const wchar_t* texture_path = data->preloaded_textures[index].path.buffer;
			ResourceIdentifier identifier(texture_path);

			// Deallocate the path
			Deallocate(editor_state->EditorAllocator(), texture_path);

			if (resource_manager->Exists(identifier, ResourceType::Texture)) {
				// Remove the texture from the resource manager - it will be released now
				resource_manager->UnloadTexture(texture_path);
				// We need to redraw the explorer as well - to avoid any problem where this texture
				// Was taken into the snapshot and it will be incorrectly used
				data->should_redraw = true;
			}

			// Remove the texture and decrement the index to stay in the same spot
			data->preloaded_textures.RemoveSwapBack(index);
			// Also modify the was verified field in order to keep them synced
			was_verified[index] = was_verified[data->preloaded_textures.size];
			index--;
		}
	}

	// Set the preload started flag, only if there are textures to preload
	if (data->staging_preloaded_textures.size > 0) {
		data->preload_flags = SetFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED);
	}
}

void FileExplorerLaunchPreloadTextures(EditorState* editor_state) {
	TaskManager* task_manager = editor_state->task_manager;
	FileExplorerData* data = editor_state->file_explorer_data;

	unsigned int thread_count = task_manager->GetThreadCount();
 	unsigned int per_thread_textures = data->staging_preloaded_textures.size / thread_count;
	unsigned int per_thread_remainder = data->staging_preloaded_textures.size % thread_count;
	unsigned int total_texture_count = 0;

	if (per_thread_textures > 0 || per_thread_remainder > 0) {
		data->preload_flags = SetFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED);

		Semaphore* semaphore = (Semaphore*)Allocate(editor_state->MultithreadedEditorAllocator(), sizeof(Semaphore));
		semaphore->ClearCount();
		unsigned int launch_thread_count = per_thread_textures > 0 ? thread_count : per_thread_remainder;
		semaphore->target.store(launch_thread_count, ECS_RELAXED);

		for (size_t index = 0; index < launch_thread_count; index++) {
			bool has_remainder = per_thread_remainder > 0;
			per_thread_remainder -= has_remainder;
			unsigned int thread_texture_count = per_thread_textures + has_remainder;

			FileExplorerPreloadTextureThreadTaskData thread_data;
			thread_data.preload_textures = { data->staging_preloaded_textures.buffer + total_texture_count, thread_texture_count };
			thread_data.editor_state = editor_state;
			thread_data.semaphore = semaphore;

			ThreadTask thread_task = ECS_THREAD_TASK_NAME(FileExplorerPreloadTextureThreadTask, &thread_data, sizeof(thread_data));
			task_manager->AddDynamicTaskAndWake(thread_task);

			total_texture_count += thread_texture_count;
		}
	}

	data->preload_flags = SetFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_LAUNCHED_THREADS);
}

#pragma endregion

#pragma region Generate Mesh Thumbnails

void FileExplorerReleaseMeshThumbnail(EditorState* editor_state, FileExplorerData* explorer_data, unsigned int table_index) {
	auto thumbnail = explorer_data->mesh_thumbnails.GetValueFromIndex(table_index);
	// Release the resource view and the memory allocated for the resource identifier
	ResourceIdentifier identifier = explorer_data->mesh_thumbnails.GetIdentifierFromIndex(table_index);

	editor_state->editor_allocator->Deallocate(identifier.ptr);
	// Release the resources only if the thumbnail could be loaded
	if (thumbnail.could_be_read) {
		Graphics* graphics = editor_state->UIGraphics();
		graphics->FreeView(thumbnail.texture);
	}

	explorer_data->mesh_thumbnails.EraseFromIndex(table_index);
	// Set the redraw to true just in case
	explorer_data->should_redraw = true;
}

void FileExplorerGenerateMeshThumbnails(EditorState* editor_state) {
	FileExplorerData* data = editor_state->file_explorer_data;

	// Evict out of data thumbnails
	data->mesh_thumbnails.ForEachIndex([&](unsigned int index) {
		ResourceIdentifier identifier = data->mesh_thumbnails.GetIdentifierFromIndex(index);

		Stream<wchar_t> path = { identifier.ptr, identifier.size };
		// If the mesh was changed or deleted, remove it
		size_t last_write = OS::GetFileLastWrite(path);
		if (!ExistsFileOrFolder(path) || last_write > data->mesh_thumbnails.GetValueFromIndex(index).last_write_time) {
			FileExplorerReleaseMeshThumbnail(editor_state, data, index);
			return true;
		}

		return false;
	});

	struct FunctorData {
		MemoryManager* editor_allocator;
		ResourceManager* resource_manager;
		FileExplorerData* explorer_data;
		unsigned char thumbnail_count = 0;
	};

	FunctorData functor_data = { editor_state->editor_allocator, editor_state->ui_resource_manager, data };

	auto functor = [](Stream<wchar_t> path, void* _data) {
		FunctorData* data = (FunctorData*)_data;

		// If the path doesn't exist, record it, add it to the GPU tasks and end the loop
		// Also add it to the 
		if (data->explorer_data->mesh_thumbnails.Find(path) == -1) {
			// Allocate the identifier for the hash table
			Stream<wchar_t> allocated_path = StringCopy(data->editor_allocator, path);
			FileExplorerMeshThumbnail thumbnail;

			//Try to read the mesh here and create it's buffers GPU buffers
			CoalescedMesh* mesh = data->resource_manager->LoadCoalescedMeshImplementation(path, false);
			// If the mesh is nullptr, the read failed
			thumbnail.could_be_read = mesh != nullptr;
			thumbnail.last_write_time = OS::GetFileLastWrite(path);
			thumbnail.texture = nullptr;
			if (thumbnail.could_be_read) {
				// Call the GLTFThumbnail generator
				GLTFThumbnail gltf_thumbnail = GLTFGenerateThumbnail(data->resource_manager->m_graphics, FILE_EXPLORER_MESH_THUMBNAIL_TEXTURE_SIZE, &mesh->mesh);
				thumbnail.texture = gltf_thumbnail.texture;

				// Free the coalesced mesh
				data->resource_manager->UnloadCoalescedMeshImplementation(mesh, false);
			}

			// Update the hash table
			data->explorer_data->mesh_thumbnails.InsertDynamic(data->editor_allocator, thumbnail, allocated_path);

			data->thumbnail_count++;
			// We also need to redraw the explorer
			data->explorer_data->should_redraw = true;
			// Exit the search - only a thumbnail is generated per frame
			return data->thumbnail_count < MAX_MESH_THUMBNAILS_PER_FRAME;
		}
		return true;
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, assests_folder, 256);
	GetProjectAssetsFolder(editor_state, assests_folder);
	ForEachFileInDirectoryRecursiveWithExtension(assests_folder, { ASSET_MESH_EXTENSIONS, std::size(ASSET_MESH_EXTENSIONS) }, &functor_data, functor);

	if (functor_data.thumbnail_count == MAX_MESH_THUMBNAILS_PER_FRAME) {
		// In another 30 ms the lazy evaluation should be triggered again (there might be more thumbnails to generate). 
		// A couple of frames will run in between helping to preserve the fluidity of the editor meantime
		EditorStateLazyEvaluationSet(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_MESH_THUMBNAIL, FILE_EXPLORER_MESH_THUMBNAIL_LAZY_EVALUATION - 30);
	}
}

#pragma endregion

template<bool (*CreateFunction)(const EditorState*, Stream<wchar_t>, WindowTable*)>
struct CreateAssetFileStruct {
	static void Function(ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		EditorState* editor_state = (EditorState*)_data;
		CapacityStream<char>* additional_data = (CapacityStream<char>*)_additional_data;
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		absolute_path.CopyOther(editor_state->file_explorer_data->current_directory);
		absolute_path.Add(ECS_OS_PATH_SEPARATOR);
		ConvertASCIIToWide(absolute_path, *additional_data);

		WindowTable* window_table = system->GetWindowTable(window_index);

		Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, absolute_path);

		bool success = CreateFunction(editor_state, relative_path, window_table);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to create asset file {#}.", absolute_path);
			EditorSetConsoleError(error_message);
		}
	}
};

//#define MEASURE_DURATION

void FileExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
#ifdef MEASURE_DURATION
	static float average_values = 0.0f;
	static int average_count = 0;
	Timer my_timer;
	my_timer.SetNewStart();
#endif

	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	FileExplorerData* data = editor_state->file_explorer_data;

	if (initialize) {
		ProjectFile* project_file = editor_state->project_file;
		data->current_directory.CopyOther(project_file->path);
		data->current_directory.AddAssert(ECS_OS_PATH_SEPARATOR);
		data->current_directory.AddStreamAssert(Path(PROJECT_ASSETS_RELATIVE_PATH, wcslen(PROJECT_ASSETS_RELATIVE_PATH)));
		data->current_directory.AssertCapacity(1);
		data->current_directory[data->current_directory.size] = L'\0';

		void* allocation = drawer.GetMainAllocatorBuffer(data->file_functors.MemoryOf(FILE_FUNCTORS_CAPACITY));
		data->file_functors.InitializeFromBuffer(allocation, FILE_FUNCTORS_CAPACITY);

#pragma region Deselection Handlers - Main

		// The main menu
		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FILE_EXPLORER_DESELECTION_MENU_ROW_COUNT);
		data->deselection_menu_handlers.InitializeFromBuffer(allocation, 0, FILE_EXPLORER_DESELECTION_MENU_ROW_COUNT);

		data->deselection_menu_handlers[DESELECTION_MENU_PASTE] = { FileExplorerPasteElements, editor_state, 0 };

		auto reset_copied_files = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			FileExplorerResetCopiedFiles((FileExplorerData*)_data);
		};
		data->deselection_menu_handlers[DESELECTION_MENU_RESET_COPIED_FILES] = { reset_copied_files, data, 0 };
			
#pragma endregion

#pragma region Deselection Handlers - Create submenu

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FILE_EXPLORER_DESELECTION_MENU_CREATE_ROW_COUNT);
		data->deselection_create_menu_handlers.InitializeFromBuffer(allocation, 0, FILE_EXPLORER_DESELECTION_MENU_CREATE_ROW_COUNT);

		constexpr auto create_sampler_file = [](const EditorState* editor_state, Stream<wchar_t> relative_path, WindowTable* window_table) {
			return CreateSamplerFile(editor_state, relative_path);
		};

#define SHADER_INPUT_RESOURCE_NAME "SHADER_TYPE_INPUT"

		constexpr auto create_shader_file = [](const EditorState* editor_state, Stream<wchar_t> relative_path, WindowTable* window_table) {
			ECS_SHADER_TYPE shader_type = *(ECS_SHADER_TYPE*)window_table->GetValue(SHADER_INPUT_RESOURCE_NAME);
			return CreateShaderFile(editor_state, relative_path, shader_type);
		};

		constexpr auto create_material_file = [](const EditorState* editor_state, Stream<wchar_t> relative_path, WindowTable* window_table) {
			return CreateMaterialFile(editor_state, relative_path);
		};

		constexpr auto create_misc_file = [](const EditorState* editor_state, Stream<wchar_t> relative_path, WindowTable* window_table) {
			return CreateMiscFile(editor_state, relative_path);
		};

		data->deselection_create_menu_handler_data = (TextInputWizardData*)drawer.GetMainAllocatorBuffer(sizeof(TextInputWizardData) * FILE_EXPLORER_DESELECTION_MENU_CREATE_ROW_COUNT);
		data->deselection_create_menu_handler_data[DESELECTION_MENU_CREATE_SAMPLER] = { "Sampler name", "Choose a name", CreateAssetFileStruct<create_sampler_file>::Function, editor_state, 0 };
		data->deselection_create_menu_handler_data[DESELECTION_MENU_CREATE_SHADER] = { "Shader name", "Choose a name", CreateAssetFileStruct<create_shader_file>::Function, editor_state, 0 };
		data->deselection_create_menu_handler_data[DESELECTION_MENU_CREATE_MATERIAL] = { "Material name", "Choose a name", CreateAssetFileStruct<create_material_file>::Function, editor_state, 0 };
		data->deselection_create_menu_handler_data[DESELECTION_MENU_CREATE_MISC] = { "Misc name", "Choose a name", CreateAssetFileStruct<create_misc_file>::Function, editor_state, 0 };;
			
		auto shader_file_input = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawer* drawer = (UIDrawer*)_data;
			EditorState* editor_state = (EditorState*)_additional_data;
			ECS_SHADER_TYPE* shader_type = nullptr;
			if (drawer->initializer) {
				shader_type = drawer->GetMainAllocatorBufferAndStoreAsResource<ECS_SHADER_TYPE>(SHADER_INPUT_RESOURCE_NAME);
				*shader_type = ECS_SHADER_VERTEX;
			}
			else {
				shader_type = (ECS_SHADER_TYPE*)drawer->GetResource(SHADER_INPUT_RESOURCE_NAME);
			}

			UIConfigWindowDependentSize window_dependent_size;
			UIDrawConfig config;
			config.AddFlag(window_dependent_size);

			Stream<Stream<char>> shader_type_labels = editor_state->ModuleReflectionManager()->GetEnum(STRING(ECS_SHADER_TYPE))->fields;
			drawer->ComboBox(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Shader Type", shader_type_labels, shader_type_labels.size, (unsigned char*)shader_type);
		};

		data->deselection_create_menu_handler_data[DESELECTION_MENU_CREATE_SHADER].AddExtraElement(shader_file_input, editor_state, 0);

		auto create_folder_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			EditorState* editor_state = (EditorState*)_data;
			
			CreateNewFileOrFolderWizardData wizard_data;
			wizard_data.input_name = "Choose a name";
			wizard_data.path = editor_state->file_explorer_data->current_directory;
			wizard_data.window_name = "Create folder";
			wizard_data.extension = {};
			wizard_data.text_input_available_color = EDITOR_GREEN_COLOR;
			wizard_data.text_input_conflict_color = EDITOR_RED_COLOR;
			wizard_data.warn_icon_color = EDITOR_YELLOW_COLOR;
			CreateNewFileOrFolderWizard(&wizard_data, system);
		};

		data->deselection_create_menu_handlers[DESELECTION_MENU_CREATE_FOLDER] = { create_folder_action, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->deselection_create_menu_handlers[DESELECTION_MENU_CREATE_SCENE] = { CreateEmptySceneAction, editor_state, 0, ECS_UI_DRAW_SYSTEM };

#define SET_HANDLER(index) data->deselection_create_menu_handlers[index] = { CreateTextInputWizardAction, data->deselection_create_menu_handler_data + index, 0, ECS_UI_DRAW_SYSTEM };
			
		SET_HANDLER(DESELECTION_MENU_CREATE_SAMPLER);
		SET_HANDLER(DESELECTION_MENU_CREATE_SHADER);
		SET_HANDLER(DESELECTION_MENU_CREATE_MATERIAL);
		SET_HANDLER(DESELECTION_MENU_CREATE_MISC);

#undef SET_HANDLER

		data->deselection_menu_create_state.row_count = FILE_EXPLORER_DESELECTION_MENU_CREATE_ROW_COUNT;
		data->deselection_menu_create_state.submenu_index = 1;
		data->deselection_menu_create_state.left_characters = FILE_EXPLORER_DESELECTION_MENU_CREATE_CHARACTERS;
		data->deselection_menu_create_state.click_handlers = data->deselection_create_menu_handlers.buffer;

#pragma endregion

#pragma region File Right Click Handlers

		auto CopyPath = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ECS_STACK_CAPACITY_STREAM(char, ascii_path, 256);
			Stream<wchar_t>* path = (Stream<wchar_t>*)_data;
			ConvertWideCharsToASCII(*path, ascii_path);
			system->m_application->WriteTextToClipboard(ascii_path);
		};

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FILE_RIGHT_CLICK_ROW_COUNT);
		data->file_right_click_handlers.InitializeFromBuffer(allocation, FILE_RIGHT_CLICK_ROW_COUNT, FILE_RIGHT_CLICK_ROW_COUNT);
		data->file_right_click_handlers[FILE_RIGHT_CLICK_OPEN] = { OpenFileWithDefaultApplicationStreamAction, &data->right_click_stream, 0, ECS_UI_DRAW_SYSTEM };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_SHOW_IN_EXPLORER] = { LaunchFileExplorerStreamAction, &data->right_click_stream, 0, ECS_UI_DRAW_SYSTEM };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_DELETE] = { FileExplorerDeleteSelection, data, 0, ECS_UI_DRAW_SYSTEM };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_RENAME] = { FileExplorerMenuRenameCallback, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_COPY_PATH] = { CopyPath, &data->right_click_stream, 0 };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_COPY_SELECTION] = { FileExplorerCopySelection, data, 0 };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_CUT_SELECTION] = { FileExplorerCutSelection, data, 0 };

#pragma endregion

#pragma region Folder Right Click Handlers

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FOLDER_RIGHT_CLICK_ROW_COUNT);
		data->folder_right_click_handlers.InitializeFromBuffer(allocation, FOLDER_RIGHT_CLICK_ROW_COUNT, FOLDER_RIGHT_CLICK_ROW_COUNT);
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_OPEN] = { FileExplorerChangeDirectoryFromFile, editor_state, 0 };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_SHOW_IN_EXPLORER] = { LaunchFileExplorerStreamAction, &data->right_click_stream, 0, ECS_UI_DRAW_SYSTEM };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_DELETE] = { FileExplorerDeleteSelection, data, 0, ECS_UI_DRAW_SYSTEM };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_RENAME] = { FileExplorerMenuRenameCallback, editor_state, 0, ECS_UI_DRAW_SYSTEM };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_COPY_PATH] = { CopyPath, &data->right_click_stream, 0 };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_COPY_SELECTION] = { FileExplorerCopySelection, data, 0 };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_CUT_SELECTION] = { FileExplorerCutSelection, data, 0 };

#pragma endregion

#pragma region Mesh Export Materials Click Handlers

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * MESH_EXPORT_MATERIALS_CLICK_ROW_COUNT);
		data->mesh_export_materials_click_handlers.InitializeFromBuffer(allocation, MESH_EXPORT_MATERIALS_CLICK_ROW_COUNT, MESH_EXPORT_MATERIALS_CLICK_ROW_COUNT);

		size_t mesh_exports_total_data_size = sizeof(MeshExportAllActionData) + sizeof(MeshExportSelectionActionData);
		allocation = drawer.GetMainAllocatorBuffer(mesh_exports_total_data_size);
		MeshExportAllActionData* export_all_data = (MeshExportAllActionData*)allocation;
		export_all_data->editor_state = editor_state;
		export_all_data->explorer_data = data;
		MeshExportSelectionActionData* export_selection_data = (MeshExportSelectionActionData*)OffsetPointer(export_all_data, sizeof(*export_all_data));
		export_selection_data->editor_state = editor_state;
		export_selection_data->explorer_data = data;

		data->mesh_export_materials_click_handlers[MESH_EXPORT_MATERIALS_ALL_HERE] = { MeshExportAllAction<false>, export_all_data, 0 };
		data->mesh_export_materials_click_handlers[MESH_EXPORT_MATERIALS_ALL_TO_FOLDER] = { MeshExportAllAction<true>, export_all_data, 0 };
		data->mesh_export_materials_click_handlers[MESH_EXPORT_MATERIALS_SELECTION_HERE] = { MeshExportSelectionAction<false>, export_selection_data, 0 };
		data->mesh_export_materials_click_handlers[MESH_EXPORT_MATERIALS_SELECTION_TO_FOLDER] = { MeshExportSelectionAction<true>, export_selection_data, 0 };

#pragma endregion

		ResourceIdentifier identifier;
		unsigned int hash;

#define ADD_FUNCTOR(action, string) identifier = ResourceIdentifier(string); \
data->file_functors.Insert(action, identifier);

		ADD_FUNCTOR(FileCDraw, L".c");
		ADD_FUNCTOR(FileCppDraw, L".cpp");
		ADD_FUNCTOR(FileCppDraw, L".h");
		ADD_FUNCTOR(FileCppDraw, L".hpp");
		ADD_FUNCTOR(FileConfigDraw, L".config");
		ADD_FUNCTOR(FileTextDraw, L".txt");
		ADD_FUNCTOR(FileTextDraw, L".doc");
		ADD_FUNCTOR(FileShaderDraw, ECS_SHADER_EXTENSION);
		ADD_FUNCTOR(FileShaderDraw, ECS_SHADER_INCLUDE_EXTENSION);
		for (size_t index = 0; index < std::size(ASSET_SHADER_EXTENSIONS); index++) {
			ADD_FUNCTOR(FileAssetShaderDraw, ASSET_SHADER_EXTENSIONS[index]);
		}
		for (size_t index = 0; index < std::size(ASSET_GPU_SAMPLER_EXTENSIONS); index++) {
			ADD_FUNCTOR(FileAssetGPUSamplerDraw, ASSET_GPU_SAMPLER_EXTENSIONS[index]);
		}
		for (size_t index = 0; index < std::size(ASSET_MISC_EXTENSIONS); index++) {
			ADD_FUNCTOR(FileAssetMiscDraw, ASSET_MISC_EXTENSIONS[index]);
		}
		for (size_t index = 0; index < std::size(ASSET_MESH_EXTENSIONS); index++) {
			ADD_FUNCTOR(FileMeshDraw, ASSET_MESH_EXTENSIONS[index]);
		}
		for (size_t index = 0; index < std::size(ASSET_TEXTURE_EXTENSIONS); index++) {
			ADD_FUNCTOR(FileTextureDraw, ASSET_TEXTURE_EXTENSIONS[index]);
		}
		for (size_t index = 0; index < std::size(ASSET_MATERIAL_EXTENSIONS); index++) {
			ADD_FUNCTOR(FileMaterialDraw, ASSET_MATERIAL_EXTENSIONS[index]);
		}
		ADD_FUNCTOR(FileSceneDraw, EDITOR_SCENE_EXTENSION);
		ADD_FUNCTOR(FileInputRecordingFileDraw, EDITOR_INPUT_RECORDING_FILE_EXTENSION);
		ADD_FUNCTOR(FileStateRecordingFileDraw, EDITOR_STATE_RECORDING_FILE_EXTENSION);

#undef ADD_FUNCTOR

		// Reduce the next roy y offset
		drawer.layout.next_row_y_offset *= 0.5f;

		EditorStateLazyEvaluationTrigger(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_TEXTURES);
	}

#pragma region Deselection Menu

	auto DeselectClick = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
			FileExplorerData* data = (FileExplorerData*)_data;
			FileExplorerResetSelectedFiles(data);
			data->starting_shift_index = data->ending_shift_index = -1;
		}
	};

	UIActionHandler deselect_click = { DeselectClick, data, 0 };
	drawer.SetWindowClickable(&deselect_click);

	UIDrawerMenuRightClickData deselection_menu_data;
	deselection_menu_data.name = "Deselection Menu";
	deselection_menu_data.window_index = 0;
	deselection_menu_data.state.click_handlers = data->deselection_menu_handlers.buffer;
	deselection_menu_data.state.left_characters = FILE_EXPLORER_DESELECTION_MENU_CHARACTERS;
	deselection_menu_data.state.row_count = FILE_EXPLORER_DESELECTION_MENU_ROW_COUNT;
	deselection_menu_data.state.submenues = &data->deselection_menu_create_state;
	deselection_menu_data.state.submenu_index = 0;
	deselection_menu_data.state.row_has_submenu = FILE_EXPLORER_DESELECTION_HAS_SUBMENUES;

	UIActionHandler deselect_right_click_handler = { RightClickMenu, &deselection_menu_data, sizeof(deselection_menu_data), ECS_UI_DRAW_SYSTEM };
	drawer.SetWindowClickable(&deselect_right_click_handler, ECS_MOUSE_RIGHT);

#pragma endregion

#pragma region Acquire drag drop

	ECS_STACK_CAPACITY_STREAM(Stream<char>, accept_drag_names, 1);
	accept_drag_names[0] = ENTITIES_UI_DRAG_NAME;
	accept_drag_names.size = accept_drag_names.capacity;

	drawer.PushDragDrop(FILE_EXPLORER_DRAG_NAME, accept_drag_names, true, EDITOR_GREEN_COLOR, BORDER_DRAG_THICKNESS);
	drawer.HandleAcquireDrag(UI_CONFIG_LATE_DRAW, drawer.GetRegionPosition(), drawer.GetRegionScale());

#pragma endregion

#pragma region Header

	float row_padding = drawer.layout.next_row_padding;
	float next_row_offset = drawer.layout.next_row_y_offset;
	drawer.SetDrawMode(ECS_UI_DRAWER_NOTHING);
	drawer.SetRowPadding(0.001f);
	drawer.SetNextRowYOffset(0.0025f);

	UIDrawConfig header_config;

	UIConfigBorder border;
	border.color = drawer.color_theme.borders;
	header_config.AddFlag(border);

	UIConfigMenuSprite plus_sprite;
	plus_sprite.texture = ECS_TOOLS_UI_TEXTURE_PLUS;
	plus_sprite.color = drawer.color_theme.text;
	header_config.AddFlag(plus_sprite);

	const size_t HEADER_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LATE_DRAW | UI_CONFIG_DO_NOT_ADVANCE;
	
	UIConfigAbsoluteTransform absolute_transform;
	absolute_transform.position = drawer.GetCurrentPositionStatic();
	absolute_transform.scale.x = drawer.layout.default_element_x * 2.5f;
	absolute_transform.scale.y = drawer.layout.default_element_y;
	header_config.AddFlag(absolute_transform);
	
	UIConfigTextInputHint hint;
	hint.characters = "Search";
	header_config.AddFlag(hint);

	drawer.TextInput(
		HEADER_CONFIGURATION | UI_CONFIG_BORDER | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_NO_NAME,
		header_config, 
		"Input", 
		&data->filter_stream
	);
	absolute_transform.position.x += drawer.layout.element_indentation;

	if (data->current_directory.size > 0) {
		// It will incorrectly set the min render bound - so restore it after the call
		float min_x_render_bound = drawer.min_render_bounds.x;
		drawer.Indent();
		drawer.min_render_bounds.x = min_x_render_bound;

		ProjectFile* project_file = editor_state->project_file;
		ECS_STACK_CAPACITY_STREAM(char, ascii_path, 512);
		Path wide_path = { data->current_directory.buffer + project_file->path.size + 1, data->current_directory.size - project_file->path.size - 1 };
		ConvertWideCharsToASCII(wide_path, ascii_path);
		ascii_path.AddAssert('\0');
		ascii_path.size--;

		size_t total_path_size = project_file->path.size + 1;
		char* delimiter = strchr(ascii_path.buffer, ECS_OS_PATH_SEPARATOR_ASCII);
		delimiter = delimiter == nullptr ? ascii_path.buffer : delimiter;
		ASCIIPath current_path(ascii_path.buffer, delimiter - ascii_path.buffer);

		const auto mouse_state = drawer.system->m_mouse->Get(ECS_MOUSE_LEFT);
		while (current_path.size > 0) {
			*delimiter = '\0';

			PathButtonData button_data;
			button_data.data = data;
			button_data.size = total_path_size + current_path.size;

			header_config.flag_count = 0;

			absolute_transform.position.x += absolute_transform.scale.x;
			absolute_transform.scale.x = drawer.GetLabelScale(current_path.buffer).x;
			header_config.AddFlag(absolute_transform);

			drawer.Button(
				HEADER_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y,
				header_config,
				current_path.buffer, 
				{ FileExplorerPathButtonAction, &button_data, sizeof(button_data) }
			);

			Color rectangle_color = drawer.color_theme.background;
			float2 rectangle_position = absolute_transform.position - drawer.region_render_offset;
			if (IsPointInRectangle(drawer.mouse_position, rectangle_position, absolute_transform.scale) && (mouse_state == ECS_BUTTON_PRESSED || mouse_state == ECS_BUTTON_HELD)) {
				rectangle_color = drawer.color_theme.theme;
			}
			drawer.SolidColorRectangle(UI_CONFIG_LATE_DRAW, rectangle_position, absolute_transform.scale, rectangle_color);

			header_config.flag_count--;
			absolute_transform.position.x += absolute_transform.scale.x;
			absolute_transform.scale.x = drawer.GetLabelScale("\\").x;
			header_config.AddFlag(absolute_transform);
			drawer.TextLabel(
				HEADER_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y,
				header_config,
				"\\"
			);
			drawer.SolidColorRectangle(UI_CONFIG_LATE_DRAW, absolute_transform.position - drawer.region_render_offset, absolute_transform.scale, drawer.color_theme.background);

			total_path_size += current_path.size + 1;

			current_path.buffer = current_path.buffer + current_path.size + 1;
			delimiter = strchr(current_path.buffer, ECS_OS_PATH_SEPARATOR_ASCII);
			delimiter = delimiter == nullptr ? current_path.buffer : delimiter;
			current_path.size = (uintptr_t)delimiter - (uintptr_t)current_path.buffer;
		}

		absolute_transform.position.x += absolute_transform.scale.x;

		PathButtonData button_data;
		button_data.data = data;
		button_data.size = total_path_size + ((uintptr_t)ascii_path.buffer + ascii_path.size) - (uintptr_t)current_path.buffer;

		header_config.flag_count = 0;
		header_config.AddFlag(absolute_transform);

		float label_scale = drawer.GetLabelScale(current_path.buffer).x;
		drawer.Button(
			HEADER_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y,
			header_config, 
			current_path.buffer, 
			{ FileExplorerPathButtonAction, &button_data, sizeof(button_data) }
		);

		Color rectangle_color = drawer.color_theme.background;
		float2 rectangle_position = absolute_transform.position - drawer.region_render_offset;
		float2 rectangle_scale = { label_scale, drawer.layout.default_element_y };
		if (IsPointInRectangle(drawer.mouse_position, rectangle_position, rectangle_scale) && (mouse_state == ECS_BUTTON_PRESSED || mouse_state == ECS_BUTTON_HELD)) {
			rectangle_color = drawer.color_theme.theme;
		}
		drawer.SolidColorRectangle(UI_CONFIG_LATE_DRAW, rectangle_position, rectangle_scale, rectangle_color);
	}

	drawer.SetNextRowYOffset(next_row_offset);
	drawer.SetRowPadding(row_padding);
	drawer.UpdateCurrentRowScale(drawer.layout.default_element_y);
	drawer.NextRow(2.0f);

#pragma endregion

	// Element Draw
	if (!initialize) {

		drawer.SetDrawMode(ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE, 2, THUMBNAIL_TO_LABEL_SPACING);
		UIDrawConfig config;

		UIConfigRelativeTransform transform;
		transform.scale = drawer.GetRelativeTransformFactors(drawer.GetSquareScaleScaled(THUMBNAIL_SIZE));
		config.AddFlag(transform);

		ForEachData for_each_data;
		for_each_data.config = &config;
		for_each_data.editor_state = editor_state;
		for_each_data.drawer = &drawer;
		for_each_data.element_count = 0;
		for_each_data.draw_configuration = UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE;
		for_each_data.select_function = FileExplorerSelectFromIndexNothing;
		if (HasFlag(data->flags, FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES)) {
			FileExplorerResetSelectedFiles(data);
			for_each_data.select_function = FileExplorerSelectFromIndexShift;
			data->flags = ClearFlag(data->flags, FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES);
		}

		auto directory_functor = [](Stream<wchar_t> path, void* __data) {
			ForEachData* _data = (ForEachData*)__data;

			FileExplorerData* data = _data->editor_state->file_explorer_data;
			UIDrawConfig* config = _data->config;
			UIDrawer* drawer = _data->drawer;

			size_t config_size = config->flag_count;

			Path stream_path = StringCopy(&data->temporary_allocator, path);

			SelectableData selectable_data;
			selectable_data.editor_state = _data->editor_state;
			selectable_data.selection = stream_path;
			selectable_data.index = _data->element_count;

			bool is_valid = true;
			ActionData action_data = drawer->GetDummyActionData();
			action_data.additional_data = &is_valid;
			action_data.data = &selectable_data;
			FILTER_FUNCTORS[data->filter_stream.size > 0](&action_data);

			_data->select_function(data, _data->element_count, stream_path);

			if (is_valid) {
				bool is_selected = FileExplorerIsElementSelected(data, stream_path);
				unsigned char color_alpha = FileExplorerIsElementCut(data, stream_path) ? COLOR_CUT_ALPHA : 255;

				Color white_color = ECS_COLOR_WHITE;
				white_color.alpha = color_alpha;
				drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, *config, ECS_TOOLS_UI_TEXTURE_FOLDER, white_color);

				UIConfigRectangleGeneral general_action;
				general_action.handler = { FileExplorerDirectorySelectable, &selectable_data, sizeof(selectable_data) };
				config->AddFlag(general_action);

				auto OnRightClickCallback = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					SelectableData* data = (SelectableData*)_data;
					unsigned int index = FileExplorerHasSelectedFile(data->editor_state, data->selection);
					if (index == -1) {
						ChangeFileExplorerFile(data->editor_state, data->selection, data->index);
					}
					else {
						FileExplorerData* explorer_data = data->editor_state->file_explorer_data;
						explorer_data->right_click_stream = explorer_data->selected_files[index];
					}
				};

				UIDrawerMenuRightClickData right_click_data;
				right_click_data.action = OnRightClickCallback;
				right_click_data.is_action_data_ptr = false;
				memcpy(right_click_data.action_data, &selectable_data, sizeof(selectable_data));
				right_click_data.name = "Folder File Explorer Menu";
				right_click_data.window_index = 0;
				right_click_data.state.click_handlers = data->folder_right_click_handlers.buffer;
				right_click_data.state.left_characters = FOLDER_RIGHT_CLICK_CHARACTERS;
				right_click_data.state.row_count = FOLDER_RIGHT_CLICK_ROW_COUNT;
				right_click_data.state.submenu_index = 0;

				FileExplorerDragData drag_data = { _data->editor_state, false };
				UIConfigRectangleClickable clickable_actions;
				clickable_actions.handlers[0] = { FileExplorerDrag, &drag_data, sizeof(drag_data), ECS_UI_DRAW_SYSTEM };
				clickable_actions.handlers[1] = { RightClickMenu, &right_click_data, sizeof(right_click_data), ECS_UI_DRAW_SYSTEM };
				clickable_actions.button_types[1] = ECS_MOUSE_RIGHT;
				config->AddFlag(clickable_actions);

				constexpr size_t RECTANGLE_CONFIGURATION = UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_VALIDATE_POSITION
					| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_RECTANGLE_GENERAL_ACTION | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION | UI_CONFIG_GET_TRANSFORM;

				float2 rectangle_position;
				float2 rectangle_scale;
				UIConfigGetTransform get_transform;
				get_transform.position = &rectangle_position;
				get_transform.scale = &rectangle_scale;
				config->AddFlag(get_transform);

				if (is_selected) {
					// Add the border highlight
					UIConfigColor color;
					color.color = drawer->color_theme.theme;
					color.color.alpha = color_alpha;
					config->AddFlag(color);

					UIConfigBorder border;
					border.thickness = drawer->GetDefaultBorderThickness();
					border.color = EDITOR_GREEN_COLOR;
					border.draw_phase = ECS_UI_DRAW_NORMAL;
					config->AddFlag(border);

					drawer->Rectangle(RECTANGLE_CONFIGURATION | UI_CONFIG_COLOR | UI_CONFIG_BORDER, *config);
				}
				else {
					drawer->Rectangle(RECTANGLE_CONFIGURATION, *config);
				}

				// Update the mouse index if it hovers this directory - only directories must make this check
				if (IsPointInRectangle(drawer->mouse_position, rectangle_position, rectangle_scale)) {
					_data->mouse_element_path->CopyOther(stream_path);
				}

				// Eliminate the config additions
				// It must be done before the label draw
				config->flag_count = config_size;

				FileExplorerLabelDraw(drawer, config, &selectable_data, is_selected, true);
				_data->element_count++;
			}


			return true;
		};

		auto file_functor = [](Stream<wchar_t> path, void* __data) {
			ForEachData* _data = (ForEachData*)__data;

			FileExplorerData* data = _data->editor_state->file_explorer_data;
			UIDrawConfig* config = _data->config;
			UIDrawer* drawer = _data->drawer;

			size_t config_size = config->flag_count;

			Path stream_path = StringCopy(&data->temporary_allocator, path);

			Path extension = PathExtension(stream_path);

			ResourceIdentifier identifier(extension);
			Action functor = FileBlankDraw;

			SelectableData selectable_data;
			selectable_data.editor_state = _data->editor_state;
			selectable_data.selection = stream_path;
			selectable_data.index = _data->element_count;

			bool is_valid = true;
			ActionData action_data = drawer->GetDummyActionData();
			action_data.system = drawer->system;
			action_data.data = &selectable_data;
			action_data.additional_data = &is_valid;

			_data->select_function(data, _data->element_count, stream_path);

			FILTER_FUNCTORS[data->filter_stream.size > 0](&action_data);

			if (is_valid) {
				bool is_selected = FileExplorerIsElementSelected(data, stream_path);
				unsigned char color_alpha = FileExplorerIsElementCut(data, stream_path) ? COLOR_CUT_ALPHA : 255;

				FileFunctorData functor_data;
				functor_data.for_each_data = _data;
				functor_data.color_alpha = color_alpha;
				action_data.additional_data = &functor_data;

				// Functor was initialized outside the if with FileBlankDraw
				bool has_functor = data->file_functors.TryGetValue(identifier, functor);
				bool is_mesh_draw = false;
				if (has_functor) {
					if (functor == FileTextureDraw) {
						if (HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED)) {
							// Check to see if it is a texture that is being preloaded
							for (size_t index = 0; index < data->staging_preloaded_textures.size; index++) {
								if (data->staging_preloaded_textures[index].path == stream_path) {
									// If it is a texture that is being preloaded, overwrite to blank file while it is being loaded
									functor = FileBlankDraw;
									// exit the loop by setting the index to the count
									index = data->staging_preloaded_textures.size;
								}
							}
						}
					}
					else if (functor == FileMeshDraw) {
						is_mesh_draw = true;
						FileExplorerMeshThumbnail thumbnail;
						// If the thumbnail has been generated, check to see if the texture has been finalized
						if (data->mesh_thumbnails.TryGetValue(ResourceIdentifier(stream_path.buffer, stream_path.size * sizeof(wchar_t)), thumbnail)) {
							// If the thumbnail could not be created, keep it mesh draw
							if (thumbnail.could_be_read) {
								// Replace the functor
								functor = FileMeshThumbnailDraw;
							}
						}
					}
				}
				else if (EditorStateHasFlag(_data->editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
					functor = FileBlankDraw;
				}
				functor(&action_data);

				UIDrawerMenuState nested_state[MESH_FILE_RIGHT_CLICK_ROW_COUNT];
				UIDrawerMenuState main_state;

				// This is not visible, it is used only for identification purposes
				// We need to change this identificator when using the file mesh menu
				Stream<char> right_click_menu_name = "File File Explorer Menu";
				main_state.submenu_index = 0;
				main_state.click_handlers = data->file_right_click_handlers.buffer;
				if (is_mesh_draw) {
					main_state.left_characters = MESH_FILE_RIGHT_CLICK_CHARACTERS;
					main_state.row_count = MESH_FILE_RIGHT_CLICK_ROW_COUNT;
					main_state.row_has_submenu = MESH_FILE_HAS_SUBMENUES;

					UIDrawerMenuState* mesh_export_state = nested_state + MESH_FILE_RIGHT_CLICK_ROW_COUNT - 1;
					mesh_export_state->left_characters = MESH_EXPORT_MATERIALS_CLICK_CHARACTERS;
					mesh_export_state->row_count = MESH_EXPORT_MATERIALS_CLICK_ROW_COUNT;
					mesh_export_state->click_handlers = data->mesh_export_materials_click_handlers.buffer;
					mesh_export_state->submenu_index = 1;

					main_state.submenues = nested_state;
					right_click_menu_name = "Mesh File Explorer Menu";
				}
				else {
					main_state.left_characters = FILE_RIGHT_CLICK_CHARACTERS;
					main_state.row_count = FILE_RIGHT_CLICK_ROW_COUNT;
				}

				struct OnRightClickData {
					EditorState* editor_state;
					unsigned int index;
					Stream<wchar_t> path;
				};

				OnRightClickData right_click_action_data;
				right_click_action_data.editor_state = _data->editor_state;
				right_click_action_data.path = stream_path;
				right_click_action_data.index = _data->element_count;

				auto OnRightClickAction = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					OnRightClickData* data = (OnRightClickData*)_data;
					unsigned int index = FileExplorerHasSelectedFile(data->editor_state, data->path);
					if (index == -1) {
						FileExplorerSetNewFile(data->editor_state, data->path, data->index);
					}
					else {
						FileExplorerData* explorer_data = data->editor_state->file_explorer_data;
						explorer_data->right_click_stream = explorer_data->selected_files[index];
						explorer_data->starting_shift_index = explorer_data->ending_shift_index = data->index;
					}
				};
				
				FileExplorerDragData drag_data = { _data->editor_state, false };
				UIConfigRectangleClickable clickable_actions;
				clickable_actions.handlers[0] = { FileExplorerDrag, &drag_data, sizeof(drag_data), ECS_UI_DRAW_SYSTEM };
				clickable_actions.handlers[1] = drawer->PrepareRightClickMenuHandler(
					right_click_menu_name, 
					&main_state, 
					{ OnRightClickAction, &right_click_action_data, sizeof(right_click_action_data) },
					drawer->SnapshotRunnableAllocator()
				);
				clickable_actions.button_types[1] = ECS_MOUSE_RIGHT;
				config->AddFlag(clickable_actions);

				float2 rectangle_position;
				float2 rectangle_scale;
				UIConfigGetTransform get_transform;
				get_transform.position = &rectangle_position;
				get_transform.scale = &rectangle_scale;
				config->AddFlag(get_transform);

				constexpr size_t RECTANGLE_CONFIGURATION = UI_CONFIG_GET_TRANSFORM | UI_CONFIG_RELATIVE_TRANSFORM |
					UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION;

				if (is_selected) {
					// Add the border highlight
					UIConfigBorder border;
					border.color = EDITOR_GREEN_COLOR;
					border.thickness = drawer->GetDefaultBorderThickness();
					border.draw_phase = ECS_UI_DRAW_NORMAL;
					config->AddFlag(border);

					drawer->Rectangle(RECTANGLE_CONFIGURATION | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_BORDER, *config);

					Color theme_color = drawer->color_theme.theme;
					theme_color.alpha = 100;
					drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_FIT_SPACE, *config, ECS_TOOLS_UI_TEXTURE_MASK, theme_color);

					config->flag_count--;
				}
				else {
					drawer->Rectangle(RECTANGLE_CONFIGURATION, *config);
				}

				ECS_STACK_CAPACITY_STREAM(wchar_t, null_terminated_path, 256);
				null_terminated_path.CopyOther(stream_path);
				null_terminated_path[stream_path.size] = L'\0';

				drawer->AddDoubleClickAction(
					0,
					rectangle_position,
					rectangle_scale,
					DOUBLE_CLICK_DURATION,
					{ FileExplorerSelectableBase, &selectable_data, sizeof(selectable_data) },
					{ OpenFileWithDefaultApplicationAction, null_terminated_path.buffer, (unsigned int)(sizeof(wchar_t) * (stream_path.size + 1)) },
					_data->element_count
				);

				// Eliminate the config additions
				// It must be done before the label draw
				config->flag_count = config_size;

				FileExplorerLabelDraw(drawer, config, &selectable_data, is_selected, false);
				_data->element_count++;

			}

			return true;
		};

		data->temporary_allocator.Clear();

		ECS_STACK_CAPACITY_STREAM(wchar_t, mouse_element_path, 256);
		for_each_data.mouse_element_path = &mouse_element_path;

		if (data->current_directory.size > 0) {
			ForEachInDirectory(data->current_directory, &for_each_data, directory_functor, file_functor);
		}

		// Check moved files
		if (HasFlag(data->flags, FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES)) {
			// If there is no element being hovered - skip it
			if (mouse_element_path.size > 0) {
				// If it is moved - check that it landed in a folder that is not selected
				Stream<Stream<wchar_t>> selected_files(data->selected_files.buffer, data->selected_files.size);
				if (FindString(mouse_element_path, selected_files) == -1) {
					// Move all the files - if one cannot be moved because it already exists, then ask permision
					// else skip it
					unsigned int error_file_count = 0;
					
					// Allocate a temporary buffer that will hold the error paths - if any - set it to a big size
					// and release it from the drawer after using it
					constexpr size_t ERROR_BUFFER_SIZE = 8192;
					size_t drawer_temp_allocator_marker = drawer.GetTempAllocatorMarker();
					CapacityStream<char> error_files(drawer.GetTempBuffer(ERROR_BUFFER_SIZE), 0, ERROR_BUFFER_SIZE);
					error_files.CopyOther("One or more files/folders could not be copied. These are: ");

					// For the files that already exist, allocate a buffer of indices that will point to
					// the invalid ones and at the final stage when commiting to the overwrite window
					// copy them to persistent memory
					void* _overwrite_files = drawer.GetTempBuffer(sizeof(unsigned int) * data->selected_files.size);
					Stream<unsigned int> overwrite_files(_overwrite_files, 0);

					for (size_t index = 0; index < data->selected_files.size; index++) {
						Stream<wchar_t> current_file = data->selected_files[index];
						bool error_code = FileCut(current_file, mouse_element_path);
						if (!error_code) {
							if (ExistsFileOrFolder(mouse_element_path)) {
								overwrite_files.Add(index);
							}
							else {
								error_file_count++;
								error_files.AddAssert('\n');
								error_files.AddAssert('\t');
								ConvertWideCharsToASCII(current_file, error_files);
							}
						}
					}

					if (error_file_count > 0) {
						EditorSetConsoleError(error_files);
					}

					if (overwrite_files.size > 0) {
						// The overwrite wizard will release the selected files
						FileExplorerSelectOverwriteFilesData wizard_data;
						wizard_data.destination = mouse_element_path;
						wizard_data.explorer_data = data;
						wizard_data.overwrite_files = overwrite_files;
						CreateFileExplorerSelectOverwriteFiles(drawer.system, wizard_data);
					}
					else {
						FileExplorerResetSelectedFiles(data);
					}

					drawer.ReturnTempAllocator(drawer_temp_allocator_marker);
				}
			}
			// Clear the flag
			data->flags = ClearFlag(data->flags, FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES);
		}

	}

#ifdef MEASURE_DURATION
	float microseconds = my_timer.GetDuration(ECS_TIMER_DURATION_US);
	average_values = average_values * average_count + microseconds;
	average_count++;
	average_values /= average_count;
#endif
}

void InitializeFileExplorer(EditorState* editor_state)
{
	FileExplorerData* data = editor_state->file_explorer_data;
	memset(data, 0, sizeof(*data));

	data->allocator = MemoryManager(FILE_EXPLORER_ALLOCATOR_CAPACITY, ECS_KB * 8, FILE_EXPLORER_ALLOCATOR_CAPACITY, editor_state->EditorAllocator());
	AllocatorPolymorphic polymorphic_allocator = &data->allocator;
	// Copied files must not be initialied since only Cut/Copy will set the appropriate stream
	data->current_directory.Initialize(polymorphic_allocator, 0, FILE_EXPLORER_CURRENT_DIRECTORY_CAPACITY);
	data->selected_files.Initialize(polymorphic_allocator, FILE_EXPLORER_CURRENT_SELECTED_CAPACITY);
	data->filter_stream.Initialize(polymorphic_allocator, 0, FILE_EXPLORER_FILTER_CAPACITY);
	data->preloaded_textures.Initialize(polymorphic_allocator, FILE_EXPLORER_PRELOAD_TEXTURE_INITIAL_COUNT);
	data->staging_preloaded_textures.Initialize(polymorphic_allocator, 0, FILE_EXPLORER_STAGING_PRELOAD_TEXTURE_COUNT);

	data->mesh_thumbnails.Initialize(polymorphic_allocator, FILE_EXPLORER_MESH_THUMBNAILS_INITIAL_SIZE);

	data->right_click_stream.buffer = nullptr;
	data->right_click_stream.size = 0;

	data->copied_files.buffer = nullptr;
	data->copied_files.size = 0;

	data->starting_shift_index = -1;
	data->ending_shift_index = -1;

	// Hold texture display for some frames when loading in order to make a smoother transition - noticeably faster
	data->flags = 0;
	data->preload_flags = 0;

	for (size_t index = 0; index < std::size(data->displayed_items_allocator); index++) {
		data->displayed_items_allocator[index] = ResizableLinearAllocator(
			FILE_EXPLORER_DISPLAYED_ITEMS_ALLOCATOR_CAPACITY,
			FILE_EXPLORER_DISPLAYED_ITEMS_ALLOCATOR_CAPACITY,
			polymorphic_allocator
		);
	}
	data->displayed_items.Initialize(data->displayed_items_allocator + 0, 0);
	data->displayed_items_allocator_index = 0;
	data->temporary_allocator = ResizableLinearAllocator(
		FILE_EXPLORER_TEMPORARY_ALLOCATOR_CAPACITY,
		FILE_EXPLORER_TEMPORARY_ALLOCATOR_CAPACITY,
		polymorphic_allocator
	);
}

void FileExplorerPrivateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_additional_data;
	FileExplorerData* data = editor_state->file_explorer_data;
	if (window_index == system->GetActiveWindow()) {
		if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			if (keyboard->IsPressed(ECS_KEY_C)) {
				action_data->data = data;
				FileExplorerCopySelection(action_data);
			}
			else if (keyboard->IsPressed(ECS_KEY_V)) {
				action_data->data = editor_state;
				FileExplorerPasteElements(action_data);
			}
			else if (keyboard->IsPressed(ECS_KEY_X)) {
				action_data->data = data;
				FileExplorerCutSelection(action_data);
			}
		}

		if (keyboard->IsPressed(ECS_KEY_DELETE)) {
			action_data->data = data;
			FileExplorerDeleteSelection(action_data);
		}
	}
}

static bool FileExplorerRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	EditorState* editor_state = (EditorState*)window_data;
	FileExplorerData* explorer_data = editor_state->file_explorer_data;

	if (explorer_data->should_redraw) {
		explorer_data->should_redraw = false;
		return false;
	}
	return true;
}

void FileExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
{
	descriptor.window_name = FILE_EXPLORER_WINDOW_NAME;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;

	descriptor.draw = FileExplorerDraw;
	descriptor.private_action = FileExplorerPrivateAction;
	descriptor.retained_mode = FileExplorerRetainedMode;
}

void CreateFileExplorer(EditorState* editor_state) {
	CreateDockspaceFromWindow(FILE_EXPLORER_WINDOW_NAME, editor_state, CreateFileExplorerWindow);
}

unsigned int CreateFileExplorerWindow(EditorState* editor_state) {
	UIWindowDescriptor descriptor;
	
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB);
	FileExplorerSetDescriptor(descriptor, editor_state, &stack_memory);

	return editor_state->ui_system->Create_Window(descriptor);
}

void CreateFileExplorerAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateFileExplorer((EditorState*)action_data->data);
}

void TickFileExplorer(EditorState* editor_state)
{
	FileExplorerData* data = editor_state->file_explorer_data;

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_TEXTURES, FILE_EXPLORER_PRELOAD_TEXTURE_LAZY_EVALUATION)) {
		if (!HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED)) {
			FileExplorerRegisterPreloadTextures(editor_state);
		}
		if (!EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS) && HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED)
			&& !HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_LAUNCHED_THREADS)) {
			FileExplorerLaunchPreloadTextures(editor_state);
		}
	}

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_MESH_THUMBNAIL, FILE_EXPLORER_MESH_THUMBNAIL_LAZY_EVALUATION)) {
		if (!EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
			FileExplorerGenerateMeshThumbnails(editor_state);
		}
	}

	// Commit preloaded textures
	if (HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED) && HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_ENDED)) {
		FileExplorerCommitStagingPreloadTextures(editor_state);
	}

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_RETAINED_FILE_CHECK, FILE_EXPLORER_RETAINED_MODE_FILE_RECHECK_LAZY_EVALUATION)) {
		unsigned int next_allocator_index = data->displayed_items_allocator_index == 0 ? 1 : 0;
		AllocatorPolymorphic next_allocator = data->displayed_items_allocator + next_allocator_index;
		ResizableStream<Stream<wchar_t>> current_file_paths(next_allocator, 0);
		AdditionStream<Stream<wchar_t>> addition_stream = &current_file_paths;

		GetDirectoriesOrFilesOptions options;
		options.relative_root = data->current_directory;
		GetDirectoryOrFiles(data->current_directory, next_allocator, addition_stream, options);

		bool are_different = false;
		if (current_file_paths.size != data->displayed_items.size) {
			are_different = true;
		}
		else {
			for (unsigned int index = 0; index < data->displayed_items.size && !are_different; index++) {
				unsigned int existing_index = FindString(current_file_paths[index], data->displayed_items.ToStream());
				if (existing_index == -1) {
					are_different = true;
				}
			}
		}

		if (are_different) {
			data->should_redraw = true;
			// Deallocate the current allocator
			data->displayed_items_allocator[data->displayed_items_allocator_index].Clear();
			data->displayed_items_allocator_index = next_allocator_index;
			data->displayed_items = current_file_paths;
		}
		else {
			// Clear the allocator which was used for the current entries
			ClearAllocator(next_allocator);
		}
	}
}

ResourceView FileExplorerGetMeshThumbnail(const EditorState* editor_state, Stream<wchar_t> absolute_path) {
	ResourceView view = nullptr;
	editor_state->file_explorer_data->mesh_thumbnails.ForEachConst<true>([&](const FileExplorerMeshThumbnail& thumbnail, ResourceIdentifier identifier) {
		if (thumbnail.could_be_read && identifier.Compare(absolute_path)) {
			view = thumbnail.texture;
			return true;
		}
		return false;
	});

	return view;
}