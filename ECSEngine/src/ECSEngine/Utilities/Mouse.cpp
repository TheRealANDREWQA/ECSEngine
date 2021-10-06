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

		int2 MouseState::Position() const {
			return {state.x, state.y};
		}

		int2 MouseState::PreviousPosition() const {
			return previous_position;
		}

		int MouseState::PreviousScroll() const {
			return previous_scroll;
		}

		int2 MouseState::PositionDelta() const {
			return int2(state.x, state.y) - previous_position;
		}

		int MouseState::ScrollDelta() const {
			return state.scrollWheelValue - previous_scroll;
		}

		int MouseState::MouseWheelScroll() const {
			return state.scrollWheelValue;
		}

		void MouseState::SetPreviousPosition()
		{
			previous_position.x = Position().x;
			previous_position.y = Position().y;
		}

		void MouseState::SetPreviousScroll()
		{
			previous_scroll = state.scrollWheelValue;
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
			m_state.previous_position = { 0, 0 };
			m_state.previous_scroll = 0;
			m_state.state.x = 0;
			m_state.state.y = 0;
			m_state.state.scrollWheelValue = 0;
		}

		int Mouse::GetScrollValue() const
		{
			return m_state.state.scrollWheelValue;
		}

		int2 Mouse::GetPreviousPosition() const
		{
			return m_state.PreviousPosition();
		}

		int Mouse::GetPreviousScroll() const
		{
			return m_state.PreviousScroll();
		}

		int2 Mouse::GetPositionDelta() const
		{
			return m_state.PositionDelta();
		}
		
		int Mouse::GetScrollDelta() const
		{
			return m_state.ScrollDelta();
		}

		int2 Mouse::GetPosition() const
		{
			return m_state.Position();
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

		void Mouse::SetPreviousPositionAndScroll()
		{
			m_state.SetPreviousPosition();
			m_state.SetPreviousScroll();
		}

		void Mouse::SetCursorVisible() {
			m_implementation->SetVisible(true);
		}

		void Mouse::UpdateTracker() {
			m_tracker.Update(m_state);
		}

		void Mouse::UpdateState() {
			m_state.state = m_implementation->GetState();
		}

		void Mouse::Procedure(const MouseProcedureInfo& info) {
			m_implementation->ProcessMessage(info.message, info.wParam, info.lParam);
		}

	}

}