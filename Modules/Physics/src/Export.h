#pragma once
#include "ECSEngineThreadTaskExport.h"

#ifdef Physics_BUILD_DLL
#define	Physics_API __declspec(dllexport)
#else
#define Physics_API __declspec(dllimport)
#endif 

#define Physics_THREAD_TASK_TEMPLATE_EXPORT(name) ECS_THREAD_TASK_TEMPLATE_BOOL_EXPORT(Physics_API, name)