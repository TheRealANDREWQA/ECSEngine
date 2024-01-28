#include "pch.h"
#include "ModuleFunction.h"
#include "Render.h"
#include "Components.h"

using namespace ECSEngine;

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement elements[5] = {};
	
	for (size_t index = 0; index < std::size(elements); index++) {
		elements[index].task_group = ECS_THREAD_TASK_FINALIZE_LATE;
		elements[index].initialize_task_function = nullptr;
	}
	
	ECS_REGISTER_FOR_EACH_TASK(elements[0], DrawMeshes, data);

	ECS_REGISTER_FOR_EACH_TASK(elements[1], DrawSelectables, data);
	
	ECS_REGISTER_FOR_EACH_TASK(elements[2], DrawInstancedFramebuffer, data);

	ECS_REGISTER_FOR_EACH_TASK(elements[3], FlushRenderCommands, data);

	elements[4].task_group = ECS_THREAD_TASK_FINALIZE_EARLY;
	elements[4].task_function = RecalculateCamera;
	elements[4].task_name = STRING(RecalculateCamera);
	data->tasks->AddAssert(elements[4]);

	// Maintain the order since this is important
	data->maintain_order_in_group = true;
}

#if 0

void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data) {

}

#endif

#if 0

void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data) {

}

#endif

#if 0

void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data) {

}

#endif

#if 1

void ModuleRegisterExtraInformationFunction(ECSEngine::ModuleRegisterExtraInformationFunctionData* data) {
	SetGraphicsModuleRenderMeshBounds(data, STRING(RenderMesh), STRING(mesh));
}

#endif

#if 1

void RenderMeshDebugDraw(ModuleDebugDrawComponentFunctionData* data) {
	const RenderMesh* mesh = (const RenderMesh*)data->component;
	if (mesh->Validate()) {
		const Translation* translation = (const Translation*)data->dependency_components[0];
		const Rotation* rotation = (const Rotation*)data->dependency_components[1];
		const Scale* scale = (const Scale*)data->dependency_components[2];

		float3 translation_value = float3::Splat(0.0f);
		QuaternionScalar rotation_value = QuaternionIdentityScalar();
		float3 scale_value = float3::Splat(1.0f);

		if (translation != nullptr) {
			translation_value = translation->value;
		}

		if (rotation != nullptr) {
			rotation_value = rotation->value;
		}

		if (scale != nullptr) {
			scale_value = scale->value;
		}

		AABBScalar aabb = TransformAABB(mesh->mesh->mesh.bounds, { translation_value }, QuaternionToMatrix(rotation_value), scale_value);
		float3 aabb_center = AABBCenter(aabb);
		float3 aabb_scale = AABBHalfExtents(aabb);

		data->debug_drawer->AddAABB(aabb_center, aabb_scale, ECS_COLOR_RED, { true, false });
	}
}

void ModuleRegisterComponentFunctionsFunction(ModuleRegisterComponentFunctionsData* data) {
	ModuleDebugDrawElement element;
	element.draw_function = RenderMeshDebugDraw;
	element.dependency_components[0] = { Translation::ID(), ECS_COMPONENT_UNIQUE };
	element.dependency_components[1] = { Rotation::ID(), ECS_COMPONENT_UNIQUE };
	element.dependency_components[2] = { Scale::ID(), ECS_COMPONENT_UNIQUE };
	element.dependency_component_count = 3;

	ModuleComponentFunctions render_mesh_entry;
	render_mesh_entry.debug_draw = element;
	render_mesh_entry.component_name = STRING(RenderMesh);

	data->functions->AddAssert(&render_mesh_entry);
}

#endif