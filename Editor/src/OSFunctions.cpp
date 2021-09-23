#include "OSFunctions.h"

using namespace ECSEngine;
ECS_TOOLS;
ECS_CONTAINERS;

void SetBasicErrorMessage(const char* error, CapacityStream<char>& message) {
	if (message.buffer != nullptr) {
		Stream<char> stream = ToStream(error);
		message.AddStreamSafe(stream);
		message[message.size] = '\0';
	}
}

bool FileExplorerGetFile(
	OSFileExplorerGetFileData* data
) {
	IFileOpenDialog* dialog = nullptr;
	HRESULT result = CoCreateInstance(
		CLSID_FileOpenDialog,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&dialog)
	);

	if (SUCCEEDED(result)) {
		DWORD dialog_options;
		result = dialog->GetOptions(&dialog_options);

		if (SUCCEEDED(result)) {
			result = dialog->SetOptions(dialog_options | FOS_NOCHANGEDIR);

			if (FAILED(result)) {
				SetBasicErrorMessage("Setting dialog options failed!", data->error_message);
				data->path.size = 0;
				return false;
			}
		}
		else {
			SetBasicErrorMessage("Getting dialog options failed!", data->error_message);
			data->path.size = 0;
			return false;
		}

		if (data->initial_directory != nullptr) {
			IShellItem* directory;
			result = CoCreateInstance(
				CLSID_ShellItem,
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&directory)
			);

			if (SUCCEEDED(result)) {
				result = dialog->SetFolder(directory);
				if (!SUCCEEDED(result)) {
					SetBasicErrorMessage("Setting folder failed", data->error_message);
					data->path.size = 0;
					return false;
				}
			}
			else {
				SetBasicErrorMessage("Creating shell directory failed!", data->error_message);
				data->path.size = 0;
				return false;
			}
		}

		result = dialog->Show(nullptr);

		if (SUCCEEDED(result)) {
			IShellItem* item;
			result = dialog->GetResult(&item);

			if (SUCCEEDED(result)) {
				wchar_t* temp_path;
				result = item->GetDisplayName(SIGDN_FILESYSPATH, &temp_path);
				data->path.Copy(temp_path, wcslen(temp_path));
				data->path[data->path.size] = L'\0';
				CoTaskMemFree(temp_path);
				item->Release();
				dialog->Release();

				Stream<wchar_t> _filters[128];
				Stream<Stream<wchar_t>> filters(_filters, data->filter_count);
				if (data->filter != nullptr) {
					wchar_t* current_ptr = (wchar_t*)data->filter;
					filters[0].buffer = current_ptr;
					for (size_t index = 0; index < data->filter_count - 1; index++) {
						filters[index].size = wcslen(current_ptr);
						current_ptr += filters[index].size + 1;
						filters[index + 1].buffer = current_ptr;
					}
					filters[filters.size - 1].size = wcslen(filters[filters.size - 1].buffer);
					Stream<wchar_t> extension = function::PathExtension(data->path);
					if (function::FindString(extension, filters) == -1) {
						SetBasicErrorMessage("The selected file does not conform to the accepted file formats", data->error_message);
						data->path.size = 0;
						return false;
					}
				}

				return true;
			}
			else {
				SetBasicErrorMessage("Retrieving dialog result failed!", data->error_message);
				data->path.size = 0;
				return false;
			}
		}
		/*else {
			SetBasicErrorMessage("Showing dialog failed!", data->error_message);
			return false;
		}*/
	}
	else {
		SetBasicErrorMessage("Creating dialog instance failed!", data->error_message);
		data->path.size = 0;
		return false;
	}
}

bool FileExplorerGetDirectory(OSFileExplorerGetDirectoryData* data) {
	IFileDialog* dialog = nullptr;
	HRESULT result = CoCreateInstance(
		CLSID_FileOpenDialog,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&dialog)
	);
	if (SUCCEEDED(result)) {
		DWORD file_options;
		result = dialog->GetOptions(&file_options);

		if (SUCCEEDED(result)) {
			result = dialog->SetOptions(file_options | FOS_PICKFOLDERS);

			if (SUCCEEDED(result)) {
				result = dialog->Show(nullptr);

				if (SUCCEEDED(result)) {
					IShellItem* item;
					result = dialog->GetResult(&item);

					if (SUCCEEDED(result)) {
						wchar_t* temp_path;
						result = item->GetDisplayName(SIGDN_FILESYSPATH, &temp_path);
						data->path.Copy(temp_path, wcslen(temp_path));
						data->path[data->path.size] = L'\0';
						CoTaskMemFree(temp_path);
						item->Release();
						dialog->Release();
						return true;
					}
					else {
						SetBasicErrorMessage("Getting dialog result failed!", data->error_message);
						data->path.size = 0;
						return false;
					}
				}
				/*else {
					SetBasicErrorMessage("Showing dialog failed!", data->error_message);
					return false;
				}*/
			}
			else {
				SetBasicErrorMessage("Setting dialog options failed!", data->error_message);
				data->path.size = 0;
				return false;
			}
		}
		else {
			SetBasicErrorMessage("Getting dialog options failed!", data->error_message);
			data->path.size = 0;
			return false;
		}
	}
	else {
		SetBasicErrorMessage("Creating dialog instance failed!", data->error_message);
		data->path.size = 0;
		return false;
	}
}

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
					data->update_stream->Copy(data->get_file_data.path);
				}
			}
		}
	}
}

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
					data->update_stream->Copy(data->get_directory_data.path);
				}
			}
		}
	}
}
