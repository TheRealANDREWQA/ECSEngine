#include "editorpch.h"
#include "EditorFile.h"
#include "EditorState.h"
#include "..\UI\Hub.h"
#include "EditorEvent.h"

using namespace ECSEngine;
ECS_TOOLS;

#define SAVE_FILE_ERROR_MESSAGE "Saving editor file failed."
#define LOAD_FILE_ERROR_MESSAGE "Loading editor file failed."

#define COMPILER_PATH_STRING "Compiler Path: "
#define PROJECTS_STRING "Projects:"

#define MISSING_PROJECTS_WINDOW_NAME "Missing Projects"

#define MULTISECTION_COUNT 2
#define MULTISECTION_PATH 0
#define MULTISECTION_NAME 1

constexpr size_t TEMP_ALLOCATION_SIZE = 20'000;

void MissingProjectsDestroyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	Stream<const char*>* data = (Stream<const char*>*)_additional_data;
	EditorState* editor_state = (EditorState*)_data;
	
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
	Deallocate(editor_allocator, data->buffer);
	for (size_t index = 0; index < data->size; index++) {
		Deallocate(editor_allocator, data->buffer[index]);
	}
	ReleaseLockedWindow(action_data);
}

void MissingProjectsDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	Stream<Stream<char>>* paths = (Stream<Stream<char>>*)window_data;

	UIDrawConfig config;
	drawer.Text("One or more project are missing. Their files have been deleted or moved and cannot be opened.");
	drawer.NextRow();
	drawer.LabelList("These are:", *paths);

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer.GetLabelScale("OK");
	transform.position.x = drawer.GetAlignedToCenterX(transform.scale.x);
	transform.position.y = drawer.GetAlignedToBottom(transform.scale.y).y;
	config.AddFlag(transform);
	
	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "OK", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
}

void CreateMissingProjectWindow(EditorState* editor_state, Stream<Stream<char>> paths) {
	auto editor_event = [](EditorState* editor_state, void* ECS_RESTRICT data) {
		Stream<Stream<char>>* paths = (Stream<Stream<char>>*)data;
		UIWindowDescriptor descriptor;

		descriptor.window_name = MISSING_PROJECTS_WINDOW_NAME;
		descriptor.destroy_action = MissingProjectsDestroyCallback;
		descriptor.destroy_action_data = editor_state;
		descriptor.destroy_action_data_size = 0;

		descriptor.draw = MissingProjectsDraw;
		descriptor.window_data = paths;
		descriptor.window_data_size = sizeof(*paths);

		descriptor.initial_position_x = 0.0f;
		descriptor.initial_position_y = 0.0f;
		descriptor.initial_size_x = 1000.0f;
		descriptor.initial_size_y = 1000.0f;

		editor_state->ui_system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_LOCK_WINDOW | UI_DOCKSPACE_NO_DOCKING | UI_POP_UP_WINDOW_FIT_TO_CONTENT
			| UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER | UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE);
		return false;
	};
	
	EditorAddEvent(editor_state, editor_event, &paths, sizeof(paths));
}

bool SaveEditorFile(EditorState* editor_state) {
	HubData* hub_data = (HubData*)editor_state->hub_data;
	ECS_FILE_HANDLE file = 0;
	//ECS_FILE_STATUS_FLAGS status = OpenFile(EDITOR_FILE, &file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE);
	ECS_FILE_STATUS_FLAGS status = FileCreate(EDITOR_FILE, &file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE);

	if (status == ECS_FILE_STATUS_OK) {
		ScopedFile scoped_file({ file });

		ECS_STACK_CAPACITY_STREAM(char, compiler_path_ascii, 512);
		ConvertWideCharsToASCII(editor_state->settings.compiler_path, compiler_path_ascii);
		compiler_path_ascii.AddAssert('\n');
		bool success = WriteFile(file, Stream<char>(COMPILER_PATH_STRING));
		success &= WriteFile(file, compiler_path_ascii);
		success &= WriteFile(file, Stream<char>(PROJECTS_STRING "\n"));
		for (size_t index = 0; index < hub_data->projects.size; index++) {
			ECS_STACK_CAPACITY_STREAM(char, project_ascii_path, 512);
			project_ascii_path.Add('\t');
			ConvertWideCharsToASCII(hub_data->projects[index].data.path, project_ascii_path);
			project_ascii_path.AddAssert('\n');
			success &= WriteFile(file, project_ascii_path);
		}

		//success &= FlushFileToDisk(file);
		return success;
	}
	return false;
}

bool LoadEditorFile(EditorState* editor_state) {
	HubData* hub_data = (HubData*)editor_state->hub_data;

	Stream<char> file_data = ReadWholeFileText(EDITOR_FILE, editor_state->EditorAllocator());

	ECS_STACK_CAPACITY_STREAM(Stream<char>, invalid_file_paths, 64);

	if (file_data.size > 0) {
		ResizableStream<Stream<char>> lines = { editor_state->EditorAllocator(), 8 };
		SplitString(file_data, '\n', &lines);

		auto scope_deallocator = StackScope([&]() {
			file_data.Deallocate(editor_state->EditorAllocator());
			lines.FreeBuffer();
		});

		if (!lines[0].StartsWith(COMPILER_PATH_STRING)) {
			return false;
		}

		// Use ASCII paths, even tho we internally store them as wide
		Stream<char> compiler_path_ascii = lines[0].AdvanceReturn(strlen(COMPILER_PATH_STRING));
		compiler_path_ascii = TrimWhitespace(compiler_path_ascii);

		// Add a null terminator such that the final path contains it at the end
		compiler_path_ascii.Add('\0');
		ECS_STACK_CAPACITY_STREAM(wchar_t, compiler_path, 512);
		ConvertASCIIToWide(compiler_path, compiler_path_ascii);
		editor_state->settings.compiler_path = compiler_path.Copy(editor_state->EditorAllocator());
		// Don't include the null terminator in the path size
		editor_state->settings.compiler_path.size--;

		hub_data->projects.size = 0;
		if (lines[1].StartsWith(PROJECTS_STRING)) {
			for (size_t index = 2; index < lines.size; index++) {
				Stream<char> line = TrimWhitespace(lines[index]);

				ECS_STACK_CAPACITY_STREAM(wchar_t, wide_path, 512);
				ConvertASCIIToWide(wide_path, line);
				if (wide_path.size > 0) {
					if (!ValidateProjectPath(wide_path)) {
						void* allocation = Allocate(editor_state->EditorAllocator(), sizeof(char) * (wide_path.size + 1), alignof(char));
						CapacityStream<char> allocated_path(allocation, 0, wide_path.size + 1);
						ConvertWideCharsToASCII(wide_path, allocated_path);
						invalid_file_paths.AddAssert(allocated_path);
					}
					else {
						AddHubProject(editor_state, wide_path);
					}
				}
			}
		}

		// Make an allocation for the stream pointers
		if (invalid_file_paths.size > 0) {
			void* allocation = Allocate(editor_state->EditorAllocator(), sizeof(Stream<char>) * invalid_file_paths.size);
			invalid_file_paths.CopyTo(allocation);
			CreateMissingProjectWindow(editor_state, Stream<Stream<char>>(allocation, invalid_file_paths.size));

			SaveEditorFile(editor_state);
		}

		return true;
	}
	return false;
}

void SaveEditorFileThreadTask(unsigned int thread_index, World* world, void* data) {
	bool success = SaveEditorFile((EditorState*)data);
	if (!success) {
		ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
		error_message.CopyOther(SAVE_FILE_ERROR_MESSAGE);
		EditorSetConsoleError(error_message);
	}
}

void SaveEditorFileAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	bool success = SaveEditorFile((EditorState*)_data);
	if (!success) {
		ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
		error_message.CopyOther(SAVE_FILE_ERROR_MESSAGE);
		EditorSetConsoleError(error_message);
	}
}

void LoadEditorFileAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	bool success = LoadEditorFile((EditorState*)_data);
	if (!success) {
		ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
		error_message.CopyOther(LOAD_FILE_ERROR_MESSAGE);
		EditorSetConsoleError(error_message);
	}
}