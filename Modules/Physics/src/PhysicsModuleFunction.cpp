#include "pch.h"
#include "PhysicsModuleFunction.h"
#include "ContactManifolds.h"
#include "CollisionDetection/src/FixedGrid.h"
#include "CollisionDetection/src/CollisionDetectionComponents.h"
#include "CollisionDetection/src/GJK.h"
#include "ECSEngineWorld.h"
#include "ECSEngineComponents.h"
#include "ECSEngineForEach.h"
#include "Rigidbody.h"
#include "SolverCommon.h"
#include "ContactConstraint.h"

using namespace ECSEngine;

ECS_THREAD_TASK(GridHandler) {
	FixedGridHandlerData* data = (FixedGridHandlerData*)_data;
	EntityManager* entity_manager = world->entity_manager;
	// Retrieve the meshes and check the collisions
	//const RenderMesh* first_mesh = entity_manager->TryGetComponent<RenderMesh>(data->first_identifier);
	const ConvexCollider* first_collider = entity_manager->TryGetComponent<ConvexCollider>(data->first_identifier);
	if (first_collider != nullptr && first_collider->hull.vertex_size > 0) {
		//const RenderMesh* second_mesh = entity_manager->TryGetComponent<RenderMesh>(data->second_identifier);
		const ConvexCollider* second_collider = entity_manager->TryGetComponent<ConvexCollider>(data->second_identifier);
		if (second_collider != nullptr && second_collider->hull.vertex_size > 0) {
			Translation* first_translation;
			Rotation* first_rotation;
			Scale* first_scale;
			GetEntityTransform(entity_manager, data->first_identifier, &first_translation, &first_rotation, &first_scale);

			Translation* second_translation;
			Rotation* second_rotation;
			Scale* second_scale;
			GetEntityTransform(entity_manager, data->second_identifier, &second_translation, &second_rotation, &second_scale);

			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
			Matrix first_matrix = GetEntityTransformMatrix(first_translation, first_rotation, first_scale);
			ConvexHull first_collider_transformed = first_collider->hull.TransformToTemporary(first_matrix, &stack_allocator);

			Matrix second_matrix = GetEntityTransformMatrix(second_translation, second_rotation, second_scale);
			ConvexHull second_collider_transformed = second_collider->hull.TransformToTemporary(second_matrix, &stack_allocator);

			float3* coalesced_position = (float3*)malloc(sizeof(float3) * second_collider_transformed.vertex_size);
			for (size_t index = 0; index < second_collider_transformed.vertex_size; index++) {
				coalesced_position[index] = second_collider_transformed.GetPoint(index);
			}

			Matrix3x3 inertia_tesnor = ComputeInertiaTensor(&first_collider->hull);
			free(coalesced_position);

			float distance = GJK(&first_collider_transformed, &second_collider_transformed);
			if (distance < 0.0f) {
				// Intersection
				//LogInfo("Collision");
				// Call the SAT to determine the contacting features
				//Timer timer;
				SATQuery query = SAT(&first_collider_transformed, &second_collider_transformed);
				//float duration = timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
				//ECS_FORMAT_TEMP_STRING(message, "{#}\n", duration);
				//OutputDebugStringA(message.buffer);
				if (query.type == SAT_QUERY_NONE) {
					world->debug_drawer->AddStringThread(thread_id, first_translation != nullptr ? first_translation->value : float3::Splat(0.0f),
						float3::Splat(1.0f), 1.0f, "None", ECS_COLOR_ORANGE);
					/*Line3D first_line = first_collider_transformed.GetEdgePoints(17);
					Line3D second_line = second_collider_transformed.GetEdgePoints(1);
					world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
					world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);*/
				}
				else if (query.type == SAT_QUERY_EDGE) {
					world->debug_drawer->AddStringThread(thread_id, first_translation != nullptr ? first_translation->value : float3::Splat(0.0f),
						float3::Splat(1.0f), 1.0f, "Edge", ECS_COLOR_ORANGE);
					Line3D first_line = first_collider_transformed.GetEdgePoints(query.edge.edge_1_index);
					Line3D second_line = second_collider_transformed.GetEdgePoints(query.edge.edge_2_index);
					world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
					world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);

					ContactManifold manifold = ComputeContactManifold(&first_collider_transformed, &second_collider_transformed, query);
					world->debug_drawer->AddPointThread(thread_id, manifold.contact_points[0], 2.0f, ECS_COLOR_AQUA);

					//query.edge.edge_1_index = 97;
					//query.edge.edge_2_index = 1;
					const ConvexHullEdge& first_edge = first_collider_transformed.edges[query.edge.edge_1_index];
					const ConvexHullEdge& second_edge = second_collider_transformed.edges[query.edge.edge_2_index];
					float3 first_normal_1 = first_collider_transformed.faces[first_edge.face_1_index].plane.normal;
					float3 first_normal_2 = first_collider_transformed.faces[first_edge.face_2_index].plane.normal;
					float3 second_normal_1 = second_collider_transformed.faces[second_edge.face_1_index].plane.normal;
					float3 second_normal_2 = second_collider_transformed.faces[second_edge.face_2_index].plane.normal;
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), first_normal_1, ECS_COLOR_LIME);
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), first_normal_2, ECS_COLOR_LIME);
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), second_normal_1, ECS_COLOR_RED);
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), second_normal_2, ECS_COLOR_RED);
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), -second_normal_2, ECS_COLOR_BROWN);
					world->debug_drawer->AddLineThread(thread_id, first_normal_1, first_normal_2, ECS_COLOR_AQUA);
					world->debug_drawer->AddLineThread(thread_id, second_normal_1, second_normal_2, ECS_COLOR_WHITE);
				}
				else if (query.type == SAT_QUERY_FACE) {
					world->debug_drawer->AddStringThread(thread_id, first_translation != nullptr ? first_translation->value : float3::Splat(0.0f),
						float3::Splat(1.0f), 1.0f, "Face", ECS_COLOR_ORANGE);
					const ConvexHull* convex_hull = query.face.first_collider ? &first_collider_transformed : &second_collider_transformed;
					const ConvexHull* second_hull = query.face.first_collider ? &second_collider_transformed : &first_collider_transformed;
					const ConvexHullFace& face_1 = convex_hull->faces[query.face.face_index];
					for (unsigned int index = 0; index < face_1.points.size; index++) {
						unsigned int next_index = index == face_1.points.size - 1 ? 0 : index + 1;
						float3 A = convex_hull->GetPoint(face_1.points[index]);
						float3 B = convex_hull->GetPoint(face_1.points[next_index]);
						world->debug_drawer->AddLineThread(thread_id, A, B, ECS_COLOR_ORANGE);
					}

					/*query.face.second_face_index = 9;
					const ConvexHullFace& face_2 = second_hull->faces[query.face.second_face_index];
					for (unsigned int index = 0; index < face_2.points.size; index++) {
						unsigned int next_index = index == face_2.points.size - 1 ? 0 : index + 1;
						float3 A = second_hull->GetPoint(face_2.points[index]);
						float3 B = second_hull->GetPoint(face_2.points[next_index]);
						world->debug_drawer->AddLineThread(thread_id, A, B, ECS_COLOR_ORANGE);
					}*/

					ContactManifold manifold = ComputeContactManifold(&first_collider_transformed, &second_collider_transformed, query);
					for (size_t index = 0; index < manifold.contact_point_count; index++) {
						world->debug_drawer->AddPointThread(thread_id, manifold.contact_points[index], 2.0f, ECS_COLOR_AQUA);
					}
				}
			}
		}
	}
}

static ECS_THREAD_TASK(ChangeHandler) {
	FixedGrid* fixed_grid = (FixedGrid*)_data;
	fixed_grid->ChangeHandler(GridHandler, nullptr, 0);
}

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement change_handler;
	change_handler.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	change_handler.initialize_data_task_name = STRING(CollisionBroadphase);
	ECS_REGISTER_TASK(change_handler, ChangeHandler, data);
	
	AddSolverCommonTasks(data);
	AddSolverTasks(data);
}

#if 0

void ModuleBuildFunctions(ModuleBuildFunctionsData* data) {

}

#endif

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

#if 0

void ModuleRegisterDebugDrawFunction(ECSEngine::ModuleRegisterDebugDrawFunctionData* data) {

}

#endif

#if 0

void ModuleRegisterDebugDrawTaskElementsFunction(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data) {

}

#endif