#include "ecspch.h"
#include "File.h"
#include "Function.h"
#include "FunctionInterfaces.h"
#include "Path.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "FunctionTemplates.h"
#include "ForEachFiles.h"

ECS_CONTAINERS;

#define FORWARD_STREAM(stream, function, ...) if (stream[stream.size] == L'\0') {\
												return function(stream.buffer, __VA_ARGS__); \
											} \
											else { \
												wchar_t _null_path[512]; \
												containers::CapacityStream<wchar_t> null_path(_null_path, 0, 512); \
												null_path.Copy(stream); \
												null_path[stream.size] = L'\0'; \
												return function(null_path.buffer, __VA_ARGS__); \
											}


namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	// For file operations
	template<typename char_type>
	void SetErrorMessage(CapacityStream<char>* error_message, int error, const char_type* path) {
		if (error_message != nullptr) {
			ECS_TEMP_ASCII_STRING(temp_string, 256);
			switch (error) {
			case ECS_FILE_STATUS_ACCESS_DENIED:
				ECS_FORMAT_STRING(temp_string, "Access to {0} was not granted. Possible causes: tried to open a read-only file for writing,"
					"\nfile's sharing mode does not allow the specified operations, or the given path is a directory", path);
				break;
			case ECS_FILE_STATUS_ALREADY_EXISTS:
				ECS_FORMAT_STRING(temp_string, "File {0} already exists.", path);
				break;
			case ECS_FILE_STATUS_INCORRECT_PATH:
				ECS_FORMAT_STRING(temp_string, "Incorrect path {0}.", path);
				break;
			case ECS_FILE_STATUS_INVALID_ARGUMENTS:
				ECS_FORMAT_STRING(temp_string, "Invalid combination of arguments for {0}.", path);
				break;
			case ECS_FILE_STATUS_TOO_MANY_OPENED_FILES:
				ECS_FORMAT_STRING(temp_string, "Too many opened files when trying to create {0}.", path);
				break;
			case ECS_FILE_STATUS_READING_FILE_FAILED:
				ECS_FORMAT_STRING(temp_string, "Reading from file {0} failed.", path);
				break;
			case ECS_FILE_STATUS_WRITING_FILE_FAILED:
				ECS_FORMAT_STRING(temp_string, "Writing to file {0} failed.", path);
				break;
			case ECS_FILE_STATUS_ERROR:
				ECS_FORMAT_STRING(temp_string, "An unidentifier error occured for {0}.", path);
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
		FORWARD_STREAM(path, FileCreate, file_handle, access_flags, create_flags, error_message);
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
		FORWARD_STREAM(path, OpenFile, file_handle, access_flags, error_message)
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
		FORWARD_STREAM(path, GetFileByteSize);
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
		FORWARD_STREAM(directory, HasSubdirectories);
	}

	// --------------------------------------------------------------------------------------------------

	bool ClearFile(const wchar_t* path) {
		ECS_FILE_HANDLE file_handle = 0;
		bool success = OpenFile(path, &file_handle, ECS_FILE_ACCESS_TRUNCATE_FILE | ECS_FILE_ACCESS_READ_ONLY) == ECS_FILE_STATUS_OK;
		CloseFile(file_handle);
		return success;
	}

	bool ClearFile(Stream<wchar_t> path) {
		FORWARD_STREAM(path, ClearFile);
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFile(const wchar_t* file)
	{
		return _wremove(file) == 0;
	}

	bool RemoveFile(Stream<wchar_t> file)
	{
		FORWARD_STREAM(file, RemoveFile);
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFolder(const wchar_t* folder) {
		// Delete folder contents
		DeleteFolderContents(folder);

		// The folder must be empty for this function
		return _wrmdir(folder) == 0;
	}

	bool RemoveFolder(Stream<wchar_t> folder) {
		FORWARD_STREAM(folder, RemoveFolder);
	}

	// --------------------------------------------------------------------------------------------------

	bool CreateFolder(const wchar_t* folder) {
		return _wmkdir(folder) == 0;
	}


	bool CreateFolder(Stream<wchar_t> folder) {
		FORWARD_STREAM(folder, CreateFolder);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS FileCopy(const wchar_t* from, const wchar_t* to, bool overwrite_existent, CapacityStream<char>* error_message)
	{
		// Open file in binary mode using POSIX api's
		ECS_FILE_HANDLE read_handle = 0;
		ECS_FILE_STATUS_FLAGS read_status = OpenFile(from, &read_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY);
		if (read_status != ECS_FILE_STATUS_OK) {
			SetErrorMessage(error_message, read_status, from);
			return read_status;
		}
		ScopedFile scoped_read(read_handle);

		// Get the file size of the file to be copied
		size_t file_size = GetFileByteSize(read_handle);
		if (file_size == 0) {
			SetErrorMessage(error_message, ECS_FILE_STATUS_ERROR, from);
			return ECS_FILE_STATUS_ERROR;
		}

		// Append the filename (the stem and the extension) to the to path
		ECS_TEMP_STRING(complete_to_path, 512);
		complete_to_path.Copy(ToStream(to));
		bool is_absolute = function::PathIsAbsolute(complete_to_path);
		complete_to_path.Add(is_absolute ? ECS_OS_PATH_SEPARATOR : ECS_OS_PATH_SEPARATOR_REL);
		Stream<wchar_t> from_filename = function::PathFilenameBoth(ToStream(from));
		complete_to_path.AddStream(from_filename);
		complete_to_path.Add(L'\0');
		
		ECS_FILE_HANDLE write_handle = 0;
		ECS_FILE_CREATE_FLAGS create_flags = ECS_FILE_CREATE_READ_WRITE;
		create_flags |= overwrite_existent ? (ECS_FILE_CREATE_FLAGS)0 : ECS_FILE_CREATE_FAIL_IF_EXISTS;
		ECS_FILE_STATUS_FLAGS write_status = FileCreate(complete_to_path.buffer, &write_handle, ECS_FILE_ACCESS_WRITE_ONLY 
			| ECS_FILE_ACCESS_TRUNCATE_FILE | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY, create_flags);
		if (write_status != ECS_FILE_STATUS_OK) {	
			SetErrorMessage(error_message, write_status, from_filename.buffer);
			return write_status;
		}
		ScopedFile scoped_write(write_handle);

		// Use malloc to read all the bytes from that file and copy them over to the other file
		void* buffer = ECS_MALLOCA(file_size);
		ScopedMalloca malloca(buffer);

		unsigned int read_bytes = ReadFromFile(read_handle, { buffer, file_size });
		if (read_bytes == -1) {
			SetErrorMessage(error_message, ECS_FILE_STATUS_READING_FILE_FAILED, from);
			return ECS_FILE_STATUS_READING_FILE_FAILED;
		}

		// Write the bytes now
		unsigned int written_bytes = WriteToFile(write_handle, { buffer, read_bytes });
		if (written_bytes != read_bytes) {
			SetErrorMessage(error_message, ECS_FILE_STATUS_WRITING_FILE_FAILED, from_filename.buffer);
			return ECS_FILE_STATUS_WRITING_FILE_FAILED;
		}

		return ECS_FILE_STATUS_OK;
	}

	ECS_FILE_STATUS_FLAGS FileCopy(Stream<wchar_t> from, Stream<wchar_t> to, bool overwrite_existent, CapacityStream<char>* error_message)
	{
		if (from[from.size] == L'\0' && to[to.size] == L'\0') {
			return FileCopy(from.buffer, to.buffer, overwrite_existent, error_message);
		}
		else {
			ECS_TEMP_STRING(null_from, 512);
			ECS_TEMP_STRING(null_to, 512);
			null_from.Copy(from);
			null_to.Copy(to);

			null_from.Add(L'\0');
			null_to.Add(L'\0');
			return FileCopy(null_from.buffer, null_to.buffer, overwrite_existent, error_message);
		}
	}

	// --------------------------------------------------------------------------------------------------

	ECS_FILE_STATUS_FLAGS FileCut(const wchar_t* from, const wchar_t* to, bool overwrite_existent, CapacityStream<char>* error_message)
	{
		ECS_FILE_STATUS_FLAGS status = FileCopy(from, to, overwrite_existent, error_message);
		if (status != ECS_FILE_STATUS_OK) {
			return status;
		}

		bool success = RemoveFile(from);
		if (success) {
			return ECS_FILE_STATUS_OK;
		}
		else {
			SetErrorMessage(error_message, ECS_FILE_STATUS_ERROR, from);
			return ECS_FILE_STATUS_ERROR;
		}
	}

	ECS_FILE_STATUS_FLAGS FileCut(Stream<wchar_t> from, Stream<wchar_t> to, bool overwrite_existent, CapacityStream<char>* error_message) {
		if (from[from.size] == L'\0' && to[to.size] == L'\0') {
			return FileCut(from.buffer, to.buffer, overwrite_existent, error_message);
		}
		else {
			ECS_TEMP_STRING(null_from, 512);
			ECS_TEMP_STRING(null_to, 512);
			null_from.Copy(from);
			null_to.Copy(to);

			null_from.Add(L'\0');
			null_to.Add(L'\0');
			return FileCut(null_from.buffer, null_to.buffer, overwrite_existent, error_message);
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
		null_to.Add(ECS_OS_PATH_SEPARATOR);
		null_to.Add(L'*');
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
		FORWARD_STREAM(file, ResizeFile, size);
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
		FORWARD_STREAM(path, DeleteFolderContents);
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

	bool IsFile(containers::Stream<wchar_t> path)
	{
		FORWARD_STREAM(path, IsFile);
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

	bool IsFolder(containers::Stream<wchar_t> path)
	{
		FORWARD_STREAM(path, IsFolder);
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
		FORWARD_STREAM(path, ReadWholeFileBinary, allocator);
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
		if (allocator.allocator != nullptr) {
			allocation = Allocate(allocator, file_size);
		}
		else {
			allocation = malloc(file_size);
		}
		unsigned int read_bytes = ReadFromFile(file_handle, { allocation, file_size });
		// Text files will return a smaller number of bytes since it will convert line feed carriages
		file_size = read_bytes;

		return { allocation, file_size };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> ReadWholeFileText(Stream<wchar_t> path, AllocatorPolymorphic allocator)
	{
		FORWARD_STREAM(path, ReadWholeFileText, allocator);
	}

	// --------------------------------------------------------------------------------------------------

	bool ExistsFileOrFolder(const wchar_t* path)
	{
		return _waccess(path, 0) == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool ExistsFileOrFolder(containers::Stream<wchar_t> path)
	{
		FORWARD_STREAM(path, ExistsFileOrFolder);
	}

}