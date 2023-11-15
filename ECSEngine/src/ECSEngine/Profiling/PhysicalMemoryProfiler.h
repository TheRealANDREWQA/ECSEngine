#pragma once
#include "../Core.h"
#include "Statistic.h"
#include "../Utilities/ByteUnits.h"

namespace ECSEngine {

	struct ECSENGINE_API PhysicalMemoryProfiler {
		void Begin();

		void Clear();

		void End();

		// Returns the value of bytes
		ECS_INLINE size_t GetUsage(ECS_STATISTIC_VALUE_TYPE value_type, ECS_BYTE_UNIT_TYPE unit_type) const {
			return ConvertToByteUnit(memory_usage.GetValue(value_type), unit_type);
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int entry_capacity);

		// The values are expressed in bytes
		Statistic<size_t> memory_usage;
		size_t last_process_usage;
	};

}