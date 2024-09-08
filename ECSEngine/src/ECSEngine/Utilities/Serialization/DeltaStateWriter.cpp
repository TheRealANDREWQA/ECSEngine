#include "ecspch.h"
#include "DeltaStateWriter.h"
#include "../PointerUtilities.h"
#include "SerializationHelpers.h"

#define VERSION 0
#define LAST_ENTIRE_STATE_INITIALIZE -10000.0f

namespace ECSEngine {

	// This is a header that is used by the writer in order to help accommodate changes
	struct Header {
		unsigned char version;
		ECS_INT_TYPE delta_state_int_type;
		// Not used at the moment, should be all 0s
		unsigned char reserved[6];
	};

	ECS_INLINE static size_t DeltaStateInfoByteSize(const DeltaStateWriter* writer) {
		return GetIntTypeByteSize(writer->delta_state_integer_type) * 2;
	}

	// Returns true if it succeeded, else false (the allocation could not be made)
	static bool AllocateEntireStateChunk(DeltaStateWriter* writer) {
		void* allocation = AllocateEx(writer->allocator, writer->entire_state_chunk_capacity);
		if (allocation == nullptr) {
			return false;
		}

		writer->entire_state_chunks.Add({ allocation, 0, (unsigned int)writer->entire_state_chunk_capacity });
		// Allocate a new entry in the delta state sizes as well
		unsigned int delta_info_byte_size = (unsigned int)DeltaStateInfoByteSize(writer);
		writer->delta_state_infos.Reserve(delta_info_byte_size);
		void* delta_state_info = writer->delta_state_infos.Get(writer->delta_state_infos.size, 1);
		// Set the entry to 0 for both fields
		memset(delta_state_info, 0, delta_info_byte_size);
		writer->delta_state_infos.size += delta_info_byte_size;
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

		total_size += header.size + sizeof(size_t); // Write the size of the header as well
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
		delta_state_integer_type = initialize_info->delta_state_info_integer_type;
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
						entire_state_infos.Add({ (unsigned int)written_size, elapsed_seconds });
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
							size_t entry_size = DeltaStateInfoByteSize(this);
							void* delta_info = delta_state_infos.Get(delta_state_infos.size - entry_size, 1);

							size_t delta_count = GetIntValueUnsigned(delta_info, delta_state_integer_type);
							size_t total_byte_count = GetIntValueUnsigned(OffsetPointer(delta_info, entry_size / 2), delta_state_integer_type);
							// Validate that the total written size fits into the integer type
							// We could technically verify that the delta count + 1 is also in range, but it is not necessary
							// Since each write is at least 1 byte long, so the total byte count will go out of bounds at least at the same time
							ECS_ASSERT(IsUnsignedIntInRange(delta_state_integer_type, total_byte_count + written_size), "Delta state writer: delta state chunk serialization exceeded the limit of the integer range.");

							SetIntValueUnsigned(delta_info, delta_state_integer_type, delta_count + 1);
							SetIntValueUnsigned(OffsetPointer(delta_info, entry_size / 2), delta_state_integer_type, total_byte_count + written_size);

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

	bool DeltaStateWriter::WriteTo(uintptr_t& buffer, size_t& buffer_capacity) const {
		// Do a precheck and ensure the buffer is large enough
		size_t serialize_size = GetWriteSize();
		if (buffer_capacity < serialize_size) {
			return false;
		}

		buffer_capacity -= serialize_size;
		Header write_header;
		memset(&write_header, 0, sizeof(write_header));
		write_header.version = VERSION;
		write_header.delta_state_int_type = delta_state_integer_type;

		Write<true>(&buffer, &write_header);
		// Write the user header, if there is one. Write the header size even when it is missing,
		// Such that we can always skip this entry, even if the user doesn't correctly set it
		WriteWithSize<true>(&buffer, header);

		// Write the entire chunk infos
		WriteWithSize<true>(&buffer, entire_state_infos.ToStream());
		// Write the delta chunk infos
		WriteWithSize<true>(&buffer, delta_state_infos.ToStream());

		// Write the entire chunks now
		for (size_t index = 0; index < entire_state_chunks.size; index++) {
			Write<true>(&buffer, entire_state_chunks[index].buffer, entire_state_chunks[index].size);
		}

		// Write the delta state chunks now
		for (size_t index = 0; index < delta_state_chunks.size; index++) {
			Write<true>(&buffer, delta_state_chunks[index].buffer, delta_state_chunks[index].size);
		}

		return true;
	}

	bool DeltaStateWriter::WriteTo(ECS_FILE_HANDLE file) const {
		return true;
	}

}