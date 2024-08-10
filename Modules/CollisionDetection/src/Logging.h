#pragma once
#include "ECSEngineContainersCommon.h"
#include "ECSEngineConsole.h"
#include "ECSEngineEntities.h"

using namespace ECSEngine;

void LogInfo(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MEDIUM);

void LogWarn(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MEDIUM);

void LogError(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_IMPORTANT);

void LogTrace(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MEDIUM);

void Log(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_IMPORTANT);

void LogEntity(Entity entity, ECS_CONSOLE_MESSAGE_TYPE type, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_IMPORTANT);

#define CONSOLE_INFO(message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogInfo(_UNIQUE_NAME(_console_message))
#define CONSOLE_WARN(message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogWarn(_UNIQUE_NAME(_console_message))
#define CONSOLE_ERROR(message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogError(_UNIQUE_NAME(_console_message))
#define CONSOLE_TRACE(message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogTrace(_UNIQUE_NAME(_console_message))

#define CONSOLE_INFO_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogInfo(_UNIQUE_NAME(_console_message), verbosity)
#define CONSOLE_WARN_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogWarn(_UNIQUE_NAME(_console_message), verbosity)
#define CONSOLE_ERROR_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogError(_UNIQUE_NAME(_console_message), verbosity)
#define CONSOLE_TRACE_VERBOSITY(verbosity, message, ...) ECS_FORMAT_TEMP_STRING(_UNIQUE_NAME(_console_message), message, __VA_ARGS__); LogTrace(_UNIQUE_NAME(_console_message), verbosity)