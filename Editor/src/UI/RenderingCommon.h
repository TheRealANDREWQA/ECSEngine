#pragma once
#include "ECSEngineUI.h"

ECS_TOOLS;
enum EDITOR_SANDBOX_VIEWPORT : unsigned char;
struct EditorState;

// Displays the texture on the entire content of the window. If the sandbox_index is -1 it will deduce it
// from the drawer
void DisplaySandboxTexture(
	EditorState* editor_state,
	UIDrawer& drawer,
	EDITOR_SANDBOX_VIEWPORT viewport,
	unsigned int sandbox_index = -1
);

// It verifies inside it if the module is out of date
void DisplayGraphicsModuleRecompilationWarning(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int in_sandbox_module_index,
	UIDrawer& drawer
);

void DisplayNoGraphicsModule(UIDrawer& drawer, bool multiple_graphics_modules);

void DisplayCrashedSandbox(UIDrawer& drawer, const EditorState* editor_state, unsigned int sandbox_index);

void DisplayCompilingSandbox(UIDrawer& drawer, const EditorState* editor_state, unsigned int sandbox_index);

void DisplayReplayActiveSandbox(UIDrawer& drawer, EditorState* editor_state, unsigned int sandbox_index);

// These are the basic ones like CPU usage, RAM usage, GPU usage, etc
void DisplaySandboxStatistics(UIDrawer& drawer, EditorState* editor_state, unsigned int sandbox_index);

// Performs the resizing and the re-render of the viewport if the difference between the sizes is at least
// equal to the threshold (if left at the default of 1 it means on every pixel size change). It will also update
// the previous size accordingly. If the sandbox index is -1 it will deduce it from the drawer
void ResizeSandboxTextures(
	EditorState* editor_state, 
	const UIDrawer& drawer, 
	EDITOR_SANDBOX_VIEWPORT viewport,
	ECSEngine::uint2* previous_size,
	unsigned int threshold = 1,
	unsigned int sandbox_index = -1
);

// This function return a UI custom element that can be used to override certain elements to add/remove breakpoints.
// With the identifier_types parameter you specify which elements should accept this element. The entity is used only
// For logging purposes, it isn't needed functionally.
UIConfigCustomElementDraw GetBreakpointCustomElementDraw(
	const EditorState* editor_state, 
	unsigned int sandbox_index,
	Entity entity,
	Stream<ECS_UI_ELEMENT_IDENTIFIER_TYPE> identifier_types, 
	AllocatorPolymorphic temporary_allocator
);

void SetBreakpointCustomElementInfo();