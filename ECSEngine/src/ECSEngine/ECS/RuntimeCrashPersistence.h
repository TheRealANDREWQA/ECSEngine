#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct World;
	struct WorldDescriptor;
	namespace Reflection {
		struct ReflectionManager;
	}
	struct SaveSceneData;
	struct LoadSceneData;

#define ECS_RUNTIME_CRASH_PERSISTENCE_MISC_COUNT 4

	enum ECS_RUNTIME_CRASH_FILE_TYPE : unsigned char {
		ECS_RUNTIME_CRASH_FILE_SCENE,
		ECS_RUNTIME_CRASH_FILE_CONSOLE,
		ECS_RUNTIME_CRASH_FILE_STACK_TRACE,
		ECS_RUNTIME_CRASH_FILE_SCHEDULER_ORDER,
		ECS_RUNTIME_CRASH_FILE_RUNTIME_SETTINGS,

		// These are extension files that the caller can use
		// To inject extra data into the crash
		ECS_RUNTIME_CRASH_FILE_MISC_0,
		ECS_RUNTIME_CRASH_FILE_MISC_1,
		ECS_RUNTIME_CRASH_FILE_MISC_2,
		ECS_RUNTIME_CRASH_FILE_MISC_3,

		ECS_RUNTIME_CRASH_FILE_TYPE_COUNT
	};

	// Should return true if it managed to write, else false
	typedef bool (*RuntimeCrashPersistenceWriteCallback)(Stream<wchar_t> path, void* user_data);

	struct ECSENGINE_API RuntimeCrashPersistenceWriteOptions {
		ECS_INLINE RuntimeCrashPersistenceWriteOptions() {
			memset(this, 0, sizeof(*this));
		}

		// If this is set to true, it will suspend the threads before gathering the stack trace
		bool suspend_threads;
		// If this is set to true, it will resume the threads after finishing the crash
		bool resume_threads;

		// If this is set to true, then it will write as many files as it can, even when
		// one of them failed to write
		bool dont_stop_on_first_failure;

		// These is used to write a separate file that will help the loader
		// Re-create the scene
		WorldDescriptor* world_descriptor;

		// Optional data for misc files
		Stream<void> misc_data[ECS_RUNTIME_CRASH_PERSISTENCE_MISC_COUNT];
		// Optional callbacks if you want to use a callback that writes directly to the file
		// Instead of providing the data in buffers
		RuntimeCrashPersistenceWriteCallback callbacks[ECS_RUNTIME_CRASH_PERSISTENCE_MISC_COUNT];

		// Optional detailed error message
		CapacityStream<char>* error_message;
	};

	// Should return true if it managed to read, else false
	typedef bool (*RuntimeCrashPersistenceReadCallback)(Stream<wchar_t> path, void* user_data);

	struct ECSENGINE_API RuntimeCrashPersistenceReadOptions {
		ECS_INLINE RuntimeCrashPersistenceReadOptions() {
			memset(this, 0, sizeof(this));
		}
		
		bool world_is_initialized;

		// These will be filled with the file data
		Stream<char>* stack_trace;
		// These will be filled with the file data
		Stream<char>* console;

		// If a pointer is specified, it will read the misc data using the given allocator
		Stream<void>* misc_data[ECS_RUNTIME_CRASH_PERSISTENCE_MISC_COUNT];
		AllocatorPolymorphic misc_data_allocator;

		// If you prefer to read from misc files by yourself, you can do this using these callbacks
		RuntimeCrashPersistenceReadCallback callbacks[ECS_RUNTIME_CRASH_PERSISTENCE_MISC_COUNT];

		// Optional detailed error message
		CapacityStream<char>* error_message;
	};

	ECSENGINE_API void RuntimeCrashPersistenceFilePath(Stream<wchar_t> directory, CapacityStream<wchar_t>* file_path, ECS_RUNTIME_CRASH_FILE_TYPE type);

	ECSENGINE_API void RuntimeCrashPersistenceFilePath(CapacityStream<wchar_t>* directory, ECS_RUNTIME_CRASH_FILE_TYPE type);

	// Returns true if it succeeded, else false
	// For the save scene data only the asset database and the overrides need to be specified
	ECSENGINE_API bool RuntimeCrashPersistenceWrite(
		Stream<wchar_t> directory, 
		World* world, 
		const Reflection::ReflectionManager* reflection_manager,
		const SaveSceneData* save_scene_data,
		const RuntimeCrashPersistenceWriteOptions* options
	);

	// Returns true if it succeeded, else false. Only reads the misc files.
	ECSENGINE_API bool RuntimeCrashPersistenceReadMiscFiles(Stream<wchar_t> directory, World* world, const RuntimeCrashPersistenceReadOptions* options);

	// Returns true if it managed to read the runtime settings, else false
	ECSENGINE_API bool RuntimeCrashPersistenceReadRuntimeSettings(Stream<wchar_t> directory, WorldDescriptor* world_descriptor);

	// Returns true if it succeeded, else false. If the world_is_initialized is set to false, it will attempt
	// To read the saved runtime settings. If it fails, it will stop the read since the scene cannot be instantiated
	// The world must have the graphics object created before hand in all cases.
	ECSENGINE_API bool RuntimeCrashPersistenceRead(
		Stream<wchar_t> directory, 
		World* world, 
		const Reflection::ReflectionManager* reflection_manager,
		LoadSceneData* load_scene_data,
		RuntimeCrashPersistenceReadOptions* options
	);

}