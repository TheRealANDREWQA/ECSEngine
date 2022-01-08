#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	ECSENGINE_API bool IsFileWithExtension(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		wchar_t* ECS_RESTRICT filename = nullptr,
		size_t max_size = 256
	);

	// It will be forwarded to const wchar_t* variant
	ECSENGINE_API bool IsFileWithExtension(
		Stream<wchar_t> path,
		Stream<wchar_t> extension,
		CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	ECSENGINE_API bool IsFileWithExtensionRecursive(
		const wchar_t* ECS_RESTRICT path,
		const wchar_t* ECS_RESTRICT extension,
		wchar_t* ECS_RESTRICT filename = nullptr,
		size_t max_size = 256
	);

	// It will be forwarded to const wchar_t* variant
	ECSENGINE_API bool IsFileWithExtensionRecursive(
		Stream<wchar_t> path,
		Stream<wchar_t> extension,
		CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	// the return tells the for loop to terminate early if found something
	using ForEachFolderFunction = bool (*)(const wchar_t* path, void* data);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryWithExtension(
		Stream<wchar_t> directory,
		Stream<const wchar_t*> extensions,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursiveWithExtension(
		const wchar_t* directory,
		Stream<const wchar_t*> extension,
		void* data,
		ForEachFolderFunction functor,
		bool depth_traversal = false
	);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursiveWithExtension(
		Stream<wchar_t> directory,
		Stream<const wchar_t*> extension,
		void* data,
		ForEachFolderFunction functor,
		bool depth_traversal = false
	);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectory(const wchar_t* directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectoryRecursive(const wchar_t* directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop; files and folders
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachInDirectory(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachInDirectory(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop; files and folders
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachInDirectoryRecursive(
		const wchar_t* directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor,
		bool depth_traversal = false
	);

	// Must take as arguments const wchar_t* and void* and return a bool
	// True to continue the iteration or false to stop; files and folders
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachInDirectoryRecursive(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor,
		bool depth_traversal = false
	);

	// Walks down the root and allocates the necessary memory in order to have each directory saved separetely
	ECSENGINE_API bool GetDirectories(const wchar_t* root, AllocatorPolymorphic allocator, CapacityStream<const wchar_t*>& directories_paths, bool batched_allocation = false);

	// Walks down the root and allocates the necessary memory in order to have each directory saved separetely
	ECSENGINE_API bool GetDirectoriesRecursive(
		const wchar_t* root, 
		AllocatorPolymorphic allocator, 
		CapacityStream<const wchar_t*>& directories_paths,
		bool batched_allocation = false,
		bool depth_traversal = false
	);

	// Walks down the root and allocates the necessary memory in order to have each file saved separetely
	ECSENGINE_API bool GetDirectoryFiles(const wchar_t* directory, AllocatorPolymorphic allocator, CapacityStream<const wchar_t*>& file_paths, bool batched_allocation = false);

	// Walks down the root and allocates the necessary memory in order to have each file saved separetely
	ECSENGINE_API bool GetDirectoryFilesRecursive(
		const wchar_t* directory,
		AllocatorPolymorphic allocator, 
		CapacityStream<const wchar_t*>& file_paths,
		bool batched_allocation = false,
		bool depth_traversal = false
	);

	ECSENGINE_API bool GetDirectoryFilesWithExtension(
		const wchar_t* directory,
		AllocatorPolymorphic allocator,
		CapacityStream<const wchar_t*>& files_path,
		Stream<const wchar_t*> extensions,
		bool batched_allocation = false
	);

	ECSENGINE_API bool GetDirectoryFileWithExtensionRecursive(
		const wchar_t* directory,
		AllocatorPolymorphic allocator,
		CapacityStream<const wchar_t*>& file_paths,
		Stream<const wchar_t*> extensions,
		bool batched_allocation = false,
		bool depth_traversal = false
	);

}