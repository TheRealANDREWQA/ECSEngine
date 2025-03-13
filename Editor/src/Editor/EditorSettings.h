#pragma once
#include "ECSEngineContainers.h"

struct EditorSettings {
	ECSEngine::Stream<wchar_t> compiler_path;
};

struct CompilerVersion {
	ECSEngine::Stream<char> aggregated;
	// This will reference characters inside the aggregated
	ECSEngine::Stream<char> year;
	// Type is related to Community, Professional and Enterprise (Visual Studio)
	// This will reference characters inside the aggregated
	ECSEngine::Stream<char> type;
	// It will reference characters inside the aggregated
	ECSEngine::Stream<char> compiler_name;
	ECSEngine::Stream<wchar_t> path;
};

struct EditorState;

// Returns true if the editor file was successfully saved
bool ChangeCompilerVersion(EditorState* editor_state, ECSEngine::Stream<wchar_t> path);

void AutoDetectCompilers(ECSEngine::AllocatorPolymorphic allocator, ECSEngine::AdditionStream<CompilerVersion> compiler_versions);