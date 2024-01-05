#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Rendering/RenderingStructures.h"
#include "../ECS/EntityManagerSerializeTypes.h"
#include "../Tools/Modules/ModuleDefinition.h"

// These are some chunks that can be used to fill in various information
// Without a predefined connotation
#define ECS_SCENE_EXTRA_CHUNKS 8

namespace ECSEngine {

	struct AssetDatabase;
	struct AssetDatabaseReference;
	struct AssetDatabaseReferencePointerRemap;
	struct EntityManager;

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct SceneModuleSetting {
		Stream<char> name;
		void* data;
	};

	struct LoadSceneChunkFunctionData {
		EntityManager* entity_manager;
		const Reflection::ReflectionManager* reflection_manager;
		size_t chunk_index;
		size_t file_version;
		Stream<void> chunk_data;
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
		LoadSceneData() {}

		// ----------------------- Mandatory -----------------------------
		EntityManager* entity_manager;
		// When using in memory data reading, it will advance the pointer
		// By the amount necessary to read all the scene information
		// You specify which of these options is active with the is_file_data boolean
		union {
			Stream<wchar_t> file;
			Stream<void> in_memory_data;
		};
		bool is_file_data = true;
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

		// This is the allocator used to allocate the scene module data and name
		// This must not be malloc
		AllocatorPolymorphic scene_modules_allocator = { nullptr };
		// These are the settings for the scene (if it has any - some scene may want to skip this information)
		AdditionStream<SceneModuleSetting> scene_modules = {};

		// You can retrieve the delta time of the simulation if you choose to
		float* delta_time = nullptr;
		float* speed_up_factor = nullptr;

		LoadSceneChunkFunctor chunk_functors[ECS_SCENE_EXTRA_CHUNKS] = { {} };
	};

	ECSENGINE_API bool CreateEmptyScene(Stream<wchar_t> file);

	
	ECSENGINE_API bool LoadScene(LoadSceneData* load_data);

	struct SaveSceneData {
		// ------------------------- Mandatory ---------------------------
		Stream<wchar_t> file;
		const EntityManager* entity_manager;
		const Reflection::ReflectionManager* reflection_manager;
		// A database reference cannot be given. It must firstly be converted to 
		// a standalone database and then written off
		const AssetDatabase* asset_database;

		// ------------------------- Optional ----------------------------
		Stream<SerializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };
		Stream<SerializeEntityManagerGlobalComponentInfo> global_overrides = { nullptr, 0 };

		Stream<SceneModuleSetting> scene_settings = { nullptr, 0 };

		// This value will also be serialized such that upon loading the simulation can continue as before
		float delta_time = 0.0f;
		float speed_up_factor = 1.0f;

		SaveSceneChunkFunctor chunk_functors[ECS_SCENE_EXTRA_CHUNKS] = { {} };
	};

	ECSENGINE_API bool SaveScene(SaveSceneData* save_data);

}