#include "editorpch.h"
#include "EditorFile.h"
#include "EditorState.h"
#include "..\UI\Hub.h"
#include "EditorEvent.h"

using namespace ECSEngine;
ECS_TOOLS;

#define SAVE_FILE_ERROR_MESSAGE "Saving editor file failed."
//#define LOAD_FILE_ERROR_MESSAGE "Loading editor file failed."

#define COMPILER_PATH_STRING "Compiler Path: "
#define EDITING_IDE_PATH_STRING "Editing IDE Path: "
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
	drawer.Text("One or more projects are missing. Their files have been deleted or moved and cannot be opened.");
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

		ECS_STACK_CAPACITY_STREAM(char, editing_ide_path_ascii, 512);
		ConvertWideCharsToASCII(editor_state->settings.editing_ide_path, editing_ide_path_ascii);
		editing_ide_path_ascii.AddAssert('\n');
		success &= WriteFile(file, Stream<char>(EDITING_IDE_PATH_STRING));
		success &= WriteFile(file, editing_ide_path_ascii);

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

bool LoadEditorFile(EditorState* editor_state, CapacityStream<char>* error_message) {
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

		// If there are not at least 2 lines, fail
		if (lines.size < 2) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Editor settings should have at least 2 lines.");
			return false;
		}

		if (!lines[0].StartsWith(COMPILER_PATH_STRING)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Editor settings do not start with a compiler path.");
			return false;
		}

		if (!lines[1].StartsWith(EDITING_IDE_PATH_STRING)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Editor settings do not contain the editing IDE executable path.");
			return false;
		}

		auto read_single_line_path = [editor_state](Stream<char> line, Stream<char> line_identifier_string) -> Stream<wchar_t> {
			Stream<char> compiler_path_ascii = line.AdvanceReturn(line_identifier_string.size);
			compiler_path_ascii = TrimWhitespace(compiler_path_ascii);

			// Add a null terminator such that the final path contains it at the end
			compiler_path_ascii.Add('\0');
			ECS_STACK_CAPACITY_STREAM(wchar_t, compiler_path, 512);
			ConvertASCIIToWide(compiler_path, compiler_path_ascii);
			Stream<wchar_t> compiler_path_stable = compiler_path.Copy(editor_state->EditorAllocator());
			// Don't include the null terminator in the path size
			compiler_path_stable.size--;
			
			return compiler_path_stable;
		};

		editor_state->settings.compiler_path = read_single_line_path(lines[0], COMPILER_PATH_STRING);
		if (!ExistsFileOrFolder(editor_state->settings.compiler_path)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Editor settings contain {#} as compiler path, but it is not a valid executable/CMD path. Make sure the path is adequate.", editor_state->settings.compiler_path);
		}

		editor_state->settings.editing_ide_path = read_single_line_path(lines[1], EDITING_IDE_PATH_STRING);

		hub_data->projects.size = 0;
		if (lines[2].StartsWith(PROJECTS_STRING)) {
			for (size_t index = 3; index < lines.size; index++) {
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

	// Try to auto generate the settings. Currently, only the compiler path is needed, which we can auto-detect
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
	ResizableStream<CompilerVersion> compiler_versions(&stack_allocator, 4);
	AutoDetectCompilers(&stack_allocator, &compiler_versions);

	if (compiler_versions.size > 0) {
		bool success = ChangeCompilerVersionAndEditingIdeExecutablePath(editor_state, compiler_versions[0].compiler_path, compiler_versions[0].editing_ide_path);
		if (!success) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "No editor file was found and auto generating a default file failed. Could not save the editor file after successfully generating a default.");
		}
		return success;
	}

	ECS_FORMAT_ERROR_MESSAGE(error_message, "There is no editor file and auto generating a default file failed. No compilers could be detected.");
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

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	bool success = LoadEditorFile((EditorState*)_data, &error_message);
	if (!success) {
		EditorSetConsoleError(error_message);
	}
}