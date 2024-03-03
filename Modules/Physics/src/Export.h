#pragma once
#include "ECSEngineThreadTaskExport.h"

#ifdef PHYSICS_BUILD_DLL
#define	PHYSICS_API __declspec(dllexport)
#else
#define PHYSICS_API __declspec(dllimport)
#endif 

#define PHYSICS_THREAD_TASK_TEMPLATE_EXPORT(name) ECS_THREAD_TASK_TEMPLATE_BOOL_EXPORT(PHYSICS_API, name)