#include "editorpch.h"
#include "ProjectRenameFile.h"
#include "../Editor/EditorState.h"
#include "../Assets/AssetExtensions.h"
#include "../Assets/PrefabFile.h"
#include "ProjectFolders.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/SandboxFile.h"

static void RenamePrefabFile(
	EditorState* editor_state, 
	Stream<wchar_t> path, 
	Stream<wchar_t> rename_absolute_path,
	Stream<wchar_t> relative_path,
	Stream<wchar_t> rename_relative_path
) {
	// Determine all active assets in the editor and change the name if necessary
	unsigned int prefab_index = editor_state->prefabs.FindIndexFunctor([&](const PrefabInstance& prefab_instance) {
		return prefab_instance.path == relative_path;
	});
	if (prefab_index != -1) {
		RenamePrefabID(editor_state, editor_state->prefabs.GetHandleFromIndex(prefab_index), rename_relative_path);
	}
}

static void RenameAssetFile(
	EditorState* editor_state, 
	Stream<wchar_t> path, 
	Stream<wchar_t> rename_absolute_path, 
	ECS_ASSET_TYPE asset_type,
	Stream<wchar_t> relative_path,
	Stream<wchar_t> relative_rename_path
) {
	// For assets, things are a little bit more involved. We have to rename the file itself,
	// Its corresponding metadata and any other corresponding asset that references this file
	
	// Start with the metadata file renaming
	if (asset_type == ECS_ASSET_MESH || asset_type == ECS_ASSET_TEXTURE) {
		// These 2 need to be handled separately since the name is reflected in their path
		editor_state->asset_database->ForEachAsset(asset_type, [&](unsigned int handle) {
			const void* metadata = editor_state->asset_database->GetAssetConst(handle, asset_type);
			Stream<wchar_t> file = GetAssetFile(metadata, asset_type);
			if (relative_path == file) {
				bool renamed_dependencies = false;
				bool rename_success = editor_state->asset_database->RenameAssetFile(handle, asset_type, relative_rename_path, &renamed_dependencies);
				if (!rename_success) {
					ECS_FORMAT_TEMP_STRING(message, "Failed to rename asset {#}, file {#} to new file {#}", GetAssetName(metadata, asset_type), file, relative_rename_path);
					EditorSetConsoleWarn(message);
				}
				else {
					if (!renamed_dependencies) {
						ECS_FORMAT_TEMP_STRING(message, "Failed to rename asset {#}, file {#} dependent metadata files", GetAssetName(metadata, asset_type), relative_rename_path);
						EditorSetConsoleWarn(message);
					}
				}
			}
		});

	}
	else if (asset_type == ECS_ASSET_GPU_SAMPLER || asset_type == ECS_ASSET_SHADER || asset_type == ECS_ASSET_MATERIAL || asset_type == ECS_ASSET_MISC) {
		ECS_STACK_CAPACITY_STREAM(char, ascii_relative_path, 512);
		ECS_STACK_CAPACITY_STREAM(char, ascii_new_relative_path, 512);
		ConvertWideCharsToASCII(relative_path, ascii_relative_path);
		ConvertWideCharsToASCII(relative_rename_path, ascii_new_relative_path);

		// Here we can rename directly
		editor_state->asset_database->ForEachAsset(asset_type, [&](unsigned int handle) {
			const void* metadata = editor_state->asset_database->GetAssetConst(handle, asset_type);
			Stream<char> name = GetAssetName(metadata, asset_type);
			if (name == ascii_relative_path) {
				bool renamed_dependencies = false;
				bool rename_success = editor_state->asset_database->RenameAsset(handle, asset_type, ascii_new_relative_path, &renamed_dependencies);
				if (!rename_success) {
					ECS_FORMAT_TEMP_STRING(message, "Failed to rename asset {#} to {#}", GetAssetName(metadata, asset_type), ascii_new_relative_path);
					EditorSetConsoleWarn(message);
				}
				else {
					if (!renamed_dependencies) {
						ECS_FORMAT_TEMP_STRING(message, "Failed to rename asset {#} dependent metadata files", ascii_new_relative_path);
						EditorSetConsoleWarn(message);
					}
				}
			}
		});
	}
	else {
		ECS_ASSERT(false, "Invalid asset type");
	}
}

static void RenameSceneFile(
	EditorState* editor_state, 
	Stream<wchar_t> path, 
	Stream<wchar_t> rename_absolute_path,
	Stream<wchar_t> relative_path,
	Stream<wchar_t> rename_relative_path
) {
	// Perform the renaming first and then update all sandboxes that reference
	// this scene file. We need to exclude the temporary sandboxes
	bool renamed = false;
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (sandbox->scene_path == relative_path) {
			RenameSandboxScenePath(editor_state, sandbox_index, rename_relative_path);
			renamed = true;
		}
	}, true);

	if (renamed) {
		// Save the editor sandbox file to be persisted
		bool success = SaveEditorSandboxFile(editor_state);
		if (!success) {
			EditorSetConsoleWarn("Failed to save sandbox file after renaming a sandbox scene");
		}
	}
}

bool ProjectRenameFile(EditorState* editor_state, Stream<wchar_t> path, Stream<wchar_t> rename_filename) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	Stream<wchar_t> path_parent = PathParent(path);
	absolute_path.CopyOther(path_parent);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStreamAssert(rename_filename);
	if (PathExtension(rename_filename).size == 0) {
		// Add the extension of the main
		absolute_path.AddStreamAssert(PathExtension(path));
	}
	return ProjectRenameFileAbsolute(editor_state, path, absolute_path);
}

bool ProjectRenameFileAbsolute(EditorState* editor_state, Stream<wchar_t> path, Stream<wchar_t> rename_absolute_path) {
	// Determine if it is an asset, prefab or scene file, since at the moment only these are of interest
	// The base case, renaming the file, should be performed firstly since no action should be taken
	// If we failed to change the file name
	bool success = RenameFileAbsolute(path, rename_absolute_path);
	if (success) {
		// Use stack storage for relative paths since we need to change the separator
		ECS_STACK_CAPACITY_STREAM(wchar_t, relative_path_storage, 512);
		ECS_STACK_CAPACITY_STREAM(wchar_t, rename_relative_path_storage, 512);
		Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, path);
		relative_path_storage.CopyOther(relative_path);
		relative_path = relative_path_storage;
		ReplaceCharacter(relative_path, ECS_OS_PATH_SEPARATOR, ECS_OS_PATH_SEPARATOR_REL);
		Stream<wchar_t> rename_relative_path = GetProjectAssetRelativePath(editor_state, rename_absolute_path);
		rename_relative_path_storage.CopyOther(rename_relative_path);
		rename_relative_path = rename_relative_path_storage;
		ReplaceCharacter(rename_relative_path, ECS_OS_PATH_SEPARATOR, ECS_OS_PATH_SEPARATOR_REL);

		Stream<wchar_t> path_extension = PathExtension(path);
		if (path_extension == PREFAB_FILE_EXTENSION) {
			RenamePrefabFile(editor_state, path, rename_absolute_path, relative_path, rename_relative_path);
		}
		else {
			// The asset files
			bool is_asset_type = false;
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				Stream<Stream<wchar_t>> extensions = ASSET_EXTENSIONS[index];
				is_asset_type = extensions.Find(path_extension) != -1;
				if (is_asset_type) {
					RenameAssetFile(editor_state, path, rename_absolute_path, (ECS_ASSET_TYPE)index, relative_path, rename_relative_path);
					break;
				}
			}

			// Check the scene file at last
			if (!is_asset_type && path_extension == EDITOR_SCENE_EXTENSION) {
				RenameSceneFile(editor_state, path, rename_absolute_path, relative_path, rename_relative_path);
			}
		}
	}
	return success;
}