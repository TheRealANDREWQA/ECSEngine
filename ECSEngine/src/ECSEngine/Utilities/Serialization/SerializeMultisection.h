#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Allocators/AllocatorTypes.h"
#include "../File.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// Make name nullptr if you want this to be serialized anyway
	// Only the first data.size streams will be deserialized
	// When deserializing, data[].buffer must point to the pointer that receives the data
	struct ECSENGINE_API SerializeMultisectionData {
		Stream<Stream<void>> data;
	};

	// ---------------------------------------------------------------------------------------------------------------

	// The allocator polymorphic is needed to allocate the buffer to which the contents will be written and then commited to disk
	// Allocator nullptr means use malloc
	ECSENGINE_API bool SerializeMultisection(Stream<wchar_t> file, Stream<SerializeMultisectionData> data, AllocatorPolymorphic allocator = { nullptr }, Stream<void> header = {nullptr, 0});

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeMultisection(uintptr_t& stream, Stream<SerializeMultisectionData> data, Stream<void> header = { nullptr, 0 });

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeMultisectionSize(Stream<SerializeMultisectionData> data, Stream<void> header = { nullptr, 0 });

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The allocator is needed to read the whole file into the memory then process it
	// Nullptr means use malloc
	// A value of -1 bytes written means failure
	ECSENGINE_API size_t DeserializeMultisection(
		Stream<wchar_t> file,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		AllocatorPolymorphic allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool,
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the total amount of memory allocated
	// A value of -1 bytes written means failure
	// There are 2 allocators - one for the file when it will be read into the memory
	// The other one is used to determine if the writes will happen directly into the buffers
	// or if allocations will be made
	ECSENGINE_API size_t DeserializeMultisection(
		Stream<wchar_t> file,
		CapacityStream<SerializeMultisectionData>& data,
		AllocatorPolymorphic file_allocator = { nullptr },
		AllocatorPolymorphic buffer_allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the total amount of memory allocated
	// It will write directly into the buffers
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the total amount of memory allocated
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionCount(uintptr_t stream, size_t header_size = 0);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionSize(uintptr_t stream);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionSize(uintptr_t stream, Stream<size_t> multisection_data_stream_count);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeMultisectionPerSectionSize(uintptr_t stream, CapacityStream<size_t>& sizes);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeMultisectionPerSectionSize(
		uintptr_t stream,
		CapacityStream<size_t>& sizes,
		Stream<size_t> multisection_data_stream_count
	);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeMultisectionStreamCount(uintptr_t stream, CapacityStream<size_t>& multisection_stream_count);

	// ---------------------------------------------------------------------------------------------------------------

	// These are byte sizes
	ECSENGINE_API void DeserializeMultisectionStreamSizes(uintptr_t stream, CapacityStream<Stream<size_t>>& multisection_sizes);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionHeaderSize(uintptr_t stream);

	// ---------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API void DeserializeMultisectionHeader(uintptr_t stream, CapacityStream<void>& header);

	// ---------------------------------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------------------------------
	
	// ---------------------------------------------------------------------------------------------------------------

}