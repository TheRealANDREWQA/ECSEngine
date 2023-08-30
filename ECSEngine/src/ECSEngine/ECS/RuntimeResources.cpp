#include "ecspch.h"
#include "RuntimeResources.h"
#include "SystemManager.h"
#include "../Rendering/RenderingStructures.h"
#include "../Rendering/Camera.h"
#include "../ECS/InternalStructures.h"

#define CAMERA_IDENTIFIER "__RuntimeCamera"
#define EDITOR_IDENTIFIER "__Editor"
#define SELECTED_ENTITIES_IDENTIFIER "__SelectedEntities"
#define SELECT_COLOR_IDENTIFIER "__SelectColor"
#define TRANSFORM_TOOL_IDENTIFIER "__TransformTool"
#define TRANSFORM_TOOL_EX_IDENTIFIER "__TransformToolEx"
#define INSTANCED_FRAMEBUFFER_IDENTIFIER "__InstancedFramebuffer"

namespace ECSEngine {
	
	template<typename T>
	static bool GetRuntimeResource(const SystemManager* system_manager, T* resource, Stream<char> identifier) {
		void* runtime_resource = system_manager->TryGetData(identifier);
		if (runtime_resource != nullptr) {
			*resource = *(T*)runtime_resource;
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	// Camera needs to be the first field in this structure
	struct TableCamera {
		Camera camera;
		CameraCached camera_cached;
	};

	bool GetRuntimeCamera(const SystemManager* system_manager, Camera* camera, CameraCached** camera_cached)
	{
		bool success = GetRuntimeResource(system_manager, camera, CAMERA_IDENTIFIER);
		if (success && camera_cached != nullptr) {
			*camera_cached = (CameraCached*)function::OffsetPointer(camera, sizeof(*camera));
		}
		return success;
	}

	void SetRuntimeCamera(SystemManager* system_manager, const Camera* camera, bool set_cached_camera)
	{
		if (set_cached_camera) {
			TableCamera* table_camera = (TableCamera*)system_manager->BindDataNoCopy(CAMERA_IDENTIFIER, sizeof(TableCamera));
			table_camera->camera = *camera;
			table_camera->camera_cached = camera;
		}
		else {
			system_manager->BindData(CAMERA_IDENTIFIER, camera, sizeof(*camera));
		}
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

	ECSTransformToolEx GetEditorRuntimeTransformToolEx(const SystemManager* system_manager)
	{
		ECSTransformToolEx tool_ex;
		tool_ex.display_axes = false;
		tool_ex.tool = ECS_TRANSFORM_COUNT;
		tool_ex.space = ECS_TRANSFORM_SPACE_COUNT;
		GetRuntimeResource(system_manager, &tool_ex, TRANSFORM_TOOL_EX_IDENTIFIER);
		return tool_ex;
	}

	void SetEditorRuntimeTransformToolEx(SystemManager* system_manager, ECSTransformToolEx tool_ex)
	{
		system_manager->BindData(TRANSFORM_TOOL_EX_IDENTIFIER, &tool_ex, sizeof(tool_ex));
	}

	void RemoveEditorRuntimeTransformToolEx(SystemManager* system_manager)
	{
		system_manager->RemoveData(TRANSFORM_TOOL_EX_IDENTIFIER);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool GetEditorRuntimeInstancedFramebuffer(const SystemManager* system_manager, GraphicsBoundViews* views)
	{
		return GetRuntimeResource(system_manager, views, INSTANCED_FRAMEBUFFER_IDENTIFIER);
	}

	void SetEditorRuntimeInstancedFramebuffer(SystemManager* system_manager, const GraphicsBoundViews* views)
	{
		system_manager->BindData(INSTANCED_FRAMEBUFFER_IDENTIFIER, views, sizeof(*views));
	}

	void RemoveEditorRuntimeInstancedFramebuffer(SystemManager* system_manager)
	{
		system_manager->RemoveData(INSTANCED_FRAMEBUFFER_IDENTIFIER);
	}

	// ------------------------------------------------------------------------------------------------------------

}