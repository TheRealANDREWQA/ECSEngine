#pragma once
#include "../../Containers/Stream.h"

namespace ECSEngine {

	namespace Tools {

		struct ActionData;
		
		// ---------------------------------------------------------------------------------------------------

		// Data must be a const wchar_t*
		ECSENGINE_API void LaunchFileExplorerAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void LaunchFileExplorerStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------
		
		template<typename PointerType>
		struct GetFileTimesActionData {
			Stream<wchar_t> path;
			PointerType* creation_time = nullptr;
			PointerType* access_time = nullptr;
			PointerType* last_write_time = nullptr;
		};

		template<typename PointerType>
		ECSENGINE_API void GetFileTimesAction(ActionData* action_data);
		
		// ---------------------------------------------------------------------------------------------------

		// Data must be a const wchar_t*
		ECSENGINE_API void CreateFolderAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void CreateFolderStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		// Data must be a const wchar_t*
		ECSENGINE_API void DeleteFolderAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void DeleteFolderStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		// Data must be a const wchar_t*
		ECSENGINE_API void DeleteFileAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void DeleteFileStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		struct RenameFolderActionData {
			Stream<wchar_t> path;
			Stream<wchar_t> new_name;
		};

		ECSENGINE_API void RenameFolderAction(ActionData* action_data);

		struct RenameFolderPointerActionData {
			Stream<wchar_t>* path;
			Stream<wchar_t>* new_name;
		};

		ECSENGINE_API void RenameFolderPointerAction(ActionData* action_data);

		// Data must be a const wchar_t*
		ECSENGINE_API void RenameFolderWizardAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void RenameFolderWizardStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		struct RenameFileActionData {
			Stream<wchar_t> path;
			Stream<wchar_t> new_name;
		};

		ECSENGINE_API void RenameFileAction(ActionData* action_data);

		struct RenameFilePointerActionData {
			Stream<wchar_t>* path;
			Stream<wchar_t>* new_name;
		};

		ECSENGINE_API void RenameFilePointerAction(ActionData* action_data);

		// Data must be a const wchar_t*
		ECSENGINE_API void RenameFileWizardAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void RenameFileWizardStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		struct ChangeFileExtensionActionData {
			Stream<wchar_t> path;
			Stream<wchar_t> new_extension;
		};

		ECSENGINE_API void ChangeFileExtensionAction(ActionData* action_data);

		struct ChangeFileExtensionPointerActionData {
			Stream<wchar_t>* path;
			Stream<wchar_t>* new_extension;
		};

		ECSENGINE_API void ChangeFileExtensionPointerAction(ActionData* action_data);

		// Data must be a const wchar_t*
		ECSENGINE_API void ChangeFileExtensionWizardAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void ChangeFileExtensionWizardStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		// Data must be a const wchar_t*
		ECSENGINE_API void ExistsFileOrFolderAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void ExistsFileOrFolderStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		// Data must be a const wchar_t*
		ECSENGINE_API void DeleteFolderContentsAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void DeleteFolderContentsStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------
		
		// Data must be a const wchar_t*
		ECSENGINE_API void CopyPathToClipboardAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void CopyPathToClipboardStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		// Data must be a const wchar_t*
		ECSENGINE_API void OpenFileWithDefaultApplicationAction(ActionData* action_data);

		// Data must be a Stream<wchar_t>*
		ECSENGINE_API void OpenFileWithDefaultApplicationStreamAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

		struct UIDrawerTextInput;

		// The path must be initialized
		struct OSFileExplorerGetFileData {
			CapacityStream<wchar_t> path;
			CapacityStream<char> error_message = { nullptr, 0, 0 };
			const wchar_t* initial_directory = nullptr;
			Stream<const wchar_t*> extensions = { nullptr, 0 };
			HWND hWnd = NULL;
		};

		// The path must be initialized
		struct OSFileExplorerGetDirectoryData {
			CapacityStream<wchar_t> path;
			CapacityStream<char> error_message = { nullptr, 0, 0 };
		};

		struct OSFileExplorerGetFileActionData {
			OSFileExplorerGetFileData get_file_data;
			UIDrawerTextInput* input = nullptr;
			CapacityStream<wchar_t>* update_stream = nullptr;
		};

		struct OSFileExplorerGetDirectoryActionData {
			OSFileExplorerGetDirectoryData get_directory_data;
			UIDrawerTextInput* input = nullptr;
			CapacityStream<wchar_t>* update_stream = nullptr;
		};

		ECSENGINE_API bool FileExplorerGetFile(OSFileExplorerGetFileData* data);

		ECSENGINE_API bool FileExplorerGetDirectory(OSFileExplorerGetDirectoryData* data);

		ECSENGINE_API void FileExplorerGetFileAction(ActionData* action_data);

		ECSENGINE_API void FileExplorerGetDirectoryAction(ActionData* action_data);

		// ---------------------------------------------------------------------------------------------------

	}

}