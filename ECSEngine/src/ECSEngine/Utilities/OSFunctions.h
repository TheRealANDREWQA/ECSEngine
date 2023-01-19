#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"

// Forward the UISystem and Console
namespace ECSEngine {

	namespace Tools {
		struct UISystem;
	}
}

using namespace ECSEngine::Tools;

#define ECS_OS using namespace ECSEngine::OS

namespace ECSEngine {

	namespace OS {

#pragma region Basic APIs

		ECSENGINE_API bool LaunchFileExplorer(Stream<wchar_t> folder);
		
		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetFileTimesInternal(
			HANDLE file_handle, 
			char* creation_time = nullptr,
			char* access_time = nullptr,
			char* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetFileTimesInternal(
			HANDLE file_handle,
			wchar_t* creation_time = nullptr,
			wchar_t* access_time = nullptr,
			wchar_t* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetFileTimesInternal(
			HANDLE file_handle,
			size_t* creation_time = nullptr,
			size_t* access_time = nullptr,
			size_t* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimesInternal(
			HANDLE file_handle,
			char* creation_time = nullptr,
			char* access_time = nullptr,
			char* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimesInternal(
			HANDLE file_handle,
			wchar_t* creation_time = nullptr,
			wchar_t* access_time = nullptr,
			wchar_t* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimesInternal(
			HANDLE file_handle,
			size_t* creation_time = nullptr,
			size_t* access_time = nullptr,
			size_t* last_write_time = nullptr
		);

		// This is the absolute date (like dd/ww/yyyy)
		// Works on directories too
		ECSENGINE_API bool GetFileTimes(
			Stream<wchar_t> path,
			char* creation_time = nullptr,
			char* access_time = nullptr,
			char* last_write_time = nullptr
		);

		// This is the absolute date (like dd/ww/yyyy)
		// A pointer null means I don't care; returns whether or not succeeded
		// Works on directories too
		ECSENGINE_API bool GetFileTimes(
			Stream<wchar_t> path,
			wchar_t* creation_time = nullptr,
			wchar_t* access_time = nullptr,
			wchar_t* last_write_time = nullptr
		);

		// This is like an absolute date (like dd/ww/yyyy)
		// A pointer null means I don't care; returns whether or not it succeeded
		// Works on directories too; It will convert the dates into a number;
		ECSENGINE_API bool GetFileTimes(
			Stream<wchar_t> path,
			size_t* creation_time = nullptr,
			size_t* access_time = nullptr,
			size_t* last_write_time = nullptr
		);

		// This is relative in the sense that reports what is the difference between the time
		// of the file and the current system time (for example it reports that a file was created one month ago)
		// A pointer null means I don't care; returns whether or not succeeded; output is in milliseconds
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimes(
			Stream<wchar_t> path,
			size_t* creation_time = nullptr,
			size_t* access_time = nullptr,
			size_t* last_write_time = nullptr
		);

		// This is relative in the sense that reports what is the difference between the time
		// of the file and the current system time (for example it reports that a file was created one month ago)
		// A pointer null means I don't care; returns whether or not succeeded
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimes(
			Stream<wchar_t> path,
			char* creation_time = nullptr,
			char* access_time = nullptr,
			char* last_write_time = nullptr
		);

		// This is relative in the sense that reports what is the difference between the time
		// of the file and the current system time (for example it reports that a file was created one month ago)
		// A pointer null means I don't care; returns whether or not succeeded
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimes(
			Stream<wchar_t> path,
			wchar_t* creation_time = nullptr,
			wchar_t* access_time = nullptr,
			wchar_t* last_write_time = nullptr
		);

		ECSENGINE_API bool OpenFileWithDefaultApplication(
			Stream<wchar_t> path,
			CapacityStream<char>* error_message = nullptr
		);

		// Local time
		ECSENGINE_API Date GetLocalTime();

		ECSENGINE_API size_t GetFileLastWrite(Stream<wchar_t> path);

		ECSENGINE_API void InitializeSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths);

		ECSENGINE_API void SetSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths);

		// Assumes that InitializeSymbolicLinksPaths has been called
		ECSENGINE_API void GetCallStackFunctionNames(CapacityStream<char>& string);

		enum ECS_THREAD_PRIORITY : unsigned char {
			ECS_THREAD_PRIORITY_VERY_LOW,
			ECS_THREAD_PRIORITY_LOW,
			ECS_THREAD_PRIORITY_NORMAL,
			ECS_THREAD_PRIORITY_HIGH,
			ECS_THREAD_PRIORITY_VERY_HIGH
		};

		ECSENGINE_API void ChangeThreadPriority(ECS_THREAD_PRIORITY priority);

#pragma endregion

#pragma region Error With Window Or Console

		ECSENGINE_API void LaunchFileExplorerWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void LaunchFileExplorerWithError(Stream<wchar_t> path);

		// Char*, wchar_t* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetFileTimesWithError(
			Stream<wchar_t> path,
			UISystem* system,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		// Char*, wchar_t* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			UISystem* system,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		// Char*, wchar_t* or size_t*
		// Uses the console to print the message
		template<typename PointerType>
		ECSENGINE_API void GetFileTimesWithError(
			Stream<wchar_t> path,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		// Char*, wchar_t* or size_t*
		// Uses the console to print the message
		template<typename PointerType>
		ECSENGINE_API void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		ECSENGINE_API void ClearFileWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ClearFileWithError(Stream<wchar_t> path);

		ECSENGINE_API void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to);

		ECSENGINE_API void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to);

		ECSENGINE_API void CreateFolderWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void CreateFolderWithError(Stream<wchar_t> path);

		ECSENGINE_API void DeleteFolderWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void DeleteFolderWithError(Stream<wchar_t> path);

		ECSENGINE_API void DeleteFileWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void DeleteFileWithError(Stream<wchar_t> path);

		ECSENGINE_API void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name);

		ECSENGINE_API void ResizeFileWithError(Stream<wchar_t> path, size_t new_size, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ResizeFileWithError(Stream<wchar_t> path, size_t new_size);

		ECSENGINE_API void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> extension, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> extension);

		ECSENGINE_API void ExistsFileOrFolderWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ExistsFileOrFolderWithError(Stream<wchar_t> path);

		ECSENGINE_API void DeleteFolderContentsWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void DeleteFolderContentsWithError(Stream<wchar_t> path);

		ECSENGINE_API void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path, UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path);

		// The path must be initialized
		struct FileExplorerGetFileData {
			CapacityStream<wchar_t> path;
			CapacityStream<char> error_message = { nullptr, 0, 0 };
			Stream<Stream<wchar_t>> extensions = { nullptr, 0 };
			HWND hWnd = NULL;
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

#pragma endregion

	}

}