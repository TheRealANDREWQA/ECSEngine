#include "editorpch.h"
#include "SandboxBreakpoint.h"
#include "SandboxAccessor.h"

HARDWARE_BREAKPOINT_SET_STATUS SetSandboxHardwareBreakpoint(
	EditorState* editor_state, 
	unsigned int sandbox_handle, 
	void* address, 
	size_t address_byte_size, 
	Stream<char> name, 
	OS::HardwareBreakpointHandler* breakpoint_handler
) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	// If the sandbox itself has as many entries as possible register values, early exit
	if (sandbox->hardware_breakpoints.size == ECS_HARDWARE_BREAKPOINT_REGISTER_COUNT) {
		return HARDWARE_BREAKPOINT_ALL_REGISTERS_ARE_USED;
	}

	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	bool are_all_registers_in_use = false;
	OS::HardwareBreakpointOptions options;
	options.address_byte_size = address_byte_size;
	options.are_all_registers_in_use = &are_all_registers_in_use;
	options.error_message = &error_message;
	options.handler = breakpoint_handler;
	options.name = name;
	// We always need to suspend the thread
	options.suspend_thread = true;
	options.type = OS::ECS_HARDWARE_BREAKPOINT_WRITE;

	unsigned int thread_count = sandbox->sandbox_world.task_manager->GetThreadCount();
	for (unsigned int index = 0; index < thread_count; index++) {
		Optional<OS::HardwareBreakpoint> breakpoint = OS::SetHardwareBreakpoint(sandbox->sandbox_world.task_manager->m_thread_handles[index], address, options);
		if (!breakpoint.has_value) {
			// Revert the set on the previous threads
			for (unsigned int subindex = 0; subindex < index; subindex++) {
				ECS_STACK_CAPACITY_STREAM(char, remove_error_message, 512);
				if (!OS::RemoveHardwareBreakpointByAddress(sandbox->sandbox_world.task_manager->m_thread_handles[subindex], address, true, &remove_error_message)) {
					// Emit an error, there is not much we can do in this case
					ECS_FORMAT_TEMP_STRING(complete_error_message, "Failed to rollback hardware breakpoint for thread {#} for sandbox {#} when one upstream set failed. Reason: {#}", subindex, sandbox_handle, remove_error_message);
					EditorSetConsoleError(complete_error_message);
				}
			}

			// If all the registers are in use, don't print an error message, let the caller decide what to do
			if (are_all_registers_in_use) {
				return HARDWARE_BREAKPOINT_ALL_REGISTERS_ARE_USED;
			}
			
			// For failure, print an error message
			ECS_FORMAT_TEMP_STRING(complete_error_message, "Failed to set hardware breakpoint for sandbox {#}. Reason: {#}", sandbox_handle, error_message);
			EditorSetConsoleError(complete_error_message);
			return HARDWARE_BREAKPOINT_FAILURE;
		}
	}

	// If the loop finished, then it means all threads have the value set appropriately
	// Add the address to the sandbox register entry
	sandbox->hardware_breakpoints.AddAssert(address);
	return HARDWARE_BREAKPOINT_OK;
}

HARDWARE_BREAKPOINT_SET_STATUS SetSandboxHardwareBreakpointValueChanged(
	EditorState* editor_state,
	unsigned int sandbox_handle,
	void* address,
	size_t address_byte_size,
	Stream<char> name
) {
	OS::HardwareBreakpointChangedValueHandler changed_value_handler(address, address_byte_size);
	return SetSandboxHardwareBreakpoint(editor_state, sandbox_handle, address, address_byte_size, name, &changed_value_handler);
}

bool IsSandboxHardwareBreakpointOnAddress(const EditorState* editor_state, unsigned int sandbox_handle, void* address)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	return sandbox->hardware_breakpoints.Find(address) != -1;
}

OS::ECS_REMOVE_HARDWARE_BREAKPOINT_STATUS RemoveSandboxHardwareBreakpoint(EditorState* editor_state, unsigned int sandbox_handle, void* address) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	// If the address doesn't exist in the register array, fail
	unsigned int sandbox_array_index = sandbox->hardware_breakpoints.Find(address);
	if (sandbox_array_index == -1) {
		ECS_FORMAT_TEMP_STRING(message, "Failed to remove hardware breakpoint on address {#} for sandbox {#}. It was not set previously", address, sandbox_handle);
		EditorSetConsoleError(message);
		return OS::ECS_REMOVE_HARDWARE_BREAKPOINT_FAILURE;
	}
	
	// We can remove the address from the recorded array right after, a failure to remove the breakpoint
	// For one of the threads doesn't abort the entire operation
	sandbox->hardware_breakpoints.RemoveSwapBack(sandbox_array_index);

	// With this boolean we will report whether or not the breakpoint does not exist for all threads
	bool are_all_missing = true;

	unsigned int thread_count = sandbox->sandbox_world.task_manager->GetThreadCount();
	ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
	for (unsigned int index = 0; index < thread_count; index++) {
		error_message.size = 0;

		OS::ECS_REMOVE_HARDWARE_BREAKPOINT_STATUS remove_status = OS::RemoveHardwareBreakpointByAddress(sandbox->sandbox_world.task_manager->m_thread_handles[index], address, true, &error_message);
		// Print an error message only for the failure case, the case where the breakpoint doesn't exist treat it 
		// As maybe the debugger or some other software changed the thread context
		if (remove_status == OS::ECS_REMOVE_HARDWARE_BREAKPOINT_FAILURE) {
			ECS_FORMAT_TEMP_STRING(complete_error_message, "Failed to remove hardware breakpoint for thread {#} for sandbox {#}. Reason: {#}", index, sandbox_handle, error_message);
			EditorSetConsoleError(complete_error_message);
			are_all_missing = false;
		}
		else if (remove_status != OS::ECS_REMOVE_HARDWARE_BREAKPOINT_NOT_SET) {
			are_all_missing = false;
		}
	}

	return are_all_missing ? OS::ECS_REMOVE_HARDWARE_BREAKPOINT_NOT_SET : OS::ECS_REMOVE_HARDWARE_BREAKPOINT_OK;
}
