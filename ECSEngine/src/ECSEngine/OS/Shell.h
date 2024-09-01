#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	namespace OS {

		// Runs a shell command in the specified starting directory and returns true if it succeeded, else false.
		// Does not wait for the command to finish.
		ECSENGINE_API bool ShellRunCommand(Stream<wchar_t> command, Stream<wchar_t> start_directory);
	
	}

}