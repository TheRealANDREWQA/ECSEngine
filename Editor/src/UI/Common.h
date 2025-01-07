#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineUI.h"
#include "ECSEngineLinkComponents.h"

using namespace ECSEngine;
ECS_TOOLS;

struct EditorState;

// Destroys all windows with an index greater or equal than the given value until a threshold
void DestroyIndexedWindows(EditorState* editor_state, Stream<char> window_root, unsigned int starting_value, unsigned int max_count);

// The functor must have as functions the following
// 
// Returns true if the component should appear in the menu
// bool IsUniqueEnabled(Component component) const; 
// 
// Returns true if the component should appear in the menu
// bool IsSharedEnabled(Component component) const;
// 
// Fills the data for a click handler
// void Fill(void* data, Stream<char> component_name); 
// 
// Return true if the unique entries should be made unavailable
// bool LimitUnique() const;
// 
// Return true if the shared entries should be made unavailable
// bool LimitShared() const;
template<typename Functor>
void DrawChooseComponentMenu(
	const EditorState* editor_state,
	UIDrawer& drawer,
	size_t configuration,
	UIDrawConfig& config,
	Stream<char> menu_name,
	UIActionHandler action_handler,
	Functor&& functor
) {
	unsigned int module_count = editor_state->editor_components.loaded_modules.size;
	bool* is_submenu = (bool*)ECS_STACK_ALLOC(sizeof(bool) * module_count);
	memset(is_submenu, true, sizeof(bool) * module_count);

	UIDrawerMenuState* submenues = (UIDrawerMenuState*)ECS_STACK_ALLOC(sizeof(UIDrawerMenuState) * module_count);
	// These won't be used, but still need to be provided with data size 0
	UIActionHandler* dummy_handlers = (UIActionHandler*)ECS_STACK_ALLOC(sizeof(UIActionHandler) * module_count);

	UIDrawerMenuState menu_state;
	menu_state.row_count = 0;
	menu_state.row_has_submenu = is_submenu;
	menu_state.submenues = submenues;
	menu_state.submenu_index = 0;
	menu_state.click_handlers = dummy_handlers;

	ECS_STACK_CAPACITY_STREAM(char, menu_state_characters, 512);

	for (unsigned int index = 0; index < module_count; index++) {
		ECS_STACK_CAPACITY_STREAM(unsigned int, component_indices, 256);
		ECS_STACK_CAPACITY_STREAM(unsigned int, shared_indices, 256);

		editor_state->editor_components.GetModuleComponentIndices(index, &component_indices, true);
		editor_state->editor_components.GetModuleSharedComponentIndices(index, &shared_indices, true);

		ECS_STACK_CAPACITY_STREAM(unsigned int, valid_component_indices, 256);
		ECS_STACK_CAPACITY_STREAM(unsigned int, valid_shared_indices, 256);

		auto is_unique_enabled_component = [&](Component component) {
			return functor.IsUniqueEnabled(component);
		};

		auto is_shared_enabled_component = [&](Component component) {
			return functor.IsSharedEnabled(component);
		};

		auto get_valid_indices = [&](CapacityStream<unsigned int> component_indices, CapacityStream<unsigned int>& valid_indices, auto is_enabled) {
			for (unsigned int component_index = 0; component_index < component_indices.size; component_index++) {
				Stream<char> component_name = editor_state->editor_components.loaded_modules[index].types[component_indices[component_index]];
				Stream<char> target_component = editor_state->editor_components.GetComponentFromLink(component_name);
				if (target_component.size > 0) {
					component_name = target_component;
				}
				Component id = editor_state->editor_components.GetComponentIDWithLink(component_name);
				ECS_ASSERT(id.value != -1);
				// Check to see if the reflection type is omitted or not from the UI
				const Reflection::ReflectionType* reflection_type = editor_state->GlobalReflectionManager()->GetType(component_name);
				if (!IsUIReflectionTypeOmitted(reflection_type)) {
					if (is_enabled(id)) {
						valid_indices.Add(component_indices[component_index]);
					}
				}
			}
		};

		get_valid_indices(component_indices, valid_component_indices, is_unique_enabled_component);
		get_valid_indices(shared_indices, valid_shared_indices, is_shared_enabled_component);

		// If the module doesn't have any more valid components left, eliminate it
		if (valid_component_indices.size == 0 && valid_shared_indices.size == 0) {
			continue;
		}

		// Increment the row count as we go
		unsigned int valid_module_index = menu_state.row_count;

		menu_state_characters.AddStream(editor_state->editor_components.loaded_modules[index].name);
		menu_state_characters.Add('\n');
		dummy_handlers[valid_module_index].data_size = 0;

		component_indices = valid_component_indices;
		shared_indices = valid_shared_indices;

		unsigned int total_component_count = component_indices.size + shared_indices.size;

		// The ECS_STACK_ALLOC references are going to be valid outside the scope of the for because _alloca is deallocated on function exit
		UIActionHandler* handlers = (UIActionHandler*)ECS_STACK_ALLOC(sizeof(UIActionHandler) * total_component_count);
		void* handler_data = ECS_STACK_ALLOC(action_handler.data_size * total_component_count);
		submenues[valid_module_index].click_handlers = handlers;
		submenues[valid_module_index].right_characters = { nullptr, 0 };
		submenues[valid_module_index].row_has_submenu = nullptr;
		submenues[valid_module_index].submenues = nullptr;
		submenues[valid_module_index].separation_line_count = 0;

		void* _left_character_allocation = ECS_STACK_ALLOC(sizeof(char) * 512);
		CapacityStream<char> left_characters;
		left_characters.InitializeFromBuffer(_left_character_allocation, 0, 512);

		auto write_menu = [&](size_t module_index, Stream<unsigned int> indices, unsigned int write_offset) {
			for (unsigned int subindex = 0; subindex < indices.size; subindex++) {
				Stream<char> component_name = editor_state->editor_components.loaded_modules[module_index].types[indices[subindex]];
				Stream<char> initial_name = component_name;
				// If it is a link component, prettify it
				if (editor_state->editor_components.IsLinkComponent(component_name)) {
					component_name = GetReflectionTypeLinkNameBase(component_name);
				}

				left_characters.AddStream(component_name);
				left_characters.AddAssert('\n');
				void* current_handler_data = action_handler.data;
				if (action_handler.data_size > 0) {
					current_handler_data = OffsetPointer(handler_data, action_handler.data_size * (subindex + write_offset));
					functor.Fill(current_handler_data, initial_name);
				}
				handlers[subindex + write_offset] = { 
					action_handler.action,
					current_handler_data, 
					action_handler.data_size, 
					action_handler.phase 
				};
			}
		};

		write_menu(index, component_indices, 0);
		write_menu(index, shared_indices, component_indices.size);

		// Remove the last '\n'
		left_characters.size--;

		submenues[valid_module_index].left_characters = left_characters;
		submenues[valid_module_index].row_count = total_component_count;
		if (component_indices.size > 0 && shared_indices.size) {
			submenues[valid_module_index].separation_lines[0] = component_indices.size;
			submenues[valid_module_index].separation_line_count = 1;
		}
		submenues[valid_module_index].submenu_index = 1;

		bool* is_unavailable = (bool*)ECS_STACK_ALLOC(sizeof(bool) * total_component_count);
		bool can_have_unique = !functor.LimitUnique();
		bool can_have_shared = !functor.LimitShared();
		memset(is_unavailable, !can_have_unique, sizeof(bool) * component_indices.size);
		memset(is_unavailable + component_indices.size, !can_have_shared, sizeof(bool) * shared_indices.size);
		submenues[valid_module_index].unavailables = is_unavailable;

		menu_state.row_count++;
	}
	// Remove the last '\n'
	if (menu_state_characters.size > 0) {
		menu_state_characters.size--;
	}
	menu_state.left_characters = menu_state_characters;

	UIConfigActiveState active_state;
	active_state.state = menu_state.row_count > 0;
	config.AddFlag(active_state);
	drawer.Menu(configuration | UI_CONFIG_ACTIVE_STATE | UI_CONFIG_MENU_COPY_STATES, config, menu_name, &menu_state);
}