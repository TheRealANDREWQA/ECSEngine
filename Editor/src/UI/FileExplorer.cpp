#include "editorpch.h"
#include "FileExplorer.h"
#include "FileExplorerData.h"
#include "..\Editor\EditorState.h"
#include "..\HelperWindows.h"
#include "..\Project\ProjectOperations.h"
#include "Inspector.h"
#include "..\Editor\EditorPalette.h"
#include "..\Editor\EditorEvent.h"
#include "ECSEngineUtilities.h"
#include "ECSEngineRendering.h"
#include "ECSEngineRenderingCompression.h"
#include "ECSEngineAssets.h"
#include "ECSEngineDebugDrawer.h"

#include "CreateScene.h"
#include "../Assets/AssetManagement.h"
#include "../Assets/AssetExtensions.h"
#include "AssetIcons.h"

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

constexpr size_t ADD_ROW_COUNT = 2;

constexpr size_t FILE_RIGHT_CLICK_ROW_COUNT = 7;
char* FILE_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path\nCut\nCopy";

constexpr size_t FOLDER_RIGHT_CLICK_ROW_COUNT = 7;
char* FOLDER_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path\nCut\nCopy";

constexpr size_t FILE_EXPLORER_DESELECTION_MENU_ROW_COUNT = 3;
char* FILE_EXPLORER_DESELECTION_MENU_CHARACTERS = "Create\nPaste\nReset Copied Files";
bool FILE_EXPLORER_DESELECTION_HAS_SUBMENUES[FILE_EXPLORER_DESELECTION_MENU_ROW_COUNT] = {
	true,
	false,
	false
};

constexpr size_t FILE_EXPLORER_DESELECTION_MENU_CREATE_ROW_COUNT = 5;
char* FILE_EXPLORER_DESELECTION_MENU_CREATE_CHARACTERS = "Scene\nSampler\nShader\nMaterial\nMisc";

constexpr const char* RENAME_LABEL_FILE_NAME = "Rename File";
constexpr const char* RENAME_LABEL_FILE_INPUT_NAME = "New file name";

constexpr const char* RENAME_LABEL_FOLDER_NAME = "Rename Folder";
constexpr const char* RENAME_LABEL_FOLDER_INPUT_NAME = "New folder name";

constexpr size_t FILE_EXPLORER_CURRENT_DIRECTORY_CAPACITY = 256;
constexpr size_t FILE_EXPLORER_CURRENT_SELECTED_CAPACITY = 16;
constexpr size_t FILE_EXPLORER_COPIED_FILE_CAPACITY = 16;
constexpr size_t FILE_EXPLORER_FILTER_CAPACITY = 256;
constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_INITIAL_COUNT = 16;
constexpr size_t FILE_EXPLORER_STAGING_PRELOAD_TEXTURE_COUNT = 512;
constexpr size_t FILE_EXPLORER_MESH_THUMBNAILS_INITIAL_SIZE = 128;

constexpr uint2 FILE_EXPLORER_MESH_THUMBNAIL_TEXTURE_SIZE = { 256, 256 };

constexpr unsigned int FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT = 1 << 0;
constexpr unsigned int FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES = 1 << 1;
constexpr unsigned int FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES = 1 << 2;
constexpr unsigned int FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES = 1 << 3;

constexpr unsigned int FILE_EXPLORER_FLAGS_PRELOAD_STARTED = 1 << 1;
constexpr unsigned int FILE_EXPLORER_FLAGS_PRELOAD_ENDED = 1 << 2;
constexpr unsigned int FILE_EXPLORER_FLAGS_PRELOAD_LAUNCHED_THREADS = 1 << 3;

constexpr int COLOR_CUT_ALPHA = 90;

constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_ALLOCATOR_SIZE_PER_THREAD = ECS_MB * 100;
constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_FALLBACK_SIZE = ECS_MB * 600;

constexpr size_t FILE_EXPLORER_PRELOAD_TEXTURE_LAZY_EVALUATION = 1'500;
constexpr size_t FILE_EXPLORER_MESH_THUMBNAIL_LAZY_EVALUATION = 500;

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
	DESELECTION_MENU_CREATE_SCENE,
	DESELECTION_MENU_CREATE_SAMPLER,
	DESELECTION_MENU_CREATE_SHADER,
	DESELECTION_MENU_CREATE_MATERIAL,
	DESELECTION_MENU_CREATE_MISC
};

void FileExplorerResetSelectedFiles(FileExplorerData* data) {
	// Deallocate every string stored
	for (size_t index = 0; index < data->selected_files.size; index++) {
		Deallocate(data->selected_files.allocator, data->selected_files[index].buffer);
	}

	// If it is a long list, deallocate the buffer
	if (data->selected_files.size > FILE_EXPLORER_RESET_SELECTED_FREE_LIMIT) {
		data->selected_files.FreeBuffer();
	}
	data->selected_files.size = 0;
}

void FileExplorerResetSelectedFiles(EditorState* editor_state) {
	FileExplorerData* data = editor_state->file_explorer_data;
	FileExplorerResetSelectedFiles(data);
}

void FileExplorerResetCopiedFiles(FileExplorerData* data) {
	// Cut/Copy will make a coallesced allocation
	if (data->copied_files.size > 0) {
		Deallocate(data->selected_files.allocator, data->copied_files.buffer);
		data->copied_files.size = 0;
	}
	data->flags = function::ClearFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);
}

void FileExplorerAllocateSelectedFile(FileExplorerData* data, Stream<wchar_t> path) {
	Stream<wchar_t> new_path = function::StringCopy(data->selected_files.allocator, path);
	data->selected_files.Add(new_path);
}

void FileExplorerSetShiftIndices(FileExplorerData* explorer_data, unsigned int index) {
	explorer_data->starting_shift_index = index;
	explorer_data->ending_shift_index = index;
}

void FileExplorerSetShiftIndices(EditorState* editor_state, unsigned int index) {
	FileExplorerSetShiftIndices(editor_state->file_explorer_data, index);
}

void ChangeFileExplorerDirectory(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	FileExplorerData* data = editor_state->file_explorer_data;
	data->current_directory.Copy(path);
	data->current_directory[path.size] = L'\0';

	FileExplorerResetSelectedFiles(data);
	FileExplorerSetShiftIndices(data, index);
	data->selected_files.size = 0;
}

void ChangeFileExplorerFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	FileExplorerData* data = editor_state->file_explorer_data;
	FileExplorerResetSelectedFiles(data);
	FileExplorerAllocateSelectedFile(data, path);

	FileExplorerSetShiftIndices(data, index);
	data->right_click_stream = data->selected_files[0];
}

struct SelectableData {
	bool IsTheSameData(const SelectableData* other) {
		return (other != nullptr) && function::CompareStrings(selection, other->selection);
	}

	EditorState* editor_state;
	unsigned int index;
	Path selection;
	Timer timer;
};

void FileExplorerSetNewDirectory(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	ChangeFileExplorerDirectory(editor_state, path, index);
}

void FileExplorerSetNewFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	ChangeFileExplorerFile(editor_state, path, index);
	ChangeInspectorToFile(editor_state, path);
}

// Returns -1 when it doesn't exist, else the index where it is located
unsigned int FileExplorerHasSelectedFile(EditorState* editor_state, Stream<wchar_t> path) {
	FileExplorerData* data = editor_state->file_explorer_data;
	return function::FindString(path, Stream<Stream<wchar_t>>(data->selected_files.buffer, data->selected_files.size));
}

void FileExplorerHandleControlPath(EditorState* editor_state, Stream<wchar_t> path) {
	// Check to see if the path already exists; if it is there, remove it
	// else add it
	FileExplorerData* data = editor_state->file_explorer_data;
	unsigned int index = FileExplorerHasSelectedFile(editor_state, path);
	if (index != -1) {
		Deallocate(data->selected_files.allocator, data->selected_files[index].buffer);
		data->selected_files.RemoveSwapBack(index);
	}
	else {
		FileExplorerAllocateSelectedFile(data, path);
	}
}

void FileExplorerHandleShiftSelection(EditorState* editor_state, unsigned int index) {
	FileExplorerData* explorer_data = editor_state->file_explorer_data;
	if (explorer_data->ending_shift_index < index || explorer_data->ending_shift_index == -1) {
		explorer_data->ending_shift_index = index;
	}
	if (explorer_data->starting_shift_index > index || explorer_data->starting_shift_index == -1) {
		explorer_data->starting_shift_index = index;
	}
	explorer_data->flags = function::SetFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES);
}

void FileExplorerSelectableBase(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* data = (SelectableData*)_data;
	FileExplorerData* explorer_data = data->editor_state->file_explorer_data;

	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse_tracker->LeftButton() == MBPRESSED) {
			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				FileExplorerHandleControlPath(data->editor_state, data->selection);
			}
			else if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
				FileExplorerHandleShiftSelection(data->editor_state, data->index);
			}
			else {
				// Check to see if the file is already selected - if it is, then do nothing in order for 
				// the drag to work correctly
				Stream<Stream<wchar_t>> selected_files(explorer_data->selected_files.buffer, explorer_data->selected_files.size);
				if (function::FindString(data->selection, selected_files) == -1) {
					FileExplorerSetNewFile(data->editor_state, data->selection, data->index);
				}
			}
		}
	}
}

void FileExplorerDirectorySelectable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* additional_data = (SelectableData*)_additional_data;
	SelectableData* data = (SelectableData*)_data;

	system->m_focused_window_data.clean_up_call_general = true;
	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse_tracker->LeftButton() == MBPRESSED) {
			if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				if (additional_data->timer.GetDurationSinceMarker(ECS_TIMER_DURATION_MS) < DOUBLE_CLICK_DURATION && keyboard->IsKeyUp(HID::Key::LeftControl)) {
					FileExplorerSetNewDirectory(data->editor_state, data->selection, data->index);
				}
			}
			data->timer.SetMarker();
			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				FileExplorerHandleControlPath(data->editor_state, data->selection);
			}
			else if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
				FileExplorerHandleShiftSelection(data->editor_state, data->index);
			}
			else {
				FileExplorerData* explorer_data = data->editor_state->file_explorer_data;
				// Check to see if the directory is already selected - if it is, then do nothing in order for 
				// the drag to work correctly
				Stream<Stream<wchar_t>> selected_files(explorer_data->selected_files.buffer, explorer_data->selected_files.size);
				if (function::FindString(data->selection, selected_files) == -1) {
					FileExplorerResetSelectedFiles(explorer_data);
					FileExplorerAllocateSelectedFile(explorer_data, data->selection);
					FileExplorerSetShiftIndices(explorer_data, data->index);
				}
			}
		}
	}
}

void FileExplorerChangeDirectoryFromFile(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	FileExplorerData* explorer_data = data->file_explorer_data;
	FileExplorerSetNewDirectory(data, explorer_data->selected_files[0], -1);
}

void FileExplorerLabelRenameCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* data = (SelectableData*)_data;
	SelectableData* additional_data = (SelectableData*)_additional_data;
	system->SetCleanupGeneralHandler();

	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse_tracker->LeftButton() == MBPRESSED) {
			if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				if (additional_data->timer.GetDurationSinceMarker(ECS_TIMER_DURATION_MS) < DOUBLE_CLICK_DURATION) {
					bool is_file = IsFile(data->selection);
					if (is_file) {
						CreateRenameFileWizard(data->selection, system);
					}
					else {
						CreateRenameFolderWizard(data->selection, system);
					}
				}
			}
			data->timer.SetMarker();
			FileExplorerSelectableBase(action_data);
		}
	}
}

void FileExplorerPasteElements(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	FileExplorerData* data = editor_state->file_explorer_data;

	unsigned int* _invalid_files = (unsigned int*)ECS_MALLOCA(sizeof(unsigned int) * data->copied_files.size);
	Stream<unsigned int> invalid_files(_invalid_files, 0);

	// For each path, copy it to the current directory or cut it
	if (function::HasFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT)) {
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
		error_message.Copy(BASE_ERROR_MESSAGE);

		for (size_t index = 0; index < invalid_files.size; index++) {
			error_message.Add('\n');
			error_message.Add('\t');
			function::ConvertWideCharsToASCII(data->copied_files[invalid_files[index]], error_message);
		}

		EditorSetConsoleError(error_message);
		ECS_FREEA(_error_message);
		// Also reset the copied files
		FileExplorerResetCopiedFiles(data);
	}
	data->flags = function::ClearFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);

	ECS_FREEA(invalid_files.buffer);
}

void FileExplorerCopySelection(ActionData* action_data) {
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
		data->copied_files[index].Copy(data->selected_files[index]);
	}

	data->flags = function::ClearFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);
}

void FileExplorerCutSelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;
	FileExplorerCopySelection(action_data);
	data->flags = function::SetFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT);
}

void FileExplorerDeleteSelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;

	unsigned int* _invalid_files = (unsigned int*)ECS_MALLOCA(data->selected_files.size * sizeof(unsigned int));
	unsigned int* _valid_copy_files = (unsigned int*)ECS_MALLOCA(data->copied_files.size * sizeof(unsigned int));
	Stream<unsigned int> invalid_files(_invalid_files, 0);
	Stream<unsigned int> valid_copy_files(_valid_copy_files, 0);

	function::MakeSequence(valid_copy_files);

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
			unsigned int copy_index = function::FindString(data->selected_files[index], data->copied_files);
			if (copy_index != -1) {
				for (size_t index = 0; index < valid_copy_files.size; index++) {
					if (valid_copy_files[index] == copy_index) {
						valid_copy_files.RemoveSwapBack(index);
						// exit the loop
						index = valid_copy_files.size;
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

		error_message.Copy(ERROR_MESSAGE);
		for (size_t index = 0; index < invalid_files.size; index++) {
			error_message.Add('\n');
			function::ConvertWideCharsToASCII(data->selected_files[invalid_files[index]], error_message);
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
			new_copy_files[index].Copy(data->copied_files[valid_copy_files[index]]);
		}
		FileExplorerResetCopiedFiles(data);
		data->copied_files = new_copy_files;
	}

	ECS_FREEA(_valid_copy_files);
	ECS_FREEA(_invalid_files);
	FileExplorerResetSelectedFiles(data);
}

using FileExplorerSelectFromIndex = void (*)(FileExplorerData* data, unsigned int index, Stream<wchar_t> path);

void FileExplorerSelectFromIndexNothing(FileExplorerData* data, unsigned int index, Stream<wchar_t> path) {}

void FileExplorerSelectFromIndexShift(FileExplorerData* data, unsigned int index, Stream<wchar_t> path) {
	if (data->starting_shift_index <= index && index <= data->ending_shift_index) {
		unsigned int index = data->selected_files.ReserveNewElement();
		data->selected_files.size++;
		void* new_allocation = Allocate(data->selected_files.allocator, data->selected_files[index].MemoryOf(path.size));
		data->selected_files[index].InitializeFromBuffer(new_allocation, path.size);
		data->selected_files[index].Copy(path);
	}
}

struct ForEachData {
	EditorState* editor_state;
	UIDrawer* drawer;
	UIDrawConfig* config;
	unsigned int element_count;
	CapacityStream<wchar_t>* mouse_element_path;
	FileExplorerSelectFromIndex select_function;
};

struct FileFunctorData {
	ForEachData* for_each_data;
	unsigned char color_alpha;
};

bool FileExplorerIsElementSelected(FileExplorerData* data, Path path) {
	return function::FindString(path, Stream<Stream<wchar_t>>(data->selected_files.buffer, data->selected_files.size)) != (unsigned int)-1;
}

bool FileExplorerIsElementCut(FileExplorerData* data, Path path) {
	return function::HasFlag(data->flags, FILE_EXPLORER_FLAGS_ARE_COPIED_FILES_CUT) && function::FindString(path, data->copied_files) != (unsigned int)-1;
}

void FileExplorerLabelDraw(UIDrawer* drawer, UIDrawConfig* config, SelectableData* _data, bool is_selected, bool is_folder) {
	FileExplorerData* data = _data->editor_state->file_explorer_data;
	Path current_path = _data->selection;

	size_t extension_size = function::PathExtensionSize(current_path) * (!is_folder);
	Path path_filename = function::PathFilename(current_path);
	//path_filename.size -= extension_size;

	char* allocation = (char*)data->temporary_allocator.Allocate((path_filename.size + 1) * sizeof(char), alignof(char));
	CapacityStream<char> ascii_stream(allocation, 0, 512);
	function::ConvertWideCharsToASCII(path_filename, ascii_stream);
	ascii_stream[ascii_stream.size] = '\0';
	
	// Draw the stem
	path_filename.size -= extension_size;
	ascii_stream[path_filename.size] = '\0';

	constexpr size_t LABEL_CONFIGURATION = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
		| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_PARAMETERS;

	float2 current_position = drawer->GetCurrentPosition();
	float label_horizontal_scale = drawer->GetSquareScaleScaled(THUMBNAIL_SIZE).x;

	UIConfigWindowDependentSize transform;	
	transform.type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
	transform.scale_factor = drawer->GetWindowSizeFactors(transform.type, { label_horizontal_scale, drawer->layout.default_element_y });
	config->AddFlag(transform);
	UIConfigTextParameters text_parameters;
	text_parameters.color = drawer->color_theme.text;
	text_parameters.character_spacing = drawer->font.character_spacing;
	text_parameters.size *= {0.75f, 0.8f};
	config->AddFlag(text_parameters);

	drawer->element_descriptor.label_padd.x *= 0.5f;

	if (is_selected) {
		UIConfigBorder border;
		border.thickness = drawer->GetDefaultBorderThickness();
		border.color = EDITOR_GREEN_COLOR;
		border.draw_phase = ECS_UI_DRAW_NORMAL;
		config->AddFlag(border);

		drawer->TextLabel(LABEL_CONFIGURATION | UI_CONFIG_BORDER, *config, ascii_stream.buffer);
		config->flag_count--;
	}
	else {
		drawer->TextLabel(LABEL_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT, *config, ascii_stream.buffer);
	}

	drawer->element_descriptor.label_padd.x *= 2.0f;

	config->flag_count -= 2;

	UIConfigGeneralAction general_action;
	general_action.handler = { FileExplorerLabelRenameCallback, _data, sizeof(*_data), ECS_UI_DRAW_SYSTEM };
	config->AddFlag(general_action);

	drawer->Rectangle(
		UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_RECTANGLE_GENERAL_ACTION | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION,
		*config, 
		drawer->GetCurrentPosition(), 
		{ label_horizontal_scale, drawer->layout.default_element_y }
	);

	config->flag_count--;

	ascii_stream[path_filename.size] = extension_size > 0 ? '.' : '\0';
	ascii_stream.size = path_filename.size + extension_size;

	float2 font_size = drawer->GetFontSize();
	//float2 text_span = drawer->TextSpan(ascii_stream, font_size, drawer->font.character_spacing);
	//text_span.x += 2.0f * drawer->system->m_descriptors.misc.tool_tip_padding.x;

	//float position_x = AlignMiddle(current_position.x, label_horizontal_scale, text_span.x);

	UITextTooltipHoverableData tooltip_data;
	tooltip_data.characters = ascii_stream.buffer;
	tooltip_data.base.offset_scale.y = true;
	tooltip_data.base.offset.y = TOOLTIP_OFFSET;
	//tooltip_data.base.offset.x = position_x - current_position.x;
	tooltip_data.base.center_horizontal_x = true;
	tooltip_data.base.font_size = font_size;
	
	drawer->AddTextTooltipHoverable(0, current_position, { label_horizontal_scale, drawer->layout.default_element_y }, &tooltip_data);
}

struct PathButtonData {
	FileExplorerData* data;
	size_t size;
};

void FileExplorerPathButtonAction(ActionData* action_data) {
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
white_color.alpha = functor_data->color_alpha;

void FileTextureDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE , *config, data->selection, white_color);
}

void FileBlankDraw(ActionData* action_data) {
	EXPAND_ACTION;

	Color sprite_color = drawer->color_theme.text;
	sprite_color.alpha = white_color.alpha;
	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE , *config, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, sprite_color);
}

void FileOverlayDraw(ActionData* action_data, const wchar_t* overlay_texture) {
	EXPAND_ACTION;

	Color base_color = drawer->color_theme.theme;
	Color overlay_color = drawer->color_theme.text;
	base_color.alpha = white_color.alpha;
	overlay_color.alpha = white_color.alpha;
	drawer->SpriteRectangleDouble(
		UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE,
		*config,
		ECS_TOOLS_UI_TEXTURE_FILE_BLANK,
		overlay_texture,
		overlay_color,
		base_color
	);
}

void FileCDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_C);
}

void FileCppDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_CPP);
}

void FileConfigDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG);
}

void FileTextDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_TEXT);
}

void FileShaderDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_SHADER);
}

void FileEditorDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_EDITOR);
}

void FileMeshDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_MESH);
}

void FileSceneDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ECS_TOOLS_UI_TEXTURE_FILE_SCENE);
}

void FileAssetShaderDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ASSET_SHADER_ICON);
}

void FileAssetGPUSamplerDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ASSET_GPU_SAMPLER_ICON);
}

void FileAssetMiscDraw(ActionData* action_data) {
	FileOverlayDraw(action_data, ASSET_MISC_ICON);
}

void FileMaterialDraw(ActionData* action_data) {
	EXPAND_ACTION;

	Color base_color = LightenColorClamp(drawer->color_theme.theme, 1.5f);
	Color overlay_color = drawer->color_theme.text;
	base_color.alpha = white_color.alpha;
	overlay_color.alpha = white_color.alpha;
	drawer->SpriteRectangleDouble(
		UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE,
		*config,
		ECS_TOOLS_UI_TEXTURE_FILE_BLANK,
		ASSET_MATERIAL_ICON,
		overlay_color,
		base_color
	);
}

void FileMeshThumbnailDraw(ActionData* action_data) {
	EXPAND_ACTION;

	FileExplorerData* explorer_data = data->editor_state->file_explorer_data;
	FileExplorerMeshThumbnail thumbnail = explorer_data->mesh_thumbnails.GetValue(data->selection);

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, *config, thumbnail.texture, white_color);
}

#undef EXPAND_ACTION

#pragma endregion

#pragma region Filter

void FilterNothing(ActionData* action_data) {}

void FilterActive(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	bool* is_valid = (bool*)_additional_data;
	SelectableData* data = (SelectableData*)_data;

	Path filename = function::PathFilename(data->selection);

	char temp_characters[512];
	CapacityStream<char> ascii_path(temp_characters, 0, 512);
	function::ConvertWideCharsToASCII(filename, ascii_path);
	ascii_path[ascii_path.size] = '\0';

	FileExplorerData* file_explorer_data = data->editor_state->file_explorer_data;
	*is_valid = strstr(ascii_path.buffer, file_explorer_data->filter_stream.buffer) != nullptr;
}

constexpr Action filter_functors[] = { FilterNothing, FilterActive };

#pragma endregion

#pragma region Add Functions - for the plus sign

void FileExplorerImportFile(ActionData* action_data) {

}

#pragma endregion

void FileExplorerDrag(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* explorer_data = (FileExplorerData*)_data;
	if (mouse_tracker->LeftButton() == MBHELD) {
		// Display the hover only if the mouse exited the action box
		if (!IsPointInRectangle(mouse_position, position, scale)) {
			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
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
				counts,
				theme_color,
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM
			);

			// The directory or last file type used
			Stream<wchar_t> last_file = explorer_data->selected_files[explorer_data->selected_files.size - 1];
			Stream<wchar_t> last_type_extension = function::PathExtension(last_file);
			const wchar_t* texture = ECS_TOOLS_UI_TEXTURE_FOLDER;
			ResourceView thumbnail_texture = nullptr;
			ECS_TEMP_STRING(texture_draw, 256);

			// If it has an extension, check to see existing files
			if (last_type_extension.size != 0) {
				Action action;
				ResourceIdentifier identifier(last_type_extension.buffer, last_type_extension.size * sizeof(wchar_t));
				bool success = explorer_data->file_functors.TryGetValue(identifier, action);
				if (success) {
					if (action == FileTextureDraw) {
						texture_draw.Copy(last_file);
						texture_draw.Add(L'\0');
						texture = texture_draw.buffer;
					}
					else if (action == FileCDraw) {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_C;
					}
					else if (action == FileCppDraw) {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_CPP;
					}
					else if (action == FileConfigDraw) {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_CONFIG;
					}
					else if (action == FileTextDraw) {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_TEXT;
					}
					else if (action == FileShaderDraw) {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_SHADER;
					}
					else if (action == FileEditorDraw) {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_EDITOR;
					}
					else if (action == FileMeshDraw) {
						// If it is a mesh draw, check to see if the thumbnail was generated
						FileExplorerMeshThumbnail thumbnail;
						if (explorer_data->mesh_thumbnails.TryGetValue({last_file.buffer, (unsigned int)last_file.size * sizeof(wchar_t)}, thumbnail)) {
							if (thumbnail.could_be_read) {
								// Set the thumbnail texture
								thumbnail_texture = thumbnail.texture;
							}
							else {
								texture = ECS_TOOLS_UI_TEXTURE_FILE_MESH;
							}
						}
						else {
							texture = ECS_TOOLS_UI_TEXTURE_FILE_MESH;
						}
					}
					else if (action == FileSceneDraw) {
						texture = ECS_TOOLS_UI_TEXTURE_FILE_SCENE;
					}
				}
				else {
					texture = ECS_TOOLS_UI_TEXTURE_FILE_BLANK;
				}
			}

			if (thumbnail_texture.view == nullptr) {
				// The last file texture
				system->SetSprite(
					dockspace,
					border_index,
					texture,
					hover_position,
					hover_scale,
					buffers,
					counts,
					transparent_color,
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					ECS_UI_DRAW_SYSTEM
				);
			}
			else {
				system->SetSprite(
					dockspace,
					border_index,
					thumbnail_texture,
					hover_position,
					hover_scale,
					buffers,
					counts,
					transparent_color,
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					ECS_UI_DRAW_SYSTEM
				);
			}

			explorer_data->flags = function::SetFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES);
		}
	}
	else if (mouse_tracker->LeftButton() == MBRELEASED) {
		if (function::HasFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES)) {
			explorer_data->flags = function::SetFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES);
			explorer_data->flags = function::ClearFlag(explorer_data->flags, FILE_EXPLORER_FLAGS_DRAG_SELECTED_FILES);
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
		// Make a single coallesced allocation
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
			function::ConvertWideCharsToASCII(
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
		ECS_TEMP_ASCII_STRING(error_message, 1024);
		error_message.Copy("One or more files could not be copied. These are:");
		unsigned int error_files_count = 0;
		for (size_t index = 0; index < data->data->overwrite_files.size; index++) {
			if (data->states[index]) {
				Stream<wchar_t> path_to_copy = data->data->explorer_data->selected_files[data->data->overwrite_files[index]];
				bool code = FileCut(path_to_copy, data->data->destination, true);
				if (!code) {
					error_message.Add('\n');
					error_message.Add('\t');
					function::ConvertWideCharsToASCII(path_to_copy, error_message);
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
	constexpr float2 WINDOW_SIZE = { 0.5f, 1.0f };

	UIWindowDescriptor descriptor;

	descriptor.draw = FileExplorerSelectOverwriteFilesDraw;

	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

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
	GlobalMemoryManager allocator(FILE_EXPLORER_PRELOAD_TEXTURE_ALLOCATOR_SIZE_PER_THREAD, 128, FILE_EXPLORER_PRELOAD_TEXTURE_FALLBACK_SIZE);

	// Load the texture normally - don't generate mip maps as that will require the immediate context
	ResourceManagerTextureDesc descriptor;
	AllocatorPolymorphic current_allocator = GetAllocatorPolymorphic(&allocator);
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
				GraphicsTexture2DDescriptor texture_descriptor;
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
		explorer_data->preload_flags = function::SetFlag(explorer_data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_ENDED);
	}
}

void FileExplorerCommitStagingPreloadTextures(EditorState* editor_state) {
	UISystem* ui_system = editor_state->ui_system;
	FileExplorerData* data = editor_state->file_explorer_data;
	ResourceManager* resource_manager = ui_system->m_resource_manager;

	for (size_t index = 0; index < data->staging_preloaded_textures.size; index++) {
		if (data->staging_preloaded_textures[index].texture.view != nullptr) {
			data->preloaded_textures.Add(data->staging_preloaded_textures[index]);

			// Now change the resource manager bindings
			ResourceIdentifier identifier(data->staging_preloaded_textures[index].path);

			// If it doesn't exist
			if (!resource_manager->Exists(identifier, ResourceType::Texture)) {
				resource_manager->AddResource(
					identifier,
					ResourceType::Texture,
					data->staging_preloaded_textures[index].texture.view, 
					data->staging_preloaded_textures[index].last_write_time
				);
			}
			else {
				resource_manager->RebindResource(identifier, ResourceType::Texture, data->staging_preloaded_textures[index].texture.view);
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
				if (function::CompareStrings(stream_path, data->explorer_data->preloaded_textures[index].path)) {
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
					preload_texture.path = function::StringCopy(data->editor_state->EditorAllocator(), stream_path);
					preload_texture.texture = nullptr;
					data->explorer_data->staging_preloaded_textures.Add(preload_texture);
				}
			}
		}
		return true;
	};

	ECS_TEMP_STRING(assests_folder, 256);
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
		data->preload_flags = function::SetFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED);
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
		data->preload_flags = function::SetFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED);

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

	data->preload_flags = function::SetFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_LAUNCHED_THREADS);
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
			Stream<wchar_t> allocated_path = function::StringCopy(GetAllocatorPolymorphic(data->editor_allocator), path);
			FileExplorerMeshThumbnail thumbnail;

			//Try to read the mesh here and create it's buffers GPU buffers
			CoallescedMesh* mesh = data->resource_manager->LoadCoallescedMeshImplementation(path);
			// If the mesh is nullptr, the read failed
			thumbnail.could_be_read = mesh != nullptr;
			thumbnail.last_write_time = OS::GetFileLastWrite(path);
			thumbnail.texture = nullptr;
			if (thumbnail.could_be_read) {
				// Call the GLTFThumbnail generator
				GLTFThumbnail gltf_thumbnail = GLTFGenerateThumbnail(data->resource_manager->m_graphics, FILE_EXPLORER_MESH_THUMBNAIL_TEXTURE_SIZE, &mesh->mesh);
				thumbnail.texture = gltf_thumbnail.texture;

				// Free the coallesced mesh
				data->resource_manager->UnloadCoallescedMeshImplementation(mesh);
			}

			// Update the hash table
			InsertIntoDynamicTable(data->explorer_data->mesh_thumbnails, data->editor_allocator, thumbnail, allocated_path);

			data->thumbnail_count++;

			// Exit the search - only a thumbnail is generated per frame
			return data->thumbnail_count < MAX_MESH_THUMBNAILS_PER_FRAME;
		}
		return true;
	};

	ECS_TEMP_STRING(assests_folder, 256);
	GetProjectAssetsFolder(editor_state, assests_folder);
	Stream<wchar_t> extensions[std::size(ASSET_MESH_EXTENSIONS)];
	for (size_t index = 0; index < std::size(ASSET_MESH_EXTENSIONS); index++) {
		extensions[index] = ASSET_MESH_EXTENSIONS[index];
	}
	
	ForEachFileInDirectoryRecursiveWithExtension(assests_folder, { extensions, std::size(extensions) }, &functor_data, functor);

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
		absolute_path.Copy(editor_state->file_explorer_data->current_directory);
		absolute_path.Add(ECS_OS_PATH_SEPARATOR);
		function::ConvertASCIIToWide(absolute_path, *additional_data);

		unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
		WindowTable* window_table = system->GetWindowTable(window_index);

		Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, absolute_path);

		bool success = CreateFunction(editor_state, relative_path, window_table);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to create asset file {#}.", absolute_path);
			EditorSetConsoleError(error_message);
		}
	}
};

// window_data is EditorState*
void FileExplorerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	Timer stack_timer;

	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	FileExplorerData* data = editor_state->file_explorer_data;

	if (initialize) {
		if (data->file_functors.m_capacity == 0) {
			//InitializeFileExplorer(data, editor_allocator, task_manager->GetThreadCount());
			//InitializeFileExplorer(editor_state);

			ProjectFile* project_file = editor_state->project_file;
			data->current_directory.Copy(project_file->path);
			data->current_directory.AddAssert(ECS_OS_PATH_SEPARATOR);
			data->current_directory.AddStreamAssert(Path(PROJECT_ASSETS_RELATIVE_PATH, wcslen(PROJECT_ASSETS_RELATIVE_PATH)));
			data->current_directory[data->current_directory.size] = L'\0';

			void* allocation = drawer.GetMainAllocatorBuffer(ALLOCATOR_CAPACITY);
			data->temporary_allocator = LinearAllocator(allocation, ALLOCATOR_CAPACITY);

			allocation = drawer.GetMainAllocatorBuffer(data->file_functors.MemoryOf(FILE_FUNCTORS_CAPACITY));
			data->file_functors.InitializeFromBuffer(allocation, FILE_FUNCTORS_CAPACITY);

#pragma region Add Handlers

			allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * ADD_ROW_COUNT);
			data->add_handlers.InitializeFromBuffer(allocation, ADD_ROW_COUNT, ADD_ROW_COUNT);
			data->add_handlers[0] = { SkipAction, nullptr, 0 };
			data->add_handlers[1] = { SkipAction, nullptr, 0 };

#pragma endregion

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
					*shader_type = ECS_SHADER_TYPE_COUNT;
				}
				else {
					shader_type = (ECS_SHADER_TYPE*)drawer->GetResource(SHADER_INPUT_RESOURCE_NAME);
				}

				UIConfigWindowDependentSize window_dependent_size;
				UIDrawConfig config;
				config.AddFlag(window_dependent_size);

				ECS_STACK_CAPACITY_STREAM(Stream<char>, labels, ECS_ASSET_TYPE_COUNT + 1);
				Stream<Stream<char>> shader_type_labels = editor_state->ReflectionManager()->GetEnum(STRING(ECS_SHADER_TYPE))->fields;
				labels.Copy(shader_type_labels);
				labels.Add("Unspecified");
				drawer->ComboBox(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Shader Type", labels, labels.size, (unsigned char*)shader_type);
			};

			data->deselection_create_menu_handler_data[DESELECTION_MENU_CREATE_SHADER].AddExtraElement(shader_file_input, editor_state, 0);

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

				ECS_TEMP_ASCII_STRING(ascii_path, 256);
				Stream<wchar_t>* path = (Stream<wchar_t>*)_data;
				function::ConvertWideCharsToASCII(*path, ascii_path);
				ascii_path[ascii_path.size] = '\0';
				system->m_application->WriteTextToClipboard(ascii_path.buffer);
			};

			allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FILE_RIGHT_CLICK_ROW_COUNT);
			data->file_right_click_handlers.InitializeFromBuffer(allocation, FILE_RIGHT_CLICK_ROW_COUNT, FILE_RIGHT_CLICK_ROW_COUNT);
			data->file_right_click_handlers[FILE_RIGHT_CLICK_OPEN] = { OpenFileWithDefaultApplicationStreamAction, &data->right_click_stream, 0, ECS_UI_DRAW_SYSTEM };
			data->file_right_click_handlers[FILE_RIGHT_CLICK_SHOW_IN_EXPLORER] = { LaunchFileExplorerStreamAction, &data->right_click_stream, 0, ECS_UI_DRAW_SYSTEM };
			data->file_right_click_handlers[FILE_RIGHT_CLICK_DELETE] = { FileExplorerDeleteSelection, data, 0, ECS_UI_DRAW_SYSTEM };
			data->file_right_click_handlers[FILE_RIGHT_CLICK_RENAME] = { RenameFileWizardStreamAction, &data->right_click_stream, 0, ECS_UI_DRAW_SYSTEM };
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
			data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_RENAME] = { RenameFolderWizardStreamAction, &data->right_click_stream, 0, ECS_UI_DRAW_SYSTEM };
			data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_COPY_PATH] = { CopyPath, &data->right_click_stream, 0 };
			data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_COPY_SELECTION] = { FileExplorerCopySelection, data, 0 };
			data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_CUT_SELECTION] = { FileExplorerCutSelection, data, 0 };

#pragma endregion

			ResourceIdentifier identifier;
			unsigned int hash;

#define ADD_FUNCTOR(action, string) identifier = ResourceIdentifier(string); \
ECS_ASSERT(!data->file_functors.Insert(action, identifier));

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

#undef ADD_FUNCTOR
		}

		// Reduce the next roy y offset
		drawer.layout.next_row_y_offset *= 0.5f;

		EditorStateLazyEvaluationTrigger(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_TEXTURES);
	}

#pragma region Deselection Menu

	auto DeselectClick = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		if (mouse_tracker->LeftButton() == MBPRESSED) {
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
	drawer.SetWindowHoverable(&deselect_right_click_handler);

#pragma endregion

#pragma region Header

	float row_padding = drawer.layout.next_row_padding;
	float next_row_offset = drawer.layout.next_row_y_offset;
	drawer.SetDrawMode(ECS_UI_DRAWER_NOTHING);
	drawer.SetRowPadding(0.001f);
	drawer.SetNextRowYOffset(0.0025f);

	UIDrawConfig header_config;

	UIConfigAbsoluteTransform absolute_transform;
	absolute_transform.position = drawer.GetCurrentPositionStatic();
	absolute_transform.scale = drawer.GetSquareScale(drawer.layout.default_element_y);
	header_config.AddFlag(absolute_transform);

	UIConfigBorder border;
	border.color = drawer.color_theme.borders;
	header_config.AddFlag(border);

	UIConfigMenuSprite plus_sprite;
	plus_sprite.texture = ECS_TOOLS_UI_TEXTURE_PLUS;
	plus_sprite.color = drawer.color_theme.text;
	header_config.AddFlag(plus_sprite);

	constexpr size_t HEADER_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LATE_DRAW | UI_CONFIG_DO_NOT_ADVANCE;

	UIDrawerMenuState menu_state;
	menu_state.click_handlers = data->add_handlers.buffer;
	menu_state.left_characters = "Folder\nMaterial";
	menu_state.row_count = ADD_ROW_COUNT;
	menu_state.row_has_submenu = nullptr;
	menu_state.submenues = nullptr;
	menu_state.submenu_index = 0;
	menu_state.unavailables = nullptr;
	drawer.SolidColorRectangle(HEADER_CONFIGURATION, header_config, drawer.color_theme.theme);
	drawer.Menu(HEADER_CONFIGURATION | UI_CONFIG_MENU_SPRITE | UI_CONFIG_BORDER, header_config, "Menu", &menu_state);

	header_config.flag_count = 0;
	header_config.AddFlag(border);
	absolute_transform.position.x += absolute_transform.scale.x + border.thickness;
	absolute_transform.scale.x = drawer.GetLabelScale("Import").x;
	header_config.AddFlag(absolute_transform);
	drawer.Button(
		HEADER_CONFIGURATION | UI_CONFIG_BORDER | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y,
		header_config, 
		"Import",
		{ FileExplorerImportFile, nullptr, 0 }
	);
	
	UIConfigTextInputHint hint;
	hint.characters = "Search";

	header_config.flag_count--;
	absolute_transform.position.x += absolute_transform.scale.x + border.thickness;
	absolute_transform.scale.x = drawer.layout.default_element_x * 2.5f;
	absolute_transform.scale.y = drawer.layout.default_element_y;
	header_config.AddFlags(hint, absolute_transform);
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
		char temp_characters[512];
		CapacityStream<char> ascii_path(temp_characters, 0, 512);
		Path wide_path = { data->current_directory.buffer + project_file->path.size + 1, data->current_directory.size - project_file->path.size - 1 };
		function::ConvertWideCharsToASCII(wide_path, ascii_path);
		ascii_path[ascii_path.size] = '\0';

		size_t total_path_size = project_file->path.size + 1;
		char* delimiter = strchr(ascii_path.buffer, ECS_OS_PATH_SEPARATOR_ASCII);
		delimiter = delimiter == nullptr ? ascii_path.buffer : delimiter;
		ASCIIPath current_path(ascii_path.buffer, delimiter - ascii_path.buffer);

		const auto mouse_state = drawer.system->m_mouse_tracker->LeftButton();
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
			if (drawer.IsMouseInRectangle(rectangle_position, absolute_transform.scale) && (mouse_state == MBPRESSED || mouse_state == MBHELD)) {
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
		if (drawer.IsMouseInRectangle(rectangle_position, rectangle_scale) && (mouse_state == MBPRESSED || mouse_state == MBHELD)) {
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

		UIConfigClickableAction drag_click;
		drag_click = { FileExplorerDrag, data, 0, ECS_UI_DRAW_SYSTEM };
		config.AddFlag(drag_click);

		ForEachData for_each_data;
		for_each_data.config = &config;
		for_each_data.editor_state = editor_state;
		for_each_data.drawer = &drawer;
		for_each_data.element_count = 0;
		for_each_data.select_function = FileExplorerSelectFromIndexNothing;
		if (function::HasFlag(data->flags, FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES)) {
			FileExplorerResetSelectedFiles(data);
			for_each_data.select_function = FileExplorerSelectFromIndexShift;
			data->flags = function::ClearFlag(data->flags, FILE_EXPLORER_FLAGS_GET_SELECTED_FILES_FROM_INDICES);
		}

		auto directory_functor = [](Stream<wchar_t> path, void* __data) {
			ForEachData* _data = (ForEachData*)__data;

			FileExplorerData* data = _data->editor_state->file_explorer_data;
			UIDrawConfig* config = _data->config;
			UIDrawer* drawer = _data->drawer;

			Path stream_path = function::StringCopy(GetAllocatorPolymorphic(&data->temporary_allocator), path);

			SelectableData selectable_data;
			selectable_data.editor_state = _data->editor_state;
			selectable_data.selection = stream_path;
			selectable_data.index = _data->element_count;

			bool is_valid = true;
			ActionData action_data = drawer->GetDummyActionData();
			action_data.additional_data = &is_valid;
			action_data.data = &selectable_data;
			filter_functors[data->filter_stream.size > 0](&action_data);

			_data->select_function(data, _data->element_count, stream_path);

			if (is_valid) {
				bool is_selected = FileExplorerIsElementSelected(data, stream_path);
				unsigned char color_alpha = FileExplorerIsElementCut(data, stream_path) ? COLOR_CUT_ALPHA : 255;

				Color white_color = ECS_COLOR_WHITE;
				white_color.alpha = color_alpha;
				drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, *config, ECS_TOOLS_UI_TEXTURE_FOLDER, white_color);

				UIConfigGeneralAction general_action;
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

				UIConfigHoverableAction hoverable_action;
				hoverable_action.handler = { RightClickMenu, &right_click_data, sizeof(right_click_data), ECS_UI_DRAW_SYSTEM };
				config->AddFlag(hoverable_action);

				constexpr size_t RECTANGLE_CONFIGURATION = UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_RECTANGLE_HOVERABLE_ACTION
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

					config->flag_count -= 2;
				}
				else {
					drawer->Rectangle(RECTANGLE_CONFIGURATION, *config);
				}

				// Update the mouse index if it hovers this directory - only directories must make this check
				if (drawer->IsMouseInRectangle(rectangle_position, rectangle_scale)) {
					_data->mouse_element_path->Copy(stream_path);
				}

				config->flag_count -= 3;

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

			Path stream_path = function::StringCopy(GetAllocatorPolymorphic(&data->temporary_allocator), path);

			Path extension = function::PathExtension(stream_path);

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

			filter_functors[data->filter_stream.size > 0](&action_data);

			if (is_valid && extension.size > 0) {
				bool is_selected = FileExplorerIsElementSelected(data, stream_path);
				unsigned char color_alpha = FileExplorerIsElementCut(data, stream_path) ? COLOR_CUT_ALPHA : 255;

				FileFunctorData functor_data;
				functor_data.for_each_data = _data;
				functor_data.color_alpha = color_alpha;
				action_data.additional_data = &functor_data;

				// Functor was initialized outside the if with FileBlankDraw
				bool has_functor = data->file_functors.TryGetValue(identifier, functor);
				if (has_functor) {
					if (functor == FileTextureDraw) {
						if (function::HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED)) {
							// Check to see if it is a texture that is being preloaded
							for (size_t index = 0; index < data->staging_preloaded_textures.size; index++) {
								if (function::CompareStrings(data->staging_preloaded_textures[index].path, stream_path)) {
									// If it is a texture that is being preloaded, overwrite to blank file while it is being loaded
									functor = FileBlankDraw;
									// exit the loop by setting the index to the count
									index = data->staging_preloaded_textures.size;
								}
							}
						}
					}
					else if (functor == FileMeshDraw) {
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

				UIConfigHoverableAction hoverable_action;

				UIDrawerMenuRightClickData right_click_data;
				right_click_data.name = "File File Explorer Menu";
				right_click_data.window_index = drawer->window_index;
				right_click_data.state.click_handlers = data->file_right_click_handlers.buffer;
				right_click_data.state.left_characters = FILE_RIGHT_CLICK_CHARACTERS;
				right_click_data.state.row_count = FILE_RIGHT_CLICK_ROW_COUNT;
				right_click_data.state.submenu_index = 0;

				struct OnRightClickData {
					EditorState* editor_state;
					unsigned int index;
					Stream<wchar_t> path;
				};

				OnRightClickData* action_data = (OnRightClickData*)right_click_data.action_data;
				action_data->editor_state = _data->editor_state;
				action_data->path = stream_path;
				action_data->index = _data->element_count;

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
				
				right_click_data.action = OnRightClickAction;
				right_click_data.is_action_data_ptr = false;
				hoverable_action.handler = { RightClickMenu, &right_click_data, sizeof(right_click_data), ECS_UI_DRAW_SYSTEM };
				config->AddFlag(hoverable_action);

				float2 rectangle_position;
				float2 rectangle_scale;
				UIConfigGetTransform get_transform;
				get_transform.position = &rectangle_position;
				get_transform.scale = &rectangle_scale;
				config->AddFlag(get_transform);

				constexpr size_t RECTANGLE_CONFIGURATION = UI_CONFIG_GET_TRANSFORM | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_RECTANGLE_HOVERABLE_ACTION |
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

				ECS_TEMP_STRING(null_terminated_path, 256);
				null_terminated_path.Copy(stream_path);
				null_terminated_path[stream_path.size] = L'\0';

				drawer->AddDoubleClickAction(
					0,
					rectangle_position,
					rectangle_scale,
					_data->element_count,
					DOUBLE_CLICK_DURATION,
					{ FileExplorerSelectableBase, &selectable_data, sizeof(selectable_data) },
					{ OpenFileWithDefaultApplicationAction, null_terminated_path.buffer, (unsigned int)(sizeof(wchar_t) * (stream_path.size + 1)) }
				);

				config->flag_count -= 2;

				FileExplorerLabelDraw(drawer, config, &selectable_data, is_selected, false);
				_data->element_count++;
			}


			return true;
		};

		data->temporary_allocator.Clear();

		ECS_TEMP_STRING(mouse_element_path, 256);
		for_each_data.mouse_element_path = &mouse_element_path;

		if (data->current_directory.size > 0) {
			ForEachInDirectory(data->current_directory, &for_each_data, directory_functor, file_functor);
		}

		// Check moved files
		if (function::HasFlag(data->flags, FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES)) {
			// If there is no element being hovered - skip it
			if (mouse_element_path.size > 0) {
				// If it is moved - check that it landed in a folder that is not selected
				Stream<Stream<wchar_t>> selected_files(data->selected_files.buffer, data->selected_files.size);
				if (function::FindString(mouse_element_path, selected_files) == -1) {
					// Move all the files - if one cannot be moved because it already exists, then ask permision
					// else skip it
					unsigned int error_file_count = 0;
					
					// Allocate a temporary buffer that will hold the error paths - if any - set it to a big size
					// and release it from the drawer after using it
					constexpr size_t ERROR_BUFFER_SIZE = 8192;
					size_t drawer_temp_allocator_marker = drawer.GetTempAllocatorMarker();
					CapacityStream<char> error_files(drawer.GetTempBuffer(ERROR_BUFFER_SIZE), 0, ERROR_BUFFER_SIZE);
					error_files.Copy("One or more files/folders could not be copied. These are: ");

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
								function::ConvertWideCharsToASCII(current_file, error_files);
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
			data->flags = function::ClearFlag(data->flags, FILE_EXPLORER_FLAGS_MOVE_SELECTED_FILES);
		}

	}

}

void InitializeFileExplorer(EditorState* editor_state)
{
	FileExplorerData* data = editor_state->file_explorer_data;

	AllocatorPolymorphic polymorphic_allocator = editor_state->EditorAllocator();
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
}

void FileExplorerPrivateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_additional_data;
	FileExplorerData* data = editor_state->file_explorer_data;
	if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
		if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
			action_data->data = data;
			FileExplorerCopySelection(action_data);
		}
		else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
			action_data->data = editor_state;
			FileExplorerPasteElements(action_data);
		}
		else if (keyboard_tracker->IsKeyPressed(HID::Key::X)) {
			action_data->data = data;
			FileExplorerCutSelection(action_data);
		}
	}

	if (keyboard->IsKeyPressed(HID::Key::Delete)) {
		action_data->data = data;
		FileExplorerDeleteSelection(action_data);
	}
}

void FileExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	descriptor.window_name = FILE_EXPLORER_WINDOW_NAME;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;

	descriptor.draw = FileExplorerDraw;

	descriptor.private_action = FileExplorerPrivateAction;
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

	size_t stack_memory[128];
	FileExplorerSetDescriptor(descriptor, editor_state, stack_memory);

	return editor_state->ui_system->Create_Window(descriptor);
}

void CreateFileExplorerAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateFileExplorer((EditorState*)action_data->data);
}

void FileExplorerTick(EditorState* editor_state)
{
	FileExplorerData* data = editor_state->file_explorer_data;

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_TEXTURES, FILE_EXPLORER_PRELOAD_TEXTURE_LAZY_EVALUATION)) {
		if (!function::HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED)) {
			FileExplorerRegisterPreloadTextures(editor_state);
		}
		if (!EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS) && function::HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED)
			&& !function::HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_LAUNCHED_THREADS)) {
			FileExplorerLaunchPreloadTextures(editor_state);
		}
	}

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_FILE_EXPLORER_MESH_THUMBNAIL, FILE_EXPLORER_MESH_THUMBNAIL_LAZY_EVALUATION)) {
		if (!EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
			FileExplorerGenerateMeshThumbnails(editor_state);
		}
	}

	// Commit preloaded textures
	if (function::HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_STARTED) && function::HasFlag(data->preload_flags, FILE_EXPLORER_FLAGS_PRELOAD_ENDED)) {
		FileExplorerCommitStagingPreloadTextures(editor_state);
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