#include "ecspch.h"
#include "CrashHandler.h"
#include "Crash.h"

namespace ECSEngine {

	struct RecoveryCrashHandlerData {
		CrashHandler previous_handler;

		_JBTYPE* recovery_point;
		//jmp_buf recovery_point;
		bool should_break;
		bool is_valid;
		// Keeps track of the nested count of this
		unsigned char nested_count = 0;
	};

	RecoveryCrashHandlerData RECOVERY_GLOBAL_DATA;

	// This is set after a crash call, such that the actual handler can use this string
	static Stream<char> RECOVERY_CRASH_ERROR_STRING;

	void RecoveryCrashHandlerFunction(void* _data, Stream<char> error_string) {
		RecoveryCrashHandlerData* data = (RecoveryCrashHandlerData*)_data;
		RECOVERY_CRASH_ERROR_STRING = error_string;
		if (data->is_valid) {
			if (data->should_break) {
				__debugbreak();
			}
			longjmp(data->recovery_point, 1);
		}
	}

	void SetRecoveryCrashHandler(jmp_buf jump_buffer, bool should_break)
	{
		// If the current crash handler is also a recovery crash handler
		if (ECS_GLOBAL_CRASH_HANDLER.function != RecoveryCrashHandlerFunction) {
			RECOVERY_GLOBAL_DATA.previous_handler = ECS_GLOBAL_CRASH_HANDLER;
			RECOVERY_GLOBAL_DATA.should_break = should_break;
			RECOVERY_GLOBAL_DATA.is_valid = true;
			RECOVERY_GLOBAL_DATA.nested_count = 1;
		}
		else {
			RECOVERY_GLOBAL_DATA.nested_count++;
		}
		//size_t copy_size = sizeof(jmp_buf);
		//memcpy(&RECOVERY_GLOBAL_DATA.recovery_point, jump_buffer, copy_size);
		RECOVERY_GLOBAL_DATA.recovery_point = jump_buffer;
		SetCrashHandler(RecoveryCrashHandlerFunction, &RECOVERY_GLOBAL_DATA);
	}

	Stream<char> GetRecoveryCrashHandlerErrorMessage()
	{
		return RECOVERY_CRASH_ERROR_STRING;
	}

	void ResetRecoveryCrashHandler()
	{
		if (ECS_GLOBAL_CRASH_HANDLER.function == RecoveryCrashHandlerFunction) {
			RECOVERY_GLOBAL_DATA.nested_count--;
			// If the nested count reached 0, it means all the nested entries have been cleared
			if (RECOVERY_GLOBAL_DATA.nested_count == 0) {
				RECOVERY_GLOBAL_DATA.is_valid = false;
				SetCrashHandler(RECOVERY_GLOBAL_DATA.previous_handler.function, RECOVERY_GLOBAL_DATA.previous_handler.data);
				if (RECOVERY_GLOBAL_DATA.previous_handler.function == RecoveryCrashHandlerFunction) {
					RECOVERY_GLOBAL_DATA.is_valid = false;
				}
			}
		}
	}

}