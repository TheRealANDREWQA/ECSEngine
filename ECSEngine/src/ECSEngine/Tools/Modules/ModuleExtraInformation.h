#pragma once
#include "../../Core.h"
#include "ModuleDefinition.h"

namespace ECSEngine {

	ECSENGINE_API void SetGraphicsModuleRenderMeshBounds(ModuleRegisterExtraInformationFunctionData* register_data, Stream<char> component, Stream<char> field);

	struct GraphicsModuleRenderMeshBounds {
		ECS_INLINE bool IsValid() const {
			return component.size > 0 && field.size > 0;
		}

		Stream<char> component;
		Stream<char> field;
	};

	ECSENGINE_API GraphicsModuleRenderMeshBounds GetGraphicsModuleRenderMeshBounds(ModuleExtraInformation extra_information);

}