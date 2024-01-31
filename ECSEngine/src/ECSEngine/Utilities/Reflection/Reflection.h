#pragma once
#include "../../Allocators/MemoryManager.h"
#include "../../Multithreading/TaskManager.h"
#include "../../ECS/InternalStructures.h"
#include "../../Containers/HashTable.h"
#include "../../Containers/AtomicStream.h"
#include "ReflectionTypes.h"

namespace ECSEngine {

	struct World;

	namespace Reflection {
		
		typedef HashTableDefault<ReflectionFieldInfo> ReflectionFieldTable;
		typedef HashTableDefault<ReflectionType> ReflectionTypeTable;
		typedef HashTableDefault<ReflectionEnum> ReflectionEnumTable;

#define ECS_REFLECTION_MAX_TYPE_COUNT (128)
#define ECS_REFLECTION_MAX_ENUM_COUNT (32)
#define ECS_REFLECTION_TYPE_CHANGE_SET_MAX_INDICES (8)

#pragma region Reflection Container Type functions

		enum ECS_REFLECTION_CUSTOM_TYPE_INDEX : unsigned char {
			ECS_REFLECTION_CUSTOM_TYPE_STREAM,
			ECS_REFLECTION_CUSTOM_TYPE_REFERENCE_COUNTED,
			ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET,
			ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET,
			ECS_REFLECTION_CUSTOM_TYPE_DATA_POINTER,
			ECS_REFLECTION_CUSTOM_TYPE_COUNT
		};

		// Must be kept in sync with the ECS_SERIALIZE_CUSTOM_TYPES
		extern ReflectionCustomTypeInterface* ECS_REFLECTION_CUSTOM_TYPES[];

		ECSENGINE_API void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data);

		// E.g. for Template<Type> string should be Template
		ECSENGINE_API bool ReflectionCustomTypeMatchTemplate(ReflectionCustomTypeMatchData* data, const char* string);

		ECSENGINE_API Stream<char> ReflectionCustomTypeGetTemplateArgument(Stream<char> definition);

		struct StreamCustomTypeInterface : public Reflection::ReflectionCustomTypeInterface {
			bool Match(Reflection::ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(Reflection::ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(Reflection::ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(Reflection::ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(Reflection::ReflectionCustomTypeCopyData* data) override;

			bool Compare(Reflection::ReflectionCustomTypeCompareData* data) override;
		};

		struct SparseSetCustomTypeInterface : public Reflection::ReflectionCustomTypeInterface {
			bool Match(Reflection::ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(Reflection::ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(Reflection::ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(Reflection::ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(Reflection::ReflectionCustomTypeCopyData* data) override;

			bool Compare(Reflection::ReflectionCustomTypeCompareData* data) override;
		};

		struct DataPointerCustomTypeInterface : public Reflection::ReflectionCustomTypeInterface {
			bool Match(Reflection::ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(Reflection::ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(Reflection::ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(Reflection::ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(Reflection::ReflectionCustomTypeCopyData* data) override;

			bool Compare(Reflection::ReflectionCustomTypeCompareData* data) override;
		};

		// ---------------------------------------------------------------------------------------------------------------------
		
		// Returns -1 if it is not matched
		ECSENGINE_API unsigned int FindReflectionCustomType(Stream<char> definition);

		// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

		struct ReflectionManagerParseStructuresThreadTaskData;

		struct ReflectionManagerGetQuery {
			ReflectionManagerGetQuery() : strict_compare(false), use_stream_indices(false), tags(nullptr, 0) {}

			union {
				CapacityStream<unsigned int>* indices;
				Stream<CapacityStream<unsigned int>> stream_indices;
			};

			Stream<Stream<char>> tags = { nullptr, 0 };
			bool strict_compare = false; // Uses CompareStrings if set
			bool use_stream_indices = false;
		};

		struct ECSENGINE_API ReflectionManager {
			struct AddedType {
				Stream<char> type_name;
				AllocatorPolymorphic allocator;
				bool coalesced_allocation;
			};
			
			struct FolderHierarchy {
				Stream<wchar_t> root;
				const void* allocated_buffer;
				ResizableStream<AddedType> added_types;
			};

			struct BlittableType {
				Stream<char> name;
				size_t byte_size;
				size_t alignment;
				void* default_data;
			};

			ReflectionManager() {}
			ReflectionManager(AllocatorPolymorphic allocator, size_t type_count = ECS_REFLECTION_MAX_TYPE_COUNT, size_t enum_count = ECS_REFLECTION_MAX_ENUM_COUNT);

			ReflectionManager(const ReflectionManager& other) = default;
			ReflectionManager& operator = (const ReflectionManager& other) = default;

			static void GetKnownBlittableExceptions(CapacityStream<BlittableType>* blittable_types, AllocatorPolymorphic temp_allocator);

			void AddKnownBlittableExceptions();

			void AddBlittableException(Stream<char> definition, size_t byte_size, size_t alignment, const void* default_data = nullptr);

			// Adds an enum which is not bound to any folder hierarchy. If the allocator is nullptr
			// then it will only reference the type streams, not actually copy
			void AddEnum(const ReflectionEnum* enum_, AllocatorPolymorphic allocator = { nullptr });

			// Adds all the enums from the other reflection manager without binding the enums to a particular hierarchy
			void AddEnumsFrom(const ReflectionManager* other);

			// Adds a type which is not bound to any folder hierarchy. If the allocator is nullptr
			// then it will only reference the type streams, not actually copy
			void AddType(const ReflectionType* type, AllocatorPolymorphic allocator = { nullptr }, bool coalesced = true);

			// Adds all the types from the other reflection manager without binding the types to a particular hierarchy
			void AddTypesFrom(const ReflectionManager* other);

			// Adds a type to a certain hierarchy. It will add it as is, it will be deallocated when the hierarchy is freed using the allocator given
			void AddTypeToHierarchy(const ReflectionType* type, unsigned int folder_hierarchy, AllocatorPolymorphic allocator, bool coalesced);

			ECS_INLINE AllocatorPolymorphic Allocator() const {
				return folders.allocator;
			}

			// Returns true if it succededs. It can fail due to expression evaluation
			// or if a referenced type is not yet reflected
			bool BindApprovedData(
				ReflectionManagerParseStructuresThreadTaskData* data,
				unsigned int data_count,
				unsigned int folder_index
			);

			// Returns -1 if it doesn't find it
			unsigned int BlittableExceptionIndex(Stream<char> name) const;

			// Clears all allocations made by reflection types and frees the hash table. If the isolated_use is set to true,
			// then will assume that the types have been added manually. Can optionally specify if the
			// types need to be deallocated (they can be omitted if they reference types stable from outside). 
			// For the normal use it will use the folder hierarchy allocation.
			void ClearTypesFromAllocator(bool isolated_use = false, bool isolated_use_deallocate_types = true);

			// Clears any allocations from the allocator. If the isolated_use is set to true,
			// then will assume that the types have been added manually. Can optionally specify if the
			// types need to be deallocated (they can be omitted if they reference types stable from outside). 
			// For the normal use it will use the folder hierarchy allocation.
			void ClearFromAllocator(bool isolated_use = false, bool isolated_use_deallocate_types = true);

			// Returns the current index; it may change if removals take place
			unsigned int CreateFolderHierarchy(Stream<wchar_t> root);

			void CopyEnums(ReflectionManager* other) const;

			void CopyTypes(ReflectionManager* other) const;

			void DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data);

			// Returns DBL_MAX if the evaluation fails. The expression must have constant terms,
			// ECS_CONSTANT_REFLECT macros or sizeof()/alignof() types that were reflected
			double EvaluateExpression(Stream<char> expression);

			// Returns the byte size and the alignment
			ulong2 FindBlittableException(Stream<char> name) const;

			void FreeFolderHierarchy(unsigned int folder_index);

			ReflectionType* GetType(Stream<char> name);
			ReflectionType* GetType(unsigned int index);

			const ReflectionType* GetType(Stream<char> name) const;
			const ReflectionType* GetType(unsigned int index) const;

			ReflectionEnum* GetEnum(Stream<char> name);
			ReflectionEnum* GetEnum(unsigned int index);

			const ReflectionEnum* GetEnum(Stream<char> name) const;
			const ReflectionEnum* GetEnum(unsigned int index) const;

			// Returns DBL_MAX if it doesn't exist
			double GetConstant(Stream<char> name) const;

			// Returns -1 if it fails
			unsigned int GetHierarchyIndex(Stream<wchar_t> hierarchy) const;
			
			// If the tag is specified it will include only those types that have at least one tag
			// If the hierarchy index is -1, then it will add all types from all hierarchies
			void GetHierarchyTypes(ReflectionManagerGetQuery options, unsigned int hierarchy_index = -1) const;

			unsigned int GetHierarchyCount() const;
			unsigned int GetTypeCount() const;

			void* GetTypeInstancePointer(Stream<char> name, void* instance, unsigned int pointer_index = 0) const;
			void* GetTypeInstancePointer(const ReflectionType* type, void* instance, unsigned int pointer_index = 0) const;

			Stream<char> GetTypeTag(unsigned int type_index) const;
			Stream<char> GetTypeTag(Stream<char> name) const;

			bool HasTypeTag(unsigned int type_index, Stream<char> tag) const;
			bool HasTypeTag(Stream<char> name, Stream<char> tag) const;

			// Verifies if the type has all of its user defined types reflected aswell
			// For serialization, use the other function
			bool HasValidDependencies(Stream<char> type_name) const;
			// Verifies if the type has all of its user defined types reflected aswell
			// For serialization, use the other function
			bool HasValidDependencies(const ReflectionType* type) const;

			bool TryGetType(Stream<char> name, ReflectionType& type) const;
			bool TryGetEnum(Stream<char> name, ReflectionEnum& enum_) const;

			// Copies the constants reflected from another manager into this one
			void InheritConstants(const ReflectionManager* other);

			void InitializeFieldTable();
			void InitializeTypeTable(size_t count);
			void InitializeEnumTable(size_t count);

			// It will not set the paths that need to be searched; thread memory will be allocated from the heap, must be freed manually
			void InitializeParseThreadTaskData(size_t thread_memory, size_t path_count, ReflectionManagerParseStructuresThreadTaskData& data, CapacityStream<char>* error_message = nullptr);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(Stream<wchar_t> root, CapacityStream<char>* faulty_path = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(Stream<wchar_t> root, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, CapacityStream<char>* error_message = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			void RemoveFolderHierarchy(unsigned int index);
			void RemoveFolderHierarchy(Stream<wchar_t> root);

			void RemoveBlittableException(Stream<char> name);

			// keeps reference intact, it is the same as removing all types from the respective folder hierarchy,
			// calling recreate on the folder hierarchy and then process it
			bool UpdateFolderHierarchy(unsigned int index, CapacityStream<char>* error_message = nullptr);

			// keeps reference intact, it is the same as removing all types from the respective folder hierarchy,
			// calling recreate on the folder hierarchy and then process it
			bool UpdateFolderHierarchy(unsigned int index, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			// keeps reference intact
			bool UpdateFolderHierarchy(Stream<wchar_t> root, CapacityStream<char>* error_message = nullptr);

			// keeps reference intact
			bool UpdateFolderHierarchy(Stream<wchar_t> root, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			// It will memset to 0 initially then will set the other fields
			// It will set the fields of the data according to the defaults
			void SetInstanceDefaultData(Stream<char> name, void* data) const;

			// It will memset to 0 initially then will set the other fields
			// It will set the fields of the data according to the defaults
			void SetInstanceDefaultData(unsigned int index, void* data) const;

			// It will memset to 0 initially then will set the other fields
			// It will set the fields of the data according to the defaults
			void SetInstanceDefaultData(const ReflectionType* type, void* data) const;

			// It will memset to 0 initially then will set the other fields
			// It will set the fields of the data according to the defaults
			void SetInstanceFieldDefaultData(const ReflectionField* field, void* data, bool offset_data = true) const;

			// If ignoring some fields, you can set this value manually in order
			// to correctly have the byte size
			void SetTypeByteSize(ReflectionType* type, size_t byte_size);

			// If ignoring some fields, you can set this value manually in order
			// to correctly have the byte size
			void SetTypeAlignment(ReflectionType* type, size_t alignment);

			ReflectionTypeTable type_definitions;
			ReflectionEnumTable enum_definitions;
			ReflectionFieldTable field_table;
			ResizableStream<FolderHierarchy> folders;
			ResizableStream<ReflectionConstant> constants;
			ResizableStream<BlittableType> blittable_types;
		};

		// If there are no user defined types, this version will work
		ECSENGINE_API void SetInstanceFieldDefaultData(const ReflectionField* field, void* data, bool offset_data = true);

		ECSENGINE_API bool IsTypeCharacter(char character);

		ECSENGINE_API void AddEnumDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			const char* opening_parenthese,
			const char* closing_parenthese,
			const char* name,
			unsigned int index
		);

		ECSENGINE_API void AddTypeDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			const char* opening_parenthese,
			const char* closing_parenthese,
			const char* name,
			unsigned int index
		);

		enum ECS_REFLECTION_ADD_TYPE_FIELD_RESULT : unsigned char {
			ECS_REFLECTION_ADD_TYPE_FIELD_FAILED,
			ECS_REFLECTION_ADD_TYPE_FIELD_SUCCESS,
			ECS_REFLECTION_ADD_TYPE_FIELD_OMITTED
		};

		// returns whether or not the field read succeded
		ECSENGINE_API ECS_REFLECTION_ADD_TYPE_FIELD_RESULT AddTypeField(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* last_line_character,
			const char* semicolon_character
		);

		ECSENGINE_API bool DeduceFieldType(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* new_line_character,
			const char* last_valid_character
		);

		// Checks to see if it is a pointer type, not from macros
		ECSENGINE_API bool DeduceFieldTypePointer(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* new_line_character
		);

		ECSENGINE_API bool DeduceFieldTypeStream(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* new_line_character
		);

		ECSENGINE_API void DeduceFieldTypeExtended(
			ReflectionManagerParseStructuresThreadTaskData* data,
			unsigned short& pointer_offset,
			const char* last_type_character,
			ReflectionField& info
		);

		ECSENGINE_API void GetReflectionFieldInfo(
			ReflectionManagerParseStructuresThreadTaskData* data,
			Stream<char> basic_type,
			ReflectionField& info
		);

		ECSENGINE_API ReflectionField GetReflectionFieldInfo(const ReflectionFieldTable* reflection_field_table, Stream<char> basic_type);

		ECSENGINE_API ReflectionField GetReflectionFieldInfo(const ReflectionManager* reflection, Stream<char> basic_type);

		ECSENGINE_API ReflectionField GetReflectionFieldInfoStream(const ReflectionManager* reflection, Stream<char> basic_type, ReflectionStreamFieldType stream_type);

		ECSENGINE_API bool HasReflectStructures(Stream<wchar_t> path);

		// It will deduce the size from the user defined types aswell
		ECSENGINE_API size_t CalculateReflectionTypeByteSize(const ReflectionManager* reflection_manager, const ReflectionType* type);

		// It will deduce the alignment from the user defined types aswell
		ECSENGINE_API size_t CalculateReflectionTypeAlignment(const ReflectionManager* reflection_manager, const ReflectionType* type);

		// Calculates the byte size and the alignment alongside the is_blittable values
		ECSENGINE_API void CalculateReflectionTypeCachedParameters(const ReflectionManager* reflection_manager, ReflectionType* type);

		ECS_INLINE size_t GetReflectionTypeByteSize(const ReflectionType* type) {
			return type->byte_size;
		}

		ECS_INLINE size_t GetReflectionTypeAlignment(const ReflectionType* type) {
			return type->alignment;
		}

		// Returns the index inside the misc_info of the SoA description that contains this field, else -1
		ECSENGINE_API size_t GetReflectionTypeSoaIndex(const ReflectionType* type, Stream<char> field);
		
		// Returns the index inside the misc_info of the SoA description that contains this field, else -1
		ECSENGINE_API size_t GetReflectionTypeSoaIndex(const ReflectionType* type, unsigned int field_index);

		// Returns { nullptr, 0 } if the field is not an SoA entry, else the stream with the size the actual
		// Size from the given data (for a parallel stream entry). You must also pass a bool out parameter
		// Such that you can distinguish between a missing entry and an empty stream
		ECSENGINE_API Stream<void> GetReflectionTypeSoaStream(const ReflectionType* type, const void* data, unsigned int field_index, bool* is_present);

		// Returns -1 if no match was found. It can return 0 for container types
		// which have not had their dependencies met yet.
		ECSENGINE_API size_t SearchReflectionUserDefinedTypeByteSize(
			const ReflectionManager* reflection_manager,
			Stream<char> definition
		);

		// Returns -1 if no match was found. It can return 0 for container types
		// which have not had their dependencies met yet.
		ECSENGINE_API ulong2 SearchReflectionUserDefinedTypeByteSizeAlignment(
			const ReflectionManager* reflection_manager,
			Stream<char> definition
		);

		// Returns -1 if the no match was found. It can return 0 for container types
		// which have not had their dependencies met yet. 
		ECSENGINE_API size_t SearchReflectionUserDefinedTypeAlignment(const ReflectionManager* reflection_manager, Stream<char> definition);

		// It will search in order the reflection manager, then the container type 
		// and at last the enums. If it not a ReflectionType, it will set the name to nullptr,
		// if it is not a container index it will set it to -1, if it is not an enum it will set
		// its name to nullptr
		ECSENGINE_API void SearchReflectionUserDefinedType(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			ReflectionType& type,
			unsigned int& container_index,
			ReflectionEnum& enum_
		);

		// Returns true if it can be copied with memcpy, else false
		// It returns true when all fields are fundamental types non pointer
		ECSENGINE_API bool SearchIsBlittable(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type
		);

		// Returns true if it can be copied with memcpy, else false
		// It returns true when all fields are fundamental types non pointer
		ECSENGINE_API bool SearchIsBlittable(
			const ReflectionManager* reflection_manager,
			Stream<char> definition
		);

		// Returns true if it can be copied with memcpy, else false
		// It returns true when all fields are fundamental types or pointers
		ECSENGINE_API bool SearchIsBlittableWithPointer(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type
		);

		// Returns true if it can be copied with memcpy, else false
		// It returns true when all fields are fundamental types or pointers
		ECSENGINE_API bool SearchIsBlittableWithPointer(
			const ReflectionManager* reflection_manager,
			Stream<char> definition
		);

		// For all fields which are streams it will determine the element byte size and fill it in inside the field
		ECSENGINE_API void SetReflectionTypeFieldsStreamByteSize(
			const ReflectionManager* reflection_manager,
			ReflectionType* reflection_type
		);

		// Copies all blittable fields from one type instance to the other
		ECSENGINE_API void CopyReflectionTypeBlittableFields(
			const Reflection::ReflectionManager* reflection_manager,
			const Reflection::ReflectionType* type,
			const void* source,
			void* destination
		);

		// Works for user defined types as well
		ECSENGINE_API size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, const ReflectionField& field);

		// Copies non used defined field
		ECSENGINE_API void CopyReflectionFieldBasic(const ReflectionFieldInfo* info, const void* source, void* destination, AllocatorPolymorphic allocator);

		ECSENGINE_API size_t GetBasicTypeArrayElementSize(const ReflectionFieldInfo& info);

		// Can optionally specify whether or not to offset into the data
		ECSENGINE_API void* GetReflectionFieldStreamBuffer(const ReflectionFieldInfo& info, const void* data, bool offset_into_data = true);

		// Can optionally specify whether or not to offset into the data
		ECSENGINE_API size_t GetReflectionFieldStreamSize(const ReflectionFieldInfo& info, const void* data, bool offset_into_data = true);

		// Can optionally specify whether or not to offset into the data
		// The size of the void stream is that of the elements, not that of the byte size of the elements
		// This version does not take into account embedded arrays
		ECSENGINE_API Stream<void> GetReflectionFieldStreamVoid(const ReflectionFieldInfo& info, const void* data, bool offset_into_data = true);

		// Can optionally specify whether or not to offset into the data
		// The size of the void stream is that of the elements, not that of the byte size of the elements
		// This version does not take into account embedded arrays
		ECSENGINE_API ResizableStream<void> GetReflectionFieldResizableStreamVoid(const ReflectionFieldInfo& info, const void* data, bool offset_into_data = true);

		// Can optionally specify whether or not to offset into the data
		// This version takes into account embedded arrays. The size of the void stream is the element count, not
		// the byte size of the elements
		ECSENGINE_API Stream<void> GetReflectionFieldStreamVoidEx(const ReflectionFieldInfo& info, const void* data, bool offset_into_data = true);

		// Can optionally specify whether or not to offset into the data
		// This version takes into account embedded arrays. The size of the void stream is the element count, not
		// the byte size of the elements
		ECSENGINE_API ResizableStream<void> GetReflectionFieldResizableStreamVoidEx(const ReflectionFieldInfo& info, const void* data, bool offset_into_data = true);

		ECSENGINE_API size_t GetReflectionFieldStreamElementByteSize(const ReflectionFieldInfo& info);

		ECSENGINE_API unsigned char GetReflectionFieldPointerIndirection(const ReflectionFieldInfo& info);

		// Returns the target of the given pointer field. For multi-indirection pointers, it will return
		// The next lower leveled of the indirection
		ECSENGINE_API Stream<char> GetReflectionFieldPointerTarget(const ReflectionField& field);

		ECSENGINE_API ReflectionBasicFieldType ConvertBasicTypeMultiComponentToSingle(ReflectionBasicFieldType type);

		// It will offset the data in order to reach the stream field
		ECSENGINE_API void SetReflectionFieldResizableStreamVoid(const ReflectionFieldInfo& info, void* data, ResizableStream<void> stream_data, bool offset_into_data = true);

		// It will offset the data in order to reach the stream field
		// This version takes into account embedded arrays. The size of the void stream is the element count,
		// not the byte size of the elements
		ECSENGINE_API void SetReflectionFieldResizableStreamVoidEx(const ReflectionFieldInfo& info, void* data, ResizableStream<void> stream_data, bool offset_into_data = true);

		// Returns true if both types are the same in their respective reflection managers
		// (It does not compare tags)
		ECSENGINE_API bool CompareReflectionTypes(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionType* first, 
			const ReflectionType* second
		);

		// Returns true if the tags in both types are the same. Can choose which tags to compare
		// If they have different number of fields and testing for field tags it will return false
		ECSENGINE_API bool CompareReflectionTypesTags(
			const ReflectionType* first,
			const ReflectionType* second,
			bool type_tags,
			bool field_tags
		);

		// Returns true if the instances of the reflection field match. Does not work for user defined types that are
		// basic or basic arrays. Can optionally specify whether or not should use the pointer offset in the info
		ECSENGINE_API bool CompareReflectionFieldInfoInstances(
			const ReflectionFieldInfo* info,
			const void* first,
			const void* second,
			bool offset_into_data = true
		);

		struct CompareReflectionTypeInstanceBlittableType {
			Stream<char> field_definition;
			ReflectionStreamFieldType stream_type;
		};

		struct CompareReflectionTypeInstancesOptions {
			// Optional list of field definitions to be considered blittable
			Stream<CompareReflectionTypeInstanceBlittableType> blittable_types = {};
		};

		// Returns true if the instances of the reflection field match. Works for all cases (including custom types
		// or user defined types, streams, pointers and basic arrays). Can optionally specify whether or not should use the pointer offset in the info
		ECSENGINE_API bool CompareReflectionFieldInstances(
			const ReflectionManager* reflection_manager,
			const ReflectionField* field,
			const void* first,
			const void* second,
			bool offset_into_data = true,
			const CompareReflectionTypeInstancesOptions* options = nullptr
		);

		// Compares two instances of the same type to see if they contain the same data.
		// Returns true in case they contain the same data
		ECSENGINE_API bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			const void* first,
			const void* second,
			const CompareReflectionTypeInstancesOptions* options = nullptr
		);

		// Compares two instances of a certain type to see if they contain the same data.
		// Returns true in case they contain the same data
		ECSENGINE_API bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const void* first,
			const void* second,
			size_t count,
			const CompareReflectionTypeInstancesOptions* options = {}
		);

		struct CopyReflectionDataOptions {
			AllocatorPolymorphic allocator = { nullptr };
			bool always_allocate_for_buffers = false;
			bool set_padding_bytes_to_zero = false;
			bool deallocate_existing_buffers = false;
			// For fields copies, if this is set to true, it will offset
			// Using the field pointer offset
			bool offset_pointer_data_from_field = false;
		};

		// Copies the data from the old_type into the new type and checks for remappings
		// The allocator is needed only when a stream type has changed its underlying type
		// It will allocate memory in that case for the new stream. If the allocator is not specified,
		// then it will not allocate and instead be with size 0 and nullptr (that means, the previous data will not be copied, not referenced)
		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The old and the new reflection manager can be made nullptr if you are sure there are no nested types
		// Or custom types. By default, it will copy matching padding bytes between versions, but you can specify
		// The last boolean in order to ignore that and set the padding bytes to zero for the new type
		ECSENGINE_API void CopyReflectionTypeToNewVersion(
			const ReflectionManager* old_reflection_manager,
			const ReflectionManager* new_reflection_manager,
			const ReflectionType* source_type,
			const ReflectionType* destination_type,
			const void* source_data,
			void* destination_data,
			const CopyReflectionDataOptions* options
		);

		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The reflection manager can be made nullptr if you are sure there are no nested types or custom types. 
		// By default, it will copy matching padding bytes between versions, but you can specify
		// The last boolean in order to ignore that and set the padding bytes to zero for the new type
		ECSENGINE_API void CopyReflectionTypeInstance(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options
		);

		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The reflection manager can be made nullptr if you are sure there are no nested types or custom types. 
		// By default, it will copy matching padding bytes between versions, but you can specify
		// The last boolean in order to ignore that and set the padding bytes to zero for the new type
		ECSENGINE_API void CopyReflectionTypeInstance(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options
		);

		// It does a memcpy into the corresponding field of the instance
		// It does not allocate anything. It returns true if the field exists,
		// else false (in which case it doesn't do anything)
		ECSENGINE_API bool CopyIntoReflectionFieldValue(
			const ReflectionType* type,
			Stream<char> field_name,
			void* instance,
			const void* data
		);

		// Does not work for user defined types
		ECSENGINE_API void ConvertReflectionBasicField(
			ReflectionBasicFieldType first_type,
			ReflectionBasicFieldType second_type,
			const void* first_data,
			void* second_data,
			size_t count = 1
		);

		// It works for used defined types as well
		// The allocator is needed only when a stream type has changed its underlying type
		// It will allocate memory in that case for the new stream. If the allocator is not specified,
		// then it will not allocate and instead it will let the stream nullptr and 0 (that means, the previous data will not be copied, not referenced)
		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The old and the new reflection manager can be made nullptr if you are sure there are no nested types
		// Or custom types
		ECSENGINE_API void ConvertReflectionFieldToOtherField(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionField* first_field,
			const ReflectionField* second_field,
			const void* first_data,
			void* second_data,
			const CopyReflectionDataOptions* options
		);

		// It works for used defined types as well
		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The reflection manager can be made nullptr if you are sure there are no nested types or custom types
		ECSENGINE_API void CopyReflectionFieldInstance(
			const ReflectionManager* reflection_manager,
			const ReflectionField* field,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options
		);

		// Returns true if the type references in any of its fields the subtype
		ECSENGINE_API bool DependsUpon(const ReflectionManager* reflection_manager, const ReflectionType* type, Stream<char> subtype);

		// Returns the byte size and the alignment for the field
		ECSENGINE_API ulong2 GetReflectionTypeGivenFieldTag(const ReflectionField* field);

		// Returns -1 if the field is the field does not have the byte size specified else the specified byte size
		ECSENGINE_API ulong2 GetReflectionFieldSkipMacroByteSize(const ReflectionField* field);

		// Fills in the user defined types that this field has (only custom types can have dependencies with any stream type 
		// or if a field is a user defined type)
		ECSENGINE_API void GetReflectionFieldDependentTypes(const ReflectionField* field, CapacityStream<Stream<char>>& dependencies);

		ECSENGINE_API void GetReflectionFieldDependentTypes(Stream<char> definition, CapacityStream<Stream<char>>& dependencies);

		ECSENGINE_API size_t GetReflectionDataPointerElementByteSize(const ReflectionManager* manager, Stream<char> tag);

		ECSENGINE_API void GetReflectionTypeDependentTypes(const ReflectionManager* manager, const ReflectionType* type, CapacityStream<Stream<char>>& dependent_types);

		// Returns true if the field was tagged with ECS_REFLECTION_SKIP
		ECSENGINE_API bool IsReflectionFieldSkipped(const ReflectionField* field);

		// One of the surplus fields needs to be specified. It will write the indices of the fields
		// that the respective type has and the other does not
		ECSENGINE_API void GetReflectionTypesDifferentFields(
			const ReflectionType* first,
			const ReflectionType* second,
			CapacityStream<size_t>* first_surplus_fields = nullptr,
			CapacityStream<size_t>* second_surplus_fields = nullptr
		);

		struct ReflectionTypeFieldDeep {
			ECS_INLINE bool IsValid() const {
				return type != nullptr && field_index != -1;
			}

			ECS_INLINE void* GetFieldData(const void* data) const {
				return type->GetField(OffsetPointer(data, type_offset_from_original), field_index);
			}

			const ReflectionType* type;
			unsigned int field_index;
			// This is the offset needed to index this value
			// From the original given type
			unsigned short type_offset_from_original;
		};

		// The addressing is like for normal structs, with dots in between
		// Nested type fields. If it doesn't find the field, it will return nullptr
		// type and -1 field index
		ECSENGINE_API ReflectionTypeFieldDeep FindReflectionTypeFieldDeep(
			const ReflectionManager* reflection_manager,
			const ReflectionType* reflection_type,
			Stream<char> field
		);

		// Determines the dependency graph for these reflection types. The first types are those ones
		// that have no dependencies, the second group depends only on the first and so on.
		// Returns true if the dependencies are valid, else false. It will fill in the two buffers given,
		// one with the names of the types and the other one with the subgroups
		ECSENGINE_API bool ConstructReflectionTypeDependencyGraph(
			Stream<ReflectionType> types, 
			CapacityStream<Stream<char>>& ordered_types, 
			CapacityStream<uint2>& subgroups
		);
		
		enum ECS_REFLECTION_TYPE_CHANGE_TYPE : unsigned char {
			ECS_REFLECTION_TYPE_CHANGE_ADD,
			ECS_REFLECTION_TYPE_CHANGE_REMOVE,
			ECS_REFLECTION_TYPE_CHANGE_UPDATE
		};

		// The addition index is the new type field index
		// The removal index is the previous type field index
		// The update index is the new type field index
		// In case there is a nested type, there will be more than 1 index
		struct ReflectionTypeChange {
			ECS_REFLECTION_TYPE_CHANGE_TYPE change_type;
			unsigned char indices_count = 0;
			unsigned int indices[ECS_REFLECTION_TYPE_CHANGE_SET_MAX_INDICES];
		};

		// Determines which fields have been added/removed or updated (updated in case they changed their type)
		ECSENGINE_API void DetermineReflectionTypeChangeSet(
			const ReflectionManager* previous_reflection_manager,
			const ReflectionManager* new_reflection_manager,
			const ReflectionType* previous_type,
			const ReflectionType* new_type,
			AdditionStream<ReflectionTypeChange> changes
		);

		// The change type for all of them is going to be update
		// For stream types, it will report an update if any entry
		// is different.
		ECSENGINE_API void DetermineReflectionTypeInstanceUpdates(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			const void* first_data,
			const void* second_data,
			AdditionStream<ReflectionTypeChange> updates
		);

		// Makes the data for the given destination fields the same as the source
		// Ones specified from the change set. The allocator is used for the stream
		// Allocations
		ECSENGINE_API void UpdateReflectionTypeInstanceFromChanges(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			void* destination_data,
			const void* source_data,
			Stream<ReflectionTypeChange> changes,
			AllocatorPolymorphic allocator
		);

		// Makes the data for the given destination fields the same as the source
		// Ones specified from the change set. The allocator is used for the stream
		// Allocations
		ECSENGINE_API void UpdateReflectionTypeInstancesFromChanges(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			Stream<void*> destinations,
			const void* source_data,
			Stream<ReflectionTypeChange> changes,
			AllocatorPolymorphic allocator
		);

	}

}