#include "ecspch.h"
#include "Misc.h"

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

	}

}