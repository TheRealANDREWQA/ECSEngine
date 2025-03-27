#include "ecspch.h"
#include "Process.h"
#include "../Utilities/StackScope.h"

namespace ECSEngine {

	namespace OS {

		static bool CreateProcessImpl(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, Stream<wchar_t> starting_directory, bool show_cmd_window, HANDLE* process_handle) {
			
		}

		bool CreateProcessStandalone(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, Stream<wchar_t> starting_directory, bool show_cmd_window) {
			Optional<ProcessHandle> process_handle = CreateProcessWithHandle(executable_path, command_line, starting_directory, show_cmd_window);
			if (process_handle.has_value) {
				process_handle.value.Close();
				return true;
			}
			return false;
		}

		Optional<int> CreateProcessAndWait(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, Stream<wchar_t> starting_directory, bool show_cmd_window) {
			Optional<ProcessHandle> process_handle = CreateProcessWithHandle(executable_path, command_line, starting_directory, show_cmd_window);
			if (process_handle.has_value) {
				Optional<int> exit_code = process_handle.value.WaitAndGetExitCode();
				process_handle.value.Close();
				return exit_code;
			}
			return {};
		}

		Optional<ProcessHandle> CreateProcessWithHandle(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, Stream<wchar_t> starting_directory, bool show_cmd_window) {
			NULL_TERMINATE_WIDE(executable_path);
			NULL_TERMINATE_WIDE(command_line);
			NULL_TERMINATE_WIDE_POTENTIAL_EMPTY(starting_directory);

			STARTUPINFO info;
			memset(&info, 0, sizeof(info));
			info.cb = sizeof(info);
			info.wShowWindow = show_cmd_window ? SW_SHOW : SW_HIDE;
			info.dwFlags = STARTF_USESHOWWINDOW;

			PROCESS_INFORMATION process_information;
			memset(&process_information, 0, sizeof(process_information));
			bool success = CreateProcess(
				executable_path.buffer,
				command_line.buffer,
				NULL,
				NULL,
				FALSE,
				0,
				NULL,
				starting_directory.size == 0 ? NULL : starting_directory.buffer,
				&info,
				&process_information
			);

			if (success) {
				CloseHandle(process_information.hThread);
				ProcessHandle process_handle;
				process_handle.handle = process_information.hProcess;
				return process_handle;
			}
			return {};
		}

		Optional<bool> ProcessHandle::CheckIsFinished() const
		{
			DWORD wait_result = WaitForSingleObject(handle, 0);
			if (wait_result == WAIT_OBJECT_0) {
				return true;
			}
			else if (wait_result == WAIT_TIMEOUT) {
				return false;
			}
			return {};
		}

		bool ProcessHandle::Wait(size_t milliseconds) const
		{
			return WaitForSingleObject(handle, milliseconds == UINT64_MAX ? INFINITE : milliseconds) == WAIT_OBJECT_0;
		}

		Optional<int> ProcessHandle::GetExitCode() const
		{
			DWORD exit_code;
			if (GetExitCodeProcess(handle, &exit_code)) {
				return exit_code == STILL_ACTIVE ? Optional<int>{} : exit_code;
			}
			return {};
		}

		void ProcessHandle::Close() const
		{
			CloseHandle(handle);
		}


	}

}