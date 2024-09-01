#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineEntitiesSerialize.h"

struct EditorState;

namespace ECSEngine {
	struct EntityManager;
	struct AssetDatabaseReference;
	struct AssetDatabaseReferencePointerRemap;
	struct SceneModule;
}

// Returns true if it could create the scene, else false.
// The path should contain the extension
bool CreateEmptyScene(
	const EditorState* editor_state,
	ECSEngine::Stream<wchar_t> path
);

// The path must be relative to the assets folder and with an extension
bool ExistScene(const EditorState* editor_state, ECSEngine::Stream<wchar_t> path);

// Loads only the entity manager + database reference. It does not load the referenced assets.
bool LoadEditorSceneCore(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename,
	ECSEngine::Stream<ECSEngine::CapacityStream<ECSEngine::AssetDatabaseReferencePointerRemap>> pointer_remap = { nullptr, 0 }
);

// Loads only the entity manager + database reference. It does not load the referenced assets.
bool LoadEditorSceneCoreInMemory(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<void> in_memory_data,
	ECSEngine::Stream<ECSEngine::CapacityStream<ECSEngine::AssetDatabaseReferencePointerRemap>> pointer_remap = { nullptr, 0 }
);

// It does not unload or clear the scene before doing this. 
// Loads only the entity manager + database reference. It does not load the referenced assets. (It will remap
// the asset handles tho)
bool LoadEditorSceneCore(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<wchar_t> filename
);

// Writes a combined entity_manager + database reference in a single file
// The reason the entity manager is not const is because it will remove any empty
// archetypes as well. The modules array is optional.
bool SaveEditorScene(
	const EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	const ECSEngine::AssetDatabaseReference* database,
	ECSEngine::Stream<wchar_t> filename,
	ECSEngine::Stream<ECSEngine::SceneModule> modules = {}
);

// Saves the normal editor scene
bool SaveEditorScene(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<wchar_t> filename
);

// Saves the runtime scene instead of the normal editor scene
bool SaveEditorSceneRuntime(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<wchar_t> filename
);

// Returns true if it managed to get all overrides, else false. It can fail if there is a
// link component mismatch
bool GetEditorSceneSerializeOverrides(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::CapacityStream<ECSEngine::SerializeEntityManagerComponentInfo>* unique_overrides,
	ECSEngine::CapacityStream<ECSEngine::SerializeEntityManagerSharedComponentInfo>* shared_overrides,
	ECSEngine::CapacityStream<ECSEngine::SerializeEntityManagerGlobalComponentInfo>* global_overrides,
	ECSEngine::AllocatorPolymorphic temporary_allocator
);

// Returns true if it managed to get all overrides, else false. It can fail if there is a
// link component mismatch
bool GetEditorSceneDeserializeOverrides(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::CapacityStream<ECSEngine::DeserializeEntityManagerComponentInfo>* unique_overrides,
	ECSEngine::CapacityStream<ECSEngine::DeserializeEntityManagerSharedComponentInfo>* shared_overrides,
	ECSEngine::CapacityStream<ECSEngine::DeserializeEntityManagerGlobalComponentInfo>* global_overrides,
	ECSEngine::AllocatorPolymorphic temporary_allocator
);

// Updates the link components to the new remapping from here
void UpdateEditorScenePointerRemappings(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ECSEngine::Stream<ECSEngine::CapacityStream<ECSEngine::AssetDatabaseReferencePointerRemap>> pointer_remapping
);

// Updates the link components to the new remapping from here
void UpdateEditorScenePointerRemappings(
	EditorState* editor_state,
	ECSEngine::EntityManager* entity_manager,
	ECSEngine::Stream<ECSEngine::CapacityStream<ECSEngine::AssetDatabaseReferencePointerRemap>> pointer_remapping
);