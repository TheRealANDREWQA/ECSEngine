#pragma once
#include "../../../Core.h"
#include "../../../Containers/Stream.h"
#include "../../File.h"

namespace ECSEngine {

	// If the name is nullptr, the name will be skipped when serializing
	struct SerializeMultisectionData {
		Stream<Stream<void>> data;
		const char* name = nullptr;
	};

	// ---------------------------------------------------------------------------------------------------------------

	// The allocator polymorphic is needed to allocate the buffer to which the contents will be written and then commited to disk
	// SettingsAllocator nullptr means use malloc
	ECSENGINE_API bool SerializeMultisection(Stream<SerializeMultisectionData> data, Stream<wchar_t> file, AllocatorPolymorphic allocator = { nullptr }, Stream<void> header = {nullptr, 0});

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeMultisection(Stream<SerializeMultisectionData> data, uintptr_t& stream, Stream<void> header = { nullptr, 0 });

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeMultisectionSize(Stream<SerializeMultisectionData> data, Stream<void> header = { nullptr, 0 });

	// ---------------------------------------------------------------------------------------------------------------

	// The whole file is read into a memory buffer and all of the multisections will point inside it. The memory buffer
	// is allocated from the allocator. If the read operation fails, it returns nullptr. It will set all the available multisections
	// (i. e. there is no name filtering). The buffer if not nullptr must be deallocated afterwards!!!
	// From the memory pool there will be allocated the individual Stream<void>'s of each multisection
	ECSENGINE_API void* DeserializeMultisection(
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// The whole file is read into a memory buffer and all of the multisections will point inside it. The memory buffer
	// is allocated from the allocator. If the read operation fails, it returns nullptr. It will set all the available multisections
	// (i. e. there is no name filtering). The buffer if not nullptr must be deallocated afterwards!!!
	// From the memory pool there will be allocated the individual Stream<void>'s of each multisection
	ECSENGINE_API void* DeserializeMultisectionWithMatch(
		Stream<SerializeMultisectionData> data,
		CapacityStream<void>& memory_pool,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr },
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes. It can return -1 (error) if the header capacity is not big enough to read the header. 
	// The multisections will point inside the given stream. There is no name filtering.
	// From the memory pool there will be allocated the individual Stream<void>'s of each multisection
	ECSENGINE_API size_t DeserializeMultisection(
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		uintptr_t& stream,
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes. It can return -1 (error) if the header capacity is not big enough to read the header. 
	// The multisections will point inside the given stream. There is no name filtering.
	// From the memory pool there will be allocated the individual Stream<void>'s of each multisection
	ECSENGINE_API size_t DeserializeMultisectionWithMatch(
		Stream<SerializeMultisectionData> data,
		CapacityStream<void>& memory_pool,
		uintptr_t& stream,
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionCount(uintptr_t stream, size_t header_size = 0);

	// ---------------------------------------------------------------------------------------------------------------

	// The total amount of data for all multisections
	ECSENGINE_API size_t DeserializeMultisectionSize(uintptr_t stream);

	// ---------------------------------------------------------------------------------------------------------------

	// Multisections[index].size will be the size of the entire multisection - not the number of streams
	ECSENGINE_API void DeserializeMultisectionPerSectionSize(
		uintptr_t stream,
		CapacityStream<size_t>& multisections
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Multisections[index].size will be the size of the entire multisection - not the number of streams
	// This overload will filter the multisections to only those specified
	ECSENGINE_API void DeserializeMultisectionPerSectionSizeWithMatch(
		uintptr_t stream,
		Stream<SerializeMultisectionData> multisections
	);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeMultisectionStreamCount(uintptr_t stream, CapacityStream<size_t>& multisection_stream_count);

	// ---------------------------------------------------------------------------------------------------------------

	// Each multisection must have the name set before hand. It works the same as the other variant, the only difference being that 
	// it will filter the multisections according to the names
	ECSENGINE_API void DeserializeMultisectionStreamCountWithMatch(uintptr_t stream, Stream<SerializeMultisectionData> multisections);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionHeaderSize(uintptr_t stream);

	// ---------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API void DeserializeMultisectionHeader(uintptr_t stream, CapacityStream<void>& header);

	// ---------------------------------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------------------------------
	
	// ---------------------------------------------------------------------------------------------------------------

}