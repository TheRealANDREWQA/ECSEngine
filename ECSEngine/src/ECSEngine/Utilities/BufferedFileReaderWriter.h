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

		ECS_INLINE BufferedFileWriteInstrument() : buffering(), file(-1), initial_file_offset(0), buffering_entire_write_size(0), maximum_file_size(0) {}
		// Force the user to specify the initial file offset
		ECS_INLINE BufferedFileWriteInstrument(ECS_FILE_HANDLE _file, CapacityStream<void> _buffering, size_t _initial_file_offset) : file(_file), buffering(_buffering), 
			initial_file_offset(_initial_file_offset), overall_file_offset(_initial_file_offset), buffering_entire_write_size(_buffering.size), maximum_file_size(_initial_file_offset) {}

		ECS_INLINE size_t GetOffset() const override {
			return overall_file_offset;
		}

		ECS_INLINE bool Write(const void* data, size_t data_size) override {
			// For this write, we don't have to update buffering.size to buffer_entire_write_size
			// Since we want to override that data that exists after buffering.size
			bool success = WriteFile(file, { data, data_size }, buffering);
			if (success) {
				overall_file_offset += data_size;
				maximum_file_size = max(maximum_file_size, overall_file_offset);
			}
			return success;
		}

		bool AppendUninitialized(size_t data_size) override {
			// If the data size fits in the buffering, simply add to the buffering, else
			// Commit the current buffering and resize the file.
			if (buffering.size + data_size <= buffering.capacity) {
				buffering.size += data_size;
				overall_file_offset += data_size;
				maximum_file_size = max(maximum_file_size, overall_file_offset);
				return true;
			}

			bool success = WriteFile(file, buffering);
			if (success) {
				// We must not increment the overall file offset, it already
				// Reflect the data from the buffering
				buffering.size = 0;
				success = ResizeFile(file, overall_file_offset + data_size);
				if (success) {
					overall_file_offset += data_size;
					maximum_file_size = max(maximum_file_size, overall_file_offset);
				}
			}

			return success;
		}

		bool DiscardData() override {
			// If there is buffering data left, write it, then perform the resize
			if (buffering_entire_write_size > buffering.size) {
				buffering.size = buffering_entire_write_size;
			}
			if (buffering.size > 0 && !WriteFile(file, buffering)) {
				return false;
			}
			buffering.size = 0;
			buffering_entire_write_size = 0;

			// Simply resize the file to the current offset
			size_t offset = GetOffset();
			bool success = ResizeFile(file, offset);
			maximum_file_size = success ? offset : maximum_file_size;
			return success;
		}

		bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			// If the instrument seek is end, fail if the offset is positive
			if (seek_type == ECS_INSTRUMENT_SEEK_END && offset > 0) {
				return false;
			}
			if (seek_type == ECS_INSTRUMENT_SEEK_START && offset < 0) {
				return false;
			}

			// Determine if the current seek fits inside the buffering - if it does, we can avoid having
			// To write the current buffering and then do a file seek
			int64_t relative_offset_seek_difference = 0;
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				relative_offset_seek_difference = GetOffset() - offset;
			}
			break;
			case ECS_INSTRUMENT_SEEK_CURRENT:
			{
				relative_offset_seek_difference = offset;
			}
			break;
			case ECS_INSTRUMENT_SEEK_END:
			{
				// For seeking at the end, we must use maximum_file_size
				relative_offset_seek_difference = (int64_t)(maximum_file_size - GetOffset()) + offset;
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid WriteInstrument seek type!");
			}

			// Determine if the offset falls within the buffering capacity
			if (relative_offset_seek_difference >= 0) {
				if (relative_offset_seek_difference <= buffering.size) {
					// We can simply reduce the buffering size, but don't forget
					// To record the maximum buffering capacity
					buffering_entire_write_size = max(buffering_entire_write_size, buffering.size);
					buffering.size -= relative_offset_seek_difference;
					return true;
				}
			}
			else {
				// Check to see if we can seek forwards inside the buffering
				if (-relative_offset_seek_difference <= (int64_t)(buffering_entire_write_size - buffering.size)) {
					// We can move forward
					buffering.size += (unsigned int)-relative_offset_seek_difference;
					return true;
				}
			}
			
			// Write any buffering that is still left - we must update the buffering size to that
			// Of the entire write size, such that we don't miss potential data was written - only if
			// The entire write size is larger than the current buffering size
			if (buffering_entire_write_size > buffering.size) {
				buffering.size = buffering_entire_write_size;
			}
			if (!WriteFile(file, buffering)) {
				return false;
			}
			buffering.size = 0;
			buffering_entire_write_size = 0;

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

			// Update the file offset cache, don't forget about the initial file offset
			overall_file_offset = file_cursor_offset - initial_file_offset;
			return true;
		}

		ECS_INLINE bool Flush() override {
			if (buffering.size == 0) {
				return true;
			}
			// Update the buffering size to be that of the entire write size, such that we don't miss
			// Any data that exists afterwards. Do this only if that value is larger than the current buffering size
			if (buffering_entire_write_size > buffering.size) {
				buffering.size = buffering_entire_write_size;
			}
			bool success = WriteFile(file, buffering);
			if (success) {
				overall_file_offset += buffering.size;
				maximum_file_size = max(maximum_file_size, overall_file_offset);
				buffering.size = 0;
				buffering_entire_write_size = 0;
			}
			return success;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			// Always false
			return false;
		}

		ECS_FILE_HANDLE file;
		// This value is a bit unique. In order to support seeking into the buffering data directly,
		// We need to allow going back and forward into the buffering without flushing the existing data
		// To the file. In order to do this, when going back, register the last known max buffering size,
		// Because the buffering.size must be modified directly, such that it works with the basic buffering
		// Functions, but that would mean that data is lost after the seeking point. This value records this
		// Size such that we know that there is more data than buffering.size indicates
		unsigned int buffering_entire_write_size;
		size_t initial_file_offset;
		// This offset is used to be returned to the GetOffset() call. It avoids having
		// To call into the syscall, which may or may not be expensive, but better to err on
		// The side of caution, since it is not difficult to implement this cached value.
		size_t overall_file_offset;
		// This records the maximum write size the instrument had
		size_t maximum_file_size;
		CapacityStream<void> buffering;
	};

	// This write instrument handles the automatic file creation, with the buffering being optional, and the proper release of it, when used on the stack.
	// It implements a destructor such that it gets released automatically on the stack.
	// Should be used like this - almost all the time.
	// 
	// bool success = OwningBufferedFileWriteInstrument::WriteTo([&](WriteInstrument* write_instrument){
	//		return Operation(write_instrument);
	// });
	// Alternatively, you can call Initialize, but you need to handle the Init failure and the success
	struct ECSENGINE_API OwningBufferedFileWriteInstrument : BufferedFileWriteInstrument {
	private:
		// The provided path must be stable, until this instance is destroyed
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileWriteInstrument(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

		// The provided path must be stable, until this instance is destroyed
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileWriteInstrument(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

	public:
		ECS_INLINE OwningBufferedFileWriteInstrument(OwningBufferedFileWriteInstrument&& other) {
			memcpy(this, &other, sizeof(*this));
			other.is_buffering_allocated = false;
			other.file = -1;
			other.is_failed = true;
		}

		ECS_INLINE OwningBufferedFileWriteInstrument& operator =(OwningBufferedFileWriteInstrument&& other) {
			memcpy(this, &other, sizeof(*this));
			other.is_buffering_allocated = false;
			other.file = -1;
			other.is_failed = true;
			return *this;
		}

		ECS_INLINE ~OwningBufferedFileWriteInstrument() {
			Release();
		}

		// Returns true if it couldn't open the file, else false
		ECS_INLINE bool IsInitializationFailed() const {
			return file == -1;
		}

		// Releases the resources that it is using
		void Release() {
			if (!IsInitializationFailed()) {
				// Ensure that the success status was called, to avoid programming errors
				ECS_ASSERT(is_set_success_called);

				if (!is_failed) {
					Flush();
				}
				if (is_buffering_allocated) {
					buffering.Deallocate(buffering_allocator);
				}
				CloseFile(file);

				// If it is failed, then delete the file
				if (is_failed) {
					RemoveFile(file_path);
				}
			}
		}

		ECS_INLINE void SetSuccess(bool success) override {
			is_failed = !success;
			is_set_success_called = true;
		}

		// Returns an empty optional if the initialization failed, else the properly initialized instrument
		ECS_INLINE static Optional<OwningBufferedFileWriteInstrument> Initialize(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr) {
			Optional<OwningBufferedFileWriteInstrument> instrument = OwningBufferedFileWriteInstrument(file_path, _buffering, binary_file, error_message);
			if (instrument.value.IsInitializationFailed()) {
				instrument.has_value = false;
			}
			return instrument;
		}

		// Returns an empty optional if the initialization failed, else the properly initialized instrument
		ECS_INLINE static Optional<OwningBufferedFileWriteInstrument> Initialize(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr) {
			Optional<OwningBufferedFileWriteInstrument> instrument = OwningBufferedFileWriteInstrument(file_path, _buffering_allocator, _buffering_size, binary_file, error_message);
			if (instrument.value.IsInitializationFailed()) {
				instrument.has_value = false;
			}
			return instrument;
		}

		// Calls the functor with a OwningBufferedFileWriteInstrument* argument and expects the functor to return true, if the write operation
		// Succeeded, else false. It handles the proper initialization and final SetSuccess calls, such that you don't miss them
		template<typename Functor>
		static bool WriteTo(Stream<wchar_t> file_path, Stream<void> buffering, Functor&& functor, bool binary_file = true, CapacityStream<char>* error_message = nullptr) {
			Optional<OwningBufferedFileWriteInstrument> instrument = Initialize(file_path, buffering, binary_file, error_message);
			if (!instrument.has_value) {
				return false;
			}

			bool write_success = functor(&instrument.value);
			instrument.value.SetSuccess(write_success);
			return write_success;
		}

		Stream<wchar_t> file_path;
		AllocatorPolymorphic buffering_allocator;
		bool is_buffering_allocated;
		bool is_failed;
		bool is_set_success_called;
	};

	// This write instrument handles the automatic temporary renaming of an existing file and then creates a new file to write into, 
	// With the buffering being optional, and the proper release of the buffering and renaming to the original file if the write failed, 
	// When used on the stack. If the file does not exist already, it doesn't perform any renaming, it will function as if it is a normal
	// OwningBufferedFileWriteInstrument. It will always start with a failure status, you must explicitly set the success to true to let
	// The instrument know that it shouldn't restore the temporary.
	// Should be used like this - all the time. Always check for failure to open the file
	// 
	// bool success = OwningBufferedFileWriteInstrument::WriteTo([&](WriteInstrument* write_instrument){
	//		return Operation(write_instrument);
	// });
	// Alternatively, you can call Initialize, but you need to handle the Init failure and the success
	struct ECSENGINE_API TemporaryRenameBufferedFileWriteInstrument : BufferedFileWriteInstrument {
	private:
		// The file path and the rename extension must be stable until this instance is destroyed. The provided file path
		// Must be absolute, not relative. By default, if the file doesn't exist, it will not fail, it will continue normally
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		TemporaryRenameBufferedFileWriteInstrument(Stream<wchar_t> absolute_file_path, Stream<void> _buffering, bool binary_file, Stream<wchar_t> _rename_extension, CapacityStream<char>* error_message);

		// The file path and the rename extension must be stable until this instance is destroyed. The provided file path
		// Must be absolute, not relative. By default, if the file doesn't exist, it will not fail, it will continue normally
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		TemporaryRenameBufferedFileWriteInstrument(Stream<wchar_t> absolute_file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size,
			bool binary_file, Stream<wchar_t> _rename_extension, CapacityStream<char>*error_message);

	public:
		TemporaryRenameBufferedFileWriteInstrument(TemporaryRenameBufferedFileWriteInstrument&& other) {
			memcpy(this, &other, sizeof(*this));
			other.is_buffering_allocated = false;
			other.file = -1;
			other.is_failed = true;
			other.is_initialization_failed = true;
		}

		TemporaryRenameBufferedFileWriteInstrument& operator =(TemporaryRenameBufferedFileWriteInstrument&& other) {
			memcpy(this, &other, sizeof(*this));
			other.is_buffering_allocated = false;
			other.file = -1;
			other.is_failed = true;
			other.is_initialization_failed = true;
			return *this;
		}

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
		ECS_INLINE void SetSuccess(bool success) override {
			is_failed = !success;
			is_set_success_called = true;
		}

		// Returns an empty optional if the initialization failed, else the properly initialized instrument
		ECS_INLINE static Optional<TemporaryRenameBufferedFileWriteInstrument> Initialize(Stream<wchar_t> absolute_file_path, Stream<void> _buffering, bool binary_file = true, Stream<wchar_t> _rename_extension = L".temp", CapacityStream<char>* error_message = nullptr) {
			Optional<TemporaryRenameBufferedFileWriteInstrument> instrument = TemporaryRenameBufferedFileWriteInstrument(absolute_file_path, _buffering, binary_file, _rename_extension, error_message);
			if (instrument.value.IsInitializationFailed()) {
				instrument.has_value = false;
			}
			return instrument;
		}

		// Returns an empty optional if the initialization failed, else the properly initialized instrument
		ECS_INLINE static Optional<TemporaryRenameBufferedFileWriteInstrument> Initialize(Stream<wchar_t> absolute_file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size,
			bool binary_file = true, Stream<wchar_t> _rename_extension = L".temp", CapacityStream<char>* error_message = nullptr) {
			Optional<TemporaryRenameBufferedFileWriteInstrument> instrument = TemporaryRenameBufferedFileWriteInstrument(absolute_file_path, _buffering_allocator, _buffering_size, binary_file, _rename_extension, error_message);
			if (instrument.value.IsInitializationFailed()) {
				instrument.has_value = false;
			}
			return instrument;
		}

		// Calls the functor with a TemporaryRenameBufferedFileWriteInstrument* argument and expects the functor to return true, if the write operation
		// Succeeded, else false. It handles the proper initialization and final SetSuccess calls, such that you don't miss them
		template<typename Functor>
		static bool WriteTo(Stream<wchar_t> file_path, Stream<void> buffering, Functor&& functor, bool binary_file = true, Stream<wchar_t> rename_extension = L".temp", CapacityStream<char>* error_message = nullptr) {
			Optional<TemporaryRenameBufferedFileWriteInstrument> instrument = Initialize(file_path, buffering, binary_file, rename_extension, error_message);
			if (!instrument.has_value) {
				return false;
			}

			bool write_success = functor(&instrument.value);
			instrument.value.SetSuccess(write_success);
			return write_success;
		}

		Stream<wchar_t> original_file;
		Stream<wchar_t> rename_extension;
		AllocatorPolymorphic buffering_allocator;
		bool is_buffering_allocated;
		bool is_initialization_failed;
		bool is_renaming_performed;
		bool is_failed;
		bool is_set_success_called;
	};

	struct ECSENGINE_API BufferedFileReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE BufferedFileReadInstrument() : ReadInstrument(0), buffering(), file(-1), initial_file_offset(0), overall_file_offset(0) {}
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
	// Use the static Initialize() function in order to obtain such an instrument
	struct ECSENGINE_API OwningBufferedFileReadInstrument : BufferedFileReadInstrument {
	private:
		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

		// The error message pointer is optional, in case there is an error in opening the file, it will fill in that array
		OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr);

	public:
		ECS_INLINE OwningBufferedFileReadInstrument(OwningBufferedFileReadInstrument&& other) {
			memcpy(this, &other, sizeof(*this));
			other.is_buffering_allocated = false;
			other.file = -1;
		}

		ECS_INLINE OwningBufferedFileReadInstrument& operator =(OwningBufferedFileReadInstrument&& other) {
			memcpy(this, &other, sizeof(*this));
			other.is_buffering_allocated = false;
			other.file = -1;
			return *this;
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

		// Returns an empty optional if the initialization failed, else the properly initialized instrument
		ECS_INLINE static Optional<OwningBufferedFileReadInstrument> Initialize(Stream<wchar_t> file_path, Stream<void> _buffering, bool binary_file = true, CapacityStream<char>* error_message = nullptr) {
			Optional<OwningBufferedFileReadInstrument> instrument = OwningBufferedFileReadInstrument(file_path, _buffering, binary_file, error_message);
			if (instrument.value.IsInitializationFailed()) {
				instrument.has_value = false;
			}
			return instrument;
		}

		// Returns an empty optional if the initialization failed, else the properly initialized instrument
		ECS_INLINE static Optional<OwningBufferedFileReadInstrument> Initialize(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file = true, CapacityStream<char>* error_message = nullptr) {
			Optional<OwningBufferedFileReadInstrument> instrument = OwningBufferedFileReadInstrument(file_path, _buffering_allocator, _buffering_size, binary_file, error_message);
			if (instrument.value.IsInitializationFailed()) {
				instrument.has_value = false;
			}
			return instrument;
		}

		AllocatorPolymorphic buffering_allocator;
		bool is_buffering_allocated;
	};

}