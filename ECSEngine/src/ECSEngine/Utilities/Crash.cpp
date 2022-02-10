#include "ecspch.h"
#include "Crash.h"

namespace ECSEngine {

	struct CrashHandler {
		CrashHandlerFunction function;
		void* data;
	};

	CrashHandler global_crash_handler;

	void SetCrashHandler(CrashHandlerFunction function, void* data) {
		global_crash_handler.function = function;
		global_crash_handler.data = data;
	}

	void Crash(const char* error_string) {
		global_crash_handler.function(global_crash_handler.data, error_string);
	}

}