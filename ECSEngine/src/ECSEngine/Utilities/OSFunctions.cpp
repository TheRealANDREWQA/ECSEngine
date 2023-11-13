#include "ecspch.h"
#include "OSFunctions.h"
#include "StringUtilities.h"
#include "../Tools/UI/UIDrawerWindows.h"
#include "File.h"
#include "Path.h"

#include <DbgHelp.h>
#include <winternl.h>
#include <psapi.h>

#pragma comment(lib, "dbghelp.lib")

bool SYM_INITIALIZED = false;

ECS_TOOLS;

namespace ECSEngine {

	namespace OS {

#pragma region Basic APIs

		// -----------------------------------------------------------------------------------------------------

		bool LaunchFileExplorer(Stream<wchar_t> folder) {
			wchar_t temp_characters[1024];
			CapacityStream<wchar_t> temp_stream(temp_characters, 0, 1024);
			temp_stream.AddStream(Stream<wchar_t>(L"explorer.exe /select,\"", std::size(L"explorer.exe /select,\"") - 1));
			temp_stream.AddStream(folder);
			temp_stream.Add(L'\"');
			temp_stream.AddAssert(L'\0');

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

		void ConvertSystemTimeToDate(const SYSTEMTIME* system_time, char* characters) {
			Stream<char> stream(characters, 0);
			ConvertIntToChars(stream, system_time->wDay);
			stream.Add('/');
			ConvertIntToChars(stream, system_time->wMonth);
			stream.Add('/');
			ConvertIntToChars(stream, system_time->wYear);
			stream.Add(' ');
			ConvertIntToChars(stream, system_time->wHour);
			stream.Add(':');
			ConvertIntToChars(stream, system_time->wMinute);
			stream.Add(':');
			ConvertIntToChars(stream, system_time->wSecond);
			stream[stream.size] = '\0';
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimesInternal(void* file_handle, char* creation_time, char* access_time, char* last_write_time)
		{
			FILETIME filetime_creation, filetime_last_access, filetime_last_write;

			// if getting the handle was succesful, get file times
			BOOL success = GetFileTime(file_handle, &filetime_creation, &filetime_last_access, &filetime_last_write);
			if (!success) {
				return false;
			}

			SYSTEMTIME sys_creation_time, sys_access_time, sys_last_write_time;
			if (creation_time != nullptr) {
				SYSTEMTIME local_creation_time;
				success &= FileTimeToSystemTime(&filetime_creation, &sys_creation_time);
				success &= SystemTimeToTzSpecificLocalTime(nullptr, &sys_creation_time, &local_creation_time);
				if (success) {
					ConvertSystemTimeToDate(&local_creation_time, creation_time);
				}
			}
			if (access_time != nullptr && success) {
				SYSTEMTIME local_access_time;
				success &= FileTimeToSystemTime(&filetime_last_access, &sys_access_time);
				success &= SystemTimeToTzSpecificLocalTime(nullptr, &sys_access_time, &local_access_time);
				if (success) {
					ConvertSystemTimeToDate(&local_access_time, access_time);
				}
			}
			if (last_write_time != nullptr && success) {
				SYSTEMTIME local_write_time;
				success &= FileTimeToSystemTime(&filetime_last_write, &sys_last_write_time);
				success &= SystemTimeToTzSpecificLocalTime(nullptr, &sys_last_write_time, &local_write_time);
				if (success) {
					ConvertSystemTimeToDate(&local_write_time, last_write_time);
				}
			}
			return success;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimesInternal(void* file_handle, wchar_t* creation_time, wchar_t* access_time, wchar_t* last_write_time)
		{
			char _creation_time[256];
			char _access_time[256];
			char _last_write_time[256];

			char* ptr1 = creation_time == nullptr ? nullptr : _creation_time;
			char* ptr2 = access_time == nullptr ? nullptr : _access_time;
			char* ptr3 = last_write_time == nullptr ? nullptr : _last_write_time;

			bool success = GetFileTimesInternal(file_handle, ptr1, ptr2, ptr3);
			if (success) {
				if (ptr1 != nullptr) {
					ConvertASCIIToWide(creation_time, ptr1, 256);
				}
				if (ptr2 != nullptr) {
					ConvertASCIIToWide(access_time, ptr2, 256);
				}
				if (ptr3 != nullptr) {
					ConvertASCIIToWide(last_write_time, ptr3, 256);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimesInternal(void* file_handle, size_t* creation_time, size_t* access_time, size_t* last_write_time)
		{
			FILETIME filetime_creation, filetime_last_access, filetime_last_write;

			// if getting the handle was succesful, get file times
			BOOL success = GetFileTime(file_handle, &filetime_creation, &filetime_last_access, &filetime_last_write);

			if (!success) {
				return false;
			}

			// use as per MSDN ULARGE_INTEGER in order to convert to normal integers and perform substraction
			ULARGE_INTEGER large_integer;
			if (creation_time != nullptr) {
				large_integer.LowPart = filetime_creation.dwLowDateTime;
				large_integer.HighPart = filetime_creation.dwHighDateTime;
				uint64_t value = large_integer.QuadPart;

				// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
				// Divide by 10'000 to get milliseconds
				*creation_time = value / 10'000;
			}

			if (access_time != nullptr) {
				large_integer.LowPart = filetime_last_access.dwLowDateTime;
				large_integer.HighPart = filetime_last_access.dwHighDateTime;
				uint64_t value = large_integer.QuadPart;

				// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
				// Divide by 10'000 to get milliseconds
				*access_time = value / 10'000;
			}

			if (last_write_time != nullptr) {
				large_integer.LowPart = filetime_last_write.dwLowDateTime;
				large_integer.HighPart = filetime_last_write.dwHighDateTime;
				uint64_t value = large_integer.QuadPart;

				// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
				// Divide by 10'000 to get milliseconds
				*last_write_time = value / 10'000;
			}
			return true;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimesInternal(void* file_handle, char* creation_time, char* access_time, char* last_write_time)
		{
			size_t creation_time_int, access_time_int, last_write_time_int;
			size_t* ptr1 = nullptr;
			size_t* ptr2 = nullptr;
			size_t* ptr3 = nullptr;

			ptr1 = creation_time != nullptr ? &creation_time_int : nullptr;
			ptr2 = access_time != nullptr ? &access_time_int : nullptr;
			ptr3 = last_write_time != nullptr ? &last_write_time_int : nullptr;

			bool success = GetRelativeFileTimesInternal(file_handle, ptr1, ptr2, ptr3);
			if (success) {
				if (ptr1 != nullptr) {
					ConvertDurationToChars(creation_time_int, creation_time);
				}
				if (ptr2 != nullptr) {
					ConvertDurationToChars(access_time_int, access_time);
				}
				if (ptr3 != nullptr) {
					ConvertDurationToChars(last_write_time_int, last_write_time);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimesInternal(void* file_handle, wchar_t* creation_time, wchar_t* access_time, wchar_t* last_write_time)
		{
			char temp_characters1[256];
			char temp_characters2[256];
			char temp_characters3[256];

			char* ptr1 = creation_time != nullptr ? temp_characters1 : nullptr;
			char* ptr2 = access_time != nullptr ? temp_characters2 : nullptr;
			char* ptr3 = last_write_time != nullptr ? temp_characters3 : nullptr;
			bool success = GetRelativeFileTimesInternal(file_handle, ptr1, ptr2, ptr3);

			if (success) {
				if (ptr1 != nullptr) {
					ConvertASCIIToWide(creation_time, temp_characters1, 256);
				}
				if (ptr2 != nullptr) {
					ConvertASCIIToWide(access_time, temp_characters2, 256);
				}
				if (ptr3 != nullptr) {
					ConvertASCIIToWide(last_write_time, temp_characters3, 256);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimesInternal(void* file_handle, size_t* creation_time, size_t* access_time, size_t* last_write_time)
		{
			FILETIME filetime_creation, filetime_last_access, filetime_last_write;

			// if getting the handle was succesful, get file times
			BOOL success = GetFileTime(file_handle, &filetime_creation, &filetime_last_access, &filetime_last_write);

			if (!success) {
				return false;
			}

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
					large_integer.LowPart = filetime_creation.dwLowDateTime;
					large_integer.HighPart = filetime_creation.dwHighDateTime;
					uint64_t second_value = large_integer.QuadPart;

					// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
					// Divide by 10'000 to get milliseconds
					*creation_time = (system_value - second_value) / 10'000;
				}

				if (access_time != nullptr) {
					large_integer.LowPart = filetime_last_access.dwLowDateTime;
					large_integer.HighPart = filetime_last_access.dwHighDateTime;
					uint64_t second_value = large_integer.QuadPart;

					// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
					// Divide by 10'000 to get milliseconds
					*access_time = (system_value - second_value) / 10'000;
				}

				if (last_write_time != nullptr) {
					large_integer.LowPart = filetime_last_write.dwLowDateTime;
					large_integer.HighPart = filetime_last_write.dwHighDateTime;
					uint64_t second_value = large_integer.QuadPart;

					// FILETIME: Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
					// Divide by 10'000 to get milliseconds
					*last_write_time = (system_value - second_value) / 10'000;
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimesInternal(Stream<wchar_t> path, FILETIME* filetime_creation, FILETIME* filetime_last_access, FILETIME* filetime_last_write) {
			NULL_TERMINATE_WIDE(path);
			
			// Determine whether or not it is a file or directory
			HANDLE handle = INVALID_HANDLE_VALUE;

			// Open the handle to the file/directory with delete, read and write sharing options enabled such that it doesn't interfere
			// with other read/write operations
			if (PathExtensionSize(path) == 0) {
				// Open a handle to a directory
				handle = CreateFile(path.buffer, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
			}
			else {
				// Open a handle to the file
				handle = CreateFile(path.buffer, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			}

			if (handle == INVALID_HANDLE_VALUE) {
				return false;
			}
			else {
				// if getting the handle was successful, get file times
				BOOL success = GetFileTime(handle, filetime_creation, filetime_last_access, filetime_last_write);

				CloseHandle(handle);
				if (!success) {
					return false;
				}
				return true;
			}
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(Stream<wchar_t> path, char* creation_time, char* access_time, char* last_write_time)
		{
			FILETIME os_creation_time, os_access_time, os_last_write_time;
			bool success = GetFileTimesInternal(path, &os_creation_time, &os_access_time, &os_last_write_time);

			if (success) {
				SYSTEMTIME sys_creation_time, sys_access_time, sys_last_write_time;
				if (creation_time != nullptr) {
					SYSTEMTIME local_creation_time;
					success &= (bool)FileTimeToSystemTime(&os_creation_time, &sys_creation_time);
					success &= (bool)SystemTimeToTzSpecificLocalTime(nullptr, &sys_creation_time, &local_creation_time);
					if (success) {
						ConvertSystemTimeToDate(&local_creation_time, creation_time);
					}
				}
				if (access_time != nullptr && success) {
					SYSTEMTIME local_access_time;
					success &= (bool)FileTimeToSystemTime(&os_access_time, &sys_access_time);
					success &= (bool)SystemTimeToTzSpecificLocalTime(nullptr, &sys_access_time, &local_access_time);
					if (success) {
						ConvertSystemTimeToDate(&local_access_time, access_time);
					}
				}
				if (last_write_time != nullptr && success) {
					SYSTEMTIME local_write_time;
					success &= (bool)FileTimeToSystemTime(&os_last_write_time, &sys_last_write_time);
					success &= (bool)SystemTimeToTzSpecificLocalTime(nullptr, &sys_last_write_time, &local_write_time);
					if (success) {
						ConvertSystemTimeToDate(&local_write_time, last_write_time);
					}
				}
				return success;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(Stream<wchar_t> path, wchar_t* creation_time, wchar_t* access_time, wchar_t* last_write_time)
		{
			char _creation_time[256];
			char _access_time[256];
			char _last_write_time[256];

			char* ptr1 = creation_time == nullptr ? nullptr : _creation_time;
			char* ptr2 = access_time == nullptr ? nullptr : _access_time;
			char* ptr3 = last_write_time == nullptr ? nullptr : _last_write_time;

			bool success = GetFileTimes(path, ptr1, ptr2, ptr3);
			if (success) {
				if (ptr1 != nullptr) {
					ConvertASCIIToWide(creation_time, ptr1, 256);
				}
				if (ptr2 != nullptr) {
					ConvertASCIIToWide(access_time, ptr2, 256);
				}
				if (ptr3 != nullptr) {
					ConvertASCIIToWide(last_write_time, ptr3, 256);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetFileTimes(Stream<wchar_t> path, size_t* creation_time, size_t* access_time, size_t* last_write_time)
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

		bool GetRelativeFileTimes(
			Stream<wchar_t> path,
			size_t* creation_time,
			size_t* access_time,
			size_t* last_write_time
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
			Stream<wchar_t> path,
			char* creation_time,
			char* access_time,
			char* last_write_time
		)
		{
			size_t creation_time_int, access_time_int, last_write_time_int;
			size_t* ptr1 = nullptr;
			size_t* ptr2 = nullptr;
			size_t* ptr3 = nullptr;

			ptr1 = creation_time != nullptr ? &creation_time_int : nullptr;
			ptr2 = access_time != nullptr ? &access_time_int : nullptr;
			ptr3 = last_write_time != nullptr ? &last_write_time_int : nullptr;

			bool success = GetRelativeFileTimes(path, ptr1, ptr2, ptr3);
			if (success) {
				if (ptr1 != nullptr) {
					ConvertDurationToChars(creation_time_int, creation_time);
				}
				if (ptr2 != nullptr) {
					ConvertDurationToChars(access_time_int, access_time);
				}
				if (ptr3 != nullptr) {
					ConvertDurationToChars(last_write_time_int, last_write_time);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetRelativeFileTimes(
			Stream<wchar_t> path,
			wchar_t* creation_time,
			wchar_t* access_time,
			wchar_t* last_write_time
		)
		{
			char temp_characters1[256];
			char temp_characters2[256];
			char temp_characters3[256];

			char* ptr1 = creation_time != nullptr ? temp_characters1 : nullptr;
			char* ptr2 = access_time != nullptr ? temp_characters2 : nullptr;
			char* ptr3 = last_write_time != nullptr ? temp_characters3 : nullptr;
			bool success = GetRelativeFileTimes(path, ptr1, ptr2, ptr3);

			if (success) {
				if (ptr1 != nullptr) {
					ConvertASCIIToWide(creation_time, temp_characters1, 256);
				}
				if (ptr2 != nullptr) {
					ConvertASCIIToWide(access_time, temp_characters2, 256);
				}
				if (ptr3 != nullptr) {
					ConvertASCIIToWide(last_write_time, temp_characters3, 256);
				}
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool OpenFileWithDefaultApplication(
			Stream<wchar_t> path,
			CapacityStream<char>* error_message
		) {
			NULL_TERMINATE_WIDE(path);

			HINSTANCE instance = ShellExecute(NULL, NULL, path.buffer, NULL, NULL, SW_SHOW);
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
				error_message->AddStreamSafe(error_code);
			}
			return false;
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

		size_t GetFileLastWrite(Stream<wchar_t> path)
		{
			size_t last_write = 0;
			GetFileTimes(path, nullptr, nullptr, &last_write);
			return last_write;
		}

		// -----------------------------------------------------------------------------------------------------

		void* LoadDLL(Stream<wchar_t> path)
		{
			NULL_TERMINATE_WIDE(path);
			return LoadLibraryEx(path.buffer, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
		}

		// -----------------------------------------------------------------------------------------------------

		void UnloadDLL(void* module_handle)
		{
			FreeLibrary((HMODULE)module_handle);
		}

		// -----------------------------------------------------------------------------------------------------

		void* GetDLLSymbol(void* module_handle, const char* symbol_name)
		{
			return GetProcAddress((HMODULE)module_handle, symbol_name);
		}

		// -----------------------------------------------------------------------------------------------------

		bool CaptureCurrentThreadStackContext(ThreadContext* thread_context)
		{
			LPCONTEXT context = (LPCONTEXT)thread_context->bytes;
			RtlCaptureContext(context);
			return true;
		}

		// -----------------------------------------------------------------------------------------------------

		void InitializeSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, search_paths, ECS_KB * 8);
			for (size_t index = 0; index < module_paths.size; index++) {
				search_paths.AddStream(module_paths[index]);
				search_paths.AddAssert(L':');
			}
			// This has to be size - 1 to replace the last colon that was added in the loop
			unsigned int null_terminator_index = search_paths.size == 0 ? 0 : search_paths.size - 1;
			search_paths[null_terminator_index] = L'\0';

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

#if 1

		struct Data {
			CapacityStream<Stream<wchar_t>> modules;
			AllocatorPolymorphic stack_allocator;
		};

		static BOOL EnumerateModules(PCWSTR ModuleName, DWORD64 BaseOfDll, PVOID UserContext) {
			Data* data = (Data*)UserContext;
			Stream<wchar_t> module = ModuleName;
			module.size++;
			data->modules.AddAssert(module.Copy(data->stack_allocator));
			return TRUE;
		}
		
		// This is useful only when for some reason symbols are not loaded
		// And want to inspector what modules are loaded
		static void EnumerateModulesDebug() {
			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, modules, 512);
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
			Data data = { modules, GetAllocatorPolymorphic(&stack_allocator) };
			BOOL success = SymEnumerateModulesW64(GetCurrentProcess(), EnumerateModules, &data);
		}

#endif

		void SetSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, search_paths, ECS_KB * 16);
			for (size_t index = 0; index < module_paths.size; index++) {
				search_paths.AddStream(module_paths[index]);
				search_paths.AddAssert(L';');
			}
			search_paths[search_paths.size - 1] = L'\0';
			
			bool success = SymSetSearchPathW(GetCurrentProcess(), search_paths.buffer);
			ECS_ASSERT(success, "Setting symbolic link paths failed.");
		}

		// -----------------------------------------------------------------------------------------------------

		void RefreshDLLSymbols()
		{
			SymRefreshModuleList(GetCurrentProcess());
		}

		// -----------------------------------------------------------------------------------------------------

		bool LoadDLLSymbols(void* module_handle)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, module_name, 512);
			DWORD written_size = GetModuleFileName((HMODULE)module_handle, module_name.buffer, module_name.capacity);
			if (written_size == 0 || written_size == module_name.capacity) {
				return false;
			}
			else {
				return LoadDLLSymbols(module_name, module_handle);
			}
		}

		// -----------------------------------------------------------------------------------------------------

		bool LoadDLLSymbols(Stream<wchar_t> module_name, void* module_handle)
		{
			NULL_TERMINATE_WIDE(module_name);
			HANDLE process_handle = GetCurrentProcess();
			if (module_handle == nullptr) {
				module_handle = GetModuleHandle(module_name.buffer);
			}
			DWORD64 base_address = SymLoadModuleExW(process_handle, nullptr, module_name.buffer, nullptr, (DWORD64)module_handle, 0, nullptr, 0);
			if (base_address != 0) {
				return true;
			}
			else {
				// If the module is already loaded, it returns 0 for the base address but the last error is success
				return GetLastError() == ERROR_SUCCESS;
			}
		}

		// -----------------------------------------------------------------------------------------------------

		bool UnloadDLLSymbols(void* module_handle)
		{
			return SymUnloadModule64(GetCurrentProcess(), (DWORD64)module_handle);
		}

		// -----------------------------------------------------------------------------------------------------

		void GetCallStackFunctionNames(CapacityStream<char>& string)
		{
			ThreadContext current_thread_context;
			bool success = CaptureCurrentThreadStackContext(&current_thread_context);
			if (success) {
				GetCallStackFunctionNames(&current_thread_context, GetCurrentThread(), string);
			}
		}

		void GetCallStackFunctionNames(ThreadContext* context, void* thread_handle, CapacityStream<char>& string)
		{
			bool success;
			STACKFRAME64 stack_frame;
			memset(&stack_frame, 0, sizeof(stack_frame));

			LPCONTEXT os_context = (LPCONTEXT)context->bytes;
			os_context->ContextFlags = CONTEXT_FULL;

			stack_frame.AddrPC.Offset = os_context->Rip;
			stack_frame.AddrPC.Mode = AddrModeFlat;
			stack_frame.AddrStack.Offset = os_context->Rsp;
			stack_frame.AddrStack.Mode = AddrModeFlat;
			stack_frame.AddrFrame.Offset = os_context->Rbp;
			stack_frame.AddrFrame.Mode = AddrModeFlat;
			IMAGEHLP_SYMBOL64* image_symbol = (IMAGEHLP_SYMBOL64*)ECS_STACK_ALLOC(sizeof(IMAGEHLP_SYMBOL64) + 512);
			image_symbol->MaxNameLength = 511;
			image_symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);

			HANDLE process_handle = GetCurrentProcess();

			size_t displacement = 0;
			string.AddStreamAssert("Stack trace:\n");
			while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, process_handle, thread_handle, &stack_frame, os_context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
				success = SymGetSymFromAddr64(process_handle, (size_t)stack_frame.AddrPC.Offset, &displacement, image_symbol);
				if (success) {
					DWORD characters_written = UnDecorateSymbolName(image_symbol->Name, string.buffer + string.size, string.capacity - string.size, UNDNAME_COMPLETE);
					string.size += characters_written;
					string.Add('\n');
				}
			}

			string.AssertCapacity();
			string[string.size] = '\0';
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetCallStackFunctionNames(void* thread_handle, CapacityStream<char>& string)
		{
			ThreadContext thread_context;
			bool success = OS::CaptureThreadStackContext(thread_handle, &thread_context);
			if (success) {
				GetCallStackFunctionNames(&thread_context, thread_handle, string);
				return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		ExceptionInformation GetExceptionInformationFromNative(EXCEPTION_POINTERS* exception_pointers)
		{
			ExceptionInformation exception_information;

			memcpy(exception_information.thread_context.bytes, exception_pointers->ContextRecord, sizeof(*exception_pointers->ContextRecord));
			ECS_OS_EXCEPTION_ERROR_CODE error_code = ECS_OS_EXCEPTION_UNKNOWN;
			switch (exception_pointers->ExceptionRecord->ExceptionCode) {
			case EXCEPTION_ACCESS_VIOLATION:
				error_code = ECS_OS_EXCEPTION_ACCESS_VIOLATION;
				break;
			case EXCEPTION_DATATYPE_MISALIGNMENT:
				error_code = ECS_OS_EXCEPTION_MISSALIGNMENT;
				break;
			case EXCEPTION_FLT_DENORMAL_OPERAND:
				error_code = ECS_OS_EXCEPTION_FLOAT_DENORMAL;
				break;
			case EXCEPTION_FLT_DIVIDE_BY_ZERO:
				error_code = ECS_OS_EXCEPTION_FLOAT_DIVISION_BY_ZERO;
				break;
			case EXCEPTION_FLT_INEXACT_RESULT:
				error_code = ECS_OS_EXCEPTION_FLOAT_INEXACT_VALUE;
				break;
			case EXCEPTION_FLT_OVERFLOW:
				error_code = ECS_OS_EXCEPTION_FLOAT_OVERFLOW;
				break;
			case EXCEPTION_FLT_UNDERFLOW:
				error_code = ECS_OS_EXCEPTION_FLOAT_UNDERFLOW;
				break;
			case EXCEPTION_ILLEGAL_INSTRUCTION:
				error_code = ECS_OS_EXCEPTION_ILLEGAL_INSTRUCTION;
				break;
			case EXCEPTION_INT_DIVIDE_BY_ZERO:
				error_code = ECS_OS_EXCEPTION_INT_DIVISION_BY_ZERO;
				break;
			case EXCEPTION_INT_OVERFLOW:
				error_code = ECS_OS_EXCEPTION_INT_OVERFLOW;
				break;
			case EXCEPTION_STACK_OVERFLOW:
				error_code = ECS_OS_EXCEPTION_STACK_OVERLOW;
				break;
			}
			exception_information.error_code = error_code;

			return exception_information;
		}

		// ----------------------------------------------------------------------------------------------------

		static const char* OS_EXCEPTION_TO_STRING[] = {
			"Access violation",
			"Data missalignment",
			"Stack overflow",
			"Float overflow",
			"Float underflow",
			"Float division by zero",
			"Float denormal",
			"Float inexact value",
			"Integer division by zero",
			"Integer overflow",
			"Illegal instruction",
			"Privileged instruction",
			"Unknown"
		};

		static_assert(std::size(OS_EXCEPTION_TO_STRING) == ECS_OS_EXCEPTION_ERROR_CODE_COUNT);

		void ExceptionCodeToString(ECS_OS_EXCEPTION_ERROR_CODE code, CapacityStream<char>& string)
		{
			ECS_ASSERT(code < ECS_OS_EXCEPTION_ERROR_CODE_COUNT);
			string.AddStreamAssert(OS_EXCEPTION_TO_STRING[code]);
		}

		// ----------------------------------------------------------------------------------------------------

		bool IsExceptionCodeCritical(ECS_OS_EXCEPTION_ERROR_CODE code)
		{
			// Include unknown as well
			switch (code) {
			case ECS_OS_EXCEPTION_ACCESS_VIOLATION:
			case ECS_OS_EXCEPTION_MISSALIGNMENT:
			case ECS_OS_EXCEPTION_STACK_OVERLOW:
			case ECS_OS_EXCEPTION_ILLEGAL_INSTRUCTION:
			case ECS_OS_EXCEPTION_PRIVILEGED_INSTRUCTION:
			case ECS_OS_EXCEPTION_FLOAT_DIVISION_BY_ZERO:
			case ECS_OS_EXCEPTION_INT_DIVISION_BY_ZERO:
			case ECS_OS_EXCEPTION_UNKNOWN:
				return true;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------

		void ChangeThreadPriority(ECS_THREAD_PRIORITY priority)
		{
			ChangeThreadPriority(GetCurrentThread(), priority);
		}

		// -----------------------------------------------------------------------------------------------------

		void ChangeThreadPriority(void* thread_handle, ECS_THREAD_PRIORITY priority)
		{
			int thread_priority = 0;
			switch (priority) {
			case ECS_THREAD_PRIORITY_VERY_LOW:
				thread_priority = THREAD_PRIORITY_LOWEST;
				break;
			case ECS_THREAD_PRIORITY_LOW:
				thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
				break;
			case ECS_THREAD_PRIORITY_NORMAL:
				thread_priority = THREAD_PRIORITY_NORMAL;
				break;
			case ECS_THREAD_PRIORITY_HIGH:
				thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
				break;
			case ECS_THREAD_PRIORITY_VERY_HIGH:
				thread_priority = THREAD_PRIORITY_HIGHEST;
				break;
			}

			BOOL success = SetThreadPriority(thread_handle, thread_priority);
			ECS_ASSERT(success, "Changing thread priority failed.");
		}

		// -----------------------------------------------------------------------------------------------------

		uint2 GetCursorPosition(void* window_handle) {
			POINT point;
			ECS_ASSERT(GetCursorPos(&point));
			// Also convert from screen to client position
			ECS_ASSERT(ScreenToClient((HWND)window_handle, &point));
			return { (unsigned int)point.x, (unsigned int)point.y };
		}

		void SetCursorPosition(void* window_handle, uint2 position)
		{
			// We need to convert from client to screen coordinates
			POINT cursor_point = { position.x, position.y };
			ECS_ASSERT(ClientToScreen((HWND)window_handle, &cursor_point));
			ECS_ASSERT(SetCursorPos(cursor_point.x, cursor_point.y));
		}

		uint2 SetCursorPositionRelative(void* window_handle, uint2 position)
		{
			HWND hwnd = (HWND)window_handle;
			RECT window_rect;
			ECS_ASSERT(GetClientRect(hwnd, &window_rect));

			// If we have a top border we need to take that into account
			RECT bounds;
			GetWindowRect(hwnd, &bounds);

			POINT cursor;
			GetCursorPos(&cursor);

			unsigned int width = window_rect.right - window_rect.left;
			unsigned int height = window_rect.bottom - window_rect.top;

			position.x %= width;
			position.y %= height;

			position.x += window_rect.left;
			position.y += window_rect.top;

			SetCursorPosition(window_handle, { position.x, position.y });
			return position;
		}

		// -----------------------------------------------------------------------------------------------------

		uint2 GetOSWindowPosition(void* window_handle)
		{
			HWND hwnd = (HWND)window_handle;
			RECT window_rect;
			ECS_ASSERT(GetClientRect(hwnd, &window_rect));

			return { (unsigned int)window_rect.left, (unsigned int)window_rect.top };
		}

		// -----------------------------------------------------------------------------------------------------

		void* VirtualAllocation(size_t size)
		{
			// MEM_COMMIT | MEM_RESERVE means to reserve virtual space and indicate that we want these pages
			// to be mapped to physical memory (the actual physical memory will be reserved when the process access it)
			void* allocation = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			ECS_ASSERT(allocation != nullptr, "Failed to allocated virtual memory");
			return allocation;
		}

		void VirtualDeallocation(void* block)
		{
			ECS_ASSERT(VirtualFree(block, 0, MEM_RELEASE));
		}

		bool DisableVirtualWriteProtection(void* allocation)
		{
			MEMORY_BASIC_INFORMATION memory_info;
			size_t memory_info_byte_size = VirtualQuery(allocation, &memory_info, sizeof(memory_info));
			if (memory_info_byte_size == 0) {
				// The function failed
				return false;
			}

			DWORD previous_access;
			return VirtualProtect(allocation, memory_info.RegionSize, PAGE_READWRITE, &previous_access);
		}

		bool EnableVirtualWriteProtection(void* allocation)
		{
			MEMORY_BASIC_INFORMATION memory_info;
			size_t memory_info_byte_size = VirtualQuery(allocation, &memory_info, sizeof(memory_info));
			if (memory_info_byte_size == 0) {
				// The function failed
				return false;
			}

			DWORD previous_access;
			return VirtualProtect(allocation, memory_info.RegionSize, PAGE_READONLY, &previous_access);
		}

		// -----------------------------------------------------------------------------------------------------

		void OSMessageBox(const char* message, const char* title)
		{
			MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
		}

		// -----------------------------------------------------------------------------------------------------

		void ExitThread(int error_code) {
			::ExitThread(error_code);
		}

		// -----------------------------------------------------------------------------------------------------

		bool SuspendThread(void* handle)
		{
			DWORD suspend_count = ::SuspendThread(handle);
			DWORD last_error = GetLastError();
			return suspend_count != -1;
		}

		// -----------------------------------------------------------------------------------------------------

		bool ResumeThread(void* handle)
		{
			DWORD suspend_count = ::ResumeThread(handle);
			DWORD last_error = GetLastError();
			return suspend_count != -1;
		}

		// -----------------------------------------------------------------------------------------------------

		void* GetCurrentThreadHandle()
		{
			HANDLE pseudo_handle = GetCurrentThread();
			return DuplicateThreadHandle(pseudo_handle);
		}

		// -----------------------------------------------------------------------------------------------------

		void CloseThreadHandle(void* handle)
		{
			CloseHandle(handle);
		}

		// -----------------------------------------------------------------------------------------------------

		void* DuplicateThreadHandle(void* handle)
		{
			HANDLE process_handle = GetCurrentProcess();
			HANDLE new_handle = nullptr;
			BOOL success = DuplicateHandle(process_handle, handle, process_handle, &new_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
			return success ? new_handle : nullptr;
		}

		// -----------------------------------------------------------------------------------------------------

		ECS_INLINE static size_t CombineInt32(unsigned int low, unsigned int high) {
			return (size_t)low | ((size_t)high << 32);
		}

		ulong2 GetThreadTimes(void* handle)
		{
			FILETIME creation_time;
			FILETIME exit_time;
			FILETIME user_time;
			FILETIME kernel_time;
			BOOL success = ::GetThreadTimes(handle, &creation_time, &exit_time, &kernel_time, &user_time);
			if (success) {
				// These are in 100s of nanoseconds
				size_t long_user_time = CombineInt32(user_time.dwLowDateTime, user_time.dwHighDateTime);
				size_t long_kernel_time = CombineInt32(kernel_time.dwLowDateTime, kernel_time.dwHighDateTime);
				return { long_user_time, long_kernel_time };
			}
			return { (size_t)-1, (size_t)-1 };
		}

		// -----------------------------------------------------------------------------------------------------

		float2 ThreadTimesDuration(ulong2 current_times, ulong2 previous_times, ECS_TIMER_DURATION duration_type)
		{
			// The current values are in 100s of nanoseconds
			float duration_factor = GetTimeFactorFloatInverse(duration_type);
			ulong2 difference = current_times - previous_times;
			// We need to multiply by 100.0f since these are in 100s of nanoseconds
			return { difference.x * duration_factor * 100.0f, difference.y * duration_factor * 100.0f };
		}

		// -----------------------------------------------------------------------------------------------------

		size_t GetCurrentThreadID()
		{
			return GetCurrentThreadId();
		}

		// -----------------------------------------------------------------------------------------------------

		size_t GetThreadID(void* handle)
		{
			DWORD value = GetThreadId(handle);
			size_t last_error = GetLastError();
			return value == 0 ? (size_t)-1 : (size_t)value;
		}

		// -----------------------------------------------------------------------------------------------------

		bool CaptureThreadStackContext(void* thread_handle, ThreadContext* thread_context)
		{
			static_assert(sizeof(ThreadContext) >= sizeof(CONTEXT));
			size_t thread_id = GetThreadID(thread_handle);
			size_t running_thread_id = GetCurrentThreadID();
			if (thread_id == running_thread_id) {
				// In case we are the running thread, we need to use RtlCaptureContext
				// Since GetThreadContext doesn't work on the running thread
				RtlCaptureContext((LPCONTEXT)thread_context->bytes);
				return true;
			}
			else {
				return GetThreadContext(thread_handle, (LPCONTEXT)thread_context->bytes) != 0;
			}
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
				size_t written_characters = FormatString(temp_characters, error_string, path);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void ErrorConsole(Stream<wchar_t> path, FolderFunction function, const char* error_string) {
			bool success = function(path);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = FormatString(temp_characters, error_string, path);
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
				size_t written_characters = FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
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
				size_t written_characters = FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
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
				size_t written_characters = FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
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
				size_t written_characters = FormatString(temp_characters, GET_FILE_TIMES_ERROR_STRING, path);
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
				size_t written_characters = FormatString(temp_characters, FILE_COPY_ERROR_STRING, from, to);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to) {
			bool success = FileCopy(from, to);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = FormatString(temp_characters, FILE_COPY_ERROR_STRING, from, to);
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
				size_t written_characters = FormatString(temp_characters, FOLDER_COPY_ERROR_STRING, from, to);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to) {
			bool success = FolderCopy(from, to);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = FormatString(temp_characters, FOLDER_COPY_ERROR_STRING, from, to);
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
				size_t written_characters = FormatString(temp_characters, RENAME_FOLDER_ERROR_STRING, path, new_name);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name) {
			bool success = RenameFolderOrFile(path, new_name);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = FormatString(temp_characters, RENAME_FOLDER_ERROR_STRING, path, new_name);
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
				size_t written_characters = FormatString(temp_characters, RESIZE_FILE_ERROR_STRING, path, new_size);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void ResizeFileWithError(Stream<wchar_t> path, size_t new_size) {
			bool success = ResizeFile(path, new_size);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = FormatString(temp_characters, RESIZE_FILE_ERROR_STRING, path, new_size);
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
				size_t written_characters = FormatString(temp_characters, CHANGE_FILE_EXTENSION_ERROR_STRING, path, new_extension);
				ECS_ASSERT(written_characters < 512);
				CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
			}
		}

		void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> new_extension) {
			bool success = ChangeFileExtension(path, new_extension);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = FormatString(temp_characters, CHANGE_FILE_EXTENSION_ERROR_STRING, path, new_extension);
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
			ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
			bool success = OpenFileWithDefaultApplication(path, &error_message);
			if (!success) {
				CreateErrorMessageWindow(system, error_message);
			}
		}

		void OpenFileWithDefaultApplicationWithError(Stream<wchar_t> path) {
			ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
			bool success = OpenFileWithDefaultApplication(path, &error_message);
			if (!success) {
				GetConsole()->Error(error_message);
			}
		}

		// -----------------------------------------------------------------------------------------------------

#pragma endregion

		// -----------------------------------------------------------------------------------------------------

		void SetBasicErrorMessage(const char* error, CapacityStream<char>& message) {
			if (message.buffer != nullptr) {
				Stream<char> stream = error;
				message.AddStreamSafe(stream);
				message[message.size] = '\0';
			}
		}

		bool FileExplorerGetFile(FileExplorerGetFileData* data) {
			data->user_cancelled = false;

			IFileOpenDialog* dialog = nullptr;
			HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			if (result != S_OK && result != S_FALSE) {
				SetBasicErrorMessage("Failed to initialize COM subsystem.", data->error_message);
				data->path.size = 0;
				return false;
			}

			result = CoCreateInstance(
				CLSID_FileOpenDialog,
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&dialog)
			);

			if (SUCCEEDED(result)) {
				DWORD dialog_options;
				result = dialog->GetOptions(&dialog_options);

				if (SUCCEEDED(result)) {
					if (data->extensions.size == 0) {
						result = dialog->SetOptions(dialog_options | FOS_NOCHANGEDIR);
						if (FAILED(result)) {
							SetBasicErrorMessage("Setting dialog options failed!", data->error_message);
							data->path.size = 0;
							return false;
						}
					}
					else {
						result = dialog->SetOptions(dialog_options | FOS_NOCHANGEDIR | FOS_STRICTFILETYPES);
						if (FAILED(result)) {
							SetBasicErrorMessage("Setting dialog options failed!", data->error_message);
							data->path.size = 0;
							return false;
						}

						ECS_STACK_CAPACITY_STREAM(wchar_t, modified_extensions, 512);

						size_t extension_count = data->extensions.size > 1 ? data->extensions.size + 1 : 1;

						// Have an extra field with the all extensions allowed
						COMDLG_FILTERSPEC* filters = (COMDLG_FILTERSPEC*)ECS_STACK_ALLOC(sizeof(COMDLG_FILTERSPEC) * extension_count);
						for (size_t index = 0; index < data->extensions.size; index++) {
							filters[index] = { L"", modified_extensions.buffer + modified_extensions.size };
							modified_extensions.Add(L'*');
							modified_extensions.AddStream(data->extensions[index]);
							modified_extensions.Add(L'\0');
						}
						
						if (extension_count > 1) {
							// Create the last all filter
							filters[data->extensions.size] = { L"", modified_extensions.buffer + modified_extensions.size };
							for (size_t index = 0; index < data->extensions.size; index++) {
								modified_extensions.Add(L'*');
								modified_extensions.AddStream(data->extensions[index]);
								modified_extensions.Add(L';');
							}
							modified_extensions[modified_extensions.size] = L'\0';
						}

						result = dialog->SetFileTypes(extension_count, filters);

						if (FAILED(result)) {
							SetBasicErrorMessage("Setting dialog extensions failed!", data->error_message);
							data->path.size = 0;
							return false;
						}
					}
				}
				else {
					SetBasicErrorMessage("Getting dialog options failed!", data->error_message);
					data->path.size = 0;
					return false;
				}

				result = dialog->Show(nullptr);

				if (SUCCEEDED(result)) {
					IShellItem* item;
					result = dialog->GetResult(&item);

					if (SUCCEEDED(result)) {
						wchar_t* temp_path;
						result = item->GetDisplayName(SIGDN_FILESYSPATH, &temp_path);
						if (FAILED(result)) {
							SetBasicErrorMessage("Getting the path failed!", data->error_message);
							data->path.size = 0;
							return false;
						}

						data->path.CopyOther(temp_path, wcslen(temp_path));
						data->path[data->path.size] = L'\0';
						CoTaskMemFree(temp_path);
						item->Release();
						dialog->Release();

						return true;
					}
					else {
						SetBasicErrorMessage("Retrieving dialog result failed!", data->error_message);
						data->path.size = 0;
						return false;
					}
				}
				/*else {
					SetBasicErrorMessage("Showing dialog failed!", data->error_message);
					return false;
				}*/
			}
			else {
				SetBasicErrorMessage("Creating dialog instance failed!", data->error_message);
				data->path.size = 0;
				return false;
			}

			SetBasicErrorMessage("The user cancelled the search.", data->error_message);
			data->user_cancelled = true;
			return false;
		}

		// -----------------------------------------------------------------------------------------------------

		bool FileExplorerGetDirectory(FileExplorerGetDirectoryData* data) {
			data->user_cancelled = false;

			IFileDialog* dialog = nullptr;
			HRESULT result = CoCreateInstance(
				CLSID_FileOpenDialog,
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&dialog)
			);
			if (SUCCEEDED(result)) {
				DWORD file_options;
				result = dialog->GetOptions(&file_options);

				if (SUCCEEDED(result)) {
					result = dialog->SetOptions(file_options | FOS_PICKFOLDERS);

					if (SUCCEEDED(result)) {
						result = dialog->Show(nullptr);

						if (SUCCEEDED(result)) {
							IShellItem* item;
							result = dialog->GetResult(&item);

							if (SUCCEEDED(result)) {
								wchar_t* temp_path;
								result = item->GetDisplayName(SIGDN_FILESYSPATH, &temp_path);
								data->path.CopyOther(temp_path, wcslen(temp_path));
								data->path[data->path.size] = L'\0';
								CoTaskMemFree(temp_path);
								item->Release();
								dialog->Release();
								return true;
							}
							else {
								SetBasicErrorMessage("Getting dialog result failed!", data->error_message);
								data->path.size = 0;
								return false;
							}
						}
						else {
							SetBasicErrorMessage("The user cancelled the selection.", data->error_message);
							data->user_cancelled = true;
							return false;
						}
					}
					else {
						SetBasicErrorMessage("Setting dialog options failed!", data->error_message);
						data->path.size = 0;
						return false;
					}
				}
				else {
					SetBasicErrorMessage("Getting dialog options failed!", data->error_message);
					data->path.size = 0;
					return false;
				}
			}
			else {
				SetBasicErrorMessage("Creating dialog instance failed!", data->error_message);
				data->path.size = 0;
				return false;
			}
		}

		// -----------------------------------------------------------------------------------------------------

		ThreadContext::ThreadContext()
		{
			Initialize();
		}

		void ThreadContext::Initialize()
		{
			LPCONTEXT context = (LPCONTEXT)bytes;
			context->ContextFlags = CONTEXT_FULL;
		}

		// -----------------------------------------------------------------------------------------------------

	}

}