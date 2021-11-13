#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Allocators/AllocatorTypes.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// Make name nullptr if you want this to be serialized anyway
	// Only the first data.size streams will be deserialized
	// When deserializing, data[].buffer must point to the pointer that receives the data
	struct ECSENGINE_API SerializeMultisectionData {
		Stream<Stream<void>> data;
	};

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeMultisection(std::ofstream& stream, Stream<SerializeMultisectionData> data, Stream<void> header = {nullptr, 0});

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeMultisection(CapacityStream<void>& stream, Stream<SerializeMultisectionData> data, Stream<void> header = {nullptr, 0});

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeMultisectionSize(Stream<SerializeMultisectionData> data);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		CapacityStream<void>* ECS_RESTRICT header = nullptr,
		bool* success_status = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>* header = nullptr,
		unsigned int* faulty_index = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		CapacityStream<void>* ECS_RESTRICT header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>* header = nullptr,
		unsigned int* faulty_index = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the total amount of memory allocated
	ECSENGINE_API size_t DeserializeMultisection(
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type,
		CapacityStream<void>* header = nullptr,
		bool* success = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the total amount of memory allocated
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type,
		CapacityStream<void>* header = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionCount(uintptr_t stream, size_t header_size = 0);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionCount(std::ifstream& stream, size_t header_size = 0);

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

	ECSENGINE_API void DeserializeMultisectionStreamCount(std::ifstream& stream, CapacityStream<size_t>& multisection_stream_count);

	// ---------------------------------------------------------------------------------------------------------------

	// These are byte sizes
	ECSENGINE_API void DeserializeMultisectionStreamSizes(uintptr_t stream, CapacityStream<Stream<size_t>>& multisection_sizes);

	// ---------------------------------------------------------------------------------------------------------------

	// These are byte sizes
	ECSENGINE_API void DeserializeMultisectionStreamSizes(std::ifstream& stream, CapacityStream<Stream<size_t>>& multisection_sizes);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionHeaderSize(uintptr_t stream);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionHeaderSize(std::ifstream& stream);

	// ---------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API void DeserializeMultisectionHeader(uintptr_t stream, CapacityStream<void>& header);

	// ---------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API bool DeserializeMultisectionHeader(std::ifstream& stream, CapacityStream<void>& header);

	// ---------------------------------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------------------------------
	
	// ---------------------------------------------------------------------------------------------------------------

}