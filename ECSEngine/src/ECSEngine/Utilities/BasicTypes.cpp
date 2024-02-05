#include "ecspch.h"
#include "BasicTypes.h"
#include "Assert.h"

namespace ECSEngine {

	size_t GetIntValueUnsigned(const void* ptr, ECS_INT_TYPE type) {
		switch (type) {
		case ECS_INT8:
			return *(unsigned char*)ptr;
		case ECS_INT16:
			return *(unsigned short*)ptr;
		case ECS_INT32:
			return *(unsigned int*)ptr;
		case ECS_INT64:
			return *(unsigned long long*)ptr;
		}

		ECS_ASSERT(false, "Invalid int type");
		return -1;
	}

	int64_t GetIntValueSigned(const void* ptr, ECS_INT_TYPE type) {
		switch (type) {
		case ECS_INT8:
			return *(char*)ptr;
		case ECS_INT16:
			return *(short*)ptr;
		case ECS_INT32:
			return *(int*)ptr;
		case ECS_INT64:
			return *(long long*)ptr;
		}

		ECS_ASSERT(false, "Invalid int type");
		return -1;
	}

	void SetIntValueUnsigned(void* ptr, ECS_INT_TYPE type, size_t value) {
		switch (type) {
		case ECS_INT8:
		{
			*(unsigned char*)ptr = value;
		}
		break;
		case ECS_INT16:
		{
			*(unsigned short*)ptr = value;
		}
		break;
		case ECS_INT32:
		{
			*(unsigned int*)ptr = value;
		}
		break;
		case ECS_INT64:
		{
			*(size_t*)ptr = value;
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid int type");
		}
	}

	void SetIntValueSigned(void* ptr, ECS_INT_TYPE type, int64_t value) {
		switch (type) {
		case ECS_INT8:
		{
			*(char*)ptr = value;
		}
		break;
		case ECS_INT16:
		{
			*(short*)ptr = value;
		}
		break;
		case ECS_INT32:
		{
			*(int*)ptr = value;
		}
		break;
		case ECS_INT64:
		{
			*(long long*)ptr = value;
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid int type");
		}
	}

	bool IsDateLater(Date first, Date second) {
		if (second.year > first.year) {
			return true;
		}
		if (second.year == first.year) {
			if (second.month > first.month) {
				return true;
			}
			if (second.month == first.month) {
				if (second.day > first.day) {
					return true;
				}
				if (second.day == first.day) {
					if (second.hour > first.hour) {
						return true;
					}
					if (second.hour == first.hour) {
						if (second.minute > first.minute) {
							return true;
						}
						if (second.minute == first.minute) {
							if (second.seconds > first.seconds) {
								return true;
							}
							if (second.seconds == first.seconds) {
								return second.milliseconds > first.milliseconds;
							}
							return false;
						}
						return false;
					}
					return false;
				}
				return false;
			}
			return false;
		}
		return false;
	}

}