#include "ecspch.h"
#include "Application.h"

namespace ECSEngine {

	Application::Application() {

	};
	Application::~Application() {

	};

	int Application::Run() {
		return 0;
	}

	void Application::ChangeCursor(CursorType type)
	{
	}

	CursorType Application::GetCurrentCursor() const
	{
		return CursorType(0);
	}

	void Application::WriteTextToClipboard(const char* text) {

	}

	unsigned int Application::CopyTextFromClipboard(char* text, unsigned int max_size) {
		return 0;
	}
}
