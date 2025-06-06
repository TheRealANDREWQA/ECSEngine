#pragma once
#include "../ECS/EntityManagerChangeSet.h"
#include "../Tools/Modules/ModuleSourceCode.h"
#include "../Utilities/Serialization/DeltaStateSerializationForward.h"
#include "../ECS/EntityManagerSerializeTypes.h"
#include "SceneTypes.h"

namespace ECSEngine {

	struct World;
	struct AssetDatabase;
	struct AssetDatabaseReference;
	struct AssetDatabaseDeltaChange;
	struct AssetDatabaseFullSnapshot;
	struct ModuleComponentFunctions;

	// -----------------------------------------------------------------------------------------------------------------------------

	typedef DeltaStateGenericHeader SceneDeltaSerializationHeader;

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the current scene delta serialization version. It will be at max a byte.
	ECSENGINE_API unsigned char SceneDeltaVersion();

	// -----------------------------------------------------------------------------------------------------------------------------

	// --------------------------------- Writer --------------------------------------------------------

	struct SceneDeltaWriterChunkFunctionData {
		const EntityManager* previous_entity_manager;
		const Reflection::ReflectionManager* reflection_manager;
		WriteInstrument* write_instrument;
		void* user_data;

		// The only difference to SaveSceneChunkFunctionData is this additional field
		// And separating the entity managers between a previous and a current, such that
		// A delta can be computed
		const EntityManager* current_entity_manager;
		// The asset database and its snapshot are also added, in case they are required
		const AssetDatabase* asset_database;
		const AssetDatabaseFullSnapshot* asset_database_snapshot;
	};

	// Should return true if it succeeded, else false
	typedef bool (*SceneDeltaWriterChunkFunction)(SceneDeltaWriterChunkFunctionData* data);

	struct SceneDeltaWriterChunkFunctor {
		SceneDeltaWriterChunkFunction function;
		// The size is needed in case this data needs to be allocated and moved. A size of 0
		// Means reference the pointer directly, without making another allocation
		Stream<void> user_data;
		// This name is used to identify the appropriate chunk on deserialization
		Stream<char> name;
		unsigned int version;
	};

	struct SceneDeltaWriterInitializeInfoOptions {
		// These 2 fields help identify the source code such that a faithful
		// Reconstruction can be made.
		Stream<char> source_code_branch_name = {};
		Stream<char> source_code_commit_hash = {};

		// The list of active modules that are used for this scene
		Stream<ModuleSourceCode> modules = {};

		Stream<SerializeEntityManagerComponentInfo> unique_overrides = {};
		Stream<SerializeEntityManagerSharedComponentInfo> shared_overrides = {};
		Stream<SerializeEntityManagerGlobalComponentInfo> global_overrides = {};

		// These are optional chunks that can be used to write extra data in the header of the file
		Stream<SaveSceneNamedChunkFunctor> save_header_functors = {};
		// These are optional chunks that can be used to write extra data for the entire state write
		Stream<SaveSceneNamedChunkFunctor> save_entire_functors = {};
		// These are optional chunks that can be used to write extra data for the delta state write
		// These delta functors must have a different interface in order to be able to generate deltas
		Stream<SceneDeltaWriterChunkFunctor> save_delta_functors = {};
	};

	// --------------------------------- Writer --------------------------------------------------------

	// --------------------------------- Reader --------------------------------------------------------

	struct SceneDeltaReaderAssetCallbackData {
		// It will be called with a delta change for the delta state, or with a full snapshot
		// For the entire scene deserialization
		union {
			const AssetDatabaseDeltaChange* delta_change;
			const AssetDatabaseFullSnapshot* full_snapshot;
		};
		// This boolean indicates which type is active for the union
		bool is_delta_change;

		// Untyped data that the functor can use for extra information
		Stream<void> user_data;
	};

	// A callback that is used to load/unload assets. It should return true if it succeeded, else false for a failure
	typedef bool (*SceneDeltaReaderAssetCallback)(SceneDeltaReaderAssetCallbackData* data);

	struct SceneDeltaReaderInitializeInfoOptions {
		// ------------------------------- Mandatory ----------------------------------------------
		Stream<ModuleComponentFunctions> module_component_functions;
		SceneDeltaReaderAssetCallback asset_callback;
		Stream<void> asset_callback_data;

		// ------------------------------- Optional -----------------------------------------------
		Stream<DeserializeEntityManagerComponentInfo> unique_overrides = {};
		Stream<DeserializeEntityManagerSharedComponentInfo> shared_overrides = {};
		Stream<DeserializeEntityManagerGlobalComponentInfo> global_overrides = {};
		
		// These are optional chunks that can be used to read extra data in the header of the file
		Stream<LoadSceneNamedChunkFunctor> read_header_functors = {};
		// These are optional chunks that can be used to read extra data for the entire state write
		Stream<LoadSceneNamedChunkFunctor> read_entire_functors = {};
		// These are optional chunks that can be used to read extra data for the delta state write
		// The delta functors can maintain the same interface as the other functors
		Stream<LoadSceneNamedChunkFunctor> read_delta_functors = {};
	};

	// --------------------------------- Reader --------------------------------------------------------

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - outside the runtime context
	// All provided pointers, except for options, must be stable and valid throughout the writer's instance usage
	ECSENGINE_API void SetSceneDeltaWriterInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info,
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		const float* delta_time_value,
		const float* speed_up_time_value,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaWriterInitializeInfoOptions* options
	);

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - for a simulation world
	// All provided pointers, except for options, must be stable and valid throughout the writer's instance usage
	ECSENGINE_API void SetSceneDeltaWriterWorldInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info,
		const World* world,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaWriterInitializeInfoOptions* options
	);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The entity manager and the reflection manager must be stable for the entire duration of the reader
	ECSENGINE_API void SetSceneDeltaReaderInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaReaderInitializeInfoOptions* options
	);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The reflection manager must be stable for the entire duration of the reader
	ECSENGINE_API void SetSceneDeltaReaderWorldInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaReaderInitializeInfoOptions* options
	);

	// TODO: Add a specialized function that reads the delta file source code information (branch and commit)

}