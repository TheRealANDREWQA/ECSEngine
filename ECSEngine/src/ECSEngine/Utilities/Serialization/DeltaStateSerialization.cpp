#include "ecspch.h"
#include "DeltaStateSerialization.h"
#include "../PointerUtilities.h"
#include "SerializationHelpers.h"
#include "../InMemoryReaderWriter.h"
#include "../BufferedFileReaderWriter.h"
#include "SerializeIntVariableLength.h"

#define VERSION 0
#define LAST_ENTIRE_STATE_INITIALIZE -10000.0f

namespace ECSEngine {

	// This is a header that is used by the writer in order to help accommodate changes
	struct Header {
		unsigned char version;
		// Not used at the moment, should be all 0s
		unsigned char reserved[7];
	};

	// Returns true if it succeeded, else false (the allocation could not be made)
	static bool AllocateEntireStateChunk(DeltaStateWriter* writer) {
		void* allocation = AllocateEx(writer->allocator, writer->entire_state_chunk_capacity);
		if (allocation == nullptr) {
			return false;
		}

		writer->entire_state_chunks.Add({ allocation, 0, (unsigned int)writer->entire_state_chunk_capacity });
		return true;
	}

	// Returns true if it succeeded, else false (the allocation could not be made)
	static bool AllocateDeltaStateChunk(DeltaStateWriter* writer) {
		void* allocation = AllocateEx(writer->allocator, writer->delta_state_chunk_capacity);
		if (allocation == nullptr) {
			return false;
		}

		writer->delta_state_chunks.Add({ allocation, 0, (unsigned int)writer->delta_state_chunk_capacity });
		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeltaStateWriter::Deallocate() {
		DeallocateEx(allocator, user_data);
		header.Deallocate(allocator);

		StreamDeallocateElements(entire_state_chunks, allocator);
		entire_state_chunks.FreeBuffer();
		StreamDeallocateElements(delta_state_chunks, allocator);
		delta_state_chunks.FreeBuffer();
		delta_state_infos.FreeBuffer();
		entire_state_infos.FreeBuffer();
	}

	void DeltaStateWriter::DisableHeader() {
		header.Deallocate(allocator);
	}

	size_t DeltaStateWriter::GetWriteSize() const {
		// This is our header that needs to be written before the user header, if there is any
		size_t total_size = sizeof(Header);

		total_size += header.size + sizeof(unsigned short); // Write the size of the header as well, as an unsigned short
		// This size is in bytes already
		total_size += delta_state_infos.size + sizeof(size_t); // There is an additional unsigned int for the size field
		total_size += entire_state_infos.MemoryOf(entire_state_infos.size) + sizeof(size_t); // The size of each entire state entry must be stored as well 

		for (size_t index = 0; index < entire_state_chunks.size; index++) {
			total_size += entire_state_chunks[index].size;
		}
		for (size_t index = 0; index < delta_state_chunks.size; index++) {
			total_size += delta_state_chunks[index].size;
		}

		return total_size;
	}

	void* DeltaStateWriter::Initialize(const DeltaStateWriterInitializeInfo* initialize_info) {
		allocator = initialize_info->allocator;

		user_data = AllocateEx(allocator, initialize_info->user_data_size);
		memset(user_data, 0, initialize_info->user_data_size);

		delta_function = initialize_info->delta_function;
		delta_state_chunk_capacity = initialize_info->delta_state_chunk_capacity;
		entire_function = initialize_info->entire_function;
		entire_state_chunk_capacity = initialize_info->entire_state_chunk_capacity;
		entire_state_write_seconds_tick = initialize_info->entire_state_write_seconds_tick;
		last_entire_state_write_seconds = 0.0f;
		
		size_t user_header_size = initialize_info->header.size;
		if (user_header_size > 0) {
			header.buffer = AllocateEx(allocator, user_header_size);
			memcpy(header.buffer, initialize_info->header.buffer, user_header_size);
			header.size = user_header_size;
		}

		entire_state_chunks.Initialize(allocator, 0);
		delta_state_chunks.Initialize(allocator, 0);
		delta_state_infos.Initialize(allocator, 0);
		entire_state_infos.Initialize(allocator, 0);

		return user_data;
	}

	ECS_DELTA_STATE_WRITER_STATUS DeltaStateWriter::Register(const void* current_data, float elapsed_seconds) {
		float entire_state_delta = elapsed_seconds - last_entire_state_write_seconds;

		// The function is a functor that receives as parameters (CapacityStream<void>& last_chunk)
		// And must return a ECS_DELTA_STATE_WRITER_STATUS.
		// The allocate_chunk_function is a functor that receives as arguments the this pointer
		// And must allocate a new chunk and should return true if it succeeded, else false.
		auto call_function = [this](
			ResizableStream<CapacityStream<void>>& chunks, 
			auto function, 
			auto allocate_chunk_function
		) {
			if (chunks.size == 0) {
				// Allocate an entry
				if (!allocate_chunk_function(this)) {
					return ECS_DELTA_STATE_LARGER_BUFFER_NEEDED;
				}
			}

			// Call the functor for the last chunk
			CapacityStream<void>& last_chunk = chunks[chunks.size - 1];
			if (last_chunk.size == last_chunk.capacity) {
				// Allocate a new chunk
				if (!allocate_chunk_function(this)) {
					return ECS_DELTA_STATE_LARGER_BUFFER_NEEDED;
				}
				last_chunk = chunks[chunks.size - 1];
			}


			ECS_DELTA_STATE_WRITER_STATUS status = function(last_chunk);
			if (status == ECS_DELTA_STATE_LARGER_BUFFER_NEEDED) {
				// Allocate a new chunk and call the function again
				if (!allocate_chunk_function(this)) {
					return ECS_DELTA_STATE_LARGER_BUFFER_NEEDED;
				}
				last_chunk = chunks[chunks.size - 1];
				status = function(last_chunk);
			}

			return status;
		};

		if (entire_state_delta >= entire_state_write_seconds_tick || entire_state_infos.size == 0) {
			// The delta was exceeded or it is the first serialization, an entire state must be written
			return call_function(entire_state_chunks,
				[this, current_data, elapsed_seconds](CapacityStream<void>& last_chunk) {
					DeltaStateWriterEntireFunctionData entire_data;
					entire_data.current_data = current_data;
					entire_data.elapsed_seconds = elapsed_seconds;
					entire_data.user_data = user_data;
					entire_data.write_capacity =
						entire_data.write_buffer = (uintptr_t)last_chunk.buffer + last_chunk.size;
					entire_data.write_capacity = last_chunk.capacity - last_chunk.size;
					ECS_DELTA_STATE_WRITER_STATUS status = entire_function(&entire_data);
					if (status == ECS_DELTA_STATE_OK) {
						// Register the changes to the last chunk
						size_t written_size = last_chunk.capacity - last_chunk.size - entire_data.write_capacity;
						ECS_ASSERT(written_size <= UINT_MAX, "Delta state writer: entire state serialization exceeded the limit of 4GB.");
						last_chunk.size += written_size;
						entire_state_infos.Add({ (unsigned int)written_size, 0u, elapsed_seconds, });
					}
					return status;
				},
				[](DeltaStateWriter* writer) {
					return AllocateEntireStateChunk(writer);
				}
			);		
		}
		else {
			// A delta state can be used
			return call_function(delta_state_chunks,
				[this, current_data, elapsed_seconds](CapacityStream<void>& last_chunk) {
					DeltaStateWriterDeltaFunctionData entire_data;
					entire_data.current_data = current_data;
					entire_data.elapsed_seconds = elapsed_seconds;
					entire_data.user_data = user_data;
					entire_data.write_capacity =
					entire_data.write_buffer = (uintptr_t)last_chunk.buffer + last_chunk.size;
					entire_data.write_capacity = last_chunk.capacity - last_chunk.size;
					ECS_DELTA_STATE_WRITER_STATUS status = delta_function(&entire_data);
					if (status == ECS_DELTA_STATE_OK) {
						// Register the changes to the last chunk, if there are any, since delta writers might not write anything
						size_t written_size = last_chunk.capacity - last_chunk.size - entire_data.write_capacity;

						if (written_size > 0) {
							// Modify the entry in the delta_state_count
							delta_state_infos.Add({ elapsed_seconds, (unsigned int)written_size });
							last_chunk.size += written_size;
						}
					}
					return status;
				},
				[](DeltaStateWriter* writer) {
					return AllocateDeltaStateChunk(writer);
				}
			);
		}
	}

	bool DeltaStateWriter::WriteTo(WriteInstrument* write_instrument) const {
		Header write_header;
		memset(&write_header, 0, sizeof(write_header));
		write_header.version = VERSION;

		if (!write_instrument->Write(&write_header)) {
			return false;
		}

		// Write the user header, if there is one. Write the header size even when it is missing,
		// Such that we can always skip this entry, even if the user doesn't correctly set it
		if (!write_instrument->WriteWithSize<unsigned short>(header)) {
			return false;
		}

		// Write the entire chunk infos - use dynamic integer serialization in order to reduce the size
		if (!SerializeIntVariableLengthBool(write_instrument, entire_state_infos.size)) {
			return false;
		}
		for (unsigned int index = 0; index < entire_state_infos.size; index++) {
			if (!SerializeIntVariableLengthBool(write_instrument, entire_state_infos[index].write_size)) {
				return false;
			}
			if (!SerializeIntVariableLengthBool(write_instrument, entire_state_infos[index].delta_count)) {
				return false;
			}
			if (!write_instrument->Write(&entire_state_infos[index].elapsed_seconds)) {
				return false;
			}
		}
		// Write the delta chunk infos
		if (!SerializeIntVariableLengthBool(write_instrument, delta_state_infos.size)) {
			return false;
		}
		for (unsigned int index = 0; index < delta_state_infos.size; index++) {
			if (!SerializeIntVariableLengthBool(write_instrument, delta_state_infos[index].write_size)) {
				return false;
			}
			if (!write_instrument->Write(&entire_state_infos[index].elapsed_seconds)) {
				return false;
			}
		}

		// Write the entire chunks now
		for (unsigned int index = 0; index < entire_state_chunks.size; index++) {
			if (!write_instrument->Write(entire_state_chunks[index].buffer, entire_state_chunks[index].size)) {
				return false;
			}
		}

		// Write the delta state chunks now
		for (unsigned int index = 0; index < delta_state_chunks.size; index++) {
			if (!write_instrument->Write(delta_state_chunks[index].buffer, delta_state_chunks[index].size)) {
				return false;
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeltaStateReader::Advance(float elapsed_seconds, CapacityStream<char>* error_message) {
		// Get the index of the entire state that corresponds to this moment
		size_t entire_state_index = GetEntireStateIndexForTime(elapsed_seconds);
		ECS_ASSERT(entire_state_index != -1, "DeltaStateReader: advancing to a time which does not have an entire state written!");

		if (entire_state_index != current_entire_state_index) {
			// The entire state has changed, seek to that location and read the entire state
			size_t entire_state_offset = GetOffsetForEntireState(entire_state_index);
			if (!read_instrument->Seek(entire_state_offset)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to seek at offset {#} for index {#}.", entire_state_offset, entire_state_index);
				return false;
			}

			// Read the current state
			if (!read_instrument->Read())
		}

		return false;
	}

	void DeltaStateReader::AdjustDeltaIndexForEntireIndex() {
		current_delta_state_index = 0;
		for (size_t index = 0; index < current_entire_state_index; index++) { 
			current_delta_state_index += entire_state_infos[index].delta_count;
		}
	}

	void DeltaStateReader::Deallocate() {
		header.Deallocate(allocator);
		entire_state_infos.Deallocate(allocator);
		delta_state_infos.Deallocate(allocator);
		read_instrument = nullptr;
	}

	size_t DeltaStateReader::GetEntireStateIndexForTime(float elapsed_seconds) const {
		if (entire_state_infos.size == 0 || elapsed_seconds < 0.0f) {
			return -1;
		}

		for (unsigned int index = 0; index < entire_state_infos.size; index++) {
			if (entire_state_infos[index].elapsed_seconds > elapsed_seconds) {
				// If the current index is 0, it means that we don't have an entire state before, fail
				if (index == 0) {
					return -1;
				}
				return index - 1;
			}
		}

		// Return the last entry
		return entire_state_infos.size - 1;
	}

	size_t DeltaStateReader::GetOffsetForEntireState(size_t index) const {
		size_t offset = entire_state_start_offset;
		for (size_t entire_index = 0; entire_index < index; entire_index++) {
			offset += entire_state_infos[entire_index].write_size;
		}
		return offset;
	}

	size_t DeltaStateReader::GetOffsetForDeltaState(size_t overall_delta_index) const {
		size_t offset = entire_state_start_offset + entire_state_total_write_size;
		for (size_t delta_index = 0; delta_index < overall_delta_index; delta_index++) {
			offset += delta_state_infos[delta_index].write_size;
		}
		return offset;
	}

	void* DeltaStateReader::Initialize(const DeltaStateReaderInitializeInfo& initialize_info, ReadInstrument* read_instrument_parameter) {
		read_instrument = read_instrument_parameter;

		auto error_message = [initialize_info](const char* error_message) {
			SetErrorMessage(initialize_info.error_message, error_message);
		};

		// Read the header part before continuing with the general assignment
		Header serialize_header;
		if (!read_instrument->Read(&serialize_header)) {
			error_message("Could not read the delta state header.");
			return nullptr;
		}

		if (serialize_header.version != VERSION) {
			error_message("The delta state header version is not supported.");
			return nullptr;
		}

		// Read the user header
		if (!read_instrument->ReadWithSize<unsigned short>(header, initialize_info.allocator, UINT16_MAX)) {
			error_message("The user specified header could not be read or is corrupted.");
			return nullptr;
		}

		bool deallocate_header = true;
		bool deallocate_entire_state_infos = false;
		bool deallocate_delta_state_infos = false;
		auto deallocate_stack_scope = StackScope([&]() {
			if (deallocate_header) {
				header.Deallocate(initialize_info.allocator);
			}
			if (deallocate_entire_state_infos) {
				entire_state_infos.Deallocate(initialize_info.allocator);
			}
			if (deallocate_delta_state_infos) {
				delta_state_infos.Deallocate(initialize_info.allocator);
			}
		});

		// Continue with the entire state infos
		if (!DeserializeIntVariableLengthBool(read_instrument, entire_state_infos.size)) {
			error_message("The entire state info size could not be read or is corrupted.");
			return nullptr;
		}
		deallocate_entire_state_infos = true;
		entire_state_infos.Initialize(initialize_info.allocator, entire_state_infos.size, entire_state_infos.size);
		for (unsigned int index = 0; index < entire_state_infos.size; index++) {
			if (!DeserializeIntVariableLengthBool(read_instrument, entire_state_infos[index].write_size)) {
				error_message("An entire state info write size entry could not be read or is corrupted.");
				return nullptr;
			}
			if (!DeserializeIntVariableLengthBool(read_instrument, entire_state_infos[index].delta_count)) {
				error_message("An entire state info delta count entry could not be read or is corrupted.");
				return nullptr;
			}
			if (!read_instrument->Read(&entire_state_infos[index].elapsed_seconds)) {
				error_message("An entire state elapsed seconds entry could not be read or is corrupted.");
				return nullptr;
			}
		}

		if (!DeserializeIntVariableLengthBool(read_instrument, delta_state_infos.size)) {
			error_message("The delta state info size could not be read or is corrupted.");
			return nullptr;
		}

		// Make a sanity check - the number of delta state infos should be the same as the one deduced from the entire states
		unsigned int total_delta_count = 0;
		for (unsigned int index = 0; index < entire_state_infos.size; index++) {
			total_delta_count += entire_state_infos[index].delta_count;
		}
		if (total_delta_count != delta_state_infos.size) {
			error_message("The delta state info size is mismatched with the info from the entire states, which indicates that the data is corrupted.");
			return nullptr;
		}

		deallocate_delta_state_infos = true;
		delta_state_infos.Initialize(initialize_info.allocator, delta_state_infos.size, delta_state_infos.size);
		for (unsigned int index = 0; index < delta_state_infos.size; index++) {
			if (!DeserializeIntVariableLengthBool(read_instrument, delta_state_infos[index].write_size)) {
				error_message("A delta state info write size entry could not be read or is corrupted.");
				return nullptr;
			}
			if (!read_instrument->Read(&delta_state_infos[index].elapsed_seconds)) {
				error_message("A delta state info elapsed seconds entry could not be read or is corrupted.");
				return nullptr;
			}
		}

		// Cache the first entire state offset
		entire_state_start_offset = read_instrument->GetOffset();

		// We succeeded reading the entire header part, continue with the basic assignment and deactivate the
		// Stack scope deallocation
		deallocate_header = false;
		deallocate_entire_state_infos = false;
		deallocate_delta_state_infos = false;

		current_entire_state_index = 0;
		current_delta_state_index = 0;
		delta_function = initialize_info.delta_function;
		entire_function = initialize_info.entire_function;
		allocator = initialize_info.allocator;
		user_data = AllocateEx(allocator, initialize_info.user_data_size);
		memset(user_data, 0, initialize_info.user_data_size);

		// Compute the cached total entire state size
		entire_state_total_write_size = 0;
		for (size_t index = 0; index < entire_state_infos.size; index++) {
			entire_state_total_write_size += entire_state_infos[index].write_size;
		}

		return user_data;
	}

	bool DeltaStateReader::Seek(float elapsed_seconds) {
		return false;
	}
	
	// -----------------------------------------------------------------------------------------------------------------------------

}