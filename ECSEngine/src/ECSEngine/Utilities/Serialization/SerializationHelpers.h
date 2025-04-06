#pragma once
#include "../../Core.h"
#include "../File.h"
#include "../../Containers/Stream.h"
#include "../Reflection/ReflectionTypes.h"
#include "../Reflection/ReflectionCustomTypesEnum.h"
#include "../../Math/MathHelpers.h"
#include "../Utilities.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
	}

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

	template<typename T>
	ECS_INLINE size_t WriteDeduce(uintptr_t* stream, const T* data, bool write_data) {
		return write_data ? Write<true>(stream, data) : Write<false>(stream, data);
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
		ECS_ASSERT(data_size <= UINT_MAX, "Integer overflow on stream read.");
		if constexpr (read_data) {
			memcpy(data, (const void*)((uintptr_t)stream.buffer + stream.size), data_size);
			stream.size += (unsigned int)data_size;
			return 0;
		}
		else {
			stream.size += (unsigned int)data_size;
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

	template<typename T>
	ECS_INLINE size_t ReadDeduce(uintptr_t* stream, T* data, bool read_data) {
		return read_data ? Read<true>(stream, data) : Read<false>(stream, data);
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
			void* allocation = Allocate(allocator, size);
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
			void* allocation = Allocate(allocator, size);
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

	struct SerializeOptions;
	struct WriteInstrument;
	struct ReadInstrument;

	struct SerializeCustomTypeWriteFunctionData {
		WriteInstrument* write_instrument;
		const Reflection::ReflectionManager* reflection_manager;
		const void* data;
		Stream<char> definition;

		SerializeOptions* options;

		// This is an optional field. Tags can be used to alter the behavior of the serializer. These can be user defined tags
		// Or the reflection type based tags.
		Stream<char> tags = {};
	};

	// Returns true if it succeeded, else false
	typedef bool (*SerializeCustomTypeWriteFunction)(SerializeCustomTypeWriteFunctionData* data);

#define ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(name) bool SerializeCustomTypeWrite_##name(SerializeCustomTypeWriteFunctionData* data)

	struct DeserializeOptions;

	struct ECSENGINE_API SerializeCustomTypeReadFunctionData {
		// Returns true if the functor should ignore the data, meaning it shouldn't
		// Actually write into the deserialize target
		bool ShouldIgnoreData() const;

		// This contains the versions for all custom types, since they might be nested
		// And require a special version each
		unsigned int custom_types_version[Reflection::ECS_REFLECTION_CUSTOM_TYPE_COUNT];
		// This can be used by custom serializers to take into consideration that this field
		// was allocated prior - fields should be considered invalid
		bool was_allocated;
		// This flag can be set even for instruments that are not size determination, in order
		// To skip the data, not to actual read it
		bool ignore_data = false;

		ReadInstrument* read_instrument;
		const Reflection::ReflectionManager* reflection_manager;
		void* data;
		Stream<char> definition;

		DeserializeOptions* options;

		// This is an optional field. Tags can be used to alter the behavior of the deserializer. These can be user defined tags
		// Or the reflection type based tags.
		Stream<char> tags = {};
	};

	// Returns true if it succeeded, else false
	typedef bool (*SerializeCustomTypeReadFunction)(SerializeCustomTypeReadFunctionData* data);

#define ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(name) bool SerializeCustomTypeRead_##name(SerializeCustomTypeReadFunctionData* data)

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

	struct ECSENGINE_API SerializeCustomWriteHelperData {
		// Initializes the 2 fields alongside the definition info
		void Set(SerializeCustomTypeWriteFunctionData* write_data, Stream<char> definition, Stream<char> tags);
		
		Reflection::ReflectionDefinitionInfo definition_info;
		SerializeCustomTypeWriteFunctionData* write_data;
		Stream<void> data_to_write;
		Stream<char> definition;
		Stream<char> tags;
		// This is an optional field, in case you want to address a subfield from a larger structure.
		// If left at 0, it will use the definition info byte size
		size_t element_stride = 0;
		Stream<size_t> indices = { nullptr, 0 };
	};

	// Element_byte_size should be for stream_type different from basic
	// the byte size of the target, not of the stream's
	// It does not prefix the stream with its size - should be done outside. Returns the number of bytes written
	ECSENGINE_API bool SerializeCustomWriteHelper(SerializeCustomWriteHelperData* data);

	struct ECSENGINE_API DeserializeCustomReadHelperData {
		// Initializes the 2 fields alongside the definition info
		void Set(SerializeCustomTypeReadFunctionData* read_data, Stream<char> definition, Stream<char> tags);

		Stream<char> tags;
		Stream<char> definition;
		Reflection::ReflectionDefinitionInfo definition_info;
		SerializeCustomTypeReadFunctionData* read_data;
		size_t element_count;

		// If this is 0 and the element_count is 1 then it will assume that a single instance is to be deserialized
		// and no buffer will be allocated
		size_t elements_to_allocate;

		union {
			void** allocated_buffer;
			void* deserialize_target;
		};
		AllocatorPolymorphic override_allocator = nullptr;
		Stream<size_t> indices = { nullptr, 0 };
	};

	// The element_byte_size should be the size of the target for streams.
	// Field data should be initialized with the element count to be read
	// Can provide an allocator such that it will allocate from it instead 
	// of the backup allocator. Can be useful for resizable containers
	ECSENGINE_API bool DeserializeCustomReadHelper(DeserializeCustomReadHelperData* data);

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

	ECSENGINE_API bool WriteFundamentalType(const Reflection::ReflectionFieldInfo& info, const void* data, WriteInstrument* write_instrument);

	// -----------------------------------------------------------------------------------------

	enum class ReadOrReferenceFundamentalTypeFlag : unsigned char {
		None,
		ForceAllocation,
		IgnoreData
	};

	ECS_ENUM_BITWISE_OPERATIONS(ReadOrReferenceFundamentalTypeFlag);

	// If the allocator is nullptr, then it will just reference the data
	// If the type is basic type array, the number elements to be read needs to be specified
	// because if reading from a file and the number of elements has changed, then we need to adjust
	// the reading as well. If the allocate_soa_pointer is set to true, then it will allocate
	// All SoA pointers, else it will let you perform the allocation and assignment, and write directly
	// Into it
	template<ReadOrReferenceFundamentalTypeFlag flag = ReadOrReferenceFundamentalTypeFlag::None>
	ECSENGINE_API bool ReadOrReferenceFundamentalType(
		const Reflection::ReflectionFieldInfo& info,
		void* data,
		ReadInstrument* read_instrument,
		unsigned short basic_type_array_count,
		AllocatorPolymorphic allocator,
		bool allocate_soa_pointer
	);

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

			void* allocation = Allocate(allocator, allocation_size);
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
			Deallocate(allocator, allocation);
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
			Deallocate(allocator, contents.buffer);
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

	//// A helper function to be used for serialize functions that handles how to check the buffer capacity
	//// The helper returns true if it can continue, else false.
	//typedef bool (*SerializeBufferCapacityFunctor)(size_t current_size, size_t& buffer_capacity);

	//ECS_INLINE bool SerializeBufferCapacityTrue(size_t current_size, size_t& buffer_capacity) {
	//	return true;
	//}

	//ECS_INLINE bool SerializeBufferCapacityAssert(size_t current_size, size_t& buffer_capacity) {
	//	ECS_ASSERT(buffer_capacity >= current_size, "Serialize memory buffer is not large enough");
	//	buffer_capacity -= current_size;
	//	return true;
	//}

	//ECS_INLINE bool SerializeBufferCapacityBool(size_t current_size, size_t& buffer_capacity) {
	//	if (buffer_capacity >= current_size) {
	//		buffer_capacity -= current_size;
	//		return true;
	//	}
	//	return false;
	//}

	// -----------------------------------------------------------------------------------------

	
}