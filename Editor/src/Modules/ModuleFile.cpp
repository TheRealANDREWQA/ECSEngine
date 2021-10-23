#include "Module.h"
#include "ModuleFile.h"
#include "..\Editor\EditorState.h"
#include "..\Project\ProjectOperations.h"
#include "..\Editor\EditorEvent.h"

using namespace ECSEngine;
ECS_CONTAINERS;
ECS_TOOLS;

void GetProjectModuleFilePath(EditorState* editor_state, CapacityStream<wchar_t>& path)
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

	SerializeMultisectionData multisection_datas[256];
	CapacityStream<SerializeMultisectionData> serialize_data(multisection_datas, 0, 256);

	Stream<void> void_streams[256];
	unsigned int current_void_stream = 0;
	
	ECS_TEMP_STRING(module_path, 256);
	GetProjectModuleFilePath(editor_state, module_path);
	std::ifstream stream(module_path.buffer, std::ios::in | std::ios::binary | std::ios::beg);

	bool success = true;
	if (stream.good()) {
		size_t file_modules = DeserializeMultisectionCount(stream);
		for (size_t index = 0; index < file_modules; index++) {
			serialize_data[index].data.buffer = void_streams + current_void_stream;
			serialize_data[index].data.size = 3;
			current_void_stream += 3;
		}

		DeserializeMultisection(stream, serialize_data, editor_allocator, AllocatorType::MemoryManager, &success);
		if (success) {
			ResetProjectModules(editor_state);
			for (size_t index = 0; index < serialize_data.size; index++) {
				Stream<wchar_t> solution_path(serialize_data[index].data[0].buffer, serialize_data[index].data[0].size / sizeof(wchar_t));
				Stream<wchar_t> library_name(serialize_data[index].data[1].buffer, serialize_data[index].data[1].size / sizeof(wchar_t));
				Stream<void> configuration(serialize_data[index].data[2]);
				AddProjectModule(editor_state, solution_path, library_name, (ModuleConfiguration)(*(unsigned char*)configuration.buffer));
				UpdateProjectModuleLastWrite(editor_state, index);
				editor_allocator->Deallocate(solution_path.buffer);
				editor_allocator->Deallocate(library_name.buffer);
			}
		}
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

	SerializeMultisectionData* multisection_datas = (SerializeMultisectionData*)_malloca(sizeof(SerializeMultisectionData) * project_modules->size);
	Stream<SerializeMultisectionData> serialize_data(multisection_datas, project_modules->size);
	unsigned int current_void_stream = 0;
	Stream<void>* void_streams = (Stream<void>*)_malloca(sizeof(Stream<void>) * project_modules->size * 3);

	for (size_t index = 0; index < project_modules->size; index++) {
		serialize_data[index].data.buffer = void_streams + current_void_stream;
		serialize_data[index].data.size = 3;
		serialize_data[index].data[0] = project_modules->buffer[index].solution_path;
		serialize_data[index].data[1] = project_modules->buffer[index].library_name;
		serialize_data[index].data[2].buffer = &project_modules->buffer[index].configuration;
		serialize_data[index].data[2].size = sizeof(project_modules->buffer[index].configuration);
		current_void_stream += 3;
	}

	ECS_TEMP_STRING(module_path, 256);
	GetProjectModuleFilePath(editor_state, module_path);
	std::ofstream stream(module_path.buffer, std::ios::out | std::ios::trunc | std::ios::binary);

	bool success = false;
	if (stream.good()) {
		success = SerializeMultisection(stream, serialize_data);
	}

	_freea(multisection_datas);
	_freea(void_streams);
	return success;
}

void SaveProjectModuleFileThreadTask(unsigned int thread_id, World* world, void* data) {
	EditorState* editor_state = (EditorState*)data;
	bool success = SaveModuleFile(editor_state);
	if (!success) {
		EditorSetError(editor_state, ToStream("Saving project module file failed."));
	}
}
