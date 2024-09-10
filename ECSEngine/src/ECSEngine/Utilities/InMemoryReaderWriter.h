#pragma once
#include "ReaderWriterInterface.h"

namespace ECSEngine {

	struct InMemoryWriteInstrument : WriteInstrument {
		ECS_WRITE_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryWriteInstrument() : initial_buffer(0), buffer(nullptr), buffer_capacity(nullptr) {}
		ECS_INLINE InMemoryWriteInstrument(uintptr_t& _buffer, size_t& _buffer_capacity) : initial_buffer(_buffer), buffer(&_buffer), buffer_capacity(&_buffer_capacity) {}

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

		ECS_INLINE bool Flush() override {
			return true;
		}

		uintptr_t initial_buffer;
		uintptr_t* buffer;
		size_t* buffer_capacity;
	};

	struct InMemoryReadInstrument : ReadInstrument {
		ECS_READ_INSTRUMENT_HELPER;

		ECS_INLINE InMemoryReadInstrument() : buffer(nullptr), buffer_capacity(nullptr), initial_buffer(0) {}
		ECS_INLINE InMemoryReadInstrument(uintptr_t& _buffer, size_t& _buffer_capacity) : buffer(&_buffer), buffer_capacity(&_buffer_capacity), initial_buffer(_buffer) {}

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

		ECS_INLINE bool Seek(size_t offset) override {
			*buffer = initial_buffer + offset;
			return true;
		}

		void* ReferenceData(size_t data_size, bool& is_out_of_range) override {
			if (*buffer_capacity < data_size) {
				is_out_of_range = true;
				return nullptr;
			}
			is_out_of_range = false;
			void* pointer = (void*)*buffer;
			*buffer += data_size;
			*buffer_capacity -= data_size;
			return nullptr;
		}

		// This is the value stored at construction time
		uintptr_t initial_buffer;
		uintptr_t* buffer;
		size_t* buffer_capacity;
	};

}