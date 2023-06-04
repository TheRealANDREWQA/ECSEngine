#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "File.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	// The functor can receive the data and decide to replace that data with other data.
	// This is done by returning a new Stream<void>. If that is the case, the function has the opportunity to
	// free the memory allocated (if any) on the next iteration or at the end of the call.
	typedef Stream<void> (*PackFileFunctor)(Stream<void> file_contents, void* data);

	typedef HashTableDefault<uint2> PackFilesLookupTable;

	struct PackedFile {
		PackFilesLookupTable lookup_table;
		ECS_FILE_HANDLE file_handle;
	};
	
	typedef HashTableDefault<unsigned int> MultiPackFilesLookupTable;

	// Makes the connection between multiple packed files such that the correct packed file
	// can be addressed from the get go. The deallocate is here just in case a persistent allocator is used
	struct ECSENGINE_API MultiPackedFile {
		MultiPackedFile Copy(AllocatorPolymorphic allocator) const;

		// If transitioning from a
		void Deallocate(AllocatorPolymorphic allocator);

		MultiPackFilesLookupTable lookup_table;
		// This is the list of the packed files
		Stream<Stream<wchar_t>> packed_files;
	};

	// The file handle should be opened in binary mode
	// If the call fails or if the data has been corrupted, the table will have the capacity 0
	// The caller is responsible for freeing up the memory of the table after it has finished using it
	ECSENGINE_API PackFilesLookupTable GetPackedFileLookupTable(ECS_FILE_HANDLE file_handle, AllocatorPolymorphic allocator = { nullptr });

	ECSENGINE_API unsigned int GetMultiPackedFileIndex(Stream<wchar_t> file, const MultiPackedFile* multi_packed_file);

	// Returns an empty packed_files stream if it fails.
	// A stack based allocator should be given. (performance reasons and because if it fails
	// it won't backtrack any allocations made inside). If a persistent allocator is given
	// then if it fails you need to make sure that the allocator is cleared
	ECSENGINE_API MultiPackedFile LoadMultiPackedFile(Stream<wchar_t> file, AllocatorPolymorphic allocator);

	// It needs the size in order to perform the decryption
	// Returns an empty packed_files stream if it fails.
	// A stack based allocator needs to be given. (performance reasons and because if it fails
	// it won't backtrack any allocations made inside). If a persistent allocator is given
	// then if it fails you need to make sure that the allocator is cleared
	ECSENGINE_API MultiPackedFile LoadMultiPackedFile(uintptr_t* file, size_t size, AllocatorPolymorphic allocator);

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
	// If the read fails or if the file does not exist, it will return { nullptr, 0 }
	// It is up to the caller to free the memory of the file after it has finished using it
	ECSENGINE_API Stream<void> UnpackFile(
		Stream<wchar_t> file,
		const PackedFile* packed_file,
		AllocatorPolymorphic allocator = { nullptr }
	);

	struct MultiPackedFileElement {
		Stream<Stream<wchar_t>> input;
		Stream<wchar_t> output;
	};

	ECSENGINE_API bool WriteMultiPackedFile(Stream<wchar_t> file, Stream<MultiPackedFileElement> elements);

	ECSENGINE_API void WriteMultiPackedFile(uintptr_t* ptr, Stream<MultiPackedFileElement> elements);

	ECSENGINE_API size_t WriteMultiPackedFileSize(Stream<MultiPackedFileElement> elements);

}