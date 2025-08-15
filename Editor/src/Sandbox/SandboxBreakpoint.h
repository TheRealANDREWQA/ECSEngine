#pragma once
#include "ECSEngineContainersCommon.h"

struct EditorState;

enum HARDWARE_BREAKPOINT_SET_STATUS : unsigned char {
	HARDWARE_BREAKPOINT_OK,
	HARDWARE_BREAKPOINT_FAILURE,
	HARDWARE_BREAKPOINT_ALL_REGISTERS_ARE_USED
};

namespace ECSEngine {
	namespace OS {
		struct HardwareBreakpointHandler;
		enum ECS_REMOVE_HARDWARE_BREAKPOINT_STATUS : unsigned char;
	}
}

// Attemps to set a hardware debug register (write checks only, not read-write) for the specified sandbox for a specified address, which is
// Set only for the thread pool threads, it does not affect other threads (like editor background threads or the editor main thread).
// Providing the name is mandatory, such that it is easier to know which breakpoint was hit when that does happen. A custom handler
// Can also be specified to be executed. Returns one of the status values, with a special status for all registers being used, to
// Help in instructing the user about this error, which is different from a runtime one.
HARDWARE_BREAKPOINT_SET_STATUS SetSandboxHardwareBreakpoint(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	void* address, 
	size_t address_byte_size, 
	ECSEngine::Stream<char> name, 
	ECSEngine::OS::HardwareBreakpointHandler* breakpoint_handler = nullptr
);

// The same as the other similarly named function, except that it provides a default
// Implementation for the breakpoint_handler, the one that triggers when the value changes
HARDWARE_BREAKPOINT_SET_STATUS SetSandboxHardwareBreakpointValueChanged(
	EditorState* editor_state,
	unsigned int sandbox_handle,
	void* address,
	size_t address_byte_size,
	ECSEngine::Stream<char> name
);

// Returns true if the provided address has a hardware breakpoint in the provided sandbox, else false
bool IsSandboxHardwareBreakpointOnAddress(const EditorState* editor_state, unsigned int sandbox_handle, void* address);

// Removes a hardware breakpoint for the provided sandbox for the provided address. Returns the status of the action.
ECSEngine::OS::ECS_REMOVE_HARDWARE_BREAKPOINT_STATUS RemoveSandboxHardwareBreakpoint(EditorState* editor_state, unsigned int sandbox_handle, void* address);