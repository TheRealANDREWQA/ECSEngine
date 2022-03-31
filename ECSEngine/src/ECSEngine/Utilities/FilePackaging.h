#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "File.h"

namespace ECSEngine {

	// The functor can receive the data and decide to replace that data with other data.
	// This is done by returning a new Stream<void>. If that is the case, the function has the oppurtunity to
	// free the memory allocated (if any) on the next iteration or at the end of the call.
	typedef Stream<void> (*PackFileFunctor)(Stream<void> file_contents, void* data);

	typedef HashTable<uint2, ResourceIdentifier, HashFunctionPowerOfTwo, HashFunctionMultiplyString> PackFilesLookupTable;
	
	// Returns -1 for success, otherwise the index of the input file which failed to open or read or write
	// The value of -2 indicates that the output file could not be created.
	// The value of -3 indicates that an error has occured when writing the lookup table
	// The boolean stream should have the same length as the input one
	ECSENGINE_API unsigned int PackFiles(
		Stream<Stream<wchar_t>> input,
		Stream<bool> input_is_text_file, 
		Stream<wchar_t> output,
		PackFileFunctor functor = nullptr, 
		void* data = nullptr
	);

	// The file handle should be opened in binary mode
	// If the call fails or if the data has been corrupted, the table will have the capacity 0
	// The caller is responsible for freeing up the memory of the table after it has finished using it
	ECSENGINE_API PackFilesLookupTable GetPackedFileLookupTable(ECS_FILE_HANDLE file_handle, AllocatorPolymorphic allocator = { nullptr });
	
	// The file handle should be opened in binary mode
	// If the read fails or if the file does not exist, it will return { nullptr, 0 }
	// It is up to the caller to free the memory of the file after it has finished using it
	ECSENGINE_API Stream<void> UnpackFile(
		Stream<wchar_t> file,
		const PackFilesLookupTable* table, 
		ECS_FILE_HANDLE file_handle, 
		AllocatorPolymorphic allocator = { nullptr }
	);
}