#pragma once
#include "../../Core.h"

#define ECS_TASK_MANAGER_WRAPPER
#define ECS_THREAD_TASK_NAME

namespace ECSEngine {

	struct World;

	using ThreadFunction = void (*)(unsigned int thread_id, World* world, void* data);
	using ThreadFunctionWrapper = void (*)(unsigned int thread_id, World* ECS_RESTRICT world, ThreadFunction function, void* ECS_RESTRICT data, void* ECS_RESTRICT wrapper_data);

	struct ECSENGINE_API ThreadTask
	{
		ThreadTask() : function(), data(nullptr)
#ifdef ECS_THREAD_TASK_NAME
			, name(nullptr)
#endif
		{}
		ThreadTask(ThreadFunction _function, void* _data) : function(_function), data(_data)
#ifdef ECS_THREAD_TASK_NAME
			, name(nullptr)
#endif		
		{}
#ifdef ECS_THREAD_TASK_NAME
		ThreadTask(ThreadFunction _function, void* _data, const char* _name) : function(_function), data(_data), name(_name) {}
#endif

		ThreadTask(const ThreadTask& other) = default;
		ThreadTask& operator = (const ThreadTask& other) = default;

		ThreadFunction function;
		void* data;
#ifdef ECS_THREAD_TASK_NAME
		const char* name;
#endif
	};

}