#pragma once
#include "../../Core.h"
#include "../../Containers/AtomicStream.h"
#include "../../Utilities/Function.h"

namespace ECSEngine {

	// Has a per thread atomic stream which is padded to the cache line
	// When a thread finishes its own atomic stream, it will try to steal from other queues
	template<typename T>
	struct TaskStealing {
		TaskStealing() = default;
		TaskStealing(void* allocation) {
			InititializeFromBuffer(allocation);
		}

		AtomicStream<T>* GetStream(unsigned int thread_index) const {
			return (AtomicStream<T>*)function::OffsetPointer(streams, thread_index * ECS_CACHE_LINE_SIZE);
		}
		
		// It will distribute the tasks uniformly along the streams
		void SetData(Stream<T> data, unsigned int thread_count) {
			unsigned int per_thread_count = (unsigned int)data.size / thread_count;
			unsigned int remainder = (unsigned int)data.size % thread_count;

			for (unsigned int index = 0; index < thread_count; index++) {
				AtomicStream<T>* stream = GetStream(index);
				stream->capacity = per_thread_count + (remainder > 0);
				remainder = remainder > 0 ? remainder - 1 : 0;

				stream->size.store(0, ECS_RELAXED);
				stream->buffer = data.buffer;

				// Increase the data.buffer with the capacity count
				data.buffer += stream->capacity;
			}
		}

		// It will try to get only from the thread's stream
		// Not from the others.
		bool GetFromThread(T& element, unsigned int thread_index) {
			AtomicStream<T>* stream = GetStream(thread_index);
			return stream->TryGet(element);
		}

		// Will try to get from other streams as well
		bool Get(T& element, unsigned int thread_index, unsigned int thread_count) {
			if (GetFromThread(element, thread_index)) {
				return true;
			}

			// Try stealing now
			for (unsigned int index = 0; index < thread_count; index++) {
				if (index != thread_index) {
					if (GetFromThread(element, thread_index)) {
						return true;
					}
				}
			}

			return false;
		}

		void InititializeFromBuffer(void* allocation) {
			streams = (AtomicStream<T>*)allocation;
		}

		static size_t MemoryOf(unsigned int thread_count) {
			return ECS_CACHE_LINE_SIZE * thread_count;
		}

		// This is a pointer to the first cache line containing the streams
		AtomicStream<T>* streams;
	};

}