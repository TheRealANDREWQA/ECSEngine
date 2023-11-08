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

		// Exists the current thread
		ECSENGINE_API void ExitThread(int error_code);

		// Returns true if it succeeded, else false
		ECSENGINE_API bool SuspendThread(void* handle);

		ECSENGINE_API bool ResumeThread(void* handle);

		// Returns nullptr if it failed, else the handle value. This handle value must be
		// Closed when you are finished using it
		ECSENGINE_API void* GetCurrentThreadHandle();

		// This function is given primarly to be used with GetCurrentThreadHandle
		// To release the handle after it has been used
		ECSENGINE_API void CloseThreadHandle(void* handle);
		
		// It creates a new separate handle from the given one. You must release it with CloseThreadHandle
		// Returns nullptr if it failed
		ECSENGINE_API void* DuplicateThreadHandle(void* handle);

		/*
			To determine if two threads are the same, don't use handles.
			Instead use thread IDs since these are agnostic while there
			Can be multiple thread handles for the same thread and they will
			Never be the same
		*/

		// Returns an OS ID for the current thread
		ECSENGINE_API size_t GetCurrentThreadID();

		// Returns an OS ID for that thread handle. Returns -1 if an error has occured
		ECSENGINE_API size_t GetThreadID(void* handle);

		// This struct is used to wrapp the largest OS context platform
		// We do not attempt to let the user do anything with it, just passed
		// To other OS functions
		struct ECSENGINE_API ThreadContext {
			ThreadContext();

			// Sets up anything that is necessary
			void Initialize();

			char bytes[1300];
		};

		// Returns true if it succeeded, else false. In order to have consistent behaviour,
		// Suspend the thread before capturing its context
		ECSENGINE_API bool CaptureThreadStackContext(void* thread_handle, ThreadContext* thread_context);

		// Returns true if it succeeded, else false
		ECSENGINE_API bool CaptureCurrentThreadStackContext(ThreadContext* thread_context);

		ECSENGINE_API void InitializeSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths);

		ECSENGINE_API void SetSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths);

		// Looks at the loaded modules by the process and loads the symbol information
		// For them such that stack unwinding can recognize the symbols correctly
		ECSENGINE_API void RefreshModuleSymbols();

		// Assumes that InitializeSymbolicLinksPaths has been called
		ECSENGINE_API void GetCallStackFunctionNames(CapacityStream<char>& string);

		// Assumes that InitializeSymbolicLinksPaths has been called
		// The thread should be suspended before getting its context
		ECSENGINE_API void GetCallStackFunctionNames(ThreadContext* context, void* thread_handle, CapacityStream<char>& string);

		// Assumes that InitializeSymbolicLinksPaths has been called
		// The thread should be suspended before getting its context
		// Returns true if it managed to get the stack frame, else false
		ECSENGINE_API bool GetCallStackFunctionNames(void* thread_handle, CapacityStream<char>& string);

		enum ECS_THREAD_PRIORITY : unsigned char {
			ECS_THREAD_PRIORITY_VERY_LOW,
			ECS_THREAD_PRIORITY_LOW,
			ECS_THREAD_PRIORITY_NORMAL,
			ECS_THREAD_PRIORITY_HIGH,
			ECS_THREAD_PRIORITY_VERY_HIGH
		};

		ECSENGINE_API void ChangeThreadPriority(ECS_THREAD_PRIORITY priority);

		ECSENGINE_API void ChangeThreadPriority(void* thread_handle, ECS_THREAD_PRIORITY priority);

		ECSENGINE_API uint2 GetCursorPosition(void* window_handle);

		ECSENGINE_API void SetCursorPosition(void* window_handle, uint2 position);

		// The cursor is placed relative to the OS client region
		// Returns the final position relative to the window location
		ECSENGINE_API uint2 SetCursorPositionRelative(void* window_handle, uint2 position);

		ECSENGINE_API uint2 GetOSWindowPosition(void* window_handle);

		// Returns an allocation from the system virtual allocation where you can specify if the memory is
		// to be commited or not
		ECSENGINE_API void* VirtualAllocation(size_t size);

		// The pointer must be obtained from VirtualAllocation
		ECSENGINE_API void VirtualDeallocation(void* block);

		// Returns true if it succeeded, else false. The page enter read-write mode
		ECSENGINE_API bool DisableVirtualWriteProtection(void* allocation);

		// Returns true if it succeeded, else false. The page enter read only mode
		ECSENGINE_API bool EnableVirtualWriteProtection(void* allocation);

		ECSENGINE_API void OSMessageBox(const char* message, const char* title);

#pragma endregion

#pragma region Error With Window Or Console

		ECSENGINE_API void LaunchFileExplorerWithError(Stream<wchar_t> path, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void LaunchFileExplorerWithError(Stream<wchar_t> path);

		// Char*, wchar_t* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetFileTimesWithError(
			Stream<wchar_t> path,
			Tools::UISystem* system,
			PointerType* ECS_RESTRICT creation_time = nullptr,
			PointerType* ECS_RESTRICT access_time = nullptr,
			PointerType* ECS_RESTRICT last_write_time = nullptr
		);

		// Char*, wchar_t* or size_t*
		template<typename PointerType>
		ECSENGINE_API void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			Tools::UISystem* system,
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

		ECSENGINE_API void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, Tools::UISystem* system);

		// Uses the console to print the message
		ECSENGINE_API void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name);

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