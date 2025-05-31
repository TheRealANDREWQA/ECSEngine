#include "editorpch.h"
#include "SandboxEntityOperations.h"
#include "Sandbox.h"
#include "SandboxModule.h"
#include "../Editor/EditorState.h"
#include "../Modules/Module.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Assets/AssetManagement.h"
#include "ECSEngineForEach.h"

using namespace ECSEngine;

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	Component component = editor_state->editor_components.GetComponentID(component_name);
	if (component.Valid()) {
		if (entity_manager->ExistsEntity(entity)) {
			unsigned short byte_size = entity_manager->ComponentSize(component);
			// Default initialize the component with zeroes
			// Assume a max of ECS_KB for a unique component
			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);
			entity_manager->AddComponentCommit(entity, component);
			editor_state->editor_components.ResetComponent(editor_state, sandbox_index, component_name, entity, ECS_COMPONENT_UNIQUE);

			// Destroy the old archetype if it no longer has entities
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);
			
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add component {#} to entity {#} for {#}.", component_name, entity.value, ViewportString(viewport));
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntitySharedComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	Component component = editor_state->editor_components.GetComponentID(component_name);
	if (component.Valid()) {
		if (entity_manager->ExistsEntity(entity)) {
			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);

			if (instance.value == -1) {
				// If there is a build function, we must call the build function after finding this instance
				// This instance should be valid only for this entity
				instance = GetSandboxSharedComponentDefaultInstance(editor_state, sandbox_index, component, viewport);
				EditorModuleComponentBuildEntry build_entry = GetModuleComponentBuildEntry(editor_state, component_name);
				if (build_entry.entry != nullptr) {
					// Need to add the component now or the build entry might fail if there
					// Are no matching entities
					entity_manager->AddSharedComponentCommit(entity, component, instance);
					CallModuleComponentBuildFunctionShared(
						editor_state, 
						sandbox_index, 
						&build_entry,
						component,
						instance,
						entity
					);
				}
				else {
					// Emit a warning if the module is out of date and it contains a build function for this component
					EditorModuleComponentBuildEntry out_of_date_build_entry = GetModuleComponentBuildEntry(editor_state, component_name, true);
					if (out_of_date_build_entry.entry != nullptr) {
						ECS_FORMAT_TEMP_STRING(warning, "Module {#} contains a build function for component {#}, but it is out of date.", 
							editor_state->project_modules->buffer[out_of_date_build_entry.module_index].library_name, component_name);
						EditorSetConsoleWarn(warning);
					}
					entity_manager->AddSharedComponentCommit(entity, component, instance);
				}
			}
			else {
				entity_manager->AddSharedComponentCommit(entity, component, instance);
			}

			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to add shared component {#} to entity {#} for {#}.", component_name, entity.value, ViewportString(viewport));
		EditorSetConsoleError(console_message);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	if (editor_state->editor_components.IsUniqueComponent(component_name)) {
		AddSandboxEntityComponent(editor_state, sandbox_index, entity, component_name, viewport);
	}
	else {
		AddSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, { -1 }, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void AddSandboxSelectedEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->selected_entities.Add(entity);
	sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
	SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
}

// ------------------------------------------------------------------------------------------------------------------------------

void AttachSandboxEntityName(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
		AllocatorPolymorphic allocator = entity_manager->GetComponentAllocator(name_component);

		Name name_data = { StringCopy(allocator, name) };
		entity_manager->AddComponentCommit(entity, name_component, &name_data);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

// We record this thread id in case the thread running the function
// Crashes and forgets to release the lock since we capture the crash
static size_t BACKGROUND_TASK_GPU_LOCK_THREAD_ID = -1;

static EDITOR_EVENT(BackgroundTaskGPULock) {
	AtomicFlag* flag = (AtomicFlag*)_data;
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
		flag->Signal();
		return false;
	}
	return true;
}

static void EditorModuleComponentBuildGPULockLock(void* data) {
	EditorState* editor_state = (EditorState*)data;
	// We can use the PREVENT_RESOURCE_LOADING flag as a GPU lock
	// We need an editor event to acquire this flag since there are
	// Many events which run on the main thread that don't check for
	// Async tasks acquiring this flag
	AtomicFlag atomic_flag;
	EditorAddEvent(editor_state, BackgroundTaskGPULock, &atomic_flag, 0);
	atomic_flag.Wait();
	
	// Set the background task gpu lock thread id such that
	// We know, if we crash, to release the lock
	BACKGROUND_TASK_GPU_LOCK_THREAD_ID = OS::GetCurrentThreadID();
	// We need to acquire the editor state frame gpu lock as well, such that
	// We synchronize with the main thread
	editor_state->frame_gpu_lock.Lock();
}

static void EditorModuleComponentBuildGPULockUnlock(void* data) {
	EditorState* editor_state = (EditorState*)data;
	editor_state->frame_gpu_lock.Unlock();
	EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
	// Set this to -1 to indicate that it is unlocked
	BACKGROUND_TASK_GPU_LOCK_THREAD_ID = -1;
}

static bool EditorModuleComponentBuildGPULockIsLocked(void* data) {
	EditorState* editor_state = (EditorState*)data;
	return EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
}

static bool EditorModuleComponentBuildGPULockTryLock(void* data) {
	EditorState* editor_state = (EditorState*)data;
	bool acquired_lock = EditorStateTrySetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING, 0, 1);
	if (acquired_lock) {
		// Get the editor state frame gpu lock as well
		editor_state->frame_gpu_lock.Lock();
	}
	return acquired_lock;
}

static void EditorModuleComponentBuildPrintFunction(void* data, Stream<char> message, ECS_CONSOLE_MESSAGE_TYPE message_type) {
	switch (message_type) {
	case ECS_CONSOLE_INFO:
		EditorSetConsoleInfo(message);
		break;
	case ECS_CONSOLE_WARN:
		EditorSetConsoleWarn(message);
		break;
	case ECS_CONSOLE_ERROR:
		EditorSetConsoleError(message);
		break;
	case ECS_CONSOLE_TRACE:
		EditorSetConsoleTrace(message);
		break;
	default:
		EditorSetConsoleWarn("Module component build function invalid message type");
	}
}

struct BuildFunctionWrapperData {
	ThreadTask task;
	EditorState* editor_state;
	unsigned int sandbox_index;
	unsigned char locked_components_count;
	unsigned char locked_global_count;
	bool is_shared_component;
	Component component;
	std::atomic<bool>* finish_flag;
	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION module_configuration;

	EditorSandbox::LockedEntityComponent locked_components[8];
	Component locked_globals[8];
};

struct BuildFunctionWrapperEventData {
	unsigned int sandbox_index;
};

static EDITOR_EVENT(BuildFunctionWrapperEvent) {
	BuildFunctionWrapperEventData* data = (BuildFunctionWrapperEventData*)_data;
	SetSandboxSceneDirty(editor_state, data->sandbox_index);
	RenderSandboxViewports(editor_state, data->sandbox_index);
	return false;
}

// We need this wrapper to set the finish flag once
// It has finished and to eliminate the locked dependencies
ECS_THREAD_TASK(BuildFunctionWrapper) {
	BuildFunctionWrapperData* data = (BuildFunctionWrapperData*)_data;
	EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_index);

	void* task_data = data->task.data_size == 0 ? data->task.data : OffsetPointer(data, sizeof(*data));
	CrashHandler previous_crash_handler = EditorSetBackgroundThreadSpecificCrashHandler(EditorInduceSEHCrashHandler());
	// Need to handle the crashes and asserts
	__try {
		data->task.function(thread_id, world, task_data);
	}
	__except (OS::ExceptionHandlerFilterDefault(GetExceptionInformation())) {
		Stream<char> component_name = data->editor_state->editor_components.ComponentFromID(data->component,
			data->is_shared_component ? ECS_COMPONENT_SHARED : ECS_COMPONENT_UNIQUE);
		ECS_FORMAT_TEMP_STRING(console_message, "Build function background task for component {#} has crashed in sandbox {#}", component_name, data->sandbox_index);
		EditorSetConsoleError(console_message);
		ModuleComponentBuildEntry* entry = GetModuleComponentBuildEntry(
			data->editor_state,
			data->module_index,
			data->module_configuration,
			component_name
		);
		entry->has_crashed = true;

		// Check to see if the function acquire the GPU lock. If it did, release it
		if (BACKGROUND_TASK_GPU_LOCK_THREAD_ID != -1) {
			if (BACKGROUND_TASK_GPU_LOCK_THREAD_ID == OS::GetCurrentThreadID()) {
				EditorModuleComponentBuildGPULockUnlock(data->editor_state);
			}
		}
	}
	EditorSetBackgroundThreadSpecificCrashHandler(previous_crash_handler);

	// Acquire the component lock
	if (data->locked_components_count > 0 || data->locked_global_count > 0) {
		sandbox->locked_components_lock.Lock();

		for (unsigned char index = 0; index < data->locked_components_count; index++) {
			unsigned int find_index = sandbox->locked_entity_components.Find(data->locked_components[index]);
			ECS_ASSERT(find_index != -1);
			sandbox->locked_entity_components.RemoveSwapBack(find_index);
		}

		for (unsigned char index = 0; index < data->locked_global_count; index++) {
			unsigned int find_index = sandbox->locked_global_components.Find(data->locked_globals[index]);
			ECS_ASSERT(find_index != -1);
			sandbox->locked_global_components.RemoveSwapBack(find_index);
		}

		sandbox->locked_components_lock.Unlock();
	}

	if (data->finish_flag != nullptr) {
		data->finish_flag->store(true, ECS_RELAXED);
	}
	DecrementSandboxModuleComponentBuildCount(data->editor_state, data->sandbox_index);
	DecrementModuleInfoLockCount(data->editor_state, data->module_index, data->module_configuration);
	
	// Add an event to re-render the sandbox and set the scene as dirty
	BuildFunctionWrapperEventData event_data = { data->sandbox_index };
	EditorAddEvent(data->editor_state, BuildFunctionWrapperEvent, &event_data, sizeof(event_data));
}

// Returns true if a background task was launched, else false
// This has to be run on the main thread
static bool CallModuleComponentBuildFunctionBase(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION configuration,
	Component component,
	bool is_shared,
	Entity entity,
	ModuleComponentBuildEntry* build_entry,
	ModuleComponentBuildFunctionData* build_data,
	std::atomic<bool>* finish_flag
) {
	// If the build entry has crashed, do not continue. Do not output an error message
	if (build_entry->has_crashed) {
		return false;
	}

	build_data->entity = entity;
	build_data->component = GetSandboxEntityComponentEx(editor_state, sandbox_index, entity, component, is_shared);
	build_data->component_allocator = GetSandboxComponentAllocatorEx(
		editor_state, 
		sandbox_index, 
		component, 
		is_shared ? ECS_COMPONENT_SHARED : ECS_COMPONENT_UNIQUE
	);
	build_data->component_allocator.allocation_type = ECS_ALLOCATION_MULTI;
	build_data->stack_memory->size = 0;

	// This is executed on the main thread only
	EditorSetMainThreadCrashHandlerIntercept();
	ThreadTask task;
	__try {
		 task = build_entry->function(build_data);
	}
	__except(OS::ExceptionHandlerFilterDefault(GetExceptionInformation())) {
		Stream<char> component_name = editor_state->editor_components.ComponentFromID(component, is_shared ? ECS_COMPONENT_SHARED : ECS_COMPONENT_UNIQUE);
		ECS_FORMAT_TEMP_STRING(console_message, "Build entry for component {#} has crashed", component_name);
		EditorSetConsoleError(console_message);
		build_entry->has_crashed = true;
	}
	EditorClearMainThreadCrashHandlerIntercept();

	if (task.function != nullptr) {
		ECS_STACK_VOID_STREAM(wrapper_data_storage, ECS_KB * 4);
		BuildFunctionWrapperData* wrapper_data = wrapper_data_storage.Reserve<BuildFunctionWrapperData>();
		wrapper_data->locked_components_count = 0;
		wrapper_data->locked_global_count = 0;
		wrapper_data_storage.AssertCapacity(task.data_size);

		// We also need to register the component dependencies
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		sandbox->locked_components_lock.Lock();

		for (size_t index = 0; index < build_entry->component_dependencies.size; index++) {
			ECS_COMPONENT_TYPE component_type = editor_state->editor_components.GetComponentType(build_entry->component_dependencies[index]);
			Component component_id = editor_state->editor_components.GetComponentID(build_entry->component_dependencies[index]);
			if (component_type == ECS_COMPONENT_UNIQUE || component_type == ECS_COMPONENT_SHARED) {
				EditorSandbox::LockedEntityComponent locked_entry = { entity, component_id , component_type == ECS_COMPONENT_SHARED };
				sandbox->locked_entity_components.Add(locked_entry);
				wrapper_data->locked_components[wrapper_data->locked_components_count++] = locked_entry;
				ECS_ASSERT(wrapper_data->locked_components_count <= std::size(wrapper_data->locked_components));
			}
			else {
				sandbox->locked_global_components.Add(component_id);
				wrapper_data->locked_globals[wrapper_data->locked_global_count++] = component_id;
				ECS_ASSERT(wrapper_data->locked_global_count <= std::size(wrapper_data->locked_globals));
			}
		}
		// We need to add ourselves to the locked components
		// As well such that the component is not removed accidentally
		EditorSandbox::LockedEntityComponent this_component_locked_entry = { entity, component, is_shared };
		sandbox->locked_entity_components.Add(this_component_locked_entry);
		wrapper_data->locked_components[wrapper_data->locked_components_count++] = this_component_locked_entry;
		ECS_ASSERT(wrapper_data->locked_components_count <= std::size(wrapper_data->locked_components));

		sandbox->locked_components_lock.Unlock();

		wrapper_data->editor_state = editor_state;
		wrapper_data->sandbox_index = sandbox_index;
		wrapper_data->task = task;
		wrapper_data->is_shared_component = is_shared;
		wrapper_data->component = component;
		wrapper_data->finish_flag = finish_flag;
		wrapper_data->module_index = module_index;
		wrapper_data->module_configuration = configuration;
		if (task.data_size > 0) {
			memcpy(OffsetPointer(wrapper_data, sizeof(*wrapper_data)), task.data, task.data_size);
		}

		// Increment this in order to let the main thread know that it should not run
		// The simulation for this sandbox
		IncrementSandboxModuleComponentBuildCount(editor_state, sandbox_index);
		IncrementModuleInfoLockCount(editor_state, module_index, configuration);
		EditorStateAddBackgroundTask(editor_state, { BuildFunctionWrapper, wrapper_data, sizeof(*wrapper_data) + task.data_size });
		return true;
	}

	return false;
}

static ModuleComponentBuildFunctionData CreateBuildDataBase(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<void>* stack_memory) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	ModuleComponentBuildFunctionData build_data;
	build_data.entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	build_data.world_resource_manager = sandbox->sandbox_world.resource_manager;
	build_data.world_graphics = sandbox->sandbox_world.graphics;
	build_data.world_global_memory_manager = sandbox->sandbox_world.memory;
	build_data.stack_memory = stack_memory;

	build_data.gpu_lock.lock_function = EditorModuleComponentBuildGPULockLock;
	build_data.gpu_lock.unlock_function = EditorModuleComponentBuildGPULockUnlock;
	build_data.gpu_lock.is_locked_function = EditorModuleComponentBuildGPULockIsLocked;
	build_data.gpu_lock.try_lock_function = EditorModuleComponentBuildGPULockTryLock;
	build_data.gpu_lock.data = editor_state;

	build_data.print_message.print_function = EditorModuleComponentBuildPrintFunction;
	build_data.print_message.data = nullptr;

	return build_data;
}

void CallModuleComponentBuildFunctionUnique(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ModuleComponentBuildEntry* build_entry,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION module_configuration,
	Stream<Entity> entities,
	Component component
)
{
	// If the entry has crashed, do not continue
	if (build_entry->has_crashed) {
		Stream<char> component_name = editor_state->editor_components.ComponentFromID(component, ECS_COMPONENT_UNIQUE);
		ECS_FORMAT_TEMP_STRING(console_message, "Build function for component {#} is crashed", component_name);
		EditorSetConsoleWarn(console_message);
		return;
	}

	ECS_STACK_VOID_STREAM(stack_memory, 512);

	ModuleComponentBuildFunctionData build_data = CreateBuildDataBase(editor_state, sandbox_index, &stack_memory);

	for (size_t index = 0; index < entities.size; index++) {
		CallModuleComponentBuildFunctionBase(
			editor_state,
			sandbox_index,
			module_index,
			module_configuration,
			component,
			false,
			entities[index],
			build_entry,
			&build_data,
			nullptr
		);
	}
}

void CallModuleComponentBuildFunctionUnique(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EditorModuleComponentBuildEntry* build_entry,
	Stream<Entity> entities,
	Component component
) {
	CallModuleComponentBuildFunctionUnique(editor_state, sandbox_index, build_entry->entry, build_entry->module_index, 
		build_entry->module_configuration, entities, component);
}

struct SplitBuildSharedInstanceData {
	struct BackgroundProcessing {
		std::atomic<bool> has_finished;
		// This is set to true after it was checked once
		bool checked;
		SharedInstance instance;
	};
	struct BackgroundProcessingStream {
		Stream<BackgroundProcessing> entries;
		bool is_entire_finished;
	};

	Stream<Entity> entities_to_split;
	size_t last_split_entity;
	ModuleComponentBuildEntry* build_entry;
	unsigned int sandbox_index;
	Component component;
	SharedInstance original_instance;

	unsigned int module_index;
	EDITOR_MODULE_CONFIGURATION module_configuration;

	// In case we have background processing, store these and access them later
	// To perform the merge once they have finished
	ResizableStream<BackgroundProcessingStream> background_processing;
};

static EDITOR_EVENT(SplitBuildSharedInstance) {
	SplitBuildSharedInstanceData* data = (SplitBuildSharedInstanceData*)_data;

	// Process at max a certain count per frame
	const size_t PER_FRAME_ENTITY_PROCESS_COUNT = 500;
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, data->sandbox_index);

	bool performed_background_processing = false;
	// Check firstly the background processing streams, to reduce the burden on the shared instance
	// Count and the component allocator
	if (data->background_processing.size > 0) {
		for (size_t index = 0; index < data->background_processing.size; index++) {
			if (!data->background_processing[index].is_entire_finished) {
				bool still_waiting = false;
				for (size_t subindex = 0; subindex < data->background_processing[index].entries.size; subindex++) {
					if (!data->background_processing[index].entries[subindex].has_finished.load(ECS_RELAXED)) {
						still_waiting = true;
					}
					else {
						if (!data->background_processing[index].entries[subindex].checked) {
							data->background_processing[index].entries[subindex].checked = true;
							// Try to merge this instance
							entity_manager->TryMergeSharedInstanceCommit(data->component, data->background_processing[index].entries[subindex].instance);
						}
					}
				}
				if (!still_waiting) {
					data->background_processing[index].is_entire_finished = true;
				}
				performed_background_processing = true;
			}
		}
	}

	// Check to see if we have any more entities to perform
	bool processed_entities = false;
	if (data->last_split_entity < data->entities_to_split.size) {
		// Before this, remove any entity that was deleted before hand
		for (size_t index = data->last_split_entity; index < data->entities_to_split.size; index++) {
			if (!entity_manager->ExistsEntity(data->entities_to_split[index])) {
				data->entities_to_split.RemoveSwapBack(index);
				index--;
			}
		}

		// Create new instances for them, and call the base function for them
		size_t allocate_count = ClampMax(data->entities_to_split.size - data->last_split_entity, PER_FRAME_ENTITY_PROCESS_COUNT);
		if (allocate_count > 0) {
			Stream<SplitBuildSharedInstanceData::BackgroundProcessing> background_processing;
			background_processing.Initialize(editor_state->EditorAllocator(), allocate_count);
			const void* instance_data = entity_manager->GetSharedData(data->component, data->original_instance);

			ECS_STACK_VOID_STREAM(stack_memory, 1024);
			ModuleComponentBuildFunctionData build_data = CreateBuildDataBase(editor_state, data->sandbox_index, &stack_memory);

			bool has_background_tasks = false;
			for (size_t index = 0; index < allocate_count; index++) {
				background_processing[index].has_finished.store(false, ECS_RELAXED);
				background_processing[index].checked = false;
				background_processing[index].instance = entity_manager->RegisterSharedInstanceCommit(data->component, instance_data);
				// Call the base module function
				bool background_thread = CallModuleComponentBuildFunctionBase(
					editor_state,
					data->sandbox_index,
					data->module_index,
					data->module_configuration,
					data->component,
					true,
					data->entities_to_split[data->last_split_entity + index],
					data->build_entry,
					&build_data,
					&background_processing[index].has_finished
				);
				if (background_thread) {
					has_background_tasks = true;
				}
				else {
					// We can perform the merge right now
					entity_manager->TryMergeSharedInstanceCommit(data->component, background_processing[index].instance);
				}
			}

			if (has_background_tasks) {
				// Add this entry to the overall background tasks
				data->background_processing.Add({ background_processing, false });
			}
			else {
				// We can deallocate the data
				background_processing.Deallocate(editor_state->EditorAllocator());
			}
			data->last_split_entity += allocate_count;
			processed_entities = true;
		}
	}

	if (!processed_entities && !performed_background_processing) {
		// There was nothing to be done, we can finish
		// Deallocate everything now
		data->entities_to_split.Deallocate(editor_state->EditorAllocator());
		for (size_t index = 0; index < data->background_processing.size; index++) {
			data->background_processing[index].entries.Deallocate(editor_state->EditorAllocator());
		}
		data->background_processing.FreeBuffer();

		// At the end, also perform an elimination of the unused shared instances
		// We do this mostly since the initial build instance now can be not referenced
		entity_manager->UnregisterUnreferencedSharedInstancesCommit(data->component);

		// Decrement the lock counts
		DecrementModuleInfoLockCount(editor_state, data->module_index, data->module_configuration);
		DecrementSandboxModuleComponentBuildCount(editor_state, data->sandbox_index);
		return false;
	}
	return true;
}

/*
	Here we have quite a lot to do. Firstly, we need to make the distinction between
	Shared and unique components. For unique components, we can just call the function
	Without worries since it is unique. For shared components, we need to determine if this
	Shared instance is referenced with the same dependencies everywhere. This means that if
	Any dependency is unique, we must create a separate instance for each entity that references
	This shared instance. If there are only shared dependencies, we must create new instances
	And call the build function for each permutation. At the end, the instances can be merged
	Back if they yield the same instance. So, the heavy lifting is for the shared components.

	EDIT: The comment describes what should have been done. But generating all the permutations
	Of shared components is extremely time consuming, so we fall back to a more simpler algorithm
	For each entity that references this shared instance, we launch a build function for it and merge
	At the end if multiple copies of the same shared instance appear. This covers all cases when the
	Build entry has dependencies. When it doesn't have dependencies, we can call the build function
	On it directly
*/

void CallModuleComponentBuildFunctionShared(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ModuleComponentBuildEntry* build_entry,
	unsigned int module_index,
	EDITOR_MODULE_CONFIGURATION module_configuration,
	Component component,
	SharedInstance build_instance,
	Entity changed_entity
) {
	// If the entry has crashed, do not continue
	if (build_entry->has_crashed) {
		Stream<char> component_name = editor_state->editor_components.ComponentFromID(component, ECS_COMPONENT_SHARED);
		ECS_FORMAT_TEMP_STRING(console_message, "Build function for component {#} is crashed", component_name);
		EditorSetConsoleWarn(console_message);
		return;
	}

	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index);
	if (build_entry->component_dependencies.size == 0) {
		ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 2);
		ModuleComponentBuildFunctionData build_data = CreateBuildDataBase(editor_state, sandbox_index, &stack_memory);
		// We can call the build function directly only for this instance
		CallModuleComponentBuildFunctionBase(
			editor_state, 
			sandbox_index, 
			module_index,
			module_configuration,
			component, 
			true, 
			changed_entity, 
			build_entry, 
			&build_data, 
			nullptr
		);
	}
	else {
		// Use an editor event to perform this across multiple frames since it can be quite time
		// Consuming and to not stall the main thread too much
		ResizableStream<Entity> matching_entities;
		matching_entities.Initialize(editor_state->EditorAllocator(), 0);
		entity_manager->GetEntitiesForSharedInstance(component, build_instance, &matching_entities);
		ECS_ASSERT(matching_entities.size > 0, "Trying to build component shared instance without matching entities");

		// Have a special case when this entity is the only one which references the instance.
		// We can safely call the build function directly on it, without having a split event
		if (matching_entities.size == 1) {
			ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 2);
			ModuleComponentBuildFunctionData build_data = CreateBuildDataBase(editor_state, sandbox_index, &stack_memory);
			// We can call the build function directly only for this instance
			CallModuleComponentBuildFunctionBase(
				editor_state, 
				sandbox_index, 
				module_index,
				module_configuration,
				component, 
				true, 
				changed_entity, 
				build_entry, 
				&build_data, 
				nullptr
			);
		}
		else {
			// Gather all the entities that have this shared instance
			SplitBuildSharedInstanceData split_data;
			split_data.build_entry = build_entry;
			split_data.component = component;
			split_data.last_split_entity = 0;
			split_data.entities_to_split = matching_entities.ToStream();
			split_data.original_instance = build_instance;
			split_data.sandbox_index = sandbox_index;
			split_data.background_processing.Initialize(editor_state->EditorAllocator(), 0);
			split_data.module_index = module_index;
			split_data.module_configuration = module_configuration;

			// Set the pending flag to the sandbox
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_PENDING_BUILD_FUNCTIONS);
			EditorAddEvent(editor_state, SplitBuildSharedInstance, &split_data, sizeof(split_data));

			// Lock the module info such that it won't be removed while we reference it
			IncrementModuleInfoLockCount(editor_state, module_index, module_configuration);
			IncrementSandboxModuleComponentBuildCount(editor_state, sandbox_index);
		}
	}
}

void CallModuleComponentBuildFunctionShared(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EditorModuleComponentBuildEntry* build_entry,
	Component component,
	SharedInstance build_instance,
	Entity changed_entity
) {
	CallModuleComponentBuildFunctionShared(editor_state, sandbox_index, build_entry->entry, build_entry->module_index, 
		build_entry->module_configuration, component, build_instance, changed_entity);
}

// ------------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxEntityName(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> new_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
		if (entity_manager->HasComponent(entity, name_component)) {
			Name* name_data = (Name*)entity_manager->GetComponent(entity, name_component);
			AllocatorPolymorphic allocator = entity_manager->GetComponentAllocator(name_component);
			if (name_data->name.size > 0) {
				Deallocate(allocator, name_data->name.buffer);
			}
			name_data->name = StringCopy(allocator, new_name);
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ChangeSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, Stream<Entity> entities)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->selected_entities.CopyOther(entities);
	// This is used by the UI to deduce if the change was done by it or by some other service
	sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
	SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
}

// ------------------------------------------------------------------------------------------------------------------------------

void ClearSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index)
{
	ChangeSandboxSelectedEntities(editor_state, sandbox_index, { nullptr, 0 });
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport)
{
	return CreateSandboxEntity(editor_state, sandbox_index, {}, {}, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CreateSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ComponentSignature unique, 
	SharedComponentSignature shared,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
	ECS_ASSERT(unique.count < ECS_ARCHETYPE_MAX_COMPONENTS);
	memcpy(unique_components, unique.indices, sizeof(Component) * unique.count);
	unique.indices = unique_components;

	Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
	unique[unique.count] = name_component;
	unique.count++;

	ECS_STACK_CAPACITY_STREAM(unsigned char, shared_instance_that_needs_reset, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index);
	// For every shared component, check to see if the instance exists
	for (size_t index = 0; index < shared.count; index++) {
		bool exists = entity_manager->ExistsSharedInstance(shared.indices[index], shared.instances[index]);
		if (!exists) {
			// Register a shared instance and reset it later after we have the entity
			char _component_storage[ECS_COMPONENT_MAX_BYTE_SIZE];
			// Memset this data just to be safe, in case there are buffers
			// To be deallocated or sizes that are relevant
			memset(_component_storage, 0, entity_manager->SharedComponentSize(shared.indices[index]));
			entity_manager->RegisterSharedInstanceCommit(shared.indices[index], _component_storage);
			shared_instance_that_needs_reset.Add(index);
		}
	}

	Entity entity = entity_manager->CreateEntityCommit(unique, shared);
	for (size_t index = 0; index < unique.count; index++) {
		void* component_data = entity_manager->GetComponent(entity, unique[index]);
		// Memset the data since 0 is a good default in case there are buffers
		// Or sizes in the data that would be relevant in the deallocation process
		memset(component_data, 0, entity_manager->ComponentSize(unique[index]));
		editor_state->editor_components.ResetComponent(
			editor_state, 
			sandbox_index, 
			editor_state->editor_components.ComponentFromID(unique[index], ECS_COMPONENT_UNIQUE), 
			entity, 
			ECS_COMPONENT_UNIQUE
		);
	}
	for (unsigned int index = 0; index < shared_instance_that_needs_reset.size; index++) {
		editor_state->editor_components.ResetComponent(
			editor_state,
			sandbox_index,
			editor_state->editor_components.ComponentFromID(shared.indices[index], ECS_COMPONENT_SHARED),
			entity,
			ECS_COMPONENT_SHARED
		);
	}

	ECS_STACK_CAPACITY_STREAM(char, entity_name, 512);
	entity.ToString(entity_name);
	ChangeSandboxEntityName(editor_state, sandbox_index, entity, entity_name, viewport);
	SetSandboxSceneDirty(editor_state, sandbox_index, viewport);

	return entity;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool CreateSandboxGlobalComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	const void* data, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	// Check to see if the component already exists - fail if it does
	if (entity_manager->ExistsGlobalComponent(component)) {
		return false;
	}

	ECS_STACK_CAPACITY_STREAM(size_t, component_storage, ECS_KB);
	Stream<char> component_name = editor_state->editor_components.ComponentFromID(component, ECS_COMPONENT_GLOBAL);
	size_t component_size = editor_state->editor_components.GetComponentByteSize(component_name);
	const Reflection::ReflectionType* component_type = editor_state->editor_components.GetType(component_name);
	if (data == nullptr) {
		ECS_ASSERT(component_size < component_storage.capacity * sizeof(component_storage[0]));
		editor_state->editor_components.internal_manager->SetInstanceDefaultData(component_type, component_storage.buffer);
		data = component_storage.buffer;
	}

	// Retrieve the component functions
	entity_manager->RegisterGlobalComponentCommit(component, component_size, data, component_name);
	SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	return true;
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity CopySandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	Entity destination_entity;
	if (CopySandboxEntities(editor_state, sandbox_index, entity, 1, &destination_entity, viewport)) {
		return destination_entity;
	}
	else {
		return { (unsigned int)-1 };
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

bool CopySandboxEntities(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	unsigned int count, 
	Entity* copied_entities,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->CopyEntityCommit(entity, count, true, copied_entities);

		if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
			// Increment the reference count for all assets that this entity references
			ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, entity_assets, 128);
			GetSandboxEntityAssets(editor_state, sandbox_index, entity, &entity_assets, viewport);

			for (unsigned int index = 0; index < entity_assets.size; index++) {
				IncrementAssetReferenceInSandbox(editor_state, entity_assets[index].handle, entity_assets[index].type, sandbox_index, count);
			}
		}

		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		return true;
	}
	else {
		return false;
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

struct ConvertToOrFromLinkData {
	ConvertToAndFromLinkBaseData base_data;
	Component component;
	ECS_COMPONENT_TYPE component_type;
};

static ConvertToOrFromLinkData GetConvertToOrFromLinkData(
	const EditorState* editor_state,
	Stream<char> link_component,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data;

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(link_component);
	ECS_ASSERT(target.size > 0);

	unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, link_component);
	ModuleLinkComponentTarget link_target = { nullptr, nullptr };
	// If the module index is -1, then it is an engine link component
	if (module_index != -1) {
		link_target = GetModuleLinkComponentTarget(editor_state, module_index, link_component);
	}
	else {
		// Check the engine component side
		link_target = GetEngineLinkComponentTarget(editor_state, link_component);
	}

	const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_component);
	const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target);

	ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(target_type);
	Component target_id = GetReflectionTypeComponent(target_type);

	convert_data.base_data.allocator = allocator;
	convert_data.base_data.asset_database = editor_state->asset_database;
	convert_data.base_data.link_type = link_type;
	convert_data.base_data.module_link = link_target;
	convert_data.base_data.reflection_manager = editor_state->editor_components.internal_manager;
	convert_data.base_data.target_type = target_type;
	convert_data.component = target_id;
	convert_data.component_type = component_type;

	return convert_data;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorTargetToLinkComponent(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Stream<char> link_component,
	Entity entity,
	void* link_data,
	const void* previous_link_data,
	const void* previous_target_data,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	const void* target_data = nullptr;
	if (convert_data.component_type == ECS_COMPONENT_SHARED) {
		target_data = entity_manager->GetSharedData(convert_data.component, entity_manager->GetSharedComponentInstance(convert_data.component, entity));
	}
	else {
		target_data = entity_manager->GetComponent(entity, convert_data.component);
	}

	return ConvertFromTargetToLinkComponent(
		&convert_data.base_data,
		target_data,
		link_data,
		previous_target_data,
		previous_link_data
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorTargetToLinkComponent(
	const EditorState* editor_state,
	Stream<char> link_component,
	const void* target_data,
	void* link_data,
	const void* previous_target_data,
	const void* previous_link_data,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	return ConvertFromTargetToLinkComponent(&convert_data.base_data, target_data, link_data, previous_target_data, previous_link_data);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorLinkComponentToTarget(
	EditorState* editor_state,
	EntityManager* entity_manager,
	Stream<char> link_component,
	Entity entity,
	const void* link_data,
	const void* previous_link_data,
	bool apply_modifier_function,
	const void* previous_target_data,
	AllocatorPolymorphic allocator
) {
	// Convert into a temporary component storage such that we don't get to write only partially to the component
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);

	void* target_data = nullptr;
	unsigned short component_byte_size = 0;

	// Deallocate the buffers for that entity component
	if (convert_data.component_type == ECS_COMPONENT_SHARED) {
		SharedInstance instance = entity_manager->GetSharedComponentInstance(convert_data.component, entity);
		target_data = entity_manager->GetSharedData(convert_data.component, instance);
		component_byte_size = entity_manager->SharedComponentSize(convert_data.component);
	}
	else if (convert_data.component_type == ECS_COMPONENT_UNIQUE) {
		target_data = entity_manager->GetComponent(entity, convert_data.component);
		component_byte_size = entity_manager->ComponentSize(convert_data.component);
	}
	else {
		ECS_ASSERT(false, "Requesting global component is illegal for link to target conversion for an entity");
	}

	previous_target_data = previous_target_data != nullptr ? previous_target_data : target_data;

	ECS_STACK_CAPACITY_STREAM(size_t, component_storage, ECS_KB * 4);
	bool conversion_success = ConvertLinkComponentToTarget(
		&convert_data.base_data,
		link_data,
		component_storage.buffer,
		previous_link_data,
		previous_target_data,
		apply_modifier_function
	);
	if (conversion_success) {
		// Deallocate any existing buffers
		if (convert_data.component_type == ECS_COMPONENT_SHARED) {
			SharedInstance instance = entity_manager->GetSharedComponentInstance(convert_data.component, entity);
			entity_manager->CallComponentSharedInstanceDeallocateCommit(convert_data.component, instance);
		}
		else {
			entity_manager->TryCallEntityComponentDeallocateCommit(entity, convert_data.component);
		}

		// We can copy directly into target data from the component storage
		memcpy(target_data, component_storage.buffer, component_byte_size);
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertEditorLinkComponentToTarget(
	EditorState* editor_state,
	Stream<char> link_component,
	void* target_data,
	const void* link_data,
	const void* previous_target_data,
	const void* previous_link_data,
	bool apply_modifier_function,
	AllocatorPolymorphic allocator
) {
	ConvertToOrFromLinkData convert_data = GetConvertToOrFromLinkData(editor_state, link_component, allocator);
	return ConvertLinkComponentToTarget(&convert_data.base_data, link_data, target_data, previous_link_data, previous_target_data, apply_modifier_function);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertSandboxTargetToLinkComponent(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<char> link_component,
	Entity entity,
	void* link_data,
	const void* previous_link_data,
	const void* previous_target_data,
	AllocatorPolymorphic allocator,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return ConvertEditorTargetToLinkComponent(
		editor_state, 
		GetSandboxEntityManager(editor_state, sandbox_index, viewport), 
		link_component, 
		entity, 
		link_data, 
		previous_link_data, 
		previous_target_data, 
		allocator
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool ConvertSandboxLinkComponentToTarget(
	EditorState* editor_state, 
	unsigned int sandbox_index,
	Stream<char> link_component, 
	Entity entity, 
	const void* link_data,
	const void* previous_link_data,
	bool apply_modifier_function,
	const void* previous_target_data,
	AllocatorPolymorphic allocator,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return ConvertEditorLinkComponentToTarget(
		editor_state,
		GetSandboxEntityManager(editor_state, sandbox_index, viewport),
		link_component,
		entity,
		link_data,
		previous_link_data,
		apply_modifier_function,
		previous_target_data,
		allocator
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

void DeleteSandboxUnreferencedSharedInstances(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component shared_component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetSandboxEntityManager(editor_state, sandbox_index, viewport)->UnregisterUnreferencedSharedInstancesCommit(shared_component);
}

// ------------------------------------------------------------------------------------------------------------------------------

struct DeleteSandboxEntityEventData {
	unsigned int sandbox_index;
	Entity entity;
	EDITOR_SANDBOX_VIEWPORT viewport;
};

static EDITOR_EVENT(DeleteSandboxEntityEvent) {
	DeleteSandboxEntityEventData* data = (DeleteSandboxEntityEventData*)_data;
	if (GetSandboxBackgroundComponentBuildFunctionCount(editor_state, data->sandbox_index) != 0) {
		return true;
	}

	// This function will check to see if the entity exists or not. If it doesn't, it won't do
	// anything
	DeleteSandboxEntity(editor_state, data->sandbox_index, data->entity, data->viewport);
	return false;
}

void DeleteSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	// Check to see if there are pending build entity components for this sandbox
	// If that is the case, we need to wait for them to finish since this can
	// Deletion can introduce race conditions with that function
	if (GetSandboxBackgroundComponentBuildFunctionCount(editor_state, sandbox_index) != 0) {
		// Push an event to perform this
		DeleteSandboxEntityEventData event_data;
		event_data.entity = entity;
		event_data.sandbox_index = sandbox_index;
		event_data.viewport = viewport;
		EditorAddEvent(editor_state, DeleteSandboxEntityEvent, &event_data, sizeof(event_data));
		return;
	}

	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	// Be safe. If for some reason the UI lags behind and the runtime might delete the entity before us
	if (entity_manager->ExistsEntity(entity)) {
		// We also need to remove the references to all the assets that this entity is using
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, entity_assets, 128);
		GetSandboxEntityAssets(editor_state, sandbox_index, entity, &entity_assets, viewport);

		// If we are in the runtime entity manager, we shouldn't remove the sandbox assets
		// Since they are handled by the snapshot
		if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
			UnregisterSandboxAsset(editor_state, sandbox_index, entity_assets);
		}

		// Retrieve the shared components such that we can remove the unreferenced shared instances
		ComponentSignature shared_signature = entity_manager->GetEntitySharedSignatureStable(entity).ComponentSignature();
		entity_manager->DeleteEntityCommit(entity);

		// Remove the unreferenced shared instances as well
		for (size_t index = 0; index < shared_signature.count; index++) {
			entity_manager->UnregisterUnreferencedSharedInstancesCommit(shared_signature[index]);
		}
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);

		// If this entity belongs to the selected group, remove it from there as well
		RemoveSandboxSelectedEntity(editor_state, sandbox_index, entity);
		ChangeInspectorEntitySelection(editor_state, sandbox_index);

		RenderSandboxViewports(editor_state, sandbox_index, { 0, 0 }, true);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

ComponentSignature SandboxEntityUniqueComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		return entity_manager->GetEntitySignatureStable(entity);
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

ComponentSignature SandboxEntitySharedComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		SharedComponentSignature signature = entity_manager->GetEntitySharedSignatureStable(entity);
		return { signature.indices, signature.count };
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedComponentSignature SandboxEntitySharedInstances(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		SharedComponentSignature signature = entity_manager->GetEntitySharedSignatureStable(entity);
		return signature;
	}
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedInstance SandboxEntitySharedInstance(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	SharedComponentSignature signature = SandboxEntitySharedInstances(editor_state, sandbox_index, entity, viewport);
	for (unsigned char index = 0; index < signature.count; index++) {
		if (signature.indices[index] == component) {
			return signature.instances[index];
		}
	}
	return { -1 };
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedInstance FindOrCreateSandboxSharedComponentInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	const void* data,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	SharedInstance instance = entity_manager->GetSharedComponentInstance(component, data);
	if (instance.value == -1) {
		instance = entity_manager->RegisterSharedInstanceCommit(component, data);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
	return instance;
}

// ------------------------------------------------------------------------------------------------------------------------------

void FilterSandboxEntitiesValid(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity>* entities, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	for (size_t index = 0; index < entities->size; index++) {
		if (!IsSandboxEntityValid(editor_state, sandbox_index, entities->buffer[index], viewport)) {
			entities->RemoveSwapBack(index);
			index--;
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

Entity GetSandboxEntity(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<char> name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	Entity entity = StringToEntity(name);
	// Check to see that the entity exists in that sandbox
	if (!entity_manager->ExistsEntity(entity)) {
		// Invalidate the entity
		entity = -1;
	}
	return entity;
}

// ------------------------------------------------------------------------------------------------------------------------------

SharedInstance GetSandboxSharedComponentDefaultInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	size_t component_storage[ECS_KB * 8];
	editor_state->editor_components.ResetComponentBasic(component, component_storage, ECS_COMPONENT_SHARED);
	return FindOrCreateSandboxSharedComponentInstance(editor_state, sandbox_index, component, component_storage, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return (void*)GetSandboxEntityComponent((const EditorState*)editor_state, sandbox_index, entity, component, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxEntityComponent(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity) && entity_manager->ExistsComponent(component)) {
		if (entity_manager->HasComponent(entity, component)) {
			return entity_manager->GetComponent(entity, component);
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxSharedInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return (void*)GetSandboxSharedInstance((const EditorState*)editor_state, sandbox_index, component, instance, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxSharedInstance(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	// Already checks to see if the component exists
	if (entity_manager->ExistsSharedInstance(component, instance)) {
		return entity_manager->GetSharedData(component, instance);
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	bool shared,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	// The cast should be fine here
	return (void*)GetSandboxEntityComponentEx((const EditorState*)editor_state, sandbox_index, entity, component, shared, viewport);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxEntityComponentEx(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	bool shared,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		if (shared) {
			if (entity_manager->ExistsSharedComponent(component)) {
				if (entity_manager->HasSharedComponent(entity, component)) {
					return entity_manager->GetSharedData(component, entity_manager->GetSharedComponentInstance(component, entity));
				}
			}
		}
		else {
			if (entity_manager->ExistsComponent(component)) {
				if (entity_manager->HasComponent(entity, component)) {
					return entity_manager->GetComponent(entity, component);
				}
			}
		}
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

void* GetSandboxGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	return entity_manager->TryGetGlobalComponent(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

const void* GetSandboxGlobalComponent(const EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	return entity_manager->TryGetGlobalComponent(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

float3 GetSandboxEntityTranslation(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, entity, viewport);
	if (translation != nullptr) {
		return translation->value;
	}
	return float3::Splat(0.0f);
}

// ------------------------------------------------------------------------------------------------------------------------------

QuaternionScalar GetSandboxEntityRotation(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, entity, viewport);
	if (rotation != nullptr) {
		return rotation->value;
	}
	return QuaternionIdentityScalar();
}

// ------------------------------------------------------------------------------------------------------------------------------

float3 GetSandboxEntityScale(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const Scale* scale = GetSandboxEntityComponent<Scale>(editor_state, sandbox_index, entity, viewport);
	if (scale != nullptr) {
		return scale->value;
	}
	return float3::Splat(1.0f);
}

// ------------------------------------------------------------------------------------------------------------------------------

TransformScalar GetSandboxEntityTransform(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	float3 translation = GetSandboxEntityTranslation(editor_state, sandbox_index, entity, viewport);
	QuaternionScalar rotation = GetSandboxEntityRotation(editor_state, sandbox_index, entity, viewport);
	float3 scale = GetSandboxEntityScale(editor_state, sandbox_index, entity, viewport);
	return { translation, rotation, scale };
}

// ------------------------------------------------------------------------------------------------------------------------------

AllocatorPolymorphic GetSandboxComponentAllocator(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return GetSandboxEntityManager(editor_state, sandbox_index, viewport)->GetComponentAllocator(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

AllocatorPolymorphic GetSandboxSharedComponentAllocator(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return GetSandboxEntityManager(editor_state, sandbox_index)->GetSharedComponentAllocator(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

AllocatorPolymorphic GetSandboxGlobalComponentAllocator(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	return GetSandboxEntityManager(editor_state, sandbox_index)->GetGlobalComponentAllocator(component);
}

// ------------------------------------------------------------------------------------------------------------------------------

AllocatorPolymorphic GetSandboxComponentAllocatorEx(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Component component,
	ECS_COMPONENT_TYPE type,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	switch (type) {
	case ECS_COMPONENT_UNIQUE:
		return GetSandboxComponentAllocator(editor_state, sandbox_index, component, viewport);
	case ECS_COMPONENT_SHARED:
		return GetSandboxSharedComponentAllocator(editor_state, sandbox_index, component, viewport);
	case ECS_COMPONENT_GLOBAL:
		return GetSandboxGlobalComponentAllocator(editor_state, sandbox_index, component, viewport);
	}

	return nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

Stream<char> GetSandboxEntityName(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	CapacityStream<char> storage,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const void* name_component = GetSandboxEntityComponent(editor_state, sandbox_index, entity, editor_state->editor_components.GetComponentID(STRING(Name)), viewport);
	if (name_component != nullptr) {
		Name* name = (Name*)name_component;
		return name->name;
	}
	else {
		entity.ToString(storage);
		return storage;
	}
}

void GetSandboxEntityComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetEditorEntityComponentAssets(editor_state, GetSandboxEntityManager(editor_state, sandbox_index, viewport), entity, component, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntitySharedComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetEditorEntitySharedComponentAssets(editor_state, GetSandboxEntityManager(editor_state, sandbox_index, viewport), entity, component, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxSharedInstanceComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance shared_instance, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const void* instance_data = GetSandboxSharedInstance(editor_state, sandbox_index, component, shared_instance, viewport);
	GetEditorComponentAssets(editor_state, instance_data, component, ECS_COMPONENT_SHARED, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxGlobalComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const void* component_data = GetSandboxGlobalComponent(editor_state, sandbox_index, component, viewport);
	GetEditorComponentAssets(editor_state, component_data, component, ECS_COMPONENT_GLOBAL, handles);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntityAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	ComponentSignature unique_signature = SandboxEntityUniqueComponents(editor_state, sandbox_index, entity, viewport);
	ComponentSignature shared_signature = SandboxEntitySharedComponents(editor_state, sandbox_index, entity, viewport);

	for (unsigned char index = 0; index < unique_signature.count; index++) {
		GetSandboxEntityComponentAssets(editor_state, sandbox_index, entity, unique_signature[index], handles, viewport);
	}

	for (unsigned char index = 0; index < shared_signature.count; index++) {
		GetSandboxEntitySharedComponentAssets(editor_state, sandbox_index, entity, shared_signature[index], handles, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetEditorComponentAssets(
	const EditorState* editor_state,
	const void* component_data,
	Component component,
	ECS_COMPONENT_TYPE component_type,
	CapacityStream<AssetTypedHandle>* handles
) {
	if (component_data != nullptr) {
		const Reflection::ReflectionType* component_reflection_type = editor_state->editor_components.GetType(component, component_type);

		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 128);
		ECS_STACK_CAPACITY_STREAM(unsigned int, int_handles, 128);

		GetAssetFieldsFromLinkComponentTarget(component_reflection_type, asset_fields);
		GetLinkComponentTargetHandles(component_reflection_type, editor_state->asset_database, component_data, asset_fields, int_handles.buffer);

		for (unsigned int index = 0; index < asset_fields.size; index++) {
			handles->AddAssert({ int_handles[index], asset_fields[index].type.type });
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

static void GetEntityComponentAssetsImplementation(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles,
	ECS_COMPONENT_TYPE component_type
) {
	if (component_type == ECS_COMPONENT_GLOBAL) {
		const void* component_data = entity_manager->TryGetGlobalComponent(component);
		GetEditorComponentAssets(editor_state, component_data, component, component_type, handles);
	}
	else {
		if (entity_manager->ExistsEntity(entity)) {
			const void* component_data = nullptr;
			if (component_type == ECS_COMPONENT_SHARED) {
				if (entity_manager->ExistsSharedComponent(component)) {
					if (entity_manager->HasSharedComponent(entity, component)) {
						component_data = entity_manager->GetSharedData(component, entity_manager->GetSharedComponentInstance(component, entity));
					}
				}
			}
			else {
				if (entity_manager->ExistsComponent(component)) {
					if (entity_manager->HasComponent(entity, component)) {
						component_data = entity_manager->GetComponent(entity, component);
					}
				}
			}
			GetEditorComponentAssets(editor_state, component_data, component, component_type, handles);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEditorEntityComponentAssets(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles
)
{
	GetEntityComponentAssetsImplementation(editor_state, entity_manager, entity, component, handles, ECS_COMPONENT_UNIQUE);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEditorEntitySharedComponentAssets(
	const EditorState* editor_state,
	const EntityManager* entity_manager,
	Entity entity,
	Component component,
	CapacityStream<AssetTypedHandle>* handles
)
{
	GetEntityComponentAssetsImplementation(editor_state, entity_manager, entity, component, handles, ECS_COMPONENT_SHARED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetEditorEntityAssets(const EditorState* editor_state, const EntityManager* entity_manager, Entity entity, CapacityStream<AssetTypedHandle>* handles)
{
	ComponentSignature unique_signature = entity_manager->GetEntitySignatureStable(entity);
	SharedComponentSignature shared_signature = entity_manager->GetEntitySharedSignatureStable(entity);

	for (unsigned char index = 0; index < unique_signature.count; index++) {
		GetEditorEntityComponentAssets(editor_state, entity_manager, entity, unique_signature[index], handles);
	}

	for (unsigned char index = 0; index < shared_signature.count; index++) {
		GetEditorEntitySharedComponentAssets(editor_state, entity_manager, entity, shared_signature.indices[index], handles);
	}
}


// ------------------------------------------------------------------------------------------------------------------------------

template<bool has_translation, bool has_rotation>
static void GetSandboxEntitiesMidpointImpl(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	Stream<Entity> entities,
	float3* translation_midpoint,
	QuaternionScalar* rotation_midpoint,
	Stream<TransformGizmo> transform_gizmos,
	bool add_transform_gizmos_to_total_count,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	float3 translation_average = float3::Splat(0.0f);
	QuaternionScalar rotation_average = QuaternionAverageCumulatorInitializeScalar();

	for (size_t index = 0; index < entities.size; index++) {
		if constexpr (has_translation) {
			const Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, entities[index], viewport);
			if (translation != nullptr) {
				translation_average += translation->value;
			}
		}
		if constexpr (has_rotation) {
			const Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, entities[index], viewport);
			if (rotation != nullptr) {
				QuaternionAddToAverageStep(&rotation_average, rotation->value);
			}
		}
	}

	for (size_t index = 0; index < transform_gizmos.size; index++) {
		if constexpr (has_translation) {
			translation_average += transform_gizmos[index].position;
		}
		if constexpr (has_rotation) {
			QuaternionAddToAverageStep(&rotation_average, transform_gizmos[index].rotation);
		}
	}

	size_t total_count = entities.size;
	if (add_transform_gizmos_to_total_count) {
		total_count += transform_gizmos.size;
	}

	if constexpr (has_translation) {
		*translation_midpoint = translation_average * float3::Splat(1.0f / (float)total_count);
	}
	if constexpr (has_rotation) {
		*rotation_midpoint = QuaternionAverageFromCumulator(rotation_average, total_count);
	}
}

float3 GetSandboxEntitiesTranslationMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	float3 midpoint;
	GetSandboxEntitiesMidpointImpl<true, false>(editor_state, sandbox_index, entities, &midpoint, nullptr, {}, false, viewport);
	return midpoint;
}

// ------------------------------------------------------------------------------------------------------------------------------

QuaternionScalar GetSandboxEntitiesRotationMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	QuaternionScalar midpoint;
	GetSandboxEntitiesMidpointImpl<false, true>(editor_state, sandbox_index, entities, nullptr, &midpoint, {}, false, viewport);
	return midpoint;
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntitiesMidpointWithGizmos(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	float3* translation_midpoint, 
	QuaternionScalar* rotation_midpoint, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	// Determine the additional gizmo widgets
	ECS_STACK_CAPACITY_STREAM(TransformGizmo, transform_gizmos, ECS_KB);
	GetSandboxSelectedVirtualEntitiesTransformGizmos(editor_state, sandbox_index, &transform_gizmos);

	GetSandboxEntitiesMidpoint(
		editor_state,
		sandbox_index,
		entities,
		translation_midpoint,
		rotation_midpoint,
		transform_gizmos,
		false,
		viewport
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxEntitiesMidpoint(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> entities, 
	float3* translation_midpoint, 
	QuaternionScalar* rotation_midpoint,
	Stream<TransformGizmo> transform_gizmos,
	bool add_transform_gizmos_to_total_count,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	GetSandboxEntitiesMidpointImpl<true, true>(
		editor_state, 
		sandbox_index, 
		entities, 
		translation_midpoint, 
		rotation_midpoint, 
		transform_gizmos, 
		add_transform_gizmos_to_total_count, 
		viewport
	);
}

// ------------------------------------------------------------------------------------------------------------------------------

void GetSandboxActivePrefabIDs(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	AdditionStream<unsigned int> prefab_ids, 
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	entity_manager->ForEachEntityComponent(PrefabComponent::ID(), [&](Entity entity, const void* component) {
		const PrefabComponent* prefab = (const PrefabComponent*)component;
		Stream<unsigned int> existing_ids = prefab_ids.ToStream();
		size_t existing_index = SearchBytes(existing_ids, prefab->id);
		if (existing_index == -1) {
			prefab_ids.Add(prefab->id);
		}
	});
}

// ------------------------------------------------------------------------------------------------------------------------------

bool IsSandboxEntitySelected(const EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	return FindSandboxSelectedEntityIndex(editor_state, sandbox_index, entity) != -1;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool IsSandboxEntityValid(const EditorState* editor_state, unsigned int sandbox_index, Entity entity, EDITOR_SANDBOX_VIEWPORT viewport)
{
	const EntityManager* active_entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	return active_entity_manager->ExistsEntity(entity);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool NeedsApplyModifierLinkComponent(const EditorState* editor_state, Stream<char> link_name)
{
	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, link_name);
	return link_target.build_function != nullptr && link_target.apply_modifier != nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool NeedsApplyModifierButtonLinkComponent(const EditorState* editor_state, Stream<char> link_name)
{
	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, link_name);
	return link_target.build_function != nullptr && link_target.apply_modifier != nullptr && link_target.apply_modifier_needs_button;
}

// ------------------------------------------------------------------------------------------------------------------------------

void NotifySandboxEntityComponentChange(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Component component, bool is_shared)
{
	NotifySandboxEntityComponentChange(
		editor_state, 
		sandbox_index, 
		entity, 
		editor_state->editor_components.ComponentFromID(component, is_shared ? ECS_COMPONENT_SHARED : ECS_COMPONENT_UNIQUE)
	);
}

void NotifySandboxEntityComponentChange(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name)
{
	// Determine if this component has any dependencies
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
	Stream<Stream<char>> dependent_components = RetrieveModuleComponentBuildDependentEntries(editor_state, component_name, &stack_allocator);
	for (size_t index = 0; index < dependent_components.size; index++) {
		EditorModuleComponentBuildEntry build_entry = GetModuleComponentBuildEntry(editor_state, dependent_components[index]);
		Component component = editor_state->editor_components.GetComponentID(dependent_components[index]);
		bool is_shared = editor_state->editor_components.IsSharedComponent(dependent_components[index]);
		void* component_data = GetSandboxEntityComponentEx(editor_state, sandbox_index, entity, component, is_shared);
		// Only perform the call if the entity actually has the data
		if (component_data != nullptr) {
			if (is_shared) {
				CallModuleComponentBuildFunctionShared(
					editor_state,
					sandbox_index,
					&build_entry,
					component,
					SandboxEntitySharedInstance(editor_state, sandbox_index, entity, component),
					entity
				);
			}
			else {
				CallModuleComponentBuildFunctionUnique(editor_state, sandbox_index, &build_entry, { &entity, 1 }, component);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ParentSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity child, 
	Entity parent,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	bool child_exists = entity_manager->ExistsEntity(child);
	if (child_exists) {
		if (parent.value != -1) {
			bool exists_parent = entity_manager->ExistsEntity(parent);
			if (!exists_parent) {
				return;
			}
		}

		EntityPair pair;
		pair.parent = parent;
		pair.child = child;
		entity_manager->ChangeEntityParentCommit({ &pair, 1 });
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ParentSandboxEntities(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> children, 
	Entity parent,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	for (size_t index = 0; index < children.size; index++) {
		ParentSandboxEntity(editor_state, sandbox_index, children[index], parent, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		Component component = editor_state->editor_components.GetComponentID(component_name);
		if (entity_manager->HasComponent(entity, component)) {
			const void* data = GetSandboxEntityComponent(editor_state, sandbox_index, entity, component, viewport);
			// If we are in runtime, we don't have to unload the assets - they are automatically handled
			if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
				RemoveSandboxComponentAssets(editor_state, sandbox_index, component, data, ECS_COMPONENT_UNIQUE);
			}

			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);
			entity_manager->RemoveComponentCommit(entity, { &component, 1 });
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);
			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntitySharedComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		Component component = editor_state->editor_components.GetComponentID(component_name);
		if (entity_manager->HasSharedComponent(entity, component)) {
			EntityInfo previous_info = entity_manager->GetEntityInfo(entity);
			SharedInstance previous_instance = SandboxEntitySharedInstance(editor_state, sandbox_index, entity, component, viewport);
			entity_manager->RemoveSharedComponentCommit(entity, { &component, 1 });
			entity_manager->DestroyArchetypeBaseEmptyCommit(previous_info.main_archetype, previous_info.base_archetype, true);

			bool is_unreferenced = entity_manager->IsUnreferencedSharedInstance(component, previous_instance);
			// We need to remove the sandbox references only in scene mode
			if (is_unreferenced && entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
				const void* instance_data = entity_manager->GetSharedData(component, previous_instance);
				RemoveSandboxComponentAssets(editor_state, sandbox_index, component, instance_data, ECS_COMPONENT_SHARED);
				entity_manager->UnregisterSharedInstanceCommit(component, previous_instance);
			}

			SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
) {
	if (editor_state->editor_components.IsUniqueComponent(component_name)) {
		RemoveSandboxEntityComponent(editor_state, sandbox_index, entity, component_name, viewport);
	}
	else {
		RemoveSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	
	// The runtime entity manager might not have the component
	if (entity_manager->ExistsGlobalComponent(component)) {
		// We only need to remove the sandbox references in scene mode
		if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
			const void* component_data = entity_manager->GetGlobalComponent(component);
			RemoveSandboxComponentAssets(editor_state, sandbox_index, component, component_data, ECS_COMPONENT_GLOBAL);
		}

		entity_manager->UnregisterGlobalComponentCommit(component);
		SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxEntityFromHierarchy(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	if (entity_manager->ExistsEntity(entity)) {
		entity_manager->RemoveEntityFromHierarchyCommit({ &entity, 1 });
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void RemoveSandboxComponentAssets(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	const void* data, 
	ECS_COMPONENT_TYPE component_type,
	Stream<LinkComponentAssetField> asset_fields
) {
	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, unregister_elements, 512);

	if (asset_fields.size == 0) {
		GetEditorComponentAssets(editor_state, data, component, component_type, &unregister_elements);
	}
	else {
		const Reflection::ReflectionType* component_reflection_type = editor_state->editor_components.GetType(component, component_type);
		ECS_STACK_CAPACITY_STREAM(unsigned int, handles, 512);
		GetLinkComponentTargetHandles(component_reflection_type, editor_state->asset_database, data, asset_fields, handles.buffer);

		for (unsigned int index = 0; index < asset_fields.size; index++) {
			unregister_elements[index].handle = handles[index];
			unregister_elements[index].type = asset_fields[index].type.type;
		}
		unregister_elements.size = asset_fields.size;
	}

	// Unregister these assets
	UnregisterSandboxAsset(editor_state, sandbox_index, unregister_elements);
}

// ------------------------------------------------------------------------------------------------------------------------------

bool RemoveSandboxSelectedEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity)
{
	unsigned int selected_index = FindSandboxSelectedEntityIndex(editor_state, sandbox_index, entity);
	if (selected_index != -1) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		sandbox->selected_entities.RemoveSwapBack(selected_index);
		sandbox->flags = SetFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
		SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------------------------------------

void ResetSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(component_name);
	bool is_shared = editor_state->editor_components.IsSharedComponent(target.size > 0 ? target : component_name);
	Stream<char> initial_component = component_name;
	if (target.size > 0) {
		component_name = target;
	}

	if (!is_shared) {
		// We also need to remove the sandbox asset references
		if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
			Component component = editor_state->editor_components.GetComponentID(component_name);
			const void* component_data = GetSandboxEntityComponent(editor_state, sandbox_index, entity, component, viewport);
			RemoveSandboxComponentAssets(editor_state, sandbox_index, component, component_data, ECS_COMPONENT_UNIQUE);
		}

		// It has built in checks for not crashing
		editor_state->editor_components.ResetComponent(editor_state, sandbox_index, component_name, entity, ECS_COMPONENT_UNIQUE);
	}
	else {
		// Remove it and then add it again with the default component
		// This takes care of the asset handles being removed
		RemoveSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, viewport);
		AddSandboxEntitySharedComponent(editor_state, sandbox_index, entity, component_name, { -1 }, viewport);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void ResetSandboxGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, EDITOR_SANDBOX_VIEWPORT viewport)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	void* component_data = entity_manager->GetGlobalComponent(component);
	if (entity_manager != RuntimeSandboxEntityManager(editor_state, sandbox_index)) {
		RemoveSandboxComponentAssets(editor_state, sandbox_index, component, component_data, ECS_COMPONENT_GLOBAL);
	}
	editor_state->editor_components.ResetGlobalComponent(component, component_data);
}

// -----------------------------------------------------------------------------------------------------------------------------

void RotateSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, QuaternionScalar rotation_delta)
{
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM(TransformGizmoPointers, transform_gizmo_pointers, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(Entity, transform_gizmo_entities, ECS_KB);
	GetSandboxSelectedVirtualEntitiesTransformPointers(editor_state, sandbox_index, &transform_gizmo_pointers, &transform_gizmo_entities);

	auto apply_delta = [rotation_delta](QuaternionScalar* rotation_storage) {
		// We need to use local rotation regardless of the transform space
		// The transform tool takes care of the correct rotation delta
		QuaternionScalar new_quaternion = RotateQuaternion(*rotation_storage, rotation_delta);
		*rotation_storage = new_quaternion;
	};

	for (size_t index = 0; index < selected_entities.size; index++) {
		Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, selected_entities[index]);
		if (rotation != nullptr) {
			apply_delta((QuaternionScalar*)&rotation->value);
		}
		else {
			// Check the virtual entities
			size_t gizmo_index = SearchBytes(transform_gizmo_entities.ToStream(), selected_entities[index]);
			if (gizmo_index != -1) {
				if (transform_gizmo_pointers[gizmo_index].euler_rotation != nullptr) {
					if (transform_gizmo_pointers[gizmo_index].is_euler_rotation) {
						QuaternionScalar rotation_storage = QuaternionFromEuler(*transform_gizmo_pointers[gizmo_index].euler_rotation);
						apply_delta(&rotation_storage);
						*transform_gizmo_pointers[gizmo_index].euler_rotation = QuaternionToEuler(rotation_storage);
					}
					else {
						apply_delta(transform_gizmo_pointers[gizmo_index].quaternion_rotation);
					}
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------

void SandboxForEachEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ForEachEntityUntypedFunctor functor, 
	void* functor_data, 
	const ArchetypeQueryDescriptor& query_descriptor,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	World temp_world;
	memcpy(&temp_world, &GetSandbox(editor_state, sandbox_index)->sandbox_world, sizeof(temp_world));

	temp_world.entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	ForEachEntityCommitFunctor(&temp_world, functor, functor_data, query_descriptor);
}

// ------------------------------------------------------------------------------------------------------------------------------

struct SplatLinkComponentBasicData {
	const void* link_component;
	const Reflection::ReflectionType* target_type;
	Stream<LinkComponentAssetField> asset_fields;
	Stream<Stream<void>> assets;
};

void SplatLinkComponentBasic(ForEachEntityUntypedFunctorData* functor_data) {
	SplatLinkComponentBasicData* data = (SplatLinkComponentBasicData*)functor_data->base.user_data;
	for (size_t index = 0; index < data->asset_fields.size; index++) {
		SetAssetTargetFieldFromReflection(
			data->target_type, 
			data->asset_fields[index].field_index, 
			functor_data->unique_components[0], 
			data->assets[index], 
			data->asset_fields[index].type.type
		);
	}
}

struct SplatLinkComponentBuildData {
	const void* link_component;
	Stream<Stream<void>> assets;
	ModuleLinkComponentFunction link_function;
};

void SplatLinkComponentBuild(ForEachEntityUntypedFunctorData* functor_data) {
	SplatLinkComponentBuildData* data = (SplatLinkComponentBuildData*)functor_data->base.user_data;

	ModuleLinkComponentFunctionData function_data;
	function_data.assets = data->assets;
	function_data.component = functor_data->unique_components[0];
	function_data.link_component = data->link_component;
	data->link_function(&function_data);
}

// ------------------------------------------------------------------------------------------------------------------------------

//bool SandboxSplatLinkComponentAssetFields(
//	EditorState* editor_state, 
//	unsigned int sandbox_index, 
//	const void* link_component, 
//	Stream<char> component_name, 
//	bool give_error_when_failing,
//	EDITOR_SANDBOX_VIEWPORT viewport
//)
//{
//	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
//	if (type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component doesn't exist.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
//
//	ECS_STACK_CAPACITY_STREAM(Stream<void>, assets, 512);
//	ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
//	GetAssetFieldsFromLinkComponent(type, asset_fields);
//	if (asset_fields.size == 0) {
//		return true;
//	}
//
//	GetLinkComponentAssetData(type, link_component, editor_state->asset_database, asset_fields, &assets);
//
//	if (GetReflectionTypeLinkComponentNeedsDLL(type)) {
//		unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, component_name);
//		if (module_index == -1) {
//			if (give_error_when_failing) {
//				ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component was not found in the loaded modules.", 
//					component_name);
//				EditorSetConsoleError(error_message);
//			}
//			return false;
//		}
//		ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, module_index, component_name);
//		if (link_target.build_function == nullptr) {
//			if (give_error_when_failing) {
//				ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The component requires a DLL function but none was found.", 
//					component_name);
//				EditorSetConsoleError(error_message);
//			}
//			return false;
//		}
//
//		SplatLinkComponentBuildData splat_data;
//		splat_data.link_component = link_component;
//		splat_data.assets = assets;
//		splat_data.link_function = link_target.build_function;
//		SandboxForEachEntity(editor_state, sandbox_index, SplatLinkComponentBuild, &splat_data, { &component, 1 }, { nullptr, 0 }, {}, {}, viewport);
//		return true;
//	}
//
//	SplatLinkComponentBasicData splat_data;
//	splat_data.link_component = link_component;
//	splat_data.asset_fields = asset_fields;
//	splat_data.assets = assets;
//	
//	splat_data.target_type = editor_state->editor_components.GetType(editor_state->editor_components.GetComponentFromLink(component_name));
//	if (splat_data.target_type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The target component was not found.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	if (!ValidateLinkComponent(type, splat_data.target_type)) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to splat link component {#} asset fields. The link component {#} cannot be resolved with "
//				"default conversion to {#}.", component_name, splat_data.target_type->name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//	SandboxForEachEntity(editor_state, sandbox_index, SplatLinkComponentBasic, &splat_data, { &component, 1 }, { nullptr, 0 }, {}, {}, viewport);
//	return true;
//}

// ------------------------------------------------------------------------------------------------------------------------------

// The functor receives the component as parameter
//template<typename GetData>
//bool UpdateLinkComponentBaseAssetsOnly(
//	EditorState* editor_state,
//	unsigned int sandbox_index,
//	const void* link_component,
//	Stream<char> component_name,
//	const void* previous_link_component,
//	const void* previous_target_component,
//	bool give_error_when_failing,
//	GetData&& get_data
//) {
//	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
//	if (type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The component doesn't exist.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	Stream<char> target = editor_state->editor_components.GetComponentFromLink(component_name);
//	if (target.size == 0) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The component does not have a target.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//	const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target);
//	if (target_type == nullptr) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The target {#} does not exist.", component_name, target);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	unsigned int module_index = editor_state->editor_components.FindComponentModuleInReflection(editor_state, component_name);
//	if (module_index == -1) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The module from which it came could not be identified.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//		return false;
//	}
//
//	ModuleLinkComponentTarget link_target = GetModuleLinkComponentTarget(editor_state, module_index, component_name);
//	Component component = { (short)target_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
//
//	void* converted_data = get_data(component);
//	if (converted_data == nullptr) {
//		return false;
//	}
//
//	ConvertToAndFromLinkBaseData convert_data;
//	convert_data.reflection_manager = editor_state->GlobalReflectionManager();
//	convert_data.link_type = type;
//	convert_data.target_type = target_type;
//	convert_data.asset_database = editor_state->asset_database;
//	convert_data.module_link = link_target;
//	bool success = ConvertLinkComponentToTargetAssetsOnly(&convert_data, link_component, converted_data, previous_link_data, previous_target_component);
//	if (!success) {
//		if (give_error_when_failing) {
//			ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The dll function is missing or the default link is incompatible.", component_name);
//			EditorSetConsoleError(error_message);
//		}
//	}
//	return success;
//}
//
//bool SandboxUpdateLinkComponentSharedInstance(
//	EditorState* editor_state,
//	unsigned int sandbox_index,
//	const void* link_component, 
//	Stream<char> component_name,
//	SharedInstance instance,
//	bool give_error_when_failing,
//	EDITOR_SANDBOX_VIEWPORT viewport
//)
//{
//	return UpdateLinkComponentBaseAssetsOnly(editor_state, sandbox_index, link_component, component_name, give_error_when_failing, [=](Component component) {
//		void* shared_data = GetSandboxSharedInstance(editor_state, sandbox_index, component, instance, viewport);
//		if (shared_data == nullptr) {
//			if (give_error_when_failing) {
//				ECS_FORMAT_TEMP_STRING(error_message, "Failed to update link component {#} asset fields. The given shared instance {#} is invalid for {#}.", 
//					component_name, instance.value, ViewportString(viewport));
//				EditorSetConsoleError(error_message);
//			}
//		}
//		return shared_data;
//	});
//}

// ------------------------------------------------------------------------------------------------------------------------------

bool SandboxUpdateUniqueLinkComponentForEntity(
	EditorState* editor_state,
	unsigned int sandbox_index,
	const void* link_component,
	Stream<char> link_name,
	Entity entity,
	const void* previous_link_component,
	SandboxUpdateLinkComponentForEntityInfo info
) {
	Component component = editor_state->editor_components.GetComponentIDWithLink(link_name);
	AllocatorPolymorphic component_allocator = GetSandboxComponentAllocator(editor_state, sandbox_index, component, info.viewport);
	bool success = ConvertSandboxLinkComponentToTarget(
		editor_state, 
		sandbox_index, 
		link_name, 
		entity, 
		link_component, 
		previous_link_component, 
		info.apply_modifier_function,
		info.target_previous_data,
		component_allocator, 
		info.viewport
	);
	if (!success) {
		if (info.give_error_when_failing) {
			ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 512);
			Stream<char> entity_name = GetSandboxEntityName(editor_state, sandbox_index, entity, entity_name_storage);

			ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert link component {#} to target for entity {#} in sandbox {#} for {#}.",
				link_name, entity_name, sandbox_index, ViewportString(info.viewport));
			EditorSetConsoleError(error_message);
		}
	}
	return success;
}

// ------------------------------------------------------------------------------------------------------------------------------

bool SandboxUpdateSharedLinkComponentForEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	const void* link_component, 
	Stream<char> link_name, 
	Entity entity,
	const void* previous_link_component,
	SandboxUpdateLinkComponentForEntityInfo info
)
{
	// Get the component data first to help the conversion
	Component target_component = editor_state->editor_components.GetComponentIDWithLink(link_name);
	SharedInstance current_instance = SandboxEntitySharedInstance(editor_state, sandbox_index, entity, target_component, info.viewport);

	const void* previous_shared_data = nullptr;
	if (info.target_previous_data != nullptr) {
		previous_shared_data = info.target_previous_data;
	}
	else {
		previous_shared_data = GetSandboxSharedInstance(editor_state, sandbox_index, target_component, current_instance, info.viewport);
	}

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
	size_t converted_link_storage[ECS_KB * 4];
	bool success = ConvertEditorLinkComponentToTarget(
		editor_state, 
		link_name, 
		converted_link_storage, 
		link_component, 
		previous_shared_data,
		previous_link_component,
		info.apply_modifier_function,
		&stack_allocator
	);
	if (!success) {
		if (info.give_error_when_failing) {
			ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 256);
			Stream<char> entity_name = GetSandboxEntityName(editor_state, sandbox_index, entity, entity_name_storage);
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert link component {#} to target for entity {#} in sandbox {#} for {#}.", 
				link_name, entity_name, sandbox_index, ViewportString(info.viewport));
			EditorSetConsoleError(error_message);
		}
		return false;
	}

	// Find or create the shared instance that matches this data
	SharedInstance new_shared_instance = FindOrCreateSandboxSharedComponentInstance(editor_state, sandbox_index, target_component, converted_link_storage, info.viewport);

	if (new_shared_instance != current_instance) {
		EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, info.viewport);
		SharedInstance old_instance = entity_manager->ChangeEntitySharedInstanceCommit(entity, target_component, new_shared_instance, false, true);

		// Unregister the shared instance if it longer is referenced
		entity_manager->UnregisterUnreferencedSharedInstanceCommit(target_component, old_instance);
	}

	// It can happen that the handle is not modified and so it will, in fact, have the same shared instance
	// Only change the shared instance in that case
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SandboxApplyEntityChanges(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	Stream<Entity> entities_to_be_updated, 
	ComponentSignature unique_signature, 
	const void** unique_components, 
	ComponentSignature shared_signature, 
	const void** shared_components, 
	Stream<EntityChange> changes
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	ApplyEntityChanges(
		editor_state->GlobalReflectionManager(),
		entity_manager,
		entities_to_be_updated,
		unique_components,
		unique_signature,
		shared_components,
		shared_signature,
		changes
	);
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetSandboxEntitySharedInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	SharedInstance instance,
	bool delete_previous_instance_if_unreferenced,
	EDITOR_SANDBOX_VIEWPORT viewport
)
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	if (entity_manager->ExistsEntity(entity)) {
		if (entity_manager->ExistsSharedInstance(component, instance)) {
			// It might be the same instance, in the case where a build function
			// Is run when a new shared component is added to an entity
			SharedInstance previous_instance = entity_manager->ChangeEntitySharedInstanceCommit(entity, component, instance, true, true);
			if (delete_previous_instance_if_unreferenced && previous_instance != instance) {
				entity_manager->UnregisterUnreferencedSharedInstanceCommit(component, previous_instance);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void ScaleSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, float3 scale_delta)
{
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM(TransformGizmoPointers, transform_gizmo_pointers, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(Entity, transform_gizmo_entities, ECS_KB);
	GetSandboxSelectedVirtualEntitiesTransformPointers(editor_state, sandbox_index, &transform_gizmo_pointers, &transform_gizmo_entities);

	for (size_t index = 0; index < selected_entities.size; index++) {
		Scale* scale = GetSandboxEntityComponent<Scale>(editor_state, sandbox_index, selected_entities[index]);
		if (scale != nullptr) {
			// It is a real entity
			scale->value += scale_delta;
		}
		else {
			// Check the virtual entity gizmos
			size_t gizmo_index = SearchBytes(transform_gizmo_entities.ToStream(), selected_entities[index]);
			if (gizmo_index != -1) {
				if (transform_gizmo_pointers[gizmo_index].scale != nullptr) {
					*transform_gizmo_pointers[gizmo_index].scale += scale_delta;
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void TranslateSandboxSelectedEntities(EditorState* editor_state, unsigned int sandbox_index, float3 delta)
{
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	ECS_STACK_CAPACITY_STREAM(TransformGizmoPointers, transform_gizmo_pointers, ECS_KB);
	ECS_STACK_CAPACITY_STREAM(Entity, transform_gizmo_entities, ECS_KB);
	GetSandboxSelectedVirtualEntitiesTransformPointers(editor_state, sandbox_index, &transform_gizmo_pointers, &transform_gizmo_entities);

	for (size_t index = 0; index < selected_entities.size; index++) {
		Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, selected_entities[index]);
		if (translation != nullptr) {
			// It is a real entity
			translation->value += delta;
		}
		else {
			// Check the virtual entity gizmos
			size_t gizmo_index = SearchBytes(transform_gizmo_entities.ToStream(), selected_entities[index]);
			if (gizmo_index != -1) {
				if (transform_gizmo_pointers[gizmo_index].position != nullptr) {
					*transform_gizmo_pointers[gizmo_index].position += delta;
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------------
