#pragma once
#include "ecspch.h"
#include "../Function.h"
#include "../FunctionInterfaces.h"
#include "../FunctionTemplates.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Internal/Multithreading/TaskManager.h"
#include "../../Internal/InternalStructures.h"
#include "../../Containers/HashTable.h"
#include "../../Containers/AtomicStream.h"
#include "ReflectionTypes.h"

namespace ECSEngine {

	struct World;

	namespace Reflection {

		ECS_CONTAINERS;

		using ReflectionHash = HashFunctionMultiplyString;
		using ReflectionFieldTable = containers::HashTable<ReflectionFieldInfo, ResourceIdentifier, HashFunctionPowerOfTwo, ReflectionHash>;
		using ReflectionTypeTable = containers::HashTable<ReflectionType, ResourceIdentifier, HashFunctionPowerOfTwo, ReflectionHash>;
		using ReflectionEnumTable = containers::HashTable<ReflectionEnum, ResourceIdentifier, HashFunctionPowerOfTwo, ReflectionHash>;

		constexpr size_t ECS_REFLECTION_MAX_TYPE_COUNT = 128;
		constexpr size_t ECS_REFLECTION_MAX_ENUM_COUNT = 32;

		struct ReflectionManagerParseStructuresThreadTaskData;

		struct ECSENGINE_API ReflectionManager {
			struct FolderHierarchy {
				const wchar_t* root;
				const void* allocated_buffer;
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
			ReflectionEnum GetEnum(const char* name) const;

			// Returns -1 if it fails
			unsigned int GetHierarchyIndex(const wchar_t* hierarchy) const;

			void* GetTypeInstancePointer(const char* name, void* instance, unsigned int pointer_index = 0) const;
			void* GetTypeInstancePointer(ReflectionType type, void* instance, unsigned int pointer_index = 0) const;

			bool TryGetType(const char* name, ReflectionType& type) const;
			bool TryGetEnum(const char* name, ReflectionEnum& enum_) const;

			void InitializeFieldTable();
			void InitializeTypeTable(size_t count);
			void InitializeEnumTable(size_t count);

			// It will not set the paths that need to be searched; thread memory will be allocated from the heap, must be freed manually
			void InitializeParseThreadTaskData(size_t thread_memory, size_t path_count, ReflectionManagerParseStructuresThreadTaskData& data, containers::CapacityStream<char>* error_message = nullptr);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(const wchar_t* root, containers::CapacityStream<char>* faulty_path = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(const wchar_t* root, TaskManager* task_manager, containers::CapacityStream<char>* error_message = nullptr);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, containers::CapacityStream<char>* error_message = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, TaskManager* task_manager, containers::CapacityStream<char>* error_message = nullptr);

			void RemoveFolderHierarchy(unsigned int index);
			void RemoveFolderHierarchy(const wchar_t* root);

			// keeps reference intact, it is the same as removing all types from the respective folder hierarchy,
			// calling recreate on the folder hierarchy and then process it
			bool UpdateFolderHierarchy(unsigned int index, containers::CapacityStream<char>* error_message = nullptr);

			// keeps reference intact, it is the same as removing all types from the respective folder hierarchy,
			// calling recreate on the folder hierarchy and then process it
			bool UpdateFolderHierarchy(unsigned int index, TaskManager* task_manager, containers::CapacityStream<char>* error_message = nullptr);

			// keeps reference intact
			bool UpdateFolderHierarchy(const wchar_t* root, containers::CapacityStream<char>* error_message = nullptr);

			// keeps reference intact
			bool UpdateFolderHierarchy(const wchar_t* root, TaskManager* task_manager, containers::CapacityStream<char>* error_message = nullptr);

			ReflectionTypeTable type_definitions;
			ReflectionEnumTable enum_definitions;
			ReflectionFieldTable field_table;
			containers::ResizableStream<FolderHierarchy> folders;
		};

		ECSENGINE_API bool IsTypeCharacter(char character);

		ECSENGINE_API size_t PtrDifference(const void* ptr1, const void* ptr2);

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
		ECSENGINE_API bool AddFieldType(
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

		// It will check macro tokens in order to check if it is explicitely given the type
		ECSENGINE_API bool DeduceFieldTypeFromMacros(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
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

		// Checks for single, double, triple and quadruple component integers
		ECSENGINE_API bool IsIntegral(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralSingleComponent(ReflectionBasicFieldType type);

		ECSENGINE_API bool IsIntegralMultiComponent(ReflectionBasicFieldType type);

		ECSENGINE_API unsigned char BasicTypeComponentCount(ReflectionBasicFieldType type);

		ECSENGINE_API void* GetReflectionFieldStreamBuffer(ReflectionFieldInfo info, const void* data);

		ECSENGINE_API size_t GetReflectionFieldStreamSize(ReflectionFieldInfo info, const void* data);

		// The size of the void stream is that of the elements, not that of the byte size of the elements
		ECSENGINE_API Stream<void> GetReflectionFieldStreamVoid(ReflectionFieldInfo info, const void* data);

		ECSENGINE_API size_t GetReflectionFieldStreamElementByteSize(ReflectionFieldInfo info);

		ECSENGINE_API unsigned char GetReflectionFieldPointerIndirection(ReflectionFieldInfo info);

		inline bool IsBoolBasicType(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool || type == ReflectionBasicFieldType::Bool2 || type == ReflectionBasicFieldType::Bool3 
				|| type == ReflectionBasicFieldType::Bool4;
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