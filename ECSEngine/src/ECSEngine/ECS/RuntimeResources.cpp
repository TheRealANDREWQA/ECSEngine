#include "ecspch.h"
#include "RuntimeResources.h"
#include "World.h"
#include "SystemManager.h"
#include "../Rendering/RenderingStructures.h"
#include "../Rendering/Camera.h"
#include "../ECS/InternalStructures.h"
#include "Components.h"
#include "../Tools/Modules/ModuleExtraInformation.h"
#include "../Utilities/StreamUtilities.h"

#define CAMERA_IDENTIFIER "__RuntimeCamera"
#define EDITOR_RUNTIME_TYPE_IDENTIFIER "__EditorRuntimeType"
#define SELECTED_ENTITIES_IDENTIFIER "__SelectedEntities"
#define SELECT_COLOR_IDENTIFIER "__SelectColor"
#define TRANSFORM_TOOL_IDENTIFIER "__TransformTool"
#define TRANSFORM_TOOL_EX_IDENTIFIER "__TransformToolEx"
#define INSTANCED_FRAMEBUFFER_IDENTIFIER "__InstancedFramebuffer"
#define TRANSFORM_GIZMOS "__TransformGizmos"

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
		void* runtime_resource = system_manager->TryGetData(CAMERA_IDENTIFIER);
		if (runtime_resource) {
			TableCamera* table_camera = (TableCamera*)runtime_resource;
			*camera = table_camera->camera;
			if (camera_cached != nullptr) {
				*camera_cached = &table_camera->camera_cached;
			}
			return true;
		}
		return false;
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

	ECS_EDITOR_RUNTIME_TYPE GetEditorRuntimeType(const SystemManager* system_manager)
	{
		ECS_EDITOR_RUNTIME_TYPE type;
		if (GetRuntimeResource(system_manager, &type, EDITOR_RUNTIME_TYPE_IDENTIFIER)) {
			return type;
		}
		else {
			return ECS_EDITOR_RUNTIME_TYPE_COUNT;
		}
	}

	void SetEditorRuntimeType(SystemManager* system_manager, ECS_EDITOR_RUNTIME_TYPE runtime_type)
	{
		system_manager->BindData(EDITOR_RUNTIME_TYPE_IDENTIFIER, &runtime_type, sizeof(runtime_type));
	}

	void RemoveEditorRuntimeType(SystemManager* system_manager)
	{
		system_manager->RemoveData(EDITOR_RUNTIME_TYPE_IDENTIFIER);
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
		void* allocated_data = CoalesceStreamWithData(system_manager->Allocator(), entities, entities.MemoryOf(1));
		if (allocated_data == nullptr) {
			// Allocate with Malloc
			allocated_data = CoalesceStreamWithData({ nullptr }, entities, entities.MemoryOf(1));
		}
		system_manager->BindData(SELECTED_ENTITIES_IDENTIFIER, allocated_data);
	}

	void RemoveEditorRuntimeSelectedEntities(SystemManager* system_manager) {
		void* data = system_manager->GetData(SELECTED_ENTITIES_IDENTIFIER);
		// If it doesn't belong to the allocator, it was allocated with Malloc
		if (!BelongsToAllocator(system_manager->Allocator(), data)) {
			Free(data);
		}
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

	bool GetWorldCamera(const World* world, CameraCached& camera)
	{	
		Camera normal_camera;
		CameraCached* camera_cached;
		if (GetRuntimeCamera(world->system_manager, &normal_camera, &camera_cached)) {
			camera = *camera_cached;
			return true;
		}
		
		const EntityManager* entity_manager = world->entity_manager;
		const CameraComponent* camera_component = entity_manager->TryGetGlobalComponent<CameraComponent>();

		if (camera_component) {
			camera = camera_component->value;
			return true;
		}

		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool GetEditorExtraTransformGizmos(const SystemManager* system_manager, Stream<TransformGizmo>* extra_gizmos)
	{
		return GetRuntimeResource(system_manager, extra_gizmos, TRANSFORM_GIZMOS);
	}

	void SetEditorExtraTransformGizmos(SystemManager* system_manager, Stream<TransformGizmo> extra_gizmos)
	{
		void* allocated_data = CoalesceStreamWithData(system_manager->Allocator(), extra_gizmos, sizeof(extra_gizmos[0]));
		system_manager->BindData(TRANSFORM_GIZMOS, allocated_data, 0);
	}

	void RemoveEditorExtraTransformGizmos(SystemManager* system_manager)
	{
		system_manager->RemoveData(TRANSFORM_GIZMOS);
	}

	// ------------------------------------------------------------------------------------------------------------

}