#pragma once
#include "editorpch.h"
#include "ECSEngineUI.h"

struct EditorState;

typedef void (*EditorEventFunction)(EditorState* editor_state, void* data);

#define EDITOR_EVENT(name) void name(EditorState* editor_state, void* _data)

#define EDITOR_CONSOLE_SYSTEM_NAME "Editor"
#define EDITOR_CONSOLE_SYSTEM_INDEX 1 << 0

#define EDITOR_EXPORT extern "C" __declspec(dllexport)

// Cannot place these inside a Editor namespace because it will conflict with the Editor class in Editor.cpp

EDITOR_EXPORT void EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size = 0);

// It will take the pointer as is but will set the event data size different from 0
// in order to be deallocated; useful for example when the data is coallesced from the editor
// allocator and it must still be deallocated
EDITOR_EXPORT void EditorAddEventWithPointer(EditorState* editor_state, EditorEventFunction function, void* event_data);

EDITOR_EXPORT EDITOR_EVENT(EditorSkip);

EDITOR_EXPORT EDITOR_EVENT(EditorError);

EDITOR_EXPORT EDITOR_EVENT(EditorConsoleError);

EDITOR_EXPORT EDITOR_EVENT(EditorConsoleWarn);

EDITOR_EXPORT EDITOR_EVENT(EditorConsoleInfo);

EDITOR_EXPORT EDITOR_EVENT(EditorConsoleTrace);

// Thread safe
EDITOR_EXPORT void EditorSetError(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

// Thread safe
EDITOR_EXPORT void EditorSetConsoleError(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

// Thread safe
EDITOR_EXPORT void EditorSetConsoleWarn(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

// Thread safe
EDITOR_EXPORT void EditorSetConsoleInfo(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);

// Thread safe
EDITOR_EXPORT void EditorSetConsoleTrace(EditorState* editor_state, ECSEngine::containers::Stream<char> error_message);