#include "editorpch.h"
#include "EntitiesUI.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/Sandbox.h"
#include "../Project/ProjectFolders.h"
#include "Inspector.h"
#include "DragTargets.h"

#include "ECSEngineComponents.h"
#include "CreateScene.h"
#include "../Assets/PrefabFile.h"

using namespace ECSEngine;
ECS_TOOLS;

struct EntitiesUIData {
	ECS_INLINE size_t FindVirtualEntity(Entity entity) const {
		return SearchBytes(virtual_global_components_entities, entity);
	}

	ECS_INLINE size_t FindVirtualComponent(Component component) const {
		return SearchBytes(
			virtual_global_component,
			virtual_global_components_entities.size,
			component.value
		);
	}

	// If the given entity is a virtual global component, it will return the component value associated with it
	ECS_INLINE Component IsVirtualGlobalComponent(Entity entity) const {
		size_t slot_index = FindVirtualEntity(entity);
		return slot_index == -1 ? Component::Invalid() : virtual_global_component[slot_index];
	}

	EditorState* editor_state;
	// These are chars because of the combo box
	unsigned char sandbox_index;
	unsigned char last_sandbox_index;
	CapacityStream<char> filter_string;

	// These are SoA streams
	Stream<Entity> virtual_global_components_entities;
	Component* virtual_global_component;

	// Used to know if we are the active shortcut handler
	unsigned int basic_operations_shortcut_id;
};

#define ITERATOR_LABEL_CAPACITY 128

ECS_INLINE static Entity GlobalComponentsParent() {
	return Entity((unsigned int)-2);
}

// Returns -1 if the entity is not a virtual global component, else return the component that corresponds to that virtual entity
static Component DecodeVirtualEntityToComponent(const EntitiesUIData* entities_data, Entity virtual_entity) {
	size_t component_index = entities_data->FindVirtualEntity(virtual_entity);

	if (component_index != -1) {
		return entities_data->virtual_global_component[component_index];
	}
	return -1;
}

static bool IsNormalEntity(const EntitiesUIData* entities_data, Entity entity) {
	if (entity == GlobalComponentsParent()) {
		return false;
	}

	return !DecodeVirtualEntityToComponent(entities_data, entity).Valid();
}

// Special UI iterator for the entities
// We need to create a virtual entity that is the parent of all global components such that it can be collapsed
// and allow access to the global components
struct HierarchyIteratorImpl {
	using StorageType = Entity;

	using ReturnType = Stream<char>;

	ECS_INLINE HierarchyIteratorImpl() {}

	ECS_INLINE HierarchyIteratorImpl(const EntitiesUIData* ui_data, const EntityHierarchy* hierarchy, const EntityManager* entity_manager, Component name_component) {
		Initialize(ui_data, hierarchy, entity_manager, name_component);
	}
	
	ECS_INLINE AllocatorPolymorphic GetAllocator() {
		return implementation.hierarchy->allocator;
	}

	ECS_INLINE Stream<StorageType> GetChildren(StorageType value, AllocatorPolymorphic allocator) {
		if (value == GlobalComponentsParent()) {
			return ui_data->virtual_global_components_entities;
		}
		else {
			return implementation.GetChildren(value, allocator);
		}
	}
	 
	ECS_INLINE bool HasChildren(StorageType value) const {
		if (value == GlobalComponentsParent()) {
			return entity_manager->GetGlobalComponentCount() > 0;
		}
		else {
			return implementation.HasChildren(value);
		}
	}
	 
	IteratorInterface<const StorageType>* GetRoots(AllocatorPolymorphic allocator) {
		// What we need to return are the underlying roots + the global components parent.
		// We can do this by employing a combined iterator. The first iterator must be the
		// Global components one
		return AllocateAndConstruct<StaticCombinedIterator<const StorageType>>(allocator, GetValueIterator<const StorageType>(global_components_parent, allocator), implementation.GetRoots(allocator));
	}
	 
	ReturnType GetReturnValue(StorageType value, AllocatorPolymorphic allocator) {
		// We need a separate branch for the global components parent
		if (value == GlobalComponentsParent()) {
			entity_label.size = 0;
			entity_label.CopyOther("Global Components");
			return entity_label;
		}
		else {
			// Check now the global components themselves
			Component component = DecodeVirtualEntityToComponent(ui_data, value);
			if (component.Valid()) {
				return entity_manager->GetGlobalComponentName(component);
			}
			else {
				Entity entity = implementation.GetReturnValue(value, allocator);
				entity_label.size = 0;
				const void* component_data = entity_manager->TryGetComponent(entity, name_component);
				const Name* name = (const Name*)component_data;
				if (name != nullptr) {
					return name->name;
				}

				entity.ToString(entity_label);
				return entity_label;
			}
		}
	}

	void Initialize(const EntitiesUIData* _ui_data, const EntityHierarchy* hierarchy, const EntityManager* _entity_manager, Component _name_component) {
		implementation = { hierarchy };
		entity_manager = _entity_manager;
		name_component = _name_component;
		ui_data = _ui_data;
		// Allocate the buffer here
		entity_label.Initialize(hierarchy->allocator, 0, ITERATOR_LABEL_CAPACITY);
		global_components_parent = GlobalComponentsParent();
	};

	void Free() {
		implementation.hierarchy->allocator->Deallocate(entity_label.buffer);
	}

	const EntitiesUIData* ui_data;
	EntityHierarchyIteratorImpl implementation;
	const EntityManager* entity_manager;
	Component name_component;
	CapacityStream<char> entity_label;

	// We store the entity here such that we can retrieve an iterator to it (the value must be stable)
	Entity global_components_parent;
};

typedef DFSIterator<HierarchyIteratorImpl> DFSUIHierarchy;

#pragma region Callbacks

// -------------------------------------------------------------------------------------------------------------

static void EntitiesWholeWindowCreateEmpty(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EntitiesUIData* data = (EntitiesUIData*)_data;
	Entity created_entity = CreateSandboxEntity(data->editor_state, data->sandbox_index);
	ChangeSandboxSelectedEntities(data->editor_state, data->sandbox_index, { &created_entity, 1 });
	// We also need to manually change the inspector to this entity since
	// The backend calls will not trigger the selectable UI callback
	ChangeInspectorToEntity(data->editor_state, data->sandbox_index, created_entity);
}

static void EntitiesWholeWindowAddPrefab(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EntitiesUIData* data = (EntitiesUIData*)_data;
	// This lacks the extension
	Stream<char>* prefab_relative_path = (Stream<char>*)_additional_data;
	ECS_STACK_CAPACITY_STREAM(wchar_t, prefab_absolute_path, 512);
	GetProjectPrefabFolder(data->editor_state, prefab_absolute_path);
	prefab_absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	ConvertASCIIToWide(prefab_absolute_path, *prefab_relative_path);
	prefab_absolute_path.AddStreamAssert(PREFAB_FILE_EXTENSION);
	Entity created_entity;
	bool success = AddPrefabToSandbox(data->editor_state, data->sandbox_index, prefab_absolute_path, &created_entity);
	if (!success) {
		ECS_FORMAT_TEMP_STRING(message, "Failed to instantiate prefab {#}", *prefab_relative_path);
		EditorSetConsoleError(message);
	}
	else {
		// Re-render the viewports
		// Change the selection to this newly created entity
		ChangeSandboxSelectedEntities(data->editor_state, data->sandbox_index, { &created_entity, 1 });
		// We also need to manually change the inspector to this entity since
		// The backend calls will not trigger the selectable UI callback
		ChangeInspectorToEntity(data->editor_state, data->sandbox_index, created_entity);
		RenderSandboxViewports(data->editor_state, data->sandbox_index);
		SetSandboxSceneDirty(data->editor_state, data->sandbox_index);
	}
}

// -------------------------------------------------------------------------------------------------------------

struct EntitiesCreateGlobalComponentData {
	EntitiesUIData* entities_data;
	Component global_component;
};

static void EntitiesCreateGlobalComponent(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	const EntitiesCreateGlobalComponentData* data = (const EntitiesCreateGlobalComponentData*)_data;
	CreateSandboxGlobalComponent(data->entities_data->editor_state, data->entities_data->sandbox_index, data->global_component);
}

// -------------------------------------------------------------------------------------------------------------

static void GetProjectPrefabs(const EditorState* editor_state, AdditionStream<Stream<wchar_t>> project_prefabs, AllocatorPolymorphic allocator) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, prefabs_path, 512);
	GetProjectPrefabFolder(editor_state, prefabs_path);

	if (ExistsFileOrFolder(prefabs_path)) {
		GetDirectoriesOrFilesOptions options;
		options.relative_root = prefabs_path;
		GetDirectoryFilesRecursive(prefabs_path, allocator, project_prefabs, options);

		// Exclude all the files which don't have the prefab extension
		for (unsigned int index = 0; index < project_prefabs.Size(); index++) {
			if (PathExtension(project_prefabs[index], ECS_OS_PATH_SEPARATOR_REL) != PREFAB_FILE_EXTENSION) {
				project_prefabs.RemoveSwapBack(index);
				index--;
			}
		}
	}
}

// Add a right click hoverable on the whole window
static void EntitiesWholeWindowMenu(UIDrawer& drawer, EntitiesUIData* entities_data) {
	UIActionHandler handlers[] = {
		{ EntitiesWholeWindowCreateEmpty, entities_data, 0 },
		{ nullptr, nullptr, 0 },
		{ nullptr, nullptr, 0 }
	};
	bool has_submenu[] = {
		false,
		true,
		true
	};
	UIDrawerMenuState state_submenus[] = {
		{},
		{},
		{}
	};

	static_assert(std::size(handlers) == std::size(has_submenu));
	constexpr size_t handlers_size = std::size(handlers);

	const size_t PREFABS_SUBMENU_INDEX = 1;

	EditorState* editor_state = entities_data->editor_state;

	ECS_STACK_CAPACITY_STREAM(char, menu_left_characters, 512);
	menu_left_characters.CopyOther("Create Empty");

	// The left characters are assigned at the end
	UIDrawerMenuState state;
	state.click_handlers = handlers;
	state.row_count = 1;
	state.row_has_submenu = has_submenu;
	state.submenues = state_submenus;

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 256, ECS_MB);
	ResizableStream<Stream<wchar_t>> project_prefabs(&stack_allocator, 8);
	GetProjectPrefabs(editor_state, &project_prefabs, &stack_allocator);
	
	if (project_prefabs.size > 0) {
		// We need to convert these paths into an ASCII string
		UIDrawerMenuState* prefabs_submenu = state_submenus + PREFABS_SUBMENU_INDEX;
		prefabs_submenu->row_count = project_prefabs.size;
		prefabs_submenu->click_handlers = (UIActionHandler*)stack_allocator.Allocate(sizeof(UIActionHandler) * project_prefabs.size);
		size_t total_character_count = 0;
		for (unsigned int index = 0; index < project_prefabs.size; index++) {
			total_character_count += project_prefabs[index].size + 1;
		}
		char* prefabs_submenu_string = (char*)stack_allocator.Allocate(sizeof(char) * total_character_count);
		prefabs_submenu->left_characters = { prefabs_submenu_string, total_character_count };
		for (unsigned int index = 0; index < project_prefabs.size; index++) {
			Stream<wchar_t> path_without_extension = PathNoExtension(project_prefabs[index]);
			size_t current_string_count = path_without_extension.size;
			ConvertWideCharsToASCII(path_without_extension.buffer, prefabs_submenu_string, current_string_count, total_character_count);
			prefabs_submenu_string[current_string_count] = '\n';
			total_character_count -= current_string_count + 1;
			prefabs_submenu_string += current_string_count + 1;

			// We can set the action handler here as well
			prefabs_submenu->click_handlers[index] = { EntitiesWholeWindowAddPrefab, entities_data, 0 };
		}
		menu_left_characters.AddStreamAssert("\nPrefab");
		state.row_count++;
	}

	ECS_STACK_CAPACITY_STREAM(Component, all_global_components, ECS_KB * 4);
	editor_state->editor_components.FillAllComponents(&all_global_components, ECS_COMPONENT_GLOBAL);

	const EntityManager* entity_manager = ActiveEntityManager(editor_state, entities_data->sandbox_index);
	// Determine which global components have not been created yet
	for (unsigned int index = 0; index < all_global_components.size; index++) {
		if (entity_manager->ExistsGlobalComponent(all_global_components[index])) {
			all_global_components.RemoveSwapBack(index);
			index--;
		}
	}

	// Remove the global components which are private - those should not be created by the user
	for (unsigned int index = 0; index < all_global_components.size; index++) {
		if (editor_state->editor_components.IsGlobalComponentPrivate(editor_state->editor_components.ComponentFromID(all_global_components[index], ECS_COMPONENT_GLOBAL))) {
			all_global_components.RemoveSwapBack(index);
			index--;
		}
	}

	if (all_global_components.size > 0) {
		UIActionHandler* global_component_handlers = (UIActionHandler*)stack_allocator.Allocate(sizeof(UIActionHandler) * all_global_components.size);
		ResizableStream<char> global_components_string;
		global_components_string.Initialize(&stack_allocator, 0);
		global_components_string.ResizeNoCopy(ECS_KB * 4);
		for (unsigned int index = 0; index < all_global_components.size; index++) {
			EntitiesCreateGlobalComponentData* global_handler_data = (EntitiesCreateGlobalComponentData*)stack_allocator.Allocate(sizeof(EntitiesCreateGlobalComponentData));
			global_handler_data->entities_data = entities_data;
			global_handler_data->global_component = all_global_components[index];

			global_component_handlers[index] = { EntitiesCreateGlobalComponent, global_handler_data, sizeof(*global_handler_data) };

			global_components_string.AddStream(editor_state->editor_components.ComponentFromID(all_global_components[index], ECS_COMPONENT_GLOBAL));
			global_components_string.Add('\n');
		}
		// Remove the last '\n'
		global_components_string.size--;
		unsigned int row_index = state.row_count;
		state_submenus[row_index].left_characters = global_components_string;
		state_submenus[row_index].click_handlers = global_component_handlers;
		state_submenus[row_index].row_count = all_global_components.size;
		state_submenus[row_index].submenu_index = 1;

		menu_left_characters.AddStreamAssert("\nGlobal Components");
		state.row_count++;
	}

	state.left_characters = menu_left_characters;
	drawer.SetWindowClickable(&drawer.PrepareRightClickMenuHandler("Entities Right Click", &state), ECS_MOUSE_RIGHT);
}

static void RenameCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyRenameData* rename_data = (UIDrawerLabelHierarchyRenameData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)rename_data->data;

	Entity entity = *(Entity*)rename_data->previous_label;
	ChangeSandboxEntityName(data->editor_state, data->sandbox_index, entity, rename_data->new_label);
}

// -------------------------------------------------------------------------------------------------------------

static void SelectableCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchySelectableData* select_data = (UIDrawerLabelHierarchySelectableData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)select_data->data;
	
	EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_index);
	if (!HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION)) {
		bool change_inspector_selection = true;
		if (select_data->labels.size == 1) {
			Entity* first_entity = (Entity*)select_data->labels.buffer;
			// Check global component
			Component global_component = DecodeVirtualEntityToComponent(data, *first_entity);
			if (global_component.Valid()) {
				ChangeInspectorToGlobalComponent(data->editor_state, data->sandbox_index, global_component);
				change_inspector_selection = false;
			}
			else {
				// Only change the entity if it is not the global component parent
				if (*first_entity == GlobalComponentsParent()) {
					change_inspector_selection = false;
				}
			}
		}

		if (change_inspector_selection) {
			ChangeInspectorEntitySelection(data->editor_state, data->sandbox_index);
		}
	}
	else {
		sandbox->flags = ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
	}
}

// -------------------------------------------------------------------------------------------------------------

// Placeholder at the moment
static void DoubleClickCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDoubleClickData* double_data = (UIDrawerLabelHierarchyDoubleClickData*)_data;
	
}

// -------------------------------------------------------------------------------------------------------------

struct CreatePrefabCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	Entity entity;
};

static void CreatePrefabCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreatePrefabCallbackData* data = (CreatePrefabCallbackData*)_data;
	Stream<char>* region_name = (Stream<char>*)_additional_data;

	if (*region_name == FILE_EXPLORER_DRAG_NAME) {
		bool success = SavePrefabFile(data->editor_state, data->sandbox_index, data->entity);
		if (!success) {
			ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 512);
			Stream<char> entity_name = GetSandboxEntityName(data->editor_state, data->sandbox_index, data->entity, entity_name_storage);
			ECS_FORMAT_TEMP_STRING(message, "Failed to create prefab file for entity {#} from sandbox {#}", entity_name, data->sandbox_index);
			EditorSetConsoleError(message);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

static void DragCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDragData* drag_data = (UIDrawerLabelHierarchyDragData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)drag_data->data;

	if (drag_data->is_released) {
		Entity* given_parent = (Entity*)drag_data->destination_label;
		Entity parent = given_parent != nullptr ? *given_parent : Entity{ (unsigned int)-1 };

		// Make sure that we don't parent to virtual entities
		if (IsNormalEntity(data, parent)) {
			ParentSandboxEntities(data->editor_state, data->sandbox_index, drag_data->source_labels.AsIs<Entity>(), parent);
		}

		if (drag_data->source_labels.size == 1) {
			system->EndDragDrop(ENTITIES_UI_DRAG_NAME);
		}
	}
	else {
		if (drag_data->source_labels.size == 1) {
			if (drag_data->has_started) {
				CreatePrefabCallbackData callback_data;
				callback_data.editor_state = data->editor_state;
				callback_data.sandbox_index = data->sandbox_index;
				callback_data.entity = drag_data->source_labels.AsIs<Entity>()[0];
				system->StartDragDrop({ CreatePrefabCallback, &callback_data, sizeof(callback_data) }, ENTITIES_UI_DRAG_NAME);
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

static void CopyEntityCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyCopyData* copy_data = (UIDrawerLabelHierarchyCopyData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)copy_data->data;

	// Perform the operation only if we are the active shortcut handler
	if (!data->editor_state->shortcut_focus.IsIDActive(EDITOR_SHORTCUT_FOCUS_SANDBOX_BASIC_OPERATIONS, data->sandbox_index, data->basic_operations_shortcut_id)) {
		return;
	}

	// If the editor input mapping left control is disabled, discard this action
	if (data->editor_state->input_mapping.IsKeyDisabled(ECS_KEY_LEFT_CTRL)) {
		return;
	}

	Entity* given_parent = (Entity*)copy_data->destination_label;
	Entity parent = given_parent != nullptr ? *given_parent : Entity((unsigned int)-1);

	Stream<Entity> source_labels = copy_data->source_labels.AsIs<Entity>();
	if (IsNormalEntity(data, parent)) {
		for (size_t index = 0; index < copy_data->source_labels.size; index++) {
			if (IsNormalEntity(data, source_labels[index])) {
				Entity copied_entity = CopySandboxEntity(data->editor_state, data->sandbox_index, source_labels[index]);
				if (parent.value != -1) {
					ParentSandboxEntity(data->editor_state, data->sandbox_index, copied_entity, parent);
				}
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

static void CutEntityCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyCutData* cut_data = (UIDrawerLabelHierarchyCutData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)cut_data->data;

	// Perform the operation only if we are the active shortcut handler
	if (!data->editor_state->shortcut_focus.IsIDActive(EDITOR_SHORTCUT_FOCUS_SANDBOX_BASIC_OPERATIONS, data->sandbox_index, data->basic_operations_shortcut_id)) {
		return;
	}

	// If the editor input mapping left control is disabled, discard this action
	if (data->editor_state->input_mapping.IsKeyDisabled(ECS_KEY_LEFT_CTRL)) {
		return;
	}

	Stream<Entity> entities = cut_data->source_labels.AsIs<Entity>();
	Entity parent = cut_data->destination_label == nullptr ? Entity::Invalid() : *(Entity*)cut_data->destination_label;
	if (IsNormalEntity(data, parent)) {
		if (entities.size > 0) {
			// Determine which labels are valid labels
			Stream<Entity> copy = entities.Copy(data->editor_state->EditorAllocator());
			for (size_t index = 0; index < copy.size; index++) {
				if (!IsNormalEntity(data, copy[index])) {
					copy.RemoveSwapBack(index);
					index--;
				}
			}
			ParentSandboxEntities(data->editor_state, data->sandbox_index, copy, parent);
			data->editor_state->editor_allocator->Deallocate(copy.buffer);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

static void DeleteEntityCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDeleteData* delete_data = (UIDrawerLabelHierarchyDeleteData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)delete_data->data;

	// Perform the operation only if we are the active shortcut handler
	if (!data->editor_state->shortcut_focus.IsIDActive(EDITOR_SHORTCUT_FOCUS_SANDBOX_BASIC_OPERATIONS, data->sandbox_index, data->basic_operations_shortcut_id)) {
		return;
	}

	if (data->editor_state->input_mapping.IsKeyDisabled(ECS_KEY_LEFT_CTRL)) {
		return;
	}

	Stream<Entity> source_labels = delete_data->source_labels.AsIs<Entity>();
	for (size_t index = 0; index < delete_data->source_labels.size; index++) {
		// We shouldn't delete the global parent
		// The global components we have to delete
		if (source_labels[index] != GlobalComponentsParent()) {
			Component global_component = DecodeVirtualEntityToComponent(data, source_labels[index]);
			if (global_component.Valid()) {
				RemoveSandboxGlobalComponent(data->editor_state, data->sandbox_index, global_component);
				// We must also remove it from our internal list of components
				size_t component_index = SearchBytes(data->virtual_global_components_entities, source_labels[index]);
				data->virtual_global_components_entities.RemoveSwapBack(component_index);
			}
			else {
				DeleteSandboxEntity(data->editor_state, data->sandbox_index, source_labels[index]);
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

struct RightClickHandlerData {
	UIDrawerLabelHierarchyData* label_hierarchy;
	EditorState* editor_state;
	unsigned int sandbox_index;
};

static void RightClickCopy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	data->label_hierarchy->RecordSelection(action_data);
	data->label_hierarchy->SetSelectionCut(false);
}

// -------------------------------------------------------------------------------------------------------------

static void RightClickCut(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	data->label_hierarchy->RecordSelection(action_data);
	data->label_hierarchy->SetSelectionCut(true);
}

// -------------------------------------------------------------------------------------------------------------

static void RightClickDelete(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	data->label_hierarchy->TriggerDelete(action_data);
}

// -------------------------------------------------------------------------------------------------------------

static void RightClickCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyRightClickData* right_click_data = (UIDrawerLabelHierarchyRightClickData*)_data;
	EntitiesUIData* ui_data = (EntitiesUIData*)right_click_data->data;

	Entity selected_entity;
	right_click_data->GetLabel(&selected_entity);

	Component global_component = ui_data->IsVirtualGlobalComponent(selected_entity);
	
	UIDrawerMenuState menu_state;
	menu_state.row_count = 0;

	RightClickHandlerData handler_data;
	handler_data.editor_state = ui_data->editor_state;
	handler_data.label_hierarchy = right_click_data->hierarchy;
	handler_data.sandbox_index = ui_data->sandbox_index;

	// We need to have separate branches for normal entities and global components
	if (global_component.Valid()) {
		// It might be a private global component - check that
		bool is_global_component_private = ui_data->editor_state->editor_components.IsGlobalComponentPrivate(ui_data->editor_state->editor_components.ComponentFromID(global_component, ECS_COMPONENT_GLOBAL));
		// For private global components, no menu needs to be created
		if (!is_global_component_private) {
			enum HANDLER_INDEX {
				DELETE_,
				HANDLER_COUNT
			};

			UIActionHandler handlers[HANDLER_COUNT];
			handlers[DELETE_] = { RightClickDelete, &handler_data, sizeof(handler_data) };

			menu_state.left_characters = "Delete";
			menu_state.row_count = HANDLER_COUNT;
			menu_state.click_handlers = handlers;
		}
	}
	else {
		enum HANDLER_INDEX {
			COPY,
			CUT,
			DELETE_,
			HANDLER_COUNT
		};

		UIActionHandler handlers[HANDLER_COUNT];

		handlers[COPY] = { RightClickCopy, &handler_data, sizeof(handler_data) };
		handlers[CUT] = { RightClickCut, &handler_data, sizeof(handler_data) };
		handlers[DELETE_] = { RightClickDelete, &handler_data, sizeof(handler_data) };

		menu_state.left_characters = "Copy\nCut\nDelete";
		menu_state.row_count = HANDLER_COUNT;
		menu_state.click_handlers = handlers;
	}

	if (menu_state.row_count > 0) {
		UIDrawerMenuRightClickData menu_data;
		menu_data.name = "Hierarchy Menu";
		menu_data.window_index = window_index;
		menu_data.state = menu_state;

		action_data->data = &menu_data;
		RightClickMenu(action_data);
	}
	else {
		// Call this to destroy any last menu that there is still pending
		RightClickMenuDestroyLastMenu(system);
	}
}

// -------------------------------------------------------------------------------------------------------------

#pragma endregion

// -------------------------------------------------------------------------------------------------------------

static void EntitiesUIDestroyAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EntitiesUIData* draw_data = (EntitiesUIData*)_additional_data;
	if (draw_data->virtual_global_components_entities.size > 0) {
		draw_data->editor_state->editor_allocator->Deallocate(draw_data->virtual_global_component);
	}
	draw_data->virtual_global_components_entities.Deallocate(draw_data->editor_state->EditorAllocator());
}

void EntitiesUISetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
{
	unsigned int window_index = *(unsigned int*)stack_memory->buffer;

	ECS_ASSERT(window_index < MAX_ENTITIES_UI_WINDOWS);

	EntitiesUIData* data = stack_memory->Reserve<EntitiesUIData>();
	data->editor_state = editor_state;
	data->sandbox_index = 0;
	data->virtual_global_components_entities.InitializeFromBuffer(nullptr, 0);
	// Signal that we don't have an ID yet
	data->basic_operations_shortcut_id = -1;

	CapacityStream<char> window_name(OffsetPointer(data, sizeof(*data)), 0, 128);
	GetEntitiesUIWindowName(window_index, window_name);

	descriptor.window_name = window_name;
	descriptor.draw = EntitiesUIDraw;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);

	descriptor.destroy_action = EntitiesUIDestroyAction;
}

// -------------------------------------------------------------------------------------------------------------

// PERFORMANCE TODO: Make this retained mode with a timed refresh such that the hierarchy is not redrawn
// So many times

void EntitiesUIDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize)
{
	UI_PREPARE_DRAWER(initialize);

	EntitiesUIData* data = (EntitiesUIData*)window_data;
	EditorState* editor_state = data->editor_state;
	unsigned int sandbox_index = data->sandbox_index;

	UIDrawConfig config;

	drawer.SetRowPadding(0.0f);

	if (initialize) {
		const size_t FILTER_STRING_CAPACITY = 128;

		data->filter_string.InitializeFromBuffer(drawer.GetMainAllocatorBuffer(sizeof(char) * FILTER_STRING_CAPACITY), 0, FILTER_STRING_CAPACITY);
		// Clear the changed selection flag
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		sandbox->flags = ClearFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_CHANGED_ENTITY_SELECTION);
	}
	else {
		
	}

	drawer.DisablePaddingForRenderSliders();

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	// Include temporary sandboxes as well
	if (GetSandboxCount(editor_state) > 0) {
		const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
		// Update the shortcut focus ID
		data->basic_operations_shortcut_id = editor_state->shortcut_focus.IncrementOrRegisterForAction(EDITOR_SHORTCUT_FOCUS_SANDBOX_BASIC_OPERATIONS, sandbox_index, EDITOR_SHORTCUT_FOCUS_PRIORITY1, data->basic_operations_shortcut_id);

		// Update the virtual mapping of the global components if the public global component count is different
		EDITOR_SANDBOX_VIEWPORT active_viewport = GetSandboxActiveViewport(editor_state, sandbox_index);
		unsigned int global_component_count = entity_manager->GetGlobalComponentCount();

		if (data->last_sandbox_index != data->sandbox_index || global_component_count != data->virtual_global_components_entities.size
			|| ShouldSandboxRecomputeVirtualEntitySlots(editor_state, sandbox_index)) {
			// Reset the virtual entities first
			// Check to see if there are any selected entities and update their value
			struct RemapSelectedEntity {
				unsigned int selected_index;
				Component component;
			};
			ECS_STACK_CAPACITY_STREAM(RemapSelectedEntity, remap_selected_entities, ECS_KB * 8);
			Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
			for (size_t index = 0; index < selected_entities.size; index++) {
				size_t virtual_index = data->FindVirtualEntity(selected_entities[index]);
				if (virtual_index != -1) {
					remap_selected_entities.AddAssert({ (unsigned int)index, data->virtual_global_component[virtual_index] });
				}
			}
			RemoveSandboxVirtualEntitiesSlot(editor_state, sandbox_index, EDITOR_SANDBOX_ENTITY_SLOT_VIRTUAL_GLOBAL_COMPONENTS);
			if (data->virtual_global_components_entities.size > 0) {
				editor_state->editor_allocator->Deallocate(data->virtual_global_component);
			}
			data->virtual_global_components_entities.Resize(editor_state->EditorAllocator(), global_component_count, false, true);
			if (global_component_count > 0) {
				unsigned int slot_start_index = GetSandboxVirtualEntitySlots(editor_state, sandbox_index, data->virtual_global_components_entities);
				data->virtual_global_component = (Component*)editor_state->editor_allocator->Allocate(sizeof(Component) * global_component_count);				
				memcpy(data->virtual_global_component, GetSandboxEntityManager(editor_state, sandbox_index)->m_global_components, sizeof(Component) * global_component_count);

				for (size_t index = 0; index < data->virtual_global_components_entities.size; index++) {
					EditorSandboxEntitySlot slot;
					slot.slot_type = EDITOR_SANDBOX_ENTITY_SLOT_VIRTUAL_GLOBAL_COMPONENTS;
					slot.SetData(data->virtual_global_component[index]);
					SetSandboxVirtualEntitySlotType(editor_state, sandbox_index, slot_start_index + index, slot);
				}
			}
			else {
				data->virtual_global_component = nullptr;
			}

			ECS_STACK_CAPACITY_STREAM(Entity, selected_entities_to_be_removed, ECS_KB * 8);
			// Go through each selected entity that is a virtual global component and check to see if it exists or a 
			// remapping needs to be performed
			for (unsigned int index = 0; index < remap_selected_entities.size; index++) {
				size_t new_index = data->FindVirtualComponent(remap_selected_entities[index].component);
				if (new_index == -1) {
					// Remove the entity
					selected_entities_to_be_removed.AddAssert(selected_entities[remap_selected_entities[index].selected_index]);
				}
				else {
					selected_entities[remap_selected_entities[index].selected_index] = data->virtual_global_components_entities[new_index];
				}
			}

			for (unsigned int index = 0; index < selected_entities_to_be_removed.size; index++) {
				size_t found_index = SearchBytes(selected_entities, selected_entities_to_be_removed[index]);
				ECS_ASSERT(found_index != -1);
				selected_entities.RemoveSwapBack(found_index);
			}

			SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
		}

		EntitiesWholeWindowMenu(drawer, data);

		// Display the combo box for the sandbox selection
		drawer.SetCurrentPositionToHeader();

		UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
		row_layout.SetOffsetRenderRegion({ true, false });
		row_layout.AddSquareLabel();
		row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
		row_layout.RemoveLastElementsIndentation();

		size_t scene_icon_configuration = 0;
		row_layout.GetTransform(config, scene_icon_configuration);
		drawer.SpriteRectangle(scene_icon_configuration | UI_CONFIG_DO_NOT_ADVANCE, config, ECS_TOOLS_UI_TEXTURE_FILE_SCENE, drawer.color_theme.theme);
		drawer.SolidColorRectangle(scene_icon_configuration, config, drawer.color_theme.text);

		config.flag_count = 0;

		size_t scene_name_configuration = UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_ACTIVE_STATE;
		row_layout.GetTransform(config, scene_name_configuration);

		Stream<wchar_t> scene_name = PathStem(sandbox->scene_path);
		if (scene_name.size == 0) {
			// If empty then set a message
			scene_name = L"No scene set";
		}

		UIConfigActiveState change_scene_active_state;
		change_scene_active_state.state = GetSandboxState(editor_state, sandbox_index) == EDITOR_SANDBOX_SCENE &&
			!IsSandboxTemporary(editor_state, sandbox_index);
		config.AddFlag(change_scene_active_state);

		ChangeSandboxSceneActionData change_data;
		change_data.editor_state = editor_state;
		change_data.sandbox_index = data->sandbox_index;
		drawer.ButtonWide(scene_name_configuration, config, scene_name, { ChangeSandboxSceneAction, &change_data, sizeof(change_data), ECS_UI_DRAW_SYSTEM });
		config.flag_count = 0;

		drawer.NextRow(0.0f);

		size_t HEADER_CONFIGURATION = UI_CONFIG_BORDER | UI_CONFIG_DO_NOT_UPDATE_RENDER_BOUNDS | UI_CONFIG_DO_NOT_VALIDATE_POSITION;

		ECS_STACK_CAPACITY_STREAM(char, sandbox_labels, 512);
		unsigned int sandbox_count = GetSandboxCount(editor_state);

		ECS_STACK_CAPACITY_STREAM(Stream<char>, combo_sandbox_labels, EDITOR_MAX_SANDBOX_COUNT);

		Stream<char> none_label = "None";
		Stream<Stream<char>> combo_labels;

		size_t combo_configuration = UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX | HEADER_CONFIGURATION;

		if (sandbox_count == 0) {
			UIConfigActiveState active_state;
			active_state.state = false;

			combo_configuration |= UI_CONFIG_ACTIVE_STATE;
			config.AddFlag(active_state);
			combo_labels = { &none_label, 1 };
		}
		else {
			for (unsigned int index = 0; index < sandbox_count; index++) {
				combo_sandbox_labels[index].buffer = sandbox_labels.buffer + sandbox_labels.size;
				combo_sandbox_labels[index].size = ConvertIntToChars(sandbox_labels, index);
			}

			combo_labels.buffer = combo_sandbox_labels.buffer;
			combo_labels.size = sandbox_count;
		}

		UIConfigBorder border;
		border.is_interior = true;
		border.color = drawer.color_theme.borders;

		UIConfigComboBoxPrefix prefix;
		prefix.prefix = "Sandbox: ";

		row_layout = drawer.GenerateRowLayout();
		row_layout.SetOffsetRenderRegion({ true, false });
		row_layout.AddComboBox(combo_labels, { nullptr, 0 }, prefix.prefix);
		row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
		row_layout.RemoveLastElementsIndentation();

		config.AddFlag(border);
		config.AddFlag(prefix);
		row_layout.GetTransform(config, combo_configuration);

		drawer.ComboBox(combo_configuration, config, "Sandbox Combo", combo_labels, combo_labels.size, &data->sandbox_index);

		config.flag_count = 0;
		UIConfigTextInputHint hint;
		hint.characters = "Search";
		config.AddFlag(hint);
		config.AddFlag(border);

		size_t text_input_configuration = HEADER_CONFIGURATION | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_NO_NAME;
		row_layout.GetTransform(config, text_input_configuration);

		drawer.TextInput(text_input_configuration, config, "Search filter", &data->filter_string);
		drawer.NextRow();

		if (!initialize) {
			// Display what hierarchy we are drawing
			config.flag_count = 0;
			// Now draw the hierarchy, if there is a sandbox at all
			if (sandbox_count > 0) {
				UIConfigLabelHierarchyFilter filter;
				filter.filter = data->filter_string;
				config.AddFlag(filter);

				size_t LABEL_HIERARCHY_CONFIGURATION = UI_CONFIG_LABEL_HIERARCHY_FILTER | UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK 
					| UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK | UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL | UI_CONFIG_LABEL_HIERARCHY_MONITOR_SELECTION;

				UIConfigLabelHierarchySelectableCallback selectable;
				selectable.callback = SelectableCallback;
				selectable.data = data;
				selectable.data_size = 0;
				config.AddFlag(selectable);

				auto monitor_selection_callback = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					const EntitiesUIData* entities_data = (const EntitiesUIData*)_data;
					UIConfigLabelHierarchyMonitorSelectionInfo* info = (UIConfigLabelHierarchyMonitorSelectionInfo*)_additional_data;

					Stream<Entity> entity_selection = info->selected_labels.AsIs<Entity>();
					// Remove any virtual entities
					for (size_t index = 0; index < entity_selection.size; index++) {
						/*if (!IsNormalEntity(entities_data, entity_selection[index])) {
							entity_selection.RemoveSwapBack(index);
							index--;
						}*/
						if (entity_selection[index] == GlobalComponentsParent()) {
							entity_selection.RemoveSwapBack(index);
							index--;
						}
					}

					info->selected_labels.size = entity_selection.size;
				};

				UIConfigLabelHierarchyMonitorSelection monitor_selection;
				monitor_selection.is_capacity_selection = false;
				monitor_selection.boolean_changed_flag = false;
				monitor_selection.is_changed_counter = &sandbox->selected_entities_changed_counter;
				monitor_selection.resizable_selection = (ResizableStream<void>*)&sandbox->selected_entities;
				monitor_selection.callback = { monitor_selection_callback, data, 0 };
				config.AddFlag(monitor_selection);

				UIConfigLabelHierarchyRightClick right_click_callback;
				right_click_callback.callback = RightClickCallback;
				right_click_callback.data = data;
				right_click_callback.data_size = 0;
				config.AddFlag(right_click_callback);

				UIConfigLabelHierarchyBasicOperations basic_operations;
				basic_operations.copy_handler = { CopyEntityCallback, data, 0 };
				basic_operations.cut_handler = { CutEntityCallback, data, 0 };
				basic_operations.delete_handler = { DeleteEntityCallback, data, 0 };
				basic_operations.trigger_when_window_is_focused_only = false;
				config.AddFlag(basic_operations);
				LABEL_HIERARCHY_CONFIGURATION |= UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS;

				UIConfigLabelHierarchyRenameCallback rename_callback;
				rename_callback.callback = RenameCallback;
				rename_callback.data = data;
				rename_callback.data_size = 0;
				config.AddFlag(rename_callback);

				const EntityHierarchy* hierarchy = &entity_manager->m_hierarchy;
				HierarchyIteratorImpl implementation(data, hierarchy, entity_manager, editor_state->editor_components.GetComponentID(STRING(Name)));

				AllocatorPolymorphic iterator_allocator = entity_manager->m_memory_manager;

				// Use the allocator from the entity manager because the one from the hierarchy can cause deallocating the roots
				DFSUIHierarchy iterator(iterator_allocator, implementation, 128);

				LABEL_HIERARCHY_CONFIGURATION |= UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL;

				UIConfigLabelHierarchyDragCallback drag_callback;
				drag_callback.callback = DragCallback;
				drag_callback.data = data;
				drag_callback.data_size = 0;
				drag_callback.reject_same_label_drag = true;
				drag_callback.call_on_mouse_hold = true;
				config.AddFlag(drag_callback);

				IteratorPolymorphic iterator_polymorphic = iterator.AsPolymorphic();

				drawer.LabelHierarchy(LABEL_HIERARCHY_CONFIGURATION, config, "Hierarchy", &iterator_polymorphic, true);
				iterator.Free();
			}
		}
	}
	else {
		drawer.NextRow();
		UIConfigAlignElement align_element;
		align_element.horizontal = ECS_UI_ALIGN_MIDDLE;
		config.AddFlag(align_element);
		drawer.Text(UI_CONFIG_ALIGN_ELEMENT, config, "There are no sandboxes.");
	}
	
	data->last_sandbox_index = data->sandbox_index;
}

// -------------------------------------------------------------------------------------------------------------

unsigned int CreateEntitiesUI(EditorState* editor_state)
{
	UISystem* ui_system = editor_state->ui_system;

	unsigned int last_entities_index = GetEntitiesUILastWindowIndex(editor_state);
	// This works even for the case when last entities is -1
	// This is fine even for -1, when it gets incremented then it will be 0.
	unsigned int window_index = CreateEntitiesUIWindow(editor_state, last_entities_index + 1);

	UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
	ui_system->CreateDockspace(window_transform, DockspaceType::FloatingHorizontal, window_index, false);

	return window_index;
}

// -------------------------------------------------------------------------------------------------------------

void CreateEntitiesUIAction(ActionData* action_data)
{
	CreateEntitiesUI((EditorState*)action_data->data);
}

// -------------------------------------------------------------------------------------------------------------

unsigned int CreateEntitiesUIWindow(EditorState* editor_state, unsigned int window_index)
{
	UIWindowDescriptor descriptor;
	ECS_ASSERT(window_index < MAX_ENTITIES_UI_WINDOWS, "The maximum number of EntitiesUI windows has been exceeded!");

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 2);
	unsigned int* stack_index = stack_memory.Reserve<unsigned int>();
	*stack_index = window_index;
	EntitiesUISetDescriptor(descriptor, editor_state, &stack_memory);

	descriptor.initial_size_x = 0.6f;
	descriptor.initial_size_y = 1.0f;

	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, descriptor.initial_size_x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, descriptor.initial_size_y);

	// Also trigger a re-update of the selected entities such that this newly created entities UI
	// will update itself
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	if (sandbox_count > 0) {
		SignalSandboxSelectedEntitiesCounter(editor_state, 0);
	}

	return editor_state->ui_system->Create_Window(descriptor);
}

// -------------------------------------------------------------------------------------------------------------

void GetEntitiesUIWindowName(unsigned int window_index, CapacityStream<char>& name)
{
	name.CopyOther(ENTITIES_UI_WINDOW_NAME);
	ConvertIntToChars(name, window_index);
}

// -------------------------------------------------------------------------------------------------------------

unsigned int GetEntitiesUIWindowIndex(const EditorState* editor_state, unsigned int entities_window_index)
{
	ECS_STACK_CAPACITY_STREAM(char, name, 128);
	GetEntitiesUIWindowName(entities_window_index, name);

	return editor_state->ui_system->GetWindowFromName(name);
}

// -------------------------------------------------------------------------------------------------------------

unsigned int GetEntitiesUIIndexFromName(Stream<char> name)
{
	Stream<char> space = FindFirstCharacter(name, ' ');
	ECS_ASSERT(space.buffer != nullptr);
	space.buffer += 1;
	space.size -= 1;

	return ConvertCharactersToInt(space);
}

// -------------------------------------------------------------------------------------------------------------

unsigned int GetEntitiesUILastWindowIndex(const EditorState* editor_state)
{
	// Start backwards
	for (int64_t index = MAX_ENTITIES_UI_WINDOWS - 1; index >= 0; index--) {
		ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
		GetEntitiesUIWindowName(index, window_name);

		unsigned int window_index = editor_state->ui_system->GetWindowFromName(window_name);
		if (window_index != -1) {
			return index;
		}
	}

	return -1;
}

// -------------------------------------------------------------------------------------------------------------

unsigned int GetEntitiesUITargetSandbox(const EditorState* editor_state, unsigned int entities_index)
{
	unsigned int window_index = GetEntitiesUIWindowIndex(editor_state, entities_index);
	if (window_index != -1) {
		EntitiesUIData* data = (EntitiesUIData*)editor_state->ui_system->GetWindowData(window_index);
		return data->sandbox_index;
	}
	else {
		return -1;
	}
}

// -------------------------------------------------------------------------------------------------------------

void UpdateEntitiesUITargetSandbox(EditorState* editor_state, unsigned int old_index, unsigned int new_index)
{
	for (unsigned int entities_index = 0; entities_index < MAX_ENTITIES_UI_WINDOWS; entities_index++) {
		unsigned int window_index = GetEntitiesUIWindowIndex(editor_state, entities_index);
		if (window_index != -1) {
			EntitiesUIData* data = (EntitiesUIData*)editor_state->ui_system->GetWindowData(window_index);
			if (data->sandbox_index == (unsigned char)new_index) {
				// This index was deleted, make it 0 again
				data->sandbox_index = 0;
			}
			
			if (data->sandbox_index == (unsigned char)old_index) {
				data->sandbox_index = (unsigned char)new_index;
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

unsigned int EntitiesUITargetSandbox(const EditorState* editor_state, unsigned int window_index)
{
	Stream<char> window_name = editor_state->ui_system->GetWindowName(window_index);
	Stream<char> prefix = ENTITIES_UI_WINDOW_NAME;
	if (window_name.StartsWith(prefix)) {
		unsigned int entities_index = GetEntitiesUIIndexFromName(window_name);
		return GetEntitiesUITargetSandbox(editor_state, entities_index);
	}
	else {
		return -1;
	}
}

// -------------------------------------------------------------------------------------------------------------

void SetEntitiesUITargetSandbox(const EditorState* editor_state, unsigned int window_index, unsigned int target_sandbox)
{
	EntitiesUIData* data = (EntitiesUIData*)editor_state->ui_system->GetWindowData(window_index);
	data->sandbox_index = target_sandbox;
}

// -------------------------------------------------------------------------------------------------------------
