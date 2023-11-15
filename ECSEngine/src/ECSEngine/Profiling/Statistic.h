#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/Queues.h"
#include "../Utilities/Utilities.h"

namespace ECSEngine {

#define ECS_STATISTIC_SMALL_AVERAGE_PERCENTAGE 0.2f

	enum ECS_STATISTIC_VALUE_TYPE : unsigned char {
		// The last value from the statistic
		ECS_STATISTIC_VALUE_INSTANT,
		// An average of a small set of the last values
		ECS_STATISTIC_VALUE_SMALL_AVERAGE,
		// The overall average
		ECS_STATISTIC_VALUE_AVERAGE
	};

	template<typename T>
	struct Statistic {
		void Add(T value) {
			entries.Push(value);
			min = value < min ? value : min;
			max = value > max ? value : max;
		}

		void Clear() {
			entries.Reset();
			InitializeMinMax();
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) const {
			DeallocateEx(allocator, entries.GetAllocatedBuffer());
		}

		T GetValue(ECS_STATISTIC_VALUE_TYPE value_type) const {
			// We double to calculate the value since if this is a small int like unsigned char,
			// unsigned short it can overflow and give back incorrect results
			double value = 0;
			// Use this check to avoid any divisions by zero
			// Or incorrect acces
			if (entries.GetSize() > 0) {
				if (value_type == ECS_STATISTIC_VALUE_AVERAGE) {
					entries.ForEach([&](T current_value) {
						value += (double)current_value;
					});
					value /= (double)entries.GetSize();
				}
				else if (value_type >= ECS_STATISTIC_VALUE_SMALL_AVERAGE) {
					unsigned int count = (unsigned int)((float)entries.GetSize() * ECS_STATISTIC_SMALL_AVERAGE_PERCENTAGE);
					// Make it at least contain one value
					count = count == 0 ? 1 : count;
					for (unsigned int index = 0; index < count; index++) {
						value += entries.PushPeekByIndex(index);
					}
					value /= (double)count;
				}
				else {
					value = entries.PushPeekByIndex(0);
				}
			}

			return (T)value;
		}

		void InitializeMinMax() {
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
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int entries_capacity) {
			InitializeMinMax();

			entries_capacity = entries_capacity == 0 ? 1 : entries_capacity;
			size_t allocation_size = entries.MemoryOf(entries_capacity);
			void* allocation = AllocateEx(allocator, allocation_size);
			uintptr_t allocation_ptr = (uintptr_t)allocation;
			entries.InitializeFromBuffer(allocation_ptr, entries_capacity);
		}

		ECS_INLINE static size_t MemoryOf(unsigned int entry_capacity) {
			return Queue<T>::MemoryOf(entry_capacity);
		}

		Queue<T> entries;
		T min;
		T max;
	};

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	// These variants are provided such that other files can use these declarations
	// Without including Reflection.h

	// Returns the number of statistics necessary for this type. Works for nested types as well
	// But it must contain only elementary types except pointers (so only integers, floats or doubles)
	ECSENGINE_API size_t StatisticCountForReflectionType(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> type_name
	);

	// Updates the statistics with the values from the given instance. It returns the pointer of statistics
	// Right after those that were updated (so you can keep going if there are multiple types)
	ECSENGINE_API Statistic<float>* FillStatisticsForReflectionType(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> type_name,
		const void* instance,
		Statistic<float>* statistics
	);

	// Returns the number of statistics necessary for this type. Works for nested types as well
	// But it must contain only elementary types except pointers (so only integers, floats or doubles)
	ECSENGINE_API size_t StatisticCountForReflectionType(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type
	);

	// Updates the statistics with the values from the given instance. It returns the pointer of statistics
	// Right after those that were updated (so you can keep going if there are multiple types)
	ECSENGINE_API Statistic<float>* FillStatisticsForReflectionType(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type, 
		const void* instance,
		Statistic<float>* statistics
	);

}