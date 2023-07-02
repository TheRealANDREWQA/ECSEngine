#pragma once
#include "ecspch.h"
#include "Core.h"
#include "Utilities/BasicTypes.h"

#define ECS_CURSOR_COUNT 12

namespace ECSEngine {

	constexpr size_t ECS_PLATFORM_WIN64_DX11 = 1 << 0;
	constexpr size_t ECS_PLATFORM_WIN64_DX11_STRING_INDEX = 0;
	constexpr size_t ECS_PLATFORM_ALL = ECS_PLATFORM_WIN64_DX11;

	extern ECSENGINE_API const char* ECS_PLATFORM_STRINGS[];

	enum ECS_CURSOR_TYPE : unsigned char {
		ECS_CURSOR_DEFAULT,
		ECS_CURSOR_IBEAM,
		ECS_CURSOR_APP_STARTING,
		ECS_CURSOR_CROSS,
		ECS_CURSOR_HAND,
		ECS_CURSOR_HELP,
		ECS_CURSOR_SIZE_ALL,
		ECS_CURSOR_SIZE_NESW,
		ECS_CURSOR_SIZE_NS,
		ECS_CURSOR_SIZE_EW,
		ECS_CURSOR_SIZE_NWSE,
		ECS_CURSOR_WAIT
	};

	class ECSENGINE_API Application
	{
	public:
		Application() {}
		virtual ~Application() {}

		Application(const Application& other) = default;
		Application& operator = (const Application& other) = default;
		
		virtual int Run() = 0;
		
		virtual void ChangeCursor(ECS_CURSOR_TYPE type) = 0;
		
		virtual ECS_CURSOR_TYPE GetCurrentCursor() const = 0;
		
		// null terminated
		virtual void WriteTextToClipboard(const char* text) = 0;
		
		// returns the count of characters written
		virtual unsigned int CopyTextFromClipboard(char* text, unsigned int max_size) = 0;

		virtual void* GetOSWindowHandle() = 0;

		virtual uint2 GetCursorPosition() const = 0;

		virtual void SetCursorPosition(uint2 position) = 0;

		// Sets the position of the cursor relative to the current window position
		virtual void SetCursorPositionRelative(uint2 position) = 0;
	};

	// To be defined in Client
	Application* CreateApplication(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
	Application* CreateApplication();

}
