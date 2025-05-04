#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorTypes.h"
#include "StreamUtilities.h"

namespace ECSEngine {

	// An interface which is used by the generic function to determine the delta change set from a source
	// Container to a destination container. The containers should be stored inside the derived structure.
	// To apply a delta change, implement the ApplyDeltaChangeInterface and call ApplyDeltaChange().
	struct DetermineArrayDeltaChangeInterface {
		// This function is called when a new entry has been found in the destination that does not exist in the source
		virtual void NewEntry(size_t destination_index) = 0;

		// This function is called when an entry has been found in the source that no longer exists in the destination
		virtual void RemovedEntry(size_t source_index) = 0;

		// This function is called when an entry that exists in both the source and destination has changed its index.
		// Important! The source index is not necessarily the original source index (i.e. the index inside the current source
		// Container), but rather an adjusted index which takes into consideration additions and removals.
		virtual void MovedEntry(size_t source_index, size_t destination_index) = 0;

		// Should return true when the source entry is the same as the destination entry, else false
		virtual bool CompareEntries(size_t source_index, size_t destination_index) = 0;

		// Returns the hashing value for a source element
		virtual unsigned int HashSourceElement(size_t source_index) = 0;
		
		// Returns the hashing value for a destination element
		virtual unsigned int HashDestinationElement(size_t destination_index) = 0;

		// Returns the number of elements in the source container
		virtual size_t GetSourceCount() = 0;

		// Returns the number of elements in the destination container
		virtual size_t GetDestinationCount() = 0;
	};

	// This structure helps in standardizing the way a apply delta change is performed for a container. This steps ensure that the
	// Proper final container is reconstructed from the source container.
	template<typename IntegerType>
	struct ApplyArrayDeltaChangeInterface {
		static_assert(std::is_unsigned_v<IntegerType>, "ApplyArrayDeltaChangeInterface requires an unsigned integer type as template argument!");

		// This function is called when one of the container's entries needs to be removed.
		// The index of the entry is provided in this call. It assumes that the removals are done
		// In an array like RemoveSwapBack fashion.
		virtual void RemoveEntry(IntegerType index) = 0;

		// This function is called when a new entry should be created, the interface should know which
		// Entry should be created and it must return the index this new entry should be located at.
		virtual IntegerType CreateNewEntry() = 0;

		// This function is called when an element needs to be moved from one index to another index.
		// The indices that are provided are fixed indices, taking into account the swaps that take place
		// With previous swaps.
		virtual void MoveEntry(IntegerType previous_index, IntegerType new_index) = 0;

		// Returns an iterator with the moved entries that need to be performed on this container.
		virtual IteratorInterface<const MovedElementIndex<std::remove_const_t<IntegerType>>>* GetMovedEntries() = 0;

		// Returns an iterator with the entries to be removed. The index of each entry is the index
		// Inside the container when in the initial state, without considering the RemoveSwapBack that
		// Take place. The ApplyDeltaChange() function takes care of adjusting the indices.
		virtual IteratorInterface<const std::remove_const_t<IntegerType>>* GetRemovedEntries() = 0;

		// Returns the number of new entries that need to be created.
		virtual IntegerType GetNewEntryCount() = 0;

		// Returns the number of entries in the container.
		virtual IntegerType GetContainerSize() = 0;
	};

	// Performs the generic algorithm of finding the delta change operations between a source and destination container.
	// A temporary allocator can be provided for this function to use for temporary allocations, else it will use the default of Malloc.
	ECSENGINE_API void DetermineArrayDeltaChange(DetermineArrayDeltaChangeInterface* delta_change, AllocatorPolymorphic temporary_allocator = { nullptr });

	// Performs a generic apply delta change operation, which is usually generated with DetermineDeltaChange such that
	// Previously recorded delta changes can be used to reproduce the state of a destination container from a source container.
	// A temporary allocator can be provided to use for temporary allocations, else it will the default of Malloc.
	template<typename IntegerType>
	ECSENGINE_API void ApplyArrayDeltaChange(ApplyArrayDeltaChangeInterface<IntegerType>* apply_change, AllocatorPolymorphic temporary_allocator = { nullptr });

}