#pragma once
#include "../../Reflection/ReflectionTypes.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------

	// PERFORMANCE TODO: Make the serialize/deserialize functions take a count? This can greatly speed up
	// Processing for buffers since some values can be cached outside the loop, resulting in a much tighter
	// Inner loop

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionPassdownInfo;
	}

	struct DeserializeTypeNameRemapping;
	struct WriteInstrument;
	struct ReadInstrument;

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
		DeserializeFieldInfoFlags flags;
		Reflection::ReflectionBasicFieldType basic_type;
		Reflection::ReflectionStreamFieldType stream_type;
		unsigned char stream_alignment;
		unsigned short stream_byte_size;
		unsigned short byte_size;
		unsigned short basic_type_count;
		unsigned short pointer_offset;
		unsigned int custom_serializer_index;
	};

	struct ECSENGINE_API DeserializeFieldTable {
		struct Type {
			// Returns the index of the field that corresponds to the given name, else -1
			size_t FindField(Stream<char> name) const;

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
	// OmitFields: optionally tell the serializer to omit fields of certain types
	// Error_Message: a stream where the error message will be written if an error occurs
	// Passdown_Info: a structure where information is accumulated about prior fields when iterating.
	// Should be left nullptr for the top level function that calls the serialize function
	struct SerializeOptions {
		Stream<void> header = { nullptr, 0 };
		bool write_type_table = true;
		bool write_type_table_tags = false;
		bool verify_dependent_types = true;

		Stream<SerializeOmitField> omit_fields = { nullptr, 0 };

		CapacityStream<char>* error_message = nullptr;
		Reflection::ReflectionPassdownInfo* passdown_info = nullptr;
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
	// Error_Message: a stream where an error message will be written if one occurs
	// Passdown_Info: A structure where information from previous fields is gathered to be used
	// By next fields. Should be left nullptr for the top level function that calls the deserialize function
	struct DeserializeOptions {
		// It returns the resizable stream allocator, if the field type is of resizable stream and this allocator was enabled, 
		// Else returns the fallback allocator
		ECS_INLINE AllocatorPolymorphic GetFieldAllocator(Reflection::ReflectionStreamFieldType field_type, const void* data, AllocatorPolymorphic fallback_allocator) const {
			if (field_type == Reflection::ReflectionStreamFieldType::ResizableStream && use_resizable_stream_allocator) {
				return ((ResizableStream<void>*)data)->allocator;
			}
			return fallback_allocator;
		}

		unsigned int version = -1;

		Stream<void> header = { nullptr, 0 };

		const DeserializeFieldTable* field_table = nullptr;
		// It is used for skipping fields. It can be specified such that it won't need to be recreated multiple times
		const Reflection::ReflectionManager* deserialized_field_manager = nullptr;

		bool read_type_table = true;
		bool read_type_table_tags = false;
		bool fail_if_field_mismatch = false;
		bool verify_dependent_types = true;
		bool use_resizable_stream_allocator = false;
		bool default_initialize_missing_fields = false;
		// If a type has an overall allocator, it will initialize it and provide that allocator as a fallback
		// For all the other allocations that are made to that instance
		bool initialize_type_allocators = false;
		// If a type field has an allocator assigned, it will use that to make allocations instead
		bool use_type_field_allocators = false;

		DeserializeValidateHeader validate_header = nullptr;
		void* validate_header_data = nullptr;

		Stream<SerializeOmitField> omit_fields = { nullptr, 0 };
		
		AllocatorPolymorphic field_allocator = { nullptr };

		CapacityStream<char>* error_message = nullptr;
		Reflection::ReflectionPassdownInfo* passdown_info = nullptr;
	};

	enum ECS_SERIALIZE_CODE : unsigned char {
		ECS_SERIALIZE_OK,
		ECS_SERIALIZE_MISSING_DEPENDENT_TYPES,
		ECS_SERIALIZE_CUSTOM_TYPE_FAILED,
		ECS_SERIALIZE_INSTRUMENT_FAILURE
	};

	enum ECS_DESERIALIZE_CODE : unsigned char {
		ECS_DESERIALIZE_OK,
		ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES,
		ECS_DESERIALIZE_INVALID_HEADER,
		ECS_DESERIALIZE_FIELD_TYPE_MISMATCH,
		// This value signals that a new allocator entry was added, and we don't know how to initialize it
		// For the moment, this is not possible
		ECS_DESERIALIZE_NEW_ALLOCATOR_ENTRY,
		ECS_DESERIALIZE_INSTRUMENT_FAILURE,
		ECS_DESERIALIZE_CORRUPTED_FILE
	};

	// Takes into consideration the custom serializer as well
	ECSENGINE_API bool SerializeHasDependentTypes(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type, 
		Stream<SerializeOmitField> omit_fields = { nullptr, 0 }
	);

	ECSENGINE_API ECS_SERIALIZE_CODE Serialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		const void* data,
		WriteInstrument* write_instrument,
		SerializeOptions* options = nullptr
	);

	// Convenience function that uses a buffered file writer for the given file
	ECSENGINE_API ECS_SERIALIZE_CODE Serialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		const void* data,
		Stream<wchar_t> file,
		SerializeOptions* options = nullptr
	);

	ECSENGINE_API bool SerializeFieldTable(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		WriteInstrument* write_instrument,
		Stream<SerializeOmitField> omit_fields = {},
		bool write_tags = false
	);

	ECSENGINE_API ECS_DESERIALIZE_CODE Deserialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* address,
		ReadInstrument* read_instrument,
		DeserializeOptions* options = nullptr
	);

	// Convenience function that uses a buffered file reader for the given file
	ECSENGINE_API ECS_DESERIALIZE_CODE Deserialize(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* address,
		Stream<wchar_t> file,
		DeserializeOptions* options = nullptr
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

	// If the types size is 0 it means that the table has been corrupted or an error occured
	ECSENGINE_API DeserializeFieldTable DeserializeFieldTableFromData(
		ReadInstrument* read_instrument,
		AllocatorPolymorphic temporary_allocator,
		const DeserializeFieldTableOptions* options = nullptr
	);

	// It will ignore the current type + the deserialize field table
	// Suitable for ignoring a single type at a time
	// If the version is left at -1, it will use the version from the field table
	ECSENGINE_API bool IgnoreDeserialize(
		ReadInstrument* read_instrument,
		const DeserializeFieldTableOptions* options = nullptr
	);;

	// It will ignore the current type (the first type in the field table). If the deserialized manager is not available, 
	// it will create it inside (useful for ignoring multiple elements from the same time). Can optionally give an array of name_remappings
	ECSENGINE_API bool IgnoreDeserialize(
		ReadInstrument* read_instrument,
		const DeserializeFieldTable& field_table,
		const DeserializeFieldTableOptions* options = nullptr,
		const Reflection::ReflectionManager* deserialized_manager = nullptr,
		Stream<DeserializeTypeNameRemapping> name_remapping = {}
	);

	// It will ignore a type identified by the index inside the field table. If the deserialized manager is not available, 
	// it will create it inside (useful for ignoring multiple elements from the same time). Can optionally give an array of name_remappings
	ECSENGINE_API bool IgnoreDeserialize(
		ReadInstrument* read_instrument,
		const DeserializeFieldTable& field_table,
		unsigned int field_table_type_index,
		const DeserializeFieldTableOptions* options = nullptr,
		const Reflection::ReflectionManager* deserialized_manager = nullptr,
		Stream<DeserializeTypeNameRemapping> name_remappings = {}
	);

	struct SerializeReflectionManagerOptions {
		// The name of the types to be serialized. If left empty, then it will write all types
		Stream<Stream<char>> type_names = {};
		// If specified, only these hierarchies will be written. By default, all hierarchies are considered (even types that
		// Are outside all hierarchies). If you want to include types that do not belong to any hierarchy, include -1.
		Stream<unsigned int> hierarchy_indices = {};
		// In case you want to omit some extra fields, you can do that with this field
		Stream<SerializeOmitField> omit_fields = {};
		// If this flag is set to true, then if you have specified a selection of types to be written,
		// It will not include the type dependencies of the types you specified
		bool direct_types_only = false;
	};

	// Writes the types this reflection manager contains to a specified write instrument. With the options
	// Parameter, you can control how the serialization is done. By default, it will write all reflection types.
	// Returns true if it succeeded, else false
	ECSENGINE_API bool SerializeReflectionManager(
		const Reflection::ReflectionManager* reflection_manager,
		WriteInstrument* write_instrument,
		const SerializeReflectionManagerOptions* options = nullptr
	);

	// Reads a previous serialization into the given reflection manager. The added types will not belong to any folder hierarchy.
	// It will fill in the output field table with all the types that have been read, including the custom serializer versions.
	// The fields allocated for the field table will come from the temporary allocator given. If the deserialization fails,
	// It will not deallocate the buffers that it has used, this is why it is important for the allocator to be temporary, such
	// That it can be discarded if that happens.
	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeReflectionManager(
		Reflection::ReflectionManager* reflection_manager,
		ReadInstrument* read_instrument,
		AllocatorPolymorphic temporary_allocator,
		DeserializeFieldTable* field_table
	);

#pragma region String versions

	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API ECS_SERIALIZE_CODE SerializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> definition,
		const void* data,
		WriteInstrument* write_instrument,
		SerializeOptions* options = nullptr,
		Stream<char> tags = {}
	);

	// Determines if it is a reflected type or a custom type and call the function appropriately
	// Convenience function that uses a buffered file writer to write to the provided file
	ECSENGINE_API ECS_SERIALIZE_CODE SerializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> definition,
		const void* data,
		Stream<wchar_t> file,
		SerializeOptions* options = nullptr,
		Stream<char> tags = {}
	);

	// Determines if it is a reflected type or a custom type and call the function appropriately
	ECSENGINE_API ECS_DESERIALIZE_CODE DeserializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> definition,
		void* address,
		ReadInstrument* read_instrument,
		DeserializeOptions* options = nullptr,
		Stream<char> tags = {}
	);

	// Determines if it is a reflected type or a custom type and call the function appropriately
	// Convenience function that uses a buffered file reader to read from the provided file
	ECSENGINE_API ECS_DESERIALIZE_CODE DeserializeEx(
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> definition,
		void* address,
		Stream<wchar_t> file,
		DeserializeOptions* options = nullptr,
		Stream<char> tags = {}
	);
	
#pragma endregion

}