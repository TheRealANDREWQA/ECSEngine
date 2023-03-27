#pragma once
#include "ECSEngineThreadTaskExport.h"

#ifdef COLLISIONDETECTION_BUILD_DLL
#define	COLLISIONDETECTION_API __declspec(dllexport)
#else
#define COLLISIONDETECTION_API __declspec(dllimport)
#endif 

#define COLLISIONDETECTION_THREAD_TASK_EXPORT(name) ECS_THREAD_TASK_TEMPLATE_BOOL_EXPORT(COLLISIONDETECTION_API, name)