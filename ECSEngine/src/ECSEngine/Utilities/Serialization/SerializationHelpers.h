#pragma once
#include "../../Core.h"
#include "../Function.h"
#include "../File.h"
#include "../../Containers/Stream.h"
#include "../Reflection/ReflectionTypes.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
	}

	struct SerializeOptions;

	struct SerializeCustomTypeWriteFunctionData {
		uintptr_t* stream;
		const Reflection::ReflectionManager* reflection_manager;
		const void* data;
		Stream<char> definition;
		bool write_data;

		SerializeOptions* options;
	};

	// If write data is false, just determine how many buffer bytes are needed
	typedef size_t(*SerializeCustomTypeWriteFunction)(SerializeCustomTypeWriteFunctionData* data);

	struct DeserializeOptions;

	struct SerializeCustomTypeReadFunctionData {
		unsigned int version;

		uintptr_t* stream;
		const Reflection::ReflectionManager* reflection_manager;
		void* data;
		Stream<char> definition;
		bool read_data;

		DeserializeOptions* options;
	};

	// If read_data is false, just determine how many buffer bytes are needed
	typedef size_t(*SerializeCustomTypeReadFunction)(SerializeCustomTypeReadFunctionData* data);

	struct SerializeCustomType {
		Reflection::ReflectionContainerType container_type;
		SerializeCustomTypeWriteFunction write;
		SerializeCustomTypeReadFunction read;
		unsigned int version;
	};

	extern SerializeCustomType ECS_SERIALIZE_CUSTOM_TYPES[];

	// Returns the byte size of the element in between the template parentheses.
	// It also finds out if the template parameter is a user defined type, it has a serialize functor
	// or it is just a basic type
	// At the moment, pointers can be detected
	ECSENGINE_API size_t SerializeCustomTypeBasicTypeHelper(
		Stream<char>& template_type,
		const Reflection::ReflectionManager* reflection_manager,
		Reflection::ReflectionType& type,
		unsigned int& custom_serializer_index,
		Reflection::ReflectionBasicFieldType& basic_type,
		Reflection::ReflectionStreamFieldType& stream_type
	);

	// Element_byte_size should be for stream_type different from basic
	// the byte size of the target, not of the stream's
	// It does not prefix the stream with its size - should be done outside
	ECSENGINE_API size_t SerializeCustomWriteHelper(
		Reflection::ReflectionBasicFieldType basic_type,
		Reflection::ReflectionStreamFieldType stream_type,
		Reflection::ReflectionType reflection_type,
		unsigned int custom_serializer_index,
		SerializeCustomTypeWriteFunctionData* write_data,
		Stream<void> data_to_write,
		size_t element_byte_size,
		Stream<size_t> indices = { nullptr, 0 }
	);

	// The element_byte_size should be the size of the target for streams.
	// Field data should be initialized with the element count to be read
	// Can provide an allocator such that it will allocate from it instead 
	// of the backup allocator. Can be useful for resizable containers
	ECSENGINE_API size_t DeserializeCustomReadHelper(
		Reflection::ReflectionBasicFieldType basic_type,
		Reflection::ReflectionStreamFieldType stream_type,
		Reflection::ReflectionType reflection_type,
		unsigned int custom_serializer_index,
		SerializeCustomTypeReadFunctionData* read_data,
		size_t element_count,
		size_t elements_to_allocate,
		size_t element_byte_size,
		void** allocated_buffer,
		AllocatorPolymorphic override_allocator = { nullptr },
		Stream<size_t> indices = { nullptr, 0 }
	);

	// Returns -1 if it doesn't exist
	ECSENGINE_API unsigned int FindSerializeCustomType(Stream<char> definition);

	// Returns std::size(ECS_SERIALIZE_CUSTOM_TYPES);
	ECSENGINE_API unsigned int SerializeCustomTypeCount();

	// -----------------------------------------------------------------------------------------

	template<bool write_data>
	size_t Write(CapacityStream<void>& stream, const void* data, size_t data_size) {
		if constexpr (write_data) {
			memcpy((void*)((uintptr_t)stream.buffer + stream.size), data, data_size);
			stream.size += data_size;
			return 0;
		}
		else {
			return data_size;
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool write_data>
	size_t Write(uintptr_t* stream, const void* data, size_t data_size) {
		if constexpr (write_data) {
			memcpy((void*)*stream, data, data_size);
			*stream += data_size;

			return 0;
		}
		else {
			return data_size;
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool write_data>
	size_t WriteWithSize(uintptr_t* stream, const void* data, size_t data_size) {
		return Write<write_data>(stream, &data_size, sizeof(data_size)) + Write<write_data>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------
	
	template<bool write_data>
	size_t WriteWithSizeShort(uintptr_t* stream, const void* data, unsigned short data_size) {
		return Write<write_data>(stream, &data_size, sizeof(data_size)) + Write<write_data>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t Read(CapacityStream<void>& stream, void* data, size_t data_size) {
		if constexpr (read_data) {
			memcpy(data, (const void*)((uintptr_t)stream.buffer + stream.size), data_size);
			stream.size += data_size;
			return 0;
		}
		else {
			*stream += data_size;
			return data_size;
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t Read(uintptr_t* stream, void* data, size_t data_size) {
		if constexpr (read_data) {
			memcpy(data, (const void*)*stream, data_size);
			*stream += data_size;
			return 0;
		}
		else {
			*stream += data_size;
			return data_size;
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReadWithSize(uintptr_t* stream, void* data, size_t& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return Read<read_data>(stream, data_size, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReadWithSizeShort(uintptr_t* stream, void* data, unsigned short& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return Read<read_data>(stream, data_size, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReferenceData(uintptr_t* stream, void** pointer, size_t data_size) {
		if constexpr (read_data) {
			*pointer = (void*)*stream;
			*stream += data_size;

			return 0;
		}
		else {
			return data_size;
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReferenceDataWithSize(uintptr_t* stream, void** pointer, size_t& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return ReferenceData<read_data>(stream, pointer, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReferenceDataWithSizeShort(uintptr_t* stream, void** pointer, unsigned short& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return ReferenceData<read_data>(stream, pointer, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReadOrReferenceData(uintptr_t* stream, void** pointer, size_t data_size, bool reference_data) {
		if (reference_data) {
			return ReferenceData<read_data>(stream, pointer, data_size);
		}
		else {
			return Read<read_data>(stream, *pointer, data_size);
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReadOrReferenceDataWithSize(uintptr_t* stream, void** pointer, size_t& data_size, bool reference_data) {
		if (reference_data) {
			return ReferenceDataWithSize<read_data>(stream, pointer, data_size);
		}
		else {
			return ReadWithSize<read_data>(stream, *pointer, data_size);
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	size_t ReadOrReferenceDataWithSizeShort(uintptr_t* stream, void** pointer, unsigned short& data_size, bool reference_data) {
		if (reference_data) {
			return ReferenceDataWithSizeShort<read_data>(stream, pointer, data_size);
		}
		else {
			return ReadWithSizeShort<read_data>(stream, *pointer, data_size);
		}
	}

	// -----------------------------------------------------------------------------------------

	template<bool write_data>
	size_t WriteFundamentalType(const Reflection::ReflectionFieldInfo& info, const void* data, uintptr_t& stream) {
		if (info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
			return Write<write_data>(&stream, data, info.byte_size);
		}
		else {
			if (info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				unsigned int pointer_indirection = GetReflectionFieldPointerIndirection(info);
				if (pointer_indirection == 1) {
					if (info.basic_type == Reflection::ReflectionBasicFieldType::Int8) {
						const char* characters = *(const char**)data;
						size_t character_count = strlen(characters) + 1;
						return WriteWithSize<write_data>(&stream, characters, character_count * sizeof(char));
					}
					else if (info.basic_type == Reflection::ReflectionBasicFieldType::Wchar_t) {
						const wchar_t* characters = *(const wchar_t**)data;
						size_t character_count = wcslen(characters) + 1;
						return WriteWithSize<write_data>(&stream, characters, character_count * sizeof(wchar_t));
					}
					else {
						ECS_ASSERT(false, "Failed to serialize basic pointer with indirection 1. It is not a char* or wchar_t*.");
					}
				}
				else {
					ECS_ASSERT(false, "Failed to serialize basic pointer. Indirection is greater than 1.");
				}
			}
			else if (info.stream_type == Reflection::ReflectionStreamFieldType::BasicTypeArray) {
				return Write<write_data>(&stream, &info.basic_type_count, sizeof(info.basic_type_count)) + Write<write_data>(&stream, data, info.byte_size);
			}
			else if (IsStream(info.stream_type)) {
				Stream<void> void_stream = GetReflectionFieldStreamVoid(info, data);
				size_t element_byte_size = GetReflectionFieldStreamElementByteSize(info);
				size_t write_size = WriteWithSize<write_data>(&stream, void_stream.buffer, void_stream.size * element_byte_size);

				// For strings, add a '\0' such that if the data is later on changed to a const char* or const wchar_t* it can still reference it
				if (info.basic_type == Reflection::ReflectionBasicFieldType::Int8) {
					write_size += Write<write_data>(&stream, "\0", sizeof(char));
				}
				else if (info.basic_type == Reflection::ReflectionBasicFieldType::Wchar_t) {
					write_size += Write<write_data>(&stream, "\0", sizeof(wchar_t));
				}

				return write_size;
			}
		}

		ECS_ASSERT(false, "An error has occured when trying to serialize fundamental type.");
		return 0;
	}

	// -----------------------------------------------------------------------------------------

	// If the allocator is nullptr, then it will just reference the data
	// If the type is basic type array, the number elements to be read needs to be specified
	// because if reading from a file and the number of elements has changed, then we need to adjust
	// the reading aswell
	template<bool read_data, bool force_allocation = false>
	size_t ReadOrReferenceFundamentalType(
		const Reflection::ReflectionFieldInfo& info,
		void* data,
		uintptr_t& stream,
		AllocatorPolymorphic allocator
	) {
		if (info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
			Read<read_data>(&stream, data, info.byte_size);
			return 0;
		}
		else {
			if (info.stream_type == Reflection::ReflectionStreamFieldType::BasicTypeArray) {
				unsigned short written_elements = 0;
				Read<true>(&stream, &written_elements, sizeof(written_elements));

				unsigned short elements_to_read = function::ClampMax(info.basic_type_count, written_elements);
				Read<read_data>(&stream, data, elements_to_read * GetBasicTypeArrayElementSize(info));

				Ignore(&stream, (info.basic_type_count - elements_to_read) * GetBasicTypeArrayElementSize(info));
				return 0;
			}

			if (info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				unsigned int pointer_indirection = GetReflectionFieldPointerIndirection(info);
				if (pointer_indirection == 1) {
					if (info.basic_type == Reflection::ReflectionBasicFieldType::Int8 || info.basic_type == Reflection::ReflectionBasicFieldType::Wchar_t) {
						size_t byte_size = 0;
						Read<true>(&stream, &byte_size, sizeof(byte_size));
						if constexpr (read_data) {
							if (allocator.allocator != nullptr) {
								if (byte_size > 0) {
									void* allocation = Allocate(allocator, byte_size);
									Read<true>(&stream, allocation, byte_size);

									void** pointer = (void**)data;
									*pointer = allocation;
								}
							}
							else {
								if constexpr (!force_allocation) {
									ReferenceData<true>(&stream, (void**)data, byte_size);
								}
								else {
									if (byte_size > 0) {
										void* allocation = AllocateEx(allocator, byte_size);
										Read<true>(&stream, allocation, byte_size);

										void** pointer = (void**)data;
										*pointer = allocation;
									}
								}
							}
						}
						else {
							Ignore(&stream, byte_size);
						}
						return byte_size;
					}
					else {
						ECS_ASSERT(false, "Cannot deserialize pointer with indirection 1. Type is not char* or wchar_t*.");
					}
				}
				else {
					ECS_ASSERT(false, "Cannot deserialize pointer with indirection greater than 1.");
				}
			}
			else if (IsStream(info.stream_type)) {
				size_t byte_size = 0;
				Read<true>(&stream, &byte_size, sizeof(byte_size));

				if constexpr (read_data) {
					if (allocator.allocator != nullptr) {
						void** pointer = (void**)data;
						if (byte_size > 0) {
							void* allocation = Allocate(allocator, byte_size);
							Read<true>(&stream, allocation, byte_size);

							*pointer = allocation;
						}
					}
					else {
						if constexpr (!force_allocation) {
							ReferenceData<true>(&stream, (void**)data, byte_size);
						}
						else {
							void** pointer = (void**)data;
							if (byte_size > 0) {
								void* allocation = AllocateEx(allocator, byte_size);
								Read<true>(&stream, allocation, byte_size);

								*pointer = allocation;
							}
						}
					}
				}
				else {
					Ignore(&stream, byte_size);
				}

				// If it is a string and ends with '\0', then eliminate it
				if (info.stream_type == Reflection::ReflectionStreamFieldType::Stream) {
					Stream<void>* field_stream = (Stream<void>*)data;
					field_stream->size = byte_size / info.stream_byte_size;

					if (info.basic_type == Reflection::ReflectionBasicFieldType::Int8) {
						char* characters = (char*)field_stream->buffer;
						if (field_stream->size > 0) {
							field_stream->size -= characters[field_stream->size - 1] == '\0';
						}
					}
					else if (info.basic_type == Reflection::ReflectionBasicFieldType::Wchar_t) {
						wchar_t* characters = (wchar_t*)field_stream->buffer;
						if (field_stream->size > 0) {
							field_stream->size -= characters[field_stream->size - 1] == L'\0';
						}
					}
				}
				// They can be safely aliased
				else if (info.stream_type == Reflection::ReflectionStreamFieldType::CapacityStream || info.stream_type == Reflection::ReflectionStreamFieldType::ResizableStream) {
					CapacityStream<void>* field_stream = (CapacityStream<void>*)data;
					field_stream->size = (unsigned int)byte_size / info.stream_byte_size;

					if (info.basic_type == Reflection::ReflectionBasicFieldType::Int8) {
						char* characters = (char*)field_stream->buffer;
						if (field_stream->size > 0) {
							field_stream->size -= characters[field_stream->size - 1] == '\0';
						}
					}
					else if (info.basic_type == Reflection::ReflectionBasicFieldType::Wchar_t) {
						wchar_t* characters = (wchar_t*)field_stream->buffer;
						if (field_stream->size > 0) {
							field_stream->size -= characters[field_stream->size - 1] == L'\0';
						}
					}
				}
				return byte_size;
			}
		}

		return 0;
	}

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool IgnoreFile(ECS_FILE_HANDLE file, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Ignore(CapacityStream<void>& stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool Ignore(uintptr_t* stream, size_t byte_size);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool IgnoreWithSize(uintptr_t* stream);

	// -----------------------------------------------------------------------------------------

	ECSENGINE_API bool IgnoreWithSizeShort(uintptr_t* stream);

	// -----------------------------------------------------------------------------------------

	// Prepares the file for writing, allocates the memory for the temporary buffer and then 
	template<typename Functor>
	bool SerializeWriteFile(Stream<wchar_t> file, AllocatorPolymorphic allocator, size_t allocation_size, bool binary, Functor&& functor) {
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TRUNCATE_FILE;
		access_flags |= binary ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		ECS_FILE_STATUS_FLAGS flags = FileCreate(file, &file_handle, access_flags);
		if (flags == ECS_FILE_STATUS_OK) {
			ScopedFile scoped_file(file_handle);

			void* allocation = AllocateEx(allocator, allocation_size);
			uintptr_t buffer = (uintptr_t)allocation;
			functor(buffer);

			bool success = true;
			size_t difference = buffer - (uintptr_t)allocation;
			if (difference > allocation_size) {
				success = false;
			}
			else {
				success = WriteFile(file_handle, { allocation, difference });
			}
			DeallocateEx(allocator, allocation);
			return success;
		}

		return false;
	}

	// -----------------------------------------------------------------------------------------
	
	// Returns the amount of pointer data bytes
	template<typename Functor>
	size_t DeserializeReadFile(
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator,
		bool binary,
		Functor&& functor
	) {
		Stream<void> contents = ReadWholeFile(file, binary, allocator);
		if (contents.buffer != nullptr) {
			uintptr_t buffer = (uintptr_t)contents.buffer;
			size_t pointer_bytes = functor(buffer);
			DeallocateEx(allocator, contents.buffer);
			return pointer_bytes;
		}
		return -1;
	}

	// -----------------------------------------------------------------------------------------

	template<typename Functor>
	void* DeserializeReadBinaryFileAndKeepMemory(
		Stream<wchar_t> file,
		AllocatorPolymorphic allocator,
		Functor&& functor
	) {
		Stream<void> contents = ReadWholeFileBinary(file, allocator);
		if (contents.buffer != nullptr) {
			uintptr_t buffer = (uintptr_t)contents.buffer;
			functor(buffer);
			return contents.buffer;
		}
		return nullptr;
	}

	// -----------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------

	
}