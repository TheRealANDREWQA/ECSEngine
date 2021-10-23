#include "FileExplorer.h"
#include "..\Editor\EditorState.h"
#include "AssetTypes.h"
#include "..\HelperWindows.h"
#include "..\Project\ProjectOperations.h"
#include "Inspector.h"
#include "..\Editor\EditorPalette.h"

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

constexpr size_t ADD_ROW_COUNT = 2;

constexpr size_t FILE_RIGHT_CLICK_ROW_COUNT = 5;
char* FILE_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path";

constexpr size_t FOLDER_RIGHT_CLICK_ROW_COUNT = 5;
char* FOLDER_RIGHT_CLICK_CHARACTERS = "Open\nShow In Explorer\nDelete\nRename\nCopy Path";

constexpr const char* RENAME_LABEL_FILE_NAME = "Rename File";
constexpr const char* RENAME_LABEL_FILE_INPUT_NAME = "New file name";

constexpr const char* RENAME_LABEL_FOLDER_NAME = "Rename Folder";
constexpr const char* RENAME_LABEL_FOLDER_INPUT_NAME = "New folder name";

enum FILE_RIGHT_CLICK_INDEX {
	FILE_RIGHT_CLICK_OPEN,
	FILE_RIGHT_CLICK_SHOW_IN_EXPLORER,
	FILE_RIGHT_CLICK_DELETE,
	FILE_RIGHT_CLICK_RENAME,
	FILE_RIGHT_CLICK_COPY_PATH
};

enum FOLDER_RIGHT_CLICK_INDEX {
	FOLDER_RIGHT_CLICK_OPEN,
	FOLDER_RIGHT_CLICK_SHOW_IN_EXPLORER,
	FOLDER_RIGHT_CLICK_DELETE,
	FOLDER_RIGHT_CLICK_RENAME,
	FOLDER_RIGHT_CLICK_COPY_PATH
};

constexpr size_t FILTER_RECURSIVE = 1 << 0;

using Hash = HashFunctionAdditiveString;

void ChangeFileExplorerDirectory(EditorState* editor_state, Stream<wchar_t> path) {
	EDITOR_STATE(editor_state);

	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
	data->current_directory.Copy(path);
	data->current_directory[path.size] = L'\0';
	data->current_file.size = 0;
}

void ChangeFileExplorerFile(EditorState* editor_state, Stream<wchar_t> path) {
	EDITOR_STATE(editor_state);

	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
	data->current_file.Copy(path);
	data->current_file[path.size] = L'\0';
} 

struct SelectableData {
	bool IsTheSameData(const SelectableData* other) {
		return (other != nullptr) && function::CompareStrings(selection, other->selection);
	}

	EditorState* editor_state;
	Path selection;
	Timer timer;
};

void FileExplorerSetNewDirectory(EditorState* editor_state, Stream<wchar_t> path) {
	ChangeFileExplorerDirectory(editor_state, path);
	ChangeInspectorToFolder(editor_state, path);
}

void FileExplorerSetNewFile(EditorState* editor_state, Stream<wchar_t> path) {
	ChangeFileExplorerFile(editor_state, path);
	ChangeInspectorToFile(editor_state, path);
}

void FileExplorerResetFile(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	FileExplorerData* data = (FileExplorerData*)editor_state->file_explorer_data;
	data->current_file.size = 0;
	data->current_file[0] = L'\0';
}

void FileExplorerSelectableBase(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectableData* data = (SelectableData*)_data;
	if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
		if (mouse_tracker->LeftButton() == MBPRESSED) {
			FileExplorerSetNewFile(data->editor_state, data->selection);
		}
	}
	else {
		FileExplorerResetFile(data->editor_state);
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
				if (additional_data->timer.GetDurationSinceMarker_ms() < DOUBLE_CLICK_DURATION) {
					FileExplorerSetNewDirectory(data->editor_state, data->selection);
				}
			}
			data->timer.SetMarker();
			FileExplorerData* explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;
			explorer_data->current_file.Copy(data->selection);
			explorer_data->current_file[data->selection.size] = L'\0';
			ChangeInspectorToFolder(data->editor_state, data->selection);
		}
	}
	else {
		FileExplorerResetFile(data->editor_state);
	}
}

void FileExplorerChangeDirectoryFromFile(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* data = (EditorState*)_data;
	FileExplorerData* explorer_data = (FileExplorerData*)data->file_explorer_data;
	FileExplorerSetNewDirectory(data, explorer_data->current_file);
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

struct ForEachData {
	EditorState* editor_state;
	UIDrawer<false>* drawer;
	UIDrawConfig* config;
	unsigned int element_count;
};

bool IsElementSelected(FileExplorerData* data, Path path) {
	return function::CompareStrings(data->current_file, path);
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

	ascii_stream[path_filename.size] = function::PredicateValue<char>(extension_size > 0, '.', '\0');
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
	data->data->current_file.size = 0;
}

#pragma region File functors

#define EXPAND_ACTION UI_UNPACK_ACTION_DATA; \
\
ForEachData* for_each_data = (ForEachData*)_additional_data; \
SelectableData* data = (SelectableData*)_data; \
\
UIDrawer<false>* drawer = for_each_data->drawer; \
UIDrawConfig* config = for_each_data->config; 

void TextureDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, data->selection.buffer);
}

void FileBlankDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_BLANK);
}

void FileCDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_C);
}

void FileCppDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_CPP);
}

void FileConfigDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG);
}

void FileTextDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_TEXT);
}

void FileShaderDraw(ActionData* action_data) {
	EXPAND_ACTION;
	
	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_SHADER);
}

void FileEditorDraw(ActionData* action_data) {
	EXPAND_ACTION;

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_EDITOR);
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

#pragma region File Right Click Handlers

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FILE_RIGHT_CLICK_ROW_COUNT);
		data->file_right_click_handlers.InitializeFromBuffer(allocation, FILE_RIGHT_CLICK_ROW_COUNT, FILE_RIGHT_CLICK_ROW_COUNT);
		data->file_right_click_handlers[FILE_RIGHT_CLICK_OPEN] = { OpenFileWithDefaultApplicationStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_SHOW_IN_EXPLORER] = { LaunchFileExplorerStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_DELETE] = { DeleteFileStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->file_right_click_handlers[FILE_RIGHT_CLICK_RENAME] = { RenameFileWizardStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };

		auto CopyPath = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ECS_TEMP_ASCII_STRING(ascii_path, 256);
			Stream<wchar_t>* path = (Stream<wchar_t>*)_data;
			function::ConvertWideCharsToASCII(*path, ascii_path);
			system->m_application->WriteTextToClipboard(ascii_path.buffer);
		};

		data->file_right_click_handlers[FILE_RIGHT_CLICK_COPY_PATH] = { CopyPath, &data->right_click_stream, 0 };

#pragma endregion

#pragma region Folder Right Click Handlers

		allocation = drawer.GetMainAllocatorBuffer(sizeof(UIActionHandler) * FOLDER_RIGHT_CLICK_ROW_COUNT);
		data->folder_right_click_handlers.InitializeFromBuffer(allocation, FOLDER_RIGHT_CLICK_ROW_COUNT, FOLDER_RIGHT_CLICK_ROW_COUNT);
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_OPEN] = { FileExplorerChangeDirectoryFromFile, editor_state, 0 };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_SHOW_IN_EXPLORER] = { LaunchFileExplorerStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_DELETE] = { DeleteFolderContentsStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_RENAME] = { RenameFolderWizardStreamAction, &data->right_click_stream, 0, UIDrawPhase::System };
		data->folder_right_click_handlers[FOLDER_RIGHT_CLICK_COPY_PATH] = { CopyPath, &data->right_click_stream, 0 };

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
		delimiter = (char*)function::PredicateValue<uintptr_t>(delimiter == nullptr, (uintptr_t)ascii_path.buffer, (uintptr_t)delimiter);
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
			delimiter = (char*)function::PredicateValue<uintptr_t>(delimiter == nullptr, (uintptr_t)current_path.buffer, (uintptr_t)delimiter);
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

			bool is_valid = true;
			ActionData action_data = drawer->GetDummyActionData();
			action_data.additional_data = &is_valid;
			action_data.data = &selectable_data;
			filter_functors[data->filter_stream.size > 0](&action_data);

			if (is_valid) {
				drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FOLDER);

				bool is_selected = IsElementSelected(data, path);

				UIConfigGeneralAction general_action;
				general_action.handler = { FileExplorerDirectorySelectable, &selectable_data, sizeof(selectable_data) };
				config->AddFlag(general_action);

				auto OnRightClickCallback = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					SelectableData* data = (SelectableData*)_data;
					ChangeFileExplorerFile(data->editor_state, data->selection);
					ChangeInspectorToFolder(data->editor_state, data->selection);
					FileExplorerData* explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;
					explorer_data->right_click_stream.size = data->selection.size;
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

			bool is_valid = true;
			ActionData action_data = drawer->GetDummyActionData();
			action_data.system = drawer->system;
			action_data.data = &selectable_data;
			action_data.additional_data = &is_valid;

			filter_functors[data->filter_stream.size > 0](&action_data);

			if (is_valid && extension.size > 0) {
				if (data->file_functors.TryGetValue(hash, identifier, functor)) {
					action_data.additional_data = _data;

					functor(&action_data);
				}
				else {
					drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE>(*config, ECS_TOOLS_UI_TEXTURE_FILE_BLANK);
				}

				bool is_selected = IsElementSelected(data, path);

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
					Stream<wchar_t> path;
				};

				OnRightClickData* action_data = (OnRightClickData*)right_click_data.action_data;
				action_data->editor_state = _data->editor_state;
				action_data->path = path;

				auto OnRightClickAction = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					OnRightClickData* data = (OnRightClickData*)_data;
					action_data->data = data->editor_state;
					FileExplorerSetNewFile(data->editor_state, data->path);
					FileExplorerData* explorer_data = (FileExplorerData*)data->editor_state->file_explorer_data;
					explorer_data->right_click_stream.size = data->path.size;
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

void FileExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	descriptor.window_name = FILE_EXPLORER_WINDOW_NAME;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;

	descriptor.draw = FileExplorerDraw<false>;
	descriptor.initialize = FileExplorerDraw<true>;
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
