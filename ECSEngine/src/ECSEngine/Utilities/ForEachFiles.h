#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	// Includes C source files as well
	ECSENGINE_API extern Stream<wchar_t> ECS_CPP_SOURCE_FILES_EXTENSIONS[4];
	
	ECS_INLINE Stream<Stream<wchar_t>> GetCppSourceFilesExtensions() {
		return { ECS_CPP_SOURCE_FILES_EXTENSIONS, ECS_COUNTOF(ECS_CPP_SOURCE_FILES_EXTENSIONS) };
	}

	ECSENGINE_API bool IsFileWithExtension(
		Stream<wchar_t> path,
		Stream<wchar_t> extension,
		CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	ECSENGINE_API bool IsFileWithExtensionRecursive(
		Stream<wchar_t> path,
		Stream<wchar_t> extension,
		CapacityStream<wchar_t> filename = { nullptr, 0, 0 }
	);

	// the return tells the for loop to terminate early if found something
	typedef bool (*ForEachFolderFunction)(Stream<wchar_t> path, void* data);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);

	// Must take as parameter a Stream<wchar_t> representing the current absolute path
	// Must return a bool true to continue the iteration or false to stop
	template<typename Functor>
	ECS_INLINE bool ForEachFileInDirectory(Stream<wchar_t> directory, Functor functor) {
		auto wrapper = [](Stream<wchar_t> path, void* _data) {
			Functor* functor = (Functor*)_data;
			return (*functor)(path);
		};
		return ForEachFileInDirectory(directory, &functor, wrapper);
	}

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryWithExtension(
		Stream<wchar_t> directory,
		Stream<Stream<wchar_t>> extensions,
		void* data,
		ForEachFolderFunction functor
	);

	// Must take as parameter a Stream<wchar_t> representing the current absolute path
	// Must return a bool true to continue the iteration or false to stop
	template<typename Functor>
	ECS_INLINE bool ForEachFileInDirectoryWithExtension(Stream<wchar_t> directory, Stream<Stream<wchar_t>> extensions, Functor functor) {
		auto wrapper = [](Stream<wchar_t> path, void* _data) {
			Functor* functor = (Functor*)_data;
			return (*functor)(path);
		};
		return ForEachFileInDirectoryWithExtension(directory, extensions, &functor, wrapper);
	}

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as parameter a Stream<wchar_t> representing the current absolute path
	// Must return a bool true to continue the iteration or false to stop
	template<typename Functor>
	ECS_INLINE bool ForEachFileInDirectoryRecursive(Stream<wchar_t> directory, Functor functor, bool depth_traversal = false) {
		auto wrapper = [](Stream<wchar_t> path, void* _data) {
			Functor* functor = (Functor*)_data;
			return (*functor)(path);
		};
		return ForEachFileInDirectoryRecursive(directory, &functor, wrapper, depth_traversal);
	}

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachFileInDirectoryRecursiveWithExtension(
		Stream<wchar_t> directory,
		Stream<Stream<wchar_t>> extension,
		void* data,
		ForEachFolderFunction functor,
		bool depth_traversal = false
	);

	// Must take as parameter a Stream<wchar_t> representing the current absolute path
	// Must return a bool true to continue the iteration or false to stop
	template<typename Functor>
	ECS_INLINE bool ForEachFileInDirectoryRecursiveWithExtension(Stream<wchar_t> directory, Stream<Stream<wchar_t>> extension, Functor functor, bool depth_traversal = false) {
		auto wrapper = [](Stream<wchar_t> path, void* _data) {
			Functor* functor = (Functor*)_data;
			return (*functor)(path);
		};
		return ForEachFileInDirectoryRecursiveWithExtension(directory, extension, &functor, wrapper, depth_traversal);
	}


	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectory(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor);
	
	// Must take as parameter a Stream<wchar_t> representing the current absolute path
	// Must return a bool true to continue the iteration or false to stop
	template<typename Functor>
	ECS_INLINE bool ForEachDirectory(Stream<wchar_t> directory, Functor functor) {
		auto wrapper = [](Stream<wchar_t> path, void* _data) {
			Functor* functor = (Functor*)_data;
			return (*functor)(path);
		};
		return ForEachDirectory(directory, &functor, wrapper);
	}

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachDirectoryRecursive(Stream<wchar_t> directory, void* data, ForEachFolderFunction functor, bool depth_traversal = false);

	// Must take as parameter a Stream<wchar_t> representing the current absolute path
	// Must return a bool true to continue the iteration or false to stop
	template<typename Functor>
	ECS_INLINE bool ForEachDirectoryRecursive(Stream<wchar_t> directory, Functor functor, bool depth_traversal = false) {
		auto wrapper = [](Stream<wchar_t> path, void* _data) {
			Functor* functor = (Functor*)_data;
			return (*functor)(path);
		};

		return ForEachDirectoryRecursive(directory, &functor, wrapper, depth_traversal);
	}

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop
	// Returns false when an error occured during traversal
	ECSENGINE_API bool ForEachInDirectory(
		Stream<wchar_t> directory,
		void* data,
		ForEachFolderFunction folder_functor,
		ForEachFolderFunction file_functor
	);

	// Must take as arguments Stream<wchar_t> and void* and return a bool
	// True to continue the iteration or false to stop; files and folders
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

	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectories(
		Stream<wchar_t> root, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> directories_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectoriesRecursive(
		Stream<wchar_t> root, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> directories_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectoryFiles(
		Stream<wchar_t> directory, 
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> file_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectoryFilesRecursive(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator, 
		AdditionStream<Stream<wchar_t>> file_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectoryFilesWithExtension(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>> file_path,
		Stream<Stream<wchar_t>> extensions,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectoryFilesWithExtensionRecursive(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>> file_paths,
		Stream<Stream<wchar_t>> extensions,
		GetDirectoriesOrFilesOptions options = {}
	);

	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectoryOrFiles(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>> file_paths,
		GetDirectoriesOrFilesOptions options = {}
	);
	
	// Returns false if it encountered an error, else true
	ECSENGINE_API bool GetDirectoryOrFilesRecursive(
		Stream<wchar_t> directory,
		AllocatorPolymorphic allocator,
		AdditionStream<Stream<wchar_t>> file_paths,
		GetDirectoriesOrFilesOptions options = {}
	);

}