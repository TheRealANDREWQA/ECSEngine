#pragma once
#include "ReaderWriterInterface.h"
#include "File.h"

namespace ECSEngine {

	ECS_INLINE ECS_FILE_SEEK InstrumentSeekToFileSeek(ECS_INSTRUMENT_SEEK_TYPE seek_type) {
		switch (seek_type) {
		case ECS_INSTRUMENT_SEEK_START:
			return ECS_FILE_SEEK_BEG;
		case ECS_INSTRUMENT_SEEK_CURRENT:
			return ECS_FILE_SEEK_CURRENT;
		case ECS_INSTRUMENT_SEEK_END:
			return ECS_FILE_SEEK_END;
		default:
			// Shouldn't be reached
			ECS_ASSERT(false, "Invalid instrument seek type");
		}

		return ECS_FILE_SEEK_BEG;
	}

	struct BufferedFileWriteInstrument : WriteInstrument {
		ECS_WRITE_INSTRUMENT_HELPER;

		ECS_INLINE BufferedFileWriteInstrument() : buffering(), file(-1), initial_file_offset(0) {}
		// Force the user to specify the initial file offset
		ECS_INLINE BufferedFileWriteInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset) : file(_file), buffering(_buffering), initial_file_offset(_initial_file_offset) {}

		ECS_INLINE size_t GetOffset() const override {
			// This is a little bit more complicated, since we need to take the buffering into account
			size_t file_offset = GetFileCursor(file);
			ECS_ASSERT(file_offset != -1, "Retrieving buffered file write offset failed.");
			return file_offset - initial_file_offset + buffering.size;
		}

		ECS_INLINE bool Write(const void* data, size_t data_size) override {
			return WriteFile(file, { data, data_size }, buffering);
		}

		bool ResetAndSeekTo(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			// Write any buffering that is still left
			if (!WriteFile(file, buffering)) {
				return false;
			}
			buffering.size = 0;

			if (!SetFileCursorBool(file, offset, InstrumentSeekToFileSeek(seek_type))) {
				return false;
			}

			// Resize the file such that it ends at the current cursor
			if (!ResizeFile(file, GetFileCursor(file))) {
				return false;
			}

			return true;
		}

		ECS_INLINE bool Flush() override {
			if (buffering.size == 0) {
				return true;
			}
			return WriteFile(file, buffering);
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			// Always false
			return false;
		}

		ECS_FILE_HANDLE file;
		size_t initial_file_offset;
		CapacityStream<void> buffering;
	};

	struct BufferedFileReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE BufferedFileReadInstrument() : buffering(), file(-1), initial_file_offset(0) {}
		// The last argument is optional. If it is left at -1, it will deduce the data size as all the data from the initial offset until the end of the file
		ECS_INLINE BufferedFileReadInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset, size_t _data_byte_size = -1) : file(_file), buffering(_buffering), 
			initial_file_offset(_initial_file_offset) {
			// Retrieve the total data size now
			if (_data_byte_size == -1) {
				size_t file_byte_size = GetFileByteSize(file);
				ECS_ASSERT(file_byte_size >= _initial_file_offset, "BufferedFileReadInstrument: The file byte size is smaller than the initial file offset!");
				total_data_size = file_byte_size - _initial_file_offset;
			}
			else {
				total_data_size = _data_byte_size;
			}
		}

		ECS_INLINE size_t GetOffset() const override {
			// This is a little bit more complicated, since we need to take the buffering into account
			size_t file_offset = GetFileCursor(file);
			ECS_ASSERT(file_offset != -1, "Retrieving buffered file read offset failed.");
			return file_offset - initial_file_offset - buffering.size;
		}

		ECS_INLINE bool Read(void* data, size_t data_size) override {
			return ReadFileExact(file, { data, data_size }, buffering);
		}

		ECS_INLINE bool ReadAlways(void* data, size_t data_size) override {
			// Same as normal read
			return Read(data, data_size);
		}

		bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			// Seeking relative to the current offset needs to be handled separately
			if (seek_type == ECS_INSTRUMENT_SEEK_CURRENT) {
				// For example, if we read 4 bytes from a file and then we are seeking 4 bytes ahead,
				// That would mean to actually drop 4 bytes from the buffering, if there is enough space,
				// Or if there aren't enough bytes in the buffering, to seek with the remaining bytes ahead.
				// That is for the ahead offset, but it is similar for the seeking with negative values
				if (offset >= 0) {
					if (buffering.size >= offset) {
						// Simply discard some of the buffering
						buffering.size -= offset;
						return true;
					}
					else {
						// Discard the entire buffering, and seek ahead in the file
						offset -= buffering.size;
						buffering.size = 0;
						return SetFileCursorBool(file, offset, ECS_FILE_SEEK_CURRENT);
					}
				}
				else {
					if (buffering.size + offset <= buffering.capacity) {
						// We fit into the buffering, simply move back the buffering read end
						buffering.size += offset;
						return true;
					}
					else {
						// We need to seek for the amount of surplus offset bytes compared to the buffering,
						// Plus one entire buffering capacity, to account for this buffering that was read
						offset = offset - (int64_t)(buffering.capacity - buffering.size) - (int64_t)buffering.capacity;
						// Discard the buffering, and seek into the file
						buffering.size = 0;
						return SetFileCursorBool(file, offset, ECS_FILE_SEEK_CURRENT);
					}
				}
			}
			else {
				// Discard the buffering, since we changed the location
				buffering.size = 0;
				return SetFileCursorBool(file, offset, InstrumentSeekToFileSeek(seek_type));
			}
		}

		ECS_INLINE void* ReferenceData(size_t data_size, bool& is_out_of_range) override {
			// Cannot reference data at the moment. If we strengthen the requirements for ReferenceData,
			// Like the data is valid until the next Read or Reference, then we could technically reference
			// Data from the buffering.
			is_out_of_range = false;
			return nullptr;
		}

		ECS_INLINE size_t TotalSize() const override {
			return total_data_size;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			// Always false
			return false;
		}

		ECS_FILE_HANDLE file;
		size_t initial_file_offset;
		size_t total_data_size;
		CapacityStream<void> buffering;
	};

}