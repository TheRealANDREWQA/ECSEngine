#pragma once
#include "ECSEngineModule.h"
#include "Export.h"

extern "C" {

	GRAPHICS_API void ModuleTaskFunction(ECSEngine::ModuleTaskFunctionData* data);

#if 0

	GRAPHICS_API void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data);

#endif

#if 0

	GRAPHICS_API void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data);

#endif

#if 0

	GRAPHICS_API void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data);

#endif

#if 0

	GRAPHICS_API void ModuleSetCurrentWorld(ECSEngine::World* world);

#endif

}