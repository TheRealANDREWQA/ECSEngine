#include "ecspch.h"
#include "Serialization.h"
#include "SerializationHelpers.h"
#include "../../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	const char* ECS_PLATFORM_STRINGS[] = {
		STRING(Win64/DX11)
	};

	// -----------------------------------------------------------------------------------------

	void ConstructPointerFieldsForType(Stream<function::CopyPointer>& ECS_RESTRICT pointer_fields, ReflectionType type, const void* ECS_RESTRICT data) {
		for (size_t index = 0; index < type.fields.size; index++) {
			if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointer_fields[pointer_fields.size].destination = &stream->buffer;
				pointer_fields[pointer_fields.size].data = stream->buffer;
				pointer_fields[pointer_fields.size++].data_size = type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointer_fields[pointer_fields.size].destination = &stream->buffer;
				pointer_fields[pointer_fields.size].data = stream->buffer;
				pointer_fields[pointer_fields.size++].data_size = type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<void*, LinearAllocator>* stream = (ResizableStream<void*, LinearAllocator>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointer_fields[pointer_fields.size].destination = (void**)&stream->buffer;
				pointer_fields[pointer_fields.size].data = stream->buffer;
				pointer_fields[pointer_fields.size++].data_size = type.fields[index].info.additional_flags * stream->size;
			}
		}
	}

	// -----------------------------------------------------------------------------------------

	void SerializeInternal(
		ReflectionType type,
		uintptr_t& stream,
		const void* data
	) {
		function::CopyPointer pointer_fields_buffer[128];
		// It will contain pairs of pointer offset from the type base and total stream memory size
		Stream<function::CopyPointer> pointer_fields = Stream<function::CopyPointer>(pointer_fields_buffer, 0);

		ConstructPointerFieldsForType(pointer_fields, type, data);

		for (size_t index = 0; index < type.fields.size; index++) {
			Write(stream, (const void*)((uintptr_t)data + type.fields[index].info.pointer_offset), type.fields[index].info.byte_size);
		}

		for (size_t index = 0; index < pointer_fields.size; index++) {
			Write(stream, &pointer_fields[index].data_size, sizeof(size_t));
			Write(stream, pointer_fields[index].data, pointer_fields[index].data_size);
		}
	}
	
	// -----------------------------------------------------------------------------------------

	size_t DeserializeInternal(
		ReflectionType type,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointer_to_copy
	) {
		unsigned int memory_pool_before_size = memory_pool.size;

		for (size_t index = 0; index < type.fields.size; index++) {
			Read(stream, function::OffsetPointer(address, type.fields[index].info.pointer_offset), type.fields[index].info.byte_size);
		}

		ConstructPointerFieldsForType(type_pointer_to_copy, type, address);

		for (size_t index = 0; index < type_pointer_to_copy.size; index++) {
			type_pointer_to_copy[index].data = function::OffsetPointer(memory_pool.buffer, memory_pool.size);
			Read(stream, &type_pointer_to_copy[index].data_size, sizeof(size_t));
			Read(stream, memory_pool, type_pointer_to_copy[index].data_size);
		}

		return memory_pool.size - memory_pool_before_size;
	}

	// -----------------------------------------------------------------------------------------

	bool Serialize(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return Serialize(type, file, data, allocator);
	}
	
	// -----------------------------------------------------------------------------------------

	bool Serialize(
		ReflectionType type,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator
	) {
		size_t serialize_size = SerializeSize(type, data);
		return SerializeWriteFile(file, allocator, serialize_size, [=](uintptr_t& buffer) {
			SerializeInternal(type, buffer, data);
		});
	}

	// -----------------------------------------------------------------------------------------

	void Serialize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		const void* data
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		SerializeInternal(type, stream, data);
	}

	// -----------------------------------------------------------------------------------------

	void Serialize(ReflectionType type, uintptr_t& stream, const void* data)
	{
		SerializeInternal(type, stream, data);
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeSize(
		ReflectionType type,
		const void* data
	)
	{
		function::CopyPointer pointer_fields_buffer[128];
		Stream<function::CopyPointer> pointer_fields = Stream<function::CopyPointer>(pointer_fields_buffer, 0);
		ConstructPointerFieldsForType(pointer_fields, type, data);

		size_t total_size = 0;
		for (size_t index = 0; index < pointer_fields.size; index++) {
			total_size += pointer_fields[index].data_size;
		}

		return total_size;
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeSize(
		const ReflectionManager* reflection,
		const char* type_name,
		const void* data
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return SerializeSize(type, data);
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy,
		AllocatorPolymorphic allocator
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return Deserialize(type, file, address, memory_pool, type_pointers_to_copy, allocator);
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		ReflectionType type,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy,
		AllocatorPolymorphic allocator
	)
	{
		return DeserializeReadFile(file, allocator, [&](uintptr_t& buffer) {
			return DeserializeInternal(type, buffer, address, memory_pool, type_pointers_to_copy);
		});
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy
	) {
		ReflectionType type = reflection->GetType(type_name);
		return Deserialize(type, stream, address, memory_pool, type_pointers_to_copy);
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		ReflectionType type,
		uintptr_t& ptr_to_read,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer>& type_pointers_to_copy
	)
	{
		return DeserializeInternal(type, ptr_to_read, address, memory_pool, type_pointers_to_copy);
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeSize(
		ReflectionType type,
		uintptr_t ptr_to_read,
		void* address
	)
	{
		size_t total_size = type.fields[type.fields.size - 1].info.pointer_offset + type.fields[type.fields.size - 1].info.byte_size;
		Read(ptr_to_read, address, total_size);

		size_t total_memory = 0;
		for (size_t index = 0; index < type.fields.size; index++) {
			if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Stream) {
				const Stream<void>* stream = (const Stream<void>*)((uintptr_t)address + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				const CapacityStream<void>* stream = (const CapacityStream<void>*)((uintptr_t)address + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				const ResizableStream<void*, LinearAllocator>* stream = (const ResizableStream<void*, LinearAllocator>*)((uintptr_t)address + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
		}

		return total_memory;
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeSize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t ptr_to_read,
		void* address
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeSize(type, ptr_to_read, address);
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeSize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t ptr_to_read
	)
	{
		ReflectionType type = reflection->GetType(type_name);

		return DeserializeSize(type, ptr_to_read);
	}

	size_t DeserializeSize(ReflectionType type, uintptr_t ptr_to_read)
	{
		size_t temp_memory[1024];
		size_t total_size = type.fields[type.fields.size - 1].info.pointer_offset + type.fields[type.fields.size - 1].info.byte_size;
		ECS_ASSERT(total_size < sizeof(temp_memory));
		Read(ptr_to_read, temp_memory, total_size);

		size_t total_memory = 0;
		for (size_t index = 0; index < type.fields.size; index++) {
			if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Stream) {
				const Stream<void>* stream = (const Stream<void>*)((uintptr_t)temp_memory + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				const CapacityStream<void>* stream = (const CapacityStream<void>*)((uintptr_t)temp_memory + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				const ResizableStream<void*, LinearAllocator>* stream = (const ResizableStream<void*, LinearAllocator>*)((uintptr_t)temp_memory + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
		}

		return total_memory;
	}

	bool ValidateEnums(
		const ReflectionManager* reflection,
		const char* type_name,
		const void* data
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return ValidateEnums(reflection, type, data);
	}

	bool ValidateEnums(const ReflectionManager* reflection, ReflectionType type, const void* data)
	{
		for (size_t index = 0; index < type.fields.size; index++) {
			if (type.fields[index].info.basic_type == ReflectionBasicFieldType::Enum) {
				ReflectionEnum enum_ = reflection->GetEnum(type.fields[index].definition);
				unsigned char* enum_val = (unsigned char*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				if (*enum_val >= enum_.fields.size) {
					return false;
				}
			}
		}
		return true;
	}

}