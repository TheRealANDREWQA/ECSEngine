#include "editorpch.h"
#include "EntitiesUI.h"
#include "Editor/EditorState.h"

#include "Editor/EditorSandboxEntityOperations.h"
#include "Inspector.h"

using namespace ECSEngine;
ECS_TOOLS;

struct EntitiesUIData {
	EditorState* editor_state;
	unsigned char sandbox_index;
	unsigned char hierarchy_index;
	CapacityStream<char> filter_string;
};

const size_t ITERATOR_LABEL_CAPACITY = 128;

// Special UI iterator for the entities
struct HierarchyIteratorImpl {
	HierarchyIteratorImpl() {}

	HierarchyIteratorImpl(EntityHierarchy* hierarchy) {
		Initialize(hierarchy);
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
		EntityToString(entity, entity_label);

		return entity_label;
	}

	void Initialize(EntityHierarchy* hierarchy) {
		implementation = { hierarchy };
		// Allocate the buffer here
		entity_label.Initialize(hierarchy->allocator, 0, ITERATOR_LABEL_CAPACITY);
	};

	void Free() {
		implementation.hierarchy->allocator->Deallocate(entity_label.buffer);
	}

	EntityHierarchyIteratorImpl implementation;
	CapacityStream<char> entity_label;
};

typedef DFSIterator<HierarchyIteratorImpl> DFSUIHierarchy;

struct AllEntitiesIteratorImpl {
	using storage_type = Entity;

	using return_type = Stream<char>;

	Stream<storage_type> GetChildren(storage_type value, AllocatorPolymorphic allocator) {
		return { nullptr, 0 };
	}

	bool HasChildren(storage_type value) const {
		return false;
	}

	Stream<storage_type> GetRoots(AllocatorPolymorphic allocator) {
		// Need to return all entities
		unsigned int entities_count = entity_pool->GetCount();
		Stream<storage_type> entities;
		entities.Initialize(allocator, entities_count);
		entities.size = 0;

		entity_pool->ForEach([&](Entity entity, EntityInfo info) {
			entities.Add(entity);
		});

		return entities;
	}

	return_type GetReturnValue(storage_type value, AllocatorPolymorphic allocator) {
		Entity entity = value;
		entity_label.size = 0;
		EntityToString(entity, entity_label);

		return entity_label;
	}

	void Initialize(EntityPool* pool) {
		entity_pool = pool;
		// Allocate the buffer here
		entity_label.Initialize(pool->m_memory_manager, 0, ITERATOR_LABEL_CAPACITY);
	};

	void Free() {
		entity_pool->m_memory_manager->Deallocate(entity_label.buffer);
	}

	EntityPool* entity_pool;
	CapacityStream<char> entity_label;
};

typedef DFSIterator<AllEntitiesIteratorImpl> DFSUIAllEntities;

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

// -------------------------------------------------------------------------------------------------------------

void SelectableCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchySelectableData* select_data = (UIDrawerLabelHierarchySelectableData*)_data;
	EntitiesUIData* data = (EntitiesUIData*)select_data->data;
	
	if (select_data->labels.size == 1) {
		Entity entity = StringToEntity(select_data->labels[0]);
		ChangeInspectorToEntity(data->editor_state, data->sandbox_index, entity);
	}
}

// -------------------------------------------------------------------------------------------------------------

// Placeholder at the moment
void DoubleClickCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyDoubleClickData* double_data = (UIDrawerLabelHierarchyDoubleClickData*)_data;
	
}

// -------------------------------------------------------------------------------------------------------------

struct RightClickHandlerData {
	Entity entity;
	EditorState* editor_state;
	unsigned int sandbox_index;
};

void RightClickCopy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	CopySandboxEntity(data->editor_state, data->sandbox_index, data->entity);
	PinWindowVerticalSliderPosition(system, system->GetWindowIndexFromBorder(dockspace, border_index));
}

// -------------------------------------------------------------------------------------------------------------

void RightClickDelete(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RightClickHandlerData* data = (RightClickHandlerData*)_data;
	DeleteSandboxEntity(data->editor_state, data->sandbox_index, data->entity);
}

// -------------------------------------------------------------------------------------------------------------

void RightClickCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UIDrawerLabelHierarchyRightClickData* right_click_data = (UIDrawerLabelHierarchyRightClickData*)_data;
	EntitiesUIData* ui_data = (EntitiesUIData*)right_click_data->data;

	const size_t ROW_COUNT = 3;

	enum HANDLER_INDEX {
		COPY,
		DELETE_
	};

	UIActionHandler handlers[ROW_COUNT];

	RightClickHandlerData handler_data;
	handler_data.editor_state = ui_data->editor_state;
	handler_data.entity = StringToEntity(right_click_data->GetLabel());
	handler_data.sandbox_index = ui_data->sandbox_index;
	handlers[COPY] = { RightClickCopy, &handler_data, sizeof(handler_data) };
	handlers[DELETE_] = { RightClickDelete, &handler_data, sizeof(handler_data) };

	UIDrawerMenuState menu_state;
	menu_state.left_characters = "Copy\nDelete";
	menu_state.row_count = ROW_COUNT;
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
	data->hierarchy_index = 0;

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

		size_t HEADER_CONFIGURATION = UI_CONFIG_BORDER | UI_CONFIG_DO_NOT_UPDATE_RENDER_BOUNDS | UI_CONFIG_DO_NOT_VALIDATE_POSITION;

		UIConfigBorder border;
		border.is_interior = true;
		border.color = drawer.color_theme.borders;

		config.AddFlag(border);

		UIConfigComboBoxPrefix prefix;
		prefix.prefix = "Sandbox: ";
		config.AddFlag(prefix);

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

		drawer.ComboBox(combo_configuration, config, "Sandbox Combo", combo_labels, combo_labels.size, &data->sandbox_index);
		drawer.Indent(-1.0f);

		UIConfigWindowDependentSize dependent_size;
		dependent_size.scale_factor.x = drawer.GetWindowSizeScaleUntilBorder(true).x;
		config.AddFlag(dependent_size);

		UIConfigTextInputHint hint;
		hint.characters = "Search";
		config.AddFlag(hint);

		size_t text_input_configuration = HEADER_CONFIGURATION | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_NO_NAME;
		drawer.TextInput(text_input_configuration, config, "Search filter", &data->filter_string);

		drawer.NextRow();

		if (!initialize) {
			// Display what hierarchy we are drawing
			ECS_STACK_CAPACITY_STREAM(Stream<char>, hierarchy_labels, 16);
			ECS_STACK_CAPACITY_STREAM(char, temp_characters, 512);

			// The first "Hierarchy" is the none one. The entities are displayed as is
			hierarchy_labels.Add("None");

			EntityManager* entity_manager = &sandbox->scene_entities;
			for (size_t index = 0; index < ECS_ENTITY_HIERARCHY_MAX_COUNT; index++) {
				if (entity_manager->ExistsHierarchy(index)) {
					Stream<char> name = entity_manager->GetHierarchyName(index);
					if (name.size > 0) {
						// Use this name
						hierarchy_labels.Add(name);
					}
					else {
						// Create a default name for it
						size_t current_size = temp_characters.size;
						const char* buffer = temp_characters.buffer;
						temp_characters.AddStream("Hierarchy ");
						function::ConvertIntToChars(temp_characters, index);

						hierarchy_labels.Add({ buffer, temp_characters.size - current_size });
					}
				}
			}

			config.flag_count = 0;
			UIConfigWindowDependentSize dependent_size;
			config.AddFlag(dependent_size);
			drawer.ComboBox(UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Select Hierarchy", hierarchy_labels, hierarchy_labels.size, &data->hierarchy_index);
			drawer.NextRow();
			config.flag_count = 0;

			// Now draw the hierarchy, if there is a sandbox at all
			if (sandbox_count > 0) {
				const EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);

				UIConfigLabelHierarchyFilter filter;
				filter.filter = data->filter_string;
				config.AddFlag(filter);

				size_t LABEL_HIERARCHY_CONFIGURATION = UI_CONFIG_LABEL_HIERARCHY_FILTER | UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK;

				UIConfigLabelHierarchySelectableCallback selectable;
				selectable.callback = SelectableCallback;
				selectable.data = data;
				selectable.data_size = 0;
				config.AddFlag(selectable);

				// Draw the entities. No hierarchy
				if (data->hierarchy_index == 0) {
					EntityPool* pool = entity_manager->m_entity_pool;
					AllEntitiesIteratorImpl implementation;
					implementation.Initialize(pool);

					unsigned int pool_count = pool->GetCount();
					DFSUIAllEntities iterator(GetAllocatorPolymorphic(pool->m_memory_manager), implementation, pool_count);

					drawer.LabelHierarchy(LABEL_HIERARCHY_CONFIGURATION, config, hierarchy_labels[data->hierarchy_index], iterator);
					iterator.Free();
				}
				else {
					EntityHierarchy* hierarchy = entity_manager->GetHierarchy(data->hierarchy_index - 1);
					HierarchyIteratorImpl implementation(hierarchy);
					DFSUIHierarchy iterator(GetAllocatorPolymorphic(hierarchy->allocator), implementation, hierarchy->children_table.GetCount());

					LABEL_HIERARCHY_CONFIGURATION |= UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL | UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK;
					drawer.LabelHierarchy(LABEL_HIERARCHY_CONFIGURATION, config, hierarchy_labels[data->hierarchy_index], iterator);
					iterator.Free();
				}
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
