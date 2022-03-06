#pragma once
#include "../../Core.h"

#define ECS_TASK_MANAGER_WRAPPER
#define ECS_THREAD_TASK_NAME(function, data, data_size) ThreadTask(function, data, data_size, STRING(function))

namespace ECSEngine {

	struct World;

	enum ECS_THREAD_TASK_GROUP {
		ECS_THREAD_TASK_INITIALIZE_EARLY,
		ECS_THREAD_TASK_INITIALIZE_LATE,
		ECS_THREAD_TASK_SIMULATE_EARLY,
		ECS_THREAD_TASK_SIMULATE_LATE,
		ECS_THREAD_TASK_FINALIZE_EARLY,
		ECS_THREAD_TASK_FINALIZE_LATE,
		ECS_THREAD_TASK_GROUP_COUNT
	};

	using ThreadFunction = void (*)(unsigned int thread_id, World* world, void* _data);
	using ThreadFunctionWrapper = void (*)(unsigned int thread_id, World* ECS_RESTRICT world, ThreadFunction function, void* ECS_RESTRICT data, void* ECS_RESTRICT wrapper_data);

#define ECS_THREAD_TASK(name) void name(unsigned int thread_id, World* world, void* _data)
#define ECS_THREAD_WRAPPER_TASK(name) void name(unsigned int thread_id, World* ECS_RESTRICT world, ThreadFunction function, void* ECS_RESTRICT _data, void* ECS_RESTRICT _wrapper_data)

	struct ECSENGINE_API ThreadTask
	{
		ThreadTask() : function(), data(nullptr), name(nullptr)
		{}
		ThreadTask(ThreadFunction _function, void* _data, size_t _data_size) : function(_function), data(_data), data_size(_data_size), name(nullptr)	
		{}
		ThreadTask(ThreadFunction _function, void* _data, size_t _data_size, const char* _name) : function(_function), data(_data), data_size(_data_size), name(_name) {}

		ThreadTask(const ThreadTask& other) = default;
		ThreadTask& operator = (const ThreadTask& other) = default;

		ThreadFunction function;
		void* data;
		size_t data_size = 0;
		const char* name;
	};

}