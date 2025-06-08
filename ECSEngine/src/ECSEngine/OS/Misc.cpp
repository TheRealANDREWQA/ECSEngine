#include "ecspch.h"
#include "Misc.h"
#include "../Utilities/StackScope.h"

namespace ECSEngine {
	
	namespace OS {

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

		void OSMessageBox(const char* message, const char* title)
		{
			MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
		}

		// -----------------------------------------------------------------------------------------------------

		Stream<wchar_t> SearchForExecutable(Stream<wchar_t> executable, CapacityStream<wchar_t>& path_storage) {
			NULL_TERMINATE_WIDE(executable);
			
			DWORD result = SearchPathW(
				NULL,
				executable.buffer,
				NULL,
				path_storage.capacity - path_storage.size,
				path_storage.buffer + path_storage.size,
				NULL
			);

			if (result > 0 && result < path_storage.capacity - path_storage.size) {
				unsigned int initial_path_size = path_storage.size;
				path_storage.size += result;
				return path_storage.SliceAt(initial_path_size);
			}
			else {
				return {};
			}
		}

		// -----------------------------------------------------------------------------------------------------

	}

}