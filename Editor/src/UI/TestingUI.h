#pragma once
#include "ECSEngineUI.h"

struct EditorState;
ECS_TOOLS;

#define TESTING_UI_WINDOW_NAME "Testing"

void TestingUIWindowSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory);

void TestingUIDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// Creates the dockspace and the window if it doesn't exist, else it will set it as the active window
void CreateTestingUI(EditorState* editor_state);

void CreateTestingUIWindowAction(ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateTestingUIWindow(EditorState* editor_state);