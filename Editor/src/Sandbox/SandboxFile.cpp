#include "editorpch.h"
#include "SandboxFile.h"
#include "../Editor/EditorState.h"
#include "../Project/ProjectFolders.h"
#include "ECSEngineSerializationHelpers.h"
#include "Sandbox.h"
#include "SandboxScene.h"
#include "SandboxModule.h"
#include "SandboxProfiling.h"

using namespace ECSEngine;

#define SANDBOX_FILE_HEADER_VERSION (0)

struct SandboxFileHeader {
	size_t version;
	size_t count;
};

// -----------------------------------------------------------------------------------------------------------------------------

bool DisableEditorSandboxFileSave(EditorState* editor_state) {
	bool previous_value = editor_state->disable_editor_sandbox_write;
	editor_state->disable_editor_sandbox_write = false;
	return previous_value;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadEditorSandboxFile(EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, sandbox_file_path, 512);
	GetProjectSandboxFile(editor_state, sandbox_file_path);

	const Reflection::ReflectionManager* reflection_manager = editor_state->ModuleReflectionManager();

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);
	Stream<void> contents = ReadWholeFileBinary(sandbox_file_path, &stack_allocator);

	if (contents.buffer != nullptr) {
		// Read the header
		InMemoryReadInstrument read_instrument(contents);

		SandboxFileHeader header;
		if (!read_instrument.Read(&header)) {
			// Error invalid version
			ECS_FORMAT_TEMP_STRING(console_error, "Failed to read sandbox file. The file is corrupted (the header could not be read).");
			EditorSetConsoleError(console_error);
			return false;
		}

		if (header.version != SANDBOX_FILE_HEADER_VERSION) {
			// Error invalid version
			ECS_FORMAT_TEMP_STRING(console_error, "Failed to read sandbox file. Expected version {#} but the file has {#}.", SANDBOX_FILE_HEADER_VERSION, header.version);
			EditorSetConsoleError(console_error);
			return false;
		}

		if (header.count > EDITOR_MAX_SANDBOX_COUNT) {
			// Error, too many sandboxes
			ECS_FORMAT_TEMP_STRING(console_error, "Sandbox file has been corrupted. Maximum supported count is {#}, but file says {#}.", EDITOR_MAX_SANDBOX_COUNT, header.count);
			EditorSetConsoleError(console_error);
			return false;
		}

		// Deserialize the type table now
		Optional<DeserializeFieldTable> field_table = DeserializeFieldTableFromData(&read_instrument, &stack_allocator);
		if (!field_table.has_value) {
			// Error
			EditorSetConsoleError("The field table in the sandbox file is corrupted.");
			return false;
		}

		EditorSandbox* sandboxes = (EditorSandbox*)ECS_STACK_ALLOC(sizeof(EditorSandbox) * header.count);
		DeserializeOptions options;
		options.field_table = &field_table.value;
		options.field_allocator = &stack_allocator;
		options.read_type_table = false;
		options.default_initialize_missing_fields = true;

		const Reflection::ReflectionType* type = reflection_manager->GetType(STRING(EditorSandbox));
		for (size_t index = 0; index < header.count; index++) {
			ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, type, sandboxes + index, &read_instrument, &options);
			if (code != ECS_DESERIALIZE_OK) {
				ECS_STACK_CAPACITY_STREAM(char, console_error, 512);

				if (code == ECS_DESERIALIZE_CORRUPTED_FILE) {
					FormatString(console_error, "Failed to deserialize sandbox {#}. It is corrupted.", index);
				}
				else if (code == ECS_DESERIALIZE_MISSING_DEPENDENT_TYPES) {
					FormatString(console_error, "Failed to deserialize sandbox {#}. It is missing its dependent types.", index);
				}
				else if (code == ECS_DESERIALIZE_FIELD_TYPE_MISMATCH) {
					FormatString(console_error, "Failed to deserialize sandbox {#}. There was a field type mismatch.", index);
				}
				else {
					FormatString(console_error, "Unknown error happened when deserializing sandbox {#}.", index);
				}
				EditorSetConsoleError(console_error);
				return false;
			}
		}

		// Deallocate all the current sandboxes if any
		ECS_STACK_CAPACITY_STREAM(unsigned int, sandbox_handles, EDITOR_MAX_SANDBOX_COUNT);
		FillSandboxHandles(editor_state, sandbox_handles);
		for (size_t index = 0; index < sandbox_handles.size; index++) {
			DestroySandbox(editor_state, sandbox_handles[index]);
		}

		// Now create the "real" sandboxes
		for (size_t index = 0; index < header.count; index++) {
			unsigned int sandbox_handle = CreateSandbox(editor_state, false);

			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

			// Copy all the blittable information
			Reflection::CopyReflectionTypeBlittableFields(reflection_manager, type, sandboxes + index, sandbox);
			// The crash status needs to be set to false manually here
			sandbox->is_crashed = false;
			sandbox->is_scene_dirty = false;
			sandbox->is_temporary = false;

			// Set the runtime settings path - this will also create the runtime
			// If it fails, default initialize the runtime
			if (!ChangeSandboxRuntimeSettings(editor_state, sandbox_handle, sandboxes[index].runtime_settings)) {
				// Print an error message
				ECS_FORMAT_TEMP_STRING(console_message, "The runtime settings {#} doesn't exist or is corrupted when trying to deserialize sandbox {#}. "
					"The sandbox will revert to default settings.", sandboxes[index].runtime_settings, index);
				EditorSetConsoleWarn(console_message);

				ChangeSandboxRuntimeSettings(editor_state, sandbox_handle, { nullptr, 0 });
			}

			// Now the modules. Need to add the modules before the scene load, otherwise
			// Component functions will be lost
			for (unsigned int subindex = 0; subindex < sandboxes[index].modules_in_use.size; subindex++) {
				unsigned int module_index = sandboxes[index].modules_in_use[subindex].module_index;
				// Only if it is in bounds
				if (module_index < editor_state->project_modules->size) {
					AddSandboxModule(editor_state, sandbox_handle, module_index, sandboxes[index].modules_in_use[subindex].module_configuration);
					ChangeSandboxModuleSettings(editor_state, sandbox_handle, module_index, sandboxes[index].modules_in_use[subindex].settings_name);

					if (sandboxes[index].modules_in_use[subindex].is_deactivated) {
						DeactivateSandboxModuleInStream(editor_state, sandbox_handle, subindex);
					}
					for (unsigned int enabled_index = 0; enabled_index < sandboxes[index].modules_in_use[subindex].enabled_debug_tasks.size; enabled_index++) {
						AddSandboxModuleDebugDrawTask(editor_state, sandbox_handle, subindex, sandboxes[index].modules_in_use[subindex].enabled_debug_tasks[enabled_index]);
					}
				}
			}

			if (!ChangeSandboxScenePath(editor_state, sandbox_handle, sandboxes[index].scene_path)) {
				ECS_FORMAT_TEMP_STRING(console_message, "The scene path {#} doesn't exist or is invalid when trying to deserialize sandbox {#}. "
					"The sandbox will revert to no scene (check previous messages for more info).", sandboxes[index].scene_path, index);
				EditorSetConsoleWarn(console_message);

				ChangeSandboxScenePath(editor_state, sandbox_handle, { nullptr, 0 });
			}

			// Synchronize the profiling options
			SynchronizeSandboxProfilingWithStatisticTypes(editor_state, sandbox_handle);
		}

		return true;
	}

	ECS_FORMAT_TEMP_STRING(console_error, "Failed to read or open sandbox file. Path is {#}.", sandbox_file_path);
	EditorSetConsoleError(console_error);
	return false;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool RestoreAndSaveEditorSandboxFile(EditorState* editor_state, bool previous_state)
{
	editor_state->disable_editor_sandbox_write = previous_state;
	if (previous_state) {
		return SaveEditorSandboxFile(editor_state);
	}
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveEditorSandboxFile(const EditorState* editor_state)
{
	if (editor_state->disable_editor_sandbox_write) {
		return true;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectSandboxFile(editor_state, absolute_path);

	ECS_STACK_VOID_STREAM(file_buffering, ECS_KB * 64);
	return TemporaryRenameBufferedFileWriteInstrument::WriteTo(absolute_path, file_buffering, [&](WriteInstrument* write_instrument) {
		// Exclude the temporary sandboxes from being saved
		SandboxFileHeader header;
		header.count = GetSandboxCount(editor_state, true);
		header.version = SANDBOX_FILE_HEADER_VERSION;

		// Write the header first
		if (!write_instrument->Write(&header)) {
			ECS_FORMAT_TEMP_STRING(console_message, "Could not save sandbox file. Could not write the header.");
			EditorSetConsoleError(console_message);
			return false;
		}

		const Reflection::ReflectionManager* reflection_manager = editor_state->ui_reflection->reflection;

		// Write the type table
		const Reflection::ReflectionType* sandbox_type = reflection_manager->GetType(STRING(EditorSandbox));
		if (!SerializeFieldTable(reflection_manager, sandbox_type, write_instrument)) {
			ECS_FORMAT_TEMP_STRING(console_message, "Could not save sandbox file. Could not write the serialization type table.");
			EditorSetConsoleError(console_message);
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
		SerializeOptions options;
		options.write_type_table = false;
		options.error_message = &error_message;

		if (SandboxAction<true>(editor_state, -1, [&](unsigned int sandbox_handle) -> bool {
			ECS_SERIALIZE_CODE code = Serialize(reflection_manager, sandbox_type, GetSandbox(editor_state, sandbox_handle), write_instrument, &options);
			if (code != ECS_SERIALIZE_OK) {
				ECS_FORMAT_TEMP_STRING(console_message, "Could not save sandbox file. Faulty sandbox {#}. Detailed error message: {#}.", GetSandbox(editor_state, sandbox_handle)->name, error_message);
				EditorSetConsoleError(console_message);
				return true;
			}

			return false;
		}, true)) {
			return false;
		}

		return true;
	});
}