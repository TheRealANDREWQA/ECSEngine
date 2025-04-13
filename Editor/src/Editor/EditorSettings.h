#pragma once
#include "ECSEngineContainers.h"

struct EditorSettings {
	// This is the path to the executable for the compiler CMD
	ECSEngine::Stream<wchar_t> compiler_path;
	// This is the path to the executable/script for the editing IDE (at the moment only Visual Studio)
	ECSEngine::Stream<wchar_t> editing_ide_path;
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
	ECSEngine::Stream<wchar_t> compiler_path;
	// If the compiler comes with an editing environment (like it does with Visual Studio),
	// It will in the path to the editing IDE executable here. It can be empty if the compiler
	// Is standalone.
	ECSEngine::Stream<wchar_t> editing_ide_path;
};

struct EditorState;

// Returns true if the editor file was successfully saved, else false
bool ChangeCompilerVersion(EditorState* editor_state, ECSEngine::Stream<wchar_t> path);

// Returns true if the editor file was successfully saved, else false
bool ChangeEditingIdeExecutablePath(EditorState* editor_state, ECSEngine::Stream<wchar_t> path);

// Returns true if the editor file was successfully saved, else false. Combines the 2 actions
// Into a single save
bool ChangeCompilerVersionAndEditingIdeExecutablePath(EditorState* editor_state, ECSEngine::Stream<wchar_t> compiler_path, ECSEngine::Stream<wchar_t> editing_ide_executable_path);

void AutoDetectCompilers(ECSEngine::AllocatorPolymorphic allocator, ECSEngine::AdditionStream<CompilerVersion> compiler_versions);