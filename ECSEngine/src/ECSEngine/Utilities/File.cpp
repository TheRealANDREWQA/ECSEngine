#include "ecspch.h"
#include "File.h"
#include "Function.h"
#include "FunctionInterfaces.h"
#include "Path.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	bool HasSubdirectories(const wchar_t* directory)
	{
		return HasSubdirectories(ToStream(directory));
	}

	bool HasSubdirectories(CapacityStream<wchar_t> directory)
	{
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_directory()) {
				return true;
			}
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool ClearFile(const wchar_t* path) {
		return ClearFile(ToStream(path));
	}

	bool ClearFile(Stream<wchar_t> path) {
		return ResizeFile(path, 0);
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFile(const wchar_t* file)
	{
		return RemoveFile(ToStream(file));
	}

	bool RemoveFile(Stream<wchar_t> file)
	{
		std::filesystem::path path(file.buffer, file.buffer + file.size);
		return std::filesystem::remove(path);
	}

	// --------------------------------------------------------------------------------------------------

	bool RemoveFolder(const wchar_t* folder) {
		return RemoveFolder(ToStream(folder));
	}

	bool RemoveFolder(Stream<wchar_t> folder) {
		std::filesystem::path path(folder.buffer, folder.buffer + folder.size);
		return std::filesystem::remove_all(path) > 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool CreateFolder(const wchar_t* folder) {
		return CreateFolder(ToStream(folder));
	}


	bool CreateFolder(Stream<wchar_t> folder) {
		std::filesystem::path path(folder.buffer, folder.buffer + folder.size);
		return std::filesystem::create_directory(path);
	}

	// --------------------------------------------------------------------------------------------------

	bool FileCopy(const wchar_t* from, const wchar_t* to, bool overwrite_existent)
	{
		return FileCopy(ToStream(from), ToStream(to), overwrite_existent);
	}

	bool FileCopy(Stream<wchar_t> from, Stream<wchar_t> to, bool overwrite_existent)
	{
		std::error_code error_code;
		Stream<wchar_t> filename = function::PathFilename(from);

		std::filesystem::path to_path(to.buffer, to.buffer + to.size);
		to_path.append(filename.buffer, filename.buffer + filename.size);

		std::filesystem::copy_options option = std::filesystem::copy_options::none;
		if (overwrite_existent) {
			option = std::filesystem::copy_options::overwrite_existing;
		}
		bool success = std::filesystem::copy_file(
			std::filesystem::path(from.buffer, from.buffer + from.size), 
			to_path,
			option,
			error_code
		);
		std::string message = error_code.message();
		return success && error_code.value() == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool FileCut(const wchar_t* from, const wchar_t* to, bool overwrite_existent)
	{
		return FileCut(ToStream(from), ToStream(to), overwrite_existent);
	}

	bool FileCut(Stream<wchar_t> from, Stream<wchar_t> to, bool overwrite_existent) {
		bool success = FileCopy(from, to, overwrite_existent);
		if (success) {
			return RemoveFile(from);
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool FolderCopy(const wchar_t* from, const wchar_t* to)
	{
		return FolderCopy(ToStream(from), ToStream(to));
	}

	bool FolderCopy(Stream<wchar_t> from, Stream<wchar_t> to)
	{
		std::error_code error_code;
		std::filesystem::copy_options copy_options = std::filesystem::copy_options::recursive;
		std::filesystem::path to_path(to.buffer, to.buffer + to.size);

		Path filename = function::PathFilename(from);
		to_path.append(filename.buffer, filename.buffer + filename.size);
		std::filesystem::copy(std::filesystem::path(from.buffer, from.buffer + from.size), to_path, copy_options, error_code);
		return error_code.value() == 0;
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

	bool RenameFolder(const wchar_t* folder, const wchar_t* new_name) {
		return RenameFolder(ToStream(folder), ToStream(new_name));
	}

	bool RenameFolder(Stream<wchar_t> folder, Stream<wchar_t> new_name) {
		std::filesystem::path path(folder.buffer, folder.buffer + folder.size);
		std::filesystem::path new_path = path.parent_path();
		new_path.append(new_name.buffer, new_name.buffer + new_name.size);

		std::error_code error_code;
		std::filesystem::rename(path, new_path, error_code);
		return error_code.value() == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool RenameFile(const wchar_t* file, const wchar_t* new_name) {
		return RenameFile(ToStream(file), ToStream(new_name));
	}

	bool RenameFile(Stream<wchar_t> file, Stream<wchar_t> new_name) {
		std::filesystem::path path(file.buffer, file.buffer + file.size);
		std::filesystem::path extension = path.extension();

		std::error_code error_code;
		auto old_path = path;
		path.replace_filename(std::filesystem::path(new_name.buffer, new_name.buffer + new_name.size).concat(extension.c_str()));
		std::filesystem::rename(old_path, path, error_code);
		return error_code.value() == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool ResizeFile(const wchar_t* path, size_t new_size) {
		return ResizeFile(ToStream(path), new_size);
	}

	bool ResizeFile(Stream<wchar_t> path, size_t new_size) {
		std::error_code error_code;
		std::filesystem::resize_file(std::filesystem::path(path.buffer, path.buffer + path.size), new_size, error_code);
		return error_code.value() == 0;
	}

	// --------------------------------------------------------------------------------------------------

	bool ChangeFileExtension(const wchar_t* file, const wchar_t* extension) {
		return ChangeFileExtension(ToStream(file), ToStream(extension));
	}

	bool ChangeFileExtension(Stream<wchar_t> file, Stream<wchar_t> extension) {
		std::filesystem::path path(file.buffer, file.buffer + file.size);
		path.replace_extension({ extension.buffer, extension.buffer + extension.size });
		return true;
	}

	// --------------------------------------------------------------------------------------------------

	bool DeleteFolderContents(const wchar_t* path)
	{
		return DeleteFolderContents(ToStream(path));
	}

	bool DeleteFolderContents(Stream<wchar_t> path) {
		std::filesystem::path std_path(path.buffer, path.buffer + path.size);
		std::filesystem::directory_entry entry(std_path);
		if (!entry.exists()) {
			return false;
		}

		bool success = true;
		auto folder_action = [](const std::filesystem::path& path, void* data) {
			bool* success = (bool*)data;
			bool result = RemoveFolder(path.c_str());
			*success = (result == false) * false + (result == true) * true;

			return true;
		};

		auto file_action = [](const std::filesystem::path& path, void* data) {
			bool* success = (bool*)data;
			bool result = RemoveFile(path.c_str());
			*success = (result == false) * false + (result == true) * true;

			return true;
		};

		ForEachInDirectory(path, &success, folder_action, file_action);
		return success;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtension(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		const wchar_t* ECS_RESTRICT filename,
		size_t max_size
	)
	{
		return IsFileWithExtension(ToStream(path), ToStream(extension), { filename, 0, (unsigned int)max_size });
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtension(Stream<wchar_t> path, Stream<wchar_t> extension, CapacityStream<wchar_t> filename) {
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(path.buffer, path.buffer + path.size))) {
			if (entry.is_regular_file()) {
				auto std_extension = entry.path().extension();
				Stream<wchar_t> current_extension = ToStream(std_extension.c_str());
				if (function::CompareStrings(current_extension, extension)) {
					if (filename.buffer != nullptr) {
						filename.Copy(ToStream(entry.path().c_str()));
					}
					return true;
				}
			}
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtensionRecursive(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		const wchar_t* ECS_RESTRICT filename,
		size_t max_size
	) {
		return IsFileWithExtensionRecursive(ToStream(path), ToStream(extension), { filename, 0, (unsigned int)max_size });
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtensionRecursive(
		containers::Stream<wchar_t> path,
		containers::Stream<wchar_t> extension,
		containers::CapacityStream<wchar_t> filename
	) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(path.buffer, path.buffer + path.size))) {
			if (entry.is_regular_file()) {
				auto std_extension = entry.path().extension();
				Stream<wchar_t> current_extension = ToStream(std_extension.c_str());
				if (function::CompareStrings(current_extension, extension)) {
					if (filename.buffer != nullptr) {
						filename.Copy(ToStream(entry.path().c_str()));
					}
					return true;
				}
			}
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachFileInDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor) {
		ForEachFileInDirectory(ToStream(directory), data, functor);
	}

	void ForEachFileInDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_regular_file()) {
				if (!functor(entry.path(), data)) {
					return;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachFileInDirectoryWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor
	) {
		ForEachFileInDirectoryWithExtension(ToStream(directory), extensions, data, functor);
	}

	void ForEachFileInDirectoryWithExtension(Stream<wchar_t> directory, Stream<const wchar_t*> extensions, void* data, ForEachFolderFunction functor)
	{
		ECS_ASSERT(extensions.size < 128);
		Stream<wchar_t> _stream_extensions[128];
		for (size_t index = 0; index < extensions.size; index++) {
			_stream_extensions[index] = ToStream(extensions[index]);
		}
		Stream<Stream<wchar_t>> stream_extensions(_stream_extensions, extensions.size);

		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_regular_file()) {
				std::filesystem::path _path = entry.path();
				if (_path.has_extension()) {
					auto string = _path.extension().wstring();
					const wchar_t* extension = string.c_str();
					Stream<wchar_t> stream_extension = ToStream(extension);
					if (function::IsStringInStream(stream_extension, stream_extensions) != -1) {
						if (!functor(_path, data)) {
							return;
						}
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachFileInDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor) {
		ForEachFileInDirectoryRecursive(ToStream(directory), data, functor);
	}

	void ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_regular_file()) {
				if (!functor(entry.path(), data)) {
					return;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachFileInDirectoryRecursiveWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor
	) {
		ForEachFileInDirectoryRecursiveWithExtension(ToStream(directory), extensions, data, functor);
	}

	void ForEachFileInDirectoryRecursiveWithExtension(Stream<wchar_t> directory, Stream<const wchar_t*> extensions, void* data, ForEachFolderFunction functor)
	{
		ECS_ASSERT(extensions.size < 128);
		Stream<wchar_t> _stream_extensions[128];
		for (size_t index = 0; index < extensions.size; index++) {
			_stream_extensions[index] = ToStream(extensions[index]);
		}
		Stream<Stream<wchar_t>> stream_extensions(_stream_extensions, extensions.size);

		for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_regular_file()) {
				std::filesystem::path _path = entry.path();
				if (_path.has_extension()) {
					const wchar_t* extension = _path.extension().c_str();
					Stream<wchar_t> stream_extension = ToStream(extension);
					if (function::IsStringInStream(stream_extension, stream_extensions) != -1) {
						if (!functor(_path, data)) {
							return;
						}
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor) {
		ForEachDirectory(ToStream(directory), data, functor);
	}

	void ForEachDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_directory()) {
				if (!functor(entry.path(), data)) {
					return;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor) {
		ForEachDirectoryRecursive(ToStream(directory), data, functor);
	}

	void ForEachDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_directory()) {
				if (!functor(entry.path(), data)) {
					return;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachInDirectory(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	)
	{
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory))) {
			if (entry.is_directory()) {
				folder_functor(entry.path(), data);
			}
			else if (entry.is_regular_file()) {
				file_functor(entry.path(), data);
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachInDirectory(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	)
	{
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_directory()) {
				if (!folder_functor(entry.path(), data)) {
					return;
				}
			}
			else if (entry.is_regular_file()) {
				if (!file_functor(entry.path(), data)) {
					return;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachInDirectoryRecursive(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	) {
		ForEachInDirectoryRecursive(ToStream(directory), data, folder_functor, file_functor);
	}

	// --------------------------------------------------------------------------------------------------

	void ForEachInDirectoryRecursive(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(directory.buffer, directory.buffer + directory.size))) {
			if (entry.is_directory()) {
				if (folder_functor(entry.path(), data)) {
					return;
				}
			}
			else if (entry.is_regular_file()) {
				if (file_functor(entry.path(), data)) {
					return;
				}
			}
		}
	}

}