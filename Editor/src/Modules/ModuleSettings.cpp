#include "editorpch.h"
#include "ModuleSettings.h"
#include "Editor/EditorState.h"
#include "Module.h"
#include "ModuleFile.h"
#include "../Project/ProjectFolders.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

#define MODULE_SETTINGS_REFLECT_TAG "SETTINGS"

size_t SearchSetting(Stream<EditorModuleReflectedSetting> settings, Stream<char> name) {
	for (size_t index = 0; index < settings.size; index++) {
		Stream<char> current_name = settings[index].name;
		if (current_name.buffer == name.buffer || name == current_name) {
			return index;
		}
	}
	return -1;
}

#define SEARCH_OPTIONS(settings_id, options_name, indices_name) ECS_STACK_CAPACITY_STREAM(char, suffix_stream, 256); \
																ECS_STACK_CAPACITY_STREAM(unsigned int, indices_name, 64); \
																UIReflectionDrawerTag settings_tag = { MODULE_SETTINGS_REFLECT_TAG, false }; \
																\
																suffix_stream.AddStreamSafe(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT); \
																ConvertIntToChars(suffix_stream, settings_id); \
\
																UIReflectionDrawerSearchOptions options_name; \
																options_name.indices = &indices_name; \
																options_name.suffix = suffix_stream; \
																options_name.include_tags = { &settings_tag, 1 };

#define SEARCH_OPTIONS_NO_SETTINGS(options_name, indices_name)  ECS_STACK_CAPACITY_STREAM(unsigned int, indices_name, 64); \
																UIReflectionDrawerTag settings_tag = { MODULE_SETTINGS_REFLECT_TAG, false}; \
																\
																UIReflectionDrawerSearchOptions options_name; \
																options_name.indices = &indices_name; \
																options_name.include_tags = { &settings_tag, 1 };

// -----------------------------------------------------------------------------------------------------------------------------

void AllocateModuleSettings(
	const EditorState* editor_state,
	unsigned int module_index,
	CapacityStream<EditorModuleReflectedSetting>& settings, 
	AllocatorPolymorphic allocator
)
{
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);

	SEARCH_OPTIONS_NO_SETTINGS(options, type_indices);
	editor_state->ui_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (type_indices.size > 0) {
		settings.Initialize(allocator, 0, type_indices.size);

		// For each instance, create a corresponding void* data buffer
		const ReflectionManager* reflection_manager = editor_state->ModuleReflectionManager();

		// Allocate a buffer for the instance pointers
		EditorModule* editor_module = editor_state->project_modules->buffer + module_index;

		for (size_t index = 0; index < type_indices.size; index++) {
			const UIReflectionType* ui_type = editor_state->ui_reflection->GetType(type_indices[index]);

			Stream<char> type_name = ui_type->name;
			const ReflectionType* type = reflection_manager->GetType(type_name);
			size_t type_size = GetReflectionTypeByteSize(type);

			// The name needs to be allocated aswell - because the UI reflection types
			// can be destroyed by the reflection underneath us
			//type_name = StringCopy(allocator, type_name).buffer;

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

	SEARCH_OPTIONS(settings_id, options, instance_indices);

	// The tagged types are excluded since they do not get a reflection type in the first place
	instance_indices.size = editor_state->ui_reflection->CreateInstanceForHierarchy(hierarchy_index, options);
	instance_indices.AssertCapacity();

	if (instance_indices.size > 0) {
		// For each instance, create a corresponding void* data buffer
		const ReflectionManager* reflection_manager = editor_state->ModuleReflectionManager();

		// Allocate a buffer for the instance pointers
		EditorModule* editor_module = editor_state->project_modules->buffer + module_index;
		ECS_ASSERT(instance_indices.size < settings.capacity);
		settings.size = 0;

		for (size_t index = 0; index < instance_indices.size; index++) {
			UIReflectionInstance* instance = editor_state->ui_reflection->GetInstance(instance_indices[index]);

			Stream<char> type_name = instance->type_name;
			const ReflectionType* type = reflection_manager->GetType(type_name);
			size_t type_size = GetReflectionTypeByteSize(type);

			// The name needs to be allocated aswell - because the UI reflection types
			// can be destroyed by the reflection underneath us
			//type_name = StringCopy(allocator, type_name).buffer;

			// Allocate the memory for an instance of the type
			void* instance_memory = Allocate(allocator, type_size);
			// Set the memory to 0
			memset(instance_memory, 0, type_size);
			// This will alias the name of the UI reflection type and that's fine
			settings.Add({ instance_memory, type_name });

			editor_state->ui_reflection->BindInstancePtrs(instance, instance_memory);
			editor_state->ui_reflection->reflection->SetInstanceDefaultData(type->name, instance_memory);

			editor_state->ui_reflection->AssignInstanceResizableAllocator(instance, allocator);
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
	EditorModule* editor_module = editor_state->project_modules->buffer + module_index;
	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);

	SEARCH_OPTIONS(settings_id, options, type_indices);

	// Destroy all instances now
	editor_state->ui_reflection->DestroyAllInstancesFromFolderHierarchy(hierarchy_index, options);
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
	full_path.AddStreamSafe(MODULE_SETTINGS_EXTENSION);
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

void GetModuleAvailableSettings(const EditorState* editor_state, unsigned int module_index, CapacityStream<Stream<wchar_t>>& paths, AllocatorPolymorphic allocator)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, module_folder, 512);
	GetModuleSettingsFolderPath(editor_state, module_index, module_folder);

	struct FunctorData {
		CapacityStream<Stream<wchar_t>>* paths;
		AllocatorPolymorphic allocator;
	};

	FunctorData functor_data = { &paths, allocator };
	ForEachFileInDirectory(module_folder, &functor_data, [](Stream<wchar_t> path, void* _data) {
		FunctorData* data = (FunctorData*)_data;
		Stream<wchar_t> stem = PathStem(path);
		data->paths->AddAssert(StringCopy(data->allocator, stem));
		return true;
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetModuleSettingsUITypesIndices(const EditorState* editor_state, unsigned int module_index, CapacityStream<unsigned int>& indices)
{
	SEARCH_OPTIONS_NO_SETTINGS(options, dummy_indices);
	options.indices = &indices;
	editor_state->ui_reflection->GetHierarchyTypes(GetModuleReflectionHierarchyIndex(editor_state, module_index), options);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetModuleSettingsUIInstancesIndices(const EditorState* editor_state, unsigned int module_index, unsigned int settings_id, ECSEngine::CapacityStream<unsigned int>& indices)
{
	SEARCH_OPTIONS(settings_id, options, dummy_indices);
	options.indices = &indices;
	editor_state->ui_reflection->GetHierarchyInstances(GetModuleReflectionHierarchyIndex(editor_state, module_index), options);
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
	SEARCH_OPTIONS_NO_SETTINGS(options, indices);

	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->ui_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (indices.size > 0) {
		AllocatorPolymorphic editor_allocator = editor_state->editor_allocator;
		ECS_STACK_VOID_STREAM(file_buffering, ECS_KB * 16);
		Optional<OwningBufferedFileReadInstrument> read_instrument = OwningBufferedFileReadInstrument::Initialize(path, file_buffering);
		if (!read_instrument.has_value) {
			return false;
		}

		DeserializeOptions deserialize_options;
		deserialize_options.field_allocator = allocator;
		
		const Reflection::ReflectionManager* reflection = editor_state->GlobalReflectionManager();
		//uintptr_t file_ptr = (uintptr_t)file_data.buffer;

		// Need the deserialize table in order to determine the setting index
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB * 8);

		// Need to continue deserializing while the file_ptr is smaller than the limit of the file
		while (!read_instrument.value.IsEndReached()) {
			stack_allocator.Clear();
			DeserializeFieldTable field_table = DeserializeFieldTableFromData(&read_instrument.value, &stack_allocator);
			// It failed
			if (field_table.types.size == 0) {
				return false;
			}

			size_t settings_index = SearchSetting(settings, field_table.types[0].name);

			if (settings_index == -1) {
				// The type no longer exists, just skip it
				IgnoreDeserialize(&read_instrument.value, field_table);
			}
			else {
				deserialize_options.field_table = &field_table;
				ECS_DESERIALIZE_CODE code = Deserialize(
					reflection,
					reflection->GetType(settings[settings_index].name),
					settings[settings_index].data,
					&read_instrument.value,
					&deserialize_options
				);

				// An error has occured - cannot recover the data from the file.
				if (code != ECS_DESERIALIZE_OK) {
					return false;
				}
			}
		}
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
	// Cannot use the serialization directly into the file - must manually walk through all types and serialize it
	// into a memory buffer
	SEARCH_OPTIONS_NO_SETTINGS(options, indices);

	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->ui_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (indices.size > 0) {
		ECS_STACK_VOID_STREAM(file_buffering, ECS_KB * 16);
		return OwningBufferedFileWriteInstrument::WriteTo(path, file_buffering, [&](WriteInstrument* write_instrument) {
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(const ReflectionType*, module_types, indices.size);
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, instance_memory, indices.size);

			// Get the reflection types in order to serialize
			ReflectionManager* reflection = editor_state->ui_reflection->reflection;
			for (size_t index = 0; index < indices.size; index++) {
				module_types[index] = reflection->GetType(editor_state->module_reflection->GetType(indices[index])->name);
				size_t setting_index = SearchSetting(settings, module_types[index]->name);
				instance_memory[index] = settings[setting_index].data;
			}

			// Write the data into the buffer
			for (size_t index = 0; index < indices.size; index++) {
				if (!Serialize(reflection, module_types[index], instance_memory[index], write_instrument)) {
					return false;
				}
			}
			return true;
		});
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
	SEARCH_OPTIONS_NO_SETTINGS(options, indices);

	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->ui_reflection->GetHierarchyTypes(hierarchy_index, options);

	if (indices.size > 0) {
		for (size_t index = 0; index < indices.size; index++) {
			UIReflectionType* ui_type = editor_state->ui_reflection->GetType(indices[index]);
			const ReflectionType* type = editor_state->ModuleReflectionManager()->GetType(ui_type->name);
			size_t setting_index = SearchSetting(settings, ui_type->name);
			void* instance_memory = settings[setting_index].data;

			editor_state->ModuleReflectionManager()->SetInstanceDefaultData(type->name, instance_memory);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------
