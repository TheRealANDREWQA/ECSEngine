#include "ecspch.h"
#include "BufferedFileReaderWriter.h"

namespace ECSEngine {

	OwningBufferedFileWriteInstrument::OwningBufferedFileWriteInstrument(Stream<wchar_t> _file_path, CapacityStream<void> _buffering, bool binary_file, CapacityStream<char>* error_message) 
		: is_buffering_allocated(false), is_failed(true), file_path(_file_path), is_set_success_called(false)
	{
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY;
		access_flags |= binary_file ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		// Use file create in order to create the file if it doesn't already exist
		ECS_FILE_STATUS_FLAGS status = FileCreate(file_path, &file_handle, access_flags, ECS_FILE_CREATE_READ_WRITE, error_message);

		file = status == ECS_FILE_STATUS_OK ? file_handle : -1;
		initial_file_offset = 0;
		buffering = _buffering;
	}

	OwningBufferedFileWriteInstrument::OwningBufferedFileWriteInstrument(
		Stream<wchar_t> file_path, 
		AllocatorPolymorphic _buffering_allocator, 
		size_t _buffering_size, 
		bool binary_file, 
		CapacityStream<char>* error_message
	)
		// In case the buffering size is 0, don't call the allocate function, to not generate an empty allocation
		: OwningBufferedFileWriteInstrument(file_path, CapacityStream<void>(_buffering_size > 0 ? Allocate(_buffering_allocator, _buffering_size) : nullptr, 0, _buffering_size), binary_file, error_message) {
		if (!IsInitializationFailed()) {
			buffering_allocator = buffering_allocator;
			is_buffering_allocated = true;
		}
		else {
			// Release the allocated buffering now
			buffering.Deallocate(_buffering_allocator);
		}
	}

	TemporaryRenameBufferedFileWriteInstrument::TemporaryRenameBufferedFileWriteInstrument(Stream<wchar_t> absolute_file_path, CapacityStream<void> _buffering, 
		bool binary_file, Stream<wchar_t> _rename_extension, CapacityStream<char>* error_message)
		: is_buffering_allocated(false), rename_extension(_rename_extension), original_file(absolute_file_path), is_initialization_failed(true), is_failed(true),
		is_renaming_performed(false), is_set_success_called(false)
	{
		// Perform the renaming first
		ECS_STACK_CAPACITY_STREAM(wchar_t, renamed_file, 1024);
		GetRenamedFile(renamed_file);
		if (ExistsFileOrFolder(absolute_file_path)) {
			if (!RenameFileAbsolute(absolute_file_path, renamed_file)) {
				// If we fail, then return now.
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not rename file from {#} to temporary", absolute_file_path);
				return;
			}

			is_renaming_performed = true;
		}
		// In case the file didn't exist already, that is not an error

		// Create the new file
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY;
		access_flags |= binary_file ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		// Use file create in order to create the file if it doesn't already exist
		ECS_FILE_STATUS_FLAGS status = FileCreate(absolute_file_path, &file_handle, access_flags, ECS_FILE_CREATE_READ_WRITE, error_message);

		file = status == ECS_FILE_STATUS_OK ? file_handle : -1;
		initial_file_offset = 0;
		buffering = _buffering;

		if (status != ECS_FILE_STATUS_OK) {
			// Perform the rename back to the previous name, since creating the file failed
			if (is_renaming_performed) {
				if (!RenameFileAbsolute(renamed_file, absolute_file_path)) {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "\nCould not rename temporary file {#} back to its original path", renamed_file);
				}
				is_renaming_performed = false;
			}
		}
		else {
			is_initialization_failed = false;
		}
	}

	TemporaryRenameBufferedFileWriteInstrument::TemporaryRenameBufferedFileWriteInstrument(
		Stream<wchar_t> absolute_file_path, 
		AllocatorPolymorphic _buffering_allocator, 
		size_t _buffering_size,
		bool binary_file, 
		Stream<wchar_t> _rename_extension,
		CapacityStream<char>* error_message
	)
		// In case the buffering size is 0, don't call the allocate function, to not generate an empty allocation
		: TemporaryRenameBufferedFileWriteInstrument(absolute_file_path, CapacityStream<void>(_buffering_size > 0 ? Allocate(_buffering_allocator, _buffering_size) : nullptr, 0, _buffering_size), binary_file, _rename_extension, error_message) {
		if (!IsInitializationFailed()) {
			buffering_allocator = buffering_allocator;
			is_buffering_allocated = true;
		}
		else {
			// Release the allocated buffering now
			buffering.Deallocate(_buffering_allocator);
		}
	}

	bool TemporaryRenameBufferedFileWriteInstrument::Finish(CapacityStream<char>* error_message) {
		bool has_errors = false;

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

			ECS_STACK_CAPACITY_STREAM(wchar_t, renamed_file, 1024);
			GetRenamedFile(renamed_file);
			if (is_failed) {
				if (is_renaming_performed) {
					// We must delete the current file and rename the temporary back to its original name
					if (!RemoveFile(original_file)) {
						ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to remove file {#} before renaming the original file back", renamed_file);
						has_errors = true;
					}

					// No matter if the previous remove succeeded, perform the rename anyways
					if (!RenameFileAbsolute(renamed_file, original_file)) {
						ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to restore temporary file {#} to its original path", renamed_file);
						has_errors = true;
					}
				}
				else {
					// The original file didn't exist, we must remove the current one only
					if (!RemoveFile(original_file)) {
						ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to remove file {#} after unsuccessful write", renamed_file);
						has_errors = true;
					}
				}
			}
			else {
				// Remove the temporary file
				if (!RemoveFile(renamed_file)) {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to delete temporary file {#} after successful write", renamed_file);
					has_errors = true;
				}
			}
		}

		// Set this to true, such that a call again to this function won't trigger another re-release
		is_initialization_failed = true;
		return has_errors;
	}

	bool BufferedFileReadInstrument::SeekImpl(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) {
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
					overall_file_offset += offset;
					return true;
				}
				else {
					// Discard the entire buffering, and seek ahead in the file
					offset -= buffering.size;
					buffering.size = 0;
					bool success = SetFileCursorBool(file, offset, ECS_FILE_SEEK_CURRENT);
					overall_file_offset += success ? offset : 0;
					return success;
				}
			}
			else {
				if (buffering.size + offset <= buffering.capacity && buffering.size + offset >= 0) {
					// We fit into the buffering, simply move back the buffering read end
					buffering.size += offset;
					overall_file_offset += offset;
					return true;
				}
				else {
					// We need to seek for the amount of surplus buffering bytes, equal to the current buffering size
					offset = offset - (int64_t)buffering.size;
					// Discard the buffering, and seek into the file
					buffering.size = 0;
					bool success = SetFileCursorBool(file, offset, ECS_FILE_SEEK_CURRENT);
					overall_file_offset += success ? offset : 0;
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
			if (success) {
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
					overall_file_offset = (size_t)((int64_t)total_size + offset);
				}
				break;
				default:
					ECS_ASSERT(false)
				}
			}
			return success;
		}
	}

	OwningBufferedFileReadInstrument::OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, CapacityStream<void> _buffering, bool binary_file, CapacityStream<char>* error_message) : is_buffering_allocated(false) {
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_READ_ONLY;
		access_flags |= binary_file ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		// Use file create in order to create the file if it doesn't already exist
		ECS_FILE_STATUS_FLAGS status = OpenFile(file_path, &file_handle, access_flags, error_message);

		file = status == ECS_FILE_STATUS_OK ? file_handle : -1;
		initial_file_offset = 0;
		buffering = _buffering;

		if (status == ECS_FILE_STATUS_OK) {
			total_size = GetFileByteSize(file);
		}
	}

	OwningBufferedFileReadInstrument::OwningBufferedFileReadInstrument(Stream<wchar_t> file_path, AllocatorPolymorphic _buffering_allocator, size_t _buffering_size, bool binary_file, CapacityStream<char>* error_message)
		// In case the buffering size is 0, don't call the allocate function, to not generate an empty allocation
		: OwningBufferedFileReadInstrument(file_path, CapacityStream<void>(_buffering_size > 0 ? Allocate(_buffering_allocator, _buffering_size) : nullptr, 0, _buffering_size), binary_file, error_message) {
		if (!IsInitializationFailed()) {
			buffering_allocator = buffering_allocator;
			is_buffering_allocated = true;
		}
		else {
			// Release the allocated buffering now
			buffering.Deallocate(_buffering_allocator);
		}
	}

}