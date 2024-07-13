#include "pch.h"
#include "Scripting.h"
#include "ECSEngineModule.h"
#include "ECSEngineInputControllers.h"
#include "ECSEngineWorld.h"
#include "ECSEngineComponents.h"
#include "Logging.h"

static ECS_THREAD_TASK(CameraController) {
	EntityManager* entity_manager = world->entity_manager;
	CameraComponent* camera = entity_manager->TryGetGlobalComponent<CameraComponent>();
	if (camera != nullptr) {
		if (world->mouse->IsVisible()) {
			world->mouse->SetCursorVisibility(false);
		}

		const Keyboard* keyboard = world->keyboard;
		FirstPersonWASDController(
			keyboard->IsDown(ECS_KEY_W),
			keyboard->IsDown(ECS_KEY_A),
			keyboard->IsDown(ECS_KEY_S),
			keyboard->IsDown(ECS_KEY_D),
			world->mouse->GetPositionDelta(),
			0.1f,
			0.1f,
			camera->value.translation,
			camera->value.rotation
		);
		camera->value.Recalculate();
	}
}

void AddScripts(ECSEngine::ModuleTaskFunctionData* data) {
	TaskSchedulerElement camera_controller;
	camera_controller.task_group = ECS_THREAD_TASK_INITIALIZE_EARLY;
	ECS_REGISTER_TASK(camera_controller, CameraController, data);
}