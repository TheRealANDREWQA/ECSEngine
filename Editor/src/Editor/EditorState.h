#pragma once
#include "editorpch.h"
#include "ECSEngineUI.h"
#include "../Project/ProjectFile.h"
#include "../UI/InspectorData.h"
#include "../UI/FileExplorerData.h"
#include "../UI/HubData.h"
#include "../Modules/ModuleDefinition.h"
#include "EditorSandbox.h"

#define EDITOR_CONSOLE_SYSTEM_NAME "Editor"

struct EditorEvent {
	void* function;
	void* data;
	size_t data_size;
};

struct EditorState;

typedef void (*EditorStateTick)(EditorState*);

#define EDITOR_SCENE_BUFFERING_COUNT 3

enum EDITOR_LAZY_EVALUATION_COUNTERS {
	EDITOR_LAZY_EVALUATION_DIRECTORY_EXPLORER,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_TEXTURES,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_MESH_THUMBNAIL,
	EDITOR_LAZY_EVALUATION_UPDATE_MODULE_STATUS,
	EDITOR_LAZY_EVALUATION_UPDATE_GRAPHICS_MODULE_STATUS,
	EDITOR_LAZY_EVALUATION_CREATE_DEFAULT_METAFILES,
	EDITOR_LAZY_EVALUATION_COUNTERS_COUNT
};

struct EditorState {
	inline void Tick() {
		editor_tick(this);
	}

	inline ECSEngine::Graphics* UIGraphics() {
		return ui_system->m_graphics;
	}

	inline ECSEngine::Graphics* CacheGraphics() {
		return cache_graphics;
	}

	inline ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return editor_allocator->m_backup;
	}

	inline ECSEngine::Reflection::ReflectionManager* ReflectionManager() {
		return ui_reflection->reflection;
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
	ECSEngine::ResourceManager* resource_manager;

	ECSEngine::Tools::UIReflectionDrawer* ui_reflection;
	ECSEngine::Tools::UIReflectionDrawer* module_reflection;
	
	ECSEngine::MemoryManager* editor_allocator;
	ECSEngine::MemoryManager* multithreaded_editor_allocator;
	ECSEngine::TaskManager* task_manager;

	ECSEngine::AssetDatabase asset_database;
	ECSEngine::ResourceManager* cache_resource_manager;
	ECSEngine::Graphics* cache_graphics;
	
	ECSEngine::ResizableStream<ECSEngine::Stream<wchar_t>> launched_module_compilation[EDITOR_MODULE_CONFIGURATION_COUNT];
	// Needed to syncronize the threads when removing the launched module compilation
	ECSEngine::SpinLock launched_module_compilation_lock;
	
	ProjectModules* project_modules;
	FileExplorerData* file_explorer_data;
	HubData* hub_data;
	ProjectFile* project_file;

	InspectorManager inspector_manager;

	// These will be played back on the main thread. If multithreaded tasks are desired,
	// use the AddBackgroundTask function
	ECSEngine::ThreadSafeQueue<EditorEvent> event_queue;

	ECSEngine::ResizableStream<EditorSandbox> sandboxes;
	
	ECSEngine::ResizableQueue<ECSEngine::ThreadTask> pending_background_tasks;

	// A queue onto which GPU tasks can be placed in order to be consumed on the immediate context
	ECSEngine::ResizableQueue<ECSEngine::ThreadTask> gpu_tasks;
	
	ECSEngine::Tools::InjectWindowData inject_data;
	const char* inject_window_name;
	
	unsigned short* lazy_evaluation_counters;
	ECSEngine::Timer lazy_evalution_timer;
	std::atomic<size_t> flags = 0;
	bool inject_window_is_pop_up_window = false;
};

void EditorSetConsoleError(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MEDIUM);

void EditorSetConsoleWarn(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MINIMAL);

void EditorSetConsoleInfo(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MINIMAL);

void EditorSetConsoleTrace(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MINIMAL);

#define EDITOR_STATE_DO_NOT_ADD_TASKS (1 << 0)
#define EDITOR_STATE_IS_PLAYING (1 << 1)
#define EDITOR_STATE_IS_PAUSED (1 << 2)
#define EDITOR_STATE_STEP (1 << 3)
#define EDITOR_STATE_FREEZE_TICKS (1 << 4)

void EditorStateSetFlag(EditorState* editor_state, size_t flag);

void EditorStateClearFlag(EditorState* editor_state, size_t flag);

bool EditorStateHasFlag(EditorState* editor_state, size_t flag);

void EditorStateProjectTick(EditorState* editor_state);

void EditorStateHubTick(EditorState* editor_state);

// It is the same as adding the task to the task manager, the only difference being that it will check the flag
// EDITOR_STATE_DO_NOT_ADD_TASKS and if it is set, it will enqueue the task for later launch 
// The flag signals that the application needs the bandwith in order to remain responsive
void EditorStateAddBackgroundTask(EditorState* editor_state, ECSEngine::ThreadTask task);

// It will place the task into the GPU tasks queue. It will be consumed later on
void EditorStateAddGPUTask(EditorState* editor_state, ECSEngine::ThreadTask task);

bool EditorStateDoNotAddBackgroundTasks(EditorState* editor_state);

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

#define EDITOR_STATE(editor_state) UIReflectionDrawer* ui_reflection = ((EditorState*)editor_state)->ui_reflection; \
UISystem* ui_system = ((EditorState*)editor_state)->ui_system; \
MemoryManager* editor_allocator = ((EditorState*)editor_state)->editor_allocator; \
MemoryManager* multithreaded_editor_allocator = ((EditorState*)editor_state)->multithreaded_editor_allocator; \
TaskManager* task_manager = ((EditorState*)editor_state)->task_manager;