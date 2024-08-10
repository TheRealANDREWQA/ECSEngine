#include "ecspch.h"
#include "Crash.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------------

	CrashHandler ECS_GLOBAL_CRASH_HANDLER = { DebugbreakCrashHandler, nullptr };
	const char* ECS_GLOBAL_DEFERRED_FILE = nullptr;
	const char* ECS_GLOBAL_DEFERRED_FUNCTION = nullptr;
	unsigned int ECS_GLOBAL_DEFERRED_LINE = -1;
	std::atomic<unsigned int> ECS_GLOBAL_CRASH_IN_PROGRESS = 0;
	size_t ECS_GLOBAL_CRASH_OS_THREAD_ID = 0;
	Semaphore ECS_GLOBAL_CRASH_SEMAPHORE;

	// --------------------------------------------------------------------------------------------------------

	void SetCrashHandler(CrashHandlerFunction function, void* data) {
		ECS_GLOBAL_CRASH_HANDLER.function = function;
		ECS_GLOBAL_CRASH_HANDLER.data = data;
	}

	// --------------------------------------------------------------------------------------------------------

	void DebugbreakCrashHandler(void* data, Stream<char> error_string)
	{
		__debugbreak();
	}

	// --------------------------------------------------------------------------------------------------------

	void Crash(Stream<char> error_string) {
		if (ECS_GLOBAL_DEFERRED_FILE == nullptr && ECS_GLOBAL_DEFERRED_FUNCTION == nullptr && ECS_GLOBAL_DEFERRED_LINE == -1) {
			ECS_GLOBAL_CRASH_HANDLER.function(ECS_GLOBAL_CRASH_HANDLER.data, error_string);
		}
		else {
			const char* COPY_STRING = "\nCaller: {#}, File: {#}; Line: {#}.";

			Stream<char> file_string = ECS_GLOBAL_DEFERRED_FILE != nullptr ? ECS_GLOBAL_DEFERRED_FILE : Stream<char>(nullptr, 0);
			Stream<char> function_string = ECS_GLOBAL_DEFERRED_FILE != nullptr ? ECS_GLOBAL_DEFERRED_FUNCTION : Stream<char>(nullptr, 0);
			unsigned int line = ECS_GLOBAL_DEFERRED_LINE;

			// Estimate how many characters will be needed
			// The line needs at most another 10
			size_t characters_needed = file_string.size + function_string.size + 10;

			// Approximate that the caller + file + line will need at most another 512 characters
			char* new_string = (char*)ECS_STACK_ALLOC(sizeof(char) * (error_string.size + characters_needed));
			error_string.CopyTo(new_string);
			CapacityStream<char> new_string_stream(new_string, error_string.size, error_string.size + characters_needed);

			FormatString(new_string_stream, "\nCaller: {#}, File: {#}, Line: {#}", function_string, file_string, line);
			ECS_GLOBAL_CRASH_HANDLER.function(ECS_GLOBAL_CRASH_HANDLER.data, new_string_stream);
		}
	}

	// --------------------------------------------------------------------------------------------------------

	void ResetCrashHandlerCaller()
	{
		SetCrashHandlerCaller(nullptr, nullptr, -1);
	}

	// --------------------------------------------------------------------------------------------------------

	void ResetCrashHandlerGlobalVariables()
	{
		ECS_GLOBAL_DEFERRED_FILE = nullptr;
		ECS_GLOBAL_DEFERRED_FUNCTION = nullptr;
		ECS_GLOBAL_DEFERRED_LINE = -1;
		ECS_GLOBAL_CRASH_IN_PROGRESS.store(0, ECS_RELAXED);
		ECS_GLOBAL_CRASH_OS_THREAD_ID = 0;
		ECS_GLOBAL_CRASH_SEMAPHORE.ClearCount();
		ECS_GLOBAL_CRASH_SEMAPHORE.ClearTarget();
	}

	// --------------------------------------------------------------------------------------------------------

	void SetCrashHandlerCaller(const char* file, const char* function, unsigned int line) {
		ECS_GLOBAL_DEFERRED_FILE = file;
		ECS_GLOBAL_DEFERRED_FUNCTION = function;
		ECS_GLOBAL_DEFERRED_LINE = line;
	}

	// --------------------------------------------------------------------------------------------------------

}