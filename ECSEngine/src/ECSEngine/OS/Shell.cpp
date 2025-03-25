#include "ecspch.h"
#include "Shell.h"
#include "../Utilities/StringUtilities.h"
#include "../Utilities/StackScope.h"

namespace ECSEngine {

	namespace OS {

		bool ShellRunCommand(Stream<wchar_t> command, Stream<wchar_t> start_directory) {
			NULL_TERMINATE_WIDE_POTENTIAL_EMPTY(start_directory);

			ECS_STACK_CAPACITY_STREAM(wchar_t, full_command, ECS_KB * 8);
			full_command.Add(L'/');
			full_command.Add(L'c');
			full_command.Add(L' ');
			full_command.AddStreamAssert(command);
			full_command.AddAssert(L'\0');
			HINSTANCE value = ShellExecute(NULL, L"runas", L"C:\\Windows\\System32\\cmd.exe", full_command.buffer, start_directory.size == 0 ? NULL : start_directory.buffer, SW_HIDE);
			return (size_t)value >= 32;
		}

	}

}