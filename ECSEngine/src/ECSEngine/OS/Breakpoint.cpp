#include "ecspch.h"
#include "Breakpoint.h"
#include "Thread.h"
#include "../Utilities/StackScope.h"
#include "../Utilities/StringUtilities.h"
#include "../Multithreading/TaskManager.h"

namespace ECSEngine {

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

		struct HardwareBreakpointManager {
			HardwareBreakpointManager() : threads(ECS_MALLOC_ALLOCATOR, 0) {}

			// These entries are parallel to the debug registers
			struct BreakpointInfo {
				bool is_enabled;
				void* address;
				Stream<char> name;
				HardwareBreakpointHandler* handler;
			};

			struct Thread {
				// This is the OS handle of the thread
				void* thread_handle;
				BreakpointInfo registers[ECS_DEBUG_REGISTER_COUNT];
			};

			// Returns -1 if it doesn't find it. This function does not acquire the lock
			unsigned int FindThread(void* thread_handle) const {
				return threads.Find(thread_handle, [&](const Thread& thread) -> void* {
					return thread.thread_handle;
				});
			}

			// This function does not acquire the lock
			unsigned int FindOrAddThread(void* thread_handle) {
				unsigned int index = FindThread(thread_handle);

				if (index == -1) {
					index = threads.ReserveRange();
					// Initialize the entry
					threads[index].thread_handle = thread_handle;
					for (size_t register_index = 0; register_index < ECS_COUNTOF(threads[index].registers); register_index++) {
						threads[index].registers[register_index].is_enabled = false;
					}
				}

				return index;
			}

			// Does not acquire the lock. Returns the hardware breakpoint that corresponds to the provided address, if there is one registered
			Optional<HardwareBreakpoint> FindRegisterInThread(unsigned int thread_index, void* address) const {
				for (size_t index = 0; index < ECS_COUNTOF(threads[thread_index].registers); index++) {
					const BreakpointInfo& info = threads[thread_index].registers[index];
					if (info.is_enabled && info.address == address) {
						return { { (unsigned char)index } };
					}
				}

				return {};
			}

			void SetOrUpdateBreakpoint(void* thread_handle, void* address, const HardwareBreakpointOptions& options, HardwareBreakpoint breakpoint) {
				lock.Lock();

				unsigned int thread_index = FindOrAddThread(thread_handle);
				BreakpointInfo& info = threads[thread_index].registers[breakpoint.index];
				if (info.is_enabled) {
					// Deallocate the current fields
					info.name.Deallocate(ECS_MALLOC_ALLOCATOR);
					CopyableDeallocate(info.handler, ECS_MALLOC_ALLOCATOR);
				}

				info.name = options.name.Copy(ECS_MALLOC_ALLOCATOR);
				info.address = address;
				info.is_enabled = true;
				info.handler = (HardwareBreakpointHandler*)CopyableCopy(options.handler, ECS_MALLOC_ALLOCATOR);

				lock.Unlock();
			}

			// Returns true if the breakpoint was removed successfully, else false
			bool RemoveBreakpoint(void* thread_handle, HardwareBreakpoint breakpoint, CapacityStream<char>* error_message = nullptr) {
				lock.Lock();

				unsigned int thread_index = FindThread(thread_handle);
				BreakpointInfo& info = threads[thread_index].registers[breakpoint.index];
				if (!info.is_enabled) {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "HardwareBreakpointManager does not contain a specified breakpoint for thread with handle {#}", thread_handle);
					return false;
				}

				info.name.Deallocate(ECS_MALLOC_ALLOCATOR);
				CopyableDeallocate(info.handler, ECS_MALLOC_ALLOCATOR);
				info.handler = nullptr;
				info.is_enabled = false;

				lock.Unlock();
			}

			// The breakpoint context contains 
			ResizableStream<Thread> threads;
			// Use to achieve thread safety
			SpinLock lock;
		};

		// This is a global structure that records information about all hardware breakpoints that have been set in this process
		static HardwareBreakpointManager BREAKPOINT_MANAGER;

		// This function uses the breakpoint manager to lookup if the breakpoint is registered in the manager and run its handler, if specified
		static ECS_OS_EXCEPTION_CONTINUE_STATUS BreakpointManagerTaskManagerExceptionHandler(TaskManagerExceptionHandlerData* function_data) {
			// If the error is not a hardware breakpoint, let the search continue
			if (function_data->exception_information.error_code != ECS_OS_EXCEPTION_HARDWARE_BREAKPOINT) {
				return ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
			}

			// Extract the address that we are looking for. The bits 0, 1, 2, 3, indicate which of the Dr0, Dr1, Dr2, Dr3 triggered this exception
			void* address = nullptr;
			CONTEXT* thread_context = (CONTEXT*)function_data->exception_information.thread_context.bytes;
			size_t dr6 = thread_context->Dr6;
			if (HasFlag(dr6, 0b1)) {
				address = (void*)thread_context->Dr0;
			}
			else if (HasFlag(dr6, 0b10)) {
				address = (void*)thread_context->Dr1;
			}
			else if (HasFlag(dr6, 0b100)) {
				address = (void*)thread_context->Dr2;
			}
			else if (HasFlag(dr6, 0b1000)) {
				address = (void*)thread_context->Dr3;
			}
			else {
				// The address couldn't be located, we cannot handle this exception
				return ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
			}

			// Don't need to lock the manager for this
			unsigned int thread_index = BREAKPOINT_MANAGER.FindThread(function_data->thread_handle);
			// If the thread index is -1, we can simply ignore this exception, and let the next handler take care of it
			if (thread_index == -1) {
				return ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
			}

			Optional<HardwareBreakpoint> breakpoint = BREAKPOINT_MANAGER.FindRegisterInThread(thread_index, address);
			if (!breakpoint.has_value) {
				// Cannot handle this exception
				return ECS_OS_EXCEPTION_CONTINUE_UNHANDLED;
			}

			// By default, we will consider this breakpoint as ignored at this point, if the handler is not set
			ECS_OS_EXCEPTION_CONTINUE_STATUS continue_status = ECS_OS_EXCEPTION_CONTINUE_IGNORE;
			HardwareBreakpointManager::BreakpointInfo& info = BREAKPOINT_MANAGER.threads[thread_index].registers[breakpoint.value.index];
			if (info.handler != nullptr) {
				continue_status = info.handler->Handle(function_data->exception_information, function_data->thread_handle, address, breakpoint.value);
			}

			// If the breakpoint is still ignored or resolved after the handler call (if it was called),
			// Trigger a software breakpoint to direct the user here. This name variable is intentional here,
			// In order to have the debugger show this variable easier than having to access info.name
			Stream<char> name = info.name;
			if (continue_status == ECS_OS_EXCEPTION_CONTINUE_RESOLVED || continue_status == ECS_OS_EXCEPTION_CONTINUE_IGNORE) {
				__debugbreak();
			}

			return continue_status;
		}

		// This function handles the set/update of a hardware debug register, it also handles any additional operations that need to be done
		// For this breakpoint to be registered properly. The functor is called with (const CONTEXT& thread_context) and should return a
		// Optional<HardwareBreakpoint>.
		template<typename GetBreakpointFunctor>
		static bool SetOrUpdateHardwareRegister(void* thread_handle, void* address, const HardwareBreakpointOptions& options, GetBreakpointFunctor&& get_breakpoint_functor)
		{
			// Suspend the thread, if specified
			if (options.suspend_thread) {
				if (!SuspendThread(thread_handle)) {
					ECS_FORMAT_ERROR_MESSAGE(options.error_message, "Failed to suspend the thread");
					return false;
				}
			}

			// Create a stack scope to resume the thread if we suspended it
			auto resume_thread = StackScope([&]() {
				if (options.suspend_thread) {
					ECS_ASSERT(ResumeThread(thread_handle), "Setting hardware breakpoint failed - could not resume the stopped thread");
				}
			});

			CONTEXT thread_context = { 0 };
			thread_context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			if (!GetThreadContext(thread_handle, &thread_context)) {
				ECS_FORMAT_ERROR_MESSAGE(options.error_message, "Failed to retrieve the thread's debug context");
				return false;
			}

			Optional<HardwareBreakpoint> breakpoint = get_breakpoint_functor(thread_context);
			if (!breakpoint.has_value) {
				ECS_FORMAT_ERROR_MESSAGE(options.error_message, "All debug registers are in use");
				return false;
			}

			size_t debug_register_bits = thread_context.Dr7;

			// Found a register which is not in use. Use it now.
			size_t debug_register_execution_bits = 0;
			switch (options.type) {
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
			switch (options.address_byte_size) {
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
				ECS_FORMAT_ERROR_MESSAGE(options.error_message, "Failed to set thread context when setting a hardware breakpoint");
				return false;
			}

			// After all of this succeeded, notify the breakpoint manager
			BREAKPOINT_MANAGER.SetOrUpdateBreakpoint(thread_handle, address, options, breakpoint.value);

			return true;
		}

		Optional<HardwareBreakpoint> SetHardwareBreakpoint(
			void* thread_handle, 
			void* address, 
			const HardwareBreakpointOptions& options
		) {
			Optional<HardwareBreakpoint> breakpoint;

			if (!SetOrUpdateHardwareRegister(thread_handle, address, options, [&](const CONTEXT& thread_context) -> Optional<HardwareBreakpoint> {
				// Check to see if all slots are occupied
				size_t debug_register_bits = thread_context.Dr7;
				for (size_t index = 0; index < ECS_DEBUG_REGISTER_COUNT; index++) {
					if (!HasFlag(debug_register_bits, (size_t)0b01 << (index * 2))) {
						breakpoint.value.index = (unsigned char)index;
						breakpoint.has_value = true;
						break;
					}
				}

				return breakpoint;
			})) {
				breakpoint.has_value = false;
			}

			return breakpoint;
		}

		bool UpdateHardwareBreakpoint(void* thread_handle, void* address, HardwareBreakpoint breakpoint, const HardwareBreakpointOptions& options) {
			return SetOrUpdateHardwareRegister(thread_handle, address, options, [&](const CONTEXT& context) -> HardwareBreakpoint {
				return breakpoint;
			});
		}

		void PushTaskManagerHardwareBreakpointExceptionHandler(TaskManager* task_manager) {
			task_manager->PushExceptionHandlerThreadSafe(BreakpointManagerTaskManagerExceptionHandler, nullptr, 0);
		}

		// Implements the common code path for removing a breakpoint. The functor takes as parameter (const CONTEXT& thread_context)
		// And should return Optional<HardwareBreakpoint> with the breakpoint to be removed
		template<typename GetBreakpointFunctor>
		static bool RemoveHardwareBreakpointImpl(void* thread_handle, bool suspend_thread, CapacityStream<char>* error_message, GetBreakpointFunctor&& get_breakpoint_functor) {
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

			Optional<HardwareBreakpoint> breakpoint = get_breakpoint_functor(thread_context);
			if (!breakpoint.has_value) {
				return false;
			}

			// If the breakpoint value is out of bounds, fail
			if (breakpoint.value.index >= ECS_DEBUG_REGISTER_COUNT) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Invalid specified breakpoint for RemoveHardwareBreakpoint");
				return false;
			}

			// Ensure that the breakpoint does exist
			size_t debug_register_bits = thread_context.Dr7;

			size_t bitshift_count = breakpoint.value.index * 2;
			if (!HasFlag(debug_register_bits, (size_t)0b01 << bitshift_count)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "The thread does not have the specified breakpoint");
				return false;
			}

			// Reset the address to nullptr
			switch (breakpoint.value.index) {
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
			thread_context.Dr7 = debug_register_bits;

			if (!SetThreadContext(thread_handle, &thread_context)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to set thread context when removing a hardware breakpoint");
				return false;
			}

			// After all of this succeeded, notify the breakpoint manager about this
			if (!BREAKPOINT_MANAGER.RemoveBreakpoint(thread_handle, breakpoint.value, error_message)) {
				return false;
			}

			return true;
		}

		bool RemoveHardwareBreakpoint(void* thread_handle, HardwareBreakpoint breakpoint, bool suspend_thread, CapacityStream<char>* error_message) {
			return RemoveHardwareBreakpointImpl(thread_handle, suspend_thread, error_message, [&](const CONTEXT& thread_context) -> HardwareBreakpoint {
				return breakpoint;
			});
		}

		bool RemoveHardwareBreakpoint(void* thread_handle, void* address, bool suspend_thread, CapacityStream<char>* error_message)
		{
			return RemoveHardwareBreakpointImpl(thread_handle, suspend_thread, error_message, [&](const CONTEXT& thread_context) -> Optional<HardwareBreakpoint> {
				Optional<HardwareBreakpoint> breakpoint;
				if (thread_context.Dr0 == (size_t)address) {
					breakpoint.value.index = 0;
				}
				else if (thread_context.Dr1 == (size_t)address) {
					breakpoint.value.index = 1;
				}
				else if (thread_context.Dr2 == (size_t)address) {
					breakpoint.value.index = 2;
				}
				else if (thread_context.Dr3 == (size_t)address) {
					breakpoint.value.index = 3;
				}
				else {
					ECS_FORMAT_ERROR_MESSAGE(error_message, "The address {#} is not set as a hardware breakpoint when trying to remove it from a thread", address);
					return {};
				}

				breakpoint.has_value = true;
				return breakpoint;
			});
		}

	}

}