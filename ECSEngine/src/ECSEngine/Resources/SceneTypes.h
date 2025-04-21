#pragma once

namespace ECSEngine {

	struct WriteInstrument;
	struct ReadInstrument;
	struct EntityManager;

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct LoadSceneChunkFunctionData {
		EntityManager* entity_manager;
		const Reflection::ReflectionManager* reflection_manager;
		size_t file_version;
		ReadInstrument* read_instrument;
		void* user_data;
	};

	// This function can be used to extract the extra information from the scene
	// It must return true if it succeeded, else false (the load overall fails as well)
	typedef bool (*LoadSceneChunkFunction)(LoadSceneChunkFunctionData* data);

	struct LoadSceneChunkFunctor {
		LoadSceneChunkFunction function;
		void* user_data;
	};

	struct SaveSceneChunkFunctionData {
		const EntityManager* entity_manager;
		const Reflection::ReflectionManager* reflection_manager;
		WriteInstrument* write_instrument;
		void* user_data;
	};

	typedef bool (*SaveSceneChunkFunction)(SaveSceneChunkFunctionData* data);

	struct SaveSceneChunkFunctor {
		SaveSceneChunkFunction function;
		void* user_data;
	};

}