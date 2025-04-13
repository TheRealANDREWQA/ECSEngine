#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	namespace OS {

		// Runs a shell command in the specified starting directory and returns true if it succeeded, else false.
		// Does not wait for the command to finish. The start directory can be empty, in which case it will use
		// The current running directory.
		ECSENGINE_API bool ShellRunCommand(Stream<wchar_t> command, Stream<wchar_t> start_directory);
	
		// Instructs a shell instance to use the "Open" verb as in Win32 terminology for a specific executable instead of the runas verb.
		// Returns true if it succeeded, else false and it does not wait for the command to finish.
		ECSENGINE_API bool ShellOpenCommand(Stream<wchar_t> executable_path, Stream<wchar_t> command);

	}

}