#include "Inspector.h"
#include "InspectorData.h"
#include "../Editor/EditorState.h"
#include "../HelperWindows.h"
#include "../OSFunctions.h"
#include "../Editor/EditorEvent.h"
#include "FileExplorer.h"

using namespace ECSEngine;
ECS_CONTAINERS;
ECS_TOOLS;

constexpr float2 WINDOW_SIZE = float2(0.5f, 1.2f);
#define INSPECTOR_DATA_NAME "Inspector Data"

constexpr float ICON_SIZE = 0.07f;

constexpr size_t HASH_TABLE_CAPACITY = 32;
constexpr float2 TEXT_FILE_BORDER_OFFSET = { 0.005f, 0.005f };
constexpr float TEXT_FILE_ROW_OFFSET = 0.008f;
constexpr float C_FILE_ROW_OFFSET = 0.015f;

using HashFunction = HashFunctionAdditiveString;

void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = InspectorWindowDraw<false>;
	descriptor.initialize = InspectorWindowDraw<true>;

	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
	descriptor.window_name = INSPECTOR_WINDOW_NAME;
}

void AddInspectorTableFunction(InspectorTable* table, InspectorDrawFunction function, const wchar_t* _identifier) {
	ResourceIdentifier identifier(_identifier);
	unsigned int hash = HashFunction::Hash(identifier);

	ECS_ASSERT(table->Find(hash, identifier) == -1);
	ECS_ASSERT(!table->Insert(hash, function, identifier));
}

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorDrawFunction& function, Stream<wchar_t> _identifier) {
	ResourceIdentifier identifier(_identifier.buffer, _identifier.size * sizeof(wchar_t));
	unsigned int hash = HashFunction::Hash(identifier);

	InspectorData* data = (InspectorData*)editor_state->inspector_data;
	return data->table.TryGetValue(hash, identifier, function);
}

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorDrawFunction& function, const wchar_t* _identifier) {
	return TryGetInspectorTableFunction(editor_state, function, ToStream(_identifier));
}

void InspectorIcon(UIDrawer<false>* drawer, const wchar_t* path) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = icon_size / drawer->GetStaticElementDefaultScale();
	config.AddFlag(transform);

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM>(config, path);
}

void InspectorIconNameAndPath(UIDrawer<false>* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	Stream<wchar_t> folder_name = function::PathStem(path);
	ECS_TEMP_ASCII_STRING(ascii_path, 256);
	function::ConvertWideCharsToASCII(path.buffer, ascii_path.buffer, path.size + 1, 256);
	drawer->TextLabel<UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT>(config, ascii_path.buffer + (folder_name.buffer - path.buffer));
	drawer->NextRow();
	drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, ascii_path.buffer);
	drawer->NextRow();
	drawer->CrossLine();
}

void InspectorShowButton(UIDrawer<false>* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button<UI_CONFIG_DO_NOT_CACHE>(config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

void InspectorOpenAndShowButton(UIDrawer<false>* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button<UI_CONFIG_DO_NOT_CACHE>(config, "Open", { OpenFileWithDefaultApplicationStreamAction, &path, sizeof(path) });

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer->GetLabelScale("Show");
	transform.position = drawer->GetAlignedToRight(transform.scale.x);
	config.AddFlag(transform);
	drawer->Button<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

void InspectorDrawFileTimes(UIDrawer<false>* drawer, const wchar_t* path) {
	char creation_time[256];
	char write_time[256];
	char access_time[256];
	bool success = OS::GetFileTimes(path, creation_time, access_time, write_time);

	UIDrawConfig config;
	// Display file times
	if (success) {
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Creation time: ");
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, creation_time);
		drawer->NextRow();

		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Access time: ");
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, access_time);
		drawer->NextRow();

		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Last write time: ");
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, write_time);
		drawer->NextRow();
	}
	// Else error message
	else {
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Could not retrieve directory times.");
		drawer->NextRow();
	}
}

void InspectorDrawFolderInfo(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FOLDER);

	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	struct OpenFolderData {
		EditorState* state;
		const wchar_t* path;
	};

	auto OpenFileExplorerFolder = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		OpenFolderData* data = (OpenFolderData*)_data;
		Stream<wchar_t> path = ToStream(data->path);
		ChangeFileExplorerDirectory(data->state, path);
	};

	UIDrawConfig config;

	config.flag_count = 0;
	OpenFolderData open_data;
	open_data.path = path;
	open_data.state = editor_state;
	drawer->Button<UI_CONFIG_DO_NOT_CACHE>(config, "Open", { OpenFileExplorerFolder, &open_data, sizeof(open_data) });

	UIConfigAbsoluteTransform absolute_transform;
	absolute_transform.scale = drawer->GetLabelScale("Show");
	absolute_transform.position = drawer->GetAlignedToRight(absolute_transform.scale.x);
	config.AddFlag(absolute_transform);
	drawer->Button<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Show", { LaunchFileExplorerAction, (void*)path, 0 });
}

void InspectorDrawTexture(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	InspectorIcon(drawer, path);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	InspectorOpenAndShowButton(drawer, stream_path);
}

void InspectorDrawBlankFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);
	InspectorOpenAndShowButton(drawer, stream_path);
}

void InspectorDrawTextFileImplementation(EditorState* editor_state, void* data, UIDrawer<false>* drawer, const wchar_t* icon_texture, float row_offset) {
	EDITOR_STATE(editor_state);
	const wchar_t* path = (const wchar_t*)data;

	InspectorIcon(drawer, icon_texture);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	float2* stabilized_render_span = DrawTextFileEx(drawer, path, TEXT_FILE_BORDER_OFFSET, TEXT_FILE_ROW_OFFSET);

	InspectorOpenAndShowButton(drawer, stream_path);
	*stabilized_render_span = drawer->GetRenderSpan();
}

void InspectorDrawTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_TEXT, TEXT_FILE_ROW_OFFSET);
}

void InspectorDrawCTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_C, C_FILE_ROW_OFFSET);
}

void InspectorDrawCppTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_CPP, C_FILE_ROW_OFFSET);
}

void InspectorDrawHlslTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_SHADER, C_FILE_ROW_OFFSET);
}

template<bool initialize>
void InspectorWindowDraw(void* window_data, void* drawer_descriptor) {
	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	InspectorData* data = (InspectorData*)editor_state->inspector_data;

	if constexpr (initialize) {
		data->draw_function = nullptr;
		data->data_size = 0;
		data->draw_data = nullptr;

		void* allocation = drawer.GetMainAllocatorBuffer(InspectorTable::MemoryOf(HASH_TABLE_CAPACITY));
		data->table.InitializeFromBuffer(allocation, HASH_TABLE_CAPACITY);

		AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".png");
		AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".jpg");
		AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".tiff");
		AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".bmp");
		AddInspectorTableFunction(&data->table, InspectorDrawTextFile, L".txt");
		AddInspectorTableFunction(&data->table, InspectorDrawTextFile, L".md");
		AddInspectorTableFunction(&data->table, InspectorDrawCppTextFile, L".cpp");
		AddInspectorTableFunction(&data->table, InspectorDrawCppTextFile, L".h");
		AddInspectorTableFunction(&data->table, InspectorDrawCppTextFile, L".hpp");
		AddInspectorTableFunction(&data->table, InspectorDrawCTextFile, L".c");
		AddInspectorTableFunction(&data->table, InspectorDrawHlslTextFile, L".hlsl");
		AddInspectorTableFunction(&data->table, InspectorDrawHlslTextFile, L".hlsli");

		ChangeInspectorToFile(editor_state, ToStream(L"C:\\Users\\Andrei\\Test\\Assets\\Textures\\ErrorIcon.png"));
	}

	if constexpr (!initialize) {
		if (data->draw_function != nullptr) {
			data->draw_function(editor_state, data->draw_data, &drawer);
		}
	}
}

unsigned int CreateInspectorWindow(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	size_t stack_memory[128];
	InspectorSetDescriptor(descriptor, editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
}

void CreateInspector(EditorState* editor_state) {
	CreateDockspaceFromWindow(INSPECTOR_WINDOW_NAME, editor_state, CreateInspectorWindow);
}

void CreateInspectorAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateInspector((EditorState*)_data);
}

void ChangeInspectorDrawFunction(EditorState* editor_state, InspectorDrawFunction draw_function, void* data, size_t data_size)
{
	EDITOR_STATE(editor_state);

	InspectorData* inspector_data = (InspectorData*)editor_state->inspector_data;
	if (inspector_data->data_size > 0) {
		editor_allocator->Deallocate(inspector_data->draw_data);
	}
	inspector_data->draw_data = function::AllocateMemory(editor_allocator, data, data_size);
	inspector_data->draw_function = draw_function;
	inspector_data->data_size = data_size;
}

void ChangeInspectorToFolder(EditorState* editor_state, Stream<wchar_t> path) {
	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';

	ChangeInspectorDrawFunction(editor_state, InspectorDrawFolderInfo, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
}

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path) {
	InspectorDrawFunction draw_function;
	Stream<wchar_t> extension = function::PathExtension(path);

	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';
	if (extension.size == 0 || !TryGetInspectorTableFunction(editor_state, draw_function, extension)) {
		draw_function = InspectorDrawBlankFile;
	}
	ChangeInspectorDrawFunction(editor_state, draw_function, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
}

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index) {
	
}

void ChangeInspectorToGraphicsModule(EditorState* editor_state) {

}