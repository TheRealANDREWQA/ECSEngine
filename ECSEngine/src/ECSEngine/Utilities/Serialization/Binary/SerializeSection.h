#pragma once
#include "../../../Containers/Stream.h"

ECS_CONTAINERS;

namespace ECSEngine {

	struct SerializeSectionData {
		Stream<void> data;
		const char* name;
	};

	// -------------------------------------------------------------------------------------------------------------------

	// A temporary buffer will be allocated and filled with the serialization data and then commited to the file
	// Allocator nullptr means use malloc
	// It will return false if the file write fails
	ECSENGINE_API bool SerializeSection(
		Stream<SerializeSectionData> data,
		Stream<wchar_t> stream, 
		AllocatorPolymorphic allocator = { nullptr },
		Stream<void> header = { nullptr, 0 }
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeSection(Stream<SerializeSectionData> data, uintptr_t& stream, Stream<void> header = {nullptr, 0});

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeSectionSize(Stream<SerializeSectionData> data);

	// -------------------------------------------------------------------------------------------------------------------

	// Return of nullptr means failure to open and read the file. The allocator will be used to read the whole inside a single allocation and
	// all sections will point inside this allocation. You must free this buffer after you are done with it!!
	ECSENGINE_API void* DeserializeSection(
		CapacityStream<SerializeSectionData>& sections,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr },
		CapacityStream<void>* header = nullptr
	);

	// Return of nullptr means failure to open and read the file. The allocator will be used to read the whole inside a single allocation and
	// all sections will point inside this allocation. You must free this buffer after you are done with it!!
	// Data must have the names set before hand and only these sections will be set, but the allocated size will still be that of the whole file
	ECSENGINE_API void* DeserializeSectionWithMatch(
		Stream<SerializeSectionData> sections,
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator = { nullptr },
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of data bytes. It will fill in all the sections that were found inside the file
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		CapacityStream<SerializeSectionData>& sections,
		uintptr_t& stream,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of data bytes. Data must have the names set before hand in order to filter only those.
	// A section which does not have a match will be { nullptr, 0 }
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSectionWithMatch(
		Stream<SerializeSectionData> sections,
		uintptr_t& stream,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionCount(uintptr_t stream, size_t header_size = 0);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of total section data needed for these fields. Sections must have the name set before hand
	// and for each entry the size member will be updated to reflect the size of that entry
	ECSENGINE_API size_t DeserializeSectionSize(uintptr_t data, Stream<SerializeSectionData> sections);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionHeaderSize(uintptr_t data);

	// -------------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API void DeserializeSectionHeader(uintptr_t data, CapacityStream<void>& header);

	// -------------------------------------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------------------------------------

}