#include "ecspch.h"
#include "Crash.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------------

	CrashHandler ECS_GLOBAL_CRASH_HANDLER = { DefaultCrashHandler, nullptr };
	CrashHandler ECS_GLOBAL_DEFERRED_CRASH_HANDLER = { DefaultCrashHandler, nullptr };
	const char* ECS_GLOBAL_DEFERRED_FILE = nullptr;
	const char* ECS_GLOBAL_DEFERRED_FUNCTION = nullptr;
	unsigned int ECS_GLOBAL_DEFERRED_LINE = 0;

	OVERRIDE_CRASH_HANDLER ECS_GLOBAL_OVERRIDE_CRASH_HANDLER = OVERRIDE_CRASH_HANDLER_NONE;

	// --------------------------------------------------------------------------------------------------------

	void SetCrashHandler(CrashHandlerFunction function, void* data) {
		ECS_GLOBAL_CRASH_HANDLER.function = function;
		ECS_GLOBAL_CRASH_HANDLER.data = data;
	}

	// --------------------------------------------------------------------------------------------------------

	void SetDeferredCrashHandler(CrashHandlerFunction function, void* data) {
		ECS_GLOBAL_DEFERRED_CRASH_HANDLER.function = function;
		ECS_GLOBAL_DEFERRED_CRASH_HANDLER.data = data;
	}

	// --------------------------------------------------------------------------------------------------------

	void DefaultCrashHandler(void* data, const char* error_string)
	{
		__debugbreak();
	}

	// --------------------------------------------------------------------------------------------------------

	void Crash(const char* error_string) {
		if (ECS_GLOBAL_OVERRIDE_CRASH_HANDLER != OVERRIDE_CRASH_HANDLER_DEFERRED) {
			ECS_GLOBAL_CRASH_HANDLER.function(ECS_GLOBAL_CRASH_HANDLER.data, error_string);
		}
		else {
			DeferredCrash(error_string);
		}
	}

	// --------------------------------------------------------------------------------------------------------

	void DeferredCrash(const char* error_string) {
		if (ECS_GLOBAL_OVERRIDE_CRASH_HANDLER != OVERRIDE_CRASH_HANDLER_NORMAL) {
			const char* COPY_STRING = "\nCaller: {#}, File: {#}; Line: {#}.";

			size_t string_size = strlen(error_string);
			size_t copy_string_size = strlen(COPY_STRING);
			size_t total_size = string_size + copy_string_size;
			char* new_string = (char*)ECS_STACK_ALLOC(sizeof(char) * (total_size + 1));
			memcpy(new_string, error_string, string_size * sizeof(char));
			memcpy(new_string + string_size, COPY_STRING, copy_string_size * sizeof(char));
			new_string[string_size + copy_string_size] = '\0';

			ECS_GLOBAL_DEFERRED_CRASH_HANDLER.function(ECS_GLOBAL_DEFERRED_CRASH_HANDLER.data, new_string);
		}
		else {
			Crash(error_string);
		}
	}

	// --------------------------------------------------------------------------------------------------------

	void SetDeferredCrashCaller(const char* file, const char* function, unsigned int line)
	{
		ECS_GLOBAL_DEFERRED_FILE = file;
		ECS_GLOBAL_DEFERRED_FUNCTION = function;
		ECS_GLOBAL_DEFERRED_LINE = line;
	}

	// --------------------------------------------------------------------------------------------------------

}