#include "editorpch.h"
#include "SandboxFile.h"
#include "../Editor/EditorState.h"
#include "../Project/ProjectFolders.h"
#include "ECSEngineSerializationHelpers.h"
#include "Sandbox.h"
#include "SandboxScene.h"
#include "SandboxModule.h"

using namespace ECSEngine;

#define SANDBOX_FILE_HEADER_VERSION (0)
#define SANDBOX_FILE_MAX_SANDBOXES (16)

struct SandboxFileHeader {
	size_t version;
	size_t count;
};

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadEditorSandboxFile(EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, sandbox_file_path, 512);
	GetProjectSandboxFile(editor_state, sandbox_file_path);

	const Reflection::ReflectionManager* reflection_manager = editor_state->EditorReflectionManager();

	const size_t STACK_ALLOCATION_CAPACITY = ECS_KB * 128;
	const size_t BACKUP_CAPACITY = ECS_MB;
	void* stack_allocation = ECS_STACK_ALLOC(STACK_ALLOCATION_CAPACITY);

	// Use malloc for extra large allocations
	ResizableLinearAllocator linear_allocator(stack_allocation, STACK_ALLOCATION_CAPACITY, BACKUP_CAPACITY, { nullptr });
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&linear_allocator);
	Stream<void> contents = ReadWholeFileBinary(sandbox_file_path, allocator);

	struct DeallocateAllocator {
		void operator() () {
			allocator->ClearBackup();
		}

		ResizableLinearAllocator* allocator;
	};

	StackScope<DeallocateAllocator> deallocate_scope({ &linear_allocator });

	if (contents.buffer != nullptr) {
		// Read the header
		uintptr_t ptr = (uintptr_t)contents.buffer;

		SandboxFileHeader header;
		Read<true>(&ptr, &header, sizeof(header));

		if (header.version != SANDBOX_FILE_HEADER_VERSION) {
			// Error invalid version
			ECS_FORMAT_TEMP_STRING(console_error, "Failed to read sandbox file. Expected version {#} but the file has {#}.", SANDBOX_FILE_HEADER_VERSION, header.version);
			EditorSetConsoleError(console_error);
			return false;
		}

		if (header.count > SANDBOX_FILE_MAX_SANDBOXES) {
			// Error, too many sandboxes
			ECS_FORMAT_TEMP_STRING(console_error, "Sandbox file has been corrupted. Maximum supported count is {#}, but file says {#}.", SANDBOX_FILE_MAX_SANDBOXES, header.count);
			EditorSetConsoleError(console_error);
			return false;
		}

		// Deserialize the type table now
		DeserializeFieldTable field_table = DeserializeFieldTableFromData(ptr, allocator);
		if (field_table.types.size == 0) {
			// Error
			EditorSetConsoleError("The field table in the sandbox file is corrupted.");
			return false;
		}

		EditorSandbox* sandboxes = (EditorSandbox*)ECS_STACK_ALLOC(sizeof(EditorSandbox) * header.count);
		DeserializeOptions options;
		options.field_table = &field_table;
		options.field_allocator = allocator;
		options.backup_allocator = allocator;
		options.read_type_table = false;

		const Reflection::ReflectionType* type = reflection_manager->GetType(STRING(EditorSandbox));
		for (size_t index = 0; index < header.count; index++) {
			ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, type, sandboxes + index, ptr, &options);
			if (code != ECS_DESERIALIZE_OK) {
				ECS_STACK_CAPACITY_STREAM(char, console_error, 512);

				if (code == ECS_DESERIALIZE_CORRUPTED_FILE) {
					ECS_FORMAT_STRING(console_error, "Failed to deserialize sandbox {#}. It is corrupted.", index);
				}
				else if (code == ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES) {
					ECS_FORMAT_STRING(console_error, "Failed to deserialize sandbox {#}. It is missing its dependent types.", index);
				}
				else if (code == ECS_DESERIALIZE_FIELD_TYPE_MISMATCH) {
					ECS_FORMAT_STRING(console_error, "Failed to deserialize sandbox {#}. There was a field type mismatch.", index);
				}
				else {
					ECS_FORMAT_STRING(console_error, "Unknown error happened when deserializing sandbox {#}.", index);
				}
				EditorSetConsoleError(console_error);
				return false;
			}
		}

		// Deallocate all the current sandboxes if any
		size_t initial_sandbox_count = editor_state->sandboxes.size;
		for (size_t index = 0; index < initial_sandbox_count; index++) {
			DestroySandbox(editor_state, 0);
		}

		// Now create the "real" sandboxes
		for (size_t index = 0; index < header.count; index++) {
			CreateSandbox(editor_state, false);

			EditorSandbox* sandbox = GetSandbox(editor_state, index);

			// Copy all the blittable information
			Reflection::CopyReflectionTypeBlittableFields(reflection_manager, type, sandboxes + index, sandbox);

			// Set the runtime settings path - this will also create the runtime
			// If it fails, default initialize the runtime
			if (!ChangeSandboxRuntimeSettings(editor_state, index, sandboxes[index].runtime_settings)) {
				// Print an error message
				ECS_FORMAT_TEMP_STRING(console_message, "The runtime settings {#} doesn't exist or is corrupted when trying to deserialize sandbox {#}. "
					"The sandbox will revert to default settings.", sandboxes[index].runtime_settings, index);
				EditorSetConsoleWarn(console_message);

				ChangeSandboxRuntimeSettings(editor_state, index, { nullptr, 0 });
			}

			if (!ChangeSandboxScenePath(editor_state, index, sandboxes[index].scene_path)) {
				ECS_FORMAT_TEMP_STRING(console_message, "The scene path {#} doesn't exist or is invalid when trying to deserialize sandbox {#}. "
					"The sandbox will revert to no scene (check previous messages for more info).", sandboxes[index].scene_path, index);
				EditorSetConsoleWarn(console_message);

				ChangeSandboxScenePath(editor_state, index, { nullptr, 0 });
			}

			// Now the modules
			for (unsigned int subindex = 0; subindex < sandboxes[index].modules_in_use.size; subindex++) {
				unsigned int module_index = sandboxes[index].modules_in_use[subindex].module_index;
				// Only if it is in bounds
				if (module_index < editor_state->project_modules->size) {
					AddSandboxModule(editor_state, index, module_index, sandboxes[index].modules_in_use[subindex].module_configuration);
					ChangeSandboxModuleSettings(editor_state, index, module_index, sandboxes[index].modules_in_use[subindex].settings_name);

					if (sandboxes[index].modules_in_use[subindex].is_deactivated) {
						DeactivateSandboxModuleInStream(editor_state, index, subindex);
					}
				}
			}
		}

		return true;
	}

	ECS_FORMAT_TEMP_STRING(console_error, "Failed to read or open sandbox file. Path is {#}.", sandbox_file_path);
	EditorSetConsoleError(console_error);
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveEditorSandboxFile(const EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectSandboxFile(editor_state, absolute_path);

	// Allocate a buffer of ECS_KB * 128 and write into it
	// Assert that the overall size is less
	const size_t STACK_CAPACITY = ECS_KB * 128;
	void* stack_buffer = ECS_STACK_ALLOC(STACK_CAPACITY);
	uintptr_t ptr = (uintptr_t)stack_buffer;

	SandboxFileHeader header;
	header.count = editor_state->sandboxes.size;
	header.version = SANDBOX_FILE_HEADER_VERSION;

	// Write the header first
	Write<true>(&ptr, &header, sizeof(header));

	const Reflection::ReflectionManager* reflection_manager = editor_state->ui_reflection->reflection;

	// Write the type table
	const Reflection::ReflectionType* sandbox_type = reflection_manager->GetType(STRING(EditorSandbox));
	SerializeFieldTable(reflection_manager, sandbox_type, ptr);

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	SerializeOptions options;
	options.write_type_table = false;
	options.error_message = &error_message;

	for (size_t index = 0; index < editor_state->sandboxes.size; index++) {
		ECS_SERIALIZE_CODE code = Serialize(reflection_manager, sandbox_type, editor_state->sandboxes.buffer + index, ptr, &options);
		if (code != ECS_SERIALIZE_OK) {
			ECS_FORMAT_TEMP_STRING(console_message, "Could not save sandbox file. Faulty sandbox {#}. Detailed error message: {#}.", index, error_message);
			EditorSetConsoleError(console_message);
			return false;
		}
	}

	size_t bytes_written = ptr - (uintptr_t)stack_buffer;
	ECS_ASSERT(bytes_written <= STACK_CAPACITY);
	// Now commit to the file
	ECS_FILE_STATUS_FLAGS status = WriteBufferToFileBinary(absolute_path, { stack_buffer, bytes_written });
	if (status != ECS_FILE_STATUS_OK) {
		ECS_FORMAT_TEMP_STRING(console_message, "Could not save sandbox file. Failed to write to path {#}.", absolute_path);
		EditorSetConsoleError(console_message);

		return false;
	}

	return true;
}