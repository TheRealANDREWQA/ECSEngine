#include "ecspch.h"
#include "SerializeIntVariableLength.h"
#include "../Utilities.h"

namespace ECSEngine
{
#define EXPORT(function_name, int_type)	template ECSENGINE_API size_t function_name<false, SerializeBufferCapacityTrue>(uintptr_t&, size_t&, int_type); \
										template ECSENGINE_API size_t function_name<false, SerializeBufferCapacityAssert>(uintptr_t&, size_t&, int_type); \
										template ECSENGINE_API size_t function_name<false, SerializeBufferCapacityBool>(uintptr_t&, size_t&, int_type); \
										template ECSENGINE_API size_t function_name<true, SerializeBufferCapacityTrue>(uintptr_t&, size_t&, int_type); \
										template ECSENGINE_API size_t function_name<true, SerializeBufferCapacityAssert>(uintptr_t&, size_t&, int_type); \
										template ECSENGINE_API size_t function_name<true, SerializeBufferCapacityBool>(uintptr_t&, size_t&, int_type);

	// Use a "Unicode" like encoding, where the last bit of a byte indicates whether or not the
	// There is a next byte. For signed integers, the last byte uses another bit to indicate whether
	// The value is negative or not.

	template<bool write_data, SerializeBufferCapacityFunctor capacity_functor>
	static size_t SerializeIntVariableLengthUnsigned(uintptr_t& ptr, size_t& buffer_capacity, size_t value) {
		// Find the last bit set, we need to write up to its byte
		unsigned int last_bit_set = FirstMSB64(value);

		size_t byte_count = 0;
		// If it is -1, then there are no bits set, and must not enter the while
		if (last_bit_set != -1) {
			// We can write 7 value bits per byte
			while (last_bit_set >= 7) {
				if constexpr (write_data) {
					if (!capacity_functor(1, buffer_capacity)) {
						return 0;
					}
					unsigned char* byte = (unsigned char*)ptr;
					*byte = ECS_BIT(7) | (unsigned char)value;
					ptr += 1;
				}
				last_bit_set -= 7;
				value >>= 7;
				byte_count++;
			}
		}

		// There is one more byte to write
		if constexpr (write_data) {
			if (!capacity_functor(1, buffer_capacity)) {
				return 0;
			}
			unsigned char* byte = (unsigned char*)ptr;
			*byte = (unsigned char)value;
			ptr += 1;
		}
		byte_count++;
		return byte_count;
	}

	EXPORT(SerializeIntVariableLengthUnsigned, size_t);

	template<bool write_data, SerializeBufferCapacityFunctor capacity_functor>
	static size_t SerializeIntVariableLengthSigned(uintptr_t& ptr, size_t& buffer_capacity, int64_t value) {
		int64_t extended_value = value;
		int last_bit = -1;
		char sign_bit = 0;
		if (extended_value >= 0) {
			// When the value is positive, look for the last set bit
			last_bit = (int)FirstMSB64(value);
		}
		else {
			// There is no intrinsic for this, do it manually
			// Start from the bit 62, since the bit 63 is set (the sign bit)
			int64_t zero_bit = ECS_BIT_SIGNED(62);
			last_bit = 62;
			while ((extended_value & zero_bit) != 0) {
				zero_bit >>= 1;
				last_bit--;
			}
			// This works fine even when all bits are 1, the last bit will be -1
			// This is the sign bit of the last byte
			sign_bit = ECS_BIT_SIGNED(6);
		}

		// We can write 7 value bits per byte for all bytes except the last one
		size_t byte_count = 0;
		if (last_bit >= 7) {
			while (last_bit >= 7) {
				if constexpr (write_data) {
					if (!capacity_functor(1, buffer_capacity)) {
						return 0;
					}
					char* byte = (char*)ptr;
					*byte = ECS_BIT_SIGNED(7) | (char)extended_value;
					ptr += 1;
				}
				last_bit -= 7;
				extended_value >>= 7;
				byte_count++;
			}
		}

		// Check to see if the last bit is 6, for that case, we must write another byte
		if (last_bit == 6) {
			if constexpr (write_data) {
				if (!capacity_functor(1, buffer_capacity)) {
					return 0;
				}
				char* byte = (char*)ptr;
				*byte = ECS_BIT_SIGNED(7) | (char)extended_value;
				ptr += 1;
			}
			// Here, the shift must be done with a 7 as well, in order for the last bit
			// To be completely removed.
			extended_value >>= 7;
			byte_count++;
		}

		// Write the last byte
		if constexpr (write_data) {
			if (!capacity_functor(1, buffer_capacity)) {
				return 0;
			}
			char* byte = (char*)ptr;
			// Make sure to zero out the MSB, since it may be 1 with signed integers
			*byte = (sign_bit | (char)extended_value) & (~(ECS_BIT_SIGNED(7)));
			ptr += 1;
		}
		byte_count++;
		return byte_count;
	}

	EXPORT(SerializeIntVariableLengthSigned, int64_t);

	template<bool read_data, SerializeBufferCapacityFunctor capacity_functor>
	static size_t DeserializeIntVariableLengthUnsigned(uintptr_t& ptr, size_t& buffer_capacity, size_t& value) {
		if (!capacity_functor(1, buffer_capacity)) {
			return 0;
		}

		uintptr_t initial_ptr = ptr;

		// Read until the MSB is 0
		unsigned char* bytes = (unsigned char*)ptr;
		unsigned char byte_MSB = ECS_BIT(7);
		unsigned char negated_byte_MSB = ~byte_MSB;
		value = 0;
		
		unsigned char current_byte = *bytes;
		size_t shift_count = 0;
		while ((current_byte & byte_MSB) != 0) {
			value |= (size_t)(current_byte & negated_byte_MSB) << shift_count;
			shift_count += 7;
			bytes++;
			current_byte = *bytes;
			ptr++;
			if (!capacity_functor(1, buffer_capacity)) {
				return 0;
			}
		}

		// Perform one more iteration
		value |= (size_t)current_byte << shift_count;
		ptr++;
		return ptr - initial_ptr;
	}

	EXPORT(DeserializeIntVariableLengthUnsigned, size_t&);

	template<bool read_data, SerializeBufferCapacityFunctor capacity_functor>
	static size_t DeserializeIntVariableLengthSigned(uintptr_t& ptr, size_t& buffer_capacity, int64_t& value) {
		if (!capacity_functor(1, buffer_capacity)) {
			return 0;
		}

		uintptr_t initial_ptr = ptr;

		// This function is largely the same as the unsigned version
		// The difference lies in the last part, where we need to read
		// The additional bit and determine how to extend the integer
		
		// Read until the MSB is 0
		char* bytes = (char*)ptr;
		char byte_MSB = ECS_BIT_SIGNED(7);
		char negated_byte_MSB = ~byte_MSB;
		value = 0;

		char current_byte = *bytes;
		int64_t shift_count = 0;
		while ((current_byte & byte_MSB) != 0) {
			value |= (int64_t)(current_byte & negated_byte_MSB) << shift_count;
			shift_count += 7;
			bytes++;
			current_byte = *bytes;
			ptr++;
			if (!capacity_functor(1, buffer_capacity)) {
				return 0;
			}
		}

		const int64_t VALUE_BIT_COUNT = sizeof(value) * 8;
		value |= (int64_t)(current_byte & negated_byte_MSB) << shift_count;
		// To perform the appropriate sign extension, we must simply use a shift left
		// To bring the sign bit as MSB and then a shift right. This works for both signed
		// And unsigned integers. There is the edge case when the shift count goes out of bounds.
		// In that case, don't do anything
		shift_count += 7;
		if (shift_count < VALUE_BIT_COUNT) {
			value <<= VALUE_BIT_COUNT - shift_count;
			value >>= VALUE_BIT_COUNT - shift_count;
		}
		ptr++;
		
		return ptr - initial_ptr;
	}

	EXPORT(DeserializeIntVariableLengthSigned, int64_t&);

}