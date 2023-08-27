#include "ecspch.h"
#include "Assert.h"
#include "Crash.h"
#include "../Allocators/AllocatorCallsDebug.h"

#define ECS_ASSERT_TRIGGER

namespace ECSEngine {

	bool ECS_GLOBAL_ASSERT_CRASH = false;
	bool ECS_GLOBAL_ASSERT_WRITE_DEBUG_ALLOCATOR_CALLS = true;

	namespace function {

		void Assert(bool condition, const char* filename, unsigned int line, const char* error_message)
		{
			if (!condition) {
				if (ECS_GLOBAL_ASSERT_CRASH) {
					if (error_message == nullptr) {
						ECS_CRASH_EX("Assert changed into crash.", filename, "Assert", line);
					}
					else {
						ECS_FORMAT_TEMP_STRING(error_message_ex, "Assert crash from file {#} and line {#}.", filename, line);
						error_message_ex.AddStreamSafe(error_message);
						Crash(error_message_ex.buffer);
					}
				}

				else {
#ifndef ECSENGINE_DISTRIBUTION
#ifdef ECS_ASSERT_TRIGGER
					if (ECS_GLOBAL_ASSERT_WRITE_DEBUG_ALLOCATOR_CALLS) {
						DebugAllocatorManagerWriteState();
					}

					ECS_FORMAT_TEMP_STRING(temp_string, "[File] {#}\n[Line] {#}\n", filename, line);
					if (error_message != nullptr) {
						temp_string.AddStream(error_message);
					}
					MessageBoxA(nullptr, temp_string.buffer, "ECS Assert", MB_OK | MB_ICONERROR);
					__debugbreak();
					::exit(0);
#endif
#else
					if (!condition) {
						__debugbreak();
					}
#endif
				}
			}
		}

	}

}