#include "ecspch.h"
#include "Reflection.h"
#include "../File.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../ForEachFiles.h"
#include "ReflectionStringFunctions.h"

namespace ECSEngine {

	namespace Reflection {

		ECSENGINE_API size_t PtrDifference(const void* ptr1, const void* ptr2);

		void AddTypeTag(
			ReflectionManager* reflection,
			const char* type_tag,
			const char* type_name
		) {
			size_t tag_index = 0;
			for (; tag_index < reflection->type_tags.size; tag_index++) {
				if (function::CompareStrings(reflection->type_tags[tag_index].tag_name, type_tag)) {
					reflection->type_tags[tag_index].type_names.Add(type_name);
					break;
				}
			}
			if (tag_index == reflection->type_tags.size) {
				const char* type_tag_copy = function::StringCopy(reflection->folders.allocator, type_tag).buffer;
				tag_index = reflection->type_tags.Add({ type_tag_copy, ResizableStream<const char*>(reflection->folders.allocator, 1) });
				reflection->type_tags[tag_index].type_names.Add(type_name);
			}
		}

		void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* _data);
		void ReflectionManagerHasReflectStructuresThreadTask(unsigned int thread_id, World* world, void* _data);

		struct ParseReflectionType {
			ReflectionType type;
			const char* type_tag;
		};

		struct ParseOmitFieldByType {
			const char* type;
			unsigned int field_index;
			const char* target_type;
		};

		struct ReflectionManagerParseStructuresThreadTaskData {
			CapacityStream<char> thread_memory;
			CapacityStream<ParseOmitFieldByType> omit_fields_by_type;
			Stream<const wchar_t*> paths;
			CapacityStream<ParseReflectionType> types;
			CapacityStream<ReflectionEnum> enums;
			const ReflectionFieldTable* field_table;
			CapacityStream<char>* error_message;
			SpinLock error_message_lock;
			size_t total_memory;
			ConditionVariable* condition_variable;
			void* allocated_buffer;
			bool success;
		};

		struct ReflectionManagerHasReflectStructuresThreadTaskData {
			ReflectionManager* reflection_manager;
			Stream<const wchar_t*> files;
			unsigned int folder_index;
			unsigned int starting_path_index;
			unsigned int ending_path_index;
			AtomicStream<unsigned int>* valid_paths;
			Semaphore* semaphore;
		};

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionManager::ReflectionManager(MemoryManager* allocator, size_t type_count, size_t enum_count) : folders(GetAllocatorPolymorphic(allocator), 0)
		{
			InitializeFieldTable();
			InitializeTypeTable(type_count);
			InitializeEnumTable(enum_count);

			type_tags.Initialize(GetAllocatorPolymorphic(allocator), 0);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearTypeDefinitions()
		{
			Stream<ReflectionType> type_defs = type_definitions.GetValueStream();
			for (size_t index = 0; index < type_defs.size;  index++) {
				if (type_definitions.IsItemAt(index)) {
					Deallocate(folders.allocator, type_defs[index].name);
				}
			}
			type_definitions.Clear();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::BindApprovedData(
			const ReflectionManagerParseStructuresThreadTaskData* data,
			unsigned int data_count,
			unsigned int folder_index
		)
		{
			size_t total_memory = 0;
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				total_memory += data[data_index].total_memory;
			}

			void* allocation = Allocate(folders.allocator, total_memory);
			uintptr_t ptr = (uintptr_t)allocation;

			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].types.size; index++) {
					ReflectionType type;
					type.folder_hierarchy_index = folder_index;

					size_t type_name_size = strlen(data[data_index].types[index].type.name) + 1;
					char* name_ptr = (char*)ptr;
					memcpy(name_ptr, data[data_index].types[index].type.name, type_name_size * sizeof(char));
					ptr += type_name_size * sizeof(char);

					type.name = name_ptr;
					ptr = function::align_pointer(ptr, alignof(ReflectionType));
					type.fields.InitializeFromBuffer(ptr, data[data_index].types[index].type.fields.size);

					for (size_t field_index = 0; field_index < data[data_index].types[index].type.fields.size; field_index++) {
						size_t field_size = strlen(data[data_index].types[index].type.fields[field_index].name) + 1;
						char* field_ptr = (char*)ptr;
						memcpy(field_ptr, data[data_index].types[index].type.fields[field_index].name, sizeof(char) * field_size);
						ptr += sizeof(char) * field_size;

						type.fields[field_index].name = field_ptr;
						type.fields[field_index].info = data[data_index].types[index].type.fields[field_index].info;

						if (data[data_index].types[index].type.fields[field_index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
							size_t definition_size = strlen(data[data_index].types[index].type.fields[field_index].definition) + 1;
							char* definition_ptr = (char*)ptr;
							memcpy(definition_ptr, data[data_index].types[index].type.fields[field_index].definition, sizeof(char) * definition_size);
							ptr += sizeof(char) * field_size;
						}
						else {
							type.fields[field_index].definition = data[data_index].types[index].type.fields[field_index].definition;
						}
					}

					ECS_RESOURCE_IDENTIFIER(type.name);
					ECS_ASSERT(!type_definitions.Insert(type, identifier));

					if (data[data_index].types[index].type_tag != nullptr) {
						AddTypeTag(this, data[data_index].types[index].type_tag, type.name);
					}
				}

				for (size_t index = 0; index < data[data_index].enums.size; index++) {
					ReflectionEnum enum_;
					enum_.folder_hierarchy_index = folder_index;

					char* name_ptr = (char*)ptr;
					size_t type_name_size = strlen(data[data_index].enums[index].name) + 1;
					memcpy(name_ptr, data[data_index].enums[index].name, type_name_size * sizeof(char));
					ptr += sizeof(char) * type_name_size;

					enum_.name = name_ptr;
					ptr = function::align_pointer(ptr, alignof(ReflectionEnum));
					enum_.fields.InitializeFromBuffer(ptr, data[data_index].enums[index].fields.size);

					for (size_t field_index = 0; field_index < data[data_index].enums[index].fields.size; field_index++) {
						size_t field_size = strlen(data[data_index].enums[index].fields[field_index]) + 1;
						char* field_ptr = (char*)ptr;
						memcpy(field_ptr, data[data_index].enums[index].fields[field_index], field_size * sizeof(char));

						//function::Capitalize(field_ptr);
						enum_.fields[field_index] = field_ptr;
						ptr += sizeof(char) * field_size;
					}

					ECS_RESOURCE_IDENTIFIER(enum_.name);
					ECS_ASSERT(!enum_definitions.Insert(enum_, identifier));
				}
			}

			size_t difference = PtrDifference(allocation, (void*)ptr);
			ECS_ASSERT(difference <= total_memory);

			folders[folder_index].allocated_buffer = allocation;

			// Resolve references to the omit fields by type
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].omit_fields_by_type.size; index++) {
					ReflectionType* type = type_definitions.GetValuePtr(data[data_index].omit_fields_by_type[index].type);
					// Test to see if the reference could be solved
					ReflectionType* target_type;
					if (type_definitions.TryGetValuePtr(data[data_index].omit_fields_by_type[index].target_type, target_type)) {
						// Offset all the fields after it with the corresponding size
						size_t type_size = GetTypeByteSize(*target_type);
						size_t type_alignment = GetTypeAlignment(*target_type);

						if (data[data_index].omit_fields_by_type[index].field_index > 0) {
							unsigned int previous_field = data[data_index].omit_fields_by_type[index].field_index - 1;
							size_t omit_field_offset = type->fields[previous_field].info.pointer_offset + type->fields[previous_field].info.byte_size;
							size_t misalignment = omit_field_offset % type_alignment;
							type_size += misalignment;
						}

						// Can't handle this case - the byte size will not be preserved
						if (data[data_index].omit_fields_by_type[index].field_index == type->fields.size) {
							ECS_ASSERT(false);
						}
						else {
							// Check to see if the type is properly alignment aftwerwards
							size_t first_field_alignment = 0;
							unsigned int field_index = data[data_index].omit_fields_by_type[index].field_index;
							if (type->fields[field_index].info.stream_type == ReflectionStreamFieldType::Basic || type->fields[field_index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
								first_field_alignment = GetFieldTypeAlignment(type->fields[field_index].info.basic_type);
							}
							else {
								first_field_alignment = GetFieldTypeAlignment(type->fields[field_index].info.stream_type);
							}
							size_t misalignment = type_size % first_field_alignment;
							type_size += misalignment;

							for (size_t field_index = data[data_index].omit_fields_by_type[index].field_index; field_index < type->fields.size; field_index++) {
								type->fields[field_index].info.pointer_offset += type_size;
							}
						}
					}
					else {
						// Assert false for the time being
						ECS_ASSERT(false);
					}
				}
			}

		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearEnumDefinitions() {
			Stream<ReflectionEnum> enum_defs = enum_definitions.GetValueStream();
			for (size_t index = 0; index < enum_defs.size; index++) {
				if (enum_definitions.IsItemAt(index)) {
					Deallocate(folders.allocator, enum_defs[index].name);
				}
			}
			enum_definitions.Clear();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::CreateFolderHierarchy(const wchar_t* root) {
			unsigned int index = folders.ReserveNewElement();
			folders[index] = { function::StringCopy(folders.allocator, root).buffer, nullptr };
			folders.size++;
			return index;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data)
		{
			free(data.thread_memory.buffer);
			Deallocate(folders.allocator, data.types.buffer);
			Deallocate(folders.allocator, data.enums.buffer);
			Deallocate(folders.allocator, data.paths.buffer);
			Deallocate(folders.allocator, data.omit_fields_by_type.buffer);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::FreeFolderHierarchy(unsigned int folder_index)
		{
			Stream<ReflectionType> type_defs = type_definitions.GetValueStream();
			Stream<ReflectionEnum> enum_defs = enum_definitions.GetValueStream();
			const ResourceIdentifier* type_identifiers = type_definitions.GetIdentifiers();

			for (int64_t index = 0; index < type_defs.size; index++) {
				if (type_definitions.IsItemAt(index)) {
					if (type_defs[index].folder_hierarchy_index == folder_index) {
						// Look to see if it belongs to a tag
						for (size_t tag_index = 0; tag_index < type_tags.size; tag_index++) {
							for (int64_t subtag_index = 0; subtag_index < (int64_t)type_tags[tag_index].type_names.size; subtag_index++) {
								if (type_tags[tag_index].type_names[subtag_index] == type_defs[index].name) {
									type_tags[tag_index].type_names.RemoveSwapBack(subtag_index);
									subtag_index--;
								}
							}
						}

						type_definitions.EraseFromIndex(index);
						index--;
					}
				}
			}
			for (int64_t index = 0; index < enum_defs.size; index++) {
				if (enum_definitions.IsItemAt(index)) {
					if (enum_defs[index].folder_hierarchy_index == folder_index) {
						enum_definitions.EraseFromIndex(index);
						index--;
					}
				}
			}

			Deallocate(folders.allocator, folders[folder_index].allocated_buffer);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType ReflectionManager::GetType(const char* name) const
		{
			size_t name_length = strlen(name);
			ResourceIdentifier identifier = ResourceIdentifier(name, name_length);
			return type_definitions.GetValue(identifier);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType ReflectionManager::GetType(unsigned int index) const
		{
			return type_definitions.GetValueFromIndex(index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum ReflectionManager::GetEnum(const char* name) const {
			size_t name_length = strlen(name);
			ResourceIdentifier identifier = ResourceIdentifier(name, name_length);
			return enum_definitions.GetValue(identifier);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum ReflectionManager::GetEnum(unsigned int index) const
		{
			return enum_definitions.GetValueFromIndex(index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::GetHierarchyIndex(const wchar_t* hierarchy) const
		{
			for (unsigned int index = 0; index < folders.size; index++) {
				if (wcscmp(folders[index].root, hierarchy) == 0) {
					return index;
				}
			}
			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::GetHierarchyTypes(unsigned int hierarchy_index, CapacityStream<unsigned int>& type_indices) {
			unsigned int extended_capacity = type_definitions.GetExtendedCapacity();
			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (type_definitions.IsItemAt(index) && type_definitions.GetValueFromIndex(index).folder_hierarchy_index == hierarchy_index) {
					type_indices.AddSafe(type_indices.size);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* ReflectionManager::GetTypeInstancePointer(const char* name, void* instance, unsigned int pointer_index) const
		{
			ReflectionType type = GetType(name);
			return GetTypeInstancePointer(type, instance, pointer_index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* ReflectionManager::GetTypeInstancePointer(ReflectionType type, void* instance, unsigned int pointer_index) const
		{
			size_t count = 0;
			for (size_t index = 0; index < type.fields.size; index++) {
				if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Pointer || IsStream(type.fields[index].info.stream_type)) {
					if (count == pointer_index) {
						return (void*)((uintptr_t)instance + type.fields[index].info.pointer_offset);
					}
					count++;
				}
			}
			ECS_ASSERT(false);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		const char* ReflectionManager::GetTypeTag(unsigned int type_index) const
		{
			for (size_t tag_index = 0; tag_index < type_tags.size; tag_index++) {
				for (size_t subtag_index = 0; subtag_index < type_tags[tag_index].type_names.size; subtag_index++) {
					if (type_definitions.GetValueFromIndex(type_index).name == type_tags[tag_index].type_names[subtag_index]) {
						return type_tags[tag_index].tag_name;
					}
				}
			}

			return nullptr;
		}

		const char* ReflectionManager::GetTypeTag(const char* name) const
		{
			unsigned int type_index = type_definitions.Find(name);
			if (type_index != -1) {
				return GetTypeTag(type_index);
			}
			return nullptr;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::IsTypeTag(unsigned int type_index, const char* tag) const
		{
			const char* type_tag = GetTypeTag(type_index);
			if (type_tag == nullptr) {
				return false;
			}
			else {
				return function::CompareStrings(type_tag, tag);
			}
		}

		bool ReflectionManager::IsTypeTag(const char* name, const char* tag) const
		{
			unsigned int type_index = type_definitions.Find(name);
			if (type_index != -1) {
				return IsTypeTag(type_index, tag);
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::GetAllFromTypeTag(const char* tag, CapacityStream<unsigned int>& type_indices) const
		{
			for (size_t index = 0; index < type_tags.size; index++) {
				if (function::CompareStrings(tag, type_tags[index].tag_name)) {
					for (size_t subindex = 0; subindex < type_tags[index].type_names.size; subindex++) {
						unsigned int type_index = type_definitions.Find(type_tags[index].type_names[subindex]);
						ECS_ASSERT(type_index != -1);
						type_indices.Add(type_index);
					}
					break;
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::TryGetType(const char* name, ReflectionType& type) const
		{
			ECS_RESOURCE_IDENTIFIER(name);
			return type_definitions.TryGetValue(identifier, type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::TryGetEnum(const char* name, ReflectionEnum& enum_) const {
			ECS_RESOURCE_IDENTIFIER(name);
			return enum_definitions.TryGetValue(identifier, enum_);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeFieldTable()
		{
			constexpr size_t table_size = 256;
			void* allocation = Allocate(folders.allocator, ReflectionFieldTable::MemoryOf(table_size));
			field_table.InitializeFromBuffer(allocation, table_size);

			// Initialize all values, helped by macros
			ResourceIdentifier identifier;
#define BASIC_TYPE(type, basic_type, stream_type) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionFieldInfo(basic_type, stream_type, sizeof(type), 1), identifier);
#define INT_TYPE(type, val) BASIC_TYPE(type, ReflectionBasicFieldType::val, ReflectionStreamFieldType::Basic)
#define COMPLEX_TYPE(type, basic_type, stream_type, byte_size) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionFieldInfo(basic_type, stream_type, byte_size, 1), identifier);

#define TYPE_234(base, reflection_type) COMPLEX_TYPE(base##2, ReflectionBasicFieldType::reflection_type##2, ReflectionStreamFieldType::Basic, sizeof(base) * 2); \
COMPLEX_TYPE(base##3, ReflectionBasicFieldType::reflection_type##3, ReflectionStreamFieldType::Basic, sizeof(base) * 3); \
COMPLEX_TYPE(base##4, ReflectionBasicFieldType::reflection_type##4, ReflectionStreamFieldType::Basic, sizeof(base) * 4);

#define TYPE_234_SIGNED_INT(base, basic_reflect) COMPLEX_TYPE(base##2, ReflectionBasicFieldType::basic_reflect##2, ReflectionStreamFieldType::Basic, sizeof(base) * 2); \
COMPLEX_TYPE(base##3, ReflectionBasicFieldType::basic_reflect##3, ReflectionStreamFieldType::Basic, sizeof(base) * 3); \
COMPLEX_TYPE(base##4, ReflectionBasicFieldType::basic_reflect##4, ReflectionStreamFieldType::Basic, sizeof(base) * 4);

#define TYPE_234_UNSIGNED_INT(base, basic_reflect) COMPLEX_TYPE(u##base##2, ReflectionBasicFieldType::U##basic_reflect##2, ReflectionStreamFieldType::Basic, sizeof(base) * 2); \
COMPLEX_TYPE(u##base##3, ReflectionBasicFieldType::U##basic_reflect##3, ReflectionStreamFieldType::Basic, sizeof(base) * 3); \
COMPLEX_TYPE(u##base##4, ReflectionBasicFieldType::U##basic_reflect##4, ReflectionStreamFieldType::Basic, sizeof(base) * 4);

#define TYPE_234_INT(base, basic_reflect, extended_reflect) TYPE_234_SIGNED_INT(base, basic_reflect); TYPE_234_UNSIGNED_INT(base, basic_reflect)

			INT_TYPE(int8_t, Int8);
			INT_TYPE(uint8_t, UInt8);
			INT_TYPE(int16_t, Int16);
			INT_TYPE(uint16_t, UInt16);
			INT_TYPE(int32_t, Int32);
			INT_TYPE(uint32_t, UInt32);
			INT_TYPE(int64_t, Int64);
			INT_TYPE(uint64_t, UInt64);
			INT_TYPE(bool, Bool);

			BASIC_TYPE(float, ReflectionBasicFieldType::Float, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(double, ReflectionBasicFieldType::Double, ReflectionStreamFieldType::Basic);

			// Basic type aliases
			BASIC_TYPE(char, ReflectionBasicFieldType::Int8, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned char, ReflectionBasicFieldType::UInt8, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(short, ReflectionBasicFieldType::Int16, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned short, ReflectionBasicFieldType::UInt16, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(int, ReflectionBasicFieldType::Int32, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned int, ReflectionBasicFieldType::UInt32, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(long long, ReflectionBasicFieldType::Int64, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(unsigned long long, ReflectionBasicFieldType::UInt64, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(size_t, ReflectionBasicFieldType::UInt64, ReflectionStreamFieldType::Basic);
			BASIC_TYPE(wchar_t, ReflectionBasicFieldType::Wchar_t, ReflectionStreamFieldType::Basic);

			COMPLEX_TYPE(enum, ReflectionBasicFieldType::Enum, ReflectionStreamFieldType::Basic, sizeof(ReflectionBasicFieldType), 1);
			COMPLEX_TYPE(pointer, ReflectionBasicFieldType::UserDefined, ReflectionStreamFieldType::Pointer, sizeof(void*), 1);
			
			TYPE_234(bool, Bool);
			TYPE_234(float, Float);
			TYPE_234(double, Double);

			TYPE_234_INT(char, Char);
			TYPE_234_INT(short, Short);
			TYPE_234_INT(int, Int);
			TYPE_234_INT(long long, Long);

#undef INT_TYPE
#undef BASIC_TYPE
#undef COMPLEX_TYPE
#undef TYPE_234_INT
#undef TYPE_234_UNSIGNED_INT
#undef TYPE_234_SIGNED_INT
#undef TYPE_234
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeTypeTable(size_t count)
		{
			void* allocation = Allocate(folders.allocator, ReflectionTypeTable::MemoryOf(count));
			type_definitions.InitializeFromBuffer(allocation, count);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeEnumTable(size_t count)
		{
			void* allocation = Allocate(folders.allocator, ReflectionEnumTable::MemoryOf(count));
			enum_definitions.InitializeFromBuffer(allocation, count);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeParseThreadTaskData(
			size_t thread_memory,
			size_t path_count, 
			ReflectionManagerParseStructuresThreadTaskData& data, 
			CapacityStream<char>* error_message
		)
		{
			data.error_message = error_message;

			// Allocate memory for the type and enum stream; speculate 64 types and 32 enums
			const size_t max_types = 64;
			const size_t max_enums = 32;
			const size_t path_size = 128;
			const size_t max_omit_fields = 128;
			

			data.types.Initialize(folders.allocator, 0, max_types);
			data.enums.Initialize(folders.allocator, 0, max_enums);
			data.paths.Initialize(folders.allocator, path_count);
			data.omit_fields_by_type.Initialize(folders.allocator, 0, max_omit_fields);

			void* thread_allocation = malloc(thread_memory);
			data.thread_memory.InitializeFromBuffer(thread_allocation, 0, thread_memory);
			data.field_table = &field_table;
			data.success = true;
			data.total_memory = 0;
			data.error_message_lock.unlock();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(const wchar_t* root, CapacityStream<char>* error_message) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return ProcessFolderHierarchy(hierarchy_index, error_message);
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(
			const wchar_t* root, 
			TaskManager* task_manager, 
			CapacityStream<char>* error_message
		) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return ProcessFolderHierarchy(hierarchy_index, task_manager, error_message);
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		// It will deallocate the files
		bool ProcessFolderHierarchyImplementation(ReflectionManager* manager, unsigned int folder_index, CapacityStream<const wchar_t*> files, CapacityStream<char>* error_message) {			
			ReflectionManagerParseStructuresThreadTaskData thread_data;

			constexpr size_t thread_memory = 5'000'000;
			// Paths that need to be searched will not be assigned here
			ECS_TEMP_ASCII_STRING(temp_error_message, 1024);
			if (error_message == nullptr) {
				error_message = &temp_error_message;
			}
			manager->InitializeParseThreadTaskData(thread_memory, files.size, thread_data, error_message);

			ConditionVariable condition_variable;
			thread_data.condition_variable = &condition_variable;
			// Assigning paths
			for (size_t path_index = 0; path_index < files.size; path_index++) {
				thread_data.paths[path_index] = files[path_index];
			}

			ReflectionManagerParseThreadTask(0, nullptr, &thread_data);
			if (thread_data.success == true) {
				manager->BindApprovedData(&thread_data, 1, folder_index);
			}

			manager->DeallocateThreadTaskData(thread_data);
			for (size_t index = 0; index < files.size; index++) {
				Deallocate(manager->folders.allocator, files[index]);
			}

			return thread_data.success;
		}

		bool ReflectionManager::ProcessFolderHierarchy(unsigned int index, CapacityStream<char>* error_message) {
			AllocatorPolymorphic allocator = folders.allocator;
			const wchar_t* c_file_extensions[] = {
				L".c",
				L".cpp",
				L".h",
				L".hpp"
			};

			const size_t MAX_DIRECTORIES = ECS_KB;
			const wchar_t** _files = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_DIRECTORIES);
			CapacityStream<const wchar_t*> files(_files, 0, MAX_DIRECTORIES);
			bool status = GetDirectoryFileWithExtensionRecursive(folders[index].root, allocator, files, { c_file_extensions, std::size(c_file_extensions) });
			if (!status) {
				return false;
			}

			return ProcessFolderHierarchyImplementation(this, index, files, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(
			unsigned int folder_index, 
			TaskManager* task_manager, 
			CapacityStream<char>* error_message
		) {
			unsigned int thread_count = task_manager->GetThreadCount();

			AllocatorPolymorphic allocator = folders.allocator;
			const wchar_t* c_file_extensions[] = {
				L".c",
				L".cpp",
				L".h",
				L".hpp"
			};

			const size_t MAX_DIRECTORIES = ECS_KB;
			const wchar_t** _files = (const wchar_t**)ECS_STACK_ALLOC(sizeof(const wchar_t*) * MAX_DIRECTORIES);
			CapacityStream<const wchar_t*> files(_files, 0, MAX_DIRECTORIES);
			bool status = GetDirectoryFileWithExtensionRecursive(folders[folder_index].root, allocator, files, { c_file_extensions, std::size(c_file_extensions) });
			if (!status) {
				return false;
			}

			// Preparsing the files in order to have thread act only on files that actually need to process
			// Otherwise unequal distribution of files will result in poor multithreaded performance
			unsigned int files_count = files.size;

			// Process these files on separate threads only if their number is greater than thread count / 2
			if (files_count < thread_count / 2) {
				return ProcessFolderHierarchyImplementation(this, folder_index, files, error_message);
			}

			unsigned int* path_indices_buffer = (unsigned int*)Allocate(folders.allocator, sizeof(unsigned int) * files_count, alignof(unsigned int));
			AtomicStream<unsigned int> path_indices = AtomicStream<unsigned int>(path_indices_buffer, 0, files_count);

			// Calculate the thread start and thread end
			unsigned int reflect_per_thread_data = files_count / thread_count;
			unsigned int reflect_remainder_tasks = files_count % thread_count;

			// If there is more than a task per thread - launch all
			// If per thread data is 0, then launch as many threads as remainder
			unsigned int reflect_thread_count = function::Select(reflect_per_thread_data == 0, reflect_remainder_tasks, thread_count);
			ReflectionManagerHasReflectStructuresThreadTaskData* reflect_thread_data = (ReflectionManagerHasReflectStructuresThreadTaskData*)ECS_STACK_ALLOC(
				sizeof(ReflectionManagerHasReflectStructuresThreadTaskData) * reflect_thread_count
			);
			Semaphore reflect_semaphore;
			reflect_semaphore.SetTarget(reflect_thread_count);

			unsigned int reflect_current_path_start = 0;
			for (size_t thread_index = 0; thread_index < reflect_thread_count; thread_index++) {
				bool has_remainder = reflect_remainder_tasks > 0;
				reflect_remainder_tasks -= has_remainder;
				unsigned int current_thread_start = reflect_current_path_start;
				unsigned int current_thread_end = reflect_current_path_start + reflect_per_thread_data + has_remainder;

				// Increase the total count of paths
				reflect_current_path_start += reflect_per_thread_data + has_remainder;

				reflect_thread_data[thread_index].ending_path_index = current_thread_end;
				reflect_thread_data[thread_index].starting_path_index = current_thread_start;
				reflect_thread_data[thread_index].folder_index = folder_index;
				reflect_thread_data[thread_index].reflection_manager = this;
				reflect_thread_data[thread_index].valid_paths = &path_indices;
				reflect_thread_data[thread_index].semaphore = &reflect_semaphore;
				reflect_thread_data[thread_index].files = files;

				// Launch the task
				ThreadTask task = ECS_THREAD_TASK_NAME(ReflectionManagerHasReflectStructuresThreadTask, reflect_thread_data + thread_index, 0);
				task_manager->AddDynamicTaskAndWake(task);
			}

			reflect_semaphore.SpinWait();

			unsigned int parse_per_thread_paths = path_indices.size / thread_count;
			unsigned int parse_thread_paths_remainder = path_indices.size % thread_count;

			constexpr size_t thread_memory = 10'000'000;
			ConditionVariable condition_variable;

			ECS_TEMP_ASCII_STRING(temp_error_message, 1024);
			if (error_message == nullptr) {
				error_message = &temp_error_message;
			}

			unsigned int parse_thread_count = function::Select(parse_per_thread_paths == 0, parse_thread_paths_remainder, thread_count);
			ReflectionManagerParseStructuresThreadTaskData* parse_thread_data = (ReflectionManagerParseStructuresThreadTaskData*)ECS_STACK_ALLOC(
				sizeof(ReflectionManagerParseStructuresThreadTaskData) * thread_count
			);

			unsigned int parse_thread_start = 0;
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				bool has_remainder = parse_thread_paths_remainder > 0;
				parse_thread_paths_remainder -= has_remainder;
				unsigned int thread_current_paths = parse_per_thread_paths + has_remainder;
				// initialize data with buffers
				InitializeParseThreadTaskData(thread_memory, thread_current_paths, parse_thread_data[thread_index], error_message);
				parse_thread_data[thread_index].condition_variable = &condition_variable;

				// Set thread paths
				for (size_t path_index = parse_thread_start; path_index < parse_thread_start + thread_current_paths; path_index++) {
					parse_thread_data[thread_index].paths[path_index - parse_thread_start] = files[path_indices[path_index]];
				}
				parse_thread_start += thread_current_paths;
			}

			condition_variable.Reset();

			// Push thread execution
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				ThreadTask task = ECS_THREAD_TASK_NAME(ReflectionManagerParseThreadTask, parse_thread_data + thread_index, 0);
				task_manager->AddDynamicTaskAndWake(task);
			}

			// Wait until threads are done
			condition_variable.Wait(parse_thread_count);

			bool success = true;
			
			size_t path_index = 0;
			// Check every thread for success and return the first error if there is such an error
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				if (parse_thread_data[thread_index].success == false) {
					success = false;
					break;
				}
				path_index += parse_thread_data[thread_index].paths.size;
			}

			if (success) {
				BindApprovedData(parse_thread_data, parse_thread_count, folder_index);
			}

			// Free the previously allocated memory
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				DeallocateThreadTaskData(parse_thread_data[thread_index]);
			}
			Deallocate(folders.allocator, path_indices_buffer);
			for (size_t index = 0; index < files.size; index++) {
				Deallocate(folders.allocator, files[index]);
			}

			return success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::RemoveFolderHierarchy(unsigned int folder_index)
		{
			FreeFolderHierarchy(folder_index);
			folders.RemoveSwapBack(folder_index);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::RemoveFolderHierarchy(const wchar_t* root) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				RemoveFolderHierarchy(hierarchy_index);
				return;
			}
			// error if not found
			ECS_ASSERT(false);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(unsigned int folder_index, CapacityStream<char>* error_message)
		{
			FreeFolderHierarchy(folder_index);
			return ProcessFolderHierarchy(folder_index, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(unsigned int folder_index, TaskManager* task_manager, CapacityStream<char>* error_message)
		{
			FreeFolderHierarchy(folder_index);
			return ProcessFolderHierarchy(folder_index, task_manager, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(const wchar_t* root, CapacityStream<char>* error_message) {
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return UpdateFolderHierarchy(hierarchy_index, error_message);
			}
			// Fail if it was not found previously
			ECS_ASSERT(false);
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(const wchar_t* root, TaskManager* task_manager, CapacityStream<char>* error_message)
		{
			unsigned int hierarchy_index = GetHierarchyIndex(root);
			if (hierarchy_index != -1) {
				return UpdateFolderHierarchy(hierarchy_index, task_manager, error_message);
			}
			// Fail if it was not found previously
			ECS_ASSERT(false);
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetTypeByteSize(ReflectionType type)
		{
			size_t total = 0;
			for (size_t index = 0; index < type.fields.size; index++) {
				total += type.fields[index].info.byte_size;
			}

			return total;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetTypeAlignment(ReflectionType type)
		{
			size_t alignment = 0;

			for (size_t index = 0; index < type.fields.size; index++) {
				if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Basic ||
					type.fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					alignment = std::max(alignment, GetFieldTypeAlignment(type.fields[index].info.basic_type));
				}
				else {
					alignment = std::max(alignment, GetFieldTypeAlignment(type.fields[index].info.stream_type));
				}
			}

			return alignment;
		}

		unsigned char ECS_BASIC_FIELD_TYPE_ALIGNMENT[] = {
			alignof(int8_t),
			alignof(uint8_t),
			alignof(int16_t),
			alignof(uint16_t),
			alignof(int32_t),
			alignof(uint32_t),
			alignof(int64_t),
			alignof(uint64_t),
			alignof(wchar_t),
			alignof(float),
			alignof(double),
			alignof(bool),
			// enum
			alignof(unsigned char),
			alignof(char2),
			alignof(uchar2),
			alignof(short2),
			alignof(ushort2),
			alignof(int2),
			alignof(uint2),
			alignof(long2),
			alignof(ulong2),
			alignof(char3),
			alignof(uchar3),
			alignof(short3),
			alignof(ushort3),
			alignof(int3),
			alignof(uint3),
			alignof(long3),
			alignof(ulong3),
			alignof(char4),
			alignof(uchar4),
			alignof(short4),
			alignof(ushort4),
			alignof(int4),
			alignof(uint4),
			alignof(long4),
			alignof(ulong4),
			alignof(float2),
			alignof(float3),
			alignof(float4),
			alignof(double2),
			alignof(double3),
			alignof(double4),
			alignof(bool2),
			alignof(bool3),
			alignof(bool4),
			// User defined
			8,
			// Unknown
			8
		};

		unsigned char ECS_STREAM_FIELD_TYPE_ALIGNMENT[] = {
			// Basic - should not be accessed
			8,
			alignof(void*),
			// BasicTypeArray - should not be accessed
			8,
			alignof(Stream<void>),
			alignof(CapacityStream<void>),
			alignof(ResizableStream<char>),
			// Unknown
			8
		};

		static_assert(std::size(ECS_BASIC_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionBasicFieldType::COUNT);
		static_assert(std::size(ECS_STREAM_FIELD_TYPE_ALIGNMENT) == (unsigned int)ReflectionStreamFieldType::COUNT);

		size_t GetFieldTypeAlignment(ReflectionBasicFieldType field_type) {
			return ECS_BASIC_FIELD_TYPE_ALIGNMENT[(unsigned int)field_type];
		}

		size_t GetFieldTypeAlignment(ReflectionStreamFieldType stream_type) {
			return ECS_STREAM_FIELD_TYPE_ALIGNMENT[(unsigned int)stream_type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsTypeCharacter(char character)
		{
			if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') 
				|| character == '_' || character == '<' || character == '>') {
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t PtrDifference(const void* start, const void* end)
		{
			return (uintptr_t)end - (uintptr_t)start;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void WriteErrorMessage(ReflectionManagerParseStructuresThreadTaskData* data, const char* message, unsigned int path_index) {
			data->success = false;
			data->error_message_lock.lock();
			data->error_message->AddStream(ToStream(message));
			if (path_index != -1) {
				function::ConvertWideCharsToASCII(ToStream(data->paths[path_index]), *data->error_message);
			}
			data->error_message->Add('\n');
			data->error_message->AssertCapacity();
			data->error_message_lock.unlock();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManagerHasReflectStructuresThreadTask(unsigned int thread_id, World* world, void* _data) {
			ReflectionManagerHasReflectStructuresThreadTaskData* data = (ReflectionManagerHasReflectStructuresThreadTaskData*)_data;
			for (unsigned int path_index = data->starting_path_index; path_index < data->ending_path_index; path_index++) {
				bool has_reflect = HasReflectStructures(data->files[path_index]);
				if (has_reflect) {
					data->valid_paths->Add(path_index);
				}
			}

			data->semaphore->Enter();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* _data) {
			ReflectionManagerParseStructuresThreadTaskData* data = (ReflectionManagerParseStructuresThreadTaskData*)_data;

			const size_t WORD_MAX_COUNT = 1024;
			unsigned int words_buffer[WORD_MAX_COUNT];
			Stream<unsigned int> words = Stream<unsigned int>(words_buffer, 0);

			size_t ecs_reflect_size = strlen(STRING(ECS_REFLECT));
			const size_t MAX_FIRST_LINE_CHARACTERS = 512;
			char first_line_characters[512];

			// search every path
			for (size_t index = 0; index < data->paths.size; index++) {
				ECS_FILE_HANDLE file = 0;
				// open the file from the beginning
				ECS_FILE_STATUS_FLAGS status = OpenFile(data->paths[index], &file, ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_READ_ONLY);

				// if access was granted
				if (status == ECS_FILE_STATUS_OK) {
					ScopedFile scoped_file(file);

					const size_t MAX_FIRST_LINE_CHARACTERS = 512;
					// Assume that the first line is at most MAX_FIRST_LINE characters
					unsigned int first_line_count = ReadFromFile(file, { first_line_characters, MAX_FIRST_LINE_CHARACTERS });
					if (first_line_count == -1) {
						WriteErrorMessage(data, "Could not read first line. Faulty path: ", index);
						return;
					}
					first_line_characters[first_line_count - 1] = '\0';

					// read the first line in order to check if there is something to be reflected	
					// if there is something to be reflected
					const char* first_new_line = strchr(first_line_characters, '\n');
					const char* has_reflect = strstr(first_line_characters, STRING(ECS_REFLECT));
					if (first_new_line != nullptr && has_reflect != nullptr && has_reflect < first_new_line) {
						// reset words stream
						words.size = 0;

						size_t file_size = GetFileByteSize(file);
						if (file_size == 0) {
							WriteErrorMessage(data, "Could not deduce file size. Faulty path: ", index);
							return;
						}
						// Reset the file pointer and read again
						SetFileCursor(file, 0, ECS_FILE_SEEK_BEG);

						char* file_contents = data->thread_memory.buffer + data->thread_memory.size;
						unsigned int bytes_read = ReadFromFile(file, { file_contents, file_size });
						file_contents[bytes_read - 1] = '\0';

						data->thread_memory.size += bytes_read;
						// Skip the first line - it contains the flag ECS_REFLECT
						const char* new_line = strchr(file_contents, '\n');
						if (new_line == nullptr) {
							WriteErrorMessage(data, "Could not skip ECS_REFLECT flag on first line. Faulty path: ", index);
							return;
						}
						file_contents = (char*)new_line + 1;

						// if there's not enough memory, fail
						if (data->thread_memory.size > data->thread_memory.capacity) {
							WriteErrorMessage(data, "Not enough memory to read file contents. Faulty path: ", index);
							return;
						}

						// search for more ECS_REFLECTs 
						function::FindToken(file_contents, STRING(ECS_REFLECT), words);

						const char* tag_name = nullptr;

						size_t last_position = 0;
						for (size_t index = 0; index < words.size; index++) {
							const char* ecs_reflect_end_position = file_contents + words[index] + ecs_reflect_size;

							// get the type name
							const char* space = strchr(ecs_reflect_end_position, ' ');
							// The reflection is tagged, record it for later on
							if (space != ecs_reflect_end_position) {
								tag_name = ecs_reflect_end_position[0] == '_' ? ecs_reflect_end_position + 1 : ecs_reflect_end_position;
							}

							// if no space was found after the token, fail
							if (space == nullptr) {
								WriteErrorMessage(data, "Finding type leading space failed. Faulty path: ", index);
								return;
							}
							space++;

							const char* second_space = strchr(space, ' ');

							// if the second space was not found, fail
							if (second_space == nullptr) {
								WriteErrorMessage(data, "Finding type final space failed. Faulty path: ", index);
								return;
							}

							// null terminate 
							char* second_space_mutable = (char*)(second_space);
							*second_space_mutable = '\0';
							data->total_memory += PtrDifference(space, second_space_mutable) + 1;

							file_contents[words[index]] = '\0';
							// find the last new line character in order to speed up processing
							const char* last_new_line = strrchr(file_contents + last_position, '\n');

							// if it failed, set it to the start of the processing block
							last_new_line = (const char*)function::Select<uintptr_t>(last_new_line == nullptr, (uintptr_t)file_contents + last_position, (uintptr_t)last_new_line);

							const char* opening_parenthese = strchr(second_space + 1, '{');
							// if no opening curly brace was found, fail
							if (opening_parenthese == nullptr) {
								WriteErrorMessage(data, "Finding opening curly brace failed. Faulty path: ", index);
								return;
							}

							const char* closing_parenthese = strchr(opening_parenthese + 1, '}');
							// if no closing curly brace was found, fail
							if (closing_parenthese == nullptr) {
								WriteErrorMessage(data, "Finding closing curly brace failed. Faulty path: ", index);
								return;
							}

							auto is_struct_closing_brace = [](const char* closing_brace) {
								const char* brace = closing_brace;
								while (*brace != '\n') {
									brace--;
								}

								brace = function::SkipWhitespace(brace + 1);
								return brace == closing_brace;
							};

							// Closing braces can appear for brace default initialization.
							// The closing brace of the struct is when there are only whitespaces on the same line
							// Keep looking for other closing braces
							while (!is_struct_closing_brace(closing_parenthese)) {
								closing_parenthese = strchr(closing_parenthese + 1, '}');
								if (closing_parenthese == nullptr) {
									WriteErrorMessage(data, "Finding closing curly brace for struct failed. Faulty path: ", index);
									return;
								}
							}
							last_position = PtrDifference(file_contents, closing_parenthese + 1);

							// enum declaration
							const char* enum_ptr = strstr(last_new_line + 1, "enum");
							if (enum_ptr != nullptr) {
								AddEnumDefinition(data, opening_parenthese, closing_parenthese, space, index);
								if (data->success == false) {
									return;
								}
							}
							else {
								// type definition
								const char* struct_ptr = strstr(last_new_line + 1, "struct");
								const char* class_ptr = strstr(last_new_line + 1, "class");

								// if none found, fail
								if (struct_ptr == nullptr && class_ptr == nullptr) {
									WriteErrorMessage(data, "Enum and type definition validation failed, didn't find neither", index);
									return;
								}

								AddTypeDefinition(data, opening_parenthese, closing_parenthese, space, index);
								if (data->success == false) {
									return;
								}
								else if (tag_name != nullptr) {
									const char* end_tag_name = function::SkipCodeIdentifier(tag_name);

									char* new_tag_name = data->thread_memory.buffer + data->thread_memory.size;
									size_t string_size = PtrDifference(tag_name, end_tag_name);

									// Record its tag
									data->thread_memory.size += string_size + 1;
									memcpy(new_tag_name, tag_name, sizeof(char) * string_size);
									new_tag_name[string_size] = '\0';
									data->types[data->types.size - 1].type_tag = new_tag_name;
								}
							}
						}
					}
				}
			}

			data->condition_variable->Notify();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionFieldInfo ExtendedTypeStringToFieldInfo(ReflectionManager* reflection, const char* extended_type_string)
		{
			ReflectionFieldInfo info;

			ResourceIdentifier identifier(extended_type_string);
			bool success = reflection->field_table.TryGetValue(identifier, info);
			ECS_ASSERT(success, "Invalid type_string");

			return info;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void AddEnumDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			const char* opening_parenthese, 
			const char* closing_parenthese,
			const char* name,
			unsigned int index
		)
		{
			ReflectionEnum enum_definition;
			enum_definition.name = name;

			// find next line tokens and exclude the next after the opening paranthese and replace
			// the closing paranthese with \0 in order to stop searching there
			constexpr size_t max_next_line_positions = 1024;
			unsigned int next_line_positions_buffer[max_next_line_positions];
			Stream<unsigned int> next_line_positions = Stream<unsigned int>(next_line_positions_buffer, 0);
			const char* dummy_space = strchr(opening_parenthese, '\n') + 1;

			char* closing_paranthese_mutable = (char*)closing_parenthese;
			*closing_paranthese_mutable = '\0';
			function::FindToken(dummy_space, '\n', next_line_positions);

			// assign the subname stream and assert enough memory to hold the pointers
			uintptr_t ptr = (uintptr_t)data->thread_memory.buffer + data->thread_memory.size;
			uintptr_t before_ptr = ptr;
			ptr = function::align_pointer(ptr, alignof(ReflectionEnum));
			enum_definition.fields = Stream<const char*>((void*)ptr, 0);
			data->thread_memory.size += sizeof(const char*) * next_line_positions.size + alignof(ReflectionEnum);
			data->total_memory += sizeof(const char*) * next_line_positions.size + alignof(ReflectionEnum);
			if (data->thread_memory.size > data->thread_memory.capacity) {
				WriteErrorMessage(data, "Not enough memory to assign enum definition stream", index);
				return;
			}

			for (size_t next_line_index = 0; next_line_index < next_line_positions.size; next_line_index++) {
				const char* current_character = dummy_space + next_line_positions[next_line_index];
				while (!IsTypeCharacter(*current_character)) {
					current_character--;
				}

				char* final_character = (char*)(current_character + 1);
				// null terminate the type
				*final_character = '\0';
				while (IsTypeCharacter(*current_character)) {
					current_character--;
				}

				enum_definition.fields.Add(current_character + 1);
				data->total_memory += PtrDifference(current_character + 1, final_character);
			}

			data->enums.Add(enum_definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void AddTypeDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			const char* opening_parenthese,
			const char* closing_parenthese, 
			const char* name,
			unsigned int index
		)
		{
			ReflectionType type;
			type.name = name;

			// find next line tokens and exclude the next after the opening paranthese and replace
			// the closing paranthese with \0 in order to stop searching there
			constexpr size_t max_next_line_positions = 1024;
			unsigned int next_line_positions_buffer[max_next_line_positions];
			Stream<unsigned int> next_line_positions = Stream<unsigned int>(next_line_positions_buffer, 0);

			opening_parenthese++;
			char* closing_paranthese_mutable = (char*)closing_parenthese;
			*closing_paranthese_mutable = '\0';
			function::FindToken(opening_parenthese, '\n', next_line_positions);

			// semicolon positions will help in getting the field name
			constexpr size_t semicolon_positions_count = 512;
			unsigned int semicolon_positions_buffer[semicolon_positions_count];
			Stream<unsigned int> semicolon_positions = Stream<unsigned int>(semicolon_positions_buffer, 0);
			function::FindToken(opening_parenthese, ';', semicolon_positions);

			// assigning the field stream
			uintptr_t ptr = (uintptr_t)data->thread_memory.buffer + data->thread_memory.size;
			uintptr_t ptr_before = ptr;
			ptr = function::align_pointer(ptr, alignof(ReflectionField));
			type.fields = Stream<ReflectionField>((void*)ptr, 0);
			data->thread_memory.size += sizeof(ReflectionField) * semicolon_positions.size + ptr - ptr_before;
			data->total_memory += sizeof(ReflectionField) * semicolon_positions.size + alignof(ReflectionField);
			if (data->thread_memory.size > data->thread_memory.capacity) {
				WriteErrorMessage(data, "Assigning type field stream failed, insufficient memory.", index);
				return;
			}

			// determining each field
			unsigned short pointer_offset = 0;
			for (size_t index = 0; index < semicolon_positions.size; index++) {
				bool succeded = AddFieldType(data, type, pointer_offset, opening_parenthese + next_line_positions[index], opening_parenthese + semicolon_positions[index]);
				
				if (!succeded) {
					WriteErrorMessage(data, "An error occured during field type determination.", index);
					return;
				}
			}
			data->types.Add({ type, false });
			data->total_memory += sizeof(ReflectionType);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool AddFieldType(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* last_line_character, 
			const char* semicolon_character
		)
		{
			// null terminate the semicolon;
			char* last_field_character = (char*)semicolon_character;
			*last_field_character = '\0';

			// check to see if the field has a default value by looking for =
			// If it does bump it back to the space before it		
			char* equal_character = (char*)strchr(last_line_character, '=');
			if (equal_character != nullptr) {
				semicolon_character = equal_character - 1;
				equal_character--;
				*equal_character = '\0';
			}

			// verify that ECS_REFLECT_OMIT_FIELD is not specified
			const char* omit_field = strstr(last_line_character, STRING(ECS_OMIT_FIELD_REFLECT));
			const char* omit_by_type_field = strstr(last_line_character, STRING(ECS_OMIT_FIELD_BY_TYPE_REFLECT));

			if (omit_field == nullptr && omit_by_type_field == nullptr) {
				const char* current_character = semicolon_character - 1;

				unsigned short embedded_array_size = 0;
				// take into consideration embedded arrays
				if (*current_character == ']') {
					const char* closing_bracket = current_character;
					while (*current_character != '[') {
						current_character--;
					}
					embedded_array_size = function::ConvertCharactersToInt<unsigned short>(Stream<char>((void*)(current_character + 1), PtrDifference(current_character + 1, closing_bracket)));
					char* mutable_ptr = (char*)current_character;
					*mutable_ptr = '\0';
					current_character--;
				}

				// getting the field name
				while (IsTypeCharacter(*current_character)) {
					current_character--;
				}
				current_character++;

				bool success = DeduceFieldType(data, type, pointer_offset, current_character, last_line_character, last_field_character - 1);
				if (success) {
					ReflectionFieldInfo& info = type.fields[type.fields.size - 1].info;
					// Set the default value to false initially
					info.has_default_value = false;

					if (embedded_array_size > 0) {
						info.basic_type_count = embedded_array_size;
						pointer_offset -= info.byte_size;
						info.byte_size *= embedded_array_size;
						pointer_offset += info.byte_size;

						// change the extended type to array
						info.stream_type = ReflectionStreamFieldType::BasicTypeArray;
					}
					// Get the default value if possible
					if (equal_character != nullptr && info.stream_type == ReflectionStreamFieldType::Basic) {
						// it was decremented before to place a '\0'
						equal_character += 2;

						const char* default_value_parse = function::SkipWhitespace(equal_character);

						auto parse_default_value = [&](const char* default_value_parse, char value_to_stop) {
							// Continue until the closing brace
							const char* start_default_value = default_value_parse + 1;

							const char* ending_default_value = start_default_value;
							while (*ending_default_value != value_to_stop && *ending_default_value != '\0' && *ending_default_value != '\n') {
								ending_default_value++;
							}
							if (*ending_default_value == '\0' || *ending_default_value == '\n') {
								// Abort. Can't deduce default value
								return;
							}
							// Use the parse function, any member from the union can be used
							bool parse_success = ParseReflectionBasicFieldType(
								info.basic_type, 
								Stream<char>(start_default_value, ending_default_value - start_default_value),
								&info.default_bool
							);
							info.has_default_value = parse_success;
						};
						// If it is an opening brace, it's ok.
						if (*default_value_parse == '{') {
							parse_default_value(default_value_parse, '}');
						}
						else if (function::IsCodeIdentifierCharacter(*default_value_parse)) {
							// Check to see that it is the constructor type - else it is the actual value
							const char* start_parse_value = default_value_parse;
							while (function::IsCodeIdentifierCharacter(*default_value_parse)) {
								*default_value_parse++;
							}

							// If it is an open paranthese, it is a constructor
							if (*default_value_parse == '(') {
								// Look for the closing paranthese
								parse_default_value(default_value_parse, ')');
							}
							else {
								// If it is a space, tab or semicolon, it means that we skipped the value
								info.has_default_value = ParseReflectionBasicFieldType(
									info.basic_type, 
									Stream<char>(start_parse_value, default_value_parse - start_parse_value),
									&info.default_bool
								);
							}
						}
					}
				}
				return success;
			}
			else {
				if (omit_field != nullptr) {
					while (*omit_field != '(') {
						omit_field++;
					}
					omit_field++;
					while (*omit_field < '0' || *omit_field > '9') {
						omit_field++;
					}
					const char* starting_byte_position = omit_field;

					while (*omit_field >= '0' && *omit_field <= '9') {
						omit_field++;
					}
					Stream<char> temp_stream = Stream<char>(starting_byte_position, PtrDifference(starting_byte_position, omit_field));

					size_t alignment = 8;

					const char* comma = strchr(omit_field, ',');
					if (comma != nullptr) {
						while (*omit_field != ')') {
							omit_field++;
						}
						Stream<char> temp_alignment = Stream<char>(comma + 1, PtrDifference(comma + 1, omit_field));
						alignment = function::ConvertCharactersToInt<size_t>(temp_alignment);
					}

					pointer_offset = function::align_pointer(pointer_offset, alignment);
					size_t byte_size = function::ConvertCharactersToInt<size_t>(temp_stream);
					pointer_offset += byte_size;
				}
				else {
					const char* current_character = omit_by_type_field - 1;
					current_character = function::SkipWhitespace(current_character, -1);
					// Skip * from pointer
					while (*current_character == '*') {
						current_character--;
					}

					const char* end_name = (const char*)current_character;
					current_character = function::SkipCodeIdentifier(current_character, -1) + 1;

					// Account for the null terminator
					size_t write_type_target_count = PtrDifference(current_character, end_name) + 1;
					const char* field_target = data->thread_memory.buffer + data->thread_memory.size;
					memcpy(data->thread_memory.buffer + data->thread_memory.size, current_character, write_type_target_count);
					data->thread_memory.size += write_type_target_count;
					data->thread_memory[data->thread_memory.size] = '\0';
					data->thread_memory.size++;

					data->omit_fields_by_type.AddSafe({ type.name, (unsigned int)type.fields.size, field_target });
				}
			}

			return true;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DeduceFieldType(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* last_line_character,
			const char* last_valid_character
		)
		{
			bool type_from_macro = DeduceFieldTypeFromMacros(data, type, pointer_offset, field_name, last_line_character);
			if (data->error_message->size > 0) {
				return false;
			}
			// if the type was not deduced from macros
			else if (!type_from_macro) {
				bool success = DeduceFieldTypePointer(data, type, pointer_offset, field_name, last_line_character);
				if (data->error_message->size > 0) {
					return false;
				}
				else if (!success) {
					success = DeduceFieldTypeStream(data, type, pointer_offset, field_name, last_line_character);
					if (data->error_message->size > 0) {
						return false;
					}

					if (!success) {
						// if this is not a pointer type, extended type then
						ReflectionField field;
						DeduceFieldTypeExtended(
							data,
							pointer_offset,
							field_name - 2,
							field
						);

						field.name = field_name;
						data->total_memory += strlen(field_name) + 1;
						type.fields.Add(field);
						success = true;
					}
				}
				return success;
			}
			return type_from_macro;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CheckFieldExplicitDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type, 
			unsigned short& pointer_offset,
			const char* last_line_character
		)
		{
			const char* explicit_definition = strstr(last_line_character, STRING(ECS_EXPLICIT_TYPE_REFLECT));
			if (explicit_definition != nullptr) {
				size_t length = strlen(STRING(ECS_EXPLICIT_TYPE_REFLECT));
				unsigned int comma_position_buffer[64];
				Stream<unsigned int> comma_positions = Stream<unsigned int>(comma_position_buffer, 0);
				comma_positions.Add(length);
				function::FindToken(explicit_definition, ',', comma_positions);

				// The definition must have an extended type with a name associated, it must be an even number of preceding elements
				// +1 because we must add the opening parenthese to the stream to be processed
				if (comma_positions.size % 2 == 1) {
					WriteErrorMessage(data, "Invalid number of fields specified in explicit definition", -1);
					return true;
				}

				char* mutable_ptr = (char*)explicit_definition;
				// null terminate every comma and closing bracket;
				for (size_t index = 1; index < comma_positions.size; index++) {
					mutable_ptr[comma_positions[index]] = '\0';
				}
				char* closing_bracket = (char*)strchr(explicit_definition, '}');
				*closing_bracket = '\0';

				// for every pair: extended type and name
				size_t iteration_count = comma_positions.size >> 1;
				for (size_t index = 0; index < iteration_count; index++) {
					// get name length in order to increase the total memory needed;
					size_t extended_type_length = strlen(explicit_definition + comma_positions[index * 2] + 1);
					
					ReflectionField field;
					// if we managed to retrieve the extended info
					GetReflectionFieldInfo(data, explicit_definition + comma_positions[index * 2] + 1, field); 

					const char* field_name = explicit_definition + comma_positions[index * 2 + 1] + 1;
					field.name = field_name;

					pointer_offset = function::align_pointer(pointer_offset, field.info.byte_size / field.info.basic_type_count);

					field.info.pointer_offset = pointer_offset;
					pointer_offset += field.info.byte_size;
					size_t field_name_size = strlen(field_name) + 1;

					data->total_memory += sizeof(char) * field_name_size;
					type.fields.Add(field);
				}
				return true;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DeduceFieldTypeFromMacros(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* last_line_character
		)
		{
			bool explicit_definition = CheckFieldExplicitDefinition(data, type, pointer_offset, last_line_character);
			
			auto pointer_identification = [&](const char* reflect_pointer) {
				// Get arguments; extended type and basic type count means the indirection count
				const char* opening_parenthese = strchr(reflect_pointer, '(');
				if (opening_parenthese == nullptr) {
					WriteErrorMessage(data, "Invalid ECS_POINTER_REFLECT, no opening parenthese", -1);
					return;
				}

				char* closing_parenthese = (char*)strchr(opening_parenthese, ')');
				if (closing_parenthese == nullptr) {
					WriteErrorMessage(data, "Invalid ECS_POINTER_REFLECT, no closing parenthese", -1);
					return;
				}

				// null terminate the comma and closing parenthese
				*closing_parenthese = '\0';

				ReflectionField field;
				GetReflectionFieldInfo(data, opening_parenthese + 1, field);

				if (field.info.stream_type == ReflectionStreamFieldType::Unknown) {
					field.info.stream_type = ReflectionStreamFieldType::Pointer;
					field.info.basic_type_count = 1;
					field.info.byte_size = sizeof(void*);
				}

				pointer_offset = function::align_pointer(pointer_offset, alignof(void*));
				field.info.pointer_offset = pointer_offset;
				pointer_offset += sizeof(void*);

				field.name = field_name;
				data->total_memory += strlen(field_name) + 1;
				type.fields.Add(field);
			};

			auto enum_identification = [&](const char* enum_ptr) {
				ReflectionField field;

				field.name = field_name;
				data->total_memory += strlen(field.name) + 1;

				char* null_ptr = (char*)enum_ptr - 1;
				*null_ptr = '\0';
				enum_ptr -= 2;
				while (!IsTypeCharacter(*enum_ptr)) {
					enum_ptr--;
				}
				while (IsTypeCharacter(*enum_ptr)) {
					enum_ptr--;
				}
				field.definition = enum_ptr + 1;
				data->total_memory += strlen(field.definition) + 1;

				field.info.basic_type = ReflectionBasicFieldType::Enum;
				field.info.stream_type = ReflectionStreamFieldType::Basic;
				field.info.byte_size = 1;
				field.info.basic_type_count = 1;
				field.info.pointer_offset = pointer_offset;

				type.fields.Add(field);
				pointer_offset++;
			};

			auto stream_identification = [&](const char* stream_ptr) {
				const char* initial_ptr = stream_ptr;
				while (*stream_ptr != '(') {
					stream_ptr++;
				}

				stream_ptr++;
				const char* starting_character = stream_ptr;
				
				while (*stream_ptr != ')') {
					stream_ptr++;
				}

				if (stream_ptr == starting_character) {
					WriteErrorMessage(data, "Invalid ECS_STREAM_REFLECT, no byte size", -1);
					return;
				}

				Stream<char> byte_size_stream = Stream<char>((void*)starting_character, PtrDifference(starting_character, stream_ptr));
				ReflectionField field;

				field.info.stream_byte_size = function::ConvertCharactersToInt<unsigned short>(byte_size_stream);
				field.name = field_name;
				data->total_memory += strlen(field_name) + 1;

				while (!IsTypeCharacter(*initial_ptr)) {
					initial_ptr--;
				}
				/*initial_ptr--;
				while (!IsTypeCharacter(*initial_ptr)) {
					initial_ptr--;
				}*/

				char* null_terminate_definition = (char*)initial_ptr - 1;
				*null_terminate_definition = '\0';
				initial_ptr = null_terminate_definition - 1;
				while (IsTypeCharacter(*initial_ptr)) {
					initial_ptr--;
				}
				initial_ptr++;
				
				field.info.basic_type = ReflectionBasicFieldType::UserDefined;
				field.info.basic_type_count = 1;
				field.definition = initial_ptr;

				// Normal stream
				if (strncmp(initial_ptr, STRING(Stream), strlen(STRING(Stream))) == 0) {
					field.info.byte_size = sizeof(Stream<void>);
					field.info.stream_type = ReflectionStreamFieldType::Stream;
				}
				// Capacity stream
				else if (strncmp(initial_ptr, STRING(CapacityStream), strlen(STRING(CapacityStream))) == 0) {
					field.info.byte_size = sizeof(CapacityStream<void>);
					field.info.stream_type = ReflectionStreamFieldType::CapacityStream;
				}
				// Resizable stream
				else if (strncmp(initial_ptr, STRING(ResizableStream), strlen(STRING(ResizableStream))) == 0) {
					field.info.byte_size = sizeof(ResizableStream<char>);
					field.info.stream_type = ReflectionStreamFieldType::ResizableStream;
				}
				else {
					WriteErrorMessage(data, "Invalid ECS_STREAM_REFLECT, no stream identifier", -1);
					return;
				}

				pointer_offset = function::align_pointer(pointer_offset, alignof(Stream<void>));
				field.info.pointer_offset = pointer_offset;
				pointer_offset += field.info.byte_size;

				type.fields.Add(field);
			};

			// explicitely defined
			if (explicit_definition) {
				return true;
			}
			// other macros; ECS_POINTER_REFLECT, ECS_ENUM_REFLECT, ECS_STREAM_REFLECT
			else {
				const char* reflect_pointer = strstr(last_line_character, STRING(ECS_POINTER_REFLECT));

				// if it is defined as pointer
				if (reflect_pointer != 0) {
					pointer_identification(reflect_pointer);
					return true;
				}
				else {
					const char* enum_ptr = strstr(last_line_character, STRING(ECS_ENUM_REFLECT));

					// if this is a an enum
					if (enum_ptr != nullptr) {
						enum_identification(enum_ptr);
						return true;
					}
					else {
						const char* stream_ptr = strstr(last_line_character, STRING(ECS_STREAM_REFLECT));

						// if this is a stream type
						if (stream_ptr != nullptr) {
							stream_identification(stream_ptr);
							return true;
						}
					}
				}
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool DeduceFieldTypePointer(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* last_line_character
		)
		{
			// Test first pointer type
			const char* star_ptr = strchr(last_line_character, '*');
			if (star_ptr != nullptr) {
				ReflectionField field;
				field.name = field_name;
				
				size_t pointer_level = 1;
				char* first_star = (char*)star_ptr;
				star_ptr++;
				while (*star_ptr == '*') {
					pointer_level++;
					star_ptr++;
				}

				*first_star = '\0';
				unsigned short before_pointer_offset = pointer_offset;
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					first_star - 1,
					field
				);

				field.info.basic_type_count = pointer_level;
				field.info.stream_type = ReflectionStreamFieldType::Pointer;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.byte_size = sizeof(void*);
				
				pointer_offset = function::align_pointer(before_pointer_offset, alignof(void*));
				field.info.pointer_offset = pointer_offset;
				pointer_offset += sizeof(void*);

				data->total_memory += strlen(field.name) + 1;
				type.fields.Add(field);
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		// The stream will store the contained element byte size inside the additional flags component
		bool DeduceFieldTypeStream(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset, 
			const char* field_name, 
			const char* new_line_character
		)
		{
			// Test each keyword

			// Start with resizable and capacity, because these contain the stream keyword
			const char* stream_ptr = strstr(new_line_character, STRING(ResizableStream));
			if (stream_ptr != nullptr) {
				ReflectionField field;
				field.name = field_name;

				unsigned short before_pointer_offset = pointer_offset;
				char* left_bracket = (char*)strchr(stream_ptr, '<');
				if (left_bracket == nullptr) {
					WriteErrorMessage(data, "Incorrect Stream, missing <.", -1);
					return true;
				}

				char* comma = (char*)strchr(left_bracket, ',');

				// Make the left bracket \0 because otherwise it will continue to get parsed
				left_bracket[0] = '\0';
				left_bracket[-1] = '\0';
				*comma = '\0';
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					comma - 1,
					field
				);

				pointer_offset = function::align_pointer(before_pointer_offset, alignof(ResizableStream<char>));
				field.info.pointer_offset = pointer_offset;
				field.info.stream_type = ReflectionStreamFieldType::ResizableStream;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.byte_size = sizeof(ResizableStream<char>);
				pointer_offset += sizeof(ResizableStream<char>);

				data->total_memory += strlen(field.name) + 1;
				type.fields.Add(field);
				return true;
			}

			stream_ptr = strstr(new_line_character, STRING(CapacityStream));
			if (stream_ptr != nullptr) {
				ReflectionField field;
				field.name = field_name;

				unsigned short before_pointer_offset = pointer_offset;
				char* left_bracket = (char*)strchr(stream_ptr, '<');
				if (left_bracket == nullptr) {
					WriteErrorMessage(data, "Incorrect CapacityStream, missing <.", -1);
					return true;
				}

				char* right_bracket = (char*)strchr(left_bracket, '>');
				if (right_bracket == nullptr) {
					WriteErrorMessage(data, "Incorrent CapacityStream, missing >.", -1);
					return true;
				}

				// Make the left bracket \0 because otherwise it will continue to get parsed
				left_bracket[0] = '\0';
				left_bracket[-1] = '\0';
				*right_bracket = '\0';
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					right_bracket - 1,
					field
				);

				pointer_offset = function::align_pointer(before_pointer_offset, alignof(CapacityStream<void>));
				field.info.pointer_offset = pointer_offset;
				field.info.stream_type = ReflectionStreamFieldType::CapacityStream;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.byte_size = sizeof(CapacityStream<void>);
				pointer_offset += sizeof(CapacityStream<void>);

				data->total_memory += strlen(field.name) + 1;
				type.fields.Add(field);
				return true;
			}

			stream_ptr = strstr(new_line_character, STRING(Stream));
			if (stream_ptr != nullptr) {
				ReflectionField field;
				field.name = field_name;

				unsigned short before_pointer_offset = pointer_offset;
				char* left_bracket = (char*)strchr(stream_ptr, '<');
				if (left_bracket == nullptr) {
					WriteErrorMessage(data, "Incorrect Stream, missing <.", -1);
					return true;
				}

				char* right_bracket = (char*)strchr(left_bracket, '>');
				if (right_bracket == nullptr) {
					WriteErrorMessage(data, "Incorrent Stream, missing >.", -1);
					return true;
				}

				// Make the left bracket and the first character before it \0 because otherwise it will continue to get parsed
				left_bracket[0] = '\0';
				left_bracket[-1] = '\0';
				*right_bracket = '\0';
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					right_bracket - 1,
					field
				);

				pointer_offset = function::align_pointer(before_pointer_offset, alignof(Stream<void>));
				field.info.pointer_offset = pointer_offset;
				field.info.stream_type = ReflectionStreamFieldType::Stream;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.byte_size = sizeof(Stream<void>);
				pointer_offset += sizeof(Stream<void>);

				data->total_memory += strlen(field.name) + 1;
				type.fields.Add(field);
				return true;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void DeduceFieldTypeExtended(
			ReflectionManagerParseStructuresThreadTaskData* data,
			unsigned short& pointer_offset,
			const char* last_type_character, 
			ReflectionField& field
		)
		{
			char* current_character = (char*)last_type_character;
			current_character[1] = '\0';
			while (IsTypeCharacter(*current_character)) {
				current_character--;
			}
			char* first_space = current_character;
			current_character--;
			while (IsTypeCharacter(*current_character)) {
				current_character--;
			}

			auto processing = [&](const char* basic_type) {
				size_t type_size = strlen(basic_type);
				GetReflectionFieldInfo(data, basic_type, field);
				if (field.info.basic_type != ReflectionBasicFieldType::UserDefined && field.info.basic_type != ReflectionBasicFieldType::Unknown) {
					unsigned int component_count = BasicTypeComponentCount(field.info.basic_type);
					pointer_offset = function::align_pointer(pointer_offset, field.info.byte_size / component_count);
					field.info.pointer_offset = pointer_offset;
					pointer_offset += field.info.byte_size;
				}
			};

			// if there is no second word to process
			if (PtrDifference(current_character, first_space) == 1) {
				processing(first_space + 1);
			}
			// if there are two words
			else {
				// if there is a const, ignore it
				const char* const_ptr = strstr(current_character, "const");

				if (const_ptr != nullptr) {
					processing(first_space + 1);
				}
				// not a const type, possibly unsigned something, so process it normaly
				else {
					processing(current_character + 1);
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionFieldInfo(ReflectionManagerParseStructuresThreadTaskData* data, const char* basic_type, ReflectionField& field)
		{
			const char* field_name = field.name;
			field = GetReflectionFieldInfo(data->field_table, basic_type);
			field.name = field_name;
			if (field.info.stream_type == ReflectionStreamFieldType::Unknown || field.info.basic_type == ReflectionBasicFieldType::UserDefined) {
				data->total_memory += strlen(basic_type) + 1;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField GetReflectionFieldInfo(const ReflectionFieldTable* reflection_field_table, const char* basic_type)
		{
			ReflectionField field;

			ResourceIdentifier identifier = ResourceIdentifier(basic_type);

			bool success = reflection_field_table->TryGetValue(identifier, field.info);
			field.definition = basic_type;
			if (!success) {
				field.info.stream_type = ReflectionStreamFieldType::Unknown;
				field.info.basic_type = ReflectionBasicFieldType::UserDefined;
			}
			else {
				// Set to global strings the definition in order to not occupy extra memory

#define CASE(type, string) case ReflectionBasicFieldType::type: field.definition = STRING(string); break;
#define CASE234(type, string) CASE(type##2, string##2); CASE(type##3, string##3); CASE(type##4, string##4);
#define CASE1234(type, string) CASE(type, string); CASE234(type, string);

				switch (field.info.basic_type) {
					CASE1234(Bool, bool);
					CASE234(Char, char);
					CASE234(UChar, uchar);
					CASE234(Short, short);
					CASE234(UShort, ushort);
					CASE234(Int, int);
					CASE234(UInt, uint);
					CASE234(Long, long);
					CASE234(ULong, ulong);
					CASE(Int8, int8_t);
					CASE(UInt8, uint8_t);
					CASE(Int16, int16_t);
					CASE(UInt16, uint16_t);
					CASE(Int32, int32_t);
					CASE(UInt32, uint32_t);
					CASE(Int64, int64_t);
					CASE(UInt64, uint64_t);
					CASE1234(Float, float);
					CASE1234(Double, double);
				}
			}

			return field;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField GetReflectionFieldInfo(const ReflectionManager* reflection, const char* basic_type)
		{
			return GetReflectionFieldInfo(&reflection->field_table, basic_type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionField GetReflectionFieldInfoStream(const ReflectionManager* reflection, const char* basic_type, ReflectionStreamFieldType stream_type)
		{
			ReflectionField field;

			field = GetReflectionFieldInfo(reflection, basic_type);
			field.info.stream_byte_size = field.info.byte_size;
			field.info.stream_type = stream_type;
			if (stream_type == ReflectionStreamFieldType::Stream) {
				field.info.byte_size = sizeof(Stream<void>);
			}
			else if (stream_type == ReflectionStreamFieldType::CapacityStream) {
				field.info.byte_size = sizeof(CapacityStream<void>);
			}
			else if (stream_type == ReflectionStreamFieldType::ResizableStream) {
				field.info.byte_size = sizeof(ResizableStream<char>);
			}
			else if (stream_type == ReflectionStreamFieldType::Pointer) {
				field.info.byte_size = sizeof(void*);
			}

			return field;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool HasReflectStructures(const wchar_t* path)
		{
			ECS_FILE_HANDLE file_handle = 0;

			const size_t max_line_characters = 4096;
			char line_characters[max_line_characters];
			// open the file from the beginning
			ECS_FILE_STATUS_FLAGS status_flags = OpenFile(path, &file_handle, ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_READ_ONLY);
			if (status_flags == ECS_FILE_STATUS_OK) {
				// read the first line in order to check if there is something to be reflected
				bool success = ReadFile(file_handle, { line_characters, max_line_characters });
				bool has_reflect = false;
				if (success) {
					has_reflect = strstr(line_characters, STRING(ECS_REFLECT)) != nullptr;
				}
				CloseFile(file_handle);
				return has_reflect;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegral(ReflectionBasicFieldType type)
		{
			// Use the negation - if it is different than all the other types
			return type != ReflectionBasicFieldType::Bool && type != ReflectionBasicFieldType::Bool2 && type != ReflectionBasicFieldType::Bool3 &&
				type != ReflectionBasicFieldType::Bool4 && type != ReflectionBasicFieldType::Double && type != ReflectionBasicFieldType::Double2 &&
				type != ReflectionBasicFieldType::Double3 && type != ReflectionBasicFieldType::Double4 && type != ReflectionBasicFieldType::Float &&
				type != ReflectionBasicFieldType::Float2 && type != ReflectionBasicFieldType::Float3 && type != ReflectionBasicFieldType::Float4 &&
				type != ReflectionBasicFieldType::Enum && type != ReflectionBasicFieldType::UserDefined && type != ReflectionBasicFieldType::Unknown &&
				type != ReflectionBasicFieldType::Wchar_t;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegralSingleComponent(ReflectionBasicFieldType type) {
			return type == ReflectionBasicFieldType::Int8 || type == ReflectionBasicFieldType::UInt8 ||
				type == ReflectionBasicFieldType::Int16 || type == ReflectionBasicFieldType::UInt16 ||
				type == ReflectionBasicFieldType::Int32 || type == ReflectionBasicFieldType::UInt32 ||
				type == ReflectionBasicFieldType::Int64 || type == ReflectionBasicFieldType::UInt64;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool IsIntegralMultiComponent(ReflectionBasicFieldType type)
		{
			return IsIntegral(type) && !IsIntegralSingleComponent(type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned char BasicTypeComponentCount(ReflectionBasicFieldType type)
		{
			if (type == ReflectionBasicFieldType::Bool || type == ReflectionBasicFieldType::Double || type == ReflectionBasicFieldType::Enum ||
				type == ReflectionBasicFieldType::Float || type == ReflectionBasicFieldType::Int8 || type == ReflectionBasicFieldType::UInt8 ||
				type == ReflectionBasicFieldType::Int16 || type == ReflectionBasicFieldType::UInt16 || type == ReflectionBasicFieldType::Int32 ||
				type == ReflectionBasicFieldType::UInt32 || type == ReflectionBasicFieldType::Int64 || type == ReflectionBasicFieldType::UInt64 ||
				type == ReflectionBasicFieldType::Wchar_t) {
				return 1;
			}
			else if (type == ReflectionBasicFieldType::Bool2 || type == ReflectionBasicFieldType::Double2 || type == ReflectionBasicFieldType::Float2 ||
				type == ReflectionBasicFieldType::Char2 || type == ReflectionBasicFieldType::UChar2 || type == ReflectionBasicFieldType::Short2 ||
				type == ReflectionBasicFieldType::UShort2 || type == ReflectionBasicFieldType::Int2 || type == ReflectionBasicFieldType::UInt2 ||
				type == ReflectionBasicFieldType::Long2 || type == ReflectionBasicFieldType::ULong2) {
				return 2;
			}
			else if (type == ReflectionBasicFieldType::Bool3 || type == ReflectionBasicFieldType::Double3 || type == ReflectionBasicFieldType::Float3 ||
				type == ReflectionBasicFieldType::Char3 || type == ReflectionBasicFieldType::UChar3 || type == ReflectionBasicFieldType::Short3 ||
				type == ReflectionBasicFieldType::UShort3 || type == ReflectionBasicFieldType::Int3 || type == ReflectionBasicFieldType::UInt3 ||
				type == ReflectionBasicFieldType::Long3 || type == ReflectionBasicFieldType::ULong3) {
				return 3;
			}
			else if (type == ReflectionBasicFieldType::Bool4 || type == ReflectionBasicFieldType::Double4 || type == ReflectionBasicFieldType::Float4 ||
				type == ReflectionBasicFieldType::Char4 || type == ReflectionBasicFieldType::UChar4 || type == ReflectionBasicFieldType::Short4 ||
				type == ReflectionBasicFieldType::UShort4 || type == ReflectionBasicFieldType::Int4 || type == ReflectionBasicFieldType::UInt4 ||
				type == ReflectionBasicFieldType::Long4 || type == ReflectionBasicFieldType::ULong4) {
				return 4;
			}
			else {
				return 0;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* GetReflectionFieldStreamBuffer(ReflectionFieldInfo info, const void* data)
		{
			const void* stream_field = function::OffsetPointer(data, info.pointer_offset);
			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				return stream->buffer;
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				return stream->buffer;
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<char>* stream = (ResizableStream<char>*)stream_field;
				return stream->buffer;
			}
			return nullptr;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldStreamSize(ReflectionFieldInfo info, const void* data)
		{
			const void* stream_field = function::OffsetPointer(data, info.pointer_offset);

			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				return stream->size;
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				return stream->size;
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<char>* stream = (ResizableStream<char>*)stream_field;
				return stream->size;
			}
			return 0;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<void> GetReflectionFieldStreamVoid(ReflectionFieldInfo info, const void* data)
		{
			const void* stream_field = function::OffsetPointer(data, info.pointer_offset);
			if (info.stream_type == ReflectionStreamFieldType::Stream) {
				Stream<void>* stream = (Stream<void>*)stream_field;
				return *stream;
			}
			else if (info.stream_type == ReflectionStreamFieldType::CapacityStream) {
				CapacityStream<void>* stream = (CapacityStream<void>*)stream_field;
				return { stream->buffer, stream->size };
			}
			else if (info.stream_type == ReflectionStreamFieldType::ResizableStream) {
				ResizableStream<char>* stream = (ResizableStream<char>*)stream_field;
				return { stream->buffer, stream->size };
			}
			return { nullptr, 0 };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionFieldStreamElementByteSize(ReflectionFieldInfo info)
		{
			return info.stream_byte_size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned char GetReflectionFieldPointerIndirection(ReflectionFieldInfo info)
		{
			ECS_ASSERT(info.stream_type == ReflectionStreamFieldType::Pointer);
			return info.basic_type_count;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
