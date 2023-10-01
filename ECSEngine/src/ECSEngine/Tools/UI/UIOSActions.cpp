#include "ecspch.h"
#include "../../Utilities/OSFunctions.h"
#include "UIOSActions.h"
#include "UIDrawerWindows.h"
#include "../../Input/Mouse.h"
#include "../../Input/Keyboard.h"
#include "../../Utilities/Path.h"

#include "../../Utilities/StackScope.h"

constexpr const char* RENAME_FOLDER_WIZARD_NAME = "Rename Folder";
constexpr const char* RENAME_FOLDER_WIZARD_INPUT_NAME = "New folder name";

constexpr const char* RENAME_FILE_WIZARD_NAME = "Rename File";
constexpr const char* RENAME_FILE_WIZARD_INPUT_NAME = "New file name";

constexpr const char* CHANGE_FILE_EXTENSION_WIZARD_NAME = "Change Extension";
constexpr const char* CHANGE_FILE_EXTENSION_WIZARD_INPUT_NAME = "New extension";

namespace ECSEngine {

	namespace Tools {

		void StreamAction(ActionData* action_data, void (*action)(Stream<wchar_t>, UISystem*)) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			action(*data, system);
		}

		void ConvertCActionToStreamAction(ActionData* action_data, Action stream_action) {
			UI_UNPACK_ACTION_DATA;

			const wchar_t* data = (const wchar_t*)_data;
			Stream<wchar_t> path = data;
			action_data->data = &path;
			stream_action(action_data);
		}

		// ----------------------------------------------------------------------------------------------------

		void LaunchFileExplorerAction(ActionData* action_data)
		{
			ConvertCActionToStreamAction(action_data, LaunchFileExplorerStreamAction);
		}

		void LaunchFileExplorerStreamAction(ActionData* action_data) {
			StreamAction(action_data, OS::LaunchFileExplorerWithError);
		}

		// ----------------------------------------------------------------------------------------------------

		template<typename PointerType>
		void GetFileTimesAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			GetFileTimesActionData<PointerType>* data = (GetFileTimesActionData<PointerType>*)_data;
			OS::GetFileTimesWithError<PointerType>(data->path, system, data->creation_time, data->access_time, data->last_write_time);
		}

		template ECSENGINE_API void GetFileTimesAction<wchar_t>(ActionData*);
		template ECSENGINE_API void GetFileTimesAction<char>(ActionData*);

		template<typename PointerType>
		void GetRelativeFileTimesAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			GetFileTimesActionData<PointerType>* data = (GetFileTimesActionData<PointerType>*)_data;
			OS::GetRelativeFileTimesWithError<PointerType>(data->path, system, data->creation_time, data->access_time, data->last_write_time);
		}

		template ECSENGINE_API void GetRelativeFileTimesAction<wchar_t>(ActionData*);
		template ECSENGINE_API void GetRelativeFileTimesAction<char>(ActionData*);
		template ECSENGINE_API void GetRelativeFileTimesAction<size_t>(ActionData*);

		// ----------------------------------------------------------------------------------------------------

		void CreateFolderAction(ActionData* action_data)
		{
			ConvertCActionToStreamAction(action_data, CreateFolderStreamAction);
		}

		void CreateFolderStreamAction(ActionData* action_data)
		{
			StreamAction(action_data, OS::CreateFolderWithError);
		}

		// ----------------------------------------------------------------------------------------------------

		void DeleteFolderAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, DeleteFolderStreamAction);
		}

		void DeleteFolderStreamAction(ActionData* action_data) {
			StreamAction(action_data, OS::DeleteFolderWithError);
		}

		// ----------------------------------------------------------------------------------------------------

		void DeleteFileAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, DeleteFileStreamAction);
		}

		void DeleteFileStreamAction(ActionData* action_data) {
			StreamAction(action_data, OS::DeleteFileWithError);
		}

		// ----------------------------------------------------------------------------------------------------

		void RenameFolderAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RenameFolderActionData* data = (RenameFolderActionData*)_data;
			OS::RenameFolderOrFileWithError(data->path, data->new_name, system);
		}

		void RenameFolderPointerAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RenameFolderPointerActionData* data = (RenameFolderPointerActionData*)_data;
			OS::RenameFolderOrFileWithError(*data->path, *data->new_name, system);
		}

		void RenameFolderWizardCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;
			const wchar_t* path = (const wchar_t*)_data;

			ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 256);
			function::ConvertASCIIToWide(wide_name, *new_name);

			RenameFolderActionData rename_data;
			rename_data.new_name = wide_name;
			rename_data.path = path;

			action_data->data = &rename_data;
			RenameFolderAction(action_data);
		}

		void RenameFolderWizardAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, RenameFolderWizardStreamAction);
		}

		void RenameFolderWizardStreamAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			ECS_STACK_CAPACITY_STREAM(wchar_t, temp_path, 256);
			TextInputWizardData wizard_data;
			wizard_data.input_name = RENAME_FOLDER_WIZARD_INPUT_NAME;
			wizard_data.window_name = RENAME_FOLDER_WIZARD_NAME;
			wizard_data.callback = RenameFolderWizardCallback;

			temp_path.CopyOther(*data);
			temp_path[data->size] = L'\0';
			wizard_data.callback_data = temp_path.buffer;
			wizard_data.callback_data_size = sizeof(wchar_t) * (data->size + 1);
			CreateTextInputWizard(&wizard_data, system);
		}

		// ----------------------------------------------------------------------------------------------------

		void RenameFileAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RenameFileActionData* data = (RenameFileActionData*)_data;
			OS::RenameFolderOrFileWithError(data->path, data->new_name, system);
		}

		void RenameFilePointerAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RenameFilePointerActionData* data = (RenameFilePointerActionData*)_data;
			OS::RenameFolderOrFileWithError(*data->path, *data->new_name, system);
		}

		void RenameFileWizardCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;
			const wchar_t* path = (const wchar_t*)_data;

			ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 256);
			function::ConvertASCIIToWide(wide_name, *new_name);

			RenameFileActionData rename_data;
			rename_data.new_name = wide_name;
			rename_data.path = path;

			action_data->data = &rename_data;
			RenameFileAction(action_data);
		}

		void RenameFileWizardAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, RenameFileWizardStreamAction);
		}

		void RenameFileWizardStreamAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			ECS_STACK_CAPACITY_STREAM(wchar_t, temp_path, 256);
			TextInputWizardData wizard_data;
			wizard_data.input_name = RENAME_FILE_WIZARD_INPUT_NAME;
			wizard_data.window_name = RENAME_FILE_WIZARD_NAME;
			wizard_data.callback = RenameFileWizardCallback;

			temp_path.CopyOther(*data);
			temp_path[data->size] = L'\0';
			wizard_data.callback_data = temp_path.buffer;
			wizard_data.callback_data_size = sizeof(wchar_t) * (data->size + 1);
			CreateTextInputWizard(&wizard_data, system);
		}

		// ----------------------------------------------------------------------------------------------------

		void ChangeFileExtensionAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ChangeFileExtensionActionData* data = (ChangeFileExtensionActionData*)_data;
			OS::ChangeFileExtensionWithError(data->path, data->new_extension, system);
		}

		void ChangeFileExtensionPointerAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ChangeFileExtensionPointerActionData* data = (ChangeFileExtensionPointerActionData*)_data;
			OS::ChangeFileExtensionWithError(*data->path, *data->new_extension, system);
		}

		void ChangeFileExtensionWizardCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			CapacityStream<char>* new_extension = (CapacityStream<char>*)_additional_data;
			const wchar_t* path = (const wchar_t*)_data;

			ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 256);
			function::ConvertASCIIToWide(wide_name, *new_extension);

			ChangeFileExtensionActionData change_data;
			change_data.new_extension = wide_name;
			change_data.path = path;

			action_data->data = &change_data;
			ChangeFileExtensionAction(action_data);
		}

		void ChangeFileExtensionWizardAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, ChangeFileExtensionWizardStreamAction);
		}

		void ChangeFileExtensionWizardStreamAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			ECS_STACK_CAPACITY_STREAM(wchar_t, temp_path, 256);
			TextInputWizardData wizard_data;
			wizard_data.input_name = CHANGE_FILE_EXTENSION_WIZARD_INPUT_NAME;
			wizard_data.window_name = CHANGE_FILE_EXTENSION_WIZARD_NAME;
			wizard_data.callback = ChangeFileExtensionWizardCallback;

			temp_path.CopyOther(*data);
			temp_path[data->size] = L'\0';
			wizard_data.callback_data = temp_path.buffer;
			wizard_data.callback_data_size = sizeof(wchar_t) * (data->size + 1);
			CreateTextInputWizard(&wizard_data, system);
		}

		// ----------------------------------------------------------------------------------------------------

		void ExistsFileOrFolderAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, ExistsFileOrFolderStreamAction);
		}

		void ExistsFileOrFolderStreamAction(ActionData* action_data) {
			StreamAction(action_data, OS::ExistsFileOrFolderWithError);
		}

		// ----------------------------------------------------------------------------------------------------

		void DeleteFolderContentsAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, DeleteFolderContentsStreamAction);
		}

		void DeleteFolderContentsStreamAction(ActionData* action_data) {
			StreamAction(action_data, OS::DeleteFolderContentsWithError);
		}

		// ----------------------------------------------------------------------------------------------------

		void CopyPathToClipboardAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			const wchar_t* data = (const wchar_t*)_data;

			ECS_STACK_CAPACITY_STREAM(char, ascii_string, 512);
			function::ConvertWideCharsToASCII(Stream<wchar_t>(data), ascii_string);
			ascii_string[ascii_string.size] = '\0';
			system->m_application->WriteTextToClipboard(ascii_string.buffer);
		}

		void CopyPathToClipboardStreamAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;

			ECS_STACK_CAPACITY_STREAM(char, temp_characters, 512);
			function::ConvertWideCharsToASCII(*data, temp_characters);
			temp_characters[temp_characters.size] = '\0';

			system->m_application->WriteTextToClipboard(temp_characters.buffer);
		}

		// ----------------------------------------------------------------------------------------------------

		void OpenFileWithDefaultApplicationAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, OpenFileWithDefaultApplicationStreamAction);
		}

		void OpenFileWithDefaultApplicationStreamAction(ActionData* action_data) {
			StreamAction(action_data, OS::OpenFileWithDefaultApplicationWithError);
		}

		// -----------------------------------------------------------------------------------------------------

		void FileExplorerGetFileAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			OSFileExplorerGetFileActionData* data = (OSFileExplorerGetFileActionData*)_data;
			bool status = FileExplorerGetFile(&data->get_file_data);
			if (!status) {
				if (data->get_file_data.error_message.buffer != nullptr) {
					CreateErrorMessageWindow(system, data->get_file_data.error_message);
				}
				else {
					CreateErrorMessageWindow(system, "Could not get file from File Explorer. Make sure it is the correct format.");
				}
			}
			else {
				if (data->input != nullptr) {
					char temp_chars[256];
					size_t written_chars;
					if (data->get_file_data.path.size > 0) {
						function::ConvertWideCharsToASCII(data->get_file_data.path.buffer, temp_chars, data->get_file_data.path.size, 256, 256, written_chars);
						data->input->DeleteAllCharacters();
						data->input->InsertCharacters(temp_chars, written_chars, 0, system);
						if (data->update_stream) {
							data->update_stream->CopyOther(data->get_file_data.path);
						}
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------

		void FileExplorerGetDirectoryAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			OSFileExplorerGetDirectoryActionData* data = (OSFileExplorerGetDirectoryActionData*)_data;
			bool status = FileExplorerGetDirectory(&data->get_directory_data);
			if (!status) {
				if (data->get_directory_data.error_message.buffer != nullptr) {
					CreateErrorMessageWindow(system, data->get_directory_data.error_message);
				}
				else {
					CreateErrorMessageWindow(system, "Could not get directory from File Explorer.");
				}
			}
			else {
				if (data->input != nullptr) {
					char temp_chars[256];
					size_t written_chars;
					if (data->get_directory_data.path.size > 0) {
						function::ConvertWideCharsToASCII(data->get_directory_data.path.buffer, temp_chars, data->get_directory_data.path.size, 256, 256, written_chars);
						data->input->DeleteAllCharacters();
						data->input->InsertCharacters(temp_chars, written_chars, 0, system);
						if (data->update_stream != nullptr) {
							data->update_stream->CopyOther(data->get_directory_data.path);
						}
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------

	}

}