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

		ECS_INLINE BufferedFileWriteInstrument() : buffering(), file(-1), initial_file_offset(0), is_binary_file(false) {}
		// Force the user to specify the initial file offset
		ECS_INLINE BufferedFileWriteInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset, bool _is_binary_file) : file(_file), buffering(_buffering), 
			initial_file_offset(_initial_file_offset), overall_file_offset(_initial_file_offset), is_binary_file(_is_binary_file) {}

		ECS_INLINE size_t GetOffset() const override {
			if (is_binary_file) {
				return overall_file_offset;
			}
			else {
				// This is a little bit more complicated, since we need to take the buffering into account
				size_t file_offset = GetFileCursor(file);
				ECS_ASSERT(file_offset != -1, "Retrieving buffered file write offset failed.");
				return file_offset - initial_file_offset + buffering.size;
			}
		}

		ECS_INLINE bool Write(const void* data, size_t data_size) override {
			bool success = WriteFile(file, { data, data_size }, buffering);
			overall_file_offset += success && is_binary_file ? data_size : 0;
			return success;
		}

		bool ResetAndSeekTo(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			// Write any buffering that is still left
			if (!WriteFile(file, buffering)) {
				return false;
			}
			buffering.size = 0;

			// For the start seek, we must add the initial offset
			int64_t set_cursor_offset = seek_type == ECS_INSTRUMENT_SEEK_START ? offset + (int64_t)initial_file_offset : offset;
			if (!SetFileCursorBool(file, set_cursor_offset, InstrumentSeekToFileSeek(seek_type))) {
				return false;
			}

			// Resize the file such that it ends at the current cursor
			size_t file_cursor_offset = GetFileCursor(file);
			if (!ResizeFile(file, file_cursor_offset)) {
				return false;
			}

			// Update the file offset, if we have to
			if (is_binary_file) {
				switch (seek_type) {
				case ECS_INSTRUMENT_SEEK_START:
				{
					overall_file_offset = offset;
				}
				break;
				case ECS_INSTRUMENT_SEEK_CURRENT:
				{
					overall_file_offset += offset;
				}
				break;
				case ECS_INSTRUMENT_SEEK_END:
				{
					// For the end case, we need to set the offset to the one taken from the OS
					overall_file_offset = file_cursor_offset - initial_file_offset;
				}
				break;
				default:
					ECS_ASSERT(false);
				}
			}

			return true;
		}

		ECS_INLINE bool Flush() override {
			if (buffering.size == 0) {
				return true;
			}
			bool success = WriteFile(file, buffering);
			overall_file_offset += success && is_binary_file ? buffering.size : 0;
			return success;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			// Always false
			return false;
		}

		ECS_FILE_HANDLE file;
		bool is_binary_file;
		// Used only for both text and binary files
		size_t initial_file_offset;
		// This offset is used to be returned to the GetOffset() call. It avoids having
		// To call into the syscall, which may or may not be expensive, but better to err on
		// The side of caution, since it is not difficult to implement this cached value.
		// Used only for binary files, since text files are handled differently on Windows
		size_t overall_file_offset;
		CapacityStream<void> buffering;
	};

	// This write instrument handles the automatic file creation, with the buffering being optional, and the proper release of it, when used on the stack.
	// It implements a destructor such that it gets released automatically on the stack.
	// Should be used like this - all the time. Always check for failure to open the file
	// 
	// OwningBufferedFileWriteInstrument instrument(...);
	// if (instrument.IsInitializationFailed()) {
	//   // Handle error
	// }
	struct OwningBufferedFileWriteInstrument : BufferedFileWriteInstrument {
		OwningBufferedFileWriteInstrument() = default;

		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileWriteInstrument(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr) : is_buffering_allocated(false) {
			ECS_FILE_HANDLE file_handle = -1;
			ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY;
			access_flags |= binary_file ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
			// Use file create in order to create the file if it doesn't already exist
			ECS_FILE_STATUS_FLAGS status = FileCreate(file_path, &file_handle, access_flags, ECS_FILE_CREATE_READ_WRITE, error_message);

			file = status == ECS_FILE_STATUS_OK ? file_handle : -1;
			initial_file_offset = 0;
			buffering.InitializeFromBuffer(_buffering.buffer, 0, _buffering.size);
		}

		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileWriteInstrument(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr)
		 : OwningBufferedFileWriteInstrument(file_path, Stream<void>(AllocateEx(_buffering_allocator, _buffering_size), _buffering_size), binary_file, error_message) {
			if (!IsInitializationFailed()) {
				buffering_allocator = buffering_allocator;
				is_buffering_allocated = true;
			}
			else {
				// Release the allocated buffering now
				buffering.Deallocate(_buffering_allocator);
			}
		}

		ECS_INLINE ~OwningBufferedFileWriteInstrument() {
			Release();
		}

		// Returns true if it couldn't open the file, else false
		ECS_INLINE bool IsInitializationFailed() const {
			return file == -1;
		}

		// Releases the resources that it is using
		ECS_INLINE void Release() {
			if (!IsInitializationFailed()) {
				if (is_buffering_allocated) {
					buffering.Deallocate(buffering_allocator);
				}
				CloseFile(file);
			}
		}

		AllocatorPolymorphic buffering_allocator;
		bool is_buffering_allocated;
	};

	struct BufferedFileReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE BufferedFileReadInstrument() : buffering(), file(-1), initial_file_offset(0), is_binary_file(false) {}
		// The last argument is optional. If it is left at -1, it will deduce the data size as all the data from the initial offset until the end of the file
		ECS_INLINE BufferedFileReadInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset, bool _is_binary_file, size_t _data_byte_size = -1) 
			: file(_file), buffering(_buffering), initial_file_offset(_initial_file_offset), overall_file_offset(_initial_file_offset), is_binary_file(_is_binary_file) {
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
			if (is_binary_file) {
				return overall_file_offset;
			}
			else {
				// This is a little bit more complicated, since we need to take the buffering into account
				size_t file_offset = GetFileCursor(file);
				ECS_ASSERT(file_offset != -1, "Retrieving buffered file write offset failed.");
				return file_offset - initial_file_offset + buffering.size;
			}
		}

		ECS_INLINE bool Read(void* data, size_t data_size) override {
			bool success = ReadFileExact(file, { data, data_size }, buffering);
			overall_file_offset += success && is_binary_file ? data_size : 0;
			return success;
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
						overall_file_offset += is_binary_file ? offset : 0;
						return true;
					}
					else {
						// Discard the entire buffering, and seek ahead in the file
						offset -= buffering.size;
						buffering.size = 0;
						bool success = SetFileCursorBool(file, offset, ECS_FILE_SEEK_CURRENT);
						overall_file_offset += success && is_binary_file ? offset : 0;
						return success;
					}
				}
				else {
					if (buffering.size + offset <= buffering.capacity) {
						// We fit into the buffering, simply move back the buffering read end
						buffering.size += offset;
						overall_file_offset += is_binary_file ? offset : 0;
						return true;
					}
					else {
						// We need to seek for the amount of surplus offset bytes compared to the buffering,
						// Plus one entire buffering capacity, to account for this buffering that was read
						offset = offset - (int64_t)(buffering.capacity - buffering.size) - (int64_t)buffering.capacity;
						// Discard the buffering, and seek into the file
						buffering.size = 0;
						bool success = SetFileCursorBool(file, offset, ECS_FILE_SEEK_CURRENT);
						overall_file_offset += success && is_binary_file ? offset : 0;
						return success;
					}
				}
			}
			else {
				// Discard the buffering, since we changed the location
				buffering.size = 0;
				// For the start seek, we must add the initial file offset
				int64_t set_cursor_offset = seek_type == ECS_INSTRUMENT_SEEK_START ? (int64_t)initial_file_offset + offset : offset;
				bool success = SetFileCursorBool(file, set_cursor_offset, InstrumentSeekToFileSeek(seek_type));
				if (success && is_binary_file) {
					// Update the file offset
					switch (seek_type) {
					case ECS_INSTRUMENT_SEEK_START:
					{
						overall_file_offset = offset;
					}
					break;
					case ECS_INSTRUMENT_SEEK_CURRENT:
					{
						overall_file_offset += offset;
					}
					break;
					case ECS_INSTRUMENT_SEEK_END:
					{
						// Takes into account the file offset as well
						overall_file_offset = total_data_size + offset;
					}
					break;
					default:
						ECS_ASSERT(false)
					}
				}
				return success;
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
		bool is_binary_file;
		// Used for both text files and binary files
		size_t initial_file_offset;
		// This offset is used to be returned to the GetOffset() call. It avoids having
		// To call into the syscall, which may or may not be expensive, but better to err on
		// The side of caution, since it is not difficult to implement this cached value.
		// Used only for binary files, since text files are handled differently on Windows
		size_t overall_file_offset;
		size_t total_data_size;
		CapacityStream<void> buffering;
	};

	// This read instrument handles the automatic file creation, with the buffering being optional, and the proper release of it, when used on the stack.
	// It implements a destructor such that it gets released automatically on the stack
	//
	// Should be used like this - all the time. Always check for failure to open the file
	// 
	// OwningBufferedFileReadInstrument instrument(...);
	// if (instrument.IsInitializationFailed()) {
	//   // Handle error
	// }
	struct OwningBufferedFileReadInstrument : BufferedFileReadInstrument {
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr) : is_buffering_allocated(false) {
			ECS_FILE_HANDLE file_handle = -1;
			ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_READ_ONLY;
			access_flags |= binary_file ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
			// Use file create in order to create the file if it doesn't already exist
			ECS_FILE_STATUS_FLAGS status = OpenFile(file_path, &file_handle, access_flags, error_message);

			file = status == ECS_FILE_STATUS_OK ? file_handle : -1;
			initial_file_offset = 0;
			buffering.InitializeFromBuffer(_buffering.buffer, 0, _buffering.size);
		}

		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr)
			: OwningBufferedFileReadInstrument(file_path, Stream<void>(AllocateEx(_buffering_allocator, _buffering_size), _buffering_size), binary_file, error_message) {
			if (!IsInitializationFailed()) {
				buffering_allocator = buffering_allocator;
				is_buffering_allocated = true;
			}
			else {
				// Release the allocated buffering now
				buffering.Deallocate(_buffering_allocator);
			}
		}

		ECS_INLINE ~OwningBufferedFileReadInstrument() {
			Release();
		}

		// Returns true if it couldn't open the file, else false
		ECS_INLINE bool IsInitializationFailed() const {
			return file == -1;
		}

		// Releases the resources that it is using
		ECS_INLINE void Release() {
			if (!IsInitializationFailed()) {
				if (is_buffering_allocated) {
					buffering.Deallocate(buffering_allocator);
				}
				CloseFile(file);
			}
		}

		AllocatorPolymorphic buffering_allocator;
		bool is_buffering_allocated;
	};

}