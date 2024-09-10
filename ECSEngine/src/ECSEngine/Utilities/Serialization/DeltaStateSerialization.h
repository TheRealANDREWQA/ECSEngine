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

	// The functor must know the read instrument type as well
	struct DeltaStateReaderDeltaFunctionData {
		void* user_data;
		void* current_data;
		void* read_instrument;
		float elapsed_seconds;
		Stream<void> header;
	};

	// The functor must know the read instrument type as well
	struct DeltaStateReaderEntireFunctionData {
		void* user_data;
		void* current_data;
		void* read_instrument;
		float elapsed_seconds;
		Stream<void> header;
	};

	// Should return true if it succeeded reading the values, else false
	typedef bool (*DeltaStateReaderDeltaFunction)(DeltaStateReaderDeltaFunctionData* data);
	
	// Should return true if it succeeded reading the values, else false
	typedef bool (*DeltaStateReaderEntireFunction)(DeltaStateReaderEntireFunctionData* data);

	struct DeltaStateWriterInitializeInfo {
		AllocatorPolymorphic allocator;
		size_t user_data_size;
		DeltaStateWriterDeltaFunction delta_function;
		DeltaStateWriterEntireFunction entire_function;
		size_t entire_state_chunk_capacity;
		size_t delta_state_chunk_capacity;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick = 0.0f;
		// An optional field to initialize this writer with a user defined header
		Stream<void> header = {};
	};

	struct DeltaStateEntireStateInfo {
		unsigned int write_size;
		// The number of delta states up until the next entire state
		unsigned int delta_count;
		// Store the elapsed seconds for the entire state in the header to allow fast seeking
		float elapsed_seconds;
	};

	struct DeltaStateDeltaInfo {
		float elapsed_seconds;
		unsigned int write_size;
	};

	struct WriteInstrument;
	struct ReadInstrument;

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

		// Returns true if it successfully wrote the entire data, else false
		bool WriteTo(WriteInstrument* write_instrument) const;

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
		// This array holds additional information per each delta state
		ResizableStream<DeltaStateDeltaInfo> delta_state_infos;
		// Additional information about the entire states written immediately after the header
		ResizableStream<DeltaStateEntireStateInfo> entire_state_infos;
		// The size of an entire state chunk
		size_t entire_state_chunk_capacity;
		// The size of a delta state chunk
		size_t delta_state_chunk_capacity;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick;
		// The elapsed second duration of the last entire write
		float last_entire_state_write_seconds;
	};

	struct DeltaStateReaderInitializeInfo {
		AllocatorPolymorphic allocator;
		size_t user_data_size;
		DeltaStateReaderDeltaFunction delta_function;
		DeltaStateReaderEntireFunction entire_function;
		// This is an optional parameter. In case the header read part fails, the reason
		// Will be filled in this parameter
		CapacityStream<char>* error_message = nullptr;
	};

	struct ReadInstrument;

	struct ECSENGINE_API DeltaStateReader {
		// Advances the state to the specified continuous moment. If no entire state or delta state is found, nothing will be performed.
		// Returns true if it succeeded, else false (the serialized state is invalid or IO related operations failed)
		// The last argument is an optional parameter that can be used to be filled with a more descriptive error
		bool Advance(float elapsed_seconds, CapacityStream<char>* error_message = nullptr);

		// It will set the delta index to start at the first delta state that starts from that entire state
		void AdjustDeltaIndexForEntireIndex();

		void Deallocate();

		// Returns index of the last entire state before the given moment. Returns -1 if there is no such entry
		size_t GetEntireStateIndexForTime(float elapsed_seconds) const;

		// Returns the offset from the beginning of the serialized range up until the entire state for the given index
		size_t GetOffsetForEntireState(size_t index) const;

		// Returns the offset from the beginning of the serialized range up until the delta state for the given index
		size_t GetOffsetForDeltaState(size_t overall_delta_index) const;

		// Returns the pointer to the user data such that you can initialize the data properly. It will be 0'ed.
		// Returns nullptr if it failed to read the header portion of the state. The read instrument must be stable
		// For the entire duration of using this reader
		void* Initialize(const DeltaStateReaderInitializeInfo& initialize_info, ReadInstrument* read_instrument);

		// Skips forward/backwards to the specified moment in time. It will choose the most optimal route, by
		// Finding the last entire state write before the specified moment and then applying deltas until the specified moment
		// Returns true if it succeeded, else false (the serialized state is invalid or IO related operations failed)
		bool Seek(float elapsed_seconds);

		DeltaStateReaderDeltaFunction delta_function;
		DeltaStateReaderEntireFunction entire_function;
		void* user_data;
		ReadInstrument* read_instrument;
		// This is the user defined header that was written in the file
		Stream<void> header;

		AllocatorPolymorphic allocator;
		CapacityStream<DeltaStateEntireStateInfo> entire_state_infos;
		CapacityStream<DeltaStateDeltaInfo> delta_state_infos;
		// The following 2 fields are used for when sequential execution is desired
		size_t current_entire_state_index;
		size_t current_delta_state_index;
		// This caches the offset from the beginning of the serialized range up to the first entire state
		size_t entire_state_start_offset;
		// This caches the total write size for the entire states
		size_t entire_state_total_write_size;
	};

}