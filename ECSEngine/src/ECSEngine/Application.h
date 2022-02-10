#pragma once
#include "ecspch.h"
#include "Core.h"

#define ECS_CURSOR_COUNT 12

namespace ECSEngine {

	constexpr size_t ECS_PLATFORM_WIN64_DX11 = 1 << 0;
	constexpr size_t ECS_PLATFORM_WIN64_DX11_STRING_INDEX = 0;
	constexpr size_t ECS_PLATFORM_ALL = ECS_PLATFORM_WIN64_DX11;

	extern ECSENGINE_API const char* ECS_PLATFORM_STRINGS[];

	enum class ECSENGINE_API CursorType : unsigned char {
		Default,
		IBeam,
		AppStarting,
		Cross,
		Hand,
		Help,
		SizeAll,
		SizeNESW,
		SizeNS,
		SizeEW,
		SizeNWSE,
		Wait
	};

	class ECSENGINE_API Application
	{
	public:
		Application();
		virtual ~Application();

		Application(const Application& other) = default;
		Application& operator = (const Application& other) = default;
		
		virtual int Run();
		
		virtual void ChangeCursor(CursorType type);
		
		virtual CursorType GetCurrentCursor() const;
		
		// null terminated
		virtual void WriteTextToClipboard(const char* text);
		
		// returns the count of characters written
		virtual unsigned int CopyTextFromClipboard(char* text, unsigned int max_size);

		virtual void* GetOSWindowHandle();
	};

	// To be defined in Client
	Application* CreateApplication(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
	Application* CreateApplication();

}
