#include "pch.h"
#include "Logging.h"

#define PROJECT_NAME "Physics"

void LogInfo(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Info(message, PROJECT_NAME, verbosity);
}

void LogWarn(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Warn(message, PROJECT_NAME, verbosity);
}

void LogError(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Error(message, PROJECT_NAME, verbosity);
}

void LogTrace(Stream<char> message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Trace(message, PROJECT_NAME, verbosity);
}

void Log(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Message(message, type, PROJECT_NAME, verbosity);
}

void LogEntity(Entity entity, ECS_CONSOLE_MESSAGE_TYPE type, ECS_CONSOLE_VERBOSITY verbosity) {
	ECS_ENTITY_TO_STRING(string, entity);
	Log(string, type, verbosity);
}