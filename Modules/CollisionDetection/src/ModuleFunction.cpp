#include "pch.h"
#include "ModuleFunction.h"
#include "Components.h"
#include "Broadphase.h"
#include "ECSEngineRendering.h"

#include "Graphics/src/Components.h"

#include "GiftWrapping.h"

static void ApplyMovementTask(
	ForEachEntityData* for_each_data,
	Translation* translation
) {
	static bool was_used = false;

	//translation->value.x -= settings->factor * for_each_data->world->delta_time;
	//translation->value.x -= 5.5f * for_each_data->world->delta_time;
	if (for_each_data->world->keyboard->IsPressed(ECS_KEY_P)) {
		translation->value.x = 0.0f;
	}
	//if (for_each_data->world->mouse->IsPressed(ECS_MOUSE_LEFT)) {
	//	translation->value.x = 0.0f;
	//}
	//translation->value.y -= 1.50f * for_each_data->world->delta_time;
	//translation->value.x -= 5.50f * for_each_data->world->delta_time;
	//if (for_each_data->thread_id == 2) {
	//	if (!was_used) {
	//		int* ptr = NULL;
	//		*ptr = 0;
	//		//for_each_data->world->entity_manager->GetComponent(Entity{ (unsigned int)-1 }, Translation::ID());
	//		was_used = true;
	//	}
	//}
}

template<bool get_query>
ECS_THREAD_TASK(ApplyMovement) {
	ForEachEntity<get_query, QueryWrite<Translation>>(thread_id, world).Function<QueryExclude<Scale>>(ApplyMovementTask);
}

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement schedule_element;

	schedule_element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	ECS_REGISTER_FOR_EACH_TASK(schedule_element, ApplyMovement, data);
	SetBroadphaseTasks(data);
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

#if 0

void ModuleRegisterExtraInformationFunction(ECSEngine::ModuleRegisterExtraInformationFunctionData* data) {

}

#endif

#if 1

void ModuleRegisterDebugDrawTaskElementsFunction(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data) {
	SetBroadphaseDebugTasks(data);
}

#endif

#if 1

//static bool ModuleCompareConvexCollider(SharedComponentCompareFunctionData* data) {
//	const ConvexCollider* first = (const ConvexCollider*)data->first;
//	const ConvexCollider* second = (const ConvexCollider*)data->second;
//	return CompareConvexCollider(first, second);
//}
//
//static void ModuleDeallocateConvexCollider(ComponentDeallocateFunctionData* data) {
//	ConvexCollider* collider = (ConvexCollider*)data->data;
//	collider->hull.Deallocate(data->allocator);
//	collider->mesh.Deallocate(data->allocator);
//}
//
//static void ModuleCopyConvexCollider(ComponentCopyFunctionData* data) {
//	ConvexCollider* destination = (ConvexCollider*)data->destination;
//	const ConvexCollider* source = (const ConvexCollider*)data->source;
//	destination->hull.Copy(&source->hull, data->allocator, data->deallocate_previous);
//	destination->mesh.Copy(&source->mesh, data->allocator, data->deallocate_previous);
//}

static void BuildConvexColliderTaskBase(ModuleComponentBuildFunctionData* data) {
	const RenderMesh* render_mesh = data->entity_manager->TryGetComponent<RenderMesh>(data->entity);
	if (render_mesh != nullptr) {
		AllocatorPolymorphic world_allocator = data->world_global_memory_manager;
		// We need to make sure that we use multithreaded allocations here
		world_allocator.allocation_type = ECS_ALLOCATION_MULTI;

		// Retrieve the mesh vertices from the GPU memory to the CPU side
		Stream<float3> vertex_positions = GetMeshPositionsCPU(data->world_graphics, render_mesh->mesh->mesh, world_allocator);
		ConvexCollider* collider = (ConvexCollider*)data->component;
		if (collider->hull.size > 0) {
			DeallocateConvexCollider(collider, data->component_allocator);
		}
		collider->hull = CreateConvexHullFromMesh(vertex_positions, data->component_allocator);
		collider->hull_size = collider->hull.size;
		collider->mesh = GiftWrapping(vertex_positions, data->component_allocator);
		vertex_positions.Deallocate(world_allocator);
	}
	else {
		// Print a message that the entity requires a render mesh
		ECS_FORMAT_TEMP_STRING(message, "The entity {#} requires a RenderMesh to calculate the ConvexCollider", GetEntityNameIndexOnlyTempStorage(data->entity_manager, data->entity));
		data->print_message.Warn(message);
	}
}

static ECS_THREAD_TASK(BuildConvexColliderTask) {
	ModuleComponentBuildFunctionData* data = (ModuleComponentBuildFunctionData*)_data;
	data->gpu_lock.Lock();
	BuildConvexColliderTaskBase(data);
	data->gpu_lock.Unlock();
}

static ThreadTask ModuleBuildConvexCollider(ModuleComponentBuildFunctionData* data) {
	// We need to launch a thread task
	return ECS_THREAD_TASK_NAME(BuildConvexColliderTask, data, sizeof(*data));
}

static void ConvexColliderDebugDraw(ModuleDebugDrawComponentFunctionData* data) {
	const ConvexCollider* collider = (const ConvexCollider*)data->component;
	const Translation* translation = (const Translation*)data->dependency_components[0];
	float3 translation_value = translation != nullptr ? translation->value : float3::Splat(0.0f);
	translation_value = float3::Splat(0.0f);
	
	size_t count = collider->mesh.triangles.size < collider->hull_size ? 0 : collider->mesh.triangles.size - collider->hull_size;
	for (size_t index = 0; index < count; index++) {
		uint3 triangle = collider->mesh.triangles[index];
		float3 a = collider->mesh.positions[triangle.x];
		float3 b = collider->mesh.positions[triangle.y];
		float3 c = collider->mesh.positions[triangle.z];

		DebugDrawCallOptions options;
		options.wireframe = true;
		data->debug_drawer->AddTriangleThread(data->thread_id, translation_value + a, translation_value + b, translation_value + c, ECS_COLOR_GREEN, options);
	}
	/*for (size_t index = 0; index < 10; index++) {
		data->debug_drawer->AddPointThread(data->thread_id, collider->mesh.positions[index], ECS_COLOR_GREEN);
	}*/
}

void ModuleRegisterComponentFunctionsFunction(ModuleRegisterComponentFunctionsData* data) {
	ModuleComponentFunctions convex_collider;
	convex_collider.build_entry.function = ModuleBuildConvexCollider;
	convex_collider.build_entry.component_dependencies.Initialize(data->allocator, 1);
	convex_collider.build_entry.component_dependencies[0] = STRING(RenderMesh);
	convex_collider.component_name = STRING(ConvexCollider);

	convex_collider.debug_draw.draw_function = ConvexColliderDebugDraw;
	convex_collider.debug_draw.AddDependency({ Translation::ID(), ECS_COMPONENT_UNIQUE });

	data->functions->AddAssert(convex_collider);
}

#endif