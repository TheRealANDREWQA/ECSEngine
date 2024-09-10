#pragma once
#include "ReaderWriterInterface.h"
#include "File.h"

namespace ECSEngine {

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

		ECS_INLINE bool Flush() override {
			if (buffering.size == 0) {
				return true;
			}
			return WriteFile(file, buffering);
		}

		ECS_FILE_HANDLE file;
		size_t initial_file_offset;
		CapacityStream<void> buffering;
	};

	struct BufferedFileReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE BufferedFileReadInstrument() : buffering(), file(-1), initial_file_offset(0) {}
		ECS_INLINE BufferedFileReadInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset) : file(_file), buffering(_buffering), initial_file_offset(_initial_file_offset) {}

		ECS_INLINE size_t GetOffset() const override {
			// This is a little bit more complicated, since we need to take the buffering into account
			size_t file_offset = GetFileCursor(file);
			ECS_ASSERT(file_offset != -1, "Retrieving buffered file read offset failed.");
			return file_offset - initial_file_offset - buffering.size;
		}

		ECS_INLINE bool Read(void* data, size_t data_size) override {
			return ReadFileExact(file, { data, data_size }, buffering);
		}

		ECS_INLINE bool Seek(size_t offset) override {
			// Discard the buffering, since we changed the location
			buffering.size = 0;
			return SetFileCursorBool(file, offset, ECS_FILE_SEEK_BEG);
		}

		ECS_INLINE void* ReferenceData(size_t data_size, bool& is_out_of_range) override {
			// Cannot reference data at the moment. If we strengthen the requirements for ReferenceData,
			// Like the data is valid until the next Read or Reference, then we could technically reference
			// Data from the buffering.
			is_out_of_range = false;
			return nullptr;
		}

		ECS_FILE_HANDLE file;
		size_t initial_file_offset;
		CapacityStream<void> buffering;
	};

}