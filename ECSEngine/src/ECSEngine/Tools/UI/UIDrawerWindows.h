#pragma once
#include "UIDrawer.h"
#include "../../Internal/Multithreading/ConcurrentPrimitives.h"
#include "UIReflection.h"
#include "UIResourcePaths.h"

#define ECS_TOOLS using namespace ECSEngine::Tools
constexpr const char* ECS_TOOLS_UI_ERROR_MESSAGE_WINDOW_NAME = "Error Message";
constexpr const char* ECS_TOOLS_UI_CONFIRM_WINDOW_NAME = "Confirm Window";
constexpr const char* ECS_TOOLS_UI_CHOOSE_WINDOW_NAME = "Choose Window";


namespace ECSEngine {

	struct TaskManager;

	namespace Tools {

		constexpr const wchar_t* CONSOLE_TEXTURE_ICONS[] = {
			ECS_TOOLS_UI_TEXTURE_INFO_ICON,
			ECS_TOOLS_UI_TEXTURE_WARN_ICON,
			ECS_TOOLS_UI_TEXTURE_ERROR_ICON,
			ECS_TOOLS_UI_TEXTURE_TRACE_ICON
		};

#define CONSOLE_INFO_COLOR Color(40, 170, 50)
#define CONSOLE_WARN_COLOR Color(120, 130, 30)
#define CONSOLE_ERROR_COLOR Color(160, 20, 20)
#define CONSOLE_TRACE_COLOR Color(140, 30, 120)

		const Color CONSOLE_COLORS[] = {
			CONSOLE_INFO_COLOR,
			CONSOLE_WARN_COLOR,
			CONSOLE_ERROR_COLOR,
			CONSOLE_TRACE_COLOR
		};

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterReturnToDefaultButton(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterColorTheme(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParametersLayout(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParametersElementDescriptor(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterDraw(void* window_data, void* drawer_descriptor, bool initializer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void OpenWindowParameters(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParametersColorTheme(UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParametersLayout(UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParametersElementDescriptor(UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterFont(UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterDockspace(UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterMaterial(UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterMiscellaneous(UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParametersDraw(void* window_data, void* drawer_descriptor, bool initializer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void OpenSystemParameters(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void CenterWindowDescriptor(UIWindowDescriptor& descriptor, float2 size);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowHandler(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DrawNothing(void* window_data, void* drawer_descriptor, bool initialize);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ErrorMessageWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

		ECSENGINE_API unsigned int CreateErrorMessageWindow(UISystem* system, const char* description);

		ECSENGINE_API unsigned int CreateErrorMessageWindow(UISystem* system, Stream<char> description);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ConfirmWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

		ECSENGINE_API unsigned int CreateConfirmWindow(UISystem* system, Stream<char> description, UIActionHandler handler);

		ECSENGINE_API unsigned int CreateConfirmWindow(UISystem* ECS_RESTRICT system, const char* ECS_RESTRICT description, UIActionHandler handler);

		// --------------------------------------------------------------------------------------------------------------

		struct ChooseOptionWindowData {
			Stream<UIActionHandler> handlers;
			const char** button_names;
			Stream<char> description;
		};

		ECSENGINE_API unsigned int CreateChooseOptionWindow(UISystem* system, ChooseOptionWindowData data);

		ECSENGINE_API void ChooseOptionWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

		// --------------------------------------------------------------------------------------------------------------

		// The callback receives the char stream through the additional_data parameter
		struct ECSENGINE_API TextInputWizardData {
			const char* input_name;
			const char* window_name;
			Action callback;
			void* callback_data;
			size_t callback_data_size;
			CapacityStream<char> input_stream;
		};

		ECSENGINE_API void TextInputWizard(void* window_data, void* drawer_descriptor, bool initialize);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API unsigned int CreateTextInputWizard(const TextInputWizardData* data, UISystem* system);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API void CreateTextInputWizardAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DrawTextFile(UIDrawer* drawer, const wchar_t* path, float2 border_padding, float next_row_y_offset);

		ECSENGINE_API void DrawTextFileEx(UIDrawer* drawer, const wchar_t* path, float2 border_padding, float next_row_y_offset);

		struct ECSENGINE_API TextFileDrawData {
			const wchar_t* path;
			float2 border_padding = { 0.01f, 0.01f };
			float next_row_y_offset = 0.01f;
		};

		ECSENGINE_API void TextFileDraw(void* window_data, void* drawer_descriptor, bool initialize);

		ECSENGINE_API unsigned int CreateTextFileWindow(TextFileDrawData data, UISystem* system, const char* window_name);
		
		struct ECSENGINE_API TextFileDrawActionData {
			TextFileDrawData draw_data;
			const char* window_name;
		};

		ECSENGINE_API void CreateTextFileWindowAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API MemoryManager DefaultConsoleAllocator(GlobalMemoryManager* global_manager);

		ECSENGINE_API void ConsoleWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

		constexpr const char* CONSOLE_WINDOW_NAME = "Console";
		constexpr size_t CONSOLE_APPEREANCE_TABLE_COUNT = 256;

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

		struct ECSENGINE_API ConsoleMessage {
			Stream<char> message;
			Color color;
			unsigned char client_message_start;
			ConsoleMessageType icon_type = ConsoleMessageType::None;
			unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL;
			size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL;
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

			void Message(const char* message, ConsoleMessageType type, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void Message(Stream<char> message, ConsoleMessageType type, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void MessageAtomic(const char* message, ConsoleMessageType type, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void MessageAtomic(Stream<char> message, ConsoleMessageType type, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

			void Write(const char* message, ConsoleMessage& console_message);
			void Write(Stream<char> message, ConsoleMessage& console_message);
			void WriteAtomic(const char* message, ConsoleMessage& console_message);
			void WriteAtomic(Stream<char> message, ConsoleMessage& console_message);

			void Info(const char* message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void Info(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void InfoAtomic(const char* message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void InfoAtomic(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

			void Warn(const char* message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void Warn(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void WarnAtomic(const char* message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void WarnAtomic(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

			void Error(const char* message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void Error(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void ErrorAtomic(const char* mssage, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void ErrorAtomic(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

			void Trace(const char* message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void Trace(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void TraceAtomic(const char* message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);
			void TraceAtomic(Stream<char> message, size_t system_filter = CONSOLE_SYSTEM_FILTER_ALL, unsigned char verbosity = CONSOLE_VERBOSITY_MINIMAL);

			void WriteFormatCharacters(Stream<char>& characters);

			void Unlock();
			void TryLock();

			void SetDumpType(ConsoleDumpType type, unsigned int count = 1);

			void SetFormat(size_t format);
			void SetVerbosity(unsigned char new_level);

			MemoryManager* allocator;
			TaskManager* task_manager;
			containers::ResizableStream<ConsoleMessage> messages;
			containers::ResizableStream<const char*> system_filter_strings;
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

		struct ECSENGINE_API ConsoleDumpData {
			Console* console;
			unsigned int starting_index;
		};

		ECSENGINE_API bool ConsoleAppendMessageToDump(ECS_FILE_HANDLE file, unsigned int index, Console* console);

		// Thread task
		ECSENGINE_API void ConsoleDump(unsigned int thread_index, World* world, void* data);
		
		// Thread task
		ECSENGINE_API void ConsoleAppendToDump(unsigned int thread_index, World* world, void* data);

		using ConsoleUIHashFunction = HashFunctionMultiplyString;

		struct ECSENGINE_API UniqueConsoleMessage {
			ConsoleMessage message;
			unsigned int counter;
		};

		struct ECSENGINE_API ConsoleWindowData {
			Console* console;
			bool collapse;
			bool clear_on_play;

			struct {
				bool filter_info;
				bool filter_warn;
				bool filter_error;
				bool filter_trace;
				// acts as padding
				bool filter_all;
			};
			bool filter_message_type_changed;

			unsigned int last_frame_message_count;
			unsigned int info_count;
			unsigned int warn_count;
			unsigned int error_count;
			unsigned int trace_count;
			bool system_filter_changed;
			unsigned char previous_verbosity_level;
			containers::ResizableStream<unsigned int> filtered_message_indices;
			containers::HashTable<UniqueConsoleMessage, ResourceIdentifier, HashFunctionPowerOfTwo, UIHash> unique_messages;
			bool* system_filter;
		};

		ECSENGINE_API void ConsoleFilterMessages(ConsoleWindowData* data, UIDrawer& drawer);

		ECSENGINE_API size_t GetSystemMaskFromConsoleWindowData(const ConsoleWindowData* data);

		ECSENGINE_API unsigned int CreateConsoleWindow(UISystem* system, Console* console);

		ECSENGINE_API void CreateConsole(UISystem* system, Console* console);

		ECSENGINE_API void CreateConsoleAction(ActionData* action_data);

		ECSENGINE_API void CreateConsoleWindowData(ConsoleWindowData& data, Console* console);

		struct InjectWindowElement {
			void* data;
			const char* name;
			const char* basic_type_string;
			Reflection::ReflectionStreamFieldType stream_type = Reflection::ReflectionStreamFieldType::Basic;
			// It is used to express the capacity of the stream for stream types other than capacity stream
			unsigned int stream_capacity = 0;
			// It is used to express the initial capacity of non capacity streams
			unsigned int stream_size = 0;
		};

		struct InjectWindowSection {
			containers::Stream<InjectWindowElement> elements;
			const char* name;
		};

		struct InjectWindowData {
			containers::Stream<InjectWindowSection> sections;
			UIReflectionDrawer* ui_reflection;
		};

		ECSENGINE_API void InjectValuesWindowDraw(void* drawer_descriptor, void* window_data, bool initialize);

		ECSENGINE_API unsigned int CreateInjectValuesWindow(UISystem* system, InjectWindowData data, const char* window_name, bool is_pop_up_window = true);
		
		struct InjectValuesActionData {
			InjectWindowData data;
			const char* name;
			bool is_pop_up_window;
		};

		ECSENGINE_API void CreateInjectValuesAction(ActionData* action_data);

		ECSENGINE_API void InjectWindowDestroyAction(ActionData* action_data);

	}

}