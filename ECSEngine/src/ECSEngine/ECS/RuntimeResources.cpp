#include "ecspch.h"
#include "RuntimeResources.h"
#include "SystemManager.h"

#define CAMERA_IDENTIFIER "__RuntimeCamera"
#define EDITOR_IDENTIFIER "__Editor"

namespace ECSEngine {
	
	template<typename T>
	bool GetRuntimeResource(const SystemManager* system_manager, T& resource, Stream<char> identifier) {
		void* runtime_resource = system_manager->TryGetData(identifier);
		if (runtime_resource != nullptr) {
			resource = *(T*)runtime_resource;
			return true;
		}
		return false;
	}

#define IMPLEMENT_RESOURCE(Name, ResourceType, Identifier) \
	bool GetRuntime##Name(const SystemManager* system_manager, ResourceType& parameter) { \
		return GetRuntimeResource(system_manager, parameter, Identifier); \
	} \
\
	void SetRuntime##Name(SystemManager* system_manager, const ResourceType& parameter) { \
		system_manager->BindData(Identifier, &parameter, sizeof(parameter)); \
	} \
\
	void RemoveRuntime##Name(SystemManager* system_manager) { \
		system_manager->RemoveData(Identifier); \
	}

	IMPLEMENT_RESOURCE(Camera, Camera, CAMERA_IDENTIFIER);

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

}