#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include <setjmp.h>

namespace ECSEngine {

	struct World;

	// The paths must be absolute paths, not relative ones
	ECSENGINE_API void SetAbortWorldCrashHandler(World* world, Stream<Stream<wchar_t>> module_search_paths, bool should_break = true);

	ECSENGINE_API void SetContinueWorldCrashHandler(World* world, Stream<Stream<wchar_t>> module_search_paths, bool should_break = true);

	// Call the macro ECS_SET_RECOVERY_CRASH_HANDLER(should_break) for easier management.
	// The jump_buffer needs to be created on the stack frame where the SetRecoveryCrashHandler is called, that's
	// why we cannot store it internally on a stack frame that is being destroyed afterwards
	ECSENGINE_API void SetRecoveryCrashHandler(jmp_buf jump_buffer, bool should_break = false);

	// When the crash happens, it will jump to the point right after this call
	// Usage: ECS_SET_RECOVERY_CRASH_HANDLER(should_break) { ResetRecoveryCrashHandler(); /* Error handling code */ }
#define ECS_SET_RECOVERY_CRASH_HANDLER(should_break) jmp_buf __jump_buffer; if (!setjmp(__jump_buffer)) { SetRecoveryCrashHandler(__jump_buffer); } else  

	// Changes the crash handler to the previous one (if it is the same, then it will reset the recorded jump)
	ECSENGINE_API void ResetRecoveryCrashHandler();
	
}