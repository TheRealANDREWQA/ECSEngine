#include "editorpch.h"
#include "EditorScene.h"
#include "EditorState.h"

#include "../Project/ProjectFolders.h"
#include "Modules/Module.h"
#include "EditorComponents.h"
#include "ECSEngineEntitiesSerialize.h"

using namespace ECSEngine;

#pragma region Internal functions

// ----------------------------------------------------------------------------------------------

SerializeEntityManagerComponentTable SerializeComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator,
	Stream<const Reflection::ReflectionType*> link_types
) {
	SerializeEntityManagerComponentTable table;

	ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerComponentInfo, overrides, 512);

	// Walk through the module reflection and retrieve their overrides
	ProjectModules project_modules = *editor_state->project_modules;
	for	(size_t index = 0; index < project_modules.size; index++) {
		EDITOR_MODULE_CONFIGURATION loaded_configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (loaded_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
			// There is a suitable loaded configuration
			// Check for its overrides
			const EditorModuleInfo* info = GetModuleInfo(editor_state, index, loaded_configuration);
			for (size_t override_index = 0; override_index < info->serialize_streams.serialize_components.size; override_index++) {
				// Only if the name is specified, otherwise skip it (it is malformed)
				if (info->serialize_streams.serialize_components[override_index].name.size > 0) {
					overrides.Add(info->serialize_streams.serialize_components[override_index]);
				}
			}
		}
	}

	// Get the link types
	ECS_ASSERT(overrides.capacity - overrides.size >= link_types.size, "Too little override slots");
	ConvertLinkTypesToSerializeEntityManagerUnique(editor_state->editor_components.internal_manager, allocator, link_types, overrides.buffer + overrides.size);
	overrides.size += link_types.size;

	CreateSerializeEntityManagerComponentTable(table, editor_state->editor_components.internal_manager, allocator, overrides);

	//CreateSerializeEntityManagerComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
	//CreateSerializeEntityManagerComponentTable(table, editor_state->module_reflection->reflection, allocator, overrides);
	
	// Add the overrides manually
	for (unsigned int index = 0; index < overrides.size; index++) {
		Component component = editor_state->editor_components.GetComponentID(overrides[index].name);
		ECS_ASSERT(!table.Insert(overrides[index], component));
	}


	return table;
}

// ----------------------------------------------------------------------------------------------

SerializeEntityManagerSharedComponentTable SerializeSharedComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator,
	Stream<const Reflection::ReflectionType*> link_types
) {
	SerializeEntityManagerSharedComponentTable table;

	ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerSharedComponentInfo, overrides, 256);

	// Walk through the module reflection and retrieve their overrides
	ProjectModules project_modules = *editor_state->project_modules;
	for (size_t index = 0; index < project_modules.size; index++) {
		EDITOR_MODULE_CONFIGURATION loaded_configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (loaded_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
			// There is a suitable loaded configuration
			// Check for its overrides
			const EditorModuleInfo* info = GetModuleInfo(editor_state, index, loaded_configuration);
			for (size_t override_index = 0; override_index < info->serialize_streams.serialize_shared_components.size; override_index++) {
				// Only if the name is specified, otherwise skip it (it is malformed)
				if (info->serialize_streams.serialize_shared_components[override_index].name.size > 0) {
					overrides.Add(info->serialize_streams.serialize_shared_components[override_index]);
				}
			}
		}
	}

	ECS_ASSERT(overrides.capacity - overrides.size >= link_types.size, "Too little override slots");
	ConvertLinkTypesToSerializeEntityManagerShared(editor_state->editor_components.internal_manager, allocator, link_types, overrides.buffer + overrides.size);
	overrides.size += link_types.size;

	CreateSerializeEntityManagerSharedComponentTable(table, editor_state->editor_components.internal_manager, allocator, overrides);

	//CreateSerializeEntityManagerSharedComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
	//CreateSerializeEntityManagerSharedComponentTable(table, editor_state->module_reflection->reflection, allocator, overrides);
	
	// Add the overrides manually
	for (unsigned int index = 0; index < overrides.size; index++) {
		Component component = editor_state->editor_components.GetComponentID(overrides[index].name);
		ECS_ASSERT(!table.Insert(overrides[index], component));
	}

	return table;
}

// ----------------------------------------------------------------------------------------------

DeserializeEntityManagerComponentTable DeserializeComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator,
	Stream<const Reflection::ReflectionType*> link_types
) {
	DeserializeEntityManagerComponentTable table;

	ECS_STACK_CAPACITY_STREAM(DeserializeEntityManagerComponentInfo, overrides, 256);

	// Walk through the module reflection and retrieve their overrides
	ProjectModules project_modules = *editor_state->project_modules;
	for (size_t index = 0; index < project_modules.size; index++) {
		EDITOR_MODULE_CONFIGURATION loaded_configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (loaded_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
			// There is a suitable loaded configuration
			// Check for its overrides
			const EditorModuleInfo* info = GetModuleInfo(editor_state, index, loaded_configuration);
			for (size_t override_index = 0; override_index < info->serialize_streams.deserialize_components.size; override_index++) {
				// Only if the name is specified, otherwise skip it (it is malformed)
				if (info->serialize_streams.deserialize_components[override_index].name.size > 0) {
					overrides.Add(info->serialize_streams.deserialize_components[override_index]);
				}
			}
		}
	}

	ECS_ASSERT(overrides.capacity - overrides.size >= link_types.size, "Too little override slots");
	ConvertLinkTypesToDeserializeEntityManagerUnique(editor_state->editor_components.internal_manager, allocator, link_types, overrides.buffer + overrides.size);
	overrides.size += link_types.size;

	CreateDeserializeEntityManagerComponentTable(table, editor_state->editor_components.internal_manager, allocator, overrides);

	//CreateDeserializeEntityManagerComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
	//CreateDeserializeEntityManagerComponentTable(table, editor_state->module_reflection->reflection, allocator, overrides);

	// Add the overrides manually
	for (unsigned int index = 0; index < overrides.size; index++) {
		Component component = editor_state->editor_components.GetComponentID(overrides[index].name);
		ECS_ASSERT(!table.Insert(overrides[index], component));
	}

	return table;
}

// ----------------------------------------------------------------------------------------------

DeserializeEntityManagerSharedComponentTable DeserializeSharedComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator,
	Stream<const Reflection::ReflectionType*> link_types
) {
	DeserializeEntityManagerSharedComponentTable table;

	ECS_STACK_CAPACITY_STREAM(DeserializeEntityManagerSharedComponentInfo, overrides, 256);

	// Walk through the module reflection and retrieve their overrides
	ProjectModules project_modules = *editor_state->project_modules;
	for (size_t index = 0; index < project_modules.size; index++) {
		EDITOR_MODULE_CONFIGURATION loaded_configuration = GetModuleLoadedConfiguration(editor_state, index);
		if (loaded_configuration != EDITOR_MODULE_CONFIGURATION_COUNT) {
			// There is a suitable loaded configuration
			// Check for its overrides
			const EditorModuleInfo* info = GetModuleInfo(editor_state, index, loaded_configuration);
			for (size_t override_index = 0; override_index < info->serialize_streams.deserialize_shared_components.size; override_index++) {
				// Only if the name is specified, otherwise skip it (it is malformed)
				if (info->serialize_streams.deserialize_shared_components[override_index].name.size > 0) {
					overrides.Add(info->serialize_streams.deserialize_shared_components[override_index]);
				}
			}
		}
	}

	ECS_ASSERT(overrides.capacity - overrides.size >= link_types.size, "Too little override slots");
	ConvertLinkTypesToDeserializeEntityManagerShared(editor_state->editor_components.internal_manager, allocator, link_types, overrides.buffer + overrides.size);
	overrides.size += link_types.size;

	CreateDeserializeEntityManagerSharedComponentTable(table, editor_state->editor_components.internal_manager, allocator, overrides);
	
	//CreateDeserializeEntityManagerSharedComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
	//CreateDeserializeEntityManagerSharedComponentTable(table, editor_state->module_reflection->reflection, allocator, overrides);
	
	// Add the overrides manually
	for (unsigned int index = 0; index < overrides.size; index++) {
		Component component = editor_state->editor_components.GetComponentID(overrides[index].name);
		ECS_ASSERT(!table.Insert(overrides[index], component));
	}

	return table;
}

// ----------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------

#pragma endregion

// ----------------------------------------------------------------------------------------------

bool CreateEmptyScene(const EditorState* editor_state, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStreamSafe(path);

	ECS_FILE_HANDLE file_handle = -1;
	ECS_FILE_STATUS_FLAGS status = FileCreate(absolute_path, &file_handle, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
	if (status == ECS_FILE_STATUS_OK) {
		CloseFile(file_handle);
		return true;
	}
	return false;
}

// ----------------------------------------------------------------------------------------------

bool ExistScene(const EditorState* editor_state, Stream<wchar_t> path)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStreamSafe(path);

	return ExistsFileOrFolder(absolute_path);
}

// ----------------------------------------------------------------------------------------------

bool LoadEditorSceneCore(EditorState* editor_state, EntityManager* entity_manager, AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB * 8);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	ECS_FILE_HANDLE file_handle = -1;
	ECS_FILE_STATUS_FLAGS status = OpenFile(filename, &file_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY);
	if (status != ECS_FILE_STATUS_OK) {
		_stack_allocator.ClearBackup();
		return false;
	}

	// If the file is empty, then the entity manager and the database reference is empty as well
	if (GetFileByteSize(file_handle) == 0) {
		CloseFile(file_handle);

		// Just reset the entity manager and database reference
		entity_manager->Reset();
		database->Reset();
		return true;
	}

	ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, unique_link_types, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, shared_link_types, ECS_KB);

	editor_state->editor_components.GetLinkComponents(unique_link_types, shared_link_types);

	// Create the 2 serialize table
	auto unique_table = DeserializeComponentTable(editor_state, stack_allocator, unique_link_types);
	auto shared_table = DeserializeSharedComponentTable(editor_state, stack_allocator, shared_link_types);

	// The entity manager first
	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS deserialize_status = DeserializeEntityManager(entity_manager, file_handle, &unique_table, &shared_table);
	if (deserialize_status != ECS_DESERIALIZE_ENTITY_MANAGER_OK) {
		_stack_allocator.ClearBackup();
		CloseFile(file_handle);
		return false;
	}

	// Read the database reference now
	// Read whatever its left in the file now and send it to the asset database
	// Choose a high enough buffer size
	void* malloc_buffer = malloc(ECS_MB * 10);
	unsigned int bytes_read = ReadFromFile(file_handle, { malloc_buffer, ECS_MB * 10 });
	if (bytes_read == -1) {
		entity_manager->Reset();
		free(malloc_buffer);
		_stack_allocator.ClearBackup();
		CloseFile(file_handle);
		return false;
	}

	uintptr_t ptr = (uintptr_t)malloc_buffer;
	bool success = database->DeserializeStandalone(editor_state->ui_reflection->reflection, ptr);

	free(malloc_buffer);
	_stack_allocator.ClearBackup();
	CloseFile(file_handle);

	if (!success) {
		entity_manager->Reset();
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------------------------------

bool SaveEditorScene(const EditorState* editor_state, const EntityManager* entity_manager, const AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	// Rename the file to a temporary name such that if we fail to serialize we don't lose the previous data
	ECS_STACK_CAPACITY_STREAM(wchar_t, renamed_file, 512);
	renamed_file.Copy(function::PathFilename(filename));
	renamed_file.AddStream(L".temp");
	if (!RenameFolderOrFile(filename, renamed_file)) {
		// If we fail, then return now.
		return false;
	}

	struct StackDeallocator {
		void operator()() {
			linear_allocator->ClearBackup();

			renamed_file.Copy(original_file);
			renamed_file.AddStream(L".temp");

			Stream<wchar_t> original_file_filename = function::PathFilename(original_file);

			if (file_handle != -1) {
				CloseFile(file_handle);

				if (!destroy_temporary) {
					RemoveFile(original_file);

					// Try to rename the previous file to this name
					RenameFolderOrFile(renamed_file, original_file_filename);
				}
				else {
					RemoveFile(renamed_file);
				}
			}
			else {
				RenameFolderOrFile(renamed_file, original_file_filename);
			}
		}

		Stream<wchar_t> original_file;
		Stream<wchar_t> renamed_file;
		ECS_FILE_HANDLE file_handle;
		bool destroy_temporary;
		ResizableLinearAllocator* linear_allocator;
	};

	StackScope<StackDeallocator> stack_deallocator({ filename, renamed_file, -1, false, &_stack_allocator });

	// Start with writing the entity manager first
	// Open the file first
	ECS_FILE_HANDLE file_handle = -1;
	ECS_FILE_STATUS_FLAGS status = FileCreate(filename, &file_handle, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
	if (status != ECS_FILE_STATUS_OK) {
		return false;
	}

	stack_deallocator.deallocator.file_handle = file_handle;
	
	ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, unique_link_types, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(const Reflection::ReflectionType*, shared_link_types, ECS_KB);
	editor_state->editor_components.GetLinkComponents(unique_link_types, shared_link_types);

	// Create the 2 serialize table
	auto unique_table = SerializeComponentTable(editor_state, stack_allocator, unique_link_types);
	auto shared_table = SerializeSharedComponentTable(editor_state, stack_allocator, shared_link_types);

	bool success = SerializeEntityManager(entity_manager, file_handle, &unique_table, &shared_table);
	if (!success) {
		return false;
	}

	// Now can write the asset database reference
	Stream<void> buffer = database->SerializeStandalone(editor_state->ui_reflection->reflection, stack_allocator);
	if (buffer.size > 0) {
		// Write it
		success = WriteFile(file_handle, buffer);
		if (!success) {
			return false;
		}
	}

	stack_deallocator.deallocator.destroy_temporary = true;
	return true;
}

// ----------------------------------------------------------------------------------------------
