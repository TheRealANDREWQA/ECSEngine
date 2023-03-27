#pragma once
#include "../Rendering/RenderingStructures.h"

namespace ECSEngine {

	/*
		These are a set of variables that the Runtime can contain.
		This is useful for injecting certain values into the Runtime
		Without having to create specialized components. Can be used in
		Editor contexts - for example setting certain values - like a camera
		transform.
	*/

	struct SystemManager;

	// Returns true if there is a runtime camera, else false
	ECSENGINE_API bool GetRuntimeCamera(const SystemManager* system_manager, Camera& camera);

	ECSENGINE_API void SetRuntimeCamera(SystemManager* system_manager, const Camera& camera);

	ECSENGINE_API void RemoveRuntimeCamera(SystemManager* system_manager);

	ECSENGINE_API bool IsEditorRuntime(const SystemManager* system_manager);

	ECSENGINE_API void SetEditorRuntime(SystemManager* system_manager);

	ECSENGINE_API void RemoveEditorRuntime(SystemManager* system_manager);

}