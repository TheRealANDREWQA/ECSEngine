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
	EDITOR_STATE(editor_state);

	editor_allocator->Deallocate(data->buffer);
	for (size_t index = 0; index < data->size; index++) {
		editor_allocator->Deallocate(data->buffer[index]);
	}
	ReleaseLockedWindow(action_data);
}

void MissingProjectsDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	Stream<const char*>* paths = (Stream<const char*>*)window_data;

	UIDrawConfig config;
	drawer.Text(UI_CONFIG_DO_NOT_CACHE, config, "One or more project are missing. Their files have been deleted or moved and cannot be opened.");
	drawer.NextRow();
	drawer.LabelList(UI_CONFIG_DO_NOT_CACHE, config, "These are:", *paths);

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer.GetLabelScale("OK");
	transform.position.x = drawer.GetAlignedToCenterX(transform.scale.x);
	transform.position.y = drawer.GetAlignedToBottom(transform.scale.y).y;
	config.AddFlag(transform);
	
	drawer.Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM, config, "OK", { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });
}

void CreateMissingProjectWindow(EditorState* editor_state, Stream<const char*> paths) {
	auto editor_event = [](EditorState* editor_state, void* ECS_RESTRICT data) {
		Stream<const char*>* paths = (Stream<const char*>*)data;
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

		EDITOR_STATE(editor_state);
		ui_system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_LOCK_WINDOW | UI_DOCKSPACE_NO_DOCKING | UI_POP_UP_WINDOW_FIT_TO_CONTENT
			| UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER | UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE);
	};
	
	EditorAddEvent(editor_state, editor_event, &paths, sizeof(paths));
}

bool SaveEditorFile(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	HubData* hub_data = (HubData*)editor_state->hub_data;
	ECS_FILE_HANDLE file = 0;
	ECS_FILE_STATUS_FLAGS status = OpenFile(EDITOR_FILE, &file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_TRUNCATE_FILE);

	if (status == ECS_FILE_STATUS_OK) {
		ScopedFile scoped_file(file);

		unsigned short project_count = hub_data->projects.size;
		bool success = WriteFile(file, { &project_count, sizeof(project_count) });
		for (size_t index = 0; index < hub_data->projects.size && success; index++) {
			unsigned short project_path_size = (unsigned short)wcslen(hub_data->projects[index].path);
			success &= WriteFile(file, { &project_path_size, sizeof(project_path_size) });
			success &= WriteFile(file, { hub_data->projects[index].path, sizeof(wchar_t) * project_path_size });
		}

		success &= FlushFileToDisk(file);
		return success;
	}
	return false;
}

bool LoadEditorFile(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	HubData* hub_data = (HubData*)editor_state->hub_data;

	ECS_FILE_HANDLE file = 0;
	ECS_FILE_STATUS_FLAGS status = OpenFile(EDITOR_FILE, &file, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_BINARY);

	const char* _invalid_file_paths[64];
	Stream<const char*> invalid_file_paths(_invalid_file_paths, 0);

	wchar_t temp_path[256];

	if (status == ECS_FILE_STATUS_OK) {
		ScopedFile scoped_file(file);

		unsigned short project_count = 0;
		bool success = true;
		success &= ReadFile(file, { &project_count, sizeof(project_count) });
		if (project_count >= hub_data->projects.capacity) {
			return false;
		}
		
		hub_data->projects.size = 0;
		for (size_t index = 0; index < project_count && success; index++) {
			unsigned short path_size = 0;
			success &= ReadFile(file, { &path_size, sizeof(path_size) });

			if (path_size == 0) {
				return false;
			}

			// If the file doesn't exist, it means it has been destroyed before hand in the OS
			// So add it to the invalid stream
			success &= ReadFile(file, { temp_path, sizeof(wchar_t) * path_size });
			Path current_path(temp_path, path_size);
			current_path[path_size] = L'\0';
			if (!ExistsFileOrFolder(current_path)) {
				void* allocation = editor_allocator->Allocate(sizeof(char) * (current_path.size + 1), alignof(char));
				CapacityStream<char> allocated_path(allocation, 0, current_path.size + 1);
				function::ConvertWideCharsToASCII(current_path, allocated_path);
				invalid_file_paths.Add(allocated_path.buffer);
			}
			else {
				unsigned int project_index = hub_data->projects.size;
				hub_data->projects[project_index].error_message = nullptr;
				hub_data->projects[project_index].path = (wchar_t*)editor_allocator->Allocate(sizeof(wchar_t) * (path_size + 1), alignof(wchar_t));
				wchar_t* mutable_ptr = (wchar_t*)hub_data->projects[project_index].path;
				memcpy(mutable_ptr, temp_path, sizeof(wchar_t) * path_size);
				mutable_ptr[path_size] = L'\0';
				hub_data->projects.size++;
			}
		}

		// Make an allocation for the stream pointers
		if (invalid_file_paths.size > 0) {
			void* allocation = editor_allocator->Allocate(sizeof(const char*) * invalid_file_paths.size);
			memcpy(allocation, invalid_file_paths.buffer, sizeof(const char*) * invalid_file_paths.size);
			CreateMissingProjectWindow(editor_state, Stream<const char*>(allocation, invalid_file_paths.size));

			SaveEditorFile(editor_state);
		}

		return success;
	}
	return false;
}

void SaveEditorFileThreadTask(unsigned int thread_index, World* world, void* data) {
	bool success = SaveEditorFile((EditorState*)data);
	if (!success) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.Copy(ToStream(SAVE_FILE_ERROR_MESSAGE));
		EditorSetConsoleError(error_message);
	}
}

void SaveEditorFileAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	bool success = SaveEditorFile((EditorState*)_data);
	if (!success) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.Copy(ToStream(SAVE_FILE_ERROR_MESSAGE));
		EditorSetConsoleError(error_message);
	}
}

void LoadEditorFileAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	bool success = LoadEditorFile((EditorState*)_data);
	if (!success) {
		ECS_TEMP_ASCII_STRING(error_message, 256);
		error_message.Copy(ToStream(LOAD_FILE_ERROR_MESSAGE));
		EditorSetConsoleError(error_message);
	}
}