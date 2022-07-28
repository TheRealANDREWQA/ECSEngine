#include "ecspch.h"
#include "File.h"
#include "Function.h"
#include "FunctionInterfaces.h"
#include "Path.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "ForEachFiles.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	// For file operations
	template<typename char_type>
	void SetErrorMessage(CapacityStream<char>* error_message, int error, const char_type* path) {
		if (error_message != nullptr) {
			ECS_TEMP_ASCII_STRING(temp_string, 256);
			switch (error) {
			case ECS_FILE_STATUS_ACCESS_DENIED:
				ECS_FORMAT_STRING(temp_string, "Access to {#} was not granted. Possible causes: tried to open a read-only file for writing,"
					"\nfile's sharing mode does not allow the specified operations, or the given path is a directory", path);
				break;
			case ECS_FILE_STATUS_ALREADY_EXISTS:
				ECS_FORMAT_STRING(temp_string, "File {#} already exists.", path);
				break;
			case ECS_FILE_STATUS_INCORRECT_PATH:
				ECS_FORMAT_STRING(temp_string, "Incorrect path {#}.", path);
				break;
			case ECS_FILE_STATUS_INVALID_ARGUMENTS:
				ECS_FORMAT_STRING(temp_string, "Invalid combination of arguments for {#}.", path);
				break;
			case ECS_FILE_STATUS_TOO_MANY_OPENED_FILES:
				ECS_FORMAT_STRING(temp_string, "Too many opened files when trying to create {#}.", path);
				break;
			case ECS_FILE_STATUS_READING_FILE_FAILED:
				ECS_FORMAT_STRING(temp_string, "Reading from file {#} failed.", path);
				break;
			case ECS_FILE_STATUS_WRITING_FILE_FAILED:
				ECS_FORMAT_STRING(temp_string, "Writing to file {#} failed.", path);
				break;
			case ECS_FILE_STATUS_ERROR:
				ECS_FORMAT_STRING(temp_string, "An unidentifier error occured for {#}.", path);
				break;
			}

			error_message->AddStreamSafe(temp_string);
		}
	}

	ECS_FILE_STATUS_FLAGS FileCreate(const wchar_t* path, ECS_FILE_HANDLE* file_handle, ECS_FILE_ACCESS_FLAGS access_flags, ECS_FILE_CREATE_FLAGS create_flags, CapacityStream<char>* error_message)
	{
		int pmode_flags = ECS_FILE_CREATE_READ_WRITE;
		pmode_flags = create_flags == ECS_FILE_CREATE_READ_ONLY ? ECS_FILE_CREATE_READ_ONLY : pmode_flags;
		pmode_flags = create_flags == ECS_FILE_CREATE_WRITE_ONLY ? ECS_FILE_CREATE_WRITE_ONLY : pmode_flags;

		int int_create_flags = (int)create_flags;
		int_create_flags ^= pmode_flags;

		if (pmode_flags == 0) {
			function::SetErrorMessage(error_message, "No file permission specified");
			return ECS_FILE_STATUS_INVALID_ARGUMENTS;
		}

		int descriptor = _wopen(path, int_create_flags | access_flags | O_CREAT, pmode_flags);
		if (descriptor == -1) {
			int error = errno;
			SetErrorMessage(error_message, error, path);
			return (ECS_FILE_STATUS_FLAGS)error;
		}

		*file_handle = descriptor;
		return ECS_FILE_STATUS_OK;
	}

	ECS_FILE_STATUS_FLAGS FileCreate(Stream<wchar_t> path, ECS_FILE_HANDLE* file_handle, ECS_FILE_ACCESS_FLAGS access_flags, ECS_FILE_CREATE_FLAGS create_flags, CapacityStream<char>* error_message)
	{
		ECS_FORWARD_STREAM_WIDE(path, FileCreate, file_handle, access_flags, create_flags, error_message);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS OpenFile(const wchar_t* path, ECS_FILE_HANDLE* file_handle, ECS_FILE_ACCESS_FLAGS access_flags, CapacityStream<char>* error_message)
	{
		int descriptor = _wopen(path, access_flags);
		if (descriptor == -1) {
			int error = errno;
			SetErrorMessage(error_message, error, path);
			return (ECS_FILE_STATUS_FLAGS)error;
		}

		*file_handle = descriptor;
		return ECS_FILE_STATUS_OK;
	}

	ECS_FILE_STATUS_FLAGS OpenFile(
		Stream<wchar_t> path,
		ECS_FILE_HANDLE* file_handle,
		ECS_FILE_ACCESS_FLAGS access_flags,
		CapacityStream<char>* error_message
	) {
		ECS_FORWARD_STREAM_WIDE(path, OpenFile, file_handle, access_flags, error_message)
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

	unsigned int WriteToFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		return _write(handle, data.buffer, data.size);
	}

	bool WriteFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		return WriteToFile(handle, data) != -1;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int WriteToFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering)
	{
		if (buffering.size + data.size < buffering.capacity) {
			memcpy(function::OffsetPointer(buffering), data.buffer, data.size);
			buffering.size += data.size;
			return data.size;
		}
		else {
			// Write any bytes left from the buffer
			bool success = WriteFile(handle, { buffering.buffer, buffering.size });
			buffering.size = 0;
			if (success) {
				unsigned int bytes_written = WriteToFile(handle, data);
				return bytes_written;
			}
			return 0;
		}
	}

	bool WriteFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering)
	{
		if (buffering.size + data.size < buffering.capacity) {
			memcpy(function::OffsetPointer(buffering), data.buffer, data.size);
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

	unsigned int ReadFromFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		return _read(handle, data.buffer, data.size);
	}

	// --------------------------------------------------------------------------------------------------

	bool ReadFile(ECS_FILE_HANDLE handle, Stream<void> data)
	{
		return ReadFromFile(handle, data) != -1;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int ReadFromFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering)
	{
		if (buffering.size >= data.size) {
			memcpy(data.buffer, function::OffsetPointer(buffering.buffer, buffering.capacity - buffering.size), data.size);
			buffering.size -= data.size;
			return data.size;
		}
		else {
			// Read any bytes left from the buffer
			unsigned int bytes_to_copy = buffering.size;
			memcpy(data.buffer, function::OffsetPointer(buffering.buffer, buffering.capacity - buffering.size), bytes_to_copy);
			unsigned int primary_byte_count = ReadFromFile(handle, { function::OffsetPointer(data.buffer, bytes_to_copy), data.size - bytes_to_copy });
			unsigned int buffering_bytes_read = ReadFromFile(handle, { buffering.buffer, buffering.capacity });
			if (buffering_bytes_read == -1) {
				buffering.size = 0;
			}
			else if (buffering_bytes_read < buffering.capacity) {
				memmove(function::OffsetPointer(buffering.buffer, buffering.capacity - buffering_bytes_read), buffering.buffer, buffering_bytes_read);
				buffering.size = buffering.capacity - buffering_bytes_read;
			}
			else {
				buffering.size = buffering_bytes_read;
			}
			return bytes_to_copy + function::Select<unsigned int>(primary_byte_count != -1, primary_byte_count, 0);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool ReadFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering)
	{
		if (buffering.size + data.size <= buffering.capacity) {
			memcpy(data.buffer, function::OffsetPointer(buffering), data.size);
			buffering.size += data.size;
			return true;
		}
		else {
			unsigned int bytes_to_copy = buffering.size;
			memcpy(data.buffer, function::OffsetPointer(buffering), bytes_to_copy);
			unsigned int primary_byte_count = ReadFromFile(handle, { function::OffsetPointer(data.buffer, bytes_to_copy), data.size - bytes_to_copy });
			unsigned int buffering_bytes_read = ReadFromFile(handle, { buffering.buffer, buffering.capacity });
			if (buffering_bytes_read == -1) {
				buffering.size = buffering.capacity;
			}
			else if (buffering_bytes_read < buffering.capacity) {
				memmove(function::OffsetPointer(buffering.buffer, buffering.capacity - buffering_bytes_read), buffering.buffer, buffering_bytes_read);
				buffering.size = buffering.capacity - buffering_bytes_read;
			}
			else {
				buffering.size = buffering_bytes_read;
			}
			return primary_byte_count != -1;
		}
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

	size_t GetFileByteSize(const wchar_t* path)
	{
		struct _stat64 statistics = {};
		int status = _wstat64(path, &statistics);
		return status == 0 ? statistics.st_size : 0;
	}

	size_t GetFileByteSize(Stream<wchar_t> path)
	{
		ECS_FORWARD_STREAM_WIDE(path, GetFileByteSize);
	}

	size_t GetFileByteSize(ECS_FILE_HANDLE file_handle)
	{
		int64_t value = _filelengthi64(file_handle);
		return value == -1 ? 0 : value;
	}
	
	// --------------------------------------------------------------------------------------------------

	bool HasSubdirectories(const wchar_t* directory)
	{
		bool has_subdirectory = false;
		ForEachDirectory(directory, &has_subdirectory, [](const wchar_t* path, void* _data) {
			bool* data = (bool*)_data;
			*data = true;
			return false;
		});

		return has_subdirectory;
	}

	bool HasSubdirectories(Stream<wchar_t> directory)
	{
		ECS_FORWARD_STREAM_WIDE(directory, HasSubdirectories);
	}

	// --------------------------------------------------------------------------------------------------

	bool ClearFile(const wchar_t* path) {
		ECS_FILE_HANDLE file_handle = 0;
		bool success = OpenFile(path, &file_handle, ECS_FILE_ACCESS_TRUNCATE_FILE | ECS_FILE_ACCESS_READ_ONLY) == ECS_FILE_STATUS_OK;
		if (success) {
			CloseFile(file_handle);
		}
		return success;
	}

	bool ClearFile(Stream<wchar_t> path) {
		ECS_FORWARD_STREAM_WIDE(path, ClearFile);
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFile(const wchar_t* file)
	{
		return _wremove(file) == 0;
	}

	bool RemoveFile(Stream<wchar_t> file)
	{
		ECS_FORWARD_STREAM_WIDE(file, RemoveFile);
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFolder(const wchar_t* folder) {
		// Delete folder contents
		DeleteFolderContents(folder);

		// The folder must be empty for this function
		return _wrmdir(folder) == 0;
	}

	bool RemoveFolder(Stream<wchar_t> folder) {
		ECS_FORWARD_STREAM_WIDE(folder, RemoveFolder);
	}

	// --------------------------------------------------------------------------------------------------

	bool CreateFolder(const wchar_t* folder) {
		return _wmkdir(folder) == 0;
	}


	bool CreateFolder(Stream<wchar_t> folder) {
		ECS_FORWARD_STREAM_WIDE(folder, CreateFolder);
	}

	// --------------------------------------------------------------------------------------------------

	bool FileCopy(const wchar_t* from, const wchar_t* to, bool use_filename_from, bool overwrite_existent)
	{
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
		//ECS_TEMP_STRING(complete_to_path, 512);
		//complete_to_path.Copy(ToStream(to));
		//bool is_absolute = function::PathIsAbsolute(complete_to_path);
		//complete_to_path.Add(is_absolute ? ECS_OS_PATH_SEPARATOR : ECS_OS_PATH_SEPARATOR_REL);
		//Stream<wchar_t> from_filename = function::PathFilenameBoth(ToStream(from));
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

		ECS_TEMP_STRING(complete_to_path, 512);
		const wchar_t* to_path = to;
		if (use_filename_from) {
			complete_to_path.Copy(ToStream(to));
			bool is_absolute = function::PathIsAbsolute(complete_to_path);
			complete_to_path.Add(is_absolute ? ECS_OS_PATH_SEPARATOR : ECS_OS_PATH_SEPARATOR_REL);
			Stream<wchar_t> from_filename = function::PathFilenameBoth(ToStream(from));
			complete_to_path.AddStream(from_filename);
			complete_to_path.Add(L'\0');

			to_path = complete_to_path.buffer;
		}

		BOOL success = CopyFile(from, to_path, !overwrite_existent);
		return success;
	}

	bool FileCopy(Stream<wchar_t> from, Stream<wchar_t> to, bool use_filename_from, bool overwrite_existent)
	{
		if (from[from.size] == L'\0' && to[to.size] == L'\0') {
			return FileCopy(from.buffer, to.buffer, use_filename_from, overwrite_existent);
		}
		else {
			ECS_TEMP_STRING(null_from, 512);
			ECS_TEMP_STRING(null_to, 512);
			null_from.Copy(from);
			null_to.Copy(to);

			null_from.Add(L'\0');
			null_to.Add(L'\0');
			return FileCopy(null_from.buffer, null_to.buffer, use_filename_from, overwrite_existent);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool FileCut(const wchar_t* from, const wchar_t* to, bool use_filename_from, bool overwrite_existent)
	{
		bool status = FileCopy(from, to, use_filename_from, overwrite_existent);
		if (!status) {
			return status;
		}

		bool success = RemoveFile(from);
		return success;
	}

	bool FileCut(Stream<wchar_t> from, Stream<wchar_t> to, bool use_filename_from, bool overwrite_existent) {
		if (from[from.size] == L'\0' && to[to.size] == L'\0') {
			return FileCut(from.buffer, to.buffer, use_filename_from, overwrite_existent);
		}
		else {
			ECS_TEMP_STRING(null_from, 512);
			ECS_TEMP_STRING(null_to, 512);
			null_from.Copy(from);
			null_to.Copy(to);

			null_from.Add(L'\0');
			null_to.Add(L'\0');
			return FileCut(null_from.buffer, null_to.buffer, use_filename_from, overwrite_existent);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool FolderCopy(const wchar_t* from, const wchar_t* to)
	{
		return FolderCopy(ToStream(from), ToStream(to));
	}

	bool FolderCopy(Stream<wchar_t> from, Stream<wchar_t> to)
	{
		ECS_TEMP_STRING(null_from, 512);
		ECS_TEMP_STRING(null_to, 512);
		null_from.Copy(from);
		null_from.Add(L'\0');
		null_from.Add(L'\0');

		null_to.Copy(to);
		null_to.Add(L'\0');
		null_to.Add(L'\0');

		SHFILEOPSTRUCT operation_data = { 0 };
		operation_data.hwnd = nullptr;
		operation_data.wFunc = FO_COPY;
		operation_data.fFlags = FOF_NO_UI;
		operation_data.pFrom = null_from.buffer;
		operation_data.pTo = null_to.buffer;
		int status = SHFileOperation(&operation_data);
		return status == 0 && !operation_data.fAnyOperationsAborted;
	}

	// --------------------------------------------------------------------------------------------------

	bool FolderCut(const wchar_t* from, const wchar_t* to)
	{
		return FolderCut(ToStream(from), ToStream(to));
	}

	bool FolderCut(Stream<wchar_t> from, Stream<wchar_t> to)
	{
		bool success = FolderCopy(from, to);
		if (success) {
			return RemoveFolder(from);
		}
		return false;
	}
	
	// --------------------------------------------------------------------------------------------------

	bool RenameFolderOrFile(const wchar_t* path, const wchar_t* new_name) {
		ECS_TEMP_STRING(new_name_stream, 512);
		Stream<wchar_t> folder_parent = function::PathParentBoth(ToStream(path));
		new_name_stream.Copy(folder_parent);
		new_name_stream.Add(ECS_OS_PATH_SEPARATOR);
		new_name_stream.AddStream(ToStream(new_name));
		new_name_stream.AddSafe(L'\0');
		return _wrename(path, new_name_stream.buffer) == 0;
	}

	bool RenameFolderOrFile(Stream<wchar_t> path, Stream<wchar_t> new_name) {
		ECS_TEMP_STRING(new_name_stream, 512);
		Stream<wchar_t> folder_parent = function::PathParentBoth(path);
		new_name_stream.Copy(folder_parent);
		new_name_stream.Add(ECS_OS_PATH_SEPARATOR);
		new_name_stream.AddStream(new_name);
		new_name_stream.AddSafe(L'\0');
		if (path[path.size] == L'\0') {
			return _wrename(path.buffer, new_name_stream.buffer) == 0;
		}
		else {
			ECS_TEMP_STRING(null_folder, 512);
			null_folder.Copy(path);
			null_folder.AddSafe(L'\0');
			return _wrename(null_folder.buffer, new_name_stream.buffer) == 0;
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool ResizeFile(const wchar_t* file, int size)
	{
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_STATUS_FLAGS status = OpenFile(file, &file_handle, ECS_FILE_ACCESS_WRITE_ONLY);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}

		bool success = _chsize(file_handle, size) == 0;
		CloseFile(file_handle);
		return success;
	}

	bool ResizeFile(Stream<wchar_t> file, int size)
	{
		ECS_FORWARD_STREAM_WIDE(file, ResizeFile, size);
	}

	// --------------------------------------------------------------------------------------------------

	bool ChangeFileExtension(const wchar_t* file, const wchar_t* extension) {
		ECS_TEMP_STRING(new_name, 512);
		Stream<wchar_t> original_extension = function::PathExtensionBoth(ToStream(file));
		new_name.Copy(Stream<wchar_t>(file, original_extension.buffer - file));
		new_name.AddStream(ToStream(extension));
		new_name.AddSafe(L'\0');
		return RenameFolderOrFile(file, new_name.buffer);
	}

	bool ChangeFileExtension(Stream<wchar_t> file, Stream<wchar_t> extension) {
		if (file[file.size] == L'\0' && extension[extension.size] == L'\0') {
			return ChangeFileExtension(file.buffer, extension.buffer);
		}
		else {
			ECS_TEMP_STRING(null_file, 512);
			ECS_TEMP_STRING(null_extension, 512);
			null_file.Copy(file);
			null_extension.Copy(extension);

			null_file.Add(L'\0');
			null_extension.Add(L'\0');
			return ChangeFileExtension(null_file.buffer, null_extension.buffer);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool DeleteFolderContents(const wchar_t* path)
	{
		bool success = true;
		auto folder_action = [](const wchar_t* path, void* data) {
			bool* success = (bool*)data;
			bool result = RemoveFolder(path);
			*success = (result == false) * false + (result == true) * true;

			return true;
		};

		auto file_action = [](const wchar_t* path, void* data) {
			bool* success = (bool*)data;
			bool result = RemoveFile(path);
			*success = (result == false) * false + (result == true) * true;

			return true;
		};

		ForEachInDirectory(path, &success, folder_action, file_action);
		return success;
	}

	bool DeleteFolderContents(Stream<wchar_t> path) {
		ECS_FORWARD_STREAM_WIDE(path, DeleteFolderContents);
	}

	// --------------------------------------------------------------------------------------------------

	bool HideFolder(const wchar_t* path)
	{
		return SetFileAttributes(path, FILE_ATTRIBUTE_HIDDEN);
	}

	bool HideFolder(Stream<wchar_t> path)
	{
		ECS_FORWARD_STREAM_WIDE(path, HideFolder);
	}

	// --------------------------------------------------------------------------------------------------

	Stream<void> ReadWholeFile(const wchar_t* path, bool binary, AllocatorPolymorphic allocator)
	{
		if (binary) {
			return ReadWholeFileBinary(path, allocator);
		}
		else {
			return ReadWholeFileText(path, allocator);
		}
	}

	// --------------------------------------------------------------------------------------------------

	Stream<void> ReadWholeFile(Stream<wchar_t> path, bool binary, AllocatorPolymorphic allocator)
	{
		if (binary) {
			return ReadWholeFileBinary(path, allocator);
		}
		else {
			return ReadWholeFileText(path, allocator);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFile(const wchar_t* path)
	{
		struct _stat64 statistics = {};
		int status = _wstat64(path, &statistics);
		if (status == 0) {
			return function::HasFlag(statistics.st_mode, S_IFREG);
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFile(Stream<wchar_t> path)
	{
		ECS_FORWARD_STREAM_WIDE(path, IsFile);
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFolder(const wchar_t* path)
	{
		struct _stat64 statistics = {};
		int status = _wstat64(path, &statistics);
		if (status == 0) {
			return function::HasFlag(statistics.st_mode, S_IFDIR);
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFolder(Stream<wchar_t> path)
	{
		ECS_FORWARD_STREAM_WIDE(path, IsFolder);
	}

	// --------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------

	Stream<void> ReadWholeFileBinary(const wchar_t* path, AllocatorPolymorphic allocator)
	{
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_ACCESS_FLAGS access = ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY;
		ECS_FILE_STATUS_FLAGS status = OpenFile(path, &file_handle, access);
		if (status != ECS_FILE_STATUS_OK) {
			return { nullptr, 0 };
		}

		ScopedFile scoped_file(file_handle);
		size_t file_size = GetFileByteSize(file_handle);
		if (file_size == 0) {
			return  { nullptr, 0 };
		}

		void* allocation = nullptr;
		allocation = AllocateEx(allocator, file_size);
		unsigned int read_bytes = ReadFromFile(file_handle, { allocation, file_size });
		if (read_bytes != file_size) {
			DeallocateEx(allocator, allocation);
			return { nullptr, 0 };
		}

		return { allocation, file_size };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<void> ReadWholeFileBinary(Stream<wchar_t> path, AllocatorPolymorphic allocator)
	{
		ECS_FORWARD_STREAM_WIDE(path, ReadWholeFileBinary, allocator);
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> ReadWholeFileText(const wchar_t* path, AllocatorPolymorphic allocator)
	{
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_ACCESS_FLAGS access = ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_TEXT;
		ECS_FILE_STATUS_FLAGS status = OpenFile(path, &file_handle, access);
		if (status != ECS_FILE_STATUS_OK) {
			return { nullptr, 0 };
		}

		ScopedFile scoped_file(file_handle);
		size_t file_size = GetFileByteSize(file_handle);
		if (file_size == 0) {
			return  { nullptr, 0 };
		}

		void* allocation = nullptr;
		// Add a '\0' at the end of the file in order to help with C functions parsing
		if (allocator.allocator != nullptr) {
			allocation = Allocate(allocator, file_size + 1);
		}
		else {
			allocation = malloc(file_size + 1);
		}
		unsigned int read_bytes = ReadFromFile(file_handle, { allocation, file_size });
		// Text files will return a smaller number of bytes since it will convert line feed carriages
		file_size = read_bytes;
		char* characters = (char*)allocation;
		characters[read_bytes] = '\0';

		return { allocation, file_size };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> ReadWholeFileText(Stream<wchar_t> path, AllocatorPolymorphic allocator)
	{
		ECS_FORWARD_STREAM_WIDE(path, ReadWholeFileText, allocator);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS WriteBufferToFileBinary(const wchar_t* path, Stream<void> buffer, bool append_data)
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

	ECS_FILE_STATUS_FLAGS WriteBufferToFileBinary(Stream<wchar_t> path, Stream<void> buffer, bool append_data)
	{
		ECS_FORWARD_STREAM_WIDE(path, WriteBufferToFileBinary, buffer, append_data);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS WriteBufferToFileText(const wchar_t* path, Stream<void> buffer, bool append_data)
	{
		ECS_FILE_HANDLE handle = 0;
		ECS_FILE_ACCESS_FLAGS access_flags = append_data ? ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_APEND | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL
			: ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL;

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
		ECS_FORWARD_STREAM_WIDE(path, WriteBufferToFileText, buffer, append_data);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS WriteBufferToFile(const wchar_t* path, Stream<void> buffer, bool binary, bool append_data)
	{
		return binary ? WriteBufferToFileBinary(path, buffer, append_data) : WriteBufferToFileText(path, buffer, append_data);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS WriteBufferToFile(Stream<wchar_t> path, Stream<void> buffer, bool binary, bool append_data)
	{
		ECS_FORWARD_STREAM_WIDE(path, WriteBufferToFile, buffer, append_data);
	}

	// --------------------------------------------------------------------------------------------------

	bool ExistsFileOrFolder(const wchar_t* path)
	{
		return _waccess(path, 0) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool ExistsFileOrFolder(Stream<wchar_t> path)
	{
		ECS_FORWARD_STREAM_WIDE(path, ExistsFileOrFolder);
	}

}