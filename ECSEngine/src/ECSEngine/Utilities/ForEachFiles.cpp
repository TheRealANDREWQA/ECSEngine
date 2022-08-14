#include "ecspch.h"
#include "ForEachFiles.h"
#include "File.h"
#include "Path.h"
#include "FunctionInterfaces.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Allocators/MultipoolAllocator.h"
#include "Function.h"

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
				for_data->filename.Copy((path));
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
				for_data->filename.Copy((path));
			}
			for_data->has_file = true;
			return false;
		});

		return for_data.has_file;
	}

	// --------------------------------------------------------------------------------------------------

	void SetSearchAllStringForEachFile(CapacityStream<wchar_t> string, Stream<wchar_t> directory) {
		string.Copy(directory);
		wchar_t separator = function::PathIsRelative(string) ? ECS_OS_PATH_SEPARATOR_REL : ECS_OS_PATH_SEPARATOR;
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
	bool ForEachInDirectoryInternal(Stream<wchar_t> directory, Function&& function) {
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
	bool ForEachInDirectoryRecursiveInternalBreadth(Stream<wchar_t> directory, Function&& function) {
		const wchar_t** _subdirectories = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_SIMULTANEOUS_DIRECTORIES);
		CapacityStream<const wchar_t*> subdirectories(_subdirectories, 0, MAX_SIMULTANEOUS_DIRECTORIES);

		void* allocation = ECS_STACK_ALLOC(MultipoolAllocator::MemoryOf(MAX_SIMULTANEOUS_DIRECTORIES, STACK_ALLOCATION));
		MultipoolAllocator allocator((unsigned char*)allocation, STACK_ALLOCATION, MAX_SIMULTANEOUS_DIRECTORIES);

		Stream<wchar_t> allocator_directory = function::StringCopy(GetAllocatorPolymorphic(&allocator), directory);
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
						Stream<wchar_t> copied_string = function::StringCopy(GetAllocatorPolymorphic(&allocator), temp_string);
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
	bool ForEachInDirectoryRecursiveInternalDepth(Stream<wchar_t> directory, Function&& function) {
		const wchar_t** _subdirectories = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_SIMULTANEOUS_DIRECTORIES);
		intptr_t* handles = (intptr_t*)ECS_STACK_ALLOC(sizeof(intptr_t) * MAX_SIMULTANEOUS_DIRECTORIES);
		memset(handles, 0, sizeof(intptr_t) * MAX_SIMULTANEOUS_DIRECTORIES);

		CapacityStream<const wchar_t*> subdirectories(_subdirectories, 0, MAX_SIMULTANEOUS_DIRECTORIES);

		void* allocation = ECS_STACK_ALLOC(MultipoolAllocator::MemoryOf(MAX_SIMULTANEOUS_DIRECTORIES, STACK_ALLOCATION));
		MultipoolAllocator allocator((unsigned char*)allocation, STACK_ALLOCATION, MAX_SIMULTANEOUS_DIRECTORIES);

		Stream<wchar_t> allocator_directory = function::StringCopy(GetAllocatorPolymorphic(&allocator), directory);
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
				temp_string.AddStream(find_data.name);
				temp_string[temp_string.size] = L'\0';
				if (!function(temp_string, find_data.attrib)) {
					return true;
				}

				if (find_data.attrib & _A_SUBDIR) {
					Stream<wchar_t> copied_string = function::StringCopy(GetAllocatorPolymorphic(&allocator), temp_string);
					subdirectories.AddSafe(copied_string.buffer);
				}
			}
		}

		return true;
	}

	template<typename Function>
	bool ForEachInDirectoryRecursiveInternal(Stream<wchar_t> directory, Function&& function, bool depth_traversal) {
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
				Stream<wchar_t> extension = function::PathExtension(path);
				if (function::FindString(extension, extensions) != -1) {
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
				Stream<wchar_t> extension = function::PathExtension(path);
				if (function::FindString(extension, extensions) != -1) {
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
	bool GetDirectoriesOrFilesImplementation(
		Stream<wchar_t> root, 
		AllocatorPolymorphic allocator,
		CapacityStream<Stream<wchar_t>>& paths, 
		bool batched_allocation,
		bool depth_traversal,
		Function&& function
	) {
		// Use the allocator to copy the paths - for the batched allocation only if the allocation type is single threaded
		if (!batched_allocation || allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			struct ForData {
				AllocatorPolymorphic allocator;
				CapacityStream<Stream<wchar_t>>* paths;
			};
			ForData for_data = { allocator, &paths };

			bool status = function(root, &for_data, [](Stream<wchar_t> path, void* _data) {
				ForData* data = (ForData*)_data;

				Stream<wchar_t> allocated_path = function::StringCopy(data->allocator, path);
				data->paths->AddSafe(allocated_path);
				return true;
			}, depth_traversal);

			// If something failed - deallocate the individual allocations
			if (!status) {
				for (size_t index = 0; index < paths.size; index++) {
					Deallocate(allocator, paths[index].buffer);
				}
				paths.size = 0;
				return false;
			}

			if (batched_allocation) {
				size_t total_size = 0;
				for (size_t index = 0; index < paths.size; index++) {
					total_size += paths[index].size + 1;
				}

				void* allocation = Allocate(allocator, sizeof(wchar_t) * total_size, alignof(wchar_t));
				uintptr_t buffer = (uintptr_t)allocation;
				for (size_t index = 0; index < paths.size; index++) {
					memcpy((void*)buffer, paths[index].buffer, sizeof(wchar_t) * (paths[index].size + 1));
					Deallocate(allocator, paths[index].buffer);
					paths[index].buffer = (wchar_t*)buffer;

					buffer += sizeof(wchar_t) * (paths[index].size + 1);
				}
			}
			return true;
		}
		// Batched allocation and multithreaded
		else {
			size_t total_size = 0;
			bool status = function(root, &total_size, [](Stream<wchar_t> path, void* _data) {
				size_t* data = (size_t*)_data;
				*data += path.size + 1;

				return true;
			}, depth_traversal);

			if (!status) {
				return false;
			}

			void* allocation = Allocate(allocator, total_size);
			void* initial_allocation = allocation;
			struct BatchedCopyStringsData {
				CapacityStream<Stream<wchar_t>>* paths;
				void** allocation;
			};
			BatchedCopyStringsData copy_data = { &paths, &allocation };
			status = function(root, &copy_data, [](Stream<wchar_t> path, void* _data) {
				BatchedCopyStringsData* data = (BatchedCopyStringsData*)_data;
				void* buffer = *data->allocation;			
				data->paths->AddSafe(Stream<wchar_t>(buffer, path.size));
				
				// Increase the path size with 1 to include the '\0'
				path.size++;
				path.CopyTo(buffer);
				*data->allocation = function::OffsetPointer(buffer, sizeof(wchar_t) * path.size);

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
		Stream<wchar_t> root, 
		AllocatorPolymorphic allocator, 
		CapacityStream<Stream<wchar_t>>& paths, 
		Stream<Stream<wchar_t>> extensions,
		bool batched_allocation,
		bool depth_traversal,
		Function&& function
	) {
		// Use the allocator to copy the paths - for the batched allocation only if the allocation type is single threaded
		if (!batched_allocation || allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			struct ForData {
				AllocatorPolymorphic allocator;
				CapacityStream<Stream<wchar_t>>* paths;
			};
			ForData for_data = { allocator, &paths };

			bool status = function(root, extensions, &for_data, [](Stream<wchar_t> path, void* _data) {
				ForData* data = (ForData*)_data;

				data->paths->AddSafe(function::StringCopy(data->allocator, path));
				return true;
			}, depth_traversal);

			// If something failed - deallocate the individual allocations
			if (!status) {
				for (size_t index = 0; index < paths.size; index++) {
					Deallocate(allocator, paths[index].buffer);
				}
				paths.size = 0;
				return false;
			}

			if (batched_allocation) {
				unsigned short* sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * paths.size);

				size_t total_size = 0;
				for (size_t index = 0; index < paths.size; index++) {
					total_size += paths[index].size + 1;
				}

				void* allocation = Allocate(allocator, sizeof(wchar_t) * total_size, alignof(wchar_t));
				uintptr_t buffer = (uintptr_t)allocation;
				for (size_t index = 0; index < paths.size; index++) {
					void* new_buffer = (void*)buffer;
					// Increment the size to include '\0'
					paths.size++;
					paths.CopyTo(buffer);
					Deallocate(allocator, paths[index].buffer);
					paths[index].buffer = (wchar_t*)new_buffer;
					paths.size--;
				}
			}
			return true;
		}
		// Batched allocation and multithreaded
		else {
			size_t total_size = 0;
			bool status = function(root, extensions, &total_size, [](Stream<wchar_t> path, void* _data) {
				size_t* data = (size_t*)_data;
				size_t path_size = path.size + 1;

				return true;
			}, depth_traversal);

			if (!status) {
				return false;
			}

			void* allocation = Allocate(allocator, total_size);
			void* initial_allocation = allocation;
			struct BatchedCopyStringsData {
				CapacityStream<Stream<wchar_t>>* paths;
				void** allocation;
			};
			BatchedCopyStringsData copy_data = { &paths, &allocation };
			status = function(root, extensions, &copy_data, [](Stream<wchar_t> path, void* _data) {
				BatchedCopyStringsData* data = (BatchedCopyStringsData*)_data;
				void* buffer = *data->allocation;
				data->paths->AddSafe(Stream<wchar_t>(buffer, path.size));

				// Increase the path size with 1 to include the '\0'
				path.size++;
				path.CopyTo(buffer);
				*data->allocation = function::OffsetPointer(buffer, sizeof(wchar_t) * path.size);

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

	// --------------------------------------------------------------------------------------------------

	bool GetDirectories(Stream<wchar_t> root, AllocatorPolymorphic allocator, CapacityStream<Stream<wchar_t>>& directories_paths, bool batched_allocation)
	{
		return GetDirectoriesOrFilesImplementation(root, allocator, directories_paths, batched_allocation, false, [](Stream<wchar_t> directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachDirectory(directory, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoriesRecursive(
		Stream<wchar_t> root,
		AllocatorPolymorphic allocator,
		CapacityStream<Stream<wchar_t>>& directories_paths,
		bool batched_allocation, 
		bool depth_traversal
	)
	{
		return GetDirectoriesOrFilesImplementation(root, allocator, directories_paths, batched_allocation, depth_traversal, [](Stream<wchar_t> directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachDirectoryRecursive(directory, data, functor, depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFiles(Stream<wchar_t> directory, AllocatorPolymorphic allocator, CapacityStream<Stream<wchar_t>>& file_paths, bool batched_allocation)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, batched_allocation, false, [](Stream<wchar_t> directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachFileInDirectory(directory, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFilesRecursive(Stream<wchar_t> directory, AllocatorPolymorphic allocator, CapacityStream<Stream<wchar_t>>& file_paths, bool batched_allocation, bool depth_traversal)
	{
		return GetDirectoriesOrFilesImplementation(directory, allocator, file_paths, batched_allocation, depth_traversal, [](Stream<wchar_t> directory, void* data,
			ForEachFolderFunction functor, bool depth_traversal) {
				return ForEachFileInDirectoryRecursive(directory, data, functor, depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFilesWithExtension(Stream<wchar_t> directory, AllocatorPolymorphic allocator, CapacityStream<Stream<wchar_t>>& file_paths, Stream<Stream<wchar_t>> extensions, bool batched_allocation)
	{
		return GetFilesWithExtensionImplementation(directory, allocator, file_paths, extensions, batched_allocation, false, [](
			Stream<wchar_t> directory, 
			Stream<Stream<wchar_t>> extensions, 
			void* data,
			ForEachFolderFunction functor, 
			bool depth_traversal
			) {
				return ForEachFileInDirectoryWithExtension(directory, extensions, data, functor);
		});
	}

	// --------------------------------------------------------------------------------------------------

	bool GetDirectoryFileWithExtensionRecursive(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator, 
		CapacityStream<Stream<wchar_t>>& file_paths,
		Stream<Stream<wchar_t>> extensions,
		bool batched_allocation,
		bool depth_traversal
	)
	{
		return GetFilesWithExtensionImplementation(directory, allocator, file_paths, extensions, batched_allocation, false, [](
			Stream<wchar_t> directory,
			Stream<Stream<wchar_t>> extensions,
			void* data,
			ForEachFolderFunction functor,
			bool depth_traversal
			) {
				return ForEachFileInDirectoryRecursiveWithExtension(directory, extensions, data, functor, depth_traversal);
		});
	}

	// --------------------------------------------------------------------------------------------------

}