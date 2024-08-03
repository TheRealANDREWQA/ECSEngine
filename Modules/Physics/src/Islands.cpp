#include "pch.h"
#include "Islands.h"
#include "ContactConstraint.h"
#include "Rigidbody.h"

#define ISLAND_STREAM_INITIAL_SIZE 8

#define ISLAND_MANAGER_INITIAL_ISLAND_CAPACITY 8
#define ISLAND_MANAGER_ENTITY_TABLE_INITIAL_SIZE 8

#define ALLOCATOR_SIZE ECS_MB
#define ALLOCATOR_COUNT 4
#define ALLOCATOR_BACKUP_SIZE ECS_MB

// Moves all entries from the second island to the first island and removes the second island
static void MergeIslands(IslandManager& island_manager, unsigned int first_island_handle, unsigned int second_island_handle) {
	// Moves all contacts from the second island to the first one, and remap all the entities that belonged to the second island
	// To the first island in the entity connection table
	Island& first_island = island_manager.islands.At(first_island_handle);
	Island& second_island = island_manager.islands.At(second_island_handle);
	
	// Firstly, perform the remapping
	for (unsigned int index = 0; index < second_island.contacts.size; index++) {
		// We will repeat the process for some entities, but it is fine, since everything
		// Should be in cache and the process will be fast, and creating an acceleration table
		// That contains the unique entities is not worth it
		Entity remap_first_entity = second_island.contacts[index]->FirstEntity();
		Entity remap_second_entity = second_island.contacts[index]->SecondEntity();

		// We must check that the handle value exists, since static entities don't have entries
		IslandHandle* remap_first_entity_handle = island_manager.GetHandle(remap_first_entity);
		if (remap_first_entity_handle != nullptr) {
			remap_first_entity_handle->handle = first_island_handle;
		}

		IslandHandle* remap_second_entity_handle = island_manager.GetHandle(remap_second_entity);
		if (remap_second_entity_handle != nullptr) {
			remap_second_entity_handle->handle = first_island_handle;
		}
	}

	// Move the contacts
	first_island.AddContacts(second_island.contacts.ToStream());

	// Remove the second island
	island_manager.islands.RemoveSwapBack(second_island_handle);
}

void Island::AddContact(ContactConstraint* constraint) {
	contacts.Add(constraint);
}

void Island::AddContacts(Stream<ContactConstraint*> constraints) {
	contacts.AddStream(constraints);
}

void Island::RemoveContact(ContactConstraint* constraint) {
	// Use a pointer search to remove the entry
	// It should be fast enough, no need for an accelerating hash table
	// Use the fast SIMD search
	size_t index = SearchBytes(contacts.ToStream(), constraint);
	ECS_CRASH_CONDITION(index != -1, "Island: Trying to remove a contact that was not added!");
	contacts.RemoveSwapBack(index);
}

void IslandManager::AddContact(ContactConstraint* constraint) {
	// Find the island which contains the first entity
	Entity first_entity = constraint->FirstEntity();
	Entity second_entity = constraint->SecondEntity();
	IslandHandle* first_entity_island = GetHandle(first_entity);
	IslandHandle* second_entity_island = GetHandle(second_entity);
	unsigned int first_entity_island_handle = first_entity_island == nullptr ? -1 : first_entity_island->handle;
	unsigned int second_entity_island_handle = second_entity_island == nullptr ? -1 : second_entity_island->handle;

	bool is_first_entity_static = constraint->rigidbody_A->is_static;
	bool is_second_entity_static = constraint->rigidbody_B->is_static;
	ECS_CRASH_CONDITION(!is_first_entity_static || !is_second_entity_static, "IslandManager: A pair of static rigidbodies shouldn't be added!");

	auto increment_handle = [](IslandHandle* handle) {
		ECS_CRASH_CONDITION(handle->reference_count < UINT_MAX, "IslandManager: Island handle reference count overflow!");
		handle->reference_count++;
	};

	if (first_entity_island_handle == second_entity_island_handle) {
		if (first_entity_island_handle == -1) {
			// Both entities did not exist beforehand, create an island for them
			Island island;
			island.contacts.Initialize(&allocator, ISLAND_STREAM_INITIAL_SIZE);
			island.AddContact(constraint);
			unsigned int island_handle = islands.Add(&island);

			// Also, add them to the entity table with a reference count of 1
			// Don't insert the entry for a static entity
			if (!is_first_entity_static) {
				entity_table.InsertDynamic(&allocator, { island_handle, 1 }, first_entity);
			}
			if (!is_second_entity_static) {
				entity_table.InsertDynamic(&allocator, { island_handle, 1 }, second_entity);
			}
		}
		else {
			// Both entities belong to the same island already - either it is a repeated addition,
			// Or the entities were connected beforehand by intermediaries. To verify that it is not a repeated addition,
			// It would be too costly
			// Just increment the reference counts for the handles and add the constraint to the island
			increment_handle(first_entity_island);
			increment_handle(second_entity_island);
			islands[first_entity_island_handle].AddContact(constraint);
		}
	}
	else {
		// Entities belong to different island - those need to be merged, if there are 2 islands,
		// Or the constraint can be added directly to the island if one of the islands does not exist (i.e. the entity didn't exist beforehand)
		if (first_entity_island_handle == -1) {
			// Add the first entity directly to the second island
			Island& island = islands[second_entity_island_handle];
			island.AddContact(constraint);
			// Add the entry only if it is not static
			if (!is_first_entity_static) {
				entity_table.InsertDynamic(&allocator, { second_entity_island_handle, 1 }, first_entity);
			}
			increment_handle(second_entity_island);;
		}
		else if (second_entity_island_handle == -1) {
			// Add the second entity directly to the first island
			Island& island = islands[first_entity_island_handle];
			island.AddContact(constraint);
			// Add the entry only if it is not static
			if (!is_second_entity_static) {
				entity_table.InsertDynamic(&allocator, { first_entity_island_handle, 1 }, second_entity);
			}
			increment_handle(first_entity_island);
		}
		else {
			// Both islands exist and are distinct - merge them
			MergeIslands(*this, first_entity_island_handle, second_entity_island_handle);
			// Add the constraint to the first island
			islands[first_entity_island_handle].AddContact(constraint);

			// Increment the reference counts for both entries
			increment_handle(first_entity_island);
			increment_handle(second_entity_island);
		}
	}
}

void IslandManager::Initialize(MemoryManager* allocator_source) {
	allocator = MemoryManager(ALLOCATOR_SIZE, ALLOCATOR_COUNT, ALLOCATOR_BACKUP_SIZE, allocator_source);
	islands.Initialize(&allocator, ISLAND_MANAGER_INITIAL_ISLAND_CAPACITY);
	entity_table.Initialize(&allocator, ISLAND_MANAGER_ENTITY_TABLE_INITIAL_SIZE);
}

void IslandManager::RemoveContact(ContactConstraint* constraint) {
	// Get the island handles for both entities
	Entity first_entity = constraint->FirstEntity();
	Entity second_entity = constraint->SecondEntity();

	unsigned int first_entity_table_index = entity_table.Find(first_entity);
	unsigned int second_entity_table_index = entity_table.Find(second_entity);

	bool is_first_valid = first_entity_table_index != -1;
	bool is_second_valid = second_entity_table_index != -1;

	// If both handles are missing, then this is an invalid call
	ECS_CRASH_CONDITION(is_first_valid || is_second_valid, "IslandManager: Trying to remove a contact that was not added!");

	// If one of the handles is missing, it must be because of an error or
	// The entity is static
	bool is_first_static = constraint->rigidbody_A->is_static;
	bool is_second_static = constraint->rigidbody_B->is_static;

	ECS_CRASH_CONDITION(is_first_valid || is_first_static, "IslandManager: Trying to remove a contact that is invalid! (first entity)");
	ECS_CRASH_CONDITION(is_second_valid || is_second_static, "IslandManager: Trying to remove a contact that is invalid! (second entity)");

	// Returns the island handle value, if the entry exists
	auto handle_entry = [this, constraint](unsigned int table_index) {
		if (table_index != -1) {		
			IslandHandle* handle = entity_table.GetValuePtrFromIndex(table_index);

			// Decrement the reference count
			handle->reference_count--;
			if (handle->reference_count == 0) {
				// Remove the entry from the entity table
				entity_table.EraseFromIndex(table_index);

				const size_t CAPACITY_REDUCTION = 4;
				// If the entity table required capacity is much smaller than the current capacity, trim the extra elements
				if (entity_table.NextCapacity(entity_table.GetCount()) < entity_table.GetCapacity() / CAPACITY_REDUCTION) {
					entity_table.Trim(&allocator);
				}
			}
			return handle->handle;
		}
		return (unsigned int)-1;
	};

	unsigned int first_island = handle_entry(first_entity_table_index);
	unsigned int second_island = handle_entry(second_entity_table_index);

	// Remove the constraint from the island storage - must be done only once
	unsigned int handle_value = first_island == -1 ? second_island : first_island;
	Island& island = islands[handle_value];
	island.RemoveContact(constraint);
}