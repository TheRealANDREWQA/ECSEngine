#pragma once
#include "../../Internal/Multithreading/ConcurrentPrimitives.h"
#include "UIReflection.h"
#include "UIResourcePaths.h"
#include "../../Utilities/Console.h"

#define ECS_TOOLS using namespace ECSEngine::Tools
constexpr const char* ECS_TOOLS_UI_ERROR_MESSAGE_WINDOW_NAME = "Error Message";
constexpr const char* ECS_TOOLS_UI_CONFIRM_WINDOW_NAME = "Confirm Window";
constexpr const char* ECS_TOOLS_UI_CHOOSE_WINDOW_NAME = "Choose Window";


namespace ECSEngine {

	struct TaskManager;

	namespace Tools {

		struct UIDrawer;

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

		ECSENGINE_API unsigned int CreateErrorMessageWindow(UISystem* system, Stream<char> description);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ConfirmWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

		ECSENGINE_API unsigned int CreateConfirmWindow(UISystem* system, Stream<char> description, UIActionHandler handler);

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
			CapacityStream<char> input_stream = { nullptr, 0, 0 }; // Does not need to be initialized - it is used internally
		};

		ECSENGINE_API void TextInputWizard(void* window_data, void* drawer_descriptor, bool initialize);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API unsigned int CreateTextInputWizard(const TextInputWizardData* data, UISystem* system);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API void CreateTextInputWizardAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DrawTextFile(UIDrawer* drawer, Stream<wchar_t> path, float2 border_padding, float next_row_y_offset);

		ECSENGINE_API void DrawTextFileEx(UIDrawer* drawer, Stream<wchar_t> path, float2 border_padding, float next_row_y_offset);

		struct ECSENGINE_API TextFileDrawData {
			const wchar_t* path;
			float2 border_padding = { 0.01f, 0.01f };
			float next_row_y_offset = 0.01f;
		};

		ECSENGINE_API void TextFileDraw(void* window_data, void* drawer_descriptor, bool initialize);

		ECSENGINE_API unsigned int CreateTextFileWindow(TextFileDrawData data, UISystem* system, Stream<char> window_name);
		
		struct ECSENGINE_API TextFileDrawActionData {
			TextFileDrawData draw_data;
			Stream<char> window_name;
		};

		ECSENGINE_API void CreateTextFileWindowAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ConsoleWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

		#define CONSOLE_WINDOW_NAME "Console"	

		struct ECSENGINE_API UniqueConsoleMessage {
			ConsoleMessage message;
			unsigned int counter;
		};

		struct ECSENGINE_API ConsoleWindowData {
			ECSEngine::Console* console;
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
			bool system_filter_changed;
			unsigned char previous_verbosity_level;

			unsigned int last_frame_message_count;
			unsigned int info_count;
			unsigned int warn_count;
			unsigned int error_count;
			unsigned int trace_count;
			ResizableStream<unsigned int> filtered_message_indices;
			HashTableDefault<UniqueConsoleMessage> unique_messages;
			Stream<bool> system_filter;
		};

		ECSENGINE_API void ConsoleFilterMessages(ConsoleWindowData* data, UIDrawer& drawer);

		ECSENGINE_API size_t GetSystemMaskFromConsoleWindowData(const ConsoleWindowData* data);

		ECSENGINE_API unsigned int CreateConsoleWindow(UISystem* system);

		ECSENGINE_API void CreateConsole(UISystem* system);

		ECSENGINE_API void CreateConsoleAction(ActionData* action_data);

		ECSENGINE_API void CreateConsoleWindowData(ConsoleWindowData& data);

		// --------------------------------------------------------------------------------------------------------------

		struct InjectWindowElement {
			void* data;
			Stream<char> name;
			Stream<char> basic_type_string;
			Reflection::ReflectionStreamFieldType stream_type = Reflection::ReflectionStreamFieldType::Basic;
			// It is used to express the initial capacity of non capacity streams
			unsigned int stream_size = 0;
		};

		struct InjectWindowSection {
			Stream<InjectWindowElement> elements;
			Stream<char> name;
		};

		struct InjectWindowData {
			Stream<InjectWindowSection> sections;
			UIReflectionDrawer* ui_reflection;
		};

		ECSENGINE_API void InjectValuesWindowDraw(void* drawer_descriptor, void* window_data, bool initialize);

		ECSENGINE_API unsigned int CreateInjectValuesWindow(UISystem* system, InjectWindowData data, Stream<char> window_name, bool is_pop_up_window = true);
		
		struct InjectValuesActionData {
			InjectWindowData data;
			Stream<char> name;
			bool is_pop_up_window;
		};

		ECSENGINE_API void CreateInjectValuesAction(ActionData* action_data);

		ECSENGINE_API void InjectWindowDestroyAction(ActionData* action_data);

	}

}