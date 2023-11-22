#include "ecspch.h"
#include "UIDrawConfig.h"
#include "../../Utilities/Assert.h"

namespace ECSEngine {

	namespace Tools {

		UIDrawConfig::UIDrawConfig() : flag_count(0) {
			parameter_start[0] = 0;
		}

		void UIDrawConfig::RemoveFlag(size_t flag_index)
		{
			for (size_t index = 0; index < flag_count; index++) {
				if (associated_bits[index] == flag_index) {
					size_t total_byte_size = parameter_start[flag_count] - parameter_start[index + 1];
					size_t current_parameter_size = parameter_start[index + 1] - parameter_start[index];
					memmove(parameters + parameter_start[index], parameters + parameter_start[index + 1], total_byte_size);
					memmove(associated_bits + index, associated_bits + index + 1, sizeof(size_t) * (flag_count - index - 1));
					for (size_t subindex = index + 1; subindex < flag_count; subindex++) {
						parameter_start[subindex - 1] = parameter_start[subindex] - current_parameter_size;
					}
					flag_count--;
					return;
				}
			}
			ECS_ASSERT(false, "Invalid flag index for UIDrawConfig::RemoveFlag");
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