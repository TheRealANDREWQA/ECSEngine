#include "ecspch.h"
#include "RuntimeCrashPersistence.h"
#include "../Resources/Scene.h"
#include "../Utilities/Console.h"

namespace ECSEngine {

	static const wchar_t* FILE_STRINGS[] = {
		L".scene",
		L".console",
		L".stack_trace",
		L".scheduler",
		L".runtime_settings",
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

	bool RuntimeCrashPersistenceWrite(
		Stream<wchar_t> directory,
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		const SaveSceneData* save_scene_data,
		const RuntimeCrashPersistenceWriteOptions* options
	) {
		// Start with the console and stack trace files
		Console* console = GetConsole();
		return false;
	}

	bool RuntimeCrashPersistenceReadMiscFiles(Stream<wchar_t> directory, World* world, const RuntimeCrashPersistenceReadOptions* options) {
		return false;
	}

	bool RuntimeCrashPersistenceReadRuntimeSettings(Stream<wchar_t> directory, WorldDescriptor* world_descriptor) {
		return false;
	}

	bool RuntimeCrashPersistenceRead(
		Stream<wchar_t> directory,
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		LoadSceneData* load_scene_data,
		RuntimeCrashPersistenceReadOptions* options
	) {
		return false;
	}

}