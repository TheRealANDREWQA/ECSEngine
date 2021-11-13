#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineUI.h"
#include "ECSEngineMultithreading.h"
#include "ECSEngineWorld.h"
#include "../Modules/ModuleConfigurationGroup.h"
#include "ECSEngineModule.h"

struct EditorEvent {
	void* function;
	void* data;
	size_t data_size;
};

struct EditorState {
	inline ECSEngine::World* ActiveWorld() {
		return worlds.buffer + active_world;
	}

	ECSEngine::Tools::UISystem* ui_system;
	ECSEngine::Tools::UIReflectionDrawer* ui_reflection;
	ECSEngine::MemoryManager* editor_allocator;
	ECSEngine::Tools::UISpriteTexture* viewport_texture;
	ECSEngine::Tools::Console* console;
	ECSEngine::TaskManager* task_manager;
	ECSEngine::TaskDependencies* project_task_graph;
	void* project_modules;
	void* file_explorer_data;
	void* hub_data;
	void* project_file;
	void* inspector_data;
	ECSEngine::containers::ThreadSafeQueue<EditorEvent> event_queue;
	ECSEngine::containers::Stream<ECSEngine::containers::Stream<char>> module_configuration_definitions;
	ECSEngine::containers::ResizableStream<ModuleConfigurationGroup, ECSEngine::MemoryManager> module_configuration_groups;
	ECSEngine::containers::CapacityStream<ECSEngine::World> worlds;
	unsigned int active_world;
	void* graphics_module_reflected_data;
	ECSEngine::Tools::InjectWindowData inject_data;
	const char* inject_window_name;
	bool inject_window_is_pop_up_window = false;
};

#define EDITOR_STATE(editor_state) UIReflectionDrawer* ui_reflection = ((EditorState*)editor_state)->ui_reflection; \
UISystem* ui_system = ((EditorState*)editor_state)->ui_system; \
MemoryManager* editor_allocator = ((EditorState*)editor_state)->editor_allocator; \
TaskManager* task_manager = ((EditorState*)editor_state)->task_manager; \
UISpriteTexture* viewport_texture = ((EditorState*)editor_state)->viewport_texture; \
Console* console = ((EditorState*)editor_state)->console; \
TaskDependencies* project_task_graph = ((EditorState*)editor_state)->project_task_graph;