#pragma once
#include "UIDrawer.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"

#define ECS_TOOLS using namespace ECSEngine::Tools
constexpr const char* ECS_TOOLS_UI_ERROR_MESSAGE_WINDOW_NAME = "Error Message";
constexpr const char* ECS_TOOLS_UI_CONFIRM_WINDOW_NAME = "Confirm Window";
constexpr const char* ECS_TOOLS_UI_CHOOSE_WINDOW_NAME = "Choose Window";


namespace ECSEngine {

	struct TaskManager;

	namespace Tools {

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterReturnToDefaultButton(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void WindowParameterColorTheme(UIWindowDrawerDescriptor* descriptor, UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void WindowParametersLayout(UIWindowDrawerDescriptor* descriptor, UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void WindowParametersElementDescriptor(UIWindowDrawerDescriptor* descriptor, UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void WindowParameterDraw(void* window_data, void* drawer_descriptor);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void OpenWindowParameters(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParametersColorTheme(UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParametersLayout(UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParametersElementDescriptor(UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParameterFont(UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParameterDockspace(UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParameterMaterial(UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParameterMiscellaneous(UIDrawer<initialize>& drawer);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void SystemParametersDraw(void* window_data, void* drawer_descriptor);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void OpenSystemParameters(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void CenterWindowDescriptor(UIWindowDescriptor& descriptor, float2 size);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowHandler(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void DrawNothing(void* window_data, void* drawer_descriptor);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void ErrorMessageWindowDraw(void* window_data, void* drawer_descriptor);

		ECSENGINE_API unsigned int CreateErrorMessageWindow(UISystem* system, const char* description);

		ECSENGINE_API unsigned int CreateErrorMessageWindow(UISystem* system, Stream<char> description);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void ConsoleWindowDraw(void* window_data, void* drawer_descriptor);

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		ECSENGINE_API void ConfirmWindowDraw(void* window_data, void* drawer_descriptor);

		ECSENGINE_API unsigned int CreateConfirmWindow(UISystem* system, Stream<char> description, UIActionHandler handler);

		ECSENGINE_API unsigned int CreateConfirmWindow(UISystem* ECS_RESTRICT system, const char* ECS_RESTRICT description, UIActionHandler handler);

		// --------------------------------------------------------------------------------------------------------------

		struct ChooseOptionWindowData {
			Stream<UIActionHandler> handlers;
			const char** button_names;
			Stream<char> description;
		};

		ECSENGINE_API unsigned int CreateChooseOptionWindow(UISystem* system, ChooseOptionWindowData data);

		template<bool initialize>
		ECSENGINE_API void ChooseOptionWindowDraw(void* window_data, void* drawer_descriptor);

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

		template<bool initialize>
		ECSENGINE_API void TextInputWizard(void* window_data, void* drawer_descriptor);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API unsigned int CreateTextInputWizard(const TextInputWizardData* data, UISystem* system);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API void CreateTextInputWizardAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DrawTextFile(UIDrawer<false>* drawer, const wchar_t* path, float2 border_padding, float next_row_y_offset);

		// Adds stabilization but the render span must be updated manually at the end of the file 
		ECSENGINE_API float2* DrawTextFileEx(UIDrawer<false>* drawer, const wchar_t* path, float2 border_padding, float next_row_y_offset);

		struct ECSENGINE_API TextFileDrawData {
			const wchar_t* path;
			float2 border_padding = { 0.01f, 0.01f };
			float next_row_y_offset = 0.01f;
		};

		template<bool initialize>
		ECSENGINE_API void TextFileDraw(void* window_data, void* drawer_descriptor);

		ECSENGINE_API unsigned int CreateTextFileWindow(TextFileDrawData data, UISystem* system, const char* window_name);
		
		struct ECSENGINE_API TextFileDrawActionData {
			TextFileDrawData draw_data;
			const char* window_name;
		};

		ECSENGINE_API void CreateTextFileWindowAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		constexpr const char* CONSOLE_WINDOW_NAME = "Console";

		constexpr size_t CONSOLE_YEAR = 1 << 0;
		constexpr size_t CONSOLE_MONTH = 1 << 1;
		constexpr size_t CONSOLE_DAY = 1 << 2;
		constexpr size_t CONSOLE_HOUR = 1 << 3;
		constexpr size_t CONSOLE_MINUTES = 1 << 4;
		constexpr size_t CONSOLE_SECONDS = 1 << 5;
		constexpr size_t CONSOLE_MILLISECONDS = 1 << 6;

		constexpr size_t CONSOLE_APPEREANCE_TABLE_COUNT = 256;

		#define CONSOLE_INFO_COLOR Color(40, 170, 50)
		#define CONSOLE_WARN_COLOR Color(120, 130, 30)
		#define CONSOLE_ERROR_COLOR Color(160, 20, 20)
		#define CONSOLE_TRACE_COLOR Color(140, 30, 120)

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
			containers::ResizableStream<ConsoleMessage, MemoryManager> messages;
			containers::ResizableStream<const char*, MemoryManager> system_filter_strings;
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

		ECSENGINE_API void ConsoleAppendMessageToDump(std::ofstream& stream, unsigned int index, Console* console);

		// Thread task
		ECSENGINE_API void ConsoleDump(unsigned int thread_index, World* world, void* data);
		
		// Thread task
		ECSENGINE_API void ConsoleAppendToDump(unsigned int thread_index, World* world, void* data);

		using ConsoleUIHashFunction = HashFunctionAdditiveString;

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
			containers::ResizableStream<unsigned int, MemoryManager> filtered_message_indices;
			containers::IdentifierHashTable<UniqueConsoleMessage, ResourceIdentifier, HashFunctionPowerOfTwo> unique_messages;
			bool* system_filter;
		};

		ECSENGINE_API size_t GetSystemMaskFromConsoleWindowData(const ConsoleWindowData* data);

		ECSENGINE_API unsigned int CreateConsoleWindow(UISystem* system, Console* console);

		ECSENGINE_API void CreateConsole(UISystem* system, Console* console);

		ECSENGINE_API void CreateConsoleAction(ActionData* action_data);

		ECSENGINE_API void CreateConsoleWindowData(ConsoleWindowData& data, Console* console);

	}

}