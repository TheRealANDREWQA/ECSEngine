#include "ecspch.h"
#include "Assert.h"

#define ECS_ASSERT_TRIGGER

namespace ECSEngine {

	namespace function {

		void Assert(bool condition, const char* filename, unsigned int line, const char* error_message)
		{
#ifdef ECSENGINE_DISTRIBUTION
#ifdef ECS_ASSERT_TRIGGER
			if (!condition) {
				char temp_memory[512];
				unsigned int memory_offset = _countof("[File] ");
				memcpy(temp_memory, "[File] ", memory_offset);
				size_t filename_size = strlen(filename);
				memcpy(temp_memory + memory_offset, filename, filename_size);
				memory_offset += filename_size;

				size_t line_size = _countof("[Line] ");
				memcpy(temp_memory + memory_offset, "[Line] ", line_size);
				memory_offset += line_size;
				temp_memory[memory_offset] = '\n';
				if (error_message != nullptr) {
					size_t message_size = strlen(error_message);
					memcpy(temp_memory + memory_offset, error_message, message_size);
					memory_offset += message_size;
				}
				temp_memory[memory_offset] = '\0';
				MessageBoxA(nullptr, temp_memory, "ECS Assert", MB_OK | MB_ICONERROR);
				__debugbreak();
				::exit(0);
			}
#endif
#else
			if (!condition) {
				__debugbreak();
			}
#endif
		}

	}

}