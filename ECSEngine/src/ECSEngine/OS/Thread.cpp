#include "ecspch.h"
#include "Thread.h"
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

namespace ECSEngine {

	namespace OS {

		// -----------------------------------------------------------------------------------------------------

		bool CaptureCurrentThreadStackContext(ThreadContext* thread_context)
		{
			LPCONTEXT context = (LPCONTEXT)thread_context->bytes;
			RtlCaptureContext(context);
			return true;
		}

		// -----------------------------------------------------------------------------------------------------

		void GetCallStackFunctionNames(CapacityStream<char>& string)
		{
			ThreadContext current_thread_context;
			bool success = CaptureCurrentThreadStackContext(&current_thread_context);
			if (success) {
				GetCallStackFunctionNames(&current_thread_context, GetCurrentThread(), string);
			}
		}

		void GetCallStackFunctionNames(ThreadContext* context, void* thread_handle, CapacityStream<char>& string)
		{
			bool success;
			STACKFRAME64 stack_frame;
			memset(&stack_frame, 0, sizeof(stack_frame));

			LPCONTEXT os_context = (LPCONTEXT)context->bytes;
			os_context->ContextFlags = CONTEXT_FULL;

			stack_frame.AddrPC.Offset = os_context->Rip;
			stack_frame.AddrPC.Mode = AddrModeFlat;
			stack_frame.AddrStack.Offset = os_context->Rsp;
			stack_frame.AddrStack.Mode = AddrModeFlat;
			stack_frame.AddrFrame.Offset = os_context->Rbp;
			stack_frame.AddrFrame.Mode = AddrModeFlat;
			IMAGEHLP_SYMBOL64* image_symbol = (IMAGEHLP_SYMBOL64*)ECS_STACK_ALLOC(sizeof(IMAGEHLP_SYMBOL64) + 512);
			image_symbol->MaxNameLength = 511;
			image_symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);

			HANDLE process_handle = GetCurrentProcess();

			size_t displacement = 0;
			string.AddStreamAssert("Stack trace:\n");
			while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, process_handle, thread_handle, &stack_frame, os_context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
				success = SymGetSymFromAddr64(process_handle, (size_t)stack_frame.AddrPC.Offset, &displacement, image_symbol);
				if (success) {
					DWORD characters_written = UnDecorateSymbolName(image_symbol->Name, string.buffer + string.size, string.capacity - string.size, UNDNAME_COMPLETE);
					string.size += characters_written;
					string.Add('\n');
				}
			}

			string.AssertCapacity();
			string[string.size] = '\0';
		}

		// -----------------------------------------------------------------------------------------------------

		bool GetCallStackFunctionNames(void* thread_handle, CapacityStream<char>& string)
		{
			ThreadContext thread_context;
			bool success = OS::CaptureThreadStackContext(thread_handle, &thread_context);
			if (success) {
				GetCallStackFunctionNames(&thread_context, thread_handle, string);
				return true;
			}
			return false;
		}

		// ----------------------------------------------------------------------------------------------------

		void ChangeThreadPriority(ECS_THREAD_PRIORITY priority)
		{
			ChangeThreadPriority(GetCurrentThread(), priority);
		}

		// -----------------------------------------------------------------------------------------------------

		void ChangeThreadPriority(void* thread_handle, ECS_THREAD_PRIORITY priority)
		{
			int thread_priority = 0;
			switch (priority) {
			case ECS_THREAD_PRIORITY_VERY_LOW:
				thread_priority = THREAD_PRIORITY_LOWEST;
				break;
			case ECS_THREAD_PRIORITY_LOW:
				thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
				break;
			case ECS_THREAD_PRIORITY_NORMAL:
				thread_priority = THREAD_PRIORITY_NORMAL;
				break;
			case ECS_THREAD_PRIORITY_HIGH:
				thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
				break;
			case ECS_THREAD_PRIORITY_VERY_HIGH:
				thread_priority = THREAD_PRIORITY_HIGHEST;
				break;
			}

			BOOL success = SetThreadPriority(thread_handle, thread_priority);
			ECS_ASSERT(success, "Changing thread priority failed.");
		}

		// -----------------------------------------------------------------------------------------------------

		void ExitThread(int error_code) {
			::ExitThread(error_code);
		}

		// -----------------------------------------------------------------------------------------------------

		bool SuspendThread(void* handle)
		{
			DWORD suspend_count = ::SuspendThread(handle);
			DWORD last_error = GetLastError();
			return suspend_count != -1;
		}

		// -----------------------------------------------------------------------------------------------------

		bool ResumeThread(void* handle)
		{
			DWORD suspend_count = ::ResumeThread(handle);
			DWORD last_error = GetLastError();
			return suspend_count != -1;
		}

		// -----------------------------------------------------------------------------------------------------

		void* GetCurrentThreadHandle()
		{
			HANDLE pseudo_handle = GetCurrentThread();
			return DuplicateThreadHandle(pseudo_handle);
		}

		// -----------------------------------------------------------------------------------------------------

		void CloseThreadHandle(void* handle)
		{
			CloseHandle(handle);
		}

		// -----------------------------------------------------------------------------------------------------

		void* DuplicateThreadHandle(void* handle)
		{
			HANDLE process_handle = GetCurrentProcess();
			HANDLE new_handle = nullptr;
			BOOL success = DuplicateHandle(process_handle, handle, process_handle, &new_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
			return success ? new_handle : nullptr;
		}

		// -----------------------------------------------------------------------------------------------------

		size_t GetCurrentThreadID()
		{
			return GetCurrentThreadId();
		}

		// -----------------------------------------------------------------------------------------------------

		size_t GetThreadID(void* handle)
		{
			DWORD value = GetThreadId(handle);
			size_t last_error = GetLastError();
			return value == 0 ? (size_t)-1 : (size_t)value;
		}

		// -----------------------------------------------------------------------------------------------------

		size_t QueryThreadCycles(void* handle)
		{
			ULONG64 value;
			BOOL success = QueryThreadCycleTime(handle, &value);
			return success ? value : 0;
		}

		// -----------------------------------------------------------------------------------------------------

		bool CaptureThreadStackContext(void* thread_handle, ThreadContext* thread_context)
		{
			static_assert(sizeof(ThreadContext) >= sizeof(CONTEXT));
			size_t thread_id = GetThreadID(thread_handle);
			size_t running_thread_id = GetCurrentThreadID();
			if (thread_id == running_thread_id) {
				// In case we are the running thread, we need to use RtlCaptureContext
				// Since GetThreadContext doesn't work on the running thread
				RtlCaptureContext((LPCONTEXT)thread_context->bytes);
				return true;
			}
			else {
				return GetThreadContext(thread_handle, (LPCONTEXT)thread_context->bytes) != 0;
			}
		}

		// -----------------------------------------------------------------------------------------------------

		ThreadContext::ThreadContext()
		{
			Initialize();
		}

		void ThreadContext::Initialize()
		{
			LPCONTEXT context = (LPCONTEXT)bytes;
			context->ContextFlags = CONTEXT_FULL;
		}

		// -----------------------------------------------------------------------------------------------------

	}

}