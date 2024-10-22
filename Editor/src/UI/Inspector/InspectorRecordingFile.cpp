#include "editorpch.h"
#include "InspectorUtilities.h"
#include "../Inspector.h"
#include "../../Sandbox/SandboxRecordingFileExtension.h"
#include "../../Editor/EditorPalette.h"
#include "../../Editor/EditorState.h"

using namespace ECSEngine;

#define ERROR_MESSAGE_CAPACITY (ECS_KB * 4)
#define RETAINED_TIMER_TICK_MS 500

struct DrawWindowData {
	Stream<wchar_t> path;
	DeltaStateReader reader;
	ResizableLinearAllocator temporary_allocator;
	CapacityStream<char> error_message;
	size_t file_time_stamp;
	size_t file_byte_size;
	// This flag indicates that there was an error in opening the file. If true, the file could not be opened
	bool reader_not_initialized;
	// This is set to false for the very first draw, after which it is set to true.
	bool initial_draw;
	// Used to check the timestamp of the file to check for changes
	Timer retained_timer;
};

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

	draw_data->file_time_stamp = GetFileLastWrite(file);
	draw_data->file_byte_size = GetFileByteSize(file);
	ScopedFile scope_deleter = { { file } };

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

static void InspectorRecordingFileClean(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	DrawWindowData* data = (DrawWindowData*)_data;
	// Only the allocator must be freed
	data->temporary_allocator.Free();
}

static bool InspectorRecordingFileRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	DrawWindowData* data = (DrawWindowData*)window_data;
	if (data->retained_timer.GetDurationFloat(ECS_TIMER_DURATION_MS) >= RETAINED_TIMER_TICK_MS) {
		data->retained_timer.SetNewStart();

		size_t current_time_stamp = OS::GetFileLastWrite(data->path);
		if (current_time_stamp != data->file_time_stamp) {
			data->file_time_stamp = current_time_stamp;
			ReadFileInfo(data);
			return false;
		}
	}
	return data->initial_draw;
}

void InspectorDrawRecordingFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	DrawWindowData* data = (DrawWindowData*)_data;

	// If the file no longer exists, change the inspector to nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	data->initial_draw = true;
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

	UIDrawConfig config;
	if (data->error_message.size > 0) {
		// This includes the case when the file could not be opened
		drawer->SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, data->error_message);
		drawer->NextRow();
	}
	else {
		// Display the number of entire states and delta states
		ECS_STACK_CAPACITY_STREAM(char, temp_characters, 256);
		temp_characters.CopyOther("Byte size: ");
		ConvertByteSizeToString(data->file_byte_size, temp_characters);
		drawer->Text(temp_characters);
		drawer->NextRow();

		temp_characters.CopyOther("Entire state count: ");
		ConvertIntToChars(temp_characters, data->reader.entire_state_indices.size);
		drawer->Text(temp_characters);
		drawer->NextRow();

		temp_characters.CopyOther("Delta state count: ");
		ConvertIntToChars(temp_characters, data->reader.state_infos.size - data->reader.entire_state_indices.size);
		drawer->Text(temp_characters);
		drawer->NextRow();

		UIDrawerTimeline timeline;
		ZeroOut(&timeline);
		
		// Use the global memory manager of the editor because this can get quite large
		Stream<UIDrawerTimelineElement> channel_elements;
		channel_elements.Initialize(editor_state->GlobalMemoryManager(), data->reader.state_infos.size);
		for (size_t index = 0; index < channel_elements.size; index++) {
			bool is_entire_state = data->reader.IsEntireState(index);
			channel_elements[index].time = data->reader.state_infos[index].elapsed_seconds;
			channel_elements[index].texture_index = is_entire_state;
			channel_elements[index].color = is_entire_state ? EDITOR_DEEP_BLUE_COLOR : EDITOR_YELLOW_COLOR;
		}

		ECS_STACK_CAPACITY_STREAM(UIDrawerTimelineChannel, channels, 1);
		channels.ReserveRange();
		channels[0].row_y_size = 0.2f;
		channels[0].description = "";
		channels[0].elements = channel_elements;
		channels[0].entry_y_size = 0.03f;

		ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, textures, 2);
		textures.size = 2;
		textures[0] = ECS_TOOLS_UI_TEXTURE_RHOMBUS;
		textures[1] = ECS_TOOLS_UI_TEXTURE_MASK;

		timeline.time_range.x = 0.0f;
		timeline.time_range.y = data->reader.state_infos[data->reader.state_infos.size - 1].elapsed_seconds;
		timeline.has_time_range = true;
		timeline.channels = channels;
		timeline.texture_paths = textures;
		timeline.time_indication_precision = 2;

		drawer->Timeline("Timeline", &timeline);

		channel_elements.Deallocate(editor_state->GlobalMemoryManager());
	}
}

static InspectorFunctions GetInspectorFunctions() {
	return { InspectorDrawRecordingFile, InspectorRecordingFileClean, InspectorRecordingFileRetainedMode };
}

void InspectorRecordingFileAddFunctors(InspectorTable* table) {
	AddInspectorTableFunction(table, GetInspectorFunctions(), EDITOR_INPUT_RECORDING_FILE_EXTENSION);
	AddInspectorTableFunction(table, GetInspectorFunctions(), EDITOR_STATE_RECORDING_FILE_EXTENSION);
}

void ChangeInspectorToRecordingFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	DrawWindowData data;
	data.path = path;
	ZeroOut(&data);

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	inspector_index = ChangeInspectorDrawFunctionWithSearch(
		editor_state,
		inspector_index,
		GetInspectorFunctions(),
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