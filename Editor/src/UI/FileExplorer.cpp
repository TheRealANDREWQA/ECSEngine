#include "FileExplorer.h"
#include "..\Editor\EditorState.h"
#include "AssetTypes.h"
#include "..\HelperWindows.h"
#include "..\Project\ProjectOperations.h"
#include "Inspector.h"
#include "..\Editor\EditorPalette.h"
#include "..\Editor\EditorEvent.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;
ECS_CONTAINERS;

constexpr float2 WINDOW_SIZE = float2(1.0f, 0.6f);
constexpr size_t DOUBLE_CLICK_DURATION = 350;
constexpr size_t ALLOCATOR_CAPACITY = 10'000;
constexpr float THUMBNAIL_SIZE = 0.12f;
constexpr float THUMBNAIL_TO_LABEL_SPACING = 0.03f;
constexpr float TOOLTIP_OFFSET = 0.02f;
constexpr size_t FILE_FUNCTORS_CAPACITY = 32;

constexpr size_t FILE_EXPLORER_RESET_SELECTED_FREE_LIMIT = 64;
constexpr size_t FILE_EXPLORER_RESET_COPIED_FREE_LIMIT = 64;

constexpr size_t ADD_ROW_COUNT = 2;

constexpr size_t FILE_RIGHT_CLICK_ROW_COUNT = 7;
char* FILE_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path\nCut\nCopy";

constexpr size_t FOLDER_RIGHT_CLICK_ROW_COUNT = 7;
char* FOLDER_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path\nCut\nCopy";

constexpr size_t FILE_EXPLORER_DESELECTION_RIGHT_CLICK_ROW_COUNT = 2;
char* FILE_EXPLORER_DESELECTION_RIGHT_CLICK_CHARACTERS = "Paste\nReset Copied Files";

constexpr const char* RENAME_LABEL_FILE_NAME = "Rename File";
constexpr const char* RENAME_LABEL_FILE_INPUT_NAME = "New file name";

constexpr const char* RENAME_LABEL_FOLDER_NAME = "Rename Folder";
constexpr const char* RENAME_LABEL_FOLDER_INPUT_NAME = "New folder name";

constexpr size_t FILE_EXPLORER_CURRENT_DIRECTORY_CAPACITY = 256;
constexpr size_t FILE_EXPLORER_CURRENT_SELECTED_CAPACITY = 16;
constexpr size_t FILE_EXPLORER_COPIED_FILE_CAPACITY = 16;
constexpr size_t FILE_EXPLORER_FILTER_CAPACITY = 256;

constexpr int COLOR_CUT_ALPHA = 90;

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

enum DESELECTION_RIGHT_CLICK_INDEX {
	DESELECTION_RIGHT_CLICK_PASTE,
	DESELECTION_RIGHT_CLICK_RESET_COPIED_FILES
};

using Hash = HashFunctionAdditiveString;

void FileExplorerResetSelectedFiles(FileExplorerData* data) {
	// Deallocate every string stored
	for (size_t index = 0; index < data->selected_files.size; index++) {
		data->selected_files.allocator->Deallocate(data->selected_files[index].buffer);
	}

	// If it is a long list, deallocate the buffer
	if (data->selected_files.size > FILE_EXPLORER_RESET_SELECTED_FREE_LIMIT) {
		data->selected_files.FreeBuffer();
	}
	data->selected_files.size = 0;
}

void FileExplorerResetSelectedFiles(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
	FileExplorerResetSelectedFiles(data);
}

void FileExplorerResetCopiedFiles(FileExplorerData* data) {
	// Cut/Copy will make a coallesced allocation
	if (data->copied_files.size > 0) {
		data->selected_files.allocator->Deallocate(data->copied_files.buffer);
		data->copied_files.size = 0;
	}
	data->are_copied_files_cut = false;
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
	FileExplorerSetShiftIndices((FileExplorerData*)editor_state->file_explorer_data, index);
}

void ChangeFileExplorerDirectory(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	EDITOR_STATE(editor_state);

	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
	data->current_directory.Copy(path);
	data->current_directory[path.size] = L'\0';

	FileExplorerResetSelectedFiles(data);
	FileExplorerSetShiftIndices(data, index);
	data->selected_files.size = 0;
}

void ChangeFileExplorerFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	EDITOR_STATE(editor_state);

	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
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
	ChangeInspectorToFolder(editor_state, path);
}

void FileExplorerSetNewFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int index) {
	ChangeFileExplorerFile(editor_state, path, index);
	ChangeInspectorToFile(editor_state, path);
}

// Returns -1 when it doesn't exist, else the index where it is located
unsigned int FileExplorerHasSelectedFile(EditorState* editor_state, Stream<wchar_t> path) {
	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
	return function::IsStringInStream(path, Stream<Stream<wchar_t>>(data->selected_files.buffer, data->selected_files.size));
}

void FileExplorerHandleControlPath(EditorState* editor_state, Stream<wchar_t> path) {
	// Check to see if the path already exists; if it is there, remove it
	// else add it
	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
	unsigned int index = FileExplorerHasSelectedFile(editor_state, path);
	if (index != -1) {
		data->selected_files.allocator->Deallocate(data->selected_files[index].buffer);
		data->selected_files.RemoveSwapBack(index);
	}
	else {
		FileExplorerAllocateSelectedFile(data, path);
	}
}

void FileExplorerHandleShiftSelection(EditorState* editor_state, unsigned int index) {
	FileExplorerData* explorer_data = (FileExplorerData*)editor_state->file_explorer_data;
	if (explorer_data->ending_shift_index < index || explorer_data->ending_shift_index == -1) {
		explorer_data->ending_shift_index = index;
	}
	if (explorer_data->starting_shift_index > index || explorer_data->starting_shift_index == -1) {
		explorer_data->starting_shift_index = index;
	}
	explorer_data->get_selected_files_from_indices = true;
}

void FileExplorerSelectableBase(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* data = (SelectableData*)_data;
	FileExplorerData* explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;

	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse_tracker->LeftButton() == MBPRESSED) {
			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				FileExplorerHandleControlPath(data->editor_state, data->selection);
			}
			else if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
				FileExplorerHandleShiftSelection(data->editor_state, data->index);
			}
			else {
				FileExplorerSetNewFile(data->editor_state, data->selection, data->index);
			}
		}
	}
	/*else {
		FileExplorerResetSelectedFiles(data->editor_state);
	}*/
}

void FileExplorerDirectorySelectable(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* additional_data = (SelectableData*)_additional_data;
	SelectableData* data = (SelectableData*)_data;

	system->m_focused_window_data.clean_up_call_general = true;
	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse_tracker->LeftButton() == MBPRESSED) {
			if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				if (additional_data->timer.GetDurationSinceMarker_ms() < DOUBLE_CLICK_DURATION && keyboard->IsKeyUp(HID::Key::LeftControl)) {
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
				FileExplorerData* explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;
				FileExplorerResetSelectedFiles(explorer_data);
				FileExplorerAllocateSelectedFile(explorer_data, data->selection);
				ChangeInspectorToFolder(data->editor_state, data->selection);
				FileExplorerSetShiftIndices(explorer_data, data->index);
			}
		}
	}
	/*else {
		FileExplorerResetSelectedFile(data->editor_state);
	}*/
}

void FileExplorerChangeDirectoryFromFile(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	FileExplorerData* explorer_data = (FileExplorerData*)data->file_explorer_data;
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
				if (additional_data->timer.GetDurationSinceMarker_ms() < DOUBLE_CLICK_DURATION) {
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
	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;

	unsigned int* _invalid_files = (unsigned int*)_malloca(sizeof(unsigned int) * data->copied_files.size);
	Stream<unsigned int> invalid_files(_invalid_files, 0);

	// For each path, copy it to the current directory or cut it
	if (data->are_copied_files_cut) {
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
				current_success = FileCopy(data->copied_files[index], data->current_directory, true);
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

		char* _error_message = (char*)_malloca(sizeof(char) * total_buffer_size);
		CapacityStream<char> error_message(_error_message, 0, total_buffer_size);
		error_message.Copy(ToStream(BASE_ERROR_MESSAGE));

		for (size_t index = 0; index < invalid_files.size; index++) {
			error_message.Add('\n');
			function::ConvertWideCharsToASCII(data->copied_files[invalid_files[index]], error_message);
			error_message.size += data->copied_files[invalid_files[index]].size;
		}

		EditorSetError(editor_state, error_message);
		_freea(_error_message);
		// Also reset the copied files
		FileExplorerResetCopiedFiles(data);
	}

	_freea(invalid_files.buffer);
}

void FileExplorerCopySelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;
	size_t total_size = 0;

	for (size_t index = 0; index < data->selected_files.size; index++) {
		total_size += data->selected_files[index].size * sizeof(wchar_t);
	}

	FileExplorerResetCopiedFiles(data);

	void* allocation = data->selected_files.allocator->Allocate(total_size + sizeof(Stream<wchar_t>) * data->selected_files.size);
	uintptr_t buffer = (uintptr_t)allocation;

	data->copied_files.InitializeFromBuffer(buffer, data->selected_files.size);
	for (size_t index = 0; index < data->copied_files.size; index++) {
		data->copied_files[index].InitializeFromBuffer(buffer, data->selected_files[index].size);
		data->copied_files[index].Copy(data->selected_files[index]);
	}

	data->are_copied_files_cut = false;
}

void FileExplorerCutSelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;
	FileExplorerCopySelection(action_data);
	data->are_copied_files_cut = true;
}

void FileExplorerDeleteSelection(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	FileExplorerData* data = (FileExplorerData*)_data;

	unsigned int* _invalid_files = (unsigned int*)_malloca(data->selected_files.size * sizeof(unsigned int));
	unsigned int* _valid_copy_files = (unsigned int*)_malloca(data->copied_files.size * sizeof(unsigned int));
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
		}
		if (!success) {
			invalid_files.Add(index);
		}
		// Check to see if this file is also in the copy stream to remove it
		else {
			unsigned int copy_index = function::IsStringInStream(data->selected_files[index], data->copied_files);
			if (copy_index != -1) {
				for (size_t index = 0; index < valid_copy_files.size; index++) {
					if (valid_copy_files[index] == copy_index) {
						valid_copy_files.RemoveSwapBack(index);
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
		char* temp_allocation = (char*)_malloca(sizeof(char) * total_size + 8);
		CapacityStream<char> error_message(temp_allocation, 0, total_size + 8);

		error_message.Copy(ToStream(ERROR_MESSAGE));
		for (size_t index = 0; index < invalid_files.size; index++) {
			error_message.Add('\n');
			function::ConvertWideCharsToASCII(data->selected_files[invalid_files[index]], error_message);
			error_message.size += data->selected_files[invalid_files[index]].size;
		}

		CreateErrorMessageWindow(system, error_message);
		_freea(temp_allocation);
	}

	if (valid_copy_files.size < data->copied_files.size) {
		size_t total_size = 0;
		for (size_t index = 0; index < valid_copy_files.size; index++) {
			total_size += data->copied_files[valid_copy_files[index]].size;
		}

		void* allocation = data->selected_files.allocator->Allocate(sizeof(wchar_t) * total_size + sizeof(Stream<wchar_t>) * valid_copy_files.size, alignof(wchar_t));
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

	_freea(_valid_copy_files);
	_freea(_invalid_files);
	FileExplorerResetSelectedFiles(data);
}

using FileExplorerSelectFromIndex = void (*)(FileExplorerData* data, unsigned int index, Stream<wchar_t> path);

void FileExplorerSelectFromIndexNothing(FileExplorerData* data, unsigned int index, Stream<wchar_t> path) {}

void FileExplorerSelectFromIndexShift(FileExplorerData* data, unsigned int index, Stream<wchar_t> path) {
	if (data->starting_shift_index <= index && index <= data->ending_shift_index) {
		unsigned int index = data->selected_files.ReserveNewElement();
		data->selected_files.size++;
		data->selected_files[index].Initialize(data->selected_files.allocator, path.size);
		data->selected_files[index].Copy(path);
	}
}

struct ForEachData {
	EditorState* editor_state;
	UIDrawer<false>* drawer;
	UIDrawConfig* config;
	unsigned int element_count;
	FileExplorerSelectFromIndex select_function;
};

struct FileFunctorData {
	ForEachData* for_each_data;
	unsigned char color_alpha;
};

bool FileExplorerIsElementSelected(FileExplorerData* data, Path path) {
	return function::IsStringInStream(path, Stream<Stream<wchar_t>>(data->selected_files.buffer, data->selected_files.size)) != (unsigned int)-1;
}

bool FileExplorerIsElementCut(FileExplorerData* data, Path path) {
	return data->are_copied_files_cut && function::IsStringInStream(path, data->copied_files) != (unsigned int)-1;
}

void FileExplorerLabelDraw(UIDrawer<false>* drawer, UIDrawConfig* config, SelectableData* _data, bool is_selected, bool is_folder) {
	FileExplorerData* data = (FileExplorerData*)_data->editor_state->file_explorer_data;
	Path current_path = _data->selection;

	size_t extension_size = function::PathExtensionSize(current_path) * (!is_folder);
	Path path_filename = function::PathFilename(current_path);
	//path_filename.size -= extension_size;

	char* allocation = (char*)data->allocator.Allocate((path_filename.size + 1) * sizeof(char), alignof(char));
	CapacityStream<char> ascii_stream(allocation, 0, 512);
	function::ConvertWideCharsToASCII(path_filename, ascii_stream);
	ascii_stream[path_filename.size] = '\0';
	
	// Draw the stem
	path_filename.size -= extension_size;
	ascii_stream[path_filename.size] = '\0';

	constexpr size_t LABEL_CONFIGURATION = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
		| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_PARAMETERS;

	float2 current_position = drawer->GetCurrentPosition();
	float label_horizontal_scale = drawer->GetSquareScale(THUMBNAIL_SIZE).x;

	UIConfigWindowDependentSize transform;	
	transform.type = WindowSizeTransformType::Horizontal;
	transform.scale_factor = drawer->GetWindowSizeFactors(transform.type, { label_horizontal_scale, drawer->layout.default_element_y });
	config->AddFlag(transform);
	UIConfigTextParameters text_parameters;
	text_parameters.color = drawer->color_theme.default_text;
	text_parameters.character_spacing = drawer->font.character_spacing;
	text_parameters.size *= {0.85f, 0.85f};
	config->AddFlag(text_parameters);

	drawer->element_descriptor.label_horizontal_padd *= 0.5f;

	if (is_selected) {
		UIConfigBorder border;
		border.thickness = drawer->GetDefaultBorderThickness();
		border.color = EDITOR_GREEN_COLOR;
		config->AddFlag(border);

		drawer->TextLabel<LABEL_CONFIGURATION | UI_CONFIG_BORDER | UI_CONFIG_BORDER_DRAW_NORMAL>(*config, ascii_stream.buffer);
		config->flag_count--;
	}
	else {
		drawer->TextLabel<LABEL_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT>(*config, ascii_stream.buffer);
	}

	drawer->element_descriptor.label_horizontal_padd *= 2.0f;

	config->flag_count -= 2;

	UIConfigGeneralAction general_action;
	general_action.handler = { FileExplorerLabelRenameCallback, _data, sizeof(*_data), UIDrawPhase::System };
	config->AddFlag(general_action);

	drawer->Rectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_GENERAL_ACTION 
		| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_CLICKABLE_ACTION>(*config, drawer->GetCurrentPosition(), { label_horizontal_scale, drawer->layout.default_element_y });

	config->flag_count--;

	ascii_stream[path_filename.size] = function::Select<char>(extension_size > 0, '.', '\0');
	ascii_stream.size = path_filename.size + extension_size;

	float2 font_size = drawer->GetFontSize();
	float2 text_span = drawer->system->GetTextSpan(ascii_stream.buffer, ascii_stream.size, font_size.x, font_size.y, drawer->font.character_spacing);
	text_span.x += 2.0f * drawer->system->m_descriptors.misc.tool_tip_padding.x;

	float position_x = AlignMiddle(current_position.x, label_horizontal_scale, text_span.x);

	UITextTooltipHoverableData tooltip_data;
	tooltip_data.characters = ascii_stream.buffer;
	tooltip_data.base.offset_scale.y = true;
	tooltip_data.base.offset.y = TOOLTIP_OFFSET;
	tooltip_data.base.offset.x = position_x - current_position.x;
	
	drawer->AddTextTooltipHoverable(current_position, { label_horizontal_scale, drawer->layout.default_element_y }, &tooltip_data);
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
UIDrawer<false>* drawer = functor_data->for_each_data->drawer; \
UIDrawConfig* config = functor_data->for_each_data->config; \
Color white_color = ECS_COLOR_WHITE; \
white_color.alpha = functor_data->color_alpha;

void TextureDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, data->selection.buffer, white_color);
}

void FileBlankDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, white_color);
}

void FileCDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_C, white_color);
}

void FileCppDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_CPP, white_color);
}

void FileConfigDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG, white_color);
}

void FileTextDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_TEXT, white_color);
}

void FileShaderDraw(ActionData* action_data) {
	EXPAND_ACTION;
	
	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_SHADER, white_color);
}

void FileEditorDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_EDITOR, white_color);
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
	ascii_path[filename.size] = '\0';

	FileExplorerData* file_explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;
	*is_valid = strstr(ascii_path.buffer, file_explorer_data->filter_stream.buffer) != nullptr;
}

constexpr Action filter_functors[] = { FilterNothing, FilterActive };

#pragma endregion

void FileExplorerImportFile(ActionData* action_data) {

}

// window_data is EditorState*
template<bool initialize>
void FileExplorerDraw(void* window_data, void* drawer_descriptor) {
	UI_PREPARE_DRAWER(initialize);

	EDITOR_STATE(window_data);
	EditorState* editor_state = (EditorState*)window_data;
	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;

	if constexpr (initialize) {
		ProjectFile* project_file = (ProjectFile*)editor_state->project_file;
		data->current_directory.Copy(project_file->path);
		data->current_directory.AddSafe(ECS_OS_PATH_SEPARATOR);
		data->current_directory.AddStreamSafe(Path(PROJECT_ASSETS_RELATIVE_PATH, wcslen(PROJECT_ASSETS_RELATIVE_PATH)));
		data->current_directory[data->current_directory.size] = L'\0';

		void* allocation = drawer.GetMainAllocatorBuffer(ALLOCATOR_CAPACITY);
		data->allocator = LinearAllocator(allocation, ALLOCATOR_CAPACITY);
		
		allocation = drawer.GetMainAllocatorBuffer(data->file_functors.MemoryOf(FILE_FUNCTORS_CAPACITY));
		data->file_functors.InitializeFromBuffer(allocation, FILE_FUNCTORS_CAPACITY);

#pragma region Add Handlers

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * ADD_ROW_COUNT);
		data->add_handlers.InitializeFromBuffer(allocation, ADD_ROW_COUNT, ADD_ROW_COUNT);
		data->add_handlers[0] = { SkipAction, nullptr, 0 };
		data->add_handlers[1] = { SkipAction, nullptr, 0 };

#pragma endregion

#pragma region Deselection Handlers

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FILE_EXPLORER_DESELECTION_RIGHT_CLICK_ROW_COUNT);
		data->deselection_right_click_handlers.InitializeFromBuffer(allocation, 0, FILE_EXPLORER_DESELECTION_RIGHT_CLICK_ROW_COUNT);

		data->deselection_right_click_handlers[DESELECTION_RIGHT_CLICK_PASTE] = { FileExplorerPasteElements, editor_state, 0 };

		auto reset_copied_files = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;
			
			FileExplorerResetCopiedFiles((FileExplorerData*)_data);
		};
		data->deselection_right_click_handlers[DESELECTION_RIGHT_CLICK_RESET_COPIED_FILES] = { reset_copied_files, data, 0 };

#pragma endregion

#pragma region File Right Click Handlers

		auto CopyPath = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ECS_TEMP_ASCII_STRING(ascii_path, 256);
			Stream<wchar_t>* path = (Stream<wchar_t>*)_data;
			function::ConvertWideCharsToASCII(*path, ascii_path);
			system->m_application->WriteTextToClipboard(ascii_path.buffer);
		};

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FILE_RIGHT_CLICK_ROW_COUNT);
		data->file_right_click_handlers.InitializeFromBuffer(allocation, FILE_RIGHT_CLICK_ROW_COUNT, FILE_RIGHT_CLICK_ROW_COUNT);
		data->file_right_click_handlers[FILE_RIGHT_CLICK_OPEN] = { OpenFileWithDefaultApplicationStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_SHOW_IN_EXPLORER] = { LaunchFileExplorerStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_DELETE] = { FileExplorerDeleteSelection, data, 0, UIDrawPhase::System };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_RENAME] = { RenameFileWizardStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_COPY_PATH] = { CopyPath, &data->right_click_stream, 0 };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_COPY_SELECTION] = { FileExplorerCopySelection, data, 0 };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_CUT_SELECTION] = { FileExplorerCutSelection, data, 0 };

#pragma endregion

#pragma region Folder Right Click Handlers

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FOLDER_RIGHT_CLICK_ROW_COUNT);
		data->folder_right_click_handlers.InitializeFromBuffer(allocation, FOLDER_RIGHT_CLICK_ROW_COUNT, FOLDER_RIGHT_CLICK_ROW_COUNT);
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_OPEN] = { FileExplorerChangeDirectoryFromFile, editor_state, 0 };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_SHOW_IN_EXPLORER] = { LaunchFileExplorerStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_DELETE] = { FileExplorerDeleteSelection, data, 0, UIDrawPhase::System };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_RENAME] = { RenameFolderWizardStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_COPY_PATH] = { CopyPath, &data->right_click_stream, 0 };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_COPY_SELECTION] = { FileExplorerCopySelection, data, 0 };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_CUT_SELECTION] = { FileExplorerCutSelection, data, 0 };

#pragma endregion

		ResourceIdentifier identifier;
		unsigned int hash;

#define ADD_FUNCTOR(action, string) identifier = ResourceIdentifier(ToStream(string)); \
hash = Hash::Hash(identifier); \
ECS_ASSERT(!data->file_functors.Insert(hash, action, identifier));

		ADD_FUNCTOR(TextureDraw, L".jpg");
		ADD_FUNCTOR(TextureDraw, L".png");
		ADD_FUNCTOR(FileCDraw, L".c");
		ADD_FUNCTOR(FileCppDraw, L".cpp");
		ADD_FUNCTOR(FileCppDraw, L".h");
		ADD_FUNCTOR(FileCppDraw, L".hpp");
		ADD_FUNCTOR(FileConfigDraw, L".config");
		ADD_FUNCTOR(FileTextDraw, L".txt");
		ADD_FUNCTOR(FileTextDraw, L".doc");
		ADD_FUNCTOR(FileShaderDraw, L".hlsl");
		ADD_FUNCTOR(FileShaderDraw, L".hlsli");

#undef ADD_FUNCTOR

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

	UIDrawerMenuRightClickData deselection_right_click_data;
	deselection_right_click_data.name = "Deselection Menu";
	deselection_right_click_data.window_index = 0;
	deselection_right_click_data.state.click_handlers = data->deselection_right_click_handlers.buffer;
	deselection_right_click_data.state.left_characters = FILE_EXPLORER_DESELECTION_RIGHT_CLICK_CHARACTERS;
	deselection_right_click_data.state.row_count = FILE_EXPLORER_DESELECTION_RIGHT_CLICK_ROW_COUNT;

	UIActionHandler deselect_right_click_handler = { RightClickMenu, &deselection_right_click_data, sizeof(deselection_right_click_data), UIDrawPhase::System };
	drawer.SetWindowHoverable(&deselect_right_click_handler);

#pragma endregion

#pragma region Header

	float row_padding = drawer.layout.next_row_padding;
	float next_row_offset = drawer.layout.next_row_y_offset;
	drawer.SetDrawMode(UIDrawerMode::Nothing);
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
	plus_sprite.color = drawer.color_theme.default_text;
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
	drawer.SolidColorRectangle<HEADER_CONFIGURATION>(header_config, drawer.color_theme.theme);
	drawer.Menu<HEADER_CONFIGURATION | UI_CONFIG_MENU_SPRITE | UI_CONFIG_BORDER>(header_config, "Menu", &menu_state);

	header_config.flag_count = 0;
	header_config.AddFlag(border);
	absolute_transform.position.x += absolute_transform.scale.x + border.thickness;
	absolute_transform.scale.x = drawer.GetLabelScale("Import").x;
	header_config.AddFlag(absolute_transform);
	drawer.Button<HEADER_CONFIGURATION | UI_CONFIG_BORDER | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(
		header_config, 
		"Import",
		{ FileExplorerImportFile, nullptr, 0 }
	);
	
	UIConfigTextInputHint hint;
	hint.characters = "Search";

	header_config.flag_count--;
	absolute_transform.position.x += absolute_transform.scale.x + border.thickness;
	absolute_transform.scale.x = drawer.layout.default_element_x * 2.5f;
	header_config.AddFlags(hint, absolute_transform);
	drawer.TextInput<HEADER_CONFIGURATION | UI_CONFIG_BORDER | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_NO_NAME>(
		header_config, 
		"Input", 
		&data->filter_stream
	);
	absolute_transform.position.x += drawer.layout.element_indentation;

	if (data->current_directory.size > 0) {
		drawer.Indent();

		ProjectFile* project_file = (ProjectFile*)editor_state->project_file;
		char temp_characters[512];
		CapacityStream<char> ascii_path(temp_characters, 0, 512);
		Path wide_path = { data->current_directory.buffer + project_file->path.size + 1, data->current_directory.size - project_file->path.size - 1 };
		function::ConvertWideCharsToASCII(wide_path, ascii_path);
		ascii_path.size = wide_path.size;
		ascii_path[ascii_path.size] = '\0';

		size_t total_path_size = project_file->path.size + 1;
		char* delimiter = strchr(ascii_path.buffer, ECS_OS_PATH_SEPARATOR_ASCII);
		delimiter = (char*)function::Select<uintptr_t>(delimiter == nullptr, (uintptr_t)ascii_path.buffer, (uintptr_t)delimiter);
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

			drawer.Button<HEADER_CONFIGURATION | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(
				header_config,
				current_path.buffer, 
				{ FileExplorerPathButtonAction, &button_data, sizeof(button_data) }
			);

			Color rectangle_color = drawer.color_theme.background;
			float2 rectangle_position = absolute_transform.position - drawer.region_render_offset;
			if (drawer.IsMouseInRectangle(rectangle_position, absolute_transform.scale) && (mouse_state == MBPRESSED || mouse_state == MBHELD)) {
				rectangle_color = drawer.color_theme.theme;
			}
			drawer.SolidColorRectangle<UI_CONFIG_LATE_DRAW>(rectangle_position, absolute_transform.scale, rectangle_color);

			header_config.flag_count--;
			absolute_transform.position.x += absolute_transform.scale.x;
			absolute_transform.scale.x = drawer.GetLabelScale("\\").x;
			header_config.AddFlag(absolute_transform);
			drawer.TextLabel<HEADER_CONFIGURATION | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(
				header_config,
				"\\"
			);
			drawer.SolidColorRectangle<UI_CONFIG_LATE_DRAW>(absolute_transform.position - drawer.region_render_offset, absolute_transform.scale, drawer.color_theme.background);

			total_path_size += current_path.size + 1;

			current_path.buffer = current_path.buffer + current_path.size + 1;
			delimiter = strchr(current_path.buffer, ECS_OS_PATH_SEPARATOR_ASCII);
			delimiter = (char*)function::Select<uintptr_t>(delimiter == nullptr, (uintptr_t)current_path.buffer, (uintptr_t)delimiter);
			current_path.size = (uintptr_t)delimiter - (uintptr_t)current_path.buffer;
		}

		absolute_transform.position.x += absolute_transform.scale.x;

		PathButtonData button_data;
		button_data.data = data;
		button_data.size = total_path_size + ((uintptr_t)ascii_path.buffer + ascii_path.size) - (uintptr_t)current_path.buffer;

		header_config.flag_count = 0;
		header_config.AddFlag(absolute_transform);

		float label_scale = drawer.GetLabelScale(current_path.buffer).x;
		drawer.Button<HEADER_CONFIGURATION | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(
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
		drawer.SolidColorRectangle<UI_CONFIG_LATE_DRAW>(rectangle_position, rectangle_scale, rectangle_color);
	}

	drawer.SetNextRowYOffset(next_row_offset);
	drawer.SetRowPadding(row_padding);
	drawer.UpdateCurrentRowScale(drawer.layout.default_element_y);
	drawer.NextRow();

#pragma endregion

	// Element Draw
	if constexpr (!initialize) {

		drawer.SetDrawMode(UIDrawerMode::ColumnDrawFitSpace, 2, THUMBNAIL_TO_LABEL_SPACING);
		UIDrawConfig config;

		UIConfigRelativeTransform transform;
		transform.scale = drawer.GetSquareScale(THUMBNAIL_SIZE) / drawer.GetElementDefaultScale();
		config.AddFlag(transform);

		UIConfigClickableAction null_click;
		null_click = { SkipAction, nullptr, 0 };
		config.AddFlag(null_click);

		ForEachData for_each_data;
		for_each_data.config = &config;
		for_each_data.editor_state = editor_state;
		for_each_data.drawer = &drawer;
		for_each_data.element_count = 0;
		for_each_data.select_function = FileExplorerSelectFromIndexNothing;
		if (data->get_selected_files_from_indices) {
			FileExplorerResetSelectedFiles(data);
			for_each_data.select_function = FileExplorerSelectFromIndexShift;
			data->get_selected_files_from_indices = false;
		}

		auto directory_functor = [](const std::filesystem::path& _path, void* __data) {
			ForEachData* _data = (ForEachData*)__data;

			FileExplorerData* data = (FileExplorerData*)_data->editor_state->file_explorer_data;
			UIDrawConfig* config = _data->config;
			UIDrawer<false>* drawer = _data->drawer;

			const wchar_t* c_path = _path.c_str();
			Path path = function::StringCopy(&data->allocator, c_path);

			SelectableData selectable_data;
			selectable_data.editor_state = _data->editor_state;
			selectable_data.selection = path;
			selectable_data.index = _data->element_count;

			bool is_valid = true;
			ActionData action_data = drawer->GetDummyActionData();
			action_data.additional_data = &is_valid;
			action_data.data = &selectable_data;
			filter_functors[data->filter_stream.size > 0](&action_data);

			_data->select_function(data, _data->element_count, path);

			if (is_valid) {
				bool is_selected = FileExplorerIsElementSelected(data, path);
				unsigned char color_alpha = function::Select(FileExplorerIsElementCut(data, path), COLOR_CUT_ALPHA, 255);

				Color white_color = ECS_COLOR_WHITE;
				white_color.alpha = color_alpha;
				drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FOLDER, white_color);

				UIConfigGeneralAction general_action;
				general_action.handler = { FileExplorerDirectorySelectable, &selectable_data, sizeof(selectable_data) };
				config->AddFlag(general_action);

				auto OnRightClickCallback = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					SelectableData* data = (SelectableData*)_data;
					unsigned int index = FileExplorerHasSelectedFile(data->editor_state, data->selection);
					if (index == -1) {
						ChangeFileExplorerFile(data->editor_state, data->selection, data->index);
						ChangeInspectorToFolder(data->editor_state, data->selection);
					}
					else {
						FileExplorerData* explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;
						explorer_data->right_click_stream = explorer_data->selected_files[index];
					}
				};

				UIDrawerMenuRightClickData right_click_data;
				right_click_data.action = OnRightClickCallback;
				memcpy(right_click_data.action_data, &selectable_data, sizeof(selectable_data));
				right_click_data.name = "Folder File Explorer Menu";
				right_click_data.window_index = 0;
				right_click_data.state.click_handlers = data->folder_right_click_handlers.buffer;
				right_click_data.state.left_characters = FOLDER_RIGHT_CLICK_CHARACTERS;
				right_click_data.state.row_count = FOLDER_RIGHT_CLICK_ROW_COUNT;
				right_click_data.state.submenu_index = 0;

				UIConfigHoverableAction hoverable_action;
				hoverable_action.handler = { RightClickMenu, &right_click_data, sizeof(right_click_data), UIDrawPhase::System };
				config->AddFlag(hoverable_action);

				constexpr size_t RECTANGLE_CONFIGURATION = UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_HOVERABLE_ACTION
					| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_GENERAL_ACTION | UI_CONFIG_CLICKABLE_ACTION;

				if (is_selected) {
					UIConfigColor color;
					color.color = drawer->color_theme.theme;
					color.color.alpha = color_alpha;
					config->AddFlag(color);

					UIConfigBorder border;
					border.thickness = drawer->GetDefaultBorderThickness();
					border.color = EDITOR_GREEN_COLOR;
					config->AddFlag(border);

					drawer->Rectangle<RECTANGLE_CONFIGURATION | UI_CONFIG_COLOR | UI_CONFIG_BORDER | UI_CONFIG_BORDER_DRAW_NORMAL>(*config);

					config->flag_count -= 2;
				}
				else {
					drawer->Rectangle<RECTANGLE_CONFIGURATION>(*config);
				}

				config->flag_count -= 2;

				FileExplorerLabelDraw(drawer, config, &selectable_data, is_selected, true);
				_data->element_count++;
			}

			return true;
		};

		auto file_functor = [](const std::filesystem::path& _path, void* __data) {
			ForEachData* _data = (ForEachData*)__data;

			FileExplorerData* data = (FileExplorerData*)_data->editor_state->file_explorer_data;
			UIDrawConfig* config = _data->config;
			UIDrawer<false>* drawer = _data->drawer;

			const wchar_t* c_path = _path.c_str();
			Path path = function::StringCopy(&data->allocator, c_path);

			Path extension = function::PathExtension(path);

			ResourceIdentifier identifier(extension);
			unsigned int hash = Hash::Hash(identifier);
			Action functor;

			SelectableData selectable_data;
			selectable_data.editor_state = _data->editor_state;
			selectable_data.selection = path;
			selectable_data.index = _data->element_count;

			bool is_valid = true;
			ActionData action_data = drawer->GetDummyActionData();
			action_data.system = drawer->system;
			action_data.data = &selectable_data;
			action_data.additional_data = &is_valid;

			_data->select_function(data, _data->element_count, path);

			filter_functors[data->filter_stream.size > 0](&action_data);

			if (is_valid && extension.size > 0) {
				bool is_selected = FileExplorerIsElementSelected(data, path);
				unsigned char color_alpha = function::Select(FileExplorerIsElementCut(data, path), COLOR_CUT_ALPHA, 255);

				FileFunctorData functor_data;
				functor_data.for_each_data = _data;
				functor_data.color_alpha = color_alpha;

				if (data->file_functors.TryGetValue(hash, identifier, functor)) {
					action_data.additional_data = &functor_data;

					functor(&action_data);
				}
				else {
					drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_BLANK);
				}

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
				action_data->path = path;
				action_data->index = _data->element_count;

				auto OnRightClickAction = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					OnRightClickData* data = (OnRightClickData*)_data;
					unsigned int index = FileExplorerHasSelectedFile(data->editor_state, data->path);
					if (index == -1) {
						FileExplorerSetNewFile(data->editor_state, data->path, data->index);
					}
					else {
						FileExplorerData* explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;
						explorer_data->right_click_stream = explorer_data->selected_files[index];
						explorer_data->starting_shift_index = explorer_data->ending_shift_index = data->index;
					}
				};
				
				right_click_data.action = OnRightClickAction;
				hoverable_action.handler = { RightClickMenu, &right_click_data, sizeof(right_click_data), UIDrawPhase::System };
				config->AddFlag(hoverable_action);

				float2 rectangle_position;
				float2 rectangle_scale;
				UIConfigGetTransform get_transform;
				get_transform.position = &rectangle_position;
				get_transform.scale = &rectangle_scale;
				config->AddFlag(get_transform);

				constexpr size_t RECTANGLE_CONFIGURATION = UI_CONFIG_GET_TRANSFORM | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_HOVERABLE_ACTION |
					UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_CLICKABLE_ACTION;

				if (is_selected) {
					UIConfigBorder border;
					border.color = EDITOR_GREEN_COLOR;
					border.thickness = drawer->GetDefaultBorderThickness();
					config->AddFlag(border);

					drawer->Rectangle<RECTANGLE_CONFIGURATION | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_BORDER | UI_CONFIG_BORDER_DRAW_NORMAL>(*config);

					Color theme_color = drawer->color_theme.theme;
					theme_color.alpha = 100;
					drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_FIT_SPACE>(*config, ECS_TOOLS_UI_TEXTURE_MASK, theme_color);

					config->flag_count--;
				}
				else {
					drawer->Rectangle<RECTANGLE_CONFIGURATION>(*config);
				}

				ECS_TEMP_STRING(null_terminated_path, 256);
				null_terminated_path.Copy(path);
				null_terminated_path[path.size] = L'\0';

				drawer->AddDoubleClickAction(
					rectangle_position,
					rectangle_scale,
					_data->element_count,
					DOUBLE_CLICK_DURATION,
					{ FileExplorerSelectableBase, &selectable_data, sizeof(selectable_data) },
					{ OpenFileWithDefaultApplicationAction, null_terminated_path.buffer, (unsigned int)(sizeof(wchar_t) * (path.size + 1)) }
				);

				config->flag_count -= 2;

				FileExplorerLabelDraw(drawer, config, &selectable_data, is_selected, false);
				_data->element_count++;
			}


			return true;
		};

		data->allocator.Clear();

		if (data->current_directory.size > 0) {
			ForEachInDirectory(data->current_directory, &for_each_data, directory_functor, file_functor);
		}

	}
	
}

void InitializeFileExplorer(FileExplorerData* file_explorer_data, MemoryManager* allocator)
{
	// Copied files must not be initialied since only Cut/Copy will set the appropriate stream
	file_explorer_data->current_directory.Initialize(allocator, 0, FILE_EXPLORER_CURRENT_DIRECTORY_CAPACITY);
	file_explorer_data->selected_files.Initialize(allocator, FILE_EXPLORER_CURRENT_SELECTED_CAPACITY);
	file_explorer_data->filter_stream.Initialize(allocator, 0, FILE_EXPLORER_FILTER_CAPACITY);

	file_explorer_data->right_click_stream.buffer = nullptr;
	file_explorer_data->right_click_stream.size = 0;

	file_explorer_data->copied_files.buffer = nullptr;
	file_explorer_data->copied_files.size = 0;

	file_explorer_data->starting_shift_index = -1;
	file_explorer_data->ending_shift_index = -1;

	file_explorer_data->are_copied_files_cut = false;
	file_explorer_data->get_selected_files_from_indices = false;
}

void FileExplorerPrivateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_additional_data;
	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
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

	descriptor.draw = FileExplorerDraw<false>;
	descriptor.initialize = FileExplorerDraw<true>;

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

	EDITOR_STATE(editor_state);

	return ui_system->Create_Window(descriptor);
}

void CreateFileExplorerAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateFileExplorer((EditorState*)action_data->data);
}
