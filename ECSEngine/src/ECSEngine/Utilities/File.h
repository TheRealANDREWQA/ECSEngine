#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"

ECS_CONTAINERS;

namespace ECSEngine {

	ECSENGINE_API bool HasSubdirectories(const wchar_t* directory);

	// Might perform a copy to a stack buffer if cannot place a null terminated character
	ECSENGINE_API bool HasSubdirectories(CapacityStream<wchar_t> directory);

	ECSENGINE_API bool ClearFile(const wchar_t* path);

	ECSENGINE_API bool ClearFile(Stream<wchar_t> path);

	// Returns true if the file was removed, false if it doesn't exist
	ECSENGINE_API bool RemoveFile(const wchar_t* file);

	// Returns true if the file was removed, false if it doesn't exist
	ECSENGINE_API bool RemoveFile(Stream<wchar_t> file);

	// Returns true if the folder was removed, false if it doesn't exist; it will destroy internal files and folders
	ECSENGINE_API bool RemoveFolder(const wchar_t* file);

	// Returns true if the folder was removed, false if it doesn't exist; it will destroy internal files and folders
	ECSENGINE_API bool RemoveFolder(Stream<wchar_t> file);

	ECSENGINE_API bool CreateFolder(const wchar_t* path);

	ECSENGINE_API bool CreateFolder(Stream<wchar_t> path);

	ECSENGINE_API bool FileCopy(const wchar_t* from, const wchar_t* to, bool overwrite_existent = false);

	ECSENGINE_API bool FileCopy(Stream<wchar_t> from, Stream<wchar_t> to, bool overwrite_existent = false);

	ECSENGINE_API bool FileCut(const wchar_t* from, const wchar_t* to, bool overwrite_existent = false);

	ECSENGINE_API bool FileCut(Stream<wchar_t> from, Stream<wchar_t> to, bool overwrite_existent = false);

	ECSENGINE_API bool FolderCopy(const wchar_t* from, const wchar_t* to);

	ECSENGINE_API bool FolderCopy(Stream<wchar_t> from, Stream<wchar_t> to);

	ECSENGINE_API bool FolderCut(const wchar_t* from, const wchar_t* to);

	ECSENGINE_API bool FolderCut(Stream<wchar_t> from, Stream<wchar_t> to);

	// New name must only be the directory name, not the fully qualified path
	ECSENGINE_API bool RenameFolder(const wchar_t* path, const wchar_t* new_name);

	// New name must only be the directory name, not the fully qualified path
	ECSENGINE_API bool RenameFolder(Stream<wchar_t> path, Stream<wchar_t> new_name);

	// New name must only be the file name, not the fully qualified path and it will not affect the extension
	ECSENGINE_API bool RenameFile(const wchar_t* path, const wchar_t* new_name);

	// New name must only be the file name, not the fully qualified path and it will not affect the extension
	ECSENGINE_API bool RenameFile(Stream<wchar_t> path, Stream<wchar_t> new_name);

	ECSENGINE_API bool ResizeFile(const wchar_t* path, size_t new_size);

	ECSENGINE_API bool ResizeFile(Stream<wchar_t> path, size_t new_size);

	// The extension must start with a dot; operation applies to the OS file, not for the in memory paths
	ECSENGINE_API bool ChangeFileExtension(const wchar_t* path, const wchar_t* new_extension);

	// The extension must start with a dot; operation applies to the OS file, not for the in memory paths
	ECSENGINE_API bool ChangeFileExtension(Stream<wchar_t> path, Stream<wchar_t> new_extension);

	ECSENGINE_API bool DeleteFolderContents(const wchar_t* path);

	ECSENGINE_API bool DeleteFolderContents(Stream<wchar_t> path);

	ECSENGINE_API bool IsFileWithExtension(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		const wchar_t* ECS_RESTRICT filename = nullptr,
		size_t max_size = 256
	);

	ECSENGINE_API bool IsFileWithExtension(
		containers::Stream<wchar_t> path,
		containers::Stream<wchar_t> extension,
		containers::CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	ECSENGINE_API bool IsFileWithExtensionRecursive(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		const wchar_t* ECS_RESTRICT filename = nullptr,
		size_t max_size = 256
	);

	ECSENGINE_API bool IsFileWithExtensionRecursive(
		containers::Stream<wchar_t> path,
		containers::Stream<wchar_t> extension,
		containers::CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	// the return tells the for loop to terminate early if found something
	using ForEachFolderFunction = bool (*)(const std::filesystem::path& path, void* data);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectoryWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectoryWithExtension(
		Stream<wchar_t> directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectoryRecursiveWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extension,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachFileInDirectoryRecursiveWithExtension(
		Stream<wchar_t> directory,
		Stream<const wchar_t*> extension,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop; files and folders
	ECSENGINE_API void ForEachInDirectory(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop
	ECSENGINE_API void ForEachInDirectory(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop; files and folders
	ECSENGINE_API void ForEachInDirectoryRecursive(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	// Must take as arguments std::filesystem::path and void* and return a bool
	// True to continue the iteration or false to stop; files and folders
	ECSENGINE_API void ForEachInDirectoryRecursive(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	inline bool ExistsFileOrFolder(const wchar_t* path) {
		return std::filesystem::exists(path);
	}

	inline bool ExistsFileOrFolder(containers::Stream<wchar_t> path) {
		return std::filesystem::exists(std::filesystem::path(path.buffer, path.buffer + path.size));
	}

	inline bool IsFile(const wchar_t* path) {
		return std::filesystem::is_regular_file(path);
	}

	inline bool IsFile(containers::Stream<wchar_t> path) {
		return std::filesystem::is_regular_file(std::filesystem::path(path.buffer, path.buffer + path.size));
	}

	inline bool IsFolder(const wchar_t* path) {
		return std::filesystem::is_directory(path);
	}

	inline bool IsFolder(containers::Stream<wchar_t> path) {
		return std::filesystem::is_directory(std::filesystem::path(path.buffer, path.buffer + path.size));
	}

}