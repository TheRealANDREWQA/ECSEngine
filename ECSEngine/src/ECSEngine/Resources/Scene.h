// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Rendering/RenderingStructures.h"
#include "../ECS/EntityManagerSerializeTypes.h"
#include "../Tools/Modules/ModuleDefinition.h"
#include "../Tools/Modules/ModuleSourceCode.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/ReaderWriterInterface.h"
#include "SceneTypes.h"

// These are some chunks that can be used to fill in various information
// Without a predefined connotation
#define ECS_SCENE_EXTRA_CHUNKS 8

namespace ECSEngine {

	struct AssetDatabase;
	struct AssetDatabaseReference;
	struct AssetDatabaseReferencePointerRemap;

	struct LoadSceneData {
		ECS_INLINE LoadSceneData() {}

		// ----------------------- Mandatory -----------------------------
		EntityManager* entity_manager;
		// This is data source that will be used to read the scene from.
		// It assumes that the read instrument is limited only to the scene data beforehand,
		// The caller must ensure this property
		FileReadInstrumentTarget read_target;
		// The reflection manager must have the SceneModule type reflected
		const Reflection::ReflectionManager* reflection_manager;
		Stream<ModuleComponentFunctions> module_component_functions;

		// One of these needs to be set
		AssetDatabase* database = nullptr;
		AssetDatabaseReference* database_reference = nullptr;

		// ------------------------ Optional -----------------------------
		CapacityStream<char>* detailed_error_string = nullptr;
		bool allow_missing_components = false;

		// If you want the asset database to have unique metadatas (but invalid, they don't point to valid data)
		// After the load such that the link assets can be still be identified, you must give an allocator for the temporary allocations needed
		bool randomize_assets = false;
		Stream<DeserializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<DeserializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };
		Stream<DeserializeEntityManagerGlobalComponentInfo> global_overrides = { nullptr, 0 };

		// These options are valid only for the reference asset database

		// When using a database reference, you can obtain the handle remapping done inside when commiting to the master database
		CapacityStream<uint2>* handle_remapping = nullptr;
		// When using randomized assets, these can clash when commiting into a master database afterwards.
		// This is used to make each randomized asset again unique
		Stream<CapacityStream<AssetDatabaseReferencePointerRemap>> pointer_remapping = { nullptr, 0 };

		// This is the allocator used to allocate the buffers needed for the modules' streams
		// Each entry will be allocated individually.
		AllocatorPolymorphic scene_modules_allocator = ECS_MALLOC_ALLOCATOR;
		AdditionStream<ModuleSourceCode> scene_modules = {};
		// These 2 fields help you identify the source code such that a faithful reconstruction can be made
		// They will be allocated from scene_modules_allocator
		Stream<char> source_code_branch_name = {};
		Stream<char> source_code_commit_hash = {};

		// You can retrieve the delta time of the simulation if you choose to
		float* delta_time = nullptr;
		float* speed_up_factor = nullptr;

		LoadSceneChunkFunctor chunk_functors[ECS_SCENE_EXTRA_CHUNKS] = { {} };
	};

	ECSENGINE_API bool CreateEmptyScene(Stream<wchar_t> file);

	
	ECSENGINE_API bool LoadScene(LoadSceneData* load_data);

	struct SaveSceneData {
		// ------------------------- Mandatory ---------------------------
		FileWriteInstrumentTarget write_target;
		const EntityManager* entity_manager;
		// The reflection manager must have the SceneModule type reflected
		const Reflection::ReflectionManager* reflection_manager;
		// A database reference cannot be given. It must firstly be converted to 
		// a standalone database and then written off
		const AssetDatabase* asset_database;

		// The list of active modules that are used for this scene
		ModulesSourceCode modules;

		// ------------------------- Optional ----------------------------
		Stream<SerializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerGlobalComponentInfo> global_overrides = { nullptr, 0 };

		// These 2 fields help identify the source code such that a faithful
		// Reconstruction can be made.
		Stream<char> source_code_branch_name = {};
		Stream<char> source_code_commit_hash = {};

		// This value will also be serialized such that upon loading the simulation can continue as before
		float delta_time = 0.0f;
		float speed_up_factor = 1.0f;

		SaveSceneChunkFunctor chunk_functors[ECS_SCENE_EXTRA_CHUNKS] = { {} };
	};

	ECSENGINE_API bool SaveScene(SaveSceneData* save_data);

}