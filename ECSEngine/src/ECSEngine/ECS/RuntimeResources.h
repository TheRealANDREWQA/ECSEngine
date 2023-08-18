#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Rendering/ColorUtilities.h"
#include "../ECS/InternalStructures.h"
#include "../Utilities/TransformTool.h"

namespace ECSEngine {

	/*
		These are a set of variables that the Runtime can contain.
		This is useful for injecting certain values into the Runtime
		Without having to create specialized components. Can be used in
		Editor contexts - for example setting certain values - like a camera
		transform.
	*/

	struct SystemManager;
	struct Camera;
	struct CameraCached;
	struct GraphicsBoundViews;

	// The entity ids are used to output the values to the instanced framebuffer
	// And the booleans is_selected informs the module if the tool is selected or not
	struct ECSTransformToolEx {
		ECS_TRANSFORM_TOOL tool;
		bool is_selected[ECS_TRANSFORM_AXIS_COUNT];
		Entity entity_ids[ECS_TRANSFORM_AXIS_COUNT];
	};

	// ------------------------------------------------------------------------------------------------------------

	// Returns true if there is a runtime camera, else false
	// Can optionally retrieve the cached camera
	ECSENGINE_API bool GetRuntimeCamera(const SystemManager* system_manager, Camera* camera, CameraCached** camera_cached = nullptr);

	// Can optionally have the system also record a cached variant of the camera that it can hand back
	ECSENGINE_API void SetRuntimeCamera(SystemManager* system_manager, const Camera* camera, bool set_cached_camera = false);

	ECSENGINE_API void RemoveRuntimeCamera(SystemManager* system_manager);

	// ------------------------------------------------------------------------------------------------------------

	// Returns true if the module runs in the editor context
	ECSENGINE_API bool IsEditorRuntime(const SystemManager* system_manager);

	ECSENGINE_API void SetEditorRuntime(SystemManager* system_manager);

	ECSENGINE_API void RemoveEditorRuntime(SystemManager* system_manager);

	// ------------------------------------------------------------------------------------------------------------

	// Returns the selected entities in the editor context
	// Returns { nullptr, 0 } if there are none or there is nothing bound
	ECSENGINE_API Stream<Entity> GetEditorRuntimeSelectedEntities(const SystemManager* system_manager);

	ECSENGINE_API void SetEditorRuntimeSelectedEntities(SystemManager* system_manager, Stream<Entity> entities);

	ECSENGINE_API void RemoveEditorRuntimeSelectedEntities(SystemManager* system_manager);

	// ------------------------------------------------------------------------------------------------------------

	// Returns ECS_COLOR_BLACK if the color is not bound
	ECSENGINE_API Color GetEditorRuntimeSelectColor(const SystemManager* system_manager);

	ECSENGINE_API void SetEditorRuntimeSelectColor(SystemManager* system_manager, Color color);

	ECSENGINE_API void RemoveEditorRuntimeSelectColor(SystemManager* system_manager);

	// ------------------------------------------------------------------------------------------------------------

	// Returns ECS_TRANSFORM_COUNT if there is no tool specified
	ECSENGINE_API ECS_TRANSFORM_TOOL GetEditorRuntimeTransformTool(const SystemManager* system_manager);

	ECSENGINE_API void SetEditorRuntimeTransformTool(SystemManager* system_manager, ECS_TRANSFORM_TOOL tool);

	ECSENGINE_API void RemoveEditorRuntimeTransformTool(SystemManager* system_manager);

	// ------------------------------------------------------------------------------------------------------------

	// Returns ECS_TRANSFORM_COUNT if there is no tool specified
	ECSENGINE_API ECSTransformToolEx GetEditorRuntimeTransformToolEx(const SystemManager* system_manager);

	ECSENGINE_API void SetEditorRuntimeTransformToolEx(SystemManager* system_manager, ECSTransformToolEx tool_ex);

	ECSENGINE_API void RemoveEditorRuntimeTransformToolEx(SystemManager* system_manager);
	
	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool GetEditorRuntimeInstancedFramebuffer(const SystemManager* system_manager, GraphicsBoundViews* views);

	ECSENGINE_API void SetEditorRuntimeInstancedFramebuffer(SystemManager* system_manager, const GraphicsBoundViews* views);

	ECSENGINE_API void RemoveEditorRuntimeInstancedFramebuffer(SystemManager* system_manager);

	// ------------------------------------------------------------------------------------------------------------
}