#include "ecspch.h"
#include "Assert.h"
#include "Crash.h"
#include "../Allocators/AllocatorCallsDebug.h"
#include "StringUtilities.h"
#include "../Utilities/OSFunctions.h"

namespace ECSEngine {

	bool ECS_GLOBAL_ASSERT_CRASH = false;
	bool ECS_GLOBAL_ASSERT_WRITE_DEBUG_ALLOCATOR_CALLS = true;

	static void HardAssertImpl(const char* filename, const char* function, unsigned int line, const char* error_message) {
		if (ECS_GLOBAL_ASSERT_WRITE_DEBUG_ALLOCATOR_CALLS) {
			DebugAllocatorManagerWriteState();
		}

		ECS_FORMAT_TEMP_STRING(temp_string, "[File] {#}\n[Function] {#}\n[Line] {#}\n", filename, function, line);
		if (error_message != nullptr) {
			temp_string.AddStream(error_message);
		}
		temp_string.AddAssert('\0');
		OS::OSMessageBox(temp_string.buffer, "ECS Assert");
		__debugbreak();
		::exit(0);
	}

	void Assert(bool condition, const char* filename, const char* function, unsigned int line, const char* error_message)
	{
		if (!condition) {
			if (ECS_GLOBAL_ASSERT_CRASH) {
				if (error_message == nullptr) {
					ECS_CRASH_EX("Assert changed into crash.", filename, "Assert", line);
				}
				else {
					ECS_FORMAT_TEMP_STRING(error_message_ex, "Assert crash from file {#} in function {#} on line {#}.", filename, function, line);
					error_message_ex.AddStreamSafe(error_message);
					Crash(error_message_ex.buffer);
				}
			}
			else {
				HardAssertImpl(filename, function, line, error_message);
			}
		}
	}

	void HardAssert(bool condition, const char* filename, const char* function, unsigned int line, const char* error_message) {
		if (!condition) {
			HardAssertImpl(filename, function, line, error_message);
		}
	}

}