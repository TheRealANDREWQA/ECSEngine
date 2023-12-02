#include "editorpch.h"
#include "OpenPrefab.h"
#include "../Assets/Prefab.h"
#include "../Assets/PrefabFile.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/SandboxModule.h"
#include "../Project/ProjectFolders.h"
#include "../Modules/Module.h"

using namespace ECSEngine;

/*
	The launching sandbox is used to inherit all the modules from that sandbox
*/

// Returns true if the prefab load was successful, else false
// It will destroy the sandbox in case it fails and it will output an error message
static bool InitializePrefabSandboxInformation(EditorState* editor_state, unsigned int sandbox_index, unsigned int launching_sandbox, unsigned int prefab_id) {
	Stream<wchar_t> prefab_relative_path = GetPrefabPath(editor_state, prefab_id);
	ECS_STACK_CAPACITY_STREAM(wchar_t, prefab_path, 512);
	GetProjectAssetsFolder(editor_state, prefab_path);
	prefab_path.Add(ECS_OS_PATH_SEPARATOR);
	prefab_path.AddStreamAssert(prefab_relative_path);

	Entity created_entity;
	bool success = AddPrefabToSandbox(editor_state, sandbox_index, prefab_path, &created_entity);
	if (success) {
		// We need to assign the prefab flag, change the path and copy the modules + their settings
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_PREFAB);
		ChangeTemporarySandboxScenePath(editor_state, sandbox_index, prefab_relative_path);

		CopySandboxModulesFromAnother(editor_state, sandbox_index, launching_sandbox);
		CopySandboxModuleSettingsFromAnother(editor_state, sandbox_index, launching_sandbox);
		return true;
	}
	else {
		DestroySandbox(editor_state, sandbox_index);
		Stream<wchar_t> prefab_path = GetPrefabPath(editor_state, prefab_id);
		ECS_FORMAT_TEMP_STRING(message, "Failed to load prefab {#} for preview", prefab_path);
		EditorSetConsoleError(message);
		return false;
	}
}

static void CreatePrefabPreviewWindows(EditorState* editor_state, unsigned int sandbox_index, unsigned int prefab_id) {
	// We need an entity viewer
}

struct OpenPrefabEventData {
	unsigned int create_sandbox_index;
	unsigned int launching_sandbox;
	unsigned int prefab_id;
};

EDITOR_EVENT(OpenPrefabEvent) {
	OpenPrefabEventData* data = (OpenPrefabEventData*)_data;
	if (IsSandboxLocked(editor_state, data->create_sandbox_index)) {
		return true;
	}

	bool success = InitializePrefabSandboxInformation(editor_state, data->create_sandbox_index, data->launching_sandbox, data->prefab_id);
	return false;
}

// Returns the index of this newly created temporary sandbox. Returns -1 if the
// Prefab load failed
static unsigned int CreatePrefabSandbox(EditorState* editor_state, unsigned int prefab_id, unsigned int launching_sandbox, bool* event_launched) {
	*event_launched = false;

	unsigned int sandbox_index = CreateSandboxTemporary(editor_state);
	if (IsSandboxLocked(editor_state, sandbox_index)) {
		*event_launched = true;
		// We need to launch an event to wait for the initialization to occur

		return sandbox_index;
	}

	bool success = InitializePrefabSandboxInformation(editor_state, sandbox_index, launching_sandbox, prefab_id);
	return success ? sandbox_index : -1;
}

void OpenPrefabAction(ActionData* action_data) {
	/*
		Create a temporary sandbox where this prefab is loaded and alongside it an entities window, an inspector window
		To control the input and a scene window to see the results. These windows can mirror the location of the already 
		existing editing windows such that the user doesn't see a difference. When the editing is finished, remove these 
		created windows and the temporary sandbox. 
		There is one more thing. In case this prefab is already opened, then highlight the scene and don't create a new entry
	*/

	UI_UNPACK_ACTION_DATA;

	OpenPrefabActionData* data = (OpenPrefabActionData*)_data;

}