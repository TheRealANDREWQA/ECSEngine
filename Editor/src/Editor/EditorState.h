#pragma once
#include "editorpch.h"
//#include "ECSEngineContainers.h"
//#include "ECSEngineMultithreading.h"
//#include "ECSEngineWorld.h"
//#include "ECSEngineAllocatorPolymorphic.h"
#include "ECSEngineUI.h"
#include "../Modules/ModuleConfigurationGroup.h"
#include "../Project/ProjectFile.h"
#include "../UI/InspectorData.h"
#include "../UI/FileExplorerData.h"
#include "../UI/HubData.h"
#include "../Modules/ModuleDefinition.h"

struct EditorEvent {
	void* function;
	void* data;
	size_t data_size;
};

struct EditorState;

using EditorStateTick = void (*)(EditorState*);

struct EditorState {
	struct BackgroundTask {
		ECSEngine::ThreadTask task;
		size_t data_size = 0;
	};

	inline ECSEngine::World* ActiveWorld() {
		return worlds.buffer + active_world;
	}

	inline void Tick() {
		editor_tick(this);
	}

	inline ECSEngine::TaskManager* TaskManager() {
		return ui_system->m_task_manager;
	}

	inline ECSEngine::Graphics* Graphics() {
		return ui_system->m_graphics;
	}

	inline ECSEngine::ResourceManager* ResourceManager() {
		return ui_system->m_resource_manager;
	}

	inline ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return editor_allocator->m_backup;
	}

	inline ECSEngine::Reflection::ReflectionManager* ReflectionManager() {
		return ui_reflection->reflection;
	}

	EditorStateTick editor_tick;
	ECSEngine::Tools::UISystem* ui_system;
	ECSEngine::Tools::UIReflectionDrawer* ui_reflection;
	ECSEngine::Tools::UIReflectionDrawer* module_reflection;
	ECSEngine::MemoryManager* editor_allocator;
	ECSEngine::Tools::UISpriteTexture* viewport_texture;
	ECSEngine::Tools::Console* console;
	ECSEngine::TaskManager* task_manager;
	ECSEngine::TaskDependencies* project_task_graph;
	ProjectModules* project_modules;
	FileExplorerData* file_explorer_data;
	HubData* hub_data;
	ProjectFile* project_file;
	InspectorData* inspector_data;
	ECSEngine::containers::ThreadSafeQueue<EditorEvent> event_queue;
	ECSEngine::containers::Stream<ECSEngine::containers::Stream<char>> module_configuration_definitions;
	ECSEngine::containers::ResizableStream<ModuleConfigurationGroup, ECSEngine::MemoryManager> module_configuration_groups;
	ECSEngine::containers::CapacityStream<ECSEngine::World> worlds;
	ECSEngine::containers::ResizableQueue<BackgroundTask, ECSEngine::MemoryManager> pending_background_tasks;
	unsigned int active_world;
	ECSEngine::Tools::InjectWindowData inject_data;
	const char* inject_window_name;
	std::atomic<size_t> flags = 0;
	bool inject_window_is_pop_up_window = false;
};

#define EDITOR_STATE_DO_NOT_ADD_TASKS (1 << 0)
#define EDITOR_STATE_IS_PLAYING (1 << 1)
#define EDITOR_STATE_IS_PAUSED (1 << 2)

void EditorStateProjectTick(EditorState* editor_state);

void EditorStateHubTick(EditorState* editor_state);

// It is the same as adding the task to the task manager, the only difference being that it will check the flag
// EDITOR_STATE_DO_NOT_ADD_TASKS and if it is set, it will enqueue the task for later launch 
// The flag signals that the application needs the bandwith in order to remain responsive
void EditorStateAddBackgroundTask(EditorState* editor_state, ECSEngine::ThreadTask task, size_t task_data_size = 0);

bool EditorStateDoNotAddBackgroundTasks(EditorState* editor_state);

void EditorStateInitialize(ECSEngine::Application* application, EditorState* editor_state, HWND hWnd, ECSEngine::HID::Mouse& mouse, ECSEngine::HID::Keyboard& keyboard);

void EditorStateDestroy(EditorState* editor_state);

#define EDITOR_STATE(editor_state) UIReflectionDrawer* ui_reflection = ((EditorState*)editor_state)->ui_reflection; \
UISystem* ui_system = ((EditorState*)editor_state)->ui_system; \
MemoryManager* editor_allocator = ((EditorState*)editor_state)->editor_allocator; \
TaskManager* task_manager = ((EditorState*)editor_state)->task_manager; \
UISpriteTexture* viewport_texture = ((EditorState*)editor_state)->viewport_texture; \
Console* console = ((EditorState*)editor_state)->console; \
TaskDependencies* project_task_graph = ((EditorState*)editor_state)->project_task_graph;