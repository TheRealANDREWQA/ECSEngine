#pragma once
#include "ecspch.h"
#include "../Function.h"
#include "../FunctionInterfaces.h"
#include "../FunctionTemplates.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Internal/Multithreading/TaskManager.h"
#include "../../Internal/InternalStructures.h"
#include "../../Containers/HashTable.h"

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
			Char,
			Wchar_t,
			Float,
			Double,
			Bool,
			Enum,
			UserDefined,
			Unknown
		};

		enum class ECSENGINE_API ReflectionExtendedFieldType : unsigned char {
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
			Pointer,
			Enum,
			Char,
			UChar,
			Short,
			UShort,
			Int,
			UInt,
			Long,
			ULong,
			SizeT,
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
			Int8Array,
			UInt8Array,
			Int16Array,
			UInt16Array,
			Int32Array,
			UInt32Array,
			Int64Array,
			UInt64Array,
			FloatArray,
			DoubleArray,
			BoolArray,
			EnumArray,
			Stream,
			CapacityStream,
			ResizableStream,
			Unknown
		};

		struct ECSENGINE_API ReflectionFieldInfo {
			ReflectionFieldInfo() {}
			ReflectionFieldInfo(ReflectionBasicFieldType _basic_type, ReflectionExtendedFieldType _extended_type, unsigned short _byte_size, unsigned short _basic_type_count)
				: extended_type(_extended_type), basic_type(_basic_type), byte_size(_byte_size), basic_type_count(_basic_type_count) {}

			ReflectionFieldInfo(const ReflectionFieldInfo& other) = default;
			ReflectionFieldInfo& operator = (const ReflectionFieldInfo& other) = default;

			ReflectionExtendedFieldType extended_type;
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

		using ReflectionFieldTable = containers::IdentifierHashTable<ReflectionFieldInfo, ResourceIdentifier, HashFunctionPowerOfTwo>;
		using ReflectionTypeTable = containers::IdentifierHashTable<ReflectionType, ResourceIdentifier, HashFunctionPowerOfTwo>;
		using ReflectionEnumTable = containers::IdentifierHashTable<ReflectionEnum, ResourceIdentifier, HashFunctionPowerOfTwo>;
		using ReflectionStringHashFunction = HashFunctionMultiplyString;

		struct ECSENGINE_API ReflectionFolderHierarchy {
			ReflectionFolderHierarchy(MemoryManager* allocator);
			ReflectionFolderHierarchy(MemoryManager* allocator, const wchar_t* path);

			ReflectionFolderHierarchy(const ReflectionFolderHierarchy& other) = default;
			ReflectionFolderHierarchy& operator = (const ReflectionFolderHierarchy& other) = default;

			void AddFilterFiles(const char* filter_name, containers::Stream<const wchar_t*> extensions);
			void CreateFromPath(const wchar_t* path);

			void FreeAllMemory();

			void Recreate();

			void RemoveFilterFiles(const char* filter_name);
			void RemoveFilterFiles(unsigned int index);

			const wchar_t* root;
			containers::ResizableStream<const wchar_t*, MemoryManager> folders;
			containers::ResizableStream<ReflectionFilteredFiles, MemoryManager> filtered_files;
		};

		struct ECSENGINE_API ReflectionManagerThreadTaskData {
			CapacityStream<char> thread_memory;
			containers::Stream<const wchar_t*> paths;
			containers::CapacityStream<ReflectionType> types;
			containers::CapacityStream<ReflectionEnum> enums;
			const ReflectionFieldTable* field_table;
			const char* error_message;
			bool success;
			unsigned int faulty_path;
			size_t total_memory;
			ConditionVariable* condition_variable;
			void* allocated_buffer;
		};

		constexpr size_t max_type_count = 1024;
		constexpr size_t max_enum_count = 128;

		struct ECSENGINE_API ReflectionManager {
			struct FolderHierarchy {
				ReflectionFolderHierarchy hierarchy;
				const void* allocated_buffer;
			};

			ReflectionManager(MemoryManager* allocator, size_t type_count = max_type_count, size_t enum_count = max_enum_count);

			ReflectionManager(const ReflectionManager& other) = default;
			ReflectionManager& operator = (const ReflectionManager& other) = default;

			void BindApprovedData(
				const ReflectionManagerThreadTaskData* data,
				unsigned int data_count,
				unsigned int folder_index
			);

			void ClearEnumDefinitions();
			void ClearTypeDefinitions();

			// Returns the current index; it may change if removals take place
			unsigned int CreateFolderHierarchy(const wchar_t* root);

			void DeallocateThreadTaskData(ReflectionManagerThreadTaskData& data);

			void FreeFolderHierarchy(unsigned int folder_index);

			ReflectionType GetType(const char* name) const;
			ReflectionEnum GetEnum(const char* name) const;

			void* GetTypeInstancePointer(const char* name, void* instance, unsigned int pointer_index = 0) const;
			void* GetTypeInstancePointer(ReflectionType type, void* instance, unsigned int pointer_index = 0) const;

			bool TryGetType(const char* name, ReflectionType& type) const;
			bool TryGetEnum(const char* name, ReflectionEnum& enum_) const;

			void InitializeFieldTable();
			void InitializeTypeTable(size_t count);
			void InitializeEnumTable(size_t count);
			// It will not set the paths that need to be searched; thread memory will be allocated from the heap, must be freed manually
			void InitializeThreadTaskData(size_t thread_memory, size_t path_count, ReflectionManagerThreadTaskData& data, const char*& error_message);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(const wchar_t* root, const char*& error_message, wchar_t* faulty_path = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(const wchar_t* root, const char**& error_message, TaskManager* task_manager, wchar_t* faulty_path);

			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, const char*& error_message, wchar_t* faulty_path = nullptr);
			// Returns success, error message will pe pointer to a predefined message, no need to allocate
			// Faulty path must have been previously allocated, 256 characters should be enough
			bool ProcessFolderHierarchy(unsigned int index, const char**& error_message, TaskManager* task_manager, wchar_t* faulty_path = nullptr);

			void RemoveFolderHierarchy(unsigned int index);
			void RemoveFolderHierarchy(const wchar_t* root);

			/*void SolveEnumReferences();
			void SolveEnumReferences(unsigned int index);*/

			// keeps reference intact, it is equal as removing all types from the respective folder hierarchy,
			// calling recreate on the folder hierarchy and then process it
			bool UpdateFolderHierarchy(unsigned int index, const char*& error_message, wchar_t* faulty_path = nullptr);
			// keeps reference intact
			bool UpdateFolderHierarchy(const wchar_t* root, const char*& error_message, wchar_t* faulty_path = nullptr);

			ReflectionTypeTable type_definitions;
			ReflectionEnumTable enum_definitions;
			containers::ResizableStream<FolderHierarchy, MemoryManager> folders;
			ReflectionFieldTable field_table;
			//ReflectionBasicFieldTable basic_field_table;
		};

		size_t ECSENGINE_API PtrDifference(const void* ptr1, const void* ptr2);

		void ECSENGINE_API ReflectionManagerThreadTask(unsigned int thread_id, World* world, void* data);

		void ECSENGINE_API AddEnumDefinition(
			ReflectionManagerThreadTaskData* data,
			const char* ECS_RESTRICT opening_parenthese,
			const char* ECS_RESTRICT closing_parenthese,
			const char* ECS_RESTRICT name,
			unsigned int index
		);

		void ECSENGINE_API AddTypeDefinition(
			ReflectionManagerThreadTaskData* data,
			const char* ECS_RESTRICT opening_parenthese,
			const char* ECS_RESTRICT closing_parenthese,
			const char* ECS_RESTRICT name,
			unsigned int index
		);

		// returns whether or not the field read succeded
		bool ECSENGINE_API AddFieldType(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT last_line_character,
			const char* ECS_RESTRICT semicolon_character
		);

		bool ECSENGINE_API DeduceFieldType(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT field_name,
			const char* ECS_RESTRICT new_line_character,
			const char* ECS_RESTRICT last_valid_character
		);

		// It returns false when there is no such definition, but returns true independently of success status,
		// If error message is nullptr, then it means processing succeded, else failure
		bool ECSENGINE_API CheckFieldExplicitDefinition(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT new_line_character
		);

		// It will check macro tokens in order to check if it is explicitely given the type
		bool ECSENGINE_API DeduceFieldTypeFromMacros(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT field_name,
			const char* ECS_RESTRICT new_line_character
		);

		// Checks to see if it is a pointer type, not from macros
		bool ECSENGINE_API DeduceFieldTypePointer(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT field_name,
			const char* ECS_RESTRICT new_line_character
		);

		void ECSENGINE_API DeduceFieldTypeExtended(
			ReflectionManagerThreadTaskData* data,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT last_type_character,
			ReflectionField& info
		);

		void ECSENGINE_API GetReflectionFieldInfo(
			ReflectionManagerThreadTaskData* data,
			const char* extended_type,
			ReflectionField& info
		);

		bool ECSENGINE_API HasReflectStructures(const wchar_t* path);

		bool ECSENGINE_API IsIntegral(ReflectionBasicFieldType type);

		bool ECSENGINE_API IsIntegral(ReflectionExtendedFieldType type);

		bool ECSENGINE_API IsIntegralArray(ReflectionExtendedFieldType type);

		bool ECSENGINE_API IsIntegralBasicType(ReflectionExtendedFieldType type);

		inline bool IsBoolBasicType(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Bool2 || type == ReflectionExtendedFieldType::Bool3 || type == ReflectionExtendedFieldType::Bool4;
		}

		inline bool IsFloating(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Float || type == ReflectionBasicFieldType::Double;
		}

		inline bool IsFloating(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Float || type == ReflectionExtendedFieldType::Double;
		}

		inline bool IsFloatingArray(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::FloatArray || type == ReflectionExtendedFieldType::DoubleArray;
		}

		inline bool IsFloatBasicType(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Float2 || type == ReflectionExtendedFieldType::Float3 || type == ReflectionExtendedFieldType::Float4;
		}

		inline bool IsDoubleBasicType(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Double2 || type == ReflectionExtendedFieldType::Double3 || type == ReflectionExtendedFieldType::Double4;
		}

		inline bool IsUserDefined(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::UserDefined;
		}

		inline bool IsBool(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Bool;
		}

		inline bool IsBool(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Bool;
		}

		inline bool IsBoolArray(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::BoolArray;
		}

		inline bool IsPointer(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Pointer;
		}

		inline bool IsEnum(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Enum;
		}

		inline bool IsEnum(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Enum;
		}

		inline bool IsEnumArray(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::EnumArray;
		}

		inline bool IsInt8(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Char || type == ReflectionExtendedFieldType::Int8;
		}

		inline bool IsUInt8(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::UChar || type == ReflectionExtendedFieldType::UInt8;
		}

		inline bool IsInt16(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Short || type == ReflectionExtendedFieldType::Int16;
		}

		inline bool isUInt16(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::UShort || type == ReflectionExtendedFieldType::UInt16;
		}

		inline bool IsInt32(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Int || type == ReflectionExtendedFieldType::Int32;
		}

		inline bool IsUInt32(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::UInt || type == ReflectionExtendedFieldType::UInt32;
		}

		inline bool IsInt64(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Long || type == ReflectionExtendedFieldType::Int64;
		}

		inline bool IsUInt64(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::ULong || type == ReflectionExtendedFieldType::UInt64 || type == ReflectionExtendedFieldType::SizeT;
		}

		inline bool IsStream(ReflectionExtendedFieldType type) {
			return type == ReflectionExtendedFieldType::Stream || type == ReflectionExtendedFieldType::CapacityStream 
				|| type == ReflectionExtendedFieldType::ResizableStream;
		}

	}

}