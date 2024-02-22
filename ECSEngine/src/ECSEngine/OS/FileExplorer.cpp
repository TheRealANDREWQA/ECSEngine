#include "ecspch.h"
#include "FileExplorer.h"
#include "WithError.h"

// For some reason, the compiler won't produce the definition for this file
// Without this include even tho we don't really need it
#include "../Tools/UI/UISystem.h"

namespace ECSEngine {

	namespace OS {

		bool LaunchFileExplorer(Stream<wchar_t> folder) {
			wchar_t temp_characters[1024];
			CapacityStream<wchar_t> temp_stream(temp_characters, 0, 1024);
			temp_stream.AddStream(Stream<wchar_t>(L"explorer.exe /select,\"", ECS_COUNTOF(L"explorer.exe /select,\"") - 1));
			temp_stream.AddStream(folder);
			temp_stream.Add(L'\"');
			temp_stream.AddAssert(L'\0');

			STARTUPINFO startup_info;
			memset(&startup_info, 0, sizeof(startup_info));
			startup_info.cb = sizeof(startup_info);

			PROCESS_INFORMATION process_information;
			memset(&process_information, 0, sizeof(process_information));

			bool value = CreateProcess(nullptr, temp_stream.buffer, nullptr, nullptr, false, 0, nullptr, nullptr, &startup_info, &process_information);
			DWORD error;
			if (!value) {
				error = GetLastError();
			}
			return value;
		}

		void SetBasicErrorMessage(const char* error, CapacityStream<char>& message) {
			if (message.buffer != nullptr) {
				Stream<char> stream = error;
				message.AddStreamSafe(stream);
				message[message.size] = '\0';
			}
		}

		bool FileExplorerGetFile(FileExplorerGetFileData* data) {
			data->user_cancelled = false;

			IFileOpenDialog* dialog = nullptr;
			HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			if (result != S_OK && result != S_FALSE) {
				SetBasicErrorMessage("Failed to initialize COM subsystem.", data->error_message);
				data->path.size = 0;
				return false;
			}

			result = CoCreateInstance(
				CLSID_FileOpenDialog,
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&dialog)
			);

			if (SUCCEEDED(result)) {
				DWORD dialog_options;
				result = dialog->GetOptions(&dialog_options);

				if (SUCCEEDED(result)) {
					if (data->extensions.size == 0) {
						result = dialog->SetOptions(dialog_options | FOS_NOCHANGEDIR);
						if (FAILED(result)) {
							SetBasicErrorMessage("Setting dialog options failed!", data->error_message);
							data->path.size = 0;
							return false;
						}
					}
					else {
						result = dialog->SetOptions(dialog_options | FOS_NOCHANGEDIR | FOS_STRICTFILETYPES);
						if (FAILED(result)) {
							SetBasicErrorMessage("Setting dialog options failed!", data->error_message);
							data->path.size = 0;
							return false;
						}

						ECS_STACK_CAPACITY_STREAM(wchar_t, modified_extensions, 512);

						size_t extension_count = data->extensions.size > 1 ? data->extensions.size + 1 : 1;

						// Have an extra field with the all extensions allowed
						COMDLG_FILTERSPEC* filters = (COMDLG_FILTERSPEC*)ECS_STACK_ALLOC(sizeof(COMDLG_FILTERSPEC) * extension_count);
						for (size_t index = 0; index < data->extensions.size; index++) {
							filters[index] = { L"", modified_extensions.buffer + modified_extensions.size };
							modified_extensions.Add(L'*');
							modified_extensions.AddStream(data->extensions[index]);
							modified_extensions.Add(L'\0');
						}

						if (extension_count > 1) {
							// Create the last all filter
							filters[data->extensions.size] = { L"", modified_extensions.buffer + modified_extensions.size };
							for (size_t index = 0; index < data->extensions.size; index++) {
								modified_extensions.Add(L'*');
								modified_extensions.AddStream(data->extensions[index]);
								modified_extensions.Add(L';');
							}
							modified_extensions[modified_extensions.size] = L'\0';
						}

						result = dialog->SetFileTypes(extension_count, filters);

						if (FAILED(result)) {
							SetBasicErrorMessage("Setting dialog extensions failed!", data->error_message);
							data->path.size = 0;
							return false;
						}
					}
				}
				else {
					SetBasicErrorMessage("Getting dialog options failed!", data->error_message);
					data->path.size = 0;
					return false;
				}

				result = dialog->Show(nullptr);

				if (SUCCEEDED(result)) {
					IShellItem* item;
					result = dialog->GetResult(&item);

					if (SUCCEEDED(result)) {
						wchar_t* temp_path;
						result = item->GetDisplayName(SIGDN_FILESYSPATH, &temp_path);
						if (FAILED(result)) {
							SetBasicErrorMessage("Getting the path failed!", data->error_message);
							data->path.size = 0;
							return false;
						}

						data->path.CopyOther(temp_path, wcslen(temp_path));
						data->path[data->path.size] = L'\0';
						CoTaskMemFree(temp_path);
						item->Release();
						dialog->Release();

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

			SetBasicErrorMessage("The user cancelled the search.", data->error_message);
			data->user_cancelled = true;
			return false;
		}

		bool FileExplorerGetDirectory(FileExplorerGetDirectoryData* data) {
			data->user_cancelled = false;

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
								data->path.CopyOther(temp_path, wcslen(temp_path));
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
						else {
							SetBasicErrorMessage("The user cancelled the selection.", data->error_message);
							data->user_cancelled = true;
							return false;
						}
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

#define LAUNCH_FILE_EXPLORER_ERROR_STRING "Launching file explorer at {#} failed. Incorrect path."

		void LaunchFileExplorerWithError(Stream<wchar_t> path, Tools::UISystem* system)
		{
			ErrorWindow(path, OS::LaunchFileExplorer, LAUNCH_FILE_EXPLORER_ERROR_STRING, system);
		}

		void LaunchFileExplorerWithError(Stream<wchar_t> path) {
			ErrorConsole(path, OS::LaunchFileExplorer, LAUNCH_FILE_EXPLORER_ERROR_STRING);
		}

	}

}