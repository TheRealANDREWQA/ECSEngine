#include "ecspch.h"
#include "Console.h"
#include "StringUtilities.h"
#include "Utilities.h"
#include "OSFunctions.h"
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
		const char header_annotation[] = "**********************************************\n";
		size_t annotation_size = std::size(header_annotation) - 1;
		success &= WriteFile(file, { header_annotation, annotation_size }, file_buffering);

		char time_characters[256];
		Stream<char> time_stream = Stream<char>(time_characters, 0);
		ConvertIntToChars(time_stream, current_date.hour);
		time_stream.Add(':');
		ConvertIntToChars(time_stream, current_date.minute);
		time_stream.Add(':');
		ConvertIntToChars(time_stream, current_date.seconds);
		time_stream.Add(':');
		ConvertIntToChars(time_stream, current_date.milliseconds);
		time_stream.Add(' ');
		ConvertIntToChars(time_stream, current_date.day);
		time_stream.Add('-');
		ConvertIntToChars(time_stream, current_date.month);
		time_stream.Add('-');
		ConvertIntToChars(time_stream, current_date.year);
		time_stream.Add(' ');
		const char description[] = "Console output\n";
		time_stream.AddStream(Stream<char>(description, std::size(description) - 1));

		success &= WriteFile(file, { time_characters, time_stream.size }, file_buffering);
		success &= WriteFile(file, { header_annotation, annotation_size }, file_buffering);

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

	MemoryManager DefaultConsoleStableAllocator(GlobalMemoryManager* global_manager)
	{
		return MemoryManager(ECS_KB * 512, ECS_KB * 4, ECS_KB * 512, GetAllocatorPolymorphic(global_manager));
	}

	// -------------------------------------------------------------------------------------------------------

	AtomicStream<char> DefaultConsoleMessageAllocator(GlobalMemoryManager* global_manager) {
		// Estimate a large count of characters per message
		const size_t CHARACTER_COUNT_PER_MESSAGE = 250;
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

		format = ECS_LOCAL_TIME_FORMAT_HOUR | ECS_LOCAL_TIME_FORMAT_MINUTES | ECS_LOCAL_TIME_FORMAT_SECONDS;
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
		size_t count = 0;
		count += HasFlag(format, ECS_LOCAL_TIME_FORMAT_MILLISECONDS) ? 4 : 0;
		count += HasFlag(format, ECS_LOCAL_TIME_FORMAT_SECONDS) ? 3 : 0;
		count += HasFlag(format, ECS_LOCAL_TIME_FORMAT_MINUTES) ? 3 : 0;
		count += HasFlag(format, ECS_LOCAL_TIME_FORMAT_HOUR) ? 2 : 0;
		count += HasFlag(format, ECS_LOCAL_TIME_FORMAT_DAY) ? 3 : 0;
		count += HasFlag(format, ECS_LOCAL_TIME_FORMAT_MONTH) ? 3 : 0;
		count += HasFlag(format, ECS_LOCAL_TIME_FORMAT_YEAR) ? 5 : 0;

		count += 3;
		return count;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::ConvertToMessage(Stream<char> message, ConsoleMessage& console_message)
	{
		size_t format_character_count = GetFormatCharacterCount();

		bool not_enough_space = false;
		Stream<char> allocation = message_allocator.Request(message.size + format_character_count + 1, not_enough_space);
		ECS_HARD_ASSERT(!not_enough_space, "Console message allocator ran out of space");

		allocation.size = 0;
		WriteFormatCharacters(allocation);
		console_message.client_message_start = allocation.size;

		memcpy(allocation.buffer + allocation.size, message.buffer, sizeof(char) * message.size);
		allocation.size += message.size;
		allocation[allocation.size] = '\0';
		console_message.message = allocation;
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

	void Console::Message(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, Stream<char> system, ECS_CONSOLE_VERBOSITY verbosity) {
		ConsoleMessage console_message;
		console_message.verbosity = verbosity;
		console_message.type = type;

		// Get the system string index if the message is different from nullptr.
		if (system.size > 0) {
			unsigned int system_index = FindString(system, Stream<Stream<char>>(system_filter_strings.buffer, system_filter_strings.size));
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
			ConvertToMessage(message, console_message);
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

	void Console::Info(Stream<char> message, Stream<char> system, ECS_CONSOLE_VERBOSITY verbosity) {
		Message(message, ECS_CONSOLE_INFO, system, verbosity);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Warn(Stream<char> message, Stream<char> system, ECS_CONSOLE_VERBOSITY verbosity) {
		Message(message, ECS_CONSOLE_WARN, system, verbosity);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Error(Stream<char> message, Stream<char> system, ECS_CONSOLE_VERBOSITY verbosity) {
		Message(message, ECS_CONSOLE_ERROR, system, verbosity);
		if (pause_on_error) {
			if (on_error_trigger != nullptr) {
				on_error_trigger(on_error_trigger_data);
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Trace(Stream<char> message, Stream<char> system, ECS_CONSOLE_VERBOSITY verbosity) {
		Message(message, ECS_CONSOLE_TRACE, system, verbosity);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::WriteFormatCharacters(Stream<char>& characters)
	{
		Date current_date = OS::GetLocalTime();
		characters.Add('[');
		ConvertDateToString(current_date, characters, format);
		characters.Add(']');
		characters.Add(' ');
		characters[characters.size] = '\0';
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::SetDumpType(ECS_CONSOLE_DUMP_TYPE type, unsigned int count)
	{
		dump_type = type;
		dump_count_for_commit = count;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::SetFormat(size_t _format)
	{
		format = _format;
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