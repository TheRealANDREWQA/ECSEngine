#include "ecspch.h"
#include "Reflection.h"
#include "../File.h"

namespace ECSEngine {

	namespace Reflection {

		ECS_CONTAINERS;

		ReflectionManager::ReflectionManager(MemoryManager* allocator, size_t type_count, size_t enum_count) : folders(allocator, 0)
		{
			InitializeFieldTable();
			InitializeTypeTable(type_count);
			InitializeEnumTable(enum_count);
		}

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

		void ReflectionManager::BindApprovedData(
			const ReflectionManagerThreadTaskData* data,
			unsigned int data_count,
			unsigned int folder_index
		)
		{
			size_t total_memory = 0;
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				total_memory += data->total_memory;
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

		void ReflectionManager::ClearEnumDefinitions() {
			Stream<ReflectionEnum> enum_defs = enum_definitions.GetValueStream();
			for (size_t index = 0; index < enum_defs.size; index++) {
				if (enum_definitions.IsItemAt(index)) {
					folders.allocator->Deallocate(enum_defs[index].name);
				}
			}
			enum_definitions.Clear();
		}

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

		void ReflectionManager::DeallocateThreadTaskData(ReflectionManagerThreadTaskData& data)
		{
			free(data.thread_memory.buffer);
			folders.allocator->Deallocate(data.types.buffer);
			folders.allocator->Deallocate(data.enums.buffer);
			folders.allocator->Deallocate(data.paths.buffer);
		}

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

		ReflectionType ReflectionManager::GetType(const char* name) const
		{
			size_t name_length = strlen(name);
			ResourceIdentifier identifier = ResourceIdentifier(name, name_length);
			unsigned int hash = ReflectionStringHashFunction::Hash(name, name_length);
			return type_definitions.GetValue(hash, identifier);
		}

		ReflectionEnum ReflectionManager::GetEnum(const char* name) const {
			size_t name_length = strlen(name);
			ResourceIdentifier identifier = ResourceIdentifier(name, name_length);
			unsigned int hash = ReflectionStringHashFunction::Hash(name, name_length);
			return enum_definitions.GetValue(hash, identifier);
		}

		void* ReflectionManager::GetTypeInstancePointer(const char* name, void* instance, unsigned int pointer_index) const
		{
			ReflectionType type = GetType(name);
			return GetTypeInstancePointer(type, instance, pointer_index);
		}

		void* ReflectionManager::GetTypeInstancePointer(ReflectionType type, void* instance, unsigned int pointer_index) const
		{
			size_t count = 0;
			for (size_t index = 0; index < type.fields.size; index++) {
				if (type.fields[index].info.extended_type == ReflectionExtendedFieldType::Pointer || IsStream(type.fields[index].info.extended_type)) {
					if (count == pointer_index) {
						return (void*)((uintptr_t)instance + type.fields[index].info.pointer_offset);
					}
					count++;
				}
			}
			ECS_ASSERT(false);
		}

		bool ReflectionManager::TryGetType(const char* name, ReflectionType& type) const
		{
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, ReflectionStringHashFunction);
			return type_definitions.TryGetValue(hash, identifier, type);
		}

		bool ReflectionManager::TryGetEnum(const char* name, ReflectionEnum& enum_) const {
			ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, ReflectionStringHashFunction);
			return enum_definitions.TryGetValue(hash, identifier, enum_);
		}

		void ReflectionManager::InitializeFieldTable()
		{
			constexpr size_t table_size = 256;
			void* allocation = folders.allocator->Allocate(ReflectionFieldTable::MemoryOf(table_size));
			field_table.InitializeFromBuffer(allocation, table_size);

			// Initialize all values, helped by macros
			ResourceIdentifier identifier;
#define BASIC_TYPE(type, basic_type, extended_type) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionStringHashFunction::Hash(identifier.ptr, identifier.size), ReflectionFieldInfo(basic_type, extended_type, sizeof(type), 1), identifier);
#define INT_TYPE(type, val) BASIC_TYPE(type, ReflectionBasicFieldType::val, ReflectionExtendedFieldType::val)
#define COMPLEX_TYPE(type, basic_type, extended_type, byte_size, basic_type_count) identifier = ResourceIdentifier(STRING(type), strlen(STRING(type))); field_table.Insert(ReflectionStringHashFunction::Hash(identifier.ptr, identifier.size), ReflectionFieldInfo(basic_type, extended_type, byte_size, basic_type_count), identifier);

#define TYPE_234(base, reflection_type) COMPLEX_TYPE(base##2, ReflectionBasicFieldType::reflection_type, ReflectionExtendedFieldType::reflection_type##2, sizeof(base) * 2, 2); \
COMPLEX_TYPE(base##3, ReflectionBasicFieldType::reflection_type, ReflectionExtendedFieldType::reflection_type##3, sizeof(base) * 3, 3); \
COMPLEX_TYPE(base##4, ReflectionBasicFieldType::reflection_type, ReflectionExtendedFieldType::reflection_type##4, sizeof(base) * 4, 4);

#define TYPE_234_SIGNED_INT(base, basic_reflect, extended_reflect) COMPLEX_TYPE(base##2, ReflectionBasicFieldType::basic_reflect, ReflectionExtendedFieldType::extended_reflect##2, sizeof(base) * 2, 2); \
COMPLEX_TYPE(base##3, ReflectionBasicFieldType::basic_reflect, ReflectionExtendedFieldType::extended_reflect##3, sizeof(base) * 3, 3); \
COMPLEX_TYPE(base##4, ReflectionBasicFieldType::basic_reflect, ReflectionExtendedFieldType::extended_reflect##4, sizeof(base) * 4, 4);

#define TYPE_234_UNSIGNED_INT(base, basic_reflect, extended_reflect) COMPLEX_TYPE(u##base##2, ReflectionBasicFieldType::basic_reflect, ReflectionExtendedFieldType::U##extended_reflect##2, sizeof(base) * 2, 2); \
COMPLEX_TYPE(u##base##3, ReflectionBasicFieldType::basic_reflect, ReflectionExtendedFieldType::U##extended_reflect##3, sizeof(base) * 3, 3); \
COMPLEX_TYPE(u##base##4, ReflectionBasicFieldType::basic_reflect, ReflectionExtendedFieldType::U##extended_reflect##4, sizeof(base) * 4, 4);

#define TYPE_234_INT(base, basic_reflect, extended_reflect) TYPE_234_SIGNED_INT(base, basic_reflect, extended_reflect); TYPE_234_UNSIGNED_INT(base, basic_reflect, extended_reflect)

			INT_TYPE(int8_t, Int8);
			INT_TYPE(uint8_t, UInt8);
			INT_TYPE(int16_t, Int16);
			INT_TYPE(uint16_t, UInt16);
			INT_TYPE(int32_t, Int32);
			INT_TYPE(uint32_t, UInt32);
			INT_TYPE(int64_t, Int64);
			INT_TYPE(uint64_t, UInt64);
			INT_TYPE(bool, Bool);

			BASIC_TYPE(char, ReflectionBasicFieldType::Int8, ReflectionExtendedFieldType::Char);
			BASIC_TYPE(unsigned char, ReflectionBasicFieldType::UInt8, ReflectionExtendedFieldType::UChar);
			BASIC_TYPE(short, ReflectionBasicFieldType::Int16, ReflectionExtendedFieldType::Short);
			BASIC_TYPE(unsigned short, ReflectionBasicFieldType::UInt16, ReflectionExtendedFieldType::UShort);
			BASIC_TYPE(int, ReflectionBasicFieldType::Int32, ReflectionExtendedFieldType::Int);
			BASIC_TYPE(unsigned int, ReflectionBasicFieldType::UInt32, ReflectionExtendedFieldType::UInt);
			BASIC_TYPE(long long, ReflectionBasicFieldType::Int64, ReflectionExtendedFieldType::Long);
			BASIC_TYPE(unsigned long long, ReflectionBasicFieldType::UInt64, ReflectionExtendedFieldType::ULong);
			BASIC_TYPE(size_t, ReflectionBasicFieldType::UInt64, ReflectionExtendedFieldType::ULong);
			BASIC_TYPE(wchar_t, ReflectionBasicFieldType::Wchar_t, ReflectionExtendedFieldType::Wchar_t);

			BASIC_TYPE(float, ReflectionBasicFieldType::Float, ReflectionExtendedFieldType::Float);
			BASIC_TYPE(double, ReflectionBasicFieldType::Double, ReflectionExtendedFieldType::Double);
			COMPLEX_TYPE(enum, ReflectionBasicFieldType::Enum, ReflectionExtendedFieldType::Enum, sizeof(ReflectionBasicFieldType), 1);
			COMPLEX_TYPE(pointer, ReflectionBasicFieldType::UserDefined, ReflectionExtendedFieldType::Pointer, sizeof(void*), 1);
			
			TYPE_234(bool, Bool);
			TYPE_234(float, Float);
			TYPE_234(double, Double);

			TYPE_234_INT(char, Int8, Char);
			TYPE_234_INT(short, Int16, Short);
			TYPE_234_INT(int, Int32, Int);
			TYPE_234_INT(long long, Int64, Long);

#undef INT_TYPE
#undef BASIC_TYPE
#undef COMPLEX_TYPE
#undef TYPE_234_INT
#undef TYPE_234_UNSIGNED_INT
#undef TYPE_234_SIGNED_INT
#undef TYPE_234
		}

		void ReflectionManager::InitializeTypeTable(size_t count)
		{
			void* allocation = folders.allocator->Allocate(ReflectionTypeTable::MemoryOf(count));
			type_definitions.InitializeFromBuffer(allocation, count);
		}

		void ReflectionManager::InitializeEnumTable(size_t count)
		{
			void* allocation = folders.allocator->Allocate(ReflectionEnumTable::MemoryOf(count));
			enum_definitions.InitializeFromBuffer(allocation, count);
		}

		void ReflectionManager::InitializeThreadTaskData(
			size_t thread_memory,
			size_t path_count, 
			ReflectionManagerThreadTaskData& data, 
			const char*& error_message
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
			data.faulty_path = -1;
			data.field_table = &field_table;
			data.success = true;
			data.total_memory = 0;
		}

		bool ReflectionManager::ProcessFolderHierarchy(const wchar_t* root, const char*& error_message, wchar_t* faulty_path) {
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(folders[index].hierarchy.root, root) == 0) {
					return ProcessFolderHierarchy(index, error_message, faulty_path);
				}
			}
			return false;
		}

		bool ReflectionManager::ProcessFolderHierarchy(
			const wchar_t* root, 
			const char**& error_message, 
			TaskManager* task_manager, 
			wchar_t* faulty_path
		) {
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(folders[index].hierarchy.root, root) == 0) {
					return ProcessFolderHierarchy(index, error_message, task_manager, faulty_path);
				}
			}
			return false;
		}

		bool ReflectionManager::ProcessFolderHierarchy(unsigned int index, const char*& error_message, wchar_t* faulty_path) {
			ReflectionManagerThreadTaskData thread_data;

			constexpr size_t thread_memory = 20'000'000;
			// Paths that need to be searched will not be assigned here
			InitializeThreadTaskData(thread_memory, folders[index].hierarchy.filtered_files[0].files.size, thread_data, error_message);

			ConditionVariable condition_variable;
			thread_data.condition_variable = &condition_variable;
			// Assigning paths
			for (size_t path_index = 0; path_index < folders[index].hierarchy.filtered_files[0].files.size; path_index++) {
				thread_data.paths[path_index] = folders[index].hierarchy.filtered_files[0].files[path_index];
			}

			ReflectionManagerThreadTask(0, nullptr, &thread_data);
			if (thread_data.success == false) {
				if (faulty_path != nullptr) {
					wcscpy(faulty_path, thread_data.paths[thread_data.faulty_path]);
				}
			}
			else {
				BindApprovedData(&thread_data, 1, index);
			}

			DeallocateThreadTaskData(thread_data);
			return thread_data.success;
		}

		bool ReflectionManager::ProcessFolderHierarchy(
			unsigned int index, 
			const char**& error_messages, 
			TaskManager* task_manager, 
			wchar_t* faulty_path
		) {
			unsigned int thread_count = task_manager->GetThreadCount();
			ReflectionManagerThreadTaskData* thread_data = (ReflectionManagerThreadTaskData*)folders.allocator->Allocate(sizeof(ReflectionManagerThreadTaskData) * thread_count);

			// Preparsing the files in order to have thread act only on files that actually need to process
			// Otherwise unequal distribution of files will result in poor multithreaded performance
			unsigned int path_count = 0;

			unsigned int* path_indices_buffer = (unsigned int*)folders.allocator->Allocate(sizeof(unsigned int) * folders[index].hierarchy.filtered_files[0].files.size, alignof(unsigned int));
			Stream<unsigned int> path_indices = Stream<unsigned int>(path_indices_buffer, 0);
			for (size_t path_index = 0; path_index < folders[index].hierarchy.filtered_files[0].files.size; path_index++) {
				bool has_reflect = HasReflectStructures(folders[index].hierarchy.filtered_files[0].files[path_index]);
				if (has_reflect) {
					path_count++;
					path_indices.Add(path_index);
				}
			}

			unsigned int thread_paths = path_count / thread_count;
			unsigned int thread_paths_remainder = path_count % thread_count;

			constexpr size_t thread_memory = 10'000'000;

			unsigned int total_paths_counted = 0;
			ConditionVariable condition_variable;
			for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
				unsigned int should_add_remainder = 0;
				if (thread_paths_remainder > 0) {
					should_add_remainder = 1;
					thread_paths_remainder--;
				}

				unsigned int thread_current_paths = thread_paths + should_add_remainder;
				// initialize data with buffers
				InitializeThreadTaskData(thread_memory, thread_current_paths, thread_data[thread_index], error_messages[thread_index]);
				thread_data[thread_index].condition_variable = &condition_variable;

				// Set thread paths
				for (size_t path_index = total_paths_counted; path_index < total_paths_counted + thread_current_paths; path_index++) {
					thread_data[thread_index].paths[path_index - total_paths_counted] = folders[index].hierarchy.filtered_files[0].files[path_indices[path_index]];
				}
				total_paths_counted += thread_current_paths;
			}

			// Push thread execution
			for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
				ThreadTask task;
				task.function = ReflectionManagerThreadTask;
				task.data = thread_data + thread_index;
				task_manager->AddDynamicTaskAndWake(task, thread_index);
			}

			// Wait until threads are done
			condition_variable.Wait(thread_count);

			bool success = true;
			
			size_t path_index = 0;
			// Check every thread for success and return the first error if there is such an error
			for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
				if (thread_data[thread_index].success == false) {
					success = false;
					if (faulty_path != nullptr) {
						wcscpy(faulty_path, folders[index].hierarchy.filtered_files[0].files[path_indices[thread_data[thread_index].faulty_path + path_index]]);
					}
					break;
				}
				path_index += thread_data[thread_index].paths.size;
			}

			if (success) {
				BindApprovedData(thread_data, thread_count, index);
			}

			// Free the previously allocated memory
			for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
				DeallocateThreadTaskData(thread_data[thread_index]);
			}
			folders.allocator->Deallocate(path_indices_buffer);
			folders.allocator->Deallocate(thread_data);

			return success;
		}

		void ReflectionManager::RemoveFolderHierarchy(unsigned int folder_index)
		{
			FreeFolderHierarchy(folder_index);
			folders.RemoveSwapBack(folder_index);
		}

		void ReflectionManager::RemoveFolderHierarchy(const wchar_t* root) {
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(folders[index].hierarchy.root, root) == 0) {
					return RemoveFolderHierarchy(index);
				}
			}
			// error if not found
			ECS_ASSERT(false);
		}

		bool ReflectionManager::UpdateFolderHierarchy(unsigned int folder_index, const char*& error_message, wchar_t* faulty_path)
		{
			FreeFolderHierarchy(folder_index);
			folders[folder_index].hierarchy.Recreate();
			return ProcessFolderHierarchy(folder_index, error_message);
		}

		bool ReflectionManager::UpdateFolderHierarchy(const wchar_t* root, const char*& error_message, wchar_t* faulty_path) {
			for (size_t index = 0; index < folders.size; index++) {
				if (wcscmp(root, folders[index].hierarchy.root) == 0) {
					return UpdateFolderHierarchy(index, error_message, faulty_path);
				}
			}
			// Fail if it was not found previously
			ECS_ASSERT(false);
			return false;
		}

		ReflectionFolderHierarchy::ReflectionFolderHierarchy(MemoryManager* allocator) : folders(allocator, 0), filtered_files(allocator, 0), root(nullptr) {}
		ReflectionFolderHierarchy::ReflectionFolderHierarchy(MemoryManager* allocator, const wchar_t* path) : folders(allocator, 0), filtered_files(allocator, 0) {
			CreateFromPath(path);
		}

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

		void ReflectionFolderHierarchy::CreateFromPath(const wchar_t* path)
		{
			size_t root_size = wcslen(path) + 1;
			void* allocation = folders.allocator->Allocate(root_size * sizeof(wchar_t), alignof(wchar_t));
			memcpy(allocation, path, root_size * sizeof(wchar_t));
			root = (const wchar_t*)allocation;
			function::GetRecursiveDirectories(folders.allocator, path, folders);
		}

		void ReflectionFolderHierarchy::RemoveFilterFiles(const char* filter_name)
		{
			for (size_t index = 0; index < filtered_files.size; index++) {
				if (strcmp(filtered_files[index].filter_name, filter_name) == 0) {
					RemoveFilterFiles(index);
					break;
				}
			}
		}

		void ReflectionFolderHierarchy::RemoveFilterFiles(unsigned int index) {
			folders.allocator->Deallocate(filtered_files[index].filter_name);
			folders.allocator->Deallocate(filtered_files[index].extensions.buffer);
			for (size_t subindex = 0; subindex < filtered_files[index].files.size; subindex++) {
				folders.allocator->Deallocate(filtered_files[index].files[subindex]);
			}
		}

		bool IsTypeCharacter(char character)
		{
			if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') 
				|| character == '_' || character == '<' || character == '>') {
				return true;
			}
			return false;
		}

		size_t PtrDifference(const void* ptr1, const void* ptr2)
		{
			return (uintptr_t)ptr2 - (uintptr_t)ptr1;
		}

		void ReflectionManagerThreadTask(unsigned int thread_id, World* world, void* _data) {
			ReflectionManagerThreadTaskData* data = (ReflectionManagerThreadTaskData*)_data;

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
							data->success = false;
							data->error_message = "Not enough memory to read file contents";
							data->faulty_path = index;
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
								data->success = false;
								data->error_message = "Finding type leading space failed";
								data->faulty_path = index;
								return;
							}
							space++;

							const char* second_space = strchr(space, ' ');

							// if the second space was not found, fail
							if (second_space == nullptr) {
								data->success = false;
								data->error_message = "Finding type final space failed";
								data->faulty_path = index;
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
								data->success = false;
								data->error_message = "Finding opening curly brace failed";
								data->faulty_path = index;
								return;
							}

							const char* closing_parenthese = strchr(opening_parenthese + 1, '}');
							// if no closing curly brace was found, fail
							if (closing_parenthese == nullptr) {
								data->success = false;
								data->error_message = "Finding closing curly brace failed";
								data->faulty_path = index;
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
									data->success = false;
									data->error_message = "Enum and type definition validation failed, didn't find neither";
									data->faulty_path = index;
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

		void AddEnumDefinition(
			ReflectionManagerThreadTaskData* data,
			const char* ECS_RESTRICT opening_parenthese, 
			const char* ECS_RESTRICT closing_parenthese,
			const char* ECS_RESTRICT name,
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
				data->success = false;
				data->error_message = "Not enough memory to assign enum definition stream";
				data->faulty_path = index;
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

		void AddTypeDefinition(
			ReflectionManagerThreadTaskData* data,
			const char* ECS_RESTRICT opening_parenthese,
			const char* ECS_RESTRICT closing_parenthese, 
			const char* ECS_RESTRICT name,
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
				data->success = false;
				data->error_message = "Assigning type field stream failed, insufficient memory";
				data->faulty_path = index;
				return;
			}

			// determining each field
			unsigned short pointer_offset = 0;
			for (size_t index = 0; index < semicolon_positions.size; index++) {
				bool succeded = AddFieldType(data, type, pointer_offset, opening_parenthese + next_line_positions[index], opening_parenthese + semicolon_positions[index]);
				
				if (!succeded) {
					data->success = false;
					data->faulty_path = index;
					return;
				}
			}
			data->types.Add(type);
			data->total_memory += sizeof(ReflectionType);
		}

		bool AddFieldType(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT last_line_character, 
			const char* ECS_RESTRICT semicolon_character
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
						switch (info.basic_type) {
						case ReflectionBasicFieldType::Bool:
							info.extended_type = ReflectionExtendedFieldType::BoolArray;
							break;
						case ReflectionBasicFieldType::Float:
							info.extended_type = ReflectionExtendedFieldType::FloatArray;
							break;
						case ReflectionBasicFieldType::Double:
							info.extended_type = ReflectionExtendedFieldType::DoubleArray;
							break;
						case ReflectionBasicFieldType::Enum:
							info.extended_type = ReflectionExtendedFieldType::EnumArray;
							break;
						case ReflectionBasicFieldType::Int8:
							info.extended_type = ReflectionExtendedFieldType::Int8Array;
							break;
						case ReflectionBasicFieldType::Int16:
							info.extended_type = ReflectionExtendedFieldType::Int16Array;
							break;
						case ReflectionBasicFieldType::Int32:
							info.extended_type = ReflectionExtendedFieldType::Int32Array;
							break;
						case ReflectionBasicFieldType::Int64:
							info.extended_type = ReflectionExtendedFieldType::Int64Array;
							break;
						case ReflectionBasicFieldType::UInt8:
							info.extended_type = ReflectionExtendedFieldType::UInt8Array;
							break;
						case ReflectionBasicFieldType::UInt16:
							info.extended_type = ReflectionExtendedFieldType::UInt16Array;
							break;
						case ReflectionBasicFieldType::UInt32:
							info.extended_type = ReflectionExtendedFieldType::UInt32Array;
							break;
						case ReflectionBasicFieldType::UInt64:
							info.extended_type = ReflectionExtendedFieldType::UInt64Array;
							break;
						}
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
					field.info.extended_type = ReflectionExtendedFieldType::Unknown;
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

		bool DeduceFieldType(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT field_name,
			const char* ECS_RESTRICT last_line_character,
			const char* ECS_RESTRICT last_valid_character
		)
		{
			bool type_from_macro = DeduceFieldTypeFromMacros(data, type, pointer_offset, field_name, last_line_character);
			if (data->error_message != nullptr) {
				return false;
			}
			// if the type was not deduced from macros
			else if (!type_from_macro) {
				bool success = DeduceFieldTypePointer(data, type, pointer_offset, field_name, last_line_character);
				if (data->error_message != nullptr) {
					return false;
				}
				// if this is not a pointer type, extended type then
				else if (!success) {
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
					return true;
				}
				return success;
			}
			return type_from_macro;
		}

		bool CheckFieldExplicitDefinition(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type, 
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT last_line_character
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
					data->error_message = "Invalid number of fields specified in explicit definition";
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

				data->error_message = nullptr;
				return true;
			}

			return false;
		}

		bool DeduceFieldTypeFromMacros(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT field_name,
			const char* ECS_RESTRICT last_line_character
		)
		{
			bool explicit_definition = CheckFieldExplicitDefinition(data, type, pointer_offset, last_line_character);
			
			auto pointer_identification = [&](const char* reflect_pointer) {
				// Get arguments; extended type and basic type count means the indirection count
				const char* opening_parenthese = strchr(reflect_pointer, '(');
				if (opening_parenthese == nullptr) {
					data->error_message = "Invalid ECS_POINTER_REFLECT, no opening parenthese";
					return;
				}

				char* comma = (char*)strchr(opening_parenthese, ',');
				if (comma == nullptr) {
					data->error_message = "Invalid ECS_POINTER_REFLECT, no comma";
					return;
				}

				char* closing_parenthese = (char*)strchr(comma, ')');
				if (closing_parenthese == nullptr) {
					data->error_message = "Invalid ECS_POINTER_REFLECT, no closing parenthese";
					return;
				}

				// null terminate the comma and closing parenthese
				*comma = '\0';
				*closing_parenthese = '\0';

				ReflectionField field;
				GetReflectionFieldInfo(data, opening_parenthese + 1, field);

				if (field.info.extended_type == ReflectionExtendedFieldType::Unknown) {
					field.info.extended_type = ReflectionExtendedFieldType::Pointer;
					field.info.basic_type_count = function::ConvertCharactersToInt<unsigned short>(Stream<char>(comma + 2, PtrDifference(comma + 2, closing_parenthese)));
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
				field.info.extended_type = ReflectionExtendedFieldType::Enum;
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
					data->error_message = "Invalid ECS_STREAM_REFLECT, no byte size";
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
					field.info.extended_type = ReflectionExtendedFieldType::Stream;
				}
				// Capacity stream
				else if (strncmp(initial_ptr, STRING(CapacityStream), strlen(STRING(CapacityStream))) == 0) {
					field.info.byte_size = sizeof(CapacityStream<void>);
					field.info.extended_type = ReflectionExtendedFieldType::CapacityStream;
				}
				// Resizable stream
				else if (strncmp(initial_ptr, STRING(ResizableStream), strlen(STRING(ResizableStream))) == 0) {
					field.info.byte_size = sizeof(ResizableStream<void, LinearAllocator>);
					field.info.extended_type = ReflectionExtendedFieldType::ResizableStream;
				}
				else {
					data->error_message = "Invalid ECS_STREAM_REFLECT, no stream identifier";
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

		bool DeduceFieldTypePointer(
			ReflectionManagerThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT field_name,
			const char* ECS_RESTRICT last_line_character
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

				if (field.info.extended_type == ReflectionExtendedFieldType::Unknown) {
					field.info.basic_type_count = pointer_level;
					field.info.byte_size = sizeof(void*);
					field.info.extended_type = ReflectionExtendedFieldType::Pointer;
					field.info.basic_type = ReflectionBasicFieldType::UserDefined;
					field.info.additional_flags = 0;
				}
				
				pointer_offset = function::align_pointer(before_pointer_offset, alignof(void*));
				field.info.pointer_offset = pointer_offset;
				pointer_offset += sizeof(void*);

				data->total_memory += strlen(field.name) + 1;
				type.fields.Add(field);
				return true;
			}
			return false;
		}

		void DeduceFieldTypeExtended(
			ReflectionManagerThreadTaskData* data,
			unsigned short& pointer_offset,
			const char* ECS_RESTRICT last_type_character, 
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
					pointer_offset = function::align_pointer(pointer_offset, field.info.byte_size / field.info.basic_type_count);
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

		void GetReflectionFieldInfo(ReflectionManagerThreadTaskData* data, const char* extended_type, ReflectionField& field)
		{
			size_t extended_type_size = strlen(extended_type);
			ResourceIdentifier identifier = ResourceIdentifier(extended_type, extended_type_size);
			unsigned int hash = ReflectionStringHashFunction::Hash(extended_type, extended_type_size);

			bool success = data->field_table->TryGetValue(hash, identifier, field.info);
			field.definition = extended_type;
			if (!success) {
				field.info.extended_type = ReflectionExtendedFieldType::Unknown;
				field.info.basic_type = ReflectionBasicFieldType::UserDefined;
				data->total_memory += strlen(extended_type) + 1;
			}
			else {
				// Set to global strings the definition in order to not occupy extra memory

#define CASE(type, string) case ReflectionExtendedFieldType::type: field.definition = STRING(string); break;
#define CASE234(type, string) CASE(type##2, string##2); CASE(type##3, string##3); CASE(type##4, string##4);
#define CASE1234(type, string) CASE(type, string); CASE234(type, string);

				switch (field.info.extended_type) {
					CASE1234(Bool, bool);
					CASE1234(Char, char);
					CASE(UChar, unsigned char);
					CASE234(UChar, uchar);
					CASE1234(Short, short);
					CASE(UShort, unsigned short);
					CASE234(UShort, ushort);
					CASE1234(Int, int);
					CASE(UInt, unsigned int);
					CASE234(UInt, uint);
					CASE(Long, long long);
					CASE234(Long, long);
					CASE(ULong, unsigned long long);
					CASE234(ULong, ulong);
					CASE(Int8, int8_t);
					CASE(UInt8, uint8_t);
					CASE(Int16, int16_t);
					CASE(UInt16, uint16_t);
					CASE(Int32, int32_t);
					CASE(UInt32, uint32_t);
					CASE(Int64, int64_t);
					CASE(UInt64, uint64_t);
					CASE(SizeT, size_t);
					CASE1234(Float, float);
					CASE1234(Double, double);
				}
			}
		}

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

		bool ECSENGINE_API IsIntegral(ReflectionBasicFieldType type)
		{
			return type == ReflectionBasicFieldType::Int8 || type == ReflectionBasicFieldType::UInt8 ||
				type == ReflectionBasicFieldType::Int16 || type == ReflectionBasicFieldType::UInt16 ||
				type == ReflectionBasicFieldType::Int32 || type == ReflectionBasicFieldType::UInt32 ||
				type == ReflectionBasicFieldType::Int64 || type == ReflectionBasicFieldType::UInt64;
		}

		bool ECSENGINE_API IsIntegral(ReflectionExtendedFieldType type)
		{
			return type == ReflectionExtendedFieldType::Int8 || type == ReflectionExtendedFieldType::UInt8 ||
				type == ReflectionExtendedFieldType::Int16 || type == ReflectionExtendedFieldType::UInt16 ||
				type == ReflectionExtendedFieldType::Int32 || type == ReflectionExtendedFieldType::UInt32 ||
				type == ReflectionExtendedFieldType::Int64 || type == ReflectionExtendedFieldType::UInt64 ||
				type == ReflectionExtendedFieldType::Char || type == ReflectionExtendedFieldType::UChar ||
				type == ReflectionExtendedFieldType::Short || type == ReflectionExtendedFieldType::UShort ||
				type == ReflectionExtendedFieldType::Int || type == ReflectionExtendedFieldType::UInt ||
				type == ReflectionExtendedFieldType::Long || type == ReflectionExtendedFieldType::ULong ||
				type == ReflectionExtendedFieldType::SizeT;
		}

		bool ECSENGINE_API IsIntegralArray(ReflectionExtendedFieldType type)
		{
			return type == ReflectionExtendedFieldType::Int8Array || type == ReflectionExtendedFieldType::UInt8Array ||
				type == ReflectionExtendedFieldType::Int16Array || type == ReflectionExtendedFieldType::UInt16Array ||
				type == ReflectionExtendedFieldType::Int32Array || type == ReflectionExtendedFieldType::UInt32Array ||
				type == ReflectionExtendedFieldType::Int64Array || type == ReflectionExtendedFieldType::UInt64Array;
		}

		bool IsIntegralBasicType(ReflectionExtendedFieldType type)
		{
			return type == ReflectionExtendedFieldType::Char2 || type == ReflectionExtendedFieldType::Char3 || type == ReflectionExtendedFieldType::Char4 ||
				type == ReflectionExtendedFieldType::UChar2 || type == ReflectionExtendedFieldType::UChar3 || type == ReflectionExtendedFieldType::UChar4 ||
				type == ReflectionExtendedFieldType::Short2 || type == ReflectionExtendedFieldType::Short3 || type == ReflectionExtendedFieldType::Short4 ||
				type == ReflectionExtendedFieldType::UShort2 || type == ReflectionExtendedFieldType::UShort3 || type == ReflectionExtendedFieldType::UShort4 ||
				type == ReflectionExtendedFieldType::Int2 || type == ReflectionExtendedFieldType::Int3 || type == ReflectionExtendedFieldType::Int4 ||
				type == ReflectionExtendedFieldType::UInt2 || type == ReflectionExtendedFieldType::UInt3 || type == ReflectionExtendedFieldType::UInt4 ||
				type == ReflectionExtendedFieldType::Long2 || type == ReflectionExtendedFieldType::Long3 || type == ReflectionExtendedFieldType::Long4 ||
				type == ReflectionExtendedFieldType::ULong2 || type == ReflectionExtendedFieldType::ULong3 || type == ReflectionExtendedFieldType::ULong4;
		}

		/*bool GetReflectionBasicFieldType(ReflectionManagerThreadTaskData* data, const char* basic_type, ReflectionBasicFieldType& type)
		{
			size_t basic_type_size = strlen(basic_type);
			ResourceIdentifier identifier = ResourceIdentifier(basic_type, basic_type_size);
			unsigned int hash = ReflectionStringHashFunction::Hash(basic_type, basic_type_size);

			bool success = data->basic_field_table->TryGetValue(hash, identifier, type);
			if (!success) {
				data->error_message = "Invalid basic type";
				return false;
			}
			return true;
		}*/

	}
}
