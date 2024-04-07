#include "pch.h"
#include "Narrowphase.h"
#include "Graphics/src/GraphicsComponents.h"
#include "GJK.h"
#include "SAT.h"
#include "CollisionDetectionComponents.h"

ECS_THREAD_TASK(NarrowphaseGridHandler) {
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
					Line3D first_line = first_collider_transformed.GetEdgePoints(17);
					Line3D second_line = second_collider_transformed.GetEdgePoints(1);
					world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
					world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);
				}
				else if (query.type == SAT_QUERY_EDGE) {
					world->debug_drawer->AddStringThread(thread_id, first_translation != nullptr ? first_translation->value : float3::Splat(0.0f),
						float3::Splat(1.0f), 1.0f, "Edge", ECS_COLOR_ORANGE);
					Line3D first_line = first_collider_transformed.GetEdgePoints(query.edge.edge_1_index);
					Line3D second_line = second_collider_transformed.GetEdgePoints(query.edge.edge_2_index);
					world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
					world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);
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

					//const ConvexHullFace& face_2 = second_hull->faces[query.face.second_face_index];
					//for (unsigned int index = 0; index < face_2.points.size; index++) {
					//	unsigned int next_index = index == face_2.points.size - 1 ? 0 : index + 1;
					//	float3 A = second_hull->GetPoint(face_2.points[index]);
					//	float3 B = second_hull->GetPoint(face_2.points[next_index]);
					//	world->debug_drawer->AddLineThread(thread_id, A, B, ECS_COLOR_ORANGE);
					//}
				}
			}
		}
	}
}

void SetNarrowphaseTasks(ModuleTaskFunctionData* data) {
	// At the moment, there is nothing to be done
}