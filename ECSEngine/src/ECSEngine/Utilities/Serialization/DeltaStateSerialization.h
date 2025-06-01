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

	// This functor can be used to write more complicated headers, that cannot be simply added
	// To a flat buffer, or that would be expensive to do so. For this reason, you can use the
	// Write instrument directly to write the necessary extra header data
	// The functor returns true if it succeeded, else false
	typedef bool (*DeltaStateWriterHeaderWriteFunction)(void* data, WriteInstrument* write_instrument);

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

	struct DeltaStateReaderHeaderReadFunctionData {
		// This is the static header that was written
		Stream<void> header;
		void* user_data;
		ReadInstrument* read_instrument;
		// How many bytes the writer used for this extra header data
		size_t write_size;
	};

	// This is the reverse of the HeaderWriteFunction, which allows reading that extra data and
	// Performing operations on the user data to reflect this written data
	// Should return true if it succeeded reading the extra header data and that the data is valid, else false
	typedef bool (*DeltaStateReaderHeaderReadFunction)(DeltaStateReaderHeaderReadFunctionData* data);

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
		// This functor can be used to write more complicated headers, that cannot be simply added
		// To a flat buffer, or that would be expensive to do so. For this reason, you can use the
		// Write instrument directly to write the necessary extra header data
		DeltaStateWriterHeaderWriteFunction header_write_function = nullptr;
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

	// The last argument is optional, it allows you to write extra bytes in the header that you can interpret in the reader.
	// It must have at max ECS_DELTA_STATE_GENERIC_HEADER_CAPACITY size, otherwise it will fail (because there is not enough space).
	// The remaining reserved bytes will be set to 0
	ECSENGINE_API void DeltaStateWriteGenericHeader(CapacityStream<void>& stack_memory, unsigned char version, Stream<void> extra_memory = {});

	// A helper structure that helps in writing serialization data in an efficient manner, which allows
	// Quick seeking times but also efficient storage based on deltas between most states. After the Flush call,
	// The writer will need to be reinitialized in order to be reused
	struct ECSENGINE_API DeltaStateWriter {
		// Deallocates all the memory used by this writer
		void Deallocate();

		// Writes the remaining data that the writer has to write, with the option of writing the elapsed seconds of each call. 
		// It returns true if it succeeded, else false
		bool Flush(bool write_frame_elapsed_seconds = true);

		// Returns true if it succeeded, else false. It can fail if the headers could not be written into the file
		// The header write functor is called before any state is written, so you can reuse data that is initialized in the
		// Header for entire/delta state serialization
		bool Initialize(const DeltaStateWriterInitializeInfo& initialize_info);

		ECS_INLINE bool IsFailed() const {
			return is_failed;
		}

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
		WriteInstrument* write_instrument;

		AllocatorPolymorphic allocator;
		// This array holds additional information per each delta state - either an entire state or a delta state
		ResizableStream<DeltaStateInfo> state_infos;
		// This array holds the indices of entire states in the state infos. This helps distinguish between delta
		// And entire states.
		ResizableStream<unsigned int> entire_state_indices;
		// This array holds the elapsed seconds of each write - even those that didn't get to write anything in the instrument
		ResizableStream<float> frame_elapsed_seconds;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick;

		// This boolean will be set when a write call fails to serialize correctly.
		bool is_failed;
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
		// This is the reverse of the HeaderWriteFunction, which allows reading that extra data and
		// Performing operations on the user data to reflect this written data
		DeltaStateReaderHeaderReadFunction header_read_function = nullptr;
	};

	struct DeltaStateReaderInitializeInfo {
		// This is functor specific - it is valid to have this entry completely zeroed out if you only want to read the
		// Header and perform operations on the data set available in the header
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
		bool CallEntireState(size_t overall_state_index, CapacityStream<char>* error_message = nullptr);

		// Calls the entire state with the given overall state index and returns what the functor returned.
		// It assumes that the read instrument is already at the start of this state.
		bool CallDeltaState(size_t overall_state_index, CapacityStream<char>* error_message = nullptr);

		void Deallocate();

		// Returns the overall state index of the last entire state before the given moment. Returns -1 if there is no such entry
		size_t GetEntireStateIndexForTime(float elapsed_seconds) const;

		// It will search the last state that should be applied before the given elapsed seconds, starting from the current state index
		// It will return the current state index - 1 in case the given moment is before the current moment, which can be -1 if the current state index is 0
		size_t GetStateIndexFromCurrentIndex(float elapsed_seconds) const;

		// Returns the offset from the start of the serialized range up until the start of the state
		size_t GetOffsetForState(size_t index) const;

		// Returns the elapsed seconds for the current state, when advancing sequentially. If the reader has been exhausted,
		// Then it will return the elapsed seconds of the last state.
		ECS_INLINE float GetCurrentStateElapsedSeconds() const {
			return current_state_index < state_infos.size ? state_infos[current_state_index].elapsed_seconds : state_infos.Last().elapsed_seconds;
		}

		// Intended to be used sequentially and when the function HasFrameElapsedSeconds() returns true. This will return the delta time
		// Of the current frame, based on what the original writer's frames. In case HasFrameElapsedSeconds() is false, this will return 0.0f
		ECS_INLINE float GetCurrentFrameDeltaTime() {
			if (frame_elapsed_seconds.size == 0 || current_frame_index >= frame_elapsed_seconds.size - 1) {
				return 0.0f;
			}

			size_t current_frame = current_frame_index++;
			return frame_elapsed_seconds[current_frame + 1] - frame_elapsed_seconds[current_frame];
		}

		// Returns true if the given overall state index is an entire state, else false
		bool IsEntireState(size_t index) const;

		ECS_INLINE bool IsFailed() const {
			return is_failed;
		}

		// Returns true if the states from this reader have been sequentially finished, else false
		ECS_INLINE bool IsFinished() const {
			return current_state_index >= state_infos.size;
		}

		ECS_INLINE bool HasFrameElapsedSeconds() const {
			return frame_elapsed_seconds.size > 0 && current_frame_index < frame_elapsed_seconds.size - 1;
		}

		// Returns the pointer to the user data such that you can initialize the data properly. It will be 0'ed.
		// Returns false if it failed to read the header portion of the state. The read instrument must be stable
		// For the entire duration of using this reader
		bool Initialize(const DeltaStateReaderInitializeInfo& initialize_info);

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
		// This array holds the elapsed seconds of each write - even those that didn't get to write anything in the instrument
		// If the original writer did write these
		CapacityStream<float> frame_elapsed_seconds;
		// The following field is used for the fast execution of sequential frames
		// This field indicates that the state at this index is the next one to be read
		size_t current_state_index;
		// Intended to be used sequentially, to retrieve the delta times of the original writer's frames
		size_t current_frame_index;

		// This value records the offset at which the actual states start, since the user
		// Headers lie before them
		size_t state_instrument_start_offset;

		// This boolean is set when a Seek or Advance operation fails
		bool is_failed;
	};

}