#include "editorpch.h"
#include "Backups.h"
#include "HelperWindows.h"
#include "../Editor/EditorState.h"
#include "../Project/ProjectBackup.h"
#include "../Project/ProjectFolders.h"

using namespace ECSEngine;

#define WINDOW_SIZE float2(0.55f, 0.9f)
#define REFRESH_DURATION_MS 500

struct PathEntry {
	ECS_INLINE bool operator < (const PathEntry& entry) const {
		return creation_time < entry.creation_time;
	}

	ECS_INLINE bool operator == (const PathEntry& entry) const {
		return creation_time == entry.creation_time;
	}

	Stream<wchar_t> path;
	size_t creation_time;
};

struct BackupsWindowData {
	EditorState* editor_state;
	Timer refresh_timer;

	ResizableLinearAllocator temporary_allocator;
	ResizableStream<PathEntry> paths;
};

// ----------------------------------------------------------------------------------------------------------------

static bool BackupsWindowRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	BackupsWindowData* data = (BackupsWindowData*)window_data;
	return !data->refresh_timer.HasPassedAndReset(ECS_TIMER_DURATION_MS, REFRESH_DURATION_MS);
}

static void BackupsWindowDestroy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	BackupsWindowData* data = (BackupsWindowData*)_additional_data;
	data->temporary_allocator.Free();
}

// ----------------------------------------------------------------------------------------------------------------

void BackupsWindowSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
{
	descriptor.window_name = BACKUPS_WINDOW_NAME;

	BackupsWindowData* window_data = stack_memory->Reserve<BackupsWindowData>();
	window_data->editor_state = editor_state;
	window_data->refresh_timer.DelayStart(REFRESH_DURATION_MS, ECS_TIMER_DURATION_MS);
	window_data->temporary_allocator = ResizableLinearAllocator(ECS_MB, ECS_MB, ECS_MALLOC_ALLOCATOR);
	window_data->paths = {};

	descriptor.retained_mode = BackupsWindowRetainedMode;
	descriptor.draw = BackupsDraw;
	descriptor.destroy_action = BackupsWindowDestroy;
	descriptor.window_data = window_data;
	descriptor.window_data_size = sizeof(*window_data);
}

// ----------------------------------------------------------------------------------------------------------------

struct DeleteBackupActionData {
	EditorState* editor_state;
	Stream<wchar_t> path;
};

void DeleteBackupAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	DeleteBackupActionData* data = (DeleteBackupActionData*)_data;
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectBackupFolder(data->editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStreamSafe(data->path);
	absolute_path[absolute_path.size] = L'\0';

	bool success = RemoveFolder(absolute_path);
	if (!success) {
		EditorSetConsoleError("Could not delete backup.");
	}
}

// ----------------------------------------------------------------------------------------------------------------

struct RestoreBackupActionData {
	EditorState* editor_state;
	Stream<wchar_t> path;
};

void RestoreBackupAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RestoreBackupActionData* data = (RestoreBackupActionData*)_data;
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectBackupFolder(data->editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStreamSafe(data->path);

	bool success = LoadProjectBackup(data->editor_state, absolute_path);
	if (success) {
		EditorSetConsoleInfo("The backup was restored successfully.");
	}
}

// ----------------------------------------------------------------------------------------------------------------

void BackupsDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	BackupsWindowData* data = (BackupsWindowData*)window_data;
	data->paths.allocator = &data->temporary_allocator;
	data->paths.Clear();

	auto recover_last_backup = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		EditorState* editor_state = (EditorState*)_data;
		
		// Walk through the backup folder and record the last date
		ECS_STACK_CAPACITY_STREAM(wchar_t, backup_folder, 512);
		GetProjectBackupFolder(editor_state, backup_folder);

		struct FunctorData {
			CapacityStream<wchar_t>* folder;
			unsigned int base_folder_size;
			Date current_date;
		};

		FunctorData data;
		data.folder = &backup_folder;
		data.base_folder_size = backup_folder.size;
		memset(&data.current_date, 0, sizeof(Date));

		ForEachDirectory(backup_folder, &data, [](Stream<wchar_t> path, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			Stream<wchar_t> filename = PathFilename(path);
			Date date = ConvertStringToDate(filename, ECS_FORMAT_DATE_ALL_FROM_MINUTES);
			if (IsDateLater(data->current_date, date)) {
				data->current_date = date;
				data->folder->size = data->base_folder_size;
				data->folder->Add(ECS_OS_PATH_SEPARATOR);
				data->folder->AddStreamSafe(PathFilename(path));
			}

			return true;
		});

		// No folder has been identified fail
		if (data.base_folder_size == backup_folder.size) {
			EditorSetConsoleError("No backup was found when trying to restore latest backup.");
			return;
		}

		bool success = LoadProjectBackup(editor_state, backup_folder);
		if (success) {
			EditorSetConsoleInfo("Latest backup was restored with success.");
		}
	};

	UIDrawConfig config;

	ECS_STACK_CAPACITY_STREAM(wchar_t, backup_folder, 512);
	GetProjectBackupFolder(data->editor_state, backup_folder);

	// Display all the available backups. Keep the list sorted by their date, such that the user
	// Can quickly find the latest entries
	ForEachDirectory(backup_folder, data, [](Stream<wchar_t> path, void* _data) {
		BackupsWindowData* data = (BackupsWindowData*)_data;
		
		PathEntry path_entry;
		path_entry.path = path.Copy(&data->temporary_allocator);
		path_entry.creation_time = 0;
		OS::GetFileTimes(path, &path_entry.creation_time, nullptr, nullptr);
		data->paths.Add(path_entry);

		return true;
	});

	// Sort the entries, by their creation date
	InsertionSort<false>(data->paths.buffer, data->paths.size);

	UIConfigAbsoluteTransform restore_transform;
	restore_transform.scale = drawer.GetLabelScale("Restore");
	UIConfigAbsoluteTransform delete_transform;
	delete_transform.scale = drawer.GetLabelScale("Delete");
	size_t backup_total_byte_size = 0;

	for (unsigned int index = 0; index < data->paths.size; index++) {
		if (backup_total_byte_size == 0) {
			drawer.Text("Backups at: ");
			drawer.NextRow();
		}

		drawer.Indent(2.0f);
		Stream<wchar_t> filename = PathFilename(data->paths[index].path);
		ECS_STACK_CAPACITY_STREAM(char, ascii_filename, 512);
		ConvertWideCharsToASCII(filename, ascii_filename);
		ascii_filename[ascii_filename.size] = '\0';
		drawer.Text(ascii_filename.buffer);

		// Display the total size of that backup
		size_t folder_size = GetFileByteSize(data->paths[index].path);
		ECS_STACK_CAPACITY_STREAM(char, byte_size_string, 64);
		ConvertByteSizeToString(folder_size, byte_size_string);
		drawer.Text(byte_size_string.buffer);
		backup_total_byte_size += folder_size;

		restore_transform.position = drawer.GetAlignedToRight(restore_transform.scale.x);
		if (restore_transform.position.x - drawer.layout.element_indentation * 2 - delete_transform.scale.x < drawer.current_x) {
			restore_transform.position.x = drawer.current_x + drawer.layout.element_indentation * 2 + delete_transform.scale.x;
		}
		config.AddFlag(restore_transform);

		// Draw the restore button to the right border
		RestoreBackupActionData restore_data;
		restore_data.editor_state = data->editor_state;
		restore_data.path = data->paths[index].path;

		float current_row_y_scale = drawer.current_row_y_scale;

		drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y, config, "Restore", { RestoreBackupAction, &restore_data, sizeof(restore_data) });
		config.flag_count--;

		// Draw the delete button to the left of the restore button
		delete_transform.position = restore_transform.position;
		delete_transform.position.x -= delete_transform.scale.x + drawer.layout.element_indentation;
		DeleteBackupActionData delete_data;
		delete_data.editor_state = data->editor_state;
		delete_data.path = data->paths[index].path;

		drawer.current_row_y_scale = current_row_y_scale;

		config.AddFlag(delete_transform);
		drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y, config, "Delete", { DeleteBackupAction, &delete_data, sizeof(delete_data) });
		config.flag_count--;

		drawer.NextRow();
	}
	
	if (backup_total_byte_size == 0) {
		drawer.Text("There are no backups.");
		drawer.NextRow();
	}
	else {
		ECS_STACK_CAPACITY_STREAM(char, total_backup_size, 128);
		total_backup_size.CopyOther("Total backup byte size: ");
		ConvertByteSizeToString(backup_total_byte_size, total_backup_size);
		drawer.Text(total_backup_size);
		drawer.NextRow();
	}

	UIConfigWindowDependentSize window_size;
	config.AddFlag(window_size);

	UIConfigActiveState active_state;
	active_state.state = backup_total_byte_size > 0;
	config.AddFlag(active_state);
	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE, config, "Restore last backup", { recover_last_backup, data->editor_state, 0 });
	config.flag_count--;
	drawer.NextRow();

	auto manual_backup = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		EditorState* editor_state = (EditorState*)_data;
		bool success = SaveProjectBackup(editor_state);
		if (success) {
			// Delay the backup time by a minute such that there will be no folder conflicts
			EditorSetConsoleInfo("Manual backup saved successfully.");
			AddProjectNeedsBackupTime(editor_state, 60);
		}
		else {
			EditorSetConsoleError("Could not save manual project backup.");
		}
	};

	Date current_date = OS::GetLocalTime();
	backup_folder.Add(ECS_OS_PATH_SEPARATOR);
	ConvertDateToString(current_date, backup_folder, ECS_FORMAT_DATE_ALL_FROM_MINUTES);
	active_state.state = !ExistsFileOrFolder(backup_folder);
	config.AddFlag(active_state);
	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE, config, "Manual backup", { manual_backup, data->editor_state, 0 });
}

// ----------------------------------------------------------------------------------------------------------------

unsigned int CreateBackupsWindowOnly(EditorState* editor_state) {
	UIWindowDescriptor descriptor;

	BackupsWindowSetDescriptor(descriptor, editor_state, nullptr);
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	return editor_state->ui_system->Create_Window(descriptor);
}

// ----------------------------------------------------------------------------------------------------------------

void CreateBackupsWindow(EditorState* editor_state) {
	CreateDockspaceFromWindow(BACKUPS_WINDOW_NAME, editor_state, CreateBackupsWindowOnly);
}

// ----------------------------------------------------------------------------------------------------------------

void CreateBackupsWindowAction(ActionData* action_data) {
	CreateBackupsWindow((EditorState*)action_data->data);
}

// ---------------------------------------------------------------------------------------------------------------

struct RestoreBackupWindowDrawData {
	EditorState* editor_state;
	Stream<wchar_t> folder;
	bool selected[PROJECT_BACKUP_COUNT];
};

void RestoreBackupWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	RestoreBackupWindowDrawData* data = (RestoreBackupWindowDrawData*)window_data;
	if (initialize) {
		memset(data->selected, 1, sizeof(bool) * PROJECT_BACKUP_COUNT);
	}

	drawer.Text("Project backup from ");
	drawer.TextWide(data->folder);
	drawer.NextRow();

	drawer.Text("Select which files to restore from backup");
	drawer.NextRow();

	for (size_t index = 0; index < PROJECT_BACKUP_COUNT; index++) {
		drawer.CheckBox(PROJECT_BACKUP_FILE_NAMES[index], data->selected + index);
		drawer.NextRow();
	}

	auto confirm_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		RestoreBackupWindowDrawData* data = (RestoreBackupWindowDrawData*)_data;
		ECS_STACK_CAPACITY_STREAM(ProjectBackupFiles, backup_files, PROJECT_BACKUP_COUNT);

		for (size_t index = 0; index < PROJECT_BACKUP_COUNT; index++) {
			if (data->selected[index]) {
				backup_files.Add((ProjectBackupFiles)index);
			}
		}

		// This will report any errors encountered
		LoadProjectBackup(data->editor_state, data->folder, backup_files);
		DestroyCurrentActionWindow(action_data);
	};

	drawer.Button("Confirm", { confirm_action, data, 0, ECS_UI_DRAW_SYSTEM });

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer.GetLabelScale("Cancel");
	transform.position = drawer.GetAlignedToRight(transform.scale.x);

	UIDrawConfig config;
	config.AddFlag(transform);

	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Cancel", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
}

void SetRestoreBackupWindowDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, Stream<wchar_t> folder) {
	RestoreBackupWindowDrawData data;
	data.folder = folder;
	data.editor_state = editor_state;

	descriptor.window_name = "Restore Backup";
	descriptor.window_data = &data;
	descriptor.window_data_size = sizeof(data);

	descriptor.destroy_action = ReleaseLockedWindow;

	descriptor.initial_size_x = 0.7f;
	descriptor.initial_size_y = 1.0f;
}

// ---------------------------------------------------------------------------------------------------------------

void CreateRestoreBackupWindow(EditorState* editor_state, Stream<wchar_t> folder)
{
	UIWindowDescriptor descriptor;
	SetRestoreBackupWindowDescriptor(descriptor, editor_state, folder);
	editor_state->ui_system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL);
}

// ----------------------------------------------------------------------------------------------------------------

void CreateRestoreBackupWindowAction(ECSEngine::Tools::ActionData* action_data)
{
	CreateRestoreBackupWindowActionData* data = (CreateRestoreBackupWindowActionData*)action_data->data;
	CreateRestoreBackupWindow(data->editor_state, data->folder);
}

// ----------------------------------------------------------------------------------------------------------------
