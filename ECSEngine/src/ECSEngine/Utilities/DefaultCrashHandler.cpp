#include "ecspch.h"
#include "DefaultCrashHandler.h"
#include "Crash.h"
#include "../Internal/World.h"
#include "OSFunctions.h"
#include "Console.h"

namespace ECSEngine {

	struct DefaultCrashHandlerData {
		World* world;
	};

	void DefaultCrashHandlerFunction(void* _data, const char* error_string) {
		World* world = (World*)_data;

		ECS_STACK_CAPACITY_STREAM(char, full_error_message, ECS_KB);
		full_error_message.Copy(error_string);
		full_error_message.AddStream("\nThe call stack of the crash: ");

		Console* console = GetConsole();

		// Get the call stack of the crash
		OS::GetCallStackFunctionNames(full_error_message);
		console->Error(full_error_message);
	}

	void SetDefaultCrashHandler(World* world, Stream<Stream<wchar_t>> module_search_path) {
		OS::InitializeSymbolicLinksPaths(module_search_path);
		SetCrashHandler(DefaultCrashHandlerFunction, world);
	}

}