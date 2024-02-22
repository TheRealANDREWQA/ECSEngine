#include "ecspch.h"
#include "ExceptionHandling.h"

namespace ECSEngine {

	namespace OS {

		// -----------------------------------------------------------------------------------------------------

		ExceptionInformation GetExceptionInformationFromNative(EXCEPTION_POINTERS* exception_pointers)
		{
			ExceptionInformation exception_information;

			memcpy(exception_information.thread_context.bytes, exception_pointers->ContextRecord, sizeof(*exception_pointers->ContextRecord));
			ECS_OS_EXCEPTION_ERROR_CODE error_code = ECS_OS_EXCEPTION_UNKNOWN;
			exception_information.faulting_page = nullptr;
			switch (exception_pointers->ExceptionRecord->ExceptionCode) {
			case EXCEPTION_ACCESS_VIOLATION:
				error_code = ECS_OS_EXCEPTION_ACCESS_VIOLATION;
				exception_information.faulting_page = (void*)exception_pointers->ExceptionRecord->ExceptionInformation[1];
				break;
			case EXCEPTION_DATATYPE_MISALIGNMENT:
				error_code = ECS_OS_EXCEPTION_MISSALIGNMENT;
				break;
			case EXCEPTION_GUARD_PAGE:
				error_code = ECS_OS_EXCEPTION_PAGE_GUARD;
				exception_information.faulting_page = (void*)exception_pointers->ExceptionRecord->ExceptionInformation[1];
				break;
			case EXCEPTION_FLT_DENORMAL_OPERAND:
				error_code = ECS_OS_EXCEPTION_FLOAT_DENORMAL;
				break;
			case EXCEPTION_FLT_DIVIDE_BY_ZERO:
				error_code = ECS_OS_EXCEPTION_FLOAT_DIVISION_BY_ZERO;
				break;
			case EXCEPTION_FLT_INEXACT_RESULT:
				error_code = ECS_OS_EXCEPTION_FLOAT_INEXACT_VALUE;
				break;
			case EXCEPTION_FLT_OVERFLOW:
				error_code = ECS_OS_EXCEPTION_FLOAT_OVERFLOW;
				break;
			case EXCEPTION_FLT_UNDERFLOW:
				error_code = ECS_OS_EXCEPTION_FLOAT_UNDERFLOW;
				break;
			case EXCEPTION_ILLEGAL_INSTRUCTION:
				error_code = ECS_OS_EXCEPTION_ILLEGAL_INSTRUCTION;
				break;
			case EXCEPTION_INT_DIVIDE_BY_ZERO:
				error_code = ECS_OS_EXCEPTION_INT_DIVISION_BY_ZERO;
				break;
			case EXCEPTION_INT_OVERFLOW:
				error_code = ECS_OS_EXCEPTION_INT_OVERFLOW;
				break;
			case EXCEPTION_STACK_OVERFLOW:
				error_code = ECS_OS_EXCEPTION_STACK_OVERLOW;
				break;
			}
			exception_information.error_code = error_code;

			return exception_information;
		}

		// ----------------------------------------------------------------------------------------------------

		static const char* OS_EXCEPTION_TO_STRING[] = {
			"Access violation",
			"Data missalignment",
			"Stack overflow",
			"Guard Page access",
			"Float overflow",
			"Float underflow",
			"Float division by zero",
			"Float denormal",
			"Float inexact value",
			"Integer division by zero",
			"Integer overflow",
			"Illegal instruction",
			"Privileged instruction",
			"Unknown"
		};

		static_assert(ECS_COUNTOF(OS_EXCEPTION_TO_STRING) == ECS_OS_EXCEPTION_ERROR_CODE_COUNT);

		void ExceptionCodeToString(ECS_OS_EXCEPTION_ERROR_CODE code, CapacityStream<char>& string)
		{
			ECS_ASSERT(code < ECS_OS_EXCEPTION_ERROR_CODE_COUNT);
			string.AddStreamAssert(OS_EXCEPTION_TO_STRING[code]);
		}

		// ----------------------------------------------------------------------------------------------------

		bool IsExceptionCodeCritical(ECS_OS_EXCEPTION_ERROR_CODE code)
		{
			// Include unknown as well
			switch (code) {
			case ECS_OS_EXCEPTION_ACCESS_VIOLATION:
			case ECS_OS_EXCEPTION_MISSALIGNMENT:
			case ECS_OS_EXCEPTION_STACK_OVERLOW:
			case ECS_OS_EXCEPTION_ILLEGAL_INSTRUCTION:
			case ECS_OS_EXCEPTION_PRIVILEGED_INSTRUCTION:
			case ECS_OS_EXCEPTION_FLOAT_DIVISION_BY_ZERO:
			case ECS_OS_EXCEPTION_INT_DIVISION_BY_ZERO:
			case ECS_OS_EXCEPTION_UNKNOWN:
				return true;
			}

			return false;
		}

		// ----------------------------------------------------------------------------------------------------

	}

}