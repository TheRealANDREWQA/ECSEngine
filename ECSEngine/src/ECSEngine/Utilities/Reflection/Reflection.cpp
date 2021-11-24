#include "ecspch.h"
#include "Reflection.h"
#include "../File.h"

namespace ECSEngine {

	namespace Reflection {

		ECS_CONTAINERS;

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionManager::ReflectionManager(MemoryManager* allocator, size_t type_count, size_t enum_count) : folders(allocator, 0)
		{
			InitializeFieldTable();
			InitializeTypeTable(type_count);
			InitializeEnumTable(enum_count);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearTypeDefinitions()
		{
			Stream<ReflectionType> type_defs = type_definitions.GetValueStream();
			for (size_t index = 0; index < type_defs.size;  index++) {
				if (type_definitions.IsItemAt(index)) {
					folders.allocator->Deallocate(type_defs[index].name);
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

			void* allocation = folders.allocator->Allocate(total_memory);
			uintptr_t ptr = (uintptr_t)allocation;

			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].types.size; index++) {
					ReflectionType type;
					type.folder_hierarchy_index = folder_index;

					size_t type_name_size = strlen(data[data_index].types[index].name) + 1;
					char* name_ptr = (char*)ptr;
					memcpy(name_ptr, data[data_index].types[index].name, type_name_size * sizeof(char));
					ptr += type_name_size * sizeof(char);

					type.name = name_ptr;
					ptr = function::align_pointer(ptr, alignof(ReflectionType));
					type.fields.InitializeFromBuffer(ptr, data[data_index].types[index].fields.size);

					for (size_t field_index = 0; field_index < data[data_index].types[index].fields.size; field_index++) {
						size_t field_size = strlen(data[data_index].types[index].fields[field_index].name) + 1;
						char* field_ptr = (char*)ptr;
						memcpy(field_ptr, data[data_index].types[index].fields[field_index].name, sizeof(char) * field_size);
						ptr += sizeof(char) * field_size;

						//function::Capitalize(field_ptr);
						type.fields[field_index].name = field_ptr;
						type.fields[field_index].info = data[data_index].types[index].fields[field_index].info;

						if (data[data_index].types[index].fields[field_index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
							size_t definition_size = strlen(data[data_index].types[index].fields[field_index].definition) + 1;
							char* definition_ptr = (char*)ptr;
							memcpy(definition_ptr, data[data_index].types[index].fields[field_index].definition, sizeof(char) * definition_size);
							ptr += sizeof(char) * field_size;
						}
						else {
							type.fields[field_index].definition = data[data_index].types[index].fields[field_index].definition;
						}
					}

					ECS_RESOURCE_IDENTIFIER_WITH_HASH(type.name, ReflectionStringHashFunction);
					ECS_ASSERT(!type_definitions.Insert(hash, type, identifier));
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

					ECS_RESOURCE_IDENTIFIER_WITH_HASH(enum_.name, ReflectionStringHashFunction);
					ECS_ASSERT(!enum_definitions.Insert(hash, enum_, identifier));
				}
			}

			size_t difference = PtrDifference(allocation, (void*)ptr);
			ECS_ASSERT(difference <= total_memory);

			folders[folder_index].allocated_buffer = allocation;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::ClearEnumDefinitions() {
			Stream<ReflectionEnum> enum_defs = enum_definitions.GetValueStream();
			for (size_t index = 0; index < enum_defs.size; index++) {
				if (enum_definitions.IsItemAt(index)) {
					folders.allocator->Deallocate(enum_defs[index].name);
				}
			}
			enum_definitions.Clear();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int ReflectionManager::CreateFolderHierarchy(const wchar_t* root) {
			folders.ReserveNewElements(1);
			folders[folders.size++] = { ReflectionFolderHierarchy(folders.allocator, root), nullptr };

			constexpr wchar_t* c_file_extensions[] = {
				L".cpp",
				L".hpp",
				L".c",
				L".h"
			};
			folders[folders.size - 1].hierarchy.AddFilterFiles("C++ files", Stream<const wchar_t*>((void*)c_file_extensions, 4));
			return folders.size - 1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::DeallocateThreadTaskData(ReflectionManagerParseStructuresThreadTaskData& data)
		{
			free(data.thread_memory.buffer);
			folders.allocator->Deallocate(data.types.buffer);
			folders.allocator->Deallocate(data.enums.buffer);
			folders.allocator->Deallocate(data.paths.buffer);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::FreeFolderHierarchy(unsigned int folder_index)
		{
			Stream<ReflectionType> type_defs = type_definitions.GetValueStream();
			Stream<ReflectionEnum> enum_defs = enum_definitions.GetValueStream();

			for (int64_t index = 0; index < type_defs.size; index++) {
				if (type_definitions.IsItemAt(index)) {
					if (type_defs[index].folder_hierarchy_index == folder_index) {
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

			folders.allocator->Deallocate(folders[folder_index].allocated_buffer);
			folders[folder_index].hierarchy.FreeAllMemory();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionType ReflectionManager::GetType(const char* name) const
		{
			size_t name_length = strlen(name);
			ResourceIdentifier identifier = ResourceIdentifier(name, name_length);
			unsigned int hash = ReflectionStringHashFunction::Hash(name, name_length);
			return type_definitions.GetValue(hash, identifier);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionEnum ReflectionManager::GetEnum(const char* name) const {
			size_t name_length = strlen(name);
			ResourceIdentifier identifier = ResourceIdentifier(name, name_length);
			unsigned int hash = ReflectionStringHashFunction::Hash(name, name_length);
			return enum_definitions.GetValue(hash, identifier);
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

		bool ReflectionManager::TryGetType(const char* name, ReflectionType& type) const
		{
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, ReflectionStringHashFunction);
			return type_definitions.TryGetValue(hash, identifier, type);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::TryGetEnum(const char* name, ReflectionEnum& enum_) const {
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, ReflectionStringHashFunction);
			return enum_definitions.TryGetValue(hash, identifier, enum_);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeFieldTable()
		{
			constexpr size_t table_size = 256;
			void* allocation = folders.allocator->Allocate(ReflectionFieldTable::MemoryOf(table_size));
			field_table.InitializeFromBuffer(allocation, table_size);

			// Initialize all values, helped by macros
			ResourceIdentifier identifier;
#define BASIC_TYPE(type, basic_type, stream_type) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionStringHashFunction::Hash(identifier.ptr, identifier.size), ReflectionFieldInfo(basic_type, stream_type, sizeof(type), 1), identifier);
#define INT_TYPE(type, val) BASIC_TYPE(type, ReflectionBasicFieldType::val, ReflectionStreamFieldType::Basic)
#define COMPLEX_TYPE(type, basic_type, stream_type, byte_size) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionStringHashFunction::Hash(identifier.ptr, identifier.size), ReflectionFieldInfo(basic_type, stream_type, byte_size, 1), identifier);

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
			void* allocation = folders.allocator->Allocate(ReflectionTypeTable::MemoryOf(count));
			type_definitions.InitializeFromBuffer(allocation, count);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManager::InitializeEnumTable(size_t count)
		{
			void* allocation = folders.allocator->Allocate(ReflectionEnumTable::MemoryOf(count));
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
			constexpr size_t max_types = 64;
			constexpr size_t max_enums = 32;
			constexpr size_t path_size = 128;
			void* type_allocation = folders.allocator->Allocate(CapacityStream<ReflectionType>::MemoryOf(max_types));
			void* enum_allocation = folders.allocator->Allocate(CapacityStream<ReflectionEnum>::MemoryOf(max_enums));
			void* path_allocation = folders.allocator->Allocate(sizeof(const wchar_t*) * path_count);

			data.enums.InitializeFromBuffer(enum_allocation, 0, max_enums);
			data.types.InitializeFromBuffer(type_allocation, 0, max_types);
			data.paths.InitializeFromBuffer(path_allocation, path_count);

			void* thread_allocation = malloc(thread_memory);
			data.thread_memory.InitializeFromBuffer(thread_allocation, 0, thread_memory);
			data.field_table = &field_table;
			data.success = true;
			data.total_memory = 0;
			data.error_message_lock.unlock();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(const wchar_t* root, CapacityStream<char>* error_message) {
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(folders[index].hierarchy.root, root) == 0) {
					return ProcessFolderHierarchy(index, error_message);
				}
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(
			const wchar_t* root, 
			TaskManager* task_manager, 
			CapacityStream<char>* error_message
		) {
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(folders[index].hierarchy.root, root) == 0) {
					return ProcessFolderHierarchy(index, task_manager, error_message);
				}
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(unsigned int index, CapacityStream<char>* error_message) {
			ReflectionManagerParseStructuresThreadTaskData thread_data;

			constexpr size_t thread_memory = 20'000'000;
			// Paths that need to be searched will not be assigned here
			ECS_TEMP_ASCII_STRING(temp_error_message, 1024);
			if (error_message == nullptr) {
				error_message = &temp_error_message;
			}
			InitializeParseThreadTaskData(thread_memory, folders[index].hierarchy.filtered_files[0].files.size, thread_data, error_message);

			ConditionVariable condition_variable;
			thread_data.condition_variable = &condition_variable;
			// Assigning paths
			for (size_t path_index = 0; path_index < folders[index].hierarchy.filtered_files[0].files.size; path_index++) {
				thread_data.paths[path_index] = folders[index].hierarchy.filtered_files[0].files[path_index];
			}

			ReflectionManagerParseThreadTask(0, nullptr, &thread_data);
			if (thread_data.success == true) {
				BindApprovedData(&thread_data, 1, index);
			}

			DeallocateThreadTaskData(thread_data);
			return thread_data.success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::ProcessFolderHierarchy(
			unsigned int folder_index, 
			TaskManager* task_manager, 
			CapacityStream<char>* error_message
		) {
			unsigned int thread_count = task_manager->GetThreadCount();

			// Preparsing the files in order to have thread act only on files that actually need to process
			// Otherwise unequal distribution of files will result in poor multithreaded performance

			unsigned int files_count = folders[folder_index].hierarchy.filtered_files[0].files.size;
			unsigned int* path_indices_buffer = (unsigned int*)folders.allocator->Allocate(sizeof(unsigned int) * files_count, alignof(unsigned int));
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

				// Launch the task
				ThreadTask task;
				task.function = ReflectionManagerHasReflectStructuresThreadTask;
				task.data = reflect_thread_data + thread_index;
				//ReflectionManagerHasReflectStructuresThreadTask(0, task_manager->m_world, reflect_thread_data + thread_index);
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
					parse_thread_data[thread_index].paths[path_index - parse_thread_start] = folders[folder_index].hierarchy.filtered_files[0].files[path_indices[path_index]];
				}
				parse_thread_start += thread_current_paths;
			}

			condition_variable.Reset();

			// Push thread execution
			for (size_t thread_index = 0; thread_index < parse_thread_count; thread_index++) {
				ThreadTask task;
				task.function = ReflectionManagerParseThreadTask;
				task.data = parse_thread_data + thread_index;
				task_manager->AddDynamicTaskAndWake(task);
				//ReflectionManagerParseThreadTask(0, task_manager->m_world, parse_thread_data + thread_index);
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
			folders.allocator->Deallocate(path_indices_buffer);

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
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(folders[index].hierarchy.root, root) == 0) {
					return RemoveFolderHierarchy(index);
				}
			}
			// error if not found
			ECS_ASSERT(false);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(unsigned int folder_index, CapacityStream<char>* error_message)
		{
			FreeFolderHierarchy(folder_index);
			folders[folder_index].hierarchy.Recreate();
			return ProcessFolderHierarchy(folder_index, error_message);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::UpdateFolderHierarchy(const wchar_t* root, CapacityStream<char>* error_message) {
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(root, folders[index].hierarchy.root) == 0) {
					return UpdateFolderHierarchy(index, error_message);
				}
			}
			// Fail if it was not found previously
			ECS_ASSERT(false);
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionFolderHierarchy::ReflectionFolderHierarchy(MemoryManager* allocator) : folders(allocator, 0), filtered_files(allocator, 0), root(nullptr) {}
		ReflectionFolderHierarchy::ReflectionFolderHierarchy(MemoryManager* allocator, const wchar_t* path) : folders(allocator, 0), filtered_files(allocator, 0) {
			CreateFromPath(path);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionFolderHierarchy::AddFilterFiles(const char* filter_name, containers::Stream<const wchar_t*> extensions)
		{
			ReflectionFilteredFiles filter;
			size_t filter_name_size = strlen(filter_name) + 1;
			char* _filter_name = (char*)folders.allocator->Allocate(sizeof(char) * filter_name_size, alignof(char));
			filter.filter_name = _filter_name;
			memcpy(_filter_name, filter_name, filter_name_size + 1);

			size_t extension_total_size = 0;
			size_t extension_sizes[256];

			ECS_ASSERT(extensions.size <= 256);
			for (size_t index = 0; index < extensions.size; index++) {
				extension_sizes[index] = wcslen(extensions[index]) + 1;
				extension_total_size += extension_sizes[index];
			}
			void* batched_allocation = folders.allocator->Allocate(sizeof(wchar_t) * extension_total_size + sizeof(const wchar_t*) * extensions.size);
			uintptr_t extension_ptr = (uintptr_t)batched_allocation;
			filter.extensions.InitializeFromBuffer(batched_allocation, extensions.size);
			extension_ptr += sizeof(const wchar_t*) * extensions.size;

			for (size_t index = 0; index < extensions.size; index++) {
				wchar_t* _extension = (wchar_t*)extension_ptr;
				memcpy(_extension, extensions[index], sizeof(wchar_t) * extension_sizes[index]);
				filter.extensions[index] = _extension;
				extension_ptr += sizeof(wchar_t) * extension_sizes[index];
			}

			filter.files = ResizableStream<const wchar_t*, MemoryManager>(folders.allocator, 0);

			struct ForEachData {
				ReflectionFolderHierarchy* hierarchy;
				ReflectionFilteredFiles* filter;
			};

			ForEachData for_each_data;
			for_each_data.filter = &filter;
			for_each_data.hierarchy = this;

			for (size_t index = 0; index < folders.size; index++) {
				ForEachFileInDirectoryWithExtension(folders[index], filter.extensions, &for_each_data, [](const std::filesystem::path& path, void* _data) {
					ForEachData* data = (ForEachData*)_data;
					const wchar_t* c_path = path.c_str();
					size_t path_length = wcslen(c_path) + 1;
					void* allocation = data->hierarchy->folders.allocator->Allocate(path_length * sizeof(wchar_t), alignof(wchar_t));
					memcpy(allocation, c_path, sizeof(wchar_t) * path_length);
					data->filter->files.Add((const wchar_t*)allocation);

					return true;
				});
			}

			ForEachFileInDirectoryWithExtension(root, filter.extensions, &for_each_data, [](const std::filesystem::path& path, void* _data) {
				ForEachData* data = (ForEachData*)_data;
				const wchar_t* c_path = path.c_str();
				size_t path_length = wcslen(c_path) + 1;
				void* allocation = data->hierarchy->folders.allocator->Allocate(path_length * sizeof(wchar_t), alignof(wchar_t));
				memcpy(allocation, c_path, sizeof(wchar_t) * path_length);
				data->filter->files.Add((const wchar_t*)allocation);

				return true;
			});

			filtered_files.Add(filter);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionFolderHierarchy::FreeAllMemory() {
			for (size_t index = 0; index < filtered_files.size; index++) {
				RemoveFilterFiles(index);
			}

			for (size_t index = 0; index < folders.size; index++) {
				folders.allocator->Deallocate(folders[index]);
			}
			folders.FreeBuffer();
			filtered_files.FreeBuffer();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionFolderHierarchy::Recreate()
		{
			// Copy filtered file extensions into a buffer
			size_t total_memory = 0;
			for (size_t index = 0; index < filtered_files.size; index++) {
				// +1 for null terminator and +1 for alignment
				total_memory += strlen(filtered_files[index].filter_name) + 1;
				total_memory += sizeof(const wchar_t*) * filtered_files[index].extensions.size + 7;
				for (size_t subindex = 0; subindex < filtered_files[index].extensions.size; subindex++) {
					total_memory += sizeof(wchar_t) * (wcslen(filtered_files[index].extensions[subindex]) + 1);
				}
			}
			total_memory += sizeof(ReflectionFilteredFiles) * filtered_files.size;

			void* temp_allocation = folders.allocator->Allocate(total_memory);

			// recreate strings
			uintptr_t ptr = (uintptr_t)temp_allocation;
			Stream<ReflectionFilteredFiles> filtered_stream = Stream<ReflectionFilteredFiles>((void*)ptr, filtered_files.size);
			ptr += sizeof(ReflectionFilteredFiles) * filtered_files.size;

			for (size_t index = 0; index < filtered_files.size; index++) {
				size_t name_size = strlen(filtered_files[index].filter_name);
				memcpy((void*)ptr, filtered_files[index].filter_name, name_size + 1);
				filtered_stream[index].filter_name = (const char*)ptr;
				ptr += name_size + 1;

				ptr = function::align_pointer(ptr, alignof(const wchar_t*));
				filtered_stream[index].extensions.InitializeFromBuffer((void*)ptr, filtered_files[index].extensions.size);
				ptr += sizeof(const wchar_t*) * filtered_files[index].extensions.size;

				for (size_t subindex = 0; subindex < filtered_files[index].extensions.size; subindex++) {
					size_t extension_size = wcslen(filtered_files[index].extensions[subindex]) + 1;
					memcpy((void*)ptr, filtered_files[index].extensions[subindex], sizeof(wchar_t) * extension_size);
					filtered_stream[index].extensions[subindex] = (const wchar_t*)ptr;
					ptr += sizeof(wchar_t) * extension_size;
				}
			}

			FreeAllMemory();
			CreateFromPath(root);
			for (size_t index = 0; index < filtered_stream.size; index++) {
				AddFilterFiles(filtered_stream[index].filter_name, filtered_stream[index].extensions);
			}

			folders.allocator->Deallocate(temp_allocation);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionFolderHierarchy::CreateFromPath(const wchar_t* path)
		{
			size_t root_size = wcslen(path) + 1;
			void* allocation = folders.allocator->Allocate(root_size * sizeof(wchar_t), alignof(wchar_t));
			memcpy(allocation, path, root_size * sizeof(wchar_t));
			root = (const wchar_t*)allocation;
			function::GetRecursiveDirectories(folders.allocator, path, folders);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionFolderHierarchy::RemoveFilterFiles(const char* filter_name)
		{
			for (size_t index = 0; index < filtered_files.size; index++) {
				if (strcmp(filtered_files[index].filter_name, filter_name) == 0) {
					RemoveFilterFiles(index);
					break;
				}
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionFolderHierarchy::RemoveFilterFiles(unsigned int index) {
			folders.allocator->Deallocate(filtered_files[index].filter_name);
			folders.allocator->Deallocate(filtered_files[index].extensions.buffer);
			for (size_t subindex = 0; subindex < filtered_files[index].files.size; subindex++) {
				folders.allocator->Deallocate(filtered_files[index].files[subindex]);
			}
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

		size_t PtrDifference(const void* ptr1, const void* ptr2)
		{
			return (uintptr_t)ptr2 - (uintptr_t)ptr1;
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
				bool has_reflect = HasReflectStructures(data->reflection_manager->folders[data->folder_index].hierarchy.filtered_files[0].files[path_index]);
				if (has_reflect) {
					data->valid_paths->Add(path_index);
				}
			}

			data->semaphore->Enter();
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* _data) {
			ReflectionManagerParseStructuresThreadTaskData* data = (ReflectionManagerParseStructuresThreadTaskData*)_data;

			constexpr size_t word_max_count = 1024;
			constexpr size_t line_character_count = 4096;
			char line_characters[line_character_count];
			unsigned int words_buffer[word_max_count];
			Stream<unsigned int> words = Stream<unsigned int>(words_buffer, 0);

			size_t ecs_reflect_size = strlen(STRING(ECS_REFLECT));

			// search every path
			for (size_t index = 0; index < data->paths.size; index++) {
				// open the file from the beginning
				std::ifstream stream(data->paths[index], std::ios::beg | std::ios::in);

				// if access was granted
				if (stream.good()) {
					// read the first line in order to check if there is something to be reflected
					stream.getline(line_characters, line_character_count);
				
					// if there is something to be reflected
					if (strstr(line_characters, STRING(ECS_REFLECT)) != nullptr) {
						// reset words stream
						words.size = 0;

						size_t before_size = data->thread_memory.size;
						// read the whole text data by reading a very large number of bytes
						constexpr size_t read_whole_file_byte_size = 1'000'000'000;
						stream.read(data->thread_memory.buffer + data->thread_memory.size, read_whole_file_byte_size);
						data->thread_memory.size += stream.gcount();

						// if there's not enough memory, fail
						if (data->thread_memory.size > data->thread_memory.capacity) {
							WriteErrorMessage(data, "Not enough memory to read file contents. Faulty path: ", index);
							return;
						}

						// search for more ECS_REFLECTs 
						function::FindToken(data->thread_memory.buffer + before_size, STRING(ECS_REFLECT), words);

						size_t last_position = 0;
						for (size_t index = 0; index < words.size; index++) {
							// get the type name
							const char* space = strchr(data->thread_memory.buffer + before_size + words[index] + ecs_reflect_size, ' ');

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

							data->thread_memory[before_size + words[index]] = '\0';
							// find the last new line character in order to speed up processing
							const char* last_new_line = strrchr(data->thread_memory.buffer + before_size + last_position, '\n');

							// if it failed, set it to the start of the processing block
							last_new_line = (const char*)function::Select<uintptr_t>(last_new_line == nullptr, (uintptr_t)data->thread_memory.buffer + last_position, (uintptr_t)last_new_line);

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

							last_position = PtrDifference(data->thread_memory.buffer + before_size, closing_parenthese + 1);

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
			unsigned int hash = ReflectionStringHashFunction::Hash(identifier);
			bool success = reflection->field_table.TryGetValue(hash, identifier, info);
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
			data->types.Add(type);
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

			if (omit_field == nullptr) {
				// check only name reflect
				const char* reflect_only_name = strstr(last_line_character, STRING(ECS_REGISTER_ONLY_NAME_REFLECT));

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

				if (reflect_only_name == nullptr) {
					bool success = DeduceFieldType(data, type, pointer_offset, current_character, last_line_character, last_field_character - 1);
					if (success && embedded_array_size > 0) {
						ReflectionFieldInfo& info = type.fields[type.fields.size - 1].info;
						info.basic_type_count = embedded_array_size;
						pointer_offset -= info.byte_size;
						info.byte_size *= embedded_array_size;
						pointer_offset += info.byte_size;

						// change the extended type to array
						info.stream_type = ReflectionStreamFieldType::BasicTypeArray;
					}
					return success;
				}
				else {
					const char* initial_reflect_ptr = reflect_only_name;
					data->total_memory += PtrDifference(current_character, last_field_character) + 1;

					ReflectionField field;
					field.name = current_character;
					
					field.info.additional_flags = 0;
					field.info.basic_type = ReflectionBasicFieldType::UserDefined;
					field.info.stream_type = ReflectionStreamFieldType::Unknown;
					field.info.basic_type_count = 1;

					// determine the definition
					reflect_only_name--;
					while (!IsTypeCharacter(*reflect_only_name)) {
						reflect_only_name--;
					}
					char* mutable_ptr = (char*)reflect_only_name + 1;
					*mutable_ptr = '\0';
					while (IsTypeCharacter(*reflect_only_name)) {
						reflect_only_name--;
					}
					reflect_only_name++;
					data->total_memory += strlen(reflect_only_name);
					field.definition = reflect_only_name;
					
					reflect_only_name = initial_reflect_ptr;
					// getting the byte size from the argument specified
					const char* opening_parenthese = strchr(reflect_only_name, '(');
					const char* closing_parenthese = strchr(reflect_only_name, ')');

					size_t alignment = 8;
					// check for alignment
					const char* comma = strchr(opening_parenthese, ',');
					if (comma != nullptr) {
						closing_parenthese = comma;
						while (*comma < '0' && *comma > '9') {
							comma++;
						}
						alignment = function::ConvertCharactersToInt<size_t>(Stream<char>(comma, PtrDifference(comma, closing_parenthese)));
					}

					field.info.byte_size = function::ConvertCharactersToInt<unsigned short>(Stream<char>((void*)(opening_parenthese + 1), PtrDifference(opening_parenthese + 1, closing_parenthese)));
						
					pointer_offset = function::align_pointer(pointer_offset, alignment);
					field.info.pointer_offset = pointer_offset;
					pointer_offset += field.info.byte_size;

					type.fields.Add(field);
					return true;
				}
			}
			else {
				while (*omit_field != '(') {
					omit_field++;
				}
				omit_field++;
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

				field.info.additional_flags = function::ConvertCharactersToInt<unsigned short>(byte_size_stream);
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
					field.info.byte_size = sizeof(ResizableStream<void, LinearAllocator>);
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
				field.info.additional_flags = field.info.byte_size;
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

				pointer_offset = function::align_pointer(before_pointer_offset, alignof(ResizableStream<void, LinearAllocator>));
				field.info.pointer_offset = pointer_offset;
				field.info.stream_type = ReflectionStreamFieldType::ResizableStream;
				field.info.additional_flags = field.info.byte_size;
				field.info.byte_size = sizeof(ResizableStream<void, LinearAllocator>);
				pointer_offset += sizeof(ResizableStream<void, LinearAllocator>);

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
				field.info.additional_flags = field.info.byte_size;
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
				field.info.additional_flags = field.info.byte_size;
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
			unsigned int hash = ReflectionStringHashFunction::Hash(identifier);

			bool success = reflection_field_table->TryGetValue(hash, identifier, field.info);
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
			field.info.additional_flags = field.info.byte_size;
			field.info.stream_type = stream_type;
			if (stream_type == ReflectionStreamFieldType::Stream) {
				field.info.byte_size = sizeof(Stream<void>);
			}
			else if (stream_type == ReflectionStreamFieldType::CapacityStream) {
				field.info.byte_size = sizeof(CapacityStream<void>);
			}
			else if (stream_type == ReflectionStreamFieldType::ResizableStream) {
				field.info.byte_size = sizeof(ResizableStream<void, LinearAllocator>);
			}
			else if (stream_type == ReflectionStreamFieldType::Pointer) {
				field.info.byte_size = sizeof(void*);
			}

			return field;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool HasReflectStructures(const wchar_t* path)
		{
			constexpr size_t max_line_characters = 4096;
			char line_characters[max_line_characters];
			// open the file from the beginning
			std::ifstream stream(path, std::ios::beg | std::ios::in);

			// read the first line in order to check if there is something to be reflected
			stream.getline(line_characters, max_line_characters);

			return strstr(line_characters, STRING(ECS_REFLECT)) != nullptr;
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

		// ----------------------------------------------------------------------------------------------------------------------------

		// ----------------------------------------------------------------------------------------------------------------------------

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
