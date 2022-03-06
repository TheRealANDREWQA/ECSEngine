#include "ecspch.h"
#include "ForEachFiles.h"
#include "File.h"
#include "Path.h"
#include "FunctionInterfaces.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "Function.h"

#define ECS_FORWARD_STREAM_WIDE(stream, function, ...) if (stream[stream.size] == L'\0') {\
												return function(stream.buffer, __VA_ARGS__); \
											} \
											else { \
												wchar_t _null_path[512]; \
												CapacityStream<wchar_t> null_path(_null_path, 0, 512); \
												null_path.Copy(stream); \
												null_path[stream.size] = L'\0'; \
												return function(null_path.buffer, __VA_ARGS__); \
											}

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtension(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		wchar_t* ECS_RESTRICT filename,
		size_t max_size
	)
	{
		struct ForData {
			wchar_t* filename;
			bool has_file;
		};

		ForData for_data = { filename, false };
		ForEachFileInDirectoryWithExtension(path, { &extension, 1 }, &for_data, [](const wchar_t* path, void* _data) {
			ForData* for_data = (ForData*)_data;
			if (for_data->filename != nullptr) {
				wcscpy(for_data->filename, path);
			}
			for_data->has_file = true;
			return false;
			});

		return for_data.has_file;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtension(Stream<wchar_t> path, Stream<wchar_t> extension, CapacityStream<wchar_t> filename) {
		struct ForData {
			CapacityStream<wchar_t> filename;
			bool has_file;
		};

		ForData for_data = { filename, false };
		ForEachFileInDirectoryWithExtension(path, { &extension, 1 }, &for_data, [](const wchar_t* path, void* _data) {
			ForData* for_data = (ForData*)_data;
			if (for_data->filename.buffer != nullptr) {
				for_data->filename.Copy(ToStream(path));
			}
			for_data->has_file = true;
			return false;
			});

		return for_data.has_file;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtensionRecursive(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		wchar_t* ECS_RESTRICT filename,
		size_t max_size
	) {
		struct ForData {
			wchar_t* filename;
			bool has_file;
		};

		ForData for_data = { filename, false };
		ForEachFileInDirectoryRecursiveWithExtension(path, { &extension, 1 }, &for_data, [](const wchar_t* path, void* _data) {
			ForData* for_data = (ForData*)_data;
			if (for_data->filename != nullptr) {
				wcscpy(for_data->filename, path);
			}
			for_data->has_file = true;
			return false;
			});

		return for_data.has_file;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtensionRecursive(
		Stream<wchar_t> path,
		Stream<wchar_t> extension,
		CapacityStream<wchar_t> filename
	) {
		struct ForData {
			CapacityStream<wchar_t> filename;
			bool has_file;
		};

		ForData for_data = { filename, false };
		ForEachFileInDirectoryRecursiveWithExtension(path, { &extension, 1 }, &for_data, [](const wchar_t* path, void* _data) {
			ForData* for_data = (ForData*)_data;
			if (for_data->filename.buffer != nullptr) {
				for_data->filename.Copy(ToStream(path));
			}
			for_data->has_file = true;
			return false;
		});

		return for_data.has_file;
	}

	// --------------------------------------------------------------------------------------------------

	void SetSearchAllStringForEachFile(CapacityStream<wchar_t> string, const wchar_t* directory) {
		string.Copy(ToStream(directory));
		wchar_t separator = function::PathIsRelative(string) ? ECS_OS_PATH_SEPARATOR_REL : ECS_OS_PATH_SEPARATOR;
		string.Add(separator);
		string.Add(L'*');
		string.Add(L'\0');
		string.AssertCapacity();
	};

	const size_t MAX_SIMULTANEOUS_DIRECTORIES = 128;
	const size_t STACK_ALLOCATION = 16 * ECS_KB;

	void FindNextReleaseHandle(intptr_t handle) {
		_findclose(handle);
	}

	template<typename Function>
	bool ForEachInDirectoryInternal(const wchar_t* directory, Function&& function) {
		ECS_TEMP_STRING(temp_string, 512);
		struct _wfinddata64_t find_data = {};
		SetSearchAllStringForEachFile(temp_string, directory);

		intptr_t find_handle = _wfindfirst64(temp_string.buffer, &find_data);
		if (find_handle == -1) {
			if (errno == ENOENT) {
				return false;
			}
			return true;
		}

		StackScope<intptr_t, FindNextReleaseHandle> scoped_handle(find_handle);

		size_t current_directory_size = wcslen(directory) + 1;
		bool has_subdirectories = true;
		while (has_subdirectories) {
			// Make sure it is not a "." or a ".." directory - hidden one
			if (find_data.name[0] != L'.') {
				// find_data.name only contains the filename - the relative path
				temp_string.size = current_directory_size;
				temp_string.AddStream(ToStream(find_data.name));
				temp_string.AddSafe(L'\0');
				if (!function(temp_string.buffer, find_data.attrib)) {
					return true;
				}
			}
			int next = _wfindnext64(find_handle, &find_data);
			if (next == -1) {
				if (errno == EINVAL || errno == ENOMEM) {
					return false;
				}
			}
			has_subdirectories = next == 0;
		}

		return true;
	}

	template<typename Function>
	bool ForEachInDirectoryRecursiveInternalBreadth(const wchar_t* directory, Function&& function) {
		const wchar_t** _subdirectories = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_SIMULTANEOUS_DIRECTORIES);
		CapacityStream<const wchar_t*> subdirectories(_subdirectories, 0, MAX_SIMULTANEOUS_DIRECTORIES);

		void* allocation = ECS_STACK_ALLOC(MultipoolAllocator::MemoryOf(MAX_SIMULTANEOUS_DIRECTORIES, STACK_ALLOCATION));
		MultipoolAllocator allocator((unsigned char*)allocation, STACK_ALLOCATION, MAX_SIMULTANEOUS_DIRECTORIES);

		Stream<wchar_t> allocator_directory = function::StringCopy(&allocator, directory);
		subdirectories.Add(allocator_directory.buffer);

		ECS_TEMP_STRING(temp_string, 512);
		struct _wfinddata64_t find_data = {};

		while (subdirectories.size > 0) {
			const wchar_t* current_directory = subdirectories[subdirectories.size - 1];
			subdirectories.size--;
			size_t current_directory_size = wcslen(current_directory) + 1;
			SetSearchAllStringForEachFile(temp_string, current_directory);

			intptr_t find_handle = _wfindfirst64(temp_string.buffer, &find_data);
			if (find_handle == -1) {
				if (errno == ENOENT) {
					return false;
				}
				return true;
			}
			StackScope<intptr_t, FindNextReleaseHandle> scoped_handle(find_handle);

			bool has_subdirectories = true;
			while (has_subdirectories) {
				// Make sure it is not a "." or a ".." directory - current or
				if (find_data.name[0] != L'.') {
					// find_data.name only contains the filename - the relative path
					temp_string.size = current_directory_size;
					temp_string.AddStream(ToStream(find_data.name));
					temp_string.AddSafe(L'\0');
					if (!function(temp_string.buffer, find_data.attrib)) {
						return true;
					}

					if (find_data.attrib & _A_SUBDIR) {
						Stream<wchar_t> copied_string = function::StringCopy(&allocator, temp_string);
						subdirectories.AddSafe(copied_string.buffer);
					}
				}
				int next = _wfindnext64(find_handle, &find_data);
				if (next == -1) {
					if (errno == EINVAL || errno == ENOMEM) {
						return false;
					}
				}
				has_subdirectories = next == 0;
			}

			allocator.Deallocate(current_directory);
		}

		return true;
	}

	template<typename Function>
	bool ForEachInDirectoryRecursiveInternalDepth(const wchar_t* directory, Function&& function) {
		const wchar_t** _subdirectories = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_SIMULTANEOUS_DIRECTORIES);
		intptr_t* handles = (intptr_t*)ECS_STACK_ALLOC(sizeof(intptr_t) * MAX_SIMULTANEOUS_DIRECTORIES);
		memset(handles, 0, sizeof(intptr_t) * MAX_SIMULTANEOUS_DIRECTORIES);

		CapacityStream<const wchar_t*> subdirectories(_subdirectories, 0, MAX_SIMULTANEOUS_DIRECTORIES);

		void* allocation = ECS_STACK_ALLOC(MultipoolAllocator::MemoryOf(MAX_SIMULTANEOUS_DIRECTORIES, STACK_ALLOCATION));
		MultipoolAllocator allocator((unsigned char*)allocation, STACK_ALLOCATION, MAX_SIMULTANEOUS_DIRECTORIES);

		Stream<wchar_t> allocator_directory = function::StringCopy(&allocator, directory);
		subdirectories.Add(allocator_directory.buffer);

		ECS_TEMP_STRING(temp_string, 512);
		struct _wfinddata64_t find_data = {};

		while (subdirectories.size > 0) {
			subdirectories.size--;
			const wchar_t* current_directory = subdirectories[subdirectories.size];

			size_t current_directory_size = wcslen(current_directory) + 1;
			SetSearchAllStringForEachFile(temp_string, current_directory);

			intptr_t find_handle;
			if (handles[subdirectories.size] == 0) {
				handles[subdirectories.size] = _wfindfirst64(temp_string.buffer, &find_data);
				if (find_handle == -1) {
					if (errno == ENOENT) {
						return false;
					}
					return true;
				}
			}
			find_handle = handles[subdirectories.size];

			int next = _wfindnext64(find_handle, &find_data);
			if (next == -1) {
				if (errno == EINVAL || errno == ENOMEM) {
					return false;
				}
				_findclose(find_handle);
				allocator.Deallocate(current_directory);
				handles[subdirectories.size] = 0;
			}
			else {
				subdirectories.size++;
			}

			// Make sure it is not a "." or a ".." directory
			if (find_data.name[0] != L'.') {
				// find_data.name only contains the filename - the relative path
				temp_string.size = current_directory_size;
				temp_string.AddStream(ToStream(find_data.name));
				temp_string.AddSafe(L'\0');
				if (!function(temp_string.buffer, find_data.attrib)) {
					return true;
				}

				if (find_data.attrib & _A_SUBDIR) {
					Stream<wchar_t> copied_string = function::StringCopy(&allocator, temp_string);
					subdirectories.AddSafe(copied_string.buffer);
				}
			}
		}

		return true;
	}

	template<typename Function>
	bool ForEachInDirectoryRecursiveInternal(const wchar_t* directory, Function&& function, bool depth_traversal) {
		if (!depth_traversal) {
			return ForEachInDirectoryRecursiveInternalBreadth(directory, function);
		}
		else {
			return ForEachInDirectoryRecursiveInternalDepth(directory, function);
		}
	}

	bool ForEachFileInDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor) {
		return ForEachInDirectoryInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				return functor(path, data);
			}
			return true;
		});
	}

	bool ForEachFileInDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		ECS_FORWARD_STREAM_WIDE(directory, ForEachFileInDirectory, data, functor);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachFileInDirectoryWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor
	) {
		Stream<wchar_t>* stream_extensions = (Stream<wchar_t>*)ECS_STACK_ALLOC(sizeof(Stream<wchar_t>) * extensions.size);
		for (size_t index = 0; index < extensions.size; index++) {
			stream_extensions[index] = ToStream(extensions[index]);
		}
		Stream<Stream<wchar_t>> stream_of_extensions(stream_extensions, extensions.size);

		return ForEachInDirectoryInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				Stream<wchar_t> extension = function::PathExtension(ToStream(path));
				if (function::FindString(extension, stream_of_extensions) != -1) {
					return functor(path, data);
				}
			}
			return true;
		});
	}

	bool ForEachFileInDirectoryWithExtension(Stream<wchar_t> directory, Stream<const wchar_t*> extensions, void* data, ForEachFolderFunction functor)
	{
		ECS_FORWARD_STREAM_WIDE(directory, ForEachFileInDirectoryWithExtension, extensions, data, functor);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachFileInDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor, bool depth_traversal) {
		return ForEachInDirectoryRecursiveInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				return functor(path, data);
			}
			return true;
		}, depth_traversal);
	}

	bool ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal)
	{
		ECS_FORWARD_STREAM_WIDE(directory, ForEachFileInDirectoryRecursive, data, functor, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachFileInDirectoryRecursiveWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor,
		bool depth_traversal
	) {
		Stream<wchar_t>* stream_extensions = (Stream<wchar_t>*)ECS_STACK_ALLOC(sizeof(Stream<wchar_t>) * extensions.size);
		for (size_t index = 0; index < extensions.size; index++) {
			stream_extensions[index] = ToStream(extensions[index]);
		}
		Stream<Stream<wchar_t>> stream_of_extensions(stream_extensions, extensions.size);

		return ForEachInDirectoryRecursiveInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				Stream<wchar_t> extension = function::PathExtension(ToStream(path));
				if (function::FindString(extension, stream_of_extensions) != -1) {
					return functor(path, data);
				}
			}
			return true;
		}, depth_traversal);
	}

	bool ForEachFileInDirectoryRecursiveWithExtension(Stream<wchar_t> directory, Stream<const wchar_t*> extensions, void* data, ForEachFolderFunction functor, bool depth_traversal)
	{
		ECS_FORWARD_STREAM_WIDE(directory, ForEachFileInDirectoryRecursiveWithExtension, extensions, data, functor, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor) {
		return ForEachInDirectoryInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_SUBDIR) {
				return functor(path, data);
			}
			return true;
		});
	}

	bool ForEachDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		ECS_FORWARD_STREAM_WIDE(directory, ForEachDirectory, data, functor);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor, bool depth_traversal) {
		return ForEachInDirectoryRecursiveInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_SUBDIR) {
				return functor(path, data);
			}
			return true;
		}, depth_traversal);
	}

	bool ForEachDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal)
	{
		ECS_FORWARD_STREAM_WIDE(directory, ForEachDirectoryRecursive, data, functor, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachInDirectory(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	)
	{
		return ForEachInDirectoryInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				return file_functor(path, data);
			}
			else if (attribute & _A_SUBDIR) {
				return folder_functor(path, data);
			}
			return true;
			});
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachInDirectory(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	)
	{
		ECS_FORWARD_STREAM_WIDE(directory, ForEachInDirectory, data, folder_functor, file_functor);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachInDirectoryRecursive(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor,
		bool depth_traversal
	) {
		return ForEachInDirectoryRecursiveInternal(directory, [=](const wchar_t* path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				return file_functor(path, data);
			}
			else if (attribute & _A_SUBDIR) {
				return folder_functor(path, data);
			}
			return true;
		}, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachInDirectoryRecursive(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor,
		bool depth_traversal
	) {
		ECS_FORWARD_STREAM_WIDE(directory, ForEachInDirectoryRecursive, data, folder_functor, file_functor, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename Function>
	bool GetDirectoriesOrFilesImplementation(
		const wchar_t* root, 
		AllocatorPolymorphic allocator,
		CapacityStream<const wchar_t*>& paths, 
		bool batched_allocation,
		bool depth_traversal,
		Function&& function
	) {
		// Use the allocator to copy the paths - for the batched allocation only if the allocation type is single threaded
		if (!batched_allocation || allocator.allocation_type == AllocationType::SingleThreaded) {
			struct ForData {
				AllocatorPolymorphic allocator;
				CapacityStream<const wchar_t*>* paths;
			};
			ForData for_data = { allocator, &paths };

			bool status = function(root, &for_data, [](const wchar_t* path, void* _data) {
				ForData* data = (ForData*)_data;

				size_t path_size = wcslen(path) + 1;
				void* allocation = Allocate(data->allocator, sizeof(wchar_t) * path_size, alignof(wchar_t));
				memcpy(allocation, path, sizeof(wchar_t) * path_size);
				data->paths->AddSafe((const wchar_t*)allocation);
				return true;
			}, depth_traversal);

			// If something failed - deallocate the individual allocations
			if (!status) {
				for (size_t index = 0; index < paths.size; index++) {
					Deallocate(allocator, paths[index]);
				}
				paths.size = 0;
				return false;
			}

			if (batched_allocation) {
				unsigned short* sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * paths.size);

				size_t total_size = 0;
				for (size_t index = 0; index < paths.size; index++) {
					sizes[index] = wcslen(paths[index]) + 1;
					total_size += sizes[index];
				}

				void* allocation = Allocate(allocator, sizeof(wchar_t) * total_size, alignof(wchar_t));
				uintptr_t buffer = (uintptr_t)allocation;
				for (size_t index = 0; index < paths.size; index++) {
					memcpy((void*)buffer, paths[index], sizeof(wchar_t) * sizes[index]);
					Deallocate(allocator, paths[index]);
					paths[index] = (const wchar_t*)buffer;

					buffer += sizeof(wchar_t) * sizes[index];
				}
			}
			return true;
		}
		// Batched allocation and multithreaded
		else {
			size_t total_size = 0;
			bool status = function(root, &total_size, [](const wchar_t* path, void* _data) {
				size_t* data = (size_t*)_data;
				size_t path_size = wcslen(path) + 1;
				*data += path_size;

				return true;
			}, depth_traversal);

			if (!status) {
				return false;
			}

			void* allocation = Allocate(allocator, total_size);
			void* initial_allocation = allocation;
			struct BatchedCopyStringsData {
				CapacityStream<const wchar_t*>* paths;
				void** allocation;
			};
			BatchedCopyStringsData copy_data = { &paths, &allocation };
			status = function(root, &copy_data, [](const wchar_t* path, void* _data) {
				BatchedCopyStringsData* data = (BatchedCopyStringsData*)_data;
				void* buffer = *data->allocation;
				size_t path_size = wcslen(path) + 1;
				memcpy(buffer, path, sizeof(wchar_t) * path_size);
				*data->allocation = function::OffsetPointer(buffer, sizeof(wchar_t) * path_size);

				return true;
			}, depth_traversal);

			if (!status) {
				Deallocate(allocator, initial_allocation);
				paths.size = 0;
				return false;
			}

			ECS_ASSERT(function::OffsetPointer(initial_allocation, total_size) >= allocation);
			return true;
		}
	}

	template<typename Function>
	bool GetFilesWithExtensionImplementation(
		const wchar_t* root, 
		AllocatorPolymorphic allocator, 
		CapacityStream<const wchar_t*>& paths, 
		Stream<const wchar_t*> extensions,
		bool batched_allocation,
		bool depth_traversal,
		Function&& function
	) {
		// Use the allocator to copy the paths - for the batched allocation only if the allocation type is single threaded
		if (!batched_allocation || allocator.allocation_type == AllocationType::SingleThreaded) {
			struct ForData {
				AllocatorPolymorphic allocator;
				CapacityStream<const wchar_t*>* paths;
			};
			ForData for_data = { allocator, &paths };

			bool status = function(root, extensions, &for_data, [](const wchar_t* path, void* _data) {
				ForData* data = (ForData*)_data;

				size_t path_size = wcslen(path) + 1;
				void* allocation = Allocate(data->allocator, sizeof(wchar_t) * path_size, alignof(wchar_t));
				memcpy(allocation, path, sizeof(wchar_t) * path_size);
				data->paths->AddSafe((const wchar_t*)allocation);
				return true;
			}, depth_traversal);

			// If something failed - deallocate the individual allocations
			if (!status) {
				for (size_t index = 0; index < paths.size; index++) {
					Deallocate(allocator, paths[index]);
				}
				paths.size = 0;
				return false;
			}

			if (batched_allocation) {
				unsigned short* sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * paths.size);

				size_t total_size = 0;
				for (size_t index = 0; index < paths.size; index++) {
					sizes[index] = wcslen(paths[index]) + 1;
					total_size += sizes[index];
				}

				void* allocation = Allocate(allocator, sizeof(wchar_t) * total_size, alignof(wchar_t));
				uintptr_t buffer = (uintptr_t)allocation;
				for (size_t index = 0; index < paths.size; index++) {
					memcpy((void*)buffer, paths[index], sizeof(wchar_t) * sizes[index]);
					Deallocate(allocator, paths[index]);
					paths[index] = (const wchar_t*)buffer;

					buffer += sizeof(wchar_t) * sizes[index];
				}
			}
			return true;
		}
		// Batched allocation and multithreaded
		else {
			size_t total_size = 0;
			bool status = function(root, extensions, &total_size, [](const wchar_t* path, void* _data) {
				size_t* data = (size_t*)_data;
				size_t path_size = wcslen(path) + 1;
				*data += path_size;

				return true;
			}, depth_traversal);

			if (!status) {
				return false;
			}

			void* allocation = Allocate(allocator, total_size);
			void* initial_allocation = allocation;
			struct BatchedCopyStringsData {
				CapacityStream<const wchar_t*>* paths;
				void** allocation;
			};
			BatchedCopyStringsData copy_data = { &paths, &allocation };
			status = function(root, extensions, &copy_data, [](const wchar_t* path, void* _data) {
				BatchedCopyStringsData* data = (BatchedCopyStringsData*)_data;
				void* buffer = *data->allocation;
				size_t path_size = wcslen(path) + 1;
				memcpy(buffer, path, sizeof(wchar_t) * path_size);
				*data->allocation = function::OffsetPointer(buffer, sizeof(wchar_t) * path_size);

				return true;
			}, depth_traversal);

			if (!status) {
				Deallocate(allocator, initial_allocation);
				paths.size = 0;
				return false;
			}

			ECS_ASSERT(function::OffsetPointer(initial_allocation, total_size) >= allocation);
			return true;
		}
	}

	bool GetDirectories(const wchar_t* root, AllocatorPolymorphic allocator, CapacityStream<const wchar_t*>& directories_paths, bool batched_allocation)
	{
		return GetDirectoriesOrFilesImplementation(root, allocator, directories_paths, batched_allocation, false, [](const wchar_t* directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachDirectory(directory, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoriesRecursive(
		const wchar_t* root,
		AllocatorPolymorphic allocator,
		CapacityStream<const wchar_t*>& directories_paths,
		bool batched_allocation, 
		bool depth_traversal
	)
	{
		return GetDirectoriesOrFilesImplementation(root, allocator, directories_paths, batched_allocation, depth_traversal, [](const wchar_t* directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachDirectoryRecursive(directory, data, functor, depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFiles(const wchar_t* directory, AllocatorPolymorphic allocator, CapacityStream<const wchar_t*>& file_paths, bool batched_allocation)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, batched_allocation, false, [](const wchar_t* directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachFileInDirectory(directory, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFilesRecursive(const wchar_t* directory, AllocatorPolymorphic allocator, CapacityStream<const wchar_t*>& file_paths, bool batched_allocation, bool depth_traversal)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, batched_allocation, depth_traversal, [](const wchar_t* directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachFileInDirectoryRecursive(directory, data, functor, depth_traversal);
		});
	}

	bool GetDirectoryFilesWithExtension(const wchar_t* directory, AllocatorPolymorphic allocator, CapacityStream<const wchar_t*>& file_paths, Stream<const wchar_t*> extensions, bool batched_allocation)
	{
		return GetFilesWithExtensionImplementation(directory, allocator, file_paths, extensions, batched_allocation, false, [](
			const wchar_t* directory, 
			Stream<const wchar_t*> extensions, 
			void* data,
			ForEachFolderFunction functor, 
			bool depth_traversal
			) {
				return ForEachFileInDirectoryWithExtension(directory, extensions, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFileWithExtensionRecursive(
		const wchar_t* directory,
		AllocatorPolymorphic allocator, 
		CapacityStream<const wchar_t*>& file_paths,
		Stream<const wchar_t*> extensions,
		bool batched_allocation,
		bool depth_traversal
	)
	{
		return GetFilesWithExtensionImplementation(directory, allocator, file_paths, extensions, batched_allocation, false, [](
			const wchar_t* directory,
			Stream<const wchar_t*> extensions,
			void* data,
			ForEachFolderFunction functor,
			bool depth_traversal
			) {
				return ForEachFileInDirectoryRecursiveWithExtension(directory, extensions, data, functor, depth_traversal);
		});
	}

}