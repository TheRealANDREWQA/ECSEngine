#pragma once
#include "../Core.h"
#include "Thread.h"

namespace ECSEngine {

	namespace OS {

		enum ECS_OS_EXCEPTION_ERROR_CODE : unsigned char {
			ECS_OS_EXCEPTION_ACCESS_VIOLATION,
			ECS_OS_EXCEPTION_MISSALIGNMENT,
			ECS_OS_EXCEPTION_STACK_OVERLOW,
			ECS_OS_EXCEPTION_PAGE_GUARD,

			// Floating point exceptions
			ECS_OS_EXCEPTION_FLOAT_OVERFLOW,
			ECS_OS_EXCEPTION_FLOAT_UNDERFLOW,
			ECS_OS_EXCEPTION_FLOAT_DIVISION_BY_ZERO,
			ECS_OS_EXCEPTION_FLOAT_DENORMAL,
			ECS_OS_EXCEPTION_FLOAT_INEXACT_VALUE,

			// Integer exceptions
			ECS_OS_EXCEPTION_INT_DIVISION_BY_ZERO,
			ECS_OS_EXCEPTION_INT_OVERFLOW,

			// Instruction exceptions
			ECS_OS_EXCEPTION_ILLEGAL_INSTRUCTION,
			ECS_OS_EXCEPTION_PRIVILEGED_INSTRUCTION,

			// Other types of exceptions - mostly should be ignored
			ECS_OS_EXCEPTION_UNKNOWN,
			ECS_OS_EXCEPTION_ERROR_CODE_COUNT
		};

		enum ECS_OS_EXCEPTION_CONTINUE_STATUS : unsigned char {
			// The exception won't propagate further, but it is discarded
			// And the execution will continue at that location
			ECS_OS_EXCEPTION_CONTINUE_IGNORE,
			// The exception is not handled, and will look further up
			// The handler chain
			ECS_OS_EXCEPTION_CONTINUE_UNHANDLED,
			// The exception is handled and the execution continues after
			// The handler block
			ECS_OS_EXCEPTION_CONTINUE_RESOLVED
		};

		struct ExceptionInformation {
			ECS_OS_EXCEPTION_ERROR_CODE error_code;
			// This is defined for the GUARD_PAGE and ACCESS_VIOLATION
			// Else it is nullptr
			void* faulting_page;
			// This is the context of the thread at the point of the crash
			ThreadContext thread_context;
		};

		// This should be used to convert from the native OS to the ECS variant
		ECSENGINE_API ExceptionInformation GetExceptionInformationFromNative(EXCEPTION_POINTERS* exception_pointers);

		ECSENGINE_API void ExceptionCodeToString(ECS_OS_EXCEPTION_ERROR_CODE code, CapacityStream<char>& string);

		// Returns true if the error indicates a real issue with the execution, like stack overflow,
		// Memory violation or invalid instruction
		ECSENGINE_API bool IsExceptionCodeCritical(ECS_OS_EXCEPTION_ERROR_CODE code);

		ECSENGINE_API int GetExceptionNativeContinueCode(ECS_OS_EXCEPTION_CONTINUE_STATUS continue_status);

		// This is a default exception filter that executes the handler in case a critical
		// Exception has happened, else it continues the handler search
		ECSENGINE_API int ExceptionHandlerFilterDefault(EXCEPTION_POINTERS* exception_pointers);

	}

}