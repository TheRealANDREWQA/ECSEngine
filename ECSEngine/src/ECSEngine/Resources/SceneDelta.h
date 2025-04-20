#pragma once
#include "../ECS/EntityManagerChangeSet.h"
#include "../Tools/Modules/ModuleSourceCode.h"
#include "../Utilities/Serialization/DeltaStateSerializationForward.h"
#include "../ECS/EntityManagerSerializeTypes.h"

namespace ECSEngine {

	struct World;
	struct AssetDatabase;

	// -----------------------------------------------------------------------------------------------------------------------------

	typedef DeltaStateGenericHeader EntityManagerDeltaSerializationHeader;

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the current input serialization version. It will be at max a byte.
	ECSENGINE_API unsigned char SerializeEntityManagerDeltaVersion();

	// -----------------------------------------------------------------------------------------------------------------------------

	struct ModuleComponentFunctions;

	struct EntityManagerDeltaWriterInitializeInfoOptions {
		// These 2 fields help identify the source code such that a faithful
		// Reconstruction can be made.
		Stream<char> source_code_branch_name = {};
		Stream<char> source_code_commit_hash = {};

		// The list of active modules that are used for this scene
		Stream<ModuleSourceCode> modules = {};

		Stream<SerializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerGlobalComponentInfo> global_overrides = { nullptr, 0 };
	};

	struct EntityManagerDeltaReaderInitializeInfoOptions {
		Stream<ModuleComponentFunctions> module_component_functions;
		Stream<DeserializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<DeserializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };
		Stream<DeserializeEntityManagerGlobalComponentInfo> global_overrides = { nullptr, 0 };
		//Stream<LoadSceneChunkFunctor> = {};
	};

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - outside the runtime context
	// All provided pointers, except for options, must be stable and valid throughout the writer's instance usage
	ECSENGINE_API void SetEntityManagerDeltaWriterInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info,
		const EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		const float* delta_time_value,
		const float* speed_up_time_value,
		CapacityStream<void>& stack_memory,
		const EntityManagerDeltaWriterInitializeInfoOptions* options
	);

	// Sets the necessary info for the writer to be initialized as an ECS state delta writer - for a simulation world
	// All provided pointers, except for options, must be stable and valid throughout the writer's instance usage
	ECSENGINE_API void SetEntityManagerDeltaWriterWorldInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info,
		const World* world,
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		CapacityStream<void>& stack_memory,
		const EntityManagerDeltaWriterInitializeInfoOptions* options
	);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The entity manager and the reflection manager must be stable for the entire duration of the reader
	ECSENGINE_API void SetEntityManagerDeltaReaderInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		EntityManager* entity_manager,
		const Reflection::ReflectionManager* reflection_manager,

		CapacityStream<void>& stack_memory
	);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The reflection manager must be stable for the entire duration of the reader
	ECSENGINE_API void SetEntityManagerDeltaReaderWorldInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		World* world,
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory
	);

}