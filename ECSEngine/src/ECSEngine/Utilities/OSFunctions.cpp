#include "ecspch.h"
#include "OSFunctions.h"
#include "Function.h"
#include "../Tools/UI/UIDrawerWindows.h"
#include "File.h"
#include "Path.h"

#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

bool SYM_INITIALIZED = false;

ECS_TOOLS;

namespace ECSEngine {

	namespace OS {

#pragma region Basic APIs

		bool LaunchFileExplorer(const wchar_t* folder) {
			return LaunchFileExplorer(Stream<wchar_t>(folder, wcslen(folder)));
		}

		// -----------------------------------------------------------------------------------------------------

		bool LaunchFileExplorer(Stream<wchar_t> folder) {
			wchar_t temp_characters[1024];
			CapacityStream<wchar_t> temp_stream(temp_characters, 0, 1024);
			temp_stream.AddStream(Stream<wchar_t>(L"explorer.exe /select,\"", std::size(L"explorer.exe /select,\"") - 1));
			temp_stream.AddStream(folder);
			temp_stream.Add(L'\"');
			temp_stream.AddSafe(L'\0');

			STARTUPINFO startup_info;
			memset(&startup_info, 0, sizeof(startup_info));
			startup_info.cb = sizeof(startup_info);

			PROCESS_INFORMATION process_information;
			memset(&process_information, 0, sizeof(process_information));

			bool value = CreateProcess(nullptr, temp_stream.buffer, nullptr, nullptr, false, 0, nullptr, nullptr, &startup_info, &process_information);
			DWORD error;
			if (!value) {
				error = GetLastError();
			}
			return value;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimesInternal(const wchar_t* ECS_RESTRICT path, FILETIME* filetime_creation, FILETIME* filetime_last_access, FILETIME* filetime_last_write) {
			// Determine whether or not it is a file or directory
			Stream<wchar_t> stream_path = ToStream(path);
			HANDLE handle = INVALID_HANDLE_VALUE;
			if (function::PathExtensionSize(stream_path) == 0) {
				// Open a handle to a directory
				handle = CreateFile(path, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
			}
			else {
				// Open a handle to the file
				handle = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			}
			
			DWORD error = GetLastError();
			if (handle == INVALID_HANDLE_VALUE) {
				return false;
			}
			else {
				// if getting the handle was succesful, get file times
				BOOL success = GetFileTime(handle, filetime_creation, filetime_last_access, filetime_last_write);
				
				CloseHandle(handle);
				if (!success) {
					return false;
				}
				return true;
			}
		}

		// -----------------------------------------------------------------------------------------------------

		void ConvertSystemTimeToDate(const SYSTEMTIME* system_time, char* characters) {
			Stream<char> stream(characters, 0);
			function::ConvertIntToChars(stream, system_time->wDay);
			stream.Add('/');
			function::ConvertIntToChars(stream, system_time->wMonth);
			stream.Add('/');
			function::ConvertIntToChars(stream, system_time->wYear);
			stream.Add(' ');
			function::ConvertIntToChars(stream, system_time->wHour);
			stream.Add(':');
			function::ConvertIntToChars(stream, system_time->wMinute);
			stream.Add(':');
			function::ConvertIntToChars(stream, system_time->wSecond);
			stream[stream.size] = '\0';
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(
			const wchar_t* ECS_RESTRICT path,
			char* ECS_RESTRICT creation_time,
			char* ECS_RESTRICT access_time,
			char* ECS_RESTRICT last_write_time
		) {
			FILETIME os_creation_time, os_access_time, os_last_write_time;
			bool success = GetFileTimesInternal(path, &os_creation_time, &os_access_time, &os_last_write_time);
			
			if (success) {
				SYSTEMTIME sys_creation_time, sys_access_time, sys_last_write_time;
				if (creation_time != nullptr) {
					SYSTEMTIME local_creation_time;
					success &= FileTimeToSystemTime(&os_creation_time, &sys_creation_time);
					success &= SystemTimeToTzSpecificLocalTime(nullptr, &sys_creation_time, &local_creation_time);
					if (success) {
						ConvertSystemTimeToDate(&local_creation_time, creation_time);
					}
				}
				if (access_time != nullptr && success) {
					SYSTEMTIME local_access_time;
					success &= FileTimeToSystemTime(&os_access_time, &sys_access_time);
					success &= SystemTimeToTzSpecificLocalTime(nullptr, &sys_access_time, &local_access_time);
					if (success) {
						ConvertSystemTimeToDate(&local_access_time, access_time);
					}
				}
				if (last_write_time != nullptr && success) {
					SYSTEMTIME local_write_time;
					success &= FileTimeToSystemTime(&os_last_write_time, &sys_last_write_time);
					success &= SystemTimeToTzSpecificLocalTime(nullptr, &sys_last_write_time, &local_write_time);
					if (success) {
						ConvertSystemTimeToDate(&local_write_time, last_write_time);
					}
				}
				return success;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(
			const wchar_t* ECS_RESTRICT path,
			wchar_t* ECS_RESTRICT creation_time,
			wchar_t* ECS_RESTRICT access_time,
			wchar_t* ECS_RESTRICT last_write_time
		) {
			char _creation_time[256];
			char _access_time[256];
			char _last_write_time[256];

			char* ptr1 = (char*)function::Select<uintptr_t>(creation_time == nullptr, 0, (uintptr_t)_creation_time);
			char* ptr2 = (char*)function::Select<uintptr_t>(access_time == nullptr, 0, (uintptr_t)_access_time);
			char* ptr3 = (char*)function::Select<uintptr_t>(last_write_time == nullptr, 0, (uintptr_t)_last_write_time);

			bool success = GetFileTimes(path, ptr1, ptr2, ptr3);
			if (success) {
				if (ptr1 != nullptr) {
					function::ConvertASCIIToWide(creation_time, ptr1, 256);
				}
				if (ptr2 != nullptr) {
					function::ConvertASCIIToWide(access_time, ptr2, 256);
				}
				if (ptr3 != nullptr) {
					function::ConvertASCIIToWide(last_write_time, ptr3, 256);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(const wchar_t* ECS_RESTRICT path, size_t* ECS_RESTRICT creation_time, size_t* ECS_RESTRICT access_time, size_t* ECS_RESTRICT last_write_time)
		{
			FILETIME os_creation_time, os_access_time, os_last_write_time;
			bool success = GetFileTimesInternal(path, &os_creation_time, &os_access_time, &os_last_write_time);

			if (success) {
				// use as per MSDN ULARGE_INTEGER in order to convert to normal integers and perform substraction
				ULARGE_INTEGER large_integer;
				if (creation_time != nullptr) {
					large_integer.LowPart = os_creation_time.dwLowDateTime;
					large_integer.HighPart = os_creation_time.dwHighDateTime;
					uint64_t value = large_integer.QuadPart;

					// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
					// Divide by 10'000 to get milliseconds
					*creation_time = value / 10'000;
				}

				if (access_time != nullptr) {
					large_integer.LowPart = os_access_time.dwLowDateTime;
					large_integer.HighPart = os_access_time.dwHighDateTime;
					uint64_t value = large_integer.QuadPart;

					// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
					// Divide by 10'000 to get milliseconds
					*access_time = value / 10'000;
				}

				if (last_write_time != nullptr) {
					large_integer.LowPart = os_last_write_time.dwLowDateTime;
					large_integer.HighPart = os_last_write_time.dwHighDateTime;
					uint64_t value = large_integer.QuadPart;

					// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
					// Divide by 10'000 to get milliseconds
					*last_write_time = value / 10'000;
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(Stream<wchar_t> path, char* ECS_RESTRICT creation_time, char* ECS_RESTRICT access_time, char* ECS_RESTRICT last_write_time)
		{
			ECS_FORWARD_STREAM_WIDE(path, GetFileTimes, creation_time, access_time, last_write_time);
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(Stream<wchar_t> path, wchar_t* ECS_RESTRICT creation_time, wchar_t* ECS_RESTRICT access_time, wchar_t* ECS_RESTRICT last_write_time)
		{
			ECS_FORWARD_STREAM_WIDE(path, GetFileTimes, creation_time, access_time, last_write_time);
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(Stream<wchar_t> path, size_t* ECS_RESTRICT creation_time, size_t* ECS_RESTRICT access_time, size_t* ECS_RESTRICT last_write_time)
		{
			ECS_FORWARD_STREAM_WIDE(path, GetFileTimes, creation_time, access_time, last_write_time);
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimes(
			const wchar_t* ECS_RESTRICT path,
			size_t* ECS_RESTRICT creation_time,
			size_t* ECS_RESTRICT access_time,
			size_t* ECS_RESTRICT last_write_time
		)
		{
			FILETIME os_creation_time, os_access_time, os_last_write_time;
			bool success = GetFileTimesInternal(path, &os_creation_time, &os_access_time, &os_last_write_time);

			if (success) {
				SYSTEMTIME system_time;
				GetSystemTime(&system_time);

				FILETIME converted_time;
				success = SystemTimeToFileTime(&system_time, &converted_time);

				if (success) {
					// use as per MSDN ULARGE_INTEGER in order to convert to normal integers and perform substraction
					ULARGE_INTEGER large_integer;
					large_integer.LowPart = converted_time.dwLowDateTime;
					large_integer.HighPart = converted_time.dwHighDateTime;
					uint64_t system_value = large_integer.QuadPart;

					if (creation_time != nullptr) {
						large_integer.LowPart = os_creation_time.dwLowDateTime;
						large_integer.HighPart = os_creation_time.dwHighDateTime;
						uint64_t second_value = large_integer.QuadPart;

						// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
						// Divide by 10'000 to get milliseconds
						*creation_time = (system_value - second_value) / 10'000;
					}

					if (access_time != nullptr) {
						large_integer.LowPart = os_access_time.dwLowDateTime;
						large_integer.HighPart = os_access_time.dwHighDateTime;
						uint64_t second_value = large_integer.QuadPart;

						// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
						// Divide by 10'000 to get milliseconds
						*access_time = (system_value - second_value) / 10'000;
					}

					if (last_write_time != nullptr) {
						large_integer.LowPart = os_last_write_time.dwLowDateTime;
						large_integer.HighPart = os_last_write_time.dwHighDateTime;
						uint64_t second_value = large_integer.QuadPart;

						// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
						// Divide by 10'000 to get milliseconds
						*last_write_time = (system_value - second_value) / 10'000;
					}
					return true;
				}
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimes(
			const wchar_t* ECS_RESTRICT path,
			char* ECS_RESTRICT creation_time,
			char* ECS_RESTRICT access_time,
			char* ECS_RESTRICT last_write_time
		)
		{
			size_t creation_time_int, access_time_int, last_write_time_int;
			size_t* ptr1 = nullptr;
			size_t* ptr2 = nullptr;
			size_t* ptr3 = nullptr;

			ptr1 = (size_t*)function::Select<uintptr_t>(creation_time != nullptr, (uintptr_t)&creation_time_int, (uintptr_t)nullptr);
			ptr2 = (size_t*)function::Select<uintptr_t>(access_time != nullptr, (uintptr_t)&access_time_int, (uintptr_t)nullptr);
			ptr3 = (size_t*)function::Select<uintptr_t>(last_write_time != nullptr, (uintptr_t)&last_write_time_int, (uintptr_t)nullptr);

			bool success = GetRelativeFileTimes(path, ptr1, ptr2, ptr3);
			if (success) {
				if (ptr1 != nullptr) {
					function::ConvertDurationToChars(creation_time_int, creation_time);
				}
				if (ptr2 != nullptr) {
					function::ConvertDurationToChars(access_time_int, access_time);
				}
				if (ptr3 != nullptr) {
					function::ConvertDurationToChars(last_write_time_int, last_write_time);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimes(
			const wchar_t* ECS_RESTRICT path,
			wchar_t* ECS_RESTRICT creation_time,
			wchar_t* ECS_RESTRICT access_time,
			wchar_t* ECS_RESTRICT last_write_time
		)
		{
			char temp_characters1[256];
			char temp_characters2[256];
			char temp_characters3[256];

			char* ptr1 = (char*)function::Select<uintptr_t>(creation_time != nullptr, (uintptr_t)temp_characters1, (uintptr_t)nullptr);
			char* ptr2 = (char*)function::Select<uintptr_t>(access_time != nullptr, (uintptr_t)temp_characters2, (uintptr_t)nullptr);
			char* ptr3 = (char*)function::Select<uintptr_t>(last_write_time != nullptr, (uintptr_t)temp_characters3, (uintptr_t)nullptr);
			bool success = GetRelativeFileTimes(path, ptr1, ptr2, ptr3);

			if (success) {
				if (ptr1 != nullptr) {
					function::ConvertASCIIToWide(creation_time, temp_characters1, 256);
				}
				if (ptr2 != nullptr) {
					function::ConvertASCIIToWide(access_time, temp_characters2, 256);
				}
				if (ptr3 != nullptr) {
					function::ConvertASCIIToWide(last_write_time, temp_characters3, 256);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimes(Stream<wchar_t> path, size_t* ECS_RESTRICT creation_time, size_t* ECS_RESTRICT access_time, size_t* ECS_RESTRICT last_write_time)
		{
			ECS_FORWARD_STREAM_WIDE(path, GetRelativeFileTimes, creation_time, access_time, last_write_time);
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimes(Stream<wchar_t> path, char* ECS_RESTRICT creation_time, char* ECS_RESTRICT access_time, char* ECS_RESTRICT last_write_time)
		{
			ECS_FORWARD_STREAM_WIDE(path, GetRelativeFileTimes, creation_time, access_time, last_write_time);
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimes(Stream<wchar_t> path, wchar_t* ECS_RESTRICT creation_time, wchar_t* ECS_RESTRICT access_time, wchar_t* ECS_RESTRICT last_write_time)
		{
			ECS_FORWARD_STREAM_WIDE(path, GetRelativeFileTimes, creation_time, access_time, last_write_time);
		}

		// -----------------------------------------------------------------------------------------------------

		bool OpenFileWithDefaultApplication(const wchar_t* path, CapacityStream<char>* error_message)
		{
			HINSTANCE instance = ShellExecute(NULL, NULL, path, NULL, NULL, SW_SHOW);
			size_t code = (size_t)instance;
			if (code > 32) {
				return true;
			}

			if (error_message != nullptr) {
				const char* error_code = nullptr;
				switch (code) {
				case SE_ERR_FNF:
					error_code = "The specified file was not found.";
					break;
				case ERROR_BAD_FORMAT:
					error_code = "The .exe file is invalid (non-Win32 .exe or error in .exe image).";
					break;
				case SE_ERR_ACCESSDENIED:
					error_code = "The operating system denied access to the specified file.";
					break;
				case SE_ERR_ASSOCINCOMPLETE:
					error_code = "The file name association is incomplete or invalid.";
					break;
				case SE_ERR_DDEBUSY:
					error_code = "The DDE transaction could not be completed because other DDE transactions were being processed.";
					break;
				case SE_ERR_DDEFAIL:
					error_code = "The DDE transaction failed.";
					break;
				case SE_ERR_DDETIMEOUT:
					error_code = "The DDE transaction could not be completed because the request timed out.";
					break;
				case SE_ERR_DLLNOTFOUND:
					error_code = "The specified DLL was not found.";
					break;
				case SE_ERR_NOASSOC:
					error_code = "There is no application associated with the given file name extension. This error will also be returned if you attempt to print a file that is not printable.";
					break;
				case SE_ERR_OOM:
					error_code = "There was not enough memory to complete the operation.";
					break;
				case SE_ERR_SHARE:
					error_code = "A sharing violation occurred.";
					break;
				}
				error_message->AddStreamSafe(ToStream(error_code));
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool OpenFileWithDefaultApplication(
			Stream<wchar_t> path,
			CapacityStream<char>* error_message
		) {
			if (path.size > 512) {
				if (error_message != nullptr) {
					error_message->Copy(ToStream("Path is too big to copy to a stack buffer"));
				}
				return false;
			}
			wchar_t temp_path[512];
			path.CopyTo(temp_path);
			temp_path[path.size] = L'\0';
			return OpenFileWithDefaultApplication(temp_path, error_message);
		}

		// -----------------------------------------------------------------------------------------------------

		Date GetLocalTime()
		{
			Date date;

			SYSTEMTIME time;
			GetLocalTime(&time);

			date.year = time.wYear;
			date.month = time.wMonth;
			date.day = time.wDay;
			date.hour = time.wHour;
			date.minute = time.wMinute;
			date.seconds = time.wSecond;
			date.milliseconds = time.wMilliseconds;

			return date;
		}

		// -----------------------------------------------------------------------------------------------------

		size_t GetFileLastWrite(const wchar_t* path)
		{
			size_t last_write = 0;
			GetFileTimes(path, nullptr, nullptr, &last_write);
			return last_write;
		}

		// -----------------------------------------------------------------------------------------------------

		size_t GetFileLastWrite(Stream<wchar_t> path)
		{
			if (path[path.size] == L'\0') {
				return GetFileLastWrite(path.buffer);
			}
			else {
				ECS_ASSERT(path.size < 1024);
				wchar_t* null_terminated_path = (wchar_t*)ECS_STACK_ALLOC(sizeof(wchar_t) * (path.size + 1));
				path.CopyTo(null_terminated_path);
				null_terminated_path[path.size] = L'\0';
				return GetFileLastWrite(null_terminated_path);
			}
		}

		// -----------------------------------------------------------------------------------------------------

		void InitializeSymbolicLinksPaths(Stream<wchar_t*> module_paths)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, search_paths, ECS_KB * 8);
			for (size_t index = 0; index < module_paths.size; index++) {
				search_paths.AddStream(ToStream(module_paths[index]));
				search_paths.AddSafe(L':');
			}
			search_paths[search_paths.size - 1] = L'\0';

			bool success = false;
			if (!SYM_INITIALIZED) {
				success = SymInitializeW(GetCurrentProcess(), search_paths.buffer, true);
				SYM_INITIALIZED = true;
			}
			else {
				success = SymSetSearchPathW(GetCurrentProcess(), search_paths.buffer);
			}

			ECS_ASSERT(success, "Initializing symbolic link paths failed.");
		}

		// -----------------------------------------------------------------------------------------------------

		void SetSymbolicLinksPaths(Stream<wchar_t*> module_paths)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, search_paths, ECS_KB * 16);
			for (size_t index = 0; index < module_paths.size; index++) {
				search_paths.AddStream(ToStream(module_paths[index]));
				search_paths.AddSafe(L':');
			}
			search_paths[search_paths.size - 1] = L'\0';

			bool success = SymSetSearchPathW(GetCurrentProcess(), search_paths.buffer);
			ECS_ASSERT(success, "Setting symbolic link paths failed.");
		}

		// -----------------------------------------------------------------------------------------------------

		void GetCallStackFunctionNames(CapacityStream<char>& string)
		{
			bool success;
			STACKFRAME64 stack_frame;
			memset(&stack_frame, 0, sizeof(stack_frame));

			CONTEXT context;
			RtlCaptureContext(&context);

			stack_frame.AddrPC.Offset = context.Rip;
			stack_frame.AddrPC.Mode = AddrModeFlat;
			stack_frame.AddrStack.Offset = context.Rsp;
			stack_frame.AddrStack.Mode = AddrModeFlat;
			stack_frame.AddrFrame.Offset = context.Rsp;
			stack_frame.AddrFrame.Mode = AddrModeFlat;
			IMAGEHLP_SYMBOL64* image_symbol = (IMAGEHLP_SYMBOL64*)ECS_STACK_ALLOC(sizeof(IMAGEHLP_SYMBOL64) + 512);
			image_symbol->MaxNameLength = 511;
			image_symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);

			HANDLE process_handle = GetCurrentProcess();

			size_t displacement = 0;
			string.AddStreamSafe(ToStream("Stack trace:\n"));
			while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, process_handle, GetCurrentThread(), &stack_frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
				success = SymGetSymFromAddr64(process_handle, (size_t)stack_frame.AddrPC.Offset, &displacement, image_symbol);
				DWORD characters_written = UnDecorateSymbolName(image_symbol->Name, string.buffer + string.size, string.capacity - string.size, UNDNAME_COMPLETE);
				string.size += characters_written;
				string.Add('\n');
			}

			string.AssertCapacity();
			string[string.size] = '\0';
		}

		// -----------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Error With Window Or Console

		using FolderFunction = bool (*)(Stream<wchar_t> path);

		// -----------------------------------------------------------------------------------------------------

		void ErrorWindow(Stream<wchar_t> path, FolderFunction function, const char* error_string, UISystem* system) {
			bool success = function(path);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, error_string, path);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void ErrorConsole(Stream<wchar_t> path, FolderFunction function, const char* error_string) {
			bool success = function(path);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, error_string, path);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define LAUNCH_FILE_EXPLORER_ERROR_STRING "Launching file explorer at {#} failed. Incorrect path."

		// -----------------------------------------------------------------------------------------------------

		void LaunchFileExplorerWithError(Stream<wchar_t> path, UISystem* system)
		{
			ErrorWindow(path, OS::LaunchFileExplorer, LAUNCH_FILE_EXPLORER_ERROR_STRING, system);
		}

		void LaunchFileExplorerWithError(Stream<wchar_t> path) {
			ErrorConsole(path, OS::LaunchFileExplorer, LAUNCH_FILE_EXPLORER_ERROR_STRING);
		}

		// -----------------------------------------------------------------------------------------------------

#define GET_FILE_TIMES_ERROR_STRING "Getting file {#} times failed!"

		// -----------------------------------------------------------------------------------------------------

		template<typename PointerType>
		void GetFileTimesWithError(
			Stream<wchar_t> path,
			UISystem* system,
			PointerType* ECS_RESTRICT creation_time,
			PointerType* ECS_RESTRICT access_time,
			PointerType* ECS_RESTRICT last_write_time
		)
		{
			wchar_t temp_wide_characters[256];
			ECS_ASSERT(path.size < 256);
			path.CopyTo(temp_wide_characters);
			temp_wide_characters[path.size] = L'\0';

			bool success = GetFileTimes(temp_wide_characters, creation_time, access_time, last_write_time);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		template<typename PointerType>
		void GetRelativeFileTimesWithError(
			Stream<wchar_t> path, 
			UISystem* system, 
			PointerType* ECS_RESTRICT creation_time,
			PointerType* ECS_RESTRICT access_time, 
			PointerType* ECS_RESTRICT last_write_time
		)
		{
			wchar_t temp_wide_characters[256];
			ECS_ASSERT(path.size < 256);
			path.CopyTo(temp_wide_characters);
			temp_wide_characters[path.size] = L'\0';

			bool success = GetRelativeFileTimes(temp_wide_characters, creation_time, access_time, last_write_time);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		template ECSENGINE_API void GetFileTimesWithError(Stream<wchar_t>, UISystem*, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT);
		template ECSENGINE_API void GetFileTimesWithError(Stream<wchar_t>, UISystem*, char* ECS_RESTRICT, char* ECS_RESTRICT, char* ECS_RESTRICT);
		template ECSENGINE_API void GetFileTimesWithError(Stream<wchar_t>, UISystem*, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT);

		template ECSENGINE_API void GetRelativeFileTimesWithError(Stream<wchar_t>, UISystem*, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT);
		template ECSENGINE_API void GetRelativeFileTimesWithError(Stream<wchar_t>, UISystem*, char* ECS_RESTRICT, char* ECS_RESTRICT, char* ECS_RESTRICT);
		template ECSENGINE_API void GetRelativeFileTimesWithError(Stream<wchar_t>, UISystem*, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT);

		// -----------------------------------------------------------------------------------------------------

		template<typename PointerType>
		void GetFileTimesWithError(
			Stream<wchar_t> path,
			PointerType* ECS_RESTRICT creation_time,
			PointerType* ECS_RESTRICT access_time,
			PointerType* ECS_RESTRICT last_write_time
		)
		{
			wchar_t temp_wide_characters[256];
			ECS_ASSERT(path.size < 256);
			path.CopyTo(temp_wide_characters);
			temp_wide_characters[path.size] = L'\0';

			bool success = GetFileTimes(temp_wide_characters, creation_time, access_time, last_write_time);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		template ECSENGINE_API void GetFileTimesWithError(Stream<wchar_t>, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT);
		template ECSENGINE_API void GetFileTimesWithError(Stream<wchar_t>, char* ECS_RESTRICT, char* ECS_RESTRICT, char* ECS_RESTRICT);
		template ECSENGINE_API void GetFileTimesWithError(Stream<wchar_t>, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT);

		// -----------------------------------------------------------------------------------------------------

		template<typename PointerType>
		void GetRelativeFileTimesWithError(
			Stream<wchar_t> path,
			PointerType* ECS_RESTRICT creation_time,
			PointerType* ECS_RESTRICT access_time,
			PointerType* ECS_RESTRICT last_write_time
		)
		{
			wchar_t temp_wide_characters[256];
			ECS_ASSERT(path.size < 256);
			path.CopyTo(temp_wide_characters);
			temp_wide_characters[path.size] = L'\0';

			bool success = GetRelativeFileTimes(temp_wide_characters, creation_time, access_time, last_write_time);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		template ECSENGINE_API void GetRelativeFileTimesWithError(Stream<wchar_t>, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT, wchar_t* ECS_RESTRICT);
		template ECSENGINE_API void GetRelativeFileTimesWithError(Stream<wchar_t>, char* ECS_RESTRICT, char* ECS_RESTRICT, char* ECS_RESTRICT);
		template ECSENGINE_API void GetRelativeFileTimesWithError(Stream<wchar_t>, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT, size_t* ECS_RESTRICT);

		// -----------------------------------------------------------------------------------------------------

#define CLEAR_FILE_ERROR_STRING "Clearing file {#} failed. Incorrect path or access denied."

		// -----------------------------------------------------------------------------------------------------

		void ClearFileWithError(Stream<wchar_t> path, UISystem* system)
		{
			ErrorWindow(path, ClearFile, CLEAR_FILE_ERROR_STRING, system);
		}

		void ClearFileWithError(Stream<wchar_t> path) {
			ErrorConsole(path, ClearFile, CLEAR_FILE_ERROR_STRING);
		}

		// -----------------------------------------------------------------------------------------------------

#define FILE_COPY_ERROR_STRING "Copying file {#} to {#} failed. Make sure that both file exist."

		// -----------------------------------------------------------------------------------------------------

		void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, UISystem* system) {
			bool success = FileCopy(from, to);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, FILE_COPY_ERROR_STRING, from, to);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to) {
			bool success = FileCopy(from, to);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, FILE_COPY_ERROR_STRING, from, to);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define FOLDER_COPY_ERROR_STRING "Copying folder {#} to {#} failed. Make sure that both folders exist."

		// -----------------------------------------------------------------------------------------------------

		void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, UISystem* system) {
			bool success = FolderCopy(from, to);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, FOLDER_COPY_ERROR_STRING, from, to);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to) {
			bool success = FolderCopy(from, to);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, FOLDER_COPY_ERROR_STRING, from, to);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define CREATE_FOLDER_ERROR_STRING "Creating folder {#} failed. Incorrect path, access denied or folder already exists."

		// -----------------------------------------------------------------------------------------------------

		void CreateFolderWithError(Stream<wchar_t> path, UISystem* system)
		{
			ErrorWindow(path, CreateFolder, CREATE_FOLDER_ERROR_STRING, system);
		}

		void CreateFolderWithError(Stream<wchar_t> path) {
			ErrorConsole(path, CreateFolder, CREATE_FOLDER_ERROR_STRING);
		}

		// -----------------------------------------------------------------------------------------------------

#define DELETE_FOLDER_ERROR_STRING "Deleting folder {#} failed. Incorrect path, access denied or folder doesn't exist."

		// -----------------------------------------------------------------------------------------------------

		void DeleteFolderWithError(Stream<wchar_t> path, UISystem* system)
		{
			ErrorWindow(path, RemoveFolder, DELETE_FOLDER_ERROR_STRING, system);
		}

		void DeleteFolderWithError(Stream<wchar_t> path) {
			ErrorConsole(path, RemoveFolder, DELETE_FOLDER_ERROR_STRING);
		}

		// -----------------------------------------------------------------------------------------------------

#define DELETE_FILE_ERROR_STRING "Deleting file {#} failed. Incorrect path, access denied or file doesn't exist."

		// -----------------------------------------------------------------------------------------------------

		void DeleteFileWithError(Stream<wchar_t> path, UISystem* system) {
			ErrorWindow(path, RemoveFile, DELETE_FILE_ERROR_STRING, system);
		}

		void DeleteFileWithError(Stream<wchar_t> path) {
			ErrorConsole(path, RemoveFile, DELETE_FILE_ERROR_STRING);
		}

		// -----------------------------------------------------------------------------------------------------

#define RENAME_FOLDER_ERROR_STRING "Renaming file/folder {#} to {#} failed. Incorrect path, invalid new name or access denied."

		// -----------------------------------------------------------------------------------------------------

		void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, UISystem* system) {
			bool success = RenameFolderOrFile(path, new_name);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, RENAME_FOLDER_ERROR_STRING, path, new_name);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name) {
			bool success = RenameFolderOrFile(path, new_name);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, RENAME_FOLDER_ERROR_STRING, path, new_name);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define RESIZE_FILE_ERROR_STRING "Resizing file {#} to {#} size failed. Incorrect path or access denied"

		// -----------------------------------------------------------------------------------------------------

		void ResizeFileWithError(Stream<wchar_t> path, size_t new_size, UISystem* system) {
			bool success = ResizeFile(path, new_size);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, RESIZE_FILE_ERROR_STRING, path, new_size);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void ResizeFileWithError(Stream<wchar_t> path, size_t new_size) {
			bool success = ResizeFile(path, new_size);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, RESIZE_FILE_ERROR_STRING, path, new_size);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define CHANGE_FILE_EXTENSION_ERROR_STRING "Changing file {#} extension to {#} failed. Incorrect file, invalid extension or access denied."

		// -----------------------------------------------------------------------------------------------------

		void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> new_extension, UISystem* system) {
			bool success = ChangeFileExtension(path, new_extension);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, CHANGE_FILE_EXTENSION_ERROR_STRING, path, new_extension);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> new_extension) {
			bool success = ChangeFileExtension(path, new_extension);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = function::FormatString(temp_characters, CHANGE_FILE_EXTENSION_ERROR_STRING, path, new_extension);
				ECS_ASSERT(written_characters < 512);
				GetConsole()->Error(Stream<char>(temp_characters, written_characters));
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define EXISTS_FILE_OR_FOLDER_ERROR_STRING "File or folder {#} does not exists."

		// -----------------------------------------------------------------------------------------------------

		void ExistsFileOrFolderWithError(Stream<wchar_t> path, UISystem* system)
		{
			ErrorWindow(path, ExistsFileOrFolder, EXISTS_FILE_OR_FOLDER_ERROR_STRING, system);
		}

		void ExistsFileOrFolderWithError(Stream<wchar_t> path)
		{
			ErrorConsole(path, ExistsFileOrFolder, EXISTS_FILE_OR_FOLDER_ERROR_STRING);
		}

		// -----------------------------------------------------------------------------------------------------

#define DELETE_FOLDER_CONTENTS_ERROR_STRING "Deleting folder contents {#} failed."

		// -----------------------------------------------------------------------------------------------------

		void DeleteFolderContentsWithError(Stream<wchar_t> path, UISystem* system)
		{
			ErrorWindow(path, DeleteFolderContents, DELETE_FOLDER_CONTENTS_ERROR_STRING, system);
		}

		void DeleteFolderContentsWithError(Stream<wchar_t> path) {
			ErrorConsole(path, DeleteFolderContents, DELETE_FOLDER_CONTENTS_ERROR_STRING);
		}
		
		// -----------------------------------------------------------------------------------------------------

		void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path, UISystem* system) {
			ECS_TEMP_ASCII_STRING(error_message, 256);
			bool success = OpenFileWithDefaultApplication(path, &error_message);
			if (!success) {
				CreateErrorMessageWindow(system, error_message);
			}
		}

		void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path) {
			ECS_TEMP_ASCII_STRING(error_message, 256);
			bool success = OpenFileWithDefaultApplication(path, &error_message);
			if (!success) {
				GetConsole()->Error(error_message);
			}
		}

		// -----------------------------------------------------------------------------------------------------

#pragma endregion


	}

}