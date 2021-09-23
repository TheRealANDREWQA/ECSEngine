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
				std::ostringstream stream;
				stream << "[File] " << filename << "\n" << "[Line] " << line << "\n";
				if (error_message != nullptr) {
					stream << error_message;
				}
				wchar_t converted_message[1024];
				MultiByteToWideChar(CP_ACP, 0, stream.str().c_str(), -1, converted_message, 1024);
				MessageBox(nullptr, converted_message, L"ECS Assert", MB_OK | MB_ICONERROR);
				__debugbreak();
				exit(line);
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