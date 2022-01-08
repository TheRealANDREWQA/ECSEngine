#pragma once
#include "../Reflection/Reflection.h"
#include "../../Allocators/AllocatorTypes.h"

ECS_CONTAINERS;


namespace ECSEngine {

	using namespace Reflection;

	struct SerializeSectionData {
		Stream<void> data;
		const char* name;
	};

	// -------------------------------------------------------------------------------------------------------------------

	// A temporary buffer will be allocated and filled with the serialization data and then commited to the file
	// Allocator nullptr means use malloc
	// It will return false if the file write fails
	ECSENGINE_API bool SerializeSection(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator = {nullptr},
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------

	// A temporary buffer will be allocated and filled with the serialization data and then commited to the file
	// Allocator nullptr means use malloc
	// It will return false if the file write fails
	ECSENGINE_API bool SerializeSection(
		ReflectionType type,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator = {nullptr},
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeSection(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		const void* data,
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------
	
	ECSENGINE_API void SerializeSection(
		ReflectionType type,
		uintptr_t& stream,
		const void* data,
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------

	// A temporary buffer will be allocated and filled with the serialization data and then commited to the file
	// Allocator nullptr means use malloc
	// It will return false if the file write fails
	ECSENGINE_API bool SerializeSection(Stream<wchar_t> stream, Stream<SerializeSectionData> data, AllocatorPolymorphic allocator = { nullptr }, Stream<void> header = { nullptr, 0 });

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeSection(uintptr_t& stream, Stream<SerializeSectionData> data, Stream<void> header = {nullptr, 0});

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeSectionSize(ReflectionType type, const void* data);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeSectionSize(Stream<SerializeSectionData> data);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be placed into the memory pool
	// The allocator is needed to read the file contents into a single temporary buffer
	// Allocator nullptr means use malloc
	// A value of -1 reports failure
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		AllocatorPolymorphic allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be written directly into where the pointers of address are set
	// The allocator is needed to read the file contents into a single temporary buffer 
	// Allocator nullptr means use malloc
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		void* address,
		AllocatorPolymorphic allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be placed into the memory pool
	// The allocator is needed to read the file contents into a single temporary buffer
	// Allocator nullptr means use malloc
	// A value of -1 reports failure
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		AllocatorPolymorphic allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be written directly into where the pointers of address are set
	// The allocator is needed to read the file contents into a single temporary buffer 
	// Allocator nullptr means use malloc
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		Stream<wchar_t> stream,
		void* address,
		AllocatorPolymorphic allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be placed into the memory pool
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be written directly into where the pointers of address are set
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be placed into the memory pool
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be written directly into where the pointers of address are set
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be placed into the memory pool
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		Stream<wchar_t> file,
		Stream<const char*> section_names,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		AllocatorPolymorphic allocator = {nullptr},
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be placed into the memory pool
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<const char*> section_names,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// A value of -1 report failure - the pointer data size was not big enough to hold the data
	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<SerializeSectionData> data,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be allocated individually from the allocator
	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<SerializeSectionData> data,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	// The pointer data will be allocated individually from the allocator
	// The allocator also will be used to create a temporary memory buffer into which the file contents
	// will be placed
	// A value of -1 report failure
	ECSENGINE_API size_t DeserializeSection(
		Stream<wchar_t> file,
		Stream<SerializeSectionData> data,
		AllocatorPolymorphic allocator,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionCount(uintptr_t stream, size_t header_size = 0);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionSize(uintptr_t data, Stream<SerializeSectionData> serialize_data);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionSize(uintptr_t data, Stream<const char*> section_names);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionSize(uintptr_t data, ReflectionType type);

	// -------------------------------------------------------------------------------------------------------------------

	// All sections will be considered; These are byte sizes
	ECSENGINE_API void DeserializeSectionStreamSizes(uintptr_t data, CapacityStream<size_t>& sizes);

	// -------------------------------------------------------------------------------------------------------------------

	// Sections names is used to select only the regions that are of interest
	// It will do a linear search through the section names - not suited for huge number
	// of sections. A hash table implementation could be helpful for that
	// These are byte sizes
	ECSENGINE_API void DeserializeSectionStreamSizes(
		uintptr_t data, 
		Stream<size_t>& sizes,
		Stream<const char*> section_names
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionHeaderSize(uintptr_t data);

	// -------------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API void DeserializeSectionHeader(uintptr_t data, CapacityStream<void>& header);

	// -------------------------------------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------------------------------------

}