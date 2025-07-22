#pragma once
#include "../Core.h"
#include "../Utilities/Optional.h"
#include "../Containers/Stream.h"
#include "ExceptionHandling.h"

#define ECS_HARDWARE_BREAKPOINT_REGISTER_COUNT 4

namespace ECSEngine {

	struct TaskManager;

	namespace OS {

		enum ECS_HARDWARE_BREAKPOINT_TYPE : unsigned char {
			// The breakpoint is triggered when the address is being written
			ECS_HARDWARE_BREAKPOINT_WRITE,
			// The brekapoint is triggered when the address is read/written
			ECS_HARDWARE_BREAKPOINT_READ_WRITE,
			// The breakpoint is triggered when the address is attempted to be executed
			ECS_HARDWARE_BREAKPOINT_CODE
		};

		enum ECS_REMOVE_HARDWARE_BREAKPOINT_STATUS : unsigned char {
			ECS_REMOVE_HARDWARE_BREAKPOINT_OK,
			ECS_REMOVE_HARDWARE_BREAKPOINT_FAILURE,
			ECS_REMOVE_HARDWARE_BREAKPOINT_NOT_SET
		};

		struct HardwareBreakpoint {
			unsigned char index;
		};

		struct HardwareBreakpointHandlerData {
			const ExceptionInformation* exception_information;
			void* thread_handle;
			void* address;
			HardwareBreakpoint breakpoint;
			size_t address_byte_size;
			ECS_HARDWARE_BREAKPOINT_TYPE breakpoint_type;
		};

		// An interface that is called to handle a hardware breakpoint stop.
		struct HardwareBreakpointHandler : public Copyable {
			ECS_INLINE HardwareBreakpointHandler(size_t byte_size) : Copyable(byte_size) {}

			// This function should behave like an exception handler, because this is the place where
			// It will be handled. Return IGNORE if this exception is handled by this handler but it shouldn't
			// Trigger anything, RESOLVED if the exception should trigger the __debugbreak and stop the search,
			// While UNHANDLED if the handler search should continue
			virtual ECS_OS_EXCEPTION_CONTINUE_STATUS Handle(const HardwareBreakpointHandlerData& data) = 0;
		};

		struct HardwareBreakpointOptions {
			ECS_HARDWARE_BREAKPOINT_TYPE type;
			// The address byte size must be 1, 2, 4 or 8 bytes.
			size_t address_byte_size;
			// This is a convenience flag to instruct the function to suspend/resume the thread right now
			bool suspend_thread;

			// ------------------- Optional --------------------------
			// If specified, it will be set to true if the failure reason is that all debug registers are currently in use
			bool* are_all_registers_in_use = nullptr;
			CapacityStream<char>* error_message = nullptr;
			// An optional name that can be attached to the breakpoint such that the user knows
			// Which breakpoint was triggered in the code when the software breakpoint is generated
			Stream<char> name = {};
			// An exception handler that is associated with this breakpoint. This allows
			// For complex operations to be performed when an exception is encountered. Useful
			// For conditional breakpoints, where a certain condition must be met.
			HardwareBreakpointHandler* handler = nullptr;
		};

		// Attemps to set a hardware breakpoint at a specified address for a specified thread.
		// Returns an empty optional if it fails, else a breakpoint value that can be used to remove
		// This breakpoint or update later on. 
		// Be aware that this function sets debug registers on the CPU that can conflict with the ones
		// Set by the debugger. The thread must be suspended before this operation can take place, otherwise
		// Undefined behavior can occur.
		ECSENGINE_API Optional<HardwareBreakpoint> SetHardwareBreakpoint(
			void* thread_handle,
			void* address,
			const HardwareBreakpointOptions& options
		);

		// After a value was set using SetHardwareBreakpoint, you can update the breakpoint
		// With new options afterwards using this function. Returns true if it succeeded, else false.
		// The handler for this breakpoint can be changed at this point, as well as the address
		ECSENGINE_API bool UpdateHardwareBreakpoint(
			void* thread_handle,
			void* address,
			HardwareBreakpoint breakpoint,
			const HardwareBreakpointOptions& options
		);

		// Registers an exception handler that takes care of the hardware breakpoint handling
		ECSENGINE_API void PushTaskManagerHardwareBreakpointExceptionHandler(TaskManager* task_manager);

		// Removes a previously set breakpoint and returns the status of the action (with a distinction for the case when the breakpoint doesn't exist).
		// As with the Set function, you can suspend the thread with this call as a convenience.
		ECSENGINE_API ECS_REMOVE_HARDWARE_BREAKPOINT_STATUS RemoveHardwareBreakpoint(void* thread_handle, HardwareBreakpoint breakpoint, bool suspend_thread, CapacityStream<char>* error_message = nullptr);

		// Same as the other remove function (the name is different since it can cause confusion which overload is called), 
		// But it uses the address to find the hardware breakpoint.
		// As with the Set function, you can suspend the thread with this call as a convenience.
		ECSENGINE_API ECS_REMOVE_HARDWARE_BREAKPOINT_STATUS RemoveHardwareBreakpointByAddress(void* thread_handle, void* address, bool suspend_thread, CapacityStream<char>* error_message = nullptr);

		// --------------------------------------------- Handlers -----------------------------------------------------------------------------------------
		// This section implements some useful breakpoint handlers that can be used generically across many different use cases

		struct ECSENGINE_API HardwareBreakpointChangedValueHandler : HardwareBreakpointHandler {
			ECS_INLINE HardwareBreakpointChangedValueHandler(void* address, size_t address_byte_size) : HardwareBreakpointHandler(sizeof(previous_value)) {
				ECS_ASSERT(address_byte_size <= sizeof(previous_value));
				memcpy(previous_value, address, address_byte_size);
			}

			ECS_OS_EXCEPTION_CONTINUE_STATUS Handle(const HardwareBreakpointHandlerData& data) override;
			
			ECS_BLITTABLE_COPYABLE_IMPLEMENT(previous_value);

			// The previous value to compare the current breakpoint's value against
			char previous_value[sizeof(void*)];
		};

		// --------------------------------------------- Handlers -----------------------------------------------------------------------------------------

	}
}