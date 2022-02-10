#include "editorpch.h"
#include "Metafiles.h"
#include "ECSEngineUtilities.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorEvent.h"

using namespace ECSEngine;
ECS_CONTAINERS;

const wchar_t* METAFILE_EXTENSIONS[] = {
	L".jpg",
	L".jpeg",
	L".png",
	L".tga",
	L".tiff",
	L".bmp",
	L".hdr",
	L".hlsl",
	L".gltf",
	L".glb"
};

size_t METAFILE_EXTENSION_COUNT = std::size(METAFILE_EXTENSIONS);

const size_t METAFILE_TEXTURE_VERSION = 0;

void WriteDefaultTextureValues(ECS_FILE_HANDLE handle) {
	// Write 
}

const size_t METAFILE_SHADER_VERSION = 0;

void WriteDefaultShaderValues(ECS_FILE_HANDLE handle) {
	
}

const size_t METAFILE_MESH_VERSION = 0;

void WriteDefaultMeshValues(ECS_FILE_HANDLE handle) {

}

using WriteDefaultValuesFunctor = void (*)(ECS_FILE_HANDLE handle);

WriteDefaultValuesFunctor METAFILE_WRITE_DEFAULT_VALUES[] = {
	WriteDefaultTextureValues,
	WriteDefaultTextureValues,
	WriteDefaultTextureValues,
	WriteDefaultTextureValues,
	WriteDefaultTextureValues,
	WriteDefaultTextureValues,
	WriteDefaultTextureValues,
	WriteDefaultShaderValues,
	WriteDefaultMeshValues,
	WriteDefaultMeshValues
};

const wchar_t* METAFILE_EXTENSION = L".meta";

void RemoveProjectUnbindedMetafiles(const EditorState* editor_state)
{
	// Delete all files that do not have the extension .meta
	ECS_TEMP_STRING(metafile_folder, 256);
	GetProjectMetafilesFolder(editor_state, metafile_folder);

	struct FunctorData {
		const EditorState* editor_state;
		CapacityStream<wchar_t> path;
		bool remove_success;
	};

	FunctorData functor_data;
	functor_data.remove_success = true;
	functor_data.editor_state = editor_state;
	ECS_TEMP_STRING(functor_path, 512);
	functor_data.path = functor_path;

	// Removes every file
	ForEachInDirectoryRecursive(metafile_folder, &functor_data,
		[](const wchar_t* path, void* _data) {
			FunctorData* data = (FunctorData*)_data;
			Stream<wchar_t> stream_path = ToStream(path);
			FromMetafilePathToAssetsPath(data->editor_state, data->path, stream_path);

			if (!ExistsFileOrFolder(data->path)) {
				bool success = RemoveFolder(stream_path);
				if (!success) {
					data->remove_success = false;
				}
			}

			data->path.size = 0;
			return true;
		},
		[](const wchar_t* path, void* _data) {
		FunctorData* data = (FunctorData*)_data;

		Stream<wchar_t> stream_path = ToStream(path);
		Stream<wchar_t> extension = function::PathExtension(stream_path);
		// If it is not a .meta file, delete it
		if (!function::CompareStrings(extension, ToStream(METAFILE_EXTENSION))) {
			bool success = RemoveFile(stream_path);
			if (!success) {
				data->remove_success = false;
			}
		}
		else {
			FromMetafilePathToAssetsPath(data->editor_state, data->path, stream_path);
			// If the associated file does not exist, remove this meta file
			if (!ExistsFileOrFolder(data->path)) {
				bool success = RemoveFile(stream_path);
				if (!success) {
					data->remove_success = false;
				}
			}
		}
		data->path.size = 0;

		return true;
	});
}

bool CreateProjectMetafile(EditorState* editor_state, CapacityStream<wchar_t> path)
{
	// Create the mangled version of the path first
	ECS_TEMP_STRING(mangled_path, 1024);
	FromAssetsPathToMetafilePath(editor_state, mangled_path, path);

	// Create the file now
	if (!ExistsFileOrFolder(mangled_path)) {
		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_STATUS_FLAGS status = FileCreate(mangled_path, &file_handle, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_TRUNCATE_FILE);
		if (status == ECS_FILE_STATUS_OK) {
			WriteMetafileDefaultValues(file_handle, path);
			CloseFile(file_handle);
		}
		else {
			ECS_FORMAT_TEMP_STRING(console_message, "An error has occured when trying to create default metafile for {0}.", path);
			EditorSetConsoleError(editor_state, console_message);
		}

		// The metafile doesn't exist
		return false;
	}
	// The metafile already exists
	return true;
}

void CreateProjectMissingMetafiles(EditorState* editor_state)
{
	EDITOR_STATE(editor_state);

	ECS_TEMP_STRING(assets_folder, 256);
	GetProjectAssetsFolder(editor_state, assets_folder);

	struct FunctorData {
		EditorState* editor_state;
		bool error;
	};

	FunctorData functor_data = { editor_state, false };

	// Create all the necessary subdirectories
	ForEachDirectoryRecursive(assets_folder, &functor_data, [](const wchar_t* path, void* _data) {
		FunctorData* data = (FunctorData*)_data;

		Stream<wchar_t> stream_path = ToStream(path);
		ECS_TEMP_STRING(metafile_path, 256);
		FromAssetsPathToMetafilePath(data->editor_state, metafile_path, stream_path);
		// Remove the .meta
		metafile_path.size -= 5;
		if (!ExistsFileOrFolder(metafile_path)) {
			bool success = CreateFolder(metafile_path);
			if (!success) {
				data->error = true;
			}
		}
		return true;
	});

	if (functor_data.error) {
		EditorSetConsoleError(editor_state, ToStream("An error has occured when creating metafile folder."));
	}

	ForEachFileInDirectoryRecursiveWithExtension(assets_folder, Stream<const wchar_t*>(METAFILE_EXTENSIONS, METAFILE_EXTENSION_COUNT), editor_state,
		[](const wchar_t* path, void* _data) {
			EditorState* data = (EditorState*)_data;

			Stream<wchar_t> stream_path = ToStream(path);
			ECS_TEMP_STRING(metafile_path, 256);
			FromAssetsPathToMetafilePath(data, metafile_path, stream_path);

			if (!ExistsFileOrFolder(metafile_path)) {
				CreateProjectMetafile(data, stream_path);
			}
		return true;
	});
}

void FromMetafilePathToAssetsPath(const EditorState* editor_state, CapacityStream<wchar_t>& demangled_path, Stream<wchar_t> path)
{
	Stream<wchar_t> relative_path = function::PathRelativeTo(path, ToStream(PROJECT_METAFILES_RELATIVE_PATH));
	GetProjectAssetsFolder(editor_state, demangled_path);
	demangled_path.Add(ECS_OS_PATH_SEPARATOR);

	// Remove the .meta extension
	size_t meta_size = wcslen(METAFILE_EXTENSION);
	// Only decrease the path size if it has the .meta extension - if used for folders it will incorrectly eliminate size from them
	relative_path.size -= 5 * (memcmp(relative_path.buffer + relative_path.size - meta_size, METAFILE_EXTENSION, sizeof(wchar_t) * meta_size) == 0);
	demangled_path.AddStreamSafe(relative_path);
	demangled_path[demangled_path.size] = L'\0';
}

void FromAssetsPathToMetafilePath(const EditorState* editor_state, CapacityStream<wchar_t>& mangled_path, Stream<wchar_t> path)
{
	// Extract the relative path to the assets folder
	Stream<wchar_t> relative_path = function::PathRelativeTo(path, ToStream(PROJECT_ASSETS_RELATIVE_PATH));

	// Copy the metafile folder path
	GetProjectMetafilesFolder(editor_state, mangled_path);
	mangled_path.Add(ECS_OS_PATH_SEPARATOR);

	mangled_path.AddStream(relative_path);

	// Append the .meta at the end
	mangled_path.AddStreamSafe(ToStream(METAFILE_EXTENSION));
	mangled_path[mangled_path.size] = L'\0';
}

void WriteMetafileDefaultValues(ECSEngine::ECS_FILE_HANDLE file_handle, Stream<wchar_t> path)
{
	Stream<wchar_t> extension = function::PathExtension(path);
	size_t index = 0;
	for (; index < METAFILE_EXTENSION_COUNT; index++) {
		if (function::CompareStrings(extension, ToStream(METAFILE_EXTENSIONS[index]))) {
			METAFILE_WRITE_DEFAULT_VALUES[index](file_handle);
			// Exit the loop
			index = METAFILE_EXTENSION_COUNT + 1;
		}
	}
	ECS_ASSERT(index > METAFILE_EXTENSION_COUNT);
}
