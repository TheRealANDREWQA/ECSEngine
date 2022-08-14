#pragma once
#include "../Core.h"
#include "../Internal/Multithreading/TaskManager.h"
#include "File.h"

namespace ECSEngine {

	ECSENGINE_API MemoryManager DefaultConsoleAllocator(GlobalMemoryManager* global_manager);

#define CONSOLE_APPEREANCE_TABLE_COUNT 256

	enum ECS_CONSOLE_VERBOSITY : unsigned char {
		ECS_CONSOLE_VERBOSITY_MINIMAL,
		ECS_CONSOLE_VERBOSITY_MEDIUM,
		ECS_CONSOLE_VERBOSITY_DETAILED
	};

#define CONSOLE_SYSTEM_FILTER_ALL 0xFFFFFFFFFFFFFFFF

	enum ECS_CONSOLE_MESSAGE_TYPE : unsigned char {
		ECS_CONSOLE_INFO,
		ECS_CONSOLE_WARN,
		ECS_CONSOLE_ERROR,
		ECS_CONSOLE_TRACE,
		ECS_CONSOLE_MESSAGE_COUNT
	};

	enum ECS_CONSOLE_DUMP_TYPE : unsigned char {
		ECS_CONSOLE_DUMP_COUNT,
		ECS_CONSOLE_DUMP_ON_CALL,
		ECS_CONSOLE_DUMP_NONE
	};

	struct ConsoleMessage {
		Stream<char> message;
		size_t system_filter;
		unsigned char client_message_start;
		ECS_CONSOLE_MESSAGE_TYPE type = ECS_CONSOLE_MESSAGE_COUNT;
		unsigned char verbosity = ECS_CONSOLE_VERBOSITY_MINIMAL;
	};

	// Dump path can be allocated on the stack, it will copy to a private buffer
	struct ECSENGINE_API Console {
		Console() = default;
		Console(MemoryManager* allocator, TaskManager* task_manager, Stream<wchar_t> _dump_path);

		Console(const Console& other) = default;
		Console& operator = (const Console& other) = default;

		void Lock();
		void Clear();

		void AddSystemFilterString(Stream<char> string);

		size_t GetFormatCharacterCount() const;

		void ConvertToMessage(Stream<char> message, ConsoleMessage& console_message);

		void ChangeDumpPath(Stream<wchar_t> new_path);

		// Appends to the dump output
		void AppendDump();

		// Dumps from the start
		void Dump();

		void Message(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, Stream<char> system = { nullptr, 0 }, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MINIMAL);

		void Info(Stream<char> message, Stream<char> system = { nullptr, 0 }, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MINIMAL);

		void Warn(Stream<char> message, Stream<char> system = { nullptr, 0 }, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MINIMAL);

		void Error(Stream<char> message, Stream<char> system = { nullptr, 0 }, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MINIMAL);

		void Trace(Stream<char> message, Stream<char> system = { nullptr, 0 }, ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_MINIMAL);

		void WriteFormatCharacters(Stream<char>& characters);

		void SetDumpType(ECS_CONSOLE_DUMP_TYPE type, unsigned int count = 1);

		void SetFormat(size_t format);
		void SetVerbosity(unsigned char new_level);

		MemoryManager* allocator;
		TaskManager* task_manager;
		ResizableStream<ConsoleMessage> messages;
		ResizableStream<Stream<char>> system_filter_strings;
		size_t format;
		SpinLock lock;
		bool pause_on_error;
		unsigned char verbosity_level;
		ECS_CONSOLE_DUMP_TYPE dump_type;
		unsigned int last_dumped_message;
		unsigned int dump_count_for_commit;
		Stream<wchar_t> dump_path;
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