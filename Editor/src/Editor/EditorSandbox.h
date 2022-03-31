#pragma once
#include "ECSEngineRendering.h"
#include "editorpch.h"

struct EditorState;

struct EditorSandboxModules {
	unsigned int module_index;

	// These are needed for reflection of the settings
	ECSEngine::HashTableDefault<void*> reflected_settings;
	ECSEngine::Stream<wchar_t> settings_path;
	ECSEngine::ResizableStream<const char*> thread_tasks;
};

struct EditorSandbox {
	ECSEngine::ResourceView viewport_texture;
	ECSEngine::DepthStencilView viewport_texture_depth;
	ECSEngine::RenderTargetView viewport_render_target;
	ECSEngine::TaskDependencies task_dependencies;

	ECSEngine::ResizableStream<EditorSandboxModules> modules_in_use;

	ECSEngine::WorldDescriptor world_descriptor;
	ECSEngine::World scene_world;
	ECSEngine::World sandbox_world;
	ECSEngine::Semaphore copy_world_status;
};

void AddSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);

void ChangeSandboxModuleSettings(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index, ECSEngine::Stream<wchar_t> settings_name);

void CopySandboxWorld(EditorState* editor_state, unsigned int sandbox_index);

void CreateSandbox(EditorState* editor_state);

void DestroySandbox(EditorState* editor_state, unsigned int index);

void RemoveSandboxModule(EditorState* editor_state, unsigned int sandbox_index, unsigned int module_index);