#include "editorpch.h"
#include "ECSEngineComponents.h"
#include "OpenPrefab.h"
#include "../Assets/Prefab.h"
#include "../Assets/PrefabFile.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/SandboxModule.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Project/ProjectFolders.h"
#include "../Modules/Module.h"
#include "Scene.h"
#include "InspectorData.h"
#include "Inspector.h"
#include "EntitiesUI.h"
#include "../Editor/EditorEvent.h"
#include "../Assets/AssetManagement.h"

using namespace ECSEngine;

/*
	The launching sandbox is used to inherit all the modules from that sandbox
*/

#define DECREMENT_ASSET_REFERENCES_DURATION_MS 1000

static unsigned int GetEntitiesUIIndexForSandbox(const EditorState* editor_state, unsigned int launching_sandbox_index) {
	unsigned int entities_ui_index = -1;
	for (size_t index = 0; index < MAX_ENTITIES_UI_WINDOWS && entities_ui_index == -1; index++) {
		if (GetEntitiesUITargetSandbox(editor_state, index) == launching_sandbox_index) {
			entities_ui_index = GetEntitiesUIWindowIndex(editor_state, index);
		}
	}
	return entities_ui_index;
}

// Returns true if the prefab load was successful, else false
// It will destroy the sandbox in case it fails and it will output an error message
static bool InitializePrefabSandboxInformation(OpenPrefabActionData* action_data, unsigned int create_sandbox_index) {
	EditorState* editor_state = action_data->editor_state;

	Stream<wchar_t> prefab_relative_path = GetPrefabPath(editor_state, action_data->prefab_id);
	ECS_STACK_CAPACITY_STREAM(wchar_t, prefab_path, 512);
	GetProjectAssetsFolder(editor_state, prefab_path);
	prefab_path.Add(ECS_OS_PATH_SEPARATOR);
	prefab_path.AddStreamAssert(prefab_relative_path);

	Entity created_entity;
	bool success = AddPrefabToSandbox(editor_state, create_sandbox_index, prefab_path, &created_entity);
	if (success) {
		// We need to assign the prefab flag, change the path and copy the modules + their settings
		EditorSandbox* sandbox = GetSandbox(editor_state, create_sandbox_index);
		sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_PREFAB);
		RenameSandboxScenePath(editor_state, create_sandbox_index, prefab_relative_path);
		CopySandboxModulesFromAnother(editor_state, create_sandbox_index, action_data->launching_sandbox);
		CopySandboxModuleSettingsFromAnother(editor_state, create_sandbox_index, action_data->launching_sandbox);

		// Set the scene camera position to be the one from the launching sandbox
		OrientedPoint launching_camera = GetSandboxCameraPoint(editor_state, action_data->launching_sandbox, EDITOR_SANDBOX_VIEWPORT_SCENE);
		SetSandboxCameraTranslation(editor_state, create_sandbox_index, launching_camera.position, EDITOR_SANDBOX_VIEWPORT_SCENE, true);
		SetSandboxCameraRotation(editor_state, create_sandbox_index, launching_camera.rotation, EDITOR_SANDBOX_VIEWPORT_SCENE, true);
		return true;
	}
	else {
		DestroySandbox(editor_state, create_sandbox_index);
		ECS_FORMAT_TEMP_STRING(message, "Failed to load prefab {#} for preview", prefab_relative_path);
		EditorSetConsoleError(message);

		// Unlock the launching sandbox
		UnlockSandbox(editor_state, action_data->launching_sandbox);
		return false;
	}
}

struct DecrementDelayedAssetReferencesData {
	Timer timer;
	Stream<AssetTypedHandle> handles;
};

EDITOR_EVENT(DecrementDelayedAssetReferences) {
	DecrementDelayedAssetReferencesData* data = (DecrementDelayedAssetReferencesData*)_data;
	if (data->timer.GetDuration(ECS_TIMER_DURATION_MS) >= DECREMENT_ASSET_REFERENCES_DURATION_MS) {
		for (size_t index = 0; index < data->handles.size; index++) {
			DecrementAssetReference(editor_state, data->handles[index].handle, data->handles[index].type, -1);
		}
		data->handles.Deallocate(editor_state->EditorAllocator());
		return false;
	}
	return true;
}

struct DestroyAuxiliaryWindowsEventData {
	// These 2 indices are used to make the original
	// Windows the ones being displayed in their dockspace region
	unsigned int inspector_index;
	unsigned int launched_sandbox_index;

	unsigned int created_sandbox_index;
};

EDITOR_EVENT(DestroyAuxiliaryWindowsEvent) {
	DestroyAuxiliaryWindowsEventData* data = (DestroyAuxiliaryWindowsEventData*)_data;
	// If any UI windows is destroyed, destroy the other ones as well
	unsigned int inspector_ui_index = -1;
	unsigned int inspector_index = -1;
	ECS_STACK_CAPACITY_STREAM(unsigned int, created_sandbox_inspector_index, MAX_INSPECTOR_WINDOWS);
	GetInspectorsForMatchingSandbox(editor_state, data->created_sandbox_index, &created_sandbox_inspector_index);
	if (created_sandbox_inspector_index.size > 0) {
		inspector_ui_index = GetInspectorUIWindowIndex(editor_state, created_sandbox_inspector_index[0]);
		inspector_index = created_sandbox_inspector_index[0];
	}
	
	unsigned int entities_ui_index = GetEntitiesUIIndexForSandbox(editor_state, data->created_sandbox_index);
	unsigned int scene_ui_index = GetSceneUIWindowIndex(editor_state, data->created_sandbox_index);
	
	if (inspector_ui_index == -1 || entities_ui_index == -1 || scene_ui_index == -1) {
		// One of the windows was destroyed, destroy all the other ones as well
		if (inspector_ui_index != -1) {
			editor_state->ui_system->DestroyWindowEx(inspector_ui_index);
			// This window destruction will destroy the inspector instance as well

			// We need to take again the entities ui index and scene ui index since
			// the window destruction might have made their index be changed
			if (entities_ui_index != -1) {
				entities_ui_index = GetEntitiesUIIndexForSandbox(editor_state, data->created_sandbox_index);
			}
			if (scene_ui_index != -1) {
				scene_ui_index = GetSceneUIWindowIndex(editor_state, data->created_sandbox_index);
			}
		}

		if (entities_ui_index != -1) {
			editor_state->ui_system->DestroyWindowEx(entities_ui_index);
			
			// Need to re-get the scene ui index
			if (scene_ui_index != -1) {
				scene_ui_index = GetSceneUIWindowIndex(editor_state, data->created_sandbox_index);
			}
		}

		if (scene_ui_index != -1) {
			editor_state->ui_system->DestroyWindowEx(scene_ui_index);
		}

		// Restore the previous windows in their respective regions
		// This is not entirely correct since it may get swapped - but it will do
		bool exists_previous_inspector = ExistsInspector(editor_state, data->inspector_index);
		unsigned int previous_inspector_ui_index = -1;
		if (exists_previous_inspector) {
			previous_inspector_ui_index = GetInspectorUIWindowIndex(editor_state, data->inspector_index);
		}
		else {
			ECS_STACK_CAPACITY_STREAM(unsigned int, previous_inspector_indices, MAX_INSPECTOR_WINDOWS);
			GetInspectorsForMatchingSandbox(editor_state, data->launched_sandbox_index, &previous_inspector_indices);
			if (previous_inspector_indices.size > 0) {
				previous_inspector_ui_index = GetInspectorUIWindowIndex(editor_state, previous_inspector_indices[0]);
			}
		}

		if (previous_inspector_ui_index != -1) {
			editor_state->ui_system->SetActiveWindowInBorder(previous_inspector_ui_index);
		}

		unsigned int previous_entities_ui_index = GetEntitiesUIIndexForSandbox(editor_state, data->launched_sandbox_index);
		if (previous_entities_ui_index != -1) {
			editor_state->ui_system->SetActiveWindowInBorder(previous_entities_ui_index);
		}

		unsigned int previous_scene_ui_index = GetSceneUIWindowIndex(editor_state, data->launched_sandbox_index);
		if (previous_scene_ui_index != -1) {
			editor_state->ui_system->SetActiveWindowInBorder(previous_scene_ui_index);
		}

		// We can now safely destroy the prefab sandbox
		// But before that an optimization: instead of removing the assets now
		// And then having to reload them again later on in the sandboxes that are affected,
		// We can do a trick - increment the reference count for all assets inside this sandbox
		// And then one second later decrement the counts. In this way, all the assets will stay in
		// Memory for the update of the assets
		const EditorSandbox* sandbox = GetSandbox(editor_state, data->created_sandbox_index);
		unsigned int sandbox_asset_count = sandbox->database.GetCount();
		DecrementDelayedAssetReferencesData delayed_event_data;
		delayed_event_data.handles.Initialize(editor_state->EditorAllocator(), sandbox_asset_count);
		delayed_event_data.handles.size = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE type = (ECS_ASSET_TYPE)index;
			sandbox->database.ForEachAssetDuplicates(type, [&](unsigned int handle) {
				delayed_event_data.handles.Add({ handle, type });
				editor_state->asset_database->AddAsset(handle, type);
			});
		}
		delayed_event_data.timer.SetNewStart();
		EditorAddEvent(editor_state, DecrementDelayedAssetReferences, &delayed_event_data, sizeof(delayed_event_data));

		DestroySandbox(editor_state, data->created_sandbox_index, false, true);
		UnlockSandbox(editor_state, data->launched_sandbox_index);
		return false;
	}
	return true;
}

static void CreatePrefabPreviewWindows(OpenPrefabActionData* action_data, unsigned int created_sandbox_index) {
	// We need an entity viewer, inspector window and scene window
	EditorState* editor_state = action_data->editor_state;
	UISystem* ui_system = editor_state->ui_system;

	unsigned int inspector_instance_index = CreateInspectorInstance(editor_state);
	SetInspectorMatchingSandbox(editor_state, inspector_instance_index, created_sandbox_index);

	// The inspector window
	unsigned int inspector_ui_index = ExistsInspector(editor_state, action_data->inspector_index) ? 
		GetInspectorUIWindowIndex(editor_state, action_data->inspector_index) : -1;
	if (inspector_ui_index != -1) {
		// Create it inside that border and make it active inside that border
		unsigned int inspector_border_index;
		DockspaceType inspector_dockspace_type;
		UIDockspace* inspector_dockspace = ui_system->GetDockspaceFromWindow(inspector_ui_index, inspector_border_index, inspector_dockspace_type);

		unsigned int new_inspector_ui_index = CreateInspectorWindow(editor_state, inspector_instance_index);
		ui_system->AddWindowToDockspaceRegion(new_inspector_ui_index, inspector_dockspace, inspector_border_index);
	}
	else {
		// Create it as a floating inspector
		CreateInspectorDockspace(editor_state, inspector_instance_index);
	}

	// The entities window
	unsigned int entities_ui_index = GetEntitiesUIIndexForSandbox(editor_state, action_data->launching_sandbox);
	unsigned int new_entities_ui_index = -1;
	if (entities_ui_index != -1) {
		unsigned int entities_border_index;
		DockspaceType entities_dockspace_type;
		UIDockspace* entities_dockspace = ui_system->GetDockspaceFromWindow(entities_ui_index, entities_border_index, entities_dockspace_type);

		unsigned int new_entities_index = GetEntitiesUILastWindowIndex(editor_state) + 1;
		new_entities_ui_index = CreateEntitiesUIWindow(editor_state, new_entities_index);
		ui_system->AddWindowToDockspaceRegion(new_entities_ui_index, entities_dockspace, entities_border_index);
	}
	else {
		new_entities_ui_index = CreateEntitiesUI(editor_state);
	}
	SetEntitiesUITargetSandbox(editor_state, new_entities_ui_index, created_sandbox_index);

	// Create the scene window
	unsigned int existing_scene_ui_index = GetSceneUIWindowIndex(editor_state, action_data->launching_sandbox);
	if (existing_scene_ui_index != -1) {
		unsigned int scene_border_index;
		DockspaceType scene_dockspace_type;
		UIDockspace* scene_dockspace = ui_system->GetDockspaceFromWindow(existing_scene_ui_index, scene_border_index, scene_dockspace_type);

		unsigned int new_scene_ui_index = CreateSceneUIWindowOnly(editor_state, created_sandbox_index);
		ui_system->AddWindowToDockspaceRegion(new_scene_ui_index, scene_dockspace, scene_border_index);
	}
	else {
		CreateSceneUIWindow(editor_state, created_sandbox_index);
	}

	// The prefab entity is always going to be the first one
	Entity prefab_entity = 0;
	ChangeSandboxSelectedEntities(editor_state, created_sandbox_index, { &prefab_entity, 1 });
	ChangeInspectorEntitySelection(editor_state, created_sandbox_index);
	FocusSceneUIOnSelection(editor_state, created_sandbox_index);

	DestroyAuxiliaryWindowsEventData event_data;
	event_data.launched_sandbox_index = action_data->launching_sandbox;
	event_data.inspector_index = action_data->inspector_index;
	event_data.created_sandbox_index = created_sandbox_index;
	EditorAddEvent(editor_state, DestroyAuxiliaryWindowsEvent, &event_data, sizeof(event_data));
}

struct OpenPrefabEventData {
	unsigned int create_sandbox_index;
	OpenPrefabActionData action_data;
};

EDITOR_EVENT(OpenPrefabEvent) {
	OpenPrefabEventData* data = (OpenPrefabEventData*)_data;
	if (IsSandboxLocked(editor_state, data->create_sandbox_index)) {
		return true;
	}

	bool success = InitializePrefabSandboxInformation(&data->action_data, data->create_sandbox_index);
	if (success) {
		CreatePrefabPreviewWindows(&data->action_data, data->create_sandbox_index);
	}
	return false;
}

// Returns true if there is a preview opened for that prefab id
static bool IsPrefabPreviewOpened(const EditorState* editor_state, unsigned int prefab_id, unsigned int& sandbox_index) {
	return SandboxAction<true>(editor_state, -1, [&](unsigned int current_sandbox_index) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, current_sandbox_index);
		if (HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_PREFAB)) {
			// Get the prefab component from the entity
			Entity prefab_entity = 0;
			const PrefabComponent* prefab_component = GetSandboxEntityComponent<PrefabComponent>(editor_state, current_sandbox_index, prefab_entity);
			if (prefab_component->id == prefab_id) {
				sandbox_index = current_sandbox_index;
				return true;
			}
			return false;
		}
		return false;
	});
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
	EditorState* editor_state = data->editor_state;

	unsigned int existing_prefab_sandbox_index = -1;
	if (!IsPrefabPreviewOpened(editor_state, data->prefab_id, existing_prefab_sandbox_index)) {
		unsigned int sandbox_index = CreateSandboxTemporary(editor_state);
		// Lock the launching sandbox such that we can later on restore its UI windows
		LockSandbox(editor_state, data->launching_sandbox);

		OpenPrefabEventData event_data;
		event_data.action_data = *data;
		event_data.create_sandbox_index = sandbox_index;
		if (IsSandboxLocked(editor_state, sandbox_index)) {
			// We need to launch an event to wait for the initialization to occur
			EditorAddEvent(editor_state, OpenPrefabEvent, &event_data, sizeof(event_data));
		}
		else {
			// We can run the event now
			OpenPrefabEvent(editor_state, &event_data);
		}
	}
	else {
		// Highlight the scene window for that sandbox
		unsigned int scene_ui_index = GetSceneUIWindowIndex(editor_state, existing_prefab_sandbox_index);
		if (scene_ui_index != -1) {
			editor_state->ui_system->SetActiveWindow(scene_ui_index);
		}
	}
}