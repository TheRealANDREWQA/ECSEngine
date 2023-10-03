#include "ecspch.h"
#include "BasicTypes.h"

namespace ECSEngine {

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