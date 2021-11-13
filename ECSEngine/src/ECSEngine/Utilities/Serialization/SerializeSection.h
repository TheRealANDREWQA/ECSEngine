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

	ECSENGINE_API bool SerializeSection(
		const ReflectionManager* reflection,
		const char* ECS_RESTRICT type_name,
		std::ofstream& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeSection(
		ReflectionType type,
		std::ofstream& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeSection(
		const ReflectionManager* reflection,
		const char* ECS_RESTRICT type_name,
		CapacityStream<void>& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------
	
	ECSENGINE_API void SerializeSection(
		ReflectionType type,
		CapacityStream<void>& stream,
		const void* ECS_RESTRICT data,
		Stream<void> header = {nullptr, 0}
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeSection(std::ofstream& stream, Stream<SerializeSectionData> data, Stream<void> header = {nullptr, 0});

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeSection(CapacityStream<void>& stream, Stream<SerializeSectionData> data, Stream<void> header = {nullptr, 0});

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeSectionSize(ReflectionType type, const void* data);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeSectionSize(Stream<SerializeSectionData> data);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, success_status must be true before calling this
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header = nullptr,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, success_status must be true before calling this
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header = nullptr,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, success_status must be true before calling this
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header = nullptr,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes, preinitialize this to a specified value
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header = nullptr,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>* header = nullptr,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		std::ifstream& ECS_RESTRICT stream,
		Stream<const char*> section_names,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT pointers,
		CapacityStream<void>* header = nullptr,
		bool* ECS_RESTRICT success_status = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		std::ifstream& stream,
		Stream<SerializeSectionData> data,
		CapacityStream<void>* header = nullptr,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<const char*> section_names,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& pointers,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data bytes
	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<SerializeSectionData> data,
		CapacityStream<void>* header = nullptr,
		unsigned int* ECS_RESTRICT faulty_index = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSection(
		uintptr_t& stream,
		Stream<SerializeSectionData> data,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type,
		CapacityStream<void>* header = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSection(
		std::ifstream& stream,
		Stream<SerializeSectionData> data,
		void* ECS_RESTRICT allocator,
		AllocatorType allocator_type, 
		CapacityStream<void>* header = nullptr,
		bool* success = nullptr
	);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionCount(uintptr_t stream, size_t header_size = 0);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t DeserializeSectionCount(std::ifstream& stream, size_t header_size = 0);

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

	ECSENGINE_API size_t DeserializeSectionHeaderSize(std::ifstream& stream);

	// -------------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API void DeserializeSectionHeader(uintptr_t data, CapacityStream<void>& header);

	// -------------------------------------------------------------------------------------------------------------------

	// It will not advance the stream so that calling deserialize will work as expected
	ECSENGINE_API bool DeserializeSectionHeader(std::ifstream& stream, CapacityStream<void>& header);

	// -------------------------------------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------------------------------------

}