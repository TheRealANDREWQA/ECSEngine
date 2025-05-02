#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorTypes.h"

namespace ECSEngine {

	// An interface which is used by the generic function to determine the delta change set from a source
	// Container to a destination container. The containers should be stored inside the derived structure.
	struct DeltaChangeInterface {
		// This function is called when a new entry has been found in the destination that does not exist in the source
		virtual void NewEntry(size_t destination_index) = 0;

		// This function is called when an entry has been found in the source that no longer exists in the destination
		virtual void RemovedEntry(size_t source_index) = 0;

		// This function is called when an entry that exists in both the source and destination has changed its index
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

	// Performs the generic algorithm of finding the delta change operations between a source and destination container.
	// A temporary allocator can be provided for this function to use for temporary allocations.
	ECSENGINE_API void DetermineDeltaChange(DeltaChangeInterface* delta_change, AllocatorPolymorphic temporary_allocator = { nullptr });

}