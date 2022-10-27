#pragma once
#include "editorpch.h"
#include "ECSEngineUI.h"
#include "../Project/ProjectFile.h"
#include "../UI/InspectorData.h"
#include "../UI/FileExplorerData.h"
#include "../UI/HubData.h"
#include "../Modules/ModuleDefinition.h"
#include "EditorSandbox.h"
#include "EditorComponents.h"

#define EDITOR_CONSOLE_SYSTEM_NAME "Editor"

struct EditorEvent {
	void* function;
	void* data;
	size_t data_size;
};

struct EditorState;

typedef void (*EditorStateTick)(EditorState*);

enum EDITOR_LAZY_EVALUATION_COUNTERS : unsigned char {
	EDITOR_LAZY_EVALUATION_DIRECTORY_EXPLORER,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_TEXTURES,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_MESH_THUMBNAIL,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_MATERIAL_THUMBNAIL,
	EDITOR_LAZY_EVALUATION_UPDATE_MODULE_STATUS,
	EDITOR_LAZY_EVALUATION_UPDATE_GRAPHICS_MODULE_STATUS,
	EDITOR_LAZY_EVALUATION_RESET_TASK_MANAGER,
	EDITOR_LAZY_EVALUATION_RUNTIME_SETTINGS,
	EDITOR_LAZY_EVALUATION_DEFAULT_METADATA_FOR_ASSETS,
	EDITOR_LAZY_EVALUATION_COUNTERS_COUNT,
};

enum EDITOR_STATE_FLAGS : unsigned char {
	EDITOR_STATE_DO_NOT_ADD_TASKS,
	EDITOR_STATE_IS_PLAYING,
	EDITOR_STATE_IS_PAUSED,
	EDITOR_STATE_IS_STEP,
	EDITOR_STATE_FREEZE_TICKS,
	EDITOR_STATE_PREVENT_LAUNCH,
	// This is for runtime only - it doesn't affect the UI loads
	EDITOR_STATE_PREVENT_RESOURCE_LOADING,
	// This is a one off flag. Used when creating the dedicated GPU device
	// such that if it is the first time a device is created on a dedicated GPU
	// and it is asleep (like it a laptop) then it will do the creation deferred
	// in another thread and this signals when that creation has finished (this
	// usually takes 1 second and if the user doesn't open up a project quicker than
	// this then there should be no problem.
	EDITOR_STATE_RUNTIME_GRAPHICS_INITIALIZATION_FINISHED,
	EDITOR_STATE_FLAG_COUNT
};

struct EditorState {
	inline void Tick() {
		editor_tick(this);
	}

	inline ECSEngine::Graphics* UIGraphics() {
		return ui_system->m_graphics;
	}

	inline ECSEngine::Graphics* RuntimeGraphics() {
		return runtime_graphics;
	}

	inline ECSEngine::ResourceManager* RuntimeResourceManager() {
		return runtime_resource_manager;
	}

	inline ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return editor_allocator->m_backup;
	}

	inline ECSEngine::Reflection::ReflectionManager* ReflectionManager() {
		return ui_reflection->reflection;
	}

	inline ECSEngine::Reflection::ReflectionManager* ModuleReflectionManager() {
		return module_reflection->reflection;
	}

	inline ECSEngine::HID::Mouse* Mouse() {
		return ui_system->m_mouse;
	}

	inline ECSEngine::HID::Keyboard* Keyboard() {
		return ui_system->m_keyboard;
	}

	inline ECSEngine::AllocatorPolymorphic EditorAllocator() const {
		return ECSEngine::GetAllocatorPolymorphic(editor_allocator);
	}

	inline ECSEngine::AllocatorPolymorphic MultithreadedEditorAllocator() const {
		return ECSEngine::GetAllocatorPolymorphic(multithreaded_editor_allocator);
	}

	EditorStateTick editor_tick;
	ECSEngine::Tools::UISystem* ui_system;
	ECSEngine::ResourceManager* ui_resource_manager;

	ECSEngine::Tools::UIReflectionDrawer* ui_reflection;
	ECSEngine::Tools::UIReflectionDrawer* module_reflection;
	
	ECSEngine::MemoryManager* editor_allocator;
	ECSEngine::MemoryManager* multithreaded_editor_allocator;
	ECSEngine::TaskManager* task_manager;

	// All sandboxes will refer to these. Cannot make a separate GPU runtime for each 
	// sandbox because of technical D3D issues which don't allow resource sharing
	// across multiple devices (for shaders, input layouts etc. and for buffers (might not even be possible)
	// and textures it is complicated to get right). So instead rely on the fact that when the device gets
	// removed due to a crash we can recover it.
	ECSEngine::AssetDatabase* asset_database;
	ECSEngine::ResourceManager* runtime_resource_manager;
	ECSEngine::Graphics* runtime_graphics;
	
	ECSEngine::ResizableStream<ECSEngine::Stream<wchar_t>> launched_module_compilation[EDITOR_MODULE_CONFIGURATION_COUNT];
	// Needed to syncronize the threads when removing the launched module compilation
	ECSEngine::SpinLock launched_module_compilation_lock;

	ProjectModules* project_modules;
	FileExplorerData* file_explorer_data;
	HubData* hub_data;
	ProjectFile* project_file;

	InspectorManager inspector_manager;
	EditorComponents editor_components;

	// These will be played back on the main thread. If multithreaded tasks are desired,
	// use the AddBackgroundTask function
	ECSEngine::Queue<EditorEvent> event_queue;

	ECSEngine::ResizableStream<EditorSandbox> sandboxes;
	
	ECSEngine::ResizableQueue<ECSEngine::ThreadTask> pending_background_tasks;

	// A queue onto which GPU tasks can be placed in order to be consumed on the immediate context
	ECSEngine::ResizableQueue<ECSEngine::ThreadTask> gpu_tasks;
	
	// There is no cache line padding. So false sharing is at play here. But there shouldn't be much
	// crossover between these flags (if one is activated the others most likely are not)
	std::atomic<size_t> flags[EDITOR_STATE_FLAG_COUNT];

	// Lazy evaluation counters
	unsigned short* lazy_evaluation_counters;
	ECSEngine::Timer lazy_evalution_timer;
};

void EditorSetConsoleError(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MEDIUM);

void EditorSetConsoleWarn(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MINIMAL);

void EditorSetConsoleInfo(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MINIMAL);

void EditorSetConsoleTrace(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MINIMAL);

// These are reference counted, can be called multiple times
void EditorStateSetFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag);

// These are reference counted, can be called multiple times
void EditorStateClearFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag);

bool EditorStateHasFlag(const EditorState* editor_state, EDITOR_STATE_FLAGS flag);

// Waits until the flag is set or cleared
void EditorStateWaitFlag(size_t sleep_milliseconds, const EditorState* editor_state, EDITOR_STATE_FLAGS flag, bool set = true);

void EditorStateProjectTick(EditorState* editor_state);

void EditorStateHubTick(EditorState* editor_state);

// It is the same as adding the task to the task manager, the only difference being that it will check the flag
// EDITOR_STATE_DO_NOT_ADD_TASKS and if it is set, it will enqueue the task for later launch 
// The flag signals that the application needs the bandwith in order to remain responsive
void EditorStateAddBackgroundTask(EditorState* editor_state, ECSEngine::ThreadTask task);

// It will place the task into the GPU tasks queue. It will be consumed later on
void EditorStateAddGPUTask(EditorState* editor_state, ECSEngine::ThreadTask task);

void EditorStateInitialize(ECSEngine::Application* application, EditorState* editor_state, HWND hWnd, ECSEngine::HID::Mouse& mouse, ECSEngine::HID::Keyboard& keyboard);

void EditorStateDestroy(EditorState* editor_state);

// Returns the timer value of that lazy evaluation
unsigned short EditorStateLazyEvaluation(EditorState* editor_state, unsigned int index);

// Returns true if lazy evaluation must be done and resets the counter, else returns false
bool EditorStateLazyEvaluationTrue(EditorState* editor_state, unsigned int index, unsigned short duration);

// It will set it to the maximum USHORT_MAX such that the lazy evaluation will trigger next time
void EditorStateLazyEvaluationSetMax(EditorState* editor_state, unsigned int index);

// Can be used to set the evaluation to a certain value (useful for triggering a lazy evaluation a bit latter)
void EditorStateLazyEvaluationSet(EditorState* editor_state, unsigned int index, unsigned short value);

// When the application wants to quit, the editor will look to see if there is anything left to do
// before quitting like saving scenes. Query the response for 0 if the user wants to continue the application,
// 1 if the quit can be done and -1 if the response is not yet ready
void EditorStateApplicationQuit(EditorState* editor_state, char* quit_response);

// Updates the asset database path from the current active project
void EditorStateSetDatabasePath(EditorState* editor_state);

#define EDITOR_STATE(editor_state) UIReflectionDrawer* ui_reflection = ((EditorState*)editor_state)->ui_reflection; \
UISystem* ui_system = ((EditorState*)editor_state)->ui_system; \
MemoryManager* editor_allocator = ((EditorState*)editor_state)->editor_allocator; \
MemoryManager* multithreaded_editor_allocator = ((EditorState*)editor_state)->multithreaded_editor_allocator; \
TaskManager* task_manager = ((EditorState*)editor_state)->task_manager;