#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineEntities.h"

using namespace ECSEngine;

struct EditorState;

enum EDITOR_SANDBOX_VIEWPORT : unsigned char;

struct PrefabInstance {
	ECS_INLINE bool operator == (PrefabInstance other) const {
		return path == other.path;
	}

	unsigned int reference_count;
	Stream<wchar_t> path;
	// This is the time stamp of the last write
	size_t write_stamp;
	// We keep an in memory version of the prefab such that
	// When a prefab is changed, we can generate the diff between
	// The old data and the new file data. We don't keep the components
	// As they are in memory since that would require updating the asset handles
	// Or asset pointers when they change and would necessitate more work to implement
	// Than this version where we store the file data and deserialize from it
	Stream<void> prefab_file_data;
};

// Returns the entity inside a prefab scene where only this prefab is loaded
ECS_INLINE Entity GetPrefabEntityFromSingle() {
	return 0;
}

// If the prefab already exists, then it will increment the reference count and return the ID
// If it doesn't exist, then it will create a new entry and return the ID
unsigned int AddPrefabID(EditorState* editor_state, Stream<wchar_t> relative_assets_path, unsigned int increment_count = 1);

// Adds the prefab component to the entity while at the same time incrementing the reference count
// For that prefab, or if it doesn't exist, it will create a new entry
void AddPrefabComponentToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<wchar_t> relative_assets_path);

// Adds the prefab component to the entity while at the same time incrementing the reference count
// For that prefab
void AddPrefabComponentToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int id);

// Returns the new reference count
unsigned int DecrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count = 1);

// Returns true if the file exists on disk, else false
bool ExistsPrefabFile(const EditorState* editor_state, Stream<wchar_t> path);

// Returns the prefab ID if it exists, else -1
unsigned int FindPrefabID(const EditorState* editor_state, Stream<wchar_t> path);

Stream<wchar_t> GetPrefabPath(const EditorState* editor_state, unsigned int id);

Stream<wchar_t> GetPrefabAbsolutePath(const EditorState* editor_state, unsigned int id, CapacityStream<wchar_t> storage);

// Can use VIEWPORT_COUNT if you want the active entity manager
Stream<Entity> GetPrefabEntitiesForSandbox(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	unsigned int prefab_id, 
	AllocatorPolymorphic allocator
);

// Returns the new reference count
unsigned int IncrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int increment_count = 1);

// Returns the new reference count for that prefab. It will be 0 if it was removed
unsigned int RemovePrefabID(EditorState* editor_state, Stream<wchar_t> relative_assets_path, unsigned int decrement_count = 1);

// Returns the new reference count for that prefab. It will be 0 if it was removed
unsigned int RemovePrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count = 1);

void TickPrefabFileChange(EditorState* editor_state);