#include "editorpch.h"
#include "EditorFile.h"
#include "EditorState.h"
#include "..\UI\Hub.h"
#include "EditorEvent.h"

using namespace ECSEngine;
ECS_TOOLS;

#define SAVE_FILE_ERROR_MESSAGE "Saving editor file failed."
#define LOAD_FILE_ERROR_MESSAGE "Loading editor file failed."

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
	ECS_FILE_STATUS_FLAGS status = OpenFile(EDITOR_FILE, &file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_TRUNCATE_FILE);

	if (status == ECS_FILE_STATUS_OK) {
		ScopedFile scoped_file({ file });

		unsigned short project_count = hub_data->projects.size;
		bool success = WriteFile(file, { &project_count, sizeof(project_count) });
		for (size_t index = 0; index < hub_data->projects.size && success; index++) {
			unsigned short project_path_size = (unsigned short)hub_data->projects[index].data.path.size;
			success &= WriteFile(file, { &project_path_size, sizeof(project_path_size) });
			success &= WriteFile(file, { hub_data->projects[index].data.path.buffer, sizeof(wchar_t) * project_path_size });
		}

		//success &= FlushFileToDisk(file);
		return success;
	}
	return false;
}

bool LoadEditorFile(EditorState* editor_state) {
	HubData* hub_data = (HubData*)editor_state->hub_data;

	ECS_FILE_HANDLE file = 0;
	ECS_FILE_STATUS_FLAGS status = OpenFile(EDITOR_FILE, &file, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_BINARY);

	const char* _invalid_file_paths[64];
	Stream<Stream<char>> invalid_file_paths(_invalid_file_paths, 0);

	wchar_t temp_path[256];

	if (status == ECS_FILE_STATUS_OK) {
		ScopedFile scoped_file({ file });

		unsigned short project_count = 0;
		bool success = true;
		success &= ReadFileExact(file, { &project_count, sizeof(project_count) });
		if (project_count >= hub_data->projects.capacity) {
			return false;
		}
		
		hub_data->projects.size = 0;
		for (unsigned short index = 0; index < project_count && success; index++) {
			unsigned short path_size = 0;
			success &= ReadFileExact(file, { &path_size, sizeof(path_size) });

			if (path_size == 0) {
				return false;
			}

			// If the file doesn't exist, it means it has been destroyed beforehand in the OS
			// So add it to the invalid stream
			success &= ReadFileExact(file, { temp_path, sizeof(wchar_t) * path_size });
			Path current_path(temp_path, path_size);
			current_path[path_size] = L'\0';
			if (!ValidateProjectPath(current_path)) {
				void* allocation = Allocate(editor_state->EditorAllocator(), sizeof(char) * (current_path.size + 1), alignof(char));
				CapacityStream<char> allocated_path(allocation, 0, current_path.size + 1);
				ConvertWideCharsToASCII(current_path, allocated_path);
				invalid_file_paths.Add(allocated_path.buffer);
			}
			else {
				AddHubProject(editor_state, current_path);
			}
		}

		// Make an allocation for the stream pointers
		if (invalid_file_paths.size > 0) {
			void* allocation = Allocate(editor_state->EditorAllocator(), sizeof(Stream<char>) * invalid_file_paths.size);
			memcpy(allocation, invalid_file_paths.buffer, sizeof(Stream<char>) * invalid_file_paths.size);
			CreateMissingProjectWindow(editor_state, Stream<Stream<char>>(allocation, invalid_file_paths.size));

			SaveEditorFile(editor_state);
		}

		return success;
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