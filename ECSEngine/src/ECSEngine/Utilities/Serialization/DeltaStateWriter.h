#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../BasicTypes.h"
#include "../File.h"

namespace ECSEngine {

#define ECS_DELTA_STATE_WRITER_VERSION 0

	// When writing into the buffer, these values must be modified such that the writer can pick up on it
	struct DeltaStateWriterDeltaFunctionData {
		void* user_data;
		const void* current_data;
		float elapsed_seconds;
		uintptr_t write_buffer;
		size_t write_capacity;
	};

	// When writing into the buffer, these values must be modified such that the writer can pick up on it
	struct DeltaStateWriterEntireFunctionData {
		void* user_data;
		const void* current_data;
		float elapsed_seconds;
		uintptr_t write_buffer;
		size_t write_capacity;
	};

	enum ECS_DELTA_STATE_WRITER_STATUS : unsigned char {
		ECS_DELTA_STATE_OK,
		ECS_DELTA_STATE_FAILURE,
		ECS_DELTA_STATE_LARGER_BUFFER_NEEDED
	};

	// The functor should return the appropriate status. The functor will be called again with a larger size when
	// A larger buffer needed status is returned.
	typedef ECS_DELTA_STATE_WRITER_STATUS (*DeltaStateWriterDeltaFunction)(DeltaStateWriterDeltaFunctionData* data);

	// The functor should return the appropriate status. The functor will be called again with a larger size when
	// A larger buffer needed status is returned.
	typedef ECS_DELTA_STATE_WRITER_STATUS (*DeltaStateWriterEntireFunction)(DeltaStateWriterEntireFunctionData* data);

	struct DeltaStateWriterInitializeInfo {
		AllocatorPolymorphic allocator;
		size_t user_data_size;
		DeltaStateWriterDeltaFunction delta_function;
		DeltaStateWriterEntireFunction entire_function;
		size_t entire_state_chunk_capacity;
		size_t delta_state_chunk_capacity;
		ECS_INT_TYPE delta_state_info_integer_type = ECS_INT32;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick = 0.0f;
		// An optional field to initialize this writer with a user defined header
		Stream<void> header = {};
	};

	// A helper structure that helps in writing input serialization data in an efficient
	struct ECSENGINE_API DeltaStateWriter {
		// Deallocates all the memory used by this writer
		void Deallocate();

		// Deallocates the header and disables writing it for future operations
		void DisableHeader();

		// Returns the user pointer data that you can initialize to certain values. It is memsetted to 0 by default.
		void* Initialize(const DeltaStateWriterInitializeInfo* initialize_info);

		// Returns the total byte write size for the current data (it includes its own header - not the user provided header - as well)
		size_t GetWriteSize() const;

		// Register a new state to be written, for the given time. Returns a status code that informs the user whether
		// Or not the operation succeeded, and in case it did not, if it is because the allocator is not large enough.
		ECS_DELTA_STATE_WRITER_STATUS Register(const void* current_data, float elapsed_seconds);

		// Returns true if it successfully wrote the entire data into the given buffer, else false (there is not enough capacity)
		bool WriteTo(uintptr_t& buffer, size_t& buffer_capacity) const;

		// Returns true if it successfully wrote the entire data into the given file, else false
		bool WriteTo(ECS_FILE_HANDLE file) const;

		// This is the struct that is stored in delta_state_count
		template<typename IntegerType>
		struct DeltaStateInfo {
			IntegerType count;
			IntegerType total_byte_size;
		};

		struct EntireStateInfo {
			unsigned int write_size;
			// Store the elapsed seconds for the entire state in the header to allow fast seeking
			float elapsed_seconds;
		};

		DeltaStateWriterDeltaFunction delta_function;
		DeltaStateWriterEntireFunction entire_function;
		void* user_data;
		// This a header that can be added before the actual data to write
		Stream<void> header;

		AllocatorPolymorphic allocator;
		// Maintain the entire state in a chunked fashion such that we eliminate the need to
		// Copy the data with a normal array.
		ResizableStream<CapacityStream<void>> entire_state_chunks;
		// Maintain the delta states in between the entire states, chunked for the same reason
		ResizableStream<CapacityStream<void>> delta_state_chunks;
		// Info about the delta states, specifically, information to help with seeking
		// The number of entries in this array must be the same as the entire_state_count
		// In order to avoid adding a template parameter to the DeltaStateWriter, use an untyped
		// Stream that has as elemetns DeltaStateInfo with a specific integer type that is given as a runtime parameter
		ResizableStream<void> delta_state_infos;
		// The size of an entire state chunk
		size_t entire_state_chunk_capacity;
		// The size of a delta state chunk
		size_t delta_state_chunk_capacity;
		// Additional information about the entire states written immediately after the header
		ResizableStream<EntireStateInfo> entire_state_infos;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick;
		// The elapsed second duration of the last entire write
		float last_entire_state_write_seconds;
		ECS_INT_TYPE delta_state_integer_type;
	};

}