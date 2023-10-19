#include "ecspch.h"
#include "ForEachFiles.h"
#include "File.h"
#include "Path.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Allocators/MultipoolAllocator.h"
#include "StringUtilities.h"
#include "PointerUtilities.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	bool IsFileWithExtension(Stream<wchar_t> path, Stream<wchar_t> extension, CapacityStream<wchar_t> filename) {
		struct ForData {
			CapacityStream<wchar_t> filename;
			bool has_file;
		};

		ForData for_data = { filename, false };
		ForEachFileInDirectoryWithExtension(path, { &extension, 1 }, &for_data, [](Stream<wchar_t> path, void* _data) {
			ForData* for_data = (ForData*)_data;
			if (for_data->filename.buffer != nullptr) {
				for_data->filename.CopyOther((path));
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
		ForEachFileInDirectoryRecursiveWithExtension(path, { &extension, 1 }, &for_data, [](Stream<wchar_t> path, void* _data) {
			ForData* for_data = (ForData*)_data;
			if (for_data->filename.buffer != nullptr) {
				for_data->filename.CopyOther((path));
			}
			for_data->has_file = true;
			return false;
		});

		return for_data.has_file;
	}

	// --------------------------------------------------------------------------------------------------

	static void SetSearchAllStringForEachFile(CapacityStream<wchar_t>& string, Stream<wchar_t> directory) {
		string.CopyOther(directory);
		wchar_t separator = PathIsRelative(string) ? ECS_OS_PATH_SEPARATOR_REL : ECS_OS_PATH_SEPARATOR;
		string.Add(separator);
		string.Add(L'*');
		string.Add(L'\0');
		string.AssertCapacity();
	};

	const size_t MAX_SIMULTANEOUS_DIRECTORIES = 128;
	const size_t STACK_ALLOCATION = 16 * ECS_KB;

	struct FindNextReleaseHandle {
		void operator() () const {
			_findclose(handle);
		}

		intptr_t handle;
	};

	template<typename Function>
	static bool ForEachInDirectoryInternal(Stream<wchar_t> directory, Function&& function) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, temp_string, 512);
		struct _wfinddata64_t find_data = {};
		SetSearchAllStringForEachFile(temp_string, directory);

		intptr_t find_handle = _wfindfirst64(temp_string.buffer, &find_data);
		if (find_handle == -1) {
			if (errno == ENOENT) {
				return false;
			}
			return true;
		}

		StackScope<FindNextReleaseHandle> scoped_handle({ find_handle });

		size_t current_directory_size = directory.size + 1;
		bool has_subdirectories = true;
		while (has_subdirectories) {
			// Make sure it is not a "." or a ".." directory - hidden one
			if (find_data.name[0] != L'.') {
				// find_data.name only contains the filename - the relative path
				temp_string.size = current_directory_size;
				temp_string.AddStream((find_data.name));
				temp_string[temp_string.size] = L'\0';
				if (!function(temp_string, find_data.attrib)) {
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
	static bool ForEachInDirectoryRecursiveInternalBreadth(Stream<wchar_t> directory, Function&& function) {
		const wchar_t** _subdirectories = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_SIMULTANEOUS_DIRECTORIES);
		CapacityStream<const wchar_t*> subdirectories(_subdirectories, 0, MAX_SIMULTANEOUS_DIRECTORIES);

		void* allocation = ECS_STACK_ALLOC(MultipoolAllocator::MemoryOf(MAX_SIMULTANEOUS_DIRECTORIES, STACK_ALLOCATION));
		MultipoolAllocator allocator((unsigned char*)allocation, STACK_ALLOCATION, MAX_SIMULTANEOUS_DIRECTORIES);

		Stream<wchar_t> allocator_directory = StringCopy(GetAllocatorPolymorphic(&allocator), directory);
		subdirectories.Add(allocator_directory.buffer);

		ECS_STACK_CAPACITY_STREAM(wchar_t, temp_string, 512);
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
			StackScope<FindNextReleaseHandle> scoped_handle({ find_handle });

			bool has_subdirectories = true;
			while (has_subdirectories) {
				// Make sure it is not a "." or a ".." directory - current or
				if (find_data.name[0] != L'.') {
					// find_data.name only contains the filename - the relative path
					temp_string.size = current_directory_size;
					temp_string.AddStream((find_data.name));
					temp_string[temp_string.size] = L'\0';
					if (!function(temp_string, find_data.attrib)) {
						return true;
					}

					if (find_data.attrib & _A_SUBDIR) {
						Stream<wchar_t> copied_string = StringCopy(GetAllocatorPolymorphic(&allocator), temp_string);
						subdirectories.AddAssert(copied_string.buffer);
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
	static bool ForEachInDirectoryRecursiveInternalDepth(Stream<wchar_t> directory, Function&& function) {
		const wchar_t** _subdirectories = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_SIMULTANEOUS_DIRECTORIES);
		intptr_t* handles = (intptr_t*)ECS_STACK_ALLOC(sizeof(intptr_t) * MAX_SIMULTANEOUS_DIRECTORIES);
		memset(handles, 0, sizeof(intptr_t) * MAX_SIMULTANEOUS_DIRECTORIES);

		CapacityStream<const wchar_t*> subdirectories(_subdirectories, 0, MAX_SIMULTANEOUS_DIRECTORIES);

		void* allocation = ECS_STACK_ALLOC(MultipoolAllocator::MemoryOf(MAX_SIMULTANEOUS_DIRECTORIES, STACK_ALLOCATION));
		MultipoolAllocator allocator((unsigned char*)allocation, STACK_ALLOCATION, MAX_SIMULTANEOUS_DIRECTORIES);

		Stream<wchar_t> allocator_directory = StringCopy(GetAllocatorPolymorphic(&allocator), directory);
		subdirectories.Add(allocator_directory.buffer);

		ECS_STACK_CAPACITY_STREAM(wchar_t, temp_string, 512);
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
				temp_string.AddStream(find_data.name);
				temp_string[temp_string.size] = L'\0';
				if (!function(temp_string, find_data.attrib)) {
					return true;
				}

				if (find_data.attrib & _A_SUBDIR) {
					Stream<wchar_t> copied_string = StringCopy(GetAllocatorPolymorphic(&allocator), temp_string);
					subdirectories.AddAssert(copied_string.buffer);
				}
			}
		}

		return true;
	}

	template<typename Function>
	static bool ForEachInDirectoryRecursiveInternal(Stream<wchar_t> directory, Function&& function, bool depth_traversal) {
		if (!depth_traversal) {
			return ForEachInDirectoryRecursiveInternalBreadth(directory, function);
		}
		else {
			return ForEachInDirectoryRecursiveInternalDepth(directory, function);
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachFileInDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		return ForEachInDirectoryInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				return functor(path, data);
			}
			return true;
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachFileInDirectoryWithExtension(Stream<wchar_t> directory, Stream<Stream<wchar_t>> extensions, void* data, ForEachFolderFunction functor)
	{
		return ForEachInDirectoryInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				Stream<wchar_t> extension = PathExtension(path);
				if (FindString(extension, extensions) != -1) {
					return functor(path, data);
				}
			}
			return true;
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal)
	{
		return ForEachInDirectoryRecursiveInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				return functor(path, data);
			}
			return true;
		}, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachFileInDirectoryRecursiveWithExtension(Stream<wchar_t> directory, Stream<Stream<wchar_t>> extensions, void* data, ForEachFolderFunction functor, bool depth_traversal)
	{
		return ForEachInDirectoryRecursiveInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
			if (attribute & _A_ARCH) {
				Stream<wchar_t> extension = PathExtension(path);
				if (FindString(extension, extensions) != -1) {
					return functor(path, data);
				}
			}
			return true;
		}, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor)
	{
		return ForEachInDirectoryInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
			if (attribute & _A_SUBDIR) {
				return functor(path, data);
			}
			return true;
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal)
	{
		return ForEachInDirectoryRecursiveInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
			if (attribute & _A_SUBDIR) {
				return functor(path, data);
			}
			return true;
		}, depth_traversal);
	}

	// --------------------------------------------------------------------------------------------------

	bool ForEachInDirectory(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	)
	{
		return ForEachInDirectoryInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
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

	bool ForEachInDirectoryRecursive(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor,
		bool depth_traversal
	) {
		return ForEachInDirectoryRecursiveInternal(directory, [=](Stream<wchar_t> path, unsigned int attribute) {
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

	template<typename Function>
	static bool GetDirectoriesOrFilesImplementation(
		Stream<wchar_t> root, 
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>> paths,
		GetDirectoriesOrFilesOptions options,
		Function&& function
	) {
		// Use the allocator to copy the paths - for the batched allocation only if the allocation type is single threaded
		if (!options.batched_allocation || allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			struct ForData {
				AllocatorPolymorphic allocator;
				AdditionStream<Stream<wchar_t>> paths;
				Stream<wchar_t> relative_root;
			};
			ForData for_data = { allocator, paths, options.relative_root };

			bool status = function(&for_data, [](Stream<wchar_t> path, void* _data) {
				ForData* data = (ForData*)_data;

				if (data->relative_root.size > 0) {
					path = PathRelativeToAbsolute(path, data->relative_root);
				}

				Stream<wchar_t> allocated_path = StringCopy(data->allocator, path);
				data->paths.Add(allocated_path);
				return true;
			});

			// If something failed - deallocate the individual allocations
			if (!status) {
				for (size_t index = 0; index < paths.Size(); index++) {
					Deallocate(allocator, paths[index].buffer);
				}
				paths.SetSize(0);
				return false;
			}

			if (options.batched_allocation) {
				size_t total_size = 0;
				for (size_t index = 0; index < paths.Size(); index++) {
					total_size += paths[index].size;
				}

				void* allocation = Allocate(allocator, sizeof(wchar_t) * total_size, alignof(wchar_t));
				uintptr_t buffer = (uintptr_t)allocation;
				for (size_t index = 0; index < paths.Size(); index++) {
					memcpy((void*)buffer, paths[index].buffer, sizeof(wchar_t) * paths[index].size);
					Deallocate(allocator, paths[index].buffer);
					paths[index].buffer = (wchar_t*)buffer;

					buffer += sizeof(wchar_t) * paths[index].size;
				}
			}
			return true;
		}
		// Batched allocation and multithreaded
		else {
			struct DetermineSizeData {
				size_t total_size;
				Stream<wchar_t> relative_root;
			};

			DetermineSizeData determine_size;
			determine_size.total_size = 0;
			determine_size.relative_root = options.relative_root;
			bool status = function(&determine_size, [](Stream<wchar_t> path, void* _data) {
				DetermineSizeData* data = (DetermineSizeData*)_data;

				size_t path_size = path.size;
				if (data->relative_root.size > 0) {
					path_size = PathRelativeToAbsolute(path, data->relative_root).size;
				}

				data->total_size += path_size;
				return true;
			});

			if (!status) {
				return false;
			}

			void* allocation = Allocate(allocator, determine_size.total_size);
			void* initial_allocation = allocation;

			struct BatchedCopyStringsData {
				Stream<wchar_t> relative_root;
				AdditionStream<Stream<wchar_t>> paths;
				void** allocation;
			};
			BatchedCopyStringsData copy_data = { options.relative_root, paths, &allocation };
			status = function(&copy_data, [](Stream<wchar_t> path, void* _data) {
				BatchedCopyStringsData* data = (BatchedCopyStringsData*)_data;
				void* buffer = *data->allocation;

				if (data->relative_root.size > 0) {
					path = PathRelativeToAbsolute(path, data->relative_root);
				}

				data->paths.Add(Stream<wchar_t>(buffer, path.size));
				
				// Increase the path size with 1 to include the '\0'
				path.CopyTo(buffer);
				*data->allocation = OffsetPointer(buffer, sizeof(wchar_t) * path.size);

				return true;
			});

			if (!status) {
				Deallocate(allocator, initial_allocation);
				paths.SetSize(0);
				return false;
			}

			ECS_ASSERT(OffsetPointer(initial_allocation, determine_size.total_size) >= allocation);
			return true;
		}
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectories(Stream<wchar_t> root, AllocatorPolymorphic allocator, AdditionStream<Stream<wchar_t>> directories_paths, GetDirectoriesOrFilesOptions options)
	{
		return GetDirectoriesOrFilesImplementation(root, allocator, directories_paths, options, [=](void* data, ForEachFolderFunction functor) {
				return ForEachDirectory(root, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoriesRecursive(
		Stream<wchar_t> root,
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>> directories_paths,
		GetDirectoriesOrFilesOptions options
	)
	{
		return GetDirectoriesOrFilesImplementation(root, allocator, directories_paths, options, [=](void* data, ForEachFolderFunction functor) {
				return ForEachDirectoryRecursive(root, data, functor, options.depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFiles(Stream<wchar_t> directory, AllocatorPolymorphic allocator, AdditionStream<Stream<wchar_t>> file_paths, GetDirectoriesOrFilesOptions options)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, options, [=](void* data, ForEachFolderFunction functor) {
			return ForEachFileInDirectory(directory, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFilesRecursive(
		Stream<wchar_t> directory, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> file_paths,
		GetDirectoriesOrFilesOptions options
	)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, options, [=](void* data, ForEachFolderFunction functor) {
			return ForEachFileInDirectoryRecursive(directory, data, functor, options.depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFilesWithExtension(
		Stream<wchar_t> directory, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> file_paths,
		Stream<Stream<wchar_t>> extensions, 
		GetDirectoriesOrFilesOptions options
	)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, options, [=](void* data, ForEachFolderFunction functor) {
			return ForEachFileInDirectoryWithExtension(directory, extensions, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFilesWithExtensionRecursive(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> file_paths,
		Stream<Stream<wchar_t>> extensions,
		GetDirectoriesOrFilesOptions options
	)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, options, [=](void* data, ForEachFolderFunction functor) {
			return ForEachFileInDirectoryRecursiveWithExtension(directory, extensions, data, functor, options.depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryOrFiles(
		Stream<wchar_t> directory, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> file_paths, 
		GetDirectoriesOrFilesOptions options
	)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, options, [=](void* data, ForEachFolderFunction functor) {
			return ForEachInDirectory(directory, data, functor, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryOrFilesRecursive(
		Stream<wchar_t> directory, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> file_paths, 
		GetDirectoriesOrFilesOptions options
	)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, options, [=](void* data, ForEachFolderFunction functor) {
			return ForEachInDirectoryRecursive(directory, data, functor, functor, options.depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

}