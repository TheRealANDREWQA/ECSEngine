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

			// Frees/Closes the underlying OS process handle.
			void Close() const;

			size_t id;
			void* handle;
		};

		// Creates a process for the given executable with the given command line without waiting for it to finish. 
		// You can optionally choose to disable the process from showing the CMD window, if it has one and choose
		// The initial starting directory for the process, which by default is the same as the working directory.
		// Returns true if it succeeded, else false
		ECSENGINE_API bool CreateProcessStandalone(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, Stream<wchar_t> starting_directory = {}, bool show_cmd_window = true);

		// Creates a process for the given executable with the given command line and waits for it to finish. 
		// You can optionally choose to disable the process from showing the CMD window, if it has one and choose
		// The initial starting directory for the process, which by default is the same as the working directory.
		// Returns a valid optional if the initial create succeeded, which contains the return code of the process, else an empty optional.
		ECSENGINE_API Optional<int> CreateProcessAndWait(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, Stream<wchar_t> starting_directory = {}, bool show_cmd_window = true);


		// Creates a process for the given executable with the given command line and returns a valid process handle if it succeeded. 
		// You can optionally choose to disable the process from showing the CMD window, if it has one and choose
		// The initial starting directory for the process, which by default is the same as the working directory.
		// Returns a valid process handle if it succeeded, else an empty optional
		ECSENGINE_API Optional<ProcessHandle> CreateProcessWithHandle(Stream<wchar_t> executable_path, Stream<wchar_t> command_line, Stream<wchar_t> starting_directory = {}, bool show_cmd_window = true);

	}
}