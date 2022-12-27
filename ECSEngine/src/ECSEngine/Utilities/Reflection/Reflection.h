#pragma once
#include "ecspch.h"
#include "../Function.h"
#include "../FunctionInterfaces.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Internal/Multithreading/TaskManager.h"
#include "../../Internal/InternalStructures.h"
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

#pragma region Reflection Container Type functions

		enum ECS_REFLECTION_CUSTOM_TYPE_INDEX : unsigned char {
			ECS_REFLECTION_CUSTOM_TYPE_STREAM,
			ECS_REFLECTION_CUSTOM_TYPE_REFERENCE_COUNTED,
			ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET,
			ECS_REFLECTION_CUSTOM_TYPE_COLOR,
			ECS_REFLECTION_CUSTOM_TYPE_COLOR_FLOAT,
			ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET,
			ECS_REFLECTION_CUSTOM_TYPE_DATA_POINTER,
			ECS_REFLECTION_CUSTOM_TYPE_COUNT
		};

		extern ReflectionCustomType ECS_REFLECTION_CUSTOM_TYPES[];

		ECSENGINE_API void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data);

		// E.g. for Template<Type> string should be Template
		ECSENGINE_API bool ReflectionCustomTypeMatchTemplate(ReflectionCustomTypeMatchData* data, const char* string);

		ECSENGINE_API Stream<char> ReflectionCustomTypeGetTemplateArgument(Stream<char> definition);

		// ---------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(Stream);

		// ---------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(SparseSet);

		// ---------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(Color);

		// ---------------------------------------------------------------------------------------------------------------------
		
		ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(ColorFloat);

		// ---------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CUSTOM_TYPE_FUNCTION_HEADER(DataPointer);

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
			struct FolderHierarchy {
				Stream<wchar_t> root;
				const void* allocated_buffer;
			};

			struct BlittableType {
				Stream<char> name;
				unsigned int byte_size;
				unsigned int alignment;
			};

			ReflectionManager() {}
			ReflectionManager(MemoryManager* allocator, size_t type_count = ECS_REFLECTION_MAX_TYPE_COUNT, size_t enum_count = ECS_REFLECTION_MAX_ENUM_COUNT);

			ReflectionManager(const ReflectionManager& other) = default;
			ReflectionManager& operator = (const ReflectionManager& other) = default;

			void AddBlittableException(Stream<char> definition, unsigned int byte_size, unsigned int alignment);

			// Returns true if it succededs. It can fail due to expression evaluation
			// or if a referenced type is not yet reflected
			bool BindApprovedData(
				ReflectionManagerParseStructuresThreadTaskData* data,
				unsigned int data_count,
				unsigned int folder_index
			);

			void ClearEnumDefinitions();
			void ClearTypeDefinitions();

			// Returns the current index; it may change if removals take place
			unsigned int CreateFolderHierarchy(Stream<wchar_t> root);

			void DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data);

			// Returns DBL_MAX if the evaluation fails. The expression must have constant terms,
			// ECS_CONSTANT_REFLECT macros or sizeof()/alignof() types that were reflected
			double EvaluateExpression(Stream<char> expression);

			// Returns the byte size and the alignment
			uint2 FindBlittableException(Stream<char> name) const;

			void FreeFolderHierarchy(unsigned int folder_index);

			ReflectionType* GetType(Stream<char> name);
			ReflectionType* GetType(unsigned int index);

			const ReflectionType* GetType(Stream<char> name) const;
			const ReflectionType* GetType(unsigned int index) const;

			ReflectionEnum* GetEnum(Stream<char> name);
			ReflectionEnum* GetEnum(unsigned int index);

			const ReflectionEnum* GetEnum(Stream<char> name) const;
			const ReflectionEnum* GetEnum(unsigned int index) const;

			// Returns -1 if it fails
			unsigned int GetHierarchyIndex(Stream<wchar_t> hierarchy) const;
			
			// If the tag is specified it will include only those types that have at least one tag
			// If the hierarchy index is -1, then it will add all types from all hierarchies
			void GetHierarchyTypes(ReflectionManagerGetQuery options, unsigned int hierarchy_index = -1) const;

			unsigned int GetHierarchyCount() const;
			unsigned int GetTypeCount() const;

			// Make a helper for components and shared components
			void GetHierarchyComponentTypes(unsigned int hierarchy_index, CapacityStream<unsigned int>* component_indices, CapacityStream<unsigned int>* shared_indices) const;

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
			void SetInstanceFieldDefaultData(const ReflectionField* field, void* data) const;

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

			// Blittable types that are easier
			ResizableStream<BlittableType> blittable_types;
		};

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

		ECSENGINE_API size_t GetReflectionTypeByteSize(const ReflectionType* type);

		ECSENGINE_API size_t GetReflectionTypeAlignment(const ReflectionType* type);

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

		// Can optionally give field definitions to be considered as trivially copyable
		ECSENGINE_API bool SearchIsBlittable(
			const ReflectionManager* reflection_manager,
			const ReflectionType* type
		);

		// Returns true if it can be copied with memcpy, else false
		// It returns true when all fields are fundamental types non pointer
		// Can optionally give field definitions to be considered as trivially copyable
		ECSENGINE_API bool SearchIsBlittable(
			const ReflectionManager* reflection_manager,
			Stream<char> definition
		);

		// Makes a deep copy of the given reflection type. The blittable streams need to be specified at the same time.
		ECSENGINE_API void CopyReflectionType(
			const Reflection::ReflectionManager* reflection_manager,
			const Reflection::ReflectionType* type,
			const void* source,
			void* destination,
			AllocatorPolymorphic field_allocator
		);

		// Makes a deep copy of the given reflection type. The blittable streams need to be specified at the same time.
		ECSENGINE_API void CopyReflectionType(
			const Reflection::ReflectionManager* reflection_manager,
			Stream<char> definition,
			const void* source,
			void* destination,
			AllocatorPolymorphic field_allocator
		);

		// Works for user defined types aswell
		ECSENGINE_API size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, ReflectionField field);

		// Copies non used defined field
		ECSENGINE_API void CopyReflectionFieldBasic(const ReflectionFieldInfo* info, const void* source, void* destination, AllocatorPolymorphic allocator);

		ECSENGINE_API size_t GetBasicTypeArrayElementSize(const ReflectionFieldInfo& info);

		ECSENGINE_API void* GetReflectionFieldStreamBuffer(const ReflectionFieldInfo& info, const void* data);

		ECSENGINE_API size_t GetReflectionFieldStreamSize(const ReflectionFieldInfo& info, const void* data);

		// It will offset the data in order to reach the stream field
		// The size of the void stream is that of the elements, not that of the byte size of the elements
		// This version does not take into account embedded arrays
		ECSENGINE_API Stream<void> GetReflectionFieldStreamVoid(const ReflectionFieldInfo& info, const void* data);

		// It will offset the data in order to reach the stream field
		ECSENGINE_API ResizableStream<void> GetReflectionFieldResizableStreamVoid(const ReflectionFieldInfo& info, const void* data);

		// It will offset the data in order to reach the stream field
		// This version takes into account embedded arrays. The size of the void stream is the element count, not
		// the byte size of the elements
		ECSENGINE_API Stream<void> GetReflectionFieldStreamVoidEx(const ReflectionFieldInfo& info, const void* data);

		// It will offset the data in order to reach the stream field
		// This version takes into account embedded arrays. The size of the void stream is the element count, not
		// the byte size of the elements
		ECSENGINE_API ResizableStream<void> GetReflectionFieldResizableStreamVoidEx(const ReflectionFieldInfo& info, const void* data);

		ECSENGINE_API size_t GetReflectionFieldStreamElementByteSize(const ReflectionFieldInfo& info);

		ECSENGINE_API unsigned char GetReflectionFieldPointerIndirection(const ReflectionFieldInfo& info);

		ECSENGINE_API ReflectionBasicFieldType ConvertBasicTypeMultiComponentToSingle(ReflectionBasicFieldType type);

		// It will offset the data in order to reach the stream field
		ECSENGINE_API void SetReflectionFieldResizableStreamVoid(const ReflectionFieldInfo& info, void* data, ResizableStream<void> stream_data);

		// It will offset the data in order to reach the stream field
		// This version takes into account embedded arrays. The size of the void stream is the element count,
		// not the byte size of the elements
		ECSENGINE_API void SetReflectionFieldResizableStreamVoidEx(const ReflectionFieldInfo& info, void* data, ResizableStream<void> stream_data);

		// Returns true if both types are the same in their respective reflection managers
		ECSENGINE_API bool CompareReflectionTypes(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionType* first, 
			const ReflectionType* second
		);

		// Copies the data from the old_type into the new type and checks for remappings
		// The allocator is needed only when a stream type has changed its underlying type
		// It will allocate memory in that case for the new stream. If the allocator is not specified,
		// then it will not allocate and instead be with size 0 and nullptr (that means, the previous data will not be copied, not referenced)
		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		ECSENGINE_API void CopyReflectionTypeToNewVersion(
			const ReflectionManager* old_reflection_manager,
			const ReflectionManager* new_reflection_manager,
			const ReflectionType* old_type,
			const ReflectionType* new_type,
			const void* old_data,
			void* new_data,
			AllocatorPolymorphic allocator = { nullptr },
			bool always_allocate_for_buffers = false
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
		// then it will not allocate and instead be with size 0 and nullptr (that means, the previous data will not be copied, not referenced)
		// If an allocator is specified, then the always_allocate_for_buffers flag can be set such that
		// for buffers it will always allocate even when the type is the same
		ECSENGINE_API void ConvertReflectionFieldToOtherField(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionField* first_info,
			const ReflectionField* second_info,
			const void* first_data,
			void* second_data,
			AllocatorPolymorphic allocator = { nullptr },
			bool always_allocate_for_buffers = false
		);

		ECSENGINE_API bool IsReflectionTypeComponent(const ReflectionType* type);

		ECSENGINE_API bool IsReflectionTypeSharedComponent(const ReflectionType* type);

		ECSENGINE_API bool IsReflectionTypeLinkComponent(const ReflectionType* type);

		// Returns { nullptr, 0 } if there is no target specified
		ECSENGINE_API Stream<char> GetReflectionTypeLinkComponentTarget(const ReflectionType* type);

		// If it ends in _Link, it will return the name without that end
		ECSENGINE_API Stream<char> GetReflectionTypeLinkNamePretty(Stream<char> name);

		// Returns true if the link component needs to be built using DLL functions
		ECSENGINE_API bool GetReflectionTypeLinkComponentNeedsDLL(const ReflectionType* type);

		// Returns true if the type references in any of its fields the subtype
		ECSENGINE_API bool DependsUpon(const ReflectionManager* reflection_manager, const ReflectionType* type, Stream<char> subtype);

		// Determines all the buffers that the ECS runtime can use
		ECSENGINE_API void GetReflectionTypeRuntimeBuffers(const ReflectionType* type, CapacityStream<ComponentBuffer>& component_buffers);

		// Returns the byte size and the alignment for the field
		ECSENGINE_API ulong2 GetReflectionTypeGivenFieldTag(const ReflectionField* field);

		// Walks through the fields and returns the component buffer and optionally the index of the field that
		// corresponds to that buffer index
		// Example struct { int, Stream<>, Stream<>, int, Stream<> }
		// buffer_index: 0 -> 1; 1 -> 2, 2 -> 4
		ECSENGINE_API ComponentBuffer GetReflectionTypeRuntimeBufferIndex(const ReflectionType* type, unsigned int buffer_index, unsigned int* field_index = nullptr);

		ECSENGINE_API size_t GetReflectionDataPointerElementByteSize(const ReflectionManager* manager, Stream<char> tag);

		ECSENGINE_API void GetReflectionTypeDependentTypes(const ReflectionManager* manager, const ReflectionType* type, CapacityStream<Stream<char>>& dependent_types);

	}

}