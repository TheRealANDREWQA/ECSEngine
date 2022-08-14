#include "ecspch.h"
#include "Console.h"
#include "Function.h"
#include "OSFunctions.h"
#include "../Internal/World.h"

namespace ECSEngine {

	Console global_console;

	// -------------------------------------------------------------------------------------------------------

	bool ConsoleAppendMessageToDump(ECS_FILE_HANDLE file, unsigned int index, Console* console) {
		// temporarly swap the \0 to \n in order to have it formatted correctly
		char* mutable_ptr = (char*)console->messages[index].message.buffer;
		mutable_ptr[console->messages[index].message.size] = '\n';
		bool success = WriteFile(file, { mutable_ptr, console->messages[index].message.size + 1 });
		mutable_ptr[console->messages[index].message.size] = '\0';
		return success;
	}

	// ------------------------------------------------------------------------------------

	void ConsoleDump(unsigned int thread_index, World* world, void* _data) {
		ConsoleDumpData* data = (ConsoleDumpData*)_data;
		ECS_FILE_HANDLE file = 0;
		ECS_FILE_STATUS_FLAGS status = FileCreate(data->console->dump_path, &file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE);

		ECS_ASSERT(status == ECS_FILE_STATUS_OK);

		data->console->dump_lock.lock();

#pragma region Header

		SYSTEMTIME system_time;
		GetLocalTime(&system_time);

		bool success = true;
		const char header_annotation[] = "**********************************************\n";
		size_t annotation_size = std::size(header_annotation) - 1;
		success &= WriteFile(file, { header_annotation, annotation_size });

		char time_characters[256];
		Stream<char> time_stream = Stream<char>(time_characters, 0);
		function::ConvertIntToChars(time_stream, system_time.wHour);
		time_stream.Add(':');
		function::ConvertIntToChars(time_stream, system_time.wMinute);
		time_stream.Add(':');
		function::ConvertIntToChars(time_stream, system_time.wSecond);
		time_stream.Add(':');
		function::ConvertIntToChars(time_stream, system_time.wMilliseconds);
		time_stream.Add(' ');
		function::ConvertIntToChars(time_stream, system_time.wDay);
		time_stream.Add('-');
		function::ConvertIntToChars(time_stream, system_time.wMonth);
		time_stream.Add('-');
		function::ConvertIntToChars(time_stream, system_time.wYear);
		time_stream.Add(' ');
		const char description[] = "Console output\n";
		time_stream.AddStream(Stream<char>(description, std::size(description) - 1));

		success &= WriteFile(file, { time_characters, time_stream.size });
		success &= WriteFile(file, { header_annotation, annotation_size });

#pragma endregion

#pragma region Messages

		for (size_t index = 0; index < data->console->messages.size && success; index++) {
			success &= ConsoleAppendMessageToDump(file, index, data->console);
		}

		data->console->dump_lock.unlock();
		data->console->last_dumped_message = data->console->messages.size;

#pragma endregion

		CloseFile(file);
		ECS_ASSERT(success);
	}

	void ConsoleAppendToDump(unsigned int thread_index, World* world, void* _data)
	{
		ConsoleDumpData* data = (ConsoleDumpData*)_data;
		ECS_FILE_HANDLE file = 0;
		ECS_FILE_STATUS_FLAGS status = OpenFile(data->console->dump_path, &file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_APEND | ECS_FILE_ACCESS_TEXT);

		ECS_ASSERT(status == ECS_FILE_STATUS_OK);

		data->console->dump_lock.lock();

		bool write_success = true;
		for (size_t index = data->starting_index; index < data->console->messages.size && write_success; index++) {
			write_success &= ConsoleAppendMessageToDump(file, index, data->console);
		}
		data->console->last_dumped_message += data->console->messages.size - data->starting_index;

		data->console->dump_lock.unlock();
		CloseFile(file);
		ECS_ASSERT(write_success);
	}

	// -------------------------------------------------------------------------------------------------------

	MemoryManager DefaultConsoleAllocator(GlobalMemoryManager* global_manager)
	{
		return MemoryManager(500'000, 1024, 500'000, global_manager);
	}

	// -------------------------------------------------------------------------------------------------------

	Console::Console(MemoryManager* allocator, TaskManager* task_manager, Stream<wchar_t> _dump_path) : allocator(allocator),
		pause_on_error(false), verbosity_level(ECS_CONSOLE_VERBOSITY_DETAILED), dump_type(ECS_CONSOLE_DUMP_COUNT),
		task_manager(task_manager), last_dumped_message(0), dump_count_for_commit(1) {
		format = ECS_LOCAL_TIME_FORMAT_HOUR | ECS_LOCAL_TIME_FORMAT_MINUTES | ECS_LOCAL_TIME_FORMAT_SECONDS;
		messages.Initialize(GetAllocatorPolymorphic(allocator), 0);
		system_filter_strings.Initialize(GetAllocatorPolymorphic(allocator), 0);

		// Make a stable reference to the dump path
		dump_path = function::StringCopy(GetAllocatorPolymorphic(allocator), _dump_path);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Lock()
	{
		lock.lock();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Clear()
	{
		for (size_t index = 0; index < messages.size; index++) {
			allocator->Deallocate(messages[index].message.buffer);
		}
		messages.FreeBuffer();
		last_dumped_message = 0;
		Dump();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::AddSystemFilterString(Stream<char> string) {
		char* new_string = (char*)function::Copy(messages.allocator, string.buffer, (string.size + 1) * sizeof(char), alignof(char));
		new_string[string.size] = '\0';
		system_filter_strings.Add(new_string);
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
		allocator->Deallocate(dump_path.buffer);
		dump_path = function::StringCopy(GetAllocatorPolymorphic(allocator), new_path);
		Dump();
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::AppendDump()
	{
		ConsoleDumpData dump_data;
		dump_data.console = this;
		dump_data.starting_index = last_dumped_message;

		ThreadTask task = ECS_THREAD_TASK_NAME(ConsoleAppendToDump, &dump_data, sizeof(dump_data));
		task_manager->AddDynamicTaskAndWake(task);
	}

	// -------------------------------------------------------------------------------------------------------

	void Console::Dump()
	{
		ConsoleDumpData dump_data;
		dump_data.console = this;
		dump_data.starting_index = 0;

		ThreadTask task = ECS_THREAD_TASK_NAME(ConsoleDump, &dump_data, sizeof(dump_data));
		task_manager->AddDynamicTaskAndWake(task);
	}

	// -------------------------------------------------------------------------------------------------------
	
	void Console::Message(Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE type, Stream<char> system, ECS_CONSOLE_VERBOSITY verbosity) {
		ConsoleMessage console_message;
		console_message.verbosity = verbosity;
		console_message.type = type;

		ConvertToMessage(message, console_message);

		// Get the system string index if the message is different from nullptr.
		if (system.size > 0) {
			unsigned int system_index = function::FindString(system, Stream<Stream<char>>(system_filter_strings.buffer, system_filter_strings.size));
			if (system_index != -1) {
				console_message.system_filter = 1 << system_index;
			}
			else {
				console_message.system_filter = CONSOLE_SYSTEM_FILTER_ALL;
			}
		}
		else {
			console_message.system_filter = CONSOLE_SYSTEM_FILTER_ALL;
		}

		lock.lock();
		messages.Add(console_message);
		lock.unlock();
		if (dump_type == ECS_CONSOLE_DUMP_COUNT) {
			if (messages.size > last_dumped_message) {
				if (messages.size - last_dumped_message >= dump_count_for_commit) {
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