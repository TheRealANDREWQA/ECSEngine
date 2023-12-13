#pragma once
#include "../../Multithreading/ConcurrentPrimitives.h"
#include "UIReflection.h"
#include "UIResourcePaths.h"
#include "../../Utilities/Console.h"
#include "../../Rendering/VisualizeTexture.h"

#define ECS_TOOLS using namespace ECSEngine::Tools
constexpr const char* ECS_TOOLS_UI_ERROR_MESSAGE_WINDOW_NAME = "Error Message";
constexpr const char* ECS_TOOLS_UI_CONFIRM_WINDOW_NAME = "Confirm Option";
constexpr const char* ECS_TOOLS_UI_CHOOSE_WINDOW_NAME = "Choose Option";


namespace ECSEngine {

	struct TaskManager;

	namespace Tools {

		struct UIDrawer;
		struct UIDrawerRowLayout;

		constexpr const wchar_t* CONSOLE_TEXTURE_ICONS[] = {
			ECS_TOOLS_UI_TEXTURE_INFO_ICON,
			ECS_TOOLS_UI_TEXTURE_WARN_ICON,
			ECS_TOOLS_UI_TEXTURE_ERROR_ICON,
			ECS_TOOLS_UI_TEXTURE_TRACE_ICON,
			ECS_TOOLS_UI_TEXTURE_GRAPHICS_ICON
		};

		static_assert(std::size(CONSOLE_TEXTURE_ICONS) == ECS_CONSOLE_MESSAGE_COUNT);

#define CONSOLE_INFO_COLOR Color(40, 170, 50)
#define CONSOLE_WARN_COLOR Color(120, 130, 30)
#define CONSOLE_ERROR_COLOR Color(160, 20, 20)
#define CONSOLE_TRACE_COLOR Color(140, 30, 120)
#define CONSOLE_GRAPHICS_COLOR Color(240, 240, 240)

		const Color CONSOLE_COLORS[] = {
			CONSOLE_INFO_COLOR,
			CONSOLE_WARN_COLOR,
			CONSOLE_ERROR_COLOR,
			CONSOLE_TRACE_COLOR,
			CONSOLE_GRAPHICS_COLOR
		};

		static_assert(std::size(CONSOLE_COLORS) == ECS_CONSOLE_MESSAGE_COUNT);

#pragma region General Additional Draw

		// Decide if it is worth the time implementing such a feature

		//// For all GET reaons, the additional data will contain a row layout that can be used to fill in the transforms that this
		//// additional draw wants to do alongside
		//// For ROW_DRAW, the data will contain an index of the current row that is being drawn
		//// For ACTION_BEFORE happens before the the window draws anything, ACTION_AFTER after all drawing has finished
		//// THe STATE_ELEMENTS reason is called at the beginning to determine which elements are covered
		//enum UIAdditionalDrawReason : unsigned char {
		//	ECS_UI_ADDITIONAL_DRAW_STATE_ELEMENTS,
		//	ECS_UI_ADDITIONAL_DRAW_HEADER_GET,
		//	ECS_UI_ADDITIONAL_DRAW_HEADER_DRAW,
		//	ECS_UI_ADDITIONAL_DRAW_TOP_GET,
		//	ECS_UI_ADDITIONAL_DRAW_TOP_DRAW,
		//	ECS_UI_ADDITIONAL_DRAW_BOTTOM_GET,
		//	ECS_UI_ADDITIONAL_DRAW_BOTTOM_DRAW,
		//	ECS_UI_ADDITIONAL_DRAW_NORMAL_GET,
		//	ECS_UI_ADDITIONAL_DRAW_NORMAL_DRAW,
		//	ECS_UI_ADDITIONAL_DRAW_ROW_GET,
		//	ECS_UI_ADDITIONAL_DRAW_ROW_DRAW,
		//	ECS_UI_ADDITIONAL_DRAW_WHOLE_SCREEN_BEFORE,
		//	ECS_UI_ADDITIONAL_DRAW_WHOLE_SCREEN_AFTER,
		//	ECS_UI_ADDITIONAL_DRAW_ACTION_BEFORE,
		//	ECS_UI_ADDITIONAL_DRAW_ACTION_AFTER
		//};

		//enum UIAdditionalParentDrawReason : unsigned char {
		//	ECS_UI_ADDITIONAL_PARENT_DRAW_HEADER,
		//	ECS_UI_ADDITIONAL_PARENT_DRAW_TOP,
		//	ECS_UI_ADDITIONAL_PARENT_DRAW_BOTTOM,
		//	ECS_UI_ADDITIONAL_PARENT_DRAW_NORMAL,
		//	ECS_UI_ADDITIONAL_PARENT_DRAW_ROW,
		//	ECS_UI_ADDITIONAL_PARENT_DRAW_WHOLE_SCREEN
		//};

		//// This is used to control where the element is going to be rendered
		//// With respect to the parent
		//enum UIAdditionalDrawOrder : unsigned char {
		//	ECS_UI_ADDITIONAL_DRAW_LEFT_BEFORE,
		//	ECS_UI_ADDITIONAL_DRAW_LEFT_AFTER,
		//	ECS_UI_ADDITIONAL_DRAW_RIGHT_BEFORE,
		//	ECS_UI_ADDITIONAL_DRAW_RIGHT_AFTER,
		//	ECS_UI_ADDITIONAL_DRAW_NORMAL_BEFORE,
		//	ECS_UI_ADDITIONAL_DRAW_NORMAL_AFTER,
		//	ECS_UI_ADDITIONAL_DRAW_MIDDLE_BEFORE,
		//	ECS_UI_ADDITIONAL_DRAW_MIDDLE_AFTER
		//};

		//struct UIAdditionalDrawElement {
		//	UIAdditionalDrawReason reason;
		//	UIAdditionalDrawOrder order;
		//};

		//struct UIAdditionalDrawData {
		//	UIDrawer* drawer;
		//	// This is the data that the main parent window receives
		//	void* window_data;
		//	// This is personal data that you set
		//	void* data;
		//	// These are extra options that the parent window can give you
		//	void* extra_data;

		//	ECS_INLINE void AddDrawElement(UIAdditionalDrawElement element) {
		//		if (draw_elements_count < std::size(draw_elements)) {
		//			draw_elements[draw_elements_count++] = element;
		//		}
		//		else {
		//			ECS_ASSERT(false, "Too many draw elements for UIAdditionalDraw");
		//		}
		//	}

		//	ECS_INLINE void AddDrawOrder(UIAdditionalDrawOrder order) {
		//		if (draw_order_count < std::size(draw_order)) {
		//			draw_order[draw_order_count++] = order;
		//		}
		//		else {
		//			ECS_ASSERT(false, "Too many draw order elements for UIAdditionalDraw");
		//		}
		//	}

		//	// This is valid for all reasons except ACTION
		//	UIDrawerRowLayout* row_layout;

		//	union {
		//		// This is valid only for STATE_ELEMENTS
		//		struct {
		//			UIAdditionalDrawElement draw_elements[16];
		//			unsigned short draw_elements_count;
		//		};
		//		// This is valid for all GET reasons to fill what parts are needed
		//		struct {
		//			UIAdditionalDrawOrder draw_order[16];
		//			unsigned short draw_order_count;
		//		};
		//	};

		//	UIAdditionalDrawReason reason;

		//	// This is valid only for ROW_DRAW reason to inform
		//	// what row it currently is
		//	unsigned int current_element_index;
		//};

		//struct UIAdditionalParentDrawData {
		//	UIDrawer* drawer;
		//	void* window_data;

		//	UIAdditionalParentDrawReason reason;
		//};

		//struct AdditionalWindowDrawWrapperData {
		//	WindowDraw parent_draw;
		//	WindowDraw child_draw;

		//	ECS_INLINE void* ParentData() const {
		//		return is_parent_data_ptr ? parent_data_ptr : (void*)parent_data;
		//	}

		//	ECS_INLINE void* ChildData() const {
		//		return is_child_data_ptr ? child_data_ptr : (void*)child_data;
		//	}

		//	bool is_parent_data_ptr;
		//	bool is_child_data_ptr;
		//	union {
		//		void* parent_data_ptr;
		//		size_t parent_data[16];
		//	};

		//	union {
		//		void* child_data_ptr;
		//		size_t child_data[16];
		//	};
		//};

		//ECSENGINE_API void AdditionalWindowDrawWrapper(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer);

#pragma endregion

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterReturnToDefaultButton(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterColorTheme(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParametersLayout(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParametersElementDescriptor(UIWindowDrawerDescriptor* descriptor, UIDrawer& drawer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer);

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

		ECSENGINE_API void SystemParametersDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void OpenSystemParameters(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void CenterWindowDescriptor(UIWindowDescriptor& descriptor, float2 size);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowHandler(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DrawNothing(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ErrorMessageWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		ECSENGINE_API unsigned int CreateErrorMessageWindow(UISystem* system, Stream<char> description);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ConfirmWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		ECSENGINE_API unsigned int CreateConfirmWindow(UISystem* system, Stream<char> description, UIActionHandler handler);

		// --------------------------------------------------------------------------------------------------------------

		struct ChooseOptionWindowData {
			Stream<UIActionHandler> handlers;
			const char** button_names;
			Stream<char> description;
			Stream<char> window_name = { nullptr, 0 };
		};

		ECSENGINE_API unsigned int CreateChooseOptionWindow(UISystem* system, ChooseOptionWindowData data);

		ECSENGINE_API void ChooseOptionWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		// --------------------------------------------------------------------------------------------------------------

#define ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_CAPACITY 2
#define ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_STORAGE_CAPACITY 16

		// The callback receives the char stream through the additional_data parameter
		// The extra draw elements receive the drawer in the _data argument and in the _additional_data
		// the data that was passed in at creation time
		struct ECSENGINE_API TextInputWizardData {
			void AddExtraElement(Action extra_element, void* data, size_t data_size);

			void SetInitialCharacters(Stream<char> characters) {
				ECS_ASSERT(characters.size <= sizeof(initial_input_characters));
				characters.CopyTo(initial_input_characters);
				initial_input_character_count = characters.size;
			}

			const char* input_name;
			const char* window_name;
			Action callback;
			void* callback_data;
			size_t callback_data_size;
			CapacityStream<char> input_stream = { nullptr, 0, 0 }; // Does not need to be initialized - it is used internally
			char initial_input_characters[64];
			size_t initial_input_character_count = 0;

			Action extra_draw_elements[ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_CAPACITY];
			void* extra_draw_elements_data[ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_CAPACITY];
			unsigned char extra_draw_elements_data_offset[ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_CAPACITY];
			unsigned char extra_draw_element_count = 0;
			unsigned short extra_draw_element_storage_offset = 0;

			size_t extra_draw_elements_storage[ECS_TEXT_INPUT_WINDOW_EXTRA_ELEMENTS_STORAGE_CAPACITY];
		};

		ECSENGINE_API void TextInputWizard(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API unsigned int CreateTextInputWizard(const TextInputWizardData* data, UISystem* system);

		// The callback receives the char stream through the additional_data parameter
		ECSENGINE_API void CreateTextInputWizardAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		struct TextFileDrawData {
			Stream<wchar_t> path;
			float2 border_padding = { 0.01f, 0.01f };
			float next_row_y_offset = 0.01f;
			unsigned int timer_milliseconds_recheck = 60;

			// These values are internal, don't need to be filled in
			Timer timer;
			Stream<char> file_data = {};
		};

		ECSENGINE_API void DeallocateTextFileDrawData(TextFileDrawData* draw_data, AllocatorPolymorphic path_allocator);

		// This is used in conjunction with a retained mode draw mode
		ECSENGINE_API void DrawTextFile(UIDrawer* drawer, TextFileDrawData* draw_data);

		// This is used in conjunction with a retained mode draw mode
		ECSENGINE_API void TextFileDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		// Window_data must be a TextFileDrawData* info
		ECSENGINE_API bool TextFileWindowRetainedMode(void* window_data, WindowRetainedModeInfo* info);

		ECSENGINE_API unsigned int CreateTextFileWindow(const TextFileDrawData* data, UISystem* system, Stream<char> window_name);
		
		struct ECSENGINE_API TextFileDrawActionData {
			TextFileDrawData draw_data;
			Stream<char> window_name;
		};

		ECSENGINE_API void CreateTextFileWindowAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ConsoleWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		#define CONSOLE_WINDOW_NAME "Console"	

		struct ECSENGINE_API UniqueConsoleMessage {
			ConsoleMessage message;
			unsigned int counter;
		};

		struct ECSENGINE_API ConsoleWindowData {
			ECSEngine::Console* console;
			bool collapse;
			bool clear_on_play;

			bool filter[ECS_CONSOLE_MESSAGE_COUNT + 1];
			bool filter_message_type_changed;
			bool system_filter_changed;
			unsigned char previous_verbosity_level;

			unsigned int last_frame_message_count;
			unsigned int type_count[ECS_CONSOLE_MESSAGE_COUNT];
			CapacityStream<unsigned int> filtered_message_indices;
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

		ECSENGINE_API void InjectValuesWindowDraw(UIDrawerDescriptor* drawer_descriptor, void* window_data, bool initialize);

		ECSENGINE_API unsigned int CreateInjectValuesWindow(UISystem* system, InjectWindowData data, Stream<char> window_name, bool is_pop_up_window = true);
		
		struct InjectValuesActionData {
			InjectWindowData data;
			Stream<char> name;
			bool is_pop_up_window;
		};

		ECSENGINE_API void CreateInjectValuesAction(ActionData* action_data);

		ECSENGINE_API void InjectWindowDestroyAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		struct VisualizeTextureSelectElement {
			Texture2D texture;
			Stream<char> name;
			bool transfer_texture_to_ui_graphics = false;
			ECS_GRAPHICS_FORMAT override_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
			VisualizeFormatConversion format_conversion = { 0.0f, 1.0f, (unsigned int)-1 };
		};

		// The select_elements are needed only when the selection_mode is triggered
		struct VisualizeTextureAdditionalDrawData {
			UIDrawer* drawer;
			void* window_data;
			void* private_data;
			Stream<char> window_name;

			// The additional draw can set this boolean to false
			// In order to prevent the draw from happening - in case this is needed
			bool can_display_texture;
			// Adds a label to the combo box that makes the transition to the selection mode
			bool include_select_label = true;
			// Hides the button in the middle that allows selection
			bool hide_select_button = true;
			// When this is true, only the select streams are valid
			// When it's false, the checkbox and the combo labels are active
			bool is_select_mode;

			CapacityStream<void>* combo_callback_memory;
			// Can optionally fill in a combo box to be displayed
			unsigned char* combo_index;
			CapacityStream<char>* combo_memory;
			CapacityStream<Stream<char>>* combo_labels;
			UIActionHandler combo_callback = {};

			CapacityStream<void>* check_box_callback_memory;
			Stream<char> check_box_name = { nullptr, 0 };
			UIActionHandler check_box_callback = {};
			bool* check_box_flag;

			// Can optionally give a list of textures that the user can select to display
			// If dynamic names are needed, use memory from the given capacity stream
			CapacityStream<char>* select_element_name_memory;
			CapacityStream<VisualizeTextureSelectElement>* select_elements;
		};

		// It will create a copy of the texture - on the same graphics object or on different ones
		struct VisualizeTextureCreateData {
			// This draw will be called before the main draw is happening
			// It allows to perform certain operations
			WindowDraw additional_draw = nullptr;
			void* additional_draw_data = nullptr;
			size_t additional_draw_data_size = 0;

			Texture2D texture = { (ID3D11Resource*)nullptr };
			Stream<char> window_name;
			UIActionHandler destroy_window_handler = {};

			// If this is set, it will automatically destroy the reference to the shared texture on destroy
			bool transfer_texture_to_ui_graphics = false;
			bool is_pop_up_window = false;
			bool automatic_update = true;
			bool select_mode = false;

			// These are used as initial values
			// Except for the conversion and override format which are permanent
			const VisualizeTextureOptions* options = {};
		};

		// Stack memory should be at least 512 bytes long
		ECSENGINE_API UIWindowDescriptor VisualizeTextureWindowDescriptor(UISystem* system, const VisualizeTextureCreateData* create_data, void* stack_memory);

		// It also creates a dockspace for it, returns the window index
		ECSENGINE_API unsigned int CreateVisualizeTextureWindow(
			UISystem* system, 
			const VisualizeTextureCreateData* create_data
		);

		// It also creates the dockspace for it
		ECSENGINE_API void CreateVisualizeTextureAction(ActionData* action_data);
		
		// Can optionally retarget the texture that is being displayed
		ECSENGINE_API void ChangeVisualizeTextureWindowOptions(
			UISystem* system, 
			Stream<char> window_name, 
			const VisualizeTextureCreateData* create_data
		);

		ECSENGINE_API void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			unsigned int window_index,
			const VisualizeTextureCreateData* create_data
		);

		ECSENGINE_API void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			Stream<char> window_name,
			const VisualizeTextureSelectElement* select_element
		);

		ECSENGINE_API void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			unsigned int window_index,
			const VisualizeTextureSelectElement* select_element
		);

		ECSENGINE_API void ChangeVisualizeTextureWindowOptions(
			UISystem* system,
			void* window_data,
			const VisualizeTextureSelectElement* select_element
		);

		// Returns the additional draw data for the given visualize texture window
		ECSENGINE_API void* GetVisualizeTextureWindowAdditionalData(
			UISystem* system,
			Stream<char> window_name
		);

		// Returns the additional draw data for the given visualize texture window
		ECSENGINE_API void* GetVisualizeTextureWindowAdditionalData(
			UISystem* system,
			unsigned int window_index
		);

		// Can change or update the visualize texture window data
		// Or can change the additional draw. If left at nullptr, then
		// it will not change the function
		ECSENGINE_API void UpdateVisualizeTextureWindowAdditionalDraw(
			UISystem* system,
			Stream<char> window_name,
			WindowDraw additional_draw = nullptr,
			void* additional_draw_data = nullptr,
			size_t additional_draw_data_size = 0
		);

		// Can change or update the visualize texture window data
		// Or can change the additional draw. If left at nullptr, then
		// it will not change the function
		ECSENGINE_API void UpdateVisualizeTextureWindowAdditionalDraw(
			UISystem* system,
			unsigned int window_index,
			WindowDraw additional_draw = nullptr,
			void* additional_draw_data = nullptr,
			size_t additional_draw_data_size = 0
		);

		ECSENGINE_API void UpdateVisualizeTextureWindowAutomaticRefresh(
			UISystem* system,
			void* window_data,
			bool automatic_refresh
		);

		ECSENGINE_API void TransitionVisualizeTextureWindowSelection(
			void* window_data
		);

		ECSENGINE_API void TransitionVisualizeTextureWindowSelection(
			UISystem* system,
			unsigned int window_index
		);

		// Draws a row with an "Ok" button on the left and a "Cancel" button on the right
		// When the action is performed, the window will be destroyed automatically
		ECSENGINE_API void UIDrawerOKCancelRow(UIDrawer& drawer, Stream<char> ok_label, Stream<char> cancel_label, UIActionHandler ok_handler, UIActionHandler cancel_handler);

	}

}