#include "editorpch.h"
#include "EntitiesUI.h"
#include "Editor/EditorState.h"

#include "Editor/EditorSandboxEntityOperations.h"
#include "Inspector.h"

#include "ECSEngineComponents.h"

#include "CreateScene.h"

using namespace ECSEngine;
ECS_TOOLS;

struct EntitiesUIData {
	EditorState* editor_state;
	unsigned char sandbox_index;
	CapacityStream<char> filter_string;
};

const size_t ITERATOR_LABEL_CAPACITY = 128;

// Special UI iterator for the entities
struct HierarchyIteratorImpl {
	HierarchyIteratorImpl() {}

	HierarchyIteratorImpl(const EntityHierarchy* hierarchy, const EntityManager* entity_manager, Component name_component) {
		Initialize(hierarchy, entity_manager, name_component);
	}

	using storage_type = Entity;
	 
	using return_type = Stream<char>;
	 
	Stream<storage_type> GetChildren(storage_type value, AllocatorPolymorphic allocator) {
		return implementation.GetChildren(value, allocator);
	}
	 
	bool HasChildren(storage_type value) const {
		return implementation.HasChildren(value);
	}
	 
	Stream<storage_type> GetRoots(AllocatorPolymorphic allocator) {
		return implementation.GetRoots(allocator);
	}
	 
	return_type GetReturnValue(storage_type value, AllocatorPolymorphic allocator) {
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

	void Initialize(const EntityHierarchy* hierarchy, const EntityManager* _entity_manager, Component _name_component) {
		implementation = { hierarchy };
		entity_manager = _entity_manager;
		name_component = _name_component;
		// Allocate the buffer here
		entity_label.Initialize(hierarchy->allocator, 0, ITERATOR_LABEL_CAPACITY);
	};

	void Free() {
		implementation.hierarchy->allocator->Deallocate(entity_label.buffer);
	}

	EntityHierarchyIteratorImpl implementation;
	const EntityManager* entity_manager;
	Component name_component;
	CapacityStream<char> entity_label;
};

typedef DFSIterator<HierarchyIteratorImpl> DFSUIHierarchy;

#pragma region Callbacks

// -------------------------------------------------------------------------------------------------------------

void EntitiesWholeWindowCreateEmpty(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EntitiesUIData* data = (EntitiesUIData*)_data;
	CreateSandboxEntity(data->editor_state, data->sandbox_index);
}

// -------------------------------------------------------------------------------------------------------------

// Add a right click hoverable on the whole window
void EntitiesWholeWindowMenu(UIDrawer& drawer, EntitiesUIData* entities_data) {
	UIActionHandler handlers[] = {
		{ EntitiesWholeWindowCreateEmpty, entities_data, 0 }
	};

	UIDrawerMenuState state;
	state.left_characters = "Create Empty";
	state.click_handlers = handlers;
	state.row_count = std::size(handlers);

	drawer.AddRightClickAction(drawer.GetRegionPosition(), drawer.GetRegionScale(), "Entities Right Click", &state);
}

void RenameCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyRenameData* rename_data = (UIDrawerLabelHierarchyRenameData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)rename_data->data;

	Entity entity = *(Entity*)rename_data->previous_label;
	ChangeEntityName(data->editor_state, data->sandbox_index, entity, rename_data->new_label);
}

// -------------------------------------------------------------------------------------------------------------

void SelectableCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchySelectableData* select_data = (UIDrawerLabelHierarchySelectableData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)select_data->data;
	
	if (select_data->labels.size == 1) {
		Entity* first_entity = (Entity*)select_data->labels.buffer;
		ChangeInspectorToEntity(data->editor_state, data->sandbox_index, *first_entity);
	}
}

// -------------------------------------------------------------------------------------------------------------

// Placeholder at the moment
void DoubleClickCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDoubleClickData* double_data = (UIDrawerLabelHierarchyDoubleClickData*)_data;
	
}

// -------------------------------------------------------------------------------------------------------------

void DragCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDragData* drag_data = (UIDrawerLabelHierarchyDragData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)drag_data->data;

	Entity* given_parent = (Entity*)drag_data->destination_label;
	Entity parent = given_parent != nullptr ? *given_parent : Entity{ (unsigned int)-1 };
	ParentSandboxEntities(data->editor_state, data->sandbox_index, drag_data->source_labels.AsIs<Entity>(), parent);
}

// -------------------------------------------------------------------------------------------------------------

void CopyEntityCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyCopyData* copy_data = (UIDrawerLabelHierarchyCopyData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)copy_data->data;

	Entity* given_parent = (Entity*)copy_data->destination_label;
	Entity parent = given_parent != nullptr ? *given_parent : Entity((unsigned int)-1);

	Stream<Entity> source_labels = copy_data->source_labels.AsIs<Entity>();
	for (size_t index = 0; index < copy_data->source_labels.size; index++) {
		Entity copied_entity = CopySandboxEntity(data->editor_state, data->sandbox_index, source_labels[index]);
		if (parent.value != -1) {
			ParentSandboxEntity(data->editor_state, data->sandbox_index, copied_entity, parent);
		}
	}
}

// -------------------------------------------------------------------------------------------------------------

void CutEntityCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyCutData* cut_data = (UIDrawerLabelHierarchyCutData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)cut_data->data;

	const Entity* entities = (const Entity*)cut_data->source_labels.buffer;
	ParentSandboxEntities(data->editor_state, data->sandbox_index, { entities, cut_data->source_labels.size }, *(Entity*)cut_data->destination_label);
}

// -------------------------------------------------------------------------------------------------------------

void DeleteEntityCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDeleteData* delete_data = (UIDrawerLabelHierarchyDeleteData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)delete_data->data;

	Stream<Entity> source_labels = delete_data->source_labels.AsIs<Entity>();
	for (size_t index = 0; index < delete_data->source_labels.size; index++) {
		DeleteSandboxEntity(data->editor_state, data->sandbox_index, source_labels[index]);
	}
}

// -------------------------------------------------------------------------------------------------------------

struct RightClickHandlerData {
	UIDrawerLabelHierarchyData* label_hierarchy;
	EditorState* editor_state;
	unsigned int sandbox_index;
};

void RightClickCopy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	data->label_hierarchy->RecordSelection(action_data);
	data->label_hierarchy->SetSelectionCut(false);
}

// -------------------------------------------------------------------------------------------------------------

void RightClickCut(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	data->label_hierarchy->RecordSelection(action_data);
	data->label_hierarchy->SetSelectionCut(true);
}

// -------------------------------------------------------------------------------------------------------------

void RightClickDelete(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	data->label_hierarchy->TriggerDelete(action_data);
}

// -------------------------------------------------------------------------------------------------------------

void RightClickCallback(ActionData* action_data) {
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

void EntitiesUISetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int window_index = *(unsigned int*)stack_memory;

	ECS_ASSERT(window_index < MAX_ENTITIES_UI_WINDOWS);

	EntitiesUIData* data = (EntitiesUIData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));
	data->editor_state = editor_state;
	data->sandbox_index = -1;

	CapacityStream<char> window_name(function::OffsetPointer(data, sizeof(*data)), 0, 128);
	GetEntitiesUIWindowName(window_index, window_name);

	descriptor.window_name = window_name;
	descriptor.draw = EntitiesUIDraw;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
}

// -------------------------------------------------------------------------------------------------------------

void EntitiesUIDraw(void* window_data, void* drawer_descriptor, bool initialize)
{
	UI_PREPARE_DRAWER(initialize);

	EntitiesUIData* data = (EntitiesUIData*)window_data;
	EditorState* editor_state = data->editor_state;

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

	EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);

	if (editor_state->sandboxes.size > 0) {
		EntitiesWholeWindowMenu(drawer, data);

		// Display the combo box for the sandbox selection
		drawer.SetCurrentPositionToHeader();

		UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
		row_layout.SetOffsetRenderRegion({ true, false });
		row_layout.AddSquareLabel();
		row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
		row_layout.CombineLastElements();

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
		row_layout.CombineLastElements();

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
				const EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);

				UIConfigLabelHierarchyFilter filter;
				filter.filter = data->filter_string;
				config.AddFlag(filter);

				size_t LABEL_HIERARCHY_CONFIGURATION = UI_CONFIG_LABEL_HIERARCHY_FILTER | UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK | UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK
					| UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS | UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL;

				UIConfigLabelHierarchySelectableCallback selectable;
				selectable.callback = SelectableCallback;
				selectable.data = data;
				selectable.data_size = 0;
				config.AddFlag(selectable);

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

				const EntityManager* entity_manager = ActiveEntityManager(editor_state, data->sandbox_index);

				const EntityHierarchy* hierarchy = &entity_manager->m_hierarchy;
				HierarchyIteratorImpl implementation(hierarchy, entity_manager, editor_state->editor_components.GetComponentID(STRING(Name)));

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
	EDITOR_STATE(editor_state);
	
	unsigned int last_entities_index = GetEntitiesUILastWindowIndex(editor_state);
	// This works even for the case when last entities is -1
	ECS_ASSERT(last_entities_index + 1 < MAX_ENTITIES_UI_WINDOWS);
	// This is fine even for -1, when it gets incremented then it will be 0.
	unsigned int window_index = CreateEntitiesUIWindow(editor_state, last_entities_index + 1);

	UIElementTransform window_transform = { ui_system->GetWindowPosition(window_index), ui_system->GetWindowScale(window_index) };
	ui_system->CreateDockspace(window_transform, DockspaceType::FloatingHorizontal, window_index, false);
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
	name.Copy(ENTITIES_UI_WINDOW_NAME);
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
