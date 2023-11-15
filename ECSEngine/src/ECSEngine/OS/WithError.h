#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace Tools {
		struct UISystem;
	}

	namespace OS {

		typedef bool (*FolderFunction)(Stream<wchar_t> path);

		ECSENGINE_API void ErrorWindow(Stream<wchar_t> path, FolderFunction function, const char* error_string, Tools::UISystem* system);

		ECSENGINE_API void ErrorConsole(Stream<wchar_t> path, FolderFunction function, const char* error_string);

	}

}