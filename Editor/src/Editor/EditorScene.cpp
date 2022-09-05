#include "editorpch.h"
#include "EditorScene.h"
#include "EditorState.h"

#include "Modules\Module.h"
#include "EditorComponents.h"
#include "ECSEngineEntitiesSerialize.h"

using namespace ECSEngine;

#pragma region Internal functions

// ----------------------------------------------------------------------------------------------

SerializeEntityManagerComponentTable SerializeComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator
) {
	SerializeEntityManagerComponentTable table;

	ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerComponentInfo, overrides, 256);

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

	CreateSerializeEntityManagerComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
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
	AllocatorPolymorphic allocator
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

	CreateSerializeEntityManagerSharedComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
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
	AllocatorPolymorphic allocator
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

	CreateDeserializeEntityManagerComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
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
	AllocatorPolymorphic allocator
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

	CreateDeserializeEntityManagerSharedComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);
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

bool SaveEditorScene(const EditorState* editor_state, const EntityManager* entity_manager, const AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	// Create the 2 serialize table
	auto unique_table = SerializeComponentTable(editor_state, stack_allocator);
	auto shared_table = SerializeSharedComponentTable(editor_state, stack_allocator);

	// Start with writing the entity manager first
	// Open the file first
	ECS_FILE_HANDLE file_handle = -1;
	ECS_FILE_STATUS_FLAGS status = FileCreate(filename, &file_handle);
	if (status != ECS_FILE_STATUS_OK) {
		_stack_allocator.ClearBackup();
		return false;
	}

	bool success = SerializeEntityManager(entity_manager, file_handle, &unique_table, &shared_table);
	if (!success) {
		_stack_allocator.ClearBackup();
		CloseFile(file_handle);
		// Also remove the file such that it is not a leftover
		RemoveFile(filename);
		return false;
	}
	
	// Now can write the asset database reference
	Stream<void> buffer = database->SerializeStandalone(editor_state->ui_reflection->reflection, stack_allocator);
	if (buffer.size > 0) {
		// Write it
		success = WriteFile(file_handle, buffer);
		if (!success) {
			CloseFile(file_handle);
			RemoveFile(filename);
			_stack_allocator.ClearBackup();
			return false;
		}
	}

	CloseFile(file_handle);
	_stack_allocator.ClearBackup();
	return true;
}

// ----------------------------------------------------------------------------------------------

bool LoadEditorSceneCore(EditorState* editor_state, EntityManager* entity_manager, AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	// Create the 2 serialize table
	auto unique_table = DeserializeComponentTable(editor_state, stack_allocator);
	auto shared_table = DeserializeSharedComponentTable(editor_state, stack_allocator);

	ECS_FILE_HANDLE file_handle = -1;
	ECS_FILE_STATUS_FLAGS status = OpenFile(filename, &file_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY);
	if (status != ECS_FILE_STATUS_OK) {
		_stack_allocator.ClearBackup();
		return false;
	}

	// The entity manager first
	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS deserialize_status = DeserializeEntityManager(entity_manager, file_handle, &unique_table, &shared_table);
	if (deserialize_status != ECS_DESERIALIZE_ENTITY_MANAGER_OK) {
		_stack_allocator.ClearBackup();
		CloseFile(file_handle);
	}

	// Read the database reference now
	// Read whatever its left in the file now and send it to the asset database
	// Choose a high enough buffer size
	void* malloc_buffer = malloc(ECS_MB * 10);
	unsigned int bytes_read = ReadFromFile(file_handle, { malloc_buffer, ECS_GB });
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
