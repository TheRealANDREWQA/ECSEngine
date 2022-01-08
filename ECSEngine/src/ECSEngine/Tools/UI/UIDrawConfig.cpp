#include "ecspch.h"
#include "UIDrawConfig.h"
#include "../../Utilities/Assert.h"

namespace ECSEngine {

	namespace Tools {

		UIDrawConfig::UIDrawConfig() : flag_count(0) {
			parameter_start[0] = 0;
		}

		const void* UIDrawConfig::GetParameter(size_t bit_flag) const
		{
			for (size_t index = 0; index < flag_count; index++) {
				if (associated_bits[index] == bit_flag) {
					return parameters + parameter_start[index];
				}
			}
			ECS_ASSERT(false);
			return nullptr;
		}

	}

}