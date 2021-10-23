#include "ecspch.h"
#include "../Utilities/OSFunctions.h"
#include "UIOSActions.h"
#include "UIDrawerWindows.h"
#include "../Utilities/Mouse.h"
#include "../Utilities/Keyboard.h"

constexpr const char* RENAME_FOLDER_WIZARD_NAME = "Rename Folder";
constexpr const char* RENAME_FOLDER_WIZARD_INPUT_NAME = "New folder name";

constexpr const char* RENAME_FILE_WIZARD_NAME = "Rename File";
constexpr const char* RENAME_FILE_WIZARD_INPUT_NAME = "New file name";

constexpr const char* CHANGE_FILE_EXTENSION_WIZARD_NAME = "Change Extension";
constexpr const char* CHANGE_FILE_EXTENSION_WIZARD_INPUT_NAME = "New extension";

namespace ECSEngine {
	
	ECS_CONTAINERS;

	namespace Tools {

		void StreamAction(ActionData* action_data, void (*action)(Stream<wchar_t>, UISystem*)) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			action(*data, system);
		}

		void ConvertCActionToStreamAction(ActionData* action_data, Action stream_action) {
			UI_UNPACK_ACTION_DATA;

			const wchar_t* data = (const wchar_t*)_data;
			Stream<wchar_t> path = ToStream(data);
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
			OS::RenameFolderWithError(data->path, data->new_name, system);
		}

		void RenameFolderPointerAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RenameFolderPointerActionData* data = (RenameFolderPointerActionData*)_data;
			OS::RenameFolderWithError(*data->path, *data->new_name, system);
		}

		void RenameFolderWizardCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;
			const wchar_t* path = (const wchar_t*)_data;

			ECS_TEMP_STRING(wide_name, 256);
			function::ConvertASCIIToWide(wide_name, *new_name);
			wide_name.size = new_name->size;

			RenameFolderActionData rename_data;
			rename_data.new_name = wide_name;
			rename_data.path = ToStream(path);

			action_data->data = &rename_data;
			RenameFolderAction(action_data);
		}

		void RenameFolderWizardAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, RenameFolderWizardStreamAction);
		}

		void RenameFolderWizardStreamAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			ECS_TEMP_STRING(temp_path, 256);
			TextInputWizardData wizard_data;
			wizard_data.input_name = RENAME_FOLDER_WIZARD_INPUT_NAME;
			wizard_data.window_name = RENAME_FOLDER_WIZARD_NAME;
			wizard_data.callback = RenameFolderWizardCallback;

			temp_path.Copy(*data);
			temp_path[data->size] = L'\0';
			wizard_data.callback_data = temp_path.buffer;
			wizard_data.callback_data_size = sizeof(wchar_t) * (data->size + 1);
			CreateTextInputWizard(&wizard_data, system);
		}

		// ----------------------------------------------------------------------------------------------------

		void RenameFileAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RenameFileActionData* data = (RenameFileActionData*)_data;
			OS::RenameFileWithError(data->path, data->new_name, system);
		}

		void RenameFilePointerAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RenameFilePointerActionData* data = (RenameFilePointerActionData*)_data;
			OS::RenameFileWithError(*data->path, *data->new_name, system);
		}

		void RenameFileWizardCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			CapacityStream<char>* new_name = (CapacityStream<char>*)_additional_data;
			const wchar_t* path = (const wchar_t*)_data;

			ECS_TEMP_STRING(wide_name, 256);
			function::ConvertASCIIToWide(wide_name, *new_name);
			wide_name.size = new_name->size;

			RenameFileActionData rename_data;
			rename_data.new_name = wide_name;
			rename_data.path = ToStream(path);

			action_data->data = &rename_data;
			RenameFileAction(action_data);
		}

		void RenameFileWizardAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, RenameFileWizardStreamAction);
		}

		void RenameFileWizardStreamAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			ECS_TEMP_STRING(temp_path, 256);
			TextInputWizardData wizard_data;
			wizard_data.input_name = RENAME_FILE_WIZARD_INPUT_NAME;
			wizard_data.window_name = RENAME_FILE_WIZARD_NAME;
			wizard_data.callback = RenameFileWizardCallback;

			temp_path.Copy(*data);
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

			ECS_TEMP_STRING(wide_name, 256);
			function::ConvertASCIIToWide(wide_name, *new_extension);
			wide_name.size = new_extension->size;

			ChangeFileExtensionActionData change_data;
			change_data.new_extension = wide_name;
			change_data.path = ToStream(path);

			action_data->data = &change_data;
			ChangeFileExtensionAction(action_data);
		}

		void ChangeFileExtensionWizardAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, ChangeFileExtensionWizardStreamAction);
		}

		void ChangeFileExtensionWizardStreamAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;
			ECS_TEMP_STRING(temp_path, 256);
			TextInputWizardData wizard_data;
			wizard_data.input_name = CHANGE_FILE_EXTENSION_WIZARD_INPUT_NAME;
			wizard_data.window_name = CHANGE_FILE_EXTENSION_WIZARD_NAME;
			wizard_data.callback = ChangeFileExtensionWizardCallback;

			temp_path.Copy(*data);
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

			size_t path_size = wcslen(data);
			ECS_ASSERT(path_size < 512);
			ECS_TEMP_ASCII_STRING(ascii_string, 512);
			function::ConvertWideCharsToASCII(data, ascii_string.buffer, path_size, 512);
			ascii_string[path_size] = '\0';
			system->m_application->WriteTextToClipboard(ascii_string.buffer);
		}

		void CopyPathToClipboardStreamAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			Stream<wchar_t>* data = (Stream<wchar_t>*)_data;

			char temp_characters[512];
			ECS_ASSERT(data->size < 512);
			function::ConvertWideCharsToASCII(*data, CapacityStream<char>(temp_characters, 0, 512));
			temp_characters[data->size] = '\0';

			system->m_application->WriteTextToClipboard(temp_characters);
		}

		// ----------------------------------------------------------------------------------------------------

		void OpenFileWithDefaultApplicationAction(ActionData* action_data) {
			ConvertCActionToStreamAction(action_data, OpenFileWithDefaultApplicationStreamAction);
		}

		void OpenFileWithDefaultApplicationStreamAction(ActionData* action_data) {
			StreamAction(action_data, OS::OpenFileWithDefaultApplicationWithError);
		}

		// ----------------------------------------------------------------------------------------------------
	
	}

}