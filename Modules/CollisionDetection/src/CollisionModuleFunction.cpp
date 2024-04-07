#include "pch.h"
#include "CollisionModuleFunction.h"
#include "CollisionDetectionComponents.h"
#include "Broadphase.h"
#include "ECSEngineRendering.h"

#include "Graphics/src/GraphicsComponents.h"

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
	//	translation->value.y = 0.0f;
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
	if (render_mesh != nullptr && render_mesh->Validate()) {
		AllocatorPolymorphic world_allocator = data->world_global_memory_manager;
		// We need to make sure that we use multithreaded allocations here
		world_allocator.allocation_type = ECS_ALLOCATION_MULTI;

		// Retrieve the mesh vertices from the GPU memory to the CPU side
		Stream<float3> vertex_positions = GetMeshPositionsCPU(data->world_graphics, render_mesh->mesh->mesh, world_allocator);
		ConvexCollider* collider = (ConvexCollider*)data->component;
		DeallocateConvexCollider(collider, data->component_allocator);
		//collider->hull = CreateConvexHullFromMesh(vertex_positions, data->component_allocator);
		collider->hull_size = 0;
		//collider->mesh = GiftWrappingTriangleMesh(vertex_positions, data->component_allocator);
		collider->hull = GiftWrappingConvexHull(vertex_positions, data->component_allocator);
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
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 20);
	const ConvexCollider* collider = (const ConvexCollider*)data->component;
	const Translation* translation = (const Translation*)data->dependency_components[0];
	const Rotation* rotation = (const Rotation*)data->dependency_components[1];
	const Scale* scale = (const Scale*)data->dependency_components[2];
	float3 translation_value = translation != nullptr ? translation->value : float3::Splat(0.0f);
	translation_value += float3::Splat(0.0f);
	//translation_value = float3::Splat(0.0f);

	Matrix entity_matrix = GetEntityTransformMatrix(translation, rotation, scale);
	TriangleMesh transformed_mesh = collider->mesh.Transform(entity_matrix, &stack_allocator);
	ConvexHull transformed_hull = collider->hull.TransformToTemporary(entity_matrix, &stack_allocator);
	
	for (size_t index = 0; index < transformed_hull.vertex_size; index++) {
		ECS_FORMAT_TEMP_STRING(nr, "{#}", index);
		//if (index == 29) {
			//data->debug_drawer->AddStringThread(data->thread_id, transformed_hull.GetPoint(index), float3(0.0f, 0.0f, -1.0f), 0.1f, nr.buffer, ECS_COLOR_ORANGE);
		//}
	}
	//if (transformed_hull.vertex_size > 273) {
	//	ECS_FORMAT_TEMP_STRING(nr, "{#}", 259);
	//	data->debug_drawer->AddStringThread(data->thread_id, transformed_hull.GetPoint(259), float3(0.0f, 0.0f, -1.0f), 0.1f, nr.buffer, ECS_COLOR_ORANGE);
	//	ECS_FORMAT_TEMP_STRING(nr2, "{#}", 273);
	//	data->debug_drawer->AddStringThread(data->thread_id, transformed_hull.GetPoint(273), float3(0.0f, 0.0f, -1.0f), 0.1f, nr2.buffer, ECS_COLOR_ORANGE);
	//}

	unsigned int edge_count = collider->hull_size > transformed_hull.edges.size ? transformed_hull.edges.size : collider->hull_size;
	for (size_t index = 0; index < edge_count; index++) {
		Line3D line = transformed_hull.GetEdgePoints(index);
		data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_GREEN);
	}
	//for (size_t index = 0; index < transformed_hull.faces.size; index++) {
	//	const ConvexHullFace& face = transformed_hull.faces[index];
	//	/*for (unsigned int subindex = 0; subindex < face.EdgeCount(); subindex++) {
	//		Line3D line = transformed_hull.GetFaceEdge(index, subindex);
	//		data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_GREEN);
	//	}*/
	//	ECS_FORMAT_TEMP_STRING(nr, "{#}", index);
	//	if (index == 5) {
	//		data->debug_drawer->AddStringThread(data->thread_id, transformed_hull.GetFaceCenter(index), float3(0.0f, 0.0f, -1.0f), 0.1f, nr.buffer, ECS_COLOR_ORANGE);
	//	}
	//}
	//if (transformed_hull.faces.size > 0) {
	//	unsigned int face_index = collider->hull_size >= transformed_hull.faces.size ? transformed_hull.faces.size - 1 : collider->hull_size;
	//	const ConvexHullFace& face = transformed_hull.faces[face_index];
	//	for (unsigned int subindex = 0; subindex < face.EdgeCount(); subindex++) {
	//		Line3D line = transformed_hull.GetFaceEdge(face_index, subindex);
	//		data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_AQUA);
	//	}
	//}
	/*for (size_t index = 0; index < transformed_hull.edges.size; index++) {
		Line3D line = transformed_hull.GetEdgePoints(index);
		data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_GREEN);
	}*/
	if (transformed_hull.faces.size == 269) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		//unsigned char* face_reference_count = (unsigned char*)stack_allocator.Allocate(sizeof(unsigned char) * transformed_hull.faces.size);
		//memset(face_reference_count, 0, sizeof(unsigned char) * transformed_hull.faces.size);
		//// Verify that each edge has 2 faces
		//// Verify that each face is referenced as the number of edges it has
		//for (unsigned int index = 0; index < transformed_hull.edges.size; index++) {
		//	face_reference_count[transformed_hull.edges[index].face_1_index]++;
		//	face_reference_count[transformed_hull.edges[index].face_2_index]++;
		//}

		//for (unsigned int index = 0; index < transformed_hull.faces.size; index++) {
		//	if (face_reference_count[index] != transformed_hull.faces[index].EdgeCount()) {
		//		const ConvexHullFace* face = &transformed_hull.faces[index];
		//		for (size_t edge_index = 0; edge_index < face->EdgeCount(); edge_index++) {
		//			Line3D line = transformed_hull.GetFaceEdge(index, edge_index);
		//			data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_ORANGE);
		//		}
		//	}
		//}

		//unsigned char* edge_reference_count = (unsigned char*)stack_allocator.Allocate(sizeof(unsigned char) * transformed_hull.edges.size);
		//memset(edge_reference_count, 0, sizeof(unsigned char) * transformed_hull.edges.size);
		//for (unsigned int index = 0; index < transformed_hull.faces.size; index++) {
		//	for (unsigned int subindex = 0; subindex < transformed_hull.faces[index].EdgeCount(); subindex++) {
		//		uint2 edge = transformed_hull.GetFaceEdgeIndices(index, subindex);
		//		unsigned int edge_index = transformed_hull.FindEdge(edge.x, edge.y);
		//		edge_reference_count[edge_index]++;
		//	}
		//}

		//for (unsigned int index = 0; index < transformed_hull.edges.size; index++) {
		//	if (edge_reference_count[index] > 2) {
		//		Line3D line = transformed_hull.GetEdgePoints(index);
		//		data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_ORANGE);
		//	}
		//}

		//const ConvexHullFace* face = &transformed_hull.faces[129];
		//for (size_t index = 0; index < face->EdgeCount(); index++) {
		//	Line3D line = transformed_hull.GetFaceEdge(129, index);
		//	data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_ORANGE);
		//}
		//float3 face_center = transformed_hull.GetFaceCenter(5);
		//float3 point = transformed_hull.GetPoint(35);
		//float3 cross = Cross(transformed_hull.faces[5].plane.normal, point - face_center);
		////float3 cross = transformed_hull.faces[5].plane.normal;
		//float dot = Dot(cross, transformed_hull.GetPoint(22) - face_center);

		//float angle = AngleBetweenVectorsNormalized(transformed_hull.faces[5].plane.normal, transformed_hull.faces[23].plane.normal);

		//data->debug_drawer->AddLineThread(data->thread_id, face_center, face_center + cross * 1.0f, ECS_COLOR_AQUA);
		////data->debug_drawer->AddLineThread(data->thread_id, face_center, face_center + (point - face_center) * 1.0f, ECS_COLOR_AQUA);
		//data->debug_drawer->AddLineThread(data->thread_id, face_center, face_center + (transformed_hull.GetPoint(22) - face_center) * 1.0f, ECS_COLOR_AQUA);
		//face = &transformed_hull.faces[88];
		////for (size_t index = 0; index < face->EdgeCount(); index++) {
		////	Line3D line = transformed_hull.GetFaceEdge(88, index);
		////	data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_ORANGE);
		////}
		//face = &transformed_hull.faces[112];
		//for (size_t index = 0; index < face->EdgeCount(); index++) {
		//	Line3D line = transformed_hull.GetFaceEdge(112, index);
		//	data->debug_drawer->AddLineThread(data->thread_id, line.A, line.B, ECS_COLOR_AQUA);
		//}
	}

	float total_area = 0.0f;
	for (size_t index = 0; index < transformed_hull.faces.size; index++) {
		const ConvexHullFace& face = transformed_hull.faces[index];
		//if (face.points.size == 4) {
			float3 face_total = float3::Splat(0.0f);
			unsigned int count = 0;
			for (size_t subindex = 0; subindex < face.points.size; subindex++) {
				face_total += transformed_hull.GetPoint(face.points[subindex]);
			}
			face_total /= float3::Splat(face.points.size);
			//data->debug_drawer->AddArrowThread(data->thread_id, face_total, face_total + transformed_hull.faces[index].plane.normal * float3::Splat(1.0f), 0.25f, ECS_COLOR_MAGENTA);
			//float area = TriangleArea(transformed_hull.GetPoint(face.points[0]), transformed_hull.GetPoint(face.points[1]), transformed_hull.GetPoint(face.points[2]));

			ECS_FORMAT_TEMP_STRING(nr, "{#}", index);
			//data->debug_drawer->AddStringThread(data->thread_id, face_total, float3(0.0f, 0.0f, -1.0f), 0.01f, nr, ECS_COLOR_AQUA);
			//total_area += area;
		//}
	}

	ECS_FORMAT_TEMP_STRING(area_string, "{#}", total_area);
	data->debug_drawer->AddStringThread(data->thread_id, transformed_hull.center, float3(0.0f, 0.0f, -1.0f), 0.2f, area_string, ECS_COLOR_AQUA);
}

void ModuleRegisterComponentFunctionsFunction(ModuleRegisterComponentFunctionsData* data) {
	ModuleComponentFunctions convex_collider;
	convex_collider.build_entry.function = ModuleBuildConvexCollider;
	convex_collider.build_entry.component_dependencies.Initialize(data->allocator, 1);
	convex_collider.build_entry.component_dependencies[0] = STRING(RenderMesh);
	convex_collider.component_name = STRING(ConvexCollider);

	convex_collider.debug_draw.draw_function = ConvexColliderDebugDraw;
	convex_collider.debug_draw.AddDependency({ Translation::ID(), ECS_COMPONENT_UNIQUE });
	convex_collider.debug_draw.AddDependency({ Rotation::ID(), ECS_COMPONENT_UNIQUE });
	convex_collider.debug_draw.AddDependency({ Scale::ID(), ECS_COMPONENT_UNIQUE });

	data->functions->AddAssert(convex_collider);
}

#endif