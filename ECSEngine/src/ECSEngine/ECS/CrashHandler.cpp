#include "ecspch.h"
#include "CrashHandler.h"
#include "../Utilities/OSFunctions.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Console.h"
#include "../Utilities/Utilities.h"
#include "RuntimeCrashPersistence.h"
#include "World.h"

namespace ECSEngine {

	struct WorldCrashHandlerData {
		SetWorldCrashHandlerDescriptor descriptor;
		bool is_abort_handler;
	};

	WorldCrashHandlerData WORLD_GLOBAL_DATA;

	static void WorldCrashHandlerFunction(void* _data, Stream<char> error_string) {
		// Increment the global crash counter to see if another crash is in progress
		unsigned int last_progress_value = ECS_GLOBAL_CRASH_IN_PROGRESS.fetch_add(1, ECS_RELAXED);
		
		// Break after the increment
		if (WORLD_GLOBAL_DATA.descriptor.should_break) {
			__debugbreak();
		}

		if (last_progress_value == 0) {
			// Set the crashing thread ID
			ECS_GLOBAL_CRASH_OS_THREAD_ID = OS::GetCurrentThreadID();

			ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB * 8);

			TaskManager* task_manager = WORLD_GLOBAL_DATA.descriptor.world->task_manager;
			// Suspend the threads now since the setup, including the callback might take a bit of
			// time and affect the stack trace
			bool suspend_success = task_manager->SuspendThreads();
			
			bool resume_threads_success = false;

			// We are the ones to do the crash persistence
			RuntimeCrashPersistenceWriteOptions write_options;
			write_options.dont_stop_on_first_failure = true;
			write_options.error_message = &error_message;
			write_options.resume_threads = !WORLD_GLOBAL_DATA.is_abort_handler && suspend_success;
			write_options.resuming_threads_success = &resume_threads_success;
			write_options.world_descriptor = WORLD_GLOBAL_DATA.descriptor.world_descriptor;

			SaveSceneData save_data;
			save_data.delta_time = WORLD_GLOBAL_DATA.descriptor.world->delta_time;
			save_data.asset_database = WORLD_GLOBAL_DATA.descriptor.asset_database;
			
			if (WORLD_GLOBAL_DATA.descriptor.pre_callback.function != nullptr) {
				WorldCrashHandlerPreCallbackFunctionData callback_data;
				callback_data.write_options = &write_options;
				callback_data.save_data = &save_data;
				callback_data.user_data = WORLD_GLOBAL_DATA.descriptor.pre_callback.data;
				WORLD_GLOBAL_DATA.descriptor.pre_callback.function(&callback_data);
			}

			// We can call the persistence function now after all the setup was done
			bool write_success = RuntimeCrashPersistenceWrite(
				WORLD_GLOBAL_DATA.descriptor.crash_directory,
				WORLD_GLOBAL_DATA.descriptor.world,
				WORLD_GLOBAL_DATA.descriptor.reflection_manager,
				&save_data,
				&write_options
			);

			if (WORLD_GLOBAL_DATA.descriptor.post_callback.function != nullptr) {
				WorldCrashHandlerPostCallbackFunctionData callback_data;
				callback_data.crash_write_success = write_success;
				callback_data.resuming_threads_success = resume_threads_success;
				callback_data.suspending_threads_success = suspend_success;
				callback_data.user_data = WORLD_GLOBAL_DATA.descriptor.post_callback.data;
				callback_data.error_message = *write_options.error_message;
				WORLD_GLOBAL_DATA.descriptor.post_callback.function(&callback_data);
			}

			if (WORLD_GLOBAL_DATA.is_abort_handler) {
				abort();
			}
			else {
				// Decrement the wait counter for the task manager for that world
				// To indicate that this thread exited
				task_manager->m_is_frame_done.Notify();

				unsigned int this_thread_id = task_manager->FindThreadID(OS::GetCurrentThreadID());
				if (this_thread_id == -1) {
					// Abort in this case
					__debugbreak();
					abort();
				}
				// Now we need to jump to the task manager thread procedure
				// We want to sleep after returning
				task_manager->ResetThreadToProcedure(this_thread_id, true);
			}
		}
		else {
			// Check to see if we are the crashing thread during the save
			// If that is the case, just abort since there is nothing that can be done at that point
			if (ECS_GLOBAL_CRASH_OS_THREAD_ID == OS::GetCurrentThreadID()) {
				abort();
			}
		}
	}

	static void SetFunction(const SetWorldCrashHandlerDescriptor* descriptor, bool is_abort) {
		if (WORLD_GLOBAL_DATA.descriptor.crash_directory.buffer != nullptr) {
			free(WORLD_GLOBAL_DATA.descriptor.crash_directory.buffer);
		}
		if (WORLD_GLOBAL_DATA.descriptor.pre_callback.data_size > 0) {
			free(WORLD_GLOBAL_DATA.descriptor.pre_callback.data);
		}
		if (WORLD_GLOBAL_DATA.descriptor.post_callback.data_size > 0) {
			free(WORLD_GLOBAL_DATA.descriptor.post_callback.data);
		}

		OS::InitializeSymbolicLinksPaths(descriptor->module_search_paths);
		WORLD_GLOBAL_DATA.descriptor = *descriptor;
		WORLD_GLOBAL_DATA.is_abort_handler = is_abort;
		
		wchar_t* allocation = (wchar_t*)malloc(descriptor->crash_directory.MemoryOf(descriptor->crash_directory.size));
		uintptr_t allocation_ptr = (uintptr_t)allocation;
		WORLD_GLOBAL_DATA.descriptor.crash_directory.InitializeAndCopy(allocation_ptr, descriptor->crash_directory);

		WORLD_GLOBAL_DATA.descriptor.pre_callback.data = CopyNonZeroMalloc(descriptor->pre_callback.data, descriptor->pre_callback.data_size);
		WORLD_GLOBAL_DATA.descriptor.post_callback.data = CopyNonZeroMalloc(descriptor->post_callback.data, descriptor->post_callback.data_size);

		SetCrashHandler(WorldCrashHandlerFunction, nullptr);
	}

	void SetAbortWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor) {
		SetFunction(descriptor, true);
	}

	void SetContinueWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor) {
		SetFunction(descriptor, false);
	}

}