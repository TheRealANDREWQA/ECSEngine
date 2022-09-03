#include "editorpch.h"
#include "EditorScene.h"
#include "EditorState.h"

#include "ECSEngineEntitiesSerialize.h"

using namespace ECSEngine;

#pragma region Internal functions



// ----------------------------------------------------------------------------------------------

SerializeEntityManagerComponentTable SerializeComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator
) {
	SerializeEntityManagerComponentTable table;

	ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerComponentInfo, overrides, 256);

	// Walk through the module reflection and retrieve their overrides
	ProjectModules project_modules = *editor_state->project_modules;
	for	(size_t index = 0; index < project_modules.size; index++) {
		// If out of date or good
		// Walk through the configurations and get those which are GOOD and then those that are OUT_OF_DATE
		// afterwards
		size_t configuration_index = 0;
		for (; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
			if (project_modules[index].infos[EDITOR_MODULE_CONFIGURATION_COUNT - 1 - index].load_status == EDITOR_MODULE_LOAD_GOOD) {
				break;
			}
		}

		if (configuration_index != EDITOR_MODULE_CONFIGURATION_COUNT) {
			// We have a good module. Reverse the index first
			configuration_index = EDITOR_MODULE_CONFIGURATION_COUNT - 1 - index;
			auto component_overrides = project_modules[index].infos[configuration_index].serialize_streams.serialize_components;
			for (size_t subindex = 0; subindex < component_overrides.size; subindex++) {
				//component_overrides
			}
			
		}
	}

	CreateSerializeEntityManagerComponentTable(table, editor_state->ui_reflection->reflection, allocator, overrides);

	return table;
}

// ----------------------------------------------------------------------------------------------

SerializeEntityManagerSharedComponentTable SerializeSharedComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator
) {
	SerializeEntityManagerSharedComponentTable table;



	return table;
}

// ----------------------------------------------------------------------------------------------

DeserializeEntityManagerComponentTable DeserializeComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator
) {
	DeserializeEntityManagerComponentTable table;



	return table;
}

// ----------------------------------------------------------------------------------------------

DeserializeEntityManagerSharedComponentTable DeserializeSharedComponentTable(
	const EditorState* editor_state,
	AllocatorPolymorphic allocator
) {
	DeserializeEntityManagerSharedComponentTable table;



	return table;
}

// ----------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------

#pragma endregion

// ----------------------------------------------------------------------------------------------

bool SaveEditorScene(const EditorState* editor_state, const EntityManager* entity_manager, const AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	//SerializeEntityManager();

	return false;
}

// ----------------------------------------------------------------------------------------------

bool LoadEditorScene(EditorState* editor_state, EntityManager* entity_manager, AssetDatabaseReference* database, Stream<wchar_t> filename)
{
	return false;
}

// ----------------------------------------------------------------------------------------------
