#pragma once
#include "editorpch.h"
#include "ECSEngineUI.h"
#include "../Project/ProjectFile.h"
#include "../Project/ProjectSettings.h"
#include "../UI/InspectorData.h"
#include "../UI/FileExplorerData.h"
#include "../UI/HubData.h"
#include "../Modules/ModuleDefinition.h"
#include "../Sandbox/SandboxTypes.h"
#include "EditorComponents.h"
#include "EditorEventDef.h"
#include "EditorStateTypes.h"
#include "ECSEngineInput.h"
#include "EditorVisualizeTexture.h"
#include "../Assets/Prefab.h"

#define EDITOR_CONSOLE_SYSTEM_NAME "Editor"

struct EditorState;

typedef void (*EditorStateTick)(EditorState*);

struct EditorState {
	ECS_INLINE void Tick() {
		editor_tick(this);
	}

	ECS_INLINE ECSEngine::Graphics* UIGraphics() {
		return ui_system->m_graphics;
	}

	ECS_INLINE ECSEngine::Graphics* RuntimeGraphics() {
		return runtime_graphics;
	}

	ECS_INLINE ECSEngine::ResourceManager* RuntimeResourceManager() {
		return runtime_resource_manager;
	}

	ECS_INLINE ECSEngine::ResourceManager* UIResourceManager() {
		return ui_system->m_resource_manager;
	}

	ECS_INLINE ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return (ECSEngine::GlobalMemoryManager*)editor_allocator->m_backup.allocator;
	}

	ECS_INLINE ECSEngine::AllocatorPolymorphic GlobalMemoryManagerPolymorphic() const {
		return editor_allocator->m_backup;
	}

	ECS_INLINE ECSEngine::Reflection::ReflectionManager* EditorReflectionManager() {
		return ui_reflection->reflection;
	}

	ECS_INLINE const ECSEngine::Reflection::ReflectionManager* EditorReflectionManager() const {
		return ui_reflection->reflection;
	}

	ECS_INLINE ECSEngine::Reflection::ReflectionManager* ModuleReflectionManager() {
		return module_reflection->reflection;
	}

	ECS_INLINE ECSEngine::Reflection::ReflectionManager* GlobalReflectionManager() {
		return editor_components.internal_manager;
	}

	ECS_INLINE const ECSEngine::Reflection::ReflectionManager* GlobalReflectionManager() const {
		return editor_components.internal_manager;
	}

	ECS_INLINE ECSEngine::Mouse* Mouse() {
		return ui_system->m_mouse;
	}

	ECS_INLINE ECSEngine::Keyboard* Keyboard() {
		return ui_system->m_keyboard;
	}

	ECS_INLINE ECSEngine::AllocatorPolymorphic EditorAllocator() {
		return editor_allocator;
	}

	ECS_INLINE ECSEngine::AllocatorPolymorphic MultithreadedEditorAllocator() {
		AllocatorPolymorphic allocator = multithreaded_editor_allocator;
		allocator.allocation_type = ECSEngine::ECS_ALLOCATION_MULTI;
		return allocator;
	}

	EditorStateTick editor_tick;
	ECSEngine::Tools::UISystem* ui_system;
	ECSEngine::ResourceManager* ui_resource_manager;

	ECSEngine::Tools::UIReflectionDrawer* ui_reflection;
	ECSEngine::Tools::UIReflectionDrawer* module_reflection;
	
	ECSEngine::MemoryManager* editor_allocator;
	ECSEngine::MemoryManager* multithreaded_editor_allocator;
	ECSEngine::TaskManager* task_manager;
	// For rendering of scenes use a separate task manager such that no static tasks
	// or wrappers are changed for the existing ones
	ECSEngine::TaskManager* render_task_manager;

	// All sandboxes will refer to these. Cannot make a separate GPU runtime for each 
	// sandbox because of technical D3D issues which don't allow resource sharing
	// across multiple devices (for shaders, input layouts etc. and for buffers (might not even be possible)
	// and textures it is complicated to get right). So instead rely on the fact that when the device gets
	// removed due to a crash we can recover it.
	ECSEngine::AssetDatabase* asset_database;
	ECSEngine::ResourceManager* runtime_resource_manager;
	ECSEngine::Graphics* runtime_graphics;
	
	// We keep these separately since we don't want to clutter the
	// ProjectModules with a "fake" module that needs special treatment
	ECSEngine::Stream<ECSEngine::ModuleLinkComponentTarget> ecs_link_components;
	ECSEngine::ModuleExtraInformation ecs_extra_information;
	ECSEngine::Stream<ModuleDebugDrawElement> ecs_debug_draw;

	ECSEngine::ResizableStream<ECSEngine::Stream<wchar_t>> launched_module_compilation[EDITOR_MODULE_CONFIGURATION_COUNT];
	// Needed to syncronize the threads when removing the launched module compilation
	ECSEngine::SpinLock launched_module_compilation_lock;

	ProjectModules* project_modules;
	FileExplorerData* file_explorer_data;
	HubData* hub_data;
	ProjectFile* project_file;
	ProjectSettings project_settings;

	InspectorManager inspector_manager;
	EditorComponents editor_components;
	ECSEngine::InputMapping input_mapping;

	EditorVisualizeTexture visualize_texture;

	// Assets that are loaded in background will be placed in this
	// Array such that we won't issue a second load for an asset that is already
	// Loading. At the moment, make this single threaded
	ECSEngine::ResizableStream<ECSEngine::AssetTypedHandle> loading_assets;

	// These will be played back on the main thread. If multithreaded tasks are desired,
	// use the AddBackgroundTask function. It is used in a multithreaded context
	ECSEngine::ThreadSafeResizableQueue<EditorEvent> event_queue;
	// When ticking the events, here the events that want to be pushed back
	// Will be placed here and if an action wants to check which events are still
	// Active it can look here during event processing
	ECSEngine::ResizableStream<EditorEvent> readd_events;

	// We need to record globally all the prefabs
	// Since they can be cross-referenced in multiple sandboxes
	ECSEngine::ResizableSparseSet<PrefabInstance> prefabs;
	// This is the allocator used for the path allocations
	// For the prefabs
	ECSEngine::MemoryManager prefabs_allocator;

	ECSEngine::ResizableStream<EditorSandbox> sandboxes;
	// We keep a counter of temporary sandboxes - sandboxes that are not meant to be
	// Run but instead to do something in them - like editing some entities, visualizing
	// Data or other related operations. These sandboxes are also not meant to be visible
	// To the user - these should not appear in the sandbox file and should not be able to
	// Be picked by the user
	unsigned int sandboxes_temporary_count;
	
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

void EditorSetConsoleError(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_IMPORTANT);

void EditorSetConsoleWarn(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MEDIUM);

void EditorSetConsoleInfo(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MEDIUM);

void EditorSetConsoleTrace(ECSEngine::Stream<char> error_message, ECSEngine::ECS_CONSOLE_VERBOSITY verbosity = ECSEngine::ECS_CONSOLE_VERBOSITY_MEDIUM);

void EditorStateProjectTick(EditorState* editor_state);

void EditorStateHubTick(EditorState* editor_state);

// It is the same as adding the task to the task manager, the only difference being that it will check the flag
// EDITOR_STATE_DO_NOT_ADD_TASKS and if it is set, it will enqueue the task for later launch 
// The flag signals that the application needs the bandwith in order to remain responsive
void EditorStateAddBackgroundTask(EditorState* editor_state, ECSEngine::ThreadTask task);

// It will place the task into the GPU tasks queue. It will be consumed later on
void EditorStateAddGPUTask(EditorState* editor_state, ECSEngine::ThreadTask task);

// This needs to be called once per application run
void EditorStateBaseInitialize(EditorState* editor_state, HWND hwnd, ECSEngine::Mouse* mouse, ECSEngine::Keyboard* keyboard);

void EditorStateInitialize(ECSEngine::Application* application, EditorState* editor_state, HWND hWnd, ECSEngine::Mouse* mouse, ECSEngine::Keyboard* keyboard);

void EditorStateDestroy(EditorState* editor_state);

// Returns the timer value of that lazy evaluation
unsigned short EditorStateLazyEvaluation(EditorState* editor_state, unsigned int index);

// Returns true if lazy evaluation must be done and resets the counter, else returns false
bool EditorStateLazyEvaluationTrue(EditorState* editor_state, unsigned int index, unsigned short duration);

// It will set it to the maximum USHORT_MAX such that the lazy evaluation will trigger next time
void EditorStateLazyEvaluationTrigger(EditorState* editor_state, unsigned int index);

// Can be used to set the evaluation to a certain value (useful for triggering a lazy evaluation a bit latter)
void EditorStateLazyEvaluationSet(EditorState* editor_state, unsigned int index, unsigned short value);

enum EDITOR_APPLICATION_QUIT_RESPONSE : unsigned char {
	EDITOR_APPLICATION_QUIT_NOT_READY,
	EDITOR_APPLICATION_QUIT_ABORTED,
	EDITOR_APPLICATION_QUIT_APPROVED
};

// When the application wants to quit, the editor will look to see if there is anything left to do
// before quitting like saving scenes. Use the enum to query for the current state
void EditorStateApplicationQuit(EditorState* editor_state, EDITOR_APPLICATION_QUIT_RESPONSE* quit_response);

// Updates the asset database path from the current active project
void EditorStateSetDatabasePath(EditorState* editor_state);

// This function should be called right before the application is about to exit
void EditorStateBeforeExitCleanup(EditorState* editor_state);