// ECS_REFLECT
#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineRuntime.h"
#include "ECSEngineProfiling.h"
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

struct EditorSandboxEntitySlot {
	ECS_INLINE EditorSandboxEntitySlot() {}

	template<typename DataType>
	ECS_INLINE DataType Data() const {
		return *(DataType*)data;
	}

	template<typename DataType>
	ECS_INLINE void SetData(DataType value) {
		static_assert(sizeof(DataType) <= sizeof(data));
		*(DataType*)data = value;
	}

	EDITOR_SANDBOX_ENTITY_SLOT slot_type;
	union {
		size_t uinteger;
		int64_t sinteger;
		char data[sizeof(uinteger)];
	};
};

#define EDITOR_SCENE_EXTENSION L".scene"
#define EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT ECS_CONSTANT_REFLECT(8)
#define EDITOR_SANDBOX_CAMERA_WASD_DEFAULT_SPEED 0.1f

// -------------------------------------------------------------------------------------------------------------

struct ECS_REFLECT EditorSandboxModule {
	ECS_INLINE ECSEngine::AllocatorPolymorphic SettingsAllocator() {
		return &settings_allocator;
	}

	ECS_INLINE ECSEngine::AllocatorPolymorphic EnabledDebugTasksAllocator() {
		return &enabled_debug_tasks_allocator;
	}

	ECS_FIELDS_START_REFLECT;

	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION module_configuration;
	bool is_deactivated;

	// These are needed for reflection of the settings
	ECSEngine::Stream<wchar_t> settings_name;
	// Here all the debug draw tasks which belong to this module which are enabled will be kept
	ECSEngine::ResizableStream<ECSEngine::Stream<char>> enabled_debug_tasks;

	ECSEngine::Stream<EditorModuleReflectedSetting> reflected_settings; ECS_SKIP_REFLECTION()
	ECSEngine::MemoryManager settings_allocator; ECS_SKIP_REFLECTION()
	ECSEngine::MemoryManager enabled_debug_tasks_allocator; ECS_SKIP_REFLECTION()
	// The time stamp is used to determine when a change has happened in order to refresh the data
	size_t time_stamp; ECS_SKIP_REFLECTION()

	ECS_FIELDS_END_REFLECT;
};

// -------------------------------------------------------------------------------------------------------------

// This structure keeps the information about the status of the module when the runtime is running
// This is useful to detect modules that are being added/removed dynamically or to detect changes
// In the settings of the module or of the configuration that is being run
struct EditorSandboxModuleSnapshot {
	EDITOR_MODULE_CONFIGURATION module_configuration;
	EDITOR_MODULE_LOAD_STATUS load_status;
	// Use this flag to determine if we should compile this module or not - by default
	// We should try to compile modules that failed since their dependencies might have been fixed
	// Now and we wouldn't catch them if we used out of date. But we need this flag in order
	// To not try to recompile endlessly when there is no changed made
	bool tried_loading;
	size_t library_timestamp;
	ECSEngine::Stream<wchar_t> library_name;
	ECSEngine::Stream<wchar_t> solution_path;
	ECSEngine::Stream<EditorModuleReflectedSetting> reflected_settings;
};

// -------------------------------------------------------------------------------------------------------------

struct EditorSandboxAssetHandlesSnapshot {
	ECSEngine::MemoryManager allocator;

	size_t FindHandle(unsigned int handle, ECS_ASSET_TYPE type) const {
		size_t offset = 0;
		size_t index = SearchBytes(Stream<unsigned int>(handles + offset, handle_count - offset), handle);
		while (index != -1) {
			if (handle_types[index + offset] == type) {
				return index + offset;
			}
			offset += index + 1;
			index = SearchBytes(Stream<unsigned int>(handles + offset, handle_count - offset), handle);
		}

		return -1;
	}

	// Use a SoA approach to make the search fast - we will use the handle to look for the values
	unsigned int* handles;
	unsigned int* reference_counts;
	// We need to record for each sandbox the increments and decrements that is has done
	// In order to be able to restore these when the simulation is finished
	int* reference_counts_change;
	ECS_ASSET_TYPE* handle_types;
	unsigned int handle_count;
	unsigned int handle_capacity;
};

// -------------------------------------------------------------------------------------------------------------

enum EDITOR_SANDBOX_FLAG : size_t {
	EDITOR_SANDBOX_FLAG_RUN_WORLD_WAITING_COMPILATION = 1 << 0,
	EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION = 1 << 1,
	EDITOR_SANDBOX_FLAG_PREFAB = 1 << 2, // Used to indicate that this sandbox is used for a prefab preview
	// This flag indicates whether or not the initialize functions were called
	// When the sandbox was started. It can happen that one of the modules is not
	// ready and when the sandbox finally resumes it is already in the run state
	// and it needs to know whether or not this initialization was performed or not
	EDITOR_SANDBOX_FLAG_WORLD_INITIALIZED = 1 << 3,
	// This flag is used by the shared component build function to indicate
	// That there is a pending event where build functions are considered and
	// The sandbox should wait them
	EDITOR_SANDBOX_FLAG_PENDING_BUILD_FUNCTIONS = 1 << 4,
	// When this flag is set, running the sandbox will result in serializing the scene
	// At the beginning of each frame. The scene is saved using a specialized delta algorithm
	EDITOR_SANDBOX_FLAG_RECORD_STATE = 1 << 5,
	// When this flag is set, the input that this sandbox sees will be written in a file, which
	// Can be replayed later on. It uses a specialized delta algorithm with fast seek times
	EDITOR_SANDBOX_FLAG_RECORD_INPUT = 1 << 6,
	// When this flag is set, the sandbox will have its runtime (entity) data read from a pre-recorded session
	EDITOR_SANDBOX_FLAG_REPLAY_STATE = 1 << 7,
	// When this flag is set, the sandbox will receive input from a pre-recorded input session
	EDITOR_SANDBOX_FLAG_REPLAY_INPUT = 1 << 8,
};

enum ECS_REFLECT EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY : unsigned char {
	EDITOR_SANDBOX_STATISTIC_CPU_USAGE,
	EDITOR_SANDBOX_STATISTIC_GPU_USAGE,
	EDITOR_SANDBOX_STATISTIC_RAM_USAGE,
	EDITOR_SANDBOX_STATISTIC_VRAM_USAGE,
	// This is the time of the simulation only
	EDITOR_SANDBOX_STATISTIC_SANDBOX_TIME,
	// This is the time of the total editor frame
	EDITOR_SANDBOX_STATISTIC_FRAME_TIME,
	// This is the time of the GPU simulation render time
	EDITOR_SANDBOX_STATISTIC_GPU_SANDBOX_TIME,
	EDITOR_SANDBOX_STATISTIC_DISPLAY_COUNT
};

extern ECSEngine::Stream<char> EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY_STRINGS[];

enum ECS_REFLECT EDITOR_SANDBOX_STATISTIC_DISPLAY_FORM : unsigned char {
	EDITOR_SANDBOX_STATISTIC_DISPLAY_TEXT,
	EDITOR_SANDBOX_STATISTIC_DISPLAY_GRAPH
};

enum ECS_REFLECT EDITOR_SANDBOX_CPU_STATISTICS_TYPE : unsigned char {
	EDITOR_SANDBOX_CPU_STATISTICS_NONE,
	EDITOR_SANDBOX_CPU_STATISTICS_BASIC,
	EDITOR_SANDBOX_CPU_STATISTICS_ADVANCED
};

enum ECS_REFLECT EDITOR_SANDBOX_GPU_STATISTICS_TYPE : unsigned char {
	EDITOR_SANDBOX_GPU_STATISTICS_NONE,
	EDITOR_SANDBOX_GPU_STATISTICS_BASIC,
	EDITOR_SANDBOX_GPU_STATISTICS_ADVANCED
};

enum EDITOR_SANDBOX_RECORDING_TYPE : unsigned char {
	EDITOR_SANDBOX_RECORDING_INPUT,
	EDITOR_SANDBOX_RECORDING_STATE,
	EDITOR_SANDBOX_RECORDING_TYPE_COUNT
};

// We are reflecting this since we want this to be stored in the sandbox file
struct ECS_REFLECT EditorSandboxStatisticsDisplay {
	ECS_INLINE bool IsGraphDisplay(EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY entry) const {
		return should_display[entry] && display_form[entry] == EDITOR_SANDBOX_STATISTIC_DISPLAY_GRAPH;
	}

	// This works like a master selection that hides all or allows the display
	bool is_enabled;
	bool should_display[EDITOR_SANDBOX_STATISTIC_DISPLAY_COUNT];
	EDITOR_SANDBOX_STATISTIC_DISPLAY_FORM display_form[EDITOR_SANDBOX_STATISTIC_DISPLAY_COUNT];
};

struct ECS_REFLECT EditorSandbox {
	ECS_INLINE ECSEngine::GlobalMemoryManager* GlobalMemoryManager() {
		return (ECSEngine::GlobalMemoryManager*)modules_in_use.allocator.allocator;
	}

	ECS_INLINE const ECSEngine::GlobalMemoryManager* GlobalMemoryManager() const {
		return (const ECSEngine::GlobalMemoryManager*)modules_in_use.allocator.allocator;
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
	
	EDITOR_SANDBOX_CPU_STATISTICS_TYPE cpu_statistics_type;
	EDITOR_SANDBOX_GPU_STATISTICS_TYPE gpu_statistics_type;

	ECSEngine::ECS_TRANSFORM_TOOL transform_tool;
	ECSEngine::ECS_TRANSFORM_SPACE transform_space;

	float camera_wasd_speed;

	ECSEngine::CameraParametersFOV camera_parameters[EDITOR_SANDBOX_VIEWPORT_COUNT];
	ECSEngine::OrientedPoint camera_saved_orientations[EDITOR_SANDBOX_SAVED_CAMERA_TRANSFORM_COUNT];

	// Save these to the sandbox file
	EditorSandboxStatisticsDisplay statistics_display;

	float simulation_speed_up_factor;

	// How many seconds it should wait until an entire input state is written for the input recording
	float input_recorder_entire_state_tick_seconds;
	// How many seconds it should wait until an entire state is written for the state recording
	float state_recorder_entire_state_tick_seconds;
	// When this flag is set, every run will generate a new file, forming a sequence
	bool is_input_recorder_file_automatic;
	// When this flag is set, every run will generate a new file, forming a sequence
	bool is_state_recorder_file_automatic;

	ECS_FIELDS_END_REFLECT;

	// This flag is set when initiating keyboard transform actions
	bool transform_display_axes;
	// Indicate which axis of the transform tool is currently selected
	bool transform_tool_selected[ECS_AXIS_COUNT];
	// We need to record separately the tool that is being used for the keyboard presses
	ECSEngine::ECS_TRANSFORM_TOOL transform_keyboard_tool;
	// We need to record the transform space for the keyboard press
	ECSEngine::ECS_TRANSFORM_SPACE transform_keyboard_space;

	bool is_camera_wasd_movement;
	// We need to keep this flag in order to prevent running the simulation
	// Once the sandbox crashed
	bool is_crashed;

	// This describes the count of background build functions that are running
	std::atomic<unsigned int> background_component_build_functions;

	// For components that have build functions, we must lock their dependencies
	// Such that they don't get modified or removed while the build is happening
	// Since this can corrupt the process
	struct LockedEntityComponent {
		ECS_INLINE bool operator == (LockedEntityComponent other) const {
			return entity == other.entity && component == other.component && is_shared == other.is_shared;
		}

		Entity entity;
		Component component;
		bool is_shared;
	};
	ResizableStream<LockedEntityComponent> locked_entity_components;
	ResizableStream<Component> locked_global_components;
	// We need this lock to synchronize the access to these 2 streams since
	// The background thread can remove from them while the main thread can
	// Push into them
	SpinLock locked_components_lock;

	EDITOR_SANDBOX_STATE run_state;
	bool is_scene_dirty;
	std::atomic<unsigned char> locked_count;

	size_t runtime_settings_last_write;
	ECSEngine::WorldDescriptor runtime_descriptor;
	ECSEngine::AssetDatabaseReference database;

	ECSEngine::RenderDestination viewport_render_destination[EDITOR_SANDBOX_VIEWPORT_COUNT];
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

	ECSEngine::ResizableStream<ECSEngine::Entity> virtual_entities_slots;
	ECSEngine::ResizableStream<EditorSandboxEntitySlot> virtual_entity_slot_type;
	// These flags indicate when the unused slots need to be recomputed (after a clear/reset for example)
	bool virtual_entities_slots_recompute;

	// These are all the components which have enabled debug drawing
	ECSEngine::ResizableStream<ECSEngine::ComponentWithType> enabled_debug_draw;
	// This drawer is used to redirect draws from the main game into the scene window
	ECSEngine::DebugDrawer redirect_drawer;

	ECSEngine::MemoryManager runtime_module_snapshot_allocator;
	ECSEngine::ResizableStream<EditorSandboxModuleSnapshot> runtime_module_snapshots;

	EditorSandboxAssetHandlesSnapshot runtime_asset_handle_snapshot;

	ECSEngine::WorldProfiling world_profiling;

	// This is data that is to be transfered in between module compilations
	ECSEngine::ResizableLinearAllocator sandbox_world_transfer_data_allocator;
	ECSEngine::Stream<ECSEngine::TaskSchedulerTransferStaticData> sandbox_world_transfer_data;

	// Record this here, such that we can reference it directly in the entity managers
	void* entity_manager_auto_generated_data;

	// A structure that contains all the common fields that are used for a recorder
	struct Recorder {
		ECSEngine::DeltaStateWriter delta_writer;
		ECSEngine::CapacityStream<wchar_t> file;
		bool is_initialized;
		// This value caches the validity of the recorder file. It takes into account automatic recording as well
		bool is_file_valid;
		// Records the highest index that is being used by the automatic sequence. If no entry exists, it will be -1
		unsigned int file_automatic_index;
	};

	// A structure that contains all the common fields that are used for a replay player
	struct ReplayPlayer {
		ECSEngine::DeltaStateReader delta_reader;
		ECSEngine::CapacityStream<wchar_t> file;
		bool is_initialized;
		// When set, the simulation will use the delta time from this file, instead of using the "real" current delta time
		bool is_driving_delta_time;
		// This boolean caches the validity of the replay file
		bool is_file_valid;
	};

	// The input recorder structure - should be used only when the flag EDITOR_SANDBOX_FLAG_RECORD_INPUT is set
	Recorder input_recorder;

	// The input replay structure - should be used only when the flag EDITOR_SANDBOX_FLAG_REPLAY_INPUT is set
	ReplayPlayer input_replay;

	// The state recorder structure - should be used only when the flag EDITOR_SANDBOX_FLAG_RECORD_STATE is set
	Recorder state_recorder;

	// The state replay structure - should be used only when the flag EDITOR_SANDBOX_FLAG_REPLAY_STATE is set
	ReplayPlayer state_replay;

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