#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../BasicTypes.h"
#include "../File.h"

namespace ECSEngine {

#define ECS_DELTA_STATE_WRITER_VERSION 0

	struct WriteInstrument;
	struct ReadInstrument;
	struct World;

	// Initializes the data with the given allocator. This function exists to allow setup functions be decoupled from the
	// The allocator that can be bound later on
	typedef void (*DeltaStateUserDataAllocatorInitialize)(void* user_data, AllocatorPolymorphic allocator);

	// Deallocates the buffers that were initially allocated from this allocator
	typedef void (*DeltaStateUserDataAllocatorDeallocate)(void* user_data, AllocatorPolymorphic allocator);

	struct DeltaStateWriterDeltaFunctionData {
		void* user_data;
		float elapsed_seconds;
		WriteInstrument* write_instrument;
	};

	struct DeltaStateWriterEntireFunctionData {
		void* user_data;
		float elapsed_seconds;
		WriteInstrument* write_instrument;
	};

	// The functor returns true if it managed to write the state successfully, else false
	typedef bool (*DeltaStateWriterDeltaFunction)(DeltaStateWriterDeltaFunctionData* data);

	// The functor returns true if it managed to write the state successfully, else false
	typedef bool (*DeltaStateWriterEntireFunction)(DeltaStateWriterEntireFunctionData* data);

	// Returns the current moment in time for the given data
	typedef float (*DeltaStateWriterSelfContainedExtractFunction)(void* data);

	// The functor must know the read instrument type as well
	struct DeltaStateReaderDeltaFunctionData {
		Stream<void> header;
		void* user_data;
		ReadInstrument* read_instrument;
		// The total write size of the current data
		size_t write_size;
		float elapsed_seconds;
	};

	// The functor must know the read instrument type as well
	struct DeltaStateReaderEntireFunctionData {
		Stream<void> header;
		void* user_data;
		ReadInstrument* read_instrument;
		// The total write size of the current data
		size_t write_size;
		float elapsed_seconds;
	};

	// Should return true if it succeeded reading the values, else false
	typedef bool (*DeltaStateReaderDeltaFunction)(DeltaStateReaderDeltaFunctionData* data);
	
	// Should return true if it succeeded reading the values, else false
	typedef bool (*DeltaStateReaderEntireFunction)(DeltaStateReaderEntireFunctionData* data);

	struct DeltaStateInfo {
		float elapsed_seconds;
		size_t write_size;
	};
	
	struct DeltaStateWriterInitializeFunctorInfo {
		// If the buffer is nullptr but the data size is non zero, then it will allocate a buffer that will be 0'ed,
		// Else it will copy the data from the user data into the allocated buffer
		Stream<void> user_data;
		DeltaStateWriterDeltaFunction delta_function;
		DeltaStateWriterEntireFunction entire_function;
		// An optional field to initialize this writer with a user defined header
		Stream<void> header = {};
		// When this field is set, it signals to the writer that it doesn't need parameters in order to write the state,
		// Only a call to Write is enough, but the elapsed seconds needs to be extracted out of it in order for the writer to
		// Work properly
		DeltaStateWriterSelfContainedExtractFunction self_contained_extract = nullptr;
		// Optional function that is called in initialize for the user data to allocate buffers, if it needs to.
		// The deallocate function must be specified as well
		DeltaStateUserDataAllocatorInitialize user_data_allocator_initialize = nullptr;
		// Must be set when the initialize function was specified
		DeltaStateUserDataAllocatorDeallocate user_data_allocator_deallocate = nullptr;
	};

	struct DeltaStateWriterInitializeInfo {
		// This information is per functor specific
		DeltaStateWriterInitializeFunctorInfo functor_info;

		// These are customization points that a user can modify agnostic of the functor
		AllocatorPolymorphic allocator;
		WriteInstrument* write_instrument;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick = 0.0f;
	};

	// A helper structure that helps in writing input serialization data in an efficient
	struct ECSENGINE_API DeltaStateWriter {
		// Deallocates all the memory used by this writer
		void Deallocate();

		// Writes the remaining data that the writer has to write. It returns true if it succeeded, else false
		bool Flush();

		// Returns the user pointer data that you can initialize to certain values. It is memsetted to 0 by default
		void Initialize(const DeltaStateWriterInitializeInfo& initialize_info);

		// Register a new state to be written, for the given time. Returns true if it succeeded in writing the state,
		// Else false
		bool Write(float elapsed_seconds);

		// Register a new state to be written, when the functor is self contained. It asserts that the functor is self contained.
		// Returns true if it succeeded in writing the state, else false
		bool Write();

		DeltaStateWriterDeltaFunction delta_function;
		DeltaStateWriterEntireFunction entire_function;
		DeltaStateWriterSelfContainedExtractFunction extract_function;
		DeltaStateUserDataAllocatorDeallocate user_deallocate_function;
		Stream<void> user_data;
		// This a header that can be added before the actual data to write
		Stream<void> header;
		WriteInstrument* write_instrument;

		AllocatorPolymorphic allocator;
		// This array holds additional information per each delta state - either an entire state or a delta state
		ResizableStream<DeltaStateInfo> state_infos;
		// This array holds the indices of entire states in the state infos. This helps distinguish between delta
		// And entire states.
		ResizableStream<unsigned int> entire_state_indices;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick;
		// The elapsed second duration of the last entire write
		float last_entire_state_write_seconds;
	};

	struct DeltaStateReaderInitializeFunctorInfo {
		// If the buffer is nullptr but the data size is non zero, then it will allocate a buffer that will be 0'ed,
		// Else it will copy the data from the user data into the allocated buffer
		Stream<void> user_data;
		DeltaStateReaderDeltaFunction delta_function;
		DeltaStateReaderEntireFunction entire_function;
		// Optional function that is called in initialize for the user data to allocate buffers, if it needs to.
		// The deallocate function must be specified as well
		DeltaStateUserDataAllocatorInitialize user_data_allocator_initialize = nullptr;
		// Must be set when the initialize function was specified
		DeltaStateUserDataAllocatorDeallocate user_data_allocator_deallocate = nullptr;
	};

	struct DeltaStateReaderInitializeInfo {
		// This is functor specific
		DeltaStateReaderInitializeFunctorInfo functor_info;

		// These fields are customization points agnostic of the functor
		AllocatorPolymorphic allocator;
		ReadInstrument* read_instrument;
		// This is an optional parameter. In case the header read part fails, the reason
		// Will be filled in this parameter
		CapacityStream<char>* error_message = nullptr;
	};

	struct ECSENGINE_API DeltaStateReader {
		// Advances the state to the specified continuous moment. If no entire state or delta state is found, nothing will be performed.
		// Returns true if it succeeded, else false (the serialized state is invalid or IO related operations failed)
		// The last argument is an optional parameter that can be used to be filled with a more descriptive error if it happens
		bool Advance(float elapsed_seconds, CapacityStream<char>* error_message = nullptr);

		// Advances a single state, either delta, if the next one is a delta state, or to a new entire state
		// If it is the next in line. Returns true if it succeeded, else false (the serialized state is invalid or
		// IO related operations failed). The last argument is an optional parameter that can be used to be filled
		// With a more descriptive error if it happens
		bool AdvanceOneState(CapacityStream<char>* error_message = nullptr);

		// Calls the entire state with the given overall state index and returns what the functor returned.
		// It assumes that the read instrument is already at the start of this state.
		bool CallEntireState(size_t overall_state_index, CapacityStream<char>* error_message = nullptr) const;

		// Calls the entire state with the given overall state index and returns what the functor returned.
		// It assumes that the read instrument is already at the start of this state.
		bool CallDeltaState(size_t overall_state_index, CapacityStream<char>* error_message = nullptr) const;

		void Deallocate();

		// Returns the overall state index of the last entire state before the given moment. Returns -1 if there is no such entry
		size_t GetEntireStateIndexForTime(float elapsed_seconds) const;

		// It will search the last state that should be applied before the given elapsed seconds, starting from the current state index
		// It will return the current state index - 1 in case the given moment is before the current moment, which can be -1 if the current state index is 0
		size_t GetStateIndexFromCurrentIndex(float elapsed_seconds) const;

		// Returns the offset from the start of the serialized range up until the start of the state
		size_t GetOffsetForState(size_t index) const;

		// Returns true if the given overall state index is an entire state, else false
		bool IsEntireState(size_t index) const;

		// Returns the pointer to the user data such that you can initialize the data properly. It will be 0'ed.
		// Returns nullptr if it failed to read the header portion of the state. The read instrument must be stable
		// For the entire duration of using this reader
		void Initialize(const DeltaStateReaderInitializeInfo& initialize_info);

		// Returns true if it succeeded in seeking at the beginning of the given state, else false.
		bool SeekInstrumentAtState(size_t state_index, CapacityStream<char>* error_message = nullptr) const;

		// Skips forward/backwards to the specified moment in time. It will choose the most optimal route, by
		// Finding the last entire state write before the specified moment and then applying deltas until the specified moment
		// Returns true if it succeeded, else false (the serialized state is invalid or IO related operations failed)
		bool Seek(float elapsed_seconds, CapacityStream<char>* error_message = nullptr);

		DeltaStateReaderDeltaFunction delta_function;
		DeltaStateReaderEntireFunction entire_function;
		DeltaStateUserDataAllocatorDeallocate user_deallocate_function;
		Stream<void> user_data;
		ReadInstrument* read_instrument;
		// This is the user defined header that was written in the file
		Stream<void> header;

		AllocatorPolymorphic allocator;
		CapacityStream<DeltaStateInfo> state_infos;
		CapacityStream<unsigned int> entire_state_indices;
		// The following field is used for the fast execution of sequential frames
		// This field indicates that the state at this index is the next one to be read
		size_t current_state_index;
	};

}