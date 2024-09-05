#pragma once
#include "../../Core.h"
#include "../Reflection/ReflectionTypes.h"

namespace ECSEngine {

	// Returns the number of written bytes
	template<bool write_data, typename IntType>
	ECSENGINE_API size_t SerializeIntVariableLength(uintptr_t& ptr, IntType value);

	// Returns the number of written bytes, while ensuring that it does not go overbounds (it will assert).
	// If the template parameter write_data is false, it will ignore the first 2 parameters
	template<bool write_data, typename IntType>
	ECSENGINE_API size_t SerializeIntVariableLength(uintptr_t& ptr, size_t& buffer_capacity, IntType value);

	// Returns the number of bytes read. The boolean parameter is an out parameter that is set to true if a value
	// Was parsed, but it lies outside the integer boundaries of the given value.
	template<bool read_data, typename IntType>
	ECSENGINE_API size_t DeserializeIntVariableLength(uintptr_t& ptr, IntType& value, bool& is_out_of_range);

	// Returns the number of bytes read, while also failing with an assert if it goes overbounds. 
	// The boolean parameter is an out parameter that is set to true if a value was parsed, but it lies outside the integer 
	// Boundaries of the given value.
	template<bool read_data, typename IntType>
	ECSENGINE_API size_t DeserializeIntVariableLength(uintptr_t& ptr, size_t& buffer_capacity, IntType& value, bool& is_out_of_range);

}