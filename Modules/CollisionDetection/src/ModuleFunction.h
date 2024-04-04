#pragma once
#include "ECSEngineModule.h"
#include "Export.h"

extern "C" {

	COLLISIONDETECTION_API void ModuleTaskFunction(ECSEngine::ModuleTaskFunctionData* data);

#if 0

	COLLISIONDETECTION_API void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data);

#endif

#if 0

	COLLISIONDETECTION_API void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data);

#endif

#if 0

	COLLISIONDETECTION_API void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data);

#endif

#if 0

	COLLISIONDETECTION_API void ModuleRegisterExtraInformationFunction(ECSEngine::ModuleRegisterExtraInformationFunctionData* data);

#endif

#if 0

	COLLISIONDETECTION_API void ModuleRegisterDebugDrawFunction(ECSEngine::ModuleRegisterDebugDrawFunctionData* data);

#endif

#if 1

	COLLISIONDETECTION_API void ModuleRegisterDebugDrawTaskElementsFunction(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data);

#endif

#if 1

	COLLISIONDETECTION_API void ModuleRegisterComponentFunctionsFunction(ECSEngine::ModuleRegisterComponentFunctionsData* data);

#endif

}