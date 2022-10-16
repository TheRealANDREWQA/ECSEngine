#pragma once
#include "../Core.h"
#include "FunctionInterfaces.h"

namespace ECSEngine {

	typedef void (*CrashHandlerFunction)(void* data, Stream<char> error_string);

	struct CrashHandler {
		CrashHandlerFunction function;
		void* data;
	};

	extern CrashHandler ECS_GLOBAL_CRASH_HANDLER;
	extern const char* ECS_GLOBAL_DEFERRED_FILE;
	extern const char* ECS_GLOBAL_DEFERRED_FUNCTION;
	extern unsigned int ECS_GLOBAL_DEFERRED_LINE;

	ECSENGINE_API void SetCrashHandler(CrashHandlerFunction handler, void* data);

	// It will __debugbreak() only
	ECSENGINE_API void DebugbreakCrashHandler(void* data, Stream<char> error_string);

	ECSENGINE_API void Crash(Stream<char> error_string);

	ECSENGINE_API void SetCrashHandlerCaller(const char* file, const char* function, unsigned int line);

	ECSENGINE_API void ResetCrashHandlerCaller();

#define ECS_STRING_CONCAT_INNER(a, b) a ## b
#define ECS_STRING_CONCAT(a, b) ECS_STRING_CONCAT_INNER(a, b)

#define ECS_CRASH_IMPLEMENTATION(error_string, crash_function, ...) ECS_STACK_CAPACITY_STREAM(char, base_characters, 512); base_characters.Copy("Engine crash in file {#}, function {#}, line {#}. "); \
	base_characters.AddStreamSafe(error_string); \
	base_characters[base_characters.size] = '\0'; \
	ECS_FORMAT_TEMP_STRING(ECS_STRING_CONCAT(crash_string_name, __LINE__), base_characters.buffer, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__); \
	crash_function(ECS_STRING_CONCAT(crash_string_name, __LINE__).buffer);

#define ECS_CRASH_IMPLEMENTATION_EX(error_string, crash_function, file, function, line, ...) ECS_STACK_CAPACITY_STREAM(char, base_characters, 512); base_characters.Copy("Engine crash in file {#}, function {#}, line {#}. "); \
	base_characters.AddStreamSafe(error_string); \
	base_characters[base_characters.size] = '\0'; \
	ECS_FORMAT_TEMP_STRING(ECS_STRING_CONCAT(crash_string_name, line), base_characters.buffer, file, function, line, __VA_ARGS__); \
	crash_function(ECS_STRING_CONCAT(crash_string_name, line).buffer);

#define ECS_CRASH_RETURN_IMPLEMENTATION(condition, crash_function, error_string, ...) if (!(condition)) { crash_function(error_string, __VA_ARGS__); return; }
#define ECS_CRASH_RETURN_VALUE_IMPLEMENTATION(condition, crash_function, return_value, error_string, ...) if (!(condition)) { crash_function(error_string, __VA_ARGS__); return return_value; }

	// The __VA_ARGS__ can be used to format the error string
#define ECS_CRASH(error_string, ...) ECS_CRASH_IMPLEMENTATION(error_string, Crash, __VA_ARGS__);
	// The __VA_ARGS__ can be used to format the error string
#define ECS_CRASH_EX(error_string, file, function, line, ...) ECS_CRASH_IMPLEMENTATION_EX(error_string, Crash, file, function, line, __VA_ARGS__);

	// The __VA_ARGS__ can be used to format the error string
#define ECS_CRASH_RETURN(condition, error_string, ...) ECS_CRASH_RETURN_IMPLEMENTATION(condition, ECS_CRASH, error_string, __VA_ARGS__)
	// The __VA_ARGS__ can be used to format the error string
#define ECS_CRASH_RETURN_VALUE(condition, return_value, error_string, ...) ECS_CRASH_RETURN_VALUE_IMPLEMENTATION(condition, ECS_CRASH, return_value, error_string, __VA_ARGS__)

	// The __VA_ARGS__ can be used to format the error string
#define ECS_CRASH_RETURN_EX(condition, error_string, file, function, line, ...) ECS_CRASH_RETURN_IMPLEMENTATION(condition, ECS_CRASH_EX, error_string, file, function, line, __VA_ARGS__)
	// The __VA_ARGS__ can be used to format the error string
#define ECS_CRASH_RETURN_VALUE_EX(condition, return_value, error_string, file, function, line, ...) if (!(condition)) { ECS_CRASH_EX(error_string, file, function, file, __VA_ARGS__); return return_value; }

}