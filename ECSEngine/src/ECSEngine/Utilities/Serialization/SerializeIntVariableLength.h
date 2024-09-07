#pragma once
#include "../../Core.h"
#include "../Reflection/ReflectionTypes.h"
#include "SerializationHelpers.h"

namespace ECSEngine {

	// The functions receive as a template parameter the SerializeBufferCapacityFunctor
	// Instead of a normal function parameter because we don't want to go through an indirection
	// For these one liner functions.

	// Returns the number of written bytes or 0 if the capacity functor returned false
	// If the template parameter write_data is false, it will ignore the first 2 parameters
	template<bool write_data, SerializeBufferCapacityFunctor capacity_functor>
	ECSENGINE_API size_t SerializeIntVariableLengthUnsigned(uintptr_t& ptr, size_t& buffer_capacity, size_t value);

	// Returns the number of written bytes or 0 if the capacity functor returned false
	// If the template parameter write_data is false, it will ignore the first 2 parameters
	template<bool write_data, SerializeBufferCapacityFunctor capacity_functor>
	ECSENGINE_API size_t SerializeIntVariableLengthSigned(uintptr_t& ptr, size_t& buffer_capacity, int64_t value);

	// Returns the number of written bytes or 0 if the capacity functor returned false
	// If the template parameter write_data is false, it will ignore the first 2 parameters
	template<bool write_data, SerializeBufferCapacityFunctor capacity_functor, typename IntType>
	ECS_INLINE size_t SerializeIntVariableLength(uintptr_t& ptr, size_t& buffer_capacity, IntType value) {
		static_assert(std::is_integral_v<IntType>, "SerializeIntVariableLength accepts only integers");

		if constexpr (std::is_unsigned_v<IntType>) {
			return SerializeIntVariableLengthUnsigned<write_data, capacity_functor>(ptr, buffer_capacity, (size_t)value);
		}
		else {
			return SerializeIntVariableLengthSigned<write_data, capacity_functor>(ptr, buffer_capacity, (int64_t)value);
		}
	}

	// Returns the number of written bytes or 0 if the capacity functor returned false
	// If the template parameter write_data is false, it will ignore the first 2 parameters
	template<bool write_data, typename IntType>
	ECS_INLINE size_t SerializeIntVariableLength(uintptr_t& ptr, IntType value) {
		size_t buffer_capacity = 0;
		return SerializeIntVariableLength<write_data, SerializeBufferCapacityTrue, IntType>(ptr, buffer_capacity, value);
	}

	// Returns true if the serialization succeeded (i.e. there is enough capacity in the buffer), else false
	template<typename IntType>
	ECS_INLINE bool SerializeIntVariableLengthBool(uintptr_t& ptr, size_t& buffer_capacity, IntType value) {
		return SerializeIntVariableLength<true, SerializeBufferCapacityBool, IntType>(ptr, buffer_capacity, value) != 0;
	}

	// Returns true if the serialization succeeded for all integers, else false
	template<typename FirstInteger, typename... Integers>
	ECS_INLINE bool SerializeIntVariableLengthBoolMultiple(uintptr_t& ptr, size_t& buffer_capacity, FirstInteger first_integer, Integers... integers) {
		if (!SerializeIntVariableLengthBool(ptr, buffer_capacity, first_integer)) {
			return false;
		}

		if constexpr (sizeof...(Integers) > 0) {
			return SerializeIntVariableLengthBoolMultiple(ptr, buffer_capacity, integers...);
		}
	}

	// Returns the maximum number of bytes the variable length serialization can use for the given integer type.
	template<typename IntType>
	ECS_INLINE size_t SerializeIntVariableLengthMaxSize() {
		if constexpr (sizeof(IntType) == 1) {
			// At maximum 2 bytes for a single byte integer
			return 2;
		}
		else if constexpr (sizeof(IntType) == 2) {
			// At a maximum 3 bytes for a double byte integer
			return 3;
		}
		else if constexpr (sizeof(IntType) == 4) {
			// At a maximum 5 bytes for a 4 byte integer
			return 5;
		}
		else {
			// At a maximum 10 bytes for 8 byte integer
			return 10;
		}
	}

	// Returns the number of bytes read or 0 if the capacity functor returned false
	// If the template parameter write_data is false, it will ignore the last 2 parameters
	template<bool write_data, SerializeBufferCapacityFunctor capacity_functor>
	ECSENGINE_API size_t DeserializeIntVariableLengthUnsigned(uintptr_t& ptr, size_t& buffer_capacity, size_t& value);

	// Returns the number of bytes read or 0 if the capacity functor returned false
	// If the template parameter write_data is false, it will ignore the last 2 parameters
	template<bool write_data, SerializeBufferCapacityFunctor capacity_functor>
	ECSENGINE_API size_t DeserializeIntVariableLengthSigned(uintptr_t& ptr, size_t& buffer_capacity, int64_t& value);

	// Returns the number of bytes read or 0 if the capacity functor returned false
	// The boolean parameter is an out parameter that is set to true if a value was parsed, but it lies outside the integer 
	// Boundaries of the given value.
	template<bool read_data, SerializeBufferCapacityFunctor capacity_functor, typename IntType>
	size_t DeserializeIntVariableLength(uintptr_t& ptr, size_t& buffer_capacity, bool& is_out_of_range, IntType& value) {
		static_assert(std::is_integral_v<IntType>, "DeserializeIntVariableLength accepts only integers");

		if constexpr (std::is_unsigned_v<IntType>) {
			size_t large_unsigned = 0;
			size_t read_count = DeserializeIntVariableLengthUnsigned<read_data, capacity_functor>(ptr, buffer_capacity, large_unsigned);
			is_out_of_range |= EnsureUnsignedIntegerIsInRange<IntType>(large_unsigned);
			value = large_unsigned;
			return read_count;
		}
		else {
			int64_t large_signed = 0;
			size_t read_count = DeserializeIntVariableLengthSigned<read_data, capacity_functor>(ptr, buffer_capacity, large_signed);
			is_out_of_range |= EnsureSignedIntegerIsInRange<IntType>(large_signed);
			value = large_signed;
			return read_count;
		}
	}

	// Returns the number of bytes read. The boolean parameter is an out parameter that is set to true if a value
	// Was parsed, but it lies outside the integer boundaries of the given value.
	template<bool read_data, typename IntType>
	ECS_INLINE size_t DeserializeIntVariableLength(uintptr_t& ptr, bool& is_out_of_range, IntType& value) {
		size_t buffer_capacity = 0;
		return DeserializeIntVariableLength<read_data, SerializeBufferCapacityTrue, IntType>(ptr, buffer_capacity, is_out_of_range, value);
	}

	// Returns true if the deserialization succeeded (i.e. there is enough space in the buffer capacity), else false
	template<typename IntType>
	ECS_INLINE bool DeserializeIntVariableLengthBool(uintptr_t& ptr, size_t& buffer_capacity, bool& is_out_of_range, IntType& value) {
		return DeserializeIntVariableLength<true, SerializeBufferCapacityBool, IntType>(ptr, buffer_capacity, is_out_of_range, value);
	}

	// Returns true if the deserialization succeeded (i.e. there is enough space in the buffer capacity) and the value is in range, else false
	template<typename IntType>
	ECS_INLINE bool DeserializeIntVariableLengthBool(uintptr_t& ptr, size_t& buffer_capacity, IntType& value) {
		bool is_out_of_range = false;
		if (!DeserializeIntVariableLengthBool(ptr, buffer_capacity, is_out_of_range, value)) {
			return false;
		}
		return is_out_of_range;
	}

	// Returns true if the serialization succeeded for all integers, else false
	// The is out of range boolean is set to true if at least one entry is out of range
	template<typename FirstInteger, typename... Integers>
	ECS_INLINE bool DeserializeIntVariableLengthBoolMultiple(uintptr_t& ptr, size_t& buffer_capacity, bool& is_out_of_range, FirstInteger& first_integer, Integers... integers) {
		if (!DeserializeIntVariableLengthBoolMultiple(ptr, buffer_capacity, is_out_of_range, first_integer)) {
			return false;
		}

		if constexpr (sizeof...(Integers) > 0) {
			return DeserializeIntVariableLengthBoolMultiple(ptr, buffer_capacity, is_out_of_range, integers...);
		}
	}

	// Returns true if the serialization succeeded for all integers and all integers are in their range, else false
	template<typename FirstInteger, typename... Integers>
	ECS_INLINE bool DeserializeIntVariableLengthBoolMultipleEnsureRange(uintptr_t& ptr, size_t& buffer_capacity, FirstInteger& first_integer, Integers... integers) {
		bool is_out_of_range = false;
		if (!DeserializeIntVariableLengthBoolMultiple(ptr, buffer_capacity, is_out_of_range, first_integer, integers...)) {
			return false;
		}
		return is_out_of_range;
	}

}