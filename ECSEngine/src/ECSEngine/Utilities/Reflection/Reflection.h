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

		using ReflectionFieldTable = HashTableDefault<ReflectionFieldInfo>;
		using ReflectionTypeTable = HashTableDefault<ReflectionType>;
		using ReflectionEnumTable = HashTableDefault<ReflectionEnum>;

#define ECS_REFLECTION_MAX_TYPE_COUNT (128)
#define ECS_REFLECTION_MAX_ENUM_COUNT (32)

#pragma region Reflection Container Type functions

		extern ReflectionContainerType ECS_REFLECTION_CONTAINER_TYPES[];

		ECSENGINE_API void ReflectionContainerTypeDependentTypes_SingleTemplate(ReflectionContainerTypeDependentTypesData* data);

		// E.g. for Template<Type> string should be Template
		ECSENGINE_API bool ReflectionContainerTypeMatchTemplate(ReflectionContainerTypeMatchData* data, const char* string);

		ECSENGINE_API Stream<char> ReflectionContainerTypeGetTemplateArgument(Stream<char> definition);

#define ECS_REFLECTION_CONTAINER_TYPE_FUNCTION_HEADER(name) ECSENGINE_API bool ReflectionContainerTypeMatch_##name(Reflection::ReflectionContainerTypeMatchData* data); \
															ECSENGINE_API ulong2 ReflectionContainerTypeByteSize_##name(Reflection::ReflectionContainerTypeByteSizeData* data); \
															ECSENGINE_API void ReflectionContainerTypeDependentTypes_##name(Reflection::ReflectionContainerTypeDependentTypesData* data);

#define ECS_REFLECTION_CONTAINER_TYPE_STRUCT(name) { ReflectionContainerTypeMatch_##name, ReflectionContainerTypeDependentTypes_##name, ReflectionContainerTypeByteSize_##name }

		// ---------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CONTAINER_TYPE_FUNCTION_HEADER(Streams);

		// ---------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CONTAINER_TYPE_FUNCTION_HEADER(SparseSet);

		// ---------------------------------------------------------------------------------------------------------------------

		ECS_REFLECTION_CONTAINER_TYPE_FUNCTION_HEADER(Color);

		// ---------------------------------------------------------------------------------------------------------------------
		
		ECS_REFLECTION_CONTAINER_TYPE_FUNCTION_HEADER(ColorFloat);

		// ---------------------------------------------------------------------------------------------------------------------
		
		// Returns -1 if it is not matched
		ECSENGINE_API unsigned int FindReflectionContainerType(Stream<char> definition);

		// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

		struct ReflectionManagerParseStructuresThreadTaskData;

		struct ECSENGINE_API ReflectionManager {
			struct FolderHierarchy {
				const wchar_t* root;
				const void* allocated_buffer;
			};

			struct TypeTag {
				const char* tag_name;
				ResizableStream<const char*> type_names;
			};

			ReflectionManager(MemoryManager* allocator, size_t type_count = ECS_REFLECTION_MAX_TYPE_COUNT, size_t enum_count = ECS_REFLECTION_MAX_ENUM_COUNT);

			ReflectionManager(const ReflectionManager& other) = default;
			ReflectionManager& operator = (const ReflectionManager& other) = default;

			void BindApprovedData(
				const ReflectionManagerParseStructuresThreadTaskData* data,
				unsigned int data_count,
				unsigned int folder_index
			);

			void ClearEnumDefinitions();
			void ClearTypeDefinitions();

			// Returns the current index; it may change if removals take place
			unsigned int CreateFolderHierarchy(const wchar_t* root);

			void DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data);

			void FreeFolderHierarchy(unsigned int folder_index);

			ReflectionType GetType(const char* name) const;
			ReflectionType GetType(unsigned int index) const;

			ReflectionEnum GetEnum(const char* name) const;
			ReflectionEnum GetEnum(unsigned int index) const;

			// Returns -1 if it fails
			unsigned int GetHierarchyIndex(const wchar_t* hierarchy) const;
			
			void GetHierarchyTypes(unsigned int hierarchy_index, CapacityStream<unsigned int>& type_indices);

			void* GetTypeInstancePointer(const char* name, void* instance, unsigned int pointer_index = 0) const;
			void* GetTypeInstancePointer(ReflectionType type, void* instance, unsigned int pointer_index = 0) const;

			const char* GetTypeTag(unsigned int type_index) const;
			const char* GetTypeTag(const char* name) const;

			bool HasTypeTag(unsigned int type_index, const char* tag) const;
			bool HasTypeTag(const char* name, const char* tag) const;

			// Verifies if the type has all of its user defined types reflected aswell
			// For serialization, use the other function
			bool HasValidDependencies(const char* type_name) const;
			// Verifies if the type has all of its user defined types reflected aswell
			// For serialization, use the other function
			bool HasValidDependencies(ReflectionType type) const;

			// Fills in the type indices with the types that correspond to the tag
			void GetAllFromTypeTag(const char* tag, CapacityStream<unsigned int>& type_indices) const;

			bool TryGetType(const char* name, ReflectionType& type) const;
			bool TryGetEnum(const char* name, ReflectionEnum& enum_) const;

			void InitializeFieldTable();
			void InitializeTypeTable(size_t count);
			void InitializeEnumTable(size_t count);

			// It will not set the paths that need to be searched; thread memory will be allocated from the heap, must be freed manually
			void InitializeParseThreadTaskData(size_t thread_memory, size_t path_count, ReflectionManagerParseStructuresThreadTaskData& data, CapacityStream<char>* error_message = nullptr);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(const wchar_t* root, CapacityStream<char>* faulty_path = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(const wchar_t* root, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, CapacityStream<char>* error_message = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			void RemoveFolderHierarchy(unsigned int index);
			void RemoveFolderHierarchy(const wchar_t* root);

			// keeps reference intact, it is the same as removing all types from the respective folder hierarchy,
			// calling recreate on the folder hierarchy and then process it
			bool UpdateFolderHierarchy(unsigned int index, CapacityStream<char>* error_message = nullptr);

			// keeps reference intact, it is the same as removing all types from the respective folder hierarchy,
			// calling recreate on the folder hierarchy and then process it
			bool UpdateFolderHierarchy(unsigned int index, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			// keeps reference intact
			bool UpdateFolderHierarchy(const wchar_t* root, CapacityStream<char>* error_message = nullptr);

			// keeps reference intact
			bool UpdateFolderHierarchy(const wchar_t* root, TaskManager* task_manager, CapacityStream<char>* error_message = nullptr);

			// It will memset to 0 initially then will set the other fields
			// It will set the fields of the data according to the defaults
			void SetInstanceDefaultData(const char* name, void* data) const;

			// It will memset to 0 initially then will set the other fields
			// It will set the fields of the data according to the defaults
			void SetInstanceDefaultData(unsigned int index, void* data) const;

			ResizableStream<TypeTag> type_tags;
			ReflectionTypeTable type_definitions;
			ReflectionEnumTable enum_definitions;
			ReflectionFieldTable field_table;
			ResizableStream<FolderHierarchy> folders;
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

		// returns whether or not the field read succeded
		ECSENGINE_API bool AddTypeField(
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

		// It returns false when there is no such definition, but returns true independently of success status,
		// If error message is nullptr, then it means processing succeded, else failure
		ECSENGINE_API bool CheckFieldExplicitDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* new_line_character
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
			const char* basic_type,
			ReflectionField& info
		);

		ECSENGINE_API ReflectionField GetReflectionFieldInfo(const ReflectionFieldTable* reflection_field_table, const char* basic_type);

		ECSENGINE_API ReflectionField GetReflectionFieldInfo(const ReflectionManager* reflection, const char* basic_type);

		ECSENGINE_API ReflectionField GetReflectionFieldInfoStream(const ReflectionManager* reflection, const char* basic_type, ReflectionStreamFieldType stream_type);

		ECSENGINE_API bool HasReflectStructures(const wchar_t* path);

		ECSENGINE_API size_t GetReflectionTypeByteSize(const ReflectionManager* reflection_manager, ReflectionType type);

		ECSENGINE_API size_t GetReflectionTypeAlignment(const ReflectionManager* reflection_manager, ReflectionType type);

		// Works for user defined types aswell
		ECSENGINE_API size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, ReflectionField field);

		// Checks for single, double, triple and quadruple component integers
		ECSENGINE_API bool IsIntegral(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralSingleComponent(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralMultiComponent(ReflectionBasicFieldType type);

		ECSENGINE_API unsigned char BasicTypeComponentCount(ReflectionBasicFieldType type);

		ECSENGINE_API size_t GetBasicTypeArrayElementSize(const ReflectionFieldInfo& info);

		ECSENGINE_API void* GetReflectionFieldStreamBuffer(const ReflectionFieldInfo& info, const void* data);

		ECSENGINE_API size_t GetReflectionFieldStreamSize(const ReflectionFieldInfo& info, const void* data);

		// The size of the void stream is that of the elements, not that of the byte size of the elements
		ECSENGINE_API Stream<void> GetReflectionFieldStreamVoid(const ReflectionFieldInfo& info, const void* data);

		ECSENGINE_API size_t GetReflectionFieldStreamElementByteSize(const ReflectionFieldInfo& info);

		ECSENGINE_API unsigned char GetReflectionFieldPointerIndirection(const ReflectionFieldInfo& info);

		ECSENGINE_API ReflectionBasicFieldType ConvertBasicTypeMultiComponentToSingle(ReflectionBasicFieldType type);

		inline bool IsBoolBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool2 || type == ReflectionBasicFieldType::Bool3 || type == ReflectionBasicFieldType::Bool4;
		}

		inline bool IsBoolBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool || IsBoolBasicTypeMultiComponent(type);
		}

		// Checks for float2, float3, float4
		inline bool IsFloatBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Float2 || type == ReflectionBasicFieldType::Float3 || type == ReflectionBasicFieldType::Float4;
		}

		// Checks for float, float2, float3, float4
		inline bool IsFloatBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Float || IsFloatBasicTypeMultiComponent(type);
		}

		// Checks for double2, double3, double4
		inline bool IsDoubleBasicTypeMultiComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Double2 || type == ReflectionBasicFieldType::Double3 || type == ReflectionBasicFieldType::Double4;
		}

		// Checks for double, double2, double3, double4
		inline bool IsDoubleBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Double || IsDoubleBasicTypeMultiComponent(type);
		}

		// Checks for float, float2, float3, float4, double, double2, double3, double4
		inline bool IsFloating(ReflectionBasicFieldType type) {
			return IsFloatBasicType(type) || IsDoubleBasicType(type);
		}

		inline bool IsUserDefined(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::UserDefined;
		}

		inline bool IsPointer(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::Pointer;
		}

		inline bool IsEnum(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Enum;
		}

		inline bool IsArray(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::BasicTypeArray;
		}

		inline bool IsStream(ReflectionStreamFieldType type) {
			return type == ReflectionStreamFieldType::Stream || type == ReflectionStreamFieldType::CapacityStream 
				|| type == ReflectionStreamFieldType::ResizableStream;
		}

	}

}