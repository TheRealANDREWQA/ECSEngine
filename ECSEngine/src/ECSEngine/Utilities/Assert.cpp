#include "ecspch.h"
#include "Assert.h"
#include "Crash.h"

#define ECS_ASSERT_TRIGGER

namespace ECSEngine {

	bool ECS_GLOBAL_ASSERT_CRASH = false;

	namespace function {

		void Assert(bool condition, const char* filename, unsigned int line, const char* error_message)
		{
			if (ECS_GLOBAL_ASSERT_CRASH) {
				if (error_message == nullptr) {
					ECS_CRASH_EX("Assert changed into crash.", filename, "Assert", line);
				}
				else {
					ECS_FORMAT_TEMP_STRING(error_message_ex, "Assert crash from file {#} and file {#}.", filename, line);
					error_message_ex.AddStreamSafe(ToStream(error_message));
					Crash(error_message_ex.buffer);
				}
			}

			else {
#ifndef ECSENGINE_DISTRIBUTION
#ifdef ECS_ASSERT_TRIGGER
				if (!condition) {
					ECS_FORMAT_TEMP_STRING(temp_string, "[File] {#}\n[Line] {#}\n", filename, line);
					if (error_message != nullptr) {
						temp_string.AddStream(ToStream(error_message));
					}
					MessageBoxA(nullptr, temp_string.buffer, "ECS Assert", MB_OK | MB_ICONERROR);
					__debugbreak();
					::exit(0);
				}
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