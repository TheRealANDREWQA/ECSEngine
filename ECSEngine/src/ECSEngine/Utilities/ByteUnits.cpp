#include "ecspch.h"
#include "ByteUnits.h"

namespace ECSEngine {

	size_t ConvertToByteUnit(size_t bytes, ECS_BYTE_UNIT_TYPE type) {
		switch (type) {
		case ECS_BYTE_TO_BYTE:
			return bytes;
		case ECS_BYTE_TO_KB:
			return bytes / ECS_KB;
		case ECS_BYTE_TO_MB:
			return bytes / ECS_MB;
		case ECS_BYTE_TO_GB:
			return bytes / ECS_GB;
		case ECS_BYTE_TO_TB:
			return bytes / ECS_TB;
		}

		// Shouldn't be reached
		return 0;
	}

	size_t ConvertToAppropriateByteUnit(size_t bytes, ECS_BYTE_UNIT_TYPE* type) {
		if (bytes < ECS_KB) {
			*type = ECS_BYTE_TO_BYTE;
			return bytes;
		}
		else if (bytes < ECS_MB) {
			*type = ECS_BYTE_TO_KB;
			return bytes / ECS_KB;
		}
		else if (bytes < ECS_GB) {
			*type = ECS_BYTE_TO_MB;
			return bytes / ECS_BYTE_TO_MB;
		}
		else if (bytes < ECS_TB) {
			*type = ECS_BYTE_TO_GB;
			return bytes / ECS_BYTE_TO_GB;
		}
		else {
			*type = ECS_BYTE_TO_TB;
			return bytes / ECS_BYTE_TO_TB;
		}
	}

}