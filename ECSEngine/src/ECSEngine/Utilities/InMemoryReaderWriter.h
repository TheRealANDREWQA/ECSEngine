#pragma once
#include "ReaderWriterInterface.h"

namespace ECSEngine {

	// Does not reference the ptr and the buffer capacity that are passed in
	struct InMemoryWriteInstrument : WriteInstrument {
		ECS_WRITE_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryWriteInstrument() : initial_buffer(0), initial_capacity(0), buffer(0), buffer_capacity(0) {}
		ECS_INLINE InMemoryWriteInstrument(uintptr_t _buffer, size_t _buffer_capacity) : initial_buffer(_buffer), initial_capacity(_buffer_capacity), buffer(_buffer), buffer_capacity(_buffer_capacity) {}

		ECS_INLINE size_t GetOffset() const override {
			return buffer - initial_buffer;
		}

		bool Write(const void* data, size_t data_size) override {
			if (buffer_capacity < data_size) {
				return false;
			}
			buffer_capacity -= data_size;
			memcpy((void*)buffer, data, data_size);
			buffer += data_size;
			return true;
		}

		ECS_INLINE bool AppendUninitialized(size_t data_size) override {
			if (buffer_capacity < data_size) {
				return false;
			}
			buffer_capacity -= data_size;
			buffer += data_size;
			return true;
		}

		ECS_INLINE bool DiscardData() override {
			// We don't have to do anything special
			return true;
		}

		bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				buffer = initial_buffer + offset;
				buffer_capacity = initial_capacity - offset;
			}
			break;
			// Current and end are the same, since the current location is basically the end
			case ECS_INSTRUMENT_SEEK_CURRENT:
			case ECS_INSTRUMENT_SEEK_END:
			{
				buffer = buffer + offset;
				buffer_capacity = buffer_capacity - offset;
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid instrument seek type.");
			}

			return true;
		}

		ECS_INLINE bool Flush() override {
			return true;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			return false;
		}

		uintptr_t initial_buffer;
		size_t initial_capacity;
		uintptr_t buffer;
		size_t buffer_capacity;
	};

	// References the ptr and the buffer capacity that are passed in
	struct InMemoryReferenceWriteInstrument : WriteInstrument {
		ECS_WRITE_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryReferenceWriteInstrument() : initial_buffer(0), initial_capacity(0), buffer(nullptr), buffer_capacity(nullptr) {}
		ECS_INLINE InMemoryReferenceWriteInstrument(uintptr_t& _buffer, size_t& _buffer_capacity) : initial_buffer(_buffer), initial_capacity(_buffer_capacity), buffer(&_buffer), buffer_capacity(&_buffer_capacity) {}

		ECS_INLINE size_t GetOffset() const override {
			return *buffer - initial_buffer;
		}

		bool Write(const void* data, size_t data_size) override {
			if (*buffer_capacity < data_size) {
				return false;
			}
			*buffer_capacity -= data_size;
			memcpy((void*)*buffer, data, data_size);
			*buffer += data_size;
			return true;
		}

		ECS_INLINE bool AppendUninitialized(size_t data_size) override {
			if (*buffer_capacity < data_size) {
				return false;
			}
			*buffer_capacity -= data_size;
			*buffer += data_size;
			return true;
		}

		ECS_INLINE bool DiscardData() override {
			// We don't have to do anything special
			return true;
		}

		bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				*buffer = initial_buffer + offset;
				*buffer_capacity = initial_capacity - offset;
			}
				break;
			// Current and end are the same, since the current location is basically the end
			case ECS_INSTRUMENT_SEEK_CURRENT:
			case ECS_INSTRUMENT_SEEK_END:
			{
				*buffer = *buffer + offset;
				*buffer_capacity = *buffer_capacity - offset;
			}
				break;
			default:
				ECS_ASSERT(false, "Invalid instrument seek type.");
			}

			return true;
		}

		ECS_INLINE bool Flush() override {
			return true;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			return false;
		}

		uintptr_t initial_buffer;
		size_t initial_capacity;
		uintptr_t* buffer;
		size_t* buffer_capacity;
	};

	// Does not reference the ptr that is passed in
	struct InMemoryReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryReadInstrument() : ReadInstrument(0), buffer(0), initial_buffer(0) {}
		ECS_INLINE InMemoryReadInstrument(uintptr_t _buffer, size_t buffer_capacity) : ReadInstrument(buffer_capacity), buffer(_buffer), initial_buffer(_buffer) {}
		ECS_INLINE InMemoryReadInstrument(Stream<void> data) : ReadInstrument(data.size), buffer((uintptr_t)data.buffer), initial_buffer((uintptr_t)data.buffer) {}

	protected:
		ECS_INLINE size_t GetOffsetImpl() const override {
			return buffer - initial_buffer;
		}

		ECS_INLINE bool ReadImpl(void* data, size_t data_size) override {
			memcpy(data, (const void*)buffer, data_size);
			buffer += data_size;
			return true;
		}

		ECS_INLINE bool ReadAlwaysImpl(void* data, size_t data_size) override {
			// Same as normal read
			return ReadImpl(data, data_size);
		}

		bool SeekImpl(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				buffer = initial_buffer + offset;
			}
			break;
			case ECS_INSTRUMENT_SEEK_CURRENT:
			{
				buffer = buffer + offset;
			}
			break;
			case ECS_INSTRUMENT_SEEK_END:
			{
				buffer = initial_buffer + total_size + offset;
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid instrument seek type");
			}

			return true;
		}

		ECS_INLINE void* ReferenceDataImpl(size_t data_size) override {
			void* data_pointer = (void*)buffer;
			buffer += data_size;
			return data_pointer;
		}

	public:

		ECS_INLINE bool IsSizeDetermination() const override {
			return false;
		}

		// This is the value stored at construction time
		uintptr_t initial_buffer;
		uintptr_t buffer;
	};

	// References the ptr that is passed in
	struct InMemoryReferenceReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryReferenceReadInstrument() : ReadInstrument(0), buffer(nullptr), initial_buffer(0) {}
		ECS_INLINE InMemoryReferenceReadInstrument(uintptr_t& _buffer, size_t buffer_capacity) : ReadInstrument(buffer_capacity), buffer(&_buffer), initial_buffer(_buffer) {}

	protected:
		ECS_INLINE size_t GetOffsetImpl() const override {
			return *buffer - initial_buffer;
		}

		ECS_INLINE bool ReadImpl(void* data, size_t data_size) override {
			memcpy(data, (const void*)*buffer, data_size);
			*buffer += data_size;
			return true;
		}

		ECS_INLINE bool ReadAlwaysImpl(void* data, size_t data_size) override {
			// Same as normal read
			return ReadImpl(data, data_size);
		}

		bool SeekImpl(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				*buffer = initial_buffer + offset;
			}
				break;
			case ECS_INSTRUMENT_SEEK_CURRENT:
			{
				*buffer = *buffer + offset;
			}
				break;
			case ECS_INSTRUMENT_SEEK_END:
			{
				*buffer = initial_buffer + total_size + offset;
			}
				break;
			default:
				ECS_ASSERT(false, "Invalid instrument seek type");
			}
			
			return true;
		}

		ECS_INLINE void* ReferenceDataImpl(size_t data_size) override {
			void* data_pointer = (void*)*buffer;
			*buffer += data_size;
			return data_pointer;
		}

	public:

		ECS_INLINE bool IsSizeDetermination() const override {
			return false;
		}

		// This is the value stored at construction time
		uintptr_t initial_buffer;
		uintptr_t* buffer;
	};

}