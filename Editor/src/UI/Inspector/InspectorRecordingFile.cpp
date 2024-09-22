#include "editorpch.h"
#include "InspectorUtilities.h"
#include "../../Sandbox/SandboxRecordingFileExtension.h"

using namespace ECSEngine;

#define ERROR_MESSAGE_CAPACITY (ECS_KB * 4)

struct DrawWindowData {
	Stream<wchar_t> path;
	DeltaStateReader reader;
	ResizableLinearAllocator temporary_allocator;
	CapacityStream<char> error_message;
	size_t file_time_stamp;
	// This flag indicates that there was an error in opening the 
	bool reader_not_initialized;
};

static void InspectorCleanRecordingFile(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	DrawWindowData* data = (DrawWindowData*)_data;
	// Only the allocator must be freed
	data->temporary_allocator.Free();
}

static void ReadFileInfo(DrawWindowData* draw_data) {
	// Reset the allocator
	draw_data->temporary_allocator.Clear();
	draw_data->error_message.Initialize(&draw_data->temporary_allocator, 0, ERROR_MESSAGE_CAPACITY);
	draw_data->reader_not_initialized = false;

	// The path is already the absolute path
	ECS_FILE_HANDLE file = -1;
	ECS_FILE_STATUS_FLAGS status = OpenFile(draw_data->path, &file, ECS_FILE_ACCESS_READ_BINARY_SEQUENTIAL, &draw_data->error_message);
	
	if (status != ECS_FILE_STATUS_OK) {
		draw_data->reader_not_initialized = true;
		return;
	}

	FileScopeDeleter scope_deleter = { file };

	// Use a stack buffering for reading the file
	ECS_STACK_VOID_STREAM(stack_buffering, ECS_KB * 64);

	// The read instrument can be temporary, since we don't indent to actually read the contents
	BufferedFileReadInstrument read_instrument(file, stack_buffering, 0);

	DeltaStateReaderInitializeInfo initialize_info;
	ZeroOut(&initialize_info.functor_info);
	initialize_info.error_message = &draw_data->error_message;
	initialize_info.allocator = &draw_data->temporary_allocator;
	initialize_info.read_instrument = &read_instrument;
	draw_data->reader.Initialize(initialize_info);
}

void InspectorDrawRecordingFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	DrawWindowData* data = (DrawWindowData*)_data;

	Stream<wchar_t> extension = PathExtension(data->path);
	// This is not very "nice" to check manually what the extension is and display the icon according to it, but it makes for an easy API
	// Where the user does not have to specify the icon manually
	Stream<wchar_t> icon_path = ECS_TOOLS_UI_TEXTURE_FILE_BLANK;
	if (extension == EDITOR_INPUT_RECORDING_FILE_EXTENSION) {
		icon_path = ECS_TOOLS_UI_TEXTURE_KEYBOARD_SQUARE;
	}
	else if (extension == EDITOR_STATE_RECORDING_FILE_EXTENSION) {
		icon_path = ECS_TOOLS_UI_TEXTURE_REPLAY;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, icon_path);
	InspectorDefaultFileInfo(editor_state, drawer, data->path);

	// TODO: Better rew input handling - by removing legacy messages
}

void InspectorRecordingFileAddFunctors(InspectorTable* table) {
	AddInspectorTableFunction(table, { InspectorDrawRecordingFile, InspectorCleanRecordingFile }, EDITOR_INPUT_RECORDING_FILE_EXTENSION);
	AddInspectorTableFunction(table, { InspectorDrawRecordingFile, InspectorCleanRecordingFile }, EDITOR_STATE_RECORDING_FILE_EXTENSION);
}

void ChangeInspectorToRecordingFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	DrawWindowData data;
	data.path = path;
	ZeroOut(&data.reader);

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	inspector_index = ChangeInspectorDrawFunctionWithSearch(
		editor_state,
		inspector_index,
		{ InspectorDrawRecordingFile, InspectorCleanRecordingFile },
		&data,
		sizeof(data) + sizeof(wchar_t) * (path.size + 1),
		-1,
		[=](void* inspector_data) {
			const DrawWindowData* other_data = (const DrawWindowData*)inspector_data;
			return other_data->path == path;
		}
	);

	if (inspector_index != -1) {
		// Get the data and set the path
		DrawWindowData* draw_data = (DrawWindowData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
		draw_data->path = { OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->path.CopyOther(path);
		UpdateLastInspectorTargetData(editor_state, inspector_index, draw_data);

		SetLastInspectorTargetInitialize(editor_state, inspector_index, [](EditorState* editor_state, void* data, unsigned int inspector_index) {
			DrawWindowData* draw_data = (DrawWindowData*)data;

			// Initialize the allocator - a resizable linear allocator is sufficient for reading the header
			draw_data->temporary_allocator = ResizableLinearAllocator(ECS_MB, ECS_MB * 128, { nullptr });
			ReadFileInfo(draw_data);
		});
	}
}