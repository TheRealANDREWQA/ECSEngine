#include "ecspch.h"
#include "Keyboard.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	namespace HID {

		Keyboard::Keyboard() : m_implementation(nullptr) {}

		Keyboard::Keyboard(GlobalMemoryManager* allocator) {
			m_implementation = new DirectX::Keyboard();
			m_tracker.Reset();
			m_character_queue = KeyboardCharacterQueue(GetAllocatorPolymorphic(allocator), ECS_KEYBOARD_CHARACTER_QUEUE_DEFAULT_SIZE);
			DoNotCaptureCharacters();
		}

		void Keyboard::UpdateState()
		{
			m_frame_state = m_implementation->GetState();
		}

		void Keyboard::UpdateTracker() {
			m_tracker.Update(m_implementation->GetState());
		}

		void Keyboard::Procedure(const KeyboardProcedureInfo& info)
		{
			switch (info.message) {
				case WM_CHAR:
					if (m_process_characters) {
						m_character_queue.Push(static_cast<char>(info.wParam));
					}
					break;
				case WM_KEYDOWN:
				case WM_KEYUP:
				case WM_SYSKEYUP:
				case WM_SYSKEYDOWN:
				case WM_ACTIVATEAPP:
					m_implementation->ProcessMessage(info.message, info.wParam, info.lParam);
					break;
			};
		}

		void KeyboardTracker::Update(KeyboardState state)
		{
			tracker.Update(state);
		}

		bool KeyboardTracker::IsKeyPressed(Key key) const
		{
			return tracker.IsKeyPressed(key);
		}

		bool KeyboardTracker::IsKeyReleased(Key key) const 
		{
			return tracker.IsKeyReleased(key);
		}

		KeyboardState KeyboardTracker::GetCurrentState() const {
			KeyboardState state;
			state = tracker.GetLastState();
			return state;
		}

		void KeyboardTracker::Reset() {
			tracker.Reset();
		}

	}
}
