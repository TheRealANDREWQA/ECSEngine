#include "ecspch.h"
#include "WithError.h"
#include "../Tools/UI/UIDrawerWindows.h"
#include "../Utilities/StringUtilities.h"

namespace ECSEngine {

	namespace OS {

		void ErrorWindow(Stream<wchar_t> path, FolderFunction function, const char* error_string, Tools::UISystem* system) {
			bool success = function(path);
			if (!success) {
				char temp_characters[512];
				size_t written_characters = FormatString(temp_characters, error_string, path);
				ECS_ASSERT(written_characters < 512);
				Tools::CreateErrorMessageWindow(system, Stream<char>(temp_characters, written_characters));
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

	}

}