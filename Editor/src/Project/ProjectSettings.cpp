#include "editorpch.h"
#include "ProjectSettings.h"
#include "../Editor/EditorState.h"
#include "ProjectFolders.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

bool ReadProjectSettings(EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, file_path, 512);
	GetProjectSettingsFilePath(editor_state, file_path);
	if (ExistsFileOrFolder(file_path)) {
		const ReflectionManager* reflection_manager = editor_state->EditorReflectionManager();
		return Deserialize(reflection_manager, reflection_manager->GetType(STRING(ProjectSettings)), &editor_state->project_settings, file_path) == ECS_DESERIALIZE_OK;
	}
	else {
		// Set default values
		editor_state->project_settings = {};
		return true;
	}
}

bool WriteProjectSettings(const EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, file_path, 512);
	GetProjectSettingsFilePath(editor_state, file_path);
	const ReflectionManager* reflection_manager = editor_state->EditorReflectionManager();
	return Serialize(reflection_manager, reflection_manager->GetType(STRING(ProjectSettings)), &editor_state->project_settings, file_path) == ECS_SERIALIZE_OK;
}

ProjectSettings ProjectSettingsLowerBound() {
	ProjectSettings settings;

	settings.fixed_timestep = 0.0f;

	return settings;
}

ProjectSettings ProjectSettingsUpperBound() {
	ProjectSettings settings;

	settings.fixed_timestep = 1.0f;

	return settings;
}