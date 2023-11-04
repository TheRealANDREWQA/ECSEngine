#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct RuntimeCrashPersistenceWriteOptions;
	struct SaveSceneData;
	namespace Reflection {
		struct ReflectionManager;
	}

	struct WorldCrashHandlerPreCallbackFunctionData {
		RuntimeCrashPersistenceWriteOptions* write_options;
		SaveSceneData* save_data; 
		void* user_data;
	};

	// This is a function that is called before writing the actual crash in order to set extra
	// parameters like misc files or component overrides for scene serialization
	typedef void (*WorldCrashHandlerPreCallbackFunction)(WorldCrashHandlerPreCallbackFunctionData* function_data);

	struct WorldCrashHandlerPostCallbackFunctionData {
		bool suspending_threads_success;
		bool resuming_threads_success;
		bool crash_write_success;
		Stream<char> error_message;
		void* user_data;
	};

	// This is a function that will be called after finishing writing the crash
	// For an abort handler, right before exiting, for continuation, right before
	// Returning the control flow
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
		Stream<Stream<wchar_t>> module_search_paths;
		// This is necessary in order to have a reliable identification of asset resources
		const AssetDatabase* asset_database;
		const Reflection::ReflectionManager* reflection_manager;
		bool should_break = true;

		// ---------------------- Optional ------------------------------
		// If the world descriptor is missing, it won't write a file for it
		WorldDescriptor* world_descriptor = nullptr;
		WorldCrashHandlerPreCallback pre_callback = {};
		WorldCrashHandlerPostCallback post_callback = {};
	};

	// The paths must be absolute paths, not relative ones
	ECSENGINE_API void SetAbortWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor);

	// The paths must be absolute paths, not relative ones
	ECSENGINE_API void SetContinueWorldCrashHandler(const SetWorldCrashHandlerDescriptor* descriptor);

}