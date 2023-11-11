#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/Queues.h"
#include "../Utilities/Utilities.h"

namespace ECSEngine {

	template<typename T>
	struct Statistic {
		void Add(T value) {
			entries.Push(value);
			min = value < min ? value : min;
			max = value > max ? value : max;
			unsigned int insert_index = 0;
			while (insert_index < min_values.size && min_values[insert_index] <= value) {
				insert_index++;
			}
			// If there is not enough space and the value is indeed smaller,
			if (min_values.size == min_values.capacity) {
				if (insert_index < min_values.capacity) {
					min_values.size--;
					min_values.Insert(insert_index, value);
				}
			}
			else {
				// We need to insert since we already have space
				min_values.Insert(insert_index, value);
			}
		}

		// Uses a coalesced allocation
		void Initialize(AllocatorPolymorphic allocator, unsigned int entries_capacity, unsigned int min_values_capacity) {
			if constexpr (std::is_integral_v<T>) {
				IntegerRange<T>(min, max);
			}
			else if constexpr (std::is_same_v<T, float>) {
				min = -FLT_MAX;
				max = FLT_MAX;
			}
			else if constexpr (std::is_same_v<T, double>) {
				min = DBL_MAX;
				max = -DBL_MAX;
			}
			else {
				static_assert(false, "Statistic does not support types other than integers and floats/doubles");
			}

			min_values_capacity = min_values_capacity == 0 ? 1 : min_values_capacity;
			entries_capacity = entries_capacity == 0 ? 1 : entries_capacity;
			size_t allocation_size = entries.MemoryOf(entries_capacity) + min_values.MemoryOf(min_values_capacity);
			void* allocation = AllocateEx(allocator, allocation_size);
			uintptr_t allocation_ptr = (uintptr_t)allocation_ptr;
			entries.InitializeFromBuffer(allocation_ptr, entries_capacity);
			min_values.InitializeFromBuffer(allocation_ptr, 0, min_values_capacity);
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
			DeallocateEx(allocator, entries.GetAllocatedBuffer());
		}

		T GetAverage() const {
			T average = 0;
			entries.ForEach([&](T value) {
				average += value;
			});
			return average / (T)entries.GetSize();
		}

		T GetMinAverage() const {
			T min_average = 0;
			for (unsigned int index = 0; index < min_values.size; index++) {
				min_average += min_values[index];
			}
			return min_average / (T)min_values.size;
		}

		Queue<T> entries;
		// The min values are kept in order - from lowest to highest
		CapacityStream<T> min_values;
		T min;
		T max;
	};

}