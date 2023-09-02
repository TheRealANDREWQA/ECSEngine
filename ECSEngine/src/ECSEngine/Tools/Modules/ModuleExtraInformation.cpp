#include "ecspch.h"
#include "ModuleExtraInformation.h"

#define RENDER_MESH_BOUNDS "RenderMeshBounds"

namespace ECSEngine {

	void SetGraphicsModuleRenderMeshBounds(ModuleRegisterExtraInformationFunctionData* register_data, Stream<char> component, Stream<char> field)
	{
		void* value_allocation = Allocate(register_data->allocator, sizeof(char) * (component.size + field.size + 1));
		Stream<char> value = { value_allocation, 0 };
		value.AddStream(component);
		value.Add(":");
		value.AddStream(field);

		register_data->extra_information.Add({ RENDER_MESH_BOUNDS, value });
	}

	GraphicsModuleRenderMeshBounds GetGraphicsModuleRenderMeshBounds(ModuleExtraInformation extra_information)
	{
		GraphicsModuleRenderMeshBounds bounds;

		Stream<char> value = extra_information.Find(RENDER_MESH_BOUNDS);
		if (value.size > 0) {
			// Split by ":"
			ECS_STACK_CAPACITY_STREAM(Stream<char>, splits, 2);
			function::SplitString(value, ":", splits);
			if (splits.size == 2) {
				bounds.component = splits[0];
				bounds.field = splits[1];
			}
		}

		return bounds;
	}

}