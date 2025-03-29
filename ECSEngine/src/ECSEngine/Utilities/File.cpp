#include "ecspch.h"
#include "File.h"
#include "StringUtilities.h"
#include "PointerUtilities.h"
#include "Utilities.h"
#include "Path.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "ForEachFiles.h"

#include "../OS/FileOS.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	// For file operations
	void SetErrorMessage(CapacityStream<char>* error_message, int error, Stream<wchar_t> path) {
		if (error_message != nullptr) {
			ECS_STACK_CAPACITY_STREAM(char, temp_string, 256);
			switch (error) {
			case ECS_FILE_STATUS_ACCESS_DENIED:
				FormatString(temp_string, "Access to {#} was not granted. Possible causes: tried to open a read-only file for writing,"
					"\nfile's sharing mode does not allow the specified operations, or the given path is a directory", path);
				break;
			case ECS_FILE_STATUS_ALREADY_EXISTS:
				FormatString(temp_string, "File {#} already exists.", path);
				break;
			case ECS_FILE_STATUS_INCORRECT_PATH:
				FormatString(temp_string, "Incorrect path {#}.", path);
				break;
			case ECS_FILE_STATUS_INVALID_ARGUMENTS:
				FormatString(temp_string, "Invalid combination of arguments for {#}.", path);
				break;
			case ECS_FILE_STATUS_TOO_MANY_OPENED_FILES:
				FormatString(temp_string, "Too many opened files when trying to create {#}.", path);
				break;
			case ECS_FILE_STATUS_READING_FILE_FAILED:
				FormatString(temp_string, "Reading from file {#} failed.", path);
				break;
			case ECS_FILE_STATUS_WRITING_FILE_FAILED:
				FormatString(temp_string, "Writing to file {#} failed.", path);
				break;
			case ECS_FILE_STATUS_ERROR:
				FormatString(temp_string, "An unidentifier error occured for {#}.", path);
				break;
			}

			error_message->AddStreamSafe(temp_string);
		}
	}

	ECS_FILE_STATUS_FLAGS FileCreate(Stream<wchar_t> path, ECS_FILE_HANDLE* file_handle, ECS_FILE_ACCESS_FLAGS access_flags, ECS_FILE_CREATE_FLAGS create_flags, CapacityStream<char>* error_message)
	{
		NULL_TERMINATE_WIDE(path);

		int pmode_flags = ECS_FILE_CREATE_READ_WRITE;
		pmode_flags = create_flags == ECS_FILE_CREATE_READ_ONLY ? ECS_FILE_CREATE_READ_ONLY : pmode_flags;
		pmode_flags = create_flags == ECS_FILE_CREATE_WRITE_ONLY ? ECS_FILE_CREATE_WRITE_ONLY : pmode_flags;

		int int_create_flags = (int)create_flags;
		int_create_flags ^= pmode_flags;

		if (pmode_flags == 0) {
			SetErrorMessage(error_message, "No file permission specified");
			return ECS_FILE_STATUS_INVALID_ARGUMENTS;
		}

		int descriptor = _wopen(path.buffer, int_create_flags | access_flags | O_CREAT, pmode_flags);
		if (descriptor == -1) {
			int error = errno;
			SetErrorMessage(error_message, error, path);
			return (ECS_FILE_STATUS_FLAGS)error;
		}

		*file_handle = descriptor;
		return ECS_FILE_STATUS_OK;
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS OpenFile(
		Stream<wchar_t> path,
		ECS_FILE_HANDLE* file_handle,
		ECS_FILE_ACCESS_FLAGS access_flags,
		CapacityStream<char>* error_message
	) {
		NULL_TERMINATE_WIDE(path);

		int descriptor = _wopen(path.buffer, access_flags);
		if (descriptor == -1) {
			int error = errno;
			SetErrorMessage(error_message, error, path);
			return (ECS_FILE_STATUS_FLAGS)error;
		}

		*file_handle = descriptor;
		return ECS_FILE_STATUS_OK;
	}

	// --------------------------------------------------------------------------------------------------

	bool CreateEmptyFile(Stream<wchar_t> path, ECS_FILE_CREATE_FLAGS create_flags, CapacityStream<char>* error_message)
	{
		ECS_FILE_HANDLE file_handle;
		ECS_FILE_STATUS_FLAGS status = FileCreate(path, &file_handle, ECS_FILE_ACCESS_WRITE_ONLY, create_flags, error_message);
		if (status == ECS_FILE_STATUS_OK) {
			CloseFile(file_handle);
			return true;
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool CloseFile(ECS_FILE_HANDLE handle)
	{
		return _close(handle) == 0;
	}

	bool CloseFile(ECS_FILE_HANDLE handle, CapacityStream<void> buffering)
	{
		if (buffering.size > 0) {
			bool success = WriteFile(handle, { buffering.buffer, buffering.size });
			return CloseFile(handle);
		}
		return CloseFile(handle);
	}

	// --------------------------------------------------------------------------------------------------

	size_t WriteToFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		const size_t max_write_per_call = INT_MAX;
		size_t total_written_size = 0;
		while (data.size > 0) {
			size_t current_write_size = data.size > max_write_per_call ? max_write_per_call : data.size;
			int written_size = _write(handle, data.buffer, current_write_size);
			if (written_size == -1) {
				return -1;
			}
			else if ((size_t)written_size != current_write_size) {
				return total_written_size + (size_t)written_size;
			}

			total_written_size += current_write_size;
			data.size -= current_write_size;
		}

		// Should be the same as with the initial data size
		return total_written_size;
	}

	bool WriteFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		return WriteToFile(handle, data) != -1;
	}

	// --------------------------------------------------------------------------------------------------

	size_t WriteToFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering)
	{
		if (buffering.size + data.size < buffering.capacity) {
			memcpy(OffsetPointer(buffering), data.buffer, data.size);
			buffering.size += data.size;
			return data.size;
		}
		else {
			// Write any bytes left from the buffer
			bool success = WriteFile(handle, { buffering.buffer, buffering.size });
			buffering.size = 0;
			if (success) {
				size_t bytes_written = WriteToFile(handle, data);
				return bytes_written;
			}
			return 0;
		}
	}

	bool WriteFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering)
	{
		if (buffering.size + data.size < buffering.capacity) {
			memcpy(OffsetPointer(buffering), data.buffer, data.size);
			buffering.size += data.size;
			return true;
		}
		else {
			// Write any bytes left from the buffer
			bool success = WriteFile(handle, { buffering.buffer, buffering.size });
			buffering.size = 0;
			if (success) {
				unsigned int bytes_written = WriteToFile(handle, data);
				return bytes_written != -1;
			}
			return false;
		}
	}

	// --------------------------------------------------------------------------------------------------

	size_t ReadFromFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		const size_t max_read_per_call = INT_MAX;
		size_t total_read_size = 0;
		while (data.size > 0) {
			size_t current_read_size = data.size > max_read_per_call ? max_read_per_call : data.size;
			int read_size = _read(handle, data.buffer, current_read_size);
			if (read_size == -1) {
				return -1;
			}
			else if ((size_t)read_size != current_read_size) {
				return total_read_size + (size_t)read_size;
			}

			total_read_size += current_read_size;
			data.size -= current_read_size;
		}
		
		// Should be the same as with the initial data size
		return total_read_size;
	}

	// --------------------------------------------------------------------------------------------------

	bool ReadFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		size_t val = ReadFromFile(handle, data);
		return val != -1;
	}

	// --------------------------------------------------------------------------------------------------

	bool ReadFileExact(ECS_FILE_HANDLE handle, Stream<void> data) {
		size_t val = ReadFromFile(handle, data);
		return val == data.size;
	}

	// --------------------------------------------------------------------------------------------------

	size_t ReadFromFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering)
	{
		if (buffering.size >= data.size) {
			memcpy(data.buffer, OffsetPointer(buffering.buffer, buffering.capacity - buffering.size), data.size);
			buffering.size -= data.size;
			return data.size;
		}
		else {
			// Read any bytes left from the buffer
			size_t bytes_to_copy = buffering.size;
			memcpy(data.buffer, OffsetPointer(buffering.buffer, buffering.capacity - buffering.size), bytes_to_copy);
			size_t primary_byte_count = ReadFromFile(handle, { OffsetPointer(data.buffer, bytes_to_copy), data.size - bytes_to_copy });
			size_t buffering_bytes_read = ReadFromFile(handle, { buffering.buffer, buffering.capacity });
			if (buffering_bytes_read == -1) {
				buffering.size = 0;
			}
			else if (buffering_bytes_read < buffering.capacity) {
				memmove(OffsetPointer(buffering.buffer, buffering.capacity - buffering_bytes_read), buffering.buffer, buffering_bytes_read);
				buffering.size = buffering_bytes_read;
			}
			else {
				buffering.size = buffering_bytes_read;
			}
			if (bytes_to_copy == 0 && primary_byte_count == -1) {
				return -1;
			}
			return bytes_to_copy + (primary_byte_count != -1 ? primary_byte_count : 0);
		}
	}

	bool ReadFileExact(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering) {
		size_t read_size = ReadFromFile(handle, data, buffering);
		return read_size == -1 ? false : read_size == data.size;
	}

	// --------------------------------------------------------------------------------------------------s

	size_t GetFileCursor(ECS_FILE_HANDLE handle)
	{
		return _telli64(handle);
	}

	// --------------------------------------------------------------------------------------------------

	size_t SetFileCursor(ECS_FILE_HANDLE handle, int64_t position, ECS_FILE_SEEK seek_point)
	{
		return _lseeki64(handle, position, seek_point);
	}

	bool SetFileCursorBool(ECS_FILE_HANDLE handle, int64_t position, ECS_FILE_SEEK seek_point)
	{
		return SetFileCursor(handle, position, seek_point) != -1;
	}

	// --------------------------------------------------------------------------------------------------

	bool FileEnd(ECS_FILE_HANDLE handle)
	{
		return _eof(handle) == 1;
	}

	// --------------------------------------------------------------------------------------------------

	bool FlushFileToDisk(ECS_FILE_HANDLE handle)
	{
		return _commit(handle) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	size_t GetFileByteSize(Stream<wchar_t> path)
	{
		NULL_TERMINATE_WIDE(path);

		struct _stat64 statistics = {};
		int status = _wstat64(path.buffer, &statistics);
		return status == 0 ? statistics.st_size : 0;
	}

	size_t GetFileByteSize(ECS_FILE_HANDLE file_handle)
	{
		int64_t value = _filelengthi64(file_handle);
		return value == -1 ? 0 : value;
	}

	// --------------------------------------------------------------------------------------------------
	
	template<typename T>
	bool GetFileTimesImpl(ECS_FILE_HANDLE file_handle, T* creation_time, T* access_time, T* last_write_time) {
		HANDLE handle = (HANDLE)_get_osfhandle(file_handle);
		if (handle == INVALID_HANDLE_VALUE) {
			return false;
		}

		return OS::GetFileTimesInternal(handle, creation_time, access_time, last_write_time);
	}

	bool GetFileTimes(ECS_FILE_HANDLE file_handle, CapacityStream<wchar_t>* creation_time, CapacityStream<wchar_t>* access_time, CapacityStream<wchar_t>* last_write_time)
	{
		return GetFileTimesImpl(file_handle, creation_time, access_time, last_write_time);
	}

	// --------------------------------------------------------------------------------------------------

	bool GetFileTimes(ECS_FILE_HANDLE file_handle, CapacityStream<char>* creation_time, CapacityStream<char>* access_time, CapacityStream<char>* last_write_time)
	{
		return GetFileTimesImpl(file_handle, creation_time, access_time, last_write_time);
	}

	// --------------------------------------------------------------------------------------------------

	bool GetFileTimes(ECS_FILE_HANDLE file_handle, size_t* creation_time, size_t* access_time, size_t* last_write_time)
	{
		return GetFileTimesImpl(file_handle, creation_time, access_time, last_write_time);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename T>
	bool GetRelativeFileTimesImpl(ECS_FILE_HANDLE file_handle, T* creation_time, T* access_time, T* last_write_time) {
		HANDLE handle = (HANDLE)_get_osfhandle(file_handle);
		if (handle == INVALID_HANDLE_VALUE) {
			return false;
		}

		return OS::GetRelativeFileTimesInternal(handle, creation_time, access_time, last_write_time);
	}

	bool GetRelativeFileTimes(ECS_FILE_HANDLE file_handle, CapacityStream<wchar_t>* creation_time, CapacityStream<wchar_t>* access_time, CapacityStream<wchar_t>* last_write_time)
	{
		return GetRelativeFileTimesImpl(file_handle, creation_time, access_time, last_write_time);
	}

	// --------------------------------------------------------------------------------------------------

	bool GetRelativeFileTimes(ECS_FILE_HANDLE file_handle, CapacityStream<char>* creation_time, CapacityStream<char>* access_time, CapacityStream<char>* last_write_time)
	{
		return GetRelativeFileTimesImpl(file_handle, creation_time, access_time, last_write_time);
	}
	
	// --------------------------------------------------------------------------------------------------

	bool GetRelativeFileTimes(ECS_FILE_HANDLE file_handle, size_t* creation_time, size_t* access_time, size_t* last_write_time)
	{
		return GetRelativeFileTimesImpl(file_handle, creation_time, access_time, last_write_time);
	}

	// --------------------------------------------------------------------------------------------------

	size_t GetFileLastWrite(ECS_FILE_HANDLE file_handle) {
		size_t last_write = 0;
		GetFileTimes(file_handle, nullptr, nullptr, &last_write);
		return last_write;
	}

	// --------------------------------------------------------------------------------------------------

	bool HasSubdirectories(Stream<wchar_t> directory)
	{
		bool has_subdirectory = false;
		ForEachDirectory(directory, &has_subdirectory, [](Stream<wchar_t> path, void* _data) {
			bool* data = (bool*)_data;
			*data = true;
			return false;
		});

		return has_subdirectory;
	}

	// --------------------------------------------------------------------------------------------------

	bool ClearFile(Stream<wchar_t> path) {
		ECS_FILE_HANDLE file_handle = 0;
		bool success = OpenFile(path, &file_handle, ECS_FILE_ACCESS_TRUNCATE_FILE | ECS_FILE_ACCESS_READ_ONLY) == ECS_FILE_STATUS_OK;
		if (success) {
			CloseFile(file_handle);
		}
		return success;
	}

	// --------------------------------------------------------------------------------------------------

	bool ClearFile(ECS_FILE_HANDLE file_handle)
	{
		bool success = ResizeFile(file_handle, 0);
		// We also need to reset the file pointer
		if (success) {
			return SetFileCursorBool(file_handle, 0, ECS_FILE_SEEK_BEG);
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFile(Stream<wchar_t> file)
	{
		NULL_TERMINATE_WIDE(file);
		bool return_status = _wremove(file.buffer) == 0;
		// For debug purposes
		int errno_val = errno;
		return return_status;
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFolder(Stream<wchar_t> folder) {
		NULL_TERMINATE_WIDE(folder);

		// Delete folder contents
		bool delete_contents_success = DeleteFolderContents(folder);
		if (!delete_contents_success) {
			return false;
		}

		// The folder must be empty for this function
		return _wrmdir(folder.buffer) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool CreateFolder(Stream<wchar_t> folder) {
		NULL_TERMINATE_WIDE(folder);
		return _wmkdir(folder.buffer) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool FileCopy(Stream<wchar_t> from, Stream<wchar_t> to, bool use_filename_from, bool overwrite_existent)
	{
		NULL_TERMINATE_WIDE(from);
		NULL_TERMINATE_WIDE(to);

		// Decide if the Win32 variant should be kept or the Posix API one

		//// Open file in binary mode using POSIX api's
		//ECS_FILE_HANDLE read_handle = 0;
		//ECS_FILE_STATUS_FLAGS read_status = OpenFile(from, &read_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY);
		//if (read_status != ECS_FILE_STATUS_OK) {
		//	return false;
		//}
		//ScopedFile scoped_read(read_handle);

		//// Get the file size of the file to be copied
		//size_t file_size = GetFileByteSize(read_handle);
		//if (file_size == 0) {
		//	return false;
		//}

		//// Append the filename (the stem and the extension) to the to path
		//ECS_STACK_CAPACITY_STREAM(wchar_t, complete_to_path, 512);
		//complete_to_path.Copy(to);
		//bool is_absolute = PathIsAbsolute(complete_to_path);
		//complete_to_path.Add(is_absolute ? ECS_OS_PATH_SEPARATOR : ECS_OS_PATH_SEPARATOR_REL);
		//Stream<wchar_t> from_filename = PathFilenameBoth(from);
		//complete_to_path.AddStream(from_filename);
		//complete_to_path.Add(L'\0');
		//
		//ECS_FILE_HANDLE write_handle = 0;
		//ECS_FILE_CREATE_FLAGS create_flags = ECS_FILE_CREATE_READ_WRITE;
		//create_flags |= overwrite_existent ? (ECS_FILE_CREATE_FLAGS)0 : ECS_FILE_CREATE_FAIL_IF_EXISTS;
		//ECS_FILE_STATUS_FLAGS write_status = FileCreate(complete_to_path.buffer, &write_handle, ECS_FILE_ACCESS_WRITE_ONLY 
		//	| ECS_FILE_ACCESS_TRUNCATE_FILE | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY, create_flags);
		//if (write_status != ECS_FILE_STATUS_OK) {	
		//	return false;
		//}
		//ScopedFile scoped_write(write_handle);

		//// Use malloc to read all the bytes from that file and copy them over to the other file
		//void* buffer = ECS_MALLOCA(file_size);
		//ScopedMalloca malloca(buffer);

		//unsigned int read_bytes = ReadFromFile(read_handle, { buffer, file_size });
		//if (read_bytes == -1) {
		//	return false;
		//}

		//// Write the bytes now
		//unsigned int written_bytes = WriteToFile(write_handle, { buffer, read_bytes });
		//if (written_bytes != read_bytes) {
		//	return false;
		//}

		//return true;

		ECS_STACK_CAPACITY_STREAM(wchar_t, complete_to_path, 512);
		const wchar_t* to_path = to.buffer;
		if (use_filename_from) {
			complete_to_path.CopyOther(to);
			bool is_absolute = PathIsAbsolute(complete_to_path);
			complete_to_path.AddAssert(is_absolute ? ECS_OS_PATH_SEPARATOR : ECS_OS_PATH_SEPARATOR_REL);
			Stream<wchar_t> from_filename = PathFilenameBoth(from);
			complete_to_path.AddStreamAssert(from_filename);
			complete_to_path.AddAssert(L'\0');

			to_path = complete_to_path.buffer;
		}

		BOOL success = CopyFile(from.buffer, to_path, !overwrite_existent);
		DWORD error = GetLastError();
		return success;
	}

	// --------------------------------------------------------------------------------------------------

	bool FileCut(Stream<wchar_t> from, Stream<wchar_t> to, bool use_filename_from, bool overwrite_existent) {
		bool status = FileCopy(from, to, use_filename_from, overwrite_existent);
		if (!status) {
			return status;
		}

		bool success = RemoveFile(from);
		return success;
	}

	// --------------------------------------------------------------------------------------------------

	bool FolderCopy(Stream<wchar_t> from, Stream<wchar_t> to)
	{
		// The strings need to be doubly null terminated.
		// Copy to a temporary buffer and null terminate both
		ECS_STACK_CAPACITY_STREAM(wchar_t, from_terminated, 512);
		ECS_STACK_CAPACITY_STREAM(wchar_t, to_terminated, 512);
		from_terminated.CopyOther(from);
		to_terminated.CopyOther(to);

		from_terminated.Add(L'\0');
		from_terminated.Add(L'\0');

		to_terminated.Add(L'\0');
		to_terminated.Add(L'\0');

		SHFILEOPSTRUCT operation_data = { 0 };
		operation_data.hwnd = nullptr;
		operation_data.wFunc = FO_COPY;
		operation_data.fFlags = FOF_NO_UI;
		operation_data.pFrom = from_terminated.buffer;
		operation_data.pTo = to_terminated.buffer;
		int status = SHFileOperation(&operation_data);
		return status == 0 && !operation_data.fAnyOperationsAborted;
	}

	// --------------------------------------------------------------------------------------------------

	bool FolderCut(Stream<wchar_t> from, Stream<wchar_t> to)
	{
		bool success = FolderCopy(from, to);
		if (success) {
			return RemoveFolder(from);
		}
		return false;
	}
	
	// --------------------------------------------------------------------------------------------------

	bool RenameFolderOrFile(Stream<wchar_t> path, Stream<wchar_t> new_name) {
		NULL_TERMINATE_WIDE(path);

		ECS_STACK_CAPACITY_STREAM(wchar_t, new_name_stream, 512);
		Stream<wchar_t> folder_parent = PathParentBoth(path);
		new_name_stream.CopyOther(folder_parent);
		new_name_stream.Add(ECS_OS_PATH_SEPARATOR);
		new_name_stream.AddStream(new_name);
		new_name_stream.AddAssert(L'\0');

		return _wrename(path.buffer, new_name_stream.buffer) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool RenameFolder(Stream<wchar_t> path, Stream<wchar_t> new_name) {
		NULL_TERMINATE_WIDE(path);

		ECS_STACK_CAPACITY_STREAM(wchar_t, new_name_stream, 512);
		Stream<wchar_t> folder_parent = PathParentBoth(path);
		new_name_stream.CopyOther(folder_parent);
		new_name_stream.Add(ECS_OS_PATH_SEPARATOR);
		new_name_stream.AddStream(new_name);
		new_name_stream.AddAssert(L'\0');

		return _wrename(path.buffer, new_name_stream.buffer) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool RenameFile(Stream<wchar_t> path, Stream<wchar_t> new_name) {
		NULL_TERMINATE_WIDE(path);

		ECS_STACK_CAPACITY_STREAM(wchar_t, new_name_stream, 512);
		Stream<wchar_t> folder_parent = PathParentBoth(path);
		new_name_stream.CopyOther(folder_parent);
		new_name_stream.Add(ECS_OS_PATH_SEPARATOR);
		new_name_stream.AddStream(new_name);
		Stream<wchar_t> extension = PathExtensionBoth(path);
		new_name_stream.AddStreamAssert(extension);
		new_name_stream.AddAssert(L'\0');

		return _wrename(path.buffer, new_name_stream.buffer) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool RenameFileAbsolute(Stream<wchar_t> path, Stream<wchar_t> new_absolute_path)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, temp_path, 512);
		// If the path and the new absolute path alias each other, then we need to copy
		// one into a temp buffer
		bool do_alias = AreAliasing(path, new_absolute_path);
		if (do_alias) {
			temp_path.CopyOther(path);
			path = temp_path;
		}

		NULL_TERMINATE_WIDE(path);
		NULL_TERMINATE_WIDE(new_absolute_path);

		return _wrename(path.buffer, new_absolute_path.buffer) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool RenameFileOrFolder(Stream<wchar_t> path, Stream<wchar_t> new_name)
	{
		if (IsFolder(path)) {
			return RenameFolder(path, new_name);
		}
		else {
			return RenameFile(path, new_name);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool ResizeFile(Stream<wchar_t> file, size_t size)
	{
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_STATUS_FLAGS status = OpenFile(file, &file_handle, ECS_FILE_ACCESS_WRITE_ONLY);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}

		bool success = ResizeFile(file_handle, size);
		CloseFile(file_handle);
		return success;
	}

	// --------------------------------------------------------------------------------------------------

	bool ResizeFile(ECS_FILE_HANDLE file_handle, size_t size)
	{
		return _chsize_s(file_handle, size) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool ChangeFileExtension(Stream<wchar_t> file, Stream<wchar_t> extension) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, new_name, 512);
		Stream<wchar_t> original_extension = PathExtensionBoth(file);
		new_name.CopyOther(Stream<wchar_t>(file.buffer, original_extension.buffer - file.buffer));
		new_name.AddStream(extension);
		new_name.AddAssert(L'\0');
		return RenameFolderOrFile(file, new_name.buffer);
	}

	// --------------------------------------------------------------------------------------------------

	bool DeleteFolderContents(Stream<wchar_t> path) {
		NULL_TERMINATE_WIDE(path);

		bool success = true;
		auto folder_action = [](Stream<wchar_t> path, void* data) {
			bool* success = (bool*)data;
			bool result = RemoveFolder(path);
			*success = (result == false) * false + (result == true) * true;

			return true;
		};

		auto file_action = [](Stream<wchar_t> path, void* data) {
			bool* success = (bool*)data;
			bool result = RemoveFile(path);
			*success = (result == false) * false + (result == true) * true;

			return true;
		};

		ForEachInDirectory(path, &success, folder_action, file_action);
		return success;
	}

	// --------------------------------------------------------------------------------------------------

	bool HideFolder(Stream<wchar_t> path)
	{
		NULL_TERMINATE_WIDE(path);
		return SetFileAttributes(path.buffer, FILE_ATTRIBUTE_HIDDEN);
	}

	// --------------------------------------------------------------------------------------------------

	Stream<void> ReadWholeFile(Stream<wchar_t> path, bool binary, AllocatorPolymorphic allocator, bool* empty_file)
	{
		if (binary) {
			return ReadWholeFileBinary(path, allocator, empty_file);
		}
		else {
			return ReadWholeFileText(path, allocator, empty_file);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFile(Stream<wchar_t> path)
	{
		NULL_TERMINATE_WIDE(path);

		struct _stat64 statistics = {};
		int status = _wstat64(path.buffer, &statistics);
		if (status == 0) {
			return HasFlag(statistics.st_mode, S_IFREG);
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFolder(Stream<wchar_t> path)
	{
		NULL_TERMINATE_WIDE(path);

		struct _stat64 statistics = {};
		int status = _wstat64(path.buffer, &statistics);
		if (status == 0) {
			return HasFlag(statistics.st_mode, S_IFDIR);
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	Stream<void> ReadWholeFileBinary(Stream<wchar_t> path, AllocatorPolymorphic allocator, bool* empty_file)
	{
		if (empty_file) {
			*empty_file = false;
		}

		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_ACCESS_FLAGS access = ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY;
		ECS_FILE_STATUS_FLAGS status = OpenFile(path, &file_handle, access);
		if (status != ECS_FILE_STATUS_OK) {
			return { nullptr, 0 };
		}

		ScopedFile scoped_file({ file_handle });
		size_t file_size = GetFileByteSize(file_handle);
		if (file_size == 0) {
			if (empty_file) {
				*empty_file = true;
			}
			return  { nullptr, 0 };
		}

		void* allocation = nullptr;
		allocation = Allocate(allocator, file_size);
		unsigned int read_bytes = ReadFromFile(file_handle, { allocation, file_size });
		if (read_bytes != file_size) {
			Deallocate(allocator, allocation);
			return { nullptr, 0 };
		}

		return { allocation, file_size };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> ReadWholeFileText(Stream<wchar_t> path, AllocatorPolymorphic allocator, bool* empty_file)
	{
		if (empty_file) {
			*empty_file = false;
		}

		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_ACCESS_FLAGS access = ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_TEXT;
		ECS_FILE_STATUS_FLAGS status = OpenFile(path, &file_handle, access);
		if (status != ECS_FILE_STATUS_OK) {
			return { nullptr, 0 };
		}

		ScopedFile scoped_file({ file_handle });
		size_t file_size = GetFileByteSize(file_handle);
		if (file_size == 0) {
			if (empty_file) {
				*empty_file = true;
			}
			return  { nullptr, 0 };
		}

		// Add a '\0' at the end of the file in order to help with C functions parsing
		void* allocation = Allocate(allocator, file_size + 1);
		unsigned int read_bytes = ReadFromFile(file_handle, { allocation, file_size });
		if (read_bytes == -1) {
			Deallocate(allocator, allocation);
			return { nullptr, 0 };
		}

		// Text files will return a smaller number of bytes since it will convert line feed carriages
		file_size = read_bytes;
		char* characters = (char*)allocation;
		characters[read_bytes] = '\0';
		return { allocation, file_size };
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS WriteBufferToFileBinary(Stream<wchar_t> path, Stream<void> buffer, bool append_data)
	{
		ECS_FILE_HANDLE handle = 0;
		ECS_FILE_ACCESS_FLAGS access_flags = append_data ? ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_APEND | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL
			: ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE;

		ECS_FILE_STATUS_FLAGS status = FileCreate(path, &handle, access_flags);
		if (status != ECS_FILE_STATUS_OK) {
			return status;
		}

		bool success = WriteFile(handle, buffer);
		CloseFile(handle);

		return success ? ECS_FILE_STATUS_OK : ECS_FILE_STATUS_WRITING_FILE_FAILED;
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS WriteBufferToFileText(Stream<wchar_t> path, Stream<void> buffer, bool append_data)
	{
		ECS_FILE_HANDLE handle = 0;
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL;
		access_flags |= append_data ? ECS_FILE_ACCESS_APEND : ECS_FILE_ACCESS_TRUNCATE_FILE;

		ECS_FILE_STATUS_FLAGS status = FileCreate(path, &handle, access_flags);
		if (status != ECS_FILE_STATUS_OK) {
			return status;
		}

		bool success = WriteFile(handle, buffer);
		CloseFile(handle);

		return success ? ECS_FILE_STATUS_OK : ECS_FILE_STATUS_WRITING_FILE_FAILED;
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS WriteBufferToFile(Stream<wchar_t> path, Stream<void> buffer, bool binary, bool append_data)
	{
		return binary ? WriteBufferToFileBinary(path, buffer, append_data) : WriteBufferToFileText(path, buffer, append_data);
	}

	// --------------------------------------------------------------------------------------------------

	bool ExistsFileOrFolder(Stream<wchar_t> path)
	{
		NULL_TERMINATE_WIDE(path);
		return _waccess(path.buffer, 0) == 0;
	}

	// --------------------------------------------------------------------------------------------------

}