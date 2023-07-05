#include "ecspch.h"
#include "RuntimeResources.h"
#include "SystemManager.h"
#include "../Rendering/RenderingStructures.h"
#include "../ECS/InternalStructures.h"

#define CAMERA_IDENTIFIER "__RuntimeCamera"
#define EDITOR_IDENTIFIER "__Editor"
#define SELECTED_ENTITIES_IDENTIFIER "__SelectedEntities"
#define SELECT_COLOR_IDENTIFIER "__SelectColor"
#define TRANSFORM_TOOL_IDENTIFIER "__TransformTool"

namespace ECSEngine {
	
	template<typename T>
	bool GetRuntimeResource(const SystemManager* system_manager, T* resource, Stream<char> identifier) {
		void* runtime_resource = system_manager->TryGetData(identifier);
		if (runtime_resource != nullptr) {
			*resource = *(T*)runtime_resource;
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool GetRuntimeCamera(const SystemManager* system_manager, Camera* camera)
	{
		return GetRuntimeResource(system_manager, camera, CAMERA_IDENTIFIER);
	}

	void SetRuntimeCamera(SystemManager* system_manager, const Camera* camera)
	{
		Camera* bound_camera = (Camera*)system_manager->BindData(CAMERA_IDENTIFIER, camera, sizeof(*camera));
	}

	void RemoveRuntimeCamera(SystemManager* system_manager)
	{
		system_manager->RemoveData(CAMERA_IDENTIFIER);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool IsEditorRuntime(const SystemManager* system_manager) {
		return system_manager->TryGetData(EDITOR_IDENTIFIER) != nullptr;
	}

	void SetEditorRuntime(SystemManager* system_manager) {
		bool dummy = false;
		system_manager->BindData(EDITOR_IDENTIFIER, &dummy, sizeof(dummy));
	}

	void RemoveEditorRuntime(SystemManager* system_manager) {
		system_manager->RemoveData(EDITOR_IDENTIFIER);
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<Entity> GetEditorRuntimeSelectedEntities(const SystemManager* system_manager) {
		Stream<Entity> entities = { nullptr, 0 };
		void* data = system_manager->TryGetData(SELECTED_ENTITIES_IDENTIFIER);
		if (data != nullptr) {
			entities = *(Stream<Entity>*)data;
		}
		return entities;
	}

	void SetEditorRuntimeSelectedEntities(SystemManager* system_manager, Stream<Entity> entities) {
		void* allocated_data = function::CoallesceStreamWithData(system_manager->Allocator(), entities, sizeof(entities[0]));
		system_manager->BindData(SELECTED_ENTITIES_IDENTIFIER, allocated_data);
	}

	void RemoveEditorRuntimeSelectedEntities(SystemManager* system_manager) {
		system_manager->RemoveData(SELECTED_ENTITIES_IDENTIFIER);
	}

	// ------------------------------------------------------------------------------------------------------------

	Color GetEditorRuntimeSelectColor(const SystemManager* system_manager) {
		Color color;
		if (GetRuntimeResource(system_manager, &color, SELECT_COLOR_IDENTIFIER)) {
			return color;
		}
		else {
			return ECS_COLOR_BLACK;
		}
	}

	void SetEditorRuntimeSelectColor(SystemManager* system_manager, Color color) {
		system_manager->BindData(SELECT_COLOR_IDENTIFIER, &color, sizeof(color));
	}

	void RemoveEditorRuntimeSelectColor(SystemManager* system_manager) {
		system_manager->RemoveData(SELECT_COLOR_IDENTIFIER);
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_TRANSFORM_TOOL GetEditorRuntimeTransformTool(const SystemManager* system_manager)
	{
		ECS_TRANSFORM_TOOL tool;
		if (GetRuntimeResource(system_manager, &tool, TRANSFORM_TOOL_IDENTIFIER)) {
			return tool;
		}
		else {
			return ECS_TRANSFORM_COUNT;
		}
	}

	void SetEditorRuntimeTransformTool(SystemManager* system_manager, ECS_TRANSFORM_TOOL tool)
	{
		system_manager->BindData(TRANSFORM_TOOL_IDENTIFIER, &tool, sizeof(tool));
	}

	void RemoveEditorRuntimeTransformTool(SystemManager* system_manager)
	{
		system_manager->RemoveData(TRANSFORM_TOOL_IDENTIFIER);
	}

	// ------------------------------------------------------------------------------------------------------------

}