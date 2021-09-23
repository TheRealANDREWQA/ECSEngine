#pragma once
#include "../Core.h"
#include "../../../Dependencies/DirectXTK/Inc/Keyboard.h"
#include "../Containers/Queues.h"
#include "../Allocators/MemoryManager.h"

#define ECS_KEYBOARD_CHARACTER_QUEUE_DEFAULT_SIZE 256

namespace ECSEngine {

	namespace HID {

		using KeyboardCharacterQueue = containers::ResizableQueue<char, GlobalMemoryManager, true>;

		struct ECSENGINE_API KeyboardProcedureInfo {
			UINT message;
			WPARAM wParam;
			LPARAM lParam;
		};

		using Key = DirectX::Keyboard::Keys;
		using KeyboardState = DirectX::Keyboard::State;

		struct ECSENGINE_API KeyboardTracker {
			void Update(KeyboardState state);
			bool IsKeyPressed(Key key) const;
			bool IsKeyReleased(Key key) const;
			void Reset();
			KeyboardState GetCurrentState() const;

			DirectX::Keyboard::KeyboardStateTracker tracker;
		};

		class ECSENGINE_API Keyboard
		{
		public:
			Keyboard();
			Keyboard(GlobalMemoryManager* allocator);

			inline void CaptureCharacters() {
				m_process_characters = true;
			}

			inline void DoNotCaptureCharacters() {
				m_process_characters = false;
			}

			inline bool GetCharacter(char& character) {
				return m_character_queue.Pop(character);
			}

			inline const KeyboardState* GetState() const {
				return &m_frame_state;
			}

			inline KeyboardTracker* GetTracker() {
				return &m_tracker;
			}

			inline bool IsKeyPressed(Key key) const {
				return m_tracker.IsKeyPressed(key);
			}

			inline bool IsKeyReleased(Key key) const {
				return m_tracker.IsKeyReleased(key);
			}

			inline bool IsKeyDown(Key key) const {
				return m_frame_state.IsKeyDown(key);
			}

			inline bool IsKeyUp(Key key) const {
				return m_frame_state.IsKeyUp(key);
			}

			void UpdateState();

			void UpdateTracker();

			void Procedure(const KeyboardProcedureInfo& info);

		private:
			std::unique_ptr<DirectX::Keyboard> m_implementation;
			KeyboardState m_frame_state;
			KeyboardTracker m_tracker;
			KeyboardCharacterQueue m_character_queue;
			bool m_process_characters;
		};
	}

}

