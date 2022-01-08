#pragma once
#include "../../Core.h"
#include "../Function.h"
#include "../File.h"
#include "../../Containers/Stream.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Write(ECS_FILE_HANDLE file, const void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Write(CapacityStream<void>& stream, const void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Write(ECS_FILE_HANDLE file, Stream<void> data);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Write(CapacityStream<void>& stream, Stream<void> data);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Read(ECS_FILE_HANDLE file, void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Read(ECS_FILE_HANDLE file, CapacityStream<void>& data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Read(CapacityStream<void>& stream, void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Read(uintptr_t& stream, void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Read(uintptr_t& stream, CapacityStream<void>& data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Ignore(ECS_FILE_HANDLE file, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Ignore(CapacityStream<void>& stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Ignore(uintptr_t& stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	// Prepares the file for writing, allocates the memory for the temporary buffer and then 
	template<typename Functor>
	bool SerializeWriteFile(Stream<wchar_t> file, AllocatorPolymorphic allocator, size_t allocation_size, Functor&& functor) {
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_STATUS_FLAGS flags = FileCreate(file, &file_handle, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_TRUNCATE_FILE);
		if (flags == ECS_FILE_STATUS_OK) {
			ScopedFile scoped_file(file_handle);

			void* allocation = AllocateEx(allocator, allocation_size);
			uintptr_t buffer = (uintptr_t)allocation;
			functor(buffer);

			bool success = true;
			size_t difference = buffer - (uintptr_t)allocation;
			if (difference > allocation_size) {
				success = false;
			}
			else {
				success = WriteFile(file_handle, { allocation, difference });
			}
			DeallocateEx(allocator, allocation);
			return success;
		}

		return false;
	}

	// -----------------------------------------------------------------------------------------
	
	// Returns the amount of pointer data bytes
	template<typename Functor>
	size_t DeserializeReadFile(
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator,
		Functor&& functor
	) {
		Stream<void> contents = ReadWholeFileBinary(file, allocator);
		if (contents.buffer != nullptr) {
			uintptr_t buffer = (uintptr_t)contents.buffer;
			size_t pointer_bytes = functor(buffer);
			DeallocateEx(allocator, contents.buffer);
			return pointer_bytes;
		}
		return -1;
	}
	
	// -----------------------------------------------------------------------------------------
}