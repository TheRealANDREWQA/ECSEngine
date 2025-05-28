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
		unsigned int file_version;
		ReadInstrument* read_instrument;
		void* user_data;
	};

	// This function can be used to extract the extra information from the scene
	// It must return true if it succeeded, else false (the load overall fails as well)
	typedef bool (*LoadSceneChunkFunction)(LoadSceneChunkFunctionData* data);

	struct LoadSceneChunkFunctor {
		LoadSceneChunkFunction function;
		// The size is needed in case this data needs to be allocated and moved. A size of 0
		// Means reference the pointer directly, without making another allocation
		Stream<void> user_data;
	};

	// The same as the other functor, but it has a name as well
	struct LoadSceneNamedChunkFunctor : LoadSceneChunkFunctor {
		Stream<char> name;
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
		// The size is needed in case this data needs to be allocated and moved. A size of 0
		// Means reference the pointer directly, without making another allocation
		Stream<void> user_data;
		// Use a per chunk version as well, to make it easier to version scene files
		unsigned int version;
	};

	// The same as the other functor, but it has a name as well
	struct SaveSceneNamedChunkFunctor : SaveSceneChunkFunctor {
		Stream<char> name;
	};

}