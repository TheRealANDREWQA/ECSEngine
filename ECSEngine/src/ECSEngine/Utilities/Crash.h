#pragma once
#include "../Core.h"
#include "FunctionTemplates.h"

namespace ECSEngine {

	typedef void (*CrashHandlerFunction)(void* data, const char* error_string);

	ECSENGINE_API void SetCrashHandler(CrashHandlerFunction handler, void* data);

	ECSENGINE_API void Crash(const char* error_string);

#define ECS_STRING_CONCAT_INNER(a, b) a ## b
#define ECS_STRING_CONCAT(a, b) ECS_STRING_CONCAT_INNER(a, b)
#define ECS_CRASH(condition, error_string) ECS_FORMAT_TEMP_STRING(ECS_STRING_CONCAT(crash_string_name, __LINE__), "Engine crash in file {0}, line {1}. " error_string, __FILE__, __LINE__);

}