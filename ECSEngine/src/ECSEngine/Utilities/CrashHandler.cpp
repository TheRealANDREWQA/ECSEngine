#include "ecspch.h"
#include "CrashHandler.h"
#include "Crash.h"
#include "../ECS/World.h"
#include "OSFunctions.h"
#include "Console.h"

namespace ECSEngine {

	struct WorldCrashHandlerData {
		World* world;
		bool should_break;
	};

	WorldCrashHandlerData ABORT_WORLD_GLOBAL_DATA;
	WorldCrashHandlerData CONTINUE_WORLD_GLOBAL_DATA;

	static void AbortWorldCrashHandlerFunction(void* _data, Stream<char> error_string) {
		if (ABORT_WORLD_GLOBAL_DATA.should_break) {
			__debugbreak();
		}

		// Increment the global crash counter to see if another crash is in progress
		unsigned int last_progress_value = ECS_GLOBAL_CRASH_IN_PROGRESS.fetch_add(1, ECS_RELAXED);
		if (last_progress_value == 0) {
			// Try to suspend the other threads of the world in order to capture their stack trace
			TaskManager* task_manager = ABORT_WORLD_GLOBAL_DATA.world->task_manager;
			size_t this_thread_id = OS::GetCurrentThreadID();
			unsigned int thread_count = task_manager->GetThreadCount();

			bool failed_to_suspend_a_thread = false;
			unsigned int crashing_thread_id = -1;
			for (unsigned int index = 0; index < thread_count; index++) {
				size_t current_thread_id = OS::GetThreadID(task_manager->m_thread_handles[index]);
				if (this_thread_id != current_thread_id) {
					// We don't want to suspend ourselves since we need to suspend all the other threads
					//bool success = OS::SuspendThread(task_manager->m_thread_handles[index]);
					//failed_to_suspend_a_thread |= !success;
				}
				else {
					crashing_thread_id = index;
				}
			}

			// Now for each thread, retrieve its

			ECS_STACK_CAPACITY_STREAM(char, full_error_message, ECS_KB);
			full_error_message.CopyOther(error_string);
			full_error_message.AddStream("\nThe call stack of the crash: ");

			Console* console = GetConsole();

			// Get the call stack of the crash
			OS::GetCallStackFunctionNames(full_error_message);
			console->Error(full_error_message);
		}
	}

	static void ContinueWorldCrashHandlerFunction(void* _data, Stream<char> error_string) {
		if (ABORT_WORLD_GLOBAL_DATA.should_break) {
			__debugbreak();
		}

		// Try to 

		ECS_STACK_CAPACITY_STREAM(char, full_error_message, ECS_KB);
		full_error_message.CopyOther(error_string);
		full_error_message.AddStream("\nThe call stack of the crash: ");

		Console* console = GetConsole();

		// Get the call stack of the crash
		OS::GetCallStackFunctionNames(full_error_message);
		console->Error(full_error_message);
	}

	struct RecoveryCrashHandlerData {
		CrashHandler previous_handler;

		_JBTYPE* recovery_point;
		//jmp_buf recovery_point;
		bool should_break;
		bool is_valid;
	};

	RecoveryCrashHandlerData RECOVERY_GLOBAL_DATA;

	void RecoveryCrashHandlerFunction(void* _data, Stream<char> error_string) {
		RecoveryCrashHandlerData* data = (RecoveryCrashHandlerData*)_data;
		if (data->is_valid) {
			if (data->should_break) {
				__debugbreak();
			}
			longjmp(data->recovery_point, 1);
		}
	}

	void SetAbortWorldCrashHandler(World* world, Stream<Stream<wchar_t>> module_search_path, bool should_break) {
		OS::InitializeSymbolicLinksPaths(module_search_path);
		ABORT_WORLD_GLOBAL_DATA.world = world;
		ABORT_WORLD_GLOBAL_DATA.should_break = should_break;
		SetCrashHandler(AbortWorldCrashHandlerFunction, nullptr);
	}

	void SetContinueWorldCrashHandler(World* world, Stream<Stream<wchar_t>> module_search_path, bool should_break) {
		OS::InitializeSymbolicLinksPaths(module_search_path);
		CONTINUE_WORLD_GLOBAL_DATA.world = world;
		CONTINUE_WORLD_GLOBAL_DATA.should_break = should_break;
		SetCrashHandler(ContinueWorldCrashHandlerFunction, nullptr);
	}

	void SetRecoveryCrashHandler(jmp_buf jump_buffer, bool should_break)
	{
		RECOVERY_GLOBAL_DATA.previous_handler = ECS_GLOBAL_CRASH_HANDLER;
		RECOVERY_GLOBAL_DATA.should_break = should_break;
		RECOVERY_GLOBAL_DATA.is_valid = true;
		//size_t copy_size = sizeof(jmp_buf);
		//memcpy(&RECOVERY_GLOBAL_DATA.recovery_point, jump_buffer, copy_size);
		RECOVERY_GLOBAL_DATA.recovery_point = jump_buffer;
		SetCrashHandler(RecoveryCrashHandlerFunction, &RECOVERY_GLOBAL_DATA);
	}

	void ResetRecoveryCrashHandler()
	{
		if (ECS_GLOBAL_CRASH_HANDLER.function == RecoveryCrashHandlerFunction) {
			RECOVERY_GLOBAL_DATA.is_valid = false;
			SetCrashHandler(RECOVERY_GLOBAL_DATA.previous_handler.function, RECOVERY_GLOBAL_DATA.previous_handler.data);
			if (RECOVERY_GLOBAL_DATA.previous_handler.function == RecoveryCrashHandlerFunction) {
				RECOVERY_GLOBAL_DATA.is_valid = false;
			}
		}
	}

}