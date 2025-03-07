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
		ECS_INLINE BufferedFileWriteInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset) : file(_file), buffering(_buffering), 
			initial_file_offset(_initial_file_offset), overall_file_offset(_initial_file_offset) {}

		ECS_INLINE size_t GetOffset() const override {
			return overall_file_offset;
		}

		ECS_INLINE bool Write(const void* data, size_t data_size) override {
			bool success = WriteFile(file, { data, data_size }, buffering);
			overall_file_offset += success ? data_size : 0;
			return success;
		}

		bool ResetAndSeekTo(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			// If the instrument seek is end, fail if the offset is positive
			if (seek_type == ECS_INSTRUMENT_SEEK_END && offset > 0) {
				return false;
			}
			
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
			// If we got to an offset that is smaller than the initial offset, fail, since that shouldn't happen
			if (file_cursor_offset < initial_file_offset) {
				return false;
			}

			if (!ResizeFile(file, file_cursor_offset)) {
				return false;
			}

			// Update the file offset cache, don't forget about the initial file offset
			overall_file_offset = file_cursor_offset - initial_file_offset;
			return true;
		}

		ECS_INLINE bool Flush() override {
			if (buffering.size == 0) {
				return true;
			}
			bool success = WriteFile(file, buffering);
			overall_file_offset += success ? buffering.size : 0;
			return success;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			// Always false
			return false;
		}

		ECS_FILE_HANDLE file;
		size_t initial_file_offset;
		// This offset is used to be returned to the GetOffset() call. It avoids having
		// To call into the syscall, which may or may not be expensive, but better to err on
		// The side of caution, since it is not difficult to implement this cached value.
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
	struct ECSENGINE_API OwningBufferedFileWriteInstrument : BufferedFileWriteInstrument {
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileWriteInstrument(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileWriteInstrument(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

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

	// This write instrument handles the automatic temporary renaming of an existing file and then creates a new file to write into, 
	// With the buffering being optional, and the proper release of the buffering and renaming to the original file if the write failed, when used on the stack.
	// It implements a destructor such that it gets released automatically on the stack.
	// Should be used like this - all the time. Always check for failure to open the file
	// 
	// TemporaryRenameBufferedFileWriteInstrument instrument(...);
	// if (instrument.IsInitializationFailed()) {
	//   // Handle error
	// }
	//
	// bool success = Operation();
	// instrument.SetSuccess(success);
	//
	//
	// Optionally - if you want to receive error messages for the final restore of the instrument,
	// You can call the function Finish() with a parameter before the instrument goes out of scope.
	struct ECSENGINE_API TemporaryRenameBufferedFileWriteInstrument : BufferedFileWriteInstrument {
		// The file path and the rename extension must be stable until this instance is destroyed. The provided file path
		// Must be absolute, not relative
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		TemporaryRenameBufferedFileWriteInstrument(Stream<wchar_t> absolute_file_path, Stream<wchar_t> _rename_extension, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

		// The file path and the rename extension must be stable until this instance is destroyed. The provided file path
		// Must be absolute, not relative
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		TemporaryRenameBufferedFileWriteInstrument(Stream<wchar_t> absolute_file_path, Stream<wchar_t> _rename_extension, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size,
			bool binary_file = true, CapacityStream<char>* error_message = nullptr);

		ECS_INLINE ~TemporaryRenameBufferedFileWriteInstrument() {
			// We cannot transmit
			Finish();
		}

		// Returns true if it couldn't open the file, else false
		ECS_INLINE bool IsInitializationFailed() const {
			return is_initialization_failed;
		}

		// Releases the resources that it is using. Returns true if any error was encountered, else false
		bool Finish(CapacityStream<char>* error_message = nullptr);

		// Fills in the storage with the full renamed file and returns it as a stream
		ECS_INLINE Stream<wchar_t> GetRenamedFile(CapacityStream<wchar_t>& storage) const {
			storage.CopyOther(original_file);
			storage.AddStreamAssert(rename_extension);
			return storage;
		}

		// Sets the status of the write instrument success, which will be used to know if the temporary restore is needed
		ECS_INLINE void SetSuccess(bool success) {
			is_failed = !success;
		}

		Stream<wchar_t> original_file;
		Stream<wchar_t> rename_extension;
		AllocatorPolymorphic buffering_allocator;
		bool is_buffering_allocated;
		bool is_initialization_failed;
		bool is_failed;
	};

	struct ECSENGINE_API BufferedFileReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE BufferedFileReadInstrument() : ReadInstrument(0), buffering(), file(-1), initial_file_offset(0) {}
		// The last argument is optional. If it is left at -1, it will deduce the data size as all the data from the initial offset until the end of the file
		ECS_INLINE BufferedFileReadInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset, size_t _data_byte_size = -1) 
			// Call the read instrument constructor with size 0, and then initialize the proper size inside the body
			: ReadInstrument(0), file(_file), buffering(_buffering), initial_file_offset(_initial_file_offset), overall_file_offset(_initial_file_offset) {
			// Retrieve the total data size now
			if (_data_byte_size == -1) {
				size_t file_byte_size = GetFileByteSize(file);
				ECS_ASSERT(file_byte_size >= _initial_file_offset, "BufferedFileReadInstrument: The file byte size is smaller than the initial file offset!");
				total_size = file_byte_size - _initial_file_offset;
			}
			else {
				total_size = _data_byte_size;
			}
		}

	protected:		
		ECS_INLINE size_t GetOffsetImpl() const override {
			return overall_file_offset;
		}

		ECS_INLINE bool ReadImpl(void* data, size_t data_size) override {
			bool success = ReadFileExact(file, { data, data_size }, buffering);
			overall_file_offset += success ? data_size : 0;
			return success;
		}

		ECS_INLINE bool ReadAlwaysImpl(void* data, size_t data_size) override {
			// Same as normal read
			return ReadImpl(data, data_size);
		}

		bool SeekImpl(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override;

		ECS_INLINE void* ReferenceDataImpl(size_t data_size) override {
			// Cannot reference data at the moment. If we strengthen the requirements for ReferenceData,
			// Like the data is valid until the next Read or Reference, then we could technically reference
			// Data from the buffering.
			return nullptr;
		}

	public:

		ECS_INLINE bool IsSizeDetermination() const override {
			// Always false
			return false;
		}

		ECS_FILE_HANDLE file;
		// Used for both text files and binary files
		size_t initial_file_offset;
		// This offset is used to be returned to the GetOffset() call. It avoids having
		// To call into the syscall, which may or may not be expensive, but better to err on
		// The side of caution, since it is not difficult to implement this cached value.
		// Used only for binary files, since text files are handled differently on Windows
		size_t overall_file_offset;
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
	struct ECSENGINE_API OwningBufferedFileReadInstrument : BufferedFileReadInstrument {
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

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