#pragma once
#include "../../Core.h"

#define ECS_TASK_MANAGER_WRAPPER
#define ECS_THREAD_TASK_NAME(function, data, data_size) ThreadTask(function, data, data_size, STRING(function))

namespace ECSEngine {

	struct World;

	typedef void (*ThreadFunction)(unsigned int thread_id, World* world, void* _data);
#define ECS_THREAD_TASK(name) void name(unsigned int thread_id, ECSEngine::World* world, void* _data)

	struct ECSENGINE_API ThreadTask
	{
		ThreadTask() : function(), data(nullptr), name(nullptr) {}
		ThreadTask(ThreadFunction _function, void* _data, size_t _data_size) : function(_function), data(_data), data_size(_data_size), name(nullptr) {}
		ThreadTask(ThreadFunction _function, void* _data, size_t _data_size, const char* _name) : function(_function), data(_data), data_size(_data_size), name(_name) {}

		ThreadTask(const ThreadTask& other) = default;
		ThreadTask& operator = (const ThreadTask& other) = default;

		ThreadFunction function;
		void* data;
		size_t data_size = 0;
		const char* name = nullptr;
	};

	typedef void (*ThreadFunctionWrapper)(unsigned int thread_id, World* world, ThreadTask task, void* wrapper_data);
#define ECS_THREAD_WRAPPER_TASK(name) void name(unsigned int thread_id, ECSEngine::World* world, ECSEngine::ThreadTask task, void* _wrapper_data)

}