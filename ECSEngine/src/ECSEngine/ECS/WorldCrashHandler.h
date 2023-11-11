#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "EntityManagerSerializeTypes.h"

namespace ECSEngine {

	struct RuntimeCrashPersistenceWriteOptions;
	struct SaveSceneData;
	namespace Reflection {
		struct ReflectionManager;
	}

	struct WorldCrashHandlerPreCallbackFunctionData {
		Stream<wchar_t> crash_directory;
		RuntimeCrashPersistenceWriteOptions* write_options;
		SaveSceneData* save_data; 
		void* user_data;
	};

	// This is a function that is called before writing the actual crash in order to set extra
	// parameters like misc files or component overrides for scene serialization
	// This function can return false if there is an error
	typedef bool (*WorldCrashHandlerPreCallbackFunction)(WorldCrashHandlerPreCallbackFunctionData* function_data);

	struct WorldCrashHandlerPostCallbackFunctionData {
		Stream<wchar_t> crash_directory;
		bool suspending_threads_success;
		bool resuming_threads_success;
		bool crash_write_success;
		Stream<char> error_message;
		void* user_data;
	};

	// This is a function that will be called after finishing writing the crash
	// For an abort handler, right before exiting, for continuation, right before
	// Returning the control flow. This callback is called even when the pre callback
	// returned false indicating an error
	typedef void (*WorldCrashHandlerPostCallbackFunction)(WorldCrashHandlerPostCallbackFunctionData* function_data);

	struct WorldCrashHandlerPreCallback {
		WorldCrashHandlerPreCallbackFunction function = nullptr;
		void* data = nullptr;
		size_t data_size = 0;
	};

	struct WorldCrashHandlerPostCallback {
		WorldCrashHandlerPostCallbackFunction function = nullptr;
		void* data = nullptr;
		size_t data_size = 0;
	};

	struct World;
	struct WorldDescriptor;
	struct AssetDatabase;

	struct SetWorldCrashHandlerDescriptor {
		// --------------------- Mandatory ------------------------------
		World* world;
		Stream<wchar_t> crash_directory;
		// This is necessary in order to have a reliable identification of asset resources
		const AssetDatabase* asset_database;
		const Reflection::ReflectionManager* reflection_manager;
		bool should_break = true;
		// If this is set to true, then it won't copy the infos, it will just reference them
		bool infos_are_stable = true;
		Stream<SerializeEntityManagerComponentInfo> unique_infos;
		Stream<SerializeEntityManagerSharedComponentInfo> shared_infos;
		Stream<SerializeEntityManagerGlobalComponentInfo> global_infos;

		// ---------------------- Optional ------------------------------
		// If the world descriptor is missing, it won't write a file for it
		// The module search paths is used to locate the .pdbs for stack walking
		Stream<Stream<wchar_t>> module_search_paths = {};
		WorldDescriptor* world_descriptor = nullptr;
		WorldCrashHandlerPreCallback pre_callback = {};
		WorldCrashHandlerPostCallback post_callback = {};
	};

	namespace OS {
		struct ThreadContext;
	}

	// This thread context can be set for hard crashes where the stack trace is given
	// to us and the moment we execute the soft crash the thread exited out of the
	// function that crashed and the stack frame won't reflect that. So you can set
	// this thread context instead and have it be reflected in the stack trace
	ECSENGINE_API void SetWorldCrashHandlerThreadContext(const OS::ThreadContext* thread_context);

	// The paths must be absolute paths, not relative ones
	ECSENGINE_API void SetAbortWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor);

	// The paths must be absolute paths, not relative ones
	ECSENGINE_API void SetContinueWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor);

	struct TaskManager;

	ECSENGINE_API void SetWorldCrashHandlerTaskManagerExceptionHandler(TaskManager* task_manager);

}