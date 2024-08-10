#include "ecspch.h"
#include "WithError.h"
#include "../Tools/UI/UIDrawerWindows.h"
#include "../Utilities/StringUtilities.h"

namespace ECSEngine {

	namespace OS {

		void ErrorWindow(Stream<wchar_t> path, FolderFunction function, const char* error_string, Tools::UISystem* system) {
			bool success = function(path);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(message, error_string, path);
				Tools::CreateErrorMessageWindow(system, message);
			}
		}

		void ErrorConsole(Stream<wchar_t> path, FolderFunction function, const char* error_string) {
			bool success = function(path);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(message, error_string, path);
				GetConsole()->Error(message);
			}
		}

	}

}