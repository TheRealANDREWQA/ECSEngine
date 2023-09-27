// ECS_REFLECT
#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineRuntime.h"
#include "../Modules/ModuleDefinition.h"

enum EDITOR_SANDBOX_STATE : unsigned char {
	EDITOR_SANDBOX_SCENE,
	EDITOR_SANDBOX_RUNNING,
	EDITOR_SANDBOX_PAUSED
};

// Returns true if the sandbox is in runtime mode (running or paused), else false
ECS_INLINE bool IsSandboxStateRuntime(EDITOR_SANDBOX_STATE state) {
	return state == EDITOR_SANDBOX_RUNNING || state == EDITOR_SANDBOX_PAUSED;
}

enum ECS_REFLECT EDITOR_SANDBOX_VIEWPORT : unsigned char {
	EDITOR_SANDBOX_VIEWPORT_SCENE,
	EDITOR_SANDBOX_VIEWPORT_RUNTIME,
	EDITOR_SANDBOX_VIEWPORT_COUNT
};

enum EDITOR_SANDBOX_ENTITY_SLOT : unsigned char {
	EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_X,
	EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_Y,
	EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_Z,
	EDITOR_SANDBOX_ENTITY_SLOT_VIRTUAL_GLOBAL_COMPONENTS,
	EDITOR_SANDBOX_ENTITY_SLOT_COUNT
};

#define EDITOR_SCENE_EXTENSION L".scene"
#define EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT ECS_CONSTANT_REFLECT(8)
#define EDITOR_SANDBOX_CAMERA_WASD_DEFAULT_SPEED 0.1f

// -------------------------------------------------------------------------------------------------------------

struct ECS_REFLECT EditorSandboxModule {
	ECS_INLINE ECSEngine::AllocatorPolymorphic Allocator() {
		return ECSEngine::GetAllocatorPolymorphic(&settings_allocator);
	}

	ECS_FIELDS_START_REFLECT;

	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION module_configuration;
	bool is_deactivated;

	// These are needed for reflection of the settings
	ECSEngine::Stream<wchar_t> settings_name;

	ECSEngine::Stream<EditorModuleReflectedSetting> reflected_settings; ECS_SKIP_REFLECTION()
	ECSEngine::MemoryManager settings_allocator; ECS_SKIP_REFLECTION(static_assert(sizeof(ECSEngine::MemoryManager) == 72))

	ECS_FIELDS_END_REFLECT;
};

// -------------------------------------------------------------------------------------------------------------

// This structure keeps the information about the status of the module when the runtime is running
// This is useful to detect modules that are being added/removed dynamically or to detect changes
// In the settings of the module or of the configuration that is being run
struct EditorSandboxModuleSnapshot {
	EDITOR_MODULE_CONFIGURATION module_configuration;
	EDITOR_MODULE_LOAD_STATUS load_status;
	size_t library_timestamp;
	ECSEngine::Stream<wchar_t> library_name;
	ECSEngine::Stream<wchar_t> solution_path;
	ECSEngine::Stream<EditorModuleReflectedSetting> reflected_settings;
};

// -------------------------------------------------------------------------------------------------------------

struct EditorSandboxAssetHandlesSnapshot {
	ECSEngine::MemoryManager allocator;

	ECS_INLINE size_t StartOffset(ECSEngine::ECS_ASSET_TYPE type) const {
		size_t offset = 0;
		for (size_t index = 0; index < type; index++) {
			offset += asset_type_count[index];
		}
		return offset;
	}

	ECS_INLINE ECSEngine::ECS_ASSET_TYPE TypeFromIndex(size_t index) const {
		size_t offset = 0;
		for (size_t asset_type = 0; asset_type < ECSEngine::ECS_ASSET_TYPE_COUNT; asset_type++) {
			if (offset <= index && index < offset + asset_type_count[asset_type]) {
				return (ECSEngine::ECS_ASSET_TYPE)asset_type;
			}
			offset += asset_type_count[asset_type];
		}
		return ECSEngine::ECS_ASSET_TYPE_COUNT;
	}

	// Use a SoA approach to make the search fast - we will use the handle to look for the values
	unsigned int* handles;
	unsigned int* reference_counts;
	size_t asset_type_count[ECSEngine::ECS_ASSET_TYPE_COUNT];
	size_t total_size;
};

// -------------------------------------------------------------------------------------------------------------

enum EDITOR_SANDBOX_FLAG : size_t {
	EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION = 1 << 0
};

struct ECS_REFLECT EditorSandbox {
	ECS_INLINE ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return (ECSEngine::GlobalMemoryManager*)modules_in_use.allocator.allocator;
	}

	ECS_INLINE EditorSandbox& operator = (const EditorSandbox& other) {
		memcpy(this, &other, sizeof(*this));
		return *this;
	}

	ECS_FIELDS_START_REFLECT;

	ECSEngine::ResizableStream<EditorSandboxModule> modules_in_use;

	// Stored as relative path from the assets folder
	ECSEngine::CapacityStream<wchar_t> scene_path;

	// The settings used for creating the ECS world
	ECSEngine::CapacityStream<wchar_t> runtime_settings;

	// When the play button is clicked, if this sandbox should run
	bool should_play;

	// When the pause button is clicked, if this sandbox should pause
	bool should_pause;

	// When the step button is clicked, if this sandbox should step
	bool should_step;

	ECSEngine::ECS_TRANSFORM_TOOL transform_tool;
	ECSEngine::ECS_TRANSFORM_SPACE transform_space;

	float camera_wasd_speed;

	ECSEngine::CameraParametersFOV camera_parameters[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::OrientedPoint camera_saved_orientations[EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT];

	ECS_FIELDS_END_REFLECT;

	// This flag is set when initiating keyboard transform actions
	bool transform_display_axes;
	// Indicate which axis of the transform tool is currently selected
	bool transform_tool_selected[3];
	// We need to record separately the tool that is being used for the keyboard presses
	ECSEngine::ECS_TRANSFORM_TOOL transform_keyboard_tool;
	// We need to record the transform space for the keyboard press
	ECSEngine::ECS_TRANSFORM_SPACE transform_keyboard_space;

	bool is_camera_wasd_movement;

	EDITOR_SANDBOX_STATE run_state;
	bool is_scene_dirty;
	std::atomic<unsigned char> locked_count;

	size_t runtime_settings_last_write;
	ECSEngine::WorldDescriptor runtime_descriptor;
	ECSEngine::AssetDatabaseReference database;

	ECSEngine::RenderDestination viewport_render_destination[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::ResourceView viewport_transferred_texture[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::RenderTargetView scene_viewport_instance_framebuffer;
	ECSEngine::DepthStencilView scene_viewport_depth_stencil_framebuffer;
	// These are set used to make calls to RenderSandbox ignore the request if the output is
	// not going to be visualized
	bool viewport_enable_rendering[EDITOR_SANDBOX_VIEWPORT_COUNT];
	// If a call to a render sandbox that wanted to resize the textures was issued but the rendering
	// Is not allowed, it will put the value to be read from here for the next render
	ECSEngine::uint2 viewport_pending_resize[EDITOR_SANDBOX_VIEWPORT_COUNT];

	ECSEngine::EntityManager scene_entities;
	ECSEngine::World sandbox_world;

	ECSEngine::ResizableStream<ECSEngine::Entity> selected_entities;
	unsigned char selected_entities_changed_counter;

	ECSEngine::ResizableStream<ECSEngine::Entity> unused_entities_slots;
	ECSEngine::ResizableStream<EDITOR_SANDBOX_ENTITY_SLOT> unused_entity_slot_type;
	// These flags indicate when the unused slots need to be recomputed (after a clear/reset for example)
	bool unused_entities_slots_recompute;

	ECSEngine::MemoryManager runtime_module_snapshot_allocator;
	ECSEngine::ResizableStream<EditorSandboxModuleSnapshot> runtime_module_snapshots;

	EditorSandboxAssetHandlesSnapshot runtime_asset_handle_snapshot;

	// Miscellaneous flags
	size_t flags;
};

ECS_INLINE ECSEngine::Stream<char> ViewportString(EDITOR_SANDBOX_VIEWPORT viewport) {
	switch (viewport) {
	case EDITOR_SANDBOX_VIEWPORT_SCENE:
		return "Scene";
	case EDITOR_SANDBOX_VIEWPORT_RUNTIME:
		return "Runtime";
	default:
		return "Unknown viewport";
	}
}

// It will map the paused state to the runtime viewport
ECS_INLINE EDITOR_SANDBOX_VIEWPORT EditorViewportTextureFromState(EDITOR_SANDBOX_STATE sandbox_state) {
	switch (sandbox_state) {
	case EDITOR_SANDBOX_SCENE:
		return EDITOR_SANDBOX_VIEWPORT_SCENE;
	case EDITOR_SANDBOX_RUNNING:
		return EDITOR_SANDBOX_VIEWPORT_RUNTIME;
	case EDITOR_SANDBOX_PAUSED:
		return EDITOR_SANDBOX_VIEWPORT_RUNTIME;
	default:
		return EDITOR_SANDBOX_VIEWPORT_COUNT;
	}
}