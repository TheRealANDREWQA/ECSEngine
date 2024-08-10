#include "ecspch.h"
#include "WorldCrashHandler.h"
#include "../OS/DLL.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Console.h"
#include "../Utilities/Utilities.h"
#include "RuntimeCrashPersistence.h"
#include "World.h"

namespace ECSEngine {

	static OS::ThreadContext ECS_WORLD_CRASH_HANDLER_THREAD_CONTEXT;
	static bool ECS_WORLD_CRASH_HANDLER_THREAD_CONTEXT_SET = false;

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

		auto return_thread_to_procedure = []() {
			TaskManager* task_manager = WORLD_GLOBAL_DATA.descriptor.world->task_manager;

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
		};

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
			write_options.create_unique_folder_entry = true;
			if (ECS_WORLD_CRASH_HANDLER_THREAD_CONTEXT_SET) {
				write_options.crashing_thread_context = &ECS_WORLD_CRASH_HANDLER_THREAD_CONTEXT;
			}

			ECS_STACK_CAPACITY_STREAM(wchar_t, crash_directory_storage, 512);
			Stream<wchar_t> crash_directory = RuntimeCrashPersistenceWriteDirectory(WORLD_GLOBAL_DATA.descriptor.crash_directory, &write_options, crash_directory_storage);

			SaveSceneData save_data;
			save_data.delta_time = WORLD_GLOBAL_DATA.descriptor.world->delta_time;
			save_data.speed_up_factor = WORLD_GLOBAL_DATA.descriptor.world->speed_up_factor;
			save_data.asset_database = WORLD_GLOBAL_DATA.descriptor.asset_database;
			save_data.unique_overrides = WORLD_GLOBAL_DATA.descriptor.unique_infos;
			save_data.shared_overrides = WORLD_GLOBAL_DATA.descriptor.shared_infos;
			save_data.global_overrides = WORLD_GLOBAL_DATA.descriptor.global_infos;

			bool should_write_crash = true;
			if (WORLD_GLOBAL_DATA.descriptor.pre_callback.function != nullptr) {
				WorldCrashHandlerPreCallbackFunctionData callback_data;
				callback_data.write_options = &write_options;
				callback_data.save_data = &save_data;
				callback_data.user_data = WORLD_GLOBAL_DATA.descriptor.pre_callback.data;
				callback_data.crash_directory = crash_directory;
				should_write_crash = WORLD_GLOBAL_DATA.descriptor.pre_callback.function(&callback_data);
			}

			bool write_success = false;
			if (should_write_crash) {
				// We can call the persistence function now after all the setup was done
				write_success = RuntimeCrashPersistenceWrite(
					crash_directory,
					WORLD_GLOBAL_DATA.descriptor.world,
					WORLD_GLOBAL_DATA.descriptor.reflection_manager,
					&save_data,
					&write_options
				);
				// now we can reset the thread context, in case there is any
				ECS_WORLD_CRASH_HANDLER_THREAD_CONTEXT_SET = false;

				if (!write_success) {
					// Check to see if the thread resuming was successful, if it was not, retry again here
					if (!resume_threads_success) {
						resume_threads_success = WORLD_GLOBAL_DATA.descriptor.world->task_manager->ResumeThreads();
					}
				}
			}

			if (WORLD_GLOBAL_DATA.descriptor.post_callback.function != nullptr) {
				WorldCrashHandlerPostCallbackFunctionData callback_data;
				callback_data.crash_write_success = write_success;
				callback_data.resuming_threads_success = resume_threads_success;
				callback_data.suspending_threads_success = suspend_success;
				callback_data.user_data = WORLD_GLOBAL_DATA.descriptor.post_callback.data;
				callback_data.save_error_message = *write_options.error_message;
				callback_data.error_message = error_string;
				callback_data.crash_directory = crash_directory;
				WORLD_GLOBAL_DATA.descriptor.post_callback.function(&callback_data);
			}

			if (WORLD_GLOBAL_DATA.is_abort_handler) {
				abort();
			}
			else {
				// We should signal the static tasks for those threads that are stuck in barriers
				task_manager->SignalStaticTasks();

				return_thread_to_procedure();
			}
		}
		else {
			// Check to see if we are the crashing thread during the save
			// If that is the case, just abort since there is nothing that can be done at that point
			if (ECS_GLOBAL_CRASH_OS_THREAD_ID == OS::GetCurrentThreadID()) {
				abort();
			}
			else {
				// We need to reset the thread to the start of the procedure still
				return_thread_to_procedure();
			}
		}
	}

	static CrashHandler GetFunction(const SetWorldCrashHandlerDescriptor* descriptor, bool is_abort) {
		if (WORLD_GLOBAL_DATA.descriptor.crash_directory.buffer != nullptr) {
			Free(WORLD_GLOBAL_DATA.descriptor.crash_directory.buffer);
		}
		if (WORLD_GLOBAL_DATA.descriptor.pre_callback.data_size > 0) {
			Free(WORLD_GLOBAL_DATA.descriptor.pre_callback.data);
		}
		if (WORLD_GLOBAL_DATA.descriptor.post_callback.data_size > 0) {
			Free(WORLD_GLOBAL_DATA.descriptor.post_callback.data);
		}
		if (!WORLD_GLOBAL_DATA.descriptor.infos_are_stable) {
			// These are coalesced into a single allocation
			Free(WORLD_GLOBAL_DATA.descriptor.unique_infos.buffer);
			Free(WORLD_GLOBAL_DATA.descriptor.shared_infos.buffer);
			Free(WORLD_GLOBAL_DATA.descriptor.global_infos.buffer);
		}

		if (descriptor->module_search_paths.size > 0) {
			OS::SetSymbolicLinksPaths(descriptor->module_search_paths);
		}
		WORLD_GLOBAL_DATA.descriptor = *descriptor;
		WORLD_GLOBAL_DATA.is_abort_handler = is_abort;

		WORLD_GLOBAL_DATA.descriptor.crash_directory.InitializeEx({ nullptr }, descriptor->crash_directory.size);
		WORLD_GLOBAL_DATA.descriptor.crash_directory.CopyOther(descriptor->crash_directory);

		WORLD_GLOBAL_DATA.descriptor.pre_callback.data = CopyNonZeroMalloc(descriptor->pre_callback.data, descriptor->pre_callback.data_size);
		WORLD_GLOBAL_DATA.descriptor.post_callback.data = CopyNonZeroMalloc(descriptor->post_callback.data, descriptor->post_callback.data_size);

		if (!descriptor->infos_are_stable) {
			WORLD_GLOBAL_DATA.descriptor.unique_infos = StreamCoalescedDeepCopy(descriptor->unique_infos, { nullptr });
			WORLD_GLOBAL_DATA.descriptor.shared_infos = StreamCoalescedDeepCopy(descriptor->shared_infos, { nullptr });
			WORLD_GLOBAL_DATA.descriptor.global_infos = StreamCoalescedDeepCopy(descriptor->global_infos, { nullptr });
		}

		return { WorldCrashHandlerFunction, nullptr };
	}

	static void SetFunction(const SetWorldCrashHandlerDescriptor* descriptor, bool is_abort) {
		CrashHandler handler = GetFunction(descriptor, is_abort);
		SetCrashHandler(handler);
	}

	void SetWorldCrashHandlerThreadContext(const OS::ThreadContext* thread_context)
	{
		memcpy(&ECS_WORLD_CRASH_HANDLER_THREAD_CONTEXT, thread_context, sizeof(*thread_context));
		ECS_WORLD_CRASH_HANDLER_THREAD_CONTEXT_SET = true;
	}

	CrashHandler GetAbortWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor) {
		return GetFunction(descriptor, true);
	}

	CrashHandler GetContinueWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor) {
		return GetFunction(descriptor, false);
	}

	void SetAbortWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor) {
		SetFunction(descriptor, true);
	}

	void SetContinueWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor) {
		SetFunction(descriptor, false);
	}

	static OS::ECS_OS_EXCEPTION_CONTINUE_STATUS WorldTaskManagerExceptionHandler(TaskManagerExceptionHandlerData* function_data) {
		SetWorldCrashHandlerThreadContext(&function_data->exception_information.thread_context);
		return OS::ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
	}

	void SetWorldCrashHandlerTaskManagerExceptionHandler(TaskManager* task_manager)
	{
		task_manager->PushExceptionHandler(WorldTaskManagerExceptionHandler, nullptr, 0);
	}

}