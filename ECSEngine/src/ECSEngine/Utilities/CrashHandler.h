#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include <setjmp.h>
#include "StackScope.h"
#include "Assert.h"

namespace ECSEngine {

	// Call the macro ECS_SET_RECOVERY_CRASH_HANDLER(should_break) for easier management.
	// The jump_buffer needs to be created on the stack frame where the SetRecoveryCrashHandler is called, that's
	// why we cannot store it internally on a stack frame that is being destroyed afterwards
	ECSENGINE_API void SetRecoveryCrashHandler(jmp_buf jump_buffer, bool should_break = false);

	// When the crash happens, it will jump to the point right after this call.
	// The first parameter can be used to automatically set ECS_GLOBAL_ASSERT_CRASH to true
	// And restore the previous state upon exit
	// Usage: ECS_SET_RECOVERY_CRASH_HANDLER(set_assert_to_crash, should_break) { /* Error handling code */ }
#define ECS_SET_RECOVERY_CRASH_HANDLER(set_assert_to_crash, should_break) \
	bool ___previous_assert_crash = ECS_GLOBAL_ASSERT_CRASH; \
	if (set_assert_to_crash) { \
		ECS_GLOBAL_ASSERT_CRASH = true; \
	} \
	jmp_buf __jump_buffer; \
	auto reset_handler_scope = StackScope([___previous_assert_crash, restore_assert_crash = set_assert_to_crash](){ \
		if (restore_assert_crash) { \
			ECS_GLOBAL_ASSERT_CRASH = ___previous_assert_crash;\
		} \
		ResetRecoveryCrashHandler(); \
	}); \
	if (!setjmp(__jump_buffer)) { SetRecoveryCrashHandler(__jump_buffer, should_break); } else  

	// The same as ECS_SET_RECOVERY_CRASH_HANDLER, but it provides default arguments.
	// Must be used like this ECS_SET_RECOVERY_CRASH_HANDLER { /* Error handling code */ }
#define ECS_SET_RECOVERY_CRASH_HANDLER_DEFAULT ECS_SET_RECOVERY_CRASH_HANDLER(true, false)

	// Call this inside ECS_SET_RECOVERY_CRASH_HANDLER only! When called, it will return an error message
	// From the Assert or Crash call
	ECSENGINE_API Stream<char> GetRecoveryCrashHandlerErrorMessage();

	// Changes the crash handler to the previous one (if it is the same, then it will reset the recorded jump)
	ECSENGINE_API void ResetRecoveryCrashHandler();
	
}