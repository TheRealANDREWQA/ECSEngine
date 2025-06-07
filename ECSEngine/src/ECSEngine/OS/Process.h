#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	namespace OS {

		union ECSENGINE_API ProcessHandle {
			// Returns true if the API call succeeded and the process is finished, if the process has not finished
			// It returns a valid false optional. If the API itself failed, it returns an empty optional.
			Optional<bool> CheckIsFinished() const;

			// Returns true if it succeeded, else false. The parameter can be configured if you want to use
			// A timed wait. By default, it will wait forever, until the process finishes.
			bool Wait(size_t milliseconds = UINT64_MAX) const;

			// Tries to retrieve the exit code of the process. In case the process it is still pending, it returns
			// An empty optional. It returns a valid optional only if the exit code was retrieved.
			Optional<int> GetExitCode() const;

			// Combines a wait and get exit code together. Returns a valid optional only if the exit code was retrieved
			// After a successful wait.
			ECS_INLINE Optional<int> WaitAndGetExitCode() const {
				if (Wait()) {
					return GetExitCode();
				}
				return {};
			}

			ECS_INLINE bool IsWritePipeValid() const {
				return write_pipe != nullptr;
			}

			ECS_INLINE bool IsReadPipeValid() const {
				return read_pipe != nullptr;
			}

			// Writes data into the stdin of the process, if the write pipe was requested.
			bool WriteToProcess(Stream<void> data) const;

			// Reads data from the stdout of the process. Returns how many bytes were read from 
			// The process. A value of -1 means failure.
			size_t ReadFromProcessBytes(void* data, size_t data_size) const;

			// Reads data from the stdout of the process, if the read pipe was requested.
			bool ReadFromProcess(void* data, size_t data_size) const;

			// Frees/Closes the underlying OS process handles.
			void Close() const;

			size_t id;
			void* handle;

			// These are in/out pipes that can be used to read/write the output/input of the process
			// Such that no shell gimnastics are needed
			void* read_pipe;
			void* write_pipe;
		};

		struct CreateProcessOptions {
			Stream<wchar_t> starting_directory = {};
			bool show_cmd_window = false;

			// These options are valid only for "CreateProcessWithHandle",
			// As all the other create functions don't return the handle to be able
			// To use them.
			bool create_read_pipe = false;
			bool create_write_pipe = false;
			// If this is option is enabled, then failing to create the pipes will fail
			// The entire process creation
			bool create_pipe_failure_is_process_failure = false;
		};

		// Creates a process for the given executable with the given command line without waiting for it to finish. 
		// You can optionally choose to disable the process from showing the CMD window, if it has one and choose
		// The initial starting directory for the process, which by default is the same as the working directory.
		// Returns true if it succeeded, else false
		ECSENGINE_API bool CreateProcessStandalone(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, const CreateProcessOptions& create_options = {});

		// Creates a process for the given executable with the given command line and waits for it to finish. 
		// You can optionally choose to disable the process from showing the CMD window, if it has one and choose
		// The initial starting directory for the process, which by default is the same as the working directory.
		// Returns a valid optional if the initial create succeeded, which contains the return code of the process, else an empty optional.
		ECSENGINE_API Optional<int> CreateProcessAndWait(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, const CreateProcessOptions& create_options = {});


		// Creates a process for the given executable with the given command line and returns a valid process handle if it succeeded. 
		// You can optionally choose to disable the process from showing the CMD window, if it has one and choose
		// The initial starting directory for the process, which by default is the same as the working directory.
		// Returns a valid process handle if it succeeded, else an empty optional
		ECSENGINE_API Optional<ProcessHandle> CreateProcessWithHandle(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, const CreateProcessOptions& create_options = {});

	}
}