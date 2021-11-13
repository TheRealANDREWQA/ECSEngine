#include "ecspch.h"
#include "Serialization.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	const char* ECS_PLATFORM_STRINGS[] = {
		STRING(Win64/DX11)
	};

	// -----------------------------------------------------------------------------------------

	void ConstructPointerFieldsForType(Stream<function::CopyPointer>& ECS_RESTRICT pointer_fields, ReflectionType type, const void* ECS_RESTRICT data) {
		for (size_t index = 0; index < type.fields.size; index++) {
			if (type.fields[index].info.extended_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointer_fields[pointer_fields.size].destination = &stream->buffer;
				pointer_fields[pointer_fields.size].data = stream->buffer;
				pointer_fields[pointer_fields.size++].data_size = type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.extended_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointer_fields[pointer_fields.size].destination = &stream->buffer;
				pointer_fields[pointer_fields.size].data = stream->buffer;
				pointer_fields[pointer_fields.size++].data_size = type.fields[index].info.additional_flags * stream->size;
			}
			else if (type.fields[index].info.extended_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<void*, LinearAllocator>* stream = (ResizableStream<void*, LinearAllocator>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointer_fields[pointer_fields.size].destination = (void**)&stream->buffer;
				pointer_fields[pointer_fields.size].data = stream->buffer;
				pointer_fields[pointer_fields.size++].data_size = type.fields[index].info.additional_flags * stream->size;
			}
		}
	}

	// -----------------------------------------------------------------------------------------

	void Write(std::ofstream& ECS_RESTRICT stream, const void* ECS_RESTRICT data, size_t data_size) {
		stream.write((const char*)data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	void Write(CapacityStream<void>& ECS_RESTRICT stream, const void* ECS_RESTRICT data, size_t data_size) {
		memcpy((void*)((uintptr_t)stream.buffer + stream.size), data, data_size);
		stream.size += data_size;
		ECS_ASSERT(stream.size < stream.capacity);
	}

	// -----------------------------------------------------------------------------------------

	void Write(std::ofstream& stream, Stream<void> data) {
		stream.write((const char*)data.buffer, data.size);
	}

	// -----------------------------------------------------------------------------------------

	void Write(CapacityStream<void>& stream, Stream<void> data) {
		stream.Add(data);
	}

	// -----------------------------------------------------------------------------------------

	void Read(std::ifstream& ECS_RESTRICT stream, void* ECS_RESTRICT data, size_t data_size) {
		stream.read((char*)data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	void Read(std::istream& stream, CapacityStream<void>& data, size_t data_size) {
		stream.read((char*)((uintptr_t)data.buffer + data.size), data_size);
		data.size += data_size;
		ECS_ASSERT(data.size < data.capacity);
	}

	// -----------------------------------------------------------------------------------------

	void Read(CapacityStream<void>& ECS_RESTRICT stream, void* ECS_RESTRICT data, size_t data_size) {
		memcpy(data, (const void*)((uintptr_t)stream.buffer + stream.size), data_size);
		stream.size += data_size;
		ECS_ASSERT(stream.size < stream.capacity);
	}

	// -----------------------------------------------------------------------------------------

	void Read(uintptr_t& ECS_RESTRICT stream, void* ECS_RESTRICT data, size_t data_size) {
		memcpy(data, (const void*)stream, data_size);
		stream += data_size;
	}

	// -----------------------------------------------------------------------------------------

	void Read(uintptr_t& stream, CapacityStream<void>& data, size_t data_size) {
		memcpy((void*)((uintptr_t)data.buffer + data.size), (const void*)stream, data_size);
		stream += data_size;
		data.size += data_size;
		ECS_ASSERT(data.size < data.capacity);
	}

	// -----------------------------------------------------------------------------------------

	void Ignore(std::ifstream& stream, size_t byte_size)
	{
		stream.ignore(byte_size);
	}

	// -----------------------------------------------------------------------------------------

	void Ignore(CapacityStream<void>& stream, size_t byte_size)
	{
		stream.size += byte_size;
	}

	// -----------------------------------------------------------------------------------------

	void Ignore(uintptr_t& stream, size_t byte_size)
	{
		stream += byte_size;
	}

	// -----------------------------------------------------------------------------------------

	template<typename StreamType>
	bool SerializeInternal(
		ReflectionType type,
		StreamType& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
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

		if constexpr (std::is_same_v<StreamType, std::ofstream>) {
			if (!stream.good()) {
				return false;
			}
		}
		return true;
	}
	
	// -----------------------------------------------------------------------------------------

	template<typename StreamType>
	size_t DeserializeInternal(
		ReflectionType type,
		StreamType& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointer_to_copy,
		bool* ECS_RESTRICT success_status
	) {
		for (size_t index = 0; index < type.fields.size; index++) {
			Read(stream, function::OffsetPointer(address, type.fields[index].info.pointer_offset), type.fields[index].info.byte_size);
		}

		ConstructPointerFieldsForType(type_pointer_to_copy, type, address);

		unsigned int memory_pool_before_size = memory_pool.size;
		for (size_t index = 0; index < type_pointer_to_copy.size; index++) {
			type_pointer_to_copy[index].data = function::OffsetPointer(memory_pool.buffer, memory_pool.size);
			Read(stream, &type_pointer_to_copy[index].data_size, sizeof(size_t));
			Read(stream, memory_pool, type_pointer_to_copy[index].data_size);
		}

		if constexpr (std::is_same_v<StreamType, std::ifstream>) {
			if (!stream.good()) {
				if (success_status != nullptr) {
					*success_status = false;
				}
			}
		}

		return memory_pool.size - memory_pool_before_size;
	}

	// -----------------------------------------------------------------------------------------

	bool Serialize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ofstream& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return SerializeInternal(type, stream, data);
	}
	
	// -----------------------------------------------------------------------------------------

	bool Serialize(
		ReflectionType type,
		std::ofstream& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	) {
		return SerializeInternal(type, stream, data);
	}

	// -----------------------------------------------------------------------------------------

	void Serialize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		CapacityStream<void>& ECS_RESTRICT stream,
		const void* ECS_RESTRICT data
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		SerializeInternal(type, stream, data);
	}

	// -----------------------------------------------------------------------------------------

	void Serialize(ReflectionType type, CapacityStream<void>& ECS_RESTRICT stream, const void* ECS_RESTRICT data)
	{
		SerializeInternal(type, stream, data);
	}

	// -----------------------------------------------------------------------------------------

	size_t SerializeSize(
		ReflectionType type,
		const void* ECS_RESTRICT data
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
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		const void* ECS_RESTRICT data
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return SerializeSize(type, data);
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy,
		bool* ECS_RESTRICT success_status
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeInternal(type, stream, address, memory_pool, type_pointers_to_copy, success_status);
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		ReflectionType type,
		std::ifstream& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy,
		bool* ECS_RESTRICT success_status
	)
	{
		return DeserializeInternal(type, stream, address, memory_pool, type_pointers_to_copy, success_status);
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT stream,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeInternal(type, stream, address, memory_pool, type_pointers_to_copy, nullptr);
	}

	// -----------------------------------------------------------------------------------------

	size_t Deserialize(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT ptr_to_read,
		void* ECS_RESTRICT address,
		CapacityStream<void>& ECS_RESTRICT memory_pool,
		Stream<function::CopyPointer>& ECS_RESTRICT type_pointers_to_copy
	)
	{
		return DeserializeInternal(type, ptr_to_read, address, memory_pool, type_pointers_to_copy, nullptr);
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeSize(
		ReflectionType type,
		uintptr_t& ECS_RESTRICT ptr_to_read,
		void* ECS_RESTRICT address
	)
	{
		size_t total_size = type.fields[type.fields.size - 1].info.pointer_offset + type.fields[type.fields.size - 1].info.byte_size;
		Read(ptr_to_read, address, total_size);

		size_t total_memory = 0;
		for (size_t index = 0; index < type.fields.size; index++) {
			if (type.fields[index].info.extended_type == ReflectionStreamFieldType::Stream) {
				const Stream<void>* stream = (const Stream<void>*)((uintptr_t)address + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.extended_type == ReflectionStreamFieldType::CapacityStream) {
				const CapacityStream<void>* stream = (const CapacityStream<void>*)((uintptr_t)address + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.extended_type == ReflectionStreamFieldType::ResizableStream) {
				const ResizableStream<void*, LinearAllocator>* stream = (const ResizableStream<void*, LinearAllocator>*)((uintptr_t)address + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
		}

		return total_memory;
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeSize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		uintptr_t& ECS_RESTRICT ptr_to_read,
		void* ECS_RESTRICT address
	)
	{
		ReflectionType type = reflection->GetType(type_name);

		return DeserializeSize(type, ptr_to_read, address);
	}

	// -----------------------------------------------------------------------------------------

	size_t DeserializeSize(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
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
			if (type.fields[index].info.extended_type == ReflectionStreamFieldType::Stream) {
				const Stream<void>* stream = (const Stream<void>*)((uintptr_t)temp_memory + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.extended_type == ReflectionStreamFieldType::CapacityStream) {
				const CapacityStream<void>* stream = (const CapacityStream<void>*)((uintptr_t)temp_memory + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
			else if (type.fields[index].info.extended_type == ReflectionStreamFieldType::ResizableStream) {
				const ResizableStream<void*, LinearAllocator>* stream = (const ResizableStream<void*, LinearAllocator>*)((uintptr_t)temp_memory + type.fields[index].info.pointer_offset);
				total_memory += stream->size * type.fields[index].info.additional_flags;
			}
		}

		return total_memory;
	}

	bool ValidateEnums(
		const ReflectionManager* ECS_RESTRICT reflection,
		const char* ECS_RESTRICT type_name,
		const void* ECS_RESTRICT data
	)
	{
		ReflectionType type = reflection->GetType(type_name);
		return ValidateEnums(reflection, type, data);
	}

	bool ValidateEnums(const ReflectionManager* ECS_RESTRICT reflection, ReflectionType type, const void* ECS_RESTRICT data)
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