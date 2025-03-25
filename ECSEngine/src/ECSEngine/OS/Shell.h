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
	
	}

}