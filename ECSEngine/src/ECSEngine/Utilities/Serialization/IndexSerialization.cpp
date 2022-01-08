#include "ecspch.h"
#include "IndexSerialization.h"
#include "Serialization.h"
#include "../../Containers/HashTable.h"
#include "SerializationHelpers.h"
#include "../../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	using namespace Reflection;
	ECS_CONTAINERS;

	// Make pointer data size equal to 0 if it is not a pointer
	// Make data size equal to 0 if not using Reflection
	// Data buffer should be a pointer to the address that needs to be filled in
	// e.g. struct int { unsigned int x }; &x
	struct SerializeIndexPointer {
		Stream<void> data;
		Stream<void> pointer_data;
		const char* name;
	};

	size_t DeserializeIndexSize(Stream<SerializeIndexPointer> pointers, uintptr_t data)
	{
		size_t total_size = 0;

		size_t field_count;
		Read(data, &field_count, sizeof(field_count));

		char name[4096];
		Stream<char> name_stream(name, 0);
		size_t total_memory = 8;

		unsigned short* pointer_name_sizes = (unsigned short*)alloca(sizeof(unsigned short) * pointers.size);
		for (size_t index = 0; index < pointers.size; index++) {
			pointer_name_sizes[index] = strlen(pointers[index].name);
		}

		for (size_t index = 0; index < field_count; index++) {
			unsigned short name_size;
			Read(data, &name_size, sizeof(unsigned short));
			ECS_ASSERT(name_size < 4096);

			Read(data, name, name_size * sizeof(char));
			name_stream.size = name_size;

			// Do a linear search and find which field corresponds
			unsigned int string_index = -1;
			for (size_t subindex = 0; subindex < pointers.size; subindex++) {
				if (function::CompareStrings(Stream<char>(name, name_size), Stream<char>(pointers[subindex].name, pointer_name_sizes[subindex]))) {
					string_index = subindex;
					break;
				}
			}

			bool should_add = string_index != -1;

			size_t data_size;
			Read(data, &data_size, sizeof(size_t));
			data += data_size;

			total_memory += data_size * should_add;

			size_t pointer_data_size;
			Read(data, &pointer_data_size, sizeof(size_t));
			total_memory += (sizeof(size_t) + pointer_data_size) * should_add;

			data += pointer_data_size;
		}

		return total_memory;
	}

	size_t SerializeIndexSize(Stream<SerializeIndexPointer> pointers) {
		size_t total_memory = 0;
		for (size_t index = 0; index < pointers.size; index++) {
			unsigned short name_size = (unsigned short)strlen(pointers[index].name);
			total_memory += sizeof(unsigned short) + sizeof(size_t) * 2 + sizeof(char) * name_size + pointers[index].data.size + pointers[index].pointer_data.size;
		}

		return total_memory;
	}
	
	void SerializeInternal(
		uintptr_t& stream,
		Stream<SerializeIndexPointer> pointers
	) {
		// Write the number of fields 
		Write(stream, &pointers.size, sizeof(size_t));

		ECS_ASSERT(pointers.size < 128);

		for (size_t index = 0; index < pointers.size; index++) {
			unsigned short name_size = (unsigned short)strlen(pointers[index].name);

			Write(stream, &name_size, sizeof(unsigned short));
			Write(stream, pointers[index].name, sizeof(char) * name_size);
			if (pointers[index].data.size > 0) {
				Write(stream, &pointers[index].data.size, sizeof(size_t));
				Write(stream, pointers[index].data);
			}
			else {
				size_t zero = 0;
				Write(stream, &zero, sizeof(size_t));
			}
			if (pointers[index].pointer_data.size > 0) {
				Write(stream, &pointers[index].pointer_data.size, sizeof(size_t));
				Write(stream, pointers[index].pointer_data);
			}
			else {
				size_t zero = 0;
				Write(stream, &zero, sizeof(size_t));
			}
		}
	}

	size_t DeserializeInternal(
		uintptr_t& stream,
		Stream<SerializeIndexPointer> pointers,
		CapacityStream<void>& memory_pool
	) {
		bool success = true;

		size_t field_count;
		Read(stream, &field_count, sizeof(size_t));

		ECS_ASSERT(field_count < 128);
		Stream<char> _pointer_names[128];
		Stream<Stream<char>> pointer_names(_pointer_names, pointers.size);
		for (size_t index = 0; index < pointers.size; index++) {
			pointer_names[index] = { pointers[index].name, strlen(pointers[index].name) };
		}

		char name[4096];
		Stream<char> name_stream(name, 0);

		size_t memory_pool_size_before = memory_pool.size;
		for (size_t index = 0; index < field_count && success; index++) {
			unsigned short name_size;
			Read(stream, &name_size, sizeof(unsigned short));
			ECS_ASSERT(name_size < 4096);

			Read(stream, name, name_size * sizeof(char));
			name_stream.size = name_size;

			// Do a linear search and find which field corresponds
			unsigned int string_index = function::FindString(name_stream, pointer_names);

			size_t data_size;
			Read(stream, &data_size, sizeof(size_t));
			if (data_size > 0) {
				if (string_index != -1) {
					if (pointers[string_index].data.size > 0) {
						ECS_ASSERT(data_size == pointers[string_index].data.size);
						if (data_size != pointers[string_index].data.size) {
							success = false;
						}
						else {
							Read(stream, pointers[string_index].data.buffer, data_size);
						}
					}
					else {
						Ignore(stream, data_size);
					}
				}
				else {
					Ignore(stream, data_size);
				}
			}

			size_t pointer_data_size;
			Read(stream, &pointer_data_size, sizeof(size_t));
			if (pointer_data_size > 0) {
				if (string_index != -1) {
					pointers[string_index].pointer_data.size = pointer_data_size;
					pointers[string_index].pointer_data.buffer = function::OffsetPointer(memory_pool);
					Read(stream, memory_pool, pointer_data_size);
				}
				else {
					Ignore(stream, pointer_data_size);
				}
			}

		}

		return success ? memory_pool.size - memory_pool_size_before : -1;
	}

	template<typename Handler>
	void AdaptTypeToIndexPointers(
		ReflectionType type, 
		Stream<SerializeIndexPointer> pointers, 
		const void* data,
		Handler&& handler
	) {
		for (size_t index = 0; index < type.fields.size; index++) {
			pointers[index].data.buffer = function::OffsetPointer(data, type.fields[index].info.pointer_offset);
			pointers[index].data.size = type.fields[index].info.byte_size;
			pointers[index].name = type.fields[index].name;

			if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointers[index].pointer_data.buffer = stream->buffer;
				pointers[index].pointer_data.size = type.fields[index].info.additional_flags * stream->size;
				handler(index, &stream->buffer);
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointers[index].pointer_data.buffer = stream->buffer;
				pointers[index].pointer_data.size = type.fields[index].info.additional_flags * stream->size;
				handler(index, &stream->buffer);
			}
			else if (type.fields[index].info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<void*, LinearAllocator>* stream = (ResizableStream<void*, LinearAllocator>*)((uintptr_t)data + type.fields[index].info.pointer_offset);
				pointers[index].pointer_data.buffer = stream->buffer;
				pointers[index].pointer_data.size = type.fields[index].info.additional_flags * stream->size;
				handler(index, (void**)&stream->buffer);
			}
			else {
				pointers[index].pointer_data.size = 0;
			}
		}
	}

	bool SerializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator
	) {
		ReflectionType type = reflection->GetType(type_name);
		return SerializeIndex(type, file, data);
	}

	bool SerializeIndex(
		ReflectionType type,
		Stream<wchar_t> file,
		const void* data,
		AllocatorPolymorphic allocator
	) {
		ECS_ASSERT(type.fields.size < 128);
		SerializeIndexPointer _pointers[128];
		Stream<SerializeIndexPointer> pointers(_pointers, type.fields.size);

		AdaptTypeToIndexPointers(type, pointers, data, [](unsigned int index, void** destination) {});

		size_t serialize_size = SerializeIndexSize(pointers);
		return SerializeWriteFile(file, allocator, serialize_size, [=](uintptr_t& buffer) {
			SerializeInternal(buffer, pointers);
		});
	}

	void SerializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		const void* data
	) {
		ReflectionType type = reflection->GetType(type_name);
		SerializeIndex(type, stream, data);
	}

	void SerializeIndex(
		ReflectionType type,
		uintptr_t& stream,
		const void* data
	) {
		ECS_ASSERT(type.fields.size < 128);
		SerializeIndexPointer _pointers[128];
		Stream<SerializeIndexPointer> pointers(_pointers, type.fields.size);

		AdaptTypeToIndexPointers(type, pointers, data, [](unsigned int index, void** destination) {});
	
		SerializeInternal(stream, pointers);
	}

	size_t SerializeIndexSize(
		const ReflectionManager* reflection,
		const char* type_name,
		const void* data
	) {
		ReflectionType type = reflection->GetType(type_name);
		return SerializeIndexSize(type, data);
	}

	size_t SerializeIndexSize(
		ReflectionType type,
		const void* data
	) {
		ECS_ASSERT(type.fields.size < 128);
		SerializeIndexPointer _pointers[128];
		Stream<SerializeIndexPointer> pointers(_pointers, type.fields.size);

		AdaptTypeToIndexPointers(type, pointers, data, [](unsigned int index, void** destination) {});

		return SerializeIndexSize(pointers);
	}

	size_t DeserializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> copy_pointers,
		AllocatorPolymorphic allocator
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeIndex(type, file, address, memory_pool, copy_pointers, allocator);
	}

	size_t DeserializeIndex(
		ReflectionType type,
		Stream<wchar_t> file,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> copy_pointers,
		AllocatorPolymorphic allocator
	) {
		return DeserializeReadFile(file, allocator, [&](uintptr_t& buffer) {
			return DeserializeIndex(type, buffer, address, memory_pool, copy_pointers);
		});
	}

	size_t DeserializeIndex(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> pointers
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeIndex(type, stream, address, memory_pool, pointers);
	}

	size_t DeserializeIndex(
		ReflectionType type,
		uintptr_t& stream,
		void* address,
		CapacityStream<void>& memory_pool,
		Stream<function::CopyPointer> copy_pointers
	) {
		ECS_ASSERT(type.fields.size < 128);
		SerializeIndexPointer _pointers[128];
		Stream<SerializeIndexPointer> pointers(_pointers, type.fields.size);

		AdaptTypeToIndexPointers(type, pointers, address, [&copy_pointers](unsigned int index, void** destination) {
			copy_pointers[index].destination = destination;
		});

		size_t memory_written = DeserializeInternal(stream, pointers, memory_pool);
		for (size_t index = 0; index < copy_pointers.size; index++) {
			copy_pointers[index].data = pointers[index].pointer_data.buffer;
			copy_pointers[index].data_size = pointers[index].pointer_data.size;
		}

		return memory_written;
	}

	size_t DeserializeIndexSize(
		const ReflectionManager* reflection,
		const char* type_name,
		uintptr_t data
	) {
		ReflectionType type = reflection->GetType(type_name);
		return DeserializeIndexSize(type, data);
	}

	size_t DeserializeIndexSize(
		ReflectionType type,
		uintptr_t data
	) {
		size_t field_count;
		Read(data, &field_count, sizeof(size_t));

		ECS_ASSERT(field_count < 128);
		Stream<char> _pointer_names[128];
		Stream<Stream<char>> pointer_names(_pointer_names, type.fields.size);
		for (size_t index = 0; index < type.fields.size; index++) {
			pointer_names[index] = { type.fields[index].name, strlen(type.fields[index].name) };
		}

		char name[4096];
		Stream<char> name_stream(name, 0);
		size_t total_memory = 0;
		for (size_t index = 0; index < field_count; index++) {
			unsigned short name_size;
			Read(data, &name_size, sizeof(unsigned short));
			ECS_ASSERT(name_size < 4096);

			Read(data, name, name_size * sizeof(char));
			name_stream.size = name_size;

			// Do a linear search and find which field corresponds
			unsigned int string_index = function::FindString(name_stream, pointer_names);
			bool should_add = string_index != -1;

			size_t data_size;
			Read(data, &data_size, sizeof(size_t));
			data += data_size;

			size_t pointer_data_size;
			Read(data, &pointer_data_size, sizeof(size_t));
			total_memory += (sizeof(size_t) + pointer_data_size) * should_add;

			data += pointer_data_size;
		}

		return total_memory;
	}

	size_t DeserializeIndexSize(Stream<const char*> pointers, uintptr_t data)
	{
		size_t total_size = 0;

		size_t field_count;
		Read(data, &field_count, sizeof(field_count));

		char name[4096];
		Stream<char> name_stream(name, 0);
		size_t total_memory = 8;

		unsigned short* pointer_name_sizes = (unsigned short*)alloca(sizeof(unsigned short) * pointers.size);
		for (size_t index = 0; index < pointers.size; index++) {
			pointer_name_sizes[index] = (unsigned short)strlen(pointers[index]);
		}

		for (size_t index = 0; index < field_count; index++) {
			unsigned short name_size;
			Read(data, &name_size, sizeof(unsigned short));
			ECS_ASSERT(name_size < 4096);

			Read(data, name, name_size * sizeof(char));
			name_stream.size = name_size;

			// Do a linear search and find which field corresponds
			unsigned int string_index = -1;
			for (size_t subindex = 0; subindex < pointers.size; subindex++) {
				if (function::CompareStrings(Stream<char>(name, name_size), Stream<char>(pointers[subindex], pointer_name_sizes[subindex]))) {
					string_index = subindex;
					break;
				}
			}

			bool should_add = string_index != -1;

			size_t data_size;
			Read(data, &data_size, sizeof(size_t));
			data += data_size;

			total_memory += data_size * should_add;

			size_t pointer_data_size;
			Read(data, &pointer_data_size, sizeof(size_t));
			total_memory += (sizeof(size_t) + pointer_data_size) * should_add;

			data += pointer_data_size;
		}

		return total_memory;
	}


}