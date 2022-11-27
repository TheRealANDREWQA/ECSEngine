#pragma once

#include "../ECSEngine/Application.h"
#include <Windows.h>

#define ECSENGINE_RENDER

#ifdef ECSENGINE_PLATFORM_WINDOWS

#ifdef ECSENGINE_RENDER

extern ECSEngine::Application* ECSEngine::CreateApplication(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	auto application = ECSEngine::CreateApplication(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

	int exit_code = application->Run();

	delete application;

	return exit_code;

}
#endif // ECSENGINE_RENDER

#ifdef ECSENGINE_CONSOLE

extern ECSEngine::Application* ECSEngine::CreateApplication();

int main() {

	ECSEngine::Log::Init();

	auto application = ECSEngine::CreateApplication();

	application->Run();

	delete application;

	return 0;

}
#endif // ECSENGINE_CONSOLE

#endif // ECSENGINE_PLATFORM_WINDOWS