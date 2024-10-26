#pragma once
#include "../../Core.h"
#include "../Reflection/ReflectionTypes.h"
#include "../Utilities.h"
#include "../ReaderWriterInterface.h"

namespace ECSEngine {

	// Returns the number of written bytes or 0 if the write_instrument returned false
	// If the template parameter write_data is false, it will ignore the first parameter
	template<bool write_data>
	ECSENGINE_API size_t SerializeIntVariableLengthUnsigned(WriteInstrument* write_instrument, size_t value);

	// Returns the number of written bytes or 0 if the write_instrument returned false
	// If the template parameter write_data is false, it will ignore the first parameter
	template<bool write_data>
	ECSENGINE_API size_t SerializeIntVariableLengthSigned(WriteInstrument* write_instrument, int64_t value);

	// Returns the number of written bytes or 0 if the write_instrument returned false
	// If the template parameter write_data is false, it will ignore the first parameter
	template<bool write_data, typename IntType>
	ECS_INLINE size_t SerializeIntVariableLength(WriteInstrument* write_instrument, IntType value) {
		static_assert(std::is_integral_v<IntType>, "SerializeIntVariableLength accepts only integers");

		if constexpr (std::is_unsigned_v<IntType>) {
			return SerializeIntVariableLengthUnsigned<write_data>(write_instrument, (size_t)value);
		}
		else {
			return SerializeIntVariableLengthSigned<write_data>(write_instrument, (int64_t)value);
		}
	}

	// Returns true if the serialization succeeded, else false
	template<typename IntType>
	ECS_INLINE bool SerializeIntVariableLengthBool(WriteInstrument* write_instrument, IntType value) {
		return SerializeIntVariableLength<true, IntType>(write_instrument, value) != 0;
	}

	// Returns true if the serialization succeeded for all integers, else false
	template<typename FirstInteger, typename... Integers>
	ECS_INLINE bool SerializeIntVariableLengthBoolMultiple(WriteInstrument* write_instrument, FirstInteger first_integer, Integers... integers) {
		if (!SerializeIntVariableLengthBool(write_instrument, first_integer)) {
			return false;
		}

		if constexpr (sizeof...(Integers) > 0) {
			return SerializeIntVariableLengthBoolMultiple(write_instrument, integers...);
		}
		return true;
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
	template<bool write_data>
	ECSENGINE_API size_t DeserializeIntVariableLengthUnsigned(ReadInstrument* read_instrument, size_t& value);

	// Returns the number of bytes read or 0 if the capacity functor returned false
	// If the template parameter write_data is false, it will ignore the last 2 parameters
	template<bool write_data>
	ECSENGINE_API size_t DeserializeIntVariableLengthSigned(ReadInstrument* read_instrument, int64_t& value);

	// Returns the number of bytes read or 0 if the capacity functor returned false
	// The boolean parameter is an out parameter that is set to true if a value was parsed, but it lies outside the integer 
	// Boundaries of the given integer range.
	template<bool read_data, typename IntType>
	size_t DeserializeIntVariableLength(ReadInstrument* read_instrument, bool& is_out_of_range, IntType& value) {
		static_assert(std::is_integral_v<IntType>, "DeserializeIntVariableLength accepts only integers");

		if constexpr (std::is_unsigned_v<IntType>) {
			size_t large_unsigned = 0;
			size_t read_count = DeserializeIntVariableLengthUnsigned<read_data>(read_instrument, large_unsigned);
			is_out_of_range |= EnsureUnsignedIntegerIsInRange<IntType>(large_unsigned);
			value = large_unsigned;
			return read_count;
		}
		else {
			int64_t large_signed = 0;
			size_t read_count = DeserializeIntVariableLengthSigned<read_data>(read_instrument, large_signed);
			is_out_of_range |= EnsureSignedIntegerIsInRange<IntType>(large_signed);
			value = large_signed;
			return read_count;
		}
	}

	// Returns true if the deserialization succeeded, else false
	template<typename IntType>
	ECS_INLINE bool DeserializeIntVariableLengthBool(ReadInstrument* read_instrument, bool& is_out_of_range, IntType& value) {
		return DeserializeIntVariableLength<true, IntType>(read_instrument, is_out_of_range, value);
	}

	// Returns true if the deserialization succeeded (i.e. there is enough space in the buffer capacity) and the value is in range, else false
	template<typename IntType>
	ECS_INLINE bool DeserializeIntVariableLengthBool(ReadInstrument* read_instrument, IntType& value) {
		bool is_out_of_range = false;
		if (!DeserializeIntVariableLengthBool(read_instrument, is_out_of_range, value)) {
			return false;
		}
		return is_out_of_range;
	}

	// Returns true if the serialization succeeded for all integers, else false
	// The is out of range boolean is set to true if at least one entry is out of range
	template<typename FirstInteger, typename... Integers>
	ECS_INLINE bool DeserializeIntVariableLengthBoolMultiple(ReadInstrument* read_instrument, bool& is_out_of_range, FirstInteger& first_integer, Integers&... integers) {
		if (!DeserializeIntVariableLengthBool(read_instrument, is_out_of_range, first_integer)) {
			return false;
		}

		if constexpr (sizeof...(Integers) > 0) {
			return DeserializeIntVariableLengthBoolMultiple(read_instrument, is_out_of_range, integers...);
		}
		return true;
	}

	// Returns true if the serialization succeeded for all integers and all integers are in their range, else false
	template<typename FirstInteger, typename... Integers>
	ECS_INLINE bool DeserializeIntVariableLengthBoolMultipleEnsureRange(ReadInstrument* read_instrument, FirstInteger& first_integer, Integers&... integers) {
		bool is_out_of_range = false;
		if (!DeserializeIntVariableLengthBoolMultiple(read_instrument, is_out_of_range, first_integer, integers...)) {
			return false;
		}
		return is_out_of_range;
	}

}