#pragma once
#include "../../Reflection/ReflectionTypes.h"

namespace ECSEngine {

#define ECS_SERIALIZATION_OMIT_FIELD

	// -------------------------------------------------------------------------------------------------------------

	namespace Reflection {
		struct ReflectionManager;
	}

	// Return true if the header is valid and the deserialization can continue
	typedef bool (*DeserializeValidateHeader)(Stream<void> header, void* data);

	struct DeserializeFieldInfo {
		Stream<char> name;
		Stream<char> definition;
		Reflection::ReflectionBasicFieldType basic_type;
		Reflection::ReflectionStreamFieldType stream_type;
		unsigned short stream_byte_size;
		unsigned short byte_size;
		unsigned short basic_type_count;
		unsigned int custom_serializer_index;
	};

	struct ECSENGINE_API DeserializeFieldTable {
		struct Type {
			Stream<DeserializeFieldInfo> fields;
			Stream<char> name;
		};

		unsigned int TypeIndex(Stream<char> type_name) const;

		unsigned int FieldIndex(unsigned int type_index, Stream<char> field_name) const;

		Stream<Type> types;
		// Each serializer has the version written at the beginning
		Stream<unsigned int> custom_serializers;
	};

	// Header: optionally write a header into the serialization
	// Write_Type_Table: write at the beginning of the section the field names and the corresponding types
	// Verify_Dependent_Types: if you want to skip the check, set this to false
	// Allocator: an allocator to be used for writing the whole data in memory for commiting then into a file
	// Error_Message: a stream where the error message will be written if an error occurs
	struct SerializeOptions {
		Stream<void> header = { nullptr, 0 };
		bool write_type_table = true;
		bool verify_dependent_types = true;

		AllocatorPolymorphic allocator = { nullptr };

		CapacityStream<char>* error_message = nullptr;
	};

	// Header: optionally read a header from the data
	// Field_Table: it is used to validate that the data inside the file conforms and to help patch up the read
	// if the data layout has changed. Can also be used to force the read of incompatible types
	// Read_Type_Table: read the table at the beginning and read off the fields that match the signature
	// Fail_If_Field_Mismatch: the read will fail if one of the fields has a mismatched type inside the type table
	// Verify_Dependent_Types: if you want to disable this check, set this to false
	// Use_Resizable_Stream_Allocator: use the allocator set for that stream instead of the given field_allocator
	// Validate_Header: a function that can reject the data if the header is not valid
	// Validate_Header_Data: data transmitted to the function
	// File_Allocator: an allocator to be used to read the whole file into memory
	// Field_Allocator: an allocator to be used to read off streams of data into the fields
	// Backup Allocator: an allocator to be used if there are incompatible user defined types
	// or streams who's data type has changed
	// Error_Message: a stream where an error message will be written if one occurs
	struct DeserializeOptions {
		// It returns the field allocator according to the given options
		AllocatorPolymorphic GetFieldAllocator(Reflection::ReflectionStreamFieldType field_type, const void* data) const;

		Stream<void> header = { nullptr, 0 };

		DeserializeFieldTable* field_table = nullptr;
		bool read_type_table = true;
		bool fail_if_field_mismatch = false;
		bool verify_dependent_types = true;
		bool use_resizable_stream_allocator = false;

		DeserializeValidateHeader validate_header = nullptr;
		void* validate_header_data = nullptr;
		
		AllocatorPolymorphic file_allocator = { nullptr };
		AllocatorPolymorphic field_allocator = { nullptr };
		AllocatorPolymorphic backup_allocator = { nullptr };

		CapacityStream<char>* error_message = nullptr;
	};

	enum ECS_SERIALIZE_CODE : unsigned char {
		ECS_SERIALIZE_OK,
		ECS_SERIALIZE_COULD_NOT_OPEN_OR_WRITE_FILE,
		ECS_SERIALIZE_MISSING_DEPENDENT_TYPES,
	};

	enum ECS_DESERIALIZE_CODE : unsigned char {
		ECS_DESERIALIZE_OK,
		ECS_DESERIALIZE_COULD_NOT_OPEN_OR_READ_FILE,
		ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES,
		ECS_DESERIALIZE_INVALID_HEADER,
		ECS_DESERIALIZE_FIELD_TYPE_MISMATCH,
		ECS_DESERIALIZE_CORRUPTED_FILE
	};

	// Takes into consideration the custom serializer aswell
	ECSENGINE_API bool SerializeHasDependentTypes(const Reflection::ReflectionManager* reflection_manager, const Reflection::ReflectionType* type);

	// Serializes into a temporary memory buffer, then commits to the file
	// Allocator nullptr means use malloc
	ECSENGINE_API ECS_SERIALIZE_CODE Serialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		const void* data,
		Stream<wchar_t> file,
		SerializeOptions* options = nullptr
	);

	// Serializes into memory
	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data
	ECSENGINE_API ECS_SERIALIZE_CODE Serialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		const void* data,
		uintptr_t& stream,
		SerializeOptions* options = nullptr
	);

	ECSENGINE_API size_t SerializeSize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		const void* data,
		SerializeOptions* options = nullptr
	);

	ECSENGINE_API void SerializeFieldTable(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		uintptr_t& stream
	);

	ECSENGINE_API size_t SerializeFieldTableSize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type
	);

	// It reads the whole file into a temporary buffer and then deserializes from memory
	// If the fields reference data from the file, then the file data will be kept alive
	// To get the file pointer allocated, provide a file_data. It must be deallocated manually
	// if it is not nullptr. If some data is referenced but no file_data is provided and no field allocator
	// is specified, then the data will be deallocated and lost if allocated from malloc, from an allocator
	// it should still be valid
	ECSENGINE_API ECS_DESERIALIZE_CODE Deserialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* address,
		Stream<wchar_t> file,
		DeserializeOptions* options = nullptr,
		void** file_data = nullptr
	);

	// The stream is taken as uintptr_t because for aggregate serialization this will point at the end
	// of the written data. The pointer data will point to the values inside the stream - are not stable
	// The user defined allocator is used to allocate when you have streams of user defined types
	// Since the types are not stored as they are, they cannot be referenced inside the stream
	// or for types which are incompatible
	// This allocator can be nullptr if an option is specified with a field allocator set
	ECSENGINE_API ECS_DESERIALIZE_CODE Deserialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* address,
		uintptr_t& stream,
		DeserializeOptions* options = nullptr
	);

	// Returns the amount of pointer data bytes
	// Returns -1 if the deserialize failed
	// Can get the actual code by providing a pointer to a ECS_DESERIALIZE_CODE
	ECSENGINE_API size_t DeserializeSize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		uintptr_t& data,
		DeserializeOptions* options = nullptr,
		ECS_DESERIALIZE_CODE* code = nullptr
	);

	// Streams will be written into the memory allocator
	// A good default capacity should be ECS_KB * 8 for it
	// If the type size is 0 it means that the table has been corrupted
	ECSENGINE_API DeserializeFieldTable DeserializeFieldTableFromData(
		uintptr_t& data,
		AllocatorPolymorphic memory
	);

	// Returns how many bytes from the data are occupied by the type table in order to skip it
	// If it returns -1 then the field table is corrupted and the data size cannot be determined
	ECSENGINE_API size_t DeserializeFieldTableFromDataSize(
		uintptr_t data
	);

	// It will ignore the current type. It must be placed after the deserialize table has been called on the
	// the data
	ECSENGINE_API void IgnoreDeserialize(
		uintptr_t& data,
		DeserializeFieldTable field_table
	);

}