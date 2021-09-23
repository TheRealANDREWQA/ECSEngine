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

	ECSENGINE_API bool SerializeMultisection(std::ofstream& stream, Stream<SerializeMultisectionData> data);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeMultisection(CapacityStream<void>& stream, Stream<SerializeMultisectionData> data);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeMultisectionSize(Stream<SerializeMultisectionData> data);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		std::ifstream& ECS_RESTRICT stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		CapacityStream<void>& memory_pool
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the total amount of memory allocated
	ECSENGINE_API size_t DeserializeMultisection(
		std::ifstream& stream,
		CapacityStream<SerializeMultisectionData>& data,
		void* allocator,
		AllocatorType allocator_type,
		bool* success = nullptr
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the total amount of memory allocated
	ECSENGINE_API size_t DeserializeMultisection(
		uintptr_t& stream,
		CapacityStream<SerializeMultisectionData>& data,
		void* allocator,
		AllocatorType allocator_type
	);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionCount(uintptr_t stream);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionCount(std::ifstream& stream);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionSize(uintptr_t stream);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeMultisectionSize(uintptr_t stream, Stream<size_t> multisection_data_stream_count);

}