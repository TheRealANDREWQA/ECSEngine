#include "ecspch.h"
#include "DeltaChange.h"
#include "../Containers/HashTable.h"
#include "../Containers/Deck.h"

namespace ECSEngine {

	void DetermineArrayDeltaChange(DetermineArrayDeltaChangeInterface* delta_change, AllocatorPolymorphic temporary_allocator) {
		// In order to determine the additions, find the entries which don't exist in source but appear in destination.
		// The removals are the entries which exist in source but not in destination.
		// The moved entries are those that changed their index from the source entry to the destination entry.
		
		// In order for this operation to not be O(N * M) where N is the source element count and M is the destination element count,
		// Use a hash table to speed up lookups. We need one that maps the destination entries (the source mapping is not needed).

		// Another important idea is that we need to perform these operations in a very specific manner, such that they can be reconstructed
		// Properly in the apply delta change function. So what we must do is to "perform" the removals first, then find out the new entries,
		// And at last the moved entries. At each point, we must use the indices of that step, not the original indices.

		size_t source_count = delta_change->GetSourceCount();
		size_t destination_count = delta_change->GetDestinationCount();

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

		typedef HashTable<TableEntry, unsigned int, HashFunctionPowerOfTwo> AccelerationTable;

		AccelerationTable acceleration_table;
		acceleration_table.Initialize(temporary_allocator, HashTablePowerOfTwoCapacityForElements(destination_count));
		
		// In order to have proper indices for moved entries, we must perform the removals and additions first,
		// Such that we have the proper indices. This array contains the index of the source element where it is
		// Located after the removals and additions were performed (so basically the delta change state before
		// the moves are performed).
		Stream<size_t> original_source_index;
		original_source_index.Initialize(temporary_allocator, destination_count);
		MakeSequence(original_source_index);

		// This container holds a bit which indicates whether or not the destination element was matched.
		// This is used to determine what elements were added after the step 1 (removal pass) in order to
		// Not perform other hash table lookup and creation of a completely different table.
		BooleanBitField is_destination_matched;
		is_destination_matched.Initialize(temporary_allocator, destination_count);
		// We can call memset even when the destination count is 0 and the buffer is nullptr, because nothing
		// Will be written in that case
		memset(is_destination_matched.m_buffer, 0, is_destination_matched.MemoryOf(destination_count));

		const size_t DECK_EXPONENT = 6;

		DeckPowerOfTwo<ulong2> matched_moved_entries;
		matched_moved_entries.Initialize(temporary_allocator, 1, DECK_EXPONENT);

		__try {
			// Step 0. Insert the entries into their respective hash tables
			for (size_t index = 0; index < destination_count; index++) {
				unsigned int hash = delta_change->HashDestinationElement(index);
				unsigned int table_index = acceleration_table.Find(hash);
				if (table_index == -1) {
					// It didn't exist previously, insert a new entry
					TableEntry entry;
					acceleration_table.Insert(entry, hash, table_index);
				}

				TableEntry* entry = acceleration_table.GetValuePtrFromIndex(table_index);
				entry->Add(index);
			}

			// Step 1. Determine all removals (exist in source but not destination -> iterate over source elements
			// And look them up in the destination table)
			size_t removal_count = 0;

			// Register a source element to be removed.
			auto add_removal = [&](size_t index) {
				delta_change->RemovedEntry(index);
				removal_count++;
				// Must update the mapping of original_source_index.
				original_source_index[source_count - removal_count] = index;
			};

			for (size_t index = 0; index < source_count; index++) {
				unsigned int hash = delta_change->HashSourceElement(index);
				unsigned int table_index = acceleration_table.Find(hash);
				if (table_index != -1) {
					const TableEntry* table_entry = acceleration_table.GetValuePtrFromIndex(table_index);
					// Determine if entry is matched by these hashes. If it is matched, also record the entry
					// Inside the matched_moved_entries container, such that we don't have to repeat this operation.
					size_t subindex = 0;
					for (; subindex < table_entry->index_count; subindex++) {
						if (delta_change->CompareEntries(index, table_entry->indices[subindex])) {
							matched_moved_entries.Add({ index, table_entry->indices[subindex] });
							// Also, set the boolean flag of matched entry in the destination bit field.
							is_destination_matched.Set(table_entry->indices[subindex]);
							break;
						}
					}

					if (subindex == table_entry->index_count) {
						add_removal(index);
					}
				}
				else {
					add_removal(index);
				}
			}
			
			size_t source_count_during_adds = source_count - removal_count;
			size_t addition_count = 0;
			// Step 2. Determine the additions by checking is_destination_matched.
			is_destination_matched.ForEach([&](size_t index, bool is_matched) {
				if (!is_matched) {
					delta_change->NewEntry(index);
					// If the index is smaller or equal to the current, we must consider the swap.
					if (index < source_count_during_adds) {
						original_source_index[source_count_during_adds] = original_source_index[index];
					}
					addition_count++;
				}
			});

			// Step 3. Add the moved elements. We must take into consideration original_source_index
			// To ensure that the appropriate element index is considered.
			matched_moved_entries.ForEachIndex([&](ulong2 move_indices) {
				delta_change->MovedEntry(original_source_index[move_indices.x], move_indices.y);
			});
		}
		__finally {
			acceleration_table.Deallocate(temporary_allocator);
			original_source_index.Deallocate(temporary_allocator);
			matched_moved_entries.Deallocate();
			is_destination_matched.Deallocate(temporary_allocator);
		}
	}

	template<typename IntegerType>
	void ApplyArrayDeltaChange(ApplyArrayDeltaChangeInterface<IntegerType>* apply_delta, AllocatorPolymorphic temporary_allocator) {
		if (temporary_allocator.allocator == nullptr) {
			temporary_allocator = ECS_MALLOC_ALLOCATOR;
		}

		// Start by performing the removals, simply forward to RemoveArrayElements.
		RemoveArrayElements(apply_delta->GetRemovedEntries(), apply_delta->GetContainerSize(), temporary_allocator, [&](IntegerType remove_index) {
			apply_delta->RemoveEntry(remove_index);
		});

		// Perform the additions now.
		CreateArrayElementsWithIndexLocation(apply_delta->GetNewEntryCount(), apply_delta->GetContainerSize(), temporary_allocator, 
			// The create functor
			[&]() {
				return apply_delta->CreateNewEntry();
			},
			// The move functor
			[&](IntegerType previous_index, IntegerType new_index) {
				apply_delta->MoveEntry(previous_index, new_index);
			});

		// At last, perform the moves.
		MoveElementsFunctor(apply_delta->GetContainerSize(), apply_delta->GetMovedEntries(), temporary_allocator, [&](IntegerType previous_index, IntegerType new_index) {
			apply_delta->MoveEntry(previous_index, new_index);
		});
	}

	template ECSENGINE_API void ApplyArrayDeltaChange(ApplyArrayDeltaChangeInterface<unsigned char>*, AllocatorPolymorphic);
	template ECSENGINE_API void ApplyArrayDeltaChange(ApplyArrayDeltaChangeInterface<unsigned short>*, AllocatorPolymorphic);
	template ECSENGINE_API void ApplyArrayDeltaChange(ApplyArrayDeltaChangeInterface<unsigned int>*, AllocatorPolymorphic);
	template ECSENGINE_API void ApplyArrayDeltaChange(ApplyArrayDeltaChangeInterface<size_t>*, AllocatorPolymorphic);

}