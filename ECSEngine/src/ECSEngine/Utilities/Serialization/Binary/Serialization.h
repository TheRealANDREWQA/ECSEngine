#pragma once
#include "../../Reflection/ReflectionTypes.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------

	// PERFORMANCE TODO: Make the serialize/deserialize functions take a count? This can greatly speed up
	// Processing for buffers since some values can be cached outside the loop, resulting in a much tighter
	// Inner loop

	namespace Reflection {
		struct ReflectionManager;
	}

	struct DeserializeTypeNameRemapping;

	// Return true if the header is valid and the deserialization can continue
	typedef bool (*DeserializeValidateHeader)(Stream<void> header, void* data);

	struct DeserializeFieldInfoFlags {
		union {
			unsigned char value = 0;
			struct {
				// When the field is tagged as ECS_GIVE_SIZE_REFLECTION this boolean will be set to true
				// such that the deserializer will know not to treat this as a user defined type and try to
				// find its definition in the deserialization table
				bool user_defined_as_blittable : 1;
			};
		};
	};

	struct DeserializeFieldInfo {
		Stream<char> name;
		Stream<char> definition;
		Stream<char> tag;
		Reflection::ReflectionBasicFieldType basic_type;
		Reflection::ReflectionStreamFieldType stream_type;
		unsigned short stream_byte_size;
		unsigned short byte_size;
		unsigned short basic_type_count;
		unsigned int custom_serializer_index;
		DeserializeFieldInfoFlags flags;
		unsigned short pointer_offset;
	};

	struct ECSENGINE_API DeserializeFieldTable {
		struct Type {
			Stream<DeserializeFieldInfo> fields;
			Stream<char> name;
			Stream<char> tag;

			// We record these as well in order to have the identical reproduction
			// Of the serialized type
			unsigned int byte_size;
			unsigned int alignment;
			bool is_blittable;
			bool is_blittable_with_pointer;
		};

		unsigned int TypeIndex(Stream<char> type_name) const;

		unsigned int FieldIndex(unsigned int type_index, Stream<char> field_name) const;

		// Returns true if the reflected type is the same as the one in the file
		// The name_remapping is applied to the type's field definitions such that they can be mapped
		// to previous definitions
 		bool IsUnchanged(
			unsigned int type_index, 
			const Reflection::ReflectionManager* reflection_manager, 
			const Reflection::ReflectionType* type,
			Stream<DeserializeTypeNameRemapping> name_remappings = { nullptr, 0 }
		) const;

		// Returns true if the reflected type from the serialization is blittable, else false
		bool IsBlittable(unsigned int type_index) const;

		size_t TypeByteSize(unsigned int type_index) const;

		// Writes all types into the reflection manager. A stack allocator should be passed such that small allocations can be made
		// Can optionally specify if the names and the definition for the fields are allocated from the given allocator
		void ToNormalReflection(
			Reflection::ReflectionManager* reflection_manager, 
			AllocatorPolymorphic allocator,
			bool allocate_all = false,
			bool check_before_insertion = false
		) const;

		// Fills in the types that came from this deserialize table
		void ExtractTypesFromReflection(Reflection::ReflectionManager* reflection_manager, CapacityStream<Reflection::ReflectionType*> types) const;

		unsigned int serialize_version;
		Stream<Type> types;
		// Each serializer has the version written at the beginning
		Stream<unsigned int> custom_serializers;
	};

	struct SerializeOmitField {
		Stream<char> type;
		Stream<char> name;
	};

	ECSENGINE_API bool SerializeShouldOmitField(Stream<char> type, Stream<char> name, Stream<SerializeOmitField> omit_fields);

	ECSENGINE_API bool AssetSerializeOmitFieldsExist(const Reflection::ReflectionManager* reflection_manager, Stream<SerializeOmitField> omit_fields);

	// Based on which fields to keep, it will populate all the omit fields such that only the given fields will be selected
	ECSENGINE_API void GetSerializeOmitFieldsFromExclude(
		const Reflection::ReflectionManager* reflection_manager, 
		CapacityStream<SerializeOmitField>& omit_fields,
		Stream<char> type_name,
		Stream<Stream<char>> fields_to_keep
	);

	// Header: optionally write a header into the serialization
	// Write_Type_Table: write at the beginning of the section the field names and the corresponding types
	// Write_Type_Table_Tags: write type tags and fields tags to the file in the header table
	// Verify_Dependent_Types: if you want to skip the check, set this to false
	// SettingsAllocator: an allocator to be used for writing the whole data in memory for commiting then into a file
	// OmitFields: optionally tell the serializer to omit fields of certain types
	// Error_Message: a stream where the error message will be written if an error occurs
	struct SerializeOptions {
		Stream<void> header = { nullptr, 0 };
		bool write_type_table = true;
		bool write_type_table_tags = false;
		bool verify_dependent_types = true;

		AllocatorPolymorphic allocator = { nullptr };
		Stream<SerializeOmitField> omit_fields = { nullptr, 0 };

		CapacityStream<char>* error_message = nullptr;
	};

	// Version: If left at -1, it will deserialize based on the version in the file, else it will use the version given
	// Header: optionally read a header from the data
	// Field_Table: it is used to validate that the data inside the file conforms and to help patch up the read
	// if the data layout has changed. Can also be used to force the read of incompatible types
	// Read_Type_Table: read the table at the beginning and read off the fields that match the signature
	// Read_Type_Table_Tags: if the file was written with tags, this will make sure to parse them correctly
	// Fail_If_Field_Mismatch: the read will fail if one of the fields has a mismatched type inside the type table
	// Verify_Dependent_Types: if you want to disable this check, set this to false
	// Use_Resizable_Stream_Allocator: use the allocator set for that stream instead of the given field_allocator
	// Default_Initialize_Missing_Fields: if a field is not found in the file, it will be default initialized
	// Validate_Header: a function that can reject the data if the header is not valid
	// Validate_Header_Data: data transmitted to the function
	// OmitFields: optionally tell the deserializer to ignore certain fields
	// File_Allocator: an allocator to be used to read the whole file into memory
	// Field_Allocator: an allocator to be used to read off streams of data into the fields
	// Backup SettingsAllocator: an allocator to be used if there are incompatible user defined types
	// or streams whose data type has changed
	// Error_Message: a stream where an error message will be written if one occurs
	struct DeserializeOptions {
		// It returns the field allocator according to the given options
		ECS_INLINE AllocatorPolymorphic GetFieldAllocator(Reflection::ReflectionStreamFieldType field_type, const void* data) const {
			if (field_type == Reflection::ReflectionStreamFieldType::ResizableStream && use_resizable_stream_allocator) {
				return ((ResizableStream<void>*)data)->allocator;
			}
			return field_allocator;
		}

		unsigned int version = -1;

		Stream<void> header = { nullptr, 0 };

		DeserializeFieldTable* field_table = nullptr;
		// It is used for skipping fields. It can be specified such that it won't need to be recreated multiple times
		Reflection::ReflectionManager* deserialized_field_manager = nullptr;

		bool read_type_table = true;
		bool read_type_table_tags = false;
		bool fail_if_field_mismatch = false;
		bool verify_dependent_types = true;
		bool use_resizable_stream_allocator = false;
		bool default_initialize_missing_fields = false;

		DeserializeValidateHeader validate_header = nullptr;
		void* validate_header_data = nullptr;

		Stream<SerializeOmitField> omit_fields = { nullptr, 0 };
		
		AllocatorPolymorphic file_allocator = { nullptr };
		AllocatorPolymorphic field_allocator = { nullptr };
		AllocatorPolymorphic backup_allocator = { nullptr };

		CapacityStream<char>* error_message = nullptr;
	};

	enum ECS_SERIALIZE_CODE : unsigned char {
		ECS_SERIALIZE_OK,
		ECS_SERIALIZE_COULD_NOT_OPEN_OR_WRITE_FILE,
		ECS_SERIALIZE_MISSING_DEPENDENT_TYPES,
		ECS_SERIALIZE_CUSTOM_TYPE_FAILED
	};

	enum ECS_DESERIALIZE_CODE : unsigned char {
		ECS_DESERIALIZE_OK,
		ECS_DESERIALIZE_COULD_NOT_OPEN_OR_READ_FILE,
		ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES,
		ECS_DESERIALIZE_INVALID_HEADER,
		ECS_DESERIALIZE_FIELD_TYPE_MISMATCH,
		ECS_DESERIALIZE_CORRUPTED_FILE
	};

	// Takes into consideration the custom serializer as well
	ECSENGINE_API bool SerializeHasDependentTypes(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type, 
		Stream<SerializeOmitField> omit_fields = { nullptr, 0 }
	);

	// Serializes into a temporary memory buffer, then commits to the file
	// SettingsAllocator nullptr means use Malloc
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
		uintptr_t& stream,
		Stream<SerializeOmitField> omit_fields = { nullptr, 0 },
		bool write_tags = false
	); 

	ECSENGINE_API size_t SerializeFieldTableSize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		Stream<SerializeOmitField> omit_fields = { nullptr, 0 },
		bool write_tags = false
	);

	// It reads the whole file into a temporary buffer and then deserializes from memory
	// If the fields reference data from the file, then the file data will be kept alive
	// To get the file pointer allocated, provide a file_data. It must be deallocated manually
	// if it is not nullptr. If some data is referenced but no file_data is provided and no field allocator
	// is specified, then the data will be deallocated and lost if allocated from Malloc, from an allocator
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

	struct DeserializeFieldTableOptions {
		unsigned int version = -1;
		bool read_type_tags = false;
	};

	ECS_INLINE unsigned int DeserializeFieldTableVersion(unsigned int file_version, const DeserializeFieldTableOptions* options) {
		unsigned int version = -1;
		if (options != nullptr) {
			version = options->version;
		}
		return version == -1 ? file_version : version;
	}

	ECS_INLINE bool DeserializeFieldTableReadTags(const DeserializeFieldTableOptions* options) {
		return options != nullptr ? options->read_type_tags : false;
	}

	// Streams will be written into the memory allocator
	// A good default capacity should be ECS_KB * 8 for it
	// If the type size is 0 it means that the table has been corrupted
	ECSENGINE_API DeserializeFieldTable DeserializeFieldTableFromData(
		uintptr_t& data,
		AllocatorPolymorphic memory,
		const DeserializeFieldTableOptions* options = nullptr
	);

	// Returns how many bytes from the data are occupied by the type table in order to skip it
	// If it returns -1 then the field table is corrupted and the data size cannot be determined
	ECSENGINE_API size_t DeserializeFieldTableFromDataSize(
		uintptr_t data,
		const DeserializeFieldTableOptions* options = nullptr
	);

	// It will ignore the current type + the deserialize field table
	// Suitable for ignoring a single type at a time
	// If the version is left at -1, it will use the version from the field table
	ECSENGINE_API void IgnoreDeserialize(
		uintptr_t& data,
		const DeserializeFieldTableOptions* options = nullptr
	);

	// It will ignore the current type. It must be placed after the deserialize table has been called on the
	// the data. If the deserialized manager is not available, it will create it inside (useful for ignoring
	// multiple elements from the same time). Can optionally give an array of name_remappings
	ECSENGINE_API void IgnoreDeserialize(
		uintptr_t& data,
		DeserializeFieldTable field_table,
		const DeserializeFieldTableOptions* options = nullptr,
		const Reflection::ReflectionManager* deserialized_manager = nullptr,
		Stream<DeserializeTypeNameRemapping> name_remapping = { nullptr, 0 }
	);

#pragma region String versions

	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API ECS_SERIALIZE_CODE SerializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> string,
		const void* data,
		Stream<wchar_t> file,
		SerializeOptions* options = nullptr
	);

	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API ECS_SERIALIZE_CODE SerializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> string,
		const void* data,
		uintptr_t& ptr,
		SerializeOptions* options = nullptr
	);

	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API size_t SerializeSizeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> string,
		const void* data,
		SerializeOptions* options = nullptr
	);

	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API ECS_DESERIALIZE_CODE DeserializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> string,
		void* address,
		Stream<wchar_t> file,
		DeserializeOptions* options = nullptr,
		void** file_data = nullptr
	);

	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API ECS_DESERIALIZE_CODE DeserializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> string,
		void* address,
		uintptr_t& stream,
		DeserializeOptions* options = nullptr
	);
	
	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API size_t DeserializeSizeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> string,
		uintptr_t& stream,
		DeserializeOptions* options = nullptr
	);

#pragma endregion

}