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

		enum ECS_REFLECTION_CUSTOM_TYPE_INDEX : unsigned char {
			ECS_REFLECTION_CUSTOM_TYPE_STREAM,
			ECS_REFLECTION_CUSTOM_TYPE_REFERENCE_COUNTED,
			ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET,
			ECS_REFLECTION_CUSTOM_TYPE_COLOR,
			ECS_REFLECTION_CUSTOM_TYPE_COLOR_FLOAT,
			ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET,
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

			struct TypeTag {
				Stream<char> tag_name;
				ResizableStream<Stream<char>> type_names;
			};

			ReflectionManager(MemoryManager* allocator, size_t type_count = ECS_REFLECTION_MAX_TYPE_COUNT, size_t enum_count = ECS_REFLECTION_MAX_ENUM_COUNT);

			ReflectionManager(const ReflectionManager& other) = default;
			ReflectionManager& operator = (const ReflectionManager& other) = default;

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
			void GetHierarchyTypes(unsigned int hierarchy_index, ReflectionManagerGetQuery options) const;

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

			// Fills in the type indices with the types that correspond to the tag
			void GetAllFromTypeTag(Stream<char> tag, CapacityStream<unsigned int>& type_indices) const;

			bool TryGetType(Stream<char> name, ReflectionType& type) const;
			bool TryGetEnum(Stream<char> name, ReflectionEnum& enum_) const;

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

			// If ignoring some fields, you can set this value manually in order
			// to correctly have the byte size
			void SetTypeByteSize(ReflectionType* type, size_t byte_size);

			// If ignoring some fields, you can set this value manually in order
			// to correctly have the byte size
			void SetTypeAlignment(ReflectionType* type, size_t alignment);

			ResizableStream<TypeTag> type_tags;
			ReflectionTypeTable type_definitions;
			ReflectionEnumTable enum_definitions;
			ReflectionFieldTable field_table;
			ResizableStream<FolderHierarchy> folders;
			ResizableStream<ReflectionConstant> constants;
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
		ECSENGINE_API size_t SearchReflectionUserDefinedTypeByteSize(const ReflectionManager* reflection_manager, Stream<char> definition);

		// Returns -1 if no match was found. It can return 0 for container types
		// which have not had their dependencies met yet.
		ECSENGINE_API ulong2 SearchReflectionUserDefinedTypeByteSizeAlignment(const ReflectionManager* reflection_manager, Stream<char> definition);

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

		// Returns true if both types are the same in their respective reflection managers
		ECSENGINE_API bool CompareReflectionTypes(
			const ReflectionManager* first_reflection_manager,
			const ReflectionManager* second_reflection_manager,
			const ReflectionType* first, 
			const ReflectionType* second
		);

		ECSENGINE_API bool IsReflectionTypeComponent(const ReflectionType* type);

		ECSENGINE_API bool IsReflectionTypeSharedComponent(const ReflectionType* type);

		// Returns true if the type references in any of its fields the subtype
		ECSENGINE_API bool DependsUpon(const ReflectionManager* reflection_manager, const ReflectionType* type, Stream<char> subtype);

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