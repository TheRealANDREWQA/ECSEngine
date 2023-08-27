#include "ecspch.h"
#include "Console.h"
#include "Function.h"
#include "OSFunctions.h"
#include "../ECS/World.h"

#define PENDING_MESSAGE_COUNT 512

#define FILE_BUFFERING_STACK_SIZE ECS_KB * 128
//#define ENABLE_PENDING_MESSAGES

namespace ECSEngine {

	Console global_console;

	// -------------------------------------------------------------------------------------------------------

	bool ConsoleAppendMessageToDump(ECS_FILE_HANDLE file, unsigned int index, Console* console, CapacityStream<void>& buffering) {
		// temporarly swap the \0 to \n in order to have it formatted correctly
		char* mutable_ptr = (char*)console->messages[index].message.buffer;
		mutable_ptr[console->messages[index].message.size] = '\n';
		bool success = WriteFile(file, { mutable_ptr, console->messages[index].message.size + 1 }, buffering);
		mutable_ptr[console->messages[index].message.size] = '\0';
		return success;
	}

	// This actually commits the pending messages - the console function also takes into account the dump thread
	// The pending messages stream is reset
	void ConsoleActualCommitPendingMessages(Console* console) {
		unsigned int pending_count = console->pending_messages.SpinWaitWrites();
		console->messages.Resize(console->messages.size + pending_count);
		console->messages.AddStream(console->pending_messages.ToStream());
		console->pending_messages.Reset();
	}

	// ------------------------------------------------------------------------------------

	struct ConsoleDumpData {
		unsigned int starting_index;
	};

	void ConsoleDump(unsigned int thread_index, World* world, void* _data) {
		ConsoleDumpData* data = (ConsoleDumpData*)_data;

		Console* console = GetConsole();

#ifdef ENABLE_PENDING_MESSAGES
		console->dump_lock.lock();
#else 
		console->commit_lock.lock();
#endif

#pragma region Header

		Date current_date = OS::GetLocalTime();

		ECS_STACK_VOID_STREAM(file_buffering, FILE_BUFFERING_STACK_SIZE);

		bool success = true;
		const char header_annotation[] = "**********************************************\n";
		size_t annotation_size = std::size(header_annotation) - 1;
		success &= WriteFile(console->dump_file, { header_annotation, annotation_size }, file_buffering);

		char time_characters[256];
		Stream<char> time_stream = Stream<char>(time_characters, 0);
		function::ConvertIntToChars(time_stream, current_date.hour);
		time_stream.Add(':');
		function::ConvertIntToChars(time_stream, current_date.minute);
		time_stream.Add(':');
		function::ConvertIntToChars(time_stream, current_date.seconds);
		time_stream.Add(':');
		function::ConvertIntToChars(time_stream, current_date.milliseconds);
		time_stream.Add(' ');
		function::ConvertIntToChars(time_stream, current_date.day);
		time_stream.Add('-');
		function::ConvertIntToChars(time_stream, current_date.month);
		time_stream.Add('-');
		function::ConvertIntToChars(time_stream, current_date.year);
		time_stream.Add(' ');
		const char description[] = "Console output\n";
		time_stream.AddStream(Stream<char>(description, std::size(description) - 1));

		success &= WriteFile(console->dump_file, { time_characters, time_stream.size }, file_buffering);
		success &= WriteFile(console->dump_file, { header_annotation, annotation_size }, file_buffering);

#pragma endregion

#pragma region Messages


#ifdef ENABLE_PENDING_MESSAGES
		// Now see if there are any pending messages - firstly check the commit lock
		if (data->console->commit_lock.is_locked()) {
			// Perform the actual commit
			ConsoleActualCommitPendingMessages(data->console);

			// Unlock the commit lock
			data->console->commit_lock.unlock();
		}
#endif

		for (unsigned int index = 0; index < console->messages.size && success; index++) {
			success &= ConsoleAppendMessageToDump(console->dump_file, index, console, file_buffering);
		}

		console->last_dumped_message = console->messages.size;

		if (file_buffering.size > 0 && success) {
			success = WriteFile(console->dump_file, file_buffering);
		}

		// Separated these for debugging
		bool write_success = success;
		if (!write_success) {
			__debugbreak();
		}
		bool flush_success = FlushFileToDisk(console->dump_file);
		
		success &= flush_success;
		ECS_ASSERT(success);

#ifdef ENABLE_PENDING_MESSAGES
		// Now add all the remaining pending messages
		//unsigned int previous_write_index = data->console->pending_messages.write_index.load(ECS_RELAXED);
		//while ()

		console->dump_lock.unlock();
#else
		console->commit_lock.unlock();
#endif

#pragma endregion
	
	}

	void ConsoleAppendToDump(unsigned int thread_index, World* world, void* _data)
	{
		Console* console = GetConsole();

#ifdef ENABLE_PENDING_MESSAGES
		console->dump_lock.lock();
#else
		console->allocator->Lock();
#endif
		
		ECS_STACK_VOID_STREAM(file_buffering, FILE_BUFFERING_STACK_SIZE);

		bool write_success = true;
		for (unsigned int index = console->last_dumped_message; index < console->messages.size && write_success; index++) {
			write_success &= ConsoleAppendMessageToDump(console->dump_file, index, console, file_buffering);
		}

		if (write_success && file_buffering.size > 0) {
			write_success = WriteFile(console->dump_file, file_buffering);
		}
		write_success &= FlushFileToDisk(console->dump_file);
		console->last_dumped_message = console->messages.size;

		ECS_ASSERT(write_success);

#ifdef ENABLE_PENDING_MESSAGES
		console->dump_lock.unlock();
#else
		console->allocator->Unlock();
#endif
	}

	// -------------------------------------------------------------------------------------------------------

	MemoryManager DefaultConsoleAllocator(GlobalMemoryManager* global_manager)
	{
		return MemoryManager(ECS_MB, ECS_KB * 16, ECS_MB, GetAllocatorPolymorphic(global_manager));
	}

	// -------------------------------------------------------------------------------------------------------

	Console::Console(MemoryManager* allocator, TaskManager* task_manager, Stream<wchar_t> _dump_path) : allocator(allocator),
		pause_on_error(false), verbosity_level(ECS_CONSOLE_VERBOSITY_DETAILED), dump_type(ECS_CONSOLE_DUMP_COUNT),
		task_manager(task_manager), last_dumped_message(0), dump_count_for_commit(1), dump_file(-1)
	{
		format = ECS_LOCAL_TIME_FORMAT_HOUR | ECS_LOCAL_TIME_FORMAT_MINUTES | ECS_LOCAL_TIME_FORMAT_SECONDS;
		messages.Initialize(GetAllocatorPolymorphic(allocator), 0);
		system_filter_strings.Initialize(GetAllocatorPolymorphic(allocator), 0);

		// Initialize the pending messages
		pending_messages.Initialize(allocator, PENDING_MESSAGE_COUNT);

		ChangeDumpPath(_dump_path);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Lock()
	{
		commit_lock.lock();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Clear()
	{
		// Wait for both commit and dump locks
		commit_lock.wait_locked();
		dump_lock.wait_locked();

		for (size_t index = 0; index < messages.size; index++) {
			allocator->Deallocate(messages[index].message.buffer);
		}
		messages.FreeBuffer();
		last_dumped_message = 0;
		Dump();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::AddSystemFilterString(Stream<char> string) {
		allocator->Lock();
		char* new_string = (char*)function::Copy(GetAllocatorPolymorphic(allocator), string.buffer, (string.size + 1) * sizeof(char), alignof(char));
		new_string[string.size] = '\0';
		system_filter_strings.Add(new_string);
		allocator->Unlock();
	}

	// -------------------------------------------------------------------------------------------------------

	size_t Console::GetFormatCharacterCount() const
	{
		size_t count = 0;
		count += function::HasFlag(format, ECS_LOCAL_TIME_FORMAT_MILLISECONDS) ? 4 : 0;
		count += function::HasFlag(format, ECS_LOCAL_TIME_FORMAT_SECONDS) ? 3 : 0;
		count += function::HasFlag(format, ECS_LOCAL_TIME_FORMAT_MINUTES) ? 3 : 0;
		count += function::HasFlag(format, ECS_LOCAL_TIME_FORMAT_HOUR) ? 2 : 0;
		count += function::HasFlag(format, ECS_LOCAL_TIME_FORMAT_DAY) ? 3 : 0;
		count += function::HasFlag(format, ECS_LOCAL_TIME_FORMAT_MONTH) ? 3 : 0;
		count += function::HasFlag(format, ECS_LOCAL_TIME_FORMAT_YEAR) ? 5 : 0;

		count += 3;
		return count;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::CommitPendingMessages()
	{
#ifdef ENABLE_PENDING_MESSAGES
#endif
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::ConvertToMessage(Stream<char> message, ConsoleMessage& console_message)
	{
		size_t format_character_count = GetFormatCharacterCount();

		void* allocation = allocator->Allocate(sizeof(char) * (message.size + format_character_count), alignof(char));

		Stream<char> stream = Stream<char>(allocation, 0);
		WriteFormatCharacters(stream);
		console_message.client_message_start = stream.size;

		memcpy((void*)((uintptr_t)allocation + stream.size), message.buffer, sizeof(char) * message.size);
		stream.size += message.size;
		console_message.message = stream;
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::ChangeDumpPath(Stream<wchar_t> new_path)
	{
		if (dump_file != -1) {
			CloseFile(dump_file);
		}

		dump_path.Deallocate(GetAllocatorPolymorphic(allocator));
		dump_path = function::StringCopy(GetAllocatorPolymorphic(allocator), new_path);

		// Open the file
		ECS_ASSERT(FileCreate(new_path, &dump_file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE) == ECS_FILE_STATUS_OK);

		Dump();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::AppendDump()
	{
		ThreadTask task = ECS_THREAD_TASK_NAME(ConsoleAppendToDump, nullptr, 0);
		task_manager->AddDynamicTaskAndWake(task);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Dump()
	{
		ConsoleDumpData dump_data;
		dump_data.starting_index = 0;

		ThreadTask task = ECS_THREAD_TASK_NAME(ConsoleDump, &dump_data, sizeof(dump_data));
		task_manager->AddDynamicTaskAndWake(task);
	}

	// -------------------------------------------------------------------------------------------------------
	
	void Console::Message(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, Stream<char> system, ECS_CONSOLE_VERBOSITY verbosity) {
		ConsoleMessage console_message;
		console_message.verbosity = verbosity;
		console_message.type = type;

		// Get the system string index if the message is different from nullptr.
		if (system.size > 0) {
			unsigned int system_index = function::FindString(system, Stream<Stream<char>>(system_filter_strings.buffer, system_filter_strings.size));
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

		// Wait while the commit lock is unlocked
#ifdef ENABLE_PENDING_MESSAGES
		commit_lock.wait_locked();

		// Request a location to write - if the index is valid go on
		unsigned int write_location = pending_messages.RequestInt(1);
		if (write_location >= pending_messages.capacity) {
			// Try to get the commit lock
			if (commit_lock.try_lock()) {
				// Commit the current pending messages
				CommitPendingMessages();
				commit_lock.unlock();
			}
			else {
				// Wait while the commit is happening
				commit_lock.wait_locked();
			}
			
			// Rerequest the location
			write_location = pending_messages.RequestInt(1);
		}	
		pending_messages[write_location] = console_message;
#else
		allocator->Lock();
		if (messages.size == messages.capacity && messages.capacity > MaxMessageCount()) {
			// Return - too many messages
			allocator->Unlock();
			return;
		}
		ConvertToMessage(message, console_message);
		messages.Add(console_message);
		allocator->Unlock();
#endif

		if (dump_type == ECS_CONSOLE_DUMP_COUNT) {
			unsigned int current_total_count = messages.size;
#ifdef ENABLE_PENDING_MESSAGES
			current_total_count += pending_messages.size.load(ECS_RELAXED);
#endif

			if (current_total_count > last_dumped_message) {
				if (current_total_count - last_dumped_message >= dump_count_for_commit) {
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
			__debugbreak();
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
		function::ConvertDateToString(current_date, characters, format);
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

	Console* GetConsole()
	{
		return &global_console;
	}

	// -------------------------------------------------------------------------------------------------------

	void SetConsole(MemoryManager* allocator, TaskManager* task_manager, const wchar_t* dump_path)
	{
		global_console = Console(allocator, task_manager, dump_path);
	}

	// -------------------------------------------------------------------------------------------------------
}