#pragma once
#include "ECSEngineModule.h"

#ifdef CollisionDetection_BUILD_DLL
	#define	CollisionDetection_API __declspec(dllexport)
#else
	#define CollisionDetection_API __declspec(dllimport)
#endif 

extern "C" {

	CollisionDetection_API void ModuleTaskFunction(ECSEngine::ModuleTaskFunctionData* data);

#if 0

	CollisionDetection_API void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data);

#endif

#if 0

	CollisionDetection_API void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data);

#endif

#if 0

	CollisionDetection_API void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data);

#endif

#if 0

	CollisionDetection_API void ModuleSetCurrentWorld(ECSEngine::World* world);

#endif

}