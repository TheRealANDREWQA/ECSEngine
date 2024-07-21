#pragma once
#include "ECSEngineContainersCommon.h"
#include "ECSEngineConsole.h"

using namespace ECSEngine;

void LogInfo(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MEDIUM);

void LogWarn(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MEDIUM);

void LogError(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_IMPORTANT);

void LogTrace(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MEDIUM);

#define CONSOLE_INFO(message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogInfo(_console_message)
#define CONSOLE_WARN(message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogWarn(_console_message)
#define CONSOLE_ERROR(message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogError(_console_message)
#define CONSOLE_TRACE(message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogTrace(_console_message)

#define CONSOLE_INFO_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogInfo(_console_message, verbosity)
#define CONSOLE_WARN_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogWarn(_console_message, verbosity)
#define CONSOLE_ERROR_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogError(_console_message, verbosity)
#define CONSOLE_TRACE_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_console_message, message, __VA_ARGS__); LogTrace(_console_message, verbosity)