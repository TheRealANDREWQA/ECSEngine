#pragma once
#include "ECSEngineThreadTaskExport.h"

#ifdef GRAPHICS_BUILD_DLL
#define	GRAPHICS_API __declspec(dllexport)
#else
#define GRAPHICS_API __declspec(dllimport)
#endif 

#define GRAPHICS_THREAD_TASK_TEMPLATE_EXPORT(name) ECS_THREAD_TASK_TEMPLATE_BOOL_EXPORT(GRAPHICS_API, name)