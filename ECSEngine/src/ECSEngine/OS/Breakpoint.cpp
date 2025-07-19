#include "ecspch.h"
#include "Breakpoint.h"
#include "Thread.h"
#include "../Utilities/StackScope.h"
#include "../Utilities/StringUtilities.h"

namespace ECSEngine {

#define DEBUG_REGISTER_COUNT 4

	/*
	* To tell what registers are enabled, each slot, Dr0, Dr1, Dr2, Dr3 has 2 bits, with the first
	* Bit indicating a local breakpoint (thread breakpoint), while the second bit specifies a global breakpoint.
	* So bits 0-7 should be checked. Bits 16-23 indicate the breakpoint execution type, 2 bits again per register,
	* With 00b for code execution, 01b for write detection, 10b reserved, and 11b for read/write. At last, bits
	* 24-31 specify the size of the address, 00b means 1 byte, 01b 2 bytes, 10b 8 bytes, 11b 4 bytes.
	* A note on "DebugControl" from the CONTEXT structure. Classical Microsoft confusion, without any source indicating clearly what it does, 
	* This field does nothing, so use Dr7.
	*/

	namespace OS {

		Optional<HardwareBreakpoint> SetHardwareBreakpoint(
			void* thread_handle, 
			ECS_HARDWARE_BREAKPOINT_TYPE breakpoint_type, 
			void* address, 
			size_t address_byte_size, 
			bool suspend_thread,
			CapacityStream<char>* error_message
		) {
			Optional<HardwareBreakpoint> breakpoint;

			// Suspend the thread, if specified
			if (suspend_thread) {
				if (!SuspendThread(thread_handle)) {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to suspend the thread");
					return breakpoint;
				}
			}

			// Create a stack scope to resume the thread if we suspended it
			auto resume_thread = StackScope([&]() {
				if (suspend_thread) {
					ECS_ASSERT(ResumeThread(thread_handle), "Setting hardware breakpoint failed - could not resume the stopped thread");
				}
			});
		
			CONTEXT thread_context = { 0 };
			thread_context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			if (!GetThreadContext(thread_handle, &thread_context)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to retrieve the thread's debug context");
				return breakpoint;
			}

			// Check to see if all slots are occupied
			size_t debug_register_bits = thread_context.Dr7;
			for (size_t index = 0; index < DEBUG_REGISTER_COUNT; index++) {
				if (!HasFlag(debug_register_bits, (size_t)0b01 << (index * 2))) {
					breakpoint.value.index = (unsigned char)index;
					breakpoint.has_value = true;
					break;
				}
			}

			if (!breakpoint.has_value) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "All debug registers are in use");
				return breakpoint;
			}

			// Found a register which is not in use. Use it now.
			size_t debug_register_execution_bits = 0;
			switch (breakpoint_type) {
			case ECS_HARDWARE_BREAKPOINT_CODE:
				debug_register_execution_bits = 0b00;
				break;
			case ECS_HARDWARE_BREAKPOINT_WRITE:
				debug_register_execution_bits = 0b01;
				break;
			case ECS_HARDWARE_BREAKPOINT_READ_WRITE:
				debug_register_execution_bits = 0b11;
				break;
			default:
				ECS_ASSERT(false, "Invalid hardware breakpoint type");
			}

			size_t debug_register_address_size_bits = 0;
			switch (address_byte_size) {
			case 1:
				debug_register_address_size_bits = 0b00;
				break;
			case 2:
				debug_register_address_size_bits = 0b01;
				break;
			case 4:
				debug_register_address_size_bits = 0b11;
				break;
			case 8:
				debug_register_address_size_bits = 0b10;
				break;
			default:
				ECS_ASSERT(false, "Invalid hardware address byte size");
			}
			
			size_t bitshift_count = breakpoint.value.index * 2;
			debug_register_bits = SetFlag(debug_register_bits, (size_t)0b01 << bitshift_count);

			// For the execution and address size bits, clear them before setting them to ensure that
			// No leftover bits remain set
			debug_register_bits = ClearFlag(debug_register_bits, (size_t)0b11 << (16 + bitshift_count));
			debug_register_bits = ClearFlag(debug_register_bits, (size_t)0b11 << (24 + bitshift_count));

			debug_register_bits = SetFlag(debug_register_bits, debug_register_execution_bits << (16 + bitshift_count));
			debug_register_bits = SetFlag(debug_register_bits, debug_register_address_size_bits << (24 + bitshift_count));

			// Set the thread context now
			switch (breakpoint.value.index) {
			case 0:
				thread_context.Dr0 = (size_t)address;
				break;
			case 1:
				thread_context.Dr1 = (size_t)address;
				break;
			case 2:
				thread_context.Dr2 = (size_t)address;
				break;
			case 3:
				thread_context.Dr3 = (size_t)address;
				break;
			default:
				ECS_ASSERT(false, "Unhandled set hardware breakpoint code path");
			}
			thread_context.Dr7 = debug_register_bits;
			if (!SetThreadContext(thread_handle, &thread_context)) {
				// Reset the optional flag, to indicate the failure
				breakpoint.has_value = false;
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to set thread context when setting a hardware breakpoint");
				return breakpoint;
			}

			return breakpoint;
		}

		bool RemoveHardwareBreakpoint(void* thread_handle, HardwareBreakpoint breakpoint, bool suspend_thread, CapacityStream<char>* error_message) {
			// If the breakpoint value is out of bounds, fail
			if (breakpoint.index >= DEBUG_REGISTER_COUNT) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Invalid specified breakpoint");
				return false;
			}

			// Start by suspending the thread, if needed
			if (suspend_thread) {
				if (!SuspendThread(thread_handle)) {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to suspend the thread");
					return false;
				}
			}

			auto resume_thread = StackScope([&]() {
				if (suspend_thread) {
					ECS_ASSERT(ResumeThread(thread_handle), "Removing a hardware breakpoint failed - could not resume the stopped thread");
				}
			});

			CONTEXT thread_context = { 0 };
			thread_context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			if (!GetThreadContext(thread_handle, &thread_context)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to retrieve the thread's debug context");
				return false;
			}

			// Ensure that the breakpoint does exist
			size_t debug_register_bits = thread_context.DebugControl;

			size_t bitshift_count = breakpoint.index * 2;
			if (!HasFlag(debug_register_bits, (size_t)0b01 << bitshift_count)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "The thread does not have the specified breakpoint");
				return false;
			}

			// Reset the address to nullptr
			switch (breakpoint.index) {
			case 0:
				thread_context.Dr0 = 0;
				break;
			case 1:
				thread_context.Dr1 = 0;
				break;
			case 2:
				thread_context.Dr2 = 0;
				break;
			case 3:
				thread_context.Dr3 = 0;
				break;
			default:
				ECS_ASSERT(false, "Remove hardware breakpoint invalid code path");
			}
			
			// Reset the enable, execution and address size bits
			debug_register_bits = ClearFlag(debug_register_bits, (size_t)0b01 << bitshift_count);
			debug_register_bits = ClearFlag(debug_register_bits, (size_t)0b11 << (16 + bitshift_count));
			debug_register_bits = ClearFlag(debug_register_bits, (size_t)0b11 << (24 + bitshift_count));
			thread_context.DebugControl = debug_register_bits;

			if (!SetThreadContext(thread_handle, &thread_context)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to set thread context when removing a hardware breakpoint");
				return false;
			}

			return true;
		}

	}

}