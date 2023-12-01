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

#define ERROR_BOX_MESSAGE WM_USER + 1
#define ERROR_BOX_CODE -2

// After this amount of milliseconds has passed, the ui system will make a full draw
// Instead of redraws
#define IS_PLAYING_DO_FRAME_MILLISECONDS 16

using namespace ECSEngine;
using namespace ECSEngine::Tools;

int ExceptionFilter(EXCEPTION_POINTERS* pointers) {
	OS::ExceptionInformation exception_information = OS::GetExceptionInformationFromNative(pointers);
	__debugbreak();
	return EXCEPTION_EXECUTE_HANDLER;
}

class Editor : public ECSEngine::Application {
public:
	Editor(int width, int height, const wchar_t* name);
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

	void WriteTextToClipboard(const char* text) override {
		assert(OpenClipboard(hWnd) == TRUE);
		assert(EmptyClipboard() == TRUE);

		size_t text_size = strlen((const char*)text);
		auto handle = GlobalAlloc(GMEM_MOVEABLE, text_size + 1);
		auto new_handle = GlobalLock(handle);
		memcpy(new_handle, text, sizeof(unsigned char) * text_size);
		char* reinterpretation = (char*)new_handle;
		reinterpretation[text_size] = '\0';
		GlobalUnlock(handle);
		SetClipboardData(CF_TEXT, handle);
		assert(CloseClipboard() == TRUE);
	}

	unsigned int CopyTextFromClipboard(char* text, unsigned int max_size) override {
		if (IsClipboardFormatAvailable(CF_TEXT)) {
			assert(OpenClipboard(hWnd) == TRUE);
			HANDLE global_handle = GetClipboardData(CF_TEXT);
			HANDLE data_handle = GlobalLock(global_handle);
			unsigned int copy_count = strlen((const char*)data_handle);
			copy_count = copy_count > max_size - 1 ? max_size - 1 : copy_count;
			memcpy(text, data_handle, copy_count);
			GlobalUnlock(global_handle);
			assert(CloseClipboard() == TRUE);
			return copy_count;
		}
		else {
			text = (char*)"";
			return 0;
		}
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

		EditorState editor_state;
		EditorStateBaseInitialize(&editor_state, hWnd, &mouse, &keyboard);
		EditorStateInitialize(this, &editor_state, hWnd, &mouse, &keyboard);

		ResourceManager* resource_manager = editor_state.ui_resource_manager;
		Graphics* graphics = editor_state.UIGraphics();

		Hub(&editor_state);

		size_t count = 0;
		auto filter = [&](EXCEPTION_POINTERS* pointers) {
			auto native_info = OS::GetExceptionInformationFromNative(pointers);
			count++;
			return EXCEPTION_CONTINUE_EXECUTION;
		};

		/*void* malloced = malloc(ECS_GB);
		OS::GuardPages(malloced, ECS_GB);
		__try {
			memset(malloced, 1, 1);
		}
		__except (filter(GetExceptionInformation())) {

		}*/

		MSG message;
		BOOL result = 0;

		while (true) {
			auto run_application = [&](EDITOR_APPLICATION_QUIT_RESPONSE application_quit_value) {
				while (result == 0 && application_quit == application_quit_value) {
					while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
						switch (message.message) {
						case WM_QUIT:
							result = -1;
							break;
						}
						TranslateMessage(&message);
						DispatchMessage(&message);
					}

					unsigned int frame_pacing = 0;

					auto handle_physical_memory_guards = [&editor_state](EXCEPTION_POINTERS* exception_pointers) {
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

					// The main thread might run into page guards that the physical memory places
					// So, we need to wrap this code into a section and check all sandboxes for their
					// physical memory profilers in case the exception is of type PAGE_GUARD
					__try {
						if (!IsIconic(hWnd)) {
							graphics->BindRenderTargetViewFromInitialViews();

							/*static float average = 0.0f;
							static int average_count = 0;*/

							timer.SetNewStart();
							// Here write the Game/Scene windows to be focused, i.e. be drawn every single frame,
							// While having all the other windows be drawn at a lesser frequency
							ECS_STACK_CAPACITY_STREAM(char, focused_window_chars, ECS_KB * 2);
							ECS_STACK_CAPACITY_STREAM(Stream<char>, focused_windows, 64);
							unsigned int sandbox_count = GetSandboxCount(&editor_state, true);
							for (unsigned int index = 0; index < sandbox_count; index++) {
								unsigned int current_start = focused_window_chars.size;
								GetSceneUIWindowName(index, focused_window_chars);
								focused_windows.AddAssert({ focused_window_chars.buffer + current_start, focused_window_chars.size - current_start });

								current_start = focused_window_chars.size;
								GetGameUIWindowName(index, focused_window_chars);
								focused_windows.AddAssert({ focused_window_chars.buffer + current_start, focused_window_chars.size - current_start });
							}
							// We also need to add the visualize texture window
							focused_windows.AddAssert(VISUALIZE_TEXTURE_WINDOW_NAME);

							// At the moment use the classical drawing since some windows have problems
							// With the retained snapshots where invalid references are kept when they shouldn't
							// For example, the inspector can have snapshot runnables on text inputs and when the
							// entity is changed the runnable will refer to an invalid field
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
						}

						// Run the tick after the UI. At the moment, the mouse/keyboard sandbox
						// Input is based on this ordering
						editor_state.Tick();

						mouse.Update();
						keyboard.Update();
					}
					__except (handle_physical_memory_guards(GetExceptionInformation())) {}

					std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_AMOUNT[frame_pacing]));
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

//ECS_OPTIMIZE_END;

Editor::EditorClass Editor::EditorClass::editorInstance;
Editor::EditorClass::EditorClass() noexcept : hInstance( GetModuleHandle(nullptr))  {
	WNDCLASSEX editor_class = { 0 };
	editor_class.cbSize = sizeof(editor_class);
	editor_class.style = CS_OWNDC;
	editor_class.lpfnWndProc = HandleMessageSetup;
	editor_class.cbClsExtra = 0;
	editor_class.cbWndExtra = 0;
	editor_class.hInstance = GetInstance();
	editor_class.hIcon = static_cast<HICON>( LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 128, 128, 0 ));
	editor_class.hCursor = nullptr;
	editor_class.hbrBackground = nullptr;
	editor_class.lpszMenuName = nullptr;
	editor_class.lpszClassName = GetName();
	editor_class.hIconSm = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0));;
	
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

Editor::Editor(int _width, int _height, const wchar_t* name)
{
	timer.SetUninitialized();
	timer.SetNewStart();
	application_quit = EDITOR_APPLICATION_QUIT_APPROVED;

	// calculate window size based on desired client region
	RECT windowRegion;
	windowRegion.left = 0;
	windowRegion.right = _width;
	windowRegion.bottom = _height;
	windowRegion.top = 0;
	AdjustWindowRect(&windowRegion, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE);

	//width = _width;
	//height = _height;

	HCURSOR* cursor_stream = new HCURSOR[ECS_CURSOR_COUNT];

	// hInstance is null because these are predefined cursors
	cursors = Stream<HCURSOR>(cursor_stream, ECS_CURSOR_COUNT);
	cursors[(unsigned int)ECS_CURSOR_APP_STARTING] = LoadCursor(NULL, IDC_APPSTARTING);
	cursors[(unsigned int)ECS_CURSOR_CROSS] = LoadCursor(NULL, IDC_CROSS);
	cursors[(unsigned int)ECS_CURSOR_DEFAULT] = LoadCursor(NULL, IDC_ARROW);
	cursors[(unsigned int)ECS_CURSOR_HAND] = LoadCursor(NULL, IDC_HAND);
	cursors[(unsigned int)ECS_CURSOR_HELP] = LoadCursor(NULL, IDC_HELP);
	cursors[(unsigned int)ECS_CURSOR_IBEAM] = LoadCursor(NULL, IDC_IBEAM);
	cursors[(unsigned int)ECS_CURSOR_SIZE_ALL] = LoadCursor(NULL, IDC_SIZEALL);
	cursors[(unsigned int)ECS_CURSOR_SIZE_EW] = LoadCursor(NULL, IDC_SIZEWE);
	cursors[(unsigned int)ECS_CURSOR_SIZE_NESW] = LoadCursor(NULL, IDC_SIZENESW);
	cursors[(unsigned int)ECS_CURSOR_SIZE_NS] = LoadCursor(NULL, IDC_SIZENS);
	cursors[(unsigned int)ECS_CURSOR_SIZE_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
	cursors[(unsigned int)ECS_CURSOR_WAIT] = LoadCursor(NULL, IDC_WAIT);

	ChangeCursor(ECS_CURSOR_DEFAULT);

	// create window and get Handle
	hWnd = CreateWindow(
		EditorClass::GetName(),
		name,
		WS_OVERLAPPEDWINDOW /*^ WS_THICKFRAME*/,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRegion.right - windowRegion.left,
		windowRegion.bottom - windowRegion.top,
		nullptr,
		nullptr,
		EditorClass::GetInstance(),
		this
	);

	// Register raw mouse input
	RAWINPUTDEVICE raw_device = {};
	raw_device.usUsagePage = 0x01; // mouse page
	raw_device.usUsage = 0x02; // mouse usage
	raw_device.dwFlags = 0;
	raw_device.hwndTarget = nullptr;
	if (!RegisterRawInputDevices(&raw_device, 1, sizeof(raw_device))) {
		abort();
	}

	uint2 cursor_position = OS::GetCursorPosition(hWnd);
	// Temporarly set the hWnd to this one
	mouse.m_window_handle = hWnd;
	mouse.SetPosition(cursor_position.x, cursor_position.y);
	mouse.m_window_handle = nullptr;

	// show window since default is hidden
	ShowWindow(hWnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hWnd);

	ECS_ASSERT(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE));
	GetWindowRect(hWnd, &windowRegion);
}

Editor::~Editor() {
	delete[] cursors.buffer;
	DestroyWindow(hWnd);
}

LRESULT WINAPI Editor::HandleMessageSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	if (message == WM_NCCREATE) {
		// extract pointer to window class from creation data
		const CREATESTRUCTW* const createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
		Editor* const editor_pointer = static_cast<Editor*>(createStruct->lpCreateParams);

		// set WinAPI managed user data to store pointer to editor class
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(editor_pointer));

		// set WinAPI procedure to forwarder
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Editor::HandleMessageForward));

		// forward message
		return editor_pointer->HandleMessageForward(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT WINAPI Editor::HandleMessageForward(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	// retrieve pointer to editor class
	Editor* editor = reinterpret_cast<Editor*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	//if (message == WM_SIZE) {
	//	float new_width = LOWORD(lParam);
	//	float new_height = HIWORD(lParam);
	//	if (new_width != 0.0f && new_height != 0.0f) {
	//		if (editor->graphics != nullptr) {
	//			editor->graphics->SetNewSize(hWnd, new_width, new_height);
	//			//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//		}
	//	}
	//}

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
	case WM_MOUSEHOVER:
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
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

ECSEngine::Application* ECSEngine::CreateApplication(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	return new Editor(ECS_TOOLS_UI_DESIGNED_WIDTH, ECS_TOOLS_UI_DESIGNED_HEIGHT, L"ECSEngine Editor");
}
ECSEngine::Application* ECSEngine::CreateApplication() {
	return new Editor(ECS_TOOLS_UI_DESIGNED_WIDTH, ECS_TOOLS_UI_DESIGNED_HEIGHT, L"ECSEngine Editor");
}