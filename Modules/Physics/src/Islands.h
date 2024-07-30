#pragma once
#include "ECSEngineEntities.h"
#include "ECSEngineContainers.h"

using namespace ECSEngine;

struct ContactConstraint;

struct Island {
	ResizableStream<ContactConstraint*> contacts;
};

struct IslandManager {
	// This is a table that is used to know which entities belong to which island
	// This allows for fast queries to determine whi
	typedef HashTable<unsigned int, Entity, HashFunctionPowerOfTwo> EntityTable;

	// We are using a sparse set because we want to iterate through this when simulating
	// The islands, but also to have a stable index for the EntityTable such that we don't
	// Need to repair references
	ResizableSparseSet<Island> islands;
	EntityTable entity_table;
	MemoryManager allocator;
};

void IslandAddContact(Island& island, ContactConstraint* constraint);

void IslandManagerAddContact(IslandManager& island_manager, ContactConstraint* constraint);

// Returns the handle for the island that contains the given entity. If there is no island
// That contains the entity, it returns -1
ECS_INLINE unsigned int IslandManagerFind(IslandManager& island_manager, Entity entity) {
	unsigned int handle;
	if (island_manager.entity_table.TryGetValue(entity, handle)) {
		return handle;
	}
	else {
		return -1;
	}
}

void IslandManagerInitialize(IslandManager& island_manager, MemoryManager* allocator_source);