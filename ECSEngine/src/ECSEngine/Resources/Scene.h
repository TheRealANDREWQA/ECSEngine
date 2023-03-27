#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "ResourceTypes.h"
#include "../Rendering/RenderingStructures.h"
#include "../ECS/EntityManagerSerializeTypes.h"

namespace ECSEngine {

	struct AssetDatabase;
	struct AssetDatabaseReference;
	struct AssetDatabaseReferencePointerRemap;
	struct EntityManager;

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	struct LoadSceneData {
		// ----------------------- Mandatory -----------------------------
		EntityManager* entity_manager;
		Stream<wchar_t> file;
		const Reflection::ReflectionManager* reflection_manager;

		// One of these needs to be set
		AssetDatabase* database = nullptr;
		AssetDatabaseReference* database_reference = nullptr;

		// ------------------------ Optional -----------------------------

		// If you want the asset database to have metadatas unique (but invalid, they don't point to valid data)
		// after the load such that the link assets can be still identified, you must give an allocator for the temporary allocations needed
		bool randomize_assets = false;
		Stream<DeserializeEntityManagerComponentInfo> unique_overrides = { nullptr, 0 };
		Stream<DeserializeEntityManagerSharedComponentInfo> shared_overrides = { nullptr, 0 };

		// These options are valid only for the reference asset database

		// When using a database reference, you can obtain the handle remapping done inside when commiting to the master database
		CapacityStream<uint2>* handle_remapping = nullptr;
		// When using randomized assets, these can clash when commiting into a master database afterwards.
		// This is used to make each randomized asset again unique
		CapacityStream<AssetDatabaseReferencePointerRemap>* pointer_remapping = nullptr;
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
	};

	ECSENGINE_API bool SaveScene(SaveSceneData* save_data);

}