#include "ecspch.h"
#include "DeltaStateSerialization.h"
#include "../PointerUtilities.h"
#include "SerializationHelpers.h"
#include "../InMemoryReaderWriter.h"
#include "../BufferedFileReaderWriter.h"
#include "SerializeIntVariableLength.h"
#include "DeltaStateSerializationForward.h"

#define VERSION 0
#define LAST_ENTIRE_STATE_INITIALIZE -10000.0f

namespace ECSEngine {

	// Important design decision. In order to avoid having the writer needing to know the number of states to be serialized,
	// We write a "footer" that contains that data. This will be serialized in a special manner, with the size at the end such
	// That we know by how much to go back to. This is an usual configuration, but it allows us to write the states as we go,
	// Without having to store them in memory and commit them at a later time. This becomes important for when the state to be written
	// Is very large and would make the application run out of RAM.

	// This is a footer that is used by the writer in order to help accommodate changes
	struct Footer {
		// The total size of the footer data without this footer structure, basically the offset to go back to
		// To get to the start of the footer
		size_t size;
		unsigned char version;
		// Not used at the moment, should be all 0s
		unsigned char reserved[7];
	};

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeltaStateWriter::Deallocate() {
		if (user_deallocate_function != nullptr) {
			user_deallocate_function(user_data.buffer, allocator);
		}
		if (user_data.size > 0) {
			ECSEngine::Deallocate(allocator, user_data.buffer);
		}
		state_infos.FreeBuffer();
		entire_state_indices.FreeBuffer();
		write_instrument = nullptr;
	}

	bool DeltaStateWriter::Flush(bool write_frame_elapsed_seconds) {
		// Write the footer in a reversed fashion, such that sizes appear after the data itself
		size_t footer_start_write_offset = write_instrument->GetOffset();

		// Write the state infos first
		if (!SerializeIntVariableLengthBool(write_instrument, state_infos.size)) {
			return false;
		}
		for (unsigned int index = 0; index < state_infos.size; index++) {
			if (!SerializeIntVariableLengthBool(write_instrument, state_infos[index].write_size)) {
				return false;
			}
			if (!write_instrument->Write(&state_infos[index].elapsed_seconds)) {
				return false;
			}
		}

		// Write the entire state indices
		if (!SerializeIntVariableLengthBool(write_instrument, entire_state_indices.size)) {
			return false;
		}
		for (unsigned int index = 0; index < entire_state_indices.size; index++) {
			if (!SerializeIntVariableLengthBool(write_instrument, entire_state_indices[index])) {
				return false;
			}
		}

		// Write the frame elapsed seconds, if the user specified it
		unsigned int frame_elapsed_seconds_count = write_frame_elapsed_seconds ? frame_elapsed_seconds.size : 0;
		if (!SerializeIntVariableLengthBool(write_instrument, frame_elapsed_seconds_count)) {
			return false;
		}
		if (!write_instrument->Write(frame_elapsed_seconds.buffer, frame_elapsed_seconds_count * frame_elapsed_seconds.MemoryOf(1))) {
			return false;
		}

		// Write the footer now
		size_t footer_end_write_offset = write_instrument->GetOffset();
		size_t footer_size = footer_end_write_offset - footer_start_write_offset;
		ECS_ASSERT(footer_size <= UINT_MAX, "DeltaStateWriter footer total write size exceeded the limit.");
		Footer footer;
		memset(&footer, 0, sizeof(footer));
		footer.size = footer_size;
		footer.version = VERSION;
		if (!write_instrument->Write(&footer)) {
			return false;
		}

		return write_instrument->Flush();
	}

	bool DeltaStateWriter::Initialize(const DeltaStateWriterInitializeInfo& initialize_info) {
		write_instrument = initialize_info.write_instrument;

		// Write the static and dynamic headers into the write instrument now,
		// Before making most allocations, such that we don't have to roll them back.
		// The only allocation that must be made is for the user data, since it might
		// Take constant pointers to itself, and these need to be stable
		if (!write_instrument->WriteWithSizeVariableLength(initialize_info.functor_info.header)) {
			is_failed = true;
			return false;
		}

		allocator = initialize_info.allocator;
		if (initialize_info.functor_info.user_data.size > 0) {
			user_data.Initialize(allocator, initialize_info.functor_info.user_data.size);
			if (initialize_info.functor_info.user_data.buffer != nullptr) {
				user_data.CopyOther(initialize_info.functor_info.user_data.buffer, initialize_info.functor_info.user_data.size);
			}
			else {
				memset(user_data.buffer, 0, user_data.size);
			}
		}
		else {
			user_data = initialize_info.functor_info.user_data;
		}

		if (initialize_info.functor_info.user_data_allocator_initialize != nullptr) {
			ECS_ASSERT(initialize_info.functor_info.user_data_allocator_deallocate != nullptr);
			initialize_info.functor_info.user_data_allocator_initialize(user_data.buffer, allocator);
		}
		user_deallocate_function = initialize_info.functor_info.user_data_allocator_deallocate;

		if (!write_instrument->WriteChunkWithSizeHeaderConditional(initialize_info.functor_info.header_write_function != nullptr, [&](WriteInstrument* write_instrument) {
			return initialize_info.functor_info.header_write_function(user_data.buffer, write_instrument);
		})) {
			// Deallocate the existing data
			if (user_deallocate_function != nullptr) {
				user_deallocate_function(user_data.buffer, allocator);
			}
			if (user_data.size > 0) {
				user_data.Deallocate(allocator);
			}
			// Set the is failed flag to true as well, to provide clarity that it is failed
			is_failed = true;
			return false;
		}

		delta_function = initialize_info.functor_info.delta_function;
		entire_function = initialize_info.functor_info.entire_function;
		extract_function = initialize_info.functor_info.self_contained_extract;
		entire_state_write_seconds_tick = initialize_info.entire_state_write_seconds_tick;
		is_failed = false;
		
		state_infos.Initialize(allocator, 0);
		entire_state_indices.Initialize(allocator, 0);
		frame_elapsed_seconds.Initialize(allocator, 0);
		return true;
	}

	bool DeltaStateWriter::Write(float elapsed_seconds) {
		float last_entire_state_elapsed_seconds = entire_state_indices.size == 0 ? 0.0f : state_infos[entire_state_indices.Last()].elapsed_seconds;
		float entire_state_delta = elapsed_seconds - last_entire_state_elapsed_seconds;

		// Add the elapsed seconds to the array first
		frame_elapsed_seconds.Add(elapsed_seconds);

		if (entire_state_delta >= entire_state_write_seconds_tick || entire_state_indices.size == 0) {
			size_t instrument_offset = write_instrument->GetOffset();

			// The delta was exceeded or it is the first serialization, an entire state must be written
			DeltaStateWriterEntireFunctionData entire_data;
			entire_data.elapsed_seconds = elapsed_seconds;
			entire_data.user_data = user_data.buffer;
			entire_data.write_instrument = write_instrument;
			bool success = entire_function(&entire_data);
			if (success) {
				// Register this new entry
				size_t current_instrument_offset = write_instrument->GetOffset();
				if (current_instrument_offset > instrument_offset) {
					entire_state_indices.Add(state_infos.size);
					state_infos.Add({ elapsed_seconds, current_instrument_offset - instrument_offset });
				}
			}
			else {
				is_failed = true;
			}
			return success;
		}
		else {
			// A delta state can be used
			size_t instrument_offset = write_instrument->GetOffset();

			DeltaStateWriterDeltaFunctionData entire_data;
			entire_data.elapsed_seconds = elapsed_seconds;
			entire_data.user_data = user_data.buffer;
			entire_data.write_instrument = write_instrument;
			bool success = delta_function(&entire_data);
			if (success) {
				size_t current_instrument_offset = write_instrument->GetOffset();
				if (current_instrument_offset > instrument_offset) {
					state_infos.Add({ elapsed_seconds, current_instrument_offset - instrument_offset });
				}
			}
			else {
				is_failed = true;
			}
			return success;
		}
	}

	bool DeltaStateWriter::Write() {
		ECS_ASSERT(extract_function != nullptr, "DeltaStateWriter: trying to make a self-contained call when no extract function has been specified.");
		float elapsed_seconds = extract_function(user_data.buffer);
		return Write(elapsed_seconds);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeltaStateReader::Advance(float elapsed_seconds, CapacityStream<char>* error_message) {
		// If it is finished, return true directly
		if (IsFinished()) {
			return true;
		}

		// Determine the entire state that corresponds to that moment
		size_t next_state_index = GetStateIndexFromCurrentIndex(elapsed_seconds);
		if (next_state_index != current_state_index - 1) {
			// We switched to a new state - verify how many states we need to deserialize
			size_t deserialize_state_count = next_state_index - current_state_index + 1;
			if (deserialize_state_count == 1) {
				// If the next state index surpasses the state info count, then don't advance
				if (next_state_index == state_infos.size) {
					// Don't report this as a failure
					current_state_index = state_infos.size;
					return true;
				}

				// Just one state to be applied, no seeking is needed
				return AdvanceOneState(error_message);
			}
			else {
				// Clamp the next state to the last state
				next_state_index = ClampMax<size_t>(next_state_index, state_infos.size);

				// There are more states to be deserialized. Determine the last entire state
				// And go from there.
				size_t last_entire_state = -1;
				size_t last_state_index = next_state_index;
				for (unsigned int index = 0; index < entire_state_indices.size; index++) {
					if (entire_state_indices[index] >= current_state_index && entire_state_indices[index] < last_state_index) {
						last_entire_state = entire_state_indices[index];
					}
					else if (entire_state_indices[index] >= last_state_index) {
						break;
					}
				}

				// Seek to that location - unless it is the current state
				if (last_entire_state != -1 && last_entire_state != current_state_index) {
					if (!SeekInstrumentAtState(last_entire_state, error_message)) {
						is_failed = true;
						return false;
					}

					// Read the entire state
					if (!CallEntireState(last_entire_state, error_message)) {
						return false;
					}

					// Modify the current state index to start at the next state
					current_state_index = last_entire_state + 1;
				}
				
				// Only deltas remaining to be applied
				for (size_t index = current_state_index; index < next_state_index; index++) {
					if (!CallDeltaState(index, error_message)) {
						return false;
					}
				}
				current_state_index = next_state_index;
			}
		}

		return true;
	}

	bool DeltaStateReader::AdvanceOneState(CapacityStream<char>* error_message) {
		if (current_state_index == state_infos.size) {
			// If it surpasses the last state, then don't continue, but don't report as a failure
			return true;
		}

		bool is_entire_state = IsEntireState(current_state_index);

		if (is_entire_state) {
			if (!CallEntireState(current_state_index, error_message)) {
				return false;
			}
		}
		else {
			if (!CallDeltaState(current_state_index, error_message)) {
				return false;
			}
		}

		current_state_index++;
		return true;
	}

	bool DeltaStateReader::CallEntireState(size_t overall_state_index, CapacityStream<char>* error_message) {
		size_t before_read_offset = read_instrument->GetOffset();

		DeltaStateReaderEntireFunctionData entire_data;
		entire_data.elapsed_seconds = state_infos[overall_state_index].elapsed_seconds;
		entire_data.write_size = state_infos[overall_state_index].write_size;
		entire_data.header = header;
		entire_data.read_instrument = read_instrument;
		entire_data.user_data = user_data.buffer;
		if (!entire_function(&entire_data)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to read entire state with index {#}, elapsed seconds {#} with write size of {#}.", overall_state_index,
				entire_data.elapsed_seconds, entire_data.write_size);
			is_failed = true;
			return false;
		}

		size_t after_read_offset = read_instrument->GetOffset();
		// Ensure that it advanced the right amount of bytes
		size_t read_difference = after_read_offset - before_read_offset;
		if (read_difference != state_infos[overall_state_index].write_size) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "The entire state with index {#}, elapsed seconds {#} was read, but the read size is mismatched: functor read {#} - expected {#}.", overall_state_index,
				entire_data.elapsed_seconds, read_difference, state_infos[overall_state_index].write_size);
			is_failed = true;
			return false;
		}
		return true;
	}

	bool DeltaStateReader::CallDeltaState(size_t overall_state_index, CapacityStream<char>* error_message) {
		size_t before_read_offset = read_instrument->GetOffset();

		DeltaStateReaderDeltaFunctionData delta_data;
		delta_data.elapsed_seconds = state_infos[overall_state_index].elapsed_seconds;
		delta_data.write_size = state_infos[overall_state_index].write_size;
		delta_data.header = header;
		delta_data.read_instrument = read_instrument;
		delta_data.user_data = user_data.buffer;
		if (!delta_function(&delta_data)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to read delta state with index {#}, elapsed seconds {#} with write size of {#}.", overall_state_index,
				delta_data.elapsed_seconds, delta_data.write_size);
			is_failed = true;
			return false;
		}

		size_t after_read_offset = read_instrument->GetOffset();
		size_t read_difference = after_read_offset - before_read_offset;
		if (read_difference != state_infos[overall_state_index].write_size) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "The delta state with index {#}, elapsed seconds {#} was read, but the read size is mismatched: functor read {#} - expected {#}.", overall_state_index,
				delta_data.elapsed_seconds, read_difference, state_infos[overall_state_index].write_size);
			is_failed = true;
			return false;
		}
		return true;
	}

	void DeltaStateReader::Deallocate() {
		if (user_deallocate_function != nullptr) {
			user_deallocate_function(user_data.buffer, allocator);
		}
		if (user_data.size > 0) {
			ECSEngine::Deallocate(allocator, user_data.buffer);
		}
		header.Deallocate(allocator);
		state_infos.Deallocate(allocator);
		entire_state_indices.Deallocate(allocator);
		read_instrument = nullptr;
	}

	size_t DeltaStateReader::GetEntireStateIndexForTime(float elapsed_seconds) const {
		if (entire_state_indices.size == 0 || elapsed_seconds < 0.0f) {
			return -1;
		}

		for (unsigned int index = 0; index < entire_state_indices.size; index++) {
			if (state_infos[entire_state_indices[index]].elapsed_seconds >= elapsed_seconds) {
				// If the current index is 0, it means that we don't have an entire state before, fail
				if (index == 0) {
					return -1;
				}
				return entire_state_indices[index];
			}
		}

		// Return the last entry
		return entire_state_indices[entire_state_indices.size - 1];
	}

	size_t DeltaStateReader::GetStateIndexFromCurrentIndex(float elapsed_seconds) const {
		for (size_t index = current_state_index; index < state_infos.size; index++) {
			if (state_infos[index].elapsed_seconds > elapsed_seconds) {
				return index == 0 ? 0 : index - 1;
			}
		}

		// It is over the serialized state
		return state_infos.size;
	}

	size_t DeltaStateReader::GetOffsetForState(size_t index) const {
		size_t offset = 0;
		for (size_t subindex = 0; subindex < index; subindex++) {
			offset += state_infos[index].write_size;
		}
		return offset + state_instrument_start_offset;
	}

	bool DeltaStateReader::IsEntireState(size_t index) const {
		for (unsigned int subindex = 0; subindex < entire_state_indices.size; subindex++) {
			if ((size_t)entire_state_indices[subindex] == index) {
				return true;
			}
			else if ((size_t)entire_state_indices[subindex] > index) {
				return false;
			}
		}

		return false;
	}

	bool DeltaStateReader::Initialize(const DeltaStateReaderInitializeInfo& initialize_info) {
		read_instrument = initialize_info.read_instrument;
		allocator = initialize_info.allocator;

		// Try to read the header data, if the user wants to read it

		// Read the user defined static header
		if (!DeserializeIntVariableLengthBool(read_instrument, header.size)) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize the user static header size.");
			is_failed = true;
			return false;
		}

		if (header.size > 0) {
			header.buffer = Allocate(allocator, header.size);
			if (!read_instrument->Read(header.buffer, header.size)) {
				ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize the user static header data.");
				header.Deallocate(allocator);
				is_failed = true;
				return false;
			}
		}
		
		size_t variable_length_header_size = 0;
		if (!read_instrument->Read(&variable_length_header_size)) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize the user variable length header data.");
			header.Deallocate(allocator);
			return false;
		}

		// The user defined variable length header
		if (initialize_info.functor_info.header_read_function != nullptr) {
			// Create a subinstrument for the header read function
			ReadInstrument::SubinstrumentData subinstrument_storage;
			auto subinstrument = read_instrument->PushSubinstrument(&subinstrument_storage, variable_length_header_size);

			DeltaStateReaderHeaderReadFunctionData header_read_data;
			header_read_data.header = header;
			header_read_data.read_instrument = read_instrument;
			header_read_data.user_data = user_data.buffer;
			header_read_data.write_size = variable_length_header_size;
			if (!initialize_info.functor_info.header_read_function(&header_read_data)) {
				ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not read variable length header or the header itself is invalid.");
				header.Deallocate(allocator);
				is_failed = true;
				return false;
			}
		}
		state_instrument_start_offset = read_instrument->GetOffset();

		// Try to deserialize the footer. If we cannot deserialize it, then we must abort
		if (!read_instrument->Seek(ECS_INSTRUMENT_SEEK_END, -(int64_t)sizeof(Footer))) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not seek at the footer location.");
			header.Deallocate(allocator);
			is_failed = true;
			return false;
		}

		Footer footer;
		if (!read_instrument->Read(&footer)) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not read the written footer or the data is corrupted.");
			header.Deallocate(allocator);
			is_failed = true;
			return false;
		}

		if (footer.version != VERSION) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Written version {#} is not accepted.", footer.version);
			header.Deallocate(allocator);
			is_failed = true;
			return false;
		}

		// Seek to the start of the footer and start deserializing from there
		if (!read_instrument->Seek(ECS_INSTRUMENT_SEEK_CURRENT, -((int64_t)footer.size + (int64_t)sizeof(footer)))) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not seek to the beginning of the footer data.");
			header.Deallocate(allocator);
			is_failed = true;
			return false;
		}

		if (!DeserializeIntVariableLengthBool(read_instrument, state_infos.size)) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize the state infos size or the data is corrupted.");
			header.Deallocate(allocator);
			is_failed = true;
			return false;
		}

		state_infos.Initialize(allocator, state_infos.size, state_infos.size);
		for (unsigned int index = 0; index < state_infos.size; index++) {
			if (!DeserializeIntVariableLengthBool(read_instrument, state_infos[index].write_size)) {
				ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize state info {#} write size or the data is corrupted.", index);
				header.Deallocate(allocator);
				state_infos.Deallocate(allocator);
				is_failed = true;
				return false;
			}
			if (!read_instrument->Read(&state_infos[index].elapsed_seconds)) {
				ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize state info {#} elapsed seconds.", index);
				header.Deallocate(allocator);
				state_infos.Deallocate(allocator);
				is_failed = true;
				return false;
			}
		}

		if (!DeserializeIntVariableLengthBool(read_instrument, entire_state_indices.size)) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize the entire state indices size or the data is corrupted.");
			header.Deallocate(allocator);
			state_infos.Deallocate(allocator);
			is_failed = true;
			return false;
		}
		entire_state_indices.Initialize(allocator, entire_state_indices.size, entire_state_indices.size);
		for (unsigned int index = 0; index < entire_state_indices.size; index++) {
			if (!DeserializeIntVariableLengthBool(read_instrument, entire_state_indices[index])) {
				ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize entire state index {#} or the data is corrupted.", index);
				header.Deallocate(allocator);
				state_infos.Deallocate(allocator);
				entire_state_indices.Deallocate(allocator);
				is_failed = true;
				return false;
			}
		}

		// Read the frame elapsed seconds - if they are written
		if (!DeserializeIntVariableLengthBool(read_instrument, frame_elapsed_seconds.size)) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize the frame elapsed seconds count or the data is corrupted.");
			header.Deallocate(allocator);
			state_infos.Deallocate(allocator);
			entire_state_indices.Deallocate(allocator);
			is_failed = true;
			return false;
		}
		frame_elapsed_seconds.Initialize(allocator, frame_elapsed_seconds.size, frame_elapsed_seconds.size);
		if (!read_instrument->Read(frame_elapsed_seconds.buffer, frame_elapsed_seconds.size * frame_elapsed_seconds.MemoryOf(1))) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not deserialize the frame elapsed seconds values or the data is corrupted.");
			header.Deallocate(allocator);
			state_infos.Deallocate(allocator);
			entire_state_indices.Deallocate(allocator);
			frame_elapsed_seconds.Deallocate(allocator);
			is_failed = true;
			return false;
		}

		// Setup the user data, such that we can call the variable length header read function on it
		if (initialize_info.functor_info.user_data.size > 0) {
			user_data.Initialize(allocator, initialize_info.functor_info.user_data.size);
			if (initialize_info.functor_info.user_data.buffer != nullptr) {
				user_data.CopyOther(initialize_info.functor_info.user_data.buffer, initialize_info.functor_info.user_data.size);
			}
			else {
				memset(user_data.buffer, 0, initialize_info.functor_info.user_data.size);
			}
		}
		else {
			user_data = initialize_info.functor_info.user_data;
		}

		if (initialize_info.functor_info.user_data_allocator_initialize != nullptr) {
			ECS_ASSERT(initialize_info.functor_info.user_data_allocator_deallocate != nullptr);
			initialize_info.functor_info.user_data_allocator_initialize(user_data.buffer, allocator);
		}
		user_deallocate_function = initialize_info.functor_info.user_data_allocator_deallocate;

		// Seek to the beginning of the instrument, such that it starts on the first state.
		if (!read_instrument->Seek(ECS_INSTRUMENT_SEEK_START, state_instrument_start_offset)) {
			ECS_FORMAT_ERROR_MESSAGE(initialize_info.error_message, "Could not seek to the start of the read instrument after reading the initialize data.");
			header.Deallocate(allocator);
			state_infos.Deallocate(allocator);
			entire_state_indices.Deallocate(allocator);
			frame_elapsed_seconds.Deallocate(allocator);
			is_failed = true;
			return false;
		}

		is_failed = false;
		current_state_index = 0;
		delta_function = initialize_info.functor_info.delta_function;
		entire_function = initialize_info.functor_info.entire_function;
		return true;
	}

	bool DeltaStateReader::SeekInstrumentAtState(size_t state_index, CapacityStream<char>* error_message) const {
		size_t state_offset = GetOffsetForState(state_index);
		if (!read_instrument->Seek(ECS_INSTRUMENT_SEEK_START, state_offset)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to seek at offset {#} for state with index {#}, elapsed seconds {#}.", state_offset,
				state_index, state_infos[state_index].elapsed_seconds);
			return false;
		}
		return true;
	}

	bool DeltaStateReader::Seek(float elapsed_seconds, CapacityStream<char>* error_message) {
		size_t entire_state_index = GetEntireStateIndexForTime(elapsed_seconds);
		if (entire_state_index == -1) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Found no matching entire state for moment {#}.", elapsed_seconds);
			is_failed = true;
			return false;
		}

		// Seek to its offset
		if (!SeekInstrumentAtState(entire_state_index, error_message)) {
			is_failed = true;
			return false;
		}

		// Read the entire state
		if (!CallEntireState(entire_state_index, error_message)) {
			return false;
		}

		// Continue with any remaining deltas
		current_state_index = entire_state_index;
		for (size_t index = entire_state_index + 1; index < (size_t)state_infos.size; index++) {
			if (state_infos[index].elapsed_seconds <= elapsed_seconds) {
				// Apply this delta state
				if (!CallDeltaState(index, error_message)) {
					return false;
				}
				current_state_index++;
			}
			else {
				// Stop the loop
				break;
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeltaStateWriteGenericHeader(CapacityStream<void>& stack_memory, unsigned char version, Stream<void> extra_memory) {
		DeltaStateGenericHeader header;
		ECS_ASSERT(extra_memory.size <= ECS_COUNTOF(header.reserved), "Delta State generic header reserved size exceeded");

		header.version = version;
		extra_memory.CopyTo(header.reserved);
		// Set the remaining reserved bytes to 0
		memset(header.reserved + extra_memory.size, 0, ECS_COUNTOF(header.reserved) - extra_memory.size);
		stack_memory.Add(&header);
	}
	
	// -----------------------------------------------------------------------------------------------------------------------------

}