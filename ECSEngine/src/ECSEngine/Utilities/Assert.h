#pragma once
#include "../Core.h"

namespace ECSEngine {

	namespace function {

		ECSENGINE_API void Assert(bool condition, const char* filename, unsigned int line, const char* error_message = nullptr);
#define ECS_ASSERT(condition, ...) function::Assert(condition, __FILE__, __LINE__, __VA_ARGS__);
	
	}

}