#include "editorpch.h"
#include "../Editor/EditorState.h"
#include "AssetManagement.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorEvent.h"

#include "AssetExtensions.h"

using namespace ECSEngine;

// ----------------------------------------------------------------------------------------------

bool CreateAssetFileImpl(const EditorState* editor_state, Stream<wchar_t> relative_path, Stream<wchar_t> extension) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStream(relative_path);
	absolute_path.AddStreamSafe(extension);

	return CreateEmptyFile(absolute_path);
}

// ----------------------------------------------------------------------------------------------

// Create it as an empty file. It will exist in the metadata folder. This just makes the connection with it
bool CreateMaterialFile(const EditorState* editor_state, Stream<wchar_t> relative_path)
{
	return CreateAssetFileImpl(editor_state, relative_path, ASSET_MATERIAL_EXTENSIONS[0]);
}

// ----------------------------------------------------------------------------------------------

// Create it as an empty file. It will exist in the metadata folder. This just makes the connection with it
bool CreateSamplerFile(const EditorState* editor_state, Stream<wchar_t> relative_path)
{
	return CreateAssetFileImpl(editor_state, relative_path, ASSET_GPU_SAMPLER_EXTENSIONS[0]);
}

// ----------------------------------------------------------------------------------------------

bool CreateShaderFile(const EditorState* editor_state, Stream<wchar_t> relative_path)
{
	return CreateAssetFileImpl(editor_state, relative_path, ASSET_SHADER_EXTENSIONS[0]);
}

// ----------------------------------------------------------------------------------------------

bool CreateMiscFile(const EditorState* editor_state, Stream<wchar_t> relative_path)
{
	return CreateAssetFileImpl(editor_state, relative_path, ASSET_MISC_EXTENSIONS[0]);
}

// ----------------------------------------------------------------------------------------------

bool CreateAssetSetting(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
{
	// Just create the default file
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	// Big enough to hold any metadata type
	size_t storage[128];
	CreateDefaultAsset(storage, name, file, type);

	return editor_state->asset_database->WriteAssetFile(storage, type);
}

// ----------------------------------------------------------------------------------------------

bool CreateAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	// If the target file is valid, then also create the asset
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	return CreateAssetFromMetadata(
		editor_state->RuntimeResourceManager(),
		editor_state->asset_database,
		editor_state->asset_database->GetAsset(handle, type),
		type,
		assets_folder
	);
}

// ----------------------------------------------------------------------------------------------

void CreateAssetDefaultSetting(const EditorState* editor_state)
{
	// At the moment only meshes and textures need this.
	Stream<wchar_t> mesh_extensions[std::size(ASSET_MESH_EXTENSIONS)];
	Stream<wchar_t> texture_extensions[std::size(ASSET_TEXTURE_EXTENSIONS)];
	
	for (size_t index = 0; index < std::size(ASSET_MESH_EXTENSIONS); index++) {
		mesh_extensions[index] = ASSET_MESH_EXTENSIONS[index];
	}

	for (size_t index = 0; index < std::size(ASSET_TEXTURE_EXTENSIONS); index++) {
		texture_extensions[index] = ASSET_TEXTURE_EXTENSIONS[index];
	}

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&stack_allocator);

	ResizableStream<Stream<wchar_t>> mesh_files(allocator, 0);
	ResizableStream<Stream<wchar_t>> texture_files(allocator, 0);

	auto get_functor = [](Stream<wchar_t> path, void* _data) {
		ResizableStream<Stream<wchar_t>>* data = (ResizableStream<Stream<wchar_t>>*)_data;

		ECS_STACK_CAPACITY_STREAM(wchar_t, current_file, 512);
		AssetDatabase::ExtractFileFromFile(path, current_file);
		if (current_file.size > 0) {
			unsigned int index = function::FindString(current_file, data->ToStream());
			if (index == -1) {
				Stream<wchar_t> copy = function::StringCopy(data->allocator, current_file);
				// Change the relative path separator into an absolute path separator
				function::ReplaceCharacter(copy, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
				data->Add(copy);
			}
		}
		return true;
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_directory, 512);
	AssetDatabaseFileDirectory(editor_state->asset_database->metadata_file_location, metadata_directory, ECS_ASSET_MESH);
	// It has a final backslash
	metadata_directory.size--;

	ForEachFileInDirectory(metadata_directory, &mesh_files, get_functor);

	metadata_directory.size = 0;
	AssetDatabaseFileDirectory(editor_state->asset_database->metadata_file_location, metadata_directory, ECS_ASSET_TEXTURE);
	// It has a final backslash
	metadata_directory.size--;
	ForEachFileInDirectory(metadata_directory, &texture_files, get_functor);

	struct FunctorData {
		const EditorState* editor_state;
		Stream<wchar_t> base_path;
		Stream<Stream<wchar_t>> mesh_extensions;
		Stream<Stream<wchar_t>> texture_extensions;
		Stream<Stream<wchar_t>> existing_mesh_files;
		Stream<Stream<wchar_t>> existing_texture_files;
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	FunctorData functor_data = { 
		editor_state, 
		assets_folder,
		{mesh_extensions, std::size(mesh_extensions)},
		{texture_extensions, std::size(texture_extensions)},
		mesh_files,
		texture_files 
	};
	auto functor = [](Stream<wchar_t> path, void* _data) {
		FunctorData* data = (FunctorData*)_data;

		Stream<wchar_t> extension = function::PathExtension(path);
		unsigned int mesh_index = function::FindString(extension, data->mesh_extensions);
		if (mesh_index != -1) {
			Stream<wchar_t> relative_path = function::PathRelativeToAbsolute(path, data->base_path);

			// Check to see if it doesn't exist in the mesh stream
			mesh_index = function::FindString(relative_path, data->existing_mesh_files);
			if (mesh_index == -1) {
				// Create a default setting for it
				CreateAssetSetting(data->editor_state, "Default", relative_path, ECS_ASSET_MESH);
			}
		}
		else {
			unsigned int texture_index = function::FindString(extension, data->texture_extensions);
			if (texture_index != -1) {
				Stream<wchar_t> relative_path = function::PathRelativeToAbsolute(path, data->base_path);
				texture_index = function::FindString(relative_path, data->existing_texture_files);
				if (texture_index == -1) {
					// Create a default setting for it
					CreateAssetSetting(data->editor_state, "Default", relative_path, ECS_ASSET_TEXTURE);
				}
			}
		}
		return true;
	};

	ForEachFileInDirectoryRecursive(assets_folder, &functor_data, functor);
	stack_allocator.ClearBackup();
}

// ----------------------------------------------------------------------------------------------

void ChangeAssetName(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, const void* new_asset)
{
	// Change the file name inside the asset database and then recreate it
	editor_state->asset_database->UpdateAsset(handle, new_asset, type);
}

// ----------------------------------------------------------------------------------------------

// This takes the pointer as is, it does not copy it
struct ChangeAssetFileEventData {
	unsigned int handle;
	ECS_ASSET_TYPE type;
	const void* new_asset;
	unsigned char* status;
};

EDITOR_EVENT(ChangeAssetFileEvent) {
	ChangeAssetFileEventData* data = (ChangeAssetFileEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		// Also deallocate the current resource and recreate it again with the new path
		bool success = DeallocateAsset(editor_state, data->handle, data->type);

		if (success) {
			// Change the file name inside the asset database and then recreate it
			editor_state->asset_database->UpdateAsset(data->handle, data->new_asset, data->type);
			success = CreateAsset(editor_state, data->handle, data->type);
		}

		*data->status = success;
		return false;
	}
	else {
		return true;
	}
}

void ChangeAssetFile(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, const void* new_asset, unsigned char* status)
{
	// Set it different from 0 and 1
	*status = UCHAR_MAX;
	ChangeAssetFileEventData event_data = { handle, type, new_asset, status };
	EditorAddEvent(editor_state, ChangeAssetFileEvent, &event_data, sizeof(event_data));
}

// ----------------------------------------------------------------------------------------------

bool DeallocateAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	return DeallocateAssetFromMetadata(editor_state->RuntimeResourceManager(), editor_state->asset_database, handle, type, assets_folder);
}

// ----------------------------------------------------------------------------------------------

struct DeleteAssetSettingEventData {
	unsigned int name_size;
	unsigned int file_size;
	ECS_ASSET_TYPE type;
};

EDITOR_EVENT(DeleteAssetSettingEvent) {
	DeleteAssetSettingEventData* data = (DeleteAssetSettingEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		unsigned int sizes[] = {
			data->name_size,
			data->file_size * sizeof(wchar_t)
		};

		Stream<char> name = function::GetCoallescedStreamFromType(data, 0, sizes).As<char>();
		Stream<wchar_t> file = function::GetCoallescedStreamFromType(data, 1, sizes).As<wchar_t>();

		unsigned int handle = editor_state->asset_database->FindAsset(name, file, data->type);
		if (handle != -1) {
			// Deallocate the asset from the resource manager before removing from the asset database
			bool success = DeallocateAsset(editor_state, handle, data->type);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to deallocate asset {#}, type {#}.", name, ConvertAssetTypeString(data->type));
				EditorSetConsoleError(error_message);
			}

			// Remove it from the asset database
			editor_state->asset_database->RemoveAssetForced(handle, data->type);

			// Remove it from the sandbox references
			for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
				EditorSandbox* sandbox = GetSandbox(editor_state, index);
				unsigned int reference_index = sandbox->database.GetIndex(handle, data->type);
				sandbox->database.RemoveAssetThisOnly(reference_index, data->type);
			}
		}

		// Now delete its disk file
		ECS_STACK_CAPACITY_STREAM(wchar_t, file_path, 512);
		editor_state->asset_database->FileLocationAsset(name, file, file_path, data->type);
		if (ExistsFileOrFolder(file_path)) {
			bool success = RemoveFile(file_path);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to delete metadata file {#}.", file_path);
				EditorSetConsoleError(error_message);
			}
		}

		return false;
	}
	else {
		return true;
	}
}

void DeleteAssetSetting(EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
{
	size_t storage[256];
	unsigned int write_size = 0;
	Stream<void> buffers[] = {
		name,
		file
	};

	DeleteAssetSettingEventData* data = function::CreateCoallescedStreamsIntoType<DeleteAssetSettingEventData>(storage, { buffers, std::size(buffers) }, &write_size);
	data->name_size = name.size;
	data->file_size = file.size;
	data->type = type;
	EditorAddEvent(editor_state, DeleteAssetSettingEvent, data, write_size);
}

// ----------------------------------------------------------------------------------------------

bool ExistsAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
{
	return FindAsset(editor_state, name, file, type) != -1;
}

// ----------------------------------------------------------------------------------------------

unsigned int FindAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) {
	return editor_state->asset_database->FindAsset(name, file, type);
}

// ----------------------------------------------------------------------------------------------

void FromAssetNameToThunkOrForwardingFile(Stream<char> name, Stream<wchar_t> extension, CapacityStream<wchar_t>& relative_path)
{
	function::ConvertASCIIToWide(relative_path, name);
	// Replace underscores with path separators
	function::ReplaceCharacter(relative_path, L'_', ECS_OS_PATH_SEPARATOR);
	relative_path.AddStreamSafe(extension);
}

// ----------------------------------------------------------------------------------------------

void FromAssetNameToThunkOrForwardingFile(Stream<char> name, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& relative_path)
{
	Stream<wchar_t> extension = { nullptr, 0 };
	
	switch (type) {
	case ECS_ASSET_GPU_SAMPLER:
		extension = ASSET_GPU_SAMPLER_EXTENSIONS[0];
		break;
	case ECS_ASSET_SHADER:
		extension = ASSET_SHADER_EXTENSIONS[0];
		break;
	case ECS_ASSET_MATERIAL:
		extension = ASSET_MATERIAL_EXTENSIONS[0];
		break;
	case ECS_ASSET_MISC:
		extension = ASSET_MISC_EXTENSIONS[0];
		break;
	default:
		ECS_ASSERT(false, "Invalid asset type.");
	}

	FromAssetNameToThunkOrForwardingFile(name, extension, relative_path);
}

// ----------------------------------------------------------------------------------------------

void GetAssetNameFromThunkOrForwardingFile(const EditorState* editor_state, Stream<wchar_t> absolute_path, CapacityStream<char>& name)
{
	Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, absolute_path);
	ECS_ASSERT(relative_path.size > 0);
	GetAssetNameFromThunkOrForwardingFileRelative(editor_state, relative_path, name);
}

// ----------------------------------------------------------------------------------------------

void GetAssetNameFromThunkOrForwardingFileRelative(const EditorState* editor_state, Stream<wchar_t> relative_path, CapacityStream<char>& name)
{
	// Eliminate the extension
	Stream<wchar_t> extension = function::PathExtension(relative_path);
	relative_path.size -= extension.size;

	function::ConvertWideCharsToASCII(relative_path, name);
	// Replace backslashes or forward slashes with underscores
	function::ReplaceCharacter(name, ECS_OS_PATH_SEPARATOR_ASCII, '_');
	function::ReplaceCharacter(name, ECS_OS_PATH_SEPARATOR_ASCII_REL, '_');
}

// ----------------------------------------------------------------------------------------------

bool GetAssetFileFromForwardingFile(Stream<wchar_t> absolute_path, CapacityStream<wchar_t>& target_path)
{
	const size_t MAX_SIZE = ECS_KB * 32;
	char buffer[MAX_SIZE];

	ECS_FILE_HANDLE file_handle = 0;
	ECS_FILE_STATUS_FLAGS status = OpenFile(absolute_path, &file_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_BINARY);
	if (status != ECS_FILE_STATUS_OK) {
		return false;
	}

	size_t file_byte_size = GetFileByteSize(file_handle);

	ScopedFile scoped_file{ { file_handle } };

	if (file_byte_size == 0) {
		return true;
	}

	if (file_byte_size > MAX_SIZE) {
		return false;
	}

	unsigned int read_size = ReadFromFile(file_handle, { buffer, ECS_KB * 32 });
	if (read_size == -1) {
		return false;
	}
	if (read_size > target_path.capacity - target_path.size) {
		// Fail if the file is too big
		return false;
	}
	target_path.AddStream({ buffer, read_size / sizeof(wchar_t) });
	return true;
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<char>> GetAssetCorrespondingMetadata(const EditorState* editor_state, Stream<wchar_t> file, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator)
{
	return editor_state->asset_database->GetMetadatasForFile(file, type, allocator);
}

// ----------------------------------------------------------------------------------------------

void TickDefaultMetadataForAssets(EditorState* editor_state)
{
	// Every second tick this
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_DEFAULT_METADATA_FOR_ASSETS, 1'000)) {
		CreateAssetDefaultSetting(editor_state);
	}
}

// ----------------------------------------------------------------------------------------------

bool WriteForwardingFile(Stream<wchar_t> absolute_path, Stream<wchar_t> target_path)
{
	return WriteBufferToFileBinary(absolute_path, target_path) == ECS_FILE_STATUS_OK;
}

// ----------------------------------------------------------------------------------------------
