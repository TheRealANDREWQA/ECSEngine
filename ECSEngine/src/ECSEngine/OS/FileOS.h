#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace Tools {
		struct UISystem;
	}

	namespace OS {

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetFileTimesInternal(
			void* file_handle,
			CapacityStream<char>* creation_time = nullptr,
			CapacityStream<char>* access_time = nullptr,
			CapacityStream<char>* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetFileTimesInternal(
			void* file_handle,
			CapacityStream<wchar_t>* creation_time = nullptr,
			CapacityStream<wchar_t>* access_time = nullptr,
			CapacityStream<wchar_t>* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetFileTimesInternal(
			void* file_handle,
			size_t* creation_time = nullptr,
			size_t* access_time = nullptr,
			size_t* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimesInternal(
			void* file_handle,
			CapacityStream<char>* creation_time = nullptr,
			CapacityStream<char>* access_time = nullptr,
			CapacityStream<char>* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimesInternal(
			void* file_handle,
			CapacityStream<wchar_t>* creation_time = nullptr,
			CapacityStream<wchar_t>* access_time = nullptr,
			CapacityStream<wchar_t>* last_write_time = nullptr
		);

		// Should prefer the other variants. This can be used when getting OS handles for files
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimesInternal(
			void* file_handle,
			size_t* creation_time = nullptr,
			size_t* access_time = nullptr,
			size_t* last_write_time = nullptr
		);

		// This is the absolute date (like dd/ww/yyyy)
		// Works on directories too
		ECSENGINE_API bool GetFileTimes(
			Stream<wchar_t> path,
			CapacityStream<char>* creation_time = nullptr,
			CapacityStream<char>* access_time = nullptr,
			CapacityStream<char>* last_write_time = nullptr
		);

		// This is the absolute date (like dd/ww/yyyy)
		// A pointer null means I don't care; returns whether or not succeeded
		// Works on directories too
		ECSENGINE_API bool GetFileTimes(
			Stream<wchar_t> path,
			CapacityStream<wchar_t>* creation_time = nullptr,
			CapacityStream<wchar_t>* access_time = nullptr,
			CapacityStream<wchar_t>* last_write_time = nullptr
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
			CapacityStream<char>* creation_time = nullptr,
			CapacityStream<char>* access_time = nullptr,
			CapacityStream<char>* last_write_time = nullptr
		);

		// This is relative in the sense that reports what is the difference between the time
		// of the file and the current system time (for example it reports that a file was created one month ago)
		// A pointer null means I don't care; returns whether or not succeeded
		// Works on directories too
		ECSENGINE_API bool GetRelativeFileTimes(
			Stream<wchar_t> path,
			CapacityStream<wchar_t>* creation_time = nullptr,
			CapacityStream<wchar_t>* access_time = nullptr,
			CapacityStream<wchar_t>* last_write_time = nullptr
		);

		ECSENGINE_API bool OpenFileWithDefaultApplication(
			Stream<wchar_t> path,
			CapacityStream<char>* error_message = nullptr
		);

		ECSENGINE_API size_t GetFileLastWrite(Stream<wchar_t> path);

		// Capacity<char>*, Capacity<wchar_t>* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetFileTimesWithError(
			Stream<wchar_t> path,
			Tools::UISystem* system,
			PointerType* creation_time = nullptr,
			PointerType* access_time = nullptr,
			PointerType* last_write_time = nullptr
		);

		// Capacity<char>*, Capacity<wchar_t>* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			Tools::UISystem* system,
			PointerType* creation_time = nullptr,
			PointerType* access_time = nullptr,
			PointerType* last_write_time = nullptr
		);

		// Capacity<char>*, Capacity<wchar_t>* or size_t*
		// Uses the console to print the message
		template<typename PointerType>
		ECSENGINE_API void GetFileTimesWithError(
			Stream<wchar_t> path,
			PointerType* creation_time = nullptr,
			PointerType* access_time = nullptr,
			PointerType* last_write_time = nullptr
		);

		// Capacity<char>*, Capacity<wchar_t>* or size_t*
		// Uses the console to print the message
		template<typename PointerType>
		ECSENGINE_API void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			PointerType* creation_time = nullptr,
			PointerType* access_time = nullptr,
			PointerType* last_write_time = nullptr
		);

		ECSENGINE_API void ClearFileWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ClearFileWithError(Stream<wchar_t> path);

		ECSENGINE_API void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to);

		ECSENGINE_API void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to);

		ECSENGINE_API void CreateFolderWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void CreateFolderWithError(Stream<wchar_t> path);

		ECSENGINE_API void DeleteFolderWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void DeleteFolderWithError(Stream<wchar_t> path);

		ECSENGINE_API void DeleteFileWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void DeleteFileWithError(Stream<wchar_t> path);

		ECSENGINE_API void RenameFolderWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, Tools::UISystem* system);

		ECSENGINE_API void RenameFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void RenameFolderWithError(Stream<wchar_t> path, Stream<wchar_t> new_name);

		// Uses the console to print the message
		ECSENGINE_API void RenameFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name);

		ECSENGINE_API void ResizeFileWithError(Stream<wchar_t> path, size_t new_size, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ResizeFileWithError(Stream<wchar_t> path, size_t new_size);

		ECSENGINE_API void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> extension, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> extension);

		ECSENGINE_API void ExistsFileOrFolderWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void ExistsFileOrFolderWithError(Stream<wchar_t> path);

		ECSENGINE_API void DeleteFolderContentsWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void DeleteFolderContentsWithError(Stream<wchar_t> path);

		ECSENGINE_API void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path);

	}

}