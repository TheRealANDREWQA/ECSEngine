#pragma once
#include "Core.h"

namespace ECSEngine {

	struct Graphics;
	struct GlobalMemoryManager;

	ECSENGINE_API void CreateGraphicsForProcess(Graphics* graphics, HWND hWnd, GlobalMemoryManager* global_manager, bool discrete_gpu);

}