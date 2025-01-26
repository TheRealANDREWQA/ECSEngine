// ECS_REFLECT
#include "editorpch.h"
#include "ECSEngine.h"
#include "../../resource.h"
#include "../UI/Hub.h"
#include "../UI/Game.h"
#include "../UI/Scene.h"
#include "EditorParameters.h"
#include "EditorState.h"
#include "EditorEvent.h"
#include "EditorPalette.h"
#include "EntryPoint.h"
#include "../Sandbox/Sandbox.h"
#include "../UI/VisualizeTexture.h"
#include "ECSEngineBenchmark.h"
#include "ECSEngineOSMonitor.h"
#include <hidusage.h>

#define ERROR_BOX_MESSAGE WM_USER + 1
#define ERROR_BOX_CODE -2

using namespace ECSEngine;
using namespace ECSEngine::Tools;

int ExceptionFilter(EXCEPTION_POINTERS* pointers) {
	OS::ExceptionInformation exception_information = OS::GetExceptionInformationFromNative(pointers);
	__debugbreak();
	return EXCEPTION_EXECUTE_HANDLER;
}

class Editor : public ECSEngine::Application {
public:
	Editor(const wchar_t* name);
	Editor() {}
	~Editor();
	Editor(const Editor&) = delete;
	Editor& operator = (const Editor&) = delete;

public:

	void ChangeCursor(ECSEngine::ECS_CURSOR_TYPE type) override {
		if (type != current_cursor) {
			SetCursor(cursors[(unsigned int)type]);
			SetClassLong(hWnd, -12, (LONG)cursors[(unsigned int)type]);
			current_cursor = type;
		}
	}

	ECSEngine::ECS_CURSOR_TYPE GetCurrentCursor() const override {
		return current_cursor;
	}

	template<typename CharacterType>
	void WriteTextToClipboardImpl(Stream<CharacterType> text) {
		assert(OpenClipboard(hWnd) == TRUE);
		assert(EmptyClipboard() == TRUE);

		HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (text.size + 1) * sizeof(CharacterType));
		LPVOID new_handle = GlobalLock(handle);
		text.CopyTo(new_handle);
		CharacterType* reinterpretation = (CharacterType*)new_handle;
		reinterpretation[text.size] = Character<CharacterType>('\0');
		GlobalUnlock(handle);
		SetClipboardData(std::is_same_v<CharacterType, char> ? CF_TEXT : CF_UNICODETEXT, handle);
		assert(CloseClipboard() == TRUE);
	}

	void WriteTextToClipboard(Stream<char> text) override {
		WriteTextToClipboardImpl<char>(text);
	}

	void WriteTextToClipboard(Stream<wchar_t> text) override {
		WriteTextToClipboardImpl<wchar_t>(text);
	}

	template<typename CharacterType>
	unsigned int CopyTextFromClipboardImpl(CapacityStream<CharacterType>* text) {
		int format = std::is_same_v<CharacterType, char> ? CF_TEXT : CF_UNICODETEXT;
		if (IsClipboardFormatAvailable(format)) {
			assert(OpenClipboard(hWnd) == TRUE);
			HANDLE global_handle = GetClipboardData(format);
			HANDLE data_handle = GlobalLock(global_handle);
			unsigned int copy_count = std::is_same_v<CharacterType, char> ? strlen((const char*)data_handle) : wcslen((const wchar_t*)data_handle);
			copy_count = ClampMax(copy_count, text->capacity - text->size);
			text->AddStreamAssert(Stream<CharacterType>((const CharacterType*)data_handle, copy_count));
			GlobalUnlock(global_handle);
			assert(CloseClipboard() == TRUE);
			return copy_count;
		}
		else {
			return 0;
		}
	}

	unsigned int CopyTextFromClipboard(CapacityStream<char>* text) override {
		return CopyTextFromClipboardImpl<char>(text);
	}

	unsigned int CopyTextFromClipboard(CapacityStream<wchar_t>* text) override {
		return CopyTextFromClipboardImpl<wchar_t>(text);
	}

	ECS_INLINE void* GetOSWindowHandle() const override {
		return hWnd;
	}

	uint2 GetCursorPosition() const {
		return ECSEngine::OS::GetCursorPosition(GetOSWindowHandle());
	}

	void SetCursorPosition(uint2 position) {
		ECSEngine::OS::SetCursorPosition(GetOSWindowHandle(), position);
	}

	void SetCursorPositionRelative(uint2 position) {
		ECSEngine::OS::SetCursorPositionRelative(GetOSWindowHandle(), position);
	}

	int Run() override {
		using namespace ECSEngine;
		using namespace ECSEngine::Tools;

		EditorStateBaseInitialize(&editor_state, hWnd, &mouse, &keyboard);
		EditorStateInitialize(this, &editor_state, hWnd, &mouse, &keyboard, monitor_size);

		is_editor_state_initialized = true;

		ResourceManager* resource_manager = editor_state.ui_resource_manager;
		Graphics* graphics = editor_state.UIGraphics();

		Hub(&editor_state);

		size_t count = 0;
		auto filter = [&](EXCEPTION_POINTERS* pointers) {
			auto native_info = OS::GetExceptionInformationFromNative(pointers);
			count++;
			return EXCEPTION_CONTINUE_EXECUTION;
		};

		MSG message;
		BOOL result = 0;

		while (true) {
			auto run_application = [&](EDITOR_APPLICATION_QUIT_RESPONSE application_quit_value) {
				ECS_UI_FRAME_PACING frame_pacing = ECS_UI_FRAME_PACING_NONE;
				while (result == 0 && application_quit == application_quit_value) {
					if (editor_state.Mouse()->GetRawInputStatus()) {
						// Process raw input
						ProcessMouseRawInput(editor_state.Mouse());
					}

					while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
						switch (message.message) {
						case WM_QUIT:
							result = -1;
							break;
						}
						TranslateMessage(&message);
						bool should_process_message = true;
						if (editor_state.Mouse()->GetRawInputStatus()) {
							// Drop mouse messages - rely only on raw input
							if (message.message == WM_MOUSEMOVE || message.message == WM_NCMOUSEMOVE || message.message == WM_LBUTTONDOWN ||
								message.message == WM_LBUTTONUP || message.message == WM_MBUTTONDOWN || message.message == WM_MBUTTONUP ||
								message.message == WM_RBUTTONUP || message.message == WM_RBUTTONDOWN || message.message == WM_XBUTTONDOWN ||
								message.message == WM_XBUTTONUP || message.message == WM_NCLBUTTONDOWN || message.message == WM_NCLBUTTONUP ||
								message.message == WM_NCMBUTTONDOWN || message.message == WM_NCMBUTTONUP || message.message == WM_NCRBUTTONUP ||
								message.message == WM_NCRBUTTONDOWN || message.message == WM_NCXBUTTONDOWN || message.message == WM_NCXBUTTONUP ||
								message.message == WM_NCHITTEST || message.message == WM_NCMOUSEHOVER || message.message == WM_MOUSEHOVER ||
								message.message == WM_MOUSEWHEEL || message.message == WM_MOUSEHWHEEL) {
								should_process_message = false;
							}
						}
						else {
							// Drop raw input messages while we do not want them
							if (message.message == WM_INPUT) {
								should_process_message = false;
							}
						}
						if (should_process_message) {
							DispatchMessage(&message);
						}
					}

					auto handle_physical_memory_guards = [this](EXCEPTION_POINTERS* exception_pointers) {
						OS::ExceptionInformation exception_information = OS::GetExceptionInformationFromNative(exception_pointers);
						if (exception_information.error_code == OS::ECS_OS_EXCEPTION_PAGE_GUARD) {
							// We are not interested in temporary sandboxes since those should not have
							// physical memory profiling activated
							unsigned int sandbox_count = GetSandboxCount(&editor_state, true);
							unsigned int index = 0;
							for (; index < sandbox_count; index++) {
								EditorSandbox* sandbox = GetSandbox(&editor_state, index);
								if (sandbox->world_profiling.HasOption(ECS_WORLD_PROFILING_PHYSICAL_MEMORY)) {
									// We can use thread_id 0 here since we are on the main thread
									bool was_handled = sandbox->world_profiling.physical_memory_profiler.HandlePageGuardEnter(0, exception_information.faulting_page);
									if (was_handled) {
										break;
									}
								}
							}
							
							if (index == sandbox_count) {
								// It doesn't belong to the physical memory profiler range
								return EXCEPTION_CONTINUE_SEARCH;
							}
							else {
								// We can continue the execution
								return EXCEPTION_CONTINUE_EXECUTION;
							}
						}
						else {
							return EXCEPTION_CONTINUE_SEARCH;
						}
					};

					timer.SetNewStart();

					// The main thread might run into page guards that the physical memory places
					// So, we need to wrap this code into a section and check all sandboxes for their
					// physical memory profilers in case the exception is of type PAGE_GUARD
					__try {
						// We need to acquire the proper locks for the frame. When rendering the UI, we need
						// The lock only on the Graphics object, when running the ticks, we also need the resource manager lock.
						if (!IsIconic(hWnd)) {
							editor_state.frame_gpu_lock.Lock();

							// Change the main render target to that of the swap chain
							graphics->ChangeMainRenderTargetToInitial();

							timer.SetNewStart();
							// Here write the Game/Scene windows to be focused, i.e. be drawn every single frame,
							// While having all the other windows be drawn at a lesser frequency
							//ECS_STACK_CAPACITY_STREAM(char, focused_window_chars, ECS_KB * 2);
							//ECS_STACK_CAPACITY_STREAM(Stream<char>, focused_windows, 64);
							//unsigned int sandbox_count = GetSandboxCount(&editor_state, true);
							//for (unsigned int index = 0; index < sandbox_count; index++) {
							//	unsigned int current_start = focused_window_chars.size;
							//	GetSceneUIWindowName(index, focused_window_chars);
							//	focused_windows.AddAssert({ focused_window_chars.buffer + current_start, focused_window_chars.size - current_start });

							//	current_start = focused_window_chars.size;
							//	GetGameUIWindowName(index, focused_window_chars);
							//	focused_windows.AddAssert({ focused_window_chars.buffer + current_start, focused_window_chars.size - current_start });
							//}
							//// We also need to add the visualize texture window
							//focused_windows.AddAssert(VISUALIZE_TEXTURE_WINDOW_NAME);

							// At the moment use the classical drawing since some windows have problems
							// TODO: Almost all windows are ready to use retained drawing or need slight
							// Modifications. Consider enabling selective drawing
							frame_pacing = editor_state.ui_system->DoFrame();
							//frame_pacing = editor_state.ui_system->DoFrame({ focused_windows, 33 });

							/*float duration = timer.GetDuration(ECS_TIMER_DURATION_US);
							if (duration < 5000) {
								average = average * average_count + duration;
								average_count++;
								average /= average_count;
							}*/

							if (AreSandboxesBeingRun(&editor_state)) {
								frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
							}

							// Refresh the graphics object since it might be changed
							graphics = editor_state.UIGraphics();

							bool removed = graphics->SwapBuffers(0);
							if (removed) {
								__debugbreak();
							}

							editor_state.frame_gpu_lock.Unlock();
						}

						// Acquire both the resource manager and the Graphics locks, in this order
						editor_state.RuntimeResourceManager()->Lock();
						editor_state.frame_gpu_lock.Lock();

						// Run the tick after the UI. At the moment, the mouse/keyboard sandbox
						// Input is based on this ordering
						editor_state.Tick();

						// Release the locks
						editor_state.frame_gpu_lock.Unlock();
						editor_state.RuntimeResourceManager()->Unlock();

						mouse.Update();
						keyboard.Update();
					}
					__except (handle_physical_memory_guards(GetExceptionInformation())) {}

					// Use milliseconds as unit of measure
					float frame_duration_ms = timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
					frame_pacing = max(frame_pacing, editor_state.elevated_frame_pacing);
					float target_duration_ms = 1000.0f / (float)FRAME_PACING_FPS[frame_pacing];

					if (frame_duration_ms < target_duration_ms) {
						std::this_thread::sleep_for(std::chrono::milliseconds((size_t)(target_duration_ms - frame_duration_ms)));
					}
				}
			};

			run_application(EDITOR_APPLICATION_QUIT_APPROVED);

			EditorStateApplicationQuit(&editor_state, &application_quit);

			run_application(EDITOR_APPLICATION_QUIT_NOT_READY);
			if (application_quit == EDITOR_APPLICATION_QUIT_APPROVED) {
				break;
			}

			application_quit = EDITOR_APPLICATION_QUIT_APPROVED;
		}

		EditorStateBeforeExitCleanup(&editor_state);

		if (result == -1)
			return -1;
		else
			return message.wParam;
	}

	private:
		static LRESULT CALLBACK HandleMessageSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
		static LRESULT CALLBACK HandleMessageForward(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
		LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;

		//int width;
		//int height;
		HWND hWnd;
		ECSEngine::Timer timer;
		ECSEngine::Mouse mouse;
		ECSEngine::Keyboard keyboard;
		ECSEngine::Stream<HCURSOR> cursors;
		ECSEngine::ECS_CURSOR_TYPE current_cursor;
		EDITOR_APPLICATION_QUIT_RESPONSE application_quit;
		EditorState editor_state;
		bool is_editor_state_initialized = false;
		// This is a handle to the monitor that the window is currently being displayed on
		void* monitor_handle;
		uint2 monitor_size;

		// singleton that manages registering and unregistering the editor class
		class EditorClass {
		public:
			static LPCWSTR GetName() noexcept;
			static HINSTANCE GetInstance() noexcept;
			EditorClass() noexcept;
			~EditorClass();
		private:
			EditorClass(const EditorClass& ) = delete;
			EditorClass& operator = (const EditorClass&) = delete;
			static constexpr LPCWSTR editorClassName = L"Editor Window";
			static EditorClass editorInstance;
			HINSTANCE hInstance;
		};
}; // Editor

Editor::EditorClass Editor::EditorClass::editorInstance;
Editor::EditorClass::EditorClass() noexcept : hInstance( GetModuleHandle(nullptr))  {
	WNDCLASSEX editor_class = { 0 };
	editor_class.cbSize = sizeof(editor_class);
	editor_class.style = CS_OWNDC;
	editor_class.lpfnWndProc = HandleMessageSetup;
	editor_class.cbClsExtra = 0;
	editor_class.cbWndExtra = 0;
	editor_class.hInstance = GetInstance();
	editor_class.hIcon = (HICON)( LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 128, 128, 0 ));
	editor_class.hCursor = nullptr;
	editor_class.hbrBackground = nullptr;
	editor_class.lpszMenuName = nullptr;
	editor_class.lpszClassName = GetName();
	editor_class.hIconSm = (HICON)(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0));
	
	// Set application thread DPI to system aware - we are using DX11 to do the scaling
	DPI_AWARENESS_CONTEXT context = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	RegisterClassEx(&editor_class);
}
Editor::EditorClass::~EditorClass() {
	UnregisterClass(editorClassName, GetInstance());
}
HINSTANCE Editor::EditorClass::GetInstance() noexcept {
	return editorInstance.hInstance;
}
LPCWSTR Editor::EditorClass::GetName() noexcept {
	return editorClassName;
}

Editor::Editor(const wchar_t* name)
{
	timer.SetUninitialized();
	timer.SetNewStart();
	application_quit = EDITOR_APPLICATION_QUIT_APPROVED;

	monitor_handle = OS::GetMonitorFromCursor();
	OS::MonitorInfo monitor_info = OS::RetrieveMonitorInfo(monitor_handle);

	// Use the work area such that we don't overlapp system UI
	monitor_size = monitor_info.size;

	//width = _width;
	//height = _height;

	HCURSOR* cursor_stream = new HCURSOR[ECS_CURSOR_COUNT];

	// hInstance is null because these are predefined cursors
	cursors = Stream<HCURSOR>(cursor_stream, ECS_CURSOR_COUNT);
	cursors[ECS_CURSOR_APP_STARTING] = LoadCursor(NULL, IDC_APPSTARTING);
	cursors[ECS_CURSOR_CROSS] = LoadCursor(NULL, IDC_CROSS);
	cursors[ECS_CURSOR_DEFAULT] = LoadCursor(NULL, IDC_ARROW);
	cursors[ECS_CURSOR_HAND] = LoadCursor(NULL, IDC_HAND);
	cursors[ECS_CURSOR_HELP] = LoadCursor(NULL, IDC_HELP);
	cursors[ECS_CURSOR_IBEAM] = LoadCursor(NULL, IDC_IBEAM);
	cursors[ECS_CURSOR_SIZE_ALL] = LoadCursor(NULL, IDC_SIZEALL);
	cursors[ECS_CURSOR_SIZE_EW] = LoadCursor(NULL, IDC_SIZEWE);
	cursors[ECS_CURSOR_SIZE_NESW] = LoadCursor(NULL, IDC_SIZENESW);
	cursors[ECS_CURSOR_SIZE_NS] = LoadCursor(NULL, IDC_SIZENS);
	cursors[ECS_CURSOR_SIZE_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
	cursors[ECS_CURSOR_WAIT] = LoadCursor(NULL, IDC_WAIT);

	ChangeCursor(ECS_CURSOR_DEFAULT);

	// create window and get Handle
	hWnd = CreateWindow(
		EditorClass::GetName(),
		name,
		WS_OVERLAPPEDWINDOW,
		monitor_info.work_origin.x,
		monitor_info.work_origin.y,
		monitor_info.work_size.x,
		monitor_info.work_size.y,
		nullptr,
		nullptr,
		EditorClass::GetInstance(),
		this
	);

	// Register raw mouse input
	RAWINPUTDEVICE raw_device = {};
	raw_device.usUsagePage = HID_USAGE_PAGE_GENERIC;
	raw_device.usUsage = HID_USAGE_GENERIC_MOUSE;
	raw_device.dwFlags = 0;
	raw_device.hwndTarget = nullptr;
	if (!RegisterRawInputDevices(&raw_device, 1, sizeof(raw_device))) {
		abort();
	}

	int2 cursor_position = OS::GetCursorPosition(hWnd);
	// Temporarly set the hWnd to this one
	mouse.m_window_handle = hWnd;
	mouse.SetPosition(cursor_position.x, cursor_position.y);
	mouse.m_window_handle = nullptr;

	// Show window since default is hidden
	ShowWindow(hWnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hWnd);

	ECS_ASSERT(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE));
}

Editor::~Editor() {
	delete[] cursors.buffer;
	DestroyWindow(hWnd);
}

LRESULT WINAPI Editor::HandleMessageSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	if (message == WM_NCCREATE) {
		// extract pointer to window class from creation data
		const CREATESTRUCTW* createStruct = (CREATESTRUCTW*)(lParam);
		Editor* editor_pointer = (Editor*)(createStruct->lpCreateParams);

		// set WinAPI managed user data to store pointer to editor class
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)(editor_pointer));

		// set WinAPI procedure to forwarder
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)(&Editor::HandleMessageForward));

		// forward message
		return editor_pointer->HandleMessageForward(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT WINAPI Editor::HandleMessageForward(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	// retrieve pointer to editor class
	Editor* editor = (Editor*)(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	return editor->HandleMessage(hWnd, message, wParam, lParam);
}

LRESULT Editor::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	switch (message) {
	case WM_CLOSE:
		application_quit = EDITOR_APPLICATION_QUIT_NOT_READY;
		return 0;
	case WM_ACTIVATEAPP:
		keyboard.Procedure({ message, wParam, lParam });
		mouse.Procedure({ message, wParam, lParam });
		break;
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
		mouse.Procedure({ message, wParam, lParam }); 
		break;
	case WM_CHAR:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_SYSKEYDOWN:
		keyboard.Procedure({ message, wParam, lParam });
		break;
	case WM_SETCURSOR:
		if (cursors[(unsigned int)current_cursor] != GetCursor()) {
			SetCursor(cursors[(unsigned int)current_cursor]);
		}
		break;
	case WM_SIZE: {
		if (is_editor_state_initialized) {
			// Retrieve the monitor handle for the current cursor position
			// If the monitor handle has changed, we must change the aspect ratio
			// Of the UI
			void* current_monitor = OS::GetMonitorFromCursor();
			if (current_monitor != monitor_handle) {
				monitor_handle = current_monitor;
				OS::MonitorInfo monitor_info = OS::RetrieveMonitorInfo(current_monitor);
				monitor_size = monitor_info.size;
			}

			unsigned int new_width = LOWORD(lParam);
			unsigned int new_height = HIWORD(lParam);
			// In some weird cases, the monitor handle that is returned is empty
			// And the monitor size would be { 0, 0 }, which would case an incorrect
			// Resizing to take place. Protect from this case			
			if (monitor_size.x != 0 && monitor_size.y != 0) {
				editor_state.ui_system->SetWindowOSSize({ new_width, new_height }, monitor_size);
				editor_state.UIGraphics()->SetNewSize(hWnd, new_width, new_height);
			}
		}
		break;
	}
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

ECSEngine::Application* CreateApplication(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	return new Editor(L"ECSEngine Editor");
}
ECSEngine::Application* CreateApplication() {
	return new Editor(L"ECSEngine Editor");
}