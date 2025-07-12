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

	// Returns the current delta time for the given data
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
		// Only a call to Write is enough, but the delta time needs to be extracted out of it in order for the writer to
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
		bool Flush(bool write_frame_delta_times = true);

		// Returns true if it succeeded, else false. It can fail if the headers could not be written into the file
		// The header write functor is called before any state is written, so you can reuse data that is initialized in the
		// Header for entire/delta state serialization
		bool Initialize(const DeltaStateWriterInitializeInfo& initialize_info);

		ECS_INLINE bool IsFailed() const {
			return is_failed;
		}

		// Register a new state to be written, for the given time. Returns true if it succeeded in writing the state,
		// Else false
		bool Write(float delta_time);

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
		// This array holds the delta times of each write - even those that didn't get to write anything in the instrument.
		// It is very important that we record the actual delta time values, not using the elapsed seconds to compute a difference,
		// Since very small inaccuracies can crop up and make the true byte to byte reconstruction impossible - we need to have the
		// Exact identical delta times written, as received from the user/extract function
		ResizableStream<float> frame_delta_times;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick;
		// This value will accumulate the total elapsed seconds for the writer, based on the delta times received
		float elapsed_seconds_accumulator;

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
		enum ADVANCE_WITH_SUBSTEPS_RETURN : unsigned char {
			// Can continue the substepping
			ADVANCE_WITH_SUBSTEPS_OK,
			// Should stop the substepping, without considering this as a failure
			ADVANCE_WITH_SUBSTEPS_PAUSE,
			// Should stop the substepping because of an error
			ADVANCE_WITH_SUBSTEPS_ERROR
		};

		// Advances the state to the specified continuous moment. If no entire state or delta state is found, nothing will be performed.
		// Returns true if it succeeded, else false (the serialized state is invalid or IO related operations failed)
		// The last argument is an optional parameter that can be used to be filled with a more descriptive error if it happens
		bool Advance(float elapsed_seconds, CapacityStream<char>* error_message = nullptr);

		// Advances a single state, either delta, if the next one is a delta state, or to a new entire state
		// If it is the next in line. Returns true if it succeeded, else false (the serialized state is invalid or
		// IO related operations failed). The last argument is an optional parameter that can be used to be filled
		// With a more descriptive error if it happens
		bool AdvanceOneState(CapacityStream<char>* error_message = nullptr);

		// This function is similar to advance, but it has some differences. It will advance the reader in sync with the frame deltas that were
		// Read, while calling the functor for each separate frame, even when no state was written at that point. This can allow certain readers
		// To obtain a perfect reconstruction of an original state that is dependent on perfect frame delta steps. The functor is called with the
		// Following parameters (float substep_delta_time, CapacityStream<char>* error_message - can be nullptr) and should return ADVANCE_WITH_SUBSTEPS_RETURN. 
		// The number of substeps that is taken is determined by the greatest number of substeps whose delta is smaller or equal to the delta time. 
		// You can limit the number of substeps that can be taken per call with the max_substeps variable, such that this function doesn't fall into the 
		// "well of despair" that physics systems are known for, but it will result in a slowing down of the simulation in that case. 
		// The function returns true if it succeeded (including the functor), else false.
		template<typename Functor>
		bool AdvanceWithSubsteps(float delta_time, unsigned int max_substeps, Functor&& functor, CapacityStream<char>* error_message = nullptr) {
			float substep_accumulated_delta_time = 0.0f;
			unsigned int substep_count = 0;
			
			// Add to the delta time the delta time remainder from the previous call.
			delta_time += advance_with_substeps_remainder;

			// We will exit from inside the while if the delta time is exceeded.
			while (substep_count < max_substeps) {
				float current_substep_delta = GetCurrentFrameDeltaTime();
				if (substep_accumulated_delta_time + current_substep_delta > delta_time) {
					// Decrement the frame index, since we didn't get to play that state.
					current_frame_index--;
					// Record the delta time remainder such that it can be used the next frame.
					advance_with_substeps_remainder = delta_time - substep_accumulated_delta_time;
					break;
				}

				// Determine if we should advance a state or not.
				size_t state_index = GetStateIndexFromCurrentIndex(advance_with_substeps_elapsed_seconds);
				// Current state index indicates the next state to be read, if the index is the same, it means we should advance
				if (!current_state_index.has_value || state_index == current_state_index.value) {
					// Advance one state
					if (!AdvanceOneState(error_message)) {
						return false;
					}
				}

				ADVANCE_WITH_SUBSTEPS_RETURN return_value = functor(current_substep_delta, error_message);
				if (return_value == ADVANCE_WITH_SUBSTEPS_ERROR) {
					return false;
				}
				else if (return_value == ADVANCE_WITH_SUBSTEPS_PAUSE) {
					return true;
				}

				advance_with_substeps_elapsed_seconds += current_substep_delta;
				substep_accumulated_delta_time += current_substep_delta;
				substep_count++;
			}

			// If we exited from the loop because the substeps were exhausted, zero out the remainder,
			// Since it doesn't make too much sense in this case.
			if (substep_count == max_substeps) {
				advance_with_substeps_remainder = 0.0f;
			}

			return true;
		}

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
			if (!current_state_index.has_value) {
				return 0.0f;
			}
			return current_state_index.value < state_infos.size ? state_infos[current_state_index.value].elapsed_seconds : state_infos.Last().elapsed_seconds;
		}

		// Intended to be used sequentially and when the function HasFrameElapsedSeconds() returns true. This will return the delta time
		// Of the current frame, based on what the original writer's frames. In case HasFrameElapsedSeconds() is false, this will return 0.0f
		ECS_INLINE float GetCurrentFrameDeltaTime() {
			if (frame_delta_times.size == 0 || current_frame_index >= frame_delta_times.size - 1) {
				return 0.0f;
			}

			size_t current_frame = current_frame_index++;
			return frame_delta_times[current_frame];
		}

		// For a provided elapsed seconds, it will return the frame index for the provided elapsed seconds. The boolean parameter
		// Determines whether or not it should use the current frame index as an acceleration (can be used when querying this sequentially).
		// If the frames have been exhausted, it will return the last valid index.
		size_t GetFrameIndexFromElapsedSeconds(float elapsed_seconds, bool use_current_frame_variable = false);

		// Returns true if the given overall state index is an entire state, else false
		bool IsEntireState(size_t index) const;

		ECS_INLINE bool IsFailed() const {
			return is_failed;
		}

		// Returns true if the states from this reader have been sequentially finished, else false
		ECS_INLINE bool IsFinished() const {
			return state_infos.size == 0 || (current_state_index.has_value && current_state_index.value >= state_infos.size);
		}

		ECS_INLINE bool HasFrameElapsedSeconds() const {
			return frame_delta_times.size > 0 && current_frame_index < frame_delta_times.size - 1;
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
		// This array holds the delta times of each write - even those that didn't get to write anything in the instrument
		// If the original writer did write these
		CapacityStream<float> frame_delta_times;
		// The following field is used for the fast execution of sequential frames
		// This field indicates that the state at this index is the next one to be read.
		// The optional will be empty if no state was read.
		Optional<size_t> current_state_index;
		// Intended to be used sequentially, to retrieve the delta times of the original writer's frames
		size_t current_frame_index;

		// This value records the offset at which the actual states start, since the user
		// Headers lie before them
		size_t state_instrument_start_offset;

		// This value accumulates the total elapsed seconds for the function advance with substeps.
		// This field is needed because there can be delta times that can be missed because it is not
		// Large enough to trigger a state change. This variable is controlled exclusively inside the
		// AdvanceWithSubsteps function, shouldn't be used in other locations
		float advance_with_substeps_elapsed_seconds;
		// When calling AdvanceWithSubsteps, there can be some delta time that is left out unsimulated.
		// For example:
		// A --- A --- A --- A
		// | -------| ------ |
		// The replay plays at 1.5x slower rate. 2 calls to AdvanceWithSubsteps should simulate 3 substeps.
		// The first time the function is called, only a simulation step is done, and the elapsed seconds is
		// Advanced only by the first frame, but there is still some delta time left, but not enough for another
		// Frame simulation. This delta needs to be recorded and used the next frame.
		float advance_with_substeps_remainder;

		// Similar to the variable "advance_with_substeps_elapsed_seconds", but for the function GetFrameIndexFromElapsedSeconds
		float get_frame_index_elapsed_seconds;

		// This boolean is set when a Seek or Advance operation fails
		bool is_failed;
	};

}