#include "ecspch.h"
#include "Console.h"
#include "StringUtilities.h"
#include "Utilities.h"
#include "../OS/Misc.h"
#include "../OS/Thread.h"
#include "../ECS/World.h"

#define FILE_BUFFERING_STACK_SIZE ECS_KB * 128

namespace ECSEngine {

	Console global_console;
	static bool GLOBAL_CONSOLE_THREAD_IS_CREATED = false;

	enum CONSOLE_DUMP_OPERATION : unsigned char {
		CONSOLE_DUMP_APPEND,
		CONSOLE_DUMP_ALL,
		CONSOLE_DUMP_NOTHING
	};

	static CONSOLE_DUMP_OPERATION DUMP_OPERATION = CONSOLE_DUMP_NOTHING;

	// -------------------------------------------------------------------------------------------------------

	static bool ConsoleAppendMessageToDump(ECS_FILE_HANDLE file, const ConsoleMessage& message, CapacityStream<void>& buffering) {
		// temporarly swap the \0 to \n in order to have it formatted correctly
		char* mutable_ptr = message.message.buffer;
		mutable_ptr[message.message.size] = '\n';
		bool success = WriteFile(file, { mutable_ptr, message.message.size + 1 }, buffering);
		mutable_ptr[message.message.size] = '\0';
		return success;
	}

	// -------------------------------------------------------------------------------------------------------

	// Returns true if it succeeded, else false
	static bool ConsoleDump(ECS_FILE_HANDLE file) {
		Console* console = GetConsole();

#pragma region Header

		Date current_date = OS::GetLocalTime();

		ECS_STACK_VOID_STREAM(file_buffering, FILE_BUFFERING_STACK_SIZE);

		bool success = true;

		ECS_STACK_CAPACITY_STREAM(char, time_characters, 256);
		Stream<char> separator_characters = "**********************************************\n";
		time_characters.AddStreamAssert(separator_characters);
		ConvertIntToChars(time_characters, current_date.hour);
		time_characters.AddAssert(':');
		ConvertIntToChars(time_characters, current_date.minute);
		time_characters.AddAssert(':');
		ConvertIntToChars(time_characters, current_date.seconds);
		time_characters.AddAssert(':');
		ConvertIntToChars(time_characters, current_date.milliseconds);
		time_characters.AddAssert(' ');
		ConvertIntToChars(time_characters, current_date.day);
		time_characters.AddAssert('-');
		ConvertIntToChars(time_characters, current_date.month);
		time_characters.AddAssert('-');
		ConvertIntToChars(time_characters, current_date.year);
		time_characters.AddAssert(' ');
		const char description[] = "Console output\n";
		time_characters.AddStreamAssert(Stream<char>(description, ECS_COUNTOF(description) - 1));
		time_characters.AddStreamAssert(separator_characters);

		success &= WriteFile(file, time_characters, file_buffering);

#pragma endregion

#pragma region Messages

		if (success) {
			if (console->messages.chunk_size > 0) {
				// Initialize with -1 such that in case there are no messages it won't mess up
				// the append to dump
				unsigned int success_write = -1;
				success &= !console->messages.ForAllCurrentElements<true>([&](const ConsoleMessage& message) {
					success &= ConsoleAppendMessageToDump(file, message, file_buffering);
					return !success;
					}, &success_write);

				// Update this only it is the same file
				if (file == console->dump_file) {
					console->last_dumped_message = success_write;
				}
			}
		}
		else {
			return false;
		}

		if (file_buffering.size > 0 && success) {
			success = WriteFile(file, file_buffering);
		}

#pragma endregion

		// Separated these for debugging
		bool write_success = success;
		if (!write_success) {
			__debugbreak();
		}

		bool flush_success = FlushFileToDisk(file);
		success &= flush_success;
		return success;
	}

	// -------------------------------------------------------------------------------------------------------

	static void ConsoleAppendToDump()
	{
		Console* console = GetConsole();

		ECS_STACK_VOID_STREAM(file_buffering, FILE_BUFFERING_STACK_SIZE);

		bool write_success = true;
		unsigned int current_message_count = console->messages.WaitWrites();
		unsigned int final_written_message = 0;
		// This case works for when the last_dumped_message is -1 as well
		unsigned int starting_index = console->last_dumped_message + 1;
		console->messages.ForEach<true>(starting_index, current_message_count, [&](const ConsoleMessage& message) {
			write_success &= ConsoleAppendMessageToDump(console->dump_file, message, file_buffering);
			return !write_success;
		}, &final_written_message);

		if (write_success && file_buffering.size > 0) {
			write_success = WriteFile(console->dump_file, file_buffering);
		}
		write_success &= FlushFileToDisk(console->dump_file);
		console->last_dumped_message = final_written_message - 1;

		ECS_ASSERT(write_success);
	}

	// -------------------------------------------------------------------------------------------------------

	static void BackgroundThreadFunction() {
		Console* console = GetConsole();
		while (true) {
			console->dump_flag.Clear();
			if (DUMP_OPERATION == CONSOLE_DUMP_APPEND) {
				ConsoleAppendToDump();
			}
			else if (DUMP_OPERATION == CONSOLE_DUMP_ALL) {
				bool success = ConsoleDump(console->dump_file);
				ECS_ASSERT(success, "Failed to write console dump");
			}
			console->dump_flag.Wait();
		}
	}

	// -------------------------------------------------------------------------------------------------------

	void DefaultConsoleStableAllocator(MemoryManager* allocator, GlobalMemoryManager* global_manager)
	{
		new (allocator) MemoryManager(ECS_KB * 512, ECS_KB * 4, ECS_KB * 512, global_manager);
	}

	// -------------------------------------------------------------------------------------------------------

	AtomicStream<char> DefaultConsoleMessageAllocator(GlobalMemoryManager* global_manager) {
		// Estimate a large count of characters per message
		const size_t CHARACTER_COUNT_PER_MESSAGE = 300;
		size_t allocation_capacity = Console::MaxMessageCount() * CHARACTER_COUNT_PER_MESSAGE;
		void* allocation = global_manager->Allocate(sizeof(char) * allocation_capacity);
		return AtomicStream<char>(allocation, 0, allocation_capacity);
	}

	// -------------------------------------------------------------------------------------------------------

	Console::Console(MemoryManager* stable_allocator, AtomicStream<char> message_allocator, Stream<wchar_t> _dump_path) 
		: stable_allocator(stable_allocator), message_allocator(message_allocator), pause_on_error(false), 
		verbosity_level(ECS_CONSOLE_VERBOSITY_DETAILED), dump_type(ECS_CONSOLE_DUMP_COUNT), last_dumped_message(-1), 
		dump_count_for_commit(1), dump_file(-1)
	{
		on_error_trigger = nullptr;
		on_error_trigger_data = nullptr;
		clear_on_play = false;

		format = ECS_FORMAT_DATE_HOUR | ECS_FORMAT_DATE_MINUTES | ECS_FORMAT_DATE_SECONDS;
		format_character_count = ConvertDateToStringMaxCharacterCount(format);
		// Don't choose a power of two as chunk size to avoid cache associativity problems
		const size_t MESSAGE_CHUNK_COUNT = 500;
		size_t max_chunk_allocation_count = (MaxMessageCount() / MESSAGE_CHUNK_COUNT) + 1;
		messages.Initialize(StableAllocator(), MESSAGE_CHUNK_COUNT, max_chunk_allocation_count);
		system_filter_strings.Initialize(StableAllocator(), 0);

		if (!GLOBAL_CONSOLE_THREAD_IS_CREATED) {
			// Create a thread that will be writing the dump file
			auto thread = std::thread([]() {
				BackgroundThreadFunction();
			});

			OS::ChangeThreadPriority(thread.native_handle(), OS::ECS_THREAD_PRIORITY_HIGH);
			// Now detach the thread
			thread.detach();
			GLOBAL_CONSOLE_THREAD_IS_CREATED = true;
		}

		ChangeDumpPath(_dump_path);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Clear()
	{
		dump_flag.Clear();
		messages.Clear();
		last_dumped_message = -1;
		message_allocator.Reset();
		ClearFile(dump_file);
		Dump();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::AddSystemFilterString(Stream<char> string) {
		string = StringCopy(StableAllocator(), string);
		system_filter_strings.Add(string);
	}

	// -------------------------------------------------------------------------------------------------------

	size_t Console::GetFormatCharacterCount() const
	{
		return format_character_count;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::ConvertToMessage(Stream<char> message, const Tools::UIActionHandler& clickable_handler, ConsoleMessage& console_message)
	{
		size_t format_character_count = GetFormatCharacterCount();

		bool not_enough_space = false;
		CapacityStream<char> allocation = message_allocator.Request(message.size + format_character_count + 1 + clickable_handler.data_size, not_enough_space);
		ECS_HARD_ASSERT(!not_enough_space, "Console message allocator ran out of space");

		allocation.size = 0;
		WriteFormatCharacters(allocation);
		console_message.client_message_start = allocation.size;

		memcpy(allocation.buffer + allocation.size, message.buffer, sizeof(char) * message.size);
		allocation.size += message.size;
		allocation[allocation.size] = '\0';
		console_message.message = allocation;

		console_message.clickable_handler = clickable_handler;
		if (clickable_handler.data_size > 0) {
			console_message.clickable_handler.data = OffsetPointer(allocation);
			memcpy(console_message.clickable_handler.data, clickable_handler.data, clickable_handler.data_size);
			allocation.size += clickable_handler.data_size;
		}

		message_allocator.FinishRequest(allocation.capacity);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::ChangeDumpPath(Stream<wchar_t> new_path)
	{
		if (dump_file != -1) {
			CloseFile(dump_file);
		}

		dump_path.Deallocate(StableAllocator());
		dump_path = StringCopy(StableAllocator(), new_path);

		// Open the file
		ECS_ASSERT(FileCreate(new_path, &dump_file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE) == ECS_FILE_STATUS_OK);

		Dump();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::AppendDump()
	{
		DUMP_OPERATION = CONSOLE_DUMP_APPEND;
		dump_flag.Signal();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::ClearOnPlay()
	{
		if (clear_on_play) {
			Clear();
		}
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Dump()
	{
		DUMP_OPERATION = CONSOLE_DUMP_ALL;
		dump_flag.Signal();
	}

	// -------------------------------------------------------------------------------------------------------
	
	bool Console::DumpToFile(Stream<wchar_t> path, bool write_as_binary)
	{
		ECS_FILE_HANDLE file_handle;
		ECS_FILE_ACCESS_FLAGS translation_mode = write_as_binary ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		ECS_FILE_STATUS_FLAGS status = FileCreate(path, &file_handle, ECS_FILE_ACCESS_WRITE_ONLY | translation_mode);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}
		else {
			bool success = ConsoleDump(file_handle);
			CloseFile(file_handle);
			return success;
		}
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Message(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, const ConsoleMessageOptions& options) {
		ConsoleMessage console_message;
		console_message.verbosity = options.verbosity;
		console_message.type = type;

		// Get the system string index if the message is different from nullptr.
		if (options.system.size > 0) {
			unsigned int system_index = FindString(options.system, system_filter_strings.ToStream());
			if (system_index != -1) {
				console_message.system_filter = (size_t)1 << (size_t)system_index;
			}
			else {
				console_message.system_filter = CONSOLE_SYSTEM_FILTER_ALL;
			}
		}
		else {
			console_message.system_filter = CONSOLE_SYSTEM_FILTER_ALL;
		}

		bool success = false;
		unsigned int write_index = messages.Request(success);
		if (success) {
			ConvertToMessage(message, options.clickable_action, console_message);
			messages[write_index] = console_message;
			messages.FinishRequest(write_index);
		}
		else {
			return;
		}

		if (dump_type == ECS_CONSOLE_DUMP_COUNT) {
			if (write_index > last_dumped_message) {
				if (write_index - last_dumped_message >= dump_count_for_commit) {
					AppendDump();
				}
			}
			else if (last_dumped_message == -1) {
				if (write_index + 1 >= dump_count_for_commit) {
					AppendDump();
				}
			}
			else {
				Dump();
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Info(Stream<char> message, const ConsoleMessageOptions& options) {
		Message(message, ECS_CONSOLE_INFO, options);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Warn(Stream<char> message, const ConsoleMessageOptions& options) {
		Message(message, ECS_CONSOLE_WARN, options);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Error(Stream<char> message, const ConsoleMessageOptions& options) {
		Message(message, ECS_CONSOLE_ERROR, options);
		if (pause_on_error) {
			if (on_error_trigger != nullptr) {
				on_error_trigger(on_error_trigger_data);
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Trace(Stream<char> message, const ConsoleMessageOptions& options) {
		Message(message, ECS_CONSOLE_TRACE, options);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Graphics(Stream<char> message, const ConsoleMessageOptions& options)
	{
		Message(message, ECS_CONSOLE_GRAPHICS, options);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::WriteFormatCharacters(CapacityStream<char>& characters)
	{
		Date current_date = OS::GetLocalTime();
		characters.AddAssert('[');
		ConvertDateToString(current_date, characters, format);
		characters.AddAssert(']');
		characters.AddAssert(' ');
		characters.AddAssert('\0');
		characters.size--;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::SetDumpType(ECS_CONSOLE_DUMP_TYPE type, unsigned int count)
	{
		dump_type = type;
		dump_count_for_commit = count;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::SetFormat(ECS_FORMAT_DATE_FLAGS _format)
	{
		format = _format;
		format_character_count = ConvertDateToStringMaxCharacterCount(format);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::SetVerbosity(unsigned char new_level)
	{
		verbosity_level = new_level;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::SetOnErrorTrigger(ConsoleOnErrorTrigger function, void* data, size_t data_size)
	{
		stable_allocator->DeallocateIfBelongs(on_error_trigger_data);
		on_error_trigger_data = CopyNonZero(stable_allocator, data, data_size);
		on_error_trigger = function;
	}

	// -------------------------------------------------------------------------------------------------------

	Console* GetConsole()
	{
		return &global_console;
	}

	// -------------------------------------------------------------------------------------------------------

	void SetConsole(MemoryManager* stable_allocator, AtomicStream<char> message_allocator, Stream<wchar_t> dump_path)
	{
		global_console = Console(stable_allocator, message_allocator, dump_path);
	}

	// -------------------------------------------------------------------------------------------------------
}