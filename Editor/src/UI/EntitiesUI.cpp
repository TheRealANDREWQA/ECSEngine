#include "editorpch.h"
#include "EntitiesUI.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/Sandbox.h"
#include "Inspector.h"

#include "ECSEngineComponents.h"
#include "CreateScene.h"

using namespace ECSEngine;
ECS_TOOLS;

struct EntitiesUIData {
	EditorState* editor_state;
	unsigned char sandbox_index;
	CapacityStream<char> filter_string;

	// Each entity corresponds to the global component at that index inside the entity manager
	Stream<Entity> virtual_global_components_entities;
	// We need to record this to change the entities that we stored here when a change of
	// runtime happens
	EDITOR_SANDBOX_VIEWPORT virtual_components_viewport;
};

#define ITERATOR_LABEL_CAPACITY 128

static ECS_INLINE Entity GlobalComponentsParent() {
	return Entity((unsigned int)-2);
}

// Returns -1 if the entity is not a virtual global component, else return the component that corresponds to that virtual entity
static Component DecodeVirtualEntityToComponent(const EntitiesUIData* entities_data, Entity virtual_entity) {
	const EntityManager* entity_manager = ActiveEntityManager(entities_data->editor_state, entities_data->sandbox_index);
	size_t component_index = function::SearchBytes(
		entities_data->virtual_global_components_entities.buffer, 
		entities_data->virtual_global_components_entities.size, 
		virtual_entity.value, 
		sizeof(virtual_entity)
	);

	if (component_index != -1) {
		return entity_manager->m_global_components[component_index];
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
	
	ECS_INLINE AllocatorPolymorphic GetAllocator() const {
		return GetAllocatorPolymorphic(implementation.hierarchy->allocator);
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
	 
	Stream<StorageType> GetRoots(AllocatorPolymorphic allocator) {
		// Unfortunately, we need to make an allocation here just for that single addition
		// The original roots don't need to be deallocated
		Stream<Entity> original_roots = implementation.GetRoots(allocator);
		Stream<Entity> new_roots;
		new_roots.Initialize(allocator, original_roots.size + 1);
		new_roots.size = 0;
		// Add the global component root first
		new_roots.Add(GlobalComponentsParent());
		new_roots.AddStream(original_roots);
		return new_roots;
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
				const void* component = entity_manager->TryGetComponent(entity, name_component);
				const Name* name = (const Name*)component;
				if (component != nullptr) {
					return name->name;
				}

				EntityToString(entity, entity_label);
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
	};

	void Free() {
		implementation.hierarchy->allocator->Deallocate(entity_label.buffer);
	}

	const EntitiesUIData* ui_data;
	EntityHierarchyIteratorImpl implementation;
	const EntityManager* entity_manager;
	Component name_component;
	CapacityStream<char> entity_label;
};

typedef DFSIterator<HierarchyIteratorImpl> DFSUIHierarchy;

#pragma region Callbacks

// -------------------------------------------------------------------------------------------------------------

static void EntitiesWholeWindowCreateEmpty(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EntitiesUIData* data = (EntitiesUIData*)_data;
	CreateSandboxEntity(data->editor_state, data->sandbox_index);
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

// Add a right click hoverable on the whole window
static void EntitiesWholeWindowMenu(UIDrawer& drawer, EntitiesUIData* entities_data) {
	UIActionHandler handlers[] = {
		{ EntitiesWholeWindowCreateEmpty, entities_data, 0 }
	};

	constexpr size_t handlers_size = std::size(handlers);

	UIDrawerMenuState state;
	state.left_characters = "Create Empty";
	state.click_handlers = handlers;
	state.row_count = handlers_size;

	ECS_STACK_CAPACITY_STREAM(Component, all_global_components, ECS_KB * 4);
	entities_data->editor_state->editor_components.FillAllComponents(&all_global_components, ECS_COMPONENT_GLOBAL);

	const EntityManager* entity_manager = ActiveEntityManager(entities_data->editor_state, entities_data->sandbox_index);
	// Determine which global components have not been created yet
	for (unsigned int index = 0; index < all_global_components.size; index++) {
		if (entity_manager->ExistsGlobalComponent(all_global_components[index])) {
			all_global_components.RemoveSwapBack(index);
			index--;
		}
	}

	bool row_has_submenu[handlers_size + 1];
	UIDrawerMenuState submenu_states[handlers_size + 1];

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 256, ECS_MB);
	if (all_global_components.size > 0) {
		UIActionHandler* global_component_handlers = (UIActionHandler*)stack_allocator.Allocate(sizeof(UIActionHandler) * all_global_components.size);
		ResizableStream<char> global_components_string;
		global_components_string.Initialize(GetAllocatorPolymorphic(&stack_allocator), 0);
		global_components_string.ResizeNoCopy(ECS_KB * 4);
		for (unsigned int index = 0; index < all_global_components.size; index++) {
			EntitiesCreateGlobalComponentData* global_handler_data = (EntitiesCreateGlobalComponentData*)stack_allocator.Allocate(sizeof(EntitiesCreateGlobalComponentData));
			global_handler_data->entities_data = entities_data;
			global_handler_data->global_component = all_global_components[index];

			global_component_handlers[index] = { EntitiesCreateGlobalComponent, global_handler_data, sizeof(*global_handler_data) };

			global_components_string.AddStream(entities_data->editor_state->editor_components.ComponentFromID(all_global_components[index], ECS_COMPONENT_GLOBAL));
			global_components_string.Add('\n');
		}
		// Remove the last '\n'
		global_components_string.size--;

		memset(row_has_submenu, 0, sizeof(bool) * handlers_size);
		row_has_submenu[handlers_size] = true;

		memset(submenu_states + handlers_size, 0, sizeof(UIDrawerMenuState));
		submenu_states[handlers_size].left_characters = global_components_string;
		submenu_states[handlers_size].click_handlers = global_component_handlers;
		submenu_states[handlers_size].row_count = all_global_components.size;
		submenu_states[handlers_size].submenu_index = 1;

		state.left_characters = "Create Empty\nGlobal Components";
		state.row_count++;
		state.row_has_submenu = row_has_submenu;
		state.submenues = submenu_states;
	}

	drawer.SetWindowClickable(&drawer.PrepareRightClickHandler("Entities Right Click", &state), ECS_MOUSE_RIGHT);
}

static void RenameCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyRenameData* rename_data = (UIDrawerLabelHierarchyRenameData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)rename_data->data;

	Entity entity = *(Entity*)rename_data->previous_label;
	ChangeEntityName(data->editor_state, data->sandbox_index, entity, rename_data->new_label);
}

// -------------------------------------------------------------------------------------------------------------

static void SelectableCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchySelectableData* select_data = (UIDrawerLabelHierarchySelectableData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)select_data->data;
	
	if (select_data->labels.size == 1) {
		Entity* first_entity = (Entity*)select_data->labels.buffer;
		// Check global component
		Component global_component = DecodeVirtualEntityToComponent(data, *first_entity);
		if (global_component.Valid()) {
			ChangeInspectorToGlobalComponent(data->editor_state, data->sandbox_index, global_component);
		}
		else {
			// Only change the entity if it is not the global component parent
			if (*first_entity != GlobalComponentsParent()) {
				ChangeInspectorToEntity(data->editor_state, data->sandbox_index, *first_entity);
			}
		}
	}
	else if (select_data->labels.size == 0) {
		unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
		unsigned int entities_ui_index = GetInspectorIndex(system->GetWindowName(window_index));
		unsigned int target_sandbox = GetEntitiesUITargetSandbox(data->editor_state, entities_ui_index);

		ECS_STACK_CAPACITY_STREAM(unsigned int, inspector_indices, 512);
		GetInspectorsForSandbox(data->editor_state, target_sandbox, &inspector_indices);
		for (unsigned int index = 0; index < inspector_indices.size; index++) {
			if (!IsInspectorLocked(data->editor_state, inspector_indices[index]) && IsInspectorDrawEntity(data->editor_state, inspector_indices[index])) {		
				ChangeInspectorToNothing(data->editor_state, inspector_indices[index]);
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

// Placeholder at the moment
static void DoubleClickCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDoubleClickData* double_data = (UIDrawerLabelHierarchyDoubleClickData*)_data;
	
}

// -------------------------------------------------------------------------------------------------------------

static void DragCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDragData* drag_data = (UIDrawerLabelHierarchyDragData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)drag_data->data;

	Entity* given_parent = (Entity*)drag_data->destination_label;
	Entity parent = given_parent != nullptr ? *given_parent : Entity{ (unsigned int)-1 };

	// Make sure that we don't parent to virtual entities
	if (IsNormalEntity(data, parent)) {
		ParentSandboxEntities(data->editor_state, data->sandbox_index, drag_data->source_labels.AsIs<Entity>(), parent);
	}
}

// -------------------------------------------------------------------------------------------------------------

static void CopyEntityCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyCopyData* copy_data = (UIDrawerLabelHierarchyCopyData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)copy_data->data;

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

	Stream<Entity> entities = cut_data->source_labels.AsIs<Entity>();
	Entity parent = *(Entity*)cut_data->destination_label;
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

	Stream<Entity> source_labels = delete_data->source_labels.AsIs<Entity>();
	for (size_t index = 0; index < delete_data->source_labels.size; index++) {
		// We shouldn't delete the global parent
		// The global components we have to delete
		if (source_labels[index] != GlobalComponentsParent()) {
			Component global_component = DecodeVirtualEntityToComponent(data, source_labels[index]);
			if (global_component.Valid()) {
				RemoveSandboxGlobalComponent(data->editor_state, data->sandbox_index, global_component);
				// We must also remove it from our internal list of components
				size_t component_index = function::SearchBytes(
					data->virtual_global_components_entities.buffer,
					data->virtual_global_components_entities.size,
					source_labels[index].value,
					sizeof(source_labels[index])
				);
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

	enum HANDLER_INDEX {
		COPY,
		CUT,
		DELETE_,
		HANDLER_COUNT
	};

	UIActionHandler handlers[HANDLER_COUNT];

	RightClickHandlerData handler_data;
	handler_data.editor_state = ui_data->editor_state;
	handler_data.label_hierarchy = right_click_data->hierarchy;
	handler_data.sandbox_index = ui_data->sandbox_index;
	handlers[COPY] = { RightClickCopy, &handler_data, sizeof(handler_data) };
	handlers[CUT] = { RightClickCut, &handler_data, sizeof(handler_data) };
	handlers[DELETE_] = { RightClickDelete, &handler_data, sizeof(handler_data) };

	UIDrawerMenuState menu_state;
	menu_state.left_characters = "Copy\nCut\nDelete";
	menu_state.row_count = HANDLER_COUNT;
	menu_state.click_handlers = handlers;

	UIDrawerMenuRightClickData menu_data;
	menu_data.name = "Hierarchy Menu";
	menu_data.window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
	menu_data.state = menu_state;

	action_data->data = &menu_data;
	RightClickMenu(action_data);
}

// -------------------------------------------------------------------------------------------------------------

#pragma endregion

// -------------------------------------------------------------------------------------------------------------

static void EntitiesUIDestroyAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EntitiesUIData* draw_data = (EntitiesUIData*)_additional_data;
	draw_data->virtual_global_components_entities.Deallocate(draw_data->editor_state->EditorAllocator());
}

void EntitiesUISetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int window_index = *(unsigned int*)stack_memory;

	ECS_ASSERT(window_index < MAX_ENTITIES_UI_WINDOWS);

	EntitiesUIData* data = (EntitiesUIData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));
	data->editor_state = editor_state;
	data->sandbox_index = -1;
	data->virtual_global_components_entities.InitializeFromBuffer(nullptr, 0);
	data->virtual_components_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;

	CapacityStream<char> window_name(function::OffsetPointer(data, sizeof(*data)), 0, 128);
	GetEntitiesUIWindowName(window_index, window_name);

	descriptor.window_name = window_name;
	descriptor.draw = EntitiesUIDraw;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);

	descriptor.destroy_action = EntitiesUIDestroyAction;
}

// -------------------------------------------------------------------------------------------------------------

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
		data->sandbox_index = 0;
	}
	else {
		
	}

	drawer.DisablePaddingForRenderSliders();

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	if (editor_state->sandboxes.size > 0) {
		const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

		// Get the count of global components and update the virtual mapping of the global components if the count is different
		unsigned int global_component_count = entity_manager->GetGlobalComponentCount();
		EDITOR_SANDBOX_VIEWPORT active_viewport = GetSandboxActiveViewport(editor_state, sandbox_index);
		if (global_component_count != data->virtual_global_components_entities.size || ShouldSandboxRecomputeEntitySlots(editor_state, sandbox_index) 
			|| data->virtual_components_viewport != active_viewport) {
			// Reset the virtual entities first
			RemoveSandboxUnusedEntitiesSlot(editor_state, sandbox_index, EDITOR_SANDBOX_ENTITY_SLOT_VIRTUAL_GLOBAL_COMPONENTS);
			data->virtual_global_components_entities.Resize(editor_state->EditorAllocator(), global_component_count, false, true);
			if (global_component_count > 0) {
				unsigned int slot_start_index = GetSandboxUnusedEntitySlots(editor_state, sandbox_index, data->virtual_global_components_entities);
				for (size_t index = 0; index < data->virtual_global_components_entities.size; index++) {
					SetSandboxUnusedEntitySlotType(editor_state, sandbox_index, slot_start_index + index, EDITOR_SANDBOX_ENTITY_SLOT_VIRTUAL_GLOBAL_COMPONENTS);
				}
			}
			data->virtual_components_viewport = active_viewport;
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

		size_t scene_name_configuration = UI_CONFIG_LABEL_TRANSPARENT;
		row_layout.GetTransform(config, scene_name_configuration);

		Stream<wchar_t> scene_name = function::PathStem(sandbox->scene_path);
		if (scene_name.size == 0) {
			// If empty then set a message
			scene_name = L"No scene set";
		}

		ChangeSandboxSceneActionData change_data;
		change_data.editor_state = editor_state;
		change_data.sandbox_index = data->sandbox_index;
		drawer.ButtonWide(scene_name_configuration, config, scene_name, { ChangeSandboxSceneAction, &change_data, sizeof(change_data), ECS_UI_DRAW_SYSTEM });
		config.flag_count = 0;

		drawer.NextRow(0.0f);

		size_t HEADER_CONFIGURATION = UI_CONFIG_BORDER | UI_CONFIG_DO_NOT_UPDATE_RENDER_BOUNDS | UI_CONFIG_DO_NOT_VALIDATE_POSITION;

		ECS_STACK_CAPACITY_STREAM(char, sandbox_labels, 512);
		unsigned int sandbox_count = editor_state->sandboxes.size;

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, combo_sandbox_labels, sandbox_count);

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
				combo_sandbox_labels[index].size = function::ConvertIntToChars(sandbox_labels, index);
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
				EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

				UIConfigLabelHierarchyFilter filter;
				filter.filter = data->filter_string;
				config.AddFlag(filter);

				size_t LABEL_HIERARCHY_CONFIGURATION = UI_CONFIG_LABEL_HIERARCHY_FILTER | UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK 
					| UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK | UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS 
					| UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL | UI_CONFIG_LABEL_HIERARCHY_MONITOR_SELECTION;

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
				config.AddFlag(basic_operations);

				UIConfigLabelHierarchyRenameCallback rename_callback;
				rename_callback.callback = RenameCallback;
				rename_callback.data = data;
				rename_callback.data_size = 0;
				config.AddFlag(rename_callback);

				const EntityHierarchy* hierarchy = &entity_manager->m_hierarchy;
				HierarchyIteratorImpl implementation(data, hierarchy, entity_manager, editor_state->editor_components.GetComponentID(STRING(Name)));

				AllocatorPolymorphic iterator_allocator = GetAllocatorPolymorphic(entity_manager->m_memory_manager);

				// Use the allocator from the entity manager because the one from the hierarchy can cause deallocating the roots
				DFSUIHierarchy iterator(iterator_allocator, implementation, hierarchy->children_table.GetCount());

				LABEL_HIERARCHY_CONFIGURATION |= UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL;

				UIConfigLabelHierarchyDragCallback drag_callback;
				drag_callback.callback = DragCallback;
				drag_callback.data = data;
				drag_callback.data_size = 0;
				drag_callback.reject_same_label_drag = true;
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
}

// -------------------------------------------------------------------------------------------------------------

void CreateEntitiesUI(EditorState* editor_state)
{
	UISystem* ui_system = editor_state->ui_system;

	unsigned int last_entities_index = GetEntitiesUILastWindowIndex(editor_state);
	// This works even for the case when last entities is -1
	ECS_ASSERT(last_entities_index + 1 < MAX_ENTITIES_UI_WINDOWS);
	// This is fine even for -1, when it gets incremented then it will be 0.
	unsigned int window_index = CreateEntitiesUIWindow(editor_state, last_entities_index + 1);

	UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
	ui_system->CreateDockspace(window_transform, DockspaceType::FloatingHorizontal, window_index, false);

	// Also trigger a re-update of the selected entities such that this newly created entities UI
	// will update itself
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	if (sandbox_count > 0) {
		SignalSandboxSelectedEntitiesCounter(editor_state, 0);
	}
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

	size_t stack_memory[256];
	unsigned int* stack_index = (unsigned int*)stack_memory;
	*stack_index = window_index;
	EntitiesUISetDescriptor(descriptor, editor_state, stack_memory);

	descriptor.initial_size_x = 0.6f;
	descriptor.initial_size_y = 1.0f;

	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, descriptor.initial_size_x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, descriptor.initial_size_y);

	return editor_state->ui_system->Create_Window(descriptor);
}

// -------------------------------------------------------------------------------------------------------------

void GetEntitiesUIWindowName(unsigned int window_index, CapacityStream<char>& name)
{
	name.CopyOther(ENTITIES_UI_WINDOW_NAME);
	function::ConvertIntToChars(name, window_index);
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
	Stream<char> space = function::FindFirstCharacter(name, ' ');
	ECS_ASSERT(space.buffer != nullptr);
	space.buffer += 1;
	space.size -= 1;

	return function::ConvertCharactersToInt(space);
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
