#include "ecspch.h"
#include "DeltaChange.h"
#include "../Containers/HashTable.h"

namespace ECSEngine {

	void DetermineDeltaChange(DeltaChangeInterface* delta_change, AllocatorPolymorphic temporary_allocator) {
		// In order to determine the additions, find the entries which don't exist in source but appear in destination.
		// The removals are the entries which exist in source but not in destination.
		// The moved entries are those that changed their index from the source entry to the destination entry.
		
		// In order for this operation to not be O(N * M) where N is the source element count and M is the destination element count,
		// Use a hash table to speed up lookups. Hash the container with fewer elements and then iterate over the other container
		// To determine if an element exists or not.
		size_t source_count = delta_change->GetSourceCount();
		size_t destination_count = delta_change->GetDestinationCount();

		size_t smaller_count = min(source_count, destination_count);
		// If the allocator is not specified, then use malloc
		if (temporary_allocator.allocator == nullptr) {
			temporary_allocator = ECS_MALLOC_ALLOCATOR;
		}

		// In case the hash has collisions, store all original indices
		// TODO: At the moment, this is fixed size, without a resizing backup.
		// Implement the resizable variant only if there is an actual need, which is detected with an assert.
		struct TableEntry {
			ECS_INLINE void Add(size_t value) {
				ECS_ASSERT(index_count < ECS_COUNTOF(indices), "Resizable code path not implemented");
				indices[index_count++] = value;
			}

			size_t indices[3];
			size_t index_count = 0;
		};

		// Maps from hashed value to the index inside the smaller container
		HashTable<TableEntry, unsigned int, HashFunctionPowerOfTwo> acceleration_table;
		acceleration_table.Initialize(temporary_allocator, HashTablePowerOfTwoCapacityForElements(smaller_count));
		
		__try {
			bool is_source_hashed = smaller_count == source_count;
			for (size_t index = 0; index < smaller_count; index++) {
				unsigned int hash = is_source_hashed ? delta_change->HashSourceElement(index) : delta_change->HashDestinationElement(index);
				unsigned int table_index = acceleration_table.Find(hash);
				if (table_index == -1) {
					// It didn't exist previously, insert a new entry
					TableEntry entry;
					acceleration_table.Insert(entry, hash, table_index);
				}

				TableEntry* entry = acceleration_table.GetValuePtrFromIndex(table_index);
				entry->Add(index);
			}

			// Iterate over the other container and perform the lookups
			size_t other_container_count = is_source_hashed ? destination_count : source_count;
			for (size_t index = 0; index < other_container_count; index++) {
				unsigned int hash = is_source_hashed ? delta_change->HashDestinationElement(index) : delta_change->HashSourceElement(index);
				unsigned int table_index = acceleration_table.Find(hash);
				if (table_index == -1) {
					// The entry doesn't exist. If the source is hashed, it means that this is a new addition, else it is a removal
					if (is_source_hashed) {
						delta_change->NewEntry(index);
					}
					else {
						delta_change->RemovedEntry(index);
					}
				}
				else {
					// Compare against all entries. If it is not found, it is the same as the entry not appearing at all in the table
					const TableEntry* table_entry = acceleration_table.GetValuePtrFromIndex(table_index);
					size_t subindex = 0;
					if (is_source_hashed) {
						size_t destination_index = index;
						for (; subindex < table_entry->index_count; subindex++) {
							if (delta_change->CompareEntries(table_entry->indices[subindex], destination_index)) {
								delta_change->MovedEntry(table_entry->indices[subindex], destination_index);
								break;
							}
						}
					}
					else {
						size_t source_index = index;
						for (; subindex < table_entry->index_count; subindex++) {
							if (delta_change->CompareEntries(source_index, table_entry->indices[subindex])) {
								delta_change->MovedEntry(source_index, table_entry->indices[subindex]);
								break;
							}
						}
					}

					if (subindex == table_entry->index_count) {
						// The entry was not matched, treat it the same as not existing at all in the table
						if (is_source_hashed) {
							delta_change->NewEntry(index);
						}
						else {
							delta_change->RemovedEntry(index);
						}
					}
				}
			}
		}
		__finally {
			acceleration_table.Deallocate(temporary_allocator);
		}
	}

}