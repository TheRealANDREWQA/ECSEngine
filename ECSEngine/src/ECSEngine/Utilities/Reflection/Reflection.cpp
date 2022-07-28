#include "ecspch.h"
#include "Reflection.h"
#include "../File.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../ForEachFiles.h"
#include "ReflectionStringFunctions.h"
#include "../../Internal/Resources/AssetDatabaseSerializeFunctions.h"
#include "../../Containers/SparseSet.h"

namespace ECSEngine {

	namespace Reflection {

#pragma region Container types

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionContainerTypeDependentTypes_SingleTemplate(ReflectionContainerTypeDependentTypesData* data)
		{
			const char* opened_bracket = strchr(data->definition.buffer, '<');
			ECS_ASSERT(opened_bracket != nullptr);

			const char* closed_bracket = strchr(opened_bracket, '>');
			ECS_ASSERT(closed_bracket != nullptr);

			data->dependent_types.AddSafe({ opened_bracket + 1, function::PointerDifference(closed_bracket, opened_bracket) - 1 });
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionContainerTypeMatchTemplate(ReflectionContainerTypeMatchData* data, const char* string)
		{
			size_t string_size = strlen(string);

			if (data->definition.size < string_size) {
				return false;
			}

			if (data->definition[data->definition.size - 1] != '>') {
				return false;
			}

			if (memcmp(data->definition.buffer, string, string_size * sizeof(char)) == 0) {
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> ReflectionContainerTypeGetTemplateArgument(Stream<char> definition)
		{
			const char* opened_bracket = strchr(definition.buffer, '<');
			const char* closed_bracket = strchr(definition.buffer, '>');
			ECS_ASSERT(opened_bracket != nullptr && closed_bracket != nullptr);

			Stream<char> type = { opened_bracket + 1, function::PointerDifference(closed_bracket, opened_bracket) - 1 };

			return type;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionContainerTypeMatch_Streams(ReflectionContainerTypeMatchData* data) {
			if (data->definition.size < sizeof("Stream<")) {
				return false;
			}

			if (data->definition[data->definition.size - 1] != '>') {
				return false;
			}

			if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
				return true;
			}

			if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
				return true;
			}

			if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
				return true;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 ReflectionContainerTypeByteSize_Streams(ReflectionContainerTypeByteSizeData* data) {
			if (memcmp(data->definition.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
				return { sizeof(Stream<void>), alignof(Stream<void>) };
			}
			else if (memcmp(data->definition.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
				return { sizeof(CapacityStream<void>), alignof(CapacityStream<void>) };
			}
			else if (memcmp(data->definition.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
				return { sizeof(ResizableStream<void>), alignof(ResizableStream<void>) };
			}

			ECS_ASSERT(false);
			return 0;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionContainerTypeDependentTypes_Streams(ReflectionContainerTypeDependentTypesData* data) {
			ReflectionContainerTypeDependentTypes_SingleTemplate(data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionContainerTypeMatch_SparseSet(ReflectionContainerTypeMatchData* data)
		{
			return ReflectionContainerTypeMatchTemplate(data, "SparseSet") || ReflectionContainerTypeMatchTemplate(data, "ResizableSparseSet");
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 ReflectionContainerTypeByteSize_SparseSet(ReflectionContainerTypeByteSizeData* data)
		{
			if (memcmp(data->definition.buffer, "SparseSet<", sizeof("SparseSet<") - 1) == 0) {
				return { sizeof(SparseSet<char>), alignof(SparseSet<char>) };
			}
			else {
				return { sizeof(ResizableSparseSet<char>), alignof(ResizableSparseSet<char>) };
			}
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionContainerTypeDependentTypes_SparseSet(ReflectionContainerTypeDependentTypesData* data)
		{
			ReflectionContainerTypeDependentTypes_SingleTemplate(data);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionContainerTypeMatch_Color(ReflectionContainerTypeMatchData* data)
		{
			return function::CompareStrings(ToStream("Color"), data->definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 ReflectionContainerTypeByteSize_Color(ReflectionContainerTypeByteSizeData* data)
		{
			return { sizeof(char) * 4, alignof(char) };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionContainerTypeDependentTypes_Color(ReflectionContainerTypeDependentTypesData* data) {}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionContainerTypeMatch_ColorFloat(ReflectionContainerTypeMatchData* data)
		{
			return function::CompareStrings(ToStream("ColorFloat"), data->definition);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ulong2 ReflectionContainerTypeByteSize_ColorFloat(ReflectionContainerTypeByteSizeData* data)
		{
			return { sizeof(float) * 4, alignof(float) };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ReflectionContainerTypeDependentTypes_ColorFloat(ReflectionContainerTypeDependentTypesData* data) {}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionContainerType ECS_REFLECTION_CONTAINER_TYPES[] = {
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(Streams),
			ECS_REFLECTION_CONTAINER_TYPE_REFERENCE_COUNTED_ASSET,
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(SparseSet),
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(Color),
			ECS_REFLECTION_CONTAINER_TYPE_STRUCT(ColorFloat)
		};

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned int FindReflectionContainerType(Stream<char> definition)
		{
			ReflectionContainerTypeMatchData match_data = { definition };

			for (unsigned int index = 0; index < std::size(ECS_REFLECTION_CONTAINER_TYPES); index++) {
				if (ECS_REFLECTION_CONTAINER_TYPES[index].match(&match_data)) {
					return index;
				}
			}

			return -1;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ----------------------------------------------------------------------------------------------------------------------------

		// Updates the type.tag
		void AddTypeTag(
			ReflectionManager* reflection,
			const char* type_tag,
			ReflectionType& type
		) {
			size_t tag_index = 0;
			for (; tag_index < reflection->type_tags.size; tag_index++) {
				if (function::CompareStrings(reflection->type_tags[tag_index].tag_name, type_tag)) {
					reflection->type_tags[tag_index].type_names.Add(type.name);
					type.tag = reflection->type_tags[tag_index].tag_name;
					break;
				}
			}
			if (tag_index == reflection->type_tags.size) {
				const char* type_tag_copy = function::StringCopy(reflection->folders.allocator, type_tag).buffer;
				tag_index = reflection->type_tags.Add({ type_tag_copy, ResizableStream<const char*>(reflection->folders.allocator, 1) });
				reflection->type_tags[tag_index].type_names.Add(type.name);
				type.tag = type_tag_copy;
			}
		}

		void ReflectionManagerParseThreadTask(unsigned int thread_id, World* world, void* _data);
		void ReflectionManagerHasReflectStructuresThreadTask(unsigned int thread_id, World* world, void* _data);

		struct ReflectionManagerParseStructuresThreadTaskData {
			CapacityStream<char> thread_memory;
			Stream<const wchar_t*> paths;
			CapacityStream<ReflectionType> types;
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

					size_t type_name_size = strlen(data[data_index].types[index].name) + 1;
					char* name_ptr = (char*)ptr;
					memcpy(name_ptr, data[data_index].types[index].name, type_name_size * sizeof(char));
					ptr += type_name_size * sizeof(char);

					type.name = name_ptr;
					ptr = function::AlignPointer(ptr, alignof(ReflectionType));
					type.fields.InitializeFromBuffer(ptr, data[data_index].types[index].fields.size);

					for (size_t field_index = 0; field_index < data[data_index].types[index].fields.size; field_index++) {
						size_t field_size = strlen(data[data_index].types[index].fields[field_index].name) + 1;
						char* field_ptr = (char*)ptr;
						memcpy(field_ptr, data[data_index].types[index].fields[field_index].name, sizeof(char) * field_size);
						ptr += sizeof(char) * field_size;

						type.fields[field_index].name = field_ptr;
						type.fields[field_index].info = data[data_index].types[index].fields[field_index].info;

						ReflectionStreamFieldType stream_type = data[data_index].types[index].fields[field_index].info.stream_type;
						ReflectionBasicFieldType basic_type = data[data_index].types[index].fields[field_index].info.basic_type;
						if (IsUserDefined(basic_type) || IsStream(stream_type) || IsEnum(basic_type)) {
							size_t definition_size = strlen(data[data_index].types[index].fields[field_index].definition) + 1;
							char* definition_ptr = (char*)ptr;
							memcpy(definition_ptr, data[data_index].types[index].fields[field_index].definition, sizeof(char) * definition_size);
							ptr += sizeof(char) * definition_size;

							type.fields[field_index].definition = definition_ptr;
						}
						else {
							type.fields[field_index].definition = data[data_index].types[index].fields[field_index].definition;
						}

						// Verify field tag
						const char* tag = data[data_index].types[index].fields[field_index].tag;
						if (tag != nullptr) {
							size_t tag_size = strlen(tag) + 1;
							char* tag_ptr = (char*)ptr;
							memcpy(tag_ptr, tag, tag_size * sizeof(char));
							ptr += sizeof(char) * tag_size;
							type.fields[field_index].tag = tag_ptr;
						}
						else {
							type.fields[field_index].tag = nullptr;
						}
					}

					if (data[data_index].types[index].tag != nullptr) {
						AddTypeTag(this, data[data_index].types[index].tag, type);
					}
					else {
						type.tag = nullptr;
					}

					ECS_RESOURCE_IDENTIFIER(type.name);
					ECS_ASSERT(!type_definitions.Insert(type, identifier));
				}

				for (size_t index = 0; index < data[data_index].enums.size; index++) {
					ReflectionEnum enum_;
					enum_.folder_hierarchy_index = folder_index;

					char* name_ptr = (char*)ptr;
					size_t type_name_size = strlen(data[data_index].enums[index].name) + 1;
					memcpy(name_ptr, data[data_index].enums[index].name, type_name_size * sizeof(char));
					ptr += sizeof(char) * type_name_size;

					enum_.name = name_ptr;
					ptr = function::AlignPointer(ptr, alignof(ReflectionEnum));
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

			size_t difference = function::PointerDifference((void*)ptr, allocation);
			ECS_ASSERT(difference <= total_memory);

			folders[folder_index].allocated_buffer = allocation;

			// If some fields depend directly on another type and when trying to get their byte size it fails,
			// then retry at the end
			ECS_STACK_CAPACITY_STREAM(uint3, retry_fields, 256);

			// Resolve references to the fields which reference other user defined types
			// Or custom containers
			ReflectionType* type = nullptr;

			auto update_type_field_user_defined = [&](const char* definition, size_t data_index, size_t index, size_t field_index) {
				// Do a lazy lookup for the type
				if (type == nullptr) {
					size_t type_name_size = strlen(data[data_index].types[index].name);
					type = type_definitions.GetValuePtr({ data[data_index].types[index].name, (unsigned int)type_name_size });
				}

				unsigned int container_type_index = FindReflectionContainerType(ToStream(definition));

				size_t byte_size = -1;
				size_t alignment = -1;
				if (container_type_index != -1) {
					// Get its byte size
					ReflectionContainerTypeByteSizeData byte_size_data;
					byte_size_data.definition = ToStream(definition);
					byte_size_data.reflection_manager = this;
					ulong2 byte_size_alignment = ECS_REFLECTION_CONTAINER_TYPES[container_type_index].byte_size(&byte_size_data);
					byte_size = byte_size_alignment.x;
					alignment = byte_size_alignment.y;

					if (byte_size == 0) {
						retry_fields.AddSafe({ (unsigned int)data_index, (unsigned int)index, (unsigned int)field_index });
					}
				}
				else {
					// Try to get the user defined type
					ReflectionType nested_type;
					if (TryGetType(definition, nested_type)) {
						byte_size = GetReflectionTypeByteSize(this, nested_type);
						alignment = GetReflectionTypeAlignment(this, nested_type);
					}
				}

				// It means it is not a container, nor a user defined which could be referenced
				// not a byte
				ECS_ASSERT(byte_size != -1 && alignment != -1);

				if (byte_size != 0) {
					size_t aligned_offset = function::AlignPointer(type->fields[field_index].info.pointer_offset, alignment);
					size_t alignment_difference = aligned_offset - type->fields[field_index].info.pointer_offset;

					type->fields[field_index].info.pointer_offset = aligned_offset;
					type->fields[field_index].info.byte_size = byte_size;
					for (size_t subindex = field_index + 1; subindex < type->fields.size; subindex++) {
						type->fields[subindex].info.pointer_offset += alignment_difference + byte_size;

						size_t current_alignment = GetFieldTypeAlignmentEx(this, type->fields[subindex]);
						aligned_offset = function::AlignPointer(type->fields[subindex].info.pointer_offset, current_alignment);

						alignment_difference += aligned_offset - type->fields[subindex].info.pointer_offset;
						type->fields[subindex].info.pointer_offset = aligned_offset;
					}
				}
			};
			
			for (size_t data_index = 0; data_index < data_count; data_index++) {
				for (size_t index = 0; index < data[data_index].types.size; index++) {
					for (size_t field_index = 0; field_index < data[data_index].types[index].fields.size; field_index++) {
						const ReflectionField* field = &data[data_index].types[index].fields[field_index];
						if (field->info.basic_type == ReflectionBasicFieldType::UserDefined 
							&& field->info.stream_type == ReflectionStreamFieldType::Basic) 
						{
							update_type_field_user_defined(field->definition, data_index, index, field_index);
						}
					}
				}
			}

			for (size_t retry_index = 0; retry_index < retry_fields.size; retry_index++) {
				size_t data_index = retry_fields[retry_index].x;
				size_t index = retry_fields[retry_index].y;
				size_t field_index = retry_fields[retry_index].z;

				update_type_field_user_defined(data[data_index].types[index].fields[field_index].definition, data_index, index, field_index);
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

			if (folders[folder_index].allocated_buffer != nullptr) {
				Deallocate(folders.allocator, folders[folder_index].allocated_buffer);
				folders[folder_index].allocated_buffer = nullptr;
			}
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
					type_indices.AddSafe(index);
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
			ReflectionType type = GetType(type_index);
			return type.tag;
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

		bool ReflectionManager::HasTypeTag(unsigned int type_index, const char* tag) const
		{
			return GetType(type_index).HasTag(tag);
		}

		bool ReflectionManager::HasTypeTag(const char* name, const char* tag) const
		{
			return GetType(name).HasTag(tag);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ReflectionManager::HasValidDependencies(const char* type_name) const
		{
			return HasValidDependencies(GetType(type_name));
		}

		bool ReflectionManager::HasValidDependencies(ReflectionType type) const
		{
			for (size_t index = 0; index < type.fields.size; index++) {
				if (type.fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					ReflectionType child_type;
					bool is_child_type = TryGetType(type.fields[index].definition, child_type);
					if (!is_child_type) {
						return false;
					}
					if (!HasValidDependencies(child_type)) {
						return false;
					}
				}
			}

			return true;
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

			// Process these files on separate threads only if their number is greater than thread count
			if (files_count < thread_count) {
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
			reflect_semaphore.target.store(reflect_thread_count, ECS_RELAXED);

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

		void ReflectionManager::SetInstanceDefaultData(const char* name, void* data) const
		{
			unsigned int type_index = type_definitions.Find(name);
			ECS_ASSERT(type_index != -1);
			SetInstanceDefaultData(type_index, data);
		}

		void ReflectionManager::SetInstanceDefaultData(unsigned int index, void* data) const
		{
			ReflectionType type = GetType(index);
			size_t type_size = GetReflectionTypeByteSize(this, type);

			memset(data, 0, type_size);

			// If it has default values, make sure to initialize them
			for (size_t field_index = 0; field_index < type.fields.size; field_index++) {
				if (type.fields[field_index].info.has_default_value) {
					memcpy(
						function::OffsetPointer(data, type.fields[field_index].info.pointer_offset),
						&type.fields[field_index].info.default_bool,
						type.fields[field_index].info.byte_size
					);
				}
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
							tag_name = nullptr;

#pragma region Skip macro definitions
							// Skip macro definitions - that define tags
							const char* verify_define_char = file_contents + words[index] - 1;
							verify_define_char = function::SkipWhitespace(verify_define_char, -1);
							verify_define_char = function::SkipCodeIdentifier(verify_define_char, -1);
							// If it is a pound, it must be a definition - skip it
							if (*verify_define_char == '#') {
								continue;
							}
#pragma endregion

#pragma region Skip Comments
							// Skip comments that contain ECS_REFLECT
							const char* verify_comment_char = file_contents + words[index] - 1;
							verify_comment_char = function::SkipWhitespace(verify_comment_char, -1);
							const char* verify_comment_last_new_line = function::FindCharacterReverse(verify_comment_char, file_contents, '\n');
							if (verify_comment_last_new_line == nullptr) {
								WriteErrorMessage(data, "Last new line could not be found when checking ECS_REFLECT for comment", index);
								return;
							}
							const char* comment_char = function::FindCharacterReverse(verify_comment_char, file_contents, '/');
							if (comment_char != nullptr && comment_char > verify_comment_last_new_line) {
								// It might be a comment
								if (comment_char[-1] == '/') {
									// It is a comment - skip it
									continue;
								}
								else if (comment_char[1] == '*') {
									// It is a comment skip it
									continue;
								}
							}
#pragma endregion

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

							// Closing braces can appear for brace default initialization or if some member functions are defined inlined.
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
									data->thread_memory.size += string_size + 1;

									// Record its tag
									data->total_memory += (string_size + 1) * sizeof(char);
									memcpy(new_tag_name, tag_name, sizeof(char) * string_size);
									new_tag_name[string_size] = '\0';
									data->types[data->types.size - 1].tag = new_tag_name;
								}
								else {
									data->types[data->types.size - 1].tag = nullptr;
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
			ptr = function::AlignPointer(ptr, alignof(ReflectionEnum));
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
			ptr = function::AlignPointer(ptr, alignof(ReflectionField));
			type.fields = Stream<ReflectionField>((void*)ptr, 0);
			data->thread_memory.size += sizeof(ReflectionField) * semicolon_positions.size + ptr - ptr_before;
			data->total_memory += sizeof(ReflectionField) * semicolon_positions.size + alignof(ReflectionField);
			if (data->thread_memory.size > data->thread_memory.capacity) {
				WriteErrorMessage(data, "Assigning type field stream failed, insufficient memory.", index);
				return;
			}

			if (next_line_positions.size > 0 && semicolon_positions.size > 0) {
				// determining each field
				unsigned short pointer_offset = 0;
				unsigned int last_new_line = next_line_positions[0];

				unsigned int current_semicolon_index = 0;
				for (size_t index = 0; index < next_line_positions.size - 1 && current_semicolon_index < semicolon_positions.size; index++) {
					// Check to see if a semicolon is in between the two new lines - if it is, then a definition might exist
					// for a data member or for a member function
					if (semicolon_positions[current_semicolon_index] < last_new_line) {
						current_semicolon_index++;
						if (current_semicolon_index >= semicolon_positions.size) {
							break;
						}
					}

					unsigned int current_new_line = next_line_positions[index + 1];
					if (semicolon_positions[current_semicolon_index] > last_new_line && semicolon_positions[current_semicolon_index] < current_new_line) {
						const char* field_start = opening_parenthese + next_line_positions[index];
						char* field_end = (char*)opening_parenthese + semicolon_positions[current_semicolon_index];

						field_end[0] = '\0';

						// Must check if it is a function definition - if it is then skip it
						const char* opened_paranthese = strchr(field_start, '(');
						field_end[0] = ';';

						if (opened_paranthese == nullptr) {
							bool succeded = AddTypeField(data, type, pointer_offset, field_start, field_end);

							if (!succeded) {
								WriteErrorMessage(data, "An error occured during field type determination.", index);
								return;
							}
						}
						current_semicolon_index++;
					}
					last_new_line = current_new_line;
				}
			}
			else {
				WriteErrorMessage(data, "There were no fields to reflect.", index);
				return;
			}
			data->types.Add(type);
			data->total_memory += sizeof(ReflectionType);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool AddTypeField(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* last_line_character, 
			const char* semicolon_character
		)
		{
			// Check to see if the field has a tag - all tags must appear after the semicolon
			// Make the tag default to nullptr
			type.fields[type.fields.size].tag = nullptr;

			const char* tag_character = semicolon_character + 1;
			const char* next_line_character = strchr(tag_character, '\n');
			if (next_line_character == nullptr) {
				return false;
			}
			const char* parsed_tag_character = function::SkipWhitespace(tag_character);
			// Some field determination functions write the field without knowing the tag. When the function exits, set the tag accordingly to 
			// what was determined here
			const char* final_field_tag = nullptr;
			// If they are the same, no tag
			if (parsed_tag_character != next_line_character) {
				// Check comments at the end of the line
				if (parsed_tag_character[0] != '/' || (parsed_tag_character[1] != '/' && parsed_tag_character[1] != '*')) {
					// Look for tag - there can be more tags separated by a space - change the space into a
					// _ when writing the tag
					char* ending_tag_character = (char*)function::SkipCodeIdentifier(parsed_tag_character);
					char* skipped_ending_tag_character = (char*)function::SkipWhitespace(ending_tag_character);

					// If there are more tags separated by spaces, then transform the spaces or the tabs into
					// _ when writing the tag
					while (skipped_ending_tag_character != next_line_character) {
						while (ending_tag_character != skipped_ending_tag_character) {
							*ending_tag_character = '_';
							ending_tag_character++;
						}

						ending_tag_character = (char*)function::SkipCodeIdentifier(skipped_ending_tag_character);
						skipped_ending_tag_character = (char*)function::SkipWhitespace(ending_tag_character);
					}

					*ending_tag_character = '\0';

					type.fields[type.fields.size].tag = parsed_tag_character;
					size_t character_count = strlen(type.fields[type.fields.size].tag) + 1;
					data->total_memory += character_count;

					final_field_tag = parsed_tag_character;
				}
			}

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
					info.stream_byte_size = info.byte_size;
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
							return false;
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

				ReflectionField& field = type.fields[type.fields.size - 1];
				field.tag = final_field_tag;

				const char* namespace_string = "ECSEngine::";
				size_t namespace_string_size = sizeof("ECSEngine::") - 1;

				// Need remove the namespace ECSEngine for template types and basic types
				if (memcmp(field.definition, namespace_string, namespace_string_size) == 0) {
					field.definition += namespace_string_size;

					// Don't forget to remove this from the total memory pool
					data->total_memory -= namespace_string_size;
				}
				char* ecsengine_namespace = (char*)strstr(field.definition, namespace_string);
				while (ecsengine_namespace != nullptr) {
					// Move back all the characters over the current characters
					char* after_characters = ecsengine_namespace + namespace_string_size;
					size_t after_character_count = strlen(after_characters);
					memcpy(ecsengine_namespace, after_characters, after_character_count * sizeof(char));
					ecsengine_namespace[after_character_count] = '\0';

					ecsengine_namespace = (char*)strstr(ecsengine_namespace, namespace_string);
					// Don't forget to remove this from the total memory pool
					data->total_memory -= namespace_string_size;
				}
			}
			return success;
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
					field.tag = type.fields[type.fields.size].tag;
					data->total_memory += strlen(field_name) + 1;
					type.fields.Add(field);
					success = true;
				}
			}
			return success;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool CheckFieldExplicitDefinition(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type, 
			unsigned short& pointer_offset,
			const char* last_line_character
		)
		{
			const char* explicit_definition = strstr(last_line_character + 1, STRING(ECS_EXPLICIT_TYPE_REFLECT));
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

					pointer_offset = function::AlignPointer(pointer_offset, field.info.byte_size / field.info.basic_type_count);

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

		bool DeduceFieldTypePointer(
			ReflectionManagerParseStructuresThreadTaskData* data,
			ReflectionType& type,
			unsigned short& pointer_offset,
			const char* field_name,
			const char* last_line_character
		)
		{
			// Test first pointer type
			const char* star_ptr = strchr(last_line_character + 1, '*');
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
				
				pointer_offset = function::AlignPointer(before_pointer_offset, alignof(void*));
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
			// Must search new line character + 1 since it might be made \0 for tags
			const char* stream_ptr = strstr(new_line_character + 1, STRING(ResizableStream));

			auto parse_stream_type = [&](ReflectionStreamFieldType stream_type, size_t byte_size, const char* stream_name) {
				ReflectionField field;
				field.name = field_name;

				unsigned short before_pointer_offset = pointer_offset;
				char* left_bracket = (char*)strchr(stream_ptr, '<');
				if (left_bracket == nullptr) {
					ECS_FORMAT_TEMP_STRING(temp_message, "Incorrect {#}, missing <.", stream_name);
					WriteErrorMessage(data, temp_message.buffer, -1);
					return true;
				}

				char* right_bracket = (char*)strchr(left_bracket, '>');
				if (right_bracket == nullptr) {
					ECS_FORMAT_TEMP_STRING(temp_message, "Incorrect {#}, missing >.", stream_name);
					WriteErrorMessage(data, temp_message.buffer, -1);
					return true;
				}

				// if nested templates, we need to find the last '>'
				char* next_right_bracket = (char*)strchr(right_bracket + 1, '>');
				while (next_right_bracket != nullptr) {
					right_bracket = next_right_bracket;
					next_right_bracket = (char*)strchr(right_bracket + 1, '>');
				}

				// Make the left bracket \0 because otherwise it will continue to get parsed
				char left_bracket_before = left_bracket[-1];
				left_bracket[0] = '\0';
				left_bracket[-1] = '\0';
				*right_bracket = '\0';
				DeduceFieldTypeExtended(
					data,
					pointer_offset,
					right_bracket - 1,
					field
				);
				left_bracket[0] = '<';
				left_bracket[-1] = left_bracket_before;
				right_bracket[0] = '>';
				right_bracket[1] = '\0';

				// All streams have aligned 8, the highest
				pointer_offset = function::AlignPointer(before_pointer_offset, alignof(Stream<void>));
				field.info.pointer_offset = pointer_offset;
				field.info.stream_type = stream_type;
				field.info.stream_byte_size = field.info.byte_size;
				field.info.byte_size = byte_size;
				field.definition = function::SkipWhitespace(new_line_character + 1);

				pointer_offset += byte_size;

				data->total_memory += strlen(field.name) + 1;
				data->total_memory += strlen(field.definition) + 1;
				type.fields.Add(field);
			};

			if (stream_ptr != nullptr) {
				parse_stream_type(ReflectionStreamFieldType::ResizableStream, sizeof(ResizableStream<void>), STRING(ResizableStream));
				return true;
			}

			// Must search new line character + 1 since it might be made \0 for tags
			stream_ptr = strstr(new_line_character + 1, STRING(CapacityStream));
			if (stream_ptr != nullptr) {
				parse_stream_type(ReflectionStreamFieldType::CapacityStream, sizeof(CapacityStream<void>), STRING(CapacityStream));
				return true;
			}

			// Must search new line character + 1 since it might be made \0 for tags
			stream_ptr = strstr(new_line_character + 1, STRING(Stream));
			if (stream_ptr != nullptr) {
				parse_stream_type(ReflectionStreamFieldType::Stream, sizeof(Stream<void>), STRING(Stream));
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
			if (*current_character == '>') {
				// Template type - remove
				unsigned int hey_there = 0;
			}

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
					pointer_offset = function::AlignPointer(pointer_offset, field.info.byte_size / component_count);
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
				field.info.stream_type = ReflectionStreamFieldType::Basic;
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

#undef CASE
#undef CASE234
#undef CASE1234

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

		size_t GetReflectionTypeByteSize(const ReflectionManager* reflection_manager, ReflectionType type)
		{
			size_t alignment = GetReflectionTypeAlignment(reflection_manager, type);

			size_t byte_size = type.fields[type.fields.size - 1].info.pointer_offset + type.fields[type.fields.size - 1].info.byte_size;
			size_t remainder = byte_size % alignment;

			return remainder == 0 ? byte_size : byte_size + alignment - remainder;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionTypeAlignment(const ReflectionManager* reflection_manager, ReflectionType type)
		{
			size_t alignment = 0;

			for (size_t index = 0; index < type.fields.size && alignment < alignof(void*); index++) {
				if (type.fields[index].info.basic_type == ReflectionBasicFieldType::UserDefined) {
					ReflectionType nested_type;
					if (reflection_manager->TryGetType(type.fields[index].definition, nested_type)) {
						alignment = std::max(alignment, GetReflectionTypeAlignment(reflection_manager, nested_type));
					}
					else {
						// The type is not found, assume alignment of alignof(void*)
						alignment = alignof(void*);
					}
				}
				else {
					if (type.fields[index].info.stream_type == ReflectionStreamFieldType::Basic ||
						type.fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
						alignment = std::max(alignment, GetFieldTypeAlignment(type.fields[index].info.basic_type));
					}
					else {
						alignment = std::max(alignment, GetFieldTypeAlignment(type.fields[index].info.stream_type));
					}
				}
			}

			return alignment;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetFieldTypeAlignmentEx(const ReflectionManager* reflection_manager, ReflectionField field)
		{
			if (field.info.basic_type != ReflectionBasicFieldType::UserDefined) {
				if (field.info.stream_type == ReflectionStreamFieldType::Basic || field.info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					return GetFieldTypeAlignment(field.info.basic_type);
				}
				else {
					return GetFieldTypeAlignment(field.info.stream_type);
				}
			}
			else {
				ReflectionType nested_type;
				if (reflection_manager->TryGetType(field.definition, nested_type)) {
					return GetReflectionTypeAlignment(reflection_manager, nested_type);
				}
				else {
					// Assume its a container type - which should always have an alignment of alignof(void*)
					return alignof(void*);
				}
			}
			
			// Should not reach here
			return 0;
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

		size_t GetBasicTypeArrayElementSize(const ReflectionFieldInfo& info)
		{
			return info.byte_size / info.basic_type_count;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void* GetReflectionFieldStreamBuffer(const ReflectionFieldInfo& info, const void* data)
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

		size_t GetReflectionFieldStreamSize(const ReflectionFieldInfo& info, const void* data)
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

		Stream<void> GetReflectionFieldStreamVoid(const ReflectionFieldInfo& info, const void* data)
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

		size_t GetReflectionFieldStreamElementByteSize(const ReflectionFieldInfo& info)
		{
			return info.stream_byte_size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		unsigned char GetReflectionFieldPointerIndirection(const ReflectionFieldInfo& info)
		{
			ECS_ASSERT(info.stream_type == ReflectionStreamFieldType::Pointer);
			return info.basic_type_count;
		}

		ReflectionBasicFieldType ConvertBasicTypeMultiComponentToSingle(ReflectionBasicFieldType type)
		{
			if (IsBoolBasicTypeMultiComponent(type)) {
				return ReflectionBasicFieldType::Bool;
			}
			else if (IsFloatBasicTypeMultiComponent(type)) {
				return ReflectionBasicFieldType::Float;
			}
			else if (IsDoubleBasicTypeMultiComponent(type)) {
				return ReflectionBasicFieldType::Double;
			}
			else if (IsIntegralMultiComponent(type)) {
				unsigned char type_char = (unsigned char)type;
				if ((unsigned char)ReflectionBasicFieldType::Char2 >= type_char && (unsigned char)ReflectionBasicFieldType::ULong2) {
					return (ReflectionBasicFieldType)((unsigned char)ReflectionBasicFieldType::Int8 + type_char - (unsigned char)ReflectionBasicFieldType::Char2);
				}
				else if ((unsigned char)ReflectionBasicFieldType::Char3 >= type_char && (unsigned char)ReflectionBasicFieldType::ULong3) {
					return (ReflectionBasicFieldType)((unsigned char)ReflectionBasicFieldType::Int8 + type_char - (unsigned char)ReflectionBasicFieldType::Char3);
				}
				else if ((unsigned char)ReflectionBasicFieldType::Char4 >= type_char && (unsigned char)ReflectionBasicFieldType::ULong4) {
					return (ReflectionBasicFieldType)((unsigned char)ReflectionBasicFieldType::Int8 + type_char - (unsigned char)ReflectionBasicFieldType::Char4);
				}
			}

			return ReflectionBasicFieldType::COUNT;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}
