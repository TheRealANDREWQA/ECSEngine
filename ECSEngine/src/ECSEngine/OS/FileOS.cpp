#include "ecspch.h"
#include "FileOS.h"
#include "../Utilities/StringUtilities.h"
#include "../Utilities/Path.h"
#include "../Utilities/StackScope.h"
#include "../Tools/UI/UIDrawerWindows.h"
#include "WithError.h"

namespace ECSEngine {

	using namespace Tools;

	namespace OS {

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

		size_t GetFileLastWrite(Stream<wchar_t> path)
		{
			size_t last_write = 0;
			GetFileTimes(path, nullptr, nullptr, &last_write);
			return last_write;
		}

		// -----------------------------------------------------------------------------------------------------

#define GET_FILE_TIMES_ERROR_STRING "Getting file {#} times failed!"

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
				ECS_FORMAT_TEMP_STRING(error_message, GET_FILE_TIMES_ERROR_STRING, path);
				CreateErrorMessageWindow(system, error_message);
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
				ECS_FORMAT_TEMP_STRING(error_message, GET_FILE_TIMES_ERROR_STRING, path);
				CreateErrorMessageWindow(system, error_message);
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
			ECS_ASSERT(path.size < ECS_COUNTOF(temp_wide_characters));
			path.CopyTo(temp_wide_characters);
			temp_wide_characters[path.size] = L'\0';

			bool success = GetFileTimes(temp_wide_characters, creation_time, access_time, last_write_time);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, GET_FILE_TIMES_ERROR_STRING, path);
				GetConsole()->Error(error_message);
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
			ECS_ASSERT(path.size < ECS_COUNTOF(temp_wide_characters));
			path.CopyTo(temp_wide_characters);
			temp_wide_characters[path.size] = L'\0';

			bool success = GetRelativeFileTimes(temp_wide_characters, creation_time, access_time, last_write_time);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, GET_FILE_TIMES_ERROR_STRING, path);
				GetConsole()->Error(error_message);
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
				ECS_FORMAT_TEMP_STRING(error_message, FILE_COPY_ERROR_STRING, from, to);
				CreateErrorMessageWindow(system, error_message);
			}
		}

		void FileCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to) {
			bool success = FileCopy(from, to);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, FILE_COPY_ERROR_STRING, from, to);
				GetConsole()->Error(error_message);
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define FOLDER_COPY_ERROR_STRING "Copying folder {#} to {#} failed. Make sure that both folders exist."

		// -----------------------------------------------------------------------------------------------------

		void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to, UISystem* system) {
			bool success = FolderCopy(from, to);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, FOLDER_COPY_ERROR_STRING, from, to);
				CreateErrorMessageWindow(system, error_message);
			}
		}

		void FolderCopyWithError(Stream<wchar_t> from, Stream<wchar_t> to) {
			bool success = FolderCopy(from, to);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, FOLDER_COPY_ERROR_STRING, from, to);
				GetConsole()->Error(error_message);
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

		void RenameFolderWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, UISystem* system) {
			bool success = RenameFolder(path, new_name);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, RENAME_FOLDER_ERROR_STRING, path, new_name);
				CreateErrorMessageWindow(system, error_message);
			}
		}

		void RenameFolderOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name) {
			bool success = RenameFolder(path, new_name);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, RENAME_FOLDER_ERROR_STRING, path, new_name);
				GetConsole()->Error(error_message);
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define RENAME_FILE_ERROR_STRING "Renaming file {#} to failed. Incorrect path, invalid new name or access denied."

		void RenameFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name, UISystem* system) {
			bool success = RenameFile(path, new_name);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(message, RENAME_FILE_ERROR_STRING, path, new_name);
				CreateErrorMessageWindow(system, message);
			}
		}

		void RenameFileOrFileWithError(Stream<wchar_t> path, Stream<wchar_t> new_name) {
			bool success = RenameFile(path, new_name);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(message, RENAME_FILE_ERROR_STRING, path, new_name);
				GetConsole()->Error(message);
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define RESIZE_FILE_ERROR_STRING "Resizing file {#} to {#} size failed. Incorrect path or access denied"

		// -----------------------------------------------------------------------------------------------------

		void ResizeFileWithError(Stream<wchar_t> path, size_t new_size, UISystem* system) {
			bool success = ResizeFile(path, new_size);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, RESIZE_FILE_ERROR_STRING, path, new_size);
				CreateErrorMessageWindow(system, error_message);
			}
		}

		void ResizeFileWithError(Stream<wchar_t> path, size_t new_size) {
			bool success = ResizeFile(path, new_size);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, RESIZE_FILE_ERROR_STRING, path, new_size);
				GetConsole()->Error(error_message);
			}
		}

		// -----------------------------------------------------------------------------------------------------

#define CHANGE_FILE_EXTENSION_ERROR_STRING "Changing file {#} extension to {#} failed. Incorrect file, invalid extension or access denied."

		// -----------------------------------------------------------------------------------------------------

		void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> new_extension, UISystem* system) {
			bool success = ChangeFileExtension(path, new_extension);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, CHANGE_FILE_EXTENSION_ERROR_STRING, path, new_extension);
				CreateErrorMessageWindow(system, error_message);
			}
		}

		void ChangeFileExtensionWithError(Stream<wchar_t> path, Stream<wchar_t> new_extension) {
			bool success = ChangeFileExtension(path, new_extension);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, CHANGE_FILE_EXTENSION_ERROR_STRING, path, new_extension);
				GetConsole()->Error(error_message);
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


	}

}