#include "editorpch.h"
#include "Module.h"
#include "ModuleFile.h"
#include "..\Editor\EditorState.h"
#include "..\Project\ProjectOperations.h"
#include "..\Editor\EditorEvent.h"

using namespace ECSEngine;
ECS_CONTAINERS;
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

// Uses the project file to determine the path
bool LoadModuleFile(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	constexpr size_t TEMP_STREAMS = 64;

	SerializeMultisectionData multisection_datas[TEMP_STREAMS];
	CapacityStream<SerializeMultisectionData> serialize_data(multisection_datas, 0, TEMP_STREAMS);

	Stream<void> _temp_memory[TEMP_STREAMS * 3];
	CapacityStream<void> memory_pool(_temp_memory, 0, sizeof(Stream<void>) * TEMP_STREAMS * 3);
	
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
			ResetProjectModules(editor_state);
			ResetProjectGraphicsModule(editor_state);

			unsigned int valid_projects = 0;
			ProjectModules* project_modules = (ProjectModules*)editor_state->project_modules;

			Stream<wchar_t> solution_path(serialize_data[0].data[0].buffer, serialize_data[0].data[0].size / sizeof(wchar_t));
			Stream<wchar_t> library_name(serialize_data[0].data[1].buffer, serialize_data[0].data[1].size / sizeof(wchar_t));
			Stream<void> configuration(serialize_data[0].data[2]);

			if (solution_path.buffer != nullptr && library_name.buffer != nullptr) {
				SetProjectGraphicsModule(editor_state, solution_path, library_name, (EditorModuleConfiguration)(*(unsigned char*)configuration.buffer));
			}

			for (size_t index = 1; index < serialize_data.size; index++) {
				Stream<wchar_t> solution_path(serialize_data[index].data[0].buffer, serialize_data[index].data[0].size / sizeof(wchar_t));
				Stream<wchar_t> library_name(serialize_data[index].data[1].buffer, serialize_data[index].data[1].size / sizeof(wchar_t));
				Stream<void> configuration(serialize_data[index].data[2]);

				if (AddProjectModule(editor_state, solution_path, library_name, (EditorModuleConfiguration)(*(unsigned char*)configuration.buffer))) {
					UpdateProjectModuleLastWrite(editor_state, 1 + valid_projects);
					valid_projects++;
					bool success = HasModuleFunction(editor_state, project_modules->size - 1);
					if (!success) {
						ECS_FORMAT_TEMP_STRING(error_message, "Module with solution path {0} and library name {1} does not have a module function.", solution_path, library_name);
						EditorSetConsoleWarn(editor_state, error_message);
						project_modules->buffer[valid_projects].load_status = EditorModuleLoadStatus::Failed;
					}
					else {
						if (project_modules->buffer[valid_projects].solution_last_write_time > project_modules->buffer[valid_projects].library_last_write_time) {
							project_modules->buffer[valid_projects].load_status = EditorModuleLoadStatus::OutOfDate;
						}
						else {
							project_modules->buffer[valid_projects].load_status = EditorModuleLoadStatus::Good;
						}
					}
				}
			}
			if (valid_projects < serialize_data.size - 1) {
				auto error_message = ToStream("One or more modules failed to load. Their paths were corrupted or deleted from outside ECSEngine."
					" They were removed from the current project's module list.");
				EditorSetConsoleError(editor_state, error_message);
				ThreadTask rewrite_module_file = { SaveProjectModuleFileThreadTask, editor_state, 0 };
				editor_state->task_manager->AddDynamicTaskAndWake(rewrite_module_file);
			}
		}
		editor_allocator->Deallocate(file_contents.buffer);
	}
	else {
		success = false;
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
	Stream<void>* void_streams = (Stream<void>*)ECS_STACK_ALLOC(sizeof(Stream<void>) * total_module_count * 3);

	auto loop_function = [](Stream<SerializeMultisectionData> serialize_data, Stream<void>* void_streams, unsigned int& current_void_stream,
		Stream<wchar_t> solution_path, Stream<wchar_t> library_name, EditorModuleConfiguration configuration, unsigned int index) {
		serialize_data[index].data.buffer = void_streams + current_void_stream;
		serialize_data[index].data.size = 3;
		serialize_data[index].data[0] = solution_path;
		serialize_data[index].data[1] = library_name;
		serialize_data[index].data[2].buffer = &configuration;
		serialize_data[index].data[2].size = sizeof(configuration);
		serialize_data[index].name = nullptr;
		current_void_stream += 3;
	};	

	for (size_t index = 0; index < project_modules->size; index++) {
		loop_function(
			serialize_data,
			void_streams,
			current_void_stream,
			project_modules->buffer[index].solution_path,
			project_modules->buffer[index].library_name,
			project_modules->buffer[index].configuration,
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
		EditorSetError(editor_state, ToStream("Saving project module file failed."));
	}
}
