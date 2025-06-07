#include "ecspch.h"
#include "Process.h"
#include "../Utilities/StackScope.h"

namespace ECSEngine {

	namespace OS {

		static CreateProcessOptions CreateOptionsWithoutPipes(const CreateProcessOptions& options) {
			CreateProcessOptions no_pipe_options;
			no_pipe_options.create_read_pipe = false;
			no_pipe_options.create_write_pipe = false;
			return no_pipe_options;
		}

		bool CreateProcessStandalone(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, const CreateProcessOptions& options) {
			// Create options without the read/write pipes set - since they are not of use in this case
			Optional<ProcessHandle> process_handle = CreateProcessWithHandle(executable_path, command_line, CreateOptionsWithoutPipes(options));
			if (process_handle.has_value) {
				process_handle.value.Close();
				return true;
			}
			return false;
		}

		Optional<int> CreateProcessAndWait(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, const CreateProcessOptions& options) {
			Optional<ProcessHandle> process_handle = CreateProcessWithHandle(executable_path, command_line, CreateOptionsWithoutPipes(options));
			if (process_handle.has_value) {
				Optional<int> exit_code = process_handle.value.WaitAndGetExitCode();
				process_handle.value.Close();
				return exit_code;
			}
			return {};
		}

		Optional<ProcessHandle> CreateProcessWithHandle(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, const CreateProcessOptions& options) {
			NULL_TERMINATE_WIDE(executable_path);
			NULL_TERMINATE_WIDE(command_line);
			// Cannot use options.starting_directory directly in the macro, so create a local for it
			Stream<wchar_t> starting_directory = options.starting_directory;
			NULL_TERMINATE_WIDE_POTENTIAL_EMPTY(starting_directory);

			STARTUPINFO info;
			memset(&info, 0, sizeof(info));
			info.cb = sizeof(info);
			info.wShowWindow = options.show_cmd_window ? SW_SHOW : SW_HIDE;
			info.dwFlags = STARTF_USESHOWWINDOW;

			struct Pipe {
				HANDLE read_handle;
				HANDLE write_handle;
			};

			// Returns a valid pipe optional if the operation succeeded, else an empty optional
			auto create_pipe = [&](bool stdin_pipe) -> Optional<Pipe> {
				SECURITY_ATTRIBUTES security_attribute{};
				security_attribute.nLength = sizeof(SECURITY_ATTRIBUTES);
				// Pipe handles are inherited
				security_attribute.bInheritHandle = TRUE;
				security_attribute.lpSecurityDescriptor = nullptr;

				HANDLE read_pipe = nullptr;
				HANDLE write_pipe = nullptr;
				// Create pipes. The failure case needs to be handled only if the create_pipe_failure_is_process_failure
				// Is enabled.
				if (!CreatePipe(&read_pipe, &write_pipe, &security_attribute, 0)) {
					return {};
				}

				// Set the handles to non inherit - the write pipe for stdout
				// And the in pipe for stdin
				SetHandleInformation(stdin_pipe ? write_pipe : read_pipe, HANDLE_FLAG_INHERIT, 0);
				info.dwFlags |= STARTF_USESTDHANDLES;
				if (stdin_pipe) {
					info.hStdInput = read_pipe;
				}
				else {
					info.hStdOutput = write_pipe;
				}
				return Pipe{ read_pipe, write_pipe };
			};

			auto close_pipe = [](const Optional<Pipe>& pipe) -> void {
				if (pipe.has_value) {
					CloseHandle(pipe.value.read_handle);
					CloseHandle(pipe.value.write_handle);
				}
			};

			Optional<Pipe> read_pipe;
			if (options.create_read_pipe) {
				read_pipe = create_pipe(false);
				// The read pipe is the stdout pipe
				if (!read_pipe.has_value && options.create_pipe_failure_is_process_failure) {
					return {};
				}
			}

			Optional<Pipe> write_pipe;
			if (options.create_write_pipe) {
				write_pipe = create_pipe(true);
				// The write pipe is the stdin pipe
				if (!write_pipe.has_value && options.create_pipe_failure_is_process_failure) {
					// Close the read pipe handles, if they are present
					close_pipe(read_pipe);
					return {};
				}
			}

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
				// Set the pipes as well.
				process_handle.read_pipe = read_pipe.value.read_handle;
				process_handle.write_pipe = write_pipe.value.write_handle;

				// We must close the opposing pipes now
				if (read_pipe.has_value) {
					CloseHandle(read_pipe.value.write_handle);
				}
				if (write_pipe.has_value) {
					CloseHandle(write_pipe.value.read_handle);
				}

				return process_handle;
			}
			else {
				// Close the pipes - if they were created
				close_pipe(read_pipe);
				close_pipe(write_pipe);
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

		bool ProcessHandle::WriteToProcess(Stream<void> data) const
		{
			// TODO: Do chunking for this case?
			ECS_ASSERT(data.size <= DWORD_MAX, "Writing to a process too much data at a time!");

			DWORD bytes_written = 0;
			return WriteFile(write_pipe, data.buffer, data.size, &bytes_written, nullptr) && bytes_written == data.size;
		}

		size_t ProcessHandle::ReadFromProcessBytes(void* data, size_t data_size) const
		{
			// TODO: Do chunking for this case?
			ECS_ASSERT(data_size <= DWORD_MAX, "Reading from a process too much data at a time!");
			
			DWORD bytes_read = 0;
			if (!ReadFile(read_pipe, data, data_size, &bytes_read, nullptr)) {
				return -1;
			}

			return bytes_read;
		}

		bool ProcessHandle::ReadFromProcess(void* data, size_t data_size) const
		{
			return ReadFromProcessBytes(data, data_size) != -1;
		}

		void ProcessHandle::Close() const
		{
			CloseHandle(handle);
			// If the read/write pipes are present, close them as well
			if (read_pipe != nullptr) {
				CloseHandle(read_pipe);
			}
			if (write_pipe != nullptr) {
				CloseHandle(write_pipe);
			}
		}


	}

}