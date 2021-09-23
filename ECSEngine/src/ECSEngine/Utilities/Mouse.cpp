#include "ecspch.h"
#include "Mouse.h"

namespace ECSEngine {

	namespace HID {

		bool MouseState::LeftButton() const
		{
			return state.leftButton;
		}

		bool MouseState::RightButton() const {
			return state.rightButton;
		}

		bool MouseState::MiddleButton() const {
			return state.middleButton;
		}

		bool MouseState::XButton1() const {
			return state.xButton1;
		}

		bool MouseState::XButton2() const {
			return state.xButton2;
		}

		int MouseState::PositionX() const {
			return state.x;
		}

		int MouseState::PositionY() const {
			return state.y;
		}

		int MouseState::MouseWheelScroll() const {
			return state.scrollWheelValue;
		}

		int MouseState::GetPreviousX() const
		{
			return m_previous_x;
		}

		int MouseState::GetPreviousY() const {
			return m_previous_y;
		}

		int MouseState::GetDeltaX() const {
			return PositionX() - m_previous_x;
		}

		int MouseState::GetDeltaY() const {
			return PositionY() - m_previous_y;
		}

		void MouseState::SetPreviousPosition()
		{
			m_previous_x = PositionX();
			m_previous_y = PositionY();
		}

		void MouseTracker::Reset() {
			tracker.Reset();
		}

		void MouseTracker::Update(const MouseState& state) {
			tracker.Update(state.state);
		}

		MouseButtonState MouseTracker::LeftButton() const
		{
			return MouseButtonState(tracker.leftButton);
		}

		MouseButtonState MouseTracker::RightButton() const
		{
			return MouseButtonState(tracker.rightButton);
		}

		MouseButtonState MouseTracker::MiddleButton() const
		{
			return MouseButtonState(tracker.middleButton);
		}

		MouseButtonState MouseTracker::XButton1() const
		{
			return MouseButtonState(tracker.xButton1);
		}

		MouseButtonState MouseTracker::XButton2() const
		{
			return MouseButtonState(tracker.xButton2);
		}

		void Mouse::AttachToProcess(const MouseProcessAttachment& info) {
			m_implementation->SetWindow(info.hWnd);
		}

		MouseState* Mouse::GetState() {
			return &m_state;
		}

		MouseTracker* Mouse::GetTracker() {
			return &m_tracker;
		}

		Mouse::Mouse() {
			m_implementation = std::make_unique<DirectX::Mouse>();
			m_tracker.Reset();
		}

		int Mouse::GetScrollValue() const
		{
			return m_state.state.scrollWheelValue;
		}

		int Mouse::GetPreviousMouseX() const
		{
			return m_state.GetPreviousX();
		}

		int Mouse::GetPreviousMouseY() const
		{
			return m_state.GetPreviousY();
		}

		int Mouse::GetDeltaX() const
		{
			return m_state.GetDeltaX();
		}

		int Mouse::GetDeltaY() const
		{
			return m_state.GetDeltaY();
		}

		void Mouse::GetMousePosition(int& x, int& y) const
		{
			x = m_state.state.x;
			y = m_state.state.y;
		}

		bool Mouse::IsCursorVisible() const {
			return m_implementation->IsVisible();
		}

		void Mouse::ResetCursorWheel() {
			m_implementation->ResetScrollWheelValue();
		}

		void Mouse::ResetTracker() {
			m_tracker.Reset();
		}

		void Mouse::SetAbsoluteMode() {
			m_implementation->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
		}

		void Mouse::SetRelativeMode() {
			m_implementation->SetMode(DirectX::Mouse::MODE_RELATIVE);
		}

		void Mouse::SetCursorInvisible() {
			m_implementation->SetVisible(false);
		}

		void Mouse::SetPreviousPosition()
		{
			m_state.SetPreviousPosition();
		}

		void Mouse::SetCursorVisible() {
			m_implementation->SetVisible(true);
		}

		void Mouse::UpdateTracker() {
			m_tracker.Update(m_state);
		}

		void Mouse::UpdateState() {
			m_state = { m_implementation->GetState() };
		}

		void Mouse::Procedure(const MouseProcedureInfo& info) {
			m_implementation->ProcessMessage(info.message, info.wParam, info.lParam);
		}

	}

}