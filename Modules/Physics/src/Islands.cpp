#include "pch.h"
#include "Islands.h"
#include "ContactConstraint.h"

#define ISLAND_STREAM_INITIAL_SIZE 8

#define ISLAND_MANAGER_INITIAL_ISLAND_CAPACITY 8
#define ISLAND_MANAGER_ENTITY_TABLE_INITIAL_SIZE 8

#define ALLOCATOR_SIZE ECS_MB
#define ALLOCATOR_COUNT 4
#define ALLOCATOR_BACKUP_SIZE ECS_MB

// 
static void MergeIslands(IslandManager& island_manager, unsigned int first_island_handle, unsigned int second_island_handle) {
	
}

void IslandAddContact(Island& island, ContactConstraint* constraint) {
	island.contacts.Add(constraint);
}

void IslandManagerAddContact(IslandManager& island_manager, ContactConstraint* constraint) {
	// Find the island which contains the first entity
	Entity first_entity = constraint->FirstEntity();
	Entity second_entity = constraint->SecondEntity();
	unsigned int first_entity_island = IslandManagerFind(island_manager, first_entity);
	unsigned int second_entity_island = IslandManagerFind(island_manager, second_entity);

	if (first_entity_island == second_entity_island) {
		if (first_entity_island == -1) {
			// Both entities did not exist beforehand, create an island for them
			Island island;
			island.contacts.Initialize(&island_manager.allocator, ISLAND_STREAM_INITIAL_SIZE);
			IslandAddContact(island, constraint);
			unsigned int island_handle = island_manager.islands.Add(&island);

			// Also, add them to the entity table
			island_manager.entity_table.Insert(island_handle, first_entity);
			island_manager.entity_table.Insert(island_handle, second_entity);
		}
		else {
			// Both entities belong to the same island already - shouldn't happen
			ECS_CRASH("IslandManager: Trying to add a contact constraint that already exists!");
		}
	}
	else {
		// Entities belong to different island - those need to be merged, if there are 2 islands,
		// Or the constraint can be added directly to the island if one of the islands does not exist (i.e. the entity didn't exist beforehand)
		if (first_entity_island == -1) {
			// Add the first entity directly to the second island
			Island& island = island_manager.islands[second_entity_island];
			IslandAddContact(island, constraint);
			island_manager.entity_table.Insert(second_entity_island, first_entity);
		}
		else if (second_entity_island == -1) {
			// Add the second entity directly to the first island
			Island& island = island_manager.islands[first_entity_island];
			IslandAddContact(island, constraint);
			island_manager.entity_table.Insert(first_entity_island, second_entity);
		}
		else {
			// Both islands exist and are distinct - merge them
			MergeIslands(island_manager, first_entity_island, second_entity_island);
		}
	}
}

void IslandManagerInitialize(IslandManager& island_manager, MemoryManager* allocator_source) {
	island_manager.allocator = MemoryManager(ALLOCATOR_SIZE, ALLOCATOR_COUNT, ALLOCATOR_BACKUP_SIZE, allocator_source);
	island_manager.islands.Initialize(&island_manager.allocator, ISLAND_MANAGER_INITIAL_ISLAND_CAPACITY);
	island_manager.entity_table.Initialize(&island_manager.allocator, ISLAND_MANAGER_ENTITY_TABLE_INITIAL_SIZE);
}