#pragma once
#include "ReaderWriterInterface.h"

namespace ECSEngine {

	struct InMemoryWriteInstrument : WriteInstrument {
		ECS_WRITE_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryWriteInstrument() : initial_buffer(0), initial_capacity(0), buffer(nullptr), buffer_capacity(nullptr) {}
		ECS_INLINE InMemoryWriteInstrument(uintptr_t& _buffer, size_t& _buffer_capacity) : initial_buffer(_buffer), initial_capacity(_buffer_capacity), buffer(&_buffer), buffer_capacity(&_buffer_capacity) {}

		ECS_INLINE size_t GetOffset() const override {
			return *buffer - initial_buffer;
		}

		ECS_INLINE bool Write(const void* data, size_t data_size) override {
			if (*buffer_capacity < data_size) {
				return false;
			}
			*buffer_capacity -= data_size;
			memcpy((void*)*buffer, data, data_size);
			*buffer += data_size;
			return true;
		}

		bool ResetAndSeekTo(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
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

	struct InMemoryReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryReadInstrument() : buffer(nullptr), buffer_capacity(nullptr), initial_buffer(0), initial_buffer_capacity(0) {}
		ECS_INLINE InMemoryReadInstrument(uintptr_t& _buffer, size_t& _buffer_capacity) : buffer(&_buffer), buffer_capacity(&_buffer_capacity), initial_buffer(_buffer),
			initial_buffer_capacity(_buffer_capacity) {}

		ECS_INLINE size_t GetOffset() const override {
			return *buffer - initial_buffer;
		}

		ECS_INLINE bool Read(void* data, size_t data_size) override {
			if (*buffer_capacity < data_size) {
				return false;
			}
			*buffer_capacity -= data_size;
			memcpy(data, (const void*)*buffer, data_size);
			*buffer += data_size;
			return true;
		}

		ECS_INLINE bool ReadAlways(void* data, size_t data_size) override {
			// Same as normal read
			return ReadAlways(data, data_size);
		}

		bool Seek(ECS_INSTRUMENT_SEEK_TYPE seek_type, int64_t offset) override {
			switch (seek_type) {
			case ECS_INSTRUMENT_SEEK_START:
			{
				*buffer = initial_buffer + offset;
				*buffer_capacity = initial_buffer_capacity - offset;
			}
				break;
			case ECS_INSTRUMENT_SEEK_CURRENT:
			{
				*buffer = *buffer + offset;
				*buffer_capacity = *buffer_capacity - offset;
			}
				break;
			case ECS_INSTRUMENT_SEEK_END:
			{
				*buffer = initial_buffer + initial_buffer_capacity + offset;
				*buffer_capacity = -offset;
			}
				break;
			default:
				ECS_ASSERT(false, "Invalid instrument seek type");
			}
			
			return true;
		}

		void* ReferenceData(size_t data_size, bool& is_out_of_range) override {
			if (*buffer_capacity < data_size) {
				is_out_of_range = true;
				return nullptr;
			}
			is_out_of_range = false;
			void* data_pointer = (void*)*buffer;
			*buffer += data_size;
			*buffer_capacity -= data_size;
			return data_pointer;
		}

		ECS_INLINE size_t TotalSize() const override {
			return initial_buffer_capacity;
		}

		ECS_INLINE bool IsSizeDetermination() const override {
			return false;
		}

		// This is the value stored at construction time
		uintptr_t initial_buffer;
		size_t initial_buffer_capacity;
		uintptr_t* buffer;
		size_t* buffer_capacity;
	};

}