#include "editorpch.h"
#include "../Inspector.h"
#include "../../Editor/EditorState.h"
#include "../../Editor/EditorSandboxEntityOperations.h"
#include "InspectorUtilities.h"
#include "ECSEngineComponents.h"
#include "../../Modules/Module.h"

using namespace ECSEngine;
ECS_TOOLS;

#define INSPECTOR_DRAW_ENTITY_NAME_INPUT_CAPACITY (256)

// The name structure is embedded in the structure
struct InspectorDrawEntityData {
	struct MatchingInputs {
		Stream<char> component_name;
		Stream<CapacityStream<void>*> capacity_inputs;
	};

	// Keep the instance and its pointer. When the components shift around the archetype
	// the UIReflectionInstance will incorrectly keep pointing to the same place when in fact
	// the component was moved or the entity changed the archetype
	struct CreatedInstance {
		Stream<char> name;
		void* pointer_bound;
	};

	// If a component has a link component, we need to create it and have it live multiple frames
	// Cannot have it be a temporary because the UI requires it to be stable
	struct LinkComponent {
		Stream<char> name;
		void* data;
	};

	void AddCreatedInstance(EditorState* editor_state, Stream<char> name, void* pointer_bound) {
		name = function::StringCopy(editor_state->EditorAllocator(), name);
		void* old_buffer = created_instances.buffer;
		size_t previous_size = created_instances.size;

		void* allocation = editor_state->editor_allocator->Allocate(created_instances.MemoryOf(created_instances.size + 1));
		uintptr_t ptr = (uintptr_t)allocation;
		created_instances.InitializeAndCopy(ptr, created_instances);
		created_instances.Add({ name, pointer_bound });
		if (previous_size > 0) {
			editor_state->editor_allocator->Deallocate(old_buffer);
		}
	}

	// Resizes the matching inputs
	void AddComponent(EditorState* editor_state, Stream<char> component_name) {
		void* allocation = editor_state->editor_allocator->Allocate(matching_inputs.MemoryOf(matching_inputs.size + 1));
		uintptr_t ptr = (uintptr_t)allocation;

		if (matching_inputs.size > 0) {
			editor_state->editor_allocator->Deallocate(matching_inputs.buffer);
		}
		matching_inputs.InitializeAndCopy(ptr, matching_inputs);
		matching_inputs[matching_inputs.size].component_name = component_name;
		matching_inputs[matching_inputs.size].capacity_inputs = { nullptr, 0 };
		matching_inputs.size++;
	}

	// Assumes that the set function was called first
	CapacityStream<void>* AddComponentInput(EditorState* editor_state, Stream<char> component_name, size_t input_capacity, size_t element_byte_size) {
		unsigned int index = FindMatchingInput(component_name);
		ECS_ASSERT(index != -1);
		void* allocation = editor_state->editor_allocator->Allocate(input_capacity * element_byte_size + sizeof(CapacityStream<void>));
		CapacityStream<void>* new_input = (CapacityStream<void>*)allocation;
		uintptr_t ptr = (uintptr_t)allocation;
		ptr += sizeof(CapacityStream<void>);
		new_input->InitializeFromBuffer(ptr, 0, input_capacity);
		unsigned int insert_index = matching_inputs[index].capacity_inputs.Add(new_input);
		return matching_inputs[index].capacity_inputs[insert_index];
	}

	// Returns the index at which it can be found
	unsigned int AddLinkComponent(EditorState* editor_state, Stream<char> name) {
		// Add it to the matching inputs
		AddComponent(editor_state, name);

		void* allocation = editor_state->editor_allocator->Allocate(link_components.MemoryOf(link_components.size + 1));
		uintptr_t ptr = (uintptr_t)allocation;

		unsigned int write_index = link_components.size;

		if (link_components.size > 0) {
			editor_state->editor_allocator->Deallocate(link_components.buffer);
		}
		link_components.InitializeAndCopy(ptr, link_components);
		link_components[write_index].name = name;

		unsigned short byte_size = editor_state->editor_components.GetComponentByteSize(name);
		link_components[write_index].data = editor_state->editor_allocator->Allocate(byte_size);
		//editor_state->editor_components.ResetComponent(name, link_components[write_index].data);

		//// Extract the data from the entity manager and reverse it
		//// But first try to get the reverse function
		//ModuleLinkComponentTarget link_functions = GetModuleLinkComponentTarget(
		//	editor_state,
		//	editor_state->editor_components.FindComponentModuleInReflection(editor_state, name),
		//	name
		//);
		//bool needs_dll = editor_state->editor_components.GetLinkComponentDLLStatus(name);
		//if (needs_dll) {
		//	if (link_functions.build_function == nullptr) {
		//		// There is no build function but it requires a function. Send a warning
		//		ECS_FORMAT_TEMP_STRING(console_message, "Link Component {#} doesn't have a DLL ready for use.", name);
		//		EditorSetConsoleWarn(console_message);
		//	}
		//	else {
		//		// extract the target data and convert it into a link component
		//		ModuleLinkComponentReverseFunctionData reverse_data;
		//		reverse_data.component = target_data;
		//		reverse_data.link_component = link_components[write_index].data;
		//		
		//		link_functions.reverse_function(&reverse_data);
		//	}
		//}
		//else {
		//	// Just do the normal conversion
		//}

		link_components.size++;
		return write_index;
	}

	// Deallocates everything
	void Clear(EditorState* editor_state) {
		if (matching_inputs.size > 0) {
			for (size_t index = 0; index < matching_inputs.size; index++) {
				ClearComponent(editor_state, index);
			}
			editor_state->editor_allocator->Deallocate(matching_inputs.buffer);
		}
		if (created_instances.size > 0) {
			for (size_t index = 0; index < created_instances.size; index++) {
				if (editor_state->module_reflection->GetInstancePtr(created_instances[index].name) != nullptr) {
					// It is from the module side
					editor_state->module_reflection->DestroyInstance(created_instances[index].name);
				}
				else {
					// It must be from the engine side
					ECS_ASSERT(editor_state->ui_reflection->GetInstancePtr(created_instances[index].name) != nullptr);
					editor_state->ui_reflection->DestroyInstance(created_instances[index].name);
				}
				editor_state->editor_allocator->Deallocate(created_instances[index].name.buffer);
			}
			editor_state->editor_allocator->Deallocate(created_instances.buffer);
		}
		if (link_components.size > 0) {
			for (size_t index = 0; index < link_components.size; index++) {
				editor_state->editor_allocator->Deallocate(link_components[index].data);
			}
			editor_state->editor_allocator->Deallocate(link_components.buffer);
		}
	}

	void ClearComponent(EditorState* editor_state, unsigned int index) {
		for (size_t buffer_index = 0; buffer_index < matching_inputs[index].capacity_inputs.size; buffer_index++) {
			editor_state->editor_allocator->Deallocate(matching_inputs[index].capacity_inputs[buffer_index]);
			matching_inputs[index].capacity_inputs[buffer_index]->size = 0;
		}
		editor_state->editor_allocator->Deallocate(matching_inputs[index].capacity_inputs.buffer);
		matching_inputs[index].capacity_inputs.size = 0;
	}

	unsigned int FindMatchingInput(Stream<char> component_name) const {
		return function::FindString(component_name, matching_inputs, [](MatchingInputs input) { return input.component_name; });
	}

	unsigned int FindCreatedInstance(Stream<char> name) const {
		return function::FindString(name, created_instances, [](CreatedInstance instance) { return instance.name; });
	}

	unsigned int FindLinkComponent(Stream<char> name) const {
		return function::FindString(name, link_components, [](LinkComponent component) { return component.name; });
	}

	// Removes all the memory allocated for the capacity inputs of that component
	void RemoveComponent(EditorState* editor_state, Stream<char> component_name) {
		unsigned int index = FindMatchingInput(component_name);
		ECS_ASSERT(index != -1);
		for (size_t buffer_index = 0; buffer_index < matching_inputs[index].capacity_inputs.size; buffer_index++) {
			editor_state->editor_allocator->Deallocate(matching_inputs[index].capacity_inputs[buffer_index]);
		}
		matching_inputs.RemoveSwapBack(index);
	}

	void RemoveLinkComponent(EditorState* editor_state, Stream<char> name) {
		unsigned int index = FindLinkComponent(name);
		ECS_ASSERT(index != -1);
		editor_state->editor_allocator->Deallocate(link_components[index].data);
		link_components.RemoveSwapBack(index);

		// Also remove it from the matching inputs
		RemoveComponent(editor_state, name);
	}

	void SetComponentInputCount(EditorState* editor_state, Stream<char> component_name, size_t count) {
		unsigned int index = FindMatchingInput(component_name);
		ECS_ASSERT(index != -1);
		void* allocation = editor_state->editor_allocator->Allocate(matching_inputs[index].capacity_inputs.MemoryOf(count));
		matching_inputs[index].capacity_inputs.InitializeFromBuffer(allocation, 0);
	}

	Entity entity;
	CapacityStream<char> name_input;
	bool header_state[ECS_ARCHETYPE_MAX_COMPONENTS + ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
	// Used for text input, file input and directory input in order to set their capacity streams
	// Every component gets a resizable stream with capacity streams for their fields
	Stream<MatchingInputs> matching_inputs;

	Stream<LinkComponent> link_components;

	// Destroy these when changing to another entity such that when returning to it it won't crash
	Stream<CreatedInstance> created_instances;
};

void InspectorCleanEntity(EditorState* editor_state, void* _data) {
	InspectorDrawEntityData* data = (InspectorDrawEntityData*)_data;
	data->Clear(editor_state);
}

// ----------------------------------------------------------------------------------------------------------------------------

// This is what the click handlers will use
struct AddComponentCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	Stream<char> component_name;
	Entity entity;
};

void AddComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	AddComponentCallbackData* data = (AddComponentCallbackData*)_data;
	Stream<char> link_target = data->editor_state->editor_components.GetComponentFromLink(data->component_name);
	AddSandboxEntityComponentEx(data->editor_state, data->sandbox_index, data->entity, link_target.size > 0 ? link_target : data->component_name);
};

// ----------------------------------------------------------------------------------------------------------------------------

struct RemoveComponentCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	Entity entity;
	Stream<char> component_name;
};

void RemoveComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RemoveComponentCallbackData* data = (RemoveComponentCallbackData*)_data;
	RemoveSandboxEntityComponentEx(data->editor_state, data->sandbox_index, data->entity, data->component_name);
}

// ----------------------------------------------------------------------------------------------------------------------------

struct ResetComponentCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	Entity entity;
	Stream<char> component_name;
};

void ResetComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ResetComponentCallbackData* data = (ResetComponentCallbackData*)_data;
	ResetSandboxEntityComponent(data->editor_state, data->sandbox_index, data->entity, data->component_name);
}

// ----------------------------------------------------------------------------------------------------------------------------

struct InspectorComponentCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

void InspectorComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	InspectorComponentCallbackData* data = (InspectorComponentCallbackData*)_data;
	SetSandboxSceneDirty(data->editor_state, data->sandbox_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

#define HEADER_BUTTON_COUNT 2

// There must be HEADER_BUTTON_COUNT of buttons. Stack memory should be at least ECS_KB in size
void InspectorEntityHeaderConstructButtons(
	UIDrawer* drawer, 
	EditorState* editor_state, 
	unsigned int sandbox_index,
	Entity entity,
	Stream<char> component_name,
	UIConfigCollapsingHeaderButton* header_buttons, 
	void* stack_memory
) {
	ResetComponentCallbackData* reset_data = (ResetComponentCallbackData*)stack_memory;
	reset_data->component_name = component_name;
	reset_data->entity = entity;
	reset_data->sandbox_index = sandbox_index;
	reset_data->editor_state = editor_state;

	stack_memory = function::OffsetPointer(stack_memory, sizeof(*reset_data));
	RemoveComponentCallbackData* remove_data = (RemoveComponentCallbackData*)stack_memory;
	remove_data->component_name = component_name;
	remove_data->entity = entity;
	remove_data->sandbox_index = sandbox_index;
	remove_data->editor_state = editor_state;

	// The reset button
	header_buttons[0].alignment = ECS_UI_ALIGN_RIGHT;
	header_buttons[0].type = ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_BUTTON;
	header_buttons[0].data.image_texture = ECS_TOOLS_UI_TEXTURE_COG;
	header_buttons[0].data.image_color = drawer->color_theme.text;

	header_buttons[0].handler = { ResetComponentCallback, reset_data, sizeof(*reset_data) };
	
	// The X, delete component button
	header_buttons[1].alignment = ECS_UI_ALIGN_RIGHT;
	header_buttons[1].type = ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_BUTTON;
	header_buttons[1].data.image_texture = ECS_TOOLS_UI_TEXTURE_X;
	header_buttons[1].data.image_color = drawer->color_theme.text;

	header_buttons[1].handler = { RemoveComponentCallback, remove_data, sizeof(*remove_data) };
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawEntity(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	EDITOR_STATE(editor_state);

	unsigned int sandbox_index = GetInspectorTargetSandbox(editor_state, inspector_index);
	InspectorDrawEntityData* data = (InspectorDrawEntityData*)_data;

	bool is_initialized = data->name_input.buffer != nullptr;

	if (!is_initialized) {
		// The name input is embedded in the structure
		data->name_input.buffer = (char*)function::OffsetPointer(data, sizeof(*data));
	}

	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);

	// Check to see if the entity still exists - else revert to draw nothing
	if (!entity_manager->ExistsEntity(data->entity)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	Color icon_color = drawer->color_theme.theme;
	icon_color = RGBToHSV(icon_color);
	icon_color.value = function::ClampMin(icon_color.value, (unsigned char)200);
	icon_color = HSVToRGB(icon_color);
	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_MESH, icon_color);

	Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
	Name* name = (Name*)entity_manager->TryGetComponent(data->entity, name_component);

	ECS_STACK_CAPACITY_STREAM(char, base_entity_name, 256);
	EntityToString(data->entity, base_entity_name, true);

	UIDrawConfig config;
	if (name != nullptr) {
		UIConfigRelativeTransform transform;
		transform.scale.x = 2.0f;
		config.AddFlag(transform);

		struct CallbackData {
			EditorState* editor_state;
			unsigned int sandbox_index;
			InspectorDrawEntityData* data;
		};

		auto name_input_callback = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			CallbackData* data = (CallbackData*)_data;
			ChangeEntityName(data->editor_state, data->sandbox_index, data->data->entity, data->data->name_input);
		};

		CallbackData callback_data;
		callback_data.editor_state = editor_state;
		callback_data.sandbox_index = sandbox_index;
		callback_data.data = data;
		UIConfigTextInputCallback input_callback = { { name_input_callback, &callback_data, sizeof(callback_data) } };
		config.AddFlag(input_callback);

		// Cannot use a constant name because it will be transported to other entities when changing to them
		// without changing the buffer being pointed
		UIDrawerTextInput* text_input = drawer->TextInput(
			UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_TEXT_INPUT_CALLBACK | UI_CONFIG_ALIGN_TO_ROW_Y,
			config,
			base_entity_name,
			&data->name_input
		);
		if (!text_input->is_currently_selected) {
			if (!function::CompareStrings(name->name, *text_input->text)) {
				text_input->text->Copy(name->name);
			}
		}
		drawer->NextRow();

		UIConfigAlignElement align_element;
		align_element.horizontal = ECS_UI_ALIGN_MIDDLE;
		config.AddFlag(align_element);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_ALIGN_ELEMENT, config, base_entity_name);
		config.flag_count = 0;
	}
	else {
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, base_entity_name);
	}

	drawer->NextRow();

	ComponentSignature unique_signature = entity_manager->GetEntitySignatureStable(data->entity);
	SharedComponentSignature shared_signature = entity_manager->GetEntitySharedSignatureStable(data->entity);

	UIConfigNamePadding name_padding;
	name_padding.alignment = ECS_UI_ALIGN_LEFT;
	name_padding.total_length = 0.15f;
	config.AddFlag(name_padding);

	UIConfigWindowDependentSize window_dependent_size;
	config.AddFlag(window_dependent_size);

	ECS_STACK_CAPACITY_STREAM(char, instance_name, 256);

	auto get_unique_data = [&](size_t index) {
		return entity_manager->GetComponentWithIndex(data->entity, index);
	};

	auto get_shared_data = [&](size_t index) {
		return entity_manager->GetSharedData(shared_signature.indices[index], shared_signature.instances[index]);
	};

	auto draw_component = [&](ComponentSignature signature, bool shared, unsigned int header_state_offset, auto get_current_data) {
		UIReflectionDrawConfig ui_draw_configs[(unsigned int)UIReflectionIndex::Count];
		memset(ui_draw_configs, 0, sizeof(ui_draw_configs));

		void* ui_draw_configs_stack_memory = ECS_STACK_ALLOC(ECS_KB);

		InspectorComponentCallbackData change_component_data;
		change_component_data.editor_state = editor_state;
		change_component_data.sandbox_index = sandbox_index;
		UIActionHandler modify_value_handler = { InspectorComponentCallback, &change_component_data, sizeof(change_component_data) };
		size_t written_configs = UIReflectionDrawConfigSplatCallback(
			{ ui_draw_configs, std::size(ui_draw_configs) }, 
			modify_value_handler, 
			ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ALL,
			ui_draw_configs_stack_memory
		);

		Stream<UIReflectionDrawConfig> valid_ui_draw_configs = { ui_draw_configs, written_configs };

		for (size_t index = 0; index < signature.count; index++) {
			// Verify that the instance exists because it might have been destroyed in the meantime
			instance_name.size = 0;
			Stream<char> current_component_name = editor_state->editor_components.TypeFromID(signature[index].value, shared);
			ECS_ASSERT(current_component_name.size > 0);
			Stream<char> original_component_name = current_component_name;

			// Check to see if the component has a link to it
			Stream<char> link_component = editor_state->editor_components.GetLinkComponentForTarget(current_component_name);
			void* current_component = nullptr;
			if (link_component.size > 0) {
				// Replace the name with the link name
				current_component_name = link_component;
			}
			else {
				// Get the data from the entity manager if not a link component
				current_component = get_current_data(index);
			}

			instance_name.Copy(current_component_name);
			instance_name.AddStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
			instance_name.AddStream(base_entity_name);
			function::ConvertIntToChars(instance_name, sandbox_index);

			// Check to see whether or not the component is from the engine side or from the module side
			UIReflectionDrawer* ui_drawer = editor_state->module_reflection;
			UIReflectionType* type = editor_state->ui_reflection->GetTypePtr(current_component_name);
			if (type != nullptr) {
				// Engine side
				ui_drawer = editor_state->ui_reflection;
			}
			// Else module side

			UIReflectionInstance* instance = ui_drawer->GetInstancePtr(instance_name);

			auto set_instance_inputs = [&]() {
				// Check to see if it has any buffers and bind their allocators
				AllocatorPolymorphic component_allocator = shared ? entity_manager->GetSharedComponentAllocatorPolymorphic(signature[index]) :
					entity_manager->GetComponentAllocatorPolymorphic(signature[index]);
				if (component_allocator.allocator != nullptr) {
					unsigned int exists_component = data->FindMatchingInput(current_component_name);
					if (exists_component != -1) {
						data->ClearComponent(editor_state, exists_component);
					}
					else {
						if (link_component.size > 0) {
							data->AddLinkComponent(editor_state, link_component);
						}
						else {
							data->AddComponent(editor_state, current_component_name);
						}
					}

					unsigned int capacity_stream_index = data->FindMatchingInput(current_component_name);

					ui_drawer->AssignInstanceResizableAllocator(instance, component_allocator, false);
					// For every text input, directory input or file input, allocate a separate buffer
					// such that we don't use the component allocator to hold that data
					UIReflectionType ui_type = ui_drawer->GetType(instance->type_name);

					ECS_STACK_CAPACITY_STREAM(unsigned int, input_indices, 64);
					ui_drawer->GetTypeMatchingFields(ui_type, UIReflectionIndex::TextInput, input_indices);
					unsigned int text_input_count = input_indices.size;
					ui_drawer->GetTypeMatchingFields(ui_type, UIReflectionIndex::DirectoryInput, input_indices);
					unsigned int directory_input_count = input_indices.size - text_input_count;
					ui_drawer->GetTypeMatchingFields(ui_type, UIReflectionIndex::FileInput, input_indices);
					unsigned int file_input_count = input_indices.size - text_input_count - directory_input_count;
					data->SetComponentInputCount(editor_state, current_component_name, input_indices.size);

					auto bind_text_input = [&](unsigned int index, CapacityStream<void>* capacity_stream) {
						ui_drawer->BindInstanceTextInput(instance, { &index, 1 }, (CapacityStream<char>*)capacity_stream);
					};

					auto bind_directory_input = [&](unsigned int index, CapacityStream<void>* capacity_stream) {
						ui_drawer->BindInstanceDirectoryInput(instance, { &index, 1 }, (CapacityStream<wchar_t>*)capacity_stream);
					};

					auto bind_file_input = [&](unsigned int index, CapacityStream<void>* capacity_stream) {
						ui_drawer->BindInstanceFileInput(instance, { &index, 1 }, (CapacityStream<wchar_t>*)capacity_stream);
					};

					auto create_input_stream = [&](unsigned int count, unsigned int base_offset, size_t element_size, auto bind_function) {
						for (unsigned int index = 0; index < count; index++) {
							CapacityStream<void>* capacity_stream = data->AddComponentInput(editor_state, current_component_name, 256, element_size);
							CapacityStream<void> current_stream = ui_drawer->GetInputStream(instance, input_indices[index + base_offset]);

							unsigned int copy_size = current_stream.size <= capacity_stream->capacity ? current_stream.size : capacity_stream->capacity;
							if (copy_size > 0) {
								memcpy(capacity_stream->buffer, current_stream.buffer, current_stream.size * element_size);
								capacity_stream->size = copy_size;
							}
							bind_function(index + base_offset, capacity_stream);
						}
					};

					create_input_stream(text_input_count, 0, sizeof(char), bind_text_input);
					create_input_stream(directory_input_count, text_input_count, sizeof(wchar_t), bind_directory_input);
					create_input_stream(file_input_count, text_input_count + directory_input_count, sizeof(wchar_t), bind_file_input);
				}
			};

			if (instance == nullptr) {
				instance = ui_drawer->CreateInstance(instance_name, current_component_name);
				// Check to see if the created instance already exists. It might happen that if the
				// component is updated the instance will become invalidated but it won't be removed
				// from the created instance stream resulting in a duplicate
				unsigned int created_instance_index = data->FindCreatedInstance(instance_name);
				if (created_instance_index == -1) {
					data->AddCreatedInstance(editor_state, instance_name, current_component);
				}
				else {
					// If it exists, only set the pointer to the same value
					data->created_instances[created_instance_index].pointer_bound = current_component;
				}

				ui_drawer->BindInstancePtrs(instance, current_component);
				set_instance_inputs();
			}

			unsigned int instance_index = data->FindCreatedInstance(instance->name);
			ECS_ASSERT(instance_index != -1);
			if (data->created_instances[instance_index].pointer_bound != current_component) {
				data->created_instances[instance_index].pointer_bound = current_component;
				ui_drawer->RebindInstancePtrs(instance, current_component);
				set_instance_inputs();
			}

			void* button_stack_allocation = ECS_STACK_ALLOC(ECS_KB);
			UIConfigCollapsingHeaderButton header_buttons[HEADER_BUTTON_COUNT];
			InspectorEntityHeaderConstructButtons(
				drawer,
				editor_state,
				sandbox_index,
				data->entity,
				current_component_name,
				header_buttons,
				button_stack_allocation
			);

			UIDrawConfig collapsing_config;
			UIConfigCollapsingHeaderButtons config_buttons;
			config_buttons.buttons = { header_buttons, HEADER_BUTTON_COUNT };
			collapsing_config.AddFlag(config_buttons);

			drawer->CollapsingHeader(UI_CONFIG_COLLAPSING_HEADER_BUTTONS, collapsing_config, instance->name, data->header_state + index + header_state_offset, [&]() {
				UIReflectionDrawInstanceOptions options;
				options.drawer = drawer;
				options.config = &config;
				options.global_configuration = UI_CONFIG_NAME_PADDING | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_WINDOW_DEPENDENT_SIZE;
				options.additional_configs = valid_ui_draw_configs;
				ui_drawer->DrawInstance(instance, &options);
			});
		}
	};

	// Now draw the entity using the reflection drawer
	draw_component(unique_signature, false, 0, get_unique_data);
	draw_component({ shared_signature.indices, shared_signature.count }, true, unique_signature.count, get_shared_data);

	config.flag_count = 0;

	unsigned int module_count = editor_state->editor_components.loaded_modules.size;
	bool* is_submenu = (bool*)ECS_STACK_ALLOC(sizeof(bool) * module_count);
	memset(is_submenu, 1, sizeof(bool) * module_count);

	UIDrawerMenuState* submenues = (UIDrawerMenuState*)ECS_STACK_ALLOC(sizeof(UIDrawerMenuState) * module_count);
	// These won't be used, but still need to be provided with data size 0
	UIActionHandler* dummy_handlers = (UIActionHandler*)ECS_STACK_ALLOC(sizeof(UIActionHandler) * module_count);

	UIDrawerMenuState add_menu_state;
	add_menu_state.row_count = 0;
	add_menu_state.row_has_submenu = is_submenu;
	add_menu_state.submenues = submenues;
	add_menu_state.submenu_index = 0;
	add_menu_state.click_handlers = dummy_handlers;

	ECS_STACK_CAPACITY_STREAM(char, add_menu_state_characters, 512);

	for (unsigned int index = 0; index < module_count; index++) {
		ECS_STACK_CAPACITY_STREAM(unsigned int, component_indices, 256);
		ECS_STACK_CAPACITY_STREAM(unsigned int, shared_indices, 256);

		editor_state->editor_components.GetModuleComponentIndices(index, &component_indices, true);
		editor_state->editor_components.GetModuleSharedComponentIndices(index, &shared_indices, true);

		ECS_STACK_CAPACITY_STREAM(unsigned int, valid_component_indices, 256);
		ECS_STACK_CAPACITY_STREAM(unsigned int, valid_shared_indices, 256);

		auto has_unique_component = [&](Component id) {
			return entity_manager->HasComponent(data->entity, id);
		};

		auto has_shared_component = [&](Component id) {
			return entity_manager->HasSharedComponent(data->entity, id);
		};

		auto get_valid_indices = [&](CapacityStream<unsigned int> component_indices, CapacityStream<unsigned int>& valid_indices, auto has_component) {
			for (unsigned int component_index = 0; component_index < component_indices.size; component_index++) {
				Stream<char> component_name = editor_state->editor_components.loaded_modules[index].types[component_indices[component_index]];
				Component id = editor_state->editor_components.GetComponentIDWithLink(component_name);
				ECS_ASSERT(id.value != -1);
				if (!has_component(id)) {
					valid_indices.Add(component_indices[component_index]);
				}
			}
		};

		get_valid_indices(component_indices, valid_component_indices, has_unique_component);
		get_valid_indices(shared_indices, valid_shared_indices, has_shared_component);

		// If the module doesn't have any more valid components left, eliminate it
		if (valid_component_indices.size == 0 && valid_shared_indices.size == 0) {
			continue;
		}

		// Increment the row count as we go
		unsigned int valid_module_index = add_menu_state.row_count;

		add_menu_state_characters.AddStream(editor_state->editor_components.loaded_modules[index].name);
		add_menu_state_characters.Add('\n');
		dummy_handlers[valid_module_index].data_size = 0;

		component_indices = valid_component_indices;
		shared_indices = valid_shared_indices;

		unsigned int total_component_count = component_indices.size + shared_indices.size;

		// The ECS_STACK_ALLOC references are going to be valid outside the scope of the for because _alloca is deallocated on function exit
		UIActionHandler* handlers = (UIActionHandler*)ECS_STACK_ALLOC(sizeof(UIActionHandler) * total_component_count);
		AddComponentCallbackData* handler_data = (AddComponentCallbackData*)ECS_STACK_ALLOC(sizeof(AddComponentCallbackData) * total_component_count);
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
					component_name = Reflection::GetReflectionTypeLinkNamePretty(component_name);
				}

				left_characters.AddStream(component_name);
				left_characters.AddSafe('\n');
				handler_data[subindex + write_offset] = { editor_state, sandbox_index, initial_name, data->entity };
				handlers[subindex + write_offset] = { AddComponentCallback, handler_data + subindex + write_offset, sizeof(AddComponentCallbackData), ECS_UI_DRAW_NORMAL };
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
		bool can_have_unique = unique_signature.count < ECS_ARCHETYPE_MAX_COMPONENTS;
		bool can_have_shared = shared_signature.count < ECS_ARCHETYPE_MAX_SHARED_COMPONENTS;
		memset(is_unavailable, !can_have_unique, sizeof(bool) * component_indices.size);
		memset(is_unavailable + component_indices.size, !can_have_shared, sizeof(bool) * shared_indices.size);
		submenues[valid_module_index].unavailables = is_unavailable;

		add_menu_state.row_count++;
	}
	// Remove the last '\n'
	add_menu_state_characters.size--;
	add_menu_state.left_characters = add_menu_state_characters;

	config.flag_count = 0;
	UIConfigAlignElement align_element;
	align_element.horizontal = ECS_UI_ALIGN_MIDDLE;
	config.AddFlag(align_element);

	UIConfigActiveState active_state;
	active_state.state = add_menu_state.row_count > 0;
	config.AddFlag(active_state);
	drawer->Menu(UI_CONFIG_ALIGN_ELEMENT | UI_CONFIG_ACTIVE_STATE | UI_CONFIG_MENU_COPY_STATES, config, "Add Component", &add_menu_state);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int inspector_index)
{
	size_t _draw_data[128];

	InspectorDrawEntityData* draw_data = (InspectorDrawEntityData*)_draw_data;
	draw_data->entity = entity;
	draw_data->name_input.buffer = nullptr;
	draw_data->name_input.size = 0;
	draw_data->name_input.capacity = INSPECTOR_DRAW_ENTITY_NAME_INPUT_CAPACITY;
	draw_data->matching_inputs.size = 0;
	draw_data->created_instances.size = 0;
	draw_data->link_components.size = 0;

	memset(draw_data->header_state, 1, sizeof(bool) * (ECS_ARCHETYPE_MAX_COMPONENTS + ECS_ARCHETYPE_MAX_SHARED_COMPONENTS));

	ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawEntity, InspectorCleanEntity },
		draw_data,
		sizeof(*draw_data) + INSPECTOR_DRAW_ENTITY_NAME_INPUT_CAPACITY
	);
}

// ----------------------------------------------------------------------------------------------------------------------------
