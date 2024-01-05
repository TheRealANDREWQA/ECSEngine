#pragma once
#include "../Core.h"
#include "../Containers/Queues.h"
#include "../Allocators/MemoryManager.h"
#include "ButtonInput.h"

#define ECS_KEYBOARD_CHARACTER_QUEUE_DEFAULT_SIZE 256
#define ECS_KEYBOARD_ALPHANUMERIC_CAPACITY 128

namespace ECSEngine {

	enum ECS_KEY : unsigned char {
        ECS_KEY_NONE,
        ECS_KEY_BACK,
        ECS_KEY_TAB,
        ECS_KEY_ENTER,
        ECS_KEY_PAUSE,
        ECS_KEY_CAPSLOCK,
        ECS_KEY_KANA,
        ECS_KEY_IME_ON,
        ECS_KEY_JUNJA,
        ECS_KEY_KANJI,
        ECS_KEY_HANJA,
        ECS_KEY_IME_OFF,
        ECS_KEY_ESCAPE,
        ECS_KEY_IME_CONVERT,
        ECS_KEY_IME_NO_CONVERT,
        ECS_KEY_SPACE,
        ECS_KEY_PAGE_UP,
        ECS_KEY_PAGE_DOWN,
        ECS_KEY_END,
        ECS_KEY_HOME,
        ECS_KEY_LEFT,
        ECS_KEY_UP,
        ECS_KEY_RIGHT,
        ECS_KEY_DOWN,
        ECS_KEY_SELECT,
        ECS_KEY_PRINT,
        ECS_KEY_EXECUTE,
        ECS_KEY_PRINTSCREEN,
        ECS_KEY_INSERT,
        ECS_KEY_DELETE,
        ECS_KEY_HELP,
        ECS_KEY_0,
        ECS_KEY_1,
        ECS_KEY_2,
        ECS_KEY_3,
        ECS_KEY_4,
        ECS_KEY_5,
        ECS_KEY_6,
        ECS_KEY_7,
        ECS_KEY_8,
        ECS_KEY_9,
        ECS_KEY_A,
        ECS_KEY_B,
        ECS_KEY_C,
        ECS_KEY_D,
        ECS_KEY_E,
        ECS_KEY_F,
        ECS_KEY_G,
        ECS_KEY_H,
        ECS_KEY_I,
        ECS_KEY_J,
        ECS_KEY_K,
        ECS_KEY_L,
        ECS_KEY_M,
        ECS_KEY_N,
        ECS_KEY_O,
        ECS_KEY_P,
        ECS_KEY_Q,
        ECS_KEY_R,
        ECS_KEY_S,
        ECS_KEY_T,
        ECS_KEY_U,
        ECS_KEY_V,
        ECS_KEY_W,
        ECS_KEY_X,
        ECS_KEY_Y,
        ECS_KEY_Z,
        ECS_KEY_LEFT_WINDOWS,
        ECS_KEY_RIGHT_WINDOWS,
        ECS_KEY_APPS,
        ECS_KEY_SLEEP,
        ECS_KEY_NUM0,
        ECS_KEY_NUM1,
        ECS_KEY_NUM2,
        ECS_KEY_NUM3,
        ECS_KEY_NUM4,
        ECS_KEY_NUM5,
        ECS_KEY_NUM6,
        ECS_KEY_NUM7,
        ECS_KEY_NUM8,
        ECS_KEY_NUM9,
        ECS_KEY_MULTIPLY,
        ECS_KEY_ADD,
        ECS_KEY_SEPARATOR,
        ECS_KEY_MINUS,
        ECS_KEY_DECIMAL,
        ECS_KEY_DIVIDE,
        ECS_KEY_F1,
        ECS_KEY_F2,
        ECS_KEY_F3,
        ECS_KEY_F4,
        ECS_KEY_F5,
        ECS_KEY_F6,
        ECS_KEY_F7,
        ECS_KEY_F8,
        ECS_KEY_F9,
        ECS_KEY_F10,
        ECS_KEY_F11,
        ECS_KEY_F12,
        ECS_KEY_F13,
        ECS_KEY_F14,
        ECS_KEY_F15,
        ECS_KEY_F16,
        ECS_KEY_F17,
        ECS_KEY_F18,
        ECS_KEY_F19,
        ECS_KEY_F20,
        ECS_KEY_F21,
        ECS_KEY_F22,
        ECS_KEY_F23,
        ECS_KEY_F24,
        ECS_KEY_NUMLOCK,
        ECS_KEY_SCROLL,
        ECS_KEY_LEFT_SHIFT,
        ECS_KEY_RIGHT_SHIFT,
        ECS_KEY_LEFT_CTRL,
        ECS_KEY_RIGHT_CTRL,
        ECS_KEY_LEFT_ALT,
        ECS_KEY_RIGHT_ALT,
        ECS_KEY_BROWSER_BACK,
        ECS_KEY_BROWSER_FORWARD,
        ECS_KEY_BROWSER_REFRESH,
        ECS_KEY_BROWSER_STOP,
        ECS_KEY_BROWSER_SEARCH,
        ECS_KEY_BROWSER_FAVORITES,
        ECS_KEY_BROWSER_HOME,
        ECS_KEY_VOLUME_MUTE,
        ECS_KEY_VOLUME_DOWN,
        ECS_KEY_VOLUME_UP,
        ECS_KEY_MEDIA_NEXT_TRACK,
        ECS_KEY_MEDIA_PREVIOUS_TRACK,
        ECS_KEY_MEDIA_STOP,
        ECS_KEY_MEDIA_PLAY_STOP,
        ECS_KEY_OEM1, // For US layout ;:
        ECS_KEY_OEM2, // For US layout /?
        ECS_KEY_OEM3, // For US layout `~
        ECS_KEY_OEM4, // For US layout [{
        ECS_KEY_OEM5, // For US layout \|
        ECS_KEY_OEM6, // For US layout ]}
        ECS_KEY_OEM7, // For US layout '"
        ECS_KEY_OEM8, // No equivalent
        ECS_KEY_OEM_PLUS,
        ECS_KEY_OEM_COMMA,
        ECS_KEY_OEM_MINUS,
        ECS_KEY_OEM_PERIOD,
        ECS_KEY_COUNT,

        // These are helpers to reference keys which have a
        // not so explicit mapping
        ECS_KEY_SEMICOLON = ECS_KEY_OEM1,
        ECS_KEY_COLON = ECS_KEY_OEM1,
        ECS_KEY_FORWARD_SLASH = ECS_KEY_OEM2,
        ECS_KEY_QUESTION_MARK = ECS_KEY_OEM2,
        ECS_KEY_SHARP_QUOTE = ECS_KEY_OEM3,
        ECS_KEY_TILDE = ECS_KEY_OEM3,
        ECS_KEY_OPEN_SQUARE_BRACKET = ECS_KEY_OEM4,
        ECS_KEY_OPEN_CURLY_BRACKET = ECS_KEY_OEM4,
        ECS_KEY_BACKSLASH = ECS_KEY_OEM5,
        ECS_KEY_PIPE = ECS_KEY_OEM5,
        ECS_KEY_CLOSED_SQUARE_BRACKET = ECS_KEY_OEM6,
        ECS_KEY_CLOSED_CURLY_BRACKET = ECS_KEY_OEM6,
        ECS_KEY_APOSTROPHE = ECS_KEY_OEM7,
        ECS_KEY_QUOTES = ECS_KEY_OEM7
	};

	typedef ResizableQueue<char> KeyboardCharacterQueue;

	struct ECSENGINE_API KeyboardProcedureInfo {
		UINT message;
		WPARAM wParam;
		LPARAM lParam;
	};

    // Returns true if that key is used for character input
    ECSENGINE_API bool IsAlphanumeric(ECS_KEY key);

	struct ECSENGINE_API Keyboard : ButtonInput<ECS_KEY, ECS_KEY_COUNT>
	{
		ECS_INLINE Keyboard() {}
        ECS_INLINE Keyboard(GlobalMemoryManager* allocator) {
            Reset();
            m_character_queue = KeyboardCharacterQueue(allocator, ECS_KEYBOARD_CHARACTER_QUEUE_DEFAULT_SIZE);
            m_alphanumeric_keys.Initialize(allocator, 0, ECS_KEYBOARD_ALPHANUMERIC_CAPACITY);
            DoNotCaptureCharacters();
        }

		Keyboard(const Keyboard& other) = default;
		Keyboard& operator = (const Keyboard& other) = default;

		ECS_INLINE void CaptureCharacters() {
			m_process_characters = true;
            // At the moment, this messes up with the UI text input
            // Disable it for the time being
            //m_alphanumeric_keys.size = 0;
		}

		ECS_INLINE void DoNotCaptureCharacters() {
			m_process_characters = false;
            m_alphanumeric_keys.size = 0;
		}

        ECS_INLINE bool IsCaptureCharacters() const {
            return m_process_characters;
        }

		ECS_INLINE bool GetCharacter(char& character) {
			return m_character_queue.Pop(character);
		}

        void Reset();

        void Update();

		void Procedure(const KeyboardProcedureInfo& info);

        void UpdateFromOther(const Keyboard* other);

        // Returns the state of an alphanumeric key while text input is activated
        ECS_BUTTON_STATE GetAlphanumericKey(ECS_KEY key) const;

        struct AlphanumericKey {
            ECS_KEY key;
            ECS_BUTTON_STATE state;
        };

		KeyboardCharacterQueue m_character_queue;
		bool m_process_characters;
        // Describes how many characters have been pushed since
        // The last message handling pass
        unsigned int m_pushed_character_count;
        // While characters are pushed into the character queue, their states
        // Are recorded here separately such that you can still ask for the button
        // States even when recording characters.
        CapacityStream<AlphanumericKey> m_alphanumeric_keys;
	};
}

