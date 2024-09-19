#include "editorpch.h"
#include "Backups.h"
#include "HelperWindows.h"
#include "../Editor/EditorState.h"
#include "../Project/ProjectBackup.h"
#include "../Project/ProjectFolders.h"

using namespace ECSEngine;

#define WINDOW_SIZE float2(0.4f, 0.7f)

// ----------------------------------------------------------------------------------------------------------------

void BackupsWindowSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
{
	descriptor.window_name = BACKUPS_WINDOW_NAME;

	descriptor.draw = BackupsDraw;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
}

// ----------------------------------------------------------------------------------------------------------------

struct DeleteBackupActionData {
	EditorState* editor_state;
	const wchar_t* path;
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
	const wchar_t* path;
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

#define LINEAR_ALLOCATOR_NAME "Allocator"
#define LINEAR_ALLOCATOR_SIZE ECS_KB * 16

void BackupsDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;

	LinearAllocator* allocator = nullptr;
	if (initialize) {
		allocator = (LinearAllocator*)drawer.GetMainAllocatorBufferAndStoreAsResource(LINEAR_ALLOCATOR_NAME, sizeof(LinearAllocator));
		*allocator = LinearAllocator(drawer.GetMainAllocatorBuffer(LINEAR_ALLOCATOR_SIZE), LINEAR_ALLOCATOR_SIZE);
	}
	else {
		allocator = (LinearAllocator*)drawer.GetResource(LINEAR_ALLOCATOR_NAME);
		allocator->Clear();
	}

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
	GetProjectBackupFolder(editor_state, backup_folder);

	struct FunctorData {
		EditorState* editor_state;
		UIDrawer* drawer;
		UIDrawConfig* config;
		UIConfigAbsoluteTransform* restore_transform;
		UIConfigAbsoluteTransform* delete_transform;
		LinearAllocator* allocator;
		size_t backup_total_byte_size = 0;
	};

	UIConfigAbsoluteTransform restore_transform;
	restore_transform.scale = drawer.GetLabelScale("Restore");
	UIConfigAbsoluteTransform delete_transform;
	delete_transform.scale = drawer.GetLabelScale("Delete");

	FunctorData functor_data;
	functor_data.editor_state = editor_state;
	functor_data.config = &config;
	functor_data.drawer = &drawer;
	functor_data.restore_transform = &restore_transform;
	functor_data.delete_transform = &delete_transform;
	functor_data.allocator = allocator;

	// Display all the available backups
	ForEachDirectory(backup_folder, &functor_data, [](Stream<wchar_t> path, void* _data) {
		FunctorData* data = (FunctorData*)_data;
		if (data->backup_total_byte_size == 0) {
			data->drawer->Text("Backups at: ");
			data->drawer->NextRow();
		}
		
		data->drawer->Indent(2.0f);
		Stream<wchar_t> filename = PathFilename(path);
		ECS_STACK_CAPACITY_STREAM(char, ascii_filename, 512);
		ConvertWideCharsToASCII(filename, ascii_filename);
		ascii_filename[ascii_filename.size] = '\0';
		data->drawer->Text(ascii_filename.buffer);

		// Display the total size of that backup
		size_t folder_size = GetFileByteSize(path);
		ECS_STACK_CAPACITY_STREAM(char, byte_size_string, 64);
		ConvertByteSizeToString(folder_size, byte_size_string);
		data->drawer->Text(byte_size_string.buffer);
		data->backup_total_byte_size += folder_size;
		
		data->restore_transform->position = data->drawer->GetAlignedToRight(data->restore_transform->scale.x);
		if (data->restore_transform->position.x - data->drawer->layout.element_indentation * 2 - data->delete_transform->scale.x < data->drawer->current_x) {
			data->restore_transform->position.x = data->drawer->current_x + data->drawer->layout.element_indentation * 2 + data->delete_transform->scale.x;
		}
		data->config->AddFlag(*data->restore_transform);

		// Draw the restore button to the right border
		RestoreBackupActionData restore_data;
		restore_data.editor_state = data->editor_state;
		wchar_t* restore_path = (wchar_t*)data->allocator->Allocate(sizeof(wchar_t) * (filename.size + 1));
		filename.CopyTo(restore_path);
		restore_path[filename.size] = L'\0';

		float current_row_y_scale = data->drawer->current_row_y_scale;

		restore_data.path = restore_path;
		data->drawer->Button(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y, *data->config, "Restore", { RestoreBackupAction, &restore_data, sizeof(restore_data) });
		data->config->flag_count--;
		
		// Draw the delete button to the left of the restore button
		data->delete_transform->position = data->restore_transform->position;
		data->delete_transform->position.x -= data->delete_transform->scale.x + data->drawer->layout.element_indentation;
		DeleteBackupActionData delete_data;
		delete_data.editor_state = data->editor_state;
		delete_data.path = restore_path;

		data->drawer->current_row_y_scale = current_row_y_scale;

		data->config->AddFlag(*data->delete_transform);
		data->drawer->Button(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y, *data->config, "Delete", { DeleteBackupAction, &delete_data, sizeof(delete_data) });
		data->config->flag_count--;

		data->drawer->NextRow();

		return true;
	});
	
	if (functor_data.backup_total_byte_size == 0) {
		drawer.Text("There are no backups.");
		drawer.NextRow();
	}
	else {
		ECS_STACK_CAPACITY_STREAM(char, total_backup_size, 128);
		total_backup_size.CopyOther("Total backup byte size: ");
		ConvertByteSizeToString(functor_data.backup_total_byte_size, total_backup_size);
		drawer.Text(total_backup_size.buffer);
		drawer.NextRow();
	}

	UIConfigWindowDependentSize window_size;
	config.AddFlag(window_size);

	UIConfigActiveState active_state;
	active_state.state = functor_data.backup_total_byte_size > 0;
	config.AddFlag(active_state);
	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE, config, "Restore last backup", { recover_last_backup, editor_state, 0 });
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
	drawer.Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE, config, "Manual backup", { manual_backup, editor_state, 0 });
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
