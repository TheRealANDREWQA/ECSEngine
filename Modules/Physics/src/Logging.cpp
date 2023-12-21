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