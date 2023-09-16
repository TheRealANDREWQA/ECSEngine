#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	// It will be forwarded to const wchar_t* variant
	ECSENGINE_API bool IsFileWithExtension(
		Stream<wchar_t> path,
		Stream<wchar_t> extension,
		CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	// It will be forwarded to const wchar_t* variant
	ECSENGINE_API bool IsFileWithExtensionRecursive(
		Stream<wchar_t> path,
		Stream<wchar_t> extension,
		CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	// the return tells the for loop to terminate early if found something
	typedef bool (*ForEachFolderFunction)(Stream<wchar_t> path, void* data);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryWithExtension(
		Stream<wchar_t> directory,
		Stream<Stream<wchar_t>> extensions,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursiveWithExtension(
		Stream<wchar_t> directory,
		Stream<Stream<wchar_t>> extension,
		void* data,
		ForEachFolderFunction functor,
		bool depth_traversal = false
	);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as arguments Stream<wchar_t>and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// It will be forwarded to const wchar_t* variant
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachInDirectory(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
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

	struct GetDirectoriesOrFilesOptions {
		// If given, it will make the paths relative to this one
		Stream<wchar_t> relative_root = {};
		bool batched_allocation = false;
		
		// This is relevant only for recursive functions
		bool depth_traversal = false;
	};

	// Walks down the root and allocates the necessary memory in order to have each directory saved separetely
	ECSENGINE_API bool GetDirectories(
		Stream<wchar_t> root, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>>& directories_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Walks down the root and allocates the necessary memory in order to have each directory saved separetely
	ECSENGINE_API bool GetDirectoriesRecursive(
		Stream<wchar_t> root, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>>& directories_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Walks down the root and allocates the necessary memory in order to have each file saved separetely
	ECSENGINE_API bool GetDirectoryFiles(
		Stream<wchar_t> directory, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>>& file_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Walks down the root and allocates the necessary memory in order to have each file saved separetely
	ECSENGINE_API bool GetDirectoryFilesRecursive(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>>& file_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	ECSENGINE_API bool GetDirectoryFilesWithExtension(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>>& file_path,
		Stream<Stream<wchar_t>> extensions,
		GetDirectoriesOrFilesOptions options = {}
	);

	ECSENGINE_API bool GetDirectoryFilesWithExtensionRecursive(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>>& file_paths,
		Stream<Stream<wchar_t>> extensions,
		GetDirectoriesOrFilesOptions options = {}
	);

}