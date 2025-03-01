#pragma once
#include "ReaderWriterInterface.h"

namespace ECSEngine {

	// An instrument useful if you want to use the generic interface to determine how many bytes will be written by a certain operation
	struct SizeDeterminationWriteInstrument : WriteInstrument {
		ECS_WRITE_INSTRUMENT_HELPER;

		ECS_INLINE SizeDeterminationWriteInstrument() : write_size(0) {}

		ECS_INLINE size_t GetOffset() const override {
			// Simply use the write size as offset
			return write_size;
		}

		ECS_INLINE bool Write(const void* data, size_t data_size) override {
			write_size += data_size;
			return true;
		}

		bool ResetAndSeekTo(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				write_size = offset;
			}
			break;
			// Current and end are the same, since the current location is basically the end
			case ECS_INSTRUMENT_SEEK_CURRENT:
			case ECS_INSTRUMENT_SEEK_END:
			{
				write_size += offset;
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid instrument seek type.");
			}

			return true;
		}

		ECS_INLINE bool Flush() override {
			return true;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			return true;
		}

		size_t write_size;
	};

	// An instrument useful if you want to use the generic interface to determine how many bytes will be read by a certain operation
	struct SizeDeterminationReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE SizeDeterminationReadInstrument(ReadInstrument* _backing_instrument, bool _ignore_seek_modifications) : backing_instrument(_backing_instrument), ignore_seek_modifications(_ignore_seek_modifications),
			read_size(0) {}

		ECS_INLINE size_t GetOffset() const override {
			return backing_instrument->GetOffset();
		}

		ECS_INLINE bool Read(void* data, size_t data_size) override {
			// In this function, don't actually read into data
			if (!backing_instrument->Ignore(data_size)) {
				return false;
			}
			read_size += data_size;
			return true;
		}

		ECS_INLINE bool ReadAlways(void* data, size_t data_size) override {
			if (!backing_instrument->Read(data, data_size)) {
				return false;
			}
			read_size += data_size;
			return true;
		}

		bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			if (!backing_instrument->Seek(seek_type, offset)) {
				return false;
			}

			// Interpret the seek in terms of read bytes
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				if (!ignore_seek_modifications) {
					read_size = offset;
				}
			}
			break;
			case ECS_INSTRUMENT_SEEK_CURRENT:
			{
				// For the current seek, make it an exception not to check for ignore_seek_modifications,
				// Because this can be used to ignore bytes
				read_size += offset;
			}
			break;
			case ECS_INSTRUMENT_SEEK_END:
			{
				if (!ignore_seek_modifications) {
					// For end, use the total size minus the offset
					read_size = TotalSize() + offset;
				}
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid seek type for read instrument!");
			}

			return true;
		}

		void* ReferenceData(size_t data_size, bool& is_out_of_range) override {
			// The backing instrument can skip over the current data
			if (!backing_instrument->Ignore(data_size)) {
				return nullptr;
			}
			// Support this operation, simply return a non nullptr value such that the calling
			// Function doesn't interpret this as a failure
			read_size += data_size;
			return (void*)0x1;
		}

		ECS_INLINE size_t TotalSize() const override {
			return backing_instrument->TotalSize();
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			return true;
		}

		// Returns how many bytes were read from the beginning of the stream
		ECS_INLINE size_t ReadSize() const {
			return read_size;
		}

		// Use another instrument as a backing instrument, from which data is read
		ReadInstrument* backing_instrument;
		// This accumulates the total read size
		size_t read_size;
		// If this value is set to true, it will not modify the read size to reflect the seek (exception
		// is the ECS_INSTRUMENT_SEEK_CURRENT, which will always modify the value), else it will adjust 
		// the read size when seeks happen (for all types of seeks)
		bool ignore_seek_modifications;
	};

}