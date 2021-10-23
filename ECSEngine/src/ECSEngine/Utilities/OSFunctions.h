#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"

ECS_CONTAINERS;

namespace ECSEngine {

	namespace Tools {
		struct UISystem;
		struct Console;
	}

}

using namespace ECSEngine::Tools;

#define ECS_OS using namespace ECSEngine::OS

namespace ECSEngine {

	namespace OS {

#pragma region Basic APIs

		ECSENGINE_API bool LaunchFileExplorer(const wchar_t* folder);

		ECSENGINE_API bool LaunchFileExplorer(containers::Stream<wchar_t> folder);

		// A pointer null means I don't care; returns whether or not succeeded
		// Works for directories too
		ECSENGINE_API bool GetFileTimes(
			const wchar_t* ECS_RESTRICT path,
			char* ECS_RESTRICT creation_time = nullptr,
			char* ECS_RESTRICT access_time = nullptr,
			char* ECS_RESTRICT last_write_time = nullptr
		);

		// A pointer null means I don't care; returns whether or not succeeded
		// Works for directories too
		ECSENGINE_API bool GetFileTimes(
			const wchar_t* ECS_RESTRICT path,
			wchar_t* ECS_RESTRICT creation_time = nullptr,
			wchar_t* ECS_RESTRICT access_time = nullptr,
			wchar_t* ECS_RESTRICT last_write_time = nullptr
		);

		// A pointer null means I don't care; returns whether or not succeeded; output is in milliseconds
		// Works for directories too
		ECSENGINE_API bool GetRelativeFileTimes(
			const wchar_t* ECS_RESTRICT path,
			size_t* ECS_RESTRICT creation_time = nullptr,
			size_t* ECS_RESTRICT access_time = nullptr,
			size_t* ECS_RESTRICT last_write_time = nullptr
		);

		// A pointer null means I don't care; returns whether or not succeeded
		// Works for directories too
		ECSENGINE_API bool GetRelativeFileTimes(
			const wchar_t* ECS_RESTRICT path,
			char* ECS_RESTRICT creation_time = nullptr,
			char* ECS_RESTRICT access_time = nullptr,
			char* ECS_RESTRICT last_write_time = nullptr
		);

		// A pointer null means I don't care; returns whether or not succeeded
		// Works for directories too
		ECSENGINE_API bool GetRelativeFileTimes(
			const wchar_t* ECS_RESTRICT path,
			wchar_t* ECS_RESTRICT creation_time = nullptr,
			wchar_t* ECS_RESTRICT access_time = nullptr,
			wchar_t* ECS_RESTRICT last_write_time = nullptr
		);

		ECSENGINE_API bool OpenFileWithDefaultApplication(
			const wchar_t* path,
			CapacityStream<char>* error_message = nullptr
		);

		ECSENGINE_API bool OpenFileWithDefaultApplication(
			Stream<wchar_t> path,
			CapacityStream<char>* error_message = nullptr
		);

#pragma endregion

#pragma region Error With Window Or Console

		ECSENGINE_API void LaunchFileExplorerWithError(containers::Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void LaunchFileExplorerWithError(containers::Stream<wchar_t> path, Console* console);

		// Char* or wchar_t*
		template<typename PointerType>
		ECSENGINE_API void GetFileTimesWithError(
			Stream<wchar_t> path,
			UISystem* ECS_RESTRICT system,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		// Char*, wchar_t* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			UISystem* ECS_RESTRICT system,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		// Char* or wchar_t*
		template<typename PointerType>
		ECSENGINE_API void GetFileTimesWithError(
			Stream<wchar_t> path,
			Console* ECS_RESTRICT console,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		// Char*, wchar_t* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			Console* ECS_RESTRICT console,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		ECSENGINE_API void ClearFileWithError(Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void ClearFileWithError(Stream<wchar_t> path, Console* console);

		ECSENGINE_API void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, UISystem* system);

		ECSENGINE_API void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, Console* console);

		ECSENGINE_API void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, UISystem* system);

		ECSENGINE_API void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, Console* console);

		ECSENGINE_API void CreateFolderWithError(Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void CreateFolderWithError(Stream<wchar_t> path, Console* console);

		ECSENGINE_API void DeleteFolderWithError(Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void DeleteFolderWithError(Stream<wchar_t> path, Console* console);

		ECSENGINE_API void DeleteFileWithError(Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void DeleteFileWithError(Stream<wchar_t> path, Console* console);

		ECSENGINE_API void RenameFolderWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, UISystem* system);

		ECSENGINE_API void RenameFolderWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, Console* console);

		ECSENGINE_API void RenameFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, UISystem* system);

		ECSENGINE_API void RenameFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, Console* console);

		ECSENGINE_API void ResizeFileWithError(Stream<wchar_t> path, size_t new_size, UISystem* system);

		ECSENGINE_API void ResizeFileWithError(Stream<wchar_t> path, size_t new_size, Console* console);

		ECSENGINE_API void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> extension, UISystem* system);

		ECSENGINE_API void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> extension, Console* console);

		ECSENGINE_API void ExistsFileOrFolderWithError(Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void ExistsFileOrFolderWithError(Stream<wchar_t> path, Console* console);

		ECSENGINE_API void DeleteFolderContentsWithError(Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void DeleteFolderContentsWithError(Stream<wchar_t> path, Console* console);

		ECSENGINE_API void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path, UISystem* system);

		ECSENGINE_API void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path, Console* console);

#pragma endregion

	}

}