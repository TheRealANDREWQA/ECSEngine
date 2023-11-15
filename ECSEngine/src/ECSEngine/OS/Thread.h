#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace OS {

		// Exists the current thread
		ECSENGINE_API void ExitThread(int error_code);

		// Returns true if it succeeded, else false
		ECSENGINE_API bool SuspendThread(void* handle);

		ECSENGINE_API bool ResumeThread(void* handle);

		// Returns nullptr if it failed, else the handle value. This handle value must be
		// Closed when you are finished using it
		ECSENGINE_API void* GetCurrentThreadHandle();

		// This function is given primarly to be used with GetCurrentThreadHandle
		// To release the handle after it has been used
		ECSENGINE_API void CloseThreadHandle(void* handle);

		// It creates a new separate handle from the given one. You must release it with CloseThreadHandle
		// Returns nullptr if it failed
		ECSENGINE_API void* DuplicateThreadHandle(void* handle);

		/*
			To determine if two threads are the same, don't use handles.
			Instead use thread IDs since these are agnostic while there
			Can be multiple thread handles for the same thread and they will
			Never be the same
		*/

		// Returns an OS ID for the current thread
		ECSENGINE_API size_t GetCurrentThreadID();

		// Returns an OS ID for that thread handle. Returns -1 if an error has occured
		ECSENGINE_API size_t GetThreadID(void* handle);

		// Returns 0 if it failed in getting the value
		ECSENGINE_API size_t QueryThreadCycles(void* handle);

		// This struct is used to wrapp the largest OS context platform
		// We do not attempt to let the user do anything with it, just passed
		// To other OS functions
		struct ECSENGINE_API ThreadContext {
			ThreadContext();

			// Sets up anything that is necessary
			void Initialize();

			char bytes[1300];
		};

		// Returns true if it succeeded, else false. In order to have consistent behaviour,
		// Suspend the thread before capturing its context
		ECSENGINE_API bool CaptureThreadStackContext(void* thread_handle, ThreadContext* thread_context);

		// Returns true if it succeeded, else false
		ECSENGINE_API bool CaptureCurrentThreadStackContext(ThreadContext* thread_context);

		// Assumes that InitializeSymbolicLinksPaths has been called
		ECSENGINE_API void GetCallStackFunctionNames(CapacityStream<char>& string);

		// Assumes that InitializeSymbolicLinksPaths has been called
		// The thread should be suspended before getting its context
		ECSENGINE_API void GetCallStackFunctionNames(ThreadContext* context, void* thread_handle, CapacityStream<char>& string);

		// Assumes that InitializeSymbolicLinksPaths has been called
		// The thread should be suspended before getting its context
		// Returns true if it managed to get the stack frame, else false
		ECSENGINE_API bool GetCallStackFunctionNames(void* thread_handle, CapacityStream<char>& string);

		enum ECS_THREAD_PRIORITY : unsigned char {
			ECS_THREAD_PRIORITY_VERY_LOW,
			ECS_THREAD_PRIORITY_LOW,
			ECS_THREAD_PRIORITY_NORMAL,
			ECS_THREAD_PRIORITY_HIGH,
			ECS_THREAD_PRIORITY_VERY_HIGH
		};

		ECSENGINE_API void ChangeThreadPriority(ECS_THREAD_PRIORITY priority);

		ECSENGINE_API void ChangeThreadPriority(void* thread_handle, ECS_THREAD_PRIORITY priority);

	}

}