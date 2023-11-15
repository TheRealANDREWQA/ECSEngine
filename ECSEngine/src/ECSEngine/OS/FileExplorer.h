#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace Tools {
		struct UISystem;
	}

	namespace OS {

		ECSENGINE_API bool LaunchFileExplorer(Stream<wchar_t> folder);

		// The path must be initialized
		struct FileExplorerGetFileData {
			CapacityStream<wchar_t> path;
			CapacityStream<char> error_message = { nullptr, 0, 0 };
			Stream<Stream<wchar_t>> extensions = { nullptr, 0 };
			void* hWnd = NULL;
			bool user_cancelled = false;
		};

		// The path must be initialized
		struct FileExplorerGetDirectoryData {
			CapacityStream<wchar_t> path;
			CapacityStream<char> error_message = { nullptr, 0, 0 };
			bool user_cancelled = false;
		};

		ECSENGINE_API bool FileExplorerGetFile(FileExplorerGetFileData* data);

		ECSENGINE_API bool FileExplorerGetDirectory(FileExplorerGetDirectoryData* data);

		ECSENGINE_API void LaunchFileExplorerWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void LaunchFileExplorerWithError(Stream<wchar_t> path);

	}

}