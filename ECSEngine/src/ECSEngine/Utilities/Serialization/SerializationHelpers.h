#pragma once
#include "../../Core.h"
#include "../Function.h"
#include "../File.h"
#include "../../Containers/Stream.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Write(CapacityStream<void>& stream, const void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Write(uintptr_t* stream, const void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	template<typename StreamType>
	bool WriteHelper(StreamType stream, const void* data, size_t data_size) {
		if constexpr (std::is_same_v<StreamType, uintptr_t*>) {
			Write(stream, data, data_size);
			return true;
		}
		else if constexpr (std::is_same_v<StreamType, ECS_FILE_HANDLE>) {
			return WriteFile(stream, { data, data_size });
		}
		else if constexpr (std::is_same_v<StreamType, CapacityStream<void>&>) {
			return Write(stream, data, data_size);
		}

		ECS_ASSERT(false);
		return false;
	}

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Read(CapacityStream<void>& stream, void* data, size_t data_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API void Read(uintptr_t* stream, void* data, size_t data_size);

	template<typename StreamType>
	bool ReadHelper(StreamType stream, void* data, size_t data_size) {
		if constexpr (std::is_same_v<StreamType, uintptr_t&>) {
			Read(&stream, data, data_size);
			return true;
		}
		else if constexpr (std::is_same_v<StreamType, ECS_FILE_HANDLE>) {
			return ReadFile(stream, { data, data_size });
		}
		else if constexpr (std::is_same_v<StreamType, CapacityStream<void>&>) {
			return Read(stream, data, data_size);
		}

		ECS_ASSERT(false);
		return false;
	}

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool IgnoreFile(ECS_FILE_HANDLE file, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Ignore(CapacityStream<void>& stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Ignore(uintptr_t* stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	// Prepares the file for writing, allocates the memory for the temporary buffer and then 
	template<typename Functor>
	bool SerializeWriteFile(Stream<wchar_t> file, AllocatorPolymorphic allocator, size_t allocation_size, bool binary, Functor&& functor) {
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TRUNCATE_FILE;
		access_flags |= binary ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		ECS_FILE_STATUS_FLAGS flags = FileCreate(file, &file_handle, access_flags);
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
		bool binary,
		Functor&& functor
	) {
		Stream<void> contents = ReadWholeFile(file, binary, allocator);
		if (contents.buffer != nullptr) {
			uintptr_t buffer = (uintptr_t)contents.buffer;
			size_t pointer_bytes = functor(buffer);
			DeallocateEx(allocator, contents.buffer);
			return pointer_bytes;
		}
		return -1;
	}

	template<typename Functor>
	void* DeserializeReadBinaryFileAndKeepMemory(
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator,
		Functor&& functor
	) {
		Stream<void> contents = ReadWholeFileBinary(file, allocator);
		if (contents.buffer != nullptr) {
			uintptr_t buffer = (uintptr_t)contents.buffer;
			functor(buffer);
			return contents.buffer;
		}
		return nullptr;
	}
	
}