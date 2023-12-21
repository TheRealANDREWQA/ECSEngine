#pragma once
#include "ECSEngineModule.h"
#include "Export.h"

extern "C" {

	Physics_API void ModuleTaskFunction(ECSEngine::ModuleTaskFunctionData* data);

#if 0

	Physics_API void ModuleBuildFunctions(ECSEngine::ModuleBuildFunctionsData* data);

#endif

#if 0

	Physics_API void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data);

#endif

#if 0

	Physics_API void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data);

#endif

#if 0

	Physics_API void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data);

#endif

#if 0

	Physics_API void ModuleSetCurrentWorld(ECSEngine::World* world);

#endif

#if 0

	Physics_API void ModuleRegisterExtraInformationFunction(ECSEngine::ModuleRegisterExtraInformationFunctionData* data);

#endif

#if 0

	Physics_API void ModuleRegisterDebugDrawFunction(ECSEngine::ModuleRegisterDebugDrawFunctionData* data);

#endif

#if 0

	Physics_API void ModuleRegisterDebugDrawTaskElementsFunction(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data);

#endif

}