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

namespace ECSEngine {

	struct World;

	namespace Reflection {

		ECS_CONTAINERS;

		enum class ECSENGINE_API ReflectionBasicFieldType : unsigned char {
			Int8,
			UInt8,
			Int16,
			UInt16,
			Int32,
			UInt32,
			Int64,
			UInt64,
			Wchar_t,
			Float,
			Double,
			Bool,
			Enum,
			Char2,
			UChar2,
			Short2,
			UShort2,
			Int2,
			UInt2,
			Long2,
			ULong2,
			Char3,
			UChar3,
			Short3,
			UShort3,
			Int3,
			UInt3,
			Long3,
			ULong3,
			Char4,
			UChar4,
			Short4,
			UShort4,
			Int4,
			UInt4,
			Long4,
			ULong4,
			Float2,
			Float3,
			Float4,
			Double2,
			Double3,
			Double4,
			Bool2,
			Bool3,
			Bool4,
			UserDefined,
			Unknown
		};

		// The extended type indicates whether or not this is a stream/pointer/embedded array
		enum class ECSENGINE_API ReflectionStreamFieldType : unsigned char {
			Basic,
			Pointer,
			BasicTypeArray,
			Stream,
			CapacityStream,
			ResizableStream,
			Unknown
		};

		struct ECSENGINE_API ReflectionFieldInfo {
			ReflectionFieldInfo() {}
			ReflectionFieldInfo(ReflectionBasicFieldType _basic_type, ReflectionStreamFieldType _extended_type, unsigned short _byte_size, unsigned short _basic_type_count)
				: stream_type(_extended_type), basic_type(_basic_type), byte_size(_byte_size), basic_type_count(_basic_type_count) {}

			ReflectionFieldInfo(const ReflectionFieldInfo& other) = default;
			ReflectionFieldInfo& operator = (const ReflectionFieldInfo& other) = default;

			ReflectionStreamFieldType stream_type;
			ReflectionBasicFieldType basic_type;
			unsigned short basic_type_count;
			unsigned short additional_flags;
			unsigned short byte_size;
			unsigned short pointer_offset;
		};

		struct ECSENGINE_API ReflectionField {
			const char* name;
			const char* definition;
			ReflectionFieldInfo info;
		};

		bool ECSENGINE_API IsTypeCharacter(char character);

		struct ECSENGINE_API ReflectionType {
			const char* name;
			containers::Stream<ReflectionField> fields;
			unsigned int folder_hierarchy_index;
		};

		struct ECSENGINE_API ReflectionEnum {
			const char* name;
			Stream<const char*> fields;
			unsigned int folder_hierarchy_index;
		};

		struct ECSENGINE_API ReflectionFilteredFiles {
			containers::ResizableStream<const wchar_t*, MemoryManager> files;
			containers::Stream<const wchar_t*> extensions;
			const char* filter_name;
		};

		using ReflectionHash = HashFunctionMultiplyString;
		using ReflectionFieldTable = containers::HashTable<ReflectionFieldInfo, ResourceIdentifier, HashFunctionPowerOfTwo, ReflectionHash>;
		using ReflectionTypeTable = containers::HashTable<ReflectionType, ResourceIdentifier, HashFunctionPowerOfTwo, ReflectionHash>;
		using ReflectionEnumTable = containers::HashTable<ReflectionEnum, ResourceIdentifier, HashFunctionPowerOfTwo, ReflectionHash>;

		constexpr size_t max_type_count = 1024;
		constexpr size_t max_enum_count = 128;

		struct ECSENGINE_API ReflectionManagerParseStructuresThreadTaskData {
			CapacityStream<char> thread_memory;
			containers::Stream<const wchar_t*> paths;
			containers::CapacityStream<ReflectionType> types;
			containers::CapacityStream<ReflectionEnum> enums;
			const ReflectionFieldTable* field_table;
			CapacityStream<char>* error_message;
			SpinLock error_message_lock;
			size_t total_memory;
			ConditionVariable* condition_variable;
			void* allocated_buffer;
			bool success;
		};

		struct ECSENGINE_API ReflectionManager {
			struct FolderHierarchy {
				const wchar_t* root;
				const void* allocated_buffer;
			};

			ReflectionManager(MemoryManager* allocator, size_t type_count = max_type_count, size_t enum_count = max_enum_count);

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
			containers::ResizableStream<FolderHierarchy, MemoryManager> folders;
		};

		struct ECSENGINE_API ReflectionManagerHasReflectStructuresThreadTaskData {
			ReflectionManager* reflection_manager;
			Stream<const wchar_t*> files;
			unsigned int folder_index;
			unsigned int starting_path_index;
			unsigned int ending_path_index;
			containers::AtomicStream<unsigned int>* valid_paths;
			Semaphore* semaphore;
		};

		ECSENGINE_API size_t PtrDifference(const void* ptr1, const void* ptr2);

		ECSENGINE_API void ReflectionManagerHasReflectStructuresThreadTask(unsigned int thread_id, World* world, void* data);

		ECSENGINE_API void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* data);

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