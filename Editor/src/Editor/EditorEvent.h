#pragma once
#include "ECSEngineUI.h"

struct EditorState;

using EditorEventFunction = void (*)(EditorState* editor_state, void* ECS_RESTRICT data);

#define EDITOR_CONSOLE_SYSTEM_NAME "Editor"
#define EDITOR_CONSOLE_SYSTEM_INDEX 1 << 0

#define EDITOR_EXPORT extern "C" __declspec(dllexport)

// Cannot place these inside a Editor namespace because it will conflict with the Editor class in Editor.cpp

EDITOR_EXPORT void EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size = 0);

// It will take the pointer as is but will set the event data size different from 0
// in order to be deallocated; useful for example when the data is coallesced from the editor
// allocator and it must still be deallocated
EDITOR_EXPORT void EditorAddEventWithPointer(EditorState* editor_state, EditorEventFunction function, void* event_data);

EDITOR_EXPORT void EditorSkip(EditorState* editor_state, void* ECS_RESTRICT data);

EDITOR_EXPORT void EditorError(EditorState* editor_state, void* ECS_RESTRICT data);

EDITOR_EXPORT void EditorConsoleError(EditorState* editor_state, void* ECS_RESTRICT data);

EDITOR_EXPORT void EditorConsoleWarn(EditorState* editor_state, void* ECS_RESTRICT data);

EDITOR_EXPORT void EditorConsoleInfo(EditorState* editor_state, void* ECS_RESTRICT data);

EDITOR_EXPORT void EditorConsoleInfoFocus(EditorState* editor_state, void* ECS_RESTRICT data);

EDITOR_EXPORT void EditorConsoleTrace(EditorState* editor_state, void* ECS_RESTRICT data);

EDITOR_EXPORT void EditorSetError(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

EDITOR_EXPORT void EditorSetConsoleError(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

EDITOR_EXPORT void EditorSetConsoleWarn(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

EDITOR_EXPORT void EditorSetConsoleInfo(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

EDITOR_EXPORT void EditorSetConsoleInfoFocus(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

EDITOR_EXPORT void EditorSetConsoleTrace(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);