#pragma once
#include "../../Core.h"

namespace ECSEngine {

	namespace Tools {

		struct ECSENGINE_API UIDrawConfig {
			UIDrawConfig();

			template<typename ConfigFlag>
			void AddFlag(const ConfigFlag& flag) {
				size_t associated_bit = flag.GetAssociatedBit();
				for (size_t index = 0; index < flag_count; index++) {
					ECS_ASSERT(associated_bits[index] != associated_bit, "Repeated flag addition");
				}

				unsigned int parameter_count = sizeof(flag);
				unsigned int new_start = parameter_start[flag_count] + parameter_count;
				ECS_ASSERT(new_start <= sizeof(parameters));
				
				parameter_start[flag_count + 1] = new_start;
				memcpy(parameters + parameter_start[flag_count], &flag, parameter_count);
				associated_bits[flag_count] = associated_bit;
				flag_count++;
			}

			template<typename ConfigFlag1, typename ConfigFlag2>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2) {
				AddFlag(flag1);
				AddFlag(flag2);
			}

			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
			}

			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3, typename ConfigFlag4>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3, const ConfigFlag4& flag4) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
				AddFlag(flag4);
			}

			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3, typename ConfigFlag4, typename ConfigFlag5>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3, const ConfigFlag4& flag4, const ConfigFlag5& flag5) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
				AddFlag(flag4);
				AddFlag(flag5);
			}

			// it returns the replaced config flag
			template<typename ConfigFlag>
			void SetExistingFlag(const ConfigFlag& flag, ConfigFlag& previous_flag) {
				size_t bit_flag = flag.GetAssociatedBit();
				for (size_t index = 0; index < flag_count; index++) {
					if (associated_bits[index] == bit_flag) {
						ConfigFlag* ptr = (ConfigFlag*)(parameters + parameter_start[index]);
						previous_flag = *ptr;
						*ptr = flag;
					}
				}
			}

			template<typename ConfigFlag>
			void RestoreFlag(const ConfigFlag& previous_flag) {
				size_t bit_flag = previous_flag.GetAssociatedBit();
				for (size_t index = 0; index < flag_count; index++) {
					if (associated_bits[index] == bit_flag) {
						ConfigFlag* flag = (ConfigFlag*)(parameters + parameter_start[index]);
						*flag = previous_flag;
						return;
					}
				}
			}

			const void* GetParameter(size_t bit_flag) const;

			size_t flag_count;
			unsigned int parameter_start[16];
			char parameters[320];
			size_t associated_bits[16];
		};

	}

}