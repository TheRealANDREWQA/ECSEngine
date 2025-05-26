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

	// -----------------------------------------------------------------------------------------------------------------------------

	typedef DeltaStateGenericHeader SceneDeltaSerializationHeader;

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the current scene delta serialization version. It will be at max a byte.
	ECSENGINE_API unsigned char SceneDeltaVersion();

	// -----------------------------------------------------------------------------------------------------------------------------

	struct ModuleComponentFunctions;

	struct SceneDeltaReaderAssetLoadCallbackData {
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

	// A callback that is used to load assets. It should return true if it succeeded, else false for a failure
	typedef bool (*SceneDeltaReaderAssetLoadCallback)(SceneDeltaReaderAssetLoadCallbackData* data);

	struct SceneDeltaWriterInitializeInfoOptions {
		// These 2 fields help identify the source code such that a faithful
		// Reconstruction can be made.
		Stream<char> source_code_branch_name = {};
		Stream<char> source_code_commit_hash = {};

		// The list of active modules that are used for this scene
		Stream<ModuleSourceCode> modules = {};

		Stream<SerializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerGlobalComponentInfo> global_overrides = { nullptr, 0 };
		// These are optional chunks that can be used to write extra data for the entire state write
		Stream<SaveSceneChunkFunctor> save_entire_functors = {};
		// These are optional chunks that can be used to write extra data for the delta state write
		Stream<SaveSceneChunkFunctor> save_delta_functors = {};
	};

	struct SceneDeltaReaderInitializeInfoOptions {
		// ------------------------------- Mandatory ----------------------------------------------
		Stream<ModuleComponentFunctions> module_component_functions;
		SceneDeltaReaderAssetLoadCallback asset_load_callback;
		Stream<void> asset_load_callback_data;

		// ------------------------------- Optional -----------------------------------------------
		Stream<DeserializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<DeserializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };
		Stream<DeserializeEntityManagerGlobalComponentInfo> global_overrides = { nullptr, 0 };
		//Stream<LoadSceneChunkFunctor> = {};
	};

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

}