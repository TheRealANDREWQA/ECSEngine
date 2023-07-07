#pragma once
#include "UIStructures.h"
#include "UISystem.h"
#include "../../Utilities/Keyboard.h"
#include "UIDrawerStructures.h"
#include "UIDrawConfig.h"

namespace ECSEngine {

	namespace Tools {

		inline bool UIDrawerTextInputFilterAll(char character, CharacterType type) {
			return true;
		}

		inline bool UIDrawerTextInputFilterLetters(char character, CharacterType type) {
			return type == CharacterType::LowercaseLetter || type == CharacterType::CapitalLetter || type == CharacterType::Space;
		}

		inline bool UIDrawerTextInputFilterLowercaseLetters(char character, CharacterType type) {
			return type == CharacterType::LowercaseLetter;
		}

		inline bool UIDrawerTextInputFilterUppercaseLetters(char character, CharacterType type) {
			return type == CharacterType::CapitalLetter;
		}
		
		inline bool UIDrawerTextInputFilterDigits(char character, CharacterType type) {
			return type == CharacterType::Digit;
		}

		inline bool UIDrawerTextInputFilterInteger(char character, CharacterType type) {
			return type == CharacterType::Digit || character == '-';
		}

		inline bool UIDrawerTextInputFilterSymbols(char character, CharacterType type) {
			return type == CharacterType::Symbol;
		}

		inline bool UIDrawerTextInputFilterLettersAndDigits(char character, CharacterType type) {
			return UIDrawerTextInputFilterLetters(character, type) || UIDrawerTextInputFilterDigits(character, type);
		}

		inline bool UIDrawerTextInputFilterNumbers(char character, CharacterType type) {
			return type == CharacterType::Digit || character == '-' || character == '.';
		}

		inline bool UIDrawerTextInputHexFilter(char character, CharacterType type) {
			return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
		}

		struct ECSENGINE_API UIDrawerTextInput {
			float2* TextPosition();
			float2* TextScale();
			UISpriteVertex* TextBuffer();
			unsigned int* TextSize();
			CapacityStream<UISpriteVertex>* TextStream();

			float GetLowestX() const;
			float GetLowestY() const;
			float2 GetLowest() const;

			float2 GetZoom() const;
			float2 GetInverseZoom() const;
			float GetZoomX() const;
			float GetZoomY() const;
			float GetInverseZoomX() const;
			float GetInverseZoomY() const;

			void SetZoomFactor(float2 zoom);
			bool IsTheSameData(const UIDrawerTextInput* other) const;

			unsigned int GetSpritePositionFromMouse(const UISystem* system, float2 mouse_position) const;

			void SetSpritePositionFromMouse(const UISystem* system, float2 mouse_position);

			unsigned int GetVisibleSpriteCount(const UISystem* system, float right_bound) const;

			float2 GetCaretPosition(const UISystem* system) const;

			float2 GetPositionFromSprite(const UISystem* system, unsigned int index, bool bottom = false, bool left = true) const;

			template<typename Action>
			void RepeatKeyAction(
				const UISystem* system,
				const Keyboard* keyboard,
				ECS_KEY key,
				ECS_KEY& repeat_key_pressed,
				Action&& action
			) {
				if (keyboard->IsPressed(key) && repeat_key == ECS_KEY_NONE) {
					repeat_key = key;
					key_repeat_start = std::chrono::high_resolution_clock::now();
					repeat_key_count = 0;
					repeat_key_pressed = key;

					action();
				}
				else if (keyboard->IsDown(key) && repeat_key == key) {
					repeat_key_pressed = key;
					size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - key_repeat_start).count();

					if (duration > system->m_descriptors.misc.text_input_repeat_start_duration) {
						duration -= system->m_descriptors.misc.text_input_repeat_start_duration;
						if (duration / system->m_descriptors.misc.text_input_repeat_time > repeat_key_count) {
							action();
							repeat_key_count++;
						}
					}
				}
			}

			void SetNewZoom(float2 new_zoom);

			// returns whether or not it deleted something
			bool Backspace(unsigned int* text_count, unsigned int* text_position, char* text);

			void PasteCharacters(
				const char* characters,
				unsigned int character_count,
				UISystem* system,
				unsigned int window_index,
				char* deleted_characters,
				unsigned int* deleted_character_count,
				unsigned int* delete_position
			);

			void Callback(ActionData* action_data);

			bool HasCallback() const;

			void CopyCharacters(UISystem* system);

			// does not place a revert command
			void DeleteAllCharacters();

			void InsertCharacters(const char* characters, unsigned int character_count, unsigned int character_position, UISystem* system);

			CapacityStream<char>* text;
			// This buffer is used to determine when a change has occured to the text
			// outside of the UI
			CapacityStream<char> previous_text;
			Stream<char> hint_text;
			UIDrawerTextElement name;
			float2 inverse_zoom;
			float2 current_zoom;
			float2 font_size;
			float2 position;
			float2 padding;
			float solid_color_y_scale;
			float character_spacing;
			float bound;
			Color text_color;
			unsigned int current_sprite_position;
			int current_selection;
			unsigned int sprite_render_offset;
			std::chrono::high_resolution_clock::time_point caret_start;
			std::chrono::high_resolution_clock::time_point key_repeat_start;
			std::chrono::high_resolution_clock::time_point word_click_start;
			unsigned int repeat_key_count;
			unsigned int char_click_index;
			unsigned int filter_characters_start;
			unsigned int filter_character_count;
			unsigned int convert_characters_start;
			unsigned int convert_character_count;
			bool is_caret_display;
			bool suppress_arrow_movement;
			bool is_word_selection;
			unsigned char word_click_count;
			ECS_KEY repeat_key;
			bool is_currently_selected;

			bool display_tooltip;

			// This is set by the functions that manipulate the input in order to preserve consistency
			enum TRIGGER_CALLBACK : unsigned char {
				TRIGGER_CALLBACK_NONE,
				TRIGGER_CALLBACK_MODIFY,
				TRIGGER_CALLBACK_EXIT
			};

			TRIGGER_CALLBACK trigger_callback;
			bool trigger_callback_on_release;
			Action callback;
			void* callback_data;
		};

		// slider position is expressed as percentage of its scale
		struct ECSENGINE_API UIDrawerSlider {
			bool IsTheSameData(const UIDrawerSlider* other) const;

			float2* TextPosition();
			float2* TextScale();
			UISpriteVertex* TextBuffer();
			unsigned int* TextSize();
			CapacityStream<UISpriteVertex>* TextStream();

			float2 GetZoom() const;
			float2 GetInverseZoom() const;
			float GetZoomX() const;
			float GetZoomY() const;
			float GetInverseZoomX() const;
			float GetInverseZoomY() const;

			void SetZoomFactor(float2 zoom);

			void Callback(ActionData* action_data);

			bool is_vertical;
			bool interpolate_value;
			bool changed_value;
			bool character_value;
			unsigned char value_byte_size;
			unsigned int text_input_counter;
			float slider_position;
			float total_length_main_axis;
			float2 initial_scale;
			float2 current_scale;
			float2 current_position;
			CapacityStream<char> characters;
			UIDrawerTextElement label;
			UIDrawerTextInput* text_input;
			void* value_to_change;
			void* default_value;
			UIActionHandler changed_value_callback;
			std::chrono::high_resolution_clock::time_point enter_value_click;
		};

		struct ECSENGINE_API UIDrawerColorInput {
			void Callback(ActionData* action_data, bool released);

			float2* TextPosition();
			float2* TextScale();
			UISpriteVertex* TextBuffer();
			unsigned int* TextSize();
			CapacityStream<UISpriteVertex>* TextStream();

			float GetLowestX() const;
			float GetLowestY() const;
			float2 GetLowest() const;

			float2 GetZoom() const;
			float2 GetInverseZoom() const;
			float GetZoomX() const;
			float GetZoomY() const;
			float GetInverseZoomX() const;
			float GetInverseZoomY() const;

			void SetZoomFactor(float2 zoom);

			UIDrawerSlider* r_slider;
			UIDrawerSlider* g_slider;
			UIDrawerSlider* b_slider;
			UIDrawerSlider* h_slider;
			UIDrawerSlider* s_slider;
			UIDrawerSlider* v_slider;
			UIDrawerSlider* a_slider;
			UIDrawerTextInput* hex_input;
			CapacityStream<char> hex_characters;
			Color hsv;
			Color* rgb;
			Color color_picker_initial_color;
			Color default_color;
			UIDrawerTextElement name;

			UIActionHandler callback;
			UIActionHandler final_callback;
		};

		struct ColorInputHSVGradientInfo {
			UIDrawerColorInput* input;
			float2 gradient_position;
			float2 gradient_scale;
		};

		struct UIDrawerSliderBringToMouse {
			UIDrawerSlider* slider;
			float position;
			float slider_length;
			bool is_finished;
			std::chrono::high_resolution_clock::time_point start_point;
		};

		struct UIDrawerTextInputAddCommandInfo {
			UIDrawerTextInput* input;
			unsigned int text_position;
			unsigned int text_count;
		};

		struct UIDrawerTextInputRemoveCommandInfo {
			UIDrawerTextInput* input;
			unsigned int text_position;
			unsigned int text_count;
			void* deallocate_buffer;
			bool deallocate_data;
		};

		struct UIDrawerTextInputReplaceCommandInfo {
			UIDrawerTextInput* input;
			unsigned int text_position;
			unsigned int text_count;
			unsigned int replaced_text_count;
			void* deallocate_buffer;
		};

		struct UIDrawerColorInputWindowData {
			UIDrawerColorInput* input;
			Color initial_color;
		};

		struct UIDrawerColorInputSliderCallback {
			UIDrawerColorInput* input;
			bool is_rgb;
			bool is_hsv;
			bool is_alpha;
		};

		struct UIDrawerComboBox {
			unsigned char* active_label;
			
			unsigned int label_display_count;
			unsigned int biggest_label_x_index;
			
			float label_y_scale;
			float prefix_x_scale;

			UIDrawerTextElement name;
			Stream<Stream<char>> labels;
			
			Stream<char> prefix;
			
			Action callback;
			void* callback_data;

			unsigned short mapping_byte_size;
			// The number of mappings reserved
			unsigned char mapping_capacity;
			// The number of unavailable slots reserved
			unsigned char unavailables_capacity;
			void* mappings;

			bool* unavailables;
		};

		struct UIDrawerComboBoxClickable {
			UIDrawerComboBox* box;
			UIDrawConfig config;
			size_t configuration;
			bool is_opened_on_press;
		};

		struct UIDrawerComboBoxLabelHoverable {
			UIDrawerComboBox* box;
			Color color;
		};

		struct UIDrawerComboBoxLabelClickable {
			UIDrawerComboBox* box;
			unsigned int index;
		};

		struct UIDrawerBoolClickableWithPinData {
			bool* pointer;
			bool is_horizontal = false;
			bool is_vertical = false;
			unsigned int pin_count = 1;
		};

		struct UIDrawerHierarchySelectableData {
			UIDrawerBoolClickableWithPinData bool_data;
			UIDrawerHierarchy* hierarchy;
			unsigned int node_index;
		};

		struct UIDrawerHierarchyDragNode {
			UIDrawerHierarchiesData* hierarchies_data;
			UIDrawerHierarchySelectableData selectable_data;
			unsigned int previous_index = 0;
			bool has_been_cancelled = false;
			Timer timer;
		};

		struct UIDrawerGraphHoverableData {
			unsigned int sample_index;
			float2 first_sample_values;
			float2 second_sample_values;
			UITextTooltipHoverableData tool_tip_data;
		};

		struct UIDrawerHistogramHoverableData {
			unsigned int sample_index;
			float sample_value;
			UITextTooltipHoverableData tool_tip_data;
		};

		// Name needs to be a stable reference; this does not allocate memory for it
		// Action can be used to do additional stuff on right click; There can be
		// 64 bytes of action data embedded into this structure
		struct UIDrawerMenuRightClickData {
			Stream<char> name;
			UIDrawerMenuState state;
			Action action = nullptr;

			unsigned int window_index;
			bool is_action_data_ptr = false;

			union {
				char action_data[64];
				void* action_data_ptr;
			};
		};

		struct UIDrawerRightClickMenuSystemHandlerData {
			Stream<char> menu_window_name;
			Stream<char> menu_resource_name;
			Stream<char> parent_window_name;
		};

		struct UIDrawerColorInputSystemHandlerData {
			UIDockspace* dockspace;
			unsigned int border_index;
			DockspaceType type;
		};

		struct UIDrawerSliderReturnToDefaultMouseDraggable {
			UIDefaultHoverableData hoverable_data;
			UIDrawerSlider* slider;
		};

		struct UIDrawerMenuCleanupSystemHandlerData {
			Stream<char> window_names[6];
			int64_t window_count;
		};

		struct UIDrawerNumberInputCallbackData {
			inline void* GetUserData() const {
				return relative_user_data ? function::OffsetPointer(this, user_action_data_offset) : user_action_data;
			}

			Action user_action;
			union {
				void* user_action_data;
				size_t user_action_data_offset;
			};
			UIDrawerTextInput* input;
			bool return_to_default;
			bool display_range;
			bool relative_user_data;
		};

		// input, return_to_default and display_range must be the first data members
		// for type punning inside the number input initializer
		struct UIDrawerFloatInputCallbackData {
			UIDrawerNumberInputCallbackData number_data;
			float* number;
			float default_value;
			float min;
			float max;
		};

		// input, return_to_default and display_range must be the first data members
		// for type punning inside the number input initializer
		struct UIDrawerDoubleInputCallbackData {
			UIDrawerNumberInputCallbackData number_data;
			double* number;
			double default_value;
			double min;
			double max;
		};

		// input, return_to_default and display_range must be the first data members
		// for type punning inside the number input initializer
		template<typename Integer>
		struct UIDrawerIntegerInputCallbackData {
			UIDrawerNumberInputCallbackData number_data;
			Integer* number;
			Integer default_value;
			Integer min;
			Integer max;
		};

		// Type pun the types - all have UITextTooltipHoverableData as first field
		// and second field a pointer to the input callback data
		struct UIDrawerFloatInputHoverableData {
			UITextTooltipHoverableData tool_tip;
			UIDrawerFloatInputCallbackData* data;
		};

		// Type pun the types - all have UITextTooltipHoverableData as first field
		// and second field a pointer to the input callback data
		struct UIDrawerDoubleInputHoverableData {
			UITextTooltipHoverableData tool_tip;
			UIDrawerDoubleInputCallbackData* data;
		};

		// Type pun the types - all have UITextTooltipHoverableData as first field
		// and second field a pointer to the input callback data
		template<typename Integer>
		struct UIDrawerIntInputHoverableData {
			UITextTooltipHoverableData tool_tip;
			UIDrawerIntegerInputCallbackData<Integer>* data;
		};

		struct UIDrawerFloatInputDragData {
			UIDrawerFloatInputCallbackData callback_data;
			bool callback_on_release;
		};

		struct UIDrawerDoubleInputDragData {
			UIDrawerDoubleInputCallbackData callback_data;
			bool callback_on_release;
		};

		template<typename Integer>
		struct UIDrawerIntInputDragData {
			UIDrawerIntegerInputCallbackData<Integer> data;
			float last_position;
			bool callback_on_release;
		};

		struct UIChangeStateData {
			size_t* state;
			size_t flag;
		};

		struct UIChangeAtomicStateData {
			std::atomic<size_t>* state;
			size_t flag;
		};

		struct UIDrawerStateTableAllButtonData {
			bool all_true;
			bool single_pointer;
			Stream<bool*> states;
			bool* notifier;
		};

		struct UIDrawerFilesystemHierarchyRightClickData {
			void* data;
			Stream<char> label;
		};

		struct UIDrawerFilesystemHierarchyChangeStateData {
			UIDrawerFilesystemHierarchy* hierarchy;
			Stream<char> label;
		};

		struct UIDrawerArrayDragData {
			UIDrawerArrayData* array_data;
			float row_y_scale;
			unsigned int index;
		};

		struct UIDrawerColorFloatInput {
			UIDrawerColorInput* color_input;
			ColorFloat* color_float;
			Color base_color;
			float intensity;
		};

		struct UIDrawerColorFloatInputCallbackData {
			UIDrawerColorFloatInput* input;
			Action callback;
			void* callback_data;
		};

		struct ECSENGINE_API UIDrawerTextInputActionData {
			bool IsTheSameData(const UIDrawerTextInputActionData* other) const;

			UIDrawerTextInput* input;
			UIDrawerTextInputFilter filter;
		};

		struct ECSENGINE_API UIDrawerSliderEnterValuesData {
			bool IsTheSameData(const UIDrawerSliderEnterValuesData* other) const;

			UIDrawerSlider* slider;
			UIDrawerSliderConvertTextInput convert_input;
			UIDrawerTextInputFilter filter_function;
		};

		struct UIDrawerPathInputFolderActionData {
			UIDrawerTextInput* input;
			CapacityStream<wchar_t>* path;
			UIActionHandler custom_handler = { nullptr, nullptr, 0 };
			Stream<Stream<wchar_t>> extensions;
			Stream<Stream<wchar_t>> roots;
		};

		struct UIDrawerPathInputFolderWindowData {
			UIDrawerTextInput* input;
			CapacityStream<wchar_t>* path;
			UIActionHandler draw_handler;
			// This is the name of the window that originally spawned the window
			const char* window_bind;
		};

		struct ECSENGINE_API UIDrawerPathInputWithInputsActionData {
			UIDrawerPathInputWithInputsActionData() {}

			UIDrawerTextInput* input;
			CapacityStream<wchar_t>* path;
			union {
				Stream<Stream<wchar_t>> files;
				UIActionHandler callback_handler;
			};
			bool is_callback = false;
		};

		struct SliderMouseDraggableData {
			UIDrawerSlider* slider;
			bool interpolate_bounds;
		};

		struct ECSENGINE_API LabelHierarchyChangeStateData {
			void GetCurrentLabel(void* storage) const;

			// Returns the total size of the structure with the string embedded
			unsigned int WriteLabel(const void* untyped_data);

			UIDrawerLabelHierarchyData* hierarchy;
			unsigned int label_size;
		};

		struct ECSENGINE_API LabelHierarchyClickActionData {
			void GetCurrentLabel(void* storage) const;

			// Returns the total size of the structure with the label embedded (both for normal string
			// or untyped data). Must set the hierarchy before calling this function
			unsigned int WriteLabel(const void* untyped_data);

			inline bool IsTheSameData(const LabelHierarchyClickActionData* other) const {
				return other != nullptr && hierarchy == other->hierarchy;
			}

			UIDrawerLabelHierarchyData* hierarchy;
			// The characters are written at the end
			unsigned int label_size;
			unsigned char click_count;
			Timer timer;
		};

		// Wrapps a base data with a user supplied callback
		struct ECSENGINE_API ActionWrapperWithCallbackData {
			unsigned int WriteCallback(UIActionHandler handler);

			UIActionHandler GetCallback() const;

			void* GetBaseData() const;

			Action base_action;
			// Only used if the base action references directly a pointer
			void* base_action_data;
			unsigned int base_action_data_size;
			UIActionHandler user_callback;
		};

		ECSENGINE_API void ActionWrapperWithCallback(ActionData* action_data);


	}

}