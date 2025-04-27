#pragma once
#include "../Core.h"
#include "File.h"
#include "../Containers/ResizableAtomicDeck.h"
#include "../Allocators/MemoryManager.h"
#include "../Tools/UI/UIStructures.h"

namespace ECSEngine {

	enum ECS_FORMAT_DATE_FLAGS : size_t;

	// This is a small memory manager used for smaller allocations. It will initialize the first parameter
	// With memory from the second parameter
	ECSENGINE_API void DefaultConsoleStableAllocator(MemoryManager* allocator, GlobalMemoryManager* global_manager);

	// This is a an atomic stream "allocator" that is used for the message allocations
	// This allows extremely fast allocations since it is essentially a single atomic add
	// Operation to make the allocation
	ECSENGINE_API AtomicStream<char> DefaultConsoleMessageAllocator(GlobalMemoryManager* global_manager);

#define CONSOLE_APPEREANCE_TABLE_COUNT 256

	enum ECS_CONSOLE_VERBOSITY : unsigned char {
		ECS_CONSOLE_VERBOSITY_IMPORTANT,
		ECS_CONSOLE_VERBOSITY_MEDIUM,
		ECS_CONSOLE_VERBOSITY_DETAILED
	};

#define CONSOLE_SYSTEM_FILTER_ALL 0xFFFFFFFFFFFFFFFF

	enum ECS_CONSOLE_MESSAGE_TYPE : unsigned char {
		ECS_CONSOLE_INFO,
		ECS_CONSOLE_WARN,
		ECS_CONSOLE_ERROR,
		ECS_CONSOLE_TRACE,
		ECS_CONSOLE_GRAPHICS,
		ECS_CONSOLE_MESSAGE_COUNT
	};

	enum ECS_CONSOLE_DUMP_TYPE : unsigned char {
		ECS_CONSOLE_DUMP_COUNT,
		ECS_CONSOLE_DUMP_ON_CALL,
		ECS_CONSOLE_DUMP_NONE
	};

	typedef void (*ConsoleOnErrorTrigger)(void* user_data);

	struct ConsoleMessage {
		Stream<char> message;
		size_t system_filter;
		// This is an optional clickable action that can be added to the 
		Tools::UIActionHandler clickable_handler = { nullptr };
		unsigned char client_message_start;
		ECS_CONSOLE_MESSAGE_TYPE type = ECS_CONSOLE_MESSAGE_COUNT;
		unsigned char verbosity = ECS_CONSOLE_VERBOSITY_IMPORTANT;
	};

	struct ConsoleMessageOptions {
		Stream<char> system = {};
		ECS_CONSOLE_VERBOSITY verbosity = ECS_CONSOLE_VERBOSITY_IMPORTANT;
		Tools::UIActionHandler clickable_action = { nullptr };
	};

	// Dump path can be allocated on the stack, it will copy to a private buffer
	struct ECSENGINE_API Console {
		ECS_INLINE Console() = default;
		Console(MemoryManager* stable_allocator, AtomicStream<char> message_allocator, Stream<wchar_t> _dump_path);

		Console(const Console& other) = default;
		Console& operator = (const Console& other) = default;

		ECS_INLINE AllocatorPolymorphic StableAllocator() {
			return stable_allocator;
		}

		void Clear();

		void AddSystemFilterString(Stream<char> string);

		size_t GetFormatCharacterCount() const;

		void ConvertToMessage(Stream<char> message, const Tools::UIActionHandler& clickable_action, ConsoleMessage& console_message);

		void ChangeDumpPath(Stream<wchar_t> new_path);

		// Appends to the dump output
		void AppendDump();

		// It will clear the console if the clear on play flag is set to true
		void ClearOnPlay();

		// Dumps from the start using the background thread
		void Dump();

		// Dumps all the contents into the specified file - it uses this thread,
		// It does not use the background thread. Returns true if it succeeded, else false
		bool DumpToFile(Stream<wchar_t> path, bool write_as_binary = true);

		void Message(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, const ConsoleMessageOptions& options = {});

		void Info(Stream<char> message, const ConsoleMessageOptions& options = {});

		void Warn(Stream<char> message, const ConsoleMessageOptions& options = {});

		void Error(Stream<char> message, const ConsoleMessageOptions& options = {});

		void Trace(Stream<char> message, const ConsoleMessageOptions& options = {});

		void Graphics(Stream<char> message, const ConsoleMessageOptions& options = {});

		void WriteFormatCharacters(CapacityStream<char>& characters);

		void SetDumpType(ECS_CONSOLE_DUMP_TYPE type, unsigned int count = 1);

		void SetFormat(ECS_FORMAT_DATE_FLAGS format);

		void SetVerbosity(unsigned char new_level);

		// If the data_size is 0 then it will only reference the pointer, it will not copy the data
		void SetOnErrorTrigger(ConsoleOnErrorTrigger function, void* data, size_t data_size);

		ECS_INLINE static size_t MaxMessageCount() {
			return 25'000;
		}

		// This flag is used to notify the dumping thread when to write
		AtomicFlag dump_flag;
		bool pause_on_error;
		bool clear_on_play;
		unsigned char verbosity_level;
		ECS_CONSOLE_DUMP_TYPE dump_type;
		ECS_FORMAT_DATE_FLAGS format;
		// Caches the amount of necessary characters for the current format option
		unsigned char format_character_count;
		unsigned int last_dumped_message;
		unsigned int dump_count_for_commit;
		ECS_FILE_HANDLE dump_file;

		ConsoleOnErrorTrigger on_error_trigger;
		void* on_error_trigger_data;
		MemoryManager* stable_allocator;
		
		ResizableAtomicDeck<ConsoleMessage> messages;
		ResizableStream<Stream<char>> system_filter_strings;
		Stream<wchar_t> dump_path;

	private:
		char padding[ECS_CACHE_LINE_SIZE];
	public:
		// The message allocator is an atomic stream to allow for very fast
		// Allocations of the message. Even tho it is fixed, we can set a reasonable
		// Bound for the size of the message. Padd this stream on a separate cache line
		// Such that it won't false share a cache line with other fields and cause slow downs
		AtomicStream<char> message_allocator;
	};

	//ECSENGINE_API bool ConsoleAppendMessageToDump(ECS_FILE_HANDLE file, unsigned int index, Console* console);

	struct World;

	ECSENGINE_API Console* GetConsole();

	ECSENGINE_API void SetConsole(MemoryManager* stable_allocator, AtomicStream<char> message_allocator, Stream<wchar_t> dump_path);

}