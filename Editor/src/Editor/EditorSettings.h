#pragma once
#include "ECSEngineContainers.h"

struct EditorSettings {
	ECSEngine::Stream<wchar_t> compiler_path;
};

bool AutoDetectCompiler(ECSEngine::AdditionStream<wchar_t> path);