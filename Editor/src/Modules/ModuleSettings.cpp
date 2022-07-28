#include "editorpch.h"
#include "ModuleSettings.h"
#include "Editor\EditorState.h"
#include "Module.h"
#include "ModuleFile.h"
#include "../Project/ProjectFolders.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

#define MODULE_SETTINGS_REFLECT_TAG "SETTINGS"

size_t SearchSetting(Stream<EditorModuleReflectedSetting> settings, const char* name) {
	for (size_t index = 0; index < settings.size; index++) {
		const char* current_name = settings[index].name;
		if (current_name == name || function::CompareStrings(name, current_name)) {
			return index;
		}
	}
	return -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

void AllocateModuleSettings(
	const EditorState* editor_state,
	unsigned int module_index,
	CapacityStream<EditorModuleReflectedSetting>& settings, 
	AllocatorPolymorphic allocator
)
{
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);

	ECS_STACK_CAPACITY_STREAM(unsigned int, type_indices, 64);
	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;
	
	UIReflectionDrawerSearchOptions options;
	options.indices = &type_indices;
	options.include_tags = { &settings_tag, 1 };
	editor_state->module_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (type_indices.size > 0) {
		settings.Initialize(allocator, 0, type_indices.size);

		// For each instance, create a corresponding void* data buffer
		ReflectionManager* reflection_manager = editor_state->module_reflection->reflection;

		// Allocate a buffer for the instance pointers
		EditorModule* editor_module = editor_state->project_modules->buffer + module_index;

		for (size_t index = 0; index < type_indices.size; index++) {
			UIReflectionType* ui_type = editor_state->module_reflection->GetTypePtr(type_indices[index]);

			const char* type_name = ui_type->name;
			ReflectionType type = reflection_manager->GetType(type_name);
			size_t type_size = GetReflectionTypeByteSize(reflection_manager, type);

			// The name needs to be allocated aswell - because the UI reflection types
			// can be destroyed by the reflection underneath us
			//type_name = function::StringCopy(allocator, type_name).buffer;

			// Allocate the memory for an instance of the type
			void* instance_memory = Allocate(allocator, type_size);
			// Set the memory to 0
			memset(instance_memory, 0, type_size);
			// This will alias the name of the UI reflection type and that's fine
			settings.Add({ instance_memory, type_name });
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void CreateModuleSettings(
	const EditorState* editor_state,
	unsigned int module_index,
	CapacityStream<EditorModuleReflectedSetting>& settings,
	AllocatorPolymorphic allocator,
	unsigned int settings_id
)
{
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);

	ECS_STACK_CAPACITY_STREAM(char, suffix_stream, 256);
	suffix_stream.AddStreamSafe(ToStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT));
	function::ConvertIntToChars(suffix_stream, settings_id);

	ECS_STACK_CAPACITY_STREAM(unsigned int, instance_indices, 64);
	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;

	UIReflectionDrawerSearchOptions options;
	options.indices = &instance_indices;
	options.suffix = suffix_stream.buffer;
	options.include_tags = { &settings_tag, 1 };

	// The tagged types are excluded since they do not get a reflection type in the first place
	instance_indices.size = editor_state->module_reflection->CreateInstanceForHierarchy(hierarchy_index, options);
	instance_indices.AssertCapacity();

	if (instance_indices.size > 0) {
		// For each instance, create a corresponding void* data buffer
		ReflectionManager* reflection_manager = editor_state->module_reflection->reflection;

		// Allocate a buffer for the instance pointers
		EditorModule* editor_module = editor_state->project_modules->buffer + module_index;
		ECS_ASSERT(instance_indices.size < settings.capacity);
		settings.size = 0;

		for (size_t index = 0; index < instance_indices.size; index++) {
			UIReflectionInstance* instance = editor_state->module_reflection->GetInstancePtr(instance_indices[index]);

			const char* type_name = instance->type_name;
			ReflectionType type = reflection_manager->GetType(type_name);
			size_t type_size = GetReflectionTypeByteSize(reflection_manager, type);

			// The name needs to be allocated aswell - because the UI reflection types
			// can be destroyed by the reflection underneath us
			//type_name = function::StringCopy(allocator, type_name).buffer;

			// Allocate the memory for an instance of the type
			void* instance_memory = Allocate(allocator, type_size);
			// Set the memory to 0
			memset(instance_memory, 0, type_size);
			// This will alias the name of the UI reflection type and that's fine
			settings.Add({ instance_memory, type_name });

			editor_state->module_reflection->BindInstancePtrs(instance, instance_memory);
			editor_state->module_reflection->reflection->SetInstanceDefaultData(type.name, instance_memory);

			editor_state->module_reflection->AssignInstanceResizableAllocator(instance, allocator);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool CreateModuleSettingsFolder(const EditorState* editor_state, unsigned int module_index)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetModuleSettingsFolderPath(editor_state, module_index, absolute_path);
	return CreateFolder(absolute_path);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool CreateModuleSettingsFile(const EditorState* editor_state, unsigned int module_index, ECSEngine::Stream<wchar_t> name)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetModuleSettingsFilePath(editor_state, module_index, name, absolute_path);

	ECS_FILE_HANDLE file_handle = 0;
	ECS_FILE_STATUS_FLAGS status = FileCreate(absolute_path, &file_handle);
	if (status != ECS_FILE_STATUS_OK) {
		return false;
	}
	
	CloseFile(file_handle);
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void DestroyModuleSettings(
	const EditorState* editor_state,
	unsigned int module_index, 
	Stream<EditorModuleReflectedSetting> settings,
	unsigned int settings_id
)
{
	EDITOR_STATE(editor_state);

	EditorModule* editor_module = editor_state->project_modules->buffer + module_index;
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);

	ECS_STACK_CAPACITY_STREAM(unsigned int, type_indices, 64);
	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;

	ECS_STACK_CAPACITY_STREAM(char, suffix_stream, 256);
	suffix_stream.AddStreamSafe(ToStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT));
	function::ConvertIntToChars(suffix_stream, settings_id);

	UIReflectionDrawerSearchOptions options;
	options.indices = &type_indices;
	options.include_tags = { &settings_tag, 1 };
	options.suffix = suffix_stream.buffer;

	// Destroy all instances now
	editor_state->module_reflection->DestroyAllInstancesFromFolderHierarchy(hierarchy_index, options);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetModuleSettingsFilePath(
	const EditorState* editor_state, 
	unsigned int module_index, 
	Stream<wchar_t> filename, 
	CapacityStream<wchar_t>& full_path
)
{
	GetModuleSettingsFolderPath(editor_state, module_index, full_path);
	full_path.Add(ECS_OS_PATH_SEPARATOR);
	full_path.AddStream(filename);
	full_path.AddStreamSafe(ToStream(MODULE_SETTINGS_EXTENSION));
	full_path[full_path.size] = L'\0';
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetModuleSettingsFolderPath(const EditorState* editor_state, unsigned int module_index, CapacityStream<wchar_t>& path)
{
	GetProjectConfigurationModuleFolder(editor_state, path);
	path.Add(ECS_OS_PATH_SEPARATOR);

	const EditorModule* module = editor_state->project_modules->buffer + module_index;
	path.AddStreamSafe(module->library_name);
	path[path.size] = L'\0';
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetModuleSettingsUITypesIndices(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& indices)
{
	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;

	UIReflectionDrawerSearchOptions options;
	options.indices = &indices;
	options.include_tags = { &settings_tag, 1 };
	editor_state->module_reflection->GetHierarchyTypes(GetModuleReflectionHierarchyIndex(editor_state, module_index), options);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetModuleSettingsUIInstancesIndices(const EditorState* editor_state, unsigned int module_index, unsigned int settings_id, ECSEngine::CapacityStream<unsigned int>& indices)
{
	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;

	ECS_STACK_CAPACITY_STREAM(char, suffix_stream, 512);
	suffix_stream.AddStreamSafe(ToStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT));
	function::ConvertIntToChars(suffix_stream, settings_id);

	UIReflectionDrawerSearchOptions options;
	options.indices = &indices;
	options.include_tags = { &settings_tag, 1 };
	options.suffix = suffix_stream.buffer;
	editor_state->module_reflection->GetHierarchyInstances(GetModuleReflectionHierarchyIndex(editor_state, module_index), options);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadModuleSettings(
	const EditorState* editor_state, 
	unsigned int module_index, 
	Stream<wchar_t> path, 
	Stream<EditorModuleReflectedSetting> settings,
	AllocatorPolymorphic allocator
)
{
	// Cannot use the deserialization directly from file - must manually walk through all types and check that it exists
	ECS_STACK_CAPACITY_STREAM(unsigned int, indices, 64);

	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;

	UIReflectionDrawerSearchOptions options;
	options.include_tags = { &settings_tag, 1 };
	options.indices = &indices;

	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->module_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (indices.size > 0) {
		AllocatorPolymorphic editor_allocator = GetAllocatorPolymorphic(editor_state->editor_allocator);

		Stream<void> file_data = ReadWholeFileBinary(path, editor_allocator);
		if (file_data.buffer == nullptr) {
			return false;
		}

		DeserializeOptions deserialize_options;
		deserialize_options.backup_allocator = allocator;
		deserialize_options.field_allocator = allocator;
		
		const Reflection::ReflectionManager* reflection = editor_state->module_reflection->reflection;
		uintptr_t file_ptr = (uintptr_t)file_data.buffer;

		// Need to continue deserializing while the file_ptr is smaller than the limit of the file
		uintptr_t file_ptr_limit = file_ptr + file_data.size;
		while (file_ptr < file_ptr_limit) {
			// Need the deserialize table in order to determine the setting index
			ECS_STACK_CAPACITY_STREAM(char, table_memory, ECS_KB * 8);

			CapacityStream<void> void_memory = table_memory;
			DeserializeFieldTable field_table = DeserializeFieldTableFromData(file_ptr, &void_memory);
			// It failed
			if (field_table.types.size == 0) {
				editor_state->editor_allocator->Deallocate(file_data.buffer);
				return false;
			}

			// Null terminate the name
			char previous_char = field_table.types[0].name[field_table.types[0].name.size];
			field_table.types[0].name[field_table.types[0].name.size] = '\0';
			size_t settings_index = SearchSetting(settings, field_table.types[0].name.buffer);
			field_table.types[0].name[field_table.types[0].name.size] = previous_char;

			if (settings_index == -1) {
				// The type no longer exists, just skip it
				IgnoreDeserialize(file_ptr, field_table);
			}
			else {
				deserialize_options.field_table = &field_table;
				ECS_DESERIALIZE_CODE code = Deserialize(
					reflection,
					reflection->GetType(settings[settings_index].name),
					settings[settings_index].data,
					file_ptr,
					&deserialize_options
				);

				// An error has occured - cannot recover the data from the file.
				if (code != ECS_DESERIALIZE_OK) {
					editor_state->editor_allocator->Deallocate(file_data.buffer);
					return false;
				}
			}
		}

		// Deallocate the file content
		editor_state->editor_allocator->Deallocate(file_data.buffer);
	}
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void RemoveModuleSettingsFileAndFolder(const EditorState* editor_state, unsigned int module_index)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetModuleSettingsFolderPath(editor_state, module_index, absolute_path);
	ECS_ASSERT(RemoveFolder(absolute_path));
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveModuleSettings(const EditorState* editor_state, unsigned int module_index, Stream<wchar_t> path, Stream<EditorModuleReflectedSetting> settings)
{
	// Use text serialization
	// Cannot use the serialization directly into the file - must manually walk through all types and serialize it
	// into a memory buffer
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, 64);

	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;
	UIReflectionDrawerSearchOptions options;
	options.indices = &indices;
	options.include_tags = { &settings_tag, 1 };

	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->module_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (indices.size > 0) {
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(ReflectionType, module_types, indices.size);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, instance_memory, indices.size);

		// Get the reflection types in order to serialize
		ReflectionManager* reflection = editor_state->module_reflection->reflection;
		for (size_t index = 0; index < indices.size; index++) {
			module_types[index] = reflection->GetType(editor_state->module_reflection->GetType(indices[index]).name);
			size_t setting_index = SearchSetting(settings, module_types[index].name);
			instance_memory[index] = settings[setting_index].data;
		}

		// Now determine how much memory is needed in order to allocate a single buffer and serialize into it
		size_t buffer_size = 0;
		for (size_t index = 0; index < indices.size; index++) {
			size_t serialize_size = SerializeSize(reflection, module_types[index], instance_memory[index]);
			ECS_ASSERT(serialize_size != -1);
			buffer_size += serialize_size;
		}

		// Allocate a buffer of the corresponding size
		void* buffer_allocation = editor_state->editor_allocator->Allocate(buffer_size);

		// Write the data into the buffer
		uintptr_t buffer = (uintptr_t)buffer_allocation;
		for (size_t index = 0; index < indices.size; index++) {
			// The check for the code can be omitted since it would fail at the SerializeSize pass
			Serialize(reflection, module_types[index], instance_memory[index], buffer);
		}

		size_t difference = buffer - (uintptr_t)buffer_allocation;
		ECS_ASSERT(difference <= buffer_size);

		// Now write the buffer to the file
		bool success = WriteBufferToFileBinary(path, { buffer_allocation, difference }) == ECS_FILE_STATUS_OK;

		// Deallocate the buffer
		editor_state->editor_allocator->Deallocate(buffer_allocation);
		return success;
	}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetModuleDefaultSettings(
	const EditorState* editor_state, 
	unsigned int module_index, 
	Stream<EditorModuleReflectedSetting> settings
)
{
	unsigned int type_count = editor_state->module_reflection->GetTypeCount();
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, type_count);

	const char* settings_tag = MODULE_SETTINGS_REFLECT_TAG;
	UIReflectionDrawerSearchOptions options;
	options.include_tags = { &settings_tag, 1 };
	options.indices = &indices;

	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->module_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (indices.size > 0) {
		for (size_t index = 0; index < indices.size; index++) {
			UIReflectionType* ui_type = editor_state->module_reflection->GetTypePtr(indices[index]);
			ReflectionType type = editor_state->module_reflection->reflection->GetType(ui_type->name);
			size_t setting_index = SearchSetting(settings, ui_type->name);
			void* instance_memory = settings[setting_index].data;
			editor_state->module_reflection->reflection->SetInstanceDefaultData(type.name, instance_memory);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------
