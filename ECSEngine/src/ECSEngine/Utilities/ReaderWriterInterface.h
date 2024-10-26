#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	enum ECS_INSTRUMENT_SEEK_TYPE : unsigned char {
		ECS_INSTRUMENT_SEEK_START,
		ECS_INSTRUMENT_SEEK_CURRENT,
		ECS_INSTRUMENT_SEEK_END
	};

	// The following are some read/write instruments that can be used generically
	// For write/read functions.

	struct WriteInstrument {
		// Returns the offset of the current write location compared to the start of the write range
		virtual size_t GetOffset() const = 0;

		virtual bool Write(const void* data, size_t data_size) = 0;

		// It will discard any data that was written after that point
		virtual bool ResetAndSeekTo(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) = 0;

		virtual bool Flush() = 0;

		// Returns true if it succeeded, else false
		template<typename T>
		ECS_INLINE bool Write(const T* data) {
			return Write(data, sizeof(*data));
		}

		// Returns true if it succeeded, else false
		// The template parameter selects the integer type to be used for the data field
		template<typename IntegerType>
		ECS_INLINE bool WriteWithSize(Stream<void> data) {
			static_assert(std::is_integral_v<IntegerType>, "WriteWithSize template parameter must be an integer!");
			IntegerType integer_size = data.size;
			if (!Write(&integer_size)) {
				return false;
			}
			return Write(data.buffer, data.size);
		}

		// Returns true if it succeeded, else false. It asserts that the data size fits in the specified integer parameter
		template<typename IntegerType>
		ECS_INLINE bool WriteWithSizeCheck(Stream<void> data) {
			static_assert(std::is_integral_v<IntegerType>, "WriteWithSizeCheck template parameter must be an integer!");
			ECS_ASSERT(IsUnsignedIntInRange<IntegerType>(data.size), "WriteWithSizeCheck of a write instrument failed because the data size does not fit in the integer range.");
			return WriteWithSize<IntegerType>(data);
		}
	};

	// Should be added to the structure that inherits from WriteInstrumentHelper
#define ECS_WRITE_INSTRUMENT_HELPER using WriteInstrument::Write

	struct ReadInstrument {
		// Returns the offset from the beginning of the read range up until the current read location
		virtual size_t GetOffset() const = 0;

		virtual bool Read(void* data, size_t data_size) = 0;

		virtual bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) = 0;

		// Returns a valid pointer if the data size can be directly referenced in the reader, without having you to allocate
		// A buffer and then read into it. The out boolean parameter will be set to true if the current data size is out of bounds
		// For the serialized range, else it will be set to false. In case it can reference the data but it is out of bounds, it will return nullptr,
		// But if it cannot reference the data and the data size is out of bounds, the out parameter won't be set to true.
		virtual void* ReferenceData(size_t data_size, bool& out_of_bounds) = 0;

		// Returns the total byte size of the written data
		virtual size_t TotalSize() const = 0;

		// Returns true if it succeeded, else false
		template<typename T>
		ECS_INLINE bool Read(T* data) {
			return Read(data, sizeof(*data));
		}

		// Returns true if it succeeded, else false
		// To determine if the overbound protection was triggered,
		// Compare size with data_capacity. Data_capacity must be in bytes
		template<typename IntegerType>
		ECS_INLINE bool ReadWithSize(void* data, IntegerType& size, size_t data_capacity) {
			static_assert(std::is_integral_v<IntegerType>, "ReadWithSize template parameter must be an integer!");
			if (!Read(&size)) {
				return false;
			}
			if ((size_t)size > data_capacity) {
				return false;
			}
			return Read(data, (size_t)size);
		}

		template<typename IntegerType, typename ElementType>
		ECS_INLINE bool ReadWithSize(CapacityStream<ElementType>& stream) {
			IntegerType integer_size = 0;
			bool success = ReadWithSize<IntegerType>(stream.buffer, integer_size, stream.capacity * sizeof(ElementType));
			stream.size = integer_size / sizeof(ElementType);
			return success;
		}

		// Returns true if it succeeded, else false
		// Data_capacity must be expressed in number of elements
		// Size will be the number of elements as well
		template<typename IntegerType, typename T>
		ECS_INLINE bool ReadWithSizeTyped(T* data, IntegerType& size, size_t data_capacity) {
			static_assert(std::is_integral_v<IntegerType>, "ReadWithSizeTyped template parameter must be an integer!");
			bool success = ReadWithSize<IntegerType>(data, size, data_capacity * sizeof(*data));
			// Must be converted to the number of elements
			size /= (IntegerType)sizeof(*data);
			return success;
		}

		// This variant allows you to dynamically allocate the data, using an allocator
		// The last argument allows to limit the maximum number of bytes, by failing with a false
		// If it surpasses that value
		template<typename IntegerType>
		ECS_INLINE bool ReadWithSize(Stream<void>& data, AllocatorPolymorphic allocator, size_t max_allowed_capacity = UINT64_MAX) {
			IntegerType integer_size = 0;
			if (!Read(&integer_size)) {
				return false;
			}
			if ((size_t)integer_size > max_allowed_capacity) {
				return false;
			}
			data.size = (size_t)integer_size;
			data.buffer = data.size == 0 ? nullptr : AllocateEx(allocator, data.size);
			return Read(data.buffer, data.size);
		}

		// It will dynamically allocate the data with an optional maximum capacity (expressed in element counts)
		template<typename IntegerType, typename ElementType>
		ECS_INLINE bool ReadWithSize(CapacityStream<ElementType>& data, AllocatorPolymorphic allocator, size_t max_allowed_capacity = UINT64_MAX) {
			Stream<void> stream_data;
			if (!ReadWithSize<IntegerType>(stream_data, allocator, max_allowed_capacity == UINT64_MAX ? UINT64_MAX : max_allowed_capacity * sizeof(ElementType))) {
				return false;
			}
			data.buffer = (ElementType*)stream_data.buffer;
			data.size = stream_data.size / sizeof(ElementType);
			data.capacity = data.size;
			return true;
		}

		// It will dynamically allocate the data with an optional maximum capacity (expressed in element counts)
		// The data parameter must be empty! (it will not clear it first)
		template<typename IntegerType, typename ElementType>
		ECS_INLINE bool ReadWithSize(ResizableStream<ElementType>& data, size_t max_allowed_capacity = UINT64_MAX) {
			Stream<void> stream_data;
			if (!ReadWithSize<IntegerType>(stream_data, data.allocator, max_allowed_capacity == UINT64_MAX ? UINT64_MAX : max_allowed_capacity * sizeof(ElementType))) {
				return false;
			}
			data.buffer = (ElementType*)stream_data.buffer;
			data.size = stream_data.size / sizeof(ElementType);
			data.capacity = data.size;
			return true;
		}

		// A structure returned by the ReadInstrument::ReadOrReferenceData
		struct ReadOrReferenceBuffer {
			void* buffer = nullptr;
			// It is set to true if the buffer was allocated from the allocator
			bool was_allocated = false;
			// Set to true if the reference operation failed, else it is the read operation that failed
			// Should only be checked if the returned buffer is nullptr.
			bool is_reference_failure = false;
		};

		// It will try to reference the data. If that succeeds, it returns that pointer without making an allocation
		// And sets the output boolean to false. Else, it will allocate a buffer, read the data into it and return that pointer.
		// There are boolean parameters to help you handle failure cases.
		ReadOrReferenceBuffer ReadOrReferenceData(AllocatorPolymorphic allocator, size_t data_size) {
			ReadOrReferenceBuffer result;

			result.buffer = ReferenceData(data_size, result.is_reference_failure);
			// If we have a reference failure, it indicate that the instrument could reference the data
			// But the given range is outside the bounds
			if (result.buffer == nullptr && !result.is_reference_failure) {
				result.buffer = AllocateEx(allocator, data_size);
				if (!Read(result.buffer, data_size)) {
					DeallocateEx(allocator, result.buffer);
					result.buffer = nullptr;
				}
				else {
					result.was_allocated = true;
				}
			}

			return result;
		}

	};

	// Should be added to the structure that inherits from ReadInstrumentHelper
#define ECS_READ_INSTRUMENT_HELPER using ReadInstrument::Read

}