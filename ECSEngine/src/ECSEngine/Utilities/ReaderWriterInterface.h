#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "Serialization/SerializeIntVariableLength.h"

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

		// Should return true if this writer is used to determine how much memory the writing requires for a particular
		// Structure, else false. The function that uses this information should ideally cache this call, to not induce
		// A function call every time this is called
		virtual bool IsSizeDetermination() const = 0;

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

		// Returns true if it succeeded, else false. It uses variable length encoding for the size
		template<typename ElementType>
		ECS_INLINE bool WriteWithSizeVariableLength(Stream<ElementType> data) {
			// Don't write data.size * sizeof(ElementType), since that can enlarge the size
			// And make it more likely that more bytes are used
			if (!SerializeIntVariableLengthBool(this, data.size)) {
				return false;
			}
			size_t element_size = 0;
			return Write(data.buffer, data.size * GetStructureByteSize<ElementType>());
		}
	};

	// Should be added to the structure that inherits from WriteInstrumentHelper
#define ECS_WRITE_INSTRUMENT_HELPER using WriteInstrument::Write

	struct ReadInstrument {
		ECS_INLINE ReadInstrument(size_t _total_size) : total_size(_total_size) {}

		// This structure will remove the subinstrument in the destructor
		struct SubinstrumentDeallocator {
			ECS_INLINE ~SubinstrumentDeallocator() {
				instrument->PopSubinstrument();
			}

			ReadInstrument* instrument;
		};

		constexpr static size_t MAX_SUBINSTRUMENT_COUNT = 8;

		struct SubinstrumentData {
			size_t start_offset;
			size_t range_size;
		};

	protected:
		// These functions are protected because the front facing functions do a little bit more to handle subinstruments

		// Returns the offset from the beginning of the read range up until the current read location
		virtual size_t GetOffsetImpl() const = 0;

		// This function can assume that the read is in bounds, since the upstream ReadInstrument function ensures this property
		virtual bool ReadImpl(void* data, size_t data_size) = 0;

		// This function can assume that the read is in bounds, since the upstream ReadInstrument function ensures this property
		// This function should be called when you always want to read into data - this distinction
		// Is relevant only for size determination instruments, which for read simply skips the data
		virtual bool ReadAlwaysImpl(void* data, size_t data_size) = 0;

		// This function can assume that the read is in bounds, since the upstream ReadInstrument function ensures this property
		virtual bool SeekImpl(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) = 0;

		// This function can assume that the read is in bounds, since the upstream ReadInstrument function ensures this property
		// Returns a valid pointer if the data size can be directly referenced in the reader, without having you to allocate
		// A buffer and then read into it. The out boolean parameter will be set to true if the current data size is out of bounds
		// For the serialized range, else it will be set to false. In case it can reference the data but it is out of bounds, it will return nullptr,
		// But if it cannot reference the data and the data size is out of bounds, the out parameter won't be set to true.
		virtual void* ReferenceDataImpl(size_t data_size) = 0;

	private:
		// ReadFunctor must be a lambda that returns the provided return type and takes no parameters
		// And performs the actual read. At the moment, it should be either the function ReadImpl, ReadAlwaysImpl, ReferenceDataImpl.
		// is_out_of_range is set to true for the case where there are subinstruments and the read falls outside the subinstrument range
		template<typename ReturnType, typename ReadFunctor>
		ReturnType ReadImplementation(size_t data_size, bool& is_out_of_range, ReadFunctor&& read_functor) {
			is_out_of_range = false;
			// If we have a subinstrument, we must ensure that the data size fits in the subrange
			size_t offset = GetOffset();
			size_t instrument_range = subinstrument_count == 0 ? total_size : subinstruments[subinstrument_count - 1]->range_size;
			if (offset + data_size > instrument_range) {
				is_out_of_range = true;
				if constexpr (std::is_same_v<bool, ReturnType>) {
					return false;
				}
				else if constexpr (std::is_same_v<void*, ReturnType>) {
					return nullptr;
				}
				else {
					static_assert(false, "Invalid ReadInstrument ReadImplementation parameters!");
				}
			}
		
			return read_functor();
		}

	public:
		// Similarly to the writer, this function indicates whether or not this call is used to determine the byte size
		// Read for a particular structure, without intending to actually read all data (some data might still be read, the data
		// that is required to know how much to advance)
		virtual bool IsSizeDetermination() const = 0;

		// Returns the offset from the beginning of the read range up until the current read location
		size_t GetOffset() const {
			size_t offset = GetOffsetImpl();
			// If we have a subinstrument, we must deduct the start offset for that subinstrument
			size_t subinstrument_offset = 0;
			if (subinstrument_count > 0) {
				subinstrument_offset = subinstruments[subinstrument_count - 1]->start_offset;
			}
			return offset - subinstrument_offset;
		}

		// Returns true if it succeeded, else false
		ECS_INLINE bool Read(void* data, size_t data_size) {
			bool is_out_of_range;
			return ReadImplementation<bool>(data_size, is_out_of_range, [this, data, data_size]() -> bool {
				return ReadImpl(data, data_size);
			});
		}

		// This function should be called when you always want to read into data - this distinction
		// Is relevant only for size determination instruments, which for read simply skips the data.
		// Returns true if it succeeded, else false
		ECS_INLINE bool ReadAlways(void* data, size_t data_size) {
			bool is_out_of_range;
			return ReadImplementation<bool>(data_size, is_out_of_range, [this, data, data_size]() -> bool {
				return ReadAlwaysImpl(data, data_size);
			});
		}

		// Returns a valid pointer if the data size can be directly referenced in the reader, without having you to allocate
		// A buffer and then read into it. The out boolean parameter will be set to true if the current data size is out of bounds
		// For the serialized range, else it will be set to false. In case it can reference the data but it is out of bounds, it will return nullptr,
		// But if it cannot reference the data and the data size is out of bounds, the out parameter won't be set to true.
		ECS_INLINE void* ReferenceData(size_t data_size, bool& is_out_of_bounds) {
			return ReadImplementation<void*>(data_size, is_out_of_bounds, [this, data_size, &is_out_of_bounds]() -> void* {
				return ReferenceDataImpl(data_size);
			});
		}

		bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) {
			// Ensure that the provided seek is in bounds.
			size_t range_size = subinstrument_count == 0 ? total_size : subinstruments[subinstrument_count - 1]->range_size;
			size_t range_offset = subinstrument_count == 0 ? 0 : subinstruments[subinstrument_count - 1]->start_offset;
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				// The check for offset > 0 is a bit redundant, since that checks
				// That the integer is not negative, but the previous offset < 0 check ensures that,
				// But leave it nonetheless for better clarity
				if (offset < 0 || (offset > 0 && offset > range_size)) {
					return false;
				}

				// The call can be forwarded, while adding the subinstrument offset
				return SeekImpl(seek_type, offset + (int64_t)range_offset);
			}
			break;
			case ECS_INSTRUMENT_SEEK_CURRENT:
			{
				size_t current_offset = GetOffset();
				if ((offset < 0 && offset < -(int64_t)current_offset) || (offset > 0 && (size_t)offset > range_size - current_offset)) {
					return false;
				}

				// Can forward the call as is, without modifying the offset
				return SeekImpl(seek_type, offset);
			}
			break;
			case ECS_INSTRUMENT_SEEK_END:
			{
				if (offset > 0 || (offset < 0 && offset < -(int64_t)range_size)) {
					return false;
				}

				// The call needs to be transformed into a start seek, because it's easier to compute
				// The offset has to be negative, and the addition is the way to compose them
				return SeekImpl(ECS_INSTRUMENT_SEEK_START, (int64_t)(range_offset + range_size) + offset);
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid ReadInstrument code path!");
			}

			// Shouldn't be reached
			return false;
		}

		// Returns the total byte size of the written data
		ECS_INLINE size_t TotalSize() const {
			if (subinstrument_count == 0) {
				return total_size;
			}
			return subinstruments[subinstrument_count - 1]->range_size;
		}

		// Returns true if it succeeded, else false
		template<typename T>
		ECS_INLINE bool Read(T* data) {
			return Read(data, sizeof(*data));
		}

		// Returns true if it succeeded, else false
		template<typename T>
		ECS_INLINE bool ReadAlways(T* data) {
			return ReadAlways(data, sizeof(*data));
		}

		// Returns true if it succeeded, else false
		// To determine if the overbound protection was triggered,
		// Compare size with data_capacity. Data_capacity must be in bytes
		template<typename IntegerType>
		ECS_INLINE bool ReadWithSize(void* data, IntegerType& size, size_t data_capacity) {
			static_assert(std::is_integral_v<IntegerType>, "ReadWithSize template parameter must be an integer!");
			if (!ReadAlways(&size)) {
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
		bool ReadWithSize(Stream<void>& data, AllocatorPolymorphic allocator, size_t max_allowed_capacity = UINT64_MAX) {
			IntegerType integer_size = 0;
			if (!ReadAlways(&integer_size)) {
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
		bool ReadWithSize(CapacityStream<ElementType>& data, AllocatorPolymorphic allocator, size_t max_allowed_capacity = UINT64_MAX) {
			Stream<void> stream_data;
			if (!ReadWithSize<IntegerType>(stream_data, allocator, max_allowed_capacity == UINT64_MAX ? UINT64_MAX : max_allowed_capacity * GetStructureByteSize<ElementType>())) {
				return false;
			}
			data.buffer = (ElementType*)stream_data.buffer;
			data.size = stream_data.size / GetStructureByteSize<ElementType>();
			data.capacity = data.size;
			return true;
		}

		// It will dynamically allocate the data with an optional maximum capacity (expressed in element counts)
		// The data parameter must be empty! (it will not clear it first)
		template<typename IntegerType, typename ElementType>
		bool ReadWithSize(ResizableStream<ElementType>& data, size_t max_allowed_capacity = UINT64_MAX) {
			Stream<void> stream_data;
			if (!ReadWithSize<IntegerType>(stream_data, data.allocator, max_allowed_capacity == UINT64_MAX ? UINT64_MAX : max_allowed_capacity * GetStructureByteSize<ElementType>())) {
				return false;
			}
			data.buffer = (ElementType*)stream_data.buffer;
			data.size = stream_data.size / GetStructureByteSize<ElementType>();
			data.capacity = data.size;
			return true;
		}

		// It reads a stream written with a variable length size, by adding it to the provided argument
		// Returns true if it succeeded, else false
		template<typename ElementType>
		bool ReadWithSizeVariableLength(CapacityStream<ElementType>& data) {
			size_t data_size = 0;
			if (!DeserializeIntVariableLengthBool(this, data_size)) {
				return false;
			}

			if (data.size + data_size > (size_t)data.capacity) {
				return false;
			}

			if (!Read(data.buffer + data.size, data_size * GetStructureByteSize<ElementType>())) {
				return false;
			}

			data.size += data_size;
			return true;
		}

		// Reads a written stream with a variable length size by allocating the necessary data size from the allocator
		// Returns true if it succeeded, else false
		template<typename ElementType>
		bool ReadWithSizeVariableLength(CapacityStream<ElementType>& data, AllocatorPolymorphic allocator) {
			size_t data_size = 0;
			if (!DeserializeIntVariableLengthBool(this, data_size)) {
				return false;
			}

			if (!EnsureUnsignedIntegerIsInRange<unsigned int>(data_size)) {
				return false;
			}

			data.Initialize(allocator, data_size, data_size);
			return Read(data.buffer, data.CopySize());
		}

		// Reads a written stream with a variable length size by allocating the necessary data size from the allocator
		// Returns true if it succeeded, else false
		template<typename ElementType>
		bool ReadWithSizeVariableLength(Stream<ElementType>& data, AllocatorPolymorphic allocator) {
			size_t data_size = 0;
			if (!DeserializeIntVariableLengthBool(this, data_size)) {
				return false;
			}

			data.Initialize(allocator, data_size);
			return Read(data.buffer, data.CopySize());
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

		// It will try to reference the data. If that succeeds, it returns that pointer without making an allocation
		// And sets the output boolean was_allocated to false. Else, it will allocate a buffer, read the data into it and return that pointer.
		// There extra output boolean parameters can help you handle failure cases. In case the read failed altogether, the returned stream is empty
		template<typename IntegerType>
		Stream<void> ReadOrReferenceDataWithSize(AllocatorPolymorphic allocator, bool* was_allocated = nullptr, bool* is_reference_failure = nullptr) {
			Stream<void> data;

			IntegerType byte_size;
			if (!ReadAlways(&byte_size)) {
				if (was_allocated) {
					*was_allocated = false;
				}
				if (is_reference_failure) {
					*is_reference_failure = false;
				}
				return data;
			}

			ReadOrReferenceBuffer result = ReadOrReferenceData(allocator, byte_size);
			if (result.buffer != nullptr) {
				data = { result.buffer, byte_size };
			}
			
			if (was_allocated) {
				*was_allocated = result.was_allocated;
			}
			if (is_reference_failure) {
				*is_reference_failure = result.is_reference_failure;
			}
			return data;
		}

		// The same as the other overload, with the exception that the size is written with a variable length instead.
		// It will try to reference the data. If that succeeds, it returns that pointer without making an allocation
		// And sets the output boolean was_allocated to false. Else, it will allocate a buffer, read the data into it and return that pointer.
		// There extra output boolean parameters can help you handle failure cases. In case the read failed altogether, the returned stream is empty
		Stream<void> ReadOrReferenceDataWithSizeVariableLength(AllocatorPolymorphic allocator, bool* was_allocated = nullptr, bool* is_reference_failure = nullptr) {
			Stream<void> data;

			size_t byte_size;
			if (!DeserializeIntVariableLengthBool(this, byte_size)) {
				if (was_allocated) {
					*was_allocated = false;
				}
				if (is_reference_failure) {
					*is_reference_failure = false;
				}
				return data;
			}

			ReadOrReferenceBuffer result = ReadOrReferenceData(allocator, byte_size);
			if (result.buffer != nullptr) {
				data = { result.buffer, byte_size };
			}

			if (was_allocated) {
				*was_allocated = result.was_allocated;
			}
			if (is_reference_failure) {
				*is_reference_failure = result.is_reference_failure;
			}
			return data;
		}

		// Tries to read or reference a stream of data, with the specified integer type range.
		// IMPORTANT: It assumes that the reference data can be natively reported by this structure, meaning
		// That it can provide stable pointers. If always can't, like a typical buffered file reader, you shouldn't
		// Call this function, as it will always fail
		template<typename IntegerType, typename ElementType>
		bool ReferenceDataWithSize(Stream<ElementType>& data) {
			IntegerType byte_size;
			if (!ReadAlways(&byte_size)) {
				return false;
			}

			bool is_out_of_bounds = false;
			void* buffer = ReferenceData(byte_size, is_out_of_bounds);
			if (buffer == nullptr) {
				return false;
			}

			data.InitializeFromBuffer(buffer, byte_size / GetStructureByteSize<ElementType>());
			return true;
		}
		
		// References a stream written with variable length size
		// Returns true if it succeeded (including obtaining the reference), else false
		template<typename ElementType>
		bool ReferenceDataWithSizeVariableLength(Stream<ElementType>& data) {
			size_t data_size = 0;
			if (!DeserializeIntVariableLengthBool(this, data_size)) {
				return false;
			}

			data.buffer = ReferenceData(data_size * GetStructureByteSize<ElementType>());
			data.size = data_size;
			return data.buffer != nullptr;
		}

		// Convenience function which omits the out of range flag
		ECS_INLINE void* ReferenceData(size_t data_size) {
			bool is_out_of_range = false;
			return ReferenceData(data_size, is_out_of_range);
		}

		// Ignores the following bytes, the number being describes by the parameter.
		// Returns true if it succeeded, else false
		ECS_INLINE bool Ignore(size_t byte_size) {
			return Seek(ECS_INSTRUMENT_SEEK_CURRENT, byte_size);
		}

		// Returns true if it succeeded, else false
		template<typename IntegerType>
		ECS_INLINE bool IgnoreWithSize() {
			IntegerType byte_size;
			if (!ReadAlways(&byte_size)) {
				return false;
			}

			return Ignore(byte_size);
		}

		// Can use void as template parameter in order to indicate that these are untyped bytes
		// Returns true if it succeeded, else false
		template<typename ElementType>
		ECS_INLINE bool IgnoreWithSizeVariableLength() {
			size_t data_size = 0;
			if (!DeserializeIntVariableLengthBool(this, data_size)) {
				return false;
			}

			// We must receive the template parameter, because otherwise we don't know
			// How many bytes each element takes
			return Ignore(data_size * GetStructureByteSize<ElementType>());
		}

		// Does not use the parameter in any runtime capacity, it is used only to deduce the type of the template parameter
		// Returns true if it succeeded, else false
		template<typename ElementType>
		ECS_INLINE bool IgnoreWithSizeVariableLength(Stream<ElementType> stream_type) {
			return IgnoreWithSizeVariableLength<ElementType>();
		}

		// The subinstrument data will be used as storage to hold some extra information that is used internally.
		// You shouldn't modify this data, only provide it and ensure it is valid for the entire duration of the
		// Subinstrument. It will limit this instrument to a particular range, "virtualizing" all calls as if
		// The instrument knows about only that range.
		ECS_INLINE SubinstrumentDeallocator PushSubinstrument(SubinstrumentData* data_storage, size_t subrange_size) {
			ECS_ASSERT(subinstrument_count < MAX_SUBINSTRUMENT_COUNT, "The maximum amount of ReadInstrument subinstruments was reached!");
			// If there is another subinstrument in effect, we need to be relative to that one.
			if (subinstrument_count > 0) {
				SubinstrumentData last_subinstrument = *subinstruments[subinstrument_count - 1];
				// This offset is relative to the last subinstrument
				size_t offset = GetOffset();
				ECS_ASSERT(subrange_size < last_subinstrument.range_size - offset, "Invalid ReadInstrument subinstrument call - the subrange exceeds the previous subinstrument range!");
				data_storage->start_offset = last_subinstrument.start_offset + offset;
				data_storage->range_size = subrange_size;
			}
			else {
				data_storage->start_offset = GetOffset();
				ECS_ASSERT(total_size - data_storage->start_offset >= subrange_size, "Invalid ReadInstrument subinstrument call - the subrange exceeds the current state!");
				data_storage->range_size = subrange_size;
			}
			subinstrument_count++;
		}

		// Removes the last subinstrument used. Should not be generally called manually, it should be done
		// Through the destructor of SubinstrumentDeallocator.
		ECS_INLINE void PopSubinstrument() {
			ECS_ASSERT(subinstrument_count > 0);
			subinstrument_count--;
		}

		// This field must be initialized by the derived struct, such that we have this information readily available
		size_t total_size = 0;
		// These field implement the ability of creating subinstruments, which act as if there is currently another active read instrument
		// Inside the current read instrument. Multiple nested subinstruments are allowed, up to a reasonable limit.
		// Use a small embedded buffer where subinstrument pointers are recorded, force the user to provide the SubinstrumentData
		// Storage, such that we reduce the byte size of this structure, since these subinstruments are temporary in nature.
		size_t subinstrument_count = 0;
		SubinstrumentData* subinstruments[MAX_SUBINSTRUMENT_COUNT];
	};

	// Should be added to the structure that inherits from ReadInstrumentHelper
#define ECS_READ_INSTRUMENT_HELPER using ReadInstrument::Read

}