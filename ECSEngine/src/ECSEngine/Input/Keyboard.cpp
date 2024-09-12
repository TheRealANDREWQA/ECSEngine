#include "ecspch.h"
#include "Keyboard.h"

namespace ECSEngine {

	void Keyboard::CopyOther(const Keyboard* other) {
		memcpy(m_states, other->m_states, sizeof(m_states));
		m_process_characters = other->m_process_characters;
		m_pushed_character_count = other->m_pushed_character_count;
		m_alphanumeric_keys.CopyOther(other->m_alphanumeric_keys);
		
		m_character_queue.Reset();
		other->m_character_queue.ForEach([this](char character) {
			m_character_queue.Push(character);
		});
	}
	
	void Keyboard::Reset() {
		ButtonInput<ECS_KEY, ECS_KEY_COUNT>::Reset();
		m_pushed_character_count = 0;
		//m_alphanumeric_keys.size = 0;
	}

	void Keyboard::Update()
	{
		Tick();
		m_pushed_character_count = 0;

		// We need to update the alphanumeric keys as well
		for (unsigned int index = 0; index < m_alphanumeric_keys.size; index++) {
			if (m_alphanumeric_keys[index].state == ECS_BUTTON_PRESSED) {
				m_alphanumeric_keys[index].state = ECS_BUTTON_HELD;
			}
			else if (m_alphanumeric_keys[index].state == ECS_BUTTON_RELEASED) {
				// Remove this entry
				m_alphanumeric_keys.RemoveSwapBack(index);
				index--;
			}
		}
	}

	void Keyboard::Procedure(const KeyboardProcedureInfo& info)
	{
		bool down = false;

		switch (info.message)
		{
		case WM_CHAR:
			if (m_process_characters) {
				m_character_queue.Push(static_cast<char>(info.wParam));
				m_pushed_character_count++;
			}
			return;
			break;
		case WM_ACTIVATEAPP:
			Reset();
			return;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			down = true;
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			break;

		default:
			return;
		}

		int vk = static_cast<int>(info.wParam);
		switch (vk)
		{
		case VK_SHIFT:
		{
			WORD key_flags = HIWORD(info.lParam);
			WORD scan_code = LOBYTE(key_flags);
			vk = (int)(MapVirtualKeyW(scan_code, MAPVK_VSC_TO_VK_EX));
			if (!down)
			{
				// Workaround to ensure left and right shift get cleared when both were pressed at same time
				UpdateButton(ECS_KEY_LEFT_SHIFT, true);
				UpdateButton(ECS_KEY_RIGHT_SHIFT, true);
			}
		}
		break;
		case VK_CONTROL:
			vk = ((unsigned int)(info.lParam) & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
			break;

		case VK_MENU:
			vk = ((unsigned int)(info.lParam) & 0x01000000) ? VK_RMENU : VK_LMENU;
			break;
		}

		ECS_KEY ecs_key = ECS_KEY_NONE;
		// Pulled out of the definition header all VK's that are actually handled
		switch (vk) {
		case VK_BACK:
			ecs_key = ECS_KEY_BACK;
			break;
		case VK_TAB:
			ecs_key = ECS_KEY_TAB;
			break;

		case VK_RETURN:
			ecs_key = ECS_KEY_ENTER;
			break;

		case VK_SHIFT:
			ecs_key = ECS_KEY_LEFT_SHIFT; // At the moment assume left shift
			break;
		case VK_CONTROL:
			ecs_key = ECS_KEY_LEFT_CTRL; // At the moment assume left control
			break;
		case VK_MENU:
			ecs_key = ECS_KEY_LEFT_ALT; // At the moment assume left alt
			break;
		case VK_PAUSE:
			ecs_key = ECS_KEY_PAUSE;
			break;
		case VK_CAPITAL:
			ecs_key = ECS_KEY_CAPSLOCK;
			break;

		case VK_KANA:
			ecs_key = ECS_KEY_KANA;
			break;
		case VK_IME_ON:
			ecs_key = ECS_KEY_IME_ON;
			break;
		case VK_JUNJA:
			ecs_key = ECS_KEY_JUNJA;
			break;
		case VK_HANJA:
			ecs_key = ECS_KEY_HANJA;
			break;
		case VK_IME_OFF:
			ecs_key = ECS_KEY_IME_OFF;
			break;

		case VK_ESCAPE:
			ecs_key = ECS_KEY_ESCAPE;
			break;

		case VK_CONVERT:
			ecs_key = ECS_KEY_IME_CONVERT;
			break;
		case VK_NONCONVERT:
			ecs_key = ECS_KEY_IME_NO_CONVERT;
			break;

		case VK_SPACE:
			ecs_key = ECS_KEY_SPACE;
			break;
		case VK_PRIOR:
			ecs_key = ECS_KEY_PAGE_UP;
			break;
		case VK_NEXT:
			ecs_key = ECS_KEY_PAGE_DOWN;
			break;
		case VK_END:
			ecs_key = ECS_KEY_END;
			break;
		case VK_HOME:
			ecs_key = ECS_KEY_HOME;
			break;
		case VK_LEFT:
			ecs_key = ECS_KEY_LEFT;
			break;
		case VK_UP:
			ecs_key = ECS_KEY_UP;
			break;
		case VK_RIGHT:
			ecs_key = ECS_KEY_RIGHT;
			break;
		case VK_DOWN:
			ecs_key = ECS_KEY_DOWN;
			break;
		case VK_SELECT:
			ecs_key = ECS_KEY_SELECT;
			break;
		case VK_PRINT:
			ecs_key = ECS_KEY_PRINT;
			break;
		case VK_EXECUTE:
			ecs_key = ECS_KEY_EXECUTE;
			break;
		case VK_SNAPSHOT:
			ecs_key = ECS_KEY_PRINTSCREEN;
			break;
		case VK_INSERT:
			ecs_key = ECS_KEY_INSERT;
			break;
		case VK_DELETE:
			ecs_key = ECS_KEY_DELETE;
			break;
		case VK_HELP:
			ecs_key = ECS_KEY_HELP;
			break;

		case VK_LWIN:
			ecs_key = ECS_KEY_LEFT_WINDOWS;
			break;
		case VK_RWIN:
			ecs_key = ECS_KEY_RIGHT_WINDOWS;
			break;
		case VK_APPS:
			ecs_key = ECS_KEY_APPS;
			break;

		case VK_SLEEP:
			ecs_key = ECS_KEY_SLEEP;
			break;

		case VK_NUMPAD0:
			ecs_key = ECS_KEY_NUM0;
			break;
		case VK_NUMPAD1:
			ecs_key = ECS_KEY_NUM1;
			break;
		case VK_NUMPAD2:
			ecs_key = ECS_KEY_NUM2;
			break;
		case VK_NUMPAD3:
			ecs_key = ECS_KEY_NUM3;
			break;
		case VK_NUMPAD4:
			ecs_key = ECS_KEY_NUM4;
			break;
		case VK_NUMPAD5:
			ecs_key = ECS_KEY_NUM5;
			break;
		case VK_NUMPAD6:
			ecs_key = ECS_KEY_NUM6;
			break;
		case VK_NUMPAD7:
			ecs_key = ECS_KEY_NUM7;
			break;
		case VK_NUMPAD8:
			ecs_key = ECS_KEY_NUM8;
			break;
		case VK_NUMPAD9:
			ecs_key = ECS_KEY_NUM9;
			break;
		case VK_MULTIPLY:
			ecs_key = ECS_KEY_MULTIPLY;
			break;
		case VK_ADD:
			ecs_key = ECS_KEY_ADD;
			break;
		case VK_SEPARATOR:
			ecs_key = ECS_KEY_SEPARATOR;
			break;
		case VK_SUBTRACT:
			ecs_key = ECS_KEY_MINUS;
			break;
		case VK_DECIMAL:
			ecs_key = ECS_KEY_DECIMAL;
			break;
		case VK_DIVIDE:
			ecs_key = ECS_KEY_DIVIDE;
			break;
		case VK_F1:
			ecs_key = ECS_KEY_F1;
			break;
		case VK_F2:
			ecs_key = ECS_KEY_F2;
			break;
		case VK_F3:
			ecs_key = ECS_KEY_F3;
			break;
		case VK_F4:
			ecs_key = ECS_KEY_F4;
			break;
		case VK_F5:
			ecs_key = ECS_KEY_F5;
			break;
		case VK_F6:
			ecs_key = ECS_KEY_F6;
			break;
		case VK_F7:
			ecs_key = ECS_KEY_F7;
			break;
		case VK_F8:
			ecs_key = ECS_KEY_F8;
			break;
		case VK_F9:
			ecs_key = ECS_KEY_F9;
			break;
		case VK_F10:
			ecs_key = ECS_KEY_F10;
			break;
		case VK_F11:
			ecs_key = ECS_KEY_F11;
			break;
		case VK_F12:
			ecs_key = ECS_KEY_F12;
			break;
		case VK_F13:
			ecs_key = ECS_KEY_F13;
			break;
		case VK_F14:
			ecs_key = ECS_KEY_F14;
			break;
		case VK_F15:
			ecs_key = ECS_KEY_F15;
			break;
		case VK_F16:
			ecs_key = ECS_KEY_F16;
			break;
		case VK_F17:
			ecs_key = ECS_KEY_F17;
			break;
		case VK_F18:
			ecs_key = ECS_KEY_F18;
			break;
		case VK_F19:
			ecs_key = ECS_KEY_F19;
			break;
		case VK_F20:
			ecs_key = ECS_KEY_F20;
			break;
		case VK_F21:
			ecs_key = ECS_KEY_F21;
			break;
		case VK_F22:
			ecs_key = ECS_KEY_F22;
			break;
		case VK_F23:
			ecs_key = ECS_KEY_F23;
			break;
		case VK_F24:
			ecs_key = ECS_KEY_F24;
			break;

		case VK_NUMLOCK:
			ecs_key = ECS_KEY_NUMLOCK;
			break;
		case VK_SCROLL:
			ecs_key = ECS_KEY_SCROLL;
			break;

		case VK_LSHIFT:
			ecs_key = ECS_KEY_LEFT_SHIFT;
			break;
		case VK_RSHIFT:
			ecs_key = ECS_KEY_RIGHT_SHIFT;
			break;
		case VK_LCONTROL:
			ecs_key = ECS_KEY_LEFT_CTRL;
			break;
		case VK_RCONTROL:
			ecs_key = ECS_KEY_RIGHT_CTRL;
			break;
		case VK_LMENU:
			ecs_key = ECS_KEY_LEFT_ALT;
			break;
		case VK_RMENU:
			ecs_key = ECS_KEY_RIGHT_ALT;
			break;

		case VK_BROWSER_BACK:
			ecs_key = ECS_KEY_BROWSER_BACK;
			break;
		case VK_BROWSER_FORWARD:
			ecs_key = ECS_KEY_BROWSER_FORWARD;
			break;
		case VK_BROWSER_REFRESH:
			ecs_key = ECS_KEY_BROWSER_REFRESH;
			break;
		case VK_BROWSER_STOP:
			ecs_key = ECS_KEY_BROWSER_STOP;
			break;
		case VK_BROWSER_SEARCH:
			ecs_key = ECS_KEY_BROWSER_SEARCH;
			break;
		case VK_BROWSER_FAVORITES:
			ecs_key = ECS_KEY_BROWSER_FAVORITES;
			break;
		case VK_BROWSER_HOME:
			ecs_key = ECS_KEY_BROWSER_HOME;
			break;

		case VK_VOLUME_MUTE:
			ecs_key = ECS_KEY_VOLUME_MUTE;
			break;
		case VK_VOLUME_DOWN:
			ecs_key = ECS_KEY_VOLUME_DOWN;
			break;
		case VK_VOLUME_UP:
			ecs_key = ECS_KEY_VOLUME_UP;
			break;
		case VK_MEDIA_NEXT_TRACK:
			ecs_key = ECS_KEY_MEDIA_NEXT_TRACK;
			break;
		case VK_MEDIA_PREV_TRACK:
			ecs_key = ECS_KEY_MEDIA_PREVIOUS_TRACK;
			break;
		case VK_MEDIA_STOP:
			ecs_key = ECS_KEY_MEDIA_STOP;
			break;
		case VK_MEDIA_PLAY_PAUSE:
			ecs_key = ECS_KEY_MEDIA_PLAY_STOP;
			break;


		case VK_OEM_1:    // ';:' for US
			ecs_key = ECS_KEY_OEM1;
			break;
		case VK_OEM_PLUS:    // '+' any country
			ecs_key = ECS_KEY_OEM_PLUS;
			break;
		case VK_OEM_COMMA:    // ',' any country
			ecs_key = ECS_KEY_OEM_COMMA;
			break;
		case VK_OEM_MINUS:    // '-' any country
			ecs_key = ECS_KEY_OEM_MINUS;
			break;
		case VK_OEM_PERIOD:    // '.' any country
			ecs_key = ECS_KEY_OEM_PERIOD;
			break;
		case VK_OEM_2:    // '/?' for US
			ecs_key = ECS_KEY_OEM2;
			break;
		case VK_OEM_3:    // '`~' for US
			ecs_key = ECS_KEY_OEM3;
			break;

		case VK_OEM_4:   //  '[{' for US
			ecs_key = ECS_KEY_OEM4;
			break;
		case VK_OEM_5:   //  '\|' for US
			ecs_key = ECS_KEY_OEM5;
			break;
		case VK_OEM_6:   //  ']}' for US
			ecs_key = ECS_KEY_OEM6;
			break;
		case VK_OEM_7:   //  ''"' for US
			ecs_key = ECS_KEY_OEM7;
			break;
		case VK_OEM_8:
			ecs_key = ECS_KEY_OEM8;
			break;
		}

		const int VK_DIGIT_START = 0x30;
		const int VK_DIGIT_END = 0x39;
		const int VK_LETTER_START = 0x41;
		const int VK_LETTER_END = 0x5A;

		if (ecs_key == ECS_KEY_NONE) {
			// Now check for letters and digits - they don't have VK's but instead ranges
			if (VK_DIGIT_START <= vk && vk <= VK_DIGIT_END) {
				ecs_key = (ECS_KEY)(ECS_KEY_0 + (vk - VK_DIGIT_START));
			}
			else if (VK_LETTER_START <= vk && vk <= VK_LETTER_END) {
				ecs_key = (ECS_KEY)(ECS_KEY_A + (vk - VK_LETTER_START));
			}
		}

		// If the key is repeated, ignore the message
		bool is_key_repeatead = (HIWORD(info.lParam) & KF_REPEAT) == KF_REPEAT;
		if (is_key_repeatead) {
			if (down && IsDown(ecs_key)) {
				return;
			}
			else if (!down && IsUp(ecs_key)) {
				return;
			}
		}

		bool is_alphanumeric = IsAlphanumeric(ecs_key);
		if (!is_alphanumeric || !m_process_characters) {
			UpdateButton(ecs_key, !down);
		}
		else if (is_alphanumeric) {
			// Update the entry from the alphanumeric keys
			unsigned int index = 0;
			for (; index < m_alphanumeric_keys.size; index++) {
				if (m_alphanumeric_keys[index].key == ecs_key) {
					if (down) {
						m_alphanumeric_keys[index].state = m_alphanumeric_keys[index].state != ECS_BUTTON_PRESSED ? ECS_BUTTON_PRESSED : ECS_BUTTON_HELD;
					}
					else {
						m_alphanumeric_keys[index].state = m_alphanumeric_keys[index].state != ECS_BUTTON_RELEASED ? ECS_BUTTON_RELEASED : ECS_BUTTON_RAISED;
					}
					break;
				}
			}

			if (index == m_alphanumeric_keys.size) {
				// Add a new entry
				m_alphanumeric_keys.Add({ ecs_key, down ? ECS_BUTTON_PRESSED : ECS_BUTTON_RELEASED });
			}
		}
	}

	void Keyboard::UpdateFromOther(const Keyboard* other)
	{
		ButtonInput<ECS_KEY, ECS_KEY_COUNT>::UpdateFromOther(other);
		// We also need to update the character queue
		for (unsigned int index = 0; index < other->m_pushed_character_count; index++) {
			m_character_queue.Push(other->m_character_queue.PushPeekByIndex(index));
		}
		m_pushed_character_count = other->m_pushed_character_count;
		// And update the alphanumeric keys as well
		m_process_characters = other->m_process_characters;
		m_alphanumeric_keys.CopyOther(other->m_alphanumeric_keys);
	}

	ECS_BUTTON_STATE Keyboard::GetAlphanumericKey(ECS_KEY key) const
	{
		for (unsigned int index = 0; index < m_alphanumeric_keys.size; index++) {
			if (m_alphanumeric_keys[index].key == key) {
				return m_alphanumeric_keys[index].state;
			}
		}

		return ECS_BUTTON_RAISED;
	}

	bool IsAlphanumeric(ECS_KEY key)
	{
		if (key >= ECS_KEY_0 && key <= ECS_KEY_9) {
			return true;
		}

		if (key >= ECS_KEY_A && key <= ECS_KEY_Z) {
			return true;
		}

		// These are the keys like TILDE, braces, semicolon, slash,
		// quotes, backslash
		if (key >= ECS_KEY_OEM1 && key <= ECS_KEY_OEM7) {
			return true;
		}

		if (key == ECS_KEY_ADD || key == ECS_KEY_MINUS) {
			return true;
		}

		return false;
	}

	void GetKeyboardButtonDelta(const Keyboard* previous_state, const Keyboard* current_state, CapacityStream<ECS_KEY>& keys) 
	{
		for (size_t index = 0; index < ECS_KEY_COUNT; index++) {
			if (previous_state->IsDown((ECS_KEY)index) != current_state->IsDown((ECS_KEY)index)) {
				keys.AddAssert((ECS_KEY)index);
			}
		}
	}

}
