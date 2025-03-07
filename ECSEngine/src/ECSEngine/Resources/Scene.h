// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Rendering/RenderingStructures.h"
#include "../ECS/EntityManagerSerializeTypes.h"
#include "../Tools/Modules/ModuleDefinition.h"
#include "../Utilities/Reflection/ReflectionMacros.h"

// These are some chunks that can be used to fill in various information
// Without a predefined connotation
#define ECS_SCENE_EXTRA_CHUNKS 8

namespace ECSEngine {

	struct AssetDatabase;
	struct AssetDatabaseReference;
	struct AssetDatabaseReferencePointerRemap;
	struct EntityManager;

	struct WriteInstrument;
	struct ReadInstrument;

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct ECS_REFLECT SceneModule {
		// Deallocates each individual field
		void Deallocate(AllocatorPolymorphic allocator) {
			solution_path.Deallocate(allocator);
			library_name.Deallocate(allocator);
			branch_name.Deallocate(allocator);
			commit_hash.Deallocate(allocator);
			configuration.Deallocate(allocator);
		}

		Stream<wchar_t> solution_path;
		Stream<wchar_t> library_name;
		// These fields are added to help identify the source code for the file
		// In case they match the main repository, these can be left empty.
		Stream<char> branch_name;
		Stream<char> commit_hash;
		// This is the configuration used for the module, i.e. Debug/Release/Distribution
		Stream<char> configuration;
	};

	struct ECS_REFLECT SceneModules {
		Stream<SceneModule> values;
	};
	
	struct LoadSceneChunkFunctionData {
		EntityManager* entity_manager;
		const Reflection::ReflectionManager* reflection_manager;
		size_t chunk_index;
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

	// The write data will be empty ({ nullptr, 0}) when the function wants to know
	// How much there is to written, then the function will be called once again
	// To actually write the data after an allocation was prepared. The other write
	// Mode is to actually use the file handle directly
	struct SaveSceneChunkFunctionData {
		ECS_INLINE SaveSceneChunkFunctionData() {}

		const EntityManager* entity_manager;
		const Reflection::ReflectionManager* reflection_manager;
		size_t chunk_index;
		union {
			Stream<void> write_data;
			ECS_FILE_HANDLE write_handle;
		};
		void* user_data;
	};

	typedef bool (*SaveSceneChunkFunction)(SaveSceneChunkFunctionData* data);

	struct SaveSceneChunkFunctor {
		SaveSceneChunkFunction function;
		void* user_data;
		// If this is specified, it will use the file handle directly to make the write
		bool file_handle_write;
	};

	struct LoadSceneData {
		ECS_INLINE LoadSceneData() {}

		// ----------------------- Mandatory -----------------------------
		EntityManager* entity_manager;
		// This is data source that will be used to read the scene from.
		// It assumes that the read instrument is limited only to the scene data beforehand,
		// The caller must ensure this property
		ReadInstrument* read_instrument;
		// The reflection manager must have the SceneModule type reflected
		const Reflection::ReflectionManager* reflection_manager;
		Stream<ModuleComponentFunctions> module_component_functions;

		// One of these needs to be set
		AssetDatabase* database = nullptr;
		AssetDatabaseReference* database_reference = nullptr;

		// ------------------------ Optional -----------------------------
		CapacityStream<char>* detailed_error_string = nullptr;
		bool allow_missing_components = false;

		// If you want the asset database to have metadatas unique (but invalid, they don't point to valid data)
		// after the load such that the link assets can be still identified, you must give an allocator for the temporary allocations needed
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
		// This must not be Malloc. Each entry will be allocated individually.
		AllocatorPolymorphic scene_modules_allocator = { nullptr };
		AdditionStream<SceneModule> scene_modules = {};
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
		WriteInstrument* write_instrument;
		const EntityManager* entity_manager;
		// The reflection manager must have the SceneModule type reflected
		const Reflection::ReflectionManager* reflection_manager;
		// A database reference cannot be given. It must firstly be converted to 
		// a standalone database and then written off
		const AssetDatabase* asset_database;

		// The list of active modules that are used for this scene
		Stream<SceneModule> modules;

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