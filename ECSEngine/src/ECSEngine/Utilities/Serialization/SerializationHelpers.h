#pragma once
#include "../../Core.h"
#include "../File.h"
#include "../../Containers/Stream.h"
#include "../Reflection/ReflectionTypes.h"
#include "../../Math/MathHelpers.h"
#include "../Utilities.h"

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
	typedef size_t (*SerializeCustomTypeWriteFunction)(SerializeCustomTypeWriteFunctionData* data);

#define ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(name) size_t SerializeCustomTypeWrite_##name(SerializeCustomTypeWriteFunctionData* data)

	struct DeserializeOptions;

	struct SerializeCustomTypeReadFunctionData {
		unsigned int version;
		bool read_data;
		// This can be used by custom serializers to take into consideration that this field
		// was allocated prior - fields should be considered invalid
		bool was_allocated;

		uintptr_t* stream;
		const Reflection::ReflectionManager* reflection_manager;
		void* data;
		Stream<char> definition;

		DeserializeOptions* options;
	};

	// If read_data is false, just determine how many buffer bytes are needed
	typedef size_t (*SerializeCustomTypeReadFunction)(SerializeCustomTypeReadFunctionData* data);

#define ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(name) size_t SerializeCustomTypeRead_##name(SerializeCustomTypeReadFunctionData* data)

#define ECS_SERIALIZE_CUSTOM_TYPE_FUNCTION_HEADER(name) ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(name); \
														ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(name);

#define ECS_SERIALIZE_CUSTOM_TYPE_STRUCT(name, version) { SerializeCustomTypeWrite_##name, SerializeCustomTypeRead_##name, version, STRING(name), nullptr } 

#define ECS_SERIALIZE_CUSTOM_TYPE_SWITCH_CAPACITY 8

	struct SerializeCustomType {
		SerializeCustomTypeWriteFunction write;
		SerializeCustomTypeReadFunction read;
		unsigned int version;
		// This is used to restore the mapping for the serialization in case 
		// new serialization types are added or some are removed
		Stream<char> name;

		// Can modify the behaviour of the serializer
		void* user_data;
		bool switches[ECS_SERIALIZE_CUSTOM_TYPE_SWITCH_CAPACITY] = { false };
	};

	// Must be kept in sync with the ECS_REFLECTION_CUSTOM_TYPES
	extern SerializeCustomType ECS_SERIALIZE_CUSTOM_TYPES[];

	struct SerializeCustomTypeDeduceTypeHelperData {
		Stream<char>* template_type;
		const Reflection::ReflectionManager* reflection_manager;
	};

	struct SerializeCustomTypeDeduceTypeHelperResult {
		Reflection::ReflectionType type;
		unsigned int custom_serializer_index;
		Reflection::ReflectionBasicFieldType basic_type;
		Reflection::ReflectionStreamFieldType stream_type;
		size_t byte_size;
		size_t alignment;
	};

	// Returns the byte size of the element in between the template parenthesis.
	// It also finds out if the template parameter is a user defined type, it has a serialize functor
	// or it is just a basic type
	// At the moment, pointers cannot be detected (is that a case for someone?)
	ECSENGINE_API SerializeCustomTypeDeduceTypeHelperResult SerializeCustomTypeDeduceTypeHelper(SerializeCustomTypeDeduceTypeHelperData* data);

	struct SerializeCustomWriteHelperData {
		Reflection::ReflectionBasicFieldType basic_type;
		Reflection::ReflectionStreamFieldType stream_type;
		const Reflection::ReflectionType* reflection_type;
		unsigned int custom_serializer_index;
		SerializeCustomTypeWriteFunctionData* write_data;
		Stream<void> data_to_write;
		size_t element_byte_size;
		Stream<size_t> indices = { nullptr, 0 };
	};

	// Element_byte_size should be for stream_type different from basic
	// the byte size of the target, not of the stream's
	// It does not prefix the stream with its size - should be done outside. Returns the number of bytes written
	ECSENGINE_API size_t SerializeCustomWriteHelper(SerializeCustomWriteHelperData* data);

	// All the fields that can be filled in from the result will be filled
	ECSENGINE_API SerializeCustomWriteHelperData FillSerializeCustomWriteHelper(const SerializeCustomTypeDeduceTypeHelperResult* result);

	struct SerializeCustomWriteHelperExData {
		Stream<char> template_type;
		SerializeCustomTypeWriteFunctionData* write_data;
		Stream<void> data_to_write;
		size_t element_byte_size = -1; // -1 Means use the return value of the deduce type helper
	};

	// Combines SerializeCustomTypeDeduceTypeHelper with a SerializeCustomWriteHelper call
	// into a single step. Returns what SerializeCustomWriteHelper would return, the number of bytes written
	ECSENGINE_API size_t SerializeCustomWriteHelperEx(SerializeCustomWriteHelperExData* data);

	struct DeserializeCustomReadHelperData {
		Reflection::ReflectionBasicFieldType basic_type;
		Reflection::ReflectionStreamFieldType stream_type;
		const Reflection::ReflectionType* reflection_type;
		unsigned int custom_serializer_index;
		SerializeCustomTypeReadFunctionData* read_data;
		size_t element_count;

		// If this is 0 and the element_count is 1 then it will assume that a single instance is to be deserialized
		// and no buffer will be allocated
		size_t elements_to_allocate;
		size_t element_byte_size;
		size_t element_alignment;

		union {
			void** allocated_buffer;
			void* deserialize_target;
		};
		AllocatorPolymorphic override_allocator = { nullptr };
		Stream<size_t> indices = { nullptr, 0 };
	};

	// The element_byte_size should be the size of the target for streams.
	// Field data should be initialized with the element count to be read
	// Can provide an allocator such that it will allocate from it instead 
	// of the backup allocator. Can be useful for resizable containers
	ECSENGINE_API size_t DeserializeCustomReadHelper(DeserializeCustomReadHelperData* data);

	// All the fields that can be filled in from the result will be filled
	ECSENGINE_API DeserializeCustomReadHelperData FillDeserializeCustomReadHelper(const SerializeCustomTypeDeduceTypeHelperResult* result);

	struct DeserializeCustomReadHelperExData {
		Stream<char> definition;
		SerializeCustomTypeReadFunctionData* data;
		size_t element_count;
		
		// -1 means the same as the element count
		// This can also be 0 if you want to deserialize a single instance
		// and the element count is 1
		size_t elements_to_allocate = -1; 

		union {
			void** allocated_buffer;
			void* deserialize_target;
		};
	};

	// Combines SerializeCustomTypeDeduceTypeHelper with a DeserializeCustomReadHelper call
	// into a single step. Returns what DeserializeCustomReadHelper would return, the number of buffer bytes
	ECSENGINE_API size_t DeserializeCustomReadHelperEx(DeserializeCustomReadHelperExData* data);

	ECSENGINE_API void SerializeCustomTypeCopyBlit(Reflection::ReflectionCustomTypeCopyData* data, size_t byte_size);

	// Returns -1 if it doesn't exist
	ECSENGINE_API unsigned int FindSerializeCustomType(Stream<char> definition);

	ECSENGINE_API unsigned int SerializeCustomTypeCount();

#pragma region User defined influence

	// Activates the mode for the sparse set where it writes only the T data
	// without the indirection buffer
	ECSENGINE_API void SetSerializeCustomSparsetSet();

	// Only references it, needs to be stable for the duration it is being used.
	// After the data is no longer needed, clear using the ClearSerializeCustomTypeUserData
	ECSENGINE_API void SetSerializeCustomTypeUserData(unsigned int index, void* buffer);

	ECSENGINE_API void ClearSerializeCustomTypeUserData(unsigned int index);

	// Sets the switch to the new value and returns the old value
	ECSENGINE_API bool SetSerializeCustomTypeSwitch(unsigned int index, unsigned char switch_index, bool new_status);

#pragma endregion

	// -----------------------------------------------------------------------------------------

	template<bool write_data>
	ECS_INLINE size_t Write(CapacityStream<void>& stream, const void* data, size_t data_size) {
		if constexpr (write_data) {
			memcpy((void*)((uintptr_t)stream.buffer + stream.size), data, data_size);
			stream.size += data_size;
			return 0;
		}
		else {
			return data_size;
		}
	}

	ECS_INLINE size_t Write(CapacityStream<void>& stream, const void* data, size_t data_size, bool write_data) {
		return write_data ? Write<true>(stream, data, data_size) : Write<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool write_data>
	ECS_INLINE size_t Write(uintptr_t* stream, const void* data, size_t data_size) {
		if constexpr (write_data) {
			memcpy((void*)*stream, data, data_size);
			*stream += data_size;

			return 0;
		}
		else {
			return data_size;
		}
	}

	template<bool write_data, typename T>
	ECS_INLINE size_t Write(uintptr_t* stream, const T* data) {
		return Write<write_data>(stream, data, sizeof(*data));
	}

	ECS_INLINE size_t Write(uintptr_t* stream, const void* data, size_t data_size, bool write_data) {
		return write_data ? Write<true>(stream, data, data_size) : Write<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool write_data>
	ECS_INLINE size_t WriteWithSize(uintptr_t* stream, const void* data, size_t data_size) {
		return Write<write_data>(stream, &data_size, sizeof(data_size)) + Write<write_data>(stream, data, data_size);
	}

	template<bool write_data>
	ECS_INLINE size_t WriteWithSize(uintptr_t* stream, Stream<void> data) {
		return WriteWithSize<write_data>(stream, data.buffer, data.size);
	}

	ECS_INLINE size_t WriteWithSize(uintptr_t* stream, const void* data, size_t data_size, bool write_data) {
		return write_data ? WriteWithSize<true>(stream, data, data_size) : WriteWithSize<false>(stream, data, data_size);
	}

	ECS_INLINE size_t WriteWithSize(uintptr_t* stream, Stream<void> data, bool write_data) {
		return write_data ? WriteWithSize<true>(stream, data) : WriteWithSize<false>(stream, data);
	}

	// -----------------------------------------------------------------------------------------
	
	template<bool write_data>
	ECS_INLINE size_t WriteWithSizeShort(uintptr_t* stream, const void* data, unsigned short data_size) {
		return Write<write_data>(stream, &data_size, sizeof(data_size)) + Write<write_data>(stream, data, data_size);
	}

	template<bool write_data>
	ECS_INLINE size_t WriteWithSizeShort(uintptr_t* stream, Stream<void> data) {
		return WriteWithSizeShort<write_data>(stream, data.buffer, data.size);
	}

	ECS_INLINE size_t WriteWithSizeShort(uintptr_t* stream, const void* data, unsigned short data_size, bool write_data) {
		return write_data ? WriteWithSizeShort<true>(stream, data, data_size) : WriteWithSizeShort<false>(stream, data, data_size);
	}

	ECS_INLINE size_t WriteWithSizeShort(uintptr_t* stream, Stream<void> data, bool write_data) {
		return write_data ? WriteWithSizeShort<true>(stream, data) : WriteWithSizeShort<false>(stream, data);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t Read(CapacityStream<void>& stream, void* data, size_t data_size) {
		if constexpr (read_data) {
			memcpy(data, (const void*)((uintptr_t)stream.buffer + stream.size), data_size);
			stream.size += data_size;
			return 0;
		}
		else {
			stream.size += data_size;
			return data_size;
		}
	}

	ECS_INLINE size_t Read(CapacityStream<void>& stream, void* data, size_t data_size, bool read_data) {
		return read_data ? Read<true>(stream, data, data_size) : Read<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t Read(uintptr_t* stream, void* data, size_t data_size) {
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

	template<bool read_data, typename T>
	ECS_INLINE size_t Read(uintptr_t* stream, T* data) {
		return Read<read_data>(stream, data, sizeof(*data));
	}

	ECS_INLINE size_t Read(uintptr_t* stream, void* data, size_t data_size, bool read_data) {
		return read_data ? Read<true>(stream, data, data_size) : Read<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReadWithSize(uintptr_t* stream, void* data, size_t& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return Read<read_data>(stream, data, data_size);
	}

	ECS_INLINE size_t ReadWithSize(uintptr_t* stream, void* data, size_t& data_size, bool read_data) {
		return read_data ? ReadWithSize<true>(stream, data, data_size) : ReadWithSize<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReadWithSizeShort(uintptr_t* stream, void* data, unsigned short& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return Read<read_data>(stream, data, data_size);
	}

	ECS_INLINE size_t ReadWithSizeShort(uintptr_t* stream, void* data, unsigned short& data_size, bool read_data) {
		return read_data ? ReadWithSizeShort<true>(stream, data, data_size) : ReadWithSizeShort<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReferenceData(uintptr_t* stream, void** pointer, size_t data_size) {
		if constexpr (read_data) {
			*pointer = (void*)*stream;
			*stream += data_size;

			return 0;
		}
		else {
			return data_size;
		}
	}

	ECS_INLINE size_t ReferenceData(uintptr_t* stream, void** data, size_t data_size, bool read_data) {
		return read_data ? ReferenceData<true>(stream, data, data_size) : ReferenceData<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReferenceDataWithSize(uintptr_t* stream, void** pointer, size_t& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return ReferenceData<read_data>(stream, pointer, data_size);
	}

	ECS_INLINE size_t ReferenceDataWithSize(uintptr_t* stream, void** data, size_t& data_size, bool read_data) {
		return read_data ? ReferenceDataWithSize<true>(stream, data, data_size) : ReferenceDataWithSize<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReferenceDataWithSizeShort(uintptr_t* stream, void** pointer, unsigned short& data_size) {
		Read<true>(stream, &data_size, sizeof(data_size));
		return ReferenceData<read_data>(stream, pointer, data_size);
	}

	ECS_INLINE size_t ReferenceDataWithSizeShort(uintptr_t* stream, void** data, unsigned short& data_size, bool read_data) {
		return read_data ? ReferenceDataWithSizeShort<true>(stream, data, data_size) : ReferenceDataWithSizeShort<false>(stream, data, data_size);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	Stream<void> ReadAllocateData(uintptr_t* stream, AllocatorPolymorphic allocator) {
		size_t size;
		Read<true>(stream, &size, sizeof(size));
		if constexpr (read_data) {
			void* allocation = AllocateEx(allocator, size);
			Read<true>(stream, allocation, size);
			return { allocation, size };
		}
		else {
			Ignore(stream, size);
			return { nullptr, size };
		}
	}

	ECS_INLINE Stream<void> ReadAllocateData(uintptr_t* stream, AllocatorPolymorphic allocator, bool read_data) {
		return read_data ? ReadAllocateData<true>(stream, allocator) : ReadAllocateData<false>(stream, allocator);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	Stream<void> ReadAllocateDataShort(uintptr_t* stream, AllocatorPolymorphic allocator) {
		unsigned short size;
		Read<true>(stream, &size, sizeof(size));
		if constexpr (read_data) {
			void* allocation = AllocateEx(allocator, size);
			Read<true>(stream, allocation, size);
			return { allocation, size };
		}
		else {
			Ignore(stream, size);
			return { nullptr, size };
		}
	}

	ECS_INLINE Stream<void> ReadAllocateDataShort(uintptr_t* stream, AllocatorPolymorphic allocator, bool read_data) {
		return read_data ? ReadAllocateDataShort<true>(stream, allocator) : ReadAllocateDataShort<false>(stream, allocator);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReadOrReferenceData(uintptr_t* stream, void** pointer, size_t data_size, bool reference_data) {
		if (reference_data) {
			return ReferenceData<read_data>(stream, pointer, data_size);
		}
		else {
			return Read<read_data>(stream, *pointer, data_size);
		}
	}

	ECS_INLINE size_t ReadOrReferenceData(uintptr_t* stream, void** data, size_t data_size, bool reference_data, bool read_data) {
		return read_data ? ReadOrReferenceData<true>(stream, data, data_size, reference_data) : ReadOrReferenceData<false>(stream, data, data_size, reference_data);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReadOrReferenceDataWithSize(uintptr_t* stream, void** pointer, size_t& data_size, bool reference_data) {
		if (reference_data) {
			return ReferenceDataWithSize<read_data>(stream, pointer, data_size);
		}
		else {
			return ReadWithSize<read_data>(stream, *pointer, data_size);
		}
	}

	ECS_INLINE size_t ReadOrReferenceDataWithSize(uintptr_t* stream, void** data, size_t& data_size, bool reference_data, bool read_data) {
		return read_data ? ReadOrReferenceDataWithSize<true>(stream, data, data_size, reference_data) : ReadOrReferenceDataWithSize<false>(stream, data, data_size, reference_data);
	}

	// -----------------------------------------------------------------------------------------

	template<bool read_data>
	ECS_INLINE size_t ReadOrReferenceDataWithSizeShort(uintptr_t* stream, void** pointer, unsigned short& data_size, bool reference_data) {
		if (reference_data) {
			return ReferenceDataWithSizeShort<read_data>(stream, pointer, data_size);
		}
		else {
			return ReadWithSizeShort<read_data>(stream, *pointer, data_size);
		}
	}

	ECS_INLINE size_t ReadOrReferenceDataWithSizeShort(uintptr_t* stream, void** data, unsigned short& data_size, bool reference_data, bool read_data) {
		return read_data ? ReadOrReferenceDataWithSizeShort<true>(stream, data, data_size, reference_data) : ReadOrReferenceDataWithSizeShort<false>(stream, data, data_size, reference_data);
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
			else if (IsStreamWithSoA(info.stream_type)) {
				// The SoA case can be handled here as well
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
	// the reading as well. If the allocate_soa_pointer is set to true, then it will allocate
	// All SoA pointers, else it will let you perform the allocation and assignment, and write directly
	// Into it
	template<bool read_data, bool force_allocation = false>
	size_t ReadOrReferenceFundamentalType(
		const Reflection::ReflectionFieldInfo& info,
		void* data,
		uintptr_t& stream,
		unsigned short basic_type_array_count,
		AllocatorPolymorphic allocator,
		bool allocate_soa_pointer
	) {
		if (info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
			Read<read_data>(&stream, data, info.byte_size);
			return 0;
		}
		else {
			if (info.stream_type == Reflection::ReflectionStreamFieldType::BasicTypeArray) {
				unsigned short elements_to_read = ClampMax(info.basic_type_count, basic_type_array_count);
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
							size_t element_byte_size = info.basic_type == Reflection::ReflectionBasicFieldType::Int8 ? sizeof(char) : sizeof(wchar_t);
							size_t allocate_size = byte_size + element_byte_size;
							if (allocator.allocator != nullptr) {
								if (byte_size > 0) {
									void* allocation = Allocate(allocator, allocate_size, info.stream_alignment);
									Read<true>(&stream, allocation, byte_size);

									void* null_terminator = OffsetPointer(allocation, byte_size);
									memset(null_terminator, 0, element_byte_size);

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
										void* allocation = AllocateEx(allocator, allocate_size, info.stream_alignment);
										Read<true>(&stream, allocation, byte_size);

										void* null_terminator = OffsetPointer(allocation, byte_size);
										memset(null_terminator, 0, element_byte_size);

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
			else if (info.stream_type == ReflectionStreamFieldType::PointerSoA) {
				size_t byte_size = 0;

				Read<true>(&stream, &byte_size, sizeof(byte_size));
				if constexpr (read_data) {
					if (allocate_soa_pointer) {
						if (allocator.allocator != nullptr) {
							void** pointer = (void**)data;
							if (byte_size > 0) {
								void* allocation = Allocate(allocator, byte_size, info.stream_alignment);
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
									void* allocation = AllocateEx(allocator, byte_size, info.stream_alignment);
									Read<true>(&stream, allocation, byte_size);

									*pointer = allocation;
								}
							}
						}
					}
					else {
						// Read the data into the buffer
						Read<true>(&stream, *(void**)data, byte_size);
					}
				}
				else {
					Ignore(&stream, byte_size);
				}

				return byte_size;
			}
			// We can put the PointerSoA in the same case as this
			else if (IsStream(info.stream_type)) {
				size_t byte_size = 0;
				Read<true>(&stream, &byte_size, sizeof(byte_size));

				bool update_stream_capacity = false;
				if constexpr (read_data) {
					if (allocator.allocator != nullptr) {
						void** pointer = (void**)data;
						if (byte_size > 0) {
							void* allocation = Allocate(allocator, byte_size, info.stream_alignment);
							Read<true>(&stream, allocation, byte_size);

							*pointer = allocation;
						}
						update_stream_capacity = true;
					}
					else {
						if constexpr (!force_allocation) {
							ReferenceData<true>(&stream, (void**)data, byte_size);
						}
						else {
							void** pointer = (void**)data;
							if (byte_size > 0) {
								void* allocation = AllocateEx(allocator, byte_size, info.stream_alignment);
								Read<true>(&stream, allocation, byte_size);

								*pointer = allocation;
							}
							update_stream_capacity = true;
						}
					}
				}
				else {
					Ignore(&stream, byte_size);
				}

				if constexpr (read_data) {
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

						if (update_stream_capacity) {
							field_stream->capacity = field_stream->size;
						}

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

	// Returns the amount of pointer data
	ECSENGINE_API size_t IgnoreWithSize(uintptr_t* stream);

	// -----------------------------------------------------------------------------------------

	// Returns the amount of pointer data
	ECSENGINE_API size_t IgnoreWithSizeShort(uintptr_t* stream);

	// -----------------------------------------------------------------------------------------

	// Prepares the file for writing, allocates the memory for the temporary buffer and then 
	template<typename Functor>
	bool SerializeWriteFile(Stream<wchar_t> file, AllocatorPolymorphic allocator, size_t allocation_size, bool binary, Functor&& functor) {
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TRUNCATE_FILE;
		access_flags |= binary ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		ECS_FILE_STATUS_FLAGS flags = FileCreate(file, &file_handle, access_flags);
		if (flags == ECS_FILE_STATUS_OK) {
			ScopedFile scoped_file({ file_handle });

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

	// A helper function to be used for serialize functions that handles how to check the buffer capacity
	// The helper returns true if it can continue, else false.
	typedef bool (*SerializeBufferCapacityFunctor)(size_t current_size, size_t& buffer_capacity);

	ECS_INLINE bool SerializeBufferCapacityTrue(size_t current_size, size_t& buffer_capacity) {
		return true;
	}

	ECS_INLINE bool SerializeBufferCapacityAssert(size_t current_size, size_t& buffer_capacity) {
		ECS_ASSERT(buffer_capacity >= current_size, "Serialize memory buffer is not large enough");
		buffer_capacity -= current_size;
		return true;
	}

	ECS_INLINE bool SerializeBufferCapacityBool(size_t current_size, size_t& buffer_capacity) {
		if (buffer_capacity >= current_size) {
			buffer_capacity -= current_size;
			return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------------------------

	
}