#include "editorpch.h"
#include "Module.h"
#include "ModuleFile.h"
#include "..\Editor\EditorState.h"
#include "..\Project\ProjectOperations.h"
#include "..\Editor\EditorEvent.h"
#include "ModuleSettings.h"

using namespace ECSEngine;
ECS_TOOLS;

constexpr const wchar_t* NO_GRAPHICS_MODULE = L"No graphics module";

void GetProjectModuleFilePath(const EditorState* editor_state, CapacityStream<wchar_t>& path)
{
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(project_file->project_name);
	path.AddStreamSafe(ToStream(PROJECT_MODULE_EXTENSION));
	path[path.size] = L'\0';
	path.AssertCapacity();
}

enum SERIALIZE_ORDER {
	SERIALIZE_SOLUTION_PATH,
	SERIALIZE_LIBRARY_NAME,
	SERIALIZE_COUNT
};

// Uses the project file to determine the path
bool LoadModuleFile(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	constexpr size_t TEMP_STREAMS = 64;

	SerializeMultisectionData multisection_datas[TEMP_STREAMS];
	CapacityStream<SerializeMultisectionData> serialize_data(multisection_datas, 0, TEMP_STREAMS);

	Stream<void> _temp_memory[TEMP_STREAMS * SERIALIZE_COUNT];
	CapacityStream<void> memory_pool(_temp_memory, 0, sizeof(Stream<void>) * TEMP_STREAMS * SERIALIZE_COUNT);
	
	ECS_TEMP_STRING(module_path, 256);
	GetProjectModuleFilePath(editor_state, module_path);

	Stream<void> file_contents = ReadWholeFileBinary(module_path, GetAllocatorPolymorphic(editor_allocator));

	bool success = true;
	if (file_contents.buffer != nullptr) {
		uintptr_t file_ptr = (uintptr_t)file_contents.buffer;
		size_t file_modules = DeserializeMultisectionCount(file_ptr);
		ECS_ASSERT(file_modules < TEMP_STREAMS, "Too many modules");

		success = DeserializeMultisection(serialize_data, memory_pool, file_ptr) != -1;
		if (success) {
			ResetModules(editor_state);
			ResetGraphicsModule(editor_state);

			unsigned int valid_projects = 0;
			ProjectModules* project_modules = (ProjectModules*)editor_state->project_modules;

			Stream<wchar_t> solution_path(serialize_data[0].data[SERIALIZE_SOLUTION_PATH].buffer, serialize_data[0].data[SERIALIZE_SOLUTION_PATH].size / sizeof(wchar_t));
			Stream<wchar_t> library_name(serialize_data[0].data[SERIALIZE_LIBRARY_NAME].buffer, serialize_data[0].data[SERIALIZE_LIBRARY_NAME].size / sizeof(wchar_t));

			if (solution_path.buffer != nullptr && library_name.buffer != nullptr) {
				SetGraphicsModule(editor_state, solution_path, library_name);
			}

			for (size_t index = 1; index < serialize_data.size; index++) {
				Stream<wchar_t> solution_path(serialize_data[index].data[SERIALIZE_SOLUTION_PATH].buffer, serialize_data[index].data[SERIALIZE_SOLUTION_PATH].size / sizeof(wchar_t));
				Stream<wchar_t> library_name(serialize_data[index].data[SERIALIZE_LIBRARY_NAME].buffer, serialize_data[index].data[SERIALIZE_LIBRARY_NAME].size / sizeof(wchar_t));

				if (AddModule(editor_state, solution_path, library_name)) {
					valid_projects++;

					UpdateModuleLastWrite(editor_state, valid_projects);

					for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
						EDITOR_MODULE_CONFIGURATION configuration = (EDITOR_MODULE_CONFIGURATION)configuration_index;
						EditorModuleInfo* info = GetModuleInfo(editor_state, project_modules->size - 1, configuration);

						bool success = HasModuleFunction(editor_state, project_modules->size - 1, configuration);
						if (success) {
							if (project_modules->buffer[valid_projects].solution_last_write_time > info->library_last_write_time) {
								info->load_status = EDITOR_MODULE_LOAD_OUT_OF_DATE;
							}
							else {
								info->load_status = EDITOR_MODULE_LOAD_GOOD;
							}
						}
					}
				}
			}
			if (valid_projects < serialize_data.size - 1) {
				auto error_message = ToStream("One or more modules failed to load. Their paths were corrupted or deleted from outside ECSEngine."
					" They were removed from the current project's module list.");
				EditorSetConsoleError(error_message);
				ThreadTask rewrite_module_file = ECS_THREAD_TASK_NAME(SaveProjectModuleFileThreadTask, editor_state, 0);
				editor_state->task_manager->AddDynamicTaskAndWake(rewrite_module_file);
			}
		}
		editor_allocator->Deallocate(file_contents.buffer);
	}
	else {
		success = false;
		editor_state->project_modules->size = 0;
		editor_state->project_modules->ReserveNewElement();	
		editor_state->project_modules->size = 1;
		editor_state->project_modules->buffer[GRAPHICS_MODULE_INDEX].library_name = { nullptr, 0 };
		editor_state->project_modules->buffer[GRAPHICS_MODULE_INDEX].solution_path = { nullptr, 0 };
	}

	// Delete the locked files
	DeleteVisualStudioLockedFiles(editor_state);

	return success;
}

// Uses the project file to determine the path
bool SaveModuleFile(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	ProjectModules* project_modules = (ProjectModules*)editor_state->project_modules;

	ECS_ASSERT(project_modules->size < 64);
	unsigned int total_module_count = project_modules->size;
	SerializeMultisectionData* multisection_datas = (SerializeMultisectionData*)ECS_STACK_ALLOC(sizeof(SerializeMultisectionData) * total_module_count);
	Stream<SerializeMultisectionData> serialize_data(multisection_datas, total_module_count);
	unsigned int current_void_stream = 0;
	Stream<void>* void_streams = (Stream<void>*)ECS_STACK_ALLOC(sizeof(Stream<void>) * total_module_count * SERIALIZE_COUNT);

	auto loop_function = [](Stream<SerializeMultisectionData> serialize_data, Stream<void>* void_streams, unsigned int& current_void_stream,
		EditorModule* editor_module, unsigned int index) {
		serialize_data[index].data.buffer = void_streams + current_void_stream;
		serialize_data[index].data.size = SERIALIZE_COUNT;
		serialize_data[index].data[SERIALIZE_SOLUTION_PATH] = editor_module->solution_path;
		serialize_data[index].data[SERIALIZE_LIBRARY_NAME] = editor_module->library_name;
		;
		serialize_data[index].name = nullptr;
		current_void_stream += SERIALIZE_COUNT;
	};	

	for (size_t index = 0; index < project_modules->size; index++) {
		loop_function(
			serialize_data,
			void_streams,
			current_void_stream,
			project_modules->buffer + index,
			index
		);
	}

	ECS_TEMP_STRING(module_path, 256);
	GetProjectModuleFilePath(editor_state, module_path);
	return SerializeMultisection(serialize_data, module_path);
}

void SaveProjectModuleFileThreadTask(unsigned int thread_id, World* world, void* data) {
	EditorState* editor_state = (EditorState*)data;
	bool success = SaveModuleFile(editor_state);
	if (!success) {
		EditorSetConsoleError(ToStream("Saving project module file failed."));
	}
}
