#pragma once
#include "../Core.h"
#include "../Utilities/Optional.h"

namespace ECSEngine {

	template<typename T>
	struct CapacityStream;

	namespace OS {

		enum ECS_HARDWARE_BREAKPOINT_TYPE : unsigned char {
			// The breakpoint is triggered when the address is being written
			ECS_HARDWARE_BREAKPOINT_WRITE,
			// The brekapoint is triggered when the address is read/written
			ECS_HARDWARE_BREAKPOINT_READ_WRITE,
			// The breakpoint is triggered when the address is attempted to be executed
			ECS_HARDWARE_BREAKPOINT_CODE
		};

		struct HardwareBreakpoint {
			unsigned char index;
		};

		// Attemps to set a hardware breakpoint at a specified address for a specified thread.
		// Returns an empty optional if it fails, else a breakpoint value that can be used to remove
		// This breakpoint or update later on. The address byte size must be 1, 2, 4 or 8 bytes.
		// Be aware that this function sets debug registers on the CPU that can conflict with the ones
		// Set by the debugger. The thread must be suspended before this operation can take place, otherwise
		// Undefined behavior can occur, with the last boolean you can have the convenience of this function taking
		// Care of the suspension.
		ECSENGINE_API Optional<HardwareBreakpoint> SetHardwareBreakpoint(
			void* thread_handle,
			ECS_HARDWARE_BREAKPOINT_TYPE breakpoint_type,
			void* address,
			size_t address_byte_size,
			bool suspend_thread,
			CapacityStream<char>* error_message = nullptr
		);

		// Removes a previously set breakpoint and returns true if it succeeded, else false.
		// As with the Set function, you can suspend the thread with this call as a convenience.
		ECSENGINE_API bool RemoveHardwareBreakpoint(void* thread_handle, HardwareBreakpoint breakpoint, bool suspend_thread, CapacityStream<char>* error_message);

	}
}