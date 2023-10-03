#include "pch.h"
#include "ModuleFunction.h"
#include "Render.h"
#include "Components.h"

using namespace ECSEngine;

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement elements[4];
	
	for (size_t index = 0; index < std::size(elements); index++) {
		elements[index].task_group = ECS_THREAD_TASK_FINALIZE_LATE;
		elements[index].initialize_task_function = nullptr;
	}
	
	elements[0].initialize_task_function = RenderTaskInitialize;
	ECS_REGISTER_FOR_EACH_TASK(elements[0], RenderTask, data);

	elements[1].initialize_task_function = RenderSelectablesInitialize;
	ECS_REGISTER_FOR_EACH_TASK(elements[1], RenderSelectables, data);
	
	ECS_REGISTER_FOR_EACH_TASK(elements[2], RenderInstancedFramebuffer, data);

	ECS_REGISTER_FOR_EACH_TASK(elements[3], RenderFlush, data);
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
	const Translation* translation = (const Translation*)data->dependency_components[0];
	const Rotation* rotation = (const Rotation*)data->dependency_components[1];
	const Scale* scale = (const Scale*)data->dependency_components[2];

	float3 translation_value = float3::Splat(0.0f);
	Quaternion rotation_value = QuaternionIdentity();
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

	AABB aabb = TransformAABB(mesh->mesh->mesh.bounds, { translation_value }, QuaternionToMatrixLow(rotation_value), scale_value);
	float3 aabb_center = AABBCenter(aabb).AsFloat3Low();
	float3 aabb_scale = AABBHalfExtents(aabb).AsFloat3Low();

	data->debug_drawer->AddAABB(aabb_center, aabb_scale, ECS_COLOR_RED, { true, false });
}

void ModuleRegisterDebugDrawFunction(ECSEngine::ModuleRegisterDebugDrawFunctionData* data) {
	ModuleDebugDrawElement element;
	element.component = RenderMesh::ID();
	element.component_type = ECS_COMPONENT_SHARED;
	element.draw_function = RenderMeshDebugDraw;

	element.dependency_components[0] = { Translation::ID(), ECS_COMPONENT_UNIQUE };
	element.dependency_components[1] = { Rotation::ID(), ECS_COMPONENT_UNIQUE };
	element.dependency_components[2] = { Scale::ID(), ECS_COMPONENT_UNIQUE };
	element.dependency_component_count = 3;

	data->elements->AddAssert(element);
}

#endif