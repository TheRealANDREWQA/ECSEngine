#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineEntities.h"

using namespace ECSEngine;

struct EditorState;

struct PrefabInstance {
	ECS_INLINE bool operator == (PrefabInstance other) const {
		return path == other.path;
	}

	unsigned int reference_count;
	Stream<wchar_t> path;
};

// If the prefab already exists, then it will increment the reference count and return the ID
// If it doesn't exist, then it will create a new entry and return the ID
unsigned int AddPrefabID(EditorState* editor_state, Stream<wchar_t> path, unsigned int increment_count = 1);

// Adds the prefab component to the entity while at the same time incrementing the reference count
// For that prefab, or if it doesn't exist, it will create a new entry
void AddPrefabComponentToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<wchar_t> relative_assets_path);

// Returns the new reference count
unsigned int DecrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count = 1);

// Returns the prefab ID if it exists, else -1
unsigned int FindPrefabID(const EditorState* editor_state, Stream<wchar_t> path);

// Returns the new reference count
unsigned int IncrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int increment_count = 1);

// Returns the new reference count for that prefab. It will be 0 if it was removed
unsigned int RemovePrefabID(EditorState* editor_state, Stream<wchar_t> path, unsigned int decrement_count = 1);

// Returns the new reference count for that prefab. It will be 0 if it was removed
unsigned int RemovePrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count = 1);