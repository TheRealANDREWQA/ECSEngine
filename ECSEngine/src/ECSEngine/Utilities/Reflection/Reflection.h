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
		typedef HashTableDefault<ReflectionTypedef> ReflectionTypedefTable;
		typedef HashTableDefault<ReflectionTypeTemplate> ReflectionTypeTemplateTable;
		typedef HashTableDefault<ReflectionValidDependency> ReflectionValidDependenciesTable;

#define ECS_REFLECTION_MAX_TYPE_COUNT (128)
#define ECS_REFLECTION_MAX_ENUM_COUNT (32)
#define ECS_REFLECTION_TYPE_CHANGE_SET_MAX_INDICES (8)
#define ECS_REFLECTION_TYPE_FIELD_DEEP_MAX_INDEX_COUNT (7)

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
			
			struct AddFromOptions {
				bool types = true;
				bool enums = true;
				bool typedefs = true;
				bool templates = true;
				bool constants = true;
				bool valid_dependencies = true;
				// If this is false, it will add only those that match the folder hierarchy. -1 can be a valid entry
				// In that case, for types that are not bound to any hierarchy from the other instance.
				bool all_hierarchies = false;
				unsigned int folder_hierarchy = -1;
			};

			ReflectionManager() {}
			ReflectionManager(AllocatorPolymorphic allocator, size_t type_count = ECS_REFLECTION_MAX_TYPE_COUNT, size_t enum_count = ECS_REFLECTION_MAX_ENUM_COUNT);

			ReflectionManager(const ReflectionManager& other) = default;
			ReflectionManager& operator = (const ReflectionManager& other) = default;

			static void GetKnownBlittableExceptions(CapacityStream<BlittableType>* blittable_types, AllocatorPolymorphic temp_allocator);

			void AddKnownBlittableExceptions();

			void AddBlittableException(Stream<char> definition, size_t byte_size, size_t alignment, const void* default_data = nullptr);

			// Copies the constants reflected from another manager into this one
			void AddConstantsFrom(const ReflectionManager* other);

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

			// Adds all parsed typedefs from the other instance to this instance without binding them to a 
			// Particular folder hierarchy
			void AddTypedefsFrom(const ReflectionManager* other);

			// Adds all parsed type templates from the other instance to this instance without binding them to
			// A particular folder hierarchy
			void AddTypeTemplatesFrom(const ReflectionManager* other);

			// Adds all parsed valid dependencies from the other instance to this instance without binding them to
			// A particular folder hierarchy
			void AddValidDependenciesFrom(const ReflectionManager* other);

			// Copies all parsed resources from an other reflection manager to this instance without binding the resources
			// To a particular folder hierarchy
			void AddAllFrom(const ReflectionManager* other);

			// Adds selective elements from the other reflection manager into this instance. Each entry will be added as an individual
			// Entry, be aware that calling the normal FreeFolderHierarchy() for entries that are added with options will not be picked up.
			// You need to use the function FreeEntries() to deallocate the entries.
			void AddFrom(const ReflectionManager* other, const AddFromOptions& options);

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

			// Given an allocator, it will create a hash table large enough to hold those entries
			// And copy all the types from the other
			void CreateStaticFrom(const ReflectionManager* other, AllocatorPolymorphic allocator);

			void CopyEnums(ReflectionManager* other) const;

			void CopyTypes(ReflectionManager* other) const;

			// Deallocates this static reflection manager. It uses a fast path
			// Compared to ClearTypesFromAllocator()
			void DeallocateStatic(AllocatorPolymorphic allocator);

			void DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data);

			// Returns DBL_MAX if the evaluation fails. The expression must have constant terms,
			// ECS_CONSTANT_REFLECT macros or sizeof()/alignof() types that were reflected
			double EvaluateExpression(Stream<char> expression);

			// Returns the byte size and the alignment
			ulong2 FindBlittableException(Stream<char> name) const;

			void FreeFolderHierarchy(unsigned int folder_index);

			// Frees the entries that were added with the call AddFrom(). The same options that were used to add the entries should be used here as well
			void FreeEntries(const AddFromOptions& options);

			// It asserts that it exists
			ReflectionType* GetType(Stream<char> name);
			ReflectionType* GetType(unsigned int index);

			// It asserts that it exists
			const ReflectionType* GetType(Stream<char> name) const;
			const ReflectionType* GetType(unsigned int index) const;

			// It asserts that it exists
			ReflectionEnum* GetEnum(Stream<char> name);
			ReflectionEnum* GetEnum(unsigned int index);

			// It asserts that it exists
			const ReflectionEnum* GetEnum(Stream<char> name) const;
			const ReflectionEnum* GetEnum(unsigned int index) const;

			// Returns nullptr if no template matches the given definition
			const ReflectionTypeTemplate* GetTypeTemplate(Stream<char> definition) const;

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

			const ReflectionType* TryGetType(Stream<char> name) const;
			const ReflectionEnum* TryGetEnum(Stream<char> name) const;

			void InitializeFieldTable();
			void InitializeTypeTable(size_t count);
			void InitializeEnumTable(size_t count);

			// It will not set the paths that need to be searched; thread memory will be allocated from the heap, must be freed manually
			void InitializeParseThreadTaskData(size_t thread_memory, size_t path_count, ReflectionManagerParseStructuresThreadTaskData& data, CapacityStream<char>* error_message = nullptr);

			ECS_INLINE bool IsKnownValidDependency(Stream<char> definition) const {
				return valid_dependencies.Find(definition) != -1;
			}

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
			ReflectionTypedefTable typedefs;
			ReflectionTypeTemplateTable type_templates;
			// Structure names or typedefs or any other string that might appear
			// Inside other structures and that it is considered valid without having
			// It properly reflected. I.e. template functors that provide an indexer
			ReflectionValidDependenciesTable valid_dependencies;
			ResizableStream<FolderHierarchy> folders;
			ResizableStream<ReflectionConstant> constants;
			ResizableStream<BlittableType> blittable_types;
		};

		// This structure contains information that upper level types
		// Or fields can send to fields or nested types to change their behavior
		struct ECSENGINE_API ReflectionPassdownInfo {
			struct PointerReferenceTarget {
				Stream<char> key;
				// When a simply pointer is the target, this needs to be handled differently
				bool is_pointer;
				// The definition will be looked at, only if the is_pointer field
				// Is set to false
				Stream<char> definition;
				// If this is a custom type, at first, it will be -1, and the first
				// Function to need this entry will set it to the actual custom index
				// Such that next calls don't have to perform the finding again
				unsigned int custom_type_index;
				// There are 2 pointers, in order to make the copying situation easier
				// When no source and destination are needed at the same time, they can
				// Be made the same
				const void* source_data;
				const void* destination_data;
				// For compound custom types, this differentiates between the element types
				Stream<char> element_type_name;

				// Maintain these caches automatically, such that we can speed up subsequent queries
				// Without having to burden the user of having to pass in these structures
				ReflectionCustomTypeFindElementData cached_find_data;
				ReflectionCustomTypeGetElementData cached_get_data;
			};

			// Adding this constructor in order to make the user not forget to initialize the fields
			// Inside it
			ECS_INLINE ReflectionPassdownInfo(AllocatorPolymorphic allocator) {
				pointer_reference_targets.Initialize(allocator, 0);
			}

			// The definition can be left as empty if the info type is a pointer
			// If you have just a single data pointer, use it as both binding spots,
			// As source and as destination at the same time
			void AddPointerReference(
				Stream<char> key, 
				bool is_pointer, 
				Stream<char> definition, 
				Stream<char> element_type_name,
				const void* source_data,
				const void* destination_data
			);

			// Adds all the pointer references for the given field. It correctly handles the custom types
			// And pointer types
			void AddPointerReferencesFromField(
				Stream<char> definition, 
				Stream<char> tag, 
				ReflectionBasicFieldType basic_type, 
				ReflectionStreamFieldType stream_type, 
				const void* source_data, 
				const void* destination_data
			);

			// Adds all the pointer references for the given field. It correctly handles the custom types
			// And pointer types
			void AddPointerReferencesFromField(const ReflectionField* field, const void* source_data, const void* destination_data);

			// Returns nullptr if it doesn't find the target
			ECS_INLINE PointerReferenceTarget* FindPointerTarget(Stream<char> key, Stream<char> custom_element_name) {
				for (size_t index = 0; index < pointer_reference_targets.size; index++) {
					if (pointer_reference_targets[index].key == key && pointer_reference_targets[index].element_type_name == custom_element_name) {
						return pointer_reference_targets.buffer + index;
					}
				}
				return nullptr;
			}

			// This variant uses the slower index value lookup, which corresponds to an iteration
			// Order, but if you only need an identifier to restore this, use the token variant.
			// Returns -1 if it couldn't be found (either the key or the pointer value).
			// With the boolean is_source_data you can control whether the source or the destination data
			// Is being used
			ReflectionCustomTypeGetElementIndexOrToken GetPointerTargetIndex(Stream<char> key, Stream<char> custom_element_name, const void* pointer_value, bool is_source_data);

			// This variant uses the faster token value lookup. It is a unique value that
			// Can be used later on to identify the entry
			// Returns -1 if it couldn't be found (either the key or the pointer value)
			ReflectionCustomTypeGetElementIndexOrToken GetPointerTargetToken(Stream<char> key, Stream<char> custom_element_name, const void* pointer_value, bool is_source_data);

			// From a previous index value for a certain state, it returns the pointer
			// Value that corresponds to that token for that key. The behavior is undefined
			// If the index value is not valid - it may be nullptr or a garbage value
			void* RetrievePointerTargetValueFromIndex(Stream<char> key, Stream<char> custom_element_name, ReflectionCustomTypeGetElementIndexOrToken index_value, bool is_source_data);

			// From a previous token value for a certain state, it returns the pointer
			// Value that corresponds for the token for that key. The behavior is undefined
			// If the token value is not valid - it may be nullptr or a garbage value
			void* RetrievePointerTargetValueFromToken(Stream<char> key, Stream<char> custom_element_name, ReflectionCustomTypeGetElementIndexOrToken token_value, bool is_source_data);

			ResizableStream<PointerReferenceTarget> pointer_reference_targets;
		};

		// Create a resizable stack allocator and initializes a structure of type
		// Passdown info with the given name with that allocator
#define ECS_REFLECTION_STACK_PASSDOWN_INFO(name) \
		/* The allocator size can be small, since we are not expecting many entries in it */ \
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_allocator##name, ECS_KB * 4, ECS_MB * 32); \
		ReflectionPassdownInfo name(&_allocator##name);

		// If there are no user defined types, this version will work
		ECSENGINE_API void SetInstanceFieldDefaultData(const ReflectionField* field, void* data, bool offset_data = true);

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

		// The soa_index must be the index from the misc info inside the type
		ECSENGINE_API Stream<void> GetReflectionTypeSoaStream(const ReflectionType* type, const void* data, size_t soa_index);

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
			const ReflectionType*& type,
			unsigned int& custom_type_index,
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

		// This call combines a separate call to SearchReflectionUserDefinedTypeByteSizeAlignment and SearchIsBlittable
		// Into a single search, making it faster than calling separately those 2 functions. If an error is encountered,
		// The byte size and the alignment returned are -1.
		ECSENGINE_API ReflectionDefinitionInfo SearchReflectionDefinitionInfo(
			const ReflectionManager* reflection_manager,
			Stream<char> definition
		);

		// For all fields which are streams it will determine the element byte size and fill it in inside the field
		ECSENGINE_API void SetReflectionTypeFieldsStreamByteSizeAlignment(
			const ReflectionManager* reflection_manager,
			ReflectionType* reflection_type
		);

		// Copies all blittable fields from one type instance to the other
		ECSENGINE_API void CopyReflectionTypeBlittableFields(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			const void* source,
			void* destination
		);

		// Works for user defined types as well
		ECSENGINE_API size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, const ReflectionField& field);

		// Copies non user defined fields
		ECSENGINE_API void CopyReflectionFieldBasic(const ReflectionFieldInfo* info, const void* source, void* destination, AllocatorPolymorphic allocator);

		// Copies non user defined fields, and takes into consideration for certain types
		// The tags and the passdown information
		ECSENGINE_API void CopyReflectionFieldBasicWithTag(
			const ReflectionFieldInfo* info,
			const void* source,
			void* destination,
			AllocatorPolymorphic allocator,
			Stream<char> tag,
			ReflectionPassdownInfo* passdown_info
		);

		// Deallocates non user defined fields. By default, it will reset buffers, but you can disable this option
		ECSENGINE_API void DeallocateReflectionFieldBasic(const ReflectionFieldInfo* info, void* destination, AllocatorPolymorphic allocator, bool reset_buffers = true);

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

		// Returns the target of the given pointer field
		ECSENGINE_API Stream<char> GetReflectionFieldPointerTarget(const ReflectionField& field);
		
		// Returns the size of the SoA pointer. Can't return a pointer directly to the size since it
		// Can have different byte sizes
		ECSENGINE_API size_t GetReflectionFieldPointerSoASize(const ReflectionFieldInfo& info, const void* data);

		// Returns the size of the SoA pointer. Can't return a pointer directly to the size since it
		// Can have different byte sizes
		ECSENGINE_API size_t GetReflectionPointerSoASize(const ReflectionType* type, size_t soa_index, const void* data);

		// Returns the element byte size for each entry in the SoA stream
		ECSENGINE_API size_t GetReflectionPointerSoAPerElementSize(const ReflectionType* type, size_t soa_index);

		// If the SoA pointers have separate allocations, this function will merge them into a single coalesced allocation
		ECSENGINE_API void MergeReflectionPointerSoAAllocations(const ReflectionType* type, size_t soa_index, void* data, AllocatorPolymorphic allocator);

		// It will perform the merge for all SoA streams
		ECSENGINE_API void MergeReflectionPointerSoAAllocationsForType(const ReflectionType* type, void* data, AllocatorPolymorphic allocator);

		// Returns true if the field index is the first pointer in a parallel SoA stream
		ECSENGINE_API bool IsReflectionPointerSoAAllocationHolder(const ReflectionType* type, unsigned int field_index);

		// Deallocate the SoA pointer of the given field only if it is the first one from the SoA stream. Returns true
		// in that case, else false
		ECSENGINE_API bool DeallocateReflectionPointerSoAAllocation(const ReflectionType* type, unsigned int field_index, void* data, AllocatorPolymorphic allocator);

		// Writes the size into the size field of the given SoA pointer. This function takes care of the
		// Byte size of the size field. The info must be for a PointerSoA!
		ECSENGINE_API void SetReflectionFieldPointerSoASize(const ReflectionFieldInfo& info, void* data, size_t value);

		// Writes the size into the size field of the given SoA pointer. This function takes care of the
		// Byte size of the size field
		ECSENGINE_API void SetReflectionPointerSoASize(const ReflectionType* type, size_t soa_index, void* data, size_t value);

		ECSENGINE_API void SetReflectionPointerSoASize(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, void* data, size_t value);

		// Returns -1 if there is no capacity field
		ECSENGINE_API size_t GetReflectionFieldSoaCapacityValue(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, const void* data);

		// It does nothing if there is no capacity field
		ECSENGINE_API void SetReflectionFieldSoaCapacityValue(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, void* data, size_t value);

		// It will assign as many fields as possible
		// It will offset into the data pointer according to the stored offsets.
		// This is the function to use when you want to change a pointer since
		// It is mandatory to know the size of the new allocation
		ECSENGINE_API void SetReflectionFieldSoaStream(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, void* data, ResizableStream<void> stream_data);

		// Returns the offset from the current field info to the size field
		ECS_INLINE short GetReflectionFieldPointerSoASizeRelativeOffset(const ReflectionFieldInfo& info) {
			if (info.soa_size_pointer_offset > info.pointer_offset) {
				return info.soa_size_pointer_offset - info.pointer_offset;
			}
			else {
				return info.pointer_offset - info.soa_size_pointer_offset;
			}
		}

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

		// Returns true if the instances of the reflection field match. Works for all cases (including custom types
		// or user defined types, streams, pointers and basic arrays). Can optionally specify whether or not should use the pointer offset in the info
		ECSENGINE_API bool CompareReflectionFieldInstances(
			const ReflectionManager* reflection_manager,
			const ReflectionField* field,
			const void* first,
			const void* second,
			bool offset_into_data = true,
			const ReflectionCustomTypeCompareOptions* options = nullptr
		);

		// Compares two instances of the same type to see if they contain the same data.
		// Returns true in case they contain the same data
		ECSENGINE_API bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			const void* first,
			const void* second,
			const ReflectionCustomTypeCompareOptions* options = nullptr
		);

		// Compares two instances of a certain type to see if they contain the same data.
		// Returns true in case they contain the same data
		ECSENGINE_API bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const void* first,
			const void* second,
			size_t count,
			const ReflectionCustomTypeCompareOptions* options = nullptr
		);

		// Compares two instances of a certain type to see if they contain the same data.
		// Returns true in case they contain the same data
		ECSENGINE_API bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const ReflectionDefinitionInfo& definition_info,
			const void* first,
			const void* second,
			const ReflectionCustomTypeCompareOptions* options = nullptr
		);

		// Compares two instances of a certain type to see if they contain the same data.
		// Returns true in case they contain the same data
		ECSENGINE_API bool CompareReflectionTypeInstances(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const ReflectionDefinitionInfo& definition_info,
			const void* first,
			const void* second,
			size_t count,
			const ReflectionCustomTypeCompareOptions* options = nullptr
		);

		struct CopyReflectionDataOptions {
			ECS_INLINE CopyReflectionDataOptions() {}
			// Copies the relevant information from the given custom data
			ECS_INLINE CopyReflectionDataOptions(const ReflectionCustomTypeCopyData* custom_data) {
				allocator = custom_data->allocator;
				always_allocate_for_buffers = true;
				custom_options = custom_data->options;
				passdown_info = custom_data->passdown_info;
			}

			AllocatorPolymorphic allocator = { nullptr };
			bool always_allocate_for_buffers = false;
			bool set_padding_bytes_to_zero = false;
			// For field copies, if this is set to true, it will offset
			// Using the field pointer offset
			bool offset_pointer_data_from_field = false;
			ReflectionCustomTypeCopyOptions custom_options = {};
			// This pointer should be created by the top level function and keep
			// Being used for the rest of the calls, such that the data is properly transmitted
			ReflectionPassdownInfo* passdown_info = nullptr;
		};

		// Copies the data from the old_type into the new type and checks for remappings
		// The allocator is needed only when a stream type has changed its underlying type
		// It will allocate memory in that case for the new stream. If the allocator is not specified,
		// then it will not allocate and instead be with size 0 and nullptr (that means, the previous data will not be copied, not referenced)
		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The old and the new reflection manager can be made nullptr if you are sure there are no nested types
		// Or custom types. By default, it will copy matching padding bytes between versions, but you can specify
		// The last boolean in order to ignore that and set the padding bytes to zero for the new type.
		// The following custom options are not accepted: initialize_type_allocators, use_field_allocators and overwrite_resizable_allocators
		// And passdown info.
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
		// By default, it will copy matching padding bytes between instances, but you can specify
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
		// By default, it will copy matching padding bytes between instances, but you can specify
		// The last boolean in order to ignore that and set the padding bytes to zero for the new type
		ECSENGINE_API void CopyReflectionTypeInstance(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options,
			Stream<char> tags = {}
		);

		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The reflection manager can be made nullptr if you are sure there are no nested types or custom types. 
		// By default, it will copy matching padding bytes between instances, but you can specify
		// The last boolean in order to ignore that and set the padding bytes to zero for the new type
		// You can optionally specify tags for this definition to be used
		ECSENGINE_API void CopyReflectionTypeInstance(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const ReflectionDefinitionInfo& definition_info,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options,
			Stream<char> tags = {}
		);

		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		// The reflection manager can be made nullptr if you are sure there are no nested types or custom types. 
		// By default, it will copy matching padding bytes between instances, but you can specify
		// The last boolean in order to ignore that and set the padding bytes to zero for the new type
		// You can optionally specify tags for this definition to be used
		// Element_count describes how many consecutive elements to copy
		ECSENGINE_API void CopyReflectionTypeInstance(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const ReflectionDefinitionInfo& definition_info,
			const void* source,
			void* destination,
			size_t element_count,
			const CopyReflectionDataOptions* options,
			Stream<char> tags = {}
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
		// The following custom options are not accepted: initialize_type_allocators, use_field_allocators and overwrite_resizable_allocators
		// And passdown info
		ECSENGINE_API void ConvertReflectionFieldToOtherField(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionType* first_type,
			const ReflectionType* second_type,
			unsigned int first_type_field_index,
			unsigned int second_type_field_index,
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
			const ReflectionType* type,
			unsigned int field_index,
			const void* source,
			void* destination,
			const CopyReflectionDataOptions* options
		);

		// If the last boolean parameter is set to true, it will
		// Reset the buffers (as it is by default). By default, it will deallocate
		// A single element. But you can specify a contiguous array as well
		// In order to increase the performance for buffers, in which case you must specify the element byte size
		ECSENGINE_API void DeallocateReflectionInstanceBuffers(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			void* source,
			AllocatorPolymorphic allocator,
			size_t element_count = 1,
			bool reset_buffers = true
		);

		// If the last boolean parameter is set to true, it will
		// Reset the buffers (as it is by default). By default, it will deallocate
		// A single element. But you can specify a contiguous array as well
		// In order to increase the performance for buffers. If the element stride
		// Is left as 0, it will use the type's byte size as stride, else it will
		// Use the parameter value. Useful if you want to deallocate a subfield of
		// A larger type
		ECSENGINE_API void DeallocateReflectionTypeInstanceBuffers(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			void* source,
			AllocatorPolymorphic allocator,
			size_t element_count = 1,
			size_t element_stride = 0,
			bool reset_buffers = true
		);

		// This function deallocates only a single entry. It functions similarly
		// To the other 2 overloads, the difference is that it extracts its information
		// From the definition info. If the last boolean parameter is set to true, it will
		// Reset the buffers (as it is by default)
		ECSENGINE_API void DeallocateReflectionInstanceBuffers(
			const ReflectionManager* reflection_manager,
			Stream<char> definition,
			const ReflectionDefinitionInfo& definition_info,
			void* source,
			AllocatorPolymorphic allocator,
			size_t count = 1,
			bool reset_buffers = true
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

		// It asserts that all user defined types have a match, i.e. they are matched by a type or custom type interface or it is assigned as blittable
		ECSENGINE_API void GetReflectionTypeDependentTypes(const ReflectionManager* manager, const ReflectionType* type, CapacityStream<Stream<char>>& dependent_types);

		// Fills in the missing dependencies of a given type, including nested dependencies, if there are any
		ECSENGINE_API void GetReflectionTypeMissingDependencies(const ReflectionManager* manager, const ReflectionType* type, CapacityStream<Stream<char>>& missing_dependencies);

		// Returns true if a user defined field of the given type cannot be matched by a normal reflection type, by a custom type interface
		// Or by a known valid dependency and that it is not declared as blittable. You can optionally retrieve the first missing dependency as an out parameter
		ECS_INLINE bool HasReflectionTypeMissingDependencies(const ReflectionManager* manager, const ReflectionType* type, Stream<char>* missing_dependency = nullptr) {
			ECS_STACK_CAPACITY_STREAM(Stream<char>, missing_dependencies, 512);
			GetReflectionTypeMissingDependencies(manager, type, missing_dependencies);
			if (missing_dependencies.size > 0 && missing_dependency != nullptr) {
				*missing_dependency = missing_dependencies[0];
			}
			return missing_dependencies.size > 0;
 		}

		// Returns true if the given definition is a user defined type that can be matched, else false
		ECSENGINE_API bool IsReflectionDefinitionAValidDefinition(const ReflectionManager* manager, Stream<char> definition);

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

			// Returns the offset from the base of the initial type
			// To reach the current nested type
			ECS_INLINE unsigned short NestedTypeOffset() const {
				return type_offset_from_original - type->fields[field_index].info.pointer_offset;
			}

			const ReflectionType* type;
			unsigned int field_index;
			// This is the offset needed to index this value
			// From the original given type
			unsigned short type_offset_from_original;
		};

		struct SetReflectionTypeInstanceBufferOptions {
			AllocatorPolymorphic allocator = { nullptr };
			// If this value is set, it will perform a copy operation
			// Only if the data has changed
			bool checked_copy = false;
			// If this flag is set, it will deallocate the existing data
			bool deallocate_existing = true;
		};

		// The pointer of the type references the type from the reflection manager
		// Make sure no changes are made to the reflection manager's types while you
		// Are using this deep field!
		ECSENGINE_API ReflectionTypeFieldDeep ConvertReflectionNestedFieldIndexToDeep(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type,
			ReflectionNestedFieldIndex field_index
		);

		// This will correctly work for SoA pointers as well. The capacity of compare_data 
		// is needed only for SoA pointers
		ECSENGINE_API bool CompareReflectionTypeBufferData(
			ReflectionTypeFieldDeep deep_field,
			const void* target,
			CapacityStream<void> compare_data
		);

		// It will set a buffer in the given instance of a type.
		// It accepts nested buffers as well. The capacity is used
		// For those cases where it is appropriate (when there is
		// A capacity field present)
		ECSENGINE_API void SetReflectionTypeInstanceBuffer(
			ReflectionTypeFieldDeep deep_field,
			void* target,
			CapacityStream<void> buffer_data,
			const SetReflectionTypeInstanceBufferOptions* options
		);

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
		// Allocations. Only UPDATE changes need to be in the given stream!
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
		// Allocations. Only UPDATE changes need to be in the given stream!
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