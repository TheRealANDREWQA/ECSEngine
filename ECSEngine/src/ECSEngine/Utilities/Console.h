#pragma once
#include "../Core.h"
#include "../Internal/Multithreading/TaskManager.h"
#include "File.h"

namespace ECSEngine {

	ECSENGINE_API MemoryManager DefaultConsoleAllocator(GlobalMemoryManager* global_manager);

#define CONSOLE_APPEREANCE_TABLE_COUNT 256

#define CONSOLE_VERBOSITY_MINIMAL 0
#define CONSOLE_VERBOSITY_MEDIUM 1
#define CONSOLE_VERBOSITY_DETAILED 2

#define CONSOLE_SYSTEM_FILTER_ALL 0xFFFFFFFFFFFFFFFF

	enum class ConsoleMessageType : unsigned char {
		Info,
		Warn,
		Error,
		Trace,
		None
	};

	enum class ConsoleDumpType : unsigned char {
		CountMessages,
		OnCall,
		None
	};

	struct ConsoleMessage {
		Stream<char> message;
		unsigned char client_message_start;
		ConsoleMessageType type = ConsoleMessageType::None;
		unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL;
		size_t system_filter;
	};

	// Dump path can be allocated on the stack, it will copy to a private buffer
	struct ECSENGINE_API Console {
		Console() = default;
		Console(MemoryManager* allocator, TaskManager* task_manager, const wchar_t* _dump_path);

		Console(const Console& other) = default;
		Console& operator = (const Console& other) = default;

		void Lock();
		void Clear();

		void AddSystemFilterString(const char* string);
		void AddSystemFilterString(Stream<char> string);

		size_t GetFormatCharacterCount() const;
		void ConvertToMessage(const char* message, ConsoleMessage& console_message);
		void ConvertToMessage(Stream<char> message, ConsoleMessage& console_message);

		void ChangeDumpPath(const wchar_t* new_path);
		void ChangeDumpPath(Stream<wchar_t> new_path);

		// Appends to the dump output
		void AppendDump();

		// Dumps from the start
		void Dump();

		void Message(Stream<char> message, ConsoleMessageType type, const char* system = nullptr, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

		void Info(Stream<char> message, const char* system = nullptr, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

		void Warn(Stream<char> message, const char* system = nullptr, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

		void Error(Stream<char> message, const char* system = nullptr, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

		void Trace(Stream<char> message, const char* system = nullptr, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

		void WriteFormatCharacters(Stream<char>& characters);

		void SetDumpType(ConsoleDumpType type, unsigned int count = 1);

		void SetFormat(size_t format);
		void SetVerbosity(unsigned char new_level);

		MemoryManager* allocator;
		TaskManager* task_manager;
		ResizableStream<ConsoleMessage> messages;
		ResizableStream<const char*> system_filter_strings;
		size_t format;
		SpinLock lock;
		bool pause_on_error;
		unsigned char verbosity_level;
		ConsoleDumpType dump_type;
		unsigned int last_dumped_message;
		unsigned int dump_count_for_commit;
		const wchar_t* dump_path;
		SpinLock dump_lock;
	};

	struct ConsoleDumpData {
		Console* console;
		unsigned int starting_index;
	};

	ECSENGINE_API bool ConsoleAppendMessageToDump(ECS_FILE_HANDLE file, unsigned int index, Console* console);

	struct World;

	// Thread task
	ECSENGINE_API void ConsoleDump(unsigned int thread_index, World* world, void* data);

	// Thread task
	ECSENGINE_API void ConsoleAppendToDump(unsigned int thread_index, World* world, void* data);

	ECSENGINE_API Console* GetConsole();

	ECSENGINE_API void SetConsole(MemoryManager* allocator, TaskManager* task_manager, const wchar_t* dump_path);

}