#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Utilities/StackScope.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>

namespace ECSEngine {

	enum ECS_FILE_STATUS_FLAGS : int {
		// The call succeeded
		ECS_FILE_STATUS_OK = 0,
		// Tried to open a read-only file for writing, file's sharing mode does not allow the specified operations, or the given path is a directory
		ECS_FILE_STATUS_ACCESS_DENIED = EACCES,
		// File already exists when trying to create a file that specified exclusion
		ECS_FILE_STATUS_ALREADY_EXISTS = EEXIST,
		// Create flags or access flag combination invalid
		ECS_FILE_STATUS_INVALID_ARGUMENTS = EINVAL,
		// Too many file handles opened
		ECS_FILE_STATUS_TOO_MANY_OPENED_FILES = EMFILE,
		// File or path not found
		ECS_FILE_STATUS_INCORRECT_PATH = ENOENT,
		// Reading from a file failed
		ECS_FILE_STATUS_READING_FILE_FAILED = 3,
		// Writing to a file failed
		ECS_FILE_STATUS_WRITING_FILE_FAILED = 4,
		// Unidentified error
		ECS_FILE_STATUS_ERROR = 1
	};

	enum ECS_FILE_CREATE_FLAGS : int {
		// The file will be deleted when the last file descriptor is closed
		ECS_FILE_CREATE_TEMPORARY = O_TEMPORARY,
		// Avoids flushing the contents to disk
		ECS_FILE_CREATE_AVOID_FLUSH_DISK = _O_SHORT_LIVED,
		// Fails if already exists
		ECS_FILE_CREATE_FAIL_IF_EXISTS = O_EXCL,
		// The file can only be read
		ECS_FILE_CREATE_READ_ONLY = S_IREAD,
		// The file can only be written
		ECS_FILE_CREATE_WRITE_ONLY = S_IWRITE,
		// The file can be read and written
		ECS_FILE_CREATE_READ_WRITE = S_IREAD | S_IWRITE
	};

	enum ECS_FILE_ACCESS_FLAGS : int {
		// Moves the file pointer to the end of the file before every write operation
		ECS_FILE_ACCESS_APEND = O_APPEND,
		ECS_FILE_ACCESS_BINARY = O_BINARY,
		ECS_FILE_ACCESS_PREVENT_SHARING = O_NOINHERIT,
		// Specifies that caching is optimized for, but not restricted to, random access from disk
		ECS_FILE_ACCESS_OPTIMIZE_RANDOM = O_RANDOM,
		// Specifies that caching is optimized for, but not restricted to, sequential access from disk
		ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL = O_SEQUENTIAL,
		// Cannot be used at the same time with WRITE_ONLY or READ_WRITE
		ECS_FILE_ACCESS_READ_ONLY = O_RDONLY,
		// Cannot be used at the same time with READ_ONLY or READ_WRITE
		ECS_FILE_ACCESS_WRITE_ONLY = O_WRONLY,
		// Cannot be used at the same time with READ_ONLY or WRITE_ONLY
		ECS_FILE_ACCESS_READ_WRITE = O_RDWR,
		// Opens a file and truncates it to zero length; the file must have write permission.
		// Cannot be specified with READ_ONLY. Note: The flag destroys the contents of the specified file if already exists.
		ECS_FILE_ACCESS_TRUNCATE_FILE = O_TRUNC,
		ECS_FILE_ACCESS_TEXT = O_TEXT,

		// Combination of multiple flags
		ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE = O_WRONLY | O_BINARY | O_TRUNC,
		ECS_FILE_ACCESS_READ_BINARY_SEQUENTIAL = O_RDONLY | O_BINARY | O_SEQUENTIAL
	};

	typedef int ECS_FILE_HANDLE;

	ECS_ENUM_BITWISE_OPERATIONS(ECS_FILE_STATUS_FLAGS);
	ECS_ENUM_BITWISE_OPERATIONS(ECS_FILE_CREATE_FLAGS);
	ECS_ENUM_BITWISE_OPERATIONS(ECS_FILE_ACCESS_FLAGS);

	// At least one of ECS_FILE_CREATE_READ_ONLY, ECS_FILE_CREATE_WRITE_ONLY or ECS_FILE_CREATE_READ_WRITE must be specified
	// The file handle can be optionally be specified for further operations
	// If the error message is specified, a formatted error message will be set to describe the error
	ECSENGINE_API ECS_FILE_STATUS_FLAGS FileCreate(
		Stream<wchar_t> path,
		ECS_FILE_HANDLE* file_handle,
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY,
		ECS_FILE_CREATE_FLAGS create_flags = ECS_FILE_CREATE_READ_WRITE,
		CapacityStream<char>* error_message = nullptr
	);

	// Opens a handle to a file using the specified modes
	// The file handle can be optionally be specified for further operations
	// If the error message is specified, a formatted error message will be set to describe the error
	ECSENGINE_API ECS_FILE_STATUS_FLAGS OpenFile(
		Stream<wchar_t> path,
		ECS_FILE_HANDLE* file_handle,
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_READ_ONLY,
		CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API bool CreateEmptyFile(Stream<wchar_t> path, ECS_FILE_CREATE_FLAGS create_flags = ECS_FILE_CREATE_READ_WRITE, CapacityStream<char>* error_message = nullptr);

	// Closes a file
	ECSENGINE_API bool CloseFile(ECS_FILE_HANDLE handle);

	// Closes a file with buffering - if any bytes are left in the buffer than these will be commited to the file
	ECSENGINE_API bool CloseFile(ECS_FILE_HANDLE handle, CapacityStream<void> buffering);

	// Unbuffered write to a file 
	// Returns the amount of bytes written, -1 signals an error
	ECSENGINE_API size_t WriteToFile(ECS_FILE_HANDLE handle, Stream<void> data);

	// Unbuffered write to a file; reports if a failure has occured
	ECSENGINE_API bool WriteFile(ECS_FILE_HANDLE handle, Stream<void> data);

	// Buffered write to a file
	// Returns the amount of bytes written, -1 signals an error
	ECSENGINE_API size_t WriteToFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering);

	// Buffered write to a file
	// Reports if an error occured
	ECSENGINE_API bool WriteFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering);

	// Unbuffered read from a file. Returns the amount of bytes actually read from the file 
	// - it might be less for text files or if there are fewer bytes left, -1 signals an error
	ECSENGINE_API size_t ReadFromFile(ECS_FILE_HANDLE handle, Stream<void> data);

	// Unbuffered read from a file. Reports if a failure has occured. This returns true even when the file doesn't have
	// Data.size bytes, but the read was successful
	ECSENGINE_API bool ReadFile(ECS_FILE_HANDLE handle, Stream<void> data);

	// Unbuffered read from a file. Reports if a failure has occured. This returns true only if the read size coincides
	// With the data.size
	ECSENGINE_API bool ReadFileExact(ECS_FILE_HANDLE handle, Stream<void> data);

	// Buffered read from a file. Returns the amount of bytes actually read from the file 
	// - it might be less for text files or if there are fewer bytes left, -1 signals an error
	ECSENGINE_API size_t ReadFromFile(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering);

	// Buffered read from a file. Reports if a failure has occured. This returns true only if the read size coincides
	// With the data.size
	ECSENGINE_API bool ReadFileExact(ECS_FILE_HANDLE handle, Stream<void> data, CapacityStream<void>& buffering);

	// A value of -1 means error
	ECSENGINE_API size_t GetFileCursor(ECS_FILE_HANDLE handle);

	enum ECS_FILE_SEEK {
		ECS_FILE_SEEK_BEG = SEEK_SET,
		ECS_FILE_SEEK_CURRENT = SEEK_CUR,
		ECS_FILE_SEEK_END = SEEK_END
	};

	// Returns the offset from the beginning of the file
	// A value of -1 means error
	ECSENGINE_API size_t SetFileCursor(ECS_FILE_HANDLE handle, int64_t position, ECS_FILE_SEEK seek_point);

	// Reports if an error occured
	ECSENGINE_API bool SetFileCursorBool(ECS_FILE_HANDLE handle, int64_t position, ECS_FILE_SEEK seek_point);

	// Returns whether or not the file end has been reached
	ECSENGINE_API bool FileEnd(ECS_FILE_HANDLE handle);

	ECSENGINE_API bool FlushFileToDisk(ECS_FILE_HANDLE handle);

	// If it fails, it returns 0
	// It will be forwarded to const wchar_t* variant
	ECSENGINE_API size_t GetFileByteSize(Stream<wchar_t> path);

	// If it fails, it returns 0
	ECSENGINE_API size_t GetFileByteSize(ECS_FILE_HANDLE file_handle);

	// A pointer null means I don't care; returns whether or not succeeded
	// Works for directories too
	ECSENGINE_API bool GetFileTimes(
		ECS_FILE_HANDLE file_handle,
		CapacityStream<wchar_t>* creation_time = nullptr,
		CapacityStream<wchar_t>* access_time = nullptr,
		CapacityStream<wchar_t>* last_write_time = nullptr
	);

	// A pointer null means I don't care; returns whether or not succeeded
	// Works for directories too
	ECSENGINE_API bool GetFileTimes(
		ECS_FILE_HANDLE file_handle,
		CapacityStream<char>* creation_time = nullptr,
		CapacityStream<char>* access_time = nullptr,
		CapacityStream<char>* last_write_time = nullptr
	);

	// A pointer null means I don't care; returns whether or not succeeded
	// Works for directories too
	ECSENGINE_API bool GetFileTimes(
		ECS_FILE_HANDLE file_handle,
		size_t* creation_time = nullptr,
		size_t* access_time = nullptr,
		size_t* last_write_time = nullptr
	);

	// A pointer null means I don't care; returns whether or not succeeded
	// Works for directories too
	ECSENGINE_API bool GetRelativeFileTimes(
		ECS_FILE_HANDLE file_handle,
		CapacityStream<wchar_t>* creation_time = nullptr,
		CapacityStream<wchar_t>* access_time = nullptr,
		CapacityStream<wchar_t>* last_write_time = nullptr
	);

	// A pointer null means I don't care; returns whether or not succeeded
	// Works for directories too
	ECSENGINE_API bool GetRelativeFileTimes(
		ECS_FILE_HANDLE file_handle,
		CapacityStream<char>* creation_time = nullptr,
		CapacityStream<char>* access_time = nullptr,
		CapacityStream<char>* last_write_time = nullptr
	);

	// A pointer null means I don't care; returns whether or not succeeded
	// Works for directories too
	ECSENGINE_API bool GetRelativeFileTimes(
		ECS_FILE_HANDLE file_handle,
		size_t* creation_time = nullptr,
		size_t* access_time = nullptr,
		size_t* last_write_time = nullptr
	);

	ECSENGINE_API size_t GetFileLastWrite(ECS_FILE_HANDLE file_handle);

	ECSENGINE_API bool HasSubdirectories(Stream<wchar_t> directory);

	ECSENGINE_API bool ClearFile(Stream<wchar_t> path);

	ECSENGINE_API bool ClearFile(ECS_FILE_HANDLE file_handle);

	// Returns true if the file was removed, false if it doesn't exist
	ECSENGINE_API bool RemoveFile(Stream<wchar_t> file);

	// Returns true if the folder was removed, false if it doesn't exist; it will destroy internal files and folders
	ECSENGINE_API bool RemoveFolder(Stream<wchar_t> file);

	ECSENGINE_API bool CreateFolder(Stream<wchar_t> path);

	ECSENGINE_API bool FileCopy(Stream<wchar_t> from, Stream<wchar_t> to, bool use_filename_from = true, bool overwrite_existent = false);

	// Equivalent to FileCopy and then deleting the old file
	ECSENGINE_API bool FileCut(Stream<wchar_t> from, Stream<wchar_t> to, bool use_filename_from = true, bool overwrite_existent = false);

	// From and to must be absolute paths
	ECSENGINE_API bool FolderCopy(Stream<wchar_t> from, Stream<wchar_t> to);

	ECSENGINE_API bool FolderCut(Stream<wchar_t> from, Stream<wchar_t> to);

	// New name must only be the directory name, not the fully qualified path
	ECSENGINE_API bool RenameFolder(Stream<wchar_t> path, Stream<wchar_t> new_name);

	// New name must only be the filename, not the fully qualified path
	ECSENGINE_API bool RenameFile(Stream<wchar_t> path, Stream<wchar_t> new_name);
	
	// This version takes the name as an absolute path
	ECSENGINE_API bool RenameFileAbsolute(Stream<wchar_t> path, Stream<wchar_t> new_absolute_path);

	// New name must only be the directory/filename name, not the fully qualified path
	ECSENGINE_API bool RenameFileOrFolder(Stream<wchar_t> path, Stream<wchar_t> new_name);

	ECSENGINE_API bool ResizeFile(Stream<wchar_t> file, size_t size);

	// The file handle must be opened for write
	ECSENGINE_API bool ResizeFile(ECS_FILE_HANDLE file_handle, size_t size);

	// The extension must start with a dot; operation applies to the OS file, not for the in memory paths
	ECSENGINE_API bool ChangeFileExtension(Stream<wchar_t> path, Stream<wchar_t> new_extension);

	ECSENGINE_API bool DeleteFolderContents(Stream<wchar_t> path);

	// The folder must exist before calling this function
	ECSENGINE_API bool HideFolder(Stream<wchar_t> path);

	// Reads the whole contents of a file and returns the data into a stream allocated from the given allocator 
	// or from Malloc if the allocator is nullptr
	// If the read or the opening fails, it will return { nullptr, 0 }
	// The bool* parameter can be used to discern when the return value is { nullptr, 0 } due to the file
	// Being empty rather than an error
	ECSENGINE_API Stream<void> ReadWholeFile(Stream<wchar_t> path, bool binary, AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR, bool* empty_file = nullptr);

	// Reads the whole contents of a file and returns the data into a stream allocated from the given allocator
	// or from Malloc if the allocator is nullptr
	// If the read or the opening fails, it will return { nullptr, 0 }
	// The bool* parameter can be used to discern when the return value is { nullptr, 0 } due to the file
	// Being empty rather than an error
	ECSENGINE_API Stream<void> ReadWholeFileBinary(Stream<wchar_t> path, AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR, bool* empty_file = nullptr);

	// Reads the whole contents of a file and returns the data into a stream allocated from the given allocator
	// or from Malloc if the allocator is nullptr
	// If the read or the opening fails, it will return { nullptr, 0 }
	// The bool* parameter can be used to discern when the return value is { nullptr, 0 } due to the file
	// Being empty rather than an error
	ECSENGINE_API Stream<char> ReadWholeFileText(Stream<wchar_t> path, AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR, bool* empty_file = nullptr);

	// Writes the buffer to that file. It will create the file if it doesn't exist. Can specify whether or not to append
	ECSENGINE_API ECS_FILE_STATUS_FLAGS WriteBufferToFileBinary(Stream<wchar_t> path, Stream<void> buffer, bool append_data = false);

	// Writes the buffer to that file. It will create the file if it doesn't exist. Can specify whether or not to append
	ECSENGINE_API ECS_FILE_STATUS_FLAGS WriteBufferToFileText(Stream<wchar_t> path, Stream<void> buffer, bool append_data = false);

	// Writes the buffer to that file. It will create the file if it doesn't exist. Can specify whether or not to append
	ECSENGINE_API ECS_FILE_STATUS_FLAGS WriteBufferToFile(Stream<wchar_t> path, Stream<void> buffer, bool binary, bool append_data = false);
	
	ECSENGINE_API bool ExistsFileOrFolder(Stream<wchar_t> path);

	ECSENGINE_API bool IsFile(Stream<wchar_t> path);

	ECSENGINE_API bool IsFolder(Stream<wchar_t> path);

	struct FileScopeDeleter {
		void operator() () const {
			if (handle != 0 && handle != -1) {
				CloseFile(handle);
			}
		}

		ECS_FILE_HANDLE handle;
	};

	typedef StackScope<FileScopeDeleter> ScopedFile;

}