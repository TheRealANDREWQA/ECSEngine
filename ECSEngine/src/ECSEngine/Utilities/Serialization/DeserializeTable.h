#pragma once
#include "../../Containers/HashTable.h"
#include "../../Utilities/Function.h"
#include "../../Internal/InternalStructures.h"

namespace ECSEngine {

	using DeserializeTable = containers::HashTable<
		function::CopyPointer,
		ResourceIdentifier,
		HashFunctionPowerOfTwo,
		HashFunctionMultiplyString
	>;

	// -----------------------------------------------------------------------------------------

	// Copies the given fields into the new allocation
	ECSENGINE_API void DeserializeTableCopyPointers(
		containers::Stream<const char*> fields,
		const DeserializeTable& table,
		void* allocation
	);

	// Verifies that these fields exist
	ECSENGINE_API bool DeserializeTableHas(
		containers::Stream<const char*> fields,
		const DeserializeTable& table
	);

	// Returns the amount of memory that those fields require. It returns -1 if one of the fields doesn't exist
	ECSENGINE_API size_t DeserializeTablePointerDataSize(
		containers::Stream<const char*> fields,
		const DeserializeTable& table
	);

	// Writes the amount of pointer data per field. Stream size will represent the data for that field
	// Returns the total amount of pointer data or -1 if one of the fields doesn't exist
	ECSENGINE_API size_t DeserializeTablePointerDataSize(
		containers::Stream<containers::Stream<char>> fields,
		const DeserializeTable& table
	);

}