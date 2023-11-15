#pragma once
#include "ECSEngineUI.h"

#define NOTIFICATION_BAR_WINDOW_NAME "Notification Bar"
#define NOTIFICATION_BAR_WINDOW_SIZE 0.05f

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
void NotificationBarSetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void NotificationBarDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateNotificationBar(EditorState* editor_state);

// It creates the dockspace and the window
void CreateNotificationBarAction(ECSEngine::Tools::ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateNotificationBarWindow(EditorState* editor_state);