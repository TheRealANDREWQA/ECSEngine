#include "ecspch.h"
#include "DeserializeTable.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------

	void DeserializeTableCopyPointers(
		Stream<const char*> fields,
		const DeserializeTable& table,
		void* allocation
	) {
		function::CopyPointer copy_pointer;
		uintptr_t ptr = (uintptr_t)allocation;

		for (size_t index = 0; index < fields.size; index++) {
			if (table.TryGetValue(fields[index], copy_pointer)) {
				*copy_pointer.destination = (void*)ptr;
				memcpy((void*)ptr, copy_pointer.data, copy_pointer.data_size);
				ptr += copy_pointer.data_size;
			}
		}
	}

	// -----------------------------------------------------------------------------------------

	bool DeserializeTableHas(
		Stream<const char*> fields,
		const DeserializeTable& table
	) {
		function::CopyPointer copy_pointer;
		for (size_t index = 0; index < fields.size; index++) {
			if (table.Find(fields[index]) == -1) {
				return false;
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeTablePointerDataSize(
		Stream<const char*> fields,
		const DeserializeTable& table
	) {
		size_t total_memory = 0;

		function::CopyPointer copy_pointer;
		for (size_t index = 0; index < fields.size; index++) {
			if (table.TryGetValue(fields[index], copy_pointer)) {
				total_memory += copy_pointer.data_size;
			}
			else {
				return -1;
			}
		}

		return total_memory;
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeTablePointerDataSize(
		Stream<Stream<char>> fields,
		const DeserializeTable& table
	) {
		size_t total_memory = 0;

		function::CopyPointer copy_pointer;
		for (size_t index = 0; index < fields.size; index++) {
			if (table.TryGetValue(fields[index].buffer, copy_pointer)) {
				total_memory += copy_pointer.data_size;
				fields[index].size = copy_pointer.data_size;
			}
			else {
				return -1;
			}
		}

		return total_memory;
	}

	// -----------------------------------------------------------------------------------------

}