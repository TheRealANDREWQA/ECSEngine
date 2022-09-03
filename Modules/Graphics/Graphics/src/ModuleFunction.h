#pragma once
#include "ECSEngineModule.h"

#ifdef Graphics_BUILD_DLL
	#define	SYSTEM_API __declspec(dllexport)
#else
	#define SYSTEM_API __declspec(dllimport)
#endif 

extern "C" {

	SYSTEM_API void ModuleTaskFunction(ECSEngine::ModuleTaskFunctionData* data);

#if 0

	SYSTEM_API void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data);

#endif

#if 0

	SYSTEM_API void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data);

#endif

#if 0

	SYSTEM_API void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data);

#endif

#if 0

	SYSTEM_API void ModuleSetCurrentWorld(ECSEngine::World* world);

#endif

}