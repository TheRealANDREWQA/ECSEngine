// ECS_REFLECT
#pragma once
#include "ECSEngineEntities.h"
#include "ECSEngineContainers.h"

using namespace ECSEngine;

struct ContactConstraint;

struct ECS_REFLECT IslandHandle {
	unsigned int handle;
	unsigned int reference_count;
};

struct ECS_REFLECT Island {
	void AddContact(ContactConstraint* constraint);

	void AddContacts(Stream<ContactConstraint*> constraints);

	void RemoveContact(ContactConstraint* constraint);
	
	[[ECS_POINTER_AS_REFERENCE(ContactConstraint)]]
	ResizableStream<ContactConstraint*> contacts;
};

struct ECS_REFLECT IslandManager {
	void AddContact(ContactConstraint* constraint);

	// Returns the handle for the island that contains the given entity. If there is no island
	// That contains the entity, it returns -1
	ECS_INLINE unsigned int Find(Entity entity) const {
		IslandHandle handle;
		if (entity_table.TryGetValue(entity, handle)) {
			return handle.handle;
		}
		else {
			return -1;
		}
	}

	// Returns the island handle for that entity, else nullptr if it doesn't exist
	// The value is not stable across handle insertions and removals.
	ECS_INLINE IslandHandle* GetHandle(Entity entity) {
		return entity_table.TryGetValuePtr(entity);
	}

	void Initialize(MemoryManager* allocator_source);

	void RemoveContact(ContactConstraint* constraint);

	// This is a table that is used to know which entities belong to which island
	// This allows for fast queries to determine which island an entity belongs to.
	// Handles for static entities are meaningless, since they can belong to multiple islands at the same time
	// Use reference counting to know when an entity no longer has constraints in any island to evict it
	// From the hash table
	typedef HashTable<IslandHandle, Entity, HashFunctionPowerOfTwo> EntityTable;

	[[ECS_MAIN_ALLOCATOR]]
	MemoryManager allocator;
	// We are using a sparse set because we want to iterate through this when simulating
	// The islands, but also to have a stable index for the EntityTable such that we don't
	// Need to repair references
	ResizableSparseSet<Island> islands;
	EntityTable entity_table;
};