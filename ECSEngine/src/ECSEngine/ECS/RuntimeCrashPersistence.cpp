#include "ecspch.h"
#include "RuntimeCrashPersistence.h"
#include "../Resources/Scene.h"
#include "../Utilities/Console.h"
#include "World.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	static const wchar_t* FILE_STRINGS[] = {
		L".scene",
		L".console",
		L".stack_trace",
		L".scheduler",
		L".world_descriptor",
		L".misc0",
		L".misc1",
		L".misc2",
		L".misc3"
	};

	static_assert(std::size(FILE_STRINGS) == ECS_RUNTIME_CRASH_FILE_TYPE_COUNT);

	void RuntimeCrashPersistenceFilePath(Stream<wchar_t> directory, CapacityStream<wchar_t>* file_path, ECS_RUNTIME_CRASH_FILE_TYPE type)
	{
		file_path->CopyOther(directory);
		file_path->Add(ECS_OS_PATH_SEPARATOR);
		file_path->AddStreamAssert(FILE_STRINGS[type]);
	}

	void RuntimeCrashPersistenceFilePath(CapacityStream<wchar_t>* directory, ECS_RUNTIME_CRASH_FILE_TYPE type)
	{
		directory->Add(ECS_OS_PATH_SEPARATOR);
		directory->AddStreamAssert(FILE_STRINGS[type]);
	}

	static void WriteErrorMessage(const RuntimeCrashPersistenceWriteOptions* options, Stream<char> error_message) {
		if (options->error_message) {
			options->error_message->AddStreamAssert(error_message);
		}
	}

	static void WriteErrorMessage(const RuntimeCrashPersistenceReadOptions* options, Stream<char> error_message) {
		if (options->error_message) {
			options->error_message->AddStreamAssert(error_message);
		}
	}

#pragma region Write

	// ------------------------------------------------------------------------------------------------------------

	static bool WriteSceneFile(
		CapacityStream<wchar_t>& mutable_directory,
		const World* world,
		const Reflection::ReflectionManager* reflection_manager,
		const SaveSceneData* save_scene_data,
		const RuntimeCrashPersistenceWriteOptions* options
	) {
		if (save_scene_data->asset_database == nullptr) {
			// Just for safety
			__debugbreak();
			return false;
		}
		
		RuntimeCrashPersistenceFilePath(&mutable_directory, ECS_RUNTIME_CRASH_FILE_SCENE);

		SaveSceneData save_data = *save_scene_data;
		save_data.entity_manager = world->entity_manager;
		save_data.file = mutable_directory;
		save_data.reflection_manager = reflection_manager;
		save_data.delta_time = world->delta_time;
		bool success = SaveScene(&save_data);
		if (!success) {
			WriteErrorMessage(options, "Failed to write scene file\n");
			if (!options->dont_stop_on_first_failure) {
				return false;
			}
		}
		return true;
	}

	static bool WriteConsoleFile(CapacityStream<wchar_t>& mutable_directory, const RuntimeCrashPersistenceWriteOptions* options) {
		RuntimeCrashPersistenceFilePath(&mutable_directory, ECS_RUNTIME_CRASH_FILE_CONSOLE);
		Console* console = GetConsole();
		bool success = console->DumpToFile(mutable_directory);
		if (!success) {
			WriteErrorMessage(options, "Failed to write console dump\n");
			if (!options->dont_stop_on_first_failure) {
				return false;
			}
		}

		return true;
	}

	static bool WriteStackTraceFile(CapacityStream<wchar_t>& mutable_directory, const World* world, const RuntimeCrashPersistenceWriteOptions* options) {
		// Now get the call stack of each thread
		ECS_STACK_CAPACITY_STREAM(char, stack_trace, ECS_KB * 128);
		const TaskManager* task_manager = world->task_manager;
		unsigned int thread_count = task_manager->GetThreadCount();
		for (unsigned int index = 0; index < thread_count; index++) {
			OS::ThreadContext thread_context;
			bool success = OS::CaptureThreadStackContext(task_manager->m_thread_handles[index], &thread_context);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(message, "Failed to retrieve thread {#} stack context\n", index);
				WriteErrorMessage(options, message);
			}
			else {
				stack_trace.AddStreamAssert("Thread ");
				ConvertIntToChars(stack_trace, index);
				stack_trace.AddAssert(' ');
				OS::GetCallStackFunctionNames(&thread_context, stack_trace);
			}
		}

		RuntimeCrashPersistenceFilePath(&mutable_directory, ECS_RUNTIME_CRASH_FILE_STACK_TRACE);
		// We can write the stack trace file now
		ECS_FILE_STATUS_FLAGS status = WriteBufferToFileBinary(mutable_directory, stack_trace);
		if (status != ECS_FILE_STATUS_OK) {
			WriteErrorMessage(options, "Failed to write the stack trace file\n");
			if (!options->dont_stop_on_first_failure) {
				return false;
			}
		}

		return true;
	}

	static bool WriteSchedulerFile(CapacityStream<wchar_t>& mutable_directory, const World* world, const RuntimeCrashPersistenceWriteOptions* options) {
		const TaskScheduler* task_scheduler = world->task_scheduler;
		ECS_STACK_CAPACITY_STREAM(char, scheduler_string, ECS_KB * 128);
		AdditionStream<char> addition_stream = &scheduler_string;
		task_scheduler->StringifyScheduleOrder(addition_stream);

		RuntimeCrashPersistenceFilePath(&mutable_directory, ECS_RUNTIME_CRASH_FILE_SCHEDULER_ORDER);
		ECS_FILE_STATUS_FLAGS status = WriteBufferToFileBinary(mutable_directory, scheduler_string);
		if (status != ECS_FILE_STATUS_OK) {
			WriteErrorMessage(options, "Failed to write the task scheduler order file\n");
			if (!options->dont_stop_on_first_failure) {
				return false;
			}
		}

		return true;
	}

	static bool WriteWorldDescriptorFile(
		CapacityStream<wchar_t>& mutable_directory, 
		const World* world, 
		const Reflection::ReflectionManager* reflection_manager,
		const RuntimeCrashPersistenceWriteOptions* options
	) {
		RuntimeCrashPersistenceFilePath(&mutable_directory, ECS_RUNTIME_CRASH_FILE_WORLD_DESCRIPTOR);
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);

		SerializeOptions serialize_options;
		serialize_options.error_message = options->error_message;
		serialize_options.allocator = GetAllocatorPolymorphic(&stack_allocator);
		ECS_SERIALIZE_CODE serialize_code = Serialize(
			reflection_manager, 
			reflection_manager->GetType(STRING(WorldDescriptor)), 
			options->world_descriptor, 
			mutable_directory,
			&serialize_options
		);
	
		bool success = serialize_code == ECS_SERIALIZE_OK;
		if (!success) {
			WriteErrorMessage(options, "Failed to write world descriptor file\n");
			if (!options->dont_stop_on_first_failure) {
				return false;
			}
		}
		return true;
	}

	static bool WriteMiscFiles(CapacityStream<wchar_t>& mutable_directory, const RuntimeCrashPersistenceWriteOptions* options) {
		ECS_RUNTIME_CRASH_FILE_TYPE file_type = ECS_RUNTIME_CRASH_FILE_MISC_0;
		unsigned int directory_size = mutable_directory.size;
		for (unsigned int index = 0; index < ECS_RUNTIME_CRASH_PERSISTENCE_MISC_COUNT; index++) {
			mutable_directory.size = directory_size;
			RuntimeCrashPersistenceFilePath(&mutable_directory, file_type);
			if (options->misc_data[index].size > 0) {
				// Write the file using the given data
				bool success = WriteBufferToFileBinary(mutable_directory, options->misc_data[index]);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(message, "Failed to write misc file {#}\n", index);
					WriteErrorMessage(options, message);
					if (!options->dont_stop_on_first_failure) {
						return false;
					}
				}
			}
			else if (options->callbacks[index] != nullptr) {
				// Use the callback
				bool success = options->callbacks[index](mutable_directory, options->callbacks_data[index]);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(message, "Failed to write misc file {#} using callback\n", index);
					WriteErrorMessage(options, message);
					if (!options->dont_stop_on_first_failure) {
						return false;
					}
				}
			}
			file_type = (ECS_RUNTIME_CRASH_FILE_TYPE)(file_type + 1);
		}

		return true;
	}

	bool RuntimeCrashPersistenceWrite(
		Stream<wchar_t> directory,
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		const SaveSceneData* save_scene_data,
		const RuntimeCrashPersistenceWriteOptions* options
	) {
		// Firstly check the suspend flag, since we need to capture an accurate description of the stack trace
		const TaskManager* task_manager = world->task_manager;
		if (options->suspend_threads) {
			bool managed_to_suspend_threads = task_manager->SuspendThreads();

			if (options->suspending_threads_success) {
				*options->suspending_threads_success = managed_to_suspend_threads;
			}
			if (!managed_to_suspend_threads) {
				WriteErrorMessage(options, "Failed to suspend the threads\n");
				if (!options->dont_stop_on_first_failure) {
					return false;
				}
			}
		}

		// Resume the threads if we have to
		auto resume_threads = StackScope([options, world]() {
			if (options->resume_threads) {
				// We can resume the threads immediately after retrieving the stack trace
				bool resume_success = world->task_manager->ResumeThreads();

				if (options->resuming_threads_success) {
					*options->resuming_threads_success = resume_success;
				}
				if (!resume_success) {
					WriteErrorMessage(options, "Failed to resume the threads\n");
				}
			}
		});

		ECS_STACK_CAPACITY_STREAM(wchar_t, mutable_directory, 512);
		mutable_directory.CopyOther(directory);

		// Start with the scene file since it's the most important
		bool success = WriteSceneFile(mutable_directory, world, reflection_manager, save_scene_data, options);
		if (!success) {
			return false;
		}

		// Continue with the descriptor file as it is the 2nd most important one
		if (options->world_descriptor) {
			mutable_directory.size = directory.size;
			success = WriteWorldDescriptorFile(mutable_directory, world, reflection_manager, options);
			if (!success) {
				return false;
			}
		}

		success = WriteConsoleFile(mutable_directory, options);
		if (!success) {
			return false;
		}

		mutable_directory.size = directory.size;
		success = WriteStackTraceFile(mutable_directory, world, options);
		if (!success) {
			return false;
		}
		
		mutable_directory.size = directory.size;
		success = WriteSchedulerFile(mutable_directory, world, options);
		if (!success) {
			return false;
		}

		// At last the misc files
		mutable_directory.size = directory.size;
		success = WriteMiscFiles(mutable_directory, options);
		if (!success) {
			return false;
		}

		return true;
	}

#pragma endregion

#pragma region Read

	// ------------------------------------------------------------------------------------------------------------

	static bool ReadSceneFile(
		CapacityStream<wchar_t>& mutable_directory,
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		LoadSceneData* load_scene_data,
		RuntimeCrashPersistenceReadOptions* options
	) {
		RuntimeCrashPersistenceFilePath(&mutable_directory, ECS_RUNTIME_CRASH_FILE_SCENE);

		LoadSceneData load_data = *load_scene_data;
		load_data.delta_time = &world->delta_time;
		load_data.detailed_error_string = options->error_message;
		load_data.allow_missing_components = false;
		load_data.entity_manager = world->entity_manager;
		load_data.file = mutable_directory;
		load_data.randomize_assets = true;
		load_data.reflection_manager = reflection_manager;

		bool success = LoadScene(&load_data);
		options->file_load_success[ECS_RUNTIME_CRASH_FILE_SCENE] = success;
		if (!success) {
			WriteErrorMessage(options, "Failed to read scene\n");
		}

		return success;
	}

	static Stream<char> ReadTextFile(
		CapacityStream<wchar_t>& mutable_directory,
		RuntimeCrashPersistenceReadOptions* options,
		ECS_RUNTIME_CRASH_FILE_TYPE file_type,
		Stream<char> error_message,
		AllocatorPolymorphic allocator
	) {
		RuntimeCrashPersistenceFilePath(&mutable_directory, file_type);
		Stream<void> output = ReadWholeFileBinary(mutable_directory, allocator);
		if (output.size == 0) {
			WriteErrorMessage(options, error_message);
			options->file_load_success[file_type] = false;
		}
		else {
			options->file_load_success[file_type] = true;
		}

		return output.AsIs<char>();
	}

	static bool ReadConsoleFile(CapacityStream<wchar_t>& mutable_directory, RuntimeCrashPersistenceReadOptions* options) {
		Stream<char> data = ReadTextFile(mutable_directory, options, ECS_RUNTIME_CRASH_FILE_CONSOLE, "Failed to read console file\n", options->console_allocator);
		*options->console = data;
		return options->file_load_success[ECS_RUNTIME_CRASH_FILE_CONSOLE];
	}

	static bool ReadStackTraceFile(CapacityStream<wchar_t>& mutable_directory, RuntimeCrashPersistenceReadOptions* options) {
		Stream<char> data = ReadTextFile(
			mutable_directory, 
			options, 
			ECS_RUNTIME_CRASH_FILE_STACK_TRACE, 
			"Failed to read console stack trace file\n", 
			options->stack_trace_allocator
		);
		*options->stack_trace = data;
		return options->file_load_success[ECS_RUNTIME_CRASH_FILE_STACK_TRACE];
	}

	static bool ReadTaskSchedulerInfo(CapacityStream<wchar_t>& mutable_directory, RuntimeCrashPersistenceReadOptions* options) {
		Stream<char> data = ReadTextFile(
			mutable_directory, 
			options, 
			ECS_RUNTIME_CRASH_FILE_SCHEDULER_ORDER, 
			"Failed to read task scheduler order file\n", 
			options->task_scheduler_allocator
		);
		*options->task_scheduler_info = data;
		return options->file_load_success[ECS_RUNTIME_CRASH_FILE_SCHEDULER_ORDER];
	}

	bool RuntimeCrashPersistenceReadMiscFiles(Stream<wchar_t> directory, RuntimeCrashPersistenceReadOptions* options) {
		bool success = true;

		ECS_STACK_CAPACITY_STREAM(wchar_t, mutable_directory, 512);
		mutable_directory.CopyOther(directory);

		ECS_RUNTIME_CRASH_FILE_TYPE file_type = ECS_RUNTIME_CRASH_FILE_MISC_0;
		for (unsigned int index = 0; index < ECS_RUNTIME_CRASH_PERSISTENCE_MISC_COUNT; index++) {
			mutable_directory.size = directory.size;
			RuntimeCrashPersistenceFilePath(&mutable_directory, file_type);
			if (options->misc_data != nullptr) {
				Stream<void> data = ReadWholeFileBinary(mutable_directory, options->misc_data_allocator);
				if (data.size == 0) {
					ECS_FORMAT_TEMP_STRING(message, "Failed to read misc file {#}\n", index);
					WriteErrorMessage(options, message);
					options->file_load_success[file_type] = false;
					success = false;
				}
				else {
					options->file_load_success[file_type] = true;
				}

				// Here we should just assign the data read
				*options->misc_data[index] = data;
			}
			else if (options->callbacks[index] != nullptr) {
				bool current_success = options->callbacks[index](mutable_directory, options->callbacks_data[index]);
				if (!current_success) {
					ECS_FORMAT_TEMP_STRING(message, "Failed to read misc file {#} using callback\n", index);
					WriteErrorMessage(options, message);
				}

				options->file_load_success[file_type] = current_success;
				success &= current_success;
			}
		}

		return true;
	}

	bool RuntimeCrashPersistenceReadWorldDescriptor(
		Stream<wchar_t> directory, 
		const Reflection::ReflectionManager* reflection_manager,
		WorldDescriptor* world_descriptor, 
		CapacityStream<char>* detailed_error
	) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, descriptor_path, 512);
		RuntimeCrashPersistenceFilePath(directory, &descriptor_path, ECS_RUNTIME_CRASH_FILE_WORLD_DESCRIPTOR);

		if (ExistsFileOrFolder(descriptor_path)) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
			DeserializeOptions deserialize_options;
			deserialize_options.error_message = detailed_error;
			deserialize_options.default_initialize_missing_fields = true;
			deserialize_options.file_allocator = GetAllocatorPolymorphic(&stack_allocator);
			ECS_DESERIALIZE_CODE deserialize_code = Deserialize(
				reflection_manager, 
				reflection_manager->GetType(STRING(WorldDescriptor)), 
				world_descriptor, 
				descriptor_path, 
				&deserialize_options
			);
			return deserialize_code == ECS_DESERIALIZE_OK;
		}
		else {
			if (detailed_error != nullptr) {
				detailed_error->AddStreamAssert("The world descriptor file is missing from the crash\n");
			}
			return false;
		}
	}

	bool RuntimeCrashPersistenceRead(
		Stream<wchar_t> directory,
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		LoadSceneData* load_scene_data,
		RuntimeCrashPersistenceReadOptions* options
	) {
		bool success = true;

		// Check to see if we need to create the world from the runtime settings
		if (!options->world_is_initialized) {
			// Try to read the descriptor file
			bool current_success = RuntimeCrashPersistenceReadWorldDescriptor(directory, reflection_manager, options->world_descriptor, options->error_message);
			if (current_success) {
				// Create the world using the descriptor
				if (options->world_descriptor->debug_drawer_allocator_size == 0) {
					options->world_descriptor->debug_drawer = world->debug_drawer;
				}
				options->world_descriptor->graphics = world->graphics;
				options->world_descriptor->resource_manager = world->resource_manager;
				options->world_descriptor->mouse = world->mouse;
				options->world_descriptor->keyboard = world->keyboard;

				*world = World(*options->world_descriptor);
			}
			success &= current_success;
			options->file_load_success[ECS_RUNTIME_CRASH_FILE_WORLD_DESCRIPTOR] = current_success;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, mutable_directory, 512);
		mutable_directory.CopyOther(directory);
		success &= ReadSceneFile(mutable_directory, world, reflection_manager, load_scene_data, options);

		if (options->console != nullptr) {
			mutable_directory.size = directory.size;
			success &= ReadConsoleFile(mutable_directory, options);
		}

		if (options->stack_trace != nullptr) {
			mutable_directory.size = directory.size;
			success &= ReadStackTraceFile(mutable_directory, options);
		}

		if (options->task_scheduler_info != nullptr) {
			mutable_directory.size = directory.size;
			success &= ReadTaskSchedulerInfo(mutable_directory, options);
		}

		success &= RuntimeCrashPersistenceReadMiscFiles(directory, options);

		return success;
	}

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

}